#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Copyright (C) 2026 OSCILL/R (DUELL)
"""Make the DUELL with TIDE site (GitHub Pages) as a static folder:

  index.html                  landing page (web/landing.html)
  assets/                     the site's stylesheet, theme script and self-hosted fonts (web/assets/); no external requests
  img/                        the cover and the device screenshots (docs/duell-tide-cover.jpg, docs/tide/*.png)
  emu/                        the browser emulator (the output of web/emu/build.sh: build/emu), or a
                              "not attached yet" page
  install/index.html          the installer page (web/install/index.html)
  install/channels.json       what the installer offers: {"channels": {"stable": {...} | null, "beta": {...} | null}}; per channel: tag,
                              date, commit, file, size, sha256, product, notes_url, summary, known_issues (from the release notes)
  install/stable/duell-tide-stable-DATE.fwsc   the Stable package (+ .sha256), when there is one
  install/beta/duell-tide-beta-DATE.fwsc       the Beta package (+ .sha256), when there is one
                              (+ LICENSE, LICENSING.md, LICENSES/: the package holds JieLi SDK files under Apache-2.0, see LICENSING.md)
  fm1pkg.js fm1ota.js fm1backup.js installer_state.js installer_backup.js   imported by the installer page as "../fm1*.js" (installer_backup.js imports editor/duellbackup.js)
  cheatsheet/                 the printable one-page cheat sheet (web/cheatsheet/index.html)
  editor/                     the editor (web/editor/: index.html the sound editor, backup.html the Backup page, layout.json from the firmware tables, the modules; ?mock=1 needs mockunit.js); it imports ../fm1backup.js
                              and ../installer_state.js
  .nojekyll

  web/make_site.py [--stable-pkg FILE.fwsc [--stable-info FILE.json]] [--beta-pkg FILE.fwsc [--beta-info FILE.json]] [--emu DIR] [OUT_DIR]

  OUT_DIR       default: site/ at the repository root
  --stable-pkg  the Stable package, named duell-tide-stable-YYYY-MM-DD[.N].fwsc
  --beta-pkg    the Beta package, named duell-tide-beta-YYYY-MM-DD[.N].fwsc
  --*-info      the release data (web/releases.py writes it from the GitHub release): tag, commit, published_at, notes_url, body, sha256.
                Without it the tag and date come from the file name, and there is no commit, notes link or known-issues list.
                When it has a sha256, the package must match it, or the build stops.
  --emu         default: build/emu if it exists.
                Without it, /emu/ is a short page that says the emulator is not attached yet.
  Without a package the installer says the build is not released yet and hides its install flow.

A package must be one made by tools/fm1pkg_make.py (Felucca's own loader, no vendor files).
Its identity (FM-1_9xx) is read from the package. The installer checks the package's SHA-256 against channels.json in the browser before it writes anything.
"""
import argparse
import hashlib
import json
import re
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent          # felucca-base/web
BASE = HERE.parent                              # felucca-base
ROOT = BASE.parent                              # repository root
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


PKG_NAME = re.compile(r"duell-tide-(stable|beta)-(\d{4}-\d{2}-\d{2})(?:\.(\d+))?\.fwsc")
REPO = "pssdrnk-cmyk/DUELL-TIDE"


def plain(text):
    """release-notes markdown -> plain text (links keep their words; bold, italics and code marks go)"""
    text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
    return re.sub(r"[*_`]+", "", text).strip()


def summary_of(body):
    """the first paragraph of the release notes, as plain text"""
    para = re.split(r"\n\s*\n", (body or "").replace("\r\n", "\n").strip())[0]
    return plain(" ".join(para.split()))[:400]


def known_issues_of(body):
    """the bullets under a 'Known open issues' / 'Known issues' line of the release notes (plain text)"""
    lines = (body or "").replace("\r\n", "\n").split("\n")
    for n, line in enumerate(lines):
        if re.match(r"\s*\**\s*Known (open )?issues", line, re.I):
            out = []
            for item in lines[n + 1:]:
                if re.match(r"\s*[-*] ", item):
                    out.append(plain(re.sub(r"^\s*[-*] ", "", item)))
                elif item.strip() or out:
                    break              # the list ended (a blank line before the first bullet is skipped)
            return out
    return []


def check_package(pkg):
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Felucca/DUELL package (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:          # never publish a package with vendor files
        raise SystemExit(f"{pkg}: no Felucca loader in it; the site ships only fm1pkg_make.py packages "
                         "(a package patched from an official one carries vendor files)")
    return raw, product


def channel_entry(channel, pkg, info_path):
    """check one package and return (manifest entry, bytes). Stops the build on a wrong name, identity or SHA-256."""
    m = PKG_NAME.fullmatch(pkg.name)
    if not m or m[1] != channel:
        raise SystemExit(f"{pkg.name}: the {channel} package must be named duell-tide-{channel}-YYYY-MM-DD[.N].fwsc")
    raw, product = check_package(pkg)
    sha = hashlib.sha256(raw).hexdigest()
    info = json.loads(Path(info_path).read_text(encoding="utf-8")) if info_path else {}
    want = (info.get("sha256") or "").lower()
    if want and want != sha:
        raise SystemExit(f"{pkg.name}: SHA-256 {sha} is not the release's {want}; refusing to publish it")
    if info.get("size") and info["size"] != len(raw):
        raise SystemExit(f"{pkg.name}: {len(raw)} bytes, the release says {info['size']}; refusing to publish it")
    body = info.get("body") or ""
    entry = {
        "channel": channel,
        "tag": info.get("tag") or f"{channel}-{m[2]}",
        "version": m[2],
        "date": m[2],
        "commit": info.get("commit") or "",
        "file": f"{channel}/{pkg.name}",
        "size": len(raw),
        "sha256": sha,
        "sha256_from_release": bool(want),
        "product": product,
        "published_at": info.get("published_at") or "",
        "notes_url": info.get("notes_url") or f"https://github.com/{REPO}/releases/tag/{info.get('tag') or channel + '-' + m[2]}",
        "summary": summary_of(body),
        "known_issues": known_issues_of(body),
    }
    return entry, raw


def main():
    ap = argparse.ArgumentParser(description="Make the DUELL with TIDE site folder")
    ap.add_argument("out", nargs="?", default=str(ROOT / "site"))
    ap.add_argument("--stable-pkg")
    ap.add_argument("--stable-info")
    ap.add_argument("--beta-pkg")
    ap.add_argument("--beta-info")
    ap.add_argument("--emu")
    a = ap.parse_args()
    out = Path(a.out).resolve()
    emu = Path(a.emu) if a.emu else (BASE / "build" / "emu")
    for flag in ("stable_pkg", "beta_pkg"):
        if getattr(a, flag) and not Path(getattr(a, flag)).is_file():
            raise SystemExit(f"--{flag.replace('_', '-')} {getattr(a, flag)}: no such file")
    if a.emu and not (emu / "index.html").is_file():
        raise SystemExit(f"--emu {emu}: no index.html in it")
    if out in (ROOT, BASE, HERE) or out == Path(out.anchor):
        raise SystemExit(f"refusing to use {out} as the output folder")

    # check the packages first: a bad one stops the build before anything is written
    entries, blobs, notes = {"stable": None, "beta": None}, {}, []
    for channel in ("stable", "beta"):
        pkg = getattr(a, channel + "_pkg")
        if pkg:
            entries[channel], blobs[channel] = channel_entry(channel, Path(pkg), getattr(a, channel + "_info"))
    s_, b_ = entries["stable"], entries["beta"]
    def build_key(e):                                  # the date and .N in the package name; the publishing time only breaks a tie
        m = PKG_NAME.fullmatch(Path(e["file"]).name)
        return (m[2], int(m[3] or 0), e.get("published_at") or "")
    if s_ and b_ and build_key(b_) <= build_key(s_):
        notes.append(f"beta {b_['tag']} is not a newer build than stable {s_['tag']}: not offered")
        entries["beta"] = b_ = None

    if out.exists():
        shutil.rmtree(out)
    inst = out / "install"
    inst.mkdir(parents=True)
    (out / ".nojekyll").write_text("", encoding="utf-8")
    shutil.copy(HERE / "landing.html", out / "index.html")
    for js in ("fm1pkg.js", "fm1ota.js", "fm1backup.js", "installer_state.js", "installer_backup.js"):
        shutil.copy(HERE / js, out / js)
    shutil.copytree(HERE / "assets", out / "assets", ignore=shutil.ignore_patterns(".DS_Store"))
    img = out / "img"
    img.mkdir()
    for src in [ROOT / "docs" / "duell-tide-cover.jpg", *sorted((ROOT / "docs" / "tide").glob("*.png"))]:
        shutil.copy(src, img / src.name)

    shutil.copytree(HERE / "editor", out / "editor", ignore=shutil.ignore_patterns(".DS_Store", "*.wav"))   # (a test WAV of the librarian is not part of the site)
    shutil.copytree(HERE / "cheatsheet", out / "cheatsheet", ignore=shutil.ignore_patterns(".DS_Store"))

    # installer page, the packages and the manifest the page reads
    shutil.copy(HERE / "install" / "index.html", inst / "index.html")
    for channel, entry in entries.items():
        if entry:
            (inst / channel).mkdir()
            name = entry["file"].split("/")[1]
            (inst / entry["file"]).write_bytes(blobs[channel])
            (inst / (entry["file"] + ".sha256")).write_text(f"{entry['sha256']} *{name}\n", encoding="utf-8")
            notes.append(f"{channel}: {name} ({entry['size']} B, {entry['product']}), tag {entry['tag']}, commit {entry['commit'] or '?'}, "
                         f"sha256 {entry['sha256'][:12]}.. ({'checked against the release' if entry['sha256_from_release'] else 'no release SHA-256 given'})")
        else:
            notes.append(f"{channel}: none")
    manifest = {"schema": 1, "repo": REPO, "generated": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                "channels": {c: ({k: v for k, v in e.items() if k != "sha256_from_release"} if e else None) for c, e in entries.items()}}
    (inst / "channels.json").write_text(json.dumps(manifest, indent=1) + "\n", encoding="utf-8")
    lic = BASE / "LICENSES"        # the package holds JieLi SDK files (Apache-2.0): the licence texts travel with it
    (inst / "LICENSES").mkdir()
    names = sorted(f.name for f in lic.glob("*.txt"))
    for n in names:
        shutil.copy(lic / n, inst / "LICENSES" / n)
    (inst / "LICENSES" / "index.html").write_text(     # Pages lists no folders
        '<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">'
        '<title>Licence texts - DUELL with TIDE</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a> · '
          f'<a href="https://github.com/{REPO}">Source code (DUELL with TIDE)</a> · '
          '<a href="https://github.com/oscillr/DUELL">DUELL by OSCILL/R</a></p>\n', encoding="utf-8")
    for doc, src in (("LICENSE", ROOT / "LICENSE"), ("LICENSING.md", BASE / "LICENSING.md")):
        shutil.copy(src, inst / doc)

    # emulator
    if (emu / "index.html").is_file():
        shutil.copytree(emu, out / "emu", ignore=shutil.ignore_patterns("native.f32", "native_check*", ".DS_Store"))
        enote = f"from {emu}"
    else:
        (out / "emu").mkdir()
        (out / "emu" / "index.html").write_text(
            '<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">'
            "<title>Emulator - DUELL with TIDE</title>"
            '<body style="font:16px/1.6 -apple-system,Helvetica,Arial,sans-serif;max-width:600px;margin:48px auto;padding:0 16px">'
            "<h1>DUELL with TIDE</h1><p>TIDE by pssdrnk, built on DUELL 1.1 by OSCILL/R.</p>"
            "<p><b>The browser emulator is not attached yet.</b> It appears here when the site is built with it.</p>"
            f'<p><a href="../">Back</a> · <a href="https://github.com/{REPO}">Source code</a> · '
            f'<a href="https://github.com/{REPO}/releases">Releases</a></p></body></html>\n', encoding="utf-8")
        enote = "no emulator: placeholder page"
    print(f"site: {out}\n  installer: " + "\n             ".join(notes) + f"\n  emu: {enote}")


if __name__ == "__main__":
    sys.exit(main())
