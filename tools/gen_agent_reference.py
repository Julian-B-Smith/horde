#!/usr/bin/env python3
"""gen_agent_reference -- writes docs/manual/agent/parameters.md, the agent's parameter lookup (B629).

WHY. An agent dictating a patch needs every key with its unit, range, law, default, what it does and
when it can act, in one page it can search. BD-5 rules that the manual's parameter list is generated
from the one manifest and that nothing hand-copies it, so this page is generated, never edited.

WHAT IT READS.
  docs/design/gui3/params.json   the manifest as SPEC-ENGINE 5 states it (tools/gen_gui3_controls.py
                                 parses the spec's tables into it): key, unit, range, law, default,
                                 description, reserved
  specs/SPEC-ENGINE.md           PA-8's live-when table, through tools/patch_check.py's reading of it
Rows that exist once per oscillator, blade or envelope are folded into one line per table row
(`oscN.`, `bladeB.`, `envE.`), as section 5 writes them; a field that differs between the copies
(blade 1's and blade 2's defaults, the two seeds) is shown per copy. A description is section 5's text,
word for word; an ENV 2 to ENV 4 row that reads "As `env1.x`" also carries env1.x's text. A row with
no description says "spec gives no description". Nothing is written that the two sources do not say.

Usage: python3 tools/gen_agent_reference.py           write docs/manual/agent/parameters.md
       python3 tools/gen_agent_reference.py --check   exit 1 if the committed page differs (writes nothing)
No clock, no set iteration: two runs give byte-identical output.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import patch_check as pc  # after the path insert above: tools/ is not a package

OUT = ROOT / "docs/manual/agent/parameters.md"
NO_DESC = "spec gives no description"

# Section 5's tables, in its order. Each: (title, spec section, predicate on a key).
GROUPS = [
    ("The voice, ENV 1, the mixer and the output", "5.2",
     lambda k: k.split(".")[0] in ("voice", "env1", "mix", "out", "quality")),
    ("The source on switches", "5.2", lambda k: k.endswith(".on") and k.count(".") == 1),
    ("ENV 2 to ENV 4", "5.2", lambda k: k.split(".")[0] in ("env2", "env3", "env4")),
    ("Each oscillator", "5.3", lambda k: k.split(".")[0] in ("osc1", "osc2") and k.count(".") == 1
     and not k.endswith(".on")),
    ("Each blade", "5.4", lambda k: k.split(".")[0] in ("osc1", "osc2") and k.count(".") == 2),
    ("The sub", "5.5", lambda k: k.startswith("sub.") and k != "sub.on"),
    ("The noise oscillator", "5.6", lambda k: k.startswith("noise.") and k != "noise.on"),
]


def pattern(key):
    p = re.sub(r"^osc[12]\.", "oscN.", key)
    p = re.sub(r"\.blade[12]\.", ".bladeB.", p)
    return re.sub(r"^env[234]\.", "envE.", p)


def copy_label(key):
    """The part of a key a folded row varies over, as a reader names it."""
    parts = []
    m = re.match(r"^osc([12])\.", key)
    if m:
        parts.append(f"osc {m.group(1)}")
    m = re.search(r"\.blade([12])\.", key)
    if m:
        parts.append(f"blade {m.group(1)}")
    m = re.match(r"^env([234])\.", key)
    if m:
        parts.append(f"ENV {m.group(1)}")
    return parts


def per_copy(keys, field):
    """One value, or the values per copy ('blade 1: 6 · blade 2: 3'), labelled by the axis they vary on."""
    vals = [field(k) for k in keys]
    if len(set(vals)) == 1:
        return vals[0]
    labels = [copy_label(k) for k in keys]
    for axis in range(max(len(x) for x in labels)):
        groups = {}
        for lab, v in zip(labels, vals):
            groups.setdefault(lab[axis], set()).add(v)
        if all(len(s) == 1 for s in groups.values()):
            return " · ".join(f"{a}: {next(iter(s))}" for a, s in groups.items())
    return " · ".join(f"{k}: {v}" for k, v in zip(keys, vals))


def description(rows, key):
    r = rows[key]
    text = r["spec"].strip()
    if not text:
        return NO_DESC
    if r.get("see") and text.startswith("As `"):
        text = f"{text.rstrip('.')}: {rows[r['see']]['spec'].strip()}"
    return text


def cell(s):
    return s.replace("|", "\\|").replace("\n", " ")


def pa8_conditions(spec_text):
    """PA-8's condition cells by parameter cell. "as above" points at the previous row, and a row read
    alone needs that row's words in place."""
    _, spec_rows = pc.read_pa8(spec_text)
    conds = dict(spec_rows)
    for (c0, t0), (c1, t1) in zip(spec_rows, spec_rows[1:]):
        if t1.startswith("as above"):
            conds[c1] = f"[{t0}]{t1[len('as above'):]}"
    return conds


def live_phrase(entries, key, conds):
    """PA-8's own condition text for the rows that name this key (blanket rows are stated per section)."""
    # A cell that pairs parameters with conditions by position ("`density` / `rate` / `length`") is kept,
    # so the pairing stays readable on each row.
    if any(c == "every reserved row (5.1)" for c, *_ in pc.matching(entries, key)):
        return conds["every reserved row (5.1)"]
    out = [(f"{c}: " if " / " in c else "") + conds[c]
           for c, _, scope, _, _ in pc.matching(entries, key) if scope == "row"]
    return "; and ".join(out) if out else "—"


def section_note(entries, keys, conds):
    """The blanket PA-8 rows that govern a whole section, and the source test (PA-8's preamble)."""
    notes = []
    for c, _, scope, _, _ in entries:
        if scope == "section" and c != "every reserved row (5.1)" and any(
                m for m in keys if any(e[0] == c for e in pc.matching(entries, m))):
            notes.append(f"PA-8, *{c}*: {conds[c]}")
    srcs = sorted({pc.source_of(k) for k in keys if pc.source_of(k) and not k.endswith(".on")})
    if srcs and not any("every noise row" in n for n in notes):
        notes.append("Every row naming a source is live only while that source is on (" +
                     ", ".join(f"`{s}.on`" for s in srcs) + "): PA-8's preamble, and an off source adds nothing (5.2).")
    elif srcs:
        notes.append("`spread`, `pan`, `mute`, `solo` and `env` are live only while `noise.on` is on.")
    return notes


def values_text(rows, key):
    """What a patch may write: the option names for a step row with named options, else the range."""
    r = rows[key]
    if r.get("options") and not pc.numeric_options(r):
        return ", ".join(f"`{o}`" for o in r["options"])
    return r["range_text"]


def render(rows, spec_text):
    entries = pc.pa8_entries(rows)
    pre, _ = pc.read_pa8(spec_text)
    conds = pa8_conditions(spec_text)
    keys = list(rows)
    n_res = sum(r["reserved"] for r in rows.values())
    n_nodesc = sum(1 for r in rows.values() if not r["spec"].strip())
    lines = [
        "# horde engine: parameters, for agents",
        "",
        "<!-- GENERATED by tools/gen_agent_reference.py from docs/design/gui3/params.json and SPEC-ENGINE PA-8."
        " Do not edit; edit the spec, regenerate params.json, then run the generator. -->",
        "",
        "Every parameter of SPEC-ENGINE section 5, the engine as built today. Read `README.md` first for the patch format.",
        "",
        "- **Keys.** `oscN.` is `osc1.` or `osc2.`; `bladeB.` is `blade1.` or `blade2.`; `envE.` is `env2.`, `env3.` or"
        " `env4.`. Every folded row exists for each copy. `oscN.blade2.` rows exist on blade 2 only.",
        "- **Value.** Give x in the values column's numbers (a `%` row with range 0..1 takes 0..1). A step row takes one of"
        " its option names, or its 0-based index; a step row whose options are numbers takes the number.",
        "- **Law.** How the host's v in [0, 1] maps to x (5.1). `tools/patch_check.py --set` converts for you.",
        "- **Live when.** PA-8's own words. A row whose condition fails in your patch is inert: it changes nothing.",
        "- **Status.** `built`: in the engine today. `reserved`: holds a key for a later feature and is refused in 1.0"
        " (5.1, PA-7); never set it.",
        "",
        f"{len(rows)} parameters in {len({pattern(k) for k in keys})} rows below; {n_res} reserved;"
        f" {n_nodesc} with no description in the spec.",
        "",
        "PA-8's preamble, verbatim: " + pre[len(pc.PA8_HEAD):].strip().rstrip(":").rstrip(")") + ".",
    ]
    for title, sec, pred in GROUPS:
        gkeys = [k for k in keys if pred(k)]
        if not gkeys:
            continue
        lines += ["", f"## {title} ({sec})", ""]
        lines += [f"- {n}" for n in section_note(entries, gkeys, conds)]
        lines += ["", "| key | unit | values | law | default | what it does (section 5) | live when (PA-8) | status |",
                  "|---|---|---|---|---|---|---|---|"]
        seen = []
        for k in gkeys:
            p = pattern(k)
            if p in seen:
                continue
            seen.append(p)
            copies = [m for m in gkeys if pattern(m) == p]
            shown = p
            if all(".blade2." in m for m in copies):
                shown = p.replace(".bladeB.", ".blade2.")
            r = rows[k]
            status = "reserved" if r["reserved"] else "built"
            lines.append("| " + " | ".join(cell(x) for x in [
                f"`{shown}`",
                r["unit"] or "—",
                per_copy(copies, lambda m: values_text(rows, m)),
                r["law"],
                per_copy(copies, lambda m: rows[m]["default_text"]),
                per_copy(copies, lambda m: description(rows, m)),
                per_copy(copies, lambda m: live_phrase(entries, m, conds)),
                status,
            ]) + " |")
    lines += ["", "Not a parameter, so not settable in a patch: `oscN.ratios`, the custom cut-rule list (5.3; PA-8: live when"
              " `cutRule` is custom).", ""]
    return "\n".join(lines)


def main(argv):
    rows = pc.load_rows()
    text = render(rows, pc.SPEC.read_text(encoding="utf-8"))
    if "--check" in argv:
        have = OUT.read_text(encoding="utf-8") if OUT.exists() else ""
        if have != text:
            print(f"gen_agent_reference: STALE — {OUT.relative_to(ROOT)} differs from its generator; "
                  f"run python3 tools/gen_agent_reference.py", file=sys.stderr)
            return 1
        print(f"gen_agent_reference: current — {OUT.relative_to(ROOT)}")
        return 0
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text, encoding="utf-8")
    print(f"gen_agent_reference: wrote {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
