#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Re-pin test hashes, but only when the change that moved them is proven (jam/repin.sh CAUSE_COMMIT [group ...]).

  1. runs the groups on HEAD in a scratch worktree; the ones that fail are the candidates (no groups named: the failures of the last fast run)
  2. finds the new value of every hash pin (`#define NAME 0x...ull` in a test the group builds) by running the group once more with the pin's
     comparisons made to print the value they saw (a scratch copy of the test; the checkout's files are not touched yet)
  3. reverts CAUSE_COMMIT in a second scratch worktree and runs the groups there with the OLD pins: if they pass, the old hashes come back without
     the cause, so the cause is what moved them ("proven")
  4. only for proven groups, writes the new values into the pins (comment "(b9: <cause subject>, proven by revert; was OLD)"), regenerates the
     golden files of regress / mrender / union, then runs the groups on the checkout once more
  5. prints a table: group, pin, old -> new, proven yes / no. Unproven pins are never written.
"""
import os, re, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import fastcheck as F  # noqa: E402

PIN = re.compile(r"^(#define\s+)([A-Za-z_]\w*)(\s+)(0x[0-9A-Fa-f]{8,16})([uUlL]*)(.*)$")
ARR = re.compile(r"^\s*(?:static\s+)?const\s+(?:uint64_t|unsigned long long)\s+([A-Za-z_]\w*)\s*\[[^\]]*\]\s*=\s*\{")
HEX = re.compile(r"0x[0-9A-Fa-f]{8,16}")
GOLDEN = {"regress": ("GOLDEN_UPDATE", "felucca-base/tests/golden.txt"), "mrender": ("MULTI_RENDER_UPDATE", "felucca-base/tests/multi_render_golden.txt"),
          "union": ("GOLDEN_UPDATE", "felucca-base/tests/union_golden.txt")}
PROBE = ("static int repin_eq_(unsigned long long a, unsigned long long b, const char *n, const char *f) "
         "{ if (a != b) { printf(" + chr(34) + "REPIN %s %s 0x%016llx 0x%016llx" + chr(92) + "n" + chr(34) + ", f, n, b, a); fflush(stdout); } return 1; }" + chr(10))


def run(cmd, cwd, check=True, env=None):
    p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env)
    if check and p.returncode != 0:
        sys.exit("repin: %s failed:\n%s" % (" ".join(cmd), p.stdout[-1500:]))
    return p


def worktree(path, rev):
    run(["git", "worktree", "add", "--detach", path, rev], REPO)
    for d in ("jam",):                              # (nothing: the scratch tree is a full checkout)
        pass
    run([sys.executable, "tools/build.py", "--gen-only"], os.path.join(path, "felucca-base"), check=False)


def fast(tree, groups, log, extra_env=None):
    """fastcheck.py of that tree -> {group: status}."""
    env = dict(os.environ, **(extra_env or {}))
    env["HT_REALCC"] = env.get("HT_REALCC") or subprocess.run(["sh", "-c", "command -v cc"], stdout=subprocess.PIPE, text=True).stdout.strip()
    p = subprocess.run([sys.executable, os.path.join(tree, "jam", "fastcheck.py"), "--log", log, "-j", str(min(len(groups), F.default_jobs()))] + groups,
                       cwd=tree, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    res = {}
    for ln in p.stdout.splitlines():
        m = re.match(r"^(PASS|FAIL|SKIP|TIMEOUT) +([^ ]+)", ln)
        if m:
            res[m.group(2)] = m.group(1)
    return res


def group_test_files(g):
    text = open(os.path.join(REPO, "host_tests.sh"), newline="").read()
    body = F.parse_hostsh(text)[1].get(g, "")
    return sorted(f for f in F.expand_tokens(body) if f.startswith("felucca-base/tests/") and f.endswith(".c"))


def pins_of(path):
    """{NAME: (first line, last line, [old values])} for `#define NAME 0x..ull` and `static const uint64_t NAME[N] = { 0x.., ... };`"""
    out, L = {}, open(path, newline="", errors="replace").read().split("\n")
    i = 0
    while i < len(L):
        m = PIN.match(L[i]); k = ARR.match(L[i])
        if m:
            out[m.group(2)] = (i, i, [m.group(4)])
        elif k:
            j = i
            while j < len(L) and "};" not in L[j]:
                j += 1
            out[k.group(1)] = (i, j, HEX.findall("\n".join(L[i:j + 1])))
            i = j
        i += 1
    return out


def instrument(path, rel, pins):
    """rewrite the test so that every `x == PIN` / `PIN == x` / `x == PIN[i]` prints the value it saw; -> the names it could instrument."""
    L = open(path, newline="").read().split("\n")
    skip = set()
    for n, (a, b, _v) in pins.items():
        skip.update(range(a, b + 1))
    done = set()
    for n in pins:
        ref = r"%s(?:\[[^\]]*\])?" % re.escape(n)
        a = re.compile(r"(?<![\w.>])([A-Za-z_][\w.\[\]>-]*)\s*==\s*(%s)(?![\w])" % ref)
        b = re.compile(r"(?<![\w])(%s)\s*==\s*([A-Za-z_][\w.\[\]>-]*)(?![\w(])" % ref)
        for i, ln in enumerate(L):
            if i in skip:
                continue
            new = a.sub(lambda m: 'repin_eq_(%s, %s, "%s", "%s")' % (m.group(1), m.group(2), n, rel), ln)
            new = b.sub(lambda m: 'repin_eq_(%s, %s, "%s", "%s")' % (m.group(2), m.group(1), n, rel), new)
            if new != ln:
                done.add(n); L[i] = new
    use = next((i for i, ln in enumerate(L) if "repin_eq_(" in ln), None)
    at = 0
    if use is not None:                              # just above the function that holds the first use (an #include may sit in an #if)
        i = use
        while i > 0 and not (L[i][:1] not in ("", " ", "\t", "}", "#", "/", "*", "{", ")") and "(" in L[i] and not L[i].rstrip().endswith(";")):
            i -= 1
        at = i
    L.insert(at, PROBE.rstrip("\n"))
    open(path, "w", newline="").write("\n".join(L))
    return done


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cause = sys.argv[1]
    groups = sys.argv[2:]
    full = run(["git", "rev-parse", cause + "^{commit}"], REPO).stdout.strip()
    subject = run(["git", "log", "-1", "--format=%s", full], REPO).stdout.strip()
    head = run(["git", "rev-parse", "HEAD"], REPO).stdout.strip()
    if not groups:
        try:
            groups = [l.split("\t")[0] for l in open(os.path.join(F.FB, "build", "check", "fast", "results.tsv")) if l.split("\t")[1] in ("FAIL", "TIMEOUT")]
        except OSError:
            groups = []
        if not groups:
            sys.exit("repin: name the failing groups (no failures recorded by the last fast run)")
    if run(["git", "status", "--porcelain", "--untracked-files=no"], REPO).stdout.strip():
        print("repin: note: uncommitted changes are not in the scratch trees (they run HEAD %s)" % head[:7])
    tmp = tempfile.mkdtemp(prefix="rp")
    th, tr = os.path.join(tmp, "head"), os.path.join(tmp, "rev")
    try:
        print("== 1/4 the groups on HEAD %s: %s" % (head[:7], " ".join(groups)))
        worktree(th, head)
        res = fast(th, groups, os.path.join(tmp, "log_head"))
        failing = [g for g in groups if res.get(g) in ("FAIL", "TIMEOUT")]
        for g in groups:
            if g not in failing:
                print("   %s: %s at HEAD, nothing to re-pin" % (g, res.get(g, "not run")))
        if not failing:
            return 0
        print("== 2/4 the new values (the pins' comparisons print what they saw): %s" % " ".join(failing))
        rows = []                                   # [group, file, name, old, new]
        for g in failing:
            if g in GOLDEN:
                continue
            files = [f for f in group_test_files(g) if pins_of(os.path.join(th, f))]
            ins = {}
            for f in files:
                ins[f] = instrument(os.path.join(th, f), f, pins_of(os.path.join(th, f)))
            fast(th, [g], os.path.join(tmp, "log_pin_" + g))
            full_log = os.path.join(tmp, "log_pin_" + g, g + ".full.log")
            text = open(full_log, errors="replace").read() if os.path.exists(full_log) else ""
            got = set()
            for m in re.finditer(r"^REPIN (\S+) (\w+) (0x[0-9a-f]{16}) (0x[0-9a-f]{16})$", text, re.M):
                if (m.group(1), m.group(2), m.group(3)) not in got:
                    got.add((m.group(1), m.group(2), m.group(3)))
                    rows.append([g, m.group(1), m.group(2), m.group(3), m.group(4)])
            for f in files:
                run(["git", "checkout", "--", f], th)
                for n in pins_of(os.path.join(th, f)):
                    if n not in ins[f]:
                        rows.append([g, f, n, "?", None])          # no comparison of it that could be instrumented
            if not any(r[0] == g for r in rows):
                rows.append([g, "(no hash pin of this group's tests moved)", "-", "-", None])
        print("== 3/4 CAUSE %s reverted on HEAD, the groups with the OLD pins" % full[:7])
        worktree(tr, head)
        rv = run(["git", "revert", "--no-commit", "--no-edit"] + (["-m", "1"] if len(run(["git", "rev-list", "--parents", "-n1", full], REPO).stdout.split()) > 2 else []) + [full], tr, check=False)
        if rv.returncode != 0:
            print("   the revert does not apply cleanly: nothing can be proven\n" + rv.stdout[-400:])
            proven = {g: False for g in failing}
        else:
            run([sys.executable, "tools/build.py", "--gen-only"], os.path.join(tr, "felucca-base"), check=False)
            rr = fast(tr, failing, os.path.join(tmp, "log_rev"))
            proven = {g: rr.get(g) in ("PASS", "SKIP") for g in failing}
        for g in failing:
            print("   %s: %s without the cause -> %s" % (g, "passes" if proven[g] else "still fails", "proven" if proven[g] else "NOT proven"))
        print("== 4/4 writing the proven pins")
        tag = "(b9: %s, proven by revert" % subject.replace("*/", "* /")
        written = 0
        for g in failing:
            if g in GOLDEN and proven[g]:
                env, gf = GOLDEN[g]
                fast(th, [g], os.path.join(tmp, "log_gold_" + g), {env: "1"})
                new = open(os.path.join(th, gf), newline="").read()
                if new != open(os.path.join(REPO, gf), newline="").read():
                    open(os.path.join(REPO, gf), "w", newline="").write(new)
                    rows.append([g, gf, "(golden file)", "old", "regenerated"]); written += 1
            elif g in GOLDEN:
                rows.append([g, GOLDEN[g][1], "(golden file)", "-", "not written"])
        edits = {}
        for r in rows:
            g, f, n, o, nw = r
            if nw and proven.get(g):
                edits.setdefault(f, {}).setdefault(n, {}).setdefault(int(o, 16), set()).add(nw)
        for f, d in edits.items():
            path = os.path.join(REPO, f)
            L = open(path, newline="").read().split("\n")
            pins = pins_of(path)
            for n, olds in d.items():
                if n not in pins:
                    continue
                a0, a1, _v = pins[n]
                for ov, news in olds.items():
                    nw = next(iter(news))
                    if len(news) != 1:
                        for r in rows:
                            if r[1] == f and r[2] == n and int(r[3], 16) == ov:
                                r[4] = "CONFLICT (groups disagree)"
                        continue
                    done = False
                    for i in range(a0, a1 + 1):
                        def sub(m):
                            nonlocal done
                            if int(m.group(0), 16) == ov:
                                done = True
                                dg = nw[2:].upper() if m.group(0)[2:] == m.group(0)[2:].upper() else nw[2:]
                                return "0x" + dg
                            return m.group(0)
                        L[i] = HEX.sub(sub, L[i])
                    if done:
                        written += 1
                L[a0] = L[a0] + " /* %s; was %s) */" % (tag, ", ".join("0x%016x" % o for o in olds))
            open(path, "w", newline="").write("\n".join(L))
        ok = [g for g in failing if proven[g]]
        if ok and written:
            after = fast(REPO, ok, os.path.join(tmp, "log_after"))
            print("   the checkout, with the new pins: " + " ".join("%s %s" % (g, after.get(g)) for g in ok))
        print("\n%-12s %-36s %-34s %-18s %-18s %s" % ("group", "file", "pin", "old", "new", "proven"))
        for g, f, n, o, nw in rows:
            print("%-12s %-36s %-34s %-18s %-18s %s" % (g, f.replace("felucca-base/tests/", ""), n, o, nw or "(unknown)", "yes, written" if proven.get(g) and nw and not str(nw).startswith("CONFLICT") else "no, not written"))
        return 0 if all(proven.values()) else 1
    finally:
        for t in (th, tr):
            if os.path.isdir(t):
                subprocess.run(["git", "worktree", "remove", "--force", t], cwd=REPO, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
