#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Pick the releases the DUELL with TIDE site publishes, and download their packages (the Pages workflow runs this).

  releases.py --repo OWNER/NAME --out DIR [--releases-json FILE] [--no-download]

Two channels, from the repository's GitHub releases (drafts never count):

  stable  the highest version among the normal (non-pre-release) releases that carry duell-tide-stable-X.Y.Z.fwsc (TIDE's version, file TIDE_VERSION; tag tide-X.Y.Z). Mark it GitHub's "Latest".
  beta    the newest pre-release that carries duell-tide-beta-X.Y.Z-bN.fwsc (tag tide-X.Y.Z-bN) AND is a newer build than that stable one (the version in the package
          name: 1.0.0-b2 is after 1.0.0-b1 and before 1.0.0; the publishing time only breaks a tie).
          An older beta is a superseded build: it is not offered. With no stable release at all, the newest beta is offered alone.
  The names from before TIDE had versions (duell-tide-stable-YYYY-MM-DD[.N].fwsc, duell-tide-beta-YYYY-MM-DD[.N].fwsc, tags stable-YYYY-MM-DD, beta-YYYY-MM-DD)
  still count, and every one of them is older than any versioned build.

For each channel found it downloads the package and its .sha256 into DIR/<channel>/ and writes DIR/<channel>.json
(the release data make_site.py turns into install/channels.json: tag, date, commit, SHA-256, release notes URL, notes body).
The SHA-256 comes from the release's .sha256 asset and from GitHub's own digest of the asset; when both exist they must agree.
Nothing found is not an error: the site then shows "not released yet" for that channel.

--releases-json FILE reads `gh api repos/OWNER/NAME/releases` output instead of calling gh (tests, offline runs).
--no-download only prints the choice (no files, no gh download or commit lookup).
"""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

PKG = re.compile(r"duell-tide-(stable|beta)-(?:(\d{4}-\d{2}-\d{2})(?:\.(\d+))?|(\d+)\.(\d+)\.(\d+)(?:-b(\d+))?)\.fwsc")
SHA = re.compile(r"\b([0-9a-fA-F]{64})\b")


def _when(rel):
    return rel.get("published_at") or rel.get("created_at") or ""


def parse_package(name):
    """a package name -> {channel, version, date, key}, or None when it is not one. A versioned name (duell-tide-beta-1.0.0-b1.fwsc,
    duell-tide-stable-1.0.0.fwsc) has date '' (the release's publishing date is used); an older dated one has the date as its version.
    A beta must carry -bN and a stable must not. key orders builds: the dated ones first, then by version, a beta before its release."""
    m = PKG.fullmatch(name or "")
    if not m:
        return None
    if m[2]:
        return {"channel": m[1], "version": m[2] + (f".{m[3]}" if m[3] else ""), "date": m[2], "key": (0, m[2], int(m[3] or 0))}
    if (m[1] == "beta") != (m[7] is not None):
        return None
    rank = (0, int(m[7])) if m[7] is not None else (1, 0)
    return {"channel": m[1], "version": ".".join(m.group(4, 5, 6)) + (f"-b{m[7]}" if m[7] is not None else ""), "date": "",
            "key": (1, (int(m[4]), int(m[5]), int(m[6])), rank)}


def _build(rel, channel):
    """a build's place in time: its package name's date and .N or version, then the publishing time (two releases created
    seconds apart must not decide which build is newer)"""
    return (parse_package(package_asset(rel, channel)["name"])["key"], _when(rel))


def package_asset(rel, channel):
    """the release's package asset for a channel, or None"""
    found = [a for a in rel.get("assets", []) if (p := parse_package(a.get("name", ""))) and p["channel"] == channel]
    return max(found, key=lambda a: parse_package(a["name"])["key"], default=None)


def pick(releases):
    """{"stable": release | None, "beta": release | None} by the rules above (releases: the REST API list)"""
    live = sorted((r for r in releases if not r.get("draft")), key=_when, reverse=True)
    stable = max((r for r in live if not r.get("prerelease") and package_asset(r, "stable")), key=lambda r: _build(r, "stable"), default=None)
    betas = sorted((r for r in live if r.get("prerelease") and package_asset(r, "beta")), key=lambda r: _build(r, "beta"), reverse=True)
    beta = next((r for r in betas if stable is None or _build(r, "beta") > _build(stable, "stable")), None)
    return {"stable": stable, "beta": beta}


def commit_in_body(body):
    """the build commit the release notes name ("Build `187e9d0`"), or ''"""
    m = re.search(r"\bBuild\s+`([0-9a-f]{7,40})`", body or "")
    return m[1] if m else ""


def sha_of(text):
    m = SHA.search(text or "")
    return m[1].lower() if m else ""


def info(rel, channel, commit="", sha_file_text="", out_pkg=""):
    """the release data for one channel (what make_site.py reads)"""
    asset = package_asset(rel, channel)
    digest = (asset.get("digest") or "")
    from_digest = digest.split(":", 1)[1].lower() if digest.startswith("sha256:") else ""
    from_file = sha_of(sha_file_text)
    if from_digest and from_file and from_digest != from_file:
        raise SystemExit(f"{rel['tag_name']}: the .sha256 asset ({from_file}) and GitHub's digest ({from_digest}) of {asset['name']} disagree")
    return {
        "channel": channel,
        "tag": rel["tag_name"],
        "name": rel.get("name") or rel["tag_name"],
        "version": parse_package(asset["name"])["version"],
        "date": parse_package(asset["name"])["date"] or (rel.get("published_at") or "")[:10],
        "published_at": rel.get("published_at") or "",
        "notes_url": rel.get("html_url") or "",
        "body": rel.get("body") or "",
        "commit": commit or commit_in_body(rel.get("body")),
        "asset": asset["name"],
        "size": asset.get("size") or 0,
        "sha256": from_file or from_digest,
        "package": out_pkg,
    }


def read_releases_json(text):
    """`gh api --paginate` prints one JSON array per page, back to back; a plain file holds one array"""
    dec, i, out = json.JSONDecoder(), 0, []
    while i < len(text):
        while i < len(text) and text[i].isspace():
            i += 1
        if i >= len(text):
            break
        part, i = dec.raw_decode(text, i)
        out.extend(part if isinstance(part, list) else [part])
    return out


def gh(*args, check=True):
    r = subprocess.run(["gh", *args], capture_output=True, text=True)
    if check and r.returncode:
        raise SystemExit(f"gh {' '.join(args)}: {r.stderr.strip()}")
    return r


def main():
    ap = argparse.ArgumentParser(description="Pick the stable and beta releases for the site")
    ap.add_argument("--repo", default="")
    ap.add_argument("--out", required=True)
    ap.add_argument("--releases-json")
    ap.add_argument("--no-download", action="store_true")
    a = ap.parse_args()
    if a.releases_json:
        releases = read_releases_json(Path(a.releases_json).read_text(encoding="utf-8"))
    else:
        if not a.repo:
            raise SystemExit("--repo OWNER/NAME is needed without --releases-json")
        r = gh("api", "--paginate", f"repos/{a.repo}/releases", check=False)
        releases = read_releases_json(r.stdout) if r.returncode == 0 else []
        if r.returncode:
            print(f"could not list the releases ({r.stderr.strip()}): no package this time")
    chosen = pick(releases)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    for channel in ("stable", "beta"):
        rel = chosen[channel]
        (out / f"{channel}.json").unlink(missing_ok=True)
        if not rel:
            print(f"{channel}: none")
            continue
        asset = package_asset(rel, channel)
        print(f"{channel}: {rel['tag_name']} ({asset['name']}, published {_when(rel)})")
        if a.no_download:
            continue
        folder = out / channel
        folder.mkdir(exist_ok=True)
        gh("release", "download", rel["tag_name"], "--repo", a.repo, "--pattern", asset["name"], "--dir", str(folder), "--clobber")
        sha_text = ""
        if any(x.get("name") == asset["name"] + ".sha256" for x in rel.get("assets", [])):
            gh("release", "download", rel["tag_name"], "--repo", a.repo, "--pattern", asset["name"] + ".sha256", "--dir", str(folder), "--clobber")
            sha_text = (folder / (asset["name"] + ".sha256")).read_text(encoding="utf-8", errors="replace")
        c = gh("api", f"repos/{a.repo}/commits/{rel['tag_name']}", "--jq", ".sha", check=False)
        commit = c.stdout.strip() if c.returncode == 0 and re.fullmatch(r"[0-9a-f]{40}", c.stdout.strip()) else ""
        data = info(rel, channel, commit, sha_text, str(folder / asset["name"]))
        (out / f"{channel}.json").write_text(json.dumps(data, indent=1) + "\n", encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
