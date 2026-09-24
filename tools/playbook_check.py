#!/usr/bin/env python3
"""playbook_check — the machine-checkable half of the source-integration playbook.

WIRED: ./verify fast

docs/playbooks/integrating-a-source.md (B233) tells a fresh agent how to wire a new
oscillator, engine or feature through every seam of the shell. Its whole value is
that it is ACCURATE, and a document full of file:line citations is accurate for
exactly one commit unless something re-reads it. Three rules, each cheap, each
something no other gate in ./verify looks at:

1. EVERY CITATION RESOLVES. A citation is a code span of the exact form

       `<path>:<line> <anchor>`

   where <anchor> is text that must appear on that line. The file must exist and
   the anchor must exist SOMEWHERE in it — a missing file or a vanished anchor is
   a FAILURE, because the playbook then describes code that is not there. A
   citation with no anchor is also a failure: a bare line number cannot be
   re-checked, so it is exactly the claim this rule exists to refuse.
   An anchor found on a DIFFERENT line is DRIFT, and drift is ADVISORY: it is
   printed with the line the anchor is on now and counted on the GREEN line.
   Failing on drift would turn every edit above a cited line into a red build for
   an unrelated PR; printing it keeps the rot visible without taxing everyone
   else (the "gaps are counted, never hidden" idiom of test_table_check).

2. THE MORPH LAYOUT MARKER IS ONE NUMBER. `morphLayout` names the ORDER of the
   positional corner arrays (ADR-159), and it is written by four writers — three
   in src/hypersaw_clap.cpp (cornerJson, liveCornerJson, morphJson) and one in
   tools/gen_factory_bank.cpp. Bumping some and not all is a silent split: files
   stamped with two numbers for one order. The writer COUNT is asserted too
   (L0033): a refactor that renames a writer out of the regex's sight would
   otherwise leave "all agree" true of three, then two, then none.

3. NO CLOCK AND NO UNSEEDED RNG IN THE ENGINE SOURCES. The charter's domain
   invariant ("mulberry32 streams only; no wall-clock reads anywhere in the core",
   SPEC §5.7) had no gate: rtsafety_probe counts allocations, not clock reads.
   A token scan over src/*.h and src/*.cpp (the cores and the shells — NOT
   src/gui/, which is the editor's thread), with C/C++ comments stripped so prose
   ABOUT the rule cannot trip it.

CALIBRATED ON EVERY RUN (L0032): selftest() feeds each rule synthetic inputs that
must fail and ones that must pass, and a rule that stops firing fails the run.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PLAYBOOK = ROOT / "docs/playbooks/integrating-a-source.md"
MARKER_WRITERS = [ROOT / "src/hypersaw_clap.cpp", ROOT / "tools/gen_factory_bank.cpp"]
MARKER_WRITER_COUNT = 4   # 3 in the shell + 1 in the bank generator; see rule 2

# A citation: `path:line anchor`. The path alphabet is deliberately narrow — repo
# paths only, no spaces — so ordinary code spans (`id == 23`) never match.
CITE_RE = re.compile(
    r"`((?:src|tools|docs|tests|specs|reference)/[A-Za-z0-9_./-]+|verify)"
    r":(\d+)(?:[ \t]+([^`]+))?`")

# `\"morphLayout\":N` as it appears inside a C++ string literal. The parser's
# `json.find("\"morphLayout\"")` has no digits after the colon and is not a writer.
MARKER_RE = re.compile(r'\\"morphLayout\\":(\d+)')

# Wall clocks and non-mulberry32 randomness. Each token is specific enough that it
# cannot occur in ordinary code by accident (`operand(` is not `rand(`).
CLOCK_RNG_RE = re.compile(
    r"std::chrono|#\s*include\s*<(?:chrono|random|ctime|time\.h)>|random_device|mt19937"
    r"|minstd_rand|default_random_engine|\bs?rand\s*\(|\bstd::time\s*\(|\btime\s*\(\s*(?:nullptr|NULL|0)\s*\)"
    r"|\bclock\s*\(\s*\)|gettimeofday|clock_gettime|mach_absolute_time|QueryPerformanceCounter"
    r"|steady_clock|system_clock|high_resolution_clock")


def check_citations(doc, read):
    """-> (failures, drifts, n_at_line, n_total). `read(path)` returns the file's
    lines or None. Pure over its inputs so selftest() can drive it."""
    fails, drifts, at_line, total = [], [], 0, 0
    for m in CITE_RE.finditer(doc):
        path, line, anchor = m.group(1), int(m.group(2)), (m.group(3) or "").strip()
        total += 1
        tag = f"`{path}:{line}{' ' + anchor if anchor else ''}`"
        if not anchor:
            fails.append(f"{tag}: citation has no anchor text, so it cannot be re-checked")
            continue
        lines = read(path)
        if lines is None:
            fails.append(f"{tag}: {path} does not exist")
            continue
        if 1 <= line <= len(lines) and anchor in lines[line - 1]:
            at_line += 1
            continue
        where = [i + 1 for i, l in enumerate(lines) if anchor in l]
        if not where:
            fails.append(f"{tag}: anchor text appears nowhere in {path} — the playbook "
                         f"describes code that is not there")
        else:
            drifts.append(f"{tag} now at line {', '.join(map(str, where[:4]))}"
                          + (" …" if len(where) > 4 else ""))
    return fails, drifts, at_line, total


def check_marker(texts):
    """texts: [(name, source text)] -> (failures, values)."""
    vals = []
    for name, text in texts:
        vals += [(name, v) for v in MARKER_RE.findall(text)]
    fails = []
    if len(vals) != MARKER_WRITER_COUNT:
        fails.append(f"found {len(vals)} morphLayout writers, expected {MARKER_WRITER_COUNT} "
                     f"({', '.join(f'{n}={v}' for n, v in vals) or 'none'}) — a writer was added "
                     f"or renamed out of sight; update MARKER_WRITER_COUNT with the reason")
    distinct = sorted({v for _, v in vals})
    if len(distinct) > 1:
        fails.append("morphLayout writers disagree: "
                     + ", ".join(f"{n}={v}" for n, v in vals)
                     + " — a bump must reach every writer (ADR-159; see cornerJson's comment)")
    return fails, distinct


def strip_comments(code):
    code = re.sub(r"/\*.*?\*/", " ", code, flags=re.S)
    return re.sub(r"//[^\n]*", " ", code)


def check_determinism(files):
    """files: [(name, text)] -> failures."""
    fails = []
    for name, text in files:
        for n, line in enumerate(strip_comments(text).split("\n"), 1):
            m = CLOCK_RNG_RE.search(line)
            if m:
                fails.append(f"{name}:{n}: `{m.group(0)}` — no wall clock and no RNG but a seeded "
                             f"mulberry32 stream (forcecore::rngNext) in the engine sources")
    return fails


def selftest():
    files = {"a.cpp": ["int x;", "void kEngineBlocks() {}", "y"]}
    read = lambda p: files.get(p)
    cases = [
        ("anchored at its line", "`src/a.cpp:2 kEngineBlocks`", False, 0),
        ("drifted anchor is advisory", "`src/a.cpp:1 kEngineBlocks`", False, 1),
        ("vanished anchor fails", "`src/a.cpp:2 kGone`", True, 0),
        ("unanchored citation fails", "`src/a.cpp:2`", True, 0),
        ("missing file fails", "`src/b.cpp:2 x`", True, 0),
    ]
    remap = lambda p: files.get(p.replace("src/", ""))
    for name, doc, must_fail, want_drift in cases:
        f, d, _, total = check_citations(doc, remap)
        if total != 1:
            return f"selftest (citations): '{name}' parsed {total} citations, not 1"
        if bool(f) != must_fail or len(d) != want_drift:
            return f"selftest (citations): '{name}' gave failures={f} drifts={d}"
    if check_citations("`id == 23` and `kParams`", read)[3] != 0:
        return "selftest (citations): an ordinary code span was read as a citation"
    w = lambda *vs: [("w", f'"{{\\"morphLayout\\":{v},')for v in vs]
    if check_marker(w(9, 9, 9, 9))[0]:
        return "selftest (marker): four agreeing writers were rejected"
    if not check_marker(w(9, 9, 8, 9))[0]:
        return "selftest (marker): a writer left behind by a bump was accepted"
    if not check_marker(w(9, 9, 9))[0]:
        return "selftest (marker): a vanished writer was accepted"
    must = ["auto t = std::chrono::steady_clock::now();", "srand(1);", "int r = rand();",
            "#include <random>", "std::mt19937 g;", "time(nullptr);"]
    for code in must:
        if not check_determinism([("s.cpp", code)]):
            return f"selftest (determinism): `{code}` was not caught"
    clean = ["// no std::chrono here", "/* rand() is banned */ int x;", "int y = operand(3);",
             "double t = lifetime(2);", "hypersaw::forcecore::rngNext(state);"]
    for code in clean:
        if check_determinism([("s.cpp", code)]):
            return f"selftest (determinism): `{code}` was flagged and must not be"
    return None


def main():
    fail = []
    broken = selftest()
    if broken:
        fail.append(broken)

    if not PLAYBOOK.exists():
        fail.append(f"{PLAYBOOK.relative_to(ROOT)} is missing")
        drifts, at_line, total = [], 0, 0
    else:
        def read(p):
            f = ROOT / p
            return f.read_text(encoding="utf-8").split("\n") if f.is_file() else None
        cf, drifts, at_line, total = check_citations(PLAYBOOK.read_text(encoding="utf-8"), read)
        fail += cf
        if total == 0:
            fail.append("the playbook parsed to ZERO citations — the citation grammar changed "
                        "or the regex stopped matching; either way nothing was checked")

    mf, marker = check_marker([(p.relative_to(ROOT).as_posix(), p.read_text(encoding="utf-8"))
                               for p in MARKER_WRITERS])
    fail += mf

    sources = sorted(ROOT.glob("src/*.h")) + sorted(ROOT.glob("src/*.cpp"))
    fail += check_determinism([(p.relative_to(ROOT).as_posix(), p.read_text(encoding="utf-8"))
                               for p in sources])

    if fail:
        print("playbook_check: FAILED", file=sys.stderr)
        for f in fail[:25]:
            print(f"  {f}", file=sys.stderr)
        if len(fail) > 25:
            print(f"  ... and {len(fail) - 25} more", file=sys.stderr)
        return 1
    for d in drifts:
        print(f"  note  drift: {d}")
    print(f"playbook_check: GREEN ({total} citations — {at_line} at their cited line, "
          f"{len(drifts)} drifted (advisory); morphLayout {marker[0] if marker else '?'} at "
          f"{MARKER_WRITER_COUNT} writers; {len(sources)} engine sources free of clocks and "
          f"unseeded RNG)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
