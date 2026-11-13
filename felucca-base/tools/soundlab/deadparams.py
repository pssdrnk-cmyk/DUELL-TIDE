#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""soundlab deadparams: which parameters change nothing? (stdlib only; renders and compares with soundlab itself)

  soundlab deadparams [--engine NAME] [--out FILE.tsv]

For every engine that has factory sounds, and every parameter id with a descriptor (`soundlab params`), on the engine's first factory sound as the base:
render one held note (C3, 1 bar, 1 s release) with the parameter at its min and at its max; if the two renders are bit-identical, again with a C minor chord held;
if still identical, again in each context where the parameter could act (alternatives() below: an LFO / ENV 2 / insert / EQ band / arp / voice mode / twin filter switched on,
the wave slot E0 at each of its values for the engine's own slots and the SHAPE depths).
DEAD = identical in every one of them. SILENT = identical and nothing audible (the render is silent). NEAR = the largest change is under 0.5 dB RMS and 1/12 octave
of centroid. LIVE = more. UNTESTED = needs a context a held note cannot give (sequencer steps, the modulation matrix, the sidechain, mute).
The page of each id is read from the PAGES table of params.c. Writes the TSV (default build/soundlab/deadparams.tsv); DEADPARAMS.md is written from it, with the reasons."""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
FB = os.path.abspath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(FB, "firmware", "src")


def strip_comments(s):
    return re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", "", s, flags=re.S))


def enum_ids(text, start_marker, base=0):
    """{name: value} of the enum that follows start_marker, up to the closing brace"""
    i = text.index(start_marker)
    j = text.index("{", i)
    k = text.index("}", j)
    body = strip_comments(text[j + 1:k])
    out, n = {}, base
    for tok in body.split(","):
        tok = tok.strip()
        if not tok:
            continue
        if "=" in tok:
            name, val = [x.strip() for x in tok.split("=", 1)]
            n = int(val, 0) if re.fullmatch(r"-?(0x[0-9a-fA-F]+|\d+)", val) else out.get(val, n)
        else:
            name = tok
        out[name] = n
        n += 1
    return out


def load_names():
    core = open(os.path.join(SRC, "core.h"), encoding="utf-8", errors="replace").read()
    ext = open(os.path.join(SRC, "ext.h"), encoding="utf-8", errors="replace").read()
    P = enum_ids(core, "enum {                          /* per-track parameters */")
    X = enum_ids(ext, "enum {                          /* slot numbers")
    ids = dict(P)
    for k, v in X.items():
        ids["XW_BASE + " + k] = 128 + v
    return P, ids


def load_pages(ids):
    """id -> [page titles] from params.c PAGES"""
    src = strip_comments(open(os.path.join(SRC, "params.c"), encoding="utf-8", errors="replace").read())
    a = src.index("static const page_t PAGES[] = {")
    b = src.index("};", a)
    pages = {}
    for m in re.finditer(r'\{"([^"]+)",\s*FAM_\w+,\s*SC_\w+,\s*GR_\w+,\s*\{([^}]*)\}', src[a:b]):
        title = m.group(1)
        for tok in m.group(2).split(","):
            tok = re.sub(r"\s+", " ", tok.strip())
            if tok in ids:
                pages.setdefault(ids[tok], []).append(title)
    return pages


# What a parameter needs before it can be heard: alternatives (label, settings), tried one after the other; "--x y" items are render options.
G_LFO1 = ("P_LRATE", "P_LWAVE", "P_LPHASE", "P_LFADE", "P_LSYNC", "P_LTRIG", "P_LPOL", "XW_BASE + X_LFO1LVL", "P_LD_PIT", "P_LD_FLT", "P_LD_AMP")
G_LFO2 = ("P_L2RATE", "P_L2WAVE", "P_L2PHASE", "P_L2FADE", "P_L2SYNC", "P_L2TRIG", "P_L2POL", "XW_BASE + X_LFO2LVL", "P_L2PIT", "P_L2FLT")
G_ENV2 = ("P_E2ATK", "P_E2DEC", "P_E2SUS", "P_E2REL")
G_INS = ("P_IA", "P_IB", "P_IC", "P_IMIX")
G_TWIN = ("P_RES2", "P_DRV2", "P_BLEND", "XW_BASE + X_ROUTE", "XW_BASE + X_LINK", "XW_BASE + X_KTRK2", "XW_BASE + X_ENV2", "XW_BASE + X_LFO1", "XW_BASE + X_LFO2",
          "XW_BASE + X_E2BLD", "XW_BASE + X_E2DRV", "XW_BASE + X_E2RES", "XW_BASE + X_L2RES2", "XW_BASE + X_L2FMODE", "XW_BASE + X_L2BLD")
G_ARP = ("P_ARATE", "P_AOCT", "P_AGATE", "P_ASWING", "P_APROB", "P_AHOLD", "P_AORDER", "P_ATGT")
SHAPE = {"P_ED_SHP": [], "P_LD_SHP": ["XW_BASE + X_LFO1LVL=127"], "P_E2SHP": [], "P_L2SHP": ["XW_BASE + X_LFO2LVL=127"]}
UNTESTED = ("P_SLEN", "P_SDIV", "P_SSWING", "P_SGATE", "P_SQNT", "P_M1SRC", "P_M1DST", "P_M1AMT", "P_M2SRC", "P_M2DST", "P_M2AMT", "P_M3SRC", "P_M3DST", "P_M3AMT",
            "P_M4SRC", "P_M4DST", "P_M4AMT", "P_M5SRC", "P_M5DST", "P_M5AMT", "P_M6SRC", "P_M6DST", "P_M6AMT", "P_M7SRC", "P_M7DST", "P_M7AMT", "P_M8SRC", "P_M8DST", "P_M8AMT",
            "P_DKSRC", "P_DKRATE", "P_DKDEP", "P_DKTHR", "P_DKCRV", "P_DKATK", "P_DKRLS", "P_MUTE")
HI_OVERRIDE = {"P_TWTYPE": 1}    # the TWIN FILTER's TYPE: its last value (COMB) reads as OFF on an engine with no engine memory for it (chain_f2.c), so OFF against LP
UNTESTED_WHY = "needs the sequencer, the modulation matrix, the sidechain or a mute state: a held note cannot show it"


def alternatives(name, desc):
    """[(label, [settings and options])] for the parameter `name`; desc: {id-name: (min, max, def, cur)} of the engine; [] = only the plain render"""
    alts = []
    E0 = list(range(desc["P_E0"][0], min(desc["P_E0"][1], desc["P_E0"][0] + 15) + 1)) if "P_E0" in desc else []
    if name in G_LFO1:
        alts.append(("LFO 1 to pitch, cutoff and level up", ["P_LD_PIT=63", "P_LD_FLT=63", "XW_BASE + X_LFO1LVL=127", "P_LTRIG=0"]))
        if name == "P_LRATE":                # b9: with SYNC on the tempo division replaces RATE (the card is hidden then, ui_draw.c card_hidden_vp): RATE is tried free-running
            alts.append(("LFO 1 free-running (SYNC off) to pitch and level", ["P_LSYNC=0", "P_LD_PIT=63", "XW_BASE + X_LFO1LVL=127", "P_LTRIG=0"]))
    if name in G_LFO2:
        alts.append(("LFO 2 to pitch, cutoff and level up", ["P_L2PIT=63", "P_L2FLT=63", "XW_BASE + X_LFO2LVL=127"]))
        if name == "P_L2RATE":
            alts.append(("LFO 2 free-running (SYNC off), matrix slot 1 to pitch", ["P_L2SYNC=0", "P_M1SRC=12", "P_M1DST=1", "P_M1AMT=63", "XW_BASE + X_LFO2LVL=127", "P_L2TRIG=0"]))
        alts.append(("LFO 2 routed to pitch in matrix slot 1 (MS_LFO2 12 -> MD_PITCH 1)", ["P_M1SRC=12", "P_M1DST=1", "P_M1AMT=63", "XW_BASE + X_LFO2LVL=127", "P_L2TRIG=0"]))
    if name in G_ENV2:
        alts.append(("ENV 2 to cutoff", ["P_E2FLT=63"]))
        alts.append(("ENV 2 routed to cutoff in matrix slot 1 (MS_ENV2 11 -> MD_CUT 2)", ["P_M1SRC=11", "P_M1DST=2", "P_M1AMT=63"]))
        if name == "P_E2REL":                # b9: RELEASE 2 starts from the level ENV 2 has when the key goes up; the factory sounds' SUSTAIN 2 is 0, so there is nothing to release
            alts.append(("ENV 2 to cutoff down, SUSTAIN 2 up, AMP REL long", ["P_E2FLT=-63", "P_E2SUS=127", "P_REL=127", "P_FREEA=40"]))
    if name in G_INS or name == "P_ITYPE":
        for v in range(1, desc.get("P_ITYPE", (0, 6, 0, 0))[1] + 1):
            alts.append(("INSERT type %d, mix full" % v, ["P_ITYPE=%d" % v, "P_IMIX=127"]))
    if name in ("P_EQLF", "P_EQLG"):
        alts.append(("EQ low band up", ["P_EQLG=12", "P_EQLF=-24"]))
    if name in ("P_EQMF", "P_EQMQ", "P_EQMG"):
        alts.append(("EQ mid band up", ["P_EQMG=12", "P_EQMF=0"]))
    if name in ("P_EQHF", "P_EQHG"):
        alts.append(("EQ high band up", ["P_EQHG=12", "P_EQHF=-24"]))
    if name in G_ARP:
        alts.append(("ARP on (mode 1), transport on", ["P_AMODE=1", "--transport on"]))
    if name == "P_AORDER":
        alts.append(("ARP on, keys pressed in turn, high to low", ["P_AMODE=1", "--transport on", "--keys chord:G3,Eb3,C3", "--keys-turn 150"]))
    if name == "P_TRANS":
        alts.append(("keys as pressed (TRN not compensated)", ["--keys-raw"]))
    if name == "P_PRIO":
        alts.append(("MONO voice, keys pressed in turn, high to low", ["P_VOICE=1", "--keys chord:G3,Eb3,C3", "--keys-turn 150"]))
    if name in ("P_VOIC", "XW_BASE + X_STRUM"):
        alts.append(("POLY voice, chord keys on (CHRD 1)", ["P_VOICE=0", "P_CHRD=1"]))
    if name in ("XW_BASE + X_SUBWAVE", "XW_BASE + X_SUBOCT"):
        alts.append(("SUB OSC level up", ["XW_BASE + X_SUBLVL=100"]))
    if name in ("XW_BASE + X_CYMODE", "XW_BASE + X_CYFALL", "XW_BASE + X_CYSYNC", "XW_BASE + X_CYRISE", "XW_BASE + X_CYCUT2"):
        alts.append(("CYCLE ENV routed to cutoff in matrix slot 1 (MS_CYCLE 15 -> MD_CUT 2)", ["P_M1SRC=15", "P_M1DST=2", "P_M1AMT=63"]))
        if name in ("XW_BASE + X_CYFALL", "XW_BASE + X_CYMODE"):   # b9: a factory sound can run a slow, tempo-synced cycle (WAVE: RISE 113, FALL 113): the fall never comes in one bar. Short RISE, SYNC off, to pitch
            alts.append(("CYCLE ENV to pitch, RISE 0, SYNC off", ["XW_BASE + X_CYPIT=63", "XW_BASE + X_CYRISE=0", "XW_BASE + X_CYSYNC=0", "XW_BASE + X_CYFALL=20"]))
        alts.append(("CYCLE ENV to TWIN CUTOFF (TWIN FILTER on)", ["P_TWTYPE=1", "P_CUT2=-20", "XW_BASE + X_CYCUT2=63"]))
    if name in ("P_CHRD", "P_SPRD"):
        alts.append(("POLY voice", ["P_VOICE=0"]))
    if name in ("P_DETUNE", "P_SPRD"):
        alts.append(("UNISON voice", ["P_VOICE=3"]))
    if name == "P_PRIO":
        alts.append(("MONO voice", ["P_VOICE=1"]))
    if name == "P_GLMODE":
        alts.append(("MONO voice, glide up", ["P_VOICE=1", "P_GLIDE=60"]))
    if name in G_TWIN:
        alts.append(("TWIN FILTER on, CUTOFF 2 low", ["P_TWTYPE=1", "P_CUT2=-20"]))
        alts.append(("TWIN FILTER on, CUTOFF 2 low, ENV 2 sustained, LFO 2 level up", ["P_TWTYPE=1", "P_CUT2=-20", "P_E2SUS=127", "XW_BASE + X_LFO2LVL=127", "P_L2TRIG=0"]))
    if name == "P_TWTYPE":
        alts.append(("CUTOFF 2 low", ["P_CUT2=-20"]))
    if name == "P_CUT2":
        alts.append(("TWIN FILTER on", ["P_TWTYPE=1"]))
    if name in SHAPE:
        for v in E0:
            alts.append(("wave slot E0=%d" % v, ["P_E0=%d" % v] + SHAPE[name]))
    if name in ("P_E1", "P_E2", "P_E3", "P_E4", "P_E5", "P_E6", "P_E7"):
        for v in E0:
            alts.append(("wave slot E0=%d" % v, ["P_E0=%d" % v]))
    return alts


class Runner:
    def __init__(self, exe):
        self.exe = exe
        self.work = os.path.join(FB, "build", "soundlab", "dead_work")
        os.makedirs(self.work, exist_ok=True)

    def run(self, *a):
        r = subprocess.run([self.exe] + list(a), capture_output=True, text=True)
        return r.returncode, r.stdout.strip(), r.stderr.strip()

    def render(self, tag, sound, keys, sets):
        out = os.path.join(self.work, tag)
        args = ["render", "--sound", str(sound), "--keys", keys, "--bars", "1", "--ringout", "1", "--out", out]
        for s in sets:
            if s.startswith("--"):
                args += s.split()
            else:
                args += ["--p", s]
        rc, o, e = self.run(*args)
        return out + ".wav" if rc == 0 else None

    def diff(self, a, b):
        rc, o, e = self.run("wavdiff", a, b)
        if rc != 0 or not o:
            return None
        t = o.split()
        if t[0] == "same":
            return (0.0, 0.0, float(t[1]))
        return (max(abs(float(t[2])), 1e-6), max(abs(float(t[3])), 1e-6), float(t[5]))     # (b9: samples differ, so not DEAD even when the change prints as 0.000 dB: NEAR)


def main():
    a = sys.argv[1:]
    exe = os.path.join(FB, "build", "soundlab", "soundlab")
    out = os.path.join(FB, "build", "soundlab", "deadparams.tsv")
    only = None
    only_p = None
    i = 0
    while i < len(a):
        if a[i] == "--exe":
            exe = a[i + 1]; i += 2
        elif a[i] == "--engine":
            only = a[i + 1].upper(); i += 2
        elif a[i] == "--out":
            out = a[i + 1]; i += 2
        elif a[i] == "--param":
            only_p = a[i + 1].split(","); i += 2
        else:
            sys.exit("usage: soundlab deadparams [--engine NAME] [--out FILE.tsv]")
    R = Runner(exe)
    rc, txt, err = R.run("params")
    if rc != 0:
        sys.exit("soundlab params failed: " + err)
    P, ids = load_names()
    pages = load_pages(ids)
    name_of = {}
    for k, v in ids.items():
        name_of.setdefault(v, k)
    untested = {ids[n] for n in UNTESTED if n in ids}

    def sym(x):                      # "P_LD_PIT=63" -> "#<id>=63" (--p takes a label or #id); options pass through
        if x.startswith("--"):
            return x
        k, v = x.rsplit("=", 1)
        return "#%d=%s" % (ids[k], v)

    lines = txt.splitlines()
    blocks, cur = [], None
    for ln in lines:
        t = ln.split("\t")
        if t[0] == "ENG":
            cur = ((t[2], int(t[3])), [])
            blocks.append(cur)
        elif cur is not None:
            cur[1].append(t)
    rows = []
    with open(out, "w", newline="\n") as f:
        f.write("engine\tid\tname\tlabel\tpages\tmin\tmax\tverdict\trms_db\tcentroid_oct\ttested_in\n")
        for eng, plist in blocks:
            if only and eng[0] != only:
                continue
            print("engine", eng[0], file=sys.stderr)
            desc = {name_of.get(int(t[2]), "#" + t[2]): (int(t[4]), int(t[5]), int(t[6]), int(t[7])) for t in plist}
            for t in plist:
                pid, label, mn, mx = int(t[2]), t[3], int(t[4]), int(t[5])
                pg = "/".join(pages.get(pid, [])) or "-"
                nm = name_of.get(pid, "#%d" % pid)
                if only_p and nm not in only_p:
                    continue
                if pid in untested:
                    row = (eng[0], pid, nm, label, pg, mn, mx, "UNTESTED", "", "", UNTESTED_WHY)
                else:
                    tries = [("note C3", "note:C3", []), ("chord C minor", "chord:C3,Eb3,G3", [])]
                    for lab, ctx in alternatives(nm, desc):
                        ctx = [sym(x) for x in ctx if x.startswith("--") or x.rsplit("=", 1)[0] != nm]
                        tries.append((lab + ", note", "note:C3", ctx))
                        tries.append((lab + ", chord", "chord:C3,Eb3,G3", ctx))
                    worst, where, loud = (0.0, 0.0), "", -200.0
                    for tn, keys, ctx in tries:
                        wa = R.render("lo", eng[1], keys, ctx + ["#%d=%d" % (pid, mn)])
                        wb = R.render("hi", eng[1], keys, ctx + ["#%d=%d" % (pid, HI_OVERRIDE.get(nm, mx))])
                        d = R.diff(wa, wb) if wa and wb else None
                        if d is None:
                            continue
                        loud = max(loud, d[2])
                        if (d[0], d[1]) > worst:
                            worst, where = (d[0], d[1]), tn
                        if (d[0], d[1]) != (0.0, 0.0):
                            break                    # it moved: the other contexts are not needed
                    if worst == (0.0, 0.0):
                        v = "DEAD" if loud > -80 else "SILENT"
                    elif worst[0] < 0.5 and worst[1] < 1.0 / 12.0:
                        v = "NEAR"
                    else:
                        v = "LIVE"
                    row = (eng[0], pid, nm, label, pg, mn, mx, v, "%.2f" % worst[0], "%.3f" % worst[1],
                           where or "no difference in %d pairs of renders (%s ...)" % (len(tries), "; ".join(x[0] for x in tries[:5])))
                f.write("\t".join(str(x) for x in row) + "\n")
                f.flush()
                rows.append(row)
    n = {k: sum(1 for r in rows if r[7] == k) for k in ("DEAD", "NEAR", "LIVE", "SILENT", "UNTESTED")}
    print("deadparams: %d rows %s -> %s" % (len(rows), n, out))


if __name__ == "__main__":
    main()
