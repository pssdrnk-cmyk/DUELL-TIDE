#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""soundlab atlas: the measured profile of every factory sound, multi and pattern (stdlib only; the renders and numbers are soundlab's own).

  soundlab atlas [--only multis|sounds|patterns] [--sound-list 1,2,3] [--out DIR]     build (default DIR: tools/soundlab/atlas)
  soundlab atlas --find "pad dark sub"   or   --find "centroid<500 sub>10%"           query the built atlas

Writes DIR/sounds.json, multis.json, patterns.json, INDEX.md and atlas.html (one self-contained page, sortable and filterable).
No clock, no random: the same tree gives the same bytes. The thresholds of the tags are in TAGS below and in README.md."""
import json, os, re, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
FB = os.path.abspath(os.path.join(HERE, "..", ".."))
STYLE = [0, 0, 1, 1, 1, 4, 4, 1, 2, 2, 2, 2, 4, 3, 1, 3, 5, 5, 2, 0, 1, 4, 3, 2]        # the suggested TIDE STYLE of each multi (0 DUB 1 DEEP 2 MINIMAL 3 AMBIENT 4 PEAK 5 DRONE)
STYLE_NAME = ["DUB", "DEEP", "MINIMAL", "AMBIENT", "PEAK", "DRONE"]
NSOUNDS = 171
FIELDS = [      # name -> JSON path in PREFIX.metrics.json (the unit is in README.md / METRICS.md)
    ("lufs", "level.lufs"), ("true_peak", "level.true_peak_dbfs"), ("rms", "level.rms_dbfs"),
    ("centroid", "spectrum.centroid_hz"), ("sub", "spectrum.bands_pct.lt60"), ("lt150", "spectrum.bands_pct.lt150"),
    ("b150_400", "spectrum.bands_pct.150_400"), ("gt2k", "spectrum.bands_pct.gt2k"), ("gt4k", "spectrum.bands_pct.gt4k"),
    ("side_mid", "space.side_mid_db"), ("tail", "space.after_noteoff.energy_pct"), ("decay_ms", "space.hits.decay_to_minus20db_ms_median"),
    ("attack_ms", "parts.p1.attack_ms"), ("odd", "parts.p1.odd_harmonic_share_pct"), ("partials", "parts.p1.harmonics_within_30db"),
    ("bed", "time.bed.both_mean"), ("silence", "time.silence_notes_pct"), ("onsets_bar", "time.onsets_per_bar.all"),
    ("low_onsets", "time.low_onsets_per_beat"), ("p_median", "pitch.p1.median"), ("p_highest", "pitch.p1.highest"),
    ("whistle", "spectrum.whistle.spike_db"), ("echo_hf", "space.echo.hf_drop_db_per_repeat"),
]
ALIAS = {"cent": "centroid", "under150": "lt150", "bass": "lt150", "air": "gt4k", "width": "side_mid", "lev": "lufs", "tp": "true_peak"}


def get(m, path):
    for k in path.split("."):
        if not isinstance(m, dict) or k not in m:
            return None
        m = m[k]
    return m


def rnd(v):
    if isinstance(v, float):
        return round(v, 2) if abs(v) < 1e6 else None
    if isinstance(v, list):
        return [rnd(x) for x in v]
    return v


def tags_of(r, kind):
    """tags from the numbers (README.md 'Tags'): thresholds written here once"""
    t = []
    c, dec, att, sus = r.get("centroid"), r.get("decay_ms"), r.get("attack_ms"), None
    if kind == "sound":
        # decay_ms stops at about 1500 ms (the metric's window): a value near it means "never fell 20 dB"; a slow attack makes the decay figure meaningless
        sus = (dec is not None and dec >= 1400) or (att is not None and att >= 100)
        if not sus and dec is not None and dec < 400:
            t.append("stab")
        elif not sus and dec is not None:
            t.append("pluck")
        if sus and att is not None and att >= 100:
            t.append("pad")
        elif sus:
            t.append("held")
        if c is not None and c < 250 and (r.get("lt150") or 0) >= 50:
            t.append("bass")
        if sus and "bass" not in t and c is not None and c >= 600:
            t.append("lead")
        if (r.get("partials") or 0) >= 10 and (r.get("gt4k") or 0) >= 20:
            t.append("texture")
        if r.get("odd") is not None and r["odd"] >= 70:
            t.append("square")
    elif kind == "multi":
        if (r.get("bed") or 0) >= 2.5:
            t.append("bed")
        if (r.get("onsets_bar") or 0) >= 8:
            t.append("busy")
    if c is not None and c < 500:
        t.append("dark")
    if c is not None and c > 1500:
        t.append("bright")
    if (r.get("sub") or 0) >= 10:
        t.append("sub")
    if r.get("tail") is not None:
        if r["tail"] >= 30:
            t.append("wet")
        elif r["tail"] < 5:
            t.append("dry")
    return t


class Lab:
    def __init__(self, exe, work):
        self.exe, self.work = exe, work
        os.makedirs(work, exist_ok=True)

    def run(self, *args):
        r = subprocess.run([self.exe] + list(args), capture_output=True, text=True)
        if r.returncode != 0:
            sys.stderr.write("soundlab %s: %s\n" % (" ".join(args), (r.stderr or r.stdout).strip()))
            return False
        return True

    def one(self, tag, render_args, held=False):
        p = os.path.join(self.work, tag)
        if not self.run("render", *render_args, "--out", p) or not self.run("measure", p):
            return None, None
        m = json.load(open(p + ".metrics.json"))
        j = json.load(open(p + ".json"))
        row = {k: rnd(get(m, path)) for k, path in FIELDS}
        if held and self.run("measure", p, "--from", "0", "--to", "1"):     # the envelope figures of the held part only (bar 0 to 1): after the key-up every sound decays
            m2 = json.load(open(p + ".metrics.json"))
            for k in ("decay_ms", "attack_ms", "odd", "partials"):
                row[k] = rnd(get(m2, dict(FIELDS)[k]))
        for ext in (".wav", ".json", ".metrics.json"):
            try:
                os.remove(p + ext)
            except OSError:
                pass
        return row, j


def build_sounds(lab, ids):
    out = []
    variants = [("note_A", ["--keys", "note:C3", "--morph", "a"]), ("chord_A", ["--keys", "chord:C3,Eb3,G3", "--morph", "a"]), ("note_B", ["--keys", "note:C3", "--morph", "b"])]
    for n in ids:
        e = {"n": n, "name": None, "engine": None, "variants": {}}
        for vn, va in variants:
            row, j = lab.one("s%d_%s" % (n, vn), ["--sound", str(n), "--bars", "1", "--ringout", "1.5"] + va, held=(vn == "note_A"))
            if row is None:
                continue
            if e["name"] is None:
                e["name"] = j["source"]["part_names"][0].split(" ", 1)[-1]
                e["engine"] = j["source"]["engines"][0]
            row["tags"] = tags_of(row, "sound" if vn == "note_A" else "sound-fx")
            e["variants"][vn] = row
        out.append(e)
        print("sound %d %s" % (n, e["name"]), file=sys.stderr)
    return out


def build_multis(lab):
    out = []
    for k in range(1, 25):
        st = STYLE[k - 1]
        e = {"n": k, "name": None, "style": STYLE_NAME[st], "variants": {}}
        for vn, va in (("play", []), ("tide", ["--tide", "evolve", "--style", STYLE_NAME[st]]), ("part1", ["--solo", "1"]), ("part2", ["--solo", "2"])):
            row, j = lab.one("m%d_%s" % (k, vn), ["--multi", str(k), "--bars", "4"] + va)
            if row is None:
                continue
            if e["name"] is None:
                e["name"] = j["source"]["name"]
                e["parts"] = j["source"]["part_names"]
            row["tags"] = tags_of(row, "multi")
            e["variants"][vn] = row
        out.append(e)
        print("multi %d %s" % (k, e["name"]), file=sys.stderr)
    return out


def build_patterns():
    src = open(os.path.join(FB, "firmware", "src", "engines.c"), encoding="utf-8", errors="replace").read()
    a = src.index("} PATTERNS[] = {")
    b = src.index("};", a)
    table = re.findall(r'\{"([^"]+)",\s*\{(\d+)u,\s*(\d+)u,\s*(\d+)u\},\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\}', src[a:b])
    a2 = src.index("PAT_NOTES[] = {")
    b2 = src.index("};", a2)
    body = re.sub(r"/\*.*?\*/", "", src[a2 + len("PAT_NOTES[] = {"):b2], flags=re.S)
    toks = [t.strip() for t in body.replace("\n", " ").split(",") if t.strip()]
    root = re.search(r"#define ROOT\s+(\d+)", src)
    out, o = [], 0
    for i, (name, f0, f1, f2, ln, dv, sw, gt) in enumerate(table):
        ln = int(ln)
        steps = toks[o:o + ln]
        o += ln
        offs = []
        for s in steps:
            mm = re.fullmatch(r"O\((-?\d+)\)", s)
            offs.append(int(mm.group(1)) if mm else (None if s == "0" else 0))
        hit = [x for x in offs if x is not None]
        n = len(hit)
        e = {"n": i + 1, "name": name, "len": ln, "div": int(dv), "swing": int(sw) or 50, "gate": int(gt) or 64, "notes": n,
             "density": round(n / ln, 2), "low": min(hit) if hit else None, "high": max(hit) if hit else None,
             "span": (max(hit) - min(hit)) if hit else 0, "accents": bin(int(f0)).count("1"), "slides": bin(int(f1)).count("1"), "ties": bin(int(f2)).count("1"),
             "steps": ["." if x is None else x for x in offs]}
        t = []
        if e["density"] < 0.3:
            t.append("sparse")
        if e["density"] > 0.6:
            t.append("busy")
        if hit and min(hit) <= -7:
            t.append("bassline")
        if e["span"] >= 12:
            t.append("wide")
        if e["ties"]:
            t.append("tied")
        if e["swing"] > 50:
            t.append("swung")
        e["tags"] = t
        out.append(e)
    if o != len(toks):
        sys.stderr.write("atlas: pattern table reads %d note bytes, PAT_NOTES has %d\n" % (o, len(toks)))
    return out


# ---- query -----------------------------------------------------------------------------------------------------------------------------------
def rows_of(atlas):
    for kind in ("sounds", "multis"):
        for e in atlas.get(kind, []):
            for vn, r in e["variants"].items():
                d = dict(r)
                d.update(kind=kind[:-1], n=e["n"], name=e["name"], variant=vn)
                yield d
    for e in atlas.get("patterns", []):
        d = dict(e)
        d.update(kind="pattern", variant="")
        yield d


def match(row, terms):
    for t in terms:
        m = re.fullmatch(r"([a-z_0-9]+)(<=|>=|<|>|=)(-?[0-9.]+)%?", t)
        if m:
            k = ALIAS.get(m.group(1), m.group(1))
            v = row.get(k)
            if not isinstance(v, (int, float)):
                return False
            x = float(m.group(3))
            if not {"<": v < x, ">": v > x, "<=": v <= x, ">=": v >= x, "=": v == x}[m.group(2)]:
                return False
        elif t not in row.get("tags", []) and t not in (row.get("kind"), row.get("variant")):
            return False
    return True


def find(adir, query):
    atlas = {}
    for k in ("sounds", "multis", "patterns"):
        p = os.path.join(adir, k + ".json")
        if os.path.exists(p):
            atlas[k] = json.load(open(p))
    terms = query.replace(",", " ").split()
    n = 0
    for r in rows_of(atlas):
        if match(r, terms):
            n += 1
            if r["kind"] == "pattern":
                print("pattern %2d %-18s %2d steps, %d notes, density %.2f, %s..%s  %s" % (r["n"], r["name"], r["len"], r["notes"], r["density"], r["low"], r["high"], " ".join(r["tags"])))
            else:
                print("%-6s %3d %-16s %-8s centroid %5s Hz  sub %5s%%  <150 %5s%%  >4k %5s%%  LUFS %6s  tail %5s%%  %s" % (
                    r["kind"], r["n"], r["name"], r["variant"], r["centroid"], r["sub"], r["lt150"], r["gt4k"], r["lufs"], r["tail"], " ".join(r["tags"])))
    print("%d match(es) for: %s" % (n, query))
    return 0 if n else 1


# ---- output ----------------------------------------------------------------------------------------------------------------------------------
HTML = """<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>soundlab atlas</title>
<style>body{font:13px/1.4 system-ui,sans-serif;margin:12px;background:#fff;color:#111}input{font:inherit;padding:4px 8px;width:min(34em,95%)}table{border-collapse:collapse;margin-top:8px}
th,td{border-bottom:1px solid #ddd;padding:2px 8px;text-align:right;white-space:nowrap}th{cursor:pointer;position:sticky;top:0;background:#eee}td.l,th.l{text-align:left}
tr:hover{background:#f3f7ff}.t{color:#556}small{color:#666}@media(prefers-color-scheme:dark){body{background:#181a1f;color:#e6e6e6}th{background:#2a2d35}td{border-color:#333}tr:hover{background:#242b3a}.t{color:#9ab}small{color:#999}}</style></head><body>
<h3>soundlab atlas <small>sounds, multis, patterns (measured by soundlab; tags from the numbers; see README.md)</small></h3>
<p><input id="q" placeholder="filter: pad dark sub  /  centroid&lt;500 sub&gt;10%  /  multi play" autofocus> <small id="c"></small></p>
<div style="overflow:auto"><table id="t"><thead></thead><tbody></tbody></table></div>
<script>var DATA=__DATA__;var COLS=[["kind","kind",1],["n","#",0],["name","name",1],["variant","variant",1],["lufs","LUFS",0],["true_peak","TP",0],["centroid","centroid Hz",0],["sub","<60 Hz %",0],["lt150","<150 %",0],["gt2k",">2k %",0],["gt4k",">4k %",0],["side_mid","side/mid dB",0],["tail","tail %",0],["decay_ms","decay ms",0],["attack_ms","attack ms",0],["odd","odd %",0],["bed","bed",0],["onsets_bar","on/bar",0],["len","steps",0],["density","density",0],["low","low",0],["high","high",0],["swing","swing",0],["tags","tags",1]];
var AL={cent:"centroid",under150:"lt150",bass:"lt150",air:"gt4k",width:"side_mid",lev:"lufs",tp:"true_peak"};var sk=null,sd=1;
function v(r,k){var x=r[k];return k=="tags"?(x||[]).join(" "):(x==null?"":(x<-200?-60:x))}
function ok(r,ts){for(var i=0;i<ts.length;i++){var t=ts[i],m=/^([a-z_0-9]+)(<=|>=|<|>|=)(-?[0-9.]+)%?$/.exec(t);if(m){var k=AL[m[1]]||m[1],x=r[k];if(typeof x!="number")return false;var y=parseFloat(m[3]);var o=m[2];if(!(o=="<"?x<y:o==">"?x>y:o=="<="?x<=y:o==">="?x>=y:x==y))return false}else if((r.tags||[]).indexOf(t)<0&&t!=r.kind&&t!=r.variant)return false}return true}
function draw(){var ts=document.getElementById("q").value.toLowerCase().replace(/,/g," ").split(/\\s+/).filter(Boolean);var rs=DATA.filter(function(r){return ok(r,ts)});
if(sk){rs.sort(function(a,b){var x=a[sk],y=b[sk];if(x==null)return 1;if(y==null)return -1;return(x<y?-1:x>y?1:0)*sd})}
var th="<tr>"+COLS.map(function(c){return"<th class='"+(c[2]?"l":"")+"' data-k='"+c[0]+"'>"+c[1]+"</th>"}).join("")+"</tr>";document.querySelector("thead").innerHTML=th;
document.querySelector("tbody").innerHTML=rs.map(function(r){return"<tr>"+COLS.map(function(c){return"<td class='"+(c[2]?"l":"")+(c[0]=="tags"?" t":"")+"'>"+v(r,c[0])+"</td>"}).join("")+"</tr>"}).join("");
document.getElementById("c").textContent=rs.length+" of "+DATA.length;
document.querySelectorAll("th").forEach(function(h){h.onclick=function(){var k=h.getAttribute("data-k");sd=(sk==k)?-sd:1;sk=k;draw()}})}
document.getElementById("q").oninput=draw;draw();</script></body></html>
"""


def write_out(adir, atlas):
    os.makedirs(adir, exist_ok=True)
    for k, v in atlas.items():
        with open(os.path.join(adir, k + ".json"), "w", newline="\n") as f:
            json.dump(v, f, indent=0, sort_keys=False)
            f.write("\n")
    rows = list(rows_of(atlas))
    with open(os.path.join(adir, "atlas.html"), "w", newline="\n") as f:
        f.write(HTML.replace("__DATA__", json.dumps(rows, separators=(",", ":")).replace("</", "<\\/")))
    L = ["# soundlab atlas (generated by `soundlab atlas`; do not edit)", "",
         "Open `atlas.html` (sortable, filterable) or run `soundlab atlas --find \"pad dark sub\"`. Numbers are soundlab's `measure`; tags from the thresholds in README.md.", ""]
    if "sounds" in atlas:
        L += ["## Sounds (C3 held, End A; render 1 bar + 1.5 s ring-out)", "", "| # | name | engine | centroid Hz | <60 Hz % | <150 % | >4k % | LUFS | tail % | attack ms | tags |", "|---|---|---|---|---|---|---|---|---|---|---|"]
        for e in atlas["sounds"]:
            r = e["variants"].get("note_A")
            if r:
                L.append("| %d | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (e["n"], e["name"], e["engine"], r["centroid"], r["sub"], r["lt150"], r["gt4k"], r["lufs"], r["tail"], r["attack_ms"], " ".join(r["tags"])))
        L.append("")
    if "multis" in atlas:
        L += ["## Multis (PLAY alone, no key, 4 bars)", "", "| # | name | style | centroid Hz | <60 Hz % | <150 % | >4k % | side/mid dB | LUFS | bed | tags |", "|---|---|---|---|---|---|---|---|---|---|---|"]
        for e in atlas["multis"]:
            r = e["variants"].get("play")
            if r:
                L.append("| M%02d | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (e["n"], e["name"], e["style"], r["centroid"], r["sub"], r["lt150"], r["gt4k"], max(r["side_mid"] or -60, -60), r["lufs"], r["bed"], " ".join(r["tags"])))
        L.append("")
    if "patterns" in atlas:
        L += ["## Patterns (engines.c PATTERNS)", "", "| # | name | steps | notes | density | low..high (semitones from the key) | swing | gate | tags |", "|---|---|---|---|---|---|---|---|---|"]
        for e in atlas["patterns"]:
            L.append("| %d | %s | %d | %d | %s | %s..%s | %d | %d | %s |" % (e["n"], e["name"], e["len"], e["notes"], e["density"], e["low"], e["high"], e["swing"], e["gate"], " ".join(e["tags"])))
    with open(os.path.join(adir, "INDEX.md"), "w", newline="\n") as f:
        f.write("\n".join(L) + "\n")


def main():
    a = sys.argv[1:]
    adir = os.path.join(HERE, "atlas")
    exe = None
    only = "all"
    ids = list(range(1, NSOUNDS + 1))
    query = None
    i = 0
    while i < len(a):
        if a[i] == "--out":
            adir = os.path.abspath(a[i + 1]); i += 2
        elif a[i] == "--exe":
            exe = a[i + 1]; i += 2
        elif a[i] == "--only":
            only = a[i + 1]; i += 2
        elif a[i] == "--sound-list":
            ids = [int(x) for x in a[i + 1].split(",")]; i += 2
        elif a[i] == "--retag":
            only = "retag"; i += 1
        elif a[i] == "--find":
            query = a[i + 1]; i += 2
        else:
            sys.exit("usage: soundlab atlas [--only multis|sounds|patterns] [--sound-list 1,2,3] [--out DIR] | --find \"pad dark sub\"")
    if query is not None:
        sys.exit(find(adir, query))
    if only == "retag":                                    # the tags again from the stored numbers (no render): after a change of tags_of
        atlas = {k: json.load(open(os.path.join(adir, k + ".json"))) for k in ("sounds", "multis", "patterns") if os.path.exists(os.path.join(adir, k + ".json"))}
        for e in atlas.get("sounds", []):
            for vn, r in e["variants"].items():
                r["tags"] = tags_of(r, "sound" if vn == "note_A" else "sound-fx")
        for e in atlas.get("multis", []):
            for r in e["variants"].values():
                r["tags"] = tags_of(r, "multi")
        write_out(adir, atlas)
        print("atlas retagged in %s" % adir)
        return
    if only not in ("all", "multis", "sounds", "patterns"):
        sys.exit("atlas: --only multis|sounds|patterns")
    exe = exe or os.path.join(FB, "build", "soundlab", "soundlab")
    work = os.path.join(FB, "build", "soundlab", "atlas_work")
    lab = Lab(exe, work)
    atlas = {}
    old = {}
    for k in ("sounds", "multis", "patterns"):                       # a partial run (--only) keeps the other parts of an earlier atlas
        p = os.path.join(adir, k + ".json")
        if os.path.exists(p):
            old[k] = json.load(open(p))
    if only in ("all", "sounds"):
        atlas["sounds"] = build_sounds(lab, ids)
    if only in ("all", "multis"):
        atlas["multis"] = build_multis(lab)
    if only in ("all", "patterns"):
        atlas["patterns"] = build_patterns()
    for k, v in old.items():
        atlas.setdefault(k, v)
    atlas = {k: atlas[k] for k in ("sounds", "multis", "patterns") if k in atlas}
    write_out(adir, atlas)
    shutil.rmtree(work, ignore_errors=True)
    print("atlas written to %s" % adir)


if __name__ == "__main__":
    main()
