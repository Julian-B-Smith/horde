#!/usr/bin/env python3
"""agent_manual_check -- the agents' operating reference agrees with the engine's table (B629).

WIRED: ./verify fast

WHY. docs/manual/agent/ is what an agent reads to dictate a patch. Its failures are silent: a recipe
whose patch the engine would refuse, a parameter page a spec round has left behind, a key named in
prose that the manifest never had, a row citation that points nowhere. An agent trusts each of them
and writes a wrong patch, and no audio oracle sees it.

WHAT IT CHECKS, on docs/manual/agent/:
  stale      parameters.md equals what tools/gen_agent_reference.py renders now (BD-5: generated,
             never hand-copied)
  patch      every ```json block in README.md and recipes.md is a horde-patch/1 that
             tools/patch_check.py accepts with no error and no inert warning
  recipe     every recipe starts from a bank preset and names it on its `Base:` line; every key its
             patch writes has a line "- `key` = value"; every such line's value equals the resolved
             patch's (the "From the base" lines included, so a claim about a preset is checked)
  key        every dotted key the hand-written pages name exists in params.json, with `oscN.`,
             `bladeB.` and `envE.` expanded to every copy
  cite       every SPEC-ENGINE row the hand-written pages cite (SW-4, BL-12, C.9.2, RC-6 ...) exists

MUST-FAIL CONTROLS (LIBRARY L0032), every run, on planted copies in a temp dir, never the repo's
files: a recipe patch with an unknown key, a recipe patch with a value out of range, a stale
parameters.md, a page naming a key that does not exist, a citation of a row that does not exist, and
a recipe line whose value disagrees with its patch. Each must read red with its own words; the
untouched copy must read green. Any control that goes the wrong way makes this check red.
"""
import json
import math
import pathlib
import re
import shutil
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_agent_reference as gen  # after the path insert above: tools/ is not a package
import patch_check as pc

AGENT = ROOT / "docs/manual/agent"
HAND = ("README.md", "concepts.md", "recipes.md")       # parameters.md is generated, checked by `stale`
JSON_BLOCK = re.compile(r"```json\n(.*?)```", re.S)
LINE = re.compile(r"^- `([^`]+)` = (.+?): ", re.M)
KEY_TOKEN = re.compile(r"`((?:voice|env[1-4E]|mix|out|osc[12N]|sub|noise)\.[A-Za-z0-9.]*[A-Za-z0-9]|quality)`")
CITE = re.compile(r"\b(?:SW|BL|ED|VO|EN|PA|DT|RT|OU|SN|IF|RC|QS|PF|MO|SB|BD)-\d+[a-z]?\b|\bC\.\d+(?:\.\d+)?\b")


def expand(token):
    """`oscN.bladeB.cut` -> every concrete key it names."""
    out = [token]
    for ph, subs in (("oscN.", ("osc1.", "osc2.")), (".bladeB.", (".blade1.", ".blade2.")),
                     ("envE.", ("env2.", "env3.", "env4."))):
        out = [k.replace(ph, s) for k in out for s in subs] if any(ph in k for k in out) else out
    return out


def cited_exists(spec_text, cid):
    if cid.startswith("C."):
        return re.search(rf"^#{{3,4}} {re.escape(cid)}\b", spec_text, re.M) is not None
    return (re.search(rf"^\| {re.escape(cid)} \|", spec_text, re.M) is not None
            or re.search(rf"^#### {re.escape(cid)}\.", spec_text, re.M) is not None)


def same(row, written, resolved):
    """A recipe line's value against the resolved patch, through the row's own parsing; floats agree to
    the 6 decimals the pages print."""
    x, err, _ = pc.parse_value(row, written)
    return err is None and pc.show(x) == pc.show(resolved)


def check(agent_dir, rows, spec_text, bank_names):
    errors = []
    d = pathlib.Path(agent_dir)
    have = (d / "parameters.md").read_text(encoding="utf-8") if (d / "parameters.md").exists() else ""
    if have != gen.render(rows, spec_text):
        errors.append("stale: parameters.md differs from tools/gen_agent_reference.py's output; run it")
    pages = {}
    for name in HAND:
        p = d / name
        if not p.exists():
            errors.append(f"missing: {name}")
            continue
        pages[name] = p.read_text(encoding="utf-8")
    for name in ("README.md", "recipes.md"):
        for block in JSON_BLOCK.findall(pages.get(name, "")):
            try:
                patch = json.loads(block)
            except ValueError as e:
                errors.append(f"patch: {name}: a json block does not parse ({e})")
                continue
            errs, warns, _, _ = pc.check_patch(patch, rows, spec_text)
            for e in errs:
                errors.append(f"patch: {name}: {e}")
            for w in warns:
                errors.append(f"patch: {name}: sets an inert row: {w}")
    sections = re.split(r"^## ", pages.get("recipes.md", ""), flags=re.M)[1:]
    n_recipes = 0
    for sec in sections:
        title = sec.splitlines()[0]
        blocks = JSON_BLOCK.findall(sec)
        if not blocks:
            continue
        n_recipes += 1
        try:
            patch = json.loads(blocks[0])
        except ValueError:
            continue                                          # reported above
        errs, _, V, _ = pc.check_patch(patch, rows, spec_text)
        if errs or V is None:
            continue                                          # reported above
        base = patch.get("base")
        if base not in bank_names:
            errors.append(f"recipe: '{title}': base {base!r} is not a bank preset (B629: each starts from one)")
        m = re.search(r"^Base: \*\*(.+?)\*\*", sec, re.M)
        if not m or m.group(1) != base:
            errors.append(f"recipe: '{title}': its `Base:` line does not name its patch's base {base!r}")
        lines = LINE.findall(sec)
        named = [k for k, _ in lines]
        for k in patch.get("values", {}):
            if k not in named:
                errors.append(f"recipe: '{title}': writes `{k}` with no line saying why")
        for k, lit in lines:
            if k not in rows:
                continue                                      # the key rule reports it
            try:
                written = json.loads(lit)
            except ValueError:
                errors.append(f"recipe: '{title}': `{k}` = {lit} is not a JSON value")
                continue
            if not same(rows[k], written, V[k]):
                errors.append(f"recipe: '{title}': `{k}` = {lit} disagrees with the resolved patch "
                              f"({pc.show(V[k])})")
    if "recipes.md" in pages and n_recipes == 0:
        errors.append("recipe: BLIND: recipes.md holds no recipe with a json patch")
    for name, text in pages.items():
        for tok in sorted(set(KEY_TOKEN.findall(text))):
            for k in expand(tok):
                if k not in rows:
                    errors.append(f"key: {name} names a key that does not exist: `{k}` (from `{tok}`)")
        for cid in sorted(set(CITE.findall(text))):
            if not cited_exists(spec_text, cid):
                errors.append(f"cite: {name} cites {cid}, which SPEC-ENGINE does not have")
    return errors, n_recipes


# ---- must-fail controls -------------------------------------------------------

def _plant(d, fault):
    rec = d / "recipes.md"
    text = rec.read_text(encoding="utf-8")
    first = JSON_BLOCK.search(text)
    patch = json.loads(first.group(1))
    if fault == "unknown key":
        patch["values"]["osc1.nonesuch"] = 1
    elif fault == "out of range":
        patch["values"]["osc1.detune"] = 7
    if fault in ("unknown key", "out of range"):
        rec.write_text(text[:first.start(1)] + json.dumps(patch) + "\n" + text[first.end(1):], encoding="utf-8")
    elif fault == "stale parameters.md":
        p = d / "parameters.md"
        p.write_text(p.read_text(encoding="utf-8") + "\n| `osc1.extra` | — | 0..1 | lin | 0 | planted | — | built |\n",
                     encoding="utf-8")
    elif fault == "missing key named":
        rec.write_text(text + "\nThen set `osc1.blade1.nonesuch` to taste.\n", encoding="utf-8")
    elif fault == "missing citation":
        c = d / "concepts.md"
        c.write_text(c.read_text(encoding="utf-8") + "\nMembers also hum (SW-99).\n", encoding="utf-8")
    elif fault == "line disagrees":
        k, lit = LINE.findall(text)[0]
        rec.write_text(text.replace(f"- `{k}` = {lit}: ", f"- `{k}` = 123456: ", 1), encoding="utf-8")


CONTROLS = (
    ("unknown key", "no such key in SPEC-ENGINE 5"),
    ("out of range", "outside its range"),
    ("stale parameters.md", "stale: parameters.md differs"),
    ("missing key named", "names a key that does not exist: `osc1.blade1.nonesuch`"),
    ("missing citation", "cites SW-99"),
    ("line disagrees", "disagrees with the resolved patch"),
)


def controls(src, rows, spec_text, bank_names):
    bad = []
    with tempfile.TemporaryDirectory() as tmp:
        clean = pathlib.Path(tmp) / "clean"
        shutil.copytree(src, clean)
        errs, _ = check(clean, rows, spec_text, bank_names)
        if errs:
            bad.append(f"positive control: the untouched copy reads red ({errs[0]})")
        for fault, words in CONTROLS:
            d = pathlib.Path(tmp) / fault.replace(" ", "-")
            shutil.copytree(src, d)
            _plant(d, fault)
            errs, _ = check(d, rows, spec_text, bank_names)
            if not any(words in e for e in errs):
                bad.append(f"control '{fault}': did not read red with \"{words}\" "
                           f"(got {len(errs)} error(s){': ' + errs[0] if errs else ''})")
    return bad


def main():
    if not AGENT.is_dir():
        print("agent_manual_check: RED — docs/manual/agent is missing", file=sys.stderr)
        return 1
    rows = pc.load_rows()
    spec_text = pc.SPEC.read_text(encoding="utf-8")
    bank_names = set(pc.bank_presets())
    errors, n_recipes = check(AGENT, rows, spec_text, bank_names)
    bad = controls(AGENT, rows, spec_text, bank_names)
    if errors or bad:
        for e in errors + bad:
            print(f"agent_manual_check: {e}", file=sys.stderr)
        print(f"agent_manual_check: RED — {len(errors)} fault(s), {len(bad)} control failure(s)", file=sys.stderr)
        return 1
    print(f"agent_manual_check: GREEN — parameters.md current, {n_recipes} recipes valid and their lines "
          f"match, every named key and cited row exists; {len(CONTROLS)} must-fail controls red + "
          f"positive control green")
    return 0


if __name__ == "__main__":
    sys.exit(main())
