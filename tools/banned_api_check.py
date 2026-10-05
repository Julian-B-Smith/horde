#!/usr/bin/env python3
"""banned_api_check -- no unbounded-copy or shell-out calls in src/ and h2/ (B446 W1b).

WIRED: ./verify fast

WHY. The plugin parses host-supplied state and preset data in a process the host
owns; an unbounded string function or a shell-out there is an overflow or a
command-injection waiting for one bad caller. The tree is clean today, so this is
a ratchet: it keeps it clean without relying on anyone's memory of the list.

BANNED, as CALLS in C/C++ code: strcpy, strcat, sprintf, vsprintf, gets, system,
popen (also reached as std::NAME or ::NAME). snprintf and vsnprintf are different
identifiers and are allowed; so is a MEMBER of that name (`obj.system(`, `p->gets(`)
or one qualified by another class (`Foo::system(`) -- those are not libc.

CODE ONLY. Comments, string literals (plain and raw), and character literals are
blanked before matching, newlines kept so line numbers stay true. Two lexer traps
are handled on purpose: a C++14 digit separator (`1'000`) is NOT the start of a
character literal, and a `//` comment ending in a backslash continues onto the
next line. The controls below fail if either goes wrong.

SCOPE. Every tracked or new-and-not-ignored C/C++ file under src/ and h2/
(`git ls-files -co --exclude-standard`; a brand-new file is exactly the case that
would otherwise slip through). A file that cannot be read is RED, and so is a
tree in which no file is found at all -- a scanner that matched nothing must not
read as a clean tree. Code under `#if 0` is still scanned (fail closed). It does
not see calls assembled by macro or by pointer.

MUST-FAIL CONTROLS, on in-memory text every run, plus one end-to-end in a scratch
repo (a planted line in a temp copy of the layout): each banned name is flagged;
the same names inside a comment, a string, a raw string and a char literal are
not; snprintf, a member call and a Foo:: call are not; the digit-separator and
comment-continuation traps read as designed.
"""
import os
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
DIRS = ("src", "h2")
EXTS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".ipp", ".tpp", ".inc", ".m", ".mm"}
BANNED = ("strcpy", "strcat", "sprintf", "vsprintf", "gets", "system", "popen")
NAME_RE = re.compile(r"(?<![A-Za-z0-9_])(" + "|".join(BANNED) + r")(?![A-Za-z0-9_])")


def strip_noncode(t):
    """Blank comments, strings and char literals with spaces (newlines kept)."""
    out = []
    i, n = 0, len(t)

    def blank(s):
        return "".join("\n" if c == "\n" else " " for c in s)

    while i < n:
        c = t[i]
        two = t[i:i + 2]
        if two == "//":
            j = i
            while True:                     # a trailing backslash continues the comment
                nl = t.find("\n", j)
                if nl < 0:
                    j = n
                    break
                k = nl - 1
                if k >= i and t[k] == "\r":
                    k -= 1
                if k >= i and t[k] == "\\":
                    j = nl + 1
                    continue
                j = nl
                break
            out.append(blank(t[i:j]))
            i = j
        elif two == "/*":
            j = t.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(blank(t[i:j]))
            i = j
        elif c == '"':
            # R, or an encoding prefix + R, that is not the tail of a longer identifier
            m = re.search(r"(?:^|[^A-Za-z0-9_])(?:u8|u|U|L)?R$", t[max(0, i - 5):i])
            raw = None
            if m:
                p = t.find("(", i + 1)
                delim = t[i + 1:p] if p > 0 else None
                if delim is not None and len(delim) <= 16 and not re.search(r'[\s\\)"]', delim):
                    end = t.find(")" + delim + '"', p)
                    raw = n if end < 0 else end + len(delim) + 2
            if raw is not None:
                out.append(blank(t[i:raw]))
                i = raw
            else:
                j = i + 1
                while j < n and t[j] != '"' and t[j] != "\n":
                    j += 2 if t[j] == "\\" else 1
                j = min(j + 1, n) if j < n and t[j] == '"' else min(j, n)
                out.append(blank(t[i:j]))
                i = j
        elif c == "'":
            k = i
            while k > 0 and re.match(r"[0-9A-Za-z_.']", t[k - 1]):
                k -= 1
            if i > 0 and k < i and t[k].isdigit():      # 1'000: digit separator, still code
                out.append(c)
                i += 1
                continue
            j = i + 1
            while j < n and t[j] != "'" and t[j] != "\n":
                j += 2 if t[j] == "\\" else 1
            j = min(j + 1, n) if j < n and t[j] == "'" else min(j, n)
            out.append(blank(t[i:j]))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def scan_text(text):
    """-> [(line, name)] for every banned CALL in the code of `text`."""
    code = strip_noncode(text)
    found = []
    for m in NAME_RE.finditer(code):
        before = code[:m.start()].rstrip()
        after = code[m.end():].lstrip()
        qualified_std = False
        if before.endswith(".") or before.endswith("->"):
            continue                                    # a member, not libc
        if before.endswith("::"):
            q = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*$", before[:-2])
            if q and q.group(1) != "std":
                continue                                # Foo::system
            qualified_std = bool(q)
        if after.startswith("(") or qualified_std:
            found.append((code.count("\n", 0, m.start()) + 1, m.group(1)))
    return found


def list_files(cwd):
    r = subprocess.run(["git", "ls-files", "-co", "--exclude-standard", "-z", "--", *DIRS],
                       cwd=cwd, capture_output=True, text=True,
                       env={k: v for k, v in os.environ.items() if not k.startswith("GIT_")})
    if r.returncode != 0:
        return None, f"git ls-files failed (exit {r.returncode})"
    files = [f for f in r.stdout.split("\0") if f and pathlib.PurePosixPath(f).suffix in EXTS]
    return sorted(set(files)), None


def check(cwd):
    """-> (problems, n_files). Every problem is a printable line."""
    files, err = list_files(cwd)
    if err:
        return [err], 0
    if not files:
        return [f"no C/C++ file found under {'/'.join(DIRS)} -- a scan that saw nothing is blind, not clean"], 0
    problems = []
    for f in files:
        try:
            text = (pathlib.Path(cwd) / f).read_text(encoding="utf-8", errors="replace")
        except OSError as e:
            problems.append(f"{f}: unreadable ({type(e).__name__}) -- fail closed")
            continue
        for line, name in scan_text(text):
            problems.append(f"{f}:{line}: banned call {name}")
    return problems, len(files)


def selftest():
    must_flag = {
        "plain call": "void f(char*a){ strcpy(a, b); }",
        "spaced call": "void f(){ system  (cmd); }",
        "std-qualified": "void f(){ std::system(c); }",
        "global-qualified": "void f(){ ::popen(c, r); }",
        "after a comment": "/* ok */ sprintf(buf, f);",
        "digit separator then call": "int n = 1'000; strcat(a, b);",
        "after a string": 'const char* s = "x"; gets(buf);',
        "vsprintf": "vsprintf(b, f, ap);",
        "std name, no call": "auto p = std::strcpy;",
        "after a plain // comment line": "// note\nstrcpy(a,b);\nint x;",
    }
    must_pass = {
        "snprintf": "snprintf(b, n, f);",
        "vsnprintf": "vsnprintf(b, n, f, ap);",
        "line comment": "// strcpy(a, b); system(x);",
        "block comment": "/* sprintf(b, f);\n gets(b); */ int x;",
        "string": 'const char* s = "call system( and strcpy(";',
        "string with escaped quote": 'const char* s = "a\\" popen(c) b";',
        "raw string": 'auto s = R"x(system(c) and gets(b))x";',
        "char literal": "char q = '\"'; int ok = 1;",
        "member call": "obj.system(c); p->gets(b);",
        "other-class qualifier": "Foo::system(c);",
        "plain identifier": "int system = 0; int gets;",
        "longer identifier": "my_strcpy(a, b); strcpy_safe(a); wcscpy(a, b);",
        "comment continued by backslash": "// note \\\n strcpy(a,b); still comment\nint x;",
        "digit separator not a char literal": "int n = 1'000; /* system(c) */ int m = 2'000;",
    }
    for name, src in must_flag.items():
        if not scan_text(src):
            return f"selftest: '{name}' was NOT flagged"
    for name, src in must_pass.items():
        got = scan_text(src)
        if got:
            return f"selftest: '{name}' was flagged ({got}) and must not be"

    # End to end: a clean temp layout reads clean, one planted line turns it red.
    with tempfile.TemporaryDirectory() as td:
        td = pathlib.Path(td)
        for d in DIRS:
            (td / d).mkdir()
        (td / "src" / "ok.cpp").write_text("// system(x)\nint f(){ return snprintf(0,0,\"\"); }\n")
        (td / "h2" / "ok.h").write_text("#pragma once\n")
        if subprocess.run(["git", "init", "-q"], cwd=td, capture_output=True).returncode != 0:
            return "selftest: could not create a scratch repo"
        probs, n = check(td)
        if probs or n != 2:
            return f"selftest: the clean scratch layout read red or miscounted ({probs}, {n})"
        (td / "h2" / "planted.h").write_text("inline void p(char* a){\n  strcpy(a, \"x\");\n}\n")
        probs, _ = check(td)
        if probs != ["h2/planted.h:2: banned call strcpy"]:
            return f"selftest: the planted line was not reported exactly ({probs})"
        (td / "h2" / "planted.h").unlink()
        (td / "h2" / "ok.h").unlink()
        (td / "src" / "ok.cpp").unlink()
        probs, _ = check(td)
        if not probs:
            return "selftest: an empty layout read clean (blind scanner)"
    return None


def main():
    bad = selftest()
    if bad:
        print(f"banned_api_check: FAILED -- {bad}", file=sys.stderr)
        return 1
    problems, n = check(ROOT)
    if problems:
        print("banned_api_check: FAILED -- banned C library calls (use snprintf / std::string / "
              "no shell-out):", file=sys.stderr)
        for p in problems:
            print(f"  {p}", file=sys.stderr)
        return 1
    print(f"banned_api_check: GREEN ({n} files under {'/'.join(DIRS)}; controls ok)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
