#!/usr/bin/env python3
"""nan_latch_check -- compile and run the behavioural check of the output guard's
latch-and-report rule (B448 B1, ADR-197 risk row 3).

WIRED: ./verify fast

  python3 tools/nan_latch_check.py

WHY A DRIVER. `./verify fast` builds nothing, and CI's fast job has no build tree
and no submodules. The latch (src/output_latch.h) needs only the standard library,
so this compiles tools/nan_latch_check.cpp with the host's C++ compiler into a temp
directory and runs it. The rows and the five must-fail controls live in the .cpp.

ALSO CHECKED HERE, as text, because the .cpp cannot see the plugin: process() in
src/hypersaw_clap.cpp reaches the latch (guardOutput -> nonFiniteLatch.guard) and
no longer calls zeroNonFinite bare, which would be the silent zero again. A
planted bare call must read red (the control below). hostile_events_check
(./verify full) proves the shell's own guard counts, behind the exported hooks.

FAILS CLOSED: no C++ compiler, a compile error, a nonzero exit, or a missing call
site is RED.
"""
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "tools" / "nan_latch_check.cpp"
SHELL = ROOT / "src" / "hypersaw_clap.cpp"


def compiler():
    for c in (os.environ.get("CXX"), "c++", "clang++", "g++"):
        if c and shutil.which(c):
            return c
    return None


def wiring_problem(text):
    """None when the shell's output guard goes through the latch, else why not.
    Pure so the control below can feed it a planted bare call."""
    if re.search(r"^[ \t]*(?:hypersaw::)?zeroNonFinite\s*\(", text, re.M):
        return "process() calls zeroNonFinite bare — the silent zero (it must go through the latch)"
    if not re.search(r"nonFiniteLatch\.guard\s*\(", text):
        return "no nonFiniteLatch.guard(...) call in the shell"
    if not re.search(r"^[ \t]*guardOutput\s*\(\s*outL\s*,\s*outR\s*,\s*nframes\s*\)\s*;", text, re.M):
        return "process() does not end its chain with guardOutput(outL, outR, nframes)"
    return None


def wiring_check():
    shell = SHELL.read_text()
    problem = wiring_problem(shell)
    if problem:
        print(f"nan_latch_check: RED — {problem}", file=sys.stderr)
        return False
    # Must-fail control: the same shell text with the B446 bare calls put back.
    planted = shell.replace("guardOutput(outL, outR, nframes);",
                            "hypersaw::zeroNonFinite(outL, nframes);\n    hypersaw::zeroNonFinite(outR, nframes);")
    if wiring_problem(planted) is None:
        print("nan_latch_check: RED — control: a bare zeroNonFinite in process() reads green "
              "(the call-site check is blind)", file=sys.stderr)
        return False
    print("PASS  shell wiring: process() -> guardOutput -> nonFiniteLatch.guard; "
          "control (bare zeroNonFinite planted) reads red")
    return True


def main():
    cxx = compiler()
    if cxx is None:
        print("nan_latch_check: RED — no C++ compiler (set CXX)", file=sys.stderr)
        return 1
    with tempfile.TemporaryDirectory() as tmp:
        exe = pathlib.Path(tmp) / "nan_latch_check"
        # No -ffast-math: it licenses the compiler to assume no NaN and fold the very
        # test under check. -O1 keeps the loop honest without that.
        build = subprocess.run([cxx, "-std=c++20", "-O1", "-Wall", "-Wextra", "-Werror",
                                str(SRC), "-o", str(exe)], capture_output=True, text=True)
        if build.returncode != 0:
            print("nan_latch_check: RED — compile failed:\n" + build.stdout + build.stderr,
                  file=sys.stderr)
            return 1
        run = subprocess.run([str(exe)], capture_output=True, text=True)
        sys.stdout.write(run.stdout)
        sys.stderr.write(run.stderr)
        ok = run.returncode == 0
    return 0 if (wiring_check() and ok) else 1


if __name__ == "__main__":
    sys.exit(main())
