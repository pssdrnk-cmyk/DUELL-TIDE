#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Offline tests of jam/jam_dev.py against the web editor's simulated unit (jam/mock_bridge.mjs, needs node): the encodings, INFO, the backup file
byte for byte against the web editor's own capture and reader, the object checks against the editor's checkObject, restore / verify, failures,
the GET retry and the flash flow with a stand-in installer. No MIDI, no FM-1.

  . jam/env.sh && python3 jam/test_jam_dev.py
"""
from __future__ import annotations

import base64
import contextlib
import io
import json
import random
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import jam_dev as jd  # noqa: E402

BRIDGE: jd.MockTransport | None = None


class NoClose:
    """The shared mock behind main(): main closes its transport at the end, the tests keep the unit."""

    def __init__(self, t: jd.MockTransport):
        self.t, self.port_name = t, t.port_name

    def request(self, cmd, args, timeout):
        return self.t.request(cmd, args, timeout)

    def close(self):
        pass


def opener(port=None) -> jd.Conn:
    t = NoClose(BRIDGE)
    return jd.Conn(t, jd.probe(t))


def ctl(action, **kw) -> dict:
    return BRIDGE.control(action, **kw)


def state() -> dict:
    return ctl("state")["state"]


def run_main(argv, ask=lambda p: False) -> tuple[int, str, str]:
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stderr(err):
        rc = jd.main(argv, opener=opener, ask=ask, out=out)
    return rc, out.getvalue(), err.getvalue()


def capture(created="X") -> dict:
    t = NoClose(BRIDGE)
    return jd.capture_backup(t, jd.probe(t), created=created)


def setUpModule():
    global BRIDGE
    BRIDGE = jd.MockTransport()


def tearDownModule():
    BRIDGE.close()


class Base(unittest.TestCase):
    def setUp(self):
        ctl("reset")
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def backup_file(self) -> Path:
        rc, out, err = run_main(["backup", "--out", str(self.dir)])
        self.assertEqual(rc, 0, err)
        files = sorted(self.dir.glob("DUELL-backup-*.json"))
        self.assertEqual(len(files), 1)
        return files[0]


class Encodings(unittest.TestCase):
    def test_u32(self):
        for n in (0, 1, 127, 128, 3840, 2 ** 28 - 1, 2 ** 28, 0xCBF43926, 0xFFFFFFFF):
            w = jd.u32(n)
            self.assertEqual(len(w), 5)
            self.assertTrue(all(b <= 127 for b in w) and w[4] <= 15)
            self.assertEqual(jd.r32([9, *w], 1), n)
        for bad in ([0, 0, 0, 0, 16], [0, 0, 0, 0], [128, 0, 0, 0, 0]):
            with self.assertRaises(jd.DuellError):
                jd.r32(bad)

    def test_pack7_round_trip(self):
        rnd = random.Random(7)
        for size in [0, 1, 6, 7, 8, 13, 14, 15, 255, 256] + [rnd.randrange(300) for _ in range(30)]:
            data = bytes(rnd.randrange(256) for _ in range(size))
            p = jd.pack7(data)
            self.assertTrue(all(b <= 127 for b in p))
            self.assertEqual(len(p), size // 7 * 8 + (size % 7 + 1 if size % 7 else 0))
            self.assertEqual(jd.unpack7(p, size), data)
        self.assertEqual(len(jd.pack7(bytes(256))), 293)       # a 256-byte piece: 36 groups of 7 and one of 4

    def test_unpack7_strict(self):
        good = jd.pack7(b"\x80\x01\xff")                     # mask 0b101, three bytes
        self.assertEqual(good, [0b101, 0, 1, 127])
        cases = {"unused mask bit": [0b1000, 0, 1, 127], "short group": good[:-1], "trailing byte": good + [0], "data byte above 127": [0, 0, 1, 128], "no mask": []}
        for what, a in cases.items():
            with self.subTest(what), self.assertRaises(jd.DuellError):
                jd.unpack7(a, 3)

    def test_crc_and_frames(self):
        self.assertEqual(jd.crc32(b"123456789"), 0xCBF43926)
        self.assertEqual(jd.crc32(b""), 0)
        self.assertEqual(jd.build_frame(65, [1]), bytes([0xF0, 0x7D, 0x46, 0x4C, 65, 1, 0xF7]))
        with self.assertRaises(ValueError):
            jd.build_frame(66, [128])
        f = bytes([0xF0, 0x7D, 0x46, 0x4C, 66, 1, 2, 0xF7])
        self.assertEqual(jd.reply_args(f, 66), [1, 2])
        self.assertIsNone(jd.reply_args(f, 65))                                  # another cmd
        self.assertIsNone(jd.reply_args(f[:-1] + bytes([0]), 66))                    # no F7 at the end
        self.assertIsNone(jd.reply_args(bytes([0xF0, 0x00, 0x32, 0x45, 66, 1, 0xF7]), 66))   # not the editor's header
        self.assertEqual(jd.reply_args(bytes([0xF0, 0x7D, 0x46, 0x4C, 25, 0xF7]), 25), [])   # len 6: an empty reply is a reply

    def test_ports(self):
        self.assertEqual(jd.pair_output("DUELL 0", ["Microsoft GS Wavetable Synth 0", "DUELL 1"]), "DUELL 1")
        self.assertEqual(jd.pair_output("DUELL", ["DUELL"]), "DUELL")
        self.assertEqual(jd.port_base("DUELL 12"), "DUELL")


class InfoTests(Base):
    def test_mock_info(self):
        i = jd.read_info(NoClose(BRIDGE))
        self.assertEqual((i.version, i.duell, i.caps, i.patch_cap, len(i.engines), i.p_count), ("FELUCCA DUELL 0.2.25 bet", "0.2.25", 63, 128, 15, 127))
        self.assertEqual(i.engines[:3], ["ANALOG", "-", "PHASE"])

    def test_firmware_info_layout(self):
        """INFO as editor.c ED_INFO sends it today, all tags (0x59 and 0x4B after 0x5A included)."""
        s = lambda t: [*t.encode(), 0]
        a = [*s("FELUCCA DUELL 1.0"), 2, 127, 27, 64, 41, *s("ANALOG"), *s("-"), 2, 0,
             0x55, 1, 9, 0x4D, 1, 64, 1, 0x42, 1, 63, 0x46, 1, 8, 0, 0x53, 1, 3, 0x50, 1, 3, 0x4E, 1, 30, 0x52, 1, 4, 0x4C, 1, 1, 0x41, 1, 0, 1,
             0x54, 1, 16, 0x56, 1, 32, 0x43, 1, 16, 0x58, 1, 0, 1, 0x5A, 1, 1, 0x59, 1, 16, 0x4B, 1, 1]
        i = jd.parse_info(a)
        self.assertEqual((i.duell, i.caps, i.patch_cap, i.engines), ("1.0", 63, 128, ["ANALOG", "-"]))
        self.assertEqual((i.tags[0x59], i.tags[0x4B], i.tags[0x41]), ([16], [1], [0, 1]))
        self.assertFalse(i.tide)
        j = jd.parse_info([*a, 0x5B, 1, 1, 0x4F, 1, 1, 0x44, 1, 1])      # the tags after 0x4B: BYPASS, the MORPH view, TIDE (this firmware sends them last)
        self.assertTrue(j.tide and j.tags[0x44] == [1] and j.tags[0x4F] == [1])
        for bad in ([], [0, 1], [*s("DUELL 1.0"), 3, 1, 1, 1, 1, *s("A")]):
            with self.assertRaises(jd.DuellError):
                jd.parse_info(bad)

    def test_duell_regex(self):
        v = lambda t: jd.Info(t, [], None, None).duell
        self.assertEqual([v("FELUCCA DUELL 1.0"), v("DUELL 0.2.25 bet"), v("FELUCCA DUELL 1.0.2")], ["1.0", "0.2.25", "1.0.2"])
        self.assertEqual([v("FELUCCA 1.6.1"), v("FELUCCA DUELL 1.0x"), v("DUELLX 1.0")], [None, None, None])

    def test_info_cli_and_not_duell(self):
        rc, out, _ = run_main(["info"])
        self.assertEqual(rc, 0)
        self.assertIn("DUELL 0.2.25", out)
        self.assertIn("128 slots", out)
        ctl("version", version="FELUCCA 1.6.1")
        rc, out, err = run_main(["info"])
        self.assertEqual(rc, 3)
        self.assertIn("does not run DUELL", err)


class BackupTests(Base):
    def test_byte_identical_to_web_capture(self):
        """The file equals what the web Backup page downloads from the same unit (JSON.stringify of captureBackup), but for the date."""
        js = ctl("captureJs")["text"]
        created = json.loads(js)["created"]
        py = jd.file_text(capture(created))
        self.assertEqual(py, js)
        self.assertRegex(js, r'^\{"format":"duell-backup","version":5,"firmware":"FELUCCA DUELL 0\.2\.25 bet","created":"[^"]+","engines":\[')

    def test_cli_backup_read_by_web_editor(self):
        p = self.backup_file()
        self.assertRegex(p.name, r"^DUELL-backup-\d{4}-\d\d-\d\d-\d{4}\.json$")
        text = p.read_bytes()
        self.assertFalse(text.endswith(b"\n") or b"\r" in text)
        r = ctl("readArchive", text=text.decode())
        self.assertTrue(r["accepted"], r.get("reason"))
        self.assertEqual((r["version"], r["objects"], r["patches"], r["problems"], r["hasPatches"]), (5, 20, [0, 1, 5, 64], [], True))
        f = json.loads(text)
        self.assertRegex(f["created"], r"^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z$")
        self.assertEqual([o["id"] for o in f["objects"]], jd.DUELL_IDS + [40] * 4)
        a = jd.read_archive(text.decode())
        self.assertEqual(sorted(a.objects), sorted(jd.DUELL_IDS + [1000, 1001, 1005, 1064]))

    def test_web_file_read_here(self):
        a = jd.read_archive(ctl("captureJs")["text"])
        self.assertTrue(a.has_patches and not a.problems and len(a.objects) == 20)

    def test_same_refusals_as_web_reader(self):
        """Damaged files: refused here exactly when the web editor's readArchive refuses them."""
        good = json.loads(ctl("captureJs")["text"])
        def mut(f):
            c = json.loads(json.dumps(good)); f(c); return json.dumps(c)
        def o(c, i):
            return next(x for x in c["objects"] if x["id"] == i)
        cases = {
            "crc": lambda c: o(c, 14).update(crc=o(c, 14)["crc"] ^ 1), "truncated": lambda c: o(c, 14).update(data=o(c, 14)["data"][:-4]),
            "twice": lambda c: c["objects"].append(c["objects"][2]), "unknown id": lambda c: o(c, 15).update(id=99),
            "group incomplete": lambda c: c["objects"].remove(o(c, 17)), "no music": lambda c: c["objects"].remove(o(c, 0)),
            "newer version": lambda c: c.update(version=6), "version 1": lambda c: c.update(version=1), "another format": lambda c: c.update(format="x"),
            "old chunk in v5": lambda c: c["objects"].append({"id": 6, "size": 0, "crc": 0, "data": ""}),
            "retired with data": lambda c: c["objects"].append({"id": 2, "size": 3, "crc": jd.crc32(b"abc"), "data": "YWJj"}),
            "retired empty": lambda c: c["objects"].append({"id": 2, "size": 0, "crc": 0, "data": ""}),
            "patch slot 999": lambda c: c["objects"][-1].update(slot=999), "size bool": lambda c: o(c, 13).update(size=True),
            "raw flag": lambda c: o(c, 14).update(raw=True, problem="test"), "no patches": lambda c: c.update(objects=[x for x in c["objects"] if x["id"] != 40]),
        }
        for what, f in cases.items():
            text = mut(f)
            with self.subTest(what):
                web = ctl("readArchive", text=text)
                try:
                    jd.read_archive(text); here = True
                except jd.DuellError:
                    here = False
                self.assertEqual(here, web["accepted"], web.get("reason"))

    def test_object_checks_match_web(self):
        """checkObject's port: the same reason (or none) as the web editor's, on every kind of object and on a few hundred damaged copies."""
        ctl("setPatch", slot=9, name="RICH ONE", rich=True)                 # (the extension block, 32 edits, the bitmap form)
        ctl("setPatch", slot=10, name="RICH FM6", engine=12, rich=True)     # (and the FM6 patch)
        a = jd.read_archive(ctl("captureJs")["text"])
        rnd, items = random.Random(11), []
        for key, o in a.objects.items():
            items.append((key, o.data))
            for _ in range(60 if jd.is_patch_key(key) or 13 <= key <= 23 else 15):
                if not o.data:
                    break
                b = bytearray(o.data)
                at = rnd.randrange(min(len(b), 120)) if rnd.random() < 0.8 else rnd.randrange(len(b))
                b[at] = rnd.choice([0, 1, 7, 31, 32, 64, 65, 126, 127, 128, 191, 192, 255, b[at] ^ 1, b[at] + 1 & 255])
                items.append((key, bytes(b)))
            items.append((key, o.data[:-1]))
        for k in (13, 21, 24):
            items.append((k, b""))
        web = ctl("checkObjects", items=[{"key": k, "data": base64.b64encode(b).decode()} for k, b in items])["reasons"]
        here = [jd.check_object(k, b) for k, b in items]
        # (jam_dev.py checks the music's revision 8 framing and its FNV hash too, the unit's proj_unpack; the web editor's checkObject only its header: that is stricter on purpose)
        diff = [(k, w, h) for (k, _), w, h in zip(items, web, here) if w != h and not (k == 0 and w is None and (h or "").startswith("damaged current music"))]
        self.maxDiff = None
        self.assertEqual(diff, [])
        self.assertGreater(sum(1 for w in web if w), 100)          # (most damaged copies are refused, with many different reasons)

    def test_playing_refused(self):
        ctl("playing", on=True)
        rc, _, err = run_main(["backup", "--out", str(self.dir)])
        self.assertEqual(rc, 1)
        self.assertIn("playing", err)
        self.assertEqual(list(self.dir.iterdir()), [])


class RetryTests(Base):
    def test_get_retried_once(self):
        ctl("drop", cmd=66, count=1)
        f = capture()
        self.assertNotIn("problems", f)
        c = state()["counts"]
        pieces = {k: v for k, v in c.items() if k.startswith("66:")}
        self.assertEqual(sorted(pieces.values()).count(2), 1)
        self.assertEqual(c["66"], sum(pieces.values()))
        self.assertEqual(c["66"], len(pieces) + 1)

    def test_get_lost_twice_is_raw(self):
        ctl("drop", cmd=66, count=2)
        f = capture()
        self.assertEqual([(p["id"], p["problem"]) for p in f["problems"]], [(0, f"it could not be read ({jd.TIMEOUT_MSG})")])
        r = ctl("readArchive", text=jd.file_text(f))
        self.assertTrue(r["accepted"])
        self.assertEqual(len(r["problems"]), 1)

    def test_put_never_retried(self):
        p = self.backup_file()
        ctl("setMulti", id=14, names=["CHANGED", None])
        ctl("drop", cmd=67, count=1, skip=3)                    # object 13 is empty (begin, commit); the reply to object 14's first data piece is lost
        rc, _, err = run_main(["restore", str(p), "--yes", "--out", str(self.dir)])
        self.assertEqual(rc, 1)
        self.assertIn("stopped at object 14: " + jd.TIMEOUT_MSG, err)
        sent = state()["sent"]
        self.assertEqual([s[:2] for s in sent], [[0, 13], [2, 13], [0, 14], [1, 14], [3, 14]])   # the lost piece is not sent again: abort, nothing after


class BankTests(Base):
    """FM6's voice bank (INFO tag 44 01 bit 1) in the archive: object 41, read with GET, written back with FM6B_BEGIN / WRITE / END."""

    def objs41(self, f):
        return [o for o in f["objects"] if o["id"] == 41]

    def test_round_trip(self):
        ctl("reset", options={"fvBank": True})
        self.assertTrue(capture()["objects"] and not self.objs41(capture()))               # a unit with no bank: no object 41
        ctl("setBank", name="MY BANK", voices=[0, 5, 31])
        before = state()["bank"]
        self.assertIsNotNone(before)
        f = capture()
        self.assertEqual(len(self.objs41(f)), 1)
        self.assertEqual(self.objs41(f)[0]["size"], 3620)
        self.assertTrue(ctl("readArchive", text=jd.file_text(f))["accepted"])               # the web editor's reader takes the file
        web = json.loads(ctl("captureJs")["text"])
        self.assertEqual(self.objs41(web), self.objs41(f))                                  # byte for byte what the web page makes
        p = self.dir / "bank.json"
        p.write_text(jd.file_text(f), encoding="utf-8", newline="\n")
        ctl("setBank", name="OTHER", voices=[2])
        self.assertNotEqual(state()["bank"], before)
        rc, out, err = run_main(["restore", str(p), "--yes", "--out", str(self.dir)])
        self.assertEqual(rc, 0, err)
        self.assertEqual(state()["bank"], before)
        self.assertIn("FM6 voice bank", out)
        self.assertTrue(ctl("readArchive", text=next(self.dir.glob("DUELL-safety-backup-*.json")).read_text())["accepted"])   # (the safety backup was made first)

    def test_no_bank_in_file_leaves_the_unit_alone(self):
        ctl("reset", options={"fvBank": True})
        p = self.dir / "nobank.json"
        p.write_text(jd.file_text(capture()), encoding="utf-8", newline="\n")
        ctl("setBank", name="KEEP ME", voices=[3])
        keep = state()["bank"]
        rc, out, err = run_main(["restore", str(p), "--yes", "--out", str(self.dir)])
        self.assertEqual(rc, 0, err)
        self.assertEqual(state()["bank"], keep)

    def test_unit_without_the_flag_is_not_sent_the_bank(self):
        ctl("reset", options={"fvBank": True})
        ctl("setBank", name="A BANK", voices=[1, 2])
        p = self.dir / "bank.json"
        p.write_text(jd.file_text(capture()), encoding="utf-8", newline="\n")
        ctl("reset")                                                                        # a unit with no INFO tag 44
        rc, out, err = run_main(["restore", str(p), "--yes", "--out", str(self.dir)])
        self.assertEqual(rc, 0, err)
        self.assertIn("cannot restore it", out)
        self.assertNotIn("cmd 74", " ".join(state()["log"]))
        self.assertIsNone(state()["bank"])

    def test_damaged_bank_is_refused(self):
        ctl("reset", options={"fvBank": True})
        ctl("setBank", name="A BANK", voices=[1])
        f = capture()
        o = self.objs41(f)[0]
        raw = bytearray(base64.b64decode(o["data"]))
        raw[3620 - 1] ^= 1                                                                  # its own CRC (the last four bytes)
        o["data"] = base64.b64encode(bytes(raw)).decode()
        o["crc"] = jd.crc32(bytes(raw))
        with self.assertRaises(jd.DuellError) as cm:
            jd.read_archive(f)
        self.assertIn("voice bank", str(cm.exception))


class RestoreTests(Base):
    def test_round_trip(self):
        ctl("setPatch", slot=10, name="BIG FM6", engine=12, rich=True)       # (a record of over 256 bytes: two pieces each way)
        self.assertGreater(int(state()["patches"]["10"].split(":")[0]), 256)
        before = state()
        p = self.backup_file()
        ctl("alterPatch", slot=0)
        ctl("deletePatch", slot=5)
        ctl("deletePatch", slot=10)
        ctl("setPatch", slot=9, name="EXTRA")
        ctl("setMulti", id=14, names=["CHANGED", None])
        changed = state()
        self.assertNotEqual((changed["objs"], changed["patches"]), (before["objs"], before["patches"]))
        rc, out, err = run_main(["restore", str(p), "--yes", "--out", str(self.dir)])
        self.assertEqual(rc, 0, err)
        after = state()
        self.assertEqual((after["objs"], after["patches"]), (before["objs"], before["patches"]))
        self.assertIn("Patches: 3 written, 1 saved on the unit but not in the file removed, 2 already identical.", out)
        self.assertRegex(out, r"Done: 20 objects written, 18 read back equal")
        self.assertTrue(any(re.match(r"DUELL-safety-backup-.*\.json$", f.name) for f in self.dir.iterdir()))
        log = after["log"]
        self.assertEqual([x for x in log if "patch" in x], ["remove patch 9", "write patch 0", "write patch 5", "write patch 10"])   # (the removals first: they free room, D05)
        self.assertEqual(log[-2:], ["write 1", "write 0"])                    # the settings, then the live music last
        safety = next(self.dir.glob("DUELL-safety-backup-*.json"))
        self.assertTrue(ctl("readArchive", text=safety.read_text())["accepted"])
        self.assertIn(9, ctl("readArchive", text=safety.read_text())["patches"])   # (the safety backup holds the unit as it was)

    def test_web_file_restores(self):
        before = state()
        p = self.dir / "web.json"
        p.write_text(ctl("captureJs")["text"], encoding="utf-8", newline="\n")
        ctl("deletePatch", slot=64)
        rc, _, err = run_main(["restore", str(p), "--yes", "--out", str(self.dir)])
        self.assertEqual(rc, 0, err)
        self.assertEqual(state()["patches"], before["patches"])

    def test_failure_mid_restore(self):
        p = self.backup_file()
        ctl("setMulti", id=14, names=["CHANGED", None])
        mid = state()
        ctl("fail", id=14, op=1, rc=4)
        rc, _, err = run_main(["restore", str(p), "--yes", "--out", str(self.dir)])
        self.assertEqual(rc, 1)
        self.assertIn("object 14 could not be restored: The unit could not write its flash memory", err)
        self.assertIn("written: object 13, object 15,", err)                 # D05: the other objects still go
        s = state()
        self.assertFalse(s["staged"])                                        # aborted
        self.assertEqual(s["objs"]["14"], mid["objs"]["14"])
        self.assertIn([0, 14], [x[:2] for x in s["sent"]])
        self.assertIn([3, 14], [x[:2] for x in s["sent"]])                   # (the failed object was aborted)
        self.assertIn("write 0", s["log"])                                   # the live music is still restored

    def test_failure_at_patch_commit(self):
        p = self.backup_file()
        ctl("alterPatch", slot=1)
        ctl("fail", id=40, op=2, rc=4)
        rc, _, err = run_main(["restore", str(p), "--yes", "--out", str(self.dir)])
        self.assertEqual(rc, 1)
        self.assertIn("patch #02 could not be restored", err)
        self.assertIn("written: object 13,", err)                            # D05: the other groups still go

    def test_safety_backup_is_mandatory(self):
        p = self.backup_file()
        ctl("alterPatch", slot=0)
        ctl("drop", cmd=65, count=1, skip=7)                     # the plan's LIST and six pages answer (JAM bank: pages 1..6), the safety backup's LIST is lost
        rc, _, err = run_main(["restore", str(p), "--yes", "--out", str(self.dir)])
        self.assertEqual(rc, 1)
        self.assertIn("the safety backup failed, nothing was written", err)
        self.assertEqual(state()["sent"], [])
        self.assertEqual(list(self.dir.glob("DUELL-safety-backup-*")), [])
        ctl("playing", on=True)                                  # (playing: LIST answers rc 3, not even a plan)
        rc, _, err = run_main(["restore", str(p), "--yes", "--out", str(self.dir)])
        self.assertEqual(rc, 1)
        self.assertIn("playing", err)
        self.assertEqual(state()["sent"], [])

    def test_cancel_and_nothing_to_do(self):
        p = self.backup_file()
        ctl("alterPatch", slot=0)
        rc, out, _ = run_main(["restore", str(p), "--out", str(self.dir)], ask=lambda q: False)
        self.assertEqual(rc, 1)
        self.assertIn("Cancelled. Nothing was written.", out)
        self.assertEqual(state()["sent"], [])
        rc, _, _ = run_main(["restore", str(p), "--out", str(self.dir)], ask=lambda q: True)
        self.assertEqual(rc, 0)

    def test_plan_rules(self):
        a = jd.read_archive(ctl("captureJs")["text"])
        t = NoClose(BRIDGE)
        ctl("setPatch", slot=100, name="ALSO MINE")
        plan = jd.plan_restore(a, jd.list_unit(t, 63), 63)
        self.assertEqual([s.key for s in plan.steps], [1100, *jd.RESTORE_ORDER])
        self.assertEqual(plan.steps[0].data, b"")
        self.assertEqual(plan.skipped, [1000, 1001, 1005, 1064])
        old = jd.plan_restore(a, jd.list_unit(t, 63), 7)
        self.assertEqual([lab for lab, _ in old.excluded], ["Patches", "User wavetables"])
        a.objects[1] = jd.Obj(1, a.objects[1].data + b"\0" * 16)
        self.assertIn("another DUELL version", dict(jd.plan_restore(a, jd.list_unit(t, 63), 63).excluded)["Settings"])


class FlashTests(Base):
    FAKE = ("import json, sys, pathlib\n"
            "pathlib.Path(sys.argv[0]).with_suffix('.ran').write_text(json.dumps(sys.argv[1:]))\n"
            "print('package: FM-1_906'); print('\\rwriting  50%\\rwriting 100%'); print('done: the FM-1 runs FM-1_906')\n"
            "sys.exit(int(sys.argv[sys.argv.index('--exit') + 1]) if '--exit' in sys.argv else 0)\n")

    def setUp(self):
        super().setUp()
        self.pkg = self.dir / "DUELL.fwsc"
        self.pkg.write_bytes(b"\0" * 64)
        self.fake = self.dir / "fake_install.py"
        self.fake.write_text(self.FAKE, encoding="utf-8", newline="\n")

    def test_flash_flow(self):
        rc, out, err = run_main(["flash", str(self.pkg), "--yes", "--out", str(self.dir), "--installer", str(self.fake)])
        self.assertEqual(rc, 0, err)
        self.assertEqual(json.loads(self.fake.with_suffix(".ran").read_text()), [str(self.pkg), "--yes", "--port", "DUELL"])
        self.assertIn("writing 100%", out)
        self.assertIn("now running: DUELL 0.2.25", out)
        self.assertLess(out.index("saved "), out.index("running: "))                 # the backup comes first
        self.assertEqual(len(list(self.dir.glob("DUELL-backup-*.json"))), 1)

    def test_backup_failure_stops_flash(self):
        ctl("playing", on=True)
        rc, _, err = run_main(["flash", str(self.pkg), "--yes", "--out", str(self.dir), "--installer", str(self.fake)])
        self.assertEqual(rc, 1)
        self.assertIn("nothing was installed", err)
        self.assertFalse(self.fake.with_suffix(".ran").exists())

    def test_installer_failure(self):
        self.fake.write_text(self.FAKE.replace("else 0)", "else 5)"), encoding="utf-8", newline="\n")
        rc, _, err = run_main(["flash", str(self.pkg), "--yes", "--out", str(self.dir), "--installer", str(self.fake)])
        self.assertEqual(rc, 1)
        self.assertIn("exited with 5", err)

    def test_cancel_and_missing_package(self):
        rc, out, _ = run_main(["flash", str(self.pkg), "--out", str(self.dir), "--installer", str(self.fake)], ask=lambda q: False)
        self.assertEqual(rc, 1)
        self.assertFalse(self.fake.with_suffix(".ran").exists())
        rc, _, _ = run_main(["flash", str(self.dir / "nope.fwsc"), "--yes"])
        self.assertEqual(rc, 2)


class CliTests(unittest.TestCase):
    """The script itself, as a user runs it (its own mock bridge each time)."""

    def run_cli(self, *args):
        return subprocess.run([sys.executable, str(HERE / "jam_dev.py"), *args], capture_output=True, text=True, timeout=120)

    def test_exit_codes_and_mock_flash(self):
        self.assertEqual(self.run_cli().returncode, 2)
        self.assertEqual(self.run_cli("bogus").returncode, 2)
        self.assertEqual(self.run_cli("--mock", "restore", "no-such-file.json", "--yes").returncode, 2)
        r = self.run_cli("--mock", "info")
        self.assertEqual(r.returncode, 0, r.stderr)
        with tempfile.TemporaryDirectory() as d:
            pkg = Path(d) / "x.fwsc"
            pkg.write_bytes(b"\0")
            r = self.run_cli("--mock", "flash", str(pkg), "--yes", "--out", d)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn("mock: not installing; would run:", r.stdout)
            self.assertIn("fm1_install.py", r.stdout)
            self.assertRegex(r.stdout, r"--yes --port DUELL\b")
            bad = Path(d) / "bad.json"
            bad.write_text("{}", encoding="utf-8")
            self.assertEqual(self.run_cli("--mock", "restore", str(bad), "--yes", "--out", d).returncode, 1)

    def test_no_unit(self):
        """Without --mock and with a port name no gear has: exit 3 (MIDI is only listed, nothing is sent)."""
        r = self.run_cli("info", "--port", "NO-SUCH-PORT-XYZ")
        if "needs mido and python-rtmidi" in r.stderr:   # a machine without the MIDI packages (CI's jam-tools job): exit 1 with that message
            self.assertEqual(r.returncode, 1, r.stderr)
            return
        self.assertEqual(r.returncode, 3, r.stderr)
        self.assertIn("no DUELL unit found", r.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
