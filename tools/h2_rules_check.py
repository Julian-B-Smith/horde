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
  2. CONTRACTION (RULED by the human 2026-09-30, ROADMAP B332: the shipped
     build is arithmetically the tested one). Every CMake target that compiles
     an h2 core carries -ffp-contract=off, because clang and GCC otherwise fuse
     a*b+c and a fused build of the SCALPEL core misses parity on 19 of 386
     scenarios (measured 2026-09-28, docs/port/scalpel-phase-1a.md). The declared
     exceptions are FMA_CONTROLS, the must-fail controls whose whole job is to be
     the contracted build (each must carry -ffp-contract=fast): the blade port's
     and, since B385, the composed engine's.
  3. PINNED TARGET (ADR-187 item 3). h2/README.md's status row pins the SCALPEL
     oracle by git blob hash, and that hash equals the file's current content.
     A changed oracle is a changed parity target and must be re-pinned on
     purpose, not discovered.
  4. NO TU SEES BOTH COPIES (B379 critic L5, N2). No file in tools/ (.cpp and
     .h), h2/ or src/ #includes both a legacy core (src/*_core.h) and an h2 core
     header, directly or through another scanned file. The
     lifted swarm core keeps the legacy names nested (horde2::swarm::hypersaw),
     so such a TU compiles and binds correctly today; the rule keeps it from
     becoming the habit that one day meets an un-namespaced name (the shared
     HZ_CULL_ENV macro already is one, h2/README.md).
  "Compiles an h2 core" means: a target whose source file #includes a path
  containing `h2/cores/` or `h2/engine/` (horde 2's one engine, B385, is h2 code
  under every rule here).

THE RULES CALIBRATE THEMSELVES ON EVERY RUN (selftest()): synthetic trees
breaking each rule must be rejected, and a conforming one accepted.
"""
import hashlib
import pathlib
import posixpath
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
FMA_CONTROLS = ("h2_scalpel_fma_control", "h2_engine_fma_control")
H2_PATH = re.compile(r"(^|/)h2/(cores|engine)/")
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
        if t in FMA_CONTROLS:
            if "-ffp-contract=fast" not in flags:
                fails.append(f"{t}: the FMA must-fail control must carry -ffp-contract=fast")
        elif "-ffp-contract=off" not in flags or "-ffp-contract=fast" in flags or "-ffp-contract=on" in flags:
            fails.append(f"{t}: compiles an h2 core without -ffp-contract=off (rule 2, the shipped build is the tested build)")
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


def check_dual_includes(files):
    """files: {relpath: text}. -> failures for rule 4: a file that includes, directly
    or THROUGH another scanned file (critic N2: a tools/ header that includes a
    legacy core, included by a .cpp that also includes an h2 core), both a legacy
    core and an h2 core header. Inside src/ a bare "x_core.h" is a legacy core.
    Quoted includes resolve against the including file's directory; only files in
    the scanned set are followed."""
    inc = re.compile(r'^\s*#\s*include\s*["<]([^">]+)[">]', re.M)
    direct, edges = {}, {}
    for rel, text in files.items():
        paths = inc.findall(text)
        legacy = [p for p in paths if re.search(r"(^|/)src/[^/]*_core\.h$", p)
                  or (rel.startswith("src/") and re.fullmatch(r"[^/]*_core\.h", p))]
        h2 = [p for p in paths if H2_PATH.search(p)]
        direct[rel] = (legacy[0] if legacy else None, h2[0] if h2 else None)
        base = posixpath.dirname(rel)
        edges[rel] = [q for q in (posixpath.normpath(posixpath.join(base, p)) for p in paths) if q in files and q != rel]
    reach = {rel: dict(zip(("legacy", "h2"), direct[rel])) for rel in files}
    changed = True
    while changed:   # propagate what each file reaches through the files it includes
        changed = False
        for rel in files:
            for q in edges[rel]:
                for k in ("legacy", "h2"):
                    if reach[rel][k] is None and reach[q][k] is not None:
                        reach[rel][k] = f"{reach[q][k]} via {q}"
                        changed = True
    return [f"{rel}: includes a legacy core ({r['legacy']}) AND an h2 core ({r['h2']}) in one TU (rule 4)"
            for rel, r in reach.items() if r["legacy"] and r["h2"]]


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
    FMA_CONTROL = FMA_CONTROLS[0]
    good = ("add_executable(h2x tools/x.cpp)\ntarget_compile_options(h2x PRIVATE -O2 -ffp-contract=off)\n"
            f"add_executable({FMA_CONTROL} tools/x.cpp)\ntarget_compile_options({FMA_CONTROL} PRIVATE -ffp-contract=fast)\n"
            f"add_executable({FMA_CONTROLS[1]} tools/x.cpp)\ntarget_compile_options({FMA_CONTROLS[1]} PRIVATE -ffp-contract=fast)\n")
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
    if len(check_dual_includes({"tools/t.cpp": '#include "../src/swarm_core.h"\n#include "../h2/cores/swarm/swarm_core.h"\n'})) != 1:
        return "selftest: a tool including both copies was not caught"
    if len(check_dual_includes({"tools/t.cpp": '#include "../src/swarm_core.h"\n#include "../h2/engine/engine.h"\n'})) != 1:
        return "selftest: a tool including a legacy core and the h2 engine was not caught"
    if len(check_includes({"h2/engine/e.h": '#include "../../src/swarm_core.h"'})) != 1:
        return "selftest: an h2/engine -> src include was not caught"
    if check_dual_includes({"tools/t.cpp": '#include "../h2/cores/swarm/swarm_core.h"\n// src/swarm_core.h:1053 cited in a comment\n'}):
        return "selftest: a comment citing a legacy core was flagged as an include"
    via = {"tools/leg.h": '#include "../src/swarm_core.h"\n',
           "tools/t.cpp": '#include "leg.h"\n#include "../h2/cores/swarm/swarm_core.h"\n'}
    got = check_dual_includes(via)
    if len(got) != 1 or not got[0].startswith("tools/t.cpp"):
        return f"selftest: a legacy core reached through a tools/ header was not caught on the .cpp: {got}"
    if check_dual_includes({"tools/leg.h": via["tools/leg.h"], "tools/u.cpp": '#include "leg.h"\n'}):
        return "selftest: a TU reaching only a legacy core was flagged"
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
        return p.suffix in (".cpp", ".h") and p.is_file() and H2_PATH.search(p.read_text(errors="ignore")) is not None

    fails, h2 = check_cmake((ROOT / "CMakeLists.txt").read_text(), source_includes_h2)
    files = {str(p.relative_to(ROOT)): p.read_text(errors="ignore")
             for g in ("h2/**/*.h", "h2/**/*.hpp", "h2/**/*.cpp", "src/**/*.h", "src/**/*.cpp", "src/**/*.mm")
             for p in sorted(ROOT.glob(g))}
    fails += check_includes(files)
    tus = dict(files)
    tus.update({str(p.relative_to(ROOT)): p.read_text(errors="ignore")
                for g in ("tools/**/*.cpp", "tools/**/*.h") for p in sorted(ROOT.glob(g))})
    fails += check_dual_includes(tus)
    fails += check_pin((ROOT / "h2/README.md").read_text(), (ROOT / ORACLE).read_bytes())
    if fails:
        print(f"h2_rules_check: FAILED — {len(fails)} rule violation(s):", file=sys.stderr)
        for f in fails:
            print("    " + f, file=sys.stderr)
        return 1
    print(f"h2_rules_check: GREEN ({len(h2)} h2 targets: {', '.join(sorted(h2))}; boundary, contraction, no TU with both copies, and the oracle pin hold)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
