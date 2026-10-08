#!/usr/bin/env python3
"""armor_dashboard -- the human's one-screen armor report (B448 A1; ADR-197,
docs/strategy/blind-spot-armor.md "The human's dashboard").

WIRED: ./verify fast (--check)

  python3 tools/armor_dashboard.py           # regenerate docs/armor/dashboard.html
  python3 tools/armor_dashboard.py --check   # red if the committed page is stale

THREE PARTS, as the brief orders them:
  1. Armor status: one line per catalogue row (docs/armor/catalogue.json), its
     gates present or missing, status, owner. A hole is drawn as a hole, never
     green; a pending gate is drawn as missing.
  2. Gate-weakening events: read from docs/armor/weakening-baseline.json and
     docs/armor/tolerances.json IF they exist. Both are built by parallel B448
     Wave A PRs; until then this part reads "not yet wired" in hole styling.
     Their schemas are theirs, so this page reports only presence and entry
     count, never an interpretation it would have to guess.
  3. Listening queue: "no batch scheduled", a hole until the listening batch
     exists.

DETERMINISTIC BY CONSTRUCTION. The page is a pure function of the catalogue's
content and the two optional inputs: no timestamp, no git, no ./verify parse.
That last one is deliberate -- reading ./verify here would make this page stale
every time any PR wired a gate, and `--check` would turn red on PRs that never
touched the armor. Whether each gate RUNS is armor_coverage_check's job.

SELF-CONTAINED: inline CSS, no script, no network; light and dark through
prefers-color-scheme.
"""
import html
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import armor_coverage_check as acc  # one loader, one set of counts

OUT = ROOT / "docs/armor/dashboard.html"
OPTIONAL = [("Tolerance registry", "docs/armor/tolerances.json"),
            ("Weakening counter", "docs/armor/weakening-baseline.json")]
GROUPS = [("Risk register", acc.RISK_IDS), ("Agent signatures", acc.SIG_IDS),
          ("Security method (B446), category level only", acc.SEC_IDS)]

CSS = """
:root{--bg:#fafaf7;--fg:#1d1d1b;--mute:#6b6b66;--line:#deded8;--chip:#ecece6;
--ok:#1f7a3a;--okbg:#e3f3e7;--part:#8a5a00;--partbg:#fbefd5;--hole:#a3191f;--holebg:#fbe1e1}
@media (prefers-color-scheme:dark){:root{--bg:#141413;--fg:#e9e9e4;--mute:#9a9a93;
--line:#2d2d2a;--chip:#252523;--ok:#6fd38a;--okbg:#17301e;--part:#f0be5a;--partbg:#33280f;
--hole:#ff7b7b;--holebg:#3a1616}}
*{box-sizing:border-box}
body{margin:0;padding:14px 18px;background:var(--bg);color:var(--fg);
font:12px/1.35 -apple-system,BlinkMacSystemFont,"Segoe UI",Helvetica,Arial,sans-serif}
h1{font-size:16px;margin:0 0 2px}h2{font-size:13px;margin:12px 0 4px}
.sum{color:var(--mute);margin:0}.sum b{color:var(--fg)}
table{border-collapse:collapse;width:100%;table-layout:fixed}
td,th{padding:2px 6px;border-bottom:1px solid var(--line);vertical-align:top;text-align:left}
th{color:var(--mute);font-weight:600}
tr.grp td{color:var(--mute);font-weight:600;padding-top:6px;border-bottom:none}
.id{width:84px;font-weight:600}.st{width:64px}.own{width:118px}.trip{width:22%}
.gaps{white-space:nowrap;overflow:hidden;text-overflow:ellipsis;color:var(--mute)}
.pill{display:inline-block;padding:0 6px;border-radius:8px;font-weight:600}
.guarded{color:var(--ok);background:var(--okbg)}.partial{color:var(--part);background:var(--partbg)}
.hole{color:var(--hole);background:var(--holebg)}
.chip{display:inline-block;margin:0 3px 1px 0;padding:0 5px;border-radius:4px;background:var(--chip)}
.chip.miss{background:transparent;border:1px dashed var(--hole);color:var(--hole)}
.box{padding:4px 8px;border-radius:6px;margin:2px 0}
.box.hole{border:1px dashed var(--hole)}.box.on{background:var(--chip)}
"""


def e(s):
    return html.escape(str(s), quote=True)


def optional_input(rel):
    """-> None when absent, else a one-line, schema-free description."""
    p = ROOT / rel
    if not p.is_file():
        return None
    data, err = acc.load(p)
    if err:
        return "present, not readable as JSON"
    n = len(data) if isinstance(data, (list, dict)) else 1
    return f"present, {n} top-level entr{'y' if n == 1 else 'ies'}"


def gate_chips(row):
    out = []
    for name, pending in acc.gate_entries(row.get("gates", [])) or []:
        if pending:
            out.append(f'<span class="chip miss" title="pending: not yet run">{e(name)} missing</span>')
        else:
            out.append(f'<span class="chip">{e(name)}</span>')
    if not out:
        out.append('<span class="chip miss">no gate</span>')
    return "".join(out)


def status_rows(rows):
    by_id = {r.get("id"): r for r in rows if isinstance(r, dict)}
    out = []
    for label, ids in GROUPS:
        out.append(f'<tr class="grp"><td colspan="6">{e(label)}</td></tr>')
        for rid in ids:
            r = by_id.get(rid)
            if r is None:
                out.append(f'<tr><td class="id">{e(rid)}</td><td colspan="5">'
                           f'<span class="pill hole">absent from the catalogue</span></td></tr>')
                continue
            st = r.get("status", "hole")
            st = st if st in acc.STATUSES else "hole"
            own = r.get("tracked_by", "")
            if st == "hole" and r.get("expires"):
                own = f"{own} · until {r['expires']}"
            gaps = r.get("gaps", "")
            out.append(
                f'<tr><td class="id">{e(rid)}</td>'
                f'<td><b>{e(r.get("title", ""))}</b><br>{gate_chips(r)}</td>'
                f'<td class="st"><span class="pill {st}">{e(st)}</span></td>'
                f'<td class="own">{e(own)}</td>'
                f'<td class="trip">{e(r.get("tripwire", ""))}</td>'
                f'<td class="gaps" title="{e(gaps)}">{e(gaps)}</td></tr>')
    return "\n".join(out)


def weakening_part():
    found = [(label, rel, optional_input(rel)) for label, rel in OPTIONAL]
    if all(desc is None for _, _, desc in found):
        return ('<div class="box hole"><span class="pill hole">hole</span> '
                'not yet wired (B448 Wave A)</div>')
    out = []
    for label, rel, desc in found:
        if desc is None:
            out.append(f'<div class="box hole"><span class="pill hole">hole</span> '
                       f'{e(label)} (<code>{e(rel)}</code>): not yet wired (B448 Wave A)</div>')
        else:
            out.append(f'<div class="box on"><b>{e(label)}</b> (<code>{e(rel)}</code>): '
                       f'{e(desc)}</div>')
    return "\n".join(out)


def render(data):
    rows = data.get("rows", []) if isinstance(data, dict) else []
    risk, rest = acc.summary(rows)
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'">
<title>horde armor dashboard</title>
<style>{CSS}</style></head><body>
<!-- GENERATED by tools/armor_dashboard.py from docs/armor/catalogue.json. Do not edit by hand;
     ./verify fast runs `--check` and is red when this page is stale. -->
<h1>horde armor dashboard</h1>
<p class="sum"><b>{e(risk)}</b> · {e(rest)}</p>
<p class="sum">A hole is a category with no gate yet. A dashed chip is a gate named but not yet run.
Hover a row's last column for its full gaps. Source: docs/armor/catalogue.json (ADR-197, B448).</p>
<h2>1. Armor status</h2>
<table><thead><tr><th class="id">Row</th><th>Category and gates</th><th class="st">Status</th>
<th class="own">Tracked by</th><th class="trip">Tripwire</th><th>Gaps</th></tr></thead>
<tbody>
{status_rows(rows)}
</tbody></table>
<h2>2. Gate-weakening events</h2>
{weakening_part()}
<h2>3. Listening queue</h2>
<div class="box hole"><span class="pill hole">hole</span> no batch scheduled</div>
</body></html>
"""


def main(argv):
    check = argv[1:] == ["--check"]
    if argv[1:] not in ([], ["--check"]):
        print("usage: armor_dashboard.py [--check]", file=sys.stderr)
        return 2
    data, err = acc.load(acc.CATALOGUE)
    if err:
        print(f"armor_dashboard: FAILED — {err}", file=sys.stderr)
        return 1
    page = render(data)
    rel = OUT.relative_to(ROOT)
    if check:
        have = OUT.read_text(encoding="utf-8") if OUT.is_file() else None
        if have != page:
            print(f"armor_dashboard: FAILED — {rel} is "
                  f"{'missing' if have is None else 'stale'}; run "
                  "python3 tools/armor_dashboard.py and commit the result", file=sys.stderr)
            return 1
        print(f"armor_dashboard: {rel} current")
        return 0
    OUT.write_text(page, encoding="utf-8")
    print(f"armor_dashboard: wrote {rel}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
