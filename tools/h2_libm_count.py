#!/usr/bin/env python3
"""h2_libm_count -- libm call sites in the composed engine's emitted assembly.

UNWIRED: context, never a verdict (B441 phase 2). A count may change on purpose,
so nothing here can be red; it is read in a PR's diff of the committed file.

WHY. h2_engine_selfdigest_check proves an output-neutral change kept every bit.
It cannot show a change that kept the bits by luck on THIS libm: clang rewrites
`pow(2, x)` into `exp2(x)` and merges a `sin` and a `cos` of one argument into
one `sincos` (the B441 phase-1(B) audit), and a refactor can move a call across
that line silently. The bits may still agree here and not on another libm. So an
output-neutral PR (C1-C3, ...) re-runs this with --write and commits the count
file: any change in which libm functions are called, and how often, shows in
its diff for a reviewer.

WHAT IS COUNTED. One translation unit includes h2/engine/engine.h and
tools/h2_engine_stream.h and exposes the parity tools' replay() with every
argument a runtime value (so no fault branch folds away), compiled to assembly
(-S) twice:
  product  the release flags of measure_h2_engine and auhost (CMakeLists.txt):
           -O3 -DNDEBUG -ffp-contract=off, no fault hooks;
  parity   h2_engine_parity_check's: -O3 -DNDEBUG -O2 -ffp-contract=off (the
           target's -O2 after the Release -O3, as CMake orders them), with
           H2_ENGINE_FAULTS, as that binary is built.
Every call or tail-call instruction (bl/b on arm64, call/jmp on x86-64) whose
target is a libm function in LIBM below is a call SITE (static, not a runtime
count). Inlined calls are counted where they are inlined.

The count file is keyed on platform and compiler (the codegen); it sits next to
the self-digest: h2/engine/libm-calls.<platform>.<compiler>.txt.

Usage: python3 tools/h2_libm_count.py [--write]
  prints the table and, where a committed file exists, how it differs (exit 0
  either way); --write rewrites the file. CXX names the compiler (default c++).
"""
import os
import platform
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# The libm surface the engine could reach, double and float spellings, plus the
# platforms' internal names for the merged forms (darwin: __sincos_stret).
LIBM = ["sin", "cos", "tan", "sincos", "__sincos_stret", "asin", "acos", "atan", "atan2",
        "sinh", "cosh", "tanh", "exp", "exp2", "expm1", "pow", "log", "log2", "log10",
        "log1p", "sqrt", "cbrt", "hypot", "fmod", "remainder", "ldexp", "frexp", "scalbn", "modf", "nan"]
LIBM = LIBM + [f + "f" for f in LIBM if not f.startswith("__")]

TU = """
#ifdef H2_LIBM_PARITY
#define H2_ENGINE_FAULTS 1
#endif
#include "h2/engine/engine.h"
#include "tools/h2_engine_stream.h"
void h2_libm_probe(const h2engine_stream::Scenario& sc, std::vector<double>& out, int fault,
                   horde2::engine::EventLog* log, double eps, h2engine_stream::ReplayInfo* info, int block) {
  h2engine_stream::replay(sc, out, fault, log, eps, info, block);
}
"""
FLAGS = {
    "product": ["-O3", "-DNDEBUG", "-ffp-contract=off"],
    "parity": ["-O3", "-DNDEBUG", "-O2", "-ffp-contract=off", "-DH2_LIBM_PARITY"],
}
# POSIX-portable call grammar: an optional leading underscore (Mach-O mangling), an
# optional @PLT (ELF). Anchored on the mnemonic so a symbol NAME containing "sin"
# (a mangled C++ function) never counts.
CALL = re.compile(r"^\s*(?:bl|b|call|callq|jmp|jmpq)\s+_?(" + "|".join(sorted(map(re.escape, LIBM), key=len, reverse=True)) + r")(?:@PLT)?\s*$")


def key():
    sysname = {"Darwin": "darwin", "Linux": "linux", "Windows": "windows"}.get(platform.system(), "other")
    arch = {"arm64": "arm64", "aarch64": "arm64", "x86_64": "x86_64", "AMD64": "x86_64"}.get(platform.machine(), platform.machine())
    v = subprocess.run([os.environ.get("CXX", "c++"), "--version"], capture_output=True, text=True).stdout
    m = re.search(r"Apple clang version (\d+)", v) or re.search(r"clang version (\d+)", v)
    if m:
        cc = ("appleclang-" if "Apple" in m.group(0) else "clang-") + m.group(1)
    else:
        g = re.search(r"\(GCC\) (\d+)|g\+\+.* (\d+)\.", v)
        cc = "gcc-" + (g.group(1) or g.group(2)) if g else "unknown"
    return f"{sysname}-{arch}.{cc}"


def count(flags):
    cmd = [os.environ.get("CXX", "c++"), "-std=gnu++20", *flags, "-I", str(ROOT), "-x", "c++", "-", "-S", "-o", "-"]
    r = subprocess.run(cmd, input=TU, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"h2_libm_count: compile failed ({' '.join(flags)}):\n{r.stderr}")
    n = {}
    for line in r.stdout.splitlines():
        m = CALL.match(line)
        if m:
            n[m.group(1)] = n.get(m.group(1), 0) + 1
    return n


def table(k):
    c = {name: count(f) for name, f in FLAGS.items()}
    fns = sorted(set(c["product"]) | set(c["parity"]))
    lines = [f"# h2_libm_count (tools/h2_libm_count.py): libm call SITES in h2/engine's emitted assembly, key {k}",
             "# Context, never a verdict: an output-neutral PR re-runs it with --write and the diff shows any change.",
             f"# product: {' '.join(FLAGS['product'])}",
             f"# parity:  {' '.join(FLAGS['parity'])} (H2_ENGINE_FAULTS)",
             f"{'fn':<16}{'product':>8}{'parity':>8}"]
    lines += [f"{f:<16}{c['product'].get(f, 0):>8}{c['parity'].get(f, 0):>8}" for f in fns]
    lines.append(f"{'TOTAL':<16}{sum(c['product'].values()):>8}{sum(c['parity'].values()):>8}")
    return "\n".join(lines) + "\n"


def main():
    k = key()
    path = ROOT / "h2" / "engine" / f"libm-calls.{k}.txt"
    t = table(k)
    print(t, end="")
    if "--write" in sys.argv[1:]:
        path.write_text(t)
        print(f"h2_libm_count: wrote {path.relative_to(ROOT)}")
    elif path.exists():
        old = path.read_text()
        print(f"h2_libm_count: {'matches' if old == t else 'DIFFERS FROM'} {path.relative_to(ROOT)} (context, not judged)")
        if old != t:
            for a, b in zip(old.splitlines(), t.splitlines()):
                if a != b:
                    print(f"  committed: {a}\n  now:       {b}")
    else:
        print(f"h2_libm_count: no committed counts for {k} (context, not judged)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
