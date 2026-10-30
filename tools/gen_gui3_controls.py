#!/usr/bin/env python3
"""gen_gui3_controls -- GUI 3's controls, generated from the engine's parameter table (ROADMAP B584, ADR-316).

WIRED: ./verify fast (as `python3 tools/gen_gui3_controls.py --check`)

WHY. ADR-316 rules that GUI 3's controls are generated from SPEC-ENGINE section 5, "so a control cannot
exist without its parameter", and that B302's gates apply to the shell (`gen_gui_controls`, `gui_reach`).
There was no machine-readable form of section 5 in the tree (engine2/core/manifest/manifest.h is a
one-line stub until the builder's stage that writes it; engine2/structure/ declares blocks, not
parameters), so this file is that reading: it parses the spec's own tables, never a hand-typed list.

WHAT IT READS AND WRITES.
  specs/SPEC-ENGINE.md            section 5's tables (5.2 to 5.6), the on-switch sentence, and 5.8's counts
  docs/design/gui3/layout.json    which page and group each key goes in (design input, hand-edited)
  -> docs/design/gui3/params.json   the manifest as the spec states it: 330 rows, canonical JSON
  -> docs/design/gui3/shell-data.js the same rows plus the pages' control lists, as one script the
                                    shell loads from file:// (a fetch of JSON is refused there)
Both outputs are generated; edit the spec or the layout, then run this. No wall clock, no set
iteration, so two runs give byte-identical files.

THE EXPANSION (5.1, 5.8). 5.3's rows exist once per oscillator, 5.4's once per blade per oscillator
(the `(blade 2 only)` rows on blade 2 only; blade 2's "as blade 1" option comes first, at index 0),
ENV 2 to ENV 4's table once per envelope; a `mute`, `solo` row is two parameters; the four on switches
come from the sentence under 5.2's table. A row whose class says `reserved` is in the manifest and is
not a host parameter (5.1), so the shell never draws it.

THE NORMALISED DEFAULT. Every parameter's host value is v in [0, 1] (IF-11); the default's v is the
inverse of the row's law (5.1's table) at the default value. The seam check
(tools/labharness/gui3_seam_check.mjs) runs the stub's forward laws on these v and must land on the
spec's defaults, so the two implementations of the laws check each other.

WHAT --check GATES (each failure carries its tag; the controls below assert the tag):
  parse           every cell of every row parses (range, default, law, class); none is skipped
  counts          the expansion's numbers equal 5.8's own: table rows, manifest parameters, host
                  parameters, and the per-table row counts, all parsed from 5.8's text
  unknown-key     every key the layout names is a parameter ("no control lacks a parameter")
  reserved-placed no reserved row is placed on any page (the host never shows a control that does nothing)
  unreached       every host parameter is placed on at least one page (B302's gui_reach idea)
  duplicate       no page places one key twice
  widget          a layout widget suits its row (a knob or slider only for a continuous row, a toggle
                  only for an off/on row, pills for at most 8 options)
  override-reason the widget rule (ADR-319 item 5, default_widget) is the default; an entry that departs
                  from it carries a non-empty `reason`, and an entry with a `reason` departs from it
  live            a live-when condition names stepped keys and options that exist (or a number for gt)
  shell           the shell page mounts parameter controls in one place only, from the generated list,
                  and holds no parameter key of its own (a hand-placed control would be a second source)
  determinism     no gui3 script reads a clock or an unseeded random source
  bank            the bottom modulator bank's critical controls (ADR-322 item 3) are chosen per kind in
                  layout.json with a reason, and every chosen row exists in each block the kind matches
  nav             every page sits in exactly one group of the two-level page bar (B589 Page nav)
  role            a section header's power/modes and a channel strip's fader/on/mute/solo suit their rows
  dashboard       each dashboard card carries its source's power switch and only keys its source page carries
  ledger          the port ledger (ADR-333, B593): every row has a reason from its closed set and a sentence, a source
                  path that exists (or "nowhere yet"), a ROADMAP row that exists, and a status (partial: what is missing)
  stale           the committed outputs equal what this run generates
MUST-FAIL CONTROLS, every run, on in-memory copies: an unknown key, an unplaced host row, a placed
reserved row, a knob on a stepped row, an override with no reason, a reason with no override, a bank control with no parameter, a bank choice
with no reason, a live condition on a missing option, a dropped spec row, a
shell with a hand-placed key, a shell with a second mount call, a script reading Date.now, and a
stale output. Each must come back red with its own tag, or this check is red.

Usage: python3 tools/gen_gui3_controls.py           write the outputs
       python3 tools/gen_gui3_controls.py --check   gate (writes nothing)
"""
import copy
import json
import math
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SPEC = ROOT / "specs/SPEC-ENGINE.md"
GUI3 = ROOT / "docs/design/gui3"
LAYOUT = GUI3 / "layout.json"
OUT_JSON = GUI3 / "params.json"
OUT_JS = GUI3 / "shell-data.js"
SHELL = ROOT / "docs/design/gui3.html"
SCRIPTS = ["engine-stub.js", "device.js", "shell-logic.js"]   # the gui3 scripts the determinism gate reads, with the shell

FORMAT = "horde.gui3-params/1"
LAWS = ("lin", "log", "pow", "lin, wraps", "log, off zone", "dB", "step")
WIDGETS = ("knob", "hslider", "vslider", "toggle", "pills", "select", "stepper")
PILLS_MAX, SELECT_MAX = 8, 64        # past 64 options a list is not browsable; a stepper steps it


class Red(Exception):
    """A gate failure: `tag` is what the must-fail controls assert."""
    def __init__(self, tag, msg):
        super().__init__(f"[{tag}] {msg}")
        self.tag = tag


# ---------------------------------------------------------------- reading the spec's tables

def num(s):
    """A number as the spec writes it: a true minus sign, thin-space thousands, a leading +, pi."""
    t = s.strip().replace("−", "-").replace(" ", "").replace("\u202f", "").replace("+", "")
    if t in ("π", "-π"):
        return math.pi if t == "π" else -math.pi
    return float(t)


def is_num(s):
    try:
        num(s)
        return True
    except ValueError:
        return False


def cells(line):
    return [c.strip() for c in line.strip().strip("|").split("|")]


def section(text, head):
    """The body of '### <head>' up to the next '### '."""
    i = text.index("\n### " + head)
    j = text.find("\n### ", i + 5)
    return text[i:j if j > 0 else len(text)]


def tables(body):
    """Every markdown table in body whose header starts with `key`, as (header, rows, offset)."""
    out, lines, i = [], body.split("\n"), 0
    while i < len(lines):
        if lines[i].startswith("| key |"):
            head, rows, j = cells(lines[i]), [], i + 2
            while j < len(lines) and lines[j].startswith("|"):
                rows.append(cells(lines[j]))
                j += 1
            out.append((head, rows, sum(len(x) + 1 for x in lines[:i])))
            i = j
        else:
            i += 1
    return out


def split_options(rng):
    """A stepped row's option list. ';' separates options that themselves hold commas (sub.order)."""
    if ";" in rng:
        return [o.strip() for o in rng.split(";")]
    return [o.strip() for o in rng.split(",")]


def int_options(lo, hi):
    """An integer range as options (1..64, -4..+4): signed labels when the range crosses zero."""
    signed = lo < 0
    return [(("+" if k > 0 and signed else "") + str(k)).replace("-", "−") for k in range(int(lo), int(hi) + 1)]


def parse_range(rng, law, blade):
    """-> dict(lo, hi, options, neg_inf). `blade` is 1 or 2 for 5.4's rows (their per-blade lists)."""
    r = rng.strip()
    if law == "step":
        m = re.match(r"^blade 1: (.*); blade 2: (.*)$", r)
        if m:                                            # 5.4 rotation: two lists
            return {"options": split_options(m.group(1) if blade == 1 else m.group(2))}
        m = re.match(r'^(.*); blade 2 adds "(.*)"$', r)
        if m:                                            # 5.1: blade 2's "as blade 1" first, at index 0
            base = split_options(m.group(1))
            return {"options": ([m.group(2)] + base) if blade == 2 else base}
        m = re.match(r"^(−?[\d ]+)\.\.([+−]?[\d ]+)$", r)
        if m:
            lo, hi = num(m.group(1)), num(m.group(2))
            n = int(hi - lo) + 1
            return {"lo": lo, "hi": hi, "n": n, "options": int_options(lo, hi) if n <= SELECT_MAX else None}
        return {"options": split_options(r)}
    neg_inf = False
    if r.startswith("−∞, "):
        neg_inf, r = True, r[4:]
    r = re.sub(r",\s*circular$", "", r)
    r = re.sub(r"\s*\(.*\)$", "", r)                     # "0..1 (a, e, i, o, u)"
    if ".." in r:
        a, b = r.split("..")
        return {"lo": num(a), "hi": num(b), "neg_inf": neg_inf}
    if is_num(r):                                        # a reserved row's only value (sampleStart: 0)
        return {"lo": num(r), "hi": num(r), "neg_inf": neg_inf}
    raise ValueError(f"range {rng!r}")


def parse_default(cell, blade, osc):
    """The default's text for this instance: 'a / b' per blade, '1234 (osc 2: 1235)' per oscillator."""
    d = cell.strip()
    if blade and " / " in d:
        d = d.split(" / ")[blade - 1].strip()
    m = re.match(r"^(.*?) \(osc 2: (.*)\)$", d)
    if m:
        d = m.group(2) if osc == 2 else m.group(1)
    return re.sub(r"\s*\((?:linear|a)\)$", "", d).strip()


def inverse(law, lo, hi, x, n=None):
    """v for value x under the law (5.1's table, inverted)."""
    if law in ("lin", "lin, wraps"):
        return 0.0 if hi == lo else (x - lo) / (hi - lo)
    if law == "log":
        return math.log(x / lo) / math.log(hi / lo)
    if law == "pow":
        return math.sqrt((x - lo) / (hi - lo))
    if law == "log, off zone":
        return 0.0 if x == 0 else 0.01 + 0.99 * math.log(x / 0.004) / math.log(hi / 0.004)
    if law == "dB":
        return 0.0 if x == -math.inf else (x - lo) / (hi - lo)
    raise ValueError(law)


def make_row(c, head, key, section_id, block, order, blade=0, osc=0):
    col = {h: i for i, h in enumerate(head)}
    what = c[col["what it does to the sound"] if "what it does to the sound" in col else col["what it does"]]
    law = re.sub(r"\s*\(.*\)$", "", c[col["law"]]).strip()
    if law not in LAWS:
        raise ValueError(f"{key}: law {c[col['law']]!r}")
    klass = c[col["class"]]
    cls = klass[0]
    if cls not in "MSD":
        raise ValueError(f"{key}: class {klass!r}")
    unit = re.sub(r"\s*\(.*\)$", "", c[col["unit"]]).strip()
    rng = parse_range(c[col["range"]], law, blade)
    dtxt = parse_default(c[col[next(h for h in head if h.startswith("default"))]], blade, osc)
    row = {"key": key, "label": c[col["label"]], "unit": "" if unit == "—" else unit, "law": law,
           "class": cls, "reserved": "reserved" in klass, "advanced": "advanced" in klass,
           "rate": c[col["rate"]], "pv": c[col["PV"]] == "yes", "section": section_id, "block": block,
           "order": order, "range_text": c[col["range"]], "default_text": dtxt,
           "spec": re.sub(r"\*\*", "", what).strip()}
    m = re.match(r"^As `([\w.]+)`", row["spec"])
    row["see"] = m.group(1) if m else None
    if law == "step":
        opts = rng["options"]
        if opts is None:                                  # a huge integer range (seed): no option list
            row.update(n=rng["n"], options=None, lo=rng["lo"], hi=rng["hi"])
            idx = int(num(dtxt) - rng["lo"])
        else:
            row.update(n=len(opts), options=opts)
            low = [re.sub(r"\s*\(.*\)$", "", o).lower() for o in opts]   # "Standard (1×)" is the default "Standard"
            if dtxt.lower() in low:
                idx = low.index(dtxt.lower())
            elif is_num(dtxt) and any(is_num(o) for o in opts):
                idx = [num(o) if is_num(o) else None for o in opts].index(num(dtxt))
            else:
                raise ValueError(f"{key}: default {dtxt!r} not in {opts}")
        row["default_index"] = idx
        row["default_v"] = 0.0 if row["n"] == 1 else idx / (row["n"] - 1)
    else:
        row.update(lo=rng["lo"], hi=rng["hi"], neg_inf=rng.get("neg_inf", False))
        if law == "dB" and rng["lo"] != -60:
            raise ValueError(f"{key}: a dB row's range starts at -60 (5.1), not {rng['lo']}")
        x = num(dtxt)
        row["default_x"] = x
        # The range test is exact and in x, the spec's own units, so it needs no tolerance; the inverse can land
        # an ulp past 1 at the top of a log range, so v is clamped after it (the seam check reads x(v) back).
        if not (min(rng["lo"], rng["hi"]) <= x <= max(rng["lo"], rng["hi"])) and not (law == "log, off zone" and x == 0):
            raise ValueError(f"{key}: default {dtxt!r} falls outside the range")
        row["default_v"] = 0.0 if row["reserved"] else min(1.0, max(0.0, inverse(law, rng["lo"], rng["hi"], x)))
    return row


def read_spec(text):
    """Section 5 expanded into the manifest's rows, in display order, and 5.8's counts."""
    rows, n_table_rows, per_table = [], 0, {}

    def add(c, head, key, sid, block, blade=0, osc=0):
        try:
            rows.append(make_row(c, head, key, sid, block, len(rows), blade, osc))
        except (ValueError, KeyError, IndexError) as e:
            raise Red("parse", f"5.{sid[-1]} `{key}`: {e}")

    def keys_of(cell):
        ks = re.findall(r"`([\w.]+)`", cell)
        if not ks:
            raise Red("parse", f"no key in {cell!r}")
        return ks

    s52 = section(text, "5.2 ")
    t = tables(s52)
    if len(t) != 2:
        raise Red("parse", f"5.2 has {len(t)} key tables, expected the voice's and ENV 2 to ENV 4's")
    (vh, vrows, _), (eh, erows, _) = t
    per_table["voice"], per_table["env"] = len(vrows), len(erows)
    for c in vrows:
        k = keys_of(c[0])[0]
        add(c, vh, k, "5.2", k.split(".")[0] if "." in k else k)
    m = re.search(r"Each source also has an on switch: (.*?): gate, class S, rate T\.", s52)
    if not m:
        raise Red("parse", "5.2: the on-switch sentence is gone")
    on_text = m.group(0)
    for k, d in re.findall(r"`(\w+\.on)` \((?:default )?(on|off)\)", m.group(1)):
        src = k.split(".")[0]
        rows.append({"key": k, "label": "On", "unit": "", "law": "step", "class": "S", "reserved": False,
                     "advanced": False, "rate": "T", "pv": False, "section": "5.2", "block": src,
                     "order": len(rows), "range_text": "off, on", "default_text": d,
                     "spec": on_text.split(": gate")[0].replace("Each source also has an on switch: ", "The source's on switch (5.2): ")
                     + ". A source that is off costs nothing (PF-6).", "see": None,
                     "n": 2, "options": ["off", "on"], "default_index": ["off", "on"].index(d),
                     "default_v": float(["off", "on"].index(d))})
    for e in ("env2", "env3", "env4"):
        for c in erows:
            add(c, eh, f"{e}.{keys_of(c[0])[0]}", "5.2", e)
    n_table_rows += len(vrows) + len(erows)

    s53, s54 = section(text, "5.3 "), section(text, "5.4 ")
    (oh, orows, _), = tables(s53)
    (bh, brows, _), = tables(s54)
    per_table["osc"], per_table["blade"] = len(orows), len(brows)
    for osc in (1, 2):
        for c in orows:
            add(c, oh, f"osc{osc}.{keys_of(c[0])[0]}", "5.3", f"osc{osc}", osc=osc)
        for blade in (1, 2):
            for c in brows:
                if "(blade 2 only)" in c[0] and blade != 2:
                    continue
                add(c, bh, f"osc{osc}.blade{blade}.{keys_of(c[0])[0]}", "5.4", f"osc{osc}.blade{blade}", blade, osc)
    n_table_rows += len(orows) + len(brows)
    per_table["blade_only2"] = sum(1 for c in brows if "(blade 2 only)" in c[0])

    for sid, src in (("5.5 ", "sub"), ("5.6 ", "noise")):
        (h, rs, _), = tables(section(text, sid))
        per_table[src] = len(rs)
        n_table_rows += len(rs)
        for c in rs:
            ks, labels = keys_of(c[0]), [x.strip() for x in c[1].split(",")]
            for i, k in enumerate(ks):                   # `mute`, `solo` | Mute, Solo: two parameters, one row
                cc = list(c)
                if len(ks) > 1:
                    cc[1] = labels[i]
                add(cc, h, f"{src}.{k}", sid.strip(), src)

    s58 = section(text, "5.8 ")
    want = {
        "table_rows": int(re.search(r"\*\*(\d+) table rows\*\*", s58).group(1)),
        "manifest": int(re.search(r"= \*\*(\d+)\*\*", s58).group(1)),
        "host": int(re.search(r"\*\*(\d+) host parameters\*\*", s58).group(1)),
        "voice": int(re.search(r"voice (\d+) \(plus", s58).group(1)),
        "on": int(re.search(r"\(plus (\d+) on switches\)", s58).group(1)),
        "env": int(re.search(r"ENV 2 to ENV 4 (\d+)", s58).group(1)),
        "osc": int(re.search(r"oscillator (\d+) \(", s58).group(1)),
        "blade": int(re.search(r"blade (\d+) \(plus", s58).group(1)),
        "blade_only2": int(re.search(r"blade (?:\d+) \(plus (\d+) blade-2-only\)", s58).group(1)),
        "sub": int(re.search(r"sub (\d+),", s58).group(1)),
        "noise": int(re.search(r"noise (\d+):", s58).group(1)),
    }
    have = {
        "table_rows": n_table_rows, "manifest": len(rows),
        "host": sum(1 for r in rows if not r["reserved"]),
        "voice": per_table["voice"], "on": sum(1 for r in rows if r["key"].endswith(".on") and r["section"] == "5.2"),
        "env": per_table["env"], "osc": per_table["osc"],
        "blade": per_table["blade"] - per_table["blade_only2"], "blade_only2": per_table["blade_only2"],
        "sub": per_table["sub"], "noise": per_table["noise"],
    }
    bad = {k: (have[k], want[k]) for k in want if have[k] != want[k]}
    if bad:
        raise Red("counts", "the expansion disagrees with 5.8: " + ", ".join(f"{k} {a} != {b}" for k, (a, b) in bad.items()))
    if len({r["key"] for r in rows}) != len(rows):
        raise Red("parse", "a key appears twice in the expansion")
    return rows, have


# ---------------------------------------------------------------- the layout -> the control list

def default_widget(r):
    """THE WIDGET RULE, the design system's (ADR-319 item 5): on/off is a toggle, up to 5 choices are buttons
    (pills), 6 to 64 a drop-down (select), more a stepper, and a continuous value a knob or a slider."""
    if r["law"] != "step":
        return "knob"
    opts = r["options"]
    if opts is not None and sorted(o.lower() for o in opts) == ["off", "on"]:
        return "toggle"
    if opts is None or r["n"] > SELECT_MAX:
        return "stepper"
    return "pills" if r["n"] <= 5 else "select"


def widget_fits(r, w):
    step = r["law"] == "step"
    if w in ("knob", "hslider", "vslider"):
        return not step
    if w == "toggle":
        return step and r["options"] is not None and sorted(o.lower() for o in r["options"]) == ["off", "on"]
    if w == "pills":
        return step and r["options"] is not None and r["n"] <= PILLS_MAX
    if w == "select":
        return step and r["options"] is not None and r["n"] <= SELECT_MAX
    return step                                           # stepper


NAV_OUT = [None]   # generate() leaves the checked nav here for render(); one run, one layout
SIZES = ("primary", "standard", "compact", "utility")   # B589's size ladder (Knob and Slider `size`)
ROLES = {"power": "toggle", "on": "toggle", "mute": "toggle", "solo": "toggle", "modes": "pills", "fader": "vslider"}


def is_switch(r):
    return r["law"] == "step" and r.get("options") is not None and sorted(o.lower() for o in r["options"]) == ["off", "on"]


def check_role(r, role, where):
    """A role puts a row in a composite (a Section header's on toggle or mode pills; a channel strip's fader and
    switches); the role fixes its widget, so the widget rule's override check does not apply. The row must suit it."""
    if role not in ROLES:
        raise Red("role", f"{where}: role {role!r} is not one of {sorted(ROLES)}")
    if ROLES[role] == "toggle" and not is_switch(r):
        raise Red("role", f"{where}: a {role} role needs an off/on row")
    if role == "modes" and not (r["law"] == "step" and r.get("options") and r["n"] <= PILLS_MAX):
        raise Red("role", f"{where}: the modes role needs a stepped row of at most {PILLS_MAX} options")
    if role == "fader" and r["law"] == "step":
        raise Red("role", f"{where}: a fader needs a continuous row")
    return ROLES[role]


def check_composites(pages, by):
    """The dashboard's cards (B589): each names its `source` page, carries that source's power switch and at least one
    control, and every control on it is a key that source's own page carries (one model, two views). The console's
    strips: one fader each; the master has no on, mute or solo. A section carries at most one power and one modes."""
    ids = {p["id"]: p for p in pages}
    for pg in pages:
        check_viz(pg.get("viz"), by, f"page {pg['id']}")
        if pg.get("power") and not (pg["power"] in by and is_switch(by[pg["power"]])):
            raise Red("role", f"page {pg['id']}: its power {pg['power']!r} is not an off/on row")
        for g in pg["groups"]:
            check_viz(g.get("viz"), by, f"page {pg['id']} group {g['title']}")
            roles = [c["role"] for c in g["controls"] if c["role"]]
            for one in ("power", "modes", "fader", "on", "mute", "solo"):
                if roles.count(one) > 1:
                    raise Red("role", f"page {pg['id']} group {g['title']}: two {one} roles")
            if pg.get("panel") == "dashboard":
                src = ids.get(g.get("source"))
                if not src:
                    raise Red("dashboard", f"dashboard card {g['title']}: source {g.get('source')!r} is not a page")
                own = {c["key"] for gg in src["groups"] for c in gg["controls"]}
                if "power" not in roles or len(g["controls"]) < 2:
                    raise Red("dashboard", f"dashboard card {g['title']}: a card carries its source's power switch and at least one control")
                stray = [c["key"] for c in g["controls"] if c["key"] not in own]
                if stray:
                    raise Red("dashboard", f"dashboard card {g['title']}: {stray} are not on {src['label']}'s own page (one model, two views)")
            if g.get("strip"):
                if roles.count("fader") != 1:
                    raise Red("role", f"strip {g['title']}: a strip has exactly one fader")
                if g.get("master") and any(r in roles for r in ("on", "mute", "solo")):
                    raise Red("role", f"strip {g['title']}: the master has no on, mute or solo")


VIZ_KINDS = ("osc", "sub", "noise", "env")


def check_viz(v, by, where):
    """A page's or card's picture (B589: visuals take the area): its kind and the keys it reads, all real rows."""
    if v is None:
        return
    if not isinstance(v, dict) or v.get("kind") not in VIZ_KINDS or not isinstance(v.get("keys"), dict):
        raise Red("role", f"{where}: a viz is {{kind: one of {VIZ_KINDS}, keys: {{...}}}}")
    for name, k in v["keys"].items():
        if k not in by or by[k]["reserved"]:
            raise Red("unknown-key", f"{where}: viz key {name} = {k!r} is not a parameter")


def check_nav(layout, pages):
    """The two-level page bar (B589 Page nav): every page sits in exactly one group, and every group's page exists."""
    nav = layout.get("nav")
    if not nav:
        raise Red("nav", "layout.json has no `nav` (B589's page bar)")
    ids = [p["id"] for p in pages]
    listed = [pid for grp in nav for pid in grp["pages"]]
    missing = [pid for pid in ids if listed.count(pid) != 1]
    unknown = [pid for pid in listed if pid not in ids]
    if missing or unknown:
        raise Red("nav", f"nav: pages in no group or in two {missing}; pages that do not exist {unknown}")
    return nav


def check_live(cond, by, where):
    if not isinstance(cond, dict):
        raise Red("live", f"{where}: a condition is an object")
    if "all" in cond or "any" in cond:
        for c in cond.get("all", []) + cond.get("any", []):
            check_live(c, by, where)
        return
    k = cond.get("key")
    if k not in by:
        raise Red("live", f"{where}: condition on unknown key {k!r}")
    r = by[k]
    if "gt" in cond:
        if r["law"] == "step" or not isinstance(cond["gt"], (int, float)):
            raise Red("live", f"{where}: `gt` needs a continuous key and a number ({k})")
        return
    opts = cond.get("is", cond.get("not"))
    if r["law"] != "step" or not isinstance(opts, list) or not opts:
        raise Red("live", f"{where}: `is`/`not` needs a stepped key and a list of its options ({k})")
    for o in opts:
        if o not in (r["options"] or []):
            raise Red("live", f"{where}: {k} has no option {o!r} (it has {r['options']})")


def build_pages(rows, layout):
    by = {r["key"]: r for r in rows}
    if layout.get("format") != "horde.gui3-layout/1":
        raise Red("parse", "layout.json: format is not horde.gui3-layout/1")
    pages, placed = [], set()
    for pg in layout["pages"]:
        seen, groups, follow_seen = set(), [], {}
        for gi, g in enumerate(pg["groups"]):
            entries = []
            if "block" in g:
                blk = [r for r in rows if r["block"] == g["block"]]
                if not blk:
                    raise Red("unknown-key", f"page {pg['id']}: no block {g['block']!r} in the spec")
                meta = g.get("meta") or {}
                if meta:
                    # A dense section lists its block's chosen rows first, by suffix, in `meta`'s order; the block's other
                    # rows follow, under the collapsed Advanced group when `rest` says so. Every row stays on the page.
                    names = {r["key"] for r in blk}
                    elsewhere = set()
                    for suf, m in meta.items():
                        k = f"{g['block']}.{suf}"
                        if k not in names:
                            raise Red("unknown-key", f"page {pg['id']}: {k!r} (meta) is not a row of block {g['block']}")
                        if m.get("elsewhere"):
                            elsewhere.add(k)          # placed by another group of this page (an envelope section's selector)
                        else:
                            entries.append(dict(m, key=k))
                    picked = {e["key"] for e in entries} | elsewhere
                    adv = g.get("rest") == "advanced"
                    tail = [dict({"key": r["key"]}, **({"advanced": True} if adv else {})) for r in blk if not r["reserved"] and r["key"] not in picked]
                else:
                    entries = [{"key": r["key"]} for r in blk if not r["reserved"]]
                    tail = []
            else:
                tail = []
            for e in g.get("keys", []):
                entries.append(e if isinstance(e, dict) else {"key": e})
            entries += tail
            follow_out = None
            if g.get("follow"):
                # ADR-333: the envelope a source's level follows (RC-6.5). Every block's suffix rows are placed, as the
                # shared keys they are; the shell shows the selected block's and draws its `viz` rows.
                fo = g["follow"]
                sel = by.get(fo.get("selector"))
                if not sel or sel["law"] != "step" or not sel.get("options"):
                    raise Red("unknown-key", f"page {pg['id']} group {g['title']}: follow selector {fo.get('selector')!r} is not a stepped row")
                if sorted(fo["blocks"]) != sorted(sel["options"]):
                    raise Red("role", f"page {pg['id']} group {g['title']}: follow blocks {sorted(fo['blocks'])} are not the selector's options {sel['options']}")
                follow_out = {"selector": sel["key"], "options": {}}
                for opt in sel["options"]:
                    blk = fo["blocks"][opt]
                    for suf in fo["suffixes"]:
                        k = f"{blk}.{suf}"
                        if k not in by:
                            raise Red("unknown-key", f"page {pg['id']} group {g['title']}: follow row {k!r} is not a parameter")
                        entries.append({"key": k, "follow": opt, "size": fo.get("size"), "sub": "Stages"})
                    vz = {}
                    for name, suf in (fo.get("viz") or {}).items():
                        k = f"{blk}.{suf}"
                        if k not in by:
                            raise Red("unknown-key", f"page {pg['id']} group {g['title']}: follow viz row {k!r} is not a parameter")
                        vz[name] = k
                    follow_out["options"][opt] = {"block": blk, "viz": vz}
            if "live" in g:
                check_live(g["live"], by, f"page {pg['id']} group {g['title']}")
            controls = []
            for e in entries:
                k = e["key"]
                if k not in by:
                    raise Red("unknown-key", f"page {pg['id']}: {k!r} is not a parameter of SPEC-ENGINE 5")
                r = by[k]
                if r["reserved"]:
                    raise Red("reserved-placed", f"page {pg['id']}: {k} is reserved (5.1): not a host parameter")
                # Two sources may follow the same envelope, so two dashboard cards may carry the same envelope row: a
                # followed row may repeat across groups of a page, never within one, and never beside a plain placement.
                if e.get("follow"):
                    if k in seen or follow_seen.get(k) == gi:
                        raise Red("duplicate", f"page {pg['id']}: {k} placed twice")
                    follow_seen[k] = gi
                elif k in seen or k in follow_seen:
                    raise Red("duplicate", f"page {pg['id']}: {k} placed twice")
                else:
                    seen.add(k)
                role = e.get("role")
                if role is not None:
                    w = check_role(r, role, f"page {pg['id']} {k}")
                    e = {kk: vv for kk, vv in e.items() if kk not in ("widget", "reason")}
                else:
                    w = e.get("widget") or default_widget(r)
                if e.get("size") not in (None,) + SIZES:
                    raise Red("widget", f"page {pg['id']}: {k} size {e.get('size')!r} is not one of {SIZES}")
                if w not in WIDGETS or not widget_fits(r, w):
                    raise Red("widget", f"page {pg['id']}: {k} cannot be a {w!r} ({r['law']}, {r.get('n')} options)")
                # The rule allows any knob or slider for a continuous row; anything else that differs from the
                # rule's choice is an override, and an override carries its reason (ADR-319 item 5). A reason on
                # an entry the rule already gives is refused too: it would claim an override that is not there.
                rule = default_widget(r)
                override = role is None and w != rule and not (r["law"] != "step" and w in ("knob", "hslider", "vslider"))
                reason = (e.get("reason") or "").strip()
                if override and not reason:
                    raise Red("override-reason", f"page {pg['id']}: {k} is a {w!r} where the widget rule gives {rule!r}: an override needs a `reason`")
                if reason and not override:
                    raise Red("override-reason", f"page {pg['id']}: {k} has a `reason` but no override (the rule already gives {w!r})")
                live = e.get("live")
                if live is not None:
                    check_live(live, by, f"page {pg['id']} {k}")
                if "live" in g:                           # a group's condition ANDs with the control's own
                    live = g["live"] if live is None else {"all": [g["live"], live]}
                controls.append({"key": k, "widget": w, "live": live, "override": reason or None, "role": role,
                                 "sub": e.get("sub"), "size": e.get("size"), "advanced": bool(e.get("advanced")),
                                 "follow": e.get("follow")})
                placed.add(k)
            if g.get("rule") not in (None, "solid", "dash", "dot", "none"):
                raise Red("role", f"page {pg['id']} group {g['title']}: rule {g.get('rule')!r} is not solid, dash, dot or none")
            groups.append({"title": g["title"], "tag": g.get("tag"), "screen": g.get("screen"), "controls": controls,
                           "num": g.get("num"), "rule": g.get("rule"), "source": g.get("source"), "viz": g.get("viz"),
                           "strip": g.get("strip"), "master": bool(g.get("master")), "follow": follow_out})
        pages.append({"id": pg["id"], "label": pg["label"], "status": pg["status"], "note": pg.get("note", ""),
                      "cite": pg.get("cite"), "panel": pg.get("panel"), "dense": bool(pg.get("dense")), "viz": pg.get("viz"),
                      "power": pg.get("power"), "groups": groups})
    check_composites(pages, by)
    unreached = [r["key"] for r in rows if not r["reserved"] and r["key"] not in placed]
    if unreached:
        raise Red("unreached", f"{len(unreached)} host parameter(s) on no page: {', '.join(unreached[:8])}"
                  + (" ..." if len(unreached) > 8 else ""))
    return pages


LEDGER = GUI3 / "port-tab.json"
LEDGER_STATUS = ("not-moved", "partial", "moved")


def check_ledger(led, exists):
    """GUI 3's port ledger (ADR-333, B593): one row per item not yet moved over. Each row names the item, where it lives
    today by repo path (a path that exists, or "nowhere yet"), a reason from the ledger's closed set with a sentence, its
    ROADMAP row (a B-number the ROADMAP has) and a status (`partial` says what is missing). `exists(path)` and the ROADMAP's
    rows are passed in, so the plants run on copies."""
    if not led or led.get("format") != "horde.gui3-port-ledger/1":
        raise Red("ledger", "docs/design/gui3/port-tab.json: format is not horde.gui3-port-ledger/1")
    reasons, ids = led.get("reasons") or {}, set()
    for i, r in enumerate(led.get("rows") or []):
        where = f"ledger row {r.get('id') or i}"
        if not r.get("id") or r["id"] in ids:
            raise Red("ledger", f"{where}: a row needs a unique id")
        ids.add(r["id"])
        if not (r.get("item") or "").strip():
            raise Red("ledger", f"{where}: no item")
        if not r.get("reason"):
            raise Red("ledger", f"{where}: no reason")
        if r["reason"] not in reasons:
            raise Red("ledger", f"{where}: reason {r['reason']!r} is not one of {sorted(reasons)}")
        if not (r.get("why") or "").strip():
            raise Red("ledger", f"{where}: a reason needs its sentence (`why`)")
        src = r.get("source") or []
        if not src:
            raise Red("ledger", f"{where}: no source path (where it lives today, or \"nowhere yet\")")
        for path in src:
            if path != "nowhere yet" and not exists(path.split("#")[0]):
                raise Red("ledger", f"{where}: source {path!r} does not exist in the repo")
        if not re.fullmatch(r"B\d+", r.get("roadmap") or ""):
            raise Red("ledger", f"{where}: no ROADMAP row (a B-number)")
        if r["roadmap"] not in exists.roadmap:
            raise Red("ledger", f"{where}: {r['roadmap']} is not a row of ROADMAP.md")
        if r.get("status") not in LEDGER_STATUS:
            raise Red("ledger", f"{where}: status {r.get('status')!r} is not one of {LEDGER_STATUS}")
        if r["status"] == "partial" and not (r.get("missing") or "").strip():
            raise Red("ledger", f"{where}: a partial row says what is missing")
    return led


def repo_exists():
    """The real repo: a path exists under the root; the ROADMAP's B-rows are read once."""
    def exists(path):
        return (ROOT / path).exists()
    road = (ROOT / "ROADMAP.md").read_text(encoding="utf-8") if (ROOT / "ROADMAP.md").exists() else ""
    exists.roadmap = set(re.findall(r"(?m)^\| (B\d+) \|", road))
    return exists


def build_bank(rows, layout):
    """THE BOTTOM MODULATOR BANK's critical controls (ADR-322 item 3, B556), generated from the table.

    layout.json's `bank.kinds` names, per modulator kind, a block pattern and the row suffixes that are the
    kind's critical controls, with the reason (the choice is design; the controls are the table's rows). For
    each spec block the pattern matches, every suffix must be a host row of that block, and each becomes a
    control by the widget rule. Today the only modulators with rows in SPEC-ENGINE 5 are ENV 1 to ENV 4
    (5.2); the LFOs and macros are deferred (RC-4) and the engine's other published sources (IF-3) have no
    rows, so their cards carry no controls, and say so."""
    by = {r["key"]: r for r in rows}
    blocks = sorted({r["block"] for r in rows})
    bank = layout.get("bank")
    if not bank:
        raise Red("bank", "layout.json has no `bank` (ADR-322 item 3)")
    out = {"note": bank.get("note", ""), "kinds": [], "controls": {}}
    for kd in bank["kinds"]:
        if not (kd.get("reason") or "").strip():
            raise Red("bank", f"bank kind {kd.get('kind')!r}: the choice of critical controls carries a `reason`")
        hit = [b for b in blocks if re.fullmatch(kd["blocks"], b)]
        if not hit:
            raise Red("bank", f"bank kind {kd['kind']!r}: no spec block matches {kd['blocks']!r}")
        for b in hit:
            ctls = []
            for suf in kd["suffixes"]:
                k = f"{b}.{suf}"
                if k not in by:
                    raise Red("unknown-key", f"bank kind {kd['kind']!r}: {k!r} is not a parameter of SPEC-ENGINE 5")
                if by[k]["reserved"]:
                    raise Red("reserved-placed", f"bank: {k} is reserved (5.1)")
                ctls.append({"key": k, "widget": default_widget(by[k]), "live": None, "override": None})
            out["controls"][b] = ctls
        out["kinds"].append({"kind": kd["kind"], "blocks": hit, "suffixes": kd["suffixes"], "reason": kd["reason"]})
    return out


# ---------------------------------------------------------------- the sources beside SPEC-ENGINE 5 (B584 slice 4a)
#
# ADR-330: the shell shows every parameter that has a machine-readable manifest today, not only the engine's.
#   - the morph's settings: SPEC-ENGINE Appendix D's D.2 table (keys as the spec names them, `morph.*`), which go to
#     the morph resolver (IF-12), never to the engine; every one is a host parameter except `morph.seed` and
#     `morph.audible`, which are page choices (IF-11, X39, X43); the step division's options are RC-7.4's table;
#   - Arp's settings (`arp.<key>`), the device's MIDI module, from its vendored params snapshot;
#   - the hosted FX modules' manifests (`fx.<module>.<key>`, a Bulwark face's as `fx.bulwark.<face>.<key>`), which go
#     to the rack (RC-5, IF-7). Each module's file is a verbatim, dated copy; its provenance row in
#     docs/design/gui3/README.md names the source path, the source commit and the bytes' sha256, and a copy whose row
#     is missing or whose hash differs is red.
# Every module row passes the same widget rule and refusals as an engine row. What a manifest does not declare is
# never filled in: a row with no law is shown linearly and marked `law_declared: false`; a row with no label shows
# its key and is marked `label_declared: false`; a row with no description says so in the info box.

README = GUI3 / "README.md"
SOURCE_FORMATS = ("shriek-lock", "bulwark-lock", "scape-lock", "echo-params", "arp-params")
CLASS_OF = {"continuous": "M", "morph": "M", "gate": "S", "jump": "S", "flip": "S", "discrete": "S", "device": "D"}


def provenance(readme_text):
    """The README's provenance table: | `file` | source | commit | sha256 | — the file relative to docs/design/gui3."""
    out = {}
    for m in re.finditer(r"(?m)^\| `([^`]+)` \| ([^|]+) \| `?([0-9a-f]{7,40})`? \| `?([0-9a-f]{64})`? \|", readme_text):
        out[m.group(1)] = {"source": m.group(2).strip(), "commit": m.group(3), "sha256": m.group(4)}
    return out


def d2_rows(spec_text):
    """Appendix D's D.2 table as rows (target: the resolver). RC-7.4's table gives the division's options."""
    body = section(spec_text, "D.2 ")
    lines = [l for l in body.split("\n") if l.startswith("| `morph.")]
    if not lines:
        raise Red("parse", "Appendix D.2 has no `morph.*` rows")
    m = re.search(r"\| index \| name \| kind \| length \|\n\s*\|---\|---\|---\|---\|\n((?:\s*\|[^\n]*\n)+)", spec_text)
    if not m:
        raise Red("parse", "RC-7.4's division table is gone")
    divisions = [cells(l)[1] for l in m.group(1).strip().split("\n")]
    nonhost = set(re.findall(r"`(morph\.\w+)`", re.search(r"\*\*Every setting but (.*?) is a host parameter\*\*", spec_text, re.S).group(1)))
    rows = []
    for i, l in enumerate(lines):
        key, label, rng, dflt, what = cells(l)
        key = key.strip("`")
        r = {"key": key, "label": label, "unit": "", "class": "D", "reserved": False, "advanced": False, "rate": "C",
             "pv": False, "section": "D.2", "block": "morph", "order": i, "range_text": rng, "default_text": dflt,
             "spec": re.sub(r"\*\*", "", what).strip(), "see": None, "target": "resolver", "host": key not in nonhost,
             "source": "SPEC-ENGINE Appendix D.2", "desc_source": "spec", "law_declared": True, "label_declared": True}
        mm = re.match(r"^(.*?) to (.*?)(?: (steps per second|s))?, (log|lin|pow|step)$", rng)
        if "index into RC-7.4" in rng:
            r.update(law="step", n=len(divisions), options=divisions)
            idx = int(re.match(r"(\d+)", dflt).group(1))
        elif mm and mm.group(4) == "step":
            lo, hi = num(mm.group(1).replace(",", "")), num(mm.group(2).replace(",", ""))
            n = int(hi - lo) + 1
            r.update(law="step", n=n, lo=lo, hi=hi, options=int_options(lo, hi) if n <= SELECT_MAX else None)
            idx = int(num(dflt) - lo)
        elif mm:
            lo, hi, law = num(mm.group(1)), num(mm.group(2)), mm.group(4)
            unit = {"steps per second": "/s", "s": "s"}.get(mm.group(3) or "", "")
            x = num(re.match(r"([\d.]+)", dflt).group(1))
            r.update(law=law, lo=lo, hi=hi, neg_inf=False, unit=unit, default_x=x,
                     default_v=min(1.0, max(0.0, inverse(law, lo, hi, x))))
            rows.append(r)
            continue
        else:
            opts = split_options(rng)
            r.update(law="step", n=len(opts), options=opts)
            idx = [o.lower() for o in opts].index(dflt.lower())
        r["default_index"] = idx
        r["default_v"] = 0.0 if r["n"] == 1 else idx / (r["n"] - 1)
        rows.append(r)
    want = {"morph.on", "morph.audible", "morph.seed"}
    if not want <= {r["key"] for r in rows} or not nonhost:
        raise Red("parse", "D.2 lost a row the shell relies on (morph.on, morph.audible, morph.seed) or its host sentence")
    return rows


def module_row(src, key, label, unit, law, cls, lo, hi, options, default, desc, order, block, values=None,
               law_declared=True, label_declared=True, reserved=False):
    """One row of a module's manifest in the shell's form. `options` (a list) makes it stepped; `values` are the
    stored values the options stand for when they are not 0..n-1 (Arp's enums, a labelled range)."""
    r = {"key": key, "label": label, "unit": unit or "", "class": cls, "reserved": reserved, "advanced": False,
         "rate": "C", "pv": False, "section": src["name"] + " manifest", "block": block, "order": order,
         "range_text": "", "default_text": str(default), "spec": desc or "", "see": None,
         "target": src["target"], "host": True, "source": src["name"], "desc_source": "manifest" if desc else "none",
         "law_declared": law_declared, "label_declared": label_declared}
    if reserved:
        r.update(law="step", n=1, options=["retired"], default_index=0, default_v=0.0, range_text="retired")
        return r
    if options is not None or law == "step":
        if options is None:
            n = int(hi - lo) + 1
            r.update(law="step", n=n, lo=lo, hi=hi, options=int_options(lo, hi) if n <= SELECT_MAX else None)
            idx = int(default - lo)
        else:
            r.update(law="step", n=len(options), options=list(options))
            vals = values if values is not None else list(range(len(options)))
            if default not in vals:
                raise Red("parse", f"{key}: default {default!r} is not one of {vals}")
            idx = vals.index(default)
        r["range_text"] = ", ".join(r["options"]) if r.get("options") else f"{lo:g}..{hi:g}"
        r["default_index"] = idx
        r["default_v"] = 0.0 if r["n"] == 1 else idx / (r["n"] - 1)
        return r
    if law == "log" and not (lo > 0 and hi > 0):
        raise Red("parse", f"{key}: a log law needs a positive range ({lo}..{hi})")
    x = float(default)
    if not (min(lo, hi) <= x <= max(lo, hi)):
        raise Red("parse", f"{key}: default {x} falls outside {lo}..{hi}")
    if law in ("io", "io-cut"):           # docs/proposals/module-io-gain.md: dB = 48·(v − ½); A1: the limiter's cut-only
        v = x / 48 + 0.5
    else:
        v = inverse(law, lo, hi, x)
    r.update(law=law, lo=lo, hi=hi, neg_inf=law == "io-cut", default_x=x, default_v=min(1.0, max(0.0, v)),
             range_text=f"{lo:g}..{hi:g}")
    return r


def read_module(src, text, extra_texts):
    """A source file's rows, by its declared format."""
    d = json.loads(text)
    fmt, mid, rows = src["format"], src["id"], []
    pre = "arp" if fmt == "arp-params" else "fx." + mid
    if fmt == "shriek-lock":
        for i, k in enumerate(d["keys"]):
            key = f"{pre}.{k['key']}"
            if k.get("retired"):
                rows.append(module_row(src, key, k["key"], "", "step", "S", 0, 0, None, 0, "", i, pre, reserved=True))
                continue
            stepped = k["type"] == "stepped"
            labels = k.get("labels") or None
            rows.append(module_row(src, key, k["name"], k["unit"], k["law"], CLASS_OF[k["morph"]], k["min"], k["max"],
                                   labels if stepped else None, k["default"], "", i, pre,
                                   values=list(range(int(k["min"]), int(k["max"]) + 1)) if stepped and labels else None))
    elif fmt == "bulwark-lock":
        for face in d["faces"]:
            block = f"{pre}.{face['face']}"
            for i, k in enumerate(face["keys"]):
                stepped = k["type"] == "stepped"
                labels = k.get("labels") or None
                vals = None
                if stepped and labels:
                    span = list(range(int(k["min"]), int(k["max"]) + 1))
                    vals = span if len(span) == len(labels) else [int(num(l)) for l in labels]
                rows.append(module_row(src, f"{block}.{k['key']}", k["name"], k["unit"], k["law"], CLASS_OF[k["morph"]],
                                       k["min"], k["max"], labels if stepped else None, k["default"], "", i, block, values=vals))
    elif fmt == "scape-lock":
        for i, k in enumerate(d["params"]):
            addr = k["address"]
            io = addr.startswith("io.")       # RS-4.6: every module's I/O gains follow the io law
            if k.get("values"):
                rows.append(module_row(src, f"{pre}.{addr}", addr.split(".")[-1], "", "step", CLASS_OF[k["class"]],
                                       k["min"], k["max"], k["values"], k["default"], "", i, pre,
                                       values=list(range(len(k["values"]))), label_declared=False))
            else:
                rows.append(module_row(src, f"{pre}.{addr}", addr.split(".")[-1], k["unit"], "io" if io else "lin",
                                       CLASS_OF[k["class"]], k["min"], k["max"], None, k["default"], "", i, pre,
                                       law_declared=io, label_declared=False))
    elif fmt == "echo-params":
        lock = json.loads(extra_texts[src["lock"]])
        if [p["key"] for p in lock["params"]] != [p["key"] for p in d["params"]]:
            raise Red("parse", "Echo's spec/params.json and its manifest lock list different keys")
        for i, k in enumerate(d["params"]):
            stepped = k["type"] == "stepped"
            labels = k.get("labels") or None
            rows.append(module_row(src, f"{pre}.{k['key']}", k["name"], k["unit"], k["law"], CLASS_OF[k["morph"]],
                                   k["min"], k["max"], labels if stepped else None, k["def"], "", i, pre,
                                   values=list(range(int(k["min"]), int(k["max"]) + 1)) if stepped and labels else None))
    elif fmt == "arp-params":
        bank = json.loads(extra_texts[src["bank"]])
        for i, k in enumerate(d["params"]):
            key, desc, cls = f"{pre}.{k['key']}", k.get("note", ""), CLASS_OF[k["kind"]]
            t = k["type"]
            if t == "bool":
                rows.append(module_row(src, key, k["label"], "", "step", cls, 0, 1, ["off", "on"], k["default"], desc, i, pre,
                                       values=[False, True]))
            elif t == "enum":
                if "values" in k:
                    vals = k["values"]
                elif k["key"] == "pattern":   # values_from: the bank's pattern ids, plus `user` (Arp SPEC §4.6)
                    vals = [p["id"] for p in bank["patterns"]] + ["user"]
                elif k["key"] == "rate":      # values_from: rate[].name
                    vals = [r["name"] for r in d["rate"]]
                else:
                    raise Red("parse", f"{key}: an enum with no values and no known values_from")
                labs = [k.get("value_labels", {}).get(v, v) for v in vals]
                rows.append(module_row(src, key, k["label"], "", "step", cls, 0, len(vals) - 1, labs, k["default"], desc, i, pre,
                                       values=vals))
            elif t == "uint32":
                rows.append(module_row(src, key, k["label"], "", "step", cls, 0, 4294967295, None, k["default"], desc, i, pre))
            elif cls == "S":
                rows.append(module_row(src, key, k["label"], k.get("unit", ""), "step", cls, k["min"], k["max"], None,
                                       k["default"], desc, i, pre))
            else:                             # a continuous integer: Arp declares no law, so it is shown linearly
                rows.append(module_row(src, key, k["label"], k.get("unit", ""), "lin", cls, k["min"], k["max"], None,
                                       k["default"], desc, i, pre, law_declared=False))
    else:
        raise Red("parse", f"source {mid}: unknown format {fmt!r}")
    return rows


def read_sources(spec_text, layout, ext):
    """Every row beside SPEC-ENGINE 5: D.2's, Arp's and the FX modules', each source's provenance checked first.
    `ext` holds the README's text and each vendored file's text by its path relative to docs/design/gui3."""
    import hashlib
    prov = provenance(ext["readme"])
    srcs = layout.get("sources") or []
    if not srcs:
        raise Red("provenance", "layout.json lists no `sources` (ADR-330)")
    rows = d2_rows(spec_text)
    for src in srcs:
        if src["format"] not in SOURCE_FORMATS:
            raise Red("parse", f"source {src['id']}: unknown format {src['format']!r}")
        files = [src["file"]] + [src[k] for k in ("lock", "bank") if k in src]
        for f in files:
            if f not in ext["files"]:
                raise Red("provenance", f"source {src['id']}: {f} is not vendored")
            row = prov.get(f)
            if not row:
                raise Red("provenance", f"{f} has no provenance row in docs/design/gui3/README.md (source path, commit, sha256)")
            h = hashlib.sha256(ext["files"][f].encode("utf-8")).hexdigest()
            if row["sha256"] != h:
                raise Red("provenance", f"{f}: its bytes hash {h[:12]}…, its provenance row says {row['sha256'][:12]}…")
        rows += read_module(src, ext["files"][src["file"]], ext["files"])
    keys = [r["key"] for r in rows]
    dup = sorted({k for k in keys if keys.count(k) > 1})
    if dup:
        raise Red("parse", f"two sources give the same key: {dup[:4]}")
    return rows


# ---------------------------------------------------------------- the shell and the scripts

KEY_LITERAL = re.compile(r"""['"`]((?:voice|env[1-4]|mix|out|osc[12]|sub|noise|morph|arp|fx)\.[A-Za-z0-9.]+|quality)['"`]""")
MOUNT_CALLS = re.compile(r"\bH\.(Knob|Slider)\s*\(")
CLOCK = re.compile(r"\b(Math\.random|Date\.now|new Date|performance\.now|crypto\.getRandomValues)\b")


def strip_comments(js):
    js = re.sub(r"/\*.*?\*/", "", js, flags=re.S)
    js = re.sub(r"<!--.*?-->", "", js, flags=re.S)
    return re.sub(r"(?m)(^|[^:'\"\\])//.*$", r"\1", js)


def check_shell(shell_text):
    """The shell mounts parameter controls from the generated list in ONE place, and names no key itself."""
    code = strip_comments(shell_text)
    lits = sorted(set(KEY_LITERAL.findall(code)))
    if lits:
        raise Red("shell", f"gui3.html names parameter key(s) itself: {', '.join(lits[:6])} (controls come from shell-data.js)")
    calls = MOUNT_CALLS.findall(code)
    if len(calls) != 2 or sorted(calls) != ["Knob", "Slider"]:
        raise Red("shell", f"gui3.html must call H.Knob and H.Slider once each, inside mountControl (found {calls})")
    if not re.search(r"\bDATA = window\.GUI3_DATA\b", code) or "DATA.pages.forEach(" not in code:
        raise Red("shell", "gui3.html does not build its pages from window.GUI3_DATA's pages")


def check_determinism(texts):
    for name, t in texts.items():
        hits = CLOCK.findall(strip_comments(t))
        if hits:
            raise Red("determinism", f"{name} reads {sorted(set(hits))}: GUI 3's stub and shell are seeded and clock-free")


# ---------------------------------------------------------------- outputs

def gaps(rows):
    # The `desc` column (B582) is the engine's; the other sources' descriptions are counted per source (src_counts).
    host = [r for r in rows if not r["reserved"] and r.get("target", "engine") == "engine"]
    return {"host": len(host), "desc_missing": len(host),
            "spec_text_missing": sum(1 for r in host if not r["spec"]),
            "note": "No `desc` column exists for horde 2's parameters yet (B582): the info box shows each "
                    "row's own SPEC-ENGINE text, marked as such, and every host parameter counts as a "
                    "missing description."}


def render(rows, pages, counts, bank, ledger=None):
    # params.json stays SPEC-ENGINE 5 alone (the engine's manifest as data); shell-data.js carries every source.
    doc = {"format": FORMAT, "source": "specs/SPEC-ENGINE.md section 5, read by tools/gen_gui3_controls.py",
           "counts": counts, "params": [r for r in rows if r.get("target", "engine") == "engine"]}
    j = json.dumps(doc, indent=2, ensure_ascii=False, sort_keys=True) + "\n"
    data = {"format": FORMAT, "counts": counts, "gaps": gaps(rows), "params": rows, "pages": pages, "bank": bank, "nav": NAV_OUT[0],
            "ledger": ledger}
    js = ("/* GENERATED by tools/gen_gui3_controls.py from specs/SPEC-ENGINE.md section 5 and\n"
          "   docs/design/gui3/layout.json. Do not edit: edit the spec or the layout, then regenerate. */\n"
          "window.GUI3_DATA = " + json.dumps(data, indent=1, ensure_ascii=False, sort_keys=True) + ";\n")
    return j, js


def generate(spec_text, layout, ext):
    rows, counts = read_spec(spec_text)
    for r in rows:
        r.update(target="engine", host=not r["reserved"], source="SPEC-ENGINE 5", desc_source="spec",
                 law_declared=True, label_declared=True)
    rows = rows + read_sources(spec_text, layout, ext)
    pages = build_pages(rows, layout)
    NAV_OUT[0] = check_nav(layout, pages)
    return rows, pages, counts, build_bank(rows, layout)


def gate(spec_text, layout, shell_text, scripts, committed, ext):
    rows, pages, counts, bank = generate(spec_text, layout, ext)
    led = check_ledger(ext.get("ledger"), ext["exists"])
    check_shell(shell_text)
    check_determinism(dict(scripts, **{"gui3.html": shell_text}))
    j, js = render(rows, pages, counts, bank, led)
    if committed.get("params.json") != j or committed.get("shell-data.js") != js:
        raise Red("stale", "docs/design/gui3/params.json or shell-data.js is not what the spec and the layout "
                           "generate: run python3 tools/gen_gui3_controls.py")
    return rows, pages, counts, bank


def controls(spec_text, layout, shell_text, scripts, committed, ext):
    """Each planted fault must come back red with its own tag. A control that passes is a blind gate."""
    def plant(name, tag, sp=spec_text, lo=None, sh=shell_text, sc=scripts, co=committed, ex=ext):
        lo = lo if lo is not None else layout
        try:
            gate(sp, lo, sh, sc, co, ex)
        except Red as e:
            return None if e.tag == tag else f"{name}: red as [{e.tag}], expected [{tag}]"
        return f"{name}: stayed GREEN (blind gate)"

    def lay(mut):
        lo = copy.deepcopy(layout)
        mut(lo)
        return lo

    page = lambda lo, pid: next(p for p in lo["pages"] if p["id"] == pid)

    def led_mut(fn):
        led = copy.deepcopy(ext["ledger"])
        fn(led["rows"][0])
        return dict(ext, ledger=led)
    sub_keys = lambda lo: next(g for g in page(lo, "sub")["groups"] if g["title"] == "Shape")["keys"]
    out = [
        plant("a control with no parameter", "unknown-key",
              lo=lay(lambda lo: page(lo, "settings")["groups"][0]["keys"].append("voice.notAParameter"))),
        plant("a host row on no page", "unreached",
              lo=lay(lambda lo: page(lo, "settings")["groups"][0]["keys"].remove("quality"))),
        plant("a reserved row placed", "reserved-placed",
              lo=lay(lambda lo: page(lo, "settings")["groups"][0]["keys"].append("sub.pitchMode"))),
        plant("a knob on a stepped row", "widget",
              lo=lay(lambda lo: sub_keys(lo).__setitem__(0, {"key": "sub.wave", "widget": "knob"}))),
        plant("an override without a reason", "override-reason",
              lo=lay(lambda lo: sub_keys(lo).__setitem__(0, {"key": "sub.wave", "widget": "pills"}))),
        plant("a reason on an entry the rule already gives", "override-reason",
              lo=lay(lambda lo: sub_keys(lo).__setitem__(1, {"key": "sub.pulseWidth", "widget": "knob", "reason": "none needed"}))),
        plant("a live condition on a missing option", "live",
              lo=lay(lambda lo: sub_keys(lo).__setitem__(1, {"key": "sub.pulseWidth", "live": {"key": "sub.wave", "is": ["noise"]}}))),
        plant("a bank control with no parameter", "unknown-key",
              lo=lay(lambda lo: lo["bank"]["kinds"][0]["suffixes"].append("notARow"))),
        plant("a bank choice with no reason", "bank",
              lo=lay(lambda lo: lo["bank"]["kinds"][0].__setitem__("reason", " "))),
        plant("a module's keys left unplaced", "unreached",
              lo=lay(lambda lo: page(lo, "fx")["groups"].pop(1))),
        plant("a vendored snapshot with no provenance row", "provenance",
              ex=dict(ext, readme=re.sub(r"(?m)^\| `vendor/scape[^\n]*\n", "", ext["readme"], count=1))),
        plant("a vendored snapshot whose bytes changed", "provenance",
              ex=dict(ext, files=dict(ext["files"], **{"vendor/echo.params.json": ext["files"]["vendor/echo.params.json"] + " "}))),
        plant("a ledger row with no reason", "ledger", ex=led_mut(lambda r: r.pop("reason"))),
        plant("a ledger row with an unknown reason", "ledger", ex=led_mut(lambda r: r.__setitem__("reason", "someday"))),
        plant("a ledger row with no source", "ledger", ex=led_mut(lambda r: r.__setitem__("source", []))),
        plant("a ledger row whose source does not exist", "ledger", ex=led_mut(lambda r: r.__setitem__("source", ["docs/design/no-such-lab.html"]))),
        plant("a ledger row with no B-number", "ledger", ex=led_mut(lambda r: r.__setitem__("roadmap", ""))),
        plant("a follow row that is not a parameter", "unknown-key",
              lo=lay(lambda lo: page(lo, "sub")["groups"][1]["follow"]["suffixes"].append("notARow"))),
        plant("a page in no nav group", "nav",
              lo=lay(lambda lo: next(g for g in lo["nav"] if "settings" in g["pages"])["pages"].remove("settings"))),
        plant("a dashboard control whose key is not on its source page", "dashboard",
              lo=lay(lambda lo: page(lo, "dashboard")["groups"][0]["keys"].append({"key": "noise.pitch", "size": "utility"}))),
        plant("a dashboard card without its power switch", "dashboard",
              lo=lay(lambda lo: page(lo, "dashboard")["groups"][2]["keys"].pop(0))),
        plant("a modes role on a continuous row", "role",
              lo=lay(lambda lo: page(lo, "main")["groups"][0]["keys"].__setitem__(1, {"key": "voice.gravity", "role": "modes"}))),
        plant("a spec row dropped", "counts",
              sp=re.sub(r"\n\| `gust` \|[^\n]*", "", spec_text, count=1)),
        plant("a hand-placed key in the shell", "shell",
              sh=shell_text.replace("</script>\n</body>", "const x = 'sub.drive';\n</script>\n</body>", 1)),
        plant("a second mount call in the shell", "shell",
              sh=shell_text.replace("</script>\n</body>", "H.Knob(document.body, {}, null);\n</script>\n</body>", 1)),
        plant("a clock read in the stub", "determinism",
              sc=dict(scripts, **{"engine-stub.js": scripts["engine-stub.js"] + "\nvar t = Date.now();\n"})),
        plant("a stale output", "stale",
              co=dict(committed, **{"shell-data.js": committed.get("shell-data.js", "") + " "})),
    ]
    # A plant that changed nothing tests nothing: each mutated input must differ from the real one.
    if "\n| `gust` |" not in spec_text or "</script>\n</body>" not in shell_text or not re.search(r"(?m)^\| `vendor/scape", ext["readme"]):
        out.append("a plant's anchor is missing from its input: the controls would be vacuous")
    return [o for o in out if o]


def src_counts(rows):
    """Rows per source beside the engine's, and how many carry a description, for the GREEN line."""
    out = []
    for name in dict.fromkeys(r["source"] for r in rows if r["target"] != "engine"):
        rs = [r for r in rows if r["source"] == name and not r["reserved"]]
        out.append(f"{name} {len(rs)} ({sum(1 for r in rs if r['spec'])} described)")
    return ", ".join(out)


def ledger_rows(ext):
    return (ext.get("ledger") or {}).get("rows") or []


def load_ext(layout):
    """The README and every source file a layout source names, as exact text (bytes decoded, newlines untouched,
    so the provenance hash is the file's own)."""
    files = {}
    for src in layout.get("sources") or []:
        for k in ("file", "lock", "bank"):
            if k in src:
                f = GUI3 / src[k]
                if f.exists():
                    files[src[k]] = f.read_bytes().decode("utf-8")
    return {"readme": README.read_bytes().decode("utf-8") if README.exists() else "", "files": files,
            "ledger": json.loads(LEDGER.read_text(encoding="utf-8")) if LEDGER.exists() else None, "exists": repo_exists()}


def main():
    spec_text = SPEC.read_text(encoding="utf-8")
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    ext = load_ext(layout)
    if "--check" not in sys.argv[1:]:
        try:
            rows, pages, counts, bank = generate(spec_text, layout, ext)
        except Red as e:
            print(f"gen_gui3_controls: RED {e}", file=sys.stderr)
            return 1
        try:
            led = check_ledger(ext.get("ledger"), ext["exists"])
        except Red as e:
            print(f"gen_gui3_controls: RED {e}", file=sys.stderr)
            return 1
        j, js = render(rows, pages, counts, bank, led)
        OUT_JSON.write_text(j, encoding="utf-8")
        OUT_JS.write_text(js, encoding="utf-8")
        print(f"gen_gui3_controls: wrote {OUT_JSON.relative_to(ROOT)} and {OUT_JS.relative_to(ROOT)} "
              f"({counts['manifest']} rows, {counts['host']} host parameters, {len(pages)} pages)")
        return 0
    shell_text = SHELL.read_text(encoding="utf-8") if SHELL.exists() else ""
    scripts = {n: (GUI3 / n).read_text(encoding="utf-8") for n in SCRIPTS}
    committed = {p.name: p.read_text(encoding="utf-8") for p in (OUT_JSON, OUT_JS) if p.exists()}
    try:
        rows, pages, counts, bank = gate(spec_text, layout, shell_text, scripts, committed, ext)
    except Red as e:
        print(f"gen_gui3_controls: RED {e}", file=sys.stderr)
        return 1
    bad = controls(spec_text, layout, shell_text, scripts, committed, ext)
    if bad:
        print("gen_gui3_controls: RED, a must-fail control did not fail:\n  " + "\n  ".join(bad), file=sys.stderr)
        return 1
    g = gaps(rows)
    per = ", ".join(f"{p['label']} {sum(len(gr['controls']) for gr in p['groups'])}" for p in pages)
    print(f"gen_gui3_controls: GREEN, {counts['manifest']} manifest rows = 5.8's, {counts['host']} host parameters "
          f"all reachable, {counts['manifest'] - counts['host']} reserved placed nowhere; controls per page: {per}; "
          f"descriptions missing {g['desc_missing']} of {g['host']} (no desc column yet, B582), spec text empty "
          f"{g['spec_text_missing']}; bank: {sum(len(c) for c in bank['controls'].values())} critical controls on "
          f"{len(bank['controls'])} modulators ({', '.join(bank['controls'])}); beside SPEC-ENGINE 5: "
          f"{src_counts(rows)}; port ledger {len(ledger_rows(ext))} rows; 27 must-fail controls red")
    return 0


if __name__ == "__main__":
    sys.exit(main())
