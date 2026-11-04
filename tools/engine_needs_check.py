#!/usr/bin/env python3
"""engine_needs_check — every GUI ruling states its engine impact, and the needs list agrees (ADR-361, B617).

WIRED: ./verify fast (wired in the PR that creates it, under ADR-180 §1)

WHY. A GUI ruling can rest on behaviour the engine doesn't have. Before ADR-361 the rule that such a
ruling adds a row to docs/design/gui3/ENGINE-NEEDS.md lived only in one session's memory, nothing
read that file, and its first version found 13 needs the engine side had no record of.

THE RECORDS. `## GUI-n` headings in DECISIONS.md; `engine_impact` in docs/design/design-ledger.json,
one entry per GUI-n, a list of GN ids or a string `none: <reason>`; the `| GN-n |` rows in
ENGINE-NEEDS.md, whose third column says who decided it. The GUI session writes all of these, and the
lead schedules by citing GN ids in its own ROADMAP rows, so neither edits the other's record.

WHAT FAILS. A ruling with no entry, an entry with no ruling, an entry of the wrong shape, an empty
reason, a GN id that doesn't exist, a duplicate GN id, a row citing no ruling, and either direction of
disagreement between a ruling's list and a row's citations. A row may cite an ADR rather than a GUI-n
(GN-23 cites only ADR-266 r7), and that isn't a disagreement: only GUI-n citations are cross-checked.

WHAT ONLY REPORTS. How many GN ids no ROADMAP row cites yet. That is the lead's queue, printed every
run so it can't be missed. Failing on it would make every new need red in the GUI session's own PR.

THE LIMIT. A behaviour change made with no ruling isn't caught (ADR-361). "none:" is the GUI session's
judgement; this checks that it was stated, not that it is right.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def rulings_of(decisions):
    # The em dash is load-bearing: `## GUI-10 A1` style lines are amendments, not new rulings.
    return set(re.findall(r"^## (GUI-\d+) —", decisions, re.M))


def rows_of(needs):
    """(id, its Decided-by column) per row, duplicates kept so they can be reported."""
    out = []
    for line in needs.splitlines():
        m = re.match(r"^\|\s*(GN-\d+)\s*\|", line)
        if m:
            cols = line.split("|")
            out.append((m.group(1), cols[3] if len(cols) > 3 else ""))
    return out


def check(decisions, impact, needs):
    errs = []
    rulings = rulings_of(decisions)
    rows = rows_of(needs)
    ids = [r[0] for r in rows]
    for dup in sorted({i for i in ids if ids.count(i) > 1}):
        errs.append(f"{dup}: more than one row")
    cited_by_row = {}
    for gid, by in rows:
        gui = set(re.findall(r"GUI-\d+", by))
        if not gui and not re.search(r"ADR-\d+", by):
            errs.append(f"{gid}: cites no GUI ruling or ADR")
        cited_by_row[gid] = gui
    for g in sorted(rulings - set(impact)):
        errs.append(f"{g}: no engine_impact entry")
    for g in sorted(set(impact) - rulings):
        errs.append(f"{g}: engine_impact entry with no ruling")
    for g, v in sorted(impact.items()):
        if isinstance(v, str):
            if not re.match(r"none: \S", v):
                errs.append(f"{g}: a string entry must be `none: <reason>`")
        elif isinstance(v, list) and v and all(isinstance(x, str) for x in v):
            for gid in v:
                if gid not in cited_by_row:
                    errs.append(f"{g}: lists {gid}, which has no row")
                elif g not in cited_by_row[gid]:
                    errs.append(f"{g}: lists {gid}, whose row doesn't cite it")
        else:
            errs.append(f"{g}: an entry is a non-empty list of GN ids or `none: <reason>`")
    for gid, gui in sorted(cited_by_row.items()):
        for g in sorted(gui):
            v = impact.get(g)
            if g in rulings and (not isinstance(v, list) or gid not in v):
                errs.append(f"{gid}: cites {g}, whose engine_impact doesn't list it")
    return errs


def unscheduled(needs, roadmap):
    cited = set(re.findall(r"\bGN-\d+\b", roadmap))
    return sorted({r[0] for r in rows_of(needs)} - cited, key=lambda s: int(s[3:]))


def selftest(decisions, impact, needs):
    """Each plant must add a failure the real records don't have; a clean copy must add none."""
    real = set(check(decisions, impact, needs))
    bad = []
    if set(check(decisions, dict(impact), needs)) != real:
        bad.append("a clean copy changed the result")
    g_list = next((g for g, v in impact.items() if isinstance(v, list)), None)
    g_none = next((g for g, v in impact.items() if isinstance(v, str)), None)
    first = rows_of(needs)[0][0] if rows_of(needs) else None
    if not (g_list and g_none and first):
        return ["the records hold no list entry, no `none:` entry or no row, so the plants can't run"]
    row_line = next(l for l in needs.splitlines() if l.startswith(f"| {first} |"))
    plants = {
        "a ruling with no entry": (decisions, {k: v for k, v in impact.items() if k != g_none}, needs),
        "an entry with no ruling": (decisions, dict(impact, **{"GUI-99999": "none: planted"}), needs),
        "an empty reason": (decisions, dict(impact, **{g_none: "none: "}), needs),
        "the wrong shape": (decisions, dict(impact, **{g_none: 3}), needs),
        "a list naming no row": (decisions, dict(impact, **{g_list: impact[g_list] + ["GN-99999"]}), needs),
        "a duplicate row": (decisions, impact, needs + "\n" + row_line),
        "a row citing nothing": (decisions, impact, needs + "\n| GN-99998 | planted | nobody | x |"),
        "a row citing a ruling that doesn't list it": (decisions, impact, needs + f"\n| GN-99997 | planted | {g_list} | x |"),
        "a ruling dropping a row it's cited by": (decisions, dict(impact, **{g_list: impact[g_list][1:]}), needs),
        "a new ruling heading": (decisions + "\n## GUI-99996 — planted\n", impact, needs),
    }
    for name, args in plants.items():
        if not set(check(*args)) - real:
            bad.append(f"plant didn't fire: {name}")
    return bad


def main():
    decisions = (ROOT / "DECISIONS.md").read_text(encoding="utf-8")
    needs = (ROOT / "docs/design/gui3/ENGINE-NEEDS.md").read_text(encoding="utf-8")
    roadmap = (ROOT / "ROADMAP.md").read_text(encoding="utf-8")
    impact = json.loads((ROOT / "docs/design/design-ledger.json").read_text(encoding="utf-8")).get("engine_impact", {})
    errs = check(decisions, impact, needs)
    bad = selftest(decisions, impact, needs)
    for e in errs:
        print(f"engine_needs_check: FAIL {e}")
    for b in bad:
        print(f"engine_needs_check: FAIL selftest: {b}")
    if errs or bad:
        return 1
    rest = unscheduled(needs, roadmap)
    total = len(rows_of(needs))
    print(f"engine_needs_check: ok ({len(rulings_of(decisions))} GUI rulings, {total} needs, 10 must-fail plants); not yet scheduled by the lead: {len(rest)} of {total}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
