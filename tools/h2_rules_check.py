#!/usr/bin/env python3
"""h2_rules_check -- horde 2's structural rules hold in the tree (ADR-186, ADR-187).

WIRED: ./verify fast

WHY. h2/README.md states rules that prose alone cannot keep: a README is read
by the next agent only if they think to look, and each rule below fails
silently when broken (the build stays green, the audio still plays). ROADMAP
B332's critic review (2026-09-28) asked for the first three to be enforced
rather than described.

WHAT IT CHECKS.
  1. BOUNDARY (ADR-186 item 4). Nothing under h2/ includes a file from src/,
     nothing under src/ includes a file from h2/, and no CMake target that
     compiles an h2 core links the legacy shell (${PROJECT_NAME}-impl /
     HYPERSAW-impl). One link that saw a legacy core and its horde-2 copy is the
     ODR hazard the namespace exists to prevent.
  2. CONTRACTION (PENDING HUMAN RULING). Every CMake target that compiles an h2
     core carries -ffp-contract=off, because clang and GCC otherwise fuse a*b+c
     and a fused build of the SCALPEL core misses parity on 19 of 386 scenarios
     (measured 2026-09-28, docs/port/scalpel-phase-1a.md). The one declared
     exception is FMA_CONTROL, the must-fail control whose whole job is to be
     the contracted build (it must carry -ffp-contract=fast). The human has been
     asked whether the build that ships must be the build that passed parity;
     until that ruling this rule keeps every h2 build on the parity side.
  3. PINNED TARGET (ADR-187 item 3). h2/README.md's status row pins the SCALPEL
     oracle by git blob hash, and that hash equals the file's current content.
     A changed oracle is a changed parity target and must be re-pinned on
     purpose, not discovered.
  "Compiles an h2 core" means: a target whose source file #includes a path
  containing `h2/cores/`.

THE RULES CALIBRATE THEMSELVES ON EVERY RUN (selftest()): synthetic trees
breaking each rule must be rejected, and a conforming one accepted.
"""
import hashlib
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
FMA_CONTROL = "h2_scalpel_fma_control"
ORACLE = "reference/scalpel/prototype/razor-core.js"
LEGACY_LINK = re.compile(r"\$\{PROJECT_NAME\}-impl|HYPERSAW-impl")


def blob_hash(data: bytes) -> str:
    """git's blob id (sha1 of 'blob <len>\\0' + content), with no git needed."""
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def strip_cmake_comments(text):
    return "\n".join(line.split("#", 1)[0] for line in text.splitlines())


def check_cmake(cmake_text, source_includes_h2):
    """-> list of failure strings. `source_includes_h2(path)` says whether a
    source file compiles an h2 core. Pure apart from that callback."""
    text = strip_cmake_comments(cmake_text)
    fails = []
    targets = {}
    for m in re.finditer(r"add_executable\(\s*([A-Za-z0-9_]+)\s+([^)]*)\)", text):
        targets[m.group(1)] = m.group(2).split()
    opts, links = {}, {}
    for m in re.finditer(r"target_compile_options\(\s*([A-Za-z0-9_]+)\s+([^)]*)\)", text):
        opts.setdefault(m.group(1), []).extend(m.group(2).split())
    for m in re.finditer(r"target_link_libraries\(\s*([A-Za-z0-9_]+)\s+([^)]*)\)", text):
        links.setdefault(m.group(1), []).append(m.group(2))
    h2 = [t for t, srcs in targets.items() if any(source_includes_h2(s) for s in srcs)]
    for t in h2:
        if any(LEGACY_LINK.search(l) for l in links.get(t, [])):
            fails.append(f"{t}: compiles an h2 core AND links the legacy shell (ADR-186 item 4)")
        flags = opts.get(t, [])
        if t == FMA_CONTROL:
            if "-ffp-contract=fast" not in flags:
                fails.append(f"{t}: the FMA must-fail control must carry -ffp-contract=fast")
        elif "-ffp-contract=off" not in flags or "-ffp-contract=fast" in flags or "-ffp-contract=on" in flags:
            fails.append(f"{t}: compiles an h2 core without -ffp-contract=off (rule 2, pending human ruling)")
    return fails, h2


def check_includes(files):
    """files: {relpath: text}. -> failures for rule 1's include half."""
    fails = []
    inc = re.compile(r'^\s*#\s*include\s*["<]([^">]+)[">]', re.M)
    for rel, text in files.items():
        for path in inc.findall(text):
            if rel.startswith("h2/") and re.search(r"(^|/)src/", path):
                fails.append(f"{rel}: includes {path} (h2 never includes src/, ADR-186 item 4)")
            if rel.startswith("src/") and re.search(r"(^|/)h2/", path):
                fails.append(f"{rel}: includes {path} (legacy never includes h2/, ADR-186 item 4)")
    return fails


def check_pin(readme_text, oracle_bytes):
    want = blob_hash(oracle_bytes)
    m = re.search(r"razor-core\.js@([0-9a-f]{40})", readme_text)
    if not m:
        return [f"h2/README.md: no razor-core.js@<blob> pin in the status row (current blob {want})"]
    if m.group(1) != want:
        return [f"h2/README.md pins razor-core.js@{m.group(1)} but the file is now {want}: re-pin on purpose (ADR-187 item 3)"]
    return []


def selftest():
    inc = lambda s: s == "tools/x.cpp"
    good = ("add_executable(h2x tools/x.cpp)\ntarget_compile_options(h2x PRIVATE -O2 -ffp-contract=off)\n"
            f"add_executable({FMA_CONTROL} tools/x.cpp)\ntarget_compile_options({FMA_CONTROL} PRIVATE -ffp-contract=fast)\n")
    cases = [
        (good, 0),
        ("add_executable(h2x tools/x.cpp)\ntarget_compile_options(h2x PRIVATE -O3)\n", 1),                           # no contract flag
        ("add_executable(h2x tools/x.cpp)\ntarget_compile_options(h2x PRIVATE -ffp-contract=off -ffp-contract=fast)\n", 1),
        ("add_executable(h2x tools/x.cpp)\ntarget_compile_options(h2x PRIVATE -ffp-contract=off)\n"
         "target_link_libraries(h2x PRIVATE ${PROJECT_NAME}-impl)\n", 1),                                            # links legacy
        (f"add_executable({FMA_CONTROL} tools/x.cpp)\ntarget_compile_options({FMA_CONTROL} PRIVATE -O2)\n", 1),     # control not contracted
        ("add_executable(h2x tools/x.cpp) # target_compile_options(h2x PRIVATE -ffp-contract=off)\n", 1),           # a comment is not a flag
        ("add_executable(legacy tools/y.cpp)\ntarget_link_libraries(legacy PRIVATE HYPERSAW-impl)\n", 0),           # not an h2 target
    ]
    for text, nfail in cases:
        got = len(check_cmake(text, inc)[0])
        if got != nfail:
            return f"selftest: cmake case expected {nfail} failure(s), got {got}:\n{text}"
    if len(check_includes({"h2/cores/a.h": '#include "../../src/swarm_core.h"'})) != 1:
        return "selftest: an h2 -> src include was not caught"
    if len(check_includes({"src/b.h": '#include "../h2/cores/scalpel/razor_core.h"'})) != 1:
        return "selftest: a src -> h2 include was not caught"
    if check_includes({"h2/cores/a.h": "#include <cmath>"}):
        return "selftest: a std include was flagged"
    if not check_pin("razor-core.js@" + "0" * 40, b"x") or check_pin("razor-core.js@" + blob_hash(b"x"), b"x"):
        return "selftest: the blob pin rule misjudged a synthetic README"
    return None


def main():
    err = selftest()
    if err:
        print("h2_rules_check: FAILED (the rule itself is broken) — " + err, file=sys.stderr)
        return 1

    def source_includes_h2(src):
        p = ROOT / src
        return p.suffix in (".cpp", ".h") and p.is_file() and "h2/cores/" in p.read_text(errors="ignore")

    fails, h2 = check_cmake((ROOT / "CMakeLists.txt").read_text(), source_includes_h2)
    files = {str(p.relative_to(ROOT)): p.read_text(errors="ignore")
             for g in ("h2/**/*.h", "h2/**/*.hpp", "h2/**/*.cpp", "src/**/*.h", "src/**/*.cpp", "src/**/*.mm")
             for p in sorted(ROOT.glob(g))}
    fails += check_includes(files)
    fails += check_pin((ROOT / "h2/README.md").read_text(), (ROOT / ORACLE).read_bytes())
    if fails:
        print(f"h2_rules_check: FAILED — {len(fails)} rule violation(s):", file=sys.stderr)
        for f in fails:
            print("    " + f, file=sys.stderr)
        return 1
    print(f"h2_rules_check: GREEN ({len(h2)} h2 targets: {', '.join(sorted(h2))}; boundary, contraction (pending ruling) and the oracle pin hold)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
