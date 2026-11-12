#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Worktrees and fork branches: what can go (jam/cleanup.sh [--apply]).

  worktrees : every `git worktree list` entry: branch or detached, clean / dirty, merged or not into the release lines (default fork/jam11 and fork/b9:
              an ancestor of one of them, or every commit has a patch-equivalent there: `git cherry`), and the action.
  branches  : every branch of the fork remote: tip, merged (same test), open pull request head (GitHub API with the token of `git credential fill`; never printed),
              protected (b8 b9 b9-release jam11 main stable), and the verdict: DELETE (merged, no open PR, not protected) or keep, with the reason; an unmerged branch
              lists the subjects of its commits that are in no release line, because only a person can call those superseded.

  dry run (default): prints the tables and, last, the `git push fork --delete ...` line for the owner. Nothing is changed.
  --apply         : for every worktree that is clean, merged, not the main or the current one, not on a protected branch and not locked: a backup ref
                    refs/backup/<date>/wt-<folder> at its HEAD, then `git worktree remove` (never --force: a refusal is reported and the worktree stays);
                    for every DELETE branch: refs/backup/<date>/<branch> at its tip. The push line is printed, never run.
  jam/cleanup.keep: one worktree folder or branch name a line (# comments): never removed, never deleted, shown as "kept (keep list)"; --apply cannot touch it.
  --into "A B"    : the release lines (default "fork/jam11 fork/b9")     --no-fetch: do not `git fetch fork` first (no pruning, ever)
  --no-pr         : skip the open-PR lookup (the push line is then withheld)
  --from-refs NS  : judge the tips stored under NS (e.g. refs/backup/2026-11-12) as if they were the remote's branches: replays a cleanup afterwards
"""
import datetime, json, os, re, subprocess, sys, urllib.request

PROTECTED = {"b8", "b9", "b9-release", "jam11", "main", "stable"}
KEEP_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cleanup.keep")


def keep_list():
    """jam/cleanup.keep: one worktree folder name or branch name a line, # starts a comment; whatever it names is never removed or deleted, in the dry run it says "kept (keep list)"."""
    try:
        return {l.split("#")[0].strip() for l in open(KEEP_FILE) if l.split("#")[0].strip()}
    except OSError:
        return set()


def git(*a, cwd=None):
    return subprocess.run(["git", *a], cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)


def out(*a, cwd=None):
    return git(*a, cwd=cwd).stdout.strip()


def merged_state(tip, lines):
    """-> ('merged', line) | ('equivalent', line) | ('unmerged', [subjects of the commits that are in no line])"""
    for l in lines:
        if git("merge-base", "--is-ancestor", tip, l).returncode == 0:
            return "merged", l
    best = None
    for l in lines:
        ch = [x for x in out("cherry", l, tip).split("\n") if x.startswith("+")]
        if not ch:
            return "equivalent", l
        best = ch if best is None or len(ch) < len(best) else best
    return "unmerged", [out("log", "-1", "--format=%s", x.split()[1]) for x in (best or [])]


def open_prs(slug):
    p = subprocess.run(["git", "credential", "fill"], input="protocol=https\nhost=github.com\n\n", stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    tok = next((l[9:] for l in p.stdout.split("\n") if l.startswith("password=")), "")
    if not tok:
        print("cleanup: no GitHub token from git credential: open pull requests unknown")
        return None
    try:
        res, page = {}, 1
        while True:
            rq = urllib.request.Request("https://api.github.com/repos/%s/pulls?state=open&per_page=100&page=%d" % (slug, page),
                                        headers={"Authorization": "Bearer " + tok, "Accept": "application/vnd.github+json", "User-Agent": "duell-cleanup"})
            d = json.load(urllib.request.urlopen(rq, timeout=20))
            for x in d:
                res.setdefault(x["head"]["ref"], []).append(x["number"])
            if len(d) < 100:
                return res
            page += 1
    except Exception as e:
        print("cleanup: open pull requests unknown (%s)" % e)
        return None


def worktrees():
    wts, cur = [], {}
    for ln in out("worktree", "list", "--porcelain").split("\n") + [""]:
        if not ln:
            if cur:
                wts.append(cur)
            cur = {}
        elif ln.startswith("worktree "):
            cur["path"] = ln[9:]
        elif ln.startswith("HEAD "):
            cur["head"] = ln[5:]
        elif ln.startswith("branch "):
            cur["branch"] = ln[7:].replace("refs/heads/", "")
        elif ln.startswith("locked"):
            cur["locked"] = True
    return wts


def mtext(ms, info):
    return "unmerged (%d)" % len(info) if ms == "unmerged" else "%s (%s)" % (ms, info.replace("fork/", ""))


def main():
    args = sys.argv[1:]
    if "-h" in args or "--help" in args:
        print(__doc__)
        return 0
    apply = "--apply" in args
    lines = args[args.index("--into") + 1].split() if "--into" in args else ["fork/jam11", "fork/b9"]
    from_refs = args[args.index("--from-refs") + 1].rstrip("/") if "--from-refs" in args else None
    here = os.path.normcase(os.path.realpath(out("rev-parse", "--show-toplevel")))
    keep = keep_list()
    if "--no-fetch" not in args and not from_refs:
        if git("fetch", "-q", "fork").returncode:
            print("cleanup: `git fetch fork` failed: judging from the refs already here")
    for l in lines:
        if git("rev-parse", "-q", "--verify", l).returncode:
            sys.exit("cleanup: release line %s not found" % l)
    date = datetime.date.today().isoformat()
    slug = re.sub(r"^.*github\.com[:/]", "", out("remote", "get-url", "fork"))
    slug = slug[:-4] if slug.endswith(".git") else slug

    # -- worktrees
    wts = worktrees()
    print("== worktrees (%d); merged = in %s" % (len(wts), " or ".join(lines)))
    print("%-24s %-22s %-8s %-18s %s" % ("folder", "branch", "state", "merged", "action"))
    todo_wt = []
    for i, w in enumerate(wts):
        name = os.path.basename(w["path"].rstrip("/"))
        br = w.get("branch") or "(detached %s)" % w.get("head", "")[:7]
        exists = os.path.isdir(w["path"])
        dirty = "missing" if not exists else ("dirty" if out("status", "--porcelain", cwd=w["path"]) else "clean")
        ms, info = merged_state(w["head"], lines)
        if i == 0:
            why = "keep: main worktree"
        elif os.path.normcase(os.path.realpath(w["path"])) == here:
            why = "keep: this worktree"
        elif name in keep or w.get("branch") in keep:
            why = "kept (keep list)"
        elif w.get("branch") in PROTECTED:
            why = "keep: protected branch"
        elif w.get("locked"):
            why = "keep: locked"
        elif dirty == "dirty":
            why = "keep: uncommitted changes"
        elif dirty == "missing":
            why = "prune: the folder is gone (git worktree prune)"
        elif ms == "unmerged":
            why = "keep: %d commit(s) in no release line" % len(info)
        else:
            why = "REMOVE"
            todo_wt.append((name, w))
        print("%-24s %-22s %-8s %-18s %s" % (name, br[:22], dirty, mtext(ms, info), why))

    # -- branches
    if from_refs:
        remote = {}
        for l in out("for-each-ref", "--format=%(objectname) %(refname)", from_refs).split("\n"):
            if l:
                s, r = l.split(" ", 1)
                remote[r[len(from_refs) + 1:]] = s
        src = from_refs
    else:
        remote = {}
        for l in git("ls-remote", "--heads", "fork").stdout.split("\n"):
            if l.strip():
                s, r = l.split("\t")
                remote[r.replace("refs/heads/", "")] = s
        src = "remote fork"
    prs = None if ("--no-pr" in args or from_refs) else open_prs(slug)
    print("\n== branches of %s (%d); merged = in %s" % (src, len(remote), " or ".join(lines)))
    print("%-26s %-8s %-20s %-8s %s" % ("branch", "tip", "merged", "open PR", "verdict"))
    delete, flagged = [], []
    for b in sorted(remote):
        tip = remote[b]
        if git("cat-file", "-e", tip + "^{commit}").returncode:
            print("%-26s %-8s %-20s %-8s %s" % (b, tip[:7], "?", "?", "keep: tip not fetched (git fetch fork)"))
            continue
        ms, info = merged_state(tip, lines)
        pr = ("#" + ",#".join(map(str, prs[b]))) if prs and b in prs else ("?" if prs is None else "-")
        if b in keep:
            v = "kept (keep list)"
        elif b in PROTECTED:
            v = "keep: protected"
        elif prs and b in prs:
            v = "keep: open PR head"
        elif ms == "unmerged":
            v = "keep: unmerged, your call: " + " | ".join(s[:60] for s in info[:3])
            flagged.append(b)
        elif prs is None and not from_refs:
            v = "(delete if no open PR; PRs unknown)"
        else:
            v = "DELETE"
            delete.append(b)
        print("%-26s %-8s %-20s %-8s %s" % (b, tip[:7], mtext(ms, info), pr, v))

    print("\n== summary: %d worktree(s) removable, %d branch(es) to delete, %d unmerged branch(es) for you to judge: %s" % (len(todo_wt), len(delete), len(flagged), " ".join(flagged)))
    if apply:
        print("\n== applying (backups in refs/backup/%s/)" % date)
        for name, w in todo_wt:
            ref = "refs/backup/%s/wt-%s" % (date, name)
            if git("rev-parse", "-q", "--verify", ref).returncode:
                git("update-ref", ref, w["head"])
            r = git("worktree", "remove", w["path"])
            print("%s %s%s" % ("removed" if r.returncode == 0 else "KEPT   ", name, "" if r.returncode == 0 else " (" + r.stderr.strip().split("\n")[-1] + ")"))
        for b in delete:
            ref = "refs/backup/%s/%s" % (date, b)
            if git("rev-parse", "-q", "--verify", ref).returncode:
                git("update-ref", ref, remote[b])
        if delete:
            print("backup refs made for %d branch(es)" % len(delete))
    if delete and prs is not None and not from_refs:
        print("\nfor the owner (not run):\n  git push fork --delete " + " ".join(delete))
    elif delete:
        print("\nwould be deleted: " + " ".join(delete) + ("" if from_refs else "  (open PRs unknown: no push line)"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
