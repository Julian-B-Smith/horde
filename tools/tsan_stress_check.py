#!/usr/bin/env python3
"""tsan_stress_check — tools/tsan_stress under ThreadSanitizer (ADR-197 risk row 2).

Builds the multi-thread stress harness for parameters, presets and state with
-fsanitize=thread, runs it, and exits non-zero on ANY ThreadSanitizer report
in the control or full run.

  PLANT    a deliberate race on a plain int, no plugin. MUST be reported, or the
           check is red before it runs anything else: a sanitizer that cannot
           start reads exactly like a clean plugin.
  CONTROL  audio thread only (host events + params_flush). Nothing else touches
           the instance, so a report here is either an audio-thread-only race
           or a harness defect. Either way the full run's verdict cannot be
           trusted until it is explained, so the control is gated, not advisory.
  FULL     audio thread + the host/editor main thread (param writes, state
           load/save, preset apply, morph, history, editor polling).

    tools/tsan_stress_check.py [build-dir] [--seed N] [--blocks N] [--ops N]

COMPILER: $HORDE_SANITIZER_CXX, else Homebrew llvm's clang++ (ADR-199), always
with -isysroot from xcrun on macOS. With neither, it prints a WARNING and exits
77 (SKIP) — never 0, because a run that measured nothing is not a pass.

HOW IT BUILDS, and why not through HYPERSAW_SANITIZE: that option instruments
host-side executables only, never the impl library, so a probe linked against
HYPERSAW-impl would never see the shell's accesses (see tools/tsan_stress.cpp's
header). The harness is ONE translation unit that includes the shell source,
compiled here with the impl target's own defines, includes and language flags
read from CMake's generated flags.make — copied by parse, never restated, so a
new include directory cannot drift out of this build. HYPERSAW-impl is built
first because it generates the headers the shell includes and, on macOS, the
GUI object gui_create links against (linked uninstrumented; no editor opens).

Reports are written whole to <build>/tsan_stress.<mode>.log; stdout carries
only counts, deduplicated by the top frame of each report's two stacks.
TSAN_OPTIONS here: halt_on_error=0 so one run lists every distinct report
rather than the first (tools/sanitize_oracles.sh halts on the first, which
suits a pass/fail matrix and hides the second race behind the first here).

UNWIRED: red on main by design until the main-thread/audio-thread handoff is fixed (B446 Tier C); wired with the fix
"""
import os
import re
import shlex
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def flags_make(build):
    """CXX_DEFINES / CXX_INCLUDES / CXX_FLAGS of the impl target, as argv lists."""
    path = os.path.join(build, "CMakeFiles", "HYPERSAW-impl.dir", "flags.make")
    out = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"^(CXX_DEFINES|CXX_INCLUDES|CXX_FLAGS) = (.*)$", line.rstrip("\n"))
            if m:
                out[m.group(1)] = shlex.split(m.group(2))
    return out


SKIP = 77   # the automake "skipped" status: not a pass, not a failure


def sanitizer_cxx():
    """The TSan-capable compiler: $HORDE_SANITIZER_CXX, else `brew --prefix llvm`/bin/clang++.

    NOT the CMake compiler. Apple's Command Line Tools 16 TSan runtime dies in
    its own initialisation on macOS 26 (ADR-199), so the compiler the plugin
    builds with is the wrong one to sanitize with. Returns None if neither
    exists; the caller SKIPs, never passes."""
    env = os.environ.get("HORDE_SANITIZER_CXX")
    if env:
        return env if os.access(env, os.X_OK) else None
    try:
        prefix = subprocess.run(["brew", "--prefix", "llvm"], capture_output=True, text=True,
                                check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None
    cxx = os.path.join(prefix, "bin", "clang++")
    return cxx if os.access(cxx, os.X_OK) else None


def sysroot_flags():
    """-isysroot is REQUIRED with Homebrew clang on macOS: without it the
    compiler looks for an SDK matching the OS version, which the Command Line
    Tools may not ship, and <pthread.h> / <cstring> fail to resolve."""
    if sys.platform != "darwin":
        return []
    sdk = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True).stdout.strip()
    return ["-isysroot", sdk] if sdk else []


def run(cmd, log):
    with open(log, "w", encoding="utf-8") as f:
        return subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT).returncode


# "    #0 Plugin::process(clap_process const*) hypersaw_clap.cpp:123:4 (bin:arm64+0x1)"
FRAME = re.compile(r"^\s+#0 (.*?)(?: \(\S+\+0x[0-9a-f]+\))?$")


def reports(log):
    """Distinct reports: (kind, first stack's #0, second stack's #0)."""
    text = open(log, encoding="utf-8", errors="replace").read()
    blocks = text.split("WARNING: ThreadSanitizer: ")[1:]
    seen = {}
    for b in blocks:
        kind = b.split(" (pid", 1)[0].split("\n", 1)[0].strip()
        tops = [FRAME.match(l).group(1) for l in b.splitlines() if FRAME.match(l)][:2]
        key = (kind,) + tuple(tops)
        seen[key] = seen.get(key, 0) + 1
    return len(blocks), seen


def main():
    args = sys.argv[1:]
    build = os.path.join(ROOT, "build-tsan-stress")
    if args and not args[0].startswith("--"):
        build = os.path.abspath(args.pop(0))
    harness_args = args or ["--seed", "1", "--blocks", "3000", "--ops", "400"]

    cxx = sanitizer_cxx()
    if not cxx:
        print("WARNING: tsan_stress_check: no TSan-capable compiler (set HORDE_SANITIZER_CXX, or "
              "install Homebrew llvm) — SKIPPED, nothing was measured")
        return SKIP

    # Every log goes INSIDE the build dir: `build-*/` is ignored, a sibling
    # `build-x.log` is not, and its absolute paths trip the leak gate.
    os.makedirs(build, exist_ok=True)
    cfg_log = os.path.join(build, "tsan_stress.configure.log")
    if run(["cmake", "-S", ROOT, "-B", build, "-G", "Unix Makefiles",
            "-DCMAKE_BUILD_TYPE=RelWithDebInfo"], cfg_log) != 0:
        print(f"tsan_stress_check: configure FAILED (see {cfg_log})")
        return 1
    if run(["cmake", "--build", build, "--target", "HYPERSAW-impl", f"-j{os.cpu_count() or 4}"],
           os.path.join(build, "tsan_stress.impl.log")) != 0:
        print("tsan_stress_check: HYPERSAW-impl build FAILED (generated headers come from it)")
        return 1

    fm = flags_make(build)
    exe = os.path.join(build, "tsan_stress")
    cmd = [cxx] + sysroot_flags() + fm.get("CXX_FLAGS", []) + fm.get("CXX_DEFINES", []) \
        + fm.get("CXX_INCLUDES", []) \
        + ["-O1", "-g", "-fno-omit-frame-pointer", "-fsanitize=thread",
           os.path.join(ROOT, "tools", "tsan_stress.cpp"), "-o", exe]
    if sys.platform == "darwin":
        gui_obj = os.path.join(build, "CMakeFiles", "HYPERSAW-impl.dir", "src", "gui", "hypersaw_gui.mm.o")
        cmd += [gui_obj, "-framework", "WebKit", "-framework", "Cocoa"]
    if run(cmd, os.path.join(build, "tsan_stress.build.log")) != 0:
        print(f"tsan_stress_check: harness build FAILED (see {build}/tsan_stress.build.log)")
        return 1

    env = dict(os.environ, TSAN_OPTIONS="halt_on_error=0:second_deadlock_stack=1")

    def harness(mode, argv):
        log = os.path.join(build, f"tsan_stress.{mode}.log")
        with open(log, "w", encoding="utf-8") as f:
            rc = subprocess.run([exe] + argv, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT,
                                env=env).returncode
        return log, rc

    # THE PLANT FIRST. A clean plugin run means nothing unless this same binary,
    # under these same options, reports a race it was handed. Found necessary on
    # the first run (macOS 26 + Command Line Tools 16): the TSan runtime
    # segfaulted at startup on a ten-line program, and every mode "had no
    # reports" for that reason alone.
    log, rc = harness("plant", ["--plant"])
    total, _ = reports(log)
    print(f"== plant: exit {rc}, {total} report(s) (must be >= 1)")
    if total == 0:
        print("== tsan_stress_check: RED — the planted race was NOT reported, so ThreadSanitizer is "
              f"not working in this build/runtime and no other verdict is meaningful (see {log})")
        return 1

    red = False
    for mode, extra in (("control", ["--control"]), ("full", [])):
        log, rc = harness(mode, ["--root", ROOT] + harness_args + extra)
        total, distinct = reports(log)
        tail = [l for l in open(log, encoding="utf-8", errors="replace") if l.startswith("tsan_stress:")]
        print(f"== {mode}: exit {rc}, {total} report(s), {len(distinct)} distinct  "
              f"{tail[-1].strip() if tail else '(harness printed no summary)'}")
        # A non-zero exit with no report (crash, usage error, empty corpus) is
        # red too: a run that did not complete has measured nothing.
        if total or rc != 0 or not tail:
            red = True
    print(f"== tsan_stress_check: {'RED' if red else 'GREEN'} (logs: {build}/tsan_stress.<mode>.log)")
    return 1 if red else 0


if __name__ == "__main__":
    sys.exit(main())
