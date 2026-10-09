#!/usr/bin/env python3
"""rtsan_check — tools/rtsan_probe under RealtimeSanitizer (ADR-197 risk row 1).

UNWIRED: 1 realtime violation on main; wired when it is fixed (B448)

Builds the probe of the real plugin's audio-thread entry points with
-fsanitize=realtime, runs it, and exits non-zero on ANY violation in the full
run.

  PLANT    one malloc inside the realtime scope, no plugin. MUST be reported, or
           the check is red before it runs anything else: a sanitizer that
           cannot start reads exactly like a clean plugin.
  CONTROL  the same scope, empty. Must be clean.
  FULL     process(), params.flush(), start/stop_processing() and reset() inside
           the scope, over a seeded schedule that covers notes and voice
           stealing, every enumerated parameter id, morph (blend and quantum),
           the intent bus, the mod matrix, MIDI and note expressions, transport,
           state loads and editor writes staged on the main thread then drained
           on the audio thread, bypass and reactivation (see the probe header).

    tools/rtsan_check.py [build-dir] [--seed N] [--blocks N]

THE COMPILER. RealtimeSanitizer needs a real LLVM; the Command Line Tools' Apple
clang has none (ADR-199). The compiler is $HORDE_SANITIZER_CXX, else
`$(brew --prefix llvm)/bin/clang++`. When neither exists the check prints a
WARNING and SKIPS (exit 0, "SKIPPED" in the output, no verdict): verify full
still runs on a machine without it, but nothing was measured and nothing says
GREEN. `-isysroot` is passed explicitly because Homebrew's clang otherwise looks
for an SDK newer than the one installed.

HOW IT BUILDS. The probe is ONE translation unit that includes the shell source
(see its header), compiled with the impl target's own defines, includes and
language flags read from CMake's generated flags.make — copied by parse, never
restated, so a new include directory cannot drift out of this build. The product
build stays on Apple clang; only this probe uses the sanitizer compiler, so no
parity reference or self-digest moves. HYPERSAW-impl is built first because it
generates the headers the shell includes and, on macOS, the GUI object
gui_create links against (linked uninstrumented; no editor opens).

Reports are written whole to <build>/rtsan.<mode>.log. stdout carries counts and
the distinct violations, deduplicated by (intercepted call, innermost src/
frame): RealtimeSanitizer itself already collapses identical full stacks.
RTSAN_OPTIONS here: halt_on_error=false so one run lists every distinct
violation rather than the first.

NEVER baselined, suppressed or allowlisted: a violation is fixed in src/, or the
check stays unwired and says how many there are.

NO WARM-UP, AND WHAT THAT FOUND. The first process() runs cold on purpose: a
function-local static that initialises on the first call is a real offence (the
first block is the one a live set hears). It found one on main, 2026-10-09:
swarm_core.h anchorTables() builds 2 x 5 x 16385 doubles behind a
__cxa_guard mutex on the first rendered voice, under a comment that says
"never on the audio thread". RealtimeSanitizer reports it as four stacks
(guard acquire and release, each a lock and an unlock) at one source line.

CALIBRATION beyond --plant (which proves the scope, not the plugin path): a
malloc planted in a COPY of src/ at the top of Plugin::process was reported at
that line. The first plant used a std::vector that nothing read back, and the
compiler elided the allocation — read-back-free allocations plant nothing, so
use a volatile sink.
"""
import os
import re
import shlex
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Classification of the intercepted libc call. RealtimeSanitizer says WHICH
# function it caught, not why that is bad; the four kinds are the brief's.
ALLOC = {"malloc", "calloc", "realloc", "reallocf", "valloc", "free", "posix_memalign",
         "aligned_alloc", "malloc_zone_malloc", "malloc_zone_calloc", "malloc_zone_realloc",
         "malloc_zone_free", "malloc_zone_memalign", "malloc_zone_valloc", "mmap", "munmap"}
BLOCKING = {"sleep", "usleep", "nanosleep", "thrd_sleep", "select", "pselect", "poll", "ppoll",
            "sched_yield", "pthread_join", "pthread_cond_wait", "pthread_cond_timedwait",
            "sem_wait", "sem_timedwait", "dispatch_semaphore_wait", "kevent", "wait", "waitpid"}


def kind_of(callee):
    if callee in ALLOC:
        return "allocation"
    if callee in BLOCKING:
        return "blocking call"
    if "mutex" in callee or "lock" in callee or "rwlock" in callee or "pthread_once" in callee \
            or "dispatch_once" in callee or "spin" in callee:
        return "lock"
    return "syscall/io"


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


def without_sysroot(argv):
    """CMake pins the Apple SDK with -isysroot; the sanitizer compiler gets its own below."""
    out, skip = [], False
    for a in argv:
        if skip:
            skip = False
        elif a == "-isysroot":
            skip = True
        else:
            out.append(a)
    return out


def sanitizer_cxx():
    env = os.environ.get("HORDE_SANITIZER_CXX")
    if env and os.path.exists(env):
        return env
    brew = shutil.which("brew")
    if brew:
        r = subprocess.run([brew, "--prefix", "llvm"], capture_output=True, text=True)
        cand = os.path.join(r.stdout.strip(), "bin", "clang++")
        if r.returncode == 0 and os.path.exists(cand):
            return cand
    return None


def sdk_path():
    r = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else None


def run(cmd, log):
    with open(log, "w", encoding="utf-8") as f:
        return subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT).returncode


SRC_BASENAMES = set()
for _d, _, _fs in os.walk(os.path.join(ROOT, "src")):
    SRC_BASENAMES.update(_fs)
SRC_BASENAMES.add("rtsan_probe.cpp")

# "    #3 0x1 in hypersaw::Foo::bar(int) hypersaw_clap.cpp:123:4 (bin:arm64+0x1)"
# or, symbolized by atos: "    #3 0x1 in bar hypersaw_clap.cpp:123". Basename:line is all that is portable.
FRAME = re.compile(r"^\s+#(\d+) 0x[0-9a-f]+ in (.*?) (\S+?):(\d+)(?::\d+)?\s*(?:\(.*\))?$")
CALLEE = re.compile(r"function `(.+?)`")


def violations(log):
    """Distinct violations: {(kind, callee, innermost src 'file:line' + function): count of stacks}."""
    text = open(log, encoding="utf-8", errors="replace").read()
    blocks = text.split("ERROR: RealtimeSanitizer: ")[1:]
    seen = {}
    for b in blocks:
        head = b.split("\n", 1)[0].strip()
        m = CALLEE.search(b)
        callee = m.group(1) if m else head
        loc = "(no src/ frame)"
        for line in b.split("SUMMARY:")[0].splitlines():
            fm = FRAME.match(line)
            if fm and os.path.basename(fm.group(3)) in SRC_BASENAMES:
                loc = f"{os.path.basename(fm.group(3))}:{fm.group(4)} in {fm.group(2)}"
                break
        key = (kind_of(callee), callee, loc)
        seen[key] = seen.get(key, 0) + 1
    return len(blocks), seen


def main():
    args = sys.argv[1:]
    build = os.path.join(ROOT, "build-rtsan")
    if args and not args[0].startswith("--"):
        build = os.path.abspath(args.pop(0))
    harness_args = args or ["--seed", "1", "--blocks", "1400"]

    cxx = sanitizer_cxx()
    sdk = sdk_path()
    if not cxx or not sdk:
        print("rtsan_check: WARNING — no sanitizer compiler (set HORDE_SANITIZER_CXX, or `brew install llvm`)"
              if not cxx else "rtsan_check: WARNING — xcrun cannot find an SDK")
        print("rtsan_check: SKIPPED — nothing was measured; this is NOT a pass (ADR-199)")
        return 0

    # Every log goes INSIDE the build dir: `build-*/` is ignored, a sibling
    # `build-x.log` is not, and its absolute paths trip the leak gate.
    os.makedirs(build, exist_ok=True)
    if run(["cmake", "-S", ROOT, "-B", build, "-G", "Unix Makefiles", "-DCMAKE_BUILD_TYPE=RelWithDebInfo"],
           os.path.join(build, "rtsan.configure.log")) != 0:
        print(f"rtsan_check: configure FAILED (see {build}/rtsan.configure.log)")
        return 1
    if run(["cmake", "--build", build, "--target", "HYPERSAW-impl", f"-j{os.cpu_count() or 4}"],
           os.path.join(build, "rtsan.impl.log")) != 0:
        print("rtsan_check: HYPERSAW-impl build FAILED (generated headers come from it)")
        return 1

    fm = flags_make(build)
    exe = os.path.join(build, "rtsan_probe")
    cmd = [cxx, "-isysroot", sdk] + without_sysroot(fm.get("CXX_FLAGS", [])) + fm.get("CXX_DEFINES", []) \
        + fm.get("CXX_INCLUDES", []) \
        + ["-O1", "-g", "-fno-omit-frame-pointer", "-fsanitize=realtime",
           os.path.join(ROOT, "tools", "rtsan_probe.cpp"), "-o", exe]
    gui_obj = os.path.join(build, "CMakeFiles", "HYPERSAW-impl.dir", "src", "gui", "hypersaw_gui.mm.o")
    if sys.platform == "darwin":
        cmd += [gui_obj, "-framework", "WebKit", "-framework", "Cocoa"]
    if run(cmd, os.path.join(build, "rtsan.build.log")) != 0:
        print(f"rtsan_check: probe build FAILED (see {build}/rtsan.build.log)")
        return 1

    env = dict(os.environ, RTSAN_OPTIONS="halt_on_error=false")

    def probe(mode, argv):
        log = os.path.join(build, f"rtsan.{mode}.log")
        with open(log, "w", encoding="utf-8") as f:
            rc = subprocess.run([exe] + argv, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT, env=env).returncode
        return log, rc

    # THE PLANT FIRST. A clean plugin run means nothing unless this same binary,
    # under these same options, reports an allocation it was handed.
    log, rc = probe("plant", ["--plant"])
    total, _ = violations(log)
    print(f"== plant: exit {rc}, {total} violation(s) (must be >= 1)")
    if total == 0:
        print("== rtsan_check: RED — the planted malloc was NOT reported, so RealtimeSanitizer is not "
              f"working in this build/runtime and no other verdict is meaningful (see {log})")
        return 1

    log, rc = probe("control", ["--control"])
    total, _ = violations(log)
    print(f"== control: exit {rc}, {total} violation(s) (must be 0)")
    if total or rc != 0:
        print(f"== rtsan_check: RED — an EMPTY realtime scope reported, so the full run's verdict cannot be "
              f"trusted (see {log})")
        return 1

    log, rc = probe("full", ["--root", ROOT] + harness_args)
    total, distinct = violations(log)
    tail = [l.strip() for l in open(log, encoding="utf-8", errors="replace") if l.startswith("rtsan_probe:")]
    print(f"== full: exit {rc}, {total} violation stack(s), {len(distinct)} distinct")
    for l in tail:
        print("   " + l)
    by_kind = {}
    for (kind, callee, loc), n in sorted(distinct.items()):
        by_kind[kind] = by_kind.get(kind, 0) + 1
        print(f"   [{kind}] {callee}  at {loc}  ({n} stack(s))")
    if by_kind:
        print("   by kind: " + ", ".join(f"{k} {n}" for k, n in sorted(by_kind.items())))
    # A non-zero exit with no report (crash, usage error, empty corpus) is red
    # too: a run that did not complete has measured nothing.
    red = bool(total) or rc != 0 or not tail
    print(f"== rtsan_check: {'RED' if red else 'GREEN'} (logs: {build}/rtsan.<mode>.log)")
    return 1 if red else 0


if __name__ == "__main__":
    sys.exit(main())
