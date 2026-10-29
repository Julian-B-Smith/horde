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
  stale           the committed outputs equal what this run generates
MUST-FAIL CONTROLS, every run, on in-memory copies: an unknown key, an unplaced host row, a placed
reserved row, a knob on a stepped row, an override with no reason, a reason with no override, a live condition on a missing option, a dropped spec row, a
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
SCRIPTS = ["engine-stub.js", "device.js"]   # the gui3 scripts the determinism gate reads, with the shell

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
        seen, groups = set(), []
        for g in pg["groups"]:
            entries = []
            if "block" in g:
                blk = [r for r in rows if r["block"] == g["block"]]
                if not blk:
                    raise Red("unknown-key", f"page {pg['id']}: no block {g['block']!r} in the spec")
                entries = [{"key": r["key"]} for r in blk if not r["reserved"]]
            for e in g.get("keys", []):
                entries.append(e if isinstance(e, dict) else {"key": e})
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
                if k in seen:
                    raise Red("duplicate", f"page {pg['id']}: {k} placed twice")
                seen.add(k)
                w = e.get("widget") or default_widget(r)
                if w not in WIDGETS or not widget_fits(r, w):
                    raise Red("widget", f"page {pg['id']}: {k} cannot be a {w!r} ({r['law']}, {r.get('n')} options)")
                # The rule allows any knob or slider for a continuous row; anything else that differs from the
                # rule's choice is an override, and an override carries its reason (ADR-319 item 5). A reason on
                # an entry the rule already gives is refused too: it would claim an override that is not there.
                rule = default_widget(r)
                override = w != rule and not (r["law"] != "step" and w in ("knob", "hslider", "vslider"))
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
                controls.append({"key": k, "widget": w, "live": live, "override": reason or None})
                placed.add(k)
            groups.append({"title": g["title"], "tag": g.get("tag"), "controls": controls})
        pages.append({"id": pg["id"], "label": pg["label"], "status": pg["status"], "note": pg.get("note", ""),
                      "cite": pg.get("cite"), "panel": pg.get("panel"), "groups": groups})
    unreached = [r["key"] for r in rows if not r["reserved"] and r["key"] not in placed]
    if unreached:
        raise Red("unreached", f"{len(unreached)} host parameter(s) on no page: {', '.join(unreached[:8])}"
                  + (" ..." if len(unreached) > 8 else ""))
    return pages


# ---------------------------------------------------------------- the shell and the scripts

KEY_LITERAL = re.compile(r"""['"`]((?:voice|env[1-4]|mix|out|osc[12]|sub|noise)\.[A-Za-z0-9.]+|quality)['"`]""")
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
    host = [r for r in rows if not r["reserved"]]
    return {"host": len(host), "desc_missing": len(host),
            "spec_text_missing": sum(1 for r in host if not r["spec"]),
            "note": "No `desc` column exists for horde 2's parameters yet (B582): the info box shows each "
                    "row's own SPEC-ENGINE text, marked as such, and every host parameter counts as a "
                    "missing description."}


def render(rows, pages, counts):
    doc = {"format": FORMAT, "source": "specs/SPEC-ENGINE.md section 5, read by tools/gen_gui3_controls.py",
           "counts": counts, "params": rows}
    j = json.dumps(doc, indent=2, ensure_ascii=False, sort_keys=True) + "\n"
    data = {"format": FORMAT, "counts": counts, "gaps": gaps(rows), "params": rows, "pages": pages}
    js = ("/* GENERATED by tools/gen_gui3_controls.py from specs/SPEC-ENGINE.md section 5 and\n"
          "   docs/design/gui3/layout.json. Do not edit: edit the spec or the layout, then regenerate. */\n"
          "window.GUI3_DATA = " + json.dumps(data, indent=1, ensure_ascii=False, sort_keys=True) + ";\n")
    return j, js


def generate(spec_text, layout):
    rows, counts = read_spec(spec_text)
    pages = build_pages(rows, layout)
    return rows, pages, counts


def gate(spec_text, layout, shell_text, scripts, committed):
    rows, pages, counts = generate(spec_text, layout)
    check_shell(shell_text)
    check_determinism(dict(scripts, **{"gui3.html": shell_text}))
    j, js = render(rows, pages, counts)
    if committed.get("params.json") != j or committed.get("shell-data.js") != js:
        raise Red("stale", "docs/design/gui3/params.json or shell-data.js is not what the spec and the layout "
                           "generate: run python3 tools/gen_gui3_controls.py")
    return rows, pages, counts


def controls(spec_text, layout, shell_text, scripts, committed):
    """Each planted fault must come back red with its own tag. A control that passes is a blind gate."""
    def plant(name, tag, sp=spec_text, lo=None, sh=shell_text, sc=scripts, co=committed):
        lo = lo if lo is not None else layout
        try:
            gate(sp, lo, sh, sc, co)
        except Red as e:
            return None if e.tag == tag else f"{name}: red as [{e.tag}], expected [{tag}]"
        return f"{name}: stayed GREEN (blind gate)"

    def lay(mut):
        lo = copy.deepcopy(layout)
        mut(lo)
        return lo

    page = lambda lo, pid: next(p for p in lo["pages"] if p["id"] == pid)
    sub_keys = lambda lo: page(lo, "sub")["groups"][1]["keys"]
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
    if "\n| `gust` |" not in spec_text or "</script>\n</body>" not in shell_text:
        out.append("a plant's anchor is missing from its input: the controls would be vacuous")
    return [o for o in out if o]


def main():
    spec_text = SPEC.read_text(encoding="utf-8")
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    if "--check" not in sys.argv[1:]:
        try:
            rows, pages, counts = generate(spec_text, layout)
        except Red as e:
            print(f"gen_gui3_controls: RED {e}", file=sys.stderr)
            return 1
        j, js = render(rows, pages, counts)
        OUT_JSON.write_text(j, encoding="utf-8")
        OUT_JS.write_text(js, encoding="utf-8")
        print(f"gen_gui3_controls: wrote {OUT_JSON.relative_to(ROOT)} and {OUT_JS.relative_to(ROOT)} "
              f"({counts['manifest']} rows, {counts['host']} host parameters, {len(pages)} pages)")
        return 0
    shell_text = SHELL.read_text(encoding="utf-8") if SHELL.exists() else ""
    scripts = {n: (GUI3 / n).read_text(encoding="utf-8") for n in SCRIPTS}
    committed = {p.name: p.read_text(encoding="utf-8") for p in (OUT_JSON, OUT_JS) if p.exists()}
    try:
        rows, pages, counts = gate(spec_text, layout, shell_text, scripts, committed)
    except Red as e:
        print(f"gen_gui3_controls: RED {e}", file=sys.stderr)
        return 1
    bad = controls(spec_text, layout, shell_text, scripts, committed)
    if bad:
        print("gen_gui3_controls: RED, a must-fail control did not fail:\n  " + "\n  ".join(bad), file=sys.stderr)
        return 1
    g = gaps(rows)
    per = ", ".join(f"{p['label']} {sum(len(gr['controls']) for gr in p['groups'])}" for p in pages)
    print(f"gen_gui3_controls: GREEN, {counts['manifest']} manifest rows = 5.8's, {counts['host']} host parameters "
          f"all reachable, {counts['manifest'] - counts['host']} reserved placed nowhere; controls per page: {per}; "
          f"descriptions missing {g['desc_missing']} of {g['host']} (no desc column yet, B582), spec text empty "
          f"{g['spec_text_missing']}; 12 must-fail controls red")
    return 0


if __name__ == "__main__":
    sys.exit(main())
