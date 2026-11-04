#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""python3 web/test_site_build.py : the release choice (releases.py) and the site build (make_site.py), with small made-up packages.
No network, no gh. The fixture fixtures/releases_api.json is the shape of `gh api repos/OWNER/NAME/releases` with the two real release notes."""
import copy
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import releases  # noqa: E402
import make_site  # noqa: E402

FIXTURE = json.loads((HERE / "fixtures" / "releases_api.json").read_text(encoding="utf-8"))


def fake_package(product="FM-1_901", loader=True, extra=b""):
    """a made-up .fwsc: the identity marker bytes of the first 20 blocks, and the Felucca loader marker"""
    raw = bytearray(4096)
    for i in range(20):
        raw[i * 48 + 47] = 0x7D
    for i, ch in enumerate(product):
        raw[i * 48 + 47] = (ord(ch) + i + 1) & 0xFF
    if loader:
        raw[2000:2016] = b"FELUCCA-LOADER-1"
    return bytes(raw) + extra


class Pick(unittest.TestCase):
    def tags(self, rels):
        c = releases.pick(rels)
        return (c["stable"] or {}).get("tag_name"), (c["beta"] or {}).get("tag_name")

    def test_real_shape(self):
        self.assertEqual(self.tags(FIXTURE), ("stable-2026-11-03", "beta-2026-11-04"))   # the older beta and the draft are not offered

    def test_no_releases(self):
        self.assertEqual(self.tags([]), (None, None))

    def test_beta_older_than_stable_is_superseded(self):
        rels = [r for r in FIXTURE if r["tag_name"] != "beta-2026-11-04"]
        self.assertEqual(self.tags(rels), ("stable-2026-11-03", None))

    def test_beta_alone(self):
        rels = [r for r in FIXTURE if r["tag_name"] == "beta-2026-11-04"]
        self.assertEqual(self.tags(rels), (None, "beta-2026-11-04"))

    def test_newest_stable_wins_and_a_beta_must_beat_it(self):
        rels = copy.deepcopy(FIXTURE)
        newer = copy.deepcopy(rels[2])
        newer.update(tag_name="stable-2026-11-10", published_at="2026-11-10T08:00:00Z", created_at="2026-11-10T08:00:00Z")
        newer["assets"] = [{"name": "duell-tide-stable-2026-11-10.fwsc", "size": 5}]
        self.assertEqual(self.tags(rels + [newer]), ("stable-2026-11-10", None))

    def test_a_newer_beta_published_first_is_offered(self):   # the build date decides, not which release was created first
        rels = copy.deepcopy(FIXTURE)
        for r in rels:
            if r["tag_name"] == "beta-2026-11-04":
                r.update(published_at="2026-11-01T00:00:00Z", created_at="2026-11-01T00:00:00Z")
        self.assertEqual(self.tags(rels), ("stable-2026-11-03", "beta-2026-11-04"))

    def test_second_beta_of_a_day_wins(self):
        rels = copy.deepcopy(FIXTURE)
        two = copy.deepcopy(next(r for r in rels if r["tag_name"] == "beta-2026-11-04"))
        two.update(tag_name="beta-2026-11-04.2", published_at="2026-11-04T05:00:00Z", created_at="2026-11-04T05:00:00Z")
        two["assets"] = [{"name": "duell-tide-beta-2026-11-04.2.fwsc", "size": 5}]
        self.assertEqual(self.tags(rels + [two]), ("stable-2026-11-03", "beta-2026-11-04.2"))

    def test_release_without_a_package_is_skipped(self):
        rels = copy.deepcopy(FIXTURE)
        emu_only = {"tag_name": "emu-1", "draft": False, "prerelease": False, "published_at": "2026-11-09T00:00:00Z", "assets": [{"name": "duell-emulator-1.zip"}]}
        self.assertEqual(self.tags(rels + [emu_only]), ("stable-2026-11-03", "beta-2026-11-04"))

    def test_a_beta_asset_on_a_normal_release_is_not_a_stable(self):
        rel = {"tag_name": "x", "draft": False, "prerelease": False, "published_at": "2026-11-09T00:00:00Z", "assets": [{"name": "duell-tide-beta-2026-11-09.fwsc"}]}
        self.assertEqual(self.tags([rel]), (None, None))

    def test_gh_paginate_output(self):
        two_pages = json.dumps(FIXTURE[:2]) + json.dumps(FIXTURE[2:])
        self.assertEqual(len(releases.read_releases_json(two_pages)), len(FIXTURE))

    def test_info(self):
        c = releases.pick(FIXTURE)
        s = releases.info(c["stable"], "stable", "", "7795e77798d053177a4d0b099f0754701e16e308c92fbe22fd54913abac41b30 *duell-tide-stable-2026-11-03.fwsc\n")
        self.assertEqual((s["tag"], s["date"], s["commit"], s["size"]), ("stable-2026-11-03", "2026-11-03", "187e9d0", 614666))   # commit from "Build `187e9d0`"
        self.assertEqual(s["sha256"], "7795e77798d053177a4d0b099f0754701e16e308c92fbe22fd54913abac41b30")
        with self.assertRaises(SystemExit):                      # the .sha256 asset and GitHub's digest must agree
            releases.info(c["stable"], "stable", "", "f" * 64 + " *x\n")

    def test_notes_parsing(self):
        b = releases.info(releases.pick(FIXTURE)["beta"], "beta")
        issues = make_site.known_issues_of(b["body"])
        self.assertEqual(len(issues), 3)
        self.assertTrue(issues[0].startswith("The SAVE > MULTIS page layout") and "**" not in "".join(issues))
        self.assertEqual(make_site.known_issues_of(releases.info(releases.pick(FIXTURE)["stable"], "stable")["body"]), [])
        self.assertTrue(make_site.summary_of(b["body"]).startswith("Beta: under test. Build 77a6df2"))
        self.assertNotIn("`", make_site.summary_of(b["body"]))


class Build(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.out = self.dir / "site"

    def tearDown(self):
        self.tmp.cleanup()

    def pkg(self, name, **kw):
        p = self.dir / name
        p.write_bytes(fake_package(**kw))
        return p

    def info(self, channel, pkg, **over):
        rel = {"tag": f"{channel}-x", "date": "x", "published_at": "2026-11-04T08:00:00Z", "notes_url": "https://github.com/pssdrnk-cmyk/DUELL-TIDE/releases/tag/x",
               "body": "**Known open issues**\n- one\n", "commit": "77a6df2", "sha256": hashlib.sha256(pkg.read_bytes()).hexdigest(), "size": pkg.stat().st_size}
        rel.update(over)
        f = self.dir / f"{channel}.json"
        f.write_text(json.dumps(rel), encoding="utf-8")
        return str(f)

    def run_site(self, *args):
        old = sys.argv
        sys.argv = ["make_site.py", *map(str, args), str(self.out)]
        try:
            return make_site.main()
        finally:
            sys.argv = old

    def manifest(self):
        return json.loads((self.out / "install" / "channels.json").read_text(encoding="utf-8"))["channels"]

    def test_no_release_at_all(self):
        self.run_site()
        self.assertEqual(self.manifest(), {"stable": None, "beta": None})
        self.assertTrue((self.out / "install" / "index.html").is_file() and (self.out / "index.html").is_file() and (self.out / "assets" / "site.css").is_file())

    def test_stable_and_beta(self):
        s, b = self.pkg("duell-tide-stable-2026-11-03.fwsc"), self.pkg("duell-tide-beta-2026-11-04.fwsc", extra=b"beta")
        self.run_site("--stable-pkg", s, "--stable-info", self.info("stable", s, published_at="2026-11-04T07:00:00Z"),
                      "--beta-pkg", b, "--beta-info", self.info("beta", b))
        m = self.manifest()
        self.assertEqual(m["stable"]["file"], "stable/duell-tide-stable-2026-11-03.fwsc")
        self.assertEqual(m["beta"]["sha256"], hashlib.sha256(b.read_bytes()).hexdigest())
        self.assertEqual((m["beta"]["commit"], m["beta"]["product"], m["beta"]["known_issues"]), ("77a6df2", "FM-1_901", ["one"]))
        for e in m.values():
            self.assertEqual((self.out / "install" / e["file"]).stat().st_size, e["size"])
            self.assertTrue((self.out / "install" / (e["file"] + ".sha256")).read_text().startswith(e["sha256"]))

    def test_newer_beta_published_before_stable_is_offered(self):   # the build date decides, not which release was created first
        s, b = self.pkg("duell-tide-stable-2026-11-03.fwsc"), self.pkg("duell-tide-beta-2026-11-04.fwsc", extra=b"beta")
        self.run_site("--stable-pkg", s, "--stable-info", self.info("stable", s, published_at="2026-11-04T07:00:00Z"),
                      "--beta-pkg", b, "--beta-info", self.info("beta", b, published_at="2026-11-04T06:59:00Z"))
        self.assertIsNotNone(self.manifest()["beta"])

    def test_beta_not_newer_is_dropped(self):
        s, b = self.pkg("duell-tide-stable-2026-11-03.fwsc"), self.pkg("duell-tide-beta-2026-11-02.fwsc", extra=b"b")
        self.run_site("--stable-pkg", s, "--beta-pkg", b)               # no release data: the dates in the names decide
        self.assertIsNone(self.manifest()["beta"])
        self.assertIsNotNone(self.manifest()["stable"])

    def test_sha_mismatch_stops_the_build(self):
        s = self.pkg("duell-tide-stable-2026-11-03.fwsc")
        with self.assertRaises(SystemExit):
            self.run_site("--stable-pkg", s, "--stable-info", self.info("stable", s, sha256="0" * 64))

    def test_wrong_channel_name_stops_the_build(self):
        s = self.pkg("duell-tide-stable-2026-11-03.fwsc")
        with self.assertRaises(SystemExit):
            self.run_site("--beta-pkg", s)

    def test_package_with_vendor_files_or_wrong_identity_stops_the_build(self):
        with self.assertRaises(SystemExit):
            self.run_site("--stable-pkg", self.pkg("duell-tide-stable-2026-11-03.fwsc", loader=False))
        with self.assertRaises(SystemExit):
            self.run_site("--stable-pkg", self.pkg("duell-tide-stable-2026-11-04.fwsc", product="FM-1_015"))

    def test_node_reads_the_manifest_the_build_wrote(self):
        s = self.pkg("duell-tide-stable-2026-11-03.fwsc")
        self.run_site("--stable-pkg", s, "--stable-info", self.info("stable", s))
        js = ('import { parseManifest } from "%s"; import { readFileSync } from "node:fs";'
              'const m = parseManifest(JSON.parse(readFileSync(process.argv[1], "utf8"))); console.log(!!m.stable, !!m.beta);'
              % (HERE / "installer_state.js").as_uri())
        try:
            r = subprocess.run(["node", "--input-type=module", "-e", js, str(self.out / "install" / "channels.json")], capture_output=True, text=True)
        except FileNotFoundError:
            self.skipTest("no node")
        self.assertEqual(r.stdout.strip(), "true false", r.stderr)


if __name__ == "__main__":
    unittest.main()
