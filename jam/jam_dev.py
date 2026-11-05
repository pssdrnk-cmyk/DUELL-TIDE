#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL device tool for the FM-1 (internal, Windows / Git Bash): identity, full backup, restore and firmware install over USB-MIDI.

  jam_dev.py [--mock] info    [--port NAME]
  jam_dev.py [--mock] backup  [--out DIR] [--port NAME]
  jam_dev.py [--mock] restore FILE [--yes] [--include-raw] [--out DIR] [--port NAME]
  jam_dev.py [--mock] flash   PACKAGE.fwsc [--yes] [--out DIR] [--port NAME]
  jam_dev.py measure [all|tide|velocity|report RUNDIR..] [--bench PKG] [--normal PKG] [--no-restore] [--com NAME] [--yes] [--out DIR]

The backup file is the web editor's own ("duell-backup" version 5, felucca-base/web/editor/duellbackup.js), byte for byte, so the Backup page can
restore it and this tool can restore the page's files. Only the editor protocol's INFO and BACKUP_LIST / GET / PUT are sent (cmds 1, 65, 66, 67);
a firmware install is done by felucca-base/tools/fm1_install.py, run as a subprocess, never here. --mock talks to the editor's simulated unit
(jam/mock_bridge.mjs, needs node) instead of MIDI.

`measure` is the unit measurement of audit A01 (TIDE's audio DSP on both cores), Q14 (PHYS per model) and T17 (key velocity): it builds the bench package, backs the unit up, installs the bench
build, runs the suites over the USB serial console, writes work/measure/<stamp>/report.md, puts the normal firmware back, then records a guided key-velocity session over USB MIDI (docs/TESTING.md).

Exit codes: 0 ok, 1 failure (or cancelled), 2 bad arguments, 3 no DUELL unit found.
"""
from __future__ import annotations

import argparse
import base64
import json
import os
import queue
import re
import shutil
import subprocess
import sys
import time
import zlib
from collections import deque
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable, Optional, Protocol, Sequence

HERE = Path(__file__).resolve().parent
REPO = HERE.parent                                        # .../DUELL
BASE = REPO / "felucca-base"
INSTALLER = BASE / "tools" / "fm1_install.py"
BRIDGE = HERE / "mock_bridge.mjs"
DEFAULT_OUT = Path(os.environ.get("JAM_WORK") or REPO.parent / "work") / "backups"   # (a worktree in work/wt/<name>: env.sh sets JAM_WORK)
EXIT_OK, EXIT_FAIL, EXIT_USAGE, EXIT_NOUNIT = 0, 1, 2, 3

# ------------------------------------------------------------------------------------------------------------------ protocol constants
HDR = bytes([0xF0, 0x7D, 0x46, 0x4C])
CMD_INFO, CMD_PING, CMD_LIST, CMD_GET, CMD_PUT = 1, 25, 65, 66, 67
T_INFO, T_LIST, T_GET, T_PUT = 2.0, 4.0, 1.5, 6.0         # seconds, as the web Backup page waits
CHUNK = 256
MAX_OBJ = 3840
PATCH_ID, PATCH_KEY0, PATCH_SLOTS, PATCH_PAGE = 40, 1000, 192, 32   # JAM bank expansion (2026-10-29): 192 slots (a unit before it: 128)
PATCH_PAGES = PATCH_SLOTS // PATCH_PAGE
PATCH_PAGES_OLD = 4                                              # a unit with 128 slots answers pages 5, 6 with rc 1


def slot_bytes(slot: int) -> list[int]:
    """A slot in a GET / PUT begin: one 7-bit byte; a slot of 128..191 a second byte, its bit 7 (JAM bank: the unit takes the longer request)."""
    return [slot & 127] if slot < 128 else [slot & 127, slot >> 7]
REC_FIX, REC_WORST = 20, 580   # (PT_VER 4: the unit's PT_WORST 577 + TIDE's 3)
CAPS_MULTI_PATTERN, CAPS_WAVETABLES, CAPS_PATCH16, CAPS_PATCH_RECORDS = 4, 8, 16, 32
FORMAT, VERSION = "duell-backup", 5
DUELL_IDS = [0, 1, *range(13, 27)]
RESTORE_ORDER = [*range(13, 27), 1, 0]
OLD_PATCH_IDS = [6, 7, 9, 10, 11, 12, 27, 28, 29, 30, 31, 35, 36, 37, 38, 39]
KNOWN_IDS = set([0, 1, 2, 3, 4, 5, 8, *range(13, 27), 41, 32, 33, 34, *OLD_PATCH_IDS])
# FM6's voice bank (DUELL with TIDE, INFO tag 44 01 bit 1; duellbackup.js FVB_ID): object 41, read with GET (not in the plain LIST; rc 1 = the unit holds none), written back voice
# by voice with FM6B_BEGIN / WRITE / END (cmds 74..76). The object is the stored payload (a 32 byte header: "FM6V", 1, 32, the used mask, a 12 character name; the 32 voices of 112 bytes,
# a record's 128 7-bit bytes packed 8 to 7) and its CRC-32 (u32 little endian) after it.
FVB_ID, FVB_LEN, FVB_N, FVB_REC, FVB_PK, FVB_HEAD, CAPS_FVBANK = 41, 3616, 32, 112, 128, 32, 2
FVB_OBJ = FVB_LEN + 4
CMD_FM6B_BEGIN, CMD_FM6B_WRITE, CMD_FM6B_END = 74, 75, 76
TAG_LEN = {0x55: 3, 0x42: 3, 0x53: 3, 0x50: 3, 0x4E: 3, 0x52: 3, 0x4C: 3, 0x54: 3, 0x56: 3, 0x43: 3, 0x5A: 3, 0x59: 3, 0x4B: 3,
           0x4D: 4, 0x46: 4, 0x41: 4, 0x58: 4, 0x5B: 3, 0x4F: 3, 0x44: 3}
DUELL_RE = re.compile(r"^(?:FELUCCA )?DUELL (\d+\.\d+(?:\.\d+)?)(?!\S)")
PORT_RE = re.compile(r"duell", re.I)                     # the unit's USB product name; never probe other gear
TIMEOUT_MSG = "Backup timed out. Keep the FM-1 connected and stopped."
RC_NOTES = {1: "The unit refused the request", 2: "The unit found the data invalid", 3: "The unit is playing. Stop playback on the unit and try again",
            4: "The unit could not write its flash memory", 5: "The backup session on the unit expired. Start again",
            6: "The unit's patch store is full (STORE FULL): delete a saved patch on the unit, then try again"}
OLD_CHUNKS_NOTE = "Patches saved by an older firmware (physical chunks) are in this file but cannot be restored."
NO_RECORDS_NOTE = ("This unit's firmware cannot back up patches one at a time (it needs DUELL with INFO caps bit 5): this file has no patches.")


class DuellError(Exception):
    """A failure the user can act on; rc is the unit's answer code when the unit refused something."""

    def __init__(self, msg: str, rc: Optional[int] = None, fatal: bool = False):
        super().__init__(msg)
        self.rc, self.fatal = rc, fatal


class TransportTimeout(DuellError):
    """No reply in time: on the wire a lost frame, so a GET may be asked again (it is idempotent), a PUT never."""


class UnitNotFound(DuellError):
    """No DUELL port answered INFO (exit code 3)."""


def rc_check(rc: int) -> None:
    """Raise the unit's error code as the web page words it, so messages and files read the same."""
    if rc:
        raise DuellError(RC_NOTES.get(rc, f"The unit answered error {rc}"), rc=rc)


# ------------------------------------------------------------------------------------------------------------------ wire encodings
def u32(n: int) -> list[int]:
    """A 32-bit number as five 7-bit bytes, least significant first (the last holds bits 28..31)."""
    return [(n >> (7 * i)) & (15 if i == 4 else 127) for i in range(5)]


def r32(a: Sequence[int], off: int = 0) -> int:
    """Read u32() back; strict like fm1backup.js bkR32 (a short field or a top byte above 15 is an error)."""
    if len(a) < off + 5 or a[off + 4] > 15 or any(b > 127 for b in a[off:off + 5]):
        raise DuellError("Invalid archive number")
    return a[off] | a[off + 1] << 7 | a[off + 2] << 14 | a[off + 3] << 21 | a[off + 4] << 28


def pack7(data: bytes) -> list[int]:
    """8-bit bytes as 7-bit groups: a mask byte (bit i = the high bit of byte i), then up to seven low-7-bit bytes."""
    out: list[int] = []
    for off in range(0, len(data), 7):
        g = data[off:off + 7]
        out.append(sum((b >> 7) << i for i, b in enumerate(g)))
        out.extend(b & 127 for b in g)
    return out


def unpack7(a: Sequence[int], size: int) -> bytes:
    """pack7() back to exactly size bytes. Strict like bkUnpack: unused mask bits, a short group or trailing bytes mean a damaged reply."""
    out, i = bytearray(), 0
    while len(out) < size:
        n = min(7, size - len(out))
        if i >= len(a) or a[i] >> n:
            raise DuellError("Invalid archive bytes")
        mask = a[i]
        i += 1
        for k in range(n):
            if i >= len(a) or a[i] > 127:
                raise DuellError("Short archive chunk")
            out.append(a[i] | (mask >> k & 1) << 7)
            i += 1
    if i != len(a):
        raise DuellError("Trailing archive bytes")
    return bytes(out)


def crc32(b: bytes) -> int:
    """CRC-32 as the unit computes it (st_crc32); 0 for an empty object, as the unit lists it."""
    return zlib.crc32(b) & 0xFFFFFFFF if b else 0


def build_frame(cmd: int, args: Sequence[int]) -> bytes:
    """F0 7D 46 4C cmd args F7; every byte between F0 and F7 must be 7-bit or the unit drops the frame."""
    if any(not 0 <= b <= 127 for b in (cmd, *args)):
        raise ValueError("a SysEx data byte above 127")
    return bytes([*HDR, cmd, *args, 0xF7])


def reply_args(frame: bytes, cmd: int) -> Optional[list[int]]:
    """The args of frame when it is the reply to cmd, else None (pushes 23/24/26/32, other replies, other gear)."""
    if len(frame) < 6 or frame[:4] != HDR or frame[-1] != 0xF7 or frame[4] != cmd:
        return None
    return list(frame[5:-1])


# ------------------------------------------------------------------------------------------------------------------ transports
class Transport(Protocol):
    """One request at a time: the unit holds a single incoming frame, a second request in flight is lost."""
    port_name: str

    def request(self, cmd: int, args: Sequence[int], timeout: float) -> list[int]: ...

    def close(self) -> None: ...


class FrameTransport:
    """The request / reply rule shared by MIDI and the mock: drain, send one frame, take the first frame that answers this cmd."""
    port_name = ""

    def _send(self, frame: bytes) -> None:
        raise NotImplementedError

    def _recv(self, timeout: float) -> Optional[bytes]:
        raise NotImplementedError

    def _drain(self) -> None:
        while self._recv(0) is not None:
            pass

    def request(self, cmd: int, args: Sequence[int], timeout: float) -> list[int]:
        """Send cmd and wait for its reply; a late reply of an earlier request is drained first so it cannot be taken for this one."""
        self._drain()
        self._send(build_frame(cmd, args))
        deadline = time.monotonic() + timeout
        while (left := deadline - time.monotonic()) > 0:
            f = self._recv(left)
            if f is None:
                break
            r = reply_args(f, cmd)
            if r is not None:
                return r
        raise TransportTimeout(TIMEOUT_MSG)

    def close(self) -> None:
        pass


class MidiTransport(FrameTransport):
    """mido with the rtmidi backend, as fm1_install.py's MidoLink: a callback queues SysEx, 0.3 s settle after opening."""

    def __init__(self, in_name: str, out_name: str):
        mido = _mido()
        self._mido, self.port_name, self.q = mido, in_name, queue.Queue()
        self.inp = mido.open_input(in_name, callback=self._rx)
        try:
            self.out = mido.open_output(out_name)
        except Exception:
            self.inp.close()
            raise
        time.sleep(0.3)

    def _rx(self, msg) -> None:
        if msg.type == "sysex":
            self.q.put(bytes([0xF0, *msg.data, 0xF7]))

    def _send(self, frame: bytes) -> None:
        try:
            self.out.send(self._mido.Message("sysex", data=frame[1:-1]))
        except Exception as e:
            raise DuellError(f"cannot send to {self.port_name}: {e}")

    def _recv(self, timeout: float) -> Optional[bytes]:
        try:
            return self.q.get(timeout=timeout) if timeout > 0 else self.q.get_nowait()
        except queue.Empty:
            return None

    def close(self) -> None:
        for p in (getattr(self, "inp", None), getattr(self, "out", None)):
            try:
                p and p.close()
            except Exception:
                pass


class MockTransport(FrameTransport):
    """The web editor's simulated unit (mockunit.js) through jam/mock_bridge.mjs: the same frames as MIDI, answered at once."""

    def __init__(self, node: Optional[str] = None):
        node = node or shutil.which("node")
        if not node:
            raise DuellError("--mock needs node on PATH (. jam/env.sh)")
        self.port_name = "DUELL 0"
        self.q: deque[bytes] = deque()
        self.proc = subprocess.Popen([node, str(BRIDGE)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, encoding="utf-8", bufsize=1)

    def _call(self, msg: dict) -> dict:
        assert self.proc.stdin and self.proc.stdout
        self.proc.stdin.write(json.dumps(msg) + "\n")
        self.proc.stdin.flush()
        line = self.proc.stdout.readline()
        if not line:
            raise DuellError("the mock bridge stopped")
        return json.loads(line)

    def _send(self, frame: bytes) -> None:
        r = self._call({"op": "frame", "data": list(frame)})
        if r.get("error"):
            raise DuellError(f"mock: {r['error']}")
        self.q.extend(bytes(f) for f in r["frames"])

    def _recv(self, timeout: float) -> Optional[bytes]:
        return self.q.popleft() if self.q else None

    def control(self, action: str, **kw) -> dict:
        """A test hook of the bridge (state, drop, fail, playing, setPatch, readArchive, captureJs ...)."""
        r = self._call({"op": "control", "action": action, "args": kw})
        if not r.get("ok"):
            raise DuellError(f"mock control {action}: {r.get('error')}")
        return r

    def close(self) -> None:
        if self.proc.poll() is None:
            try:
                self.proc.stdin.close()
                self.proc.wait(timeout=5)
            except Exception:
                self.proc.kill()
        if self.proc.stdout:
            self.proc.stdout.close()


def _mido():
    """mido with python-rtmidi (the venv of jam/env.sh); imported late so --mock and the tests run without it."""
    try:
        import mido
        mido.set_backend("mido.backends.rtmidi", load=True)
        return mido
    except ImportError:
        raise DuellError("needs mido and python-rtmidi (. jam/env.sh puts the venv on PATH)")


def pair_output(name: str, outs: list[str]) -> Optional[str]:
    """The output of an input port; Windows numbers each port ("DUELL 0" / "DUELL 1"), so compare without the number (fm1_install.py)."""
    if name in outs:
        return name
    base = port_base(name)
    same = [o for o in outs if port_base(o) == base]
    if len(same) == 1:
        return same[0]
    return outs[0] if len(outs) == 1 else None


def port_base(name: str) -> str:
    """A port name without Windows' trailing number: what --port of fm1_install.py should match (it compares part of the name)."""
    return re.sub(r"\s+\d+$", "", name)


# ------------------------------------------------------------------------------------------------------------------ INFO
@dataclass
class Info:
    version: str
    engines: list[str]
    caps: Optional[int]
    patch_cap: Optional[int]
    p_count: int = 0
    tags: dict[int, list[int]] = field(default_factory=dict)

    @property
    def tide(self) -> bool:
        """The unit is DUELL with TIDE (INFO tag 44 01, bit 0); a plain DUELL unit sends none."""
        return bool(self.tags.get(0x44, [0])[0] & 1)

    @property
    def fv_bank(self) -> bool:
        """The unit can GET object 41, FM6's voice bank (INFO tag 44 01, bit 1)."""
        return bool(self.tags.get(0x44, [0])[0] & CAPS_FVBANK)

    @property
    def duell(self) -> Optional[str]:
        """The DUELL version number, or None when the unit runs something else (installer_state.js detectUnit)."""
        m = DUELL_RE.match(self.version.strip())
        return m[1] if m else None


def parse_info(a: Sequence[int]) -> Info:
    """INFO: version\\0, nEng, P_COUNT, G_COUNT, NSTEP, P_E0, the engine names, NTRK, song rows, then tags `tag 01 value...`."""
    bad = DuellError("The unit sent an unreadable INFO")
    a = list(a)
    if 0 not in a or a.index(0) < 1:
        raise bad
    pos = a.index(0)
    version, pos = "".join(map(chr, a[:pos])), pos + 1
    if pos + 5 > len(a):
        raise bad
    n_eng, p_count = a[pos], a[pos + 1]
    pos += 5
    engines = []
    for _ in range(n_eng):
        if 0 not in a[pos:]:
            raise bad
        end = a.index(0, pos)
        engines.append("".join(map(chr, a[pos:end])))
        pos = end + 1
    pos += 2
    tags: dict[int, list[int]] = {}
    while pos < len(a) and a[pos] in TAG_LEN and pos + TAG_LEN[a[pos]] <= len(a):
        if a[pos + 1] == 1:
            tags[a[pos]] = a[pos + 2:pos + TAG_LEN[a[pos]]]
        pos += TAG_LEN[a[pos]]
    caps = tags[0x42][0] if 0x42 in tags else None
    patch_cap = tags[0x58][0] | tags[0x58][1] << 7 if 0x58 in tags else None
    return Info(version, engines, caps, patch_cap, p_count, tags)


def read_info(t: Transport) -> Info:
    return parse_info(t.request(CMD_INFO, [], T_INFO))


def caps_text(caps: Optional[int]) -> str:
    if caps is None:
        return "none reported"
    bits = [(CAPS_MULTI_PATTERN, "multis+patterns"), (CAPS_WAVETABLES, "wavetables"), (CAPS_PATCH16, "128 patches"), (CAPS_PATCH_RECORDS, "patch records")]
    return f"{caps} ({', '.join(n for b, n in bits if caps & b) or 'backup only'})"


# ------------------------------------------------------------------------------------------------------------------ object checks (duellbackup.js checkObject)
def _layout_counts() -> tuple[int, int]:
    """P_COUNT and NENGINES of the firmware the editor was built from (layout.json); the record check needs them."""
    try:
        L = json.loads((BASE / "web" / "editor" / "layout.json").read_text(encoding="utf-8"))
        return int(L["P_COUNT"]), int(L["NENGINES"])
    except (OSError, ValueError, KeyError):
        return 127, 15


P_COUNT, NENGINES = _layout_counts()
MUSIC_P_COUNT = 127
PTF = {"PAT": 1, "TR": 2, "FM6": 4, "MB": 8, "XB": 16, "AK": 32, "BM": 64, "CB": 128}
XB_VER, XB_MAX = 1, 32
PS_REC, PS_PER, PS_SLOTS, PS_HDR, PS_OBJ_MAX = 596, 11, 32, 8, 3584
WT_OBJ, WT_HDR, WT_NAME = 12 + 19 * 64, 12, 8


def _u16(b: bytes, o: int) -> int:
    return b[o] | b[o + 1] << 8


def _u32le(b: bytes, o: int) -> int:
    return int.from_bytes(b[o:o + 4], "little")


def _printable0(b: bytes, at: int, n: int) -> bool:
    """duellbackup.js printable: every non-zero byte is plain ASCII (bytes past the end count as 0)."""
    return all(not (0 < x < 32 or x > 126) for x in b[at:at + n])


def is_patch_key(k: int) -> bool:
    return PATCH_KEY0 <= k < PATCH_KEY0 + PATCH_SLOTS


def group_key(k: int) -> Optional[str]:
    if is_patch_key(k):
        return "patches"
    for g in GROUPS:
        if k in g.ids:
            return g.key
    return None


_LAYOUT: dict = {}


def _layout_json() -> dict:
    if not _LAYOUT:
        _LAYOUT.update(json.loads((BASE / "web" / "editor" / "layout.json").read_text(encoding="utf-8")))
    return _LAYOUT


REC_VB, REC_NX, XW_BASE = 20, 28, 128
PTF_ALL = 175
MBV_HDR, MBV_FMN, PIN_BYTES, NFM = 22, 57, 20, 65
# the wide values End B never differs in (patchrecord.js NEVER, morph.c mbv_never)
MBV_NEVER = {0, 5, 21, 22, 23, 24, 29, 30, 31, 32, 40, 81, 82, 86, 94, 105, 111, 112, 115, 116, 117, 118, 149, 153, 154, 155}


def _wid_ok(w: int) -> bool:
    return 0 <= w < 127 or XW_BASE <= w < XW_BASE + REC_NX


def _wid_bits(eng: int, w: int) -> int:
    """patchrecord.js widBits: the bits of the stored range of wide id w (layout.json is written from the unit's own tables)."""
    L = _layout_json()
    if w == 127:
        r = {"min": 0, "max": 0}
    elif w >= XW_BASE:
        r = L["XTP"][w - XW_BASE]
    else:
        r = L["ENG"][eng]["edit"][w - 119] if w >= 119 else L["TP"][w]
    n, b = r["max"] - r["min"] + 1, 0
    while b < 16 and (1 << b) < n:
        b += 1
    return b


def _end_b_size(b: bytes, at: int, avail: int, eng: int) -> int:
    """patchrecord.js endBSize (morph.c morph_bv_size): the End B block's length at b[at:], 0 when it is no block."""
    fm6 = next((i for i, e in enumerate(_layout_json()["ENG"]) if e["name"] == "FM6"), -1)
    if avail < MBV_HDR or eng >= NENGINES or b[at] & ~7 or b[at + 1] > 101 or (b[at] & 6 and eng != fm6):
        return 0
    nb = 0
    for w in range(160):
        if b[at + 2 + (w >> 3)] >> (w & 7) & 1:
            if not _wid_ok(w) or w in MBV_NEVER:
                return 0
            nb += _wid_bits(eng, w)
    n = MBV_HDR + (nb + 7 >> 3)
    if b[at] & 1:
        if n + PIN_BYTES > avail:
            return 0
        if any(b[at + n + (w >> 3)] >> (w & 7) & 1 and not _wid_ok(w) for w in range(160)):
            return 0
        n += PIN_BYTES
    if b[at] & 2:
        if n + MBV_FMN > avail:
            return 0
        pos = (at + n) * 8
        for _ in range(NFM):
            v = sum((b[(pos + i) >> 3] >> ((pos + i) & 7) & 1) << i for i in range(7))
            pos += 7
            if v > 99:
                return 0
        n += MBV_FMN
    if b[at] & 4:
        if n + 3 > avail or not b[at + n]:
            return 0
        n += 3
    return n


def _record_layout(b: bytes) -> tuple[dict[str, int], int]:
    """Where a patch record's optional parts start and the total length its flags imply (patchrecord.js layout, upreset.c pt_layout_x; PT_VER 4)."""
    f, ln, eng = b[4], _u16(b, 0), b[5]
    o, at = REC_FIX, {}
    at["pr"] = o
    if o + REC_VB > ln or eng >= NENGINES:
        return at, o + 65536
    nb = sum(_wid_bits(eng, w) for w in range(REC_VB * 8) if b[o + (w >> 3)] >> (w & 7) & 1 and _wid_ok(w))
    o += REC_VB + (nb + 7 >> 3)
    at["mb"] = o
    if f & PTF["MB"]:
        n = _end_b_size(b, o, ln - o, eng) if o < ln else 0
        if not n:
            return at, o + 65536
        o += n
    o += 32 if f & PTF["PAT"] else 0
    o += 112 if f & PTF["FM6"] else 0
    if f & PTF["AK"]:
        if o + 2 > ln or o + 2 > len(b):
            return at, o + 65536
        o += 2 + _u16(b, o)
    return at, o


def _parse_record_problem(b: bytes) -> Optional[str]:
    """patchrecord.js parseRecord's refusals (the decoding itself is not needed here)."""
    if len(b) < REC_FIX + REC_VB or len(b) > REC_WORST:
        return "The record has the wrong size."
    if _u16(b, 0) != len(b):
        return "The record's length field does not match its size."
    if b[4] & ~PTF_ALL:
        return "The record has flags this editor does not know."
    if not b[4] & PTF["CB"]:
        return "The record is in the form of a firmware before 0.2.14, which no longer loads."
    if b[5] >= NENGINES:
        return "The record names an engine that does not exist."
    if not b[7]:
        return "The record has no name."
    at, total = _record_layout(b)
    if total != len(b):
        return "The record's parts do not add up to its length."
    ids = [w for w in range(REC_VB * 8) if b[at["pr"] + (w >> 3)] >> (w & 7) & 1]
    if any(not _wid_ok(w) for w in ids):
        return "The record stores a parameter that does not exist."
    if len(ids) != b[19]:
        return "The record's value count does not match its bitmap."
    return None


def record_problem(b: bytes) -> Optional[str]:
    """A patch record the unit would refuse at commit (pt_rec_ok / up_put_raw; patchrecord.js recordProblem), else None."""
    why = _parse_record_problem(b)
    if why:
        return why[:-1] if why.endswith(".") else why
    name = b[7:19]
    n = next((i for i, x in enumerate(name) if not x), 12)
    if any(x < 32 or x > 126 for x in name[:n]):
        return "the name has a character that is not printable"
    if any(name[n:]):
        return "the name is not padded with 0"
    return None


def _expand_step(s: bytes, at: int, end: int) -> Optional[tuple[bytearray, int]]:
    """One compact pattern step -> its 9 bytes (patslot.js expandStep), None for a step the unit refuses."""
    if at >= end:
        return None
    c0, b = s[at], bytearray(9)
    code = c0 >> 6
    b[4] = (c0 & 3) << 3 | ((c0 >> 2) & 3) << 5
    b[5], b[8] = ((c0 >> 4) & 1) << 7, ((c0 >> 5) & 1) << 7
    if code == 0:
        return b, 1
    if code == 1:
        if end - at < 3 or s[at + 1] > 127 or s[at + 2] > 127:
            return None
        b[0], b[4], b[5] = s[at + 1], b[4] | 1, b[5] | s[at + 2]
        return b, 3
    if code != 2 or end - at < 5:
        return None
    x = s[at + 1]
    c = x >> 3
    if (x & 7) > 4 or c > 4 or (x & 0xC0) or s[at + 2] > 127 or s[at + 3] > 101 or s[at + 4] > 15 or end - at < 5 + c:
        return None
    b[4] |= x & 7
    b[5] |= s[at + 2]
    b[8] |= s[at + 3]
    for j in range(c):
        if s[at + 5 + j] > 127:
            return None
        b[j] = s[at + 5 + j]
    for j in range(4):
        b[j] |= ((s[at + 4] >> j) & 1) << 7
    return b, 5 + c


def _pattern_rec_problem(r: bytes) -> Optional[str]:
    """A canonical 596-byte pattern record (patslot.js recordProblem)."""
    if len(r) != PS_REC or r[0] != 0xA7 or r[1] != 2:
        return "not a pattern record"
    ln, div, swing, gate, qnt = r[2:7]
    if ln < 1 or ln > 64 or div > 16 or swing > 100 or gate < 1 or gate > 127 or qnt > 1 or (r[7] & ~1):
        return "a setting out of range"
    n = next((i for i in range(12) if not r[8 + i]), 12)
    if any(r[8 + n:20]):
        return "a name with a gap"
    if any(x < 32 or x > 126 for x in r[8:8 + n]):
        return "an unprintable name"
    for s in range(64):
        t = 20 + s * 9
        if (r[t + 4] & 128) or (r[t + 4] & 7) > 4 or ((r[t + 4] >> 3) & 3) > 2 or (r[t + 8] & 127) > 101 or r[t + 6] or r[t + 7]:
            return "a damaged step"
    return None


def _unpack_pattern(s: bytes, at: int, end: int) -> Optional[int]:
    """The length of the compact pattern record at `at`, None when it is damaged (patslot.js unpackRecord)."""
    if end - at < 9:
        return None
    n = 2 + _u16(s, at)
    if end - at < n or n < 9:
        return None
    fl, nl = s[at + 2], s[at + 7]
    if nl > 12 or n < 9 + nl or fl > 3 or any(x < 32 or x > 126 for x in s[at + 8:at + 8 + nl]):
        return None
    nst = s[at + 8 + nl]
    if nst > 64:
        return None
    r = bytearray(PS_REC)
    r[0:8] = bytes([0xA7, 2, s[at + 3], s[at + 4], s[at + 5], s[at + 6], (fl >> 1) & 1, fl & 1])
    r[8:8 + nl] = s[at + 8:at + 8 + nl]
    for i in range(nst, 64):
        r[20 + i * 9 + 4] = 2 << 3
    p = at + 9 + nl
    for i in range(nst):
        x = _expand_step(s, p, at + n)
        if not x:
            return None
        r[20 + i * 9:29 + i * 9] = x[0]
        p += x[1]
    return n if p == at + n and not _pattern_rec_problem(bytes(r)) else None


def pattern_problem(o: int, b: bytes) -> Optional[str]:
    """A PTS3 pattern slot object o (0..2) as the unit holds it (patslot.js parsePatObject)."""
    if len(b) < PS_HDR or len(b) > PS_OBJ_MAX:
        return "not a pattern slot object"
    if b[0:4] != b"PTS3":
        return "a pattern slot object of an older firmware (16 slots)" if b[3] == 0x32 else "not a pattern slot object"
    if _u16(b, 4) != len(b) or b[6] != min(PS_PER, PS_SLOTS - o * PS_PER) or b[7] != PS_PER:
        return "not a pattern slot object"
    at = PS_HDR
    for _ in range(b[6]):
        n = _unpack_pattern(b, at, len(b))
        if n is None:
            return "damaged pattern slot"
        at += n
    return None if at == len(b) else "not a pattern slot object"


def wavetable_problem(b: bytes) -> Optional[str]:
    """A user wavetable object (wavetable.js checkObject, firmware wt_obj_ok)."""
    if len(b) != WT_OBJ:
        return "not a user wavetable"
    if b[0] != 1 or b[1] != 19 or b[2] != 64 or b[3] != 0:
        return "a user wavetable of another version"
    if not _printable0(b, 4, WT_NAME):
        return "a bad name"
    return None if any(b[WT_HDR:]) else "an empty table"


def multi_problem(b: bytes) -> Optional[str]:
    """A MUL1 object of two multis: the shape only (duellbackup.js 0.2.25), the version is planRestore's business."""
    n = len(b)
    if not n:
        return None
    rec_max = (MAX_OBJ - 8) // 2
    if n < 8 or _u32le(b, 0) != 0x314C554D:
        return "not a multi object"
    ln = [_u16(b, 4), _u16(b, 6)]
    if ln[0] > rec_max or ln[1] > rec_max or 8 + ln[0] + ln[1] != n:
        return "not a multi object"
    at = 8
    for h in range(2):
        if ln[h] and (ln[h] < 20 or not b[at] or b[at] > 15 or b[at + 1] < 8 or b[at + 1] > 127 or not _printable0(b, at + 8, 12)):
            return "damaged multi"
        at += ln[h]
    return None


def multi_versions(b: bytes) -> list[tuple[int, int]]:
    """(version, value count) of each stored multi record in an object."""
    if len(b) < 8:
        return []
    ln, out, at = [_u16(b, 4), _u16(b, 6)], [], 8
    for h in range(2):
        if ln[h] and at + 1 < len(b):
            out.append((b[at], b[at + 1]))
        at += ln[h]
    return out


def _music8_problem(b: bytes) -> Optional[str]:
    """Revision-8 framing from project.c proj_unpack; the unit still validates on PUT.

    End-B values are variable length (unlike revision 7). Check bounds before
    reading them, and the native FNV hash as well as the archive's outer CRC.
    Motion events can be sanitized by the unit, so do not reject old targets.
    """
    if _u32le(b, 4) != MAX_OBJ or b[64] != 2:
        return "damaged current music header"
    checksum = 0x811C9DC5
    for value in b[:-4]:
        checksum = ((checksum ^ value) * 16777619) & 0xFFFFFFFF
    if checksum != _u32le(b, MAX_OBJ - 4):
        return "damaged current music checksum"
    end = MAX_OBJ - 4 - 12 - 2 * 128  # fixed FM6 patch/name trailer
    pos = 68
    for _ in range(2):
        if pos + MUSIC_P_COUNT + 1 > end or any(v > 191 for v in b[pos:pos + MUSIC_P_COUNT]):
            return "damaged current music parameters"
        pos += MUSIC_P_COUNT
        nx = b[pos]
        pos += 1
        if pos + nx + 1 > end or any(v > 191 for v in b[pos:pos + nx]):
            return "damaged current music extensions"
        pos += nx
        nb = b[pos]
        pos += 1
        if nb > 198 or pos + nb + 2 + 64 * 9 > end:
            return "damaged current music End B"
        pos += nb
        if b[pos] >= NENGINES:
            return "damaged current music engine"
        pos += 2
        for _ in range(64):
            meta, chance = b[pos + 4], b[pos + 8] & 127
            if meta > 127 or (meta & 7) > 4 or ((meta >> 3) & 3) > 2 or chance > 101:
                return "damaged current music step"
            pos += 9
    if pos + 388 > end or b[pos] > 128 or b[pos + 1] & ~3:
        return "damaged current music motion"
    return None


def check_object(key: int, b: bytes) -> Optional[str]:
    """None when the object is acceptable for key (a patch: 1000 + slot), else a short reason (duellbackup.js checkObject)."""
    n = len(b)
    if n > MAX_OBJ:
        return "too large"
    if key == 0:
        if n != MAX_OBJ or _u32le(b, 0) != 0x46554E42:
            return "not the current music of a DUELL unit"
        if b[63] != 2 or b[66] != MUSIC_P_COUNT or b[67] not in (7, 8):
            return "the music of an older DUELL version, which kept four parts and a song chain; make a new backup with this version"
        return _music8_problem(b) if b[67] == 8 else None
    if key == 1:
        return None if n >= 64 and _u32le(b, 0) == 0x50455234 else "not a settings record"
    if is_patch_key(key):
        if n < REC_FIX or n > REC_WORST:
            return "not a patch record (its size)"
        if b[2] or b[3]:
            return "not a patch record as it is backed up (the slot and revision bytes are not 0)"
        why = record_problem(b)
        if why and b[4] & (16 | 64):
            return "a patch record of an older DUELL version, which this firmware does not load; make a new backup with this version"   # (PT_VER 3: the extension block and bitmap flags, 16 and 64)
        return f"damaged patch record ({why})" if why else None
    if key == FVB_ID:
        if n != FVB_OBJ or _u32le(b, 0) != 0x56364D46 or _u16(b, 4) != 1 or _u16(b, 6) != FVB_N or not _printable0(b, 12, 12):
            return "not an FM6 voice bank"
        return None if crc32(b[:FVB_LEN]) == _u32le(b, FVB_LEN) else "the voice bank failed its checksum"
    g = group_key(key)
    if g == "multis":
        return multi_problem(b)
    if g == "patterns":
        return pattern_problem(key - 21, b) if n else None
    if g == "wavetables":
        why = wavetable_problem(b) if n else None
        return f"not a user wavetable ({why})" if why else None
    return "not an object of a DUELL backup"


def object_label(key: int, b: Optional[bytes] = None) -> str:
    """The short name the web page uses in its messages ("patch #06 BELL", "multis M03 and M04")."""
    if is_patch_key(key):
        nm = "" if not b or len(b) < REC_FIX or not b[7] else " " + b[7:19].split(b"\0")[0].decode("latin-1")
        return f"patch #{key - PATCH_KEY0 + 1:02d}{nm}"
    if key == 0:
        return "the current music"
    if key == 1:
        return "the settings"
    if 13 <= key <= 20:
        a = 2 * (key - 13) + 1
        return f"multis M{a:02d} and M{a + 1:02d}"
    if 21 <= key <= 23:
        a = 11 * (key - 21) + 1
        return f"pattern slots P{a:02d} to P{min(32, a + 10):02d}"
    if 24 <= key <= 26:
        return f"user wavetable {key - 23}"
    if key == FVB_ID:
        return "the FM6 voice bank"
    return "a chunk of the old patch store" if key in OLD_PATCH_IDS else f"object {key}"


# ------------------------------------------------------------------------------------------------------------------ the file
@dataclass
class Group:
    key: str
    label: str
    ids: list[int]
    caps: int
    dynamic: bool = False
    optional: bool = False                               # a file without it is no exclusion to report (the voice bank)


GROUPS = [Group("patches", "Patches", [], CAPS_PATCH_RECORDS, True), Group("multis", "Multis", list(range(13, 21)), CAPS_MULTI_PATTERN),
          Group("patterns", "Pattern slots", [21, 22, 23], CAPS_MULTI_PATTERN), Group("wavetables", "User wavetables", [24, 25, 26], CAPS_WAVETABLES),
          Group("fm6bank", "FM6 voice bank (B1 to B32)", [FVB_ID], 0, False, True),
          Group("settings", "Settings", [1], 0), Group("music", "Current music (both parts)", [0], 0)]


@dataclass
class Obj:
    """One object of a backup; key is the unit's id, or 1000 + slot for a patch record."""
    key: int
    data: bytes
    raw: bool = False
    problem: Optional[str] = None

    @property
    def size(self) -> int:
        return len(self.data)

    @property
    def crc(self) -> int:
        return crc32(self.data)


@dataclass
class Archive:
    version: int
    firmware: str
    created: str
    engines: list[str]
    objects: dict[int, Obj]
    problems: list[tuple[int, str]]
    notes: list[str]
    has_patches: bool
    old_patch_chunks: int


def _wire(key: int) -> dict:
    return {"id": PATCH_ID, "slot": key - PATCH_KEY0} if is_patch_key(key) else {"id": key}


def iso_now() -> str:
    """new Date().toISOString(): UTC with milliseconds and Z."""
    d = datetime.now(timezone.utc)
    return d.strftime("%Y-%m-%dT%H:%M:%S.") + f"{d.microsecond // 1000:03d}Z"


def objects_to_file(objects: list[Obj], info: Info, patch_records: Optional[bool] = None, notes: Optional[list[str]] = None,
                    created: Optional[str] = None) -> dict:
    """The file as duellbackup.js objectsToFile builds it; the key order is part of byte compatibility (JSON.stringify keeps it)."""
    ordered = [o for o in objects if not is_patch_key(o.key)] + sorted((o for o in objects if is_patch_key(o.key)), key=lambda o: o.key)
    problems = [{**_wire(o.key), "problem": o.problem or "not checked"} for o in ordered if o.raw]
    f: dict = {"format": FORMAT, "version": VERSION, "firmware": info.version, "created": created or iso_now(), "engines": info.engines,
               "patchRecords": patch_records if patch_records is not None else any(is_patch_key(o.key) for o in objects)}
    if notes:
        f["notes"] = notes
    if problems:
        f["problems"] = problems
    f["objects"] = [{**_wire(o.key), "size": o.size, "crc": o.crc, "data": base64.b64encode(o.data).decode("ascii"),
                     **({"raw": True, "problem": o.problem or "not checked"} if o.raw else {})} for o in ordered]
    return f


def file_text(f: dict) -> str:
    """JSON.stringify(file): compact, no newline at the end."""
    return json.dumps(f, separators=(",", ":"), ensure_ascii=False)


B64_RE = re.compile(r"^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$")


def _is_int(x) -> bool:
    """Number.isInteger: JSON numbers, not booleans."""
    return (isinstance(x, int) and not isinstance(x, bool)) or (isinstance(x, float) and x.is_integer())


def _decode_item(o: dict, name: str) -> bytes:
    """An object's bytes from its base64, with readArchive's size, CRC and form checks."""
    size, crc, data = o.get("size"), o.get("crc"), o.get("data")
    if not (_is_int(size) and 0 <= size <= MAX_OBJ and _is_int(crc) and 0 <= crc <= 0xFFFFFFFF and isinstance(data, str)
            and len(data) == 4 * -(-int(size) // 3) and B64_RE.match(data)):
        raise DuellError(f"{name} is damaged.")
    b = base64.b64decode(data)
    if len(b) != size or crc32(b) != crc:
        raise DuellError(f"{name} failed its checksum: the file is damaged.")
    return b


def _check_versions(version: int, items: list) -> None:
    ids = [o.get("id") for o in items if isinstance(o, dict)]
    if version < 4 and any(i in (27, 28, 29, 30, 31, 35, 36, 37, 38, 39) for i in ids):
        raise DuellError("This backup holds patch chunks a version 3 file does not have: it is damaged.")
    if version >= 5 and any(i in OLD_PATCH_IDS for i in ids):
        raise DuellError(f"This backup is version {version}, which carries patches as records, but it holds patch chunks (ids 6, 7, 9 to 12, 27 to 31, 35 to 39): it is damaged.")
    if version < 5 and PATCH_ID in ids:
        raise DuellError(f"This backup is version {version}, which has no patch records (id {PATCH_ID}): it is damaged.")


def read_archive(file) -> Archive:
    """Parse and check a whole file before anything is sent (duellbackup.js readArchive). The installer's "felucca-backup" v1 files are not read
    here (the web Backup page reads them)."""
    if isinstance(file, (str, bytes)):
        try:
            file = json.loads(file)
        except ValueError:
            raise DuellError("This is not a backup file (it is not valid JSON).")
    if not isinstance(file, dict) or not isinstance(file.get("objects"), list):
        raise DuellError("This is not a DUELL backup file.")
    if file.get("format") == "felucca-backup":
        raise DuellError("This is the installer's felucca-backup file: restore it with the web editor's Backup page.")
    if file.get("format") != FORMAT:
        raise DuellError("This is not a DUELL backup file.")
    version = file.get("version")
    if not _is_int(version) or version < 2:
        raise DuellError("This backup has an unknown version.")
    if version > VERSION:
        raise DuellError(f"This backup is version {version}; this editor reads up to version {VERSION}. Use a newer editor.")
    items = file["objects"]
    _check_versions(version, items)
    objects, old = _read_items(items)
    _check_groups(objects)
    keys = list(objects)
    return Archive(int(version), str(file.get("firmware") or ""), str(file.get("created") or ""),
                   [str(e) for e in file.get("engines", [])] if isinstance(file.get("engines"), list) else [], objects,
                   [(o.key, o.problem or "") for o in objects.values() if o.raw],
                   [*([str(n) for n in file["notes"]] if isinstance(file.get("notes"), list) else []), *([OLD_CHUNKS_NOTE] if old else [])],
                   version >= 5 and (file.get("patchRecords") is True or any(is_patch_key(k) for k in keys)), old)


def _read_items(items: list) -> tuple[dict[int, Obj], int]:
    """The objects of a file by key; the old patch chunks are counted and left out, retired ids must be empty."""
    objects: dict[int, Obj] = {}
    old = 0
    for o in items:
        if not isinstance(o, dict) or not _is_int(o.get("id")):
            raise DuellError("A backup object has no id.")
        oid, patch = int(o["id"]), o["id"] == PATCH_ID
        slot = o.get("slot")
        if patch and not (_is_int(slot) and 0 <= slot < PATCH_SLOTS):
            raise DuellError("A patch object has no valid slot.")
        key = PATCH_KEY0 + int(slot) if patch else oid
        name = f"Patch {object_label(key)[6:]}" if patch else f"Object {oid}"
        if not patch and oid not in KNOWN_IDS:
            raise DuellError(f"Unknown object id {oid} in this backup.")
        is_old = oid in OLD_PATCH_IDS
        if not patch and not is_old and oid not in DUELL_IDS and oid != FVB_ID:
            if o.get("size"):
                raise DuellError(f"Object {oid} is retired in DUELL and holds data: this is not a DUELL backup.")
            continue
        if key in objects:
            raise DuellError(f"{name} is in the file twice." if patch else f"Object {oid} is in the file twice.")
        b = _decode_item(o, name)
        if is_old:
            old += 1 if b else 0
            continue
        if o.get("raw") is True:
            if patch and len(b) > REC_WORST:
                raise DuellError(f"{name} is damaged.")
            objects[key] = Obj(key, b, True, str(o.get("problem") or "not checked"))
            continue
        why = check_object(key, b)
        if why:
            raise DuellError(f"{name}: {why}.")
        objects[key] = Obj(key, b)
    return objects, old


def _check_groups(objects: dict[int, Obj]) -> None:
    """A group is whole or absent; the music and the settings must be there."""
    for g in GROUPS[1:]:
        n = sum(1 for i in g.ids if i in objects)
        if n and n != len(g.ids):
            raise DuellError(f"The {g.label.lower()} in this file are incomplete.")
    m, s = objects.get(0), objects.get(1)
    if not m or not s or (not m.size and not m.raw) or (not s.size and not s.raw):
        raise DuellError("This backup has no current music or no settings: it is not complete.")


def raw_lines(a: Archive) -> list[str]:
    """The lines that tell which objects are in a file only as raw data (duellbackup.js rawLines)."""
    return [f"Object {k} ({object_label(k, a.objects[k].data)}) could not be checked: {p}. It is saved as raw data; a restore skips it unless you ask for it."
            for k, p in a.problems]


# ------------------------------------------------------------------------------------------------------------------ the unit: list, capture
@dataclass
class Listed:
    size: int
    crc: int


def parse_list(a: list[int]) -> dict[int, Listed]:
    """The plain LIST: 1, rc, count, then per object id, size u32, crc u32; only the DUELL objects other than the patches are kept."""
    if not a or a[0] != 1:
        raise DuellError("Unexpected answer from the unit")
    if len(a) < 3:
        raise DuellError("The unit sent an incomplete list")
    rc_check(a[1])
    n = a[2]
    if len(a) != 3 + n * 11:
        raise DuellError("The unit sent an incomplete list")
    out: dict[int, Listed] = {}
    for i in range(n):
        p = 3 + i * 11
        oid, size, crc = a[p], r32(a, p + 1), r32(a, p + 6)
        if oid not in KNOWN_IDS:
            raise DuellError(f"The unit lists an object ({oid}) this editor does not know. Use a newer editor.")
        if size > MAX_OBJ or (not size and crc):
            raise DuellError("The unit lists an object that is too large")
        if oid in DUELL_IDS:
            out[oid] = Listed(size, crc)
    return out


def parse_page(a: list[int], page: int) -> dict[int, Listed]:
    """A LIST page (1..6): the saved patches of 32 slots, in slot order, under their keys (1000 + slot). The slot byte is 7 bits: pages 5 and 6
    (slots 128..191, JAM bank) give bit 7."""
    if not a or a[0] != 1:
        raise DuellError("Unexpected answer from the unit")
    if len(a) < 3:
        raise DuellError("The unit sent an incomplete list of patches")
    rc_check(a[1])
    n = a[2]
    if n > PATCH_PAGE or len(a) != 3 + n * 11:
        raise DuellError("The unit sent an incomplete list of patches")
    out: dict[int, Listed] = {}
    last = -1
    for i in range(n):
        p = 3 + i * 11
        slot, size, crc = a[p] + (128 if page > PATCH_PAGES_OLD else 0), r32(a, p + 1), r32(a, p + 6)
        if slot < PATCH_PAGE * (page - 1) or slot >= PATCH_PAGE * page or slot <= last:
            raise DuellError(f"The unit lists a patch (slot {slot}) that does not belong on page {page}.")
        if size < REC_FIX or size > REC_WORST:
            raise DuellError("The unit lists a patch record of a size this editor does not know. Use a newer editor.")
        out[PATCH_KEY0 + slot] = Listed(size, crc)
        last = slot
    return out


def list_unit(t: Transport, caps: Optional[int]) -> dict[int, Listed]:
    """The plain LIST (it stops the transport and takes the snapshot), then with caps bit 5 the patch pages, merged by key: four, and pages 5 and 6
    on a unit with 192 slots (JAM bank; a unit with 128 answers page 5 with rc 1: its list ends there)."""
    out = parse_list(t.request(CMD_LIST, [], T_LIST))
    if (caps or 0) & CAPS_PATCH_RECORDS:
        for page in range(1, PATCH_PAGES + 1):
            a = t.request(CMD_LIST, [page], T_LIST)
            if page > PATCH_PAGES_OLD and len(a) >= 2 and a[0] == 1 and a[1] == 1:
                break
            out.update(parse_page(a, page))
    return out


Progress = Callable[[int, int], None]


def _get_piece(t: Transport, key: int, off: int, n: int) -> bytes:
    """One GET of up to 256 bytes; asked once more after a lost reply (a GET changes nothing on the unit)."""
    patch = is_patch_key(key)
    wire = PATCH_ID if patch else key
    args = [wire, *u32(off), n & 127, n >> 7, *(slot_bytes(key - PATCH_KEY0) if patch else [])]
    try:
        a = t.request(CMD_GET, args, T_GET)
    except TransportTimeout:
        a = t.request(CMD_GET, args, T_GET)
    if len(a) < 2:
        raise DuellError("Unexpected answer from the unit")
    if a[1] == 3:
        raise DuellError(RC_NOTES[3], rc=3, fatal=True)
    rc_check(a[1])
    if len(a) < 9 or a[0] != wire or r32(a, 2) != off or (a[7] | a[8] << 7) != n:
        raise DuellError("Unexpected answer from the unit")
    return unpack7(a[9:], n)


def _read_object(t: Transport, key: int, size: int, tick: Callable[[int], None]) -> bytes:
    out = bytearray()
    for off in range(0, size, CHUNK):
        piece = _get_piece(t, key, off, min(CHUNK, size - off))
        out += piece
        tick(len(piece))
    return bytes(out)


def capture_backup(t: Transport, info: Info, progress: Optional[Progress] = None, created: Optional[str] = None) -> dict:
    """Read everything the unit lists into a file (duellbackup.js captureBackup). An object that cannot be read whole, changes meanwhile or fails
    the check is kept raw with its problem; only a refused LIST, rc 3 (playing) or nothing readable at all stops the backup."""
    listed = list_unit(t, info.caps)
    if 0 not in listed or 1 not in listed:
        raise DuellError("The unit does not list its music and settings: is it running DUELL?")
    keys = [i for i in DUELL_IDS if i in listed] + sorted(k for k in listed if is_patch_key(k))
    if info.fv_bank and _has_bank(t):                    # not in the plain LIST; its own CRC is its last four bytes
        listed[FVB_ID] = Listed(FVB_OBJ, 0)
        keys.insert(keys.index(0), FVB_ID)
    total, done = sum(listed[k].size for k in keys), [0]

    def tick(n: int) -> None:
        done[0] += n
        if progress:
            progress(min(done[0], total), total)
    got = [_capture_one(t, k, listed[k], tick) for k in keys]
    if all(o.raw for o in got):
        raise DuellError(f"Nothing could be read from the unit: {got[0].problem if got else 'no answer'}.")
    records = bool((info.caps or 0) & CAPS_PATCH_RECORDS)
    f = objects_to_file(got, info, records, [] if records else [NO_RECORDS_NOTE], created)
    read_archive(f)                                      # what was read passes the checks a restore applies (raw objects as they are)
    return f


def _has_bank(t: Transport) -> bool:
    """GET 41, one byte (after the plain LIST): does the unit hold an FM6 voice bank? rc 1 = none; a playing unit stops the backup."""
    a = t.request(CMD_GET, [FVB_ID, *u32(0), 1, 0], T_GET)
    if len(a) < 2:
        raise DuellError("Unexpected answer from the unit")
    if a[1] == 3:
        raise DuellError(RC_NOTES[3], rc=3, fatal=True)
    return a[1] == 0


def _read_bank(t: Transport) -> Optional[bytes]:
    """The unit's voice bank as GET 41 gives it (None: it holds none)."""
    return _read_object(t, FVB_ID, FVB_OBJ, lambda n: None) if _has_bank(t) else None


def _capture_one(t: Transport, key: int, want: Listed, tick: Callable[[int], None]) -> Obj:
    try:
        b = _read_object(t, key, want.size, tick)
    except DuellError as e:
        if e.fatal:
            raise
        return Obj(key, b"", True, f"it could not be read ({e})")
    if key != FVB_ID and crc32(b) != want.crc:
        return Obj(key, b, True, "it changed on the unit while it was read (checksum)")
    why = check_object(key, b)
    return Obj(key, b, True, f"this editor does not recognise it ({why})") if why else Obj(key, b)


# ------------------------------------------------------------------------------------------------------------------ restore
@dataclass
class Step:
    key: int                                             # the object id, or 1000 + slot for a patch
    data: bytes
    group: str

    @property
    def patch(self) -> bool:
        return is_patch_key(self.key)


@dataclass
class Plan:
    steps: list[Step]
    excluded: list[tuple[str, str]]                      # (label, reason)
    skipped: list[int]
    caps: Optional[int]


def _group_reason(g: Group, a: Archive, listed: dict[int, Listed], caps: int, patch_cap: Optional[int] = None, fv_bank: bool = False) -> Optional[str]:
    """Why a whole group cannot be restored (duellbackup.js planRestore), None when it can."""
    if g.key == "patches":
        if not a.has_patches:
            return "patches saved by an older firmware (physical chunks) are in this file but cannot be restored" if a.old_patch_chunks else "not in this file"
        n = sum(1 for k in a.objects if is_patch_key(k))
        if not caps & g.caps:
            return "this unit's firmware cannot restore patches one at a time (update it first)"
        if patch_cap and n > patch_cap:
            return f"the file holds {n} patches and this unit stores at most {patch_cap} (STORE FULL); remove some from the file or restore them later in parts"
        return None
    if g.caps and (caps & g.caps) != g.caps:
        return "this unit's firmware cannot restore them (update it first)"
    if not all(i in a.objects for i in g.ids):
        return "not in this file"
    if g.key == "fm6bank" and not fv_bank:
        return "this unit's firmware cannot restore it (it needs DUELL with TIDE, a build with the FM6 voice bank in its backup)"
    if g.key in ("settings", "music") and (listed.get(g.ids[0]) or Listed(-1, 0)).size != a.objects[g.ids[0]].size:
        return "the file was made by another DUELL version (its size differs from the unit's)"
    return None


def _patch_steps(a: Archive, listed: dict[int, Listed], include_raw: bool, plan: Plan) -> None:
    """The patches made exactly the file's: a PUT where size or CRC differ, a size-0 PUT for a saved slot the file lacks."""
    for key in sorted(k for k in a.objects if is_patch_key(k)):
        o = a.objects[key]
        if o.raw and (not include_raw or not o.size):
            plan.excluded.append((f"Patches: {object_label(key, o.data)}", f"it could not be checked when the backup was made ({o.problem}); the unit keeps what it holds there"))
            continue
        now = listed.get(key)
        if now and now.size == o.size and now.crc == o.crc:
            plan.skipped.append(key)
            continue
        plan.steps.append(Step(key, o.data, "patches"))
    for key in listed:
        if is_patch_key(key) and key not in a.objects:
            plan.steps.append(Step(key, b"", "patches"))


def _object_steps(g: Group, a: Archive, include_raw: bool, plan: Plan) -> None:
    for i in g.ids:
        o = a.objects[i]
        label = f"{g.label}: object {i} ({object_label(i)})"
        if o.raw and not include_raw:
            plan.excluded.append((label, f"it could not be checked when the backup was made ({o.problem}); the unit keeps what it holds there"))
            continue
        old = next((v for v in multi_versions(o.data) if v != (8, MUSIC_P_COUNT)), None) if g.key == "multis" and not o.raw else None   # (MU_VER 8 since 1.1)
        if old and not include_raw:
            plan.excluded.append((label, f"a multi record of version {old[0]}, which this firmware does not load; the unit keeps what it holds there"))
            continue
        plan.steps.append(Step(i, o.data, g.key))


def plan_restore(a: Archive, listed: dict[int, Listed], caps: Optional[int], include_raw: bool = False, sel: Optional[set[str]] = None,
                 patch_cap: Optional[int] = None, fv_bank: bool = False) -> Plan:
    """What a restore would send, nothing sent: the removals of saved patches the file lacks first (they free room), the patches by slot, then 13..20,
    21..23, 24..26, 1, and the live music last. patch_cap (INFO 58 01): a file with more patches than that leaves the patches out with the reason (D05)."""
    plan, c = Plan([], [], [], caps), caps or 0
    for g in GROUPS:
        if (sel is not None and g.key not in sel) or (g.optional and not all(i in a.objects for i in g.ids)):
            continue
        why = _group_reason(g, a, listed, c, patch_cap, fv_bank)
        if why:
            plan.excluded.append((g.label, why))
        elif g.dynamic:
            _patch_steps(a, listed, include_raw, plan)
        else:
            _object_steps(g, a, include_raw, plan)
    plan.steps.sort(key=lambda s: (s.key - PATCH_KEY0 if s.data else s.key - PATCH_KEY0 - 200) if s.patch else 200 + (RESTORE_ORDER.index(1) - 0.5 if s.key == FVB_ID else RESTORE_ORDER.index(s.key)))   # (the voice bank just before the settings)
    return plan


@dataclass
class RestoreResult:
    written: list[int]
    failed: Optional[tuple[int, str, Optional[int]]]     # the first failure: (key, message, rc)
    failures: list[tuple[int, str, Optional[int]]] = field(default_factory=list)   # every failure, in the order tried (D05)
    stopped: bool = False                                # the run was cut short: the unit is playing (rc 3) or did not answer (nothing more can be written)


def _put(t: Transport, args: list[int]) -> None:
    """One PUT (6 s, never repeated: a repeated data piece would be refused for its offset or, worse, written twice)."""
    a = t.request(CMD_PUT, args, T_PUT)
    if len(a) < 3:
        raise DuellError("Unexpected answer from the unit")
    rc_check(a[2])


def _put_object(t: Transport, s: Step, tick: Callable[[int], None]) -> None:
    """begin, the 256-byte pieces in order, commit; an abort when anything after the begin fails (the unit keeps its old copy)."""
    wire = PATCH_ID if s.patch else s.key
    _put(t, [0, wire, *u32(len(s.data)), *u32(crc32(s.data)), *(slot_bytes(s.key - PATCH_KEY0) if s.patch else [])])
    try:
        for off in range(0, len(s.data), CHUNK):
            piece = s.data[off:off + CHUNK]
            _put(t, [1, wire, *u32(off), *pack7(piece)])
            tick(len(piece))
        _put(t, [2, wire])
    except DuellError:
        try:
            t.request(CMD_PUT, [3, wire], T_PUT)
        except DuellError:
            pass
        raise


def unpack_voice(b: bytes, at: int) -> list[int]:
    """112 stored bytes -> a record's 128 7-bit bytes (upf_pk_get)."""
    out, acc, n, o = [], 0, 0, at
    for _ in range(FVB_PK):
        while n < 7:
            acc |= b[o] << n
            o += 1
            n += 8
        out.append(acc & 127)
        acc >>= 7
        n -= 7
    return out


def _restore_bank(t: Transport, data: bytes, tick: Callable[[int], None]) -> None:
    """FM6B_BEGIN, one WRITE a used voice (index + the 128 bytes), END with the name and the CRC-32 of the records in index order (the unit's own bank upload)."""
    used = _u32le(data, 8)
    recs = [(k, unpack_voice(data, FVB_HEAD + k * FVB_REC)) for k in range(FVB_N) if used >> k & 1]
    a = t.request(CMD_FM6B_BEGIN, [], T_PUT)
    rc_check(a[0] if a else 1)
    for k, pk in recs:
        a = t.request(CMD_FM6B_WRITE, [k, *pk], T_PUT)
        if len(a) < 2 or a[0] != k:
            raise DuellError("Unexpected answer from the unit")
        rc_check(a[1])
    name = data[12:22].decode("latin-1")
    a = t.request(CMD_FM6B_END, [*(ord(c) & 127 for c in name.ljust(10)[:10]), *u32(zlib.crc32(b"".join(bytes(pk) for _, pk in recs)) & 0xFFFFFFFF)], T_PUT)
    rc_check(a[0] if a else 1)
    tick(len(data))


def run_restore(t: Transport, plan: Plan, progress: Optional[Progress] = None) -> RestoreResult:
    """Send the steps one object at a time. An object that fails (a full store, a flash error) is reported and the others still go; only "the unit is
    playing" (rc 3) or no answer stops the run. Says what was written and every failure (duellbackup.js runRestore)."""
    total, done, written, failures, stopped = sum(len(s.data) for s in plan.steps), [0], [], [], False

    def tick(n: int) -> None:
        done[0] += n
        if progress:
            progress(done[0], total)
    for s in plan.steps:
        try:
            if s.key == FVB_ID:                          # the voice bank goes in through the bank upload commands, not through PUT
                _restore_bank(t, s.data, tick)
            else:
                _put_object(t, s, tick)
        except DuellError as e:
            failures.append((s.key, str(e), e.rc))
            if e.rc is None or e.rc == 3:
                stopped = True
                break
            continue
        written.append(s.key)
    return RestoreResult(written, failures[0] if failures else None, failures, stopped)


@dataclass
class Verify:
    same: list[int]
    differ: list[int]
    not_compared: list[int]


def verify_restore(t: Transport, plan: Plan) -> Verify:
    """A fresh LIST: patches, multis, patterns, wavetables must equal the file (a removed patch must be gone); the music and the settings are checked by
    the unit at commit and may read back in its own normal form, so they are not compared."""
    listed, v = list_unit(t, plan.caps), Verify([], [], [])
    for s in plan.steps:
        if s.key in (0, 1):
            v.not_compared.append(s.key)
            continue
        if s.key == FVB_ID:
            try:
                ok = _read_bank(t) == s.data
            except DuellError:
                ok = False
            (v.same if ok else v.differ).append(s.key)
            continue
        now = listed.get(s.key)
        ok = now is None if s.patch and not s.data else bool(now and now.size == len(s.data) and now.crc == crc32(s.data))
        (v.same if ok else v.differ).append(s.key)
    return v


# ------------------------------------------------------------------------------------------------------------------ connecting
@dataclass
class Conn:
    t: Transport
    info: Info


Opener = Callable[[Optional[str]], Conn]


def probe(t: Transport) -> Info:
    """INFO, and only a DUELL unit is accepted: every other firmware's objects mean other things."""
    try:
        info = read_info(t)
    except DuellError as e:
        raise UnitNotFound(f"{t.port_name}: no INFO answer ({e})")
    if not info.duell:
        raise UnitNotFound(f"{t.port_name} does not run DUELL (it reports {info.version!r}). Nothing was changed.")
    return info


def open_midi(port: Optional[str]) -> Conn:
    """The first DUELL port pair (or the ones --port names, by part of the name) that answers INFO as DUELL."""
    mido = _mido()
    ins, outs = list(dict.fromkeys(mido.get_input_names())), list(dict.fromkeys(mido.get_output_names()))
    cands = [n for n in ins if port.lower() in n.lower()] if port else [n for n in ins if PORT_RE.search(n)]
    why = []
    for name in cands:
        out = pair_output(name, outs)
        if not out:
            why.append(f"{name}: no matching output")
            continue
        try:
            t = MidiTransport(name, out)
        except Exception as e:
            why.append(f"{name}: cannot open ({e}; another program using it?)")
            continue
        try:
            return Conn(t, probe(t))
        except UnitNotFound as e:
            why.append(str(e))
            t.close()
    raise UnitNotFound("no DUELL unit found" + (f" on a port matching {port!r}" if port else "") + " (USB data cable? the web editor or another app using it?)"
                       + "".join(f"\n  {w}" for w in why) + f"\n  MIDI inputs: {', '.join(ins) or '(none)'}")


def open_mock(port: Optional[str]) -> Conn:
    t = MockTransport()
    try:
        return Conn(t, probe(t))
    except Exception:
        t.close()
        raise


# ------------------------------------------------------------------------------------------------------------------ CLI
def unique_path(out_dir: Path, prefix: str) -> Path:
    """<prefix>-YYYY-MM-DD-HHMM.json (local time, as the web page names it); -2, -3 .. when that minute's file exists already."""
    out_dir.mkdir(parents=True, exist_ok=True)
    stem = f"{prefix}-{datetime.now().strftime('%Y-%m-%d-%H%M')}"
    p, k = out_dir / f"{stem}.json", 2
    while p.exists():
        p, k = out_dir / f"{stem}-{k}.json", k + 1
    return p


def save_file(f: dict, out_dir: Path, prefix: str) -> Path:
    p = unique_path(out_dir, prefix)
    with open(p, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(file_text(f))
    return p


def progress_printer(what: str) -> Optional[Progress]:
    """A percentage on stderr, only on a terminal (logs and tests stay clean)."""
    if not sys.stderr.isatty():
        return None

    def show(done: int, total: int) -> None:
        print(f"\r{what} {done * 100 // max(1, total):3d}%", end="\n" if done >= total else "", file=sys.stderr, flush=True)
    return show


def describe(a: Archive) -> str:
    """One line of what a file holds (the web page's summary)."""
    patches = sum(1 for k in a.objects if is_patch_key(k))
    has = lambda ids: any(i in a.objects and a.objects[i].size for i in ids)
    parts = [f"{patches} saved patch{'es' if patches != 1 else ''}" if a.has_patches else "no patches"]
    parts += [n for ids, n in ((range(13, 21), "multis"), ((21, 22, 23), "pattern slots"), ((24, 25, 26), "user wavetables"), ((FVB_ID,), "the FM6 voice bank")) if has(ids)]
    return ", ".join(parts + ["settings", "the current music"])


def cmd_info(a, conn: Conn, out, ask) -> int:
    i = conn.info
    print(f"DUELL {i.duell}{' with TIDE' if i.tide else ''}  ({i.version})", file=out)
    print(f"port:     {conn.t.port_name}", file=out)
    print(f"caps:     {caps_text(i.caps)}", file=out)
    print(f"patches:  {i.patch_cap if i.patch_cap is not None else 'unknown'} slots", file=out)
    named = [e for e in i.engines if e != "-"]
    print(f"engines:  {len(named)} ({', '.join(named)}; {len(i.engines)} engine slots)", file=out)
    return EXIT_OK


def do_backup(conn: Conn, out_dir: Path, prefix: str, out) -> tuple[Path, Archive]:
    f = capture_backup(conn.t, conn.info, progress_printer("backup"))
    p = save_file(f, out_dir, prefix)
    arch = read_archive(f)
    print(f"saved {p}: {describe(arch)}", file=out)
    for line in raw_lines(arch) + arch.notes:
        print(f"  warning: {line}", file=out)
    return p, arch


def cmd_backup(a, conn: Conn, out, ask) -> int:
    do_backup(conn, Path(a.out), "DUELL-backup", out)
    return EXIT_OK


def load_archive(path: str) -> Archive:
    p = Path(path)
    if not p.is_file():
        raise UsageError(f"no such file: {path}")
    if p.stat().st_size > 200000:
        raise DuellError("This file is too large to be a DUELL backup.")
    return read_archive(p.read_text(encoding="utf-8-sig"))


def plan_text(plan: Plan, unit: str) -> str:
    kinds = list(dict.fromkeys(next(g.label.lower() for g in GROUPS if g.key == s.group) for s in plan.steps))
    pw = [s for s in plan.steps if s.patch]
    put = sum(1 for s in pw if s.data)
    lines = [f"Replace on {unit}: {', '.join(kinds)}."]
    if pw or plan.skipped:
        lines.append(f"Patches: {put} written, {len(pw) - put} saved on the unit but not in the file removed, {len(plan.skipped)} already identical.")
    lines += [f"Not restored: {lab.lower()} ({why})" for lab, why in plan.excluded]
    return "\n".join(lines)


def name_of(key: int) -> str:
    return object_label(key) if is_patch_key(key) else f"object {key}"


def cmd_restore(a, conn: Conn, out, ask) -> int:
    arch = a.archive
    unit = conn.info.version.removeprefix("FELUCCA ")
    print(f"file: a backup of {arch.firmware.removeprefix('FELUCCA ') or 'a DUELL unit'}, made {arch.created or 'an unknown date'}: {describe(arch)}", file=out)
    for line in raw_lines(arch) + arch.notes:
        print(f"  warning: {line}", file=out)
    plan = plan_restore(arch, list_unit(conn.t, conn.info.caps), conn.info.caps, a.include_raw, patch_cap=conn.info.patch_cap, fv_bank=conn.info.fv_bank)
    if not plan.steps:
        print("Nothing to restore.", file=out)
        for lab, why in plan.excluded:
            print(f"  {lab}: {why}", file=out)
        return EXIT_OK
    print(plan_text(plan, unit), file=out)
    print("What the unit holds of these kinds is overwritten, empty slots in the file included. Playback on the unit is stopped.", file=out)
    if not a.yes and not ask("Restore now? A safety backup is saved first. [y/N] "):
        print("Cancelled. Nothing was written.", file=out)
        return EXIT_FAIL
    try:
        safety, _ = do_backup(conn, Path(a.out), "DUELL-safety-backup", out)
    except DuellError as e:
        print(f"error: the safety backup failed, nothing was written: {e}", file=sys.stderr)
        return EXIT_FAIL
    r = run_restore(conn.t, plan, progress_printer("restore"))
    if r.failed:
        for key, msg, _rc in r.failures:
            print(f"error: the restore stopped at {name_of(key)}: {msg}." if r.stopped and (key, msg, _rc) == r.failures[-1] else f"error: {name_of(key)} could not be restored: {msg}.", file=sys.stderr)
        print(f"  written: {', '.join(map(name_of, r.written)) or 'none'}" + (f"; not written: everything after {name_of(r.failures[-1][0])}." if r.stopped else "."), file=sys.stderr)
        print(f"  The unit holds a mix of its old data and the file's for the objects above. Fix the cause and restore the same file again. {safety} holds what was on the unit before.", file=sys.stderr)
        return EXIT_FAIL
    v = verify_restore(conn.t, plan)
    if v.differ:
        print(f"error: the restore ran but these do not match the file: {', '.join(map(name_of, v.differ))}. Restore again; keep {safety}.", file=sys.stderr)
        return EXIT_FAIL
    print(f"Done: {len(r.written)} objects written, {len(v.same)} read back equal to the file"
          + ("; settings and music accepted by the unit's own check" if v.not_compared else "") + f". Safety backup: {safety}", file=out)
    return EXIT_OK


def run_installer(cmd: list[str], out) -> int:
    """fm1_install.py as a subprocess, its output passed through as it comes (its progress line uses \\r)."""
    print("running: " + " ".join(cmd), file=out, flush=True)
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    assert p.stdout
    while chunk := p.stdout.read1(256) if hasattr(p.stdout, "read1") else p.stdout.read(1):
        out.write(chunk.decode("utf-8", "replace"))
        out.flush()
    return p.wait()


def wait_for_unit(opener: Opener, port: Optional[str], secs: float) -> Optional[Conn]:
    """After an install: the DUELL port back and answering INFO, polled once a second."""
    end = time.monotonic() + secs
    while True:
        try:
            return opener(port)
        except DuellError:
            if time.monotonic() >= end:
                return None
            time.sleep(1.0)


def cmd_flash(a, conn: Conn, out, ask, opener: Opener) -> int:
    pkg = Path(a.package)
    print(f"device: DUELL {conn.info.duell} ({conn.t.port_name}); package: {pkg}", file=out)
    if not a.yes and not ask("Back up the unit, then install this package? Do not unplug the FM-1 while writing. [y/N] "):
        print("cancelled", file=out)
        return EXIT_FAIL
    try:
        do_backup(conn, Path(a.out), "DUELL-backup", out)
    except DuellError as e:
        print(f"error: the backup failed, nothing was installed: {e}", file=sys.stderr)
        return EXIT_FAIL
    port = port_base(a.port or conn.t.port_name)
    conn.t.close()                                       # (Windows: a MIDI port has one user; fm1_install.py opens it next)
    cmd = [sys.executable, "-I", str(a.installer or INSTALLER), str(pkg), "--yes", "--port", port]
    if a.mock and not a.installer:
        print("mock: not installing; would run: " + " ".join(cmd), file=out)
    else:
        rc = run_installer(cmd, out)
        if rc:
            print(f"error: fm1_install.py exited with {rc}; the backup above holds the unit's data", file=sys.stderr)
            return EXIT_FAIL
    back = wait_for_unit(opener, a.port, 60.0)
    if not back:
        print("error: no DUELL unit answered within 60 s after the install (power-cycle it; it may run another firmware)", file=sys.stderr)
        return EXIT_NOUNIT
    try:
        print(f"now running: DUELL {back.info.duell} ({back.info.version})", file=out)
    finally:
        back.t.close()
    return EXIT_OK


class UsageError(DuellError):
    """A bad argument found after parsing (exit code 2)."""

# ------------------------------------------------------------------------------------------------------------------ measure (audit A01, Q14, T17, R14)
TOOLS = BASE / "tools"
MEASURE_TAG = "TDSP1"
BENCH_ENV = {"DUELL_BENCH": "1", "DUELL_TEST_MODE": "1", "BENCH_TAG": MEASURE_TAG, "DUELL_TWOCORE": "1", "DUELL_RAMPLACE": "1", "DUELL_HOTRAM": "3",
             "FELUCCA_UAC": "0", "FELUCCA_OTA": "0", "BENCH_BSS_MIN": "1024"}     # the bench build that fits the app slot with TIDE: no USB audio, no editor / updater (the way back is `uboot yes`)
DEFAULT_MEASURE = Path(os.environ.get("JAM_WORK") or REPO.parent / "work") / "measure"
MEASURE_HOOKS: dict = {}        # tests: build, ports, bench_main, serial_send, source, sleep, dis, wait_s (the seams of the real calls)


def kit():
    """The measurement kit of felucca-base/tools, loaded when `measure` runs (bench_run needs nothing but the standard library)."""
    import importlib
    if str(TOOLS) not in sys.path:
        sys.path.insert(0, str(TOOLS))
    return {n: importlib.import_module(n) for n in ("tide_measure", "velo_midi", "libcalls", "bench_run")}


def build_package(extra_env: dict, label: str, out) -> Path:
    """tools/build.py (the build the owner gets from a checkout) with these variables; the package it names last."""
    if MEASURE_HOOKS.get("build"):
        return Path(MEASURE_HOOKS["build"](extra_env, label))
    env = {**os.environ, **extra_env}
    print(f"building the {label} package (about 30 seconds) ...", file=out, flush=True)
    r = subprocess.run([sys.executable, str(TOOLS / "build.py")], cwd=str(BASE), env=env, capture_output=True, text=True)   # (not -I: build.py imports the modules beside it)
    m = re.findall(r"^package\s+(\S.*?)\s{2,}", r.stdout, re.M)
    if r.returncode or not m:
        tail = "\n".join((r.stdout + r.stderr).splitlines()[-12:])
        raise DuellError(f"the {label} build failed (is the toolchain set up: . jam/env.sh?):\n{tail}")
    for line in r.stdout.splitlines():
        if line.lstrip().startswith("ok    image") or line.lstrip().startswith("ok    range X"):
            print("  " + line.strip(), file=out)
    return Path(m[-1])


def serial_ports() -> list[str]:
    if MEASURE_HOOKS.get("ports"):
        return list(MEASURE_HOOKS["ports"]())
    br = kit()["bench_run"]
    return br.com_ports() if os.name == "nt" else sorted(__import__("glob").glob("/dev/cu.usbmodem*") + __import__("glob").glob("/dev/ttyACM*"))


def wait_for_serial(secs: float, want: Optional[str] = None) -> Optional[str]:
    """The bench build's console port, polled once a second after the install (the unit restarts and enumerates again)."""
    end = time.monotonic() + secs
    sleep = MEASURE_HOOKS.get("sleep", time.sleep)
    while True:
        ports = serial_ports()
        if want and want in ports:
            return want
        if not want and len(ports) == 1:
            return ports[0]
        if len(ports) > 1 and not want:
            raise DuellError(f"more than one serial port of the unit ({', '.join(ports)}): name it with --com")
        if time.monotonic() >= end:
            return None
        sleep(1.0)


def send_serial(port: str, line: str) -> None:
    if MEASURE_HOOKS.get("serial_send"):
        MEASURE_HOOKS["serial_send"](port, line)
        return
    br = kit()["bench_run"]
    link = (br.WinSerialLink if os.name == "nt" else br.SerialLink)(port)
    link.open()
    try:
        link.send(line)
        link.read(0.5)
    finally:
        link.close()


def install(pkg: Path, port: Optional[str], installer, out, mock: bool) -> int:
    """fm1_install.py as a subprocess (cmd_flash's way): the unit must be on a firmware that starts the update (the normal one) or already in update mode (after `uboot yes`)."""
    cmd = [sys.executable, "-I", str(installer or INSTALLER), str(pkg), "--yes"] + (["--port", port_base(port)] if port else [])
    if mock and not installer:
        print("mock: not installing; would run: " + " ".join(cmd), file=out)
        return 0
    return run_installer(cmd, out)


class MidiNotes:
    """The unit's note-ons over USB MIDI (mido with rtmidi): get(timeout) -> (t, note, velocity) or None. The port is the DUELL input (or --port); one program may hold it."""

    def __init__(self, name: Optional[str]):
        mido = _mido()
        ins = list(dict.fromkeys(mido.get_input_names()))
        cands = [n for n in ins if name.lower() in n.lower()] if name else [n for n in ins if PORT_RE.search(n)]
        if not cands:
            raise UnitNotFound("no DUELL MIDI input found (USB data cable? another program holding the port?): inputs " + (", ".join(ins) or "(none)"))
        self.q: queue.Queue = queue.Queue()
        self.port = mido.open_input(cands[0], callback=lambda m: self.q.put((time.monotonic(), m.note, m.velocity)) if m.type == "note_on" and m.velocity > 0 else None)
        self.name = cands[0]

    def get(self, timeout: float):
        try:
            return self.q.get(timeout=timeout)
        except queue.Empty:
            return None

    def close(self) -> None:
        self.port.close()


def measure_tide(a, out_dir: Path, out, ask, opener: Opener) -> tuple[int, list[Path]]:
    k = kit()
    ok_dis = None
    conn = opener(a.port)
    found = conn.t.port_name                             # the installer must be told it: by itself it looks for "FM-1"
    try:
        print(f"unit: DUELL {conn.info.duell} ({conn.info.version}) on {found}", file=out)
        bench = Path(a.bench) if a.bench else build_package(BENCH_ENV, "bench", out)
        normal = None
        if not a.no_restore:
            normal = Path(a.normal) if a.normal else build_package({}, "normal (to put back)", out)
        if not a.yes and not ask(f"Back up the unit, install the bench build {bench.name} and measure about 10 minutes (turn MASTER down, hands off the unit), then "
                                 + (f"put {normal.name} back" if normal else "leave the bench build installed") + "? [y/N] "):
            print("cancelled", file=out)
            return EXIT_FAIL, []
        do_backup(conn, DEFAULT_OUT if not a.out_backups else Path(a.out_backups), "DUELL-backup", out)
    finally:
        conn.t.close()
    rc = install(bench, a.port or found, a.installer, out, a.mock)
    if rc:
        print(f"error: the installer exited with {rc}; the backup above holds the unit's data", file=sys.stderr)
        return EXIT_FAIL, []
    port = wait_for_serial(MEASURE_HOOKS.get("wait_s", 90.0), a.com)
    if not port:
        print("error: no serial port of the unit within 90 s of the install (is it the bench build? the unit prints BENCH on its screen; try --com NAME)", file=sys.stderr)
        return EXIT_NOUNIT, []
    print(f"bench build running on {port}", file=out)
    runs = []
    status = EXIT_OK
    try:
        bench_main = MEASURE_HOOKS.get("bench_main") or k["bench_run"].main
        for name in ("TIDE-DSP", "PHYS-MODELS"):
            d = out_dir / name
            argv = ["--suite", name, "--port", port, "--out", str(d), "--runs", str(out_dir / "runs"), "--no-caffeinate"]
            print(f"== {name}", file=out, flush=True)
            r = bench_main(argv)
            if (d / "results.csv").exists():
                runs.append(d)
            if r not in (0, None):
                print(f"note: {name} reported failures (exit {r}); the report says which tests", file=out)
    except SystemExit as e:
        print(f"error: {e}", file=sys.stderr)
        status = EXIT_FAIL
    finally:
        if not a.no_restore:
            try:
                send_serial(port, "uboot yes")
                MEASURE_HOOKS.get("sleep", time.sleep)(3.0)
            except Exception as e:
                print(f"warning: could not send `uboot yes` ({e}); hold OCT- and OCT+ together for 5 s on the unit", file=sys.stderr)
    if not a.no_restore and normal:
        rc = install(normal, a.port, a.installer, out, a.mock)
        if rc:
            print(f"error: the normal firmware did not go back (installer exit {rc}). The unit is in update mode or on the bench build: run  jam_dev.py flash {normal}", file=sys.stderr)
            return EXIT_FAIL, runs
        back = wait_for_unit(opener, a.port, MEASURE_HOOKS.get("wait_s", 90.0))
        if not back:
            print("error: no DUELL unit answered after putting the normal firmware back (power-cycle it)", file=sys.stderr)
            return EXIT_NOUNIT, runs
        print(f"back on DUELL {back.info.duell} ({back.info.version})", file=out)
        back.t.close()
    else:
        print(f"the bench build stays installed. To go back: the console word `uboot yes` (or OCT- and OCT+ for 5 s), then  jam_dev.py flash <the normal package>", file=out)
    return status, runs


def libcalls_section(k) -> Optional[str]:
    dis = MEASURE_HOOKS.get("dis")
    path = Path(dis) if dis else BASE / "build" / "felucca.dis"
    if not path.exists():
        return None
    return k["libcalls"].report(path.read_text(errors="replace"), str(path))


def measure_velocity(a, out, ask, source=None) -> tuple[int, str]:
    k = kit()
    vm = k["velo_midi"]
    plan = vm.build_plan()
    print("KEY VELOCITY session: the unit must run the normal firmware with MENU > CONTROL > KEY VELOCITY ON and the play screen showing; OCT at its middle.\n"
          f"{len(plan)} steps, about {sum(s['n'] for s in plan)} presses. For each step press the key as asked; the next step begins by itself.", file=out)
    if not a.yes and not ask("Start? [y/N] "):
        print("cancelled", file=out)
        return EXIT_FAIL, ""
    src = source or MEASURE_HOOKS.get("source") or MidiNotes(a.port)
    try:
        presses, stray = vm.run_session(src, plan, lambda t: print(t, file=out, flush=True), step_timeout=a.step_timeout)
    finally:
        if hasattr(src, "close"):
            src.close()
    an = vm.analyse(presses)
    text = vm.report(an, stray)
    print(text, file=out)
    return EXIT_OK, text


def cmd_measure(a, out, ask, opener: Opener) -> int:
    k = kit()
    step = a.step or "all"
    stamp = datetime.now().strftime("%Y-%m-%d-%H%M")
    out_dir = Path(a.out) if a.out else DEFAULT_MEASURE / stamp
    out_dir.mkdir(parents=True, exist_ok=True)
    runs: list[Path] = []
    status, velocity = EXIT_OK, None
    if step == "report":
        runs = [Path(p) for p in a.runs]
        if not runs:
            raise UsageError("measure report RUNDIR [RUNDIR ...]")
    if step in ("all", "tide"):
        status, runs = measure_tide(a, out_dir, out, ask, opener)
        if status != EXIT_OK and not runs:
            return status
    if step in ("all", "velocity"):
        st, velocity = measure_velocity(a, out, ask)
        status = status or st
    rows = k["tide_measure"].load([str(r) for r in runs]) if runs else []
    text = k["tide_measure"].report_text(rows, stamp=stamp, build=f"bench {MEASURE_TAG}" if runs else "", libcalls=libcalls_section(k) if runs else None, velocity=velocity)
    path = out_dir / "report.md"
    path.write_text(text, encoding="utf-8", newline="\n")
    print(text, file=out)
    print(f"report: {path}", file=out)
    return status



def build_parser() -> argparse.ArgumentParser:
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--mock", action="store_true", default=argparse.SUPPRESS, help="use the simulated unit (node jam/mock_bridge.mjs)")
    common.add_argument("--port", metavar="NAME", default=argparse.SUPPRESS, help="MIDI port to use (part of its name)")
    ap = argparse.ArgumentParser(prog="jam_dev.py", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mock", action="store_true", default=False, help="use the simulated unit (node jam/mock_bridge.mjs)")
    ap.add_argument("--port", metavar="NAME", default=None, help="MIDI port to use (part of its name)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("info", parents=[common], help="the unit's identity")
    b = sub.add_parser("backup", parents=[common], help="a full backup the web editor can restore")
    b.add_argument("--out", metavar="DIR", default=str(DEFAULT_OUT))
    r = sub.add_parser("restore", parents=[common], help="restore a backup file as the web Backup page does")
    r.add_argument("file")
    r.add_argument("--yes", action="store_true", help="do not ask")
    r.add_argument("--include-raw", action="store_true", help="also send objects the backup could not check")
    r.add_argument("--out", metavar="DIR", default=str(DEFAULT_OUT), help="where the safety backup goes")
    f = sub.add_parser("flash", parents=[common], help="back up, then install a package with fm1_install.py")
    f.add_argument("package")
    f.add_argument("--yes", action="store_true", help="do not ask (here and in fm1_install.py)")
    f.add_argument("--out", metavar="DIR", default=str(DEFAULT_OUT), help="where the backup goes")
    f.add_argument("--installer", default=None, help=argparse.SUPPRESS)   # (tests: a stand-in for fm1_install.py)
    m = sub.add_parser("measure", parents=[common], help="the unit measurement of TIDE's audio DSP, PHYS per model and key velocity (audit A01, Q14, T17)")
    m.add_argument("step", nargs="?", choices=("all", "tide", "velocity", "report"), default="all",
                   help="all (default): bench measurement, then the normal firmware back, then the velocity session; tide: only the bench part; velocity: only the key session; report RUNDIR..: the report of runs made")
    m.add_argument("runs", nargs="*", help="(report) the run folders (each holds results.csv)")
    m.add_argument("--bench", metavar="PKG", help="a bench package already built (default: built here with tools/build.py)")
    m.add_argument("--normal", metavar="PKG", help="the package to put back (default: the tree's normal build)")
    m.add_argument("--no-restore", action="store_true", help="leave the bench build installed")
    m.add_argument("--com", metavar="NAME", help="the bench build's serial port (default: found by itself)")
    m.add_argument("--yes", action="store_true", help="do not ask")
    m.add_argument("--out", metavar="DIR", default=None, help="the report and the runs (default: <work folder>/measure/<date>)")
    m.add_argument("--out-backups", metavar="DIR", default=None, help=argparse.SUPPRESS)
    m.add_argument("--step-timeout", type=float, default=90.0, help="(velocity) seconds to wait for a press")
    m.add_argument("--installer", default=None, help=argparse.SUPPRESS)
    return ap


def ask_tty(prompt: str) -> bool:
    try:
        return input(prompt).strip().lower() in ("y", "yes")
    except EOFError:
        return False


def main(argv: Optional[list[str]] = None, opener: Optional[Opener] = None, ask: Callable[[str], bool] = ask_tty, out=None) -> int:
    out = out or sys.stdout
    a = build_parser().parse_args(argv)
    opener = opener or (open_mock if a.mock else open_midi)
    try:
        if a.cmd == "measure":
            return cmd_measure(a, out, ask, opener)    # (it opens the unit itself, and not at all for a report)
        if a.cmd == "restore":
            a.archive = load_archive(a.file)          # (a bad file is refused before the unit is touched)
        if a.cmd == "flash" and not Path(a.package).is_file():
            raise UsageError(f"no such package: {a.package}")
        conn = opener(a.port)
        try:
            if a.cmd == "flash":
                return cmd_flash(a, conn, out, ask, opener)
            return {"info": cmd_info, "backup": cmd_backup, "restore": cmd_restore}[a.cmd](a, conn, out, ask)
        finally:
            conn.t.close()
    except UsageError as e:
        print(f"error: {e}", file=sys.stderr)
        return EXIT_USAGE
    except UnitNotFound as e:
        print(f"error: {e}", file=sys.stderr)
        return EXIT_NOUNIT
    except DuellError as e:
        print(f"error: {e}", file=sys.stderr)
        return EXIT_FAIL
    except KeyboardInterrupt:
        print("\ninterrupted", file=sys.stderr)
        return EXIT_FAIL


if __name__ == "__main__":
    sys.exit(main())
