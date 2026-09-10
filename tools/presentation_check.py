#!/usr/bin/env python3
"""presentation_check — the presentation table is TOTAL, address-keyed, and honest.

WHAT THIS GATES, and why each part exists:

1. TOTALITY. Every parameter the shell declares has exactly one presentation row,
   and the table has no rows for parameters that do not exist. A GUI generated
   from a partial table silently omits controls — which is the 29-dead-controls
   failure with the arrow reversed, and no audio oracle can see either.

2. ADDRESS-KEYED, STRUCTURALLY. The table must have NO `id` column. FOUNDATIONS'
   D1 caution, adopted verbatim: "do not let the presentation table key on
   anything but the address — a page/group name that is also a dispatch fact is
   how the two get fused again." A rule enforced by the absence of a column
   cannot be violated by a careless row, which is why it is checked here as a
   header property rather than trusted as a convention.

3. SCOPE AGREES WITH THE ADDRESS. `scope` is not independent data — it IS the
   address prefix (their D2: scopes are named by address prefix, never by an
   enumerated type). Checking it means the column can never drift into a second,
   disagreeing vocabulary.

WHAT THIS DELIBERATELY DOES NOT GATE: patch-scope. It is dispatch, not
presentation, and `tools/gui_reach.py` already derives it from the shell with two
semantic anchors that took three wrong versions to settle. A second copy here
would be a second copy of the rule.

GAPS ARE COUNTED, NOT HIDDEN. Rows still at page=TODO or group=(ungrouped) are
reported every run. A table that is 97% filled should say so; silence would read
as complete.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TSV = ROOT / "src/param_presentation.tsv"
SHELL = ROOT / "src/hypersaw_clap.cpp"
NOSC = 2   # kNumOsc; the per-oscillator copies the shell actually declares


def shell_addresses():
    """The address set, derived from the shell — the same source the registry
    derives from, so this check compares the table against reality rather than
    against another copy of the table."""
    src = SHELL.read_text(encoding="utf-8")
    decl = src.split("kParams[] = {", 1)[1].split("\n};", 1)[0]
    rows = re.findall(r'\{\s*(\d+),\s*"([A-Za-z0-9_]+)"', decl)
    gl = src.split("kGlobalIds[] = {", 1)[1].split("};", 1)[0]
    globals_ = {int(x) for x in re.findall(r"\b(\d+)\b", re.sub(r"//.*", "", gl))}
    out = {}
    for sid, key in rows:
        if int(sid) in globals_:
            out[key] = "global"
        else:
            for o in range(NOSC):
                out[f"osc{o + 1}.{key}"] = f"osc{o + 1}"
    return out


def check_select_ranges():
    """A hand-written <select> must offer every value its param declares.

    gui2's FX type dropdowns are literal <option> lists, outside the generator's
    markers. When ADR-129 widened the type params 0..6 -> 0..8 the engine gained
    Echo and Room while the GUI kept offering seven options, so two slot types
    shipped that a player could not select at all. `gui_reach` stayed green
    throughout, and correctly: param 57 IS reachable -- it simply cannot reach
    all of its own VALUES, which is a gap that gate was never shaped to see.

    The narrow rule: for a stepped param named by a hand-written select, the
    option values must be exactly the declared range. Generated controls are
    exempt -- the generator derives them and cannot drift by construction.
    """
    src = SHELL.read_text(encoding="utf-8")
    decl = src.split("kParams[] = {", 1)[1].split("\n};", 1)[0]
    ranges = {}
    for m in re.finditer(
            r'\{\s*(\d+)\s*,\s*"[^"]+"\s*,\s*"[^"]+"\s*,'
            r'\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(true|false)', decl):
        pid, lo, hi, _d, stepped = m.groups()
        if stepped == "true":
            ranges[int(pid)] = (int(float(lo)), int(float(hi)))
    gui = (ROOT / "src/gui/gui2.html").read_text(encoding="utf-8")
    bad = 0
    for m in re.finditer(r'<select data-p="(\d+)"[^>]*>(.*?)</select>', gui, re.S):
        pid = int(m.group(1))
        if pid not in ranges:
            continue
        vals = sorted(int(v) for v in re.findall(r'<option value="(-?\d+)"', m.group(2)))
        lo, hi = ranges[pid]
        want = list(range(lo, hi + 1))
        if vals != want:
            miss = [v for v in want if v not in vals]
            extra = [v for v in vals if v not in want]
            print(f"  param {pid}: select offers {vals}, declared range {lo}..{hi}"
                  + (f" -- MISSING {miss}" if miss else "")
                  + (f" -- EXTRA {extra}" if extra else ""))
            bad += 1
    if bad:
        print(f"presentation_check: FAILED -- {bad} select(s) do not cover their param's range")
    return bad


def main():
    if not TSV.exists():
        print(f"presentation_check: FAILED — {TSV} missing", file=sys.stderr)
        return 1
    lines = [l for l in TSV.read_text(encoding="utf-8").splitlines() if l and not l.startswith("#")]
    header, body = lines[0].split("\t"), [l.split("\t") for l in lines[1:]]

    fail = []
    if "id" in [h.strip().lower() for h in header]:
        fail.append("header has an `id` column — presentation must key on the address alone")

    seen, table = set(), {}
    for r in body:
        if len(r) < 6:
            fail.append(f"short row: {r!r}")
            continue
        addr, scope = r[0], r[1]
        if addr in seen:
            fail.append(f"duplicate address: {addr}")
        seen.add(addr)
        table[addr] = scope

    want = shell_addresses()
    missing = sorted(set(want) - seen)
    extra = sorted(seen - set(want))
    for a in missing:
        fail.append(f"no presentation row for declared param: {a}")
    for a in extra:
        fail.append(f"presentation row for a param the shell does not declare: {a}")
    for a, scope in sorted(table.items()):
        if a in want and scope != want[a]:
            fail.append(f"{a}: scope column says {scope!r}, address says {want[a]!r}")

    if fail:
        print("presentation_check: FAILED", file=sys.stderr)
        for f in fail[:20]:
            print(f"  {f}", file=sys.stderr)
        if len(fail) > 20:
            print(f"  ... and {len(fail) - 20} more", file=sys.stderr)
        return 1

    undesigned = sum(1 for r in body if len(r) < 8 or not r[7].strip())
    todo = sum(1 for r in body if r[3] == "TODO")
    ungrouped = sum(1 for r in body if r[4] == "(ungrouped)")
    scopes = sorted({r[1] for r in body})
    # The undesigned count is printed every run because it IS the queue. Silence
    # would read as "the GUI is done" — which is exactly the reading that let a
    # generated-everything pass look like progress.
    if check_select_ranges():
        return 1
    print(f"presentation_check: GREEN ({len(body)} rows, scopes: {', '.join(scopes)}; "
          f"{undesigned} undesigned (no chunk named)"
          f"{f', {todo} page=TODO' if todo else ''}"
          f"{f', {ungrouped} ungrouped' if ungrouped else ''})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
