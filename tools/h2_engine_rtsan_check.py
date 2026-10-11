#!/usr/bin/env python3
"""h2_engine_rtsan_check — horde 2's composed engine under RealtimeSanitizer.

WIRED: ./verify full

ROADMAP B448 phase 2, package P16 (docs/strategy/blind-spot-armor-phase2.md §3);
risk row R1, the engine-level slice. tools/rtsan_check.py does the same for the
legacy shell's process(); this one is its sibling for h2/engine/, which has no
shell yet (B398).

Builds tools/h2_engine_rtsan_probe.cpp with -fsanitize=realtime and runs it three
times over the engine parity gate's scenario stream (tools/h2_engine_render.mjs):

  PLANT    the first scenario, with ONE malloc inside the realtime scope after its
           first render call. It MUST be reported, as a malloc, at the probe's own
           line, or the check is red before anything else is believed: a
           sanitizer that cannot start reads exactly like a clean engine.
  CONTROL  the first scenario with every engine call made outside the scope and
           the scope entered around nothing. It must read zero.
  FULL     every scenario, with every call a host block makes into the engine
           (render, and the event calls: set, setString, snap, noteOn, noteOff,
           retune, panic, setVoiceCap, setCapPolicy) inside the scope. ANY
           violation is red. So is a run that judged fewer scenarios than the
           stream's header names, or fewer than MIN_SCENARIOS.

    tools/h2_engine_rtsan_check.py [--full-from STREAM] [build-dir]

--full-from is the renderer's full output, which ./verify full already holds in a
temp file for the parity gate; without it the check renders the stream itself
(about 35 s). Run from anywhere; paths are resolved from the repo root.

WHAT "JUDGED" MEANS, and why the count is pinned. The probe counts a scenario only
when its hooked replay rendered the same samples, blade events and load readouts as
h2engine_stream::replay (the parity gate's replay) in the same binary. The count
must equal the stream's header and END, and be at least MIN_SCENARIOS (the parity
gate pins the same number as kMinScenarios, ADR-206 item 1): a header and END
rewritten to agree with a shortened stream must not pass. verdict() decides, and
selftest() feeds it fabricated runs on every run, one of them a scenario short.

THE COMPILER is tools/rtsan_check.py's rule, imported, not restated (ADR-199):
$HORDE_SANITIZER_CXX, else `$(brew --prefix llvm)/bin/clang++`, with -isysroot
passed explicitly. When there is none the check prints a WARNING and SKIPS (exit
0, "SKIPPED" in the output): nothing was measured and nothing says GREEN.

THE FLAGS: -std=gnu++20 -O1 -g -fno-omit-frame-pointer -ffp-contract=off. Not a
CMake target, so tools/h2_rules_check.py's rule 2 does not see this build; the
contraction flag is passed here by hand so the probed arithmetic is the parity
build's. It is NOT the product binary (Apple clang, -O3): RealtimeSanitizer
intercepts library calls, so it judges what the source asks libc for, and an
allocation one optimiser removes and another keeps is tools/h2_rt_lint_check.py's
to catch in the text.

NEVER baselined, suppressed or allowlisted: a violation is fixed in h2/engine/
(the JS and the C++ together, ADR-187 A2 item 3), or the check stays red.

CALIBRATION beyond the plant, which proves the scope and not the engine path
(measured 2026-10-10, by hand, in a build-directory copy of h2/engine/): a
malloc and free planted at the top of Engine::render in the copy were reported as
two stacks at engine.h's planted line over all 543 scenarios, while the CONTROL
mode of the same binary read zero.

Reports are written whole to <build>/h2rtsan.<mode>.log. stdout carries counts and
the distinct violations, deduplicated by (intercepted call, innermost frame in
h2/engine/ or the probe).

Measured cost on the dev Mac, 2026-10-10: build 6 s, plant and control under 1 s
each, the full run 44 s.
"""
import os
import re
import subprocess
import sys

# Run as `python3 tools/h2_engine_rtsan_check.py`, so tools/ is already first on the import
# path. ADR-199's compiler rule lives in rtsan_check, one copy.
from rtsan_check import kind_of, sanitizer_cxx, sdk_path

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROBE = os.path.join("tools", "h2_engine_rtsan_probe.cpp")
# The parity gate's floor (tools/h2_engine_parity_check.cpp kMinScenarios). A
# scenario set that GROWS passes; one that shrinks is red until a human moves this.
MIN_SCENARIOS = 543

OWN = {"h2_engine_rtsan_probe.cpp", "h2_engine_hooked_replay.h", "h2_engine_stream.h"}
OWN.update(f for f in os.listdir(os.path.join(ROOT, "h2", "engine")) if f.endswith(".h"))

# "    #3 0x1 in horde2::engine::Engine::render(double*, double*, int) engine.h:246:5 (bin:arm64+0x1)"
FRAME = re.compile(r"^\s+#(\d+) 0x[0-9a-f]+ in (.*?) (\S+?):(\d+)(?::\d+)?\s*(?:\(.*\))?$")
CALLEE = re.compile(r"function `(.+?)`")


def violations(text):
    """-> (stacks, {(kind, callee, innermost own 'file:line'): stacks})."""
    blocks = text.split("ERROR: RealtimeSanitizer: ")[1:]
    seen = {}
    for b in blocks:
        m = CALLEE.search(b)
        callee = m.group(1) if m else b.split("\n", 1)[0].strip()
        loc = "(no engine or probe frame)"
        for line in b.split("SUMMARY:")[0].splitlines():
            fm = FRAME.match(line)
            if fm and os.path.basename(fm.group(3)) in OWN:
                loc = f"{os.path.basename(fm.group(3))}:{fm.group(4)}"
                break
        key = (kind_of(callee), callee, loc)
        seen[key] = seen.get(key, 0) + 1
    return len(blocks), seen


def counts(text):
    """-> (header, read, judged) from the probe's own lines; None where absent."""
    h = re.search(r"^h2_engine_rtsan_probe: stream header (-?\d+), END (\S+), (\d+) scenarios read$", text, re.M)
    j = re.search(r"^h2_engine_rtsan_probe: judged (\d+)$", text, re.M)
    header = int(h.group(1)) if h and h.group(2) == h.group(1) else None
    return header, int(h.group(3)) if h else None, int(j.group(1)) if j else None


def verdict(rc, stacks, header, read, judged):
    """The full run's reasons to be red, as strings. Empty means green."""
    red = []
    if rc != 0:
        red.append(f"the probe exited {rc} (a run that did not finish has measured nothing)")
    if stacks:
        red.append(f"{stacks} violation stack(s) inside the realtime scope")
    if header is None or read is None or judged is None:
        red.append("the probe's count lines are missing or the stream's header and END disagree")
    else:
        if judged != header or read != header:
            red.append(f"judged {judged} of the {header} scenarios the stream names ({read} read)")
        if judged < MIN_SCENARIOS:
            red.append(f"judged {judged} scenarios, fewer than the pinned {MIN_SCENARIOS}")
    return red


def selftest():
    """verdict() and the parsers on fabricated runs (L0032): each red must fire, the clean one must not."""
    n = MIN_SCENARIOS
    clean = (f"h2_engine_rtsan_probe: stream header {n}, END {n}, {n} scenarios read\n"
             f"h2_engine_rtsan_probe: judged {n}\n")
    report = ("==1==ERROR: RealtimeSanitizer: unsafe-library-call\n"
              "Intercepted call to real-time unsafe function `malloc` in real-time context!\n"
              "    #0 0x1 in malloc+0x20 (libclang_rt.rtsan_osx_dynamic.dylib:arm64+0x3520)\n"
              "    #1 0x2 in horde2::engine::Engine::render(double*, double*, int) engine.h:246:5 (probe:arm64+0x1)\n"
              "SUMMARY: RealtimeSanitizer: unsafe-library-call engine.h:246 in render\n")
    stacks, seen = violations(report + clean)
    cases = [
        ("a clean full run", verdict(0, 0, *counts(clean)), False),
        ("one scenario short, header and END rewritten to agree",
         verdict(0, 0, *counts(clean.replace(str(n), str(n - 1)))), True),
        ("one scenario not judged", verdict(0, 0, n, n, n - 1), True),
        ("a stream whose END disagrees with its header",
         verdict(0, 0, *counts(clean.replace(f"END {n}", f"END {n - 1}"))), True),
        ("no count lines at all", verdict(0, 0, *counts("")), True),
        ("a probe that crashed", verdict(-11, 0, *counts(clean)), True),
        ("one violation", verdict(0, stacks, *counts(clean)), True),
    ]
    bad = [name for name, red, want in cases if bool(red) != want]
    if stacks != 1 or list(seen) != [("allocation", "malloc", "engine.h:246")]:
        bad.append(f"the report parser read {stacks} stack(s) as {sorted(seen)}")
    return bad


def render_stream(build):
    path = os.path.join(build, "h2rtsan.stream")
    with open(path, "wb") as out, open(os.path.join(build, "h2rtsan.render.log"), "w", encoding="utf-8") as err:
        rc = subprocess.run(["node", os.path.join("tools", "h2_engine_render.mjs")], stdout=out, stderr=err, cwd=ROOT).returncode
    return path if rc == 0 else None


def main():
    args = sys.argv[1:]
    stream = None
    if "--full-from" in args:
        i = args.index("--full-from")
        if i + 1 >= len(args):
            print("usage: h2_engine_rtsan_check.py [--full-from STREAM] [build-dir]")
            return 2
        stream = os.path.abspath(args[i + 1])
        del args[i:i + 2]
    build = os.path.abspath(args[0]) if args else os.path.join(ROOT, "build-h2rtsan")

    bad = selftest()
    if bad:
        print("h2_engine_rtsan_check: RED — the verdict's own self-test failed: " + "; ".join(bad))
        return 1
    print(f"== selftest: the verdict is red on a short, cut, crashed or dirty run and green on a clean one (floor {MIN_SCENARIOS})")

    cxx = sanitizer_cxx()
    sdk = sdk_path()
    if not cxx or not sdk:
        print("h2_engine_rtsan_check: WARNING — no sanitizer compiler (set HORDE_SANITIZER_CXX, or `brew install llvm`)"
              if not cxx else "h2_engine_rtsan_check: WARNING — xcrun cannot find an SDK")
        print("h2_engine_rtsan_check: SKIPPED — Homebrew LLVM is absent, so nothing was measured; this is NOT a pass (ADR-199)")
        return 0

    # Logs and the binary go INSIDE the build dir: `build-*/` is ignored, and a log
    # beside it would carry absolute paths into the leak gate.
    os.makedirs(build, exist_ok=True)
    shown = os.path.relpath(build, ROOT)   # printed; never the machine's absolute path
    exe = os.path.join(build, "h2_engine_rtsan_probe")
    cmd = [cxx, "-isysroot", sdk, "-std=gnu++20", "-O1", "-g", "-fno-omit-frame-pointer", "-ffp-contract=off",
           "-fsanitize=realtime", PROBE, "-o", exe]
    with open(os.path.join(build, "h2rtsan.build.log"), "w", encoding="utf-8") as f:
        if subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT).returncode != 0:
            print(f"h2_engine_rtsan_check: probe build FAILED (see {shown}/h2rtsan.build.log)")
            return 1

    if stream is None:
        stream = render_stream(build)
        if stream is None:
            print(f"h2_engine_rtsan_check: tools/h2_engine_render.mjs FAILED (see {shown}/h2rtsan.render.log)")
            return 1

    # halt_on_error=false: one run lists every distinct violation, not the first.
    env = dict(os.environ, RTSAN_OPTIONS="halt_on_error=false")

    def probe(mode, argv):
        log = os.path.join(build, f"h2rtsan.{mode}.log")
        with open(log, "w", encoding="utf-8") as f:
            rc = subprocess.run([exe] + argv + ["--full-from", stream], stdout=f, stderr=subprocess.STDOUT, cwd=ROOT, env=env).returncode
        return os.path.relpath(log, ROOT), rc, open(log, encoding="utf-8", errors="replace").read()

    # THE PLANT FIRST. A clean engine means nothing unless this binary, under these
    # options, reports the allocation it was handed, as an allocation, at the probe.
    log, rc, text = probe("plant", ["--plant"])
    stacks, seen = violations(text)
    hit = [k for k in seen if k[0] == "allocation" and k[1] == "malloc" and k[2].startswith("h2_engine_rtsan_probe.cpp:")]
    print(f"== plant: exit {rc}, {stacks} violation stack(s), the planted malloc "
          + (f"reported at {hit[0][2]}" if hit else "NOT reported") + " (must be reported)")
    if rc != 0 or not hit:
        if rc < 0 and not text.strip():
            print(f"== the probe died on signal {-rc} with no output. A sanitizer binary can die at start-up inside a "
                  "shell sandbox; run this check unsandboxed before reading it as a verdict.")
        print("== h2_engine_rtsan_check: RED — the planted malloc was NOT reported, so RealtimeSanitizer is not working "
              f"in this build or runtime and no other verdict means anything (see {log})")
        return 1

    log, rc, text = probe("control", ["--control"])
    stacks, _ = violations(text)
    print(f"== control: exit {rc}, {stacks} violation stack(s) (must be 0)")
    if stacks or rc != 0:
        print(f"== h2_engine_rtsan_check: RED — an EMPTY realtime scope reported, so the full run cannot be trusted (see {log})")
        return 1

    log, rc, text = probe("full", [])
    stacks, seen = violations(text)
    header, read, judged = counts(text)
    for line in text.splitlines():
        if line.startswith("h2_engine_rtsan_probe:"):
            print("   " + line)
    print(f"== full: exit {rc}, {stacks} violation stack(s), {len(seen)} distinct; "
          f"judged {judged} of {header} scenarios (floor {MIN_SCENARIOS})")
    by_kind = {}
    for (kind, callee, loc), n in sorted(seen.items()):
        by_kind[kind] = by_kind.get(kind, 0) + 1
        print(f"   [{kind}] {callee}  at {loc}  ({n} stack(s))")
    if by_kind:
        print("   by kind: " + ", ".join(f"{k} {n}" for k, n in sorted(by_kind.items())))
    red = verdict(rc, stacks, header, read, judged)
    for r in red:
        print("   RED: " + r)
    print(f"== h2_engine_rtsan_check: {'RED' if red else 'GREEN'} (logs: {shown}/h2rtsan.<mode>.log)")
    return 1 if red else 0


if __name__ == "__main__":
    sys.exit(main())
