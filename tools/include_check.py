#!/usr/bin/env python3
"""include_check — every std symbol a file uses must have its header in THAT file.

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
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
GLOBS = ("tools/*.cpp", "src/*.h", "src/*.cpp", "src/gui/*.h", "src/gui/*.cpp", "src/gui/*.mm")
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


def main():
    files = [p for g in GLOBS for p in sorted(ROOT.glob(g))]
    bad = {str(f.relative_to(ROOT)): v for f in files if (v := scan(f))}
    if bad:
        print(f"include_check: FAILED — {len(bad)} file(s) use a std symbol without its header (MSVC does not reach it transitively):", file=sys.stderr)
        for f, v in bad.items():
            for line in v:
                print(f"  {f}: {line}", file=sys.stderr)
        return 1
    print(f"include_check: GREEN ({len(files)} files, every used std symbol has its header in the file)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
