#!/usr/bin/env python3
"""embedded_page_policy_check -- compile and run the behavioural check of the GUI's
page-only rule (B446, ADR-194 D-S5).

WIRED: ./verify fast

  python3 tools/embedded_page_policy_check.py

WHY A DRIVER. `./verify fast` builds nothing, and CI's fast job has no build tree
and no submodules. The rule (src/gui/embedded_page_policy.h) and its check
(tools/embedded_page_policy_check.cpp) need only the standard library, so this
compiles the one file with the host's C++ compiler into a temp directory and runs
it. The rows and the four must-fail controls live in the .cpp; read its header.

FAILS CLOSED: no C++ compiler, a compile error, or a nonzero exit is RED.
"""
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "tools" / "embedded_page_policy_check.cpp"


def compiler():
    for c in (os.environ.get("CXX"), "c++", "clang++", "g++"):
        if c and shutil.which(c):
            return c
    return None


def main():
    cxx = compiler()
    if cxx is None:
        print("embedded_page_policy_check: RED — no C++ compiler (set CXX)", file=sys.stderr)
        return 1
    with tempfile.TemporaryDirectory() as tmp:
        exe = pathlib.Path(tmp) / "embedded_page_policy_check"
        build = subprocess.run([cxx, "-std=c++20", "-O1", "-Wall", "-Wextra", "-Werror",
                                str(SRC), "-o", str(exe)], capture_output=True, text=True)
        if build.returncode != 0:
            print("embedded_page_policy_check: RED — compile failed:\n" + build.stdout + build.stderr,
                  file=sys.stderr)
            return 1
        run = subprocess.run([str(exe)], capture_output=True, text=True)
        sys.stdout.write(run.stdout)
        sys.stderr.write(run.stderr)
        return 0 if run.returncode == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
