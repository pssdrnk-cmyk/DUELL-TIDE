#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""One firmware object for the host tests (b11 tests lane, HOSTOBJ stage 1; work/b11/reports/HOSTOBJ.md).

Every host test program is one translation unit: the test includes tests/hostsim.c, which includes the firmware. So every group compiles the whole
firmware again (about 11 s a group), and a one-file firmware edit recompiles all of them. This tool compiles the firmware (hostsim.c with
`#define main hostsim_main`) ONCE to an object and gives the groups a header to compile against instead:

    python3 jam/hostobj.py OUTDIR -- CC-COMMAND-WITH-ITS-FLAGS...

  1. preprocesses the unit with the group's own compiler flags (`cc -E -dD`: code and the surviving #defines),
  2. cuts the result into top-level items (a small brace / paren matcher; no C parser),
  3. writes OUTDIR/fw.c: the items of the project's files with `static` / `inline` dropped from the head of every definition (nothing external to the project
     is touched; function-local statics are inside bodies and stay), i.e. the same program with every file-scope symbol visible to the linker,
  4. writes OUTDIR/fwdecl.h: the system headers and the #defines as they are, type definitions as they are, and for every project function a prototype, for
     every project variable an `extern` declaration (no bodies, no initializers),
  5. compiles fw.c to OUTDIR/fw.o.

A group then compiles `tests/X.c` with `#include "hostsim.c"` replaced by `#include "fwdecl.h"` and links fw.o (host_tests.sh `ccfw`). Only groups whose test
does nothing before that include except `#define main hostsim_main` can do it; the others keep their unity line. Nothing in firmware/ or tests/ is edited and the
device build does not see any of this.

The cut is textual, so it reports what it could not do: arrays declared without a size whose size a test needs (`extern T x[]`), items it did not
recognise. `--report` prints the counts.
"""
import os, re, subprocess, sys

PROJECT_MARK = ("/firmware/", "/tests/", "firmware/", "tests/", "build/gen")


def is_project(path):
    return not path.startswith("/usr/") and not path.startswith("<") and (path.endswith(".c") or path.endswith(".h")) and not path.startswith("/lib")


def preprocess(cc, src, extra):
    cmd = cc + ["-E", "-dD", "-x", "c"] + extra + [src]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if p.returncode != 0:
        sys.exit("hostobj: preprocessing failed:\n" + p.stderr[-1500:])
    return p.stdout


LINEMARK = re.compile(r'^# (\d+) "([^"]*)"((?: \d+)*)\s*$')


def is_sys(path):
    return path.startswith("/usr/")


def sysname(path):
    """the #include <name> of a system header"""
    m = re.match(r"^(?:/usr/include/(?:x86_64-linux-gnu/)?|/usr/lib/gcc/[^/]+/[^/]+/include/)(.+)$", path)
    return m.group(1) if m else path


def items(text):
    """yield (kind, file, text): kind 'dir' for a directive line (#define / #undef / pragma), 'item' for a top-level declaration or definition (with the kind of
    its first '{' as extra: 'func', 'init', 'type', None)"""
    cur_file = ""
    skip = False                                    # inside <built-in> / <command-line>
    buf, depth, par, first_brace, body_kind = [], 0, 0, None, None
    i, n = 0, len(text)
    lines = text.split("\n")
    line_file = ""

    def flush():
        nonlocal buf, depth, par, first_brace, body_kind
        t = "\n".join(buf).strip("\n")
        buf, depth, par, first_brace, body_kind = [], 0, 0, None, None
        return t
    insys = False
    stack = []                                      # the files being read (an include stack from the line markers' flags 1 = enter, 2 = return)
    for ln in lines:
        m = LINEMARK.match(ln)
        if m:
            cur_file = m.group(2)
            flags = m.group(3).split()
            if "1" in flags:
                if is_sys(cur_file) and stack and not any(is_sys(x) for x in stack):
                    yield ("sysinc", cur_file, sysname(cur_file), None)     # (a system header a project file includes: the declaration header includes it again)
                stack.append(cur_file)
            elif "2" in flags:
                while stack and stack[-1] != cur_file:
                    stack.pop()
            elif not stack:
                stack.append(cur_file)
            skip = cur_file.startswith("<")
            insys = any(is_sys(x) for x in stack)
            continue
        if ln.startswith("#"):
            if not skip:
                yield ("sdir" if insys else "dir", cur_file, ln, None)
            continue
        if skip:
            continue
        if not ln.strip() and not buf:
            continue
        if not buf:
            line_file = cur_file
            line_sys = insys
        buf.append(ln)
        # scan the line for structure (strings and chars were left in the text: skip them)
        j, m = 0, len(ln)
        end_item = False
        while j < m:
            c = ln[j]
            if c in "\"'":
                q = c
                j += 1
                while j < m and ln[j] != q:
                    j += 2 if ln[j] == "\\" else 1
            elif c == "(":
                par += 1
            elif c == ")":
                par -= 1
            elif c == "{":
                if depth == 0:
                    head = "\n".join(buf[:-1] + [ln[:j]])
                    h = head.rstrip()
                    h0 = re.sub(r"\s+", " ", h)
                    # an '=' outside parentheses before the brace: an initializer
                    ptxt, d2, eq = "", 0, False
                    for ch in h0:
                        if ch == "(":
                            d2 += 1
                        elif ch == ")":
                            d2 -= 1
                        elif ch == "=" and d2 == 0:
                            eq = True
                    if eq:
                        first_brace = "init"
                    elif h.endswith(")"):
                        first_brace = "func"
                    else:
                        first_brace = "type"
                    body_kind = first_brace
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0 and body_kind == "func":
                    end_item = True
            elif c == ";" and depth == 0 and par == 0:
                end_item = True
            j += 1
        if end_item:
            kind = body_kind
            t = flush()
            yield ("sitem" if line_sys else "item", line_file, t, kind)
    if buf:
        yield ("sitem" if line_sys else "item", line_file, flush(), body_kind)


SPEC = re.compile(r"^\s*(?:(?:static|inline|__inline__|__inline|extern)\b\s*)+")


def head_body(t):
    """a function definition split at its body: (head, body)"""
    k = t.find("{")
    return t[:k].rstrip(), t[k:]


def fname(h):
    """the name of the function a head or prototype declares"""
    h = re.sub(r"__attribute__\s*\(\(.*?\)\)", "", h, flags=re.S)
    m = re.search(r"([A-Za-z_]\w*)\s*\(", h)
    return m.group(1) if m else None


def count_init(init):
    """the number of elements of a brace initializer `{ a, b, c }` (None for designated initializers or anything odd)"""
    init = init.strip()
    if not init.startswith("{") or not init.endswith("}"):
        return None
    body = init[1:-1]
    depth, n, cur, i, m = 0, 0, False, 0, len(body)
    while i < m:
        c = body[i]
        if c in "\"'":
            q = c
            cur = True
            i += 1
            while i < m and body[i] != q:
                i += 2 if body[i] == "\\" else 1
        elif c in "({[":
            if c == "[" and depth == 0 and not cur:
                return None                              # [index] = value
            depth += 1
            cur = True
        elif c in ")}]":
            depth -= 1
        elif c == "," and depth == 0:
            if cur:
                n += 1
            cur = False
        elif c == "." and depth == 0 and not cur:
            return None                                  # .field = value
        elif not c.isspace():
            cur = True
        i += 1
    return n + (1 if cur else 0)


def split_top(body, sep):
    """split at `sep` outside (), [], {} and strings"""
    out, depth, cur, i, m = [], 0, [], 0, len(body)
    while i < m:
        c = body[i]
        if c in "\"'":
            q = c
            j = i + 1
            while j < m and body[j] != q:
                j += 2 if body[j] == "\\" else 1
            cur.append(body[i:j + 1])
            i = j + 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        if c == sep and depth == 0:
            out.append("".join(cur))
            cur = []
        else:
            cur.append(c)
        i += 1
    out.append("".join(cur))
    return out


def extern_decls(s0, stats):
    """`static T a = 1, b[] = {..};` -> ['extern T a;', 'extern T b[2];'] (sizes of unsized arrays counted from the initializer)"""
    if re.match(r"^\s*(extern|typedef)", s0):
        return [s0]
    body = s0.rstrip().rstrip(";")
    pieces = split_top(body, ",")
    first_dec = split_top(pieces[0], "=")[0]
    if "(" in first_dec.split("[")[0] or len(pieces) == 1 and "(" in first_dec:   # (a function pointer variable: one declarator)
        pieces = [body]
        first_dec = split_top(body, "=")[0]
        return ["extern " + first_dec.rstrip() + ";"]
    names = list(re.finditer(r"[A-Za-z_]\w*", first_dec.split("[")[0]))
    if not names:
        return ["extern " + first_dec.rstrip() + ";"]
    nm = names[-1]
    spec = first_dec[: nm.start()].rstrip()
    stars = re.search(r"\**\s*$", spec).group(0) if spec else ""
    spec_base = spec[: len(spec) - len(stars)].rstrip() if stars.strip() else spec
    out = []
    for i, pc in enumerate(pieces):
        parts = split_top(pc, "=")
        dec = parts[0].rstrip()
        init = "=".join(parts[1:]).strip() if len(parts) > 1 else None
        if i == 0:
            dec = dec[len(spec_base):].lstrip() if dec.startswith(spec_base) else dec
        else:
            dec = dec.lstrip()
        if re.search(r"\[\s*\]", dec):
            cnt = count_init(init) if init else None
            if cnt is not None:
                dec = re.sub(r"\[\s*\]", "[%d]" % cnt, dec, count=1)
            else:
                stats["unsized"].append((spec_base + " " + dec)[:80])
        out.append("extern %s %s;" % (spec_base, dec))
    return out


def strip_spec(h):
    return SPEC.sub("", h, count=1)


def classify(t, kind):
    """'type' (typedef, struct / union / enum definition, static assert), 'func' (definition), 'proto' (function declaration), 'var' (variable, with or without
    initializer), 'other'"""
    s = t.lstrip()
    if s.startswith("typedef") or s.startswith("_Static_assert") or s.startswith("__extension__ typedef") or s.startswith(";"):
        return "type"
    if kind == "func":
        return "func"
    flat = re.sub(r"__attribute__\s*\(\(.*?\)\)", "", s, flags=re.S)
    # up to the first '=' / '{' / ';' outside parentheses
    d, cut = 0, len(flat)
    for k, ch in enumerate(flat):
        if ch == "(":
            d += 1
        elif ch == ")":
            d -= 1
        elif d == 0 and ch in "={;":
            cut = k
            break
    dec = flat[:cut]
    if kind == "type" and re.match(r"^\s*(?:(?:static|const|volatile|extern)\s+)*(struct|union|enum)\b[^;=]*$", flat.split("{")[0] if "{" in flat else flat) and not re.search(r"\}\s*[A-Za-z_*]", flat.rsplit("}", 1)[-1] if "}" in flat else ""):
        return "type"                                # struct X { ... };  (no declarator after the brace)
    if re.search(r"\(\s*\*\s*[A-Za-z_]\w*\s*\)\s*\(", dec):
        return "var"                                 # function pointer variable
    m = re.search(r"([A-Za-z_]\w*)\s*\(", dec)
    if m and not re.search(r"\[", dec[: m.start()]) and (kind is None):
        return "proto"
    if kind == "init" or kind == "type":
        return "var"
    return "var"


def make(cc, outdir, src, define_main=True, report=False, extra=None):
    os.makedirs(outdir, exist_ok=True)
    wrap = os.path.join(outdir, "fwunit.c")
    with open(wrap, "w") as f:
        f.write('#define main hostsim_main\n#include "%s"\n' % os.path.abspath(src))
    text = preprocess(cc, wrap, ["-I", os.path.dirname(os.path.abspath(src))] + (extra or []))
    obj_parts, decl_parts = [], []
    stats = {"func": 0, "proto": 0, "var": 0, "type": 0, "other": 0, "unsized": []}
    all_items = list(items(text))
    # functions the host build never emits: the ones with device assembly, and (closure) every function that mentions one - in the unit they are unused static
    # functions the compiler drops, as external ones they would be emitted and fail to assemble
    bodies = {}
    for kind, f, t, bk in all_items:
        if kind == "item" and bk == "func" and is_project(f):
            h, body = head_body(t)
            nm = fname(h)
            if nm:
                bodies[nm] = body
    asm_names = {nm for nm, b in bodies.items() if re.search(r"(?<![A-Za-z0-9_])(?:__asm__|__asm|asm)[ 	]*(?:volatile[ 	]*|__volatile__[ 	]*)?[(][ 	]*\"[^\"]", b)}     # (an asm with an empty template, a compiler barrier, is fine on the host)
    changed = True
    words ={nm: set(re.findall(r"[A-Za-z_]\w*", b)) for nm, b in bodies.items()}
    while changed:
        changed = False
        for nm, w in words.items():
            if nm not in asm_names and w & asm_names:
                asm_names.add(nm)
                changed = True
    for kind, f, t, bk in all_items:
        if kind == "sysinc":
            decl_parts.append("#include <%s>" % t)
            continue
        if kind == "sdir":
            continue
        if kind == "sitem":
            obj_parts.append(t)
            continue
        if kind in ("dir", "dirlate"):
            if t.startswith("#define main hostsim_main") or t.startswith("#undef main"):
                continue
            decl_parts.append(t)
            continue
        if not is_project(f) or "/usr/" in f:
            obj_parts.append(t)
            decl_parts.append(t)
            continue
        c = classify(t, bk)
        stats[c] += 1
        if c == "type":
            obj_parts.append(t)
            if not t.lstrip().startswith("_Static_assert"):      # (they size unsized arrays: only in the object)
                decl_parts.append(t)
        elif c == "func":
            h, body = head_body(t)
            if fname(h) in asm_names:     # (device-only, see above: stays static, not declared)
                obj_parts.append(t)
                stats["asm"] = stats.get("asm", 0) + 1
                continue
            h = strip_spec(h)
            obj_parts.append(h + " " + body)
            decl_parts.append(h + ";")
        elif c == "proto":
            if fname(t) in asm_names:                   # (its definition stays static, see above)
                obj_parts.append(t)
                continue
            h = strip_spec(t.rstrip().rstrip(";"))
            obj_parts.append(h + ";")
            decl_parts.append(h + ";")
        elif c == "var":
            # object: the item without static / inline; declaration: extern + each declarator without its initializer
            s0 = strip_spec(t)
            obj_parts.append(s0)
            decl_parts.extend(extern_decls(s0, stats))
        else:
            obj_parts.append(t)
            decl_parts.append(t)
    with open(os.path.join(outdir, "fw.c"), "w") as f:
        f.write("\n".join(obj_parts) + "\n")
    with open(os.path.join(outdir, "fwdecl.h"), "w") as f:
        f.write("/* generated by jam/hostobj.py: declarations of the firmware object fw.o */\n#pragma once\n" + "\n".join(decl_parts) + "\n")
    if report:
        print("hostobj: asm_names %d (%s); " % (len(asm_names), "events_block" in asm_names), end="")
        print("hostobj: %d function definitions, %d prototypes, %d variables, %d type items, %d other; %d unsized arrays" % (
            stats["func"], stats["proto"], stats["var"], stats["type"], stats["other"], len(stats["unsized"])))
        for u in stats["unsized"][:20]:
            print("   unsized: " + u)
    return stats


def main(argv):
    if "--" not in argv or len(argv) < 4:
        sys.exit(__doc__)
    outdir = argv[1]
    cc = argv[argv.index("--") + 1:]
    src = "tests/hostsim.c"
    st = make(cc, outdir, src, report=True)
    obj = os.path.join(outdir, "fw.o")
    cmd = cc + ["-c", "-o", obj, os.path.join(outdir, "fw.c")]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print("hostobj: compile fw.c: rc %d" % p.returncode)
    if p.returncode:
        print(p.stdout[-3000:])
    return p.returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv))
