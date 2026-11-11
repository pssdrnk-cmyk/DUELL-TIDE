#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The fast host-test runner behind `jam/check.sh -j N | --all | --changed | --shard I/N` (see jam/README.md "Fast tests").

  python3 jam/fastcheck.py [-j N] [--limit S] [--log DIR] [--changed [BASE]] [--shard I/N] [--out FILE] [--summary FILE]
                           [--no-cache] [--map] [--verify-map] [group ...]

Every group runs as `timeout LIMIT sh host_tests.sh GROUP` (as the serial loop of check.sh does) inside its own sandbox: a folder with junctions (links) to
the checkout's folders and a private felucca-base/build, so groups cannot clobber each other's build/host, build/*_mut ... The sandbox's `cc` is
jam/ccwrap/cc, which compiles one translation unit once per (preprocessed text + flags) into build/host-cache. The verdict of a group (PASS / FAIL /
SKIP / TIMEOUT) is decided exactly as check.sh decides it. Exit code 0 only if no group failed.
"""
import argparse, concurrent.futures as cf, contextlib, fnmatch, glob, hashlib, json, os, re, shutil, subprocess, sys, tempfile, threading, time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
FB = os.path.join(REPO, "felucca-base")
NT = os.name == "nt"
CCFLAGS = ["-O2", "-w", "-DHOST_MIX_FOLLOWS_MASTER=1", "-DDUELL_TEST_MODE=1", "-Ibuild/gen_test", "-Ibuild/gen", "-Ifirmware/src", "-Itests",
           '-DFELUCCA_VERSION=x']
SLOW_FIRST = ("regress", "multiv8", "rollbrowser", "sliders", "editorjson", "benchaudio", "mrender", "soundlab")


def posix(p):
    return p.replace("\\", "/")


def msys(p):
    """C:/x -> /c/x (a PATH entry for Git Bash)."""
    p = posix(p)
    return "/" + p[0].lower() + p[2:] if NT and re.match(r"^[A-Za-z]:/", p) else p


# ---------------------------------------------------------------------------------------------------------------- groups

def default_groups():
    t = open(os.path.join(REPO, "host_tests.sh"), newline="").read()
    m = re.search(r'^want="\$\{\*:-([^"]*)\}"$', t, re.M)
    return m.group(1).split()


def free_gb():
    try:
        if NT:
            import ctypes

            class MS(ctypes.Structure):
                _fields_ = [("l", ctypes.c_ulong), ("load", ctypes.c_ulong), ("tp", ctypes.c_ulonglong), ("ap", ctypes.c_ulonglong),
                            ("tv", ctypes.c_ulonglong), ("av", ctypes.c_ulonglong), ("tx", ctypes.c_ulonglong), ("ax", ctypes.c_ulonglong),
                            ("ae", ctypes.c_ulonglong)]
            ms = MS(); ms.l = ctypes.sizeof(MS)
            ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(ms))
            return ms.ap / 2**30
        for ln in open("/proc/meminfo"):
            if ln.startswith("MemAvailable:"):
                return int(ln.split()[1]) / 2**20
    except Exception:
        pass
    return 4.0


def default_jobs():
    return max(1, min(int(free_gb() / 1.5), (os.cpu_count() or 2) - 1))


# ---------------------------------------------------------------------------------------------------------------- sandbox

def link_dir(src, dst):
    if NT:
        subprocess.run(["cmd", "/c", "mklink", "/J", dst.replace("/", "\\"), src.replace("/", "\\")], stdout=subprocess.DEVNULL, check=True)
    else:
        os.symlink(src, dst)


def link_entry(src, dst, real=False):
    """a link to a folder, or (real=True, on POSIX) a tree of hard links: the mutation groups `cp -R firmware tests build/x/` and edit the copy, and cp -R of
    a symbolic link copies the link, so their sed would change the checkout's own files. Hard links are real files for cp; sed -i and `> f.new; mv` replace
    them with new ones, while a test that writes a file of tests/ in place (a golden update) still reaches the checkout."""
    if os.path.isdir(src) and real and not NT:
        try:
            shutil.copytree(src, dst, copy_function=os.link, symlinks=True)
        except OSError:
            shutil.rmtree(dst, ignore_errors=True)
            shutil.copytree(src, dst, symlinks=True)
    elif os.path.isdir(src):
        link_dir(src, dst)
    else:
        shutil.copy2(src, dst)


def make_sandbox(root):
    """root/{host_tests.sh, .git, jam, docs ... -> links; felucca-base/{firmware, tests ... -> links; build/ private}}."""
    os.makedirs(os.path.join(root, "felucca-base", "build"))
    for n in os.listdir(REPO):
        if n in ("felucca-base",):
            continue
        link_entry(os.path.join(REPO, n), os.path.join(root, n))
    for n in os.listdir(FB):
        if n == "build":
            continue
        link_entry(os.path.join(FB, n), os.path.join(root, "felucca-base", n), real=n in ("firmware", "tests"))
    bsrc, bdst = os.path.join(FB, "build"), os.path.join(root, "felucca-base", "build")
    for n in os.listdir(bsrc):
        s = os.path.join(bsrc, n)
        if n in ("gen", "gen_test", "gen_off"):
            shutil.copytree(s, os.path.join(bdst, n))
        elif os.path.isfile(s):                    # device-build packages etc. (build/duell-*.fwsc: ldrscreen)
            shutil.copy2(s, os.path.join(bdst, n))


def remove_sandbox(root):
    for dp, dn, fn in os.walk(root):               # junctions first: removing one must never touch its target
        for d in list(dn):
            p = os.path.join(dp, d)
            if os.path.islink(p) or (NT and os.path.isdir(p) and bool(os.stat(p, follow_symlinks=False).st_file_attributes & 0x400)):
                (os.rmdir(p) if NT else os.unlink(p))
                dn.remove(d)
    shutil.rmtree(root, ignore_errors=True)


# ---------------------------------------------------------------------------------------------------------------- verdict (as check.sh)

def verdict(rc, text):
    lines = text.splitlines()
    real = ""
    for l in lines:
        if re.search(r"FAIL|Assertion failed", l) and not re.search(r"^   FAIL|^   PASS|(^|[^0-9])0 FAIL", l):
            real = l; break
    if rc == 124:
        return "TIMEOUT", "timed out"
    if rc != 0 or real or any(l.startswith("   FAIL") for l in lines):
        return "FAIL", first_failure(lines, real)
    for l in lines:
        if l.startswith("   SKIP"):
            return "SKIP", l[len("   SKIP "):]
    return "PASS", ""


def first_failure(lines, real):
    if real:
        return real.strip()[:200]
    for l in lines:
        if re.search(r"FAIL|Assertion|rror:", l) and not re.search(r"(^|[^0-9])0 FAIL", l) and l.strip() != "FAIL" and "PASS (detected" not in l:
            return l.strip()[:200]
    for l in reversed(lines):
        if l.strip():
            return l.strip()[:200]
    return "(no output)"


# ---------------------------------------------------------------------------------------------------------------- run

class Runner:
    def __init__(self, a):
        self.a = a
        self.lock = threading.Lock()
        self.results = {}                          # group -> (status, secs, reason)
        self.cache = None if a.no_cache else posix(os.path.join(FB, "build", "host-cache"))
        self.sbroot = os.path.join(tempfile.gettempdir(), "htp%d" % os.getpid())
        self.realcc = os.environ.get("HT_REALCC") or ""
        # the groups that start headless Chrome (web/cdp.mjs) run one at a time: two launches at once on a loaded machine timed out in CI (sliders, editorjson)
        self.chrome_lock = threading.Lock()
        self.browser_groups = set()
        try:
            for g, body in parse_hostsh(open(os.path.join(REPO, "host_tests.sh"), newline="").read())[1].items():
                if re.search(r"test_(layout|install_flow|rollbrowser|sliders)\.mjs", body):
                    self.browser_groups.add(g)
        except OSError:
            pass
        try:                                        # (a checkout that lost the executable bit would silently run without the cache)
            os.chmod(os.path.join(HERE, "ccwrap", "cc"), 0o755)
        except OSError:
            pass

    def env(self, g, sandbox):
        e = dict(os.environ)
        e["HT_NOGEN"] = "1"
        e["HT_GROUP"] = g
        e["HT_REALCC"] = self.realcc
        e["PATH"] = (posix(os.path.join(HERE, "ccwrap")).replace("/", "\\") if NT else os.path.join(HERE, "ccwrap")) + os.pathsep + e.get("PATH", "")
        if self.cache:
            e["HT_CC_CACHE"] = self.cache
        e["HT_DEPS_DIR"] = posix(self.a.deps_dir)
        return e

    def run_group(self, g):
        a = self.a
        sandbox = os.path.join(self.sbroot, g)
        t0 = time.time()
        try:
            os.unlink(os.path.join(a.deps_dir, g + ".deps"))     # (this run records what the group's compiles read again)
        except OSError:
            pass
        out = os.path.join(a.log, g + ".log")
        try:
            make_sandbox(sandbox)
            with open(out, "wb") as f, (self.chrome_lock if g in self.browser_groups else contextlib.nullcontext()):
                p = subprocess.run(["sh", "-c", 'exec timeout "$0" sh host_tests.sh "$1"', str(a.limit), g], cwd=sandbox, env=self.env(g, sandbox), stdout=f,
                                   stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
            rc = p.returncode
        except Exception as ex:                    # the sandbox could not be made: reported, never a PASS
            rc = 99
            open(out, "w").write("fastcheck: %s\n" % ex)
        finally:
            try:                                    # the full output of the last test program (host_tests.sh prints a digest of it): repin.py reads it
                shutil.copyfile(os.path.join(sandbox, "felucca-base", "build", "host", "last.log"), os.path.join(a.log, g + ".full.log"))
            except OSError:
                pass
            remove_sandbox(sandbox)
        secs = int(time.time() - t0)
        text = open(out, errors="replace").read()
        st, why = verdict(rc, text)
        with self.lock:
            self.results[g] = (st, secs, why)
            if st == "PASS":
                print("PASS    %s (%d s)" % (g, secs), flush=True)
            elif st == "SKIP":
                print("SKIP    %s (%s) (%d s)" % (g, why, secs), flush=True)
            elif st == "TIMEOUT":
                print("TIMEOUT %s (over %d s)" % (g, a.limit), flush=True)
            else:
                print("FAIL    %s (%d s)" % (g, secs), flush=True)
                print("          " + why, flush=True)

    def run(self, groups):
        a = self.a
        os.makedirs(self.sbroot, exist_ok=True)
        times = load_times()
        order = sorted(groups, key=lambda g: (-(times.get(g, 600 if g in SLOW_FIRST else 60)), g))
        print("== %d host groups, %d at a time (free memory %.1f GB; cache %s)" % (len(groups), a.jobs, free_gb(), "off" if not self.cache else "build/host-cache"),
              flush=True)
        with cf.ThreadPoolExecutor(max_workers=a.jobs) as ex:
            list(ex.map(self.run_group, order))
        shutil.rmtree(self.sbroot, ignore_errors=True)
        save_times({g: r[1] for g, r in self.results.items()})


def load_times():
    try:
        return json.load(open(os.path.join(FB, "build", "host", "grouptimes.json")))
    except Exception:
        return {}


def save_times(new):
    t = load_times(); t.update(new)
    os.makedirs(os.path.join(FB, "build", "host"), exist_ok=True)
    json.dump(t, open(os.path.join(FB, "build", "host", "grouptimes.json"), "w"), indent=0, sort_keys=True)


def gen_headers():
    """The headers every group includes, generated once (host_tests.sh's preamble, HT_GEN_ONLY=1)."""
    e = dict(os.environ, HT_GEN_ONLY="1")
    p = subprocess.run(["sh", "host_tests.sh"], cwd=REPO, env=e, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if p.returncode != 0:
        sys.exit("fastcheck: host_tests.sh preamble failed:\n" + p.stdout[-2000:])


# ---------------------------------------------------------------------------------------------------------------- the group map

PATHRE = re.compile(r"(?<![\w$/.-])(?:\.\./)?((?:tests|tools|firmware|web|docs|jam)/[\w./*+@-]*[\w*])")
STRRE = re.compile(r'"((?:tests|tools|firmware|web|docs)/[\w./+-]+\.\w+)"')
IMPRE = re.compile(r"^\s*(?:from|import)\s+([A-Za-z_]\w*)", re.M)
JSIMP = re.compile(r"""from\s+['"](\.[^'"]+)['"]|import\s+['"](\.[^'"]+)['"]|require\(\s*['"](\.[^'"]+)['"]""")


def parse_hostsh(text):
    """-> (prelude_text_without_comments, {group: body_text}, {function: text}); body of a group includes the functions it calls."""
    lines = text.split("\n")
    funcs, i = {}, 0
    while i < len(lines):
        m = re.match(r"^(\w+)\(\) *\{", lines[i])
        if m:
            j = i
            if lines[i].count("{") > lines[i].count("}"):      # (a one-line function ends on its own line)
                while j < len(lines) and not lines[j].startswith("}"):
                    j += 1
            funcs[m.group(1)] = "\n".join(lines[i:j + 1]); i = j
        i += 1
    start = next(k for k, l in enumerate(lines) if l.startswith("for t in $want; do case $t in"))
    end = next(k for k in range(len(lines) - 1, start, -1) if lines[k].startswith("esac"))
    bodies, cur, names = {}, [], []
    entry = re.compile(r"^  ([A-Za-z0-9_$|]+)\) ")
    for l in lines[start + 1:end]:
        m = entry.match(l)
        if m:
            if names:
                for n in names:
                    bodies[n] = "\n".join(cur)
            names, cur = m.group(1).split("|"), [l]
        else:
            cur.append(l)
    for n in names:
        bodies[n] = "\n".join(cur)
    for n in list(bodies):                         # the functions a body calls
        for fn, ft in funcs.items():
            if re.search(r"(?<![\w-])%s(?![\w-])" % re.escape(fn), bodies[n]):
                bodies[n] += "\n" + ft
    prelude = [l for k, l in enumerate(lines) if not (start <= k <= end) and not l.lstrip().startswith("#") and not l.startswith("want=")]
    pre = "\n".join(prelude)
    for ft in funcs.values():                      # function texts are compared through the groups that call them
        pre = pre.replace(strip_comments(ft), "")
    return pre, {n: strip_comments(b) for n, b in bodies.items()}, funcs


def strip_comments(b):
    return "\n".join(l for l in b.split("\n") if not l.lstrip().startswith("#"))


STRUCT = re.compile(r"#\s*include|^\s*(?:import|from)\s|require\(|\b(?:tests|tools|firmware|web|docs|jam)/")


def tree_key():
    """what the map depends on: the file lists and the lines that name other files (#include, import, paths) -- an edit elsewhere keeps the map."""
    h = hashlib.sha1()
    h.update(open(os.path.join(REPO, "host_tests.sh"), "rb").read())
    h.update(open(os.path.abspath(__file__), "rb").read())          # (a new map logic rebuilds the map)
    for top in ("firmware", "tests", "tools", "web"):
        for dp, dn, fn in os.walk(os.path.join(FB, top)):
            dn[:] = sorted(d for d in dn if d not in ("node_modules", "__pycache__"))
            for f in sorted(fn):
                h.update(("%s/%s\n" % (dp[len(FB):], f)).encode())
                if f.endswith((".c", ".h", ".py", ".mjs", ".js", ".sh")):
                    try:
                        for ln in open(os.path.join(dp, f), errors="replace"):
                            if STRUCT.search(ln):
                                h.update(ln.encode("utf-8", "replace"))
                    except OSError:
                        pass
    return h.hexdigest()


def rel_repo(p):
    """felucca-base-relative or absolute path -> repo-relative posix path, or None."""
    p = posix(os.path.abspath(p))
    r = posix(REPO) + "/"
    return p[len(r):] if p.lower().startswith(r.lower()) else None


def expand_tokens(text):
    """repo paths named in a script / C text: existing files (globs expanded), as repo-relative paths."""
    found = set()
    for m in PATHRE.finditer(text):
        tok = m.group(1)
        for base in (FB, REPO):
            hits = glob.glob(os.path.join(base, tok)) if "*" in tok else ([os.path.join(base, tok)] if os.path.exists(os.path.join(base, tok)) else [])
            for h in hits:
                if os.path.isfile(h):
                    found.add(rel_repo(h))
                elif os.path.isdir(h):             # a whole folder named: everything in it
                    for dp, dn, fn in os.walk(h):
                        dn[:] = [d for d in dn if d not in ("node_modules", "__pycache__", "build")]
                        found.update(rel_repo(os.path.join(dp, f)) for f in fn)
            if hits:
                break
    return found


def local_imports(path, text):
    out = set()
    d = os.path.dirname(path)
    if path.endswith(".py"):
        for m in IMPRE.finditer(text):
            for base in (d, os.path.join(FB, "tools")):
                f = os.path.join(base, m.group(1) + ".py")
                if os.path.isfile(f):
                    out.add(f); break
    elif path.endswith((".mjs", ".js")):
        for m in JSIMP.finditer(text):
            spec = next(x for x in m.groups() if x)
            f = os.path.normpath(os.path.join(d, spec))
            for c in (f, f + ".mjs", f + ".js"):
                if os.path.isfile(c):
                    out.add(c); break
    return out


def close_files(seed, depth=4, exclude=()):
    """seed repo-relative paths + the files they name / import (scripts and test sources), transitively."""
    seen, todo = set(), [(s, 0) for s in seed if s]
    while todo:
        f, d = todo.pop()
        if f in seen or f.startswith(exclude):
            continue
        seen.add(f)
        full = os.path.join(REPO, f)
        if d >= depth or not os.path.isfile(full) or not f.endswith((".py", ".mjs", ".js", ".sh", ".c", ".h")):
            continue
        try:
            text = open(full, errors="replace").read()
        except OSError:
            continue
        nxt = set(STRRE.findall(text)) if f.endswith((".c", ".h")) else expand_tokens(text)
        nxt = {rel_repo(os.path.join(FB, n)) if os.path.exists(os.path.join(FB, n)) else n for n in nxt} if f.endswith((".c", ".h")) else nxt
        nxt |= {rel_repo(x) for x in local_imports(full, text)}
        todo.extend((n, d + 1) for n in nxt if n and n not in seen and not n.startswith("felucca-base/build"))
    return seen


def cc_deps(realcc, src):
    """the project files a translation unit reads (cc -E -MD)."""
    fd, tmp = tempfile.mkstemp(suffix=".d"); os.close(fd)
    try:
        subprocess.run(["sh", "-c", 'cd "$1" && d="$2" && shift 2 && "$@" -E -MD -MF "$d" -MT x -o "$d.i"', "x", FB, posix(tmp), realcc] + CCFLAGS + [src],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        text = open(tmp, errors="replace").read().replace("\\" + "\n", " ").replace("\r", "")
    except OSError:
        text = ""
    finally:
        for f in (tmp, tmp + ".i"):
            try: os.unlink(f)
            except OSError: pass
    if not text.strip():
        return None                                 # not a stand-alone translation unit with these flags: the caller is conservative
    out = set()
    for t in text.split()[1:]:
        if t == "\\":
            continue
        r = rel_repo(t) if (os.path.isabs(t) or re.match(r"^[A-Za-z]:", t)) else rel_repo(os.path.join(FB, t))
        if r and not r.startswith("felucca-base/build/host"):
            out.add(r)
    return out


def build_map(realcc, a):
    key = tree_key()
    mp = os.path.join(FB, "build", "host", "groupmap.json")
    try:
        old = json.load(open(mp))
        if old.get("key") == key:
            return old
    except Exception:
        pass
    print("== building build/host/groupmap.json (the sources each group compiles and the files it reads) ...", flush=True)
    t0 = time.time()
    text = open(os.path.join(REPO, "host_tests.sh"), newline="").read()
    pre, bodies, funcs = parse_hostsh(text)
    groups = default_groups()
    jobs = {}
    csrc = set()
    for g in groups:
        body = bodies.get(g, "")
        named = expand_tokens(body)
        jobs[g] = named
        csrc |= {n[len("felucca-base/"):] for n in named if n.endswith(".c") and n.startswith("felucca-base/")}
    with cf.ThreadPoolExecutor(max_workers=max(2, a.jobs)) as ex:
        cdeps = dict(zip(sorted(csrc), ex.map(lambda s: cc_deps(realcc, s), sorted(csrc))))
    # the generated headers (build/gen*/X.h): X.h is made by the tools/ scripts that name it (else by all gen_*.py); their inputs, never tests / web
    tools = sorted(glob.glob(os.path.join(FB, "tools", "*.py")))
    ttext = {rel_repo(t): open(t, errors="replace").read() for t in tools}
    genexcl = ("felucca-base/tests/", "felucca-base/web/", "jam/", "felucca-base/build/")
    gen_all = [r for r in ttext if os.path.basename(r).startswith("gen_") or r.endswith("/build.py")]
    gen_cache = {}

    def gen_of(h):
        if h not in gen_cache:
            roots = [r for r, t in ttext.items() if h in t] or gen_all
            if h == "duell_version.h":
                roots.append("VERSION")
            gen_cache[h] = close_files(roots + ["VERSION", "TIDE_VERSION"] if h.startswith("duell_version") else roots, exclude=genexcl)
        return gen_cache[h]
    allfw = {rel_repo(os.path.join(dp, f)) for dp, dn, fn in os.walk(os.path.join(FB, "firmware")) for f in fn}
    allfw |= {rel_repo(f) for f in glob.glob(os.path.join(FB, "tests", "*.h"))}
    gmap = {}
    for g in groups:
        files = set(jobs[g])
        for n in list(files):
            if n.endswith(".c") and n.startswith("felucca-base/"):
                d = cdeps.get(n[len("felucca-base/"):])
                files |= d if d is not None else allfw
        gen = set()
        for f in [f for f in files if f.startswith("felucca-base/build/gen")]:
            gen |= gen_of(os.path.basename(f))
        files = {f for f in files if not f.startswith("felucca-base/build/")}
        gmap[g] = sorted(close_files(files) | gen)
    res = {"key": key, "groups": gmap, "bodies": {g: hashlib.sha1(bodies.get(g, "").encode()).hexdigest() for g in groups},
           "prelude": hashlib.sha1(pre.encode()).hexdigest(), "seconds": round(time.time() - t0, 1)}
    os.makedirs(os.path.dirname(mp), exist_ok=True)
    json.dump(res, open(mp, "w"), indent=0, sort_keys=True)
    print("   %d groups mapped in %.0f s" % (len(groups), time.time() - t0), flush=True)
    return res


def sh(args, **kw):
    return subprocess.run(args, cwd=REPO, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, **kw).stdout


def changed_groups(base, gmap, groups):
    """-> (groups to run, extra tasks {'docs': bool, 'jamtools': bool, 'device': bool}, notes)."""
    files = set(sh(["git", "diff", "--name-only", "--no-renames", base]).split("\n")) | set(sh(["git", "ls-files", "-o", "--exclude-standard"]).split("\n"))
    files = {f for f in files if f}
    notes, run = [], set()
    deleted = {f for f in files if not os.path.exists(os.path.join(REPO, f))}
    rev = {}
    allfw = None
    for g, fl in gmap["groups"].items():
        fl = set(fl) | dyn_deps(g)                  # (+ what the group's compiles really read in its last run, build/host/groupdeps)
        if not os.path.exists(os.path.join(DEPS_DEFAULT, g + ".deps")):
            if allfw is None:                       # never run here: its programs may read more than the map found; the firmware is the usual rest
                allfw = {rel_repo(os.path.join(dp, f)) for dp, dn, fn in os.walk(os.path.join(FB, "firmware")) for f in fn}
            fl |= allfw
        for f in fl:
            rev.setdefault(f, set()).add(g)
    # host_tests.sh: the groups whose own body (or called function) changed, or all if the shared part changed
    if "host_tests.sh" in files:
        old = sh(["git", "show", "%s:host_tests.sh" % base])
        if old:
            opre, obod, _ = parse_hostsh(old)
            npre, nbod, _ = parse_hostsh(open(os.path.join(REPO, "host_tests.sh"), newline="").read())
            if opre != npre:
                notes.append("host_tests.sh: the shared part changed -> every group")
                run |= set(groups)
            else:
                ch = {g for g in set(obod) | set(nbod) if obod.get(g) != nbod.get(g)}
                if ch:
                    notes.append("host_tests.sh: body changed: " + " ".join(sorted(ch)))
                run |= ch
        else:
            run |= set(groups)
    docs_only = True
    for f in sorted(files):
        if f == "host_tests.sh":
            docs_only = False; continue
        if f in rev:
            run |= rev[f]; docs_only = False
        elif f in deleted:
            notes.append("deleted: %s -> every group" % f)
            run |= set(groups); docs_only = False
        elif f.startswith(("felucca-base/",)) and not f.endswith(".md"):
            notes.append("no host group reads %s" % f); docs_only = docs_only and False
        elif not (f.endswith((".md", ".txt", ".csv", ".json")) or f.startswith(("docs/", ".github/"))) and not f.startswith("jam/"):
            docs_only = False
    extra = {"docs": True,
             "jamtools": any(f.startswith("jam/") and not f.startswith("jam/ccwrap") for f in files),
             "device": any(f.startswith(("felucca-base/firmware/", "felucca-base/tools/", "felucca-base/tests/", "felucca-base/web/")) for f in files) and not docs_only}
    return sorted(run & set(groups)), extra, notes, files


DEPS_DEFAULT = os.path.join(FB, "build", "host", "groupdeps")


def dyn_deps(g, deps_dir=None):
    """the project files the compiles of group g read in its last run (jam/ccwrap/cc writes them)."""
    f = os.path.join(deps_dir or DEPS_DEFAULT, g + ".deps")
    out = set()
    try:
        lines = open(f, errors="replace").read().split("\n")
    except OSError:
        return out
    for l in lines:
        l = l.strip().rstrip("\\").strip()
        if not l or l == "\\" or re.match(r"^[A-Za-z]:/", l) or l.startswith("/"):
            continue
        r = rel_repo(os.path.join(FB, l))
        if r and not r.startswith("felucca-base/build/"):
            out.add(r)
    return out


def verify_map(a):
    """after a run with HT_DEPS_DIR: every project file a group's compiles read must be in the map."""
    mp = json.load(open(os.path.join(FB, "build", "host", "groupmap.json")))
    bad = 0
    for f in sorted(glob.glob(os.path.join(a.deps_dir, "*.deps"))):
        g = os.path.basename(f)[:-5]
        have = set(mp["groups"].get(g, []))
        seen = dyn_deps(g, a.deps_dir)
        miss = sorted(seen - have)
        if miss:
            bad += 1
            print("map misses for %s: %s" % (g, " ".join(miss[:8])))
    print("== map check: %s" % ("every compiled file is in the static map" if not bad else "%d group(s) with files only the recorded deps list (build/host/groupdeps covers them)" % bad))
    return bad == 0


# ---------------------------------------------------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("groups", nargs="*")
    ap.add_argument("-j", "--jobs", type=int, default=0)
    ap.add_argument("--limit", type=int, default=int(os.environ.get("JAM_TEST_LIMIT", "900")))
    ap.add_argument("--log", default=os.path.join(FB, "build", "check", "fast"))
    ap.add_argument("--changed", nargs="?", const="fork/b9", default=None)
    ap.add_argument("--shard", default=None)
    ap.add_argument("--out", default=None)
    ap.add_argument("--summary", default=None)
    ap.add_argument("--no-cache", action="store_true")
    ap.add_argument("--deps-dir", default=None)
    ap.add_argument("--map", action="store_true", help="only build / refresh build/host/groupmap.json")
    ap.add_argument("--verify-map", action="store_true", help="record the files every compile reads and check them against the map")
    ap.add_argument("--list-changed", action="store_true", help="with --changed: print the groups and stop")
    a = ap.parse_args()
    a.deps_dir = os.path.abspath(a.deps_dir or os.path.join(FB, "build", "host", "groupdeps"))
    os.makedirs(a.deps_dir, exist_ok=True)
    if not a.jobs:
        a.jobs = default_jobs()
    a.log = os.path.abspath(a.log)
    os.makedirs(a.log, exist_ok=True)
    if not os.environ.get("HT_REALCC"):
        r = subprocess.run(["sh", "-c", "command -v cc"], stdout=subprocess.PIPE, text=True).stdout.strip()
        os.environ["HT_REALCC"] = r
    if not os.environ.get("HT_REALCC"):
        sys.exit("fastcheck: no cc in PATH (source jam/env.sh first)")
    groups = a.groups or default_groups()
    groups = [g for g in groups if g]
    gen_headers()
    if a.map or a.changed is not None:
        gm = build_map(os.environ["HT_REALCC"], a)
        if a.map and not a.groups and a.changed is None:
            return 0
    extra = {}
    if a.changed is not None:
        sel, extra, notes, files = changed_groups(a.changed, gm, [g for g in default_groups()])
        for n in notes:
            print("   note: " + n)
        print("== changed since %s: %d files -> %d of %d groups: %s" % (a.changed, len(files), len(sel), len(default_groups()), " ".join(sel) or "(none)"))
        if a.groups:
            sel = [g for g in sel if g in a.groups]
        groups = sel
        if a.list_changed:
            return 0
    if a.shard:                                     # (as the CI's awk did: the default list without the $EMU token, every N-th group from place I)
        i, n = (int(x) for x in a.shard.split("/"))
        groups = [g for g in groups if not g.startswith("$")]
        groups = [g for k, g in enumerate(groups) if k % n == i]
    if a.verify_map:
        build_map(os.environ["HT_REALCC"], a)
    expanded = []
    for g in groups:
        if g == "$EMU":
            if shutil.which("emcc") or os.path.exists(os.path.expanduser("~/emsdk/emsdk_env.sh")):
                expanded.append("emu")
            else:
                print("SKIP    emu (SDK unavailable)")
        else:
            expanded.append(g)
    t0 = time.time()
    r = Runner(a)
    r.run(expanded)
    res = r.results
    cnt = {k: sum(1 for v in res.values() if v[0] == k) for k in ("PASS", "FAIL", "TIMEOUT", "SKIP")}
    failed = sorted(g for g, v in res.items() if v[0] in ("FAIL", "TIMEOUT"))
    with open(os.path.join(a.log, "results.tsv"), "w") as f:
        for g in sorted(res):
            f.write("%s\t%s\t%d\t%s\n" % (g, res[g][0], res[g][1], res[g][2]))
    if a.out:                                       # the groups' own output, one after the other (what the CI summary steps read)
        with open(a.out, "w", errors="replace") as f:
            for g in expanded:
                p = os.path.join(a.log, g + ".log")
                if os.path.exists(p):
                    f.write(open(p, errors="replace").read())
                    if res.get(g, ("",))[0] == "TIMEOUT":
                        f.write("   FAIL (group %s: timed out)\n" % g)
    if a.summary:
        with open(a.summary, "a") as f:
            f.write("### Host tests: %d passed, %d failed, %d skipped\n" % (cnt["PASS"], cnt["FAIL"] + cnt["TIMEOUT"], cnt["SKIP"]))
            if failed:
                f.write("| failed group | first failing line |\n|---|---|\n")
                for g in failed:
                    f.write("| %s | `%s` |\n" % (g, res[g][2].replace("|", "/").replace("`", "'")))
    if failed:
        print("== failed groups and their first failing line:")
        for g in failed:
            print("   %-14s %s" % (g, res[g][2]))
    hits = misses = 0
    try:
        for l in open(os.path.join(FB, "build", "host-cache", "stats.log")):
            hits += l.startswith("hit"); misses += l.startswith("miss")
    except OSError:
        pass
    print("== fastcheck: %d PASS, %d FAIL, %d SKIP in %d s (%d at a time); compile cache so far: %d hits, %d misses" %
          (cnt["PASS"], cnt["FAIL"] + cnt["TIMEOUT"], cnt["SKIP"], time.time() - t0, a.jobs, hits, misses))
    if a.verify_map:
        verify_map(a)
    json.dump({"pass": cnt["PASS"], "fail": failed, "skip": cnt["SKIP"], "extra": extra}, open(os.path.join(a.log, "summary.json"), "w"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
