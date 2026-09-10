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
          f"{gaps} awaiting an oracle)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
