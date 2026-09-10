#!/usr/bin/env python3
"""gen_gui_controls — build gui2's missing controls FROM the presentation table.

This is the whole point of FOUNDATIONS' D1 ruling made operational: GUI structure
is DERIVED from declarations, not hand-placed. Adding a parameter becomes adding
a row, and a control can no longer be forgotten — the 29-dead-controls failure
and the 75-missing-controls gap are the same defect from opposite sides, and both
are unreachable once the markup is generated.

WHERE EACH FIELD COMES FROM, because the split is the design:

  * `src/param_presentation.tsv` — label, page, group, widget, unit. OURS.
    Address-keyed, no id column (their D1 caution: a page name that is also a
    dispatch fact re-fuses what the criterion separates).
  * `src/hypersaw_clap.cpp` — id, min, max, default, stepped, enum labels. The
    STRUCTURE half, which becomes ParamDesc at re-point.
  * `tools/gui_reach.py` — which ids are patch-scope and therefore need
    `data-fixed`. **Executed, not re-implemented.** That derivation has two
    semantic anchors and took three wrong versions to settle; a second copy here
    is how two copies disagree later. gui_reach owns it, we consume it.

The numeric id appears in the GENERATED markup because CLAP speaks ids — but it
enters at generation time, by joining an address-keyed table against the shell.
Nothing a human authors carries an id. That is the invariant worth protecting.

IDEMPOTENT: rewrites only between the GEN markers, so running it twice is a
no-op and hand-written controls are never touched.
"""
import contextlib
import io
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
GUI = ROOT / "src/gui/gui2.html"
TSV = ROOT / "src/param_presentation.tsv"
SHELL = ROOT / "src/hypersaw_clap.cpp"
NOSC = 2


def patch_scope_ids():
    """Consume gui_reach's derivation rather than copy it. It is an unguarded
    script: exec it in a private namespace and read the variable. A red run
    raises SystemExit, which we surface rather than swallow — generating markup
    from a shell whose own gate is failing would be building on sand."""
    ns = {"__file__": str((ROOT / "tools/gui_reach.py").resolve()), "__name__": "_gr"}
    src = (ROOT / "tools/gui_reach.py").read_text(encoding="utf-8")
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            exec(compile(src, "gui_reach.py", "exec"), ns)
    except SystemExit as e:
        # gui_reach RED is normally a real stop — but it is also the NORMAL state
        # while adding parameters, because reach cannot go green until the very
        # controls this script generates exist. Refusing here made the generator
        # unable to fix the only problem it exists to fix (hit adding the bend law
        # params, 2026-08-19). The patch-scope derivation it provides is valid
        # either way, so carry on and let ./verify judge the RESULT: if the
        # generated controls do not close the gap, gui_reach is still red
        # afterwards and nothing has been hidden.
        if e.code:
            print("gen_gui_controls: gui_reach is RED (expected while adding params) — "
                  "generating; ./verify judges the result", file=sys.stderr)
    return ns["patch_scope"]


def shell_params():
    src = SHELL.read_text(encoding="utf-8")
    decl = src.split("kParams[] = {", 1)[1].split("\n};", 1)[0]
    rows = re.findall(
        r'\{\s*(\d+),\s*"([A-Za-z0-9_]+)",\s*"([^"]*)",\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*(true|false),\s*([A-Za-z0-9_]+)\s*\}',
        decl)
    gl = src.split("kGlobalIds[] = {", 1)[1].split("};", 1)[0]
    globals_ = {int(x) for x in re.findall(r"\b(\d+)\b", re.sub(r"//.*", "", gl))}
    # ENUM LABELS. The regex has always captured the label-array name and thrown
    # it away, so every stepped param arrived here looking like a bare number and
    # the generator had no way to emit a <select>. The arrays exist in the shell
    # (kDistLabels, kTopoLabels, ... ) and the strings are the SAME ones CLAP
    # reports to the host, so resolving them here keeps one source: a label
    # retyped into the table would be a second copy free to drift.
    def enum_labels(arr):
        if arr in ("nullptr", "NULL", "0"):
            return None
        m = re.search(r'k%s\b[^=]*=\s*\{(.*?)\};' % re.escape(arr[1:]) if arr.startswith("k")
                      else r'\b%s\b[^=]*=\s*\{(.*?)\};' % re.escape(arr), src, re.S)
        if not m:
            return None
        return re.findall(r'"([^"]*)"', m.group(1)) or None

    by_addr = {}
    for sid, key, name, mn, mx, dv, stepped, labels in rows:
        i = int(sid)
        num = lambda t: float(eval(t.strip(), {"__builtins__": {}}, {}))
        base = dict(min=num(mn), max=num(mx), default=num(dv), stepped=stepped == "true",
                    enum=enum_labels(labels))
        if i in globals_:
            by_addr[key] = dict(base, id=i)
        else:
            for o in range(NOSC):
                by_addr[f"osc{o+1}.{key}"] = dict(base, id=i + o * 1000)
    return by_addr


def main():
    rows = [l.split("\t") for l in TSV.read_text(encoding="utf-8").splitlines()
            if l and not l.startswith("#")][1:]
    shell = shell_params()
    fixed = patch_scope_ids()
    gui = GUI.read_text(encoding="utf-8")
    # "Already placed" must mean HAND-placed, so the GEN blocks are stripped before
    # asking. Computing it from the whole file was a checks-that-cannot-fire bug in
    # this very gate: after one run every id is present, so every param is skipped,
    # nothing regenerates, and --check compares the file to itself and always
    # passes. It reported GREEN on a table edited underneath it. The filter that
    # keeps generation from fighting a human must not also blind it to itself.
    hand_written = re.sub(r"<!--GEN:\w+-->.*?<!--/GEN:\w+-->", "", gui, flags=re.S)
    already = {int(x) for x in re.findall(r'data-p="(\d+)"', hand_written)}

    per_page = {}
    for r in rows:
        addr, scope, label, page, group, widget = r[0], r[1], r[2], r[3], r[4], r[5]
        unit = r[6] if len(r) > 6 else ""
        p = shell.get(addr)
        if not p or p["id"] in already:
            continue        # hand-placed already; generation never fights a human
        # ONE CONTROL PER BASE ID — gui2 addresses oscillators with a SELECTOR, not
        # with duplicate rows. `effId()` remaps a control's base id to the edited
        # oscillator at send time, and `setControl()` paints exactly one element
        # per base (`[data-p="base"]:not([data-fixed])`).
        #
        # Generating an osc2 row alongside its osc1 twin therefore does not add a
        # control, it adds a BROKEN one: a row carrying data-p="1017" sends 1017
        # while OSC 1 is selected (right by luck) and effId remaps it to 2017 when
        # OSC 2 is selected — a parameter that does not exist. The knob does
        # nothing exactly when you would expect it to work.
        #
        # gui_reach could not catch this: it asks whether each BASE id appears in
        # the text, so a duplicate row and a mis-addressed send are both invisible
        # to it. The paired gate below is the one with teeth.
        if scope not in ("global", "osc1"):
            continue
        # UNDESIGNED ROWS DO NOT RENDER. An empty `chunk` means the four
        # decisions in the table's header have not been made for this row, and a
        # control generated before them is what produced a numeric-slider enum, a
        # duplicated panel, and a SPECTRA surface nobody asked for.
        if len(r) < 8 or not r[7].strip():
            continue
        # ADR-108: `depends` is the SOURCE and shown_when is DERIVED from it.
        # The two columns coexist for exactly one release -- long enough that
        # depends was seeded losslessly from the hand-written gates -- and the
        # generator reading depends first makes a divergence impossible rather
        # than merely discouraged. gen_gui_controls is the only writer of the
        # gate the GUI reads, so the graph and the visibility cannot drift.
        depends = r[10].strip() if len(r) > 10 else ""
        when = depends or (r[8].strip() if len(r) > 8 else "")
        scale = r[9].strip() if len(r) > 9 else ""
        per_page.setdefault(page, {}).setdefault(group, []).append((addr, scope, label, widget, unit, p, when, scale))

    total = 0
    # ITERATE THE MARKERS IN THE FILE, not the pages we have content for. Looping
    # over `per_page` meant a page with nothing to generate was never rewritten —
    # so "generate nothing" left the previous 75 controls in place, and --check
    # compared an unchanged in-memory copy against an unchanged file and reported
    # GREEN. A generator that cannot EMPTY a block cannot revert, and a drift gate
    # that only sees what was written cannot notice what should have been erased.
    for page in sorted(set(re.findall(r"<!--GEN:(\w+)-->", gui))):
        groups = per_page.get(page, {})
        marker = f"<!--GEN:{page}-->"
        end = f"<!--/GEN:{page}-->"
        out = []
        for group, items in sorted(groups.items()):
            # SAME ELEMENT as a hand-written panel, not a parallel one. `.grp`
            # had no CSS at all, so generated groups rendered as bare rows beside
            # boxed ones — the generated majority looked like it had escaped the
            # design. Styling `.grp` to match would have created two rules that
            # must agree forever; emitting `.cluster` means there is only one box
            # in this file and generated content cannot drift away from it.
            # A group may declare a VISUAL. Kept as a small map here rather than a
            # table column because it names a DRAW FUNCTION in the GUI, not a fact
            # about the parameter — presentation code, not presentation data. The
            # canvas is emitted inside the generated cluster so the picture sits
            # with the controls that change it.
            # A group may declare ONE OR MORE visuals. Bend needs two because the
            # bench proved one picture cannot say both things: the trajectory shows
            # where the laws differ, the vibrato cost shows what they charge for it,
            # and a trajectory plot hides the cost completely.
            VISUALS = {"Envelope": ["envelope"], "Onset & scatter": ["scatter"],
                       "Bend": ["bendstep", "bendvib"], "Saw shape": ["shapewave"]}
            out.append(f'  <div class="cluster"><h2>{group}</h2>')
            for viz in VISUALS.get(group, []):
                out.append(f'    <canvas class="gviz" data-viz="{viz}" width="260" height="72"></canvas>')
            for addr, scope, label, widget, unit, p, when, scale in sorted(items, key=lambda x: x[5]["id"]):
                df = ' data-fixed="1"' if p["id"] in fixed else ""
                step = "1" if p["stepped"] else "0.005"
                sfx = "" if scope == "global" else f' <span class="sc">{scope}</span>'
                u = f' <span class="u">{unit}</span>' if unit else ""
                # ONE CONTROL KIND PER PARAMETER TYPE, decided by the SHELL, not by
                # the table's `widget` hint: the shell owns whether a parameter is
                # stepped and what its values are called, and a hint that disagreed
                # would render a dropdown over a continuous range. The table still
                # decides page/group/label/unit — presentation — which is the split
                # FOUNDATIONS' D1 ruling draws.
                # An off/on pair is a BOOLEAN wearing enum labels — a checkbox says
                # that in one glance where a two-option dropdown makes you read.
                # Any other enum keeps its dropdown, because there the labels ARE
                # the meaning ("held note (legato)" vs "last note (memory)" is not
                # a thing a tick box can say). Matches the hand-placed retrig
                # control, which was right before this rule existed.
                boolish = [x.strip().lower() for x in (p.get("enum") or [])] in (["off", "on"],)
                if p.get("enum") and not boolish:
                    opts = "".join(
                        f'<option value="{p["min"] + k:g}"'
                        f'{" selected" if abs(p["min"] + k - p["default"]) < 1e-9 else ""}'
                        f'>{lab}</option>'
                        for k, lab in enumerate(p["enum"]))
                    ctrl = (f'<select data-p="{p["id"]}"{df}>{opts}</select>')
                elif boolish or (p["stepped"] and p["max"] - p["min"] == 1):
                    chk = " checked" if p["default"] >= 0.5 else ""
                    ctrl = (f'<input type="checkbox" data-p="{p["id"]}"{df}{chk}>')
                elif scale == "log10" and p["min"] > 0:
                    # The CONTROL is log10; the PARAMETER stays linear seconds. The
                    # GUI converts on send and on paint, so CLAP, the core and the
                    # host never see the log domain — same convention as gui.html.
                    import math as _m
                    lo, hi = _m.log10(p["min"]), _m.log10(p["max"])
                    dv = _m.log10(max(p["default"], p["min"]))
                    ctrl = (f'<input type="range" data-p="{p["id"]}"{df} data-log10="1"'
                            f' min="{lo:.4g}" max="{hi:.4g}" step="0.001" value="{dv:.4g}">')
                else:
                    ctrl = (f'<input type="range" data-p="{p["id"]}"{df}'
                            f' min="{p["min"]:g}" max="{p["max"]:g}"'
                            f' step="{step}" value="{p["default"]:g}">')
                # KNOB (ADR-120). The table's `widget` hint decides PRESENTATION
                # for a control whose KIND the shell already settled — which is
                # the same split the rule above draws, applied one level down: a
                # knob is still a continuous control over the same range, so
                # honouring the hint here cannot render a dropdown over a
                # continuum. The knob is a SKIN over the range input, not a
                # replacement: the input stays in the DOM as the value, the
                # focus target and the thing every existing handler already
                # wires, so gating, morph ownership, exempt marks, the context
                # menu and double-click reset all keep working untouched.
                knob = widget == "knob" and ctrl.startswith('<input type="range"')
                if knob:
                    # `kmod`/`kmnow` are the MODULATION ring and its live
                    # position (ADR-121). No modulator writes them yet, and with
                    # no depth declared the conic gradient has zero width and the
                    # tick is transparent -- so the knob is pixel-identical until
                    # something feeds it. The space is reserved now rather than
                    # when the first modulator lands, because a ring appearing
                    # later would resize every cell and undo the density pass.
                    ctrl = ('<span class="knob"><span class="kmod"></span>'
                            '<span class="karc"></span>'
                            '<span class="kcap"></span><span class="kptr"></span>'
                            + ctrl + '<span class="kmnow"></span></span>')
                # data-when carries the GATING declaration to the runtime: the base
                # key of the controlling param and the values under which this row
                # has any effect. A control the engine cannot read in the current
                # mode is furniture that lies, so it is hidden rather than greyed.
                dw = f' data-when="{when}"' if when else ""
                cls = "row kcell" if knob else "row"
                out.append(
                    f'    <div class="{cls}" data-addr="{addr}"{dw}><label>{label}{sfx}{u}</label>'
                    f'{ctrl}<output></output></div>')
                total += 1
            out.append("  </div>")
        block = marker + "\n" + "\n".join(out) + "\n  " + end
        gui = re.sub(re.escape(marker) + r".*?" + re.escape(end), lambda m: block, gui, flags=re.S)

    # ---- the invariant gui_reach cannot express -----------------------------
    # gui_reach asks "does base id N appear in the text?", which is satisfied by a
    # control that is duplicated, mis-addressed, or both. This asks the question
    # that actually matters for a selector-based GUI: does exactly ONE non-fixed
    # control claim each base id? Two claimants means setControl paints one and
    # ignores the other, and effId sends from the ignored one to a remapped id
    # that may not exist. Checked over the WHOLE file, hand-placed included,
    # because the collision does not care who wrote it.
    dup = {}
    for m2 in re.finditer(r'data-p="(\d+)"([^>]*)', gui):
        if "data-fixed" in m2.group(2):
            continue
        dup.setdefault(int(m2.group(1)) % 1000, []).append(int(m2.group(1)))
    clashes = {b: ids for b, ids in dup.items() if len(ids) > 1}
    if clashes:
        print("gen_gui_controls: FAILED — base id claimed by more than one non-fixed control.",
              file=sys.stderr)
        print("  A selector-based GUI addresses oscillators via effId(); duplicate rows",
              file=sys.stderr)
        print("  are not extra controls, they are mis-addressed ones.", file=sys.stderr)
        for b, ids in sorted(clashes.items())[:10]:
            print(f"    base {b}: claimed by data-p {sorted(ids)}", file=sys.stderr)
        return 1

    if "--check" in sys.argv:
        # DRIFT GATE. Editing the table without regenerating leaves gui2 showing
        # yesterday's declarations — a silent, invisible-to-every-oracle failure
        # of exactly the kind generation exists to abolish. So the check is that
        # regenerating changes nothing.
        if gui != GUI.read_text(encoding="utf-8"):
            print("gen_gui_controls: FAILED — generated controls are stale.\n"
                  "  src/param_presentation.tsv changed without regenerating.\n"
                  "  Run: python3 tools/gen_gui_controls.py", file=sys.stderr)
            return 1
        print(f"gen_gui_controls: GREEN ({total} generated control(s), gui2 markup current)")
        return 0
    GUI.open("w", encoding="utf-8", newline="\n").write(gui)
    print(f"gen_gui_controls: generated {total} control(s) across {len(per_page)} page(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
