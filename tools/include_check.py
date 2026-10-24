#!/usr/bin/env python3
"""include_check — every std symbol a file uses must have its header in THAT file.

WIRED: ./verify fast, inside portability_gate()

WHY. CI's Windows build (MSVC) is the only compiler that sees MSVC-specific
breakage, and it runs after a push. On 2026-09-17 (PR #607) `routing_check.cpp`
used `std::string` with no `<string>` include: clang's library reaches it
through another header, MSVC's does not, so every local gate was green and
only `build-windows` went red. The existing portability gate knew one MSVC
trap (`M_PI`); this is the second class, and it is a pattern grep can catch.
Human ruling 2026-09-17 (B140): extend the gate.

THE RULE IS STRICT ON PURPOSE. "It compiles on MSVC today" is not the bar —
transitive reach differs per library and per version, so a file that relies
on it is one upstream refactor from red. Include what you use; the file's own
include list is the fix, always. False positives are therefore impossible by
construction: a used symbol either has its header in the file or it does not.

TABLE. Symbol -> the header(s) that declare it (any one satisfies). Grow it
when a new class bites; keep it to symbols that have actually bitten or are
certain to (containers, strings, C-library wrappers, <algorithm>).
Comments are stripped before scanning so a mention in prose never counts.

SCOPE (B535, repo audit 2026-10-21 H4). GLOBS names the roots, and engine2/ is one of
them, ALL of it (core, tests, tools): the clean-room engine and its suite compile into the
same MSVC targets as h2/, and the rule is per file. No exclusion.

MUST-FAIL CONTROLS, every run (LIBRARY L0032): in a scratch tree laid out like the repo,
a file under engine2/core/ and one under engine2/tests/ that each use std::vector with no
<vector> must both be reported by name, and the same files with the include must read
clean (the must-read-zero), through the same files_of() and scan() as the verdict.
"""
import pathlib
import re
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
GLOBS = ("tools/*.cpp", "src/*.h", "src/*.cpp", "src/gui/*.h", "src/gui/*.cpp", "src/gui/*.mm",
         # widened 2026-09-28 (B332 critic review): horde 2's cores and the tools'
         # shared headers compile into MSVC targets too
         "tools/*.h", "h2/**/*.h", "h2/**/*.cpp",
         # B535: the clean-room engine and its suite, whole (no exclusion)
         "engine2/**/*.h", "engine2/**/*.cpp")
TABLE = {
    "string": ("string",), "vector": ("vector",), "array": ("array",), "map": ("map",),
    "unordered_map": ("unordered_map",), "set": ("set",), "unique_ptr": ("memory",), "shared_ptr": ("memory",),
    "function": ("functional",), "sort": ("algorithm",), "min": ("algorithm",), "max": ("algorithm",),
    "clamp": ("algorithm",), "fill": ("algorithm",), "copy": ("algorithm",),
    "memcpy": ("cstring",), "memset": ("cstring",), "strlen": ("cstring",), "strcmp": ("cstring",),
    "strstr": ("cstring",), "strchr": ("cstring",), "strncmp": ("cstring",),
    "snprintf": ("cstdio",), "printf": ("cstdio",), "fprintf": ("cstdio",), "fopen": ("cstdio",),
    "atoi": ("cstdlib",), "atof": ("cstdlib",), "strtod": ("cstdlib",), "strtol": ("cstdlib",), "abs": ("cstdlib", "cmath"),
    "sqrt": ("cmath",), "sin": ("cmath",), "cos": ("cmath",), "exp": ("cmath",), "log": ("cmath",), "pow": ("cmath",),
    "fabs": ("cmath",), "tanh": ("cmath",), "lround": ("cmath",), "floor": ("cmath",), "ceil": ("cmath",), "round": ("cmath",),
    "atomic": ("atomic",), "mutex": ("mutex",), "thread": ("thread",),
    "ostringstream": ("sstream",), "ifstream": ("fstream",), "ofstream": ("fstream",), "runtime_error": ("stdexcept",),
}


def scan(path):
    text = path.read_text(errors="ignore")
    code = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)
    inc = set(re.findall(r"#include\s*<([^>]+)>", text))
    used = set(re.findall(r"\bstd::([A-Za-z_][A-Za-z0-9_]*)", code))
    return sorted(f"std::{s} needs <{'|'.join(TABLE[s])}>" for s in used
                  if s in TABLE and not any(h in inc for h in TABLE[s]))


def files_of(root):
    return [p for g in GLOBS for p in sorted(root.glob(g))]


def selftest():
    """-> a failure string, or None."""
    with tempfile.TemporaryDirectory() as td:
        td = pathlib.Path(td)
        planted = ("engine2/core/planted.h", "engine2/tests/planted.cpp")
        for rel in planted:
            (td / rel).parent.mkdir(parents=True, exist_ok=True)
            (td / rel).write_text("#include <cmath>\nstd::vector<double> v;\n")
        got = sorted(str(f.relative_to(td)) for f in files_of(td) if scan(f))
        if got != sorted(planted):
            return f"the engine2 plants read {got}, want {sorted(planted)} (a root that is not in GLOBS is not seen)"
        for rel in planted:
            (td / rel).write_text("#include <vector>\nstd::vector<double> v;\n")
        got = [str(f.relative_to(td)) for f in files_of(td) if scan(f)]
        if got:
            return f"the conforming engine2 files read red: {got} (a judge that is always red passes every plant)"
    return None


def main():
    bad = selftest()
    if bad:
        print(f"include_check: FAILED — a must-fail control did not behave: {bad}", file=sys.stderr)
        return 1
    files = files_of(ROOT)
    bad = {str(f.relative_to(ROOT)): v for f in files if (v := scan(f))}
    if bad:
        print(f"include_check: FAILED — {len(bad)} file(s) use a std symbol without its header (MSVC does not reach it transitively):", file=sys.stderr)
        for f, v in bad.items():
            for line in v:
                print(f"  {f}: {line}", file=sys.stderr)
        return 1
    print(f"include_check: GREEN ({len(files)} files, every used std symbol has its header in the file; {sum(1 for f in files if f.relative_to(ROOT).parts[0] == 'engine2')} under engine2/; engine2 plants fired)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
