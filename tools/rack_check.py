#!/usr/bin/env python3
"""rack_check -- the generic module slot's generated tests, its plants and its real-time gate (ROADMAP B714
acceptance (5), (6), (9); critic findings 7, 8, 9).

WIRED: ./verify full

  python3 tools/rack_check.py

WHY. Before B714 the test plug-in's FX checks ran only from tools/engine-preview/build.sh, so ./verify never gated them
(critic finding 9). The slot library is meant for S8's shell too, so its own tests, its must-fail plants and a
real-time run of its audio path are gated here, in full: they need every hosted module's sibling tree at its pin and a
FOUNDATIONS tree, which CI does not have.

THREE PARTS, each printed, none silently passed:
  A  TESTS     tools/rack/tests: rack_test against every module tools/rack/modules.json names (its test-only one too),
               then each plant build (off-keeps-gains, deaf, no-values, idle-cut, level-cut, drop-slot-state,
               ignore-legacy). The clean run must read GREEN; each plant must read RED with at least one FAIL line. A
               plant that passes is a check that cannot see its fault: red.
  B  REAL TIME rack_rtsan_probe under RealtimeSanitizer, every module's DSP compiled by the sanitizer compiler (the rule
               is tools/rtsan_check.py's, imported, not restated: $HORDE_SANITIZER_CXX, else Homebrew LLVM, with the SDK
               passed as the sysroot). Order: the planted malloc must be reported; an empty scope must read zero; the
               rack whose process() allocates (HORDE_RACK_PLANT_ALLOC) must be reported; then the clean schedule must
               read zero.
  C  PREVIEW   the test plug-in's own smoke and bridge test (tools/engine-preview/build.sh: the rack through a host,
               the whole bank, the page's keys, the migration and the loud refusal). It needs the clean-room engine's
               export, so it runs only when $HORDE_PREVIEW_ENGINE names an engine commit (build.sh's argument, from
               $HORDE_ENGINE2_REPO); otherwise it is SKIPPED with that reason. Minutes of builds.
A part that cannot run (a tree, the compiler or the engine missing) is SKIPPED with its reason, and the last line and
the `KIT-GATE skipped:` receipt line name it: a green run beside a skip is a partial measurement and says so.

Builds go to build-rack/ under the repo root (never into a sibling tree). choc's JSON header comes from
$HORDE_LIBS_DIR/choc, the repo's libs/choc, or the main checkout's (a worktree has no submodules checked out).
"""
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import rtsan_check  # noqa: E402  (the compiler rule, one copy)

BUILD = ROOT / "build-rack"
PLANTS = ["off-keeps-gains", "deaf", "no-values", "idle-cut", "level-cut", "drop-slot-state", "ignore-legacy"]
LEDGERS = [ROOT / "tools/engine-preview/host_ids.lock.json"] + sorted((ROOT / "tools/rack/test").glob("host_ids*.json"))


def run(cmd, log, env=None, cwd=None):
    with open(log, "w", encoding="utf-8") as f:
        return subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=str(cwd or ROOT), env=env).returncode


def choc_dir():
    cands = []
    if os.environ.get("HORDE_LIBS_DIR"):
        cands.append(Path(os.environ["HORDE_LIBS_DIR"]) / "choc")
    cands.append(ROOT / "libs/choc")
    r = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--git-common-dir"], capture_output=True, text=True)
    if r.returncode == 0:
        cands.append((ROOT / r.stdout.strip()).resolve().parent / "libs/choc")
    for c in cands:
        if (c / "choc/text/choc_JSON.h").is_file():
            return c
    return None


def missing_trees():
    out = []
    for m in json.loads((ROOT / "tools/rack/modules.json").read_text(encoding="utf-8"))["modules"]:
        t = m.get("tree")
        if t and not Path(os.environ.get(t["var"]) or os.path.expanduser(t["default"])).is_dir():
            out.append(f"{m['key']} (no tree at {t['var']})")
    f = os.environ.get("HORDE_FOUNDATIONS_DIR") or os.path.expanduser("~/Documents/Claude/synthetic-worlds/FOUNDATIONS")
    if not Path(f).is_dir():
        out.append("FOUNDATIONS (no tree at HORDE_FOUNDATIONS_DIR)")
    return out


def configure(build, choc, extra, log):
    build.mkdir(parents=True, exist_ok=True)
    cmd = ["cmake", "-S", str(ROOT / "tools/rack/tests"), "-B", str(build), "-G", "Unix Makefiles", "-DCMAKE_BUILD_TYPE=Release",
           f"-DHORDE_RACK_CHOC_DIR={choc}", "-DHORDE_RACK_LEDGERS=" + ";".join(str(l) for l in LEDGERS)] + extra
    return run(cmd, log)


def build_targets(build, targets, log):
    return run(["nice", "-n", "10", "cmake", "--build", str(build), "-j4", "--target"] + targets, log)


def part_a(choc):
    b = BUILD / "tests"
    if configure(b, choc, [], BUILD / "tests.configure.log") != 0:
        print(f"A  RED: configure failed (see {BUILD}/tests.configure.log)")
        return False
    if build_targets(b, ["rack_test"] + [f"rack_test_{p}" for p in PLANTS], b / "build.log") != 0:
        print(f"A  RED: build failed (see {b}/build.log)")
        return False
    ok = True
    rc = run([str(b / "rack_test")], b / "rack_test.log")
    text = (b / "rack_test.log").read_text(encoding="utf-8", errors="replace")
    clean = rc == 0 and "rack_test: GREEN" in text
    oks = sum(1 for l in text.splitlines() if l.startswith("ok  "))
    print(f"A  clean: {'GREEN' if clean else 'RED'} ({oks} ok lines; {b}/rack_test.log)")
    for line in text.splitlines():
        if line.startswith("FAIL"):
            print("   " + line)
    ok = ok and clean
    for p in PLANTS:
        log = b / f"rack_test_{p}.log"
        rc = run([str(b / f"rack_test_{p}")], log)
        t = log.read_text(encoding="utf-8", errors="replace")
        fired = rc != 0 and any(l.startswith("FAIL") for l in t.splitlines())
        first = next((l for l in t.splitlines() if l.startswith("FAIL")), "")
        print(f"A  plant {p}: {'FIRED' if fired else 'DID NOT FIRE'}{' -- ' + first if first else ''}")
        ok = ok and fired
    return ok


def part_b(choc):
    cxx, sdk = rtsan_check.sanitizer_cxx(), rtsan_check.sdk_path()
    if not cxx or not sdk:
        return None, "no RealtimeSanitizer compiler (HORDE_SANITIZER_CXX, or brew install llvm)" if not cxx else "xcrun finds no SDK"
    b = BUILD / "rtsan"
    if configure(b, choc, [f"-DCMAKE_CXX_COMPILER={cxx}", f"-DCMAKE_OSX_SYSROOT={sdk}", "-DHORDE_RACK_RTSAN=ON"], BUILD / "rtsan.configure.log") != 0:
        print(f"B  RED: configure failed (see {BUILD}/rtsan.configure.log)")
        return False, None
    if build_targets(b, ["rack_rtsan_probe", "rack_rtsan_probe_alloc"], b / "build.log") != 0:
        print(f"B  RED: build failed (see {b}/build.log)")
        return False, None
    env = dict(os.environ, RTSAN_OPTIONS="halt_on_error=false")

    def probe(exe, mode, argv):
        log = b / f"rtsan.{mode}.log"
        rc = run([str(b / exe)] + argv, log, env=env)
        total, distinct = rtsan_check.violations(str(log))
        return rc, total, distinct, log

    rc, total, _, log = probe("rack_rtsan_probe", "plant", ["--plant"])
    print(f"B  planted malloc: {total} violation(s) (must be >= 1)")
    if total == 0:
        print(f"B  RED: RealtimeSanitizer did not report the planted malloc, so nothing else here is believed ({log})")
        return False, None
    rc, total, _, log = probe("rack_rtsan_probe", "control", ["--control"])
    print(f"B  empty scope: {total} violation(s), exit {rc} (must be 0)")
    if total or rc != 0:
        print(f"B  RED: an empty realtime scope reported ({log})")
        return False, None
    rc, total, distinct, log = probe("rack_rtsan_probe_alloc", "alloc", [])
    print(f"B  the rack whose process() allocates: {total} violation(s) (must be >= 1)")
    if total == 0:
        print(f"B  RED: the planted allocation in the rack's process path was not reported ({log})")
        return False, None
    rc, total, distinct, log = probe("rack_rtsan_probe", "full", [])
    tail = [l.strip() for l in open(log, encoding="utf-8", errors="replace") if l.startswith("rack_rtsan_probe:")]
    for l in tail:
        print("   " + l)
    for (kind, callee, loc), n in sorted(distinct.items()):
        print(f"   [{kind}] {callee} at {loc} ({n} stack(s))")
    green = total == 0 and rc == 0 and bool(tail)
    print(f"B  the rack's audio path: {total} violation(s), exit {rc}: {'GREEN' if green else 'RED'} ({log})")
    return green, None


def part_c():
    engine = os.environ.get("HORDE_PREVIEW_ENGINE")
    if not engine:
        return None, "no engine commit named (set HORDE_PREVIEW_ENGINE to run tools/engine-preview/build.sh <commit>)"
    out = BUILD / "preview"
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, HORDE_PREVIEW_OUT=str(out))
    log = BUILD / "preview.log"
    rc = run(["bash", str(ROOT / "tools/engine-preview/build.sh"), engine], log, env=env)
    text = log.read_text(encoding="utf-8", errors="replace")
    for l in text.splitlines():
        if l.startswith("FAIL") or ": GREEN" in l or ": RED" in l or l.startswith("     migration") or "CPU" in l:
            print("   " + l)
    green = rc == 0 and "engine-preview smoke: GREEN" in text and "bridge_test: GREEN" in text
    print(f"C  the preview's smoke and bridge test on engine {engine}: {'GREEN' if green else 'RED'} ({log})")
    return green, None


def main():
    BUILD.mkdir(exist_ok=True)
    skipped, red = [], False
    choc = choc_dir()
    trees = missing_trees()
    if choc is None or trees:
        why = "; ".join((["choc's JSON header (HORDE_LIBS_DIR)"] if choc is None else []) + trees)
        print(f"rack_check: WARNING -- A and B need {why}")
        skipped += [f"A tests ({why})", f"B real time ({why})"]
    else:
        red = not part_a(choc) or red
        got, why = part_b(choc)
        if got is None:
            print(f"B  SKIPPED: {why}; nothing was measured, and this is NOT a pass")
            skipped.append(f"B real time ({why})")
        else:
            red = red or not got
    got, why = part_c()
    if got is None:
        print(f"C  SKIPPED: {why}; nothing was measured, and this is NOT a pass")
        skipped.append(f"C preview ({why})")
    else:
        red = red or not got
    if skipped:
        print("KIT-GATE skipped: " + "; ".join(skipped))
    print(f"rack_check: {'RED' if red else 'GREEN'}{' (partial: ' + str(len(skipped)) + ' part(s) skipped)' if skipped else ''}")
    return 1 if red else 0


if __name__ == "__main__":
    sys.exit(main())
