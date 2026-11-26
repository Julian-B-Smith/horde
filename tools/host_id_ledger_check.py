#!/usr/bin/env python3
"""host_id_ledger_check -- the rack's host ids are append-only (ROADMAP B714 acceptance (6); ADR-411 Q17 (b);
ADR-412 r2; RS-5.6).

WIRED: ./verify fast

  python3 tools/host_id_ledger_check.py

WHY. A host id is a promise to every DAW project that automated it: the number keeps its key forever. The test
plug-in's module keys get their ids from one ledger, tools/engine-preview/host_ids.lock.json (and the test module's
own block, tools/rack/test/host_ids.test.json); the rack reads them at build time and refuses a key the ledger lacks.
Nothing else stops a ledger edit from renumbering a key a saved project points at. This does.

ROWS (each red on its own):
  SHAPE     every id is hex, non-zero, unique across both ledgers, below its ledger's next_free; every entry names a
            module and either a key or the switch; aliases and legacy lines name ids the ledger holds.
  FROZEN    the pre-B714 numbering is held to its own transcription here (filter_core.h and reverb_core.h as they were):
            0xA00010 Weir's switch and 0xA00011-15 its keys in manifest order without the one-value type; 0xA00016
            Scape's switch and 0xA00017 + k Scape's manifest id k (FROZEN below). Renumbering any is red.
  APPEND    against the ledger at the merge base with origin/main (or HEAD's parent): every entry there is here,
            unchanged; next_free never goes down; an entry new since then takes an id at or above the base's next_free.
            A ledger the base lacks is new, and this row reports that it had nothing to compare.
  COMPLETE  where a module's pinned manifest is readable (its tree at its pin, `git show <pin>:<manifest>`, or the
            repo's own file), every address it declares, but a flip key with fewer than two values, has an id; so do
            its switch and, for a module that declares no io pair, the slot's io.inGain and io.outGain. A tree that is
            absent is printed SKIPPED for that module, never green.

MUST-FAIL CONTROLS, every run, on in-memory copies: a frozen id renumbered, a frozen entry removed, an id at next_free,
an id taken twice, a base entry changed, next_free lowered. Each must read red, or this check is red.
"""
import copy
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# The test plug-in's ledger, and any test module's own block beside it (tools/rack/test/host_ids*.json).
LEDGERS = ["tools/engine-preview/host_ids.lock.json"] + sorted(str(p.relative_to(ROOT)) for p in (ROOT / "tools/rack/test").glob("host_ids*.json"))
MODULES = "tools/rack/modules.json"

# The pre-B714 layout, transcribed from filter_core.h (kIdBase 0xA00010: on, mode, cutoff, resonance, ring, mix) and
# reverb_core.h (kOnId 0xA00016, kFirstParamId + Scape's manifest id) at 3159a196, with the manifest addresses the
# ledger names them by.
SCAPE_BY_ID = ["verb.mix", "verb.preDelay", "verb.erSend", "verb.erSize", "verb.erDecay", "verb.envelop", "verb.tailDecorr",
               "verb.mixTimeMs", "verb.diffuse", "verb.size", "verb.decay", "verb.damp", "verb.lowCut", "verb.modDepth", "verb.modRate",
               "verb.width", "verb.bassMono", "verb.inHP", "verb.inLP", "verb.spinDepth", "verb.spinRate", "verb.preGain", "verb.postGain",
               "io.outGain", "verb.era", "verb.delayMotion", "verb.glideTime", "io.inGain", "verb.image"]
FROZEN = [(0xA00010, "fxfilter", None)] + [(0xA00011 + k, "fxfilter", a) for k, a in enumerate(
    ["fxfilter.mode", "fxfilter.cutoff", "fxfilter.resonance", "fxfilter.morph", "fxfilter.mix"])] + \
    [(0xA00016, "scape", None)] + [(0xA00017 + k, "scape", a) for k, a in enumerate(SCAPE_BY_ID)]


def hexid(s):
    return int(s, 16) if isinstance(s, str) and re.fullmatch(r"0x[0-9A-Fa-f]+", s) else None


def entry_key(e):
    return (e.get("module"), None if e.get("switch") else e.get("key"))


def shape(docs):
    fails, seen = [], {}
    for name, d in docs.items():
        nf = hexid(d.get("next_free"))
        if nf is None:
            fails.append(f"SHAPE {name}: next_free is not hex")
            continue
        held = set()
        for e in d.get("ids", []):
            i = hexid(e.get("id"))
            if not i:
                fails.append(f"SHAPE {name}: id {e.get('id')!r} is not a non-zero hex number")
                continue
            if i in seen:
                fails.append(f"SHAPE {name}: id {e['id']} is taken twice (also in {seen[i]})")
            seen[i] = name
            held.add(i)
            if i >= nf:
                fails.append(f"SHAPE {name}: id {e['id']} is at or past next_free {d['next_free']}")
            if not e.get("module") or (not e.get("switch") and not e.get("key")):
                fails.append(f"SHAPE {name}: id {e['id']} names no module, or neither a key nor the switch")
        for l in d.get("legacy", []):
            for i in [hexid(x) for x in l.get("ids", [])] + [hexid(l.get("on_id")) if l.get("on_id") else None]:
                if i is not None and i not in held:
                    fails.append(f"SHAPE {name}: legacy line {l.get('prefix')!r} names id {i:#x}, which the ledger lacks")
            if l.get("id_base"):
                b = hexid(l["id_base"])
                for k in range(int(l.get("count", 0))):
                    if b + k not in held:
                        fails.append(f"SHAPE {name}: legacy line {l.get('prefix')!r} names id {b + k:#x}, which the ledger lacks")
                        break
    return fails


def frozen(docs):
    fails = []
    held = {}
    for d in docs.values():
        for e in d.get("ids", []):
            held[hexid(e.get("id"))] = entry_key(e)
    for i, module, key in FROZEN:
        if held.get(i) != (module, key):
            fails.append(f"FROZEN id {i:#x} must stay {module} {key or 'switch'}, the ledger has {held.get(i)}")
    return fails


def append_only(name, now, base):
    fails = []
    if base is None:
        return fails
    have = {hexid(e["id"]): entry_key(e) for e in now.get("ids", []) if hexid(e.get("id"))}
    for e in base.get("ids", []):
        i = hexid(e.get("id"))
        if have.get(i) != entry_key(e):
            fails.append(f"APPEND {name}: base entry {e.get('id')} {entry_key(e)} is renumbered or removed (now {have.get(i)})")
    bnf, nnf = hexid(base.get("next_free")), hexid(now.get("next_free"))
    if bnf is not None and nnf is not None:
        if nnf < bnf:
            fails.append(f"APPEND {name}: next_free went down, {base['next_free']} -> {now['next_free']}")
        old = {hexid(e["id"]) for e in base.get("ids", []) if hexid(e.get("id"))}
        for i in have:
            if i not in old and i < bnf:
                fails.append(f"APPEND {name}: new id {i:#x} is below the base's next_free {base['next_free']}")
    return fails


def git(*args):
    r = subprocess.run(["git", "-C", str(ROOT)] + list(args), capture_output=True, text=True)
    return r.stdout if r.returncode == 0 else None


def base_ledger(rel):
    base = (git("merge-base", "HEAD", "origin/main") or "").strip() or (git("rev-parse", "HEAD~1") or "").strip()
    if not base:
        return None, "no merge base"
    text = git("show", f"{base}:{rel}")
    return (json.loads(text) if text else None), base[:10]


def manifest_of(m):
    """The pinned manifest's text, or (None, why)."""
    if m.get("tree") is None:
        p = ROOT / m["manifest"]
        return (p.read_text(encoding="utf-8"), None) if p.is_file() else (None, "its in-repo manifest is missing")
    t = m["tree"]
    tree = os.environ.get(t["var"]) or os.path.expanduser(t["default"])
    pin = (ROOT / t["pin"]).read_text(encoding="utf-8").strip()
    r = subprocess.run(["git", "-C", tree, "show", f"{pin}:{m['manifest']}"], capture_output=True, text=True)
    return (r.stdout, None) if r.returncode == 0 else (None, f"no tree at {t['var']} holding {pin[:10]}")


def complete(docs):
    fails, notes = [], []
    held = set()
    for d in docs.values():
        for e in d.get("ids", []):
            held.add(entry_key(e))
    for m in json.loads((ROOT / MODULES).read_text(encoding="utf-8"))["modules"]:
        text, why = manifest_of(m)
        if text is None:
            notes.append(f"COMPLETE {m['key']}: SKIPPED ({why}); nothing was measured for it")
            continue
        man = json.loads(text)
        module = man.get("module")
        want = [(module, None)]
        addresses = set()
        for p in man.get("params", []):
            addresses.add(p["address"])
            n = len(p.get("values", [])) or int(p.get("steps", 0))
            if (p.get("class") == "flip" or p.get("unit") == "enum") and n < 2:
                continue   # a constant: never a host parameter
            want.append((module, p["address"]))
        if not {"io.inGain", "io.outGain"} <= addresses:
            want += [(module, "io.inGain"), (module, "io.outGain")]
        for w in want:
            if w not in held:
                fails.append(f"COMPLETE {m['key']}: {w[1] or 'its switch'} has no host id; append it at next_free")
    return fails, notes


def judge(docs, bases):
    fails = shape(docs) + frozen(docs)
    for name in docs:
        fails += append_only(name, docs[name], bases.get(name))
    return fails


def controls(docs, bases):
    """Each plant must make judge() red."""
    out = []
    main = LEDGERS[0]

    def plant(label, edit, base_edit=None):
        d, b = copy.deepcopy(docs), copy.deepcopy(bases)
        edit(d)
        if base_edit:
            base_edit(d, b)
        out.append((label, bool(judge(d, b))))

    def renumber(d):
        e = next(e for e in d[main]["ids"] if e["id"] == "0xA00021")
        e["id"] = "0xA00099"
    def remove(d):
        d[main]["ids"] = [e for e in d[main]["ids"] if e["id"] != "0xA00012"]
    def at_next_free(d):
        d[main]["ids"].append({"id": d[main]["next_free"], "module": "x", "key": "x.y"})
    def twice(d):
        d[main]["ids"].append({"id": "0xA00010", "module": "x", "key": "x.y"})
    def base_changed(d, b):
        b[main] = copy.deepcopy(d[main])
        b[main]["ids"].append({"id": "0xA00035", "module": "fxfilter", "key": "io.something"})
    def lowered(d, b):
        b[main] = copy.deepcopy(d[main])
        b[main]["next_free"] = "0xA0FFFF"
    plant("a frozen id renumbered", renumber)
    plant("a frozen entry removed", remove)
    plant("an id at next_free", at_next_free)
    plant("an id taken twice", twice)
    plant("a base entry changed", lambda d: None, base_changed)
    plant("next_free lowered against the base", lambda d: None, lowered)
    return out


def main():
    docs = {}
    for rel in LEDGERS:
        p = ROOT / rel
        if not p.is_file():
            print(f"host_id_ledger_check: RED -- {rel} is missing")
            return 1
        docs[rel] = json.loads(p.read_text(encoding="utf-8"))
    bases, base_notes = {}, []
    for rel in LEDGERS:
        b, at = base_ledger(rel)
        bases[rel] = b
        base_notes.append(f"APPEND {rel}: {'held against ' + at if b is not None else 'new since ' + at + ', nothing to compare'}")
    fails = judge(docs, bases)
    more, notes = complete(docs)
    fails += more
    fired = controls(docs, bases)
    for label, red in fired:
        print(f"{'PASS' if red else 'FAIL'}  control: {label}: {'red' if red else 'NOT red'}")
        if not red:
            fails.append(f"control {label!r} did not read red")
    for n in base_notes + notes:
        print("note  " + n)
    for f in fails:
        print("FAIL  " + f)
    n_ids = sum(len(d.get("ids", [])) for d in docs.values())
    skipped = [n for n in notes if "SKIPPED" in n]
    if skipped:
        print("KIT-GATE skipped: " + "; ".join(skipped))
    print(f"host_id_ledger_check: {'RED' if fails else 'GREEN'} ({n_ids} ids in {len(docs)} ledgers, {len(FROZEN)} frozen, "
          f"{sum(1 for _, r in fired if r)}/{len(fired)} controls fired)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
