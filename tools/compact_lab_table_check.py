#!/usr/bin/env python3
"""compact_lab_table_check — the compact lab's embedded parameter table still
matches src/param_presentation.tsv.

UNWIRED: an implementer dispatch may not edit ./verify (implementer charter); the lead wires it — one line in fast(): python3 tools/compact_lab_table_check.py || ok=1

WHY. docs/design/compact-lab.html (B212, B227) embeds a generated COPY of the
presentation table joined to the shell's ranges (`const PT = [...]`), because a
lab is a single self-contained file. A copy drifts silently: B213 (units and
tapers) or any relabel changes the TSV, the lab keeps drawing yesterday's names
and pages, and nothing fails. Round 1 left this owed (ROADMAP B212 "Open").

WHAT IS COMPARED, per address: label, page, group, widget, whether a chunk is
named ("designed"), shown_when, scale and unit — the eight TSV-sourced columns
of a PT row — plus the address SETS in both directions.

WHAT IS NOT: min/max/default/stepped/enum. Those come from kParams in
src/hypersaw_clap.cpp, and parsing that array a second time here would be one
more copy of a parser the registry tools already own. That half of the copy can
still drift; this check says so every run rather than implying full coverage.

CALIBRATED (L0032): before trusting a green, the check plants one relabel in an
in-memory copy of the lab's table and requires the comparison to catch it. A
comparator that cannot fail proves nothing.
"""
import csv
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
LAB = ROOT / "docs/design/compact-lab.html"
TSV = ROOT / "src/param_presentation.tsv"

# PT row layout, from the lab's own BLOCK 0 comment:
# [address, label, page, group, widget, min, max, default, stepped, enum,
#  designed(chunk named), shown_when, scale, unit]
FIELDS = (("label", 1), ("page", 2), ("group", 3), ("widget", 4),
          ("designed", 10), ("shown_when", 11), ("scale", 12), ("unit", 13))


def lab_rows(text):
    m = re.search(r"const PT = \[\n(.*?)\n\];", text, re.S)
    if not m:
        raise SystemExit("compact_lab_table_check: no `const PT = [` block in " + str(LAB.relative_to(ROOT)))
    rows = {}
    for line in m.group(1).splitlines():
        line = line.strip().rstrip(",")
        if not line:
            continue
        r = json.loads(line)
        rows[r[0]] = r
    return rows


def tsv_rows():
    lines = [l for l in TSV.read_text().splitlines() if l and not l.startswith("#")]
    out = {}
    for r in csv.DictReader(lines, delimiter="\t"):
        out[r["address"]] = r
    return out


def expected(t):
    """a TSV row in the PT row's terms"""
    return {"label": t["label"], "page": t["page"], "group": t["group"], "widget": t["widget"],
            "designed": 1 if (t.get("chunk") or "").strip() else 0,
            "shown_when": t.get("shown_when") or "", "scale": t.get("scale") or "",
            "unit": t.get("unit") or ""}


def compare(lab, tsv):
    errs = []
    for a in sorted(set(tsv) - set(lab)):
        errs.append(f"{a}: in the TSV, missing from the lab's table")
    for a in sorted(set(lab) - set(tsv)):
        errs.append(f"{a}: in the lab's table, not in the TSV")
    for a in sorted(set(lab) & set(tsv)):
        want = expected(tsv[a])
        for name, i in FIELDS:
            have = lab[a][i]
            if have != want[name]:
                errs.append(f"{a}: {name} is {have!r} in the lab, {want[name]!r} in the TSV")
    return errs


def main():
    lab = lab_rows(LAB.read_text())
    tsv = tsv_rows()
    # calibration: a planted relabel must be caught, and only that one
    probe = next(iter(sorted(lab)))
    planted = {a: list(r) for a, r in lab.items()}
    planted[probe][1] = planted[probe][1] + " (planted)"
    base = compare(lab, tsv)
    caught = [e for e in compare(planted, tsv) if e not in base]
    if len(caught) != 1 or not caught[0].startswith(probe + ": label"):
        print(f"compact_lab_table_check: CALIBRATION FAILED — a planted relabel of {probe} was not caught "
              f"exactly once ({len(caught)} new findings); the comparison is blind", file=sys.stderr)
        return 1
    if base:
        print(f"compact_lab_table_check: {LAB.relative_to(ROOT)} has drifted from "
              f"{TSV.relative_to(ROOT)} ({len(base)} findings). Regenerate the PT block; never hand-edit it:",
              file=sys.stderr)
        for e in base[:40]:
            print("    " + e, file=sys.stderr)
        if len(base) > 40:
            print(f"    … and {len(base) - 40} more", file=sys.stderr)
        return 1
    print(f"compact_lab_table_check: {len(lab)} rows match the TSV on 8 columns (calibration plant caught); "
          f"ranges/defaults/enums from kParams are NOT checked here")
    return 0


if __name__ == "__main__":
    sys.exit(main())
