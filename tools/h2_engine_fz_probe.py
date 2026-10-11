#!/usr/bin/env python3
"""h2_engine_fz_probe -- builds and runs the flush-to-zero probe on both self-digest builds.

This is a measurement for the human's ruling (ADR-209 item 6), not a gate: nothing
in ./verify runs it. ROADMAP B448 phase 2, package P16. What is measured, what is
set and the order of proof are in tools/h2_engine_fz_probe.cpp's header.

It answers one question: with flush-to-zero on, how many of the composed engine's
543 self-digest rows change, in the parity build and in the product build?

    tools/h2_engine_fz_probe.py [--full-from STREAM] [--rows] [build-dir]

  build-dir     a CONFIGURED CMake build directory, default build-release. The two
                self-digest targets' compiler and flags are read from its generated
                flags.make files (h2_engine_selfdigest_check for the parity build,
                h2_engine_selfdigest_product for the product build), copied by
                parse and never restated, so the probe is compiled exactly as the
                binaries that produced the pinned references. Configure first:
                  cmake -S . -B build-release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
  --full-from   tools/h2_engine_render.mjs's full output, if the caller holds one;
                otherwise it is rendered here (about 35 s).
  --rows        name every changed row, with how far its samples moved.

IT WRITES NO REFERENCE. The probe has no re-pin path, and this script writes only
inside the build directory. Whatever the count is, h2/engine/selfdigest.*.txt stay
as they are until the human rules (ADR-209 item 6: "Nothing is re-pinned before
that").

Each build's probe proves, in order, that the bit is set, that the switch and the
digest see a difference on a signal that decays through the subnormal range and
none on one that does not, and that with the bit off it reproduces every pinned
row. Only then does it count. A build whose proof fails reports no number, and
this script exits non-zero.

Measured cost on the dev Mac, 2026-10-10: about 75 to 95 s per build (four replays
of the stream each), plus 35 s when it renders the stream itself.
"""
import os
import re
import shlex
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join("tools", "h2_engine_fz_probe.cpp")
# (the probe's build name, the CMake target whose flags it borrows)
BUILDS = [("parity", "h2_engine_selfdigest_check"), ("product", "h2_engine_selfdigest_product")]


def target_flags(build, target):
    """-> (compiler, argv) from the target's generated flags.make, or None."""
    path = os.path.join(build, "CMakeFiles", target + ".dir", "flags.make")
    if not os.path.exists(path):
        return None
    cxx, parts = None, {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"^# compile CXX with (.+)$", line.rstrip("\n"))
            if m:
                cxx = m.group(1)
            m = re.match(r"^(CXX_DEFINES|CXX_INCLUDES|CXX_FLAGS) = (.*)$", line.rstrip("\n"))
            if m:
                parts[m.group(1)] = shlex.split(m.group(2))
    if not cxx or "CXX_FLAGS" not in parts:
        return None
    return cxx, parts["CXX_FLAGS"] + parts.get("CXX_DEFINES", []) + parts.get("CXX_INCLUDES", [])


def main():
    args = sys.argv[1:]
    stream, rows = None, False
    if "--rows" in args:
        rows = True
        args.remove("--rows")
    if "--full-from" in args:
        i = args.index("--full-from")
        if i + 1 >= len(args):
            print("usage: tools/h2_engine_fz_probe.py [--full-from STREAM] [--rows] [build-dir]")
            return 2
        stream = os.path.abspath(args[i + 1])
        del args[i:i + 2]
    build = os.path.abspath(args[0]) if args else os.path.join(ROOT, "build-release")
    shown = os.path.relpath(build, ROOT)

    plans = []
    for name, target in BUILDS:
        tf = target_flags(build, target)
        if tf is None:
            print(f"h2_engine_fz_probe: {shown} has no flags for {target}; configure it first (see this file's header). NOTHING MEASURED")
            return 2
        plans.append((name, target) + tf)

    if stream is None:
        stream = os.path.join(build, "h2fz.stream")
        with open(stream, "wb") as out, open(os.path.join(build, "h2fz.render.log"), "w", encoding="utf-8") as err:
            if subprocess.run(["node", os.path.join("tools", "h2_engine_render.mjs")], stdout=out, stderr=err, cwd=ROOT).returncode != 0:
                print(f"h2_engine_fz_probe: tools/h2_engine_render.mjs FAILED (see {shown}/h2fz.render.log). NOTHING MEASURED")
                return 2

    worst = 0
    summary = []
    for name, target, cxx, flags in plans:
        exe = os.path.join(build, "h2_engine_fz_probe_" + name)
        cmd = [cxx] + flags + [SOURCE, "-o", exe]
        print(f"== {name} build: {target}'s flags: {' '.join(flags)}")
        with open(os.path.join(build, f"h2fz.{name}.build.log"), "w", encoding="utf-8") as f:
            if subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT).returncode != 0:
                print(f"h2_engine_fz_probe: the {name} probe did not build (see {shown}/h2fz.{name}.build.log). NOTHING MEASURED for it")
                worst = max(worst, 2)
                continue
        r = subprocess.run([exe, "--full-from", stream] + (["--rows"] if rows else []), capture_output=True, text=True, cwd=ROOT)
        for line in r.stdout.splitlines():
            print("   " + line)
        if r.stderr.strip():
            print("   stderr: " + r.stderr.strip())
        if r.returncode != 0:
            print(f"== {name} build: the probe exited {r.returncode}: NO NUMBER for this build")
            worst = max(worst, r.returncode if r.returncode > 0 else 2)
            continue
        summary += [line for line in r.stdout.splitlines() if line.startswith(("PASS  BASELINE", "COUNT"))]

    print("== summary (measured; a build missing here reported no number):")
    for line in summary:
        print("   " + line)
    return worst


if __name__ == "__main__":
    sys.exit(main())
