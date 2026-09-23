#!/usr/bin/env python3
"""test_table_check — the feature test table cannot claim coverage it lacks.

A table of tests is a claim about what is verified, and a claim about coverage is
exactly the kind that rots invisibly: a row naming an oracle nobody wrote reads as
green forever, and a feature with no row at all reads as nothing to test. Both
failures are silent by construction, which is why they are gated rather than
reviewed.

WHAT IS CHECKED:

1. EVERY NAMED ORACLE EXISTS. An agentic row's `oracle` must be a gate `./verify`
   actually invokes — parsed from `verify`, never a hardcoded list, so a renamed
   gate fails here the day it is renamed rather than the day someone notices the
   row was fiction. `none` and `manual` are legal and mean what they say.

2. EVERY (page, feature) IN THE GUI HAS AT LEAST ONE ROW. The feature axis is
   taken from `src/param_presentation.tsv` — the same table the GUI is generated
   from — so a feature cannot appear on screen with no test row. `*` rows are
   cross-cutting and satisfy nothing specific on purpose.

3. THE CLASSIFICATION IS PRESENT AND LEGAL. `pins` is RULING or ENCODING, and
   nothing else. FOUNDATIONS' R8 is why: they asserted their own encoding as
   though it were the rule and failed a conforming consumer against it. A row
   that cannot say which it is has not been thought about.

4. A RULING NAMES ITS OWNER. `owner` must be non-empty for a RULING — an ADR, a
   FOUNDATIONS ruling, `spec`, or `human`. A decision nobody owns cannot be
   revisited, only argued about.

5. EVERY CHECK DECLARES ITS WIRING, AND THE DECLARATION IS TRUE (B190 layer 1).
   The human inverted the wiring default on 2026-09-19 (ADR-179 §4): a new
   `*_check` is WIRED into `./verify` in the PR that creates it, and the human
   gate on `./verify` narrows to WEAKENING it. So every `tools/*_check.cpp`,
   every `tools/*_check.py` and every `tools/labharness/*_check.mjs` carries, in its first 40 lines, EXACTLY
   ONE declaration line:

       WIRED: <where>          or          UNWIRED: <reason>

   and that declaration is CROSS-CHECKED against `./verify`'s own text. A file
   claiming WIRED that `./verify` does not invoke fails; so does a file claiming
   UNWIRED that it does.

   WHY THE CROSS-CHECK, not just the presence of a marker: the presence rule
   (the 2026-09-19 version of this section) tested that a sentence EXISTS, never
   that it is true, so a marker could contradict the tree indefinitely.
   `undo_check.cpp` carried "not wired" for two days while `./verify` ran it,
   and `tools/labharness/subosc_check.mjs` declared UNWIRED in a header whose
   next sentence explained where in `./verify full` it runs — both invisible,
   because a lie and a truth look identical to a presence test. A stale wiring
   claim is worse than none: the next reader budgets coverage by it.

   THE GRAMMAR IS LINE-ANCHORED. A declaration is a line that BEGINS with the
   marker, after optional indentation and a comment leader (`*`, `//`, `#`).
   A marker quoted mid-sentence is prose — which is what lets a header discuss
   this rule, or its own history, without re-declaring itself. Zero declarations
   and two declarations are both failures: the first is an undeclared
   relationship, the second an ambiguous one.

   WIRING IS DETECTED BY INVOCATION, NOT BY MENTION (known_oracles()'s parse),
   so `./verify`'s prose about why cpu_check is NOT wired cannot be mistaken
   for wiring it.

   THE RULE CALIBRATES ITSELF ON EVERY RUN (selftest(), below): synthetic
   headers asserting each falsehood in BOTH directions must be rejected, and
   the two truthful ones accepted. A gate for lying markers that could itself
   silently stop detecting them would be the same bug one level up.

   KNOWN BOUNDARY: the rule is keyed on FILE NAME, so a check binary whose
   source is not named `*_check.cpp` is outside it — today `cpu_check` (built a
   second time from tools/measure_cpu.cpp) and `alias_check`
   (tools/measure_alias.cpp). Both carry the header line anyway; widening the
   rule to CMake target names is a decision, not a fix to smuggle in here.

GAPS ARE COUNTED, NEVER HIDDEN: rows with `oracle=none` are printed every run.
That number going UP is fine — it means we found something we cannot yet test.
It reading zero when it should not is the failure this gate exists to prevent.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TSV = ROOT / "tests/feature_tests.tsv"
PRES = ROOT / "src/param_presentation.tsv"
VERIFY = ROOT / "verify"
LEGAL_PINS = {"RULING", "ENCODING"}
LEGAL_KIND = {"agentic", "human"}


def known_oracles():
    """Parsed from ./verify, never hardcoded — a list that drifts from the gates
    it names is the same fiction this file exists to catch, one level up."""
    v = VERIFY.read_text(encoding="utf-8")
    compiled = set(re.findall(r'"\$build_dir/([a-z_0-9]+)"', v))
    python = {p.rsplit("/", 1)[-1].removesuffix(".py")
              for p in re.findall(r"python3 (tools/[a-z_0-9]+\.py)", v)}
    return compiled | python


# A DECLARATION is line-anchored: optional indent, optional comment leader, then
# the marker and a non-empty body. Anchoring is what separates the declaration
# from prose ABOUT declarations — see rule 5 in the module docstring.
DECL_RE = re.compile(r"^[ \t]*(?:/\*+|\*+|//+|#+)?[ \t]*(UN)?WIRED:[ \t]*(\S.*)$")
DECL_HEAD = 40             # lines; a declaration buried below the header is not a header


def wired_checks():
    """Check names ./verify actually INVOKES. The compiled half is
    known_oracles()'s parse (a quoted "$build_dir/name"); the lab half is a
    `node tools/labharness/name.mjs` call. A mention does not count: ./verify
    explains at length why cpu_check is NOT wired, and a substring grep would
    read that explanation as the wiring."""
    v = VERIFY.read_text(encoding="utf-8")
    lab = set(re.findall(r"node tools/labharness/([a-z_0-9]+)\.mjs", v))
    return known_oracles() | lab


def declarations(head):
    """Every declaration line in `head`, as (kind, body) with kind in
    {WIRED, UNWIRED}. Pure so selftest() can feed it synthetic headers."""
    out = []
    for line in head:
        m = DECL_RE.match(line)
        if m:
            out.append(("UNWIRED" if m.group(1) else "WIRED", m.group(2).strip()))
    return out


def judge(rel, head, is_wired):
    """The whole of rule 5 for ONE file -> a failure string, or None.
    `is_wired` is the tree's answer (does ./verify invoke it); the header's
    declaration is the file's CLAIM about that answer. Disagreement is the bug."""
    decls = declarations(head)
    if len(decls) != 1:
        return (f"{rel}: {len(decls)} WIRED:/UNWIRED: declarations in its first "
                f"{DECL_HEAD} lines; exactly one is required (B190 layer 1). "
                f"./verify {'runs' if is_wired else 'does not run'} it.")
    kind, body = decls[0]
    if kind == "WIRED" and not is_wired:
        return (f"{rel}: declares `WIRED: {body}` but ./verify invokes no such "
                f"check — the declaration is false, and a false one is worse "
                f"than none (it is budgeted as coverage).")
    if kind == "UNWIRED" and is_wired:
        return (f"{rel}: declares `UNWIRED: {body}` but ./verify does invoke it "
                f"— the declaration is false. Change it to `WIRED: <where>`.")
    return None


def selftest():
    """Both falsehoods must be caught, both truths must pass. Runs every time —
    a detector for lying markers that stopped detecting them would be exactly
    the failure it exists to catch, and it would look green (LIBRARY L0032)."""
    cases = [
        ("truthful WIRED",   [" * WIRED: ./verify full"],                    True,  False),
        ("truthful UNWIRED", [" * UNWIRED: 45 s against a lab with no port"], False, False),
        ("lies: claims wired",   [" * WIRED: ./verify full"],                False, True),
        ("lies: claims unwired", [" * UNWIRED: 45 s, no port"],              True,  True),
        ("no declaration",       [" * a header that never says"],            True,  True),
        ("two declarations",     [" * WIRED: a", " * UNWIRED: b"],           True,  True),
        ("marker only in prose", [' * had been "UNWIRED: reason not stated"'], True, True),
        ("empty body",           [" * WIRED:"],                              True,  True),
    ]
    for name, head, is_wired, must_fail in cases:
        got = judge("selftest", head, is_wired)
        if must_fail and got is None:
            return f"selftest: '{name}' was accepted and must not be"
        if not must_fail and got is not None:
            return f"selftest: '{name}' was rejected and must not be ({got})"
    return None


def declaration_rule():
    """B190 layer 1: every check declares its wiring and the declaration is
    TRUE. -> (failures, n_wired, n_unwired)."""
    wired = wired_checks()
    # `*_check.py` joined 2026-09-23: the census scanned only .cpp and .mjs, so
    # six Python checks — four of them without a declaration — were invisible
    # to the rule that exists to make every check declare itself. Found when an
    # agent's new check sat UNWIRED and the census count did not move.
    files = (sorted(ROOT.glob("tools/*_check.cpp")) + sorted(ROOT.glob("tools/*_check.py"))
             + sorted(ROOT.glob("tools/labharness/*_check.mjs")))
    bad, n_unwired, n_wired = [], 0, 0
    for f in files:
        rel = f.relative_to(ROOT).as_posix()
        is_wired = f.name.rsplit(".", 1)[0] in wired
        head = f.read_text(encoding="utf-8").splitlines()[:DECL_HEAD]
        problem = judge(rel, head, is_wired)
        if problem:
            bad.append(problem)
        elif is_wired:
            n_wired += 1
        else:
            n_unwired += 1
    return bad, n_wired, n_unwired


def gui_features():
    feats = set()
    for line in PRES.read_text(encoding="utf-8").splitlines():
        if line.startswith("#") or line.startswith("address\t"):
            continue
        f = line.split("\t")
        if len(f) >= 5:
            feats.add((f[3], f[4]))
    return feats


def main():
    if not TSV.exists():
        print(f"test_table_check: FAILED — {TSV} missing", file=sys.stderr)
        return 1
    lines = [l for l in TSV.read_text(encoding="utf-8").splitlines() if l and not l.startswith("#")]
    body = [l.split("\t") for l in lines[1:]]

    oracles = known_oracles()
    fail, covered, gaps, ids = [], set(), 0, set()

    for r in body:
        if len(r) < 8:
            fail.append(f"short row: {r!r}")
            continue
        rid, page, feature, kind, pins, owner, oracle = r[0], r[1], r[2], r[3], r[4], r[5], r[6]
        if rid in ids:
            fail.append(f"duplicate test id: {rid}")
        ids.add(rid)
        if kind not in LEGAL_KIND:
            fail.append(f"{rid}: kind {kind!r} is not one of {sorted(LEGAL_KIND)}")
        if pins not in LEGAL_PINS:
            fail.append(f"{rid}: pins {pins!r} is not RULING or ENCODING")
        if pins == "RULING" and not owner.strip():
            fail.append(f"{rid}: a RULING must name its owner")
        if kind == "agentic" and oracle not in oracles and oracle != "none":
            fail.append(f"{rid}: oracle {oracle!r} is not a gate ./verify runs")
        if kind == "human" and oracle != "manual":
            fail.append(f"{rid}: a human test's oracle must be 'manual', not {oracle!r}")
        if oracle == "none":
            gaps += 1
        if page != "*":
            covered.add((page, feature))

    for pf in sorted(gui_features() - covered):
        fail.append(f"no test row for a feature the GUI shows: {pf[0]}/{pf[1]}")

    broken_detector = selftest()
    if broken_detector:
        fail.append(broken_detector)
    decl_fail, n_wired, n_unwired = declaration_rule()
    fail.extend(decl_fail)

    if fail:
        print("test_table_check: FAILED", file=sys.stderr)
        for f in fail[:20]:
            print(f"  {f}", file=sys.stderr)
        if len(fail) > 20:
            print(f"  ... and {len(fail) - 20} more", file=sys.stderr)
        return 1

    agentic = sum(1 for r in body if r[3] == "agentic")
    human = len(body) - agentic
    print(f"test_table_check: GREEN ({len(body)} tests — {agentic} agentic, {human} human; "
          f"{gaps} awaiting an oracle; {n_wired} check files declaring WIRED and "
          f"verified so, {n_unwired} declaring UNWIRED and verified so)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
