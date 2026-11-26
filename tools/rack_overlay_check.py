#!/usr/bin/env python3
"""rack_overlay_check -- horde's interim law/mod overlay names exactly each pinned manifest's keys, and steps aside the
day a manifest declares them itself (ROADMAP B714; the rack-slot contract's RS-2.9; X123 (a); critic finding 16).

WIRED: ./verify fast

  python3 tools/rack_overlay_check.py

WHY. FOUNDATIONS has not yet named the normalised `law` and the `mod` flag a manifest key carries (horde's brief
seq 6). Until it does, tools/rack/law/<module>.json states them for each module horde hosts, every entry marked
horde-interim: horde choosing on the module's behalf. Two drifts would go unseen without this: a manifest that gains or
loses a key while the overlay stays put (the rack refuses the module at build, but only in a build), and a manifest
that starts declaring `law` itself while horde's interim reading still sits beside it, redundant or contradicting it.

ROWS, per tools/rack/modules.json entry:
  FILE      its overlay exists, is JSON, and names the manifest's module.
  KEYS      the overlay's addresses are exactly the pinned manifest's: none missing, none extra.
  VALUES    each entry's law is one of lin, log, step, io, io-cut; its mod is true or false; its status is set.
  OWNED     no key of the pinned manifest declares `law` (or `mod`) itself. Once one does, its overlay entry must go:
            the module owns its law (X123 (a)).
The pinned manifest is read at its pin (`git -C <tree> show <pin>:<manifest>`, the tree from the variable modules.json
names, else its ~-relative default), or from the repo for an in-repo test module. A tree that is absent prints SKIPPED
for that module and the run's `KIT-GATE skipped:` line names it: nothing was measured there, and nothing says green.

MUST-FAIL CONTROLS, every run, on in-memory copies of the first readable module: an address dropped from the overlay,
an extra address added, a bad law word, and `law` declared in the manifest. Each must read red.
"""
import copy
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MODULES = "tools/rack/modules.json"
LAWS = {"lin", "log", "step", "io", "io-cut"}


def manifest_of(m):
    if m.get("tree") is None:
        p = ROOT / m["manifest"]
        return (p.read_text(encoding="utf-8"), None) if p.is_file() else (None, "its in-repo manifest is missing")
    t = m["tree"]
    tree = os.environ.get(t["var"]) or os.path.expanduser(t["default"])
    pin = (ROOT / t["pin"]).read_text(encoding="utf-8").strip()
    r = subprocess.run(["git", "-C", tree, "show", f"{pin}:{m['manifest']}"], capture_output=True, text=True)
    return (r.stdout, None) if r.returncode == 0 else (None, f"no tree at {t['var']} holding {pin[:10]}")


def judge(key, man, over):
    fails = []
    if over.get("module") != man.get("module"):
        fails.append(f"FILE {key}: the overlay names module {over.get('module')!r}, the manifest {man.get('module')!r}")
    addresses = [p.get("address") for p in man.get("params", [])]
    keys = over.get("keys", {})
    for a in addresses:
        if a not in keys:
            fails.append(f"KEYS {key}: {a} is in the pinned manifest and not in the overlay")
    for a in keys:
        if a not in addresses:
            fails.append(f"KEYS {key}: {a} is in the overlay and not in the pinned manifest")
    for a, e in keys.items():
        if e.get("law") not in LAWS:
            fails.append(f"VALUES {key}: {a}'s law {e.get('law')!r} is none of {sorted(LAWS)}")
        if not isinstance(e.get("mod"), bool):
            fails.append(f"VALUES {key}: {a}'s mod is not true or false")
        if not e.get("status"):
            fails.append(f"VALUES {key}: {a} carries no status (horde-interim, or why not)")
    for p in man.get("params", []):
        for field in ("law", "mod"):
            if field in p:
                fails.append(f"OWNED {key}: the pinned manifest declares {field} for {p.get('address')} itself; delete the overlay's entry")
    return fails


def main():
    modules = json.loads((ROOT / MODULES).read_text(encoding="utf-8"))["modules"]
    fails, skipped, judged, sample = [], [], 0, None
    for m in modules:
        path = ROOT / m["overlay"]
        if not path.is_file():
            fails.append(f"FILE {m['key']}: {m['overlay']} is missing")
            continue
        try:
            over = json.loads(path.read_text(encoding="utf-8"))
        except ValueError as e:
            fails.append(f"FILE {m['key']}: {m['overlay']} is not JSON ({e})")
            continue
        text, why = manifest_of(m)
        if text is None:
            skipped.append(f"{m['key']} ({why})")
            print(f"SKIP  {m['key']}: {why}; its overlay was not held to its manifest")
            continue
        man = json.loads(text)
        judged += 1
        got = judge(m["key"], man, over)
        fails += got
        print(f"{'FAIL' if got else 'PASS'}  {m['key']}: {len(over.get('keys', {}))} overlay entries against {len(man.get('params', []))} manifest keys")
        if sample is None:
            sample = (m["key"], man, over)
    if sample is None:
        fails.append("no module's pinned manifest was readable, so no control could run")
    else:
        key, man, over = sample
        plants = []
        o = copy.deepcopy(over); o["keys"].pop(next(iter(o["keys"]))); plants.append(("an address dropped from the overlay", man, o))
        o = copy.deepcopy(over); o["keys"]["planted.extra"] = {"law": "lin", "mod": True, "status": "x"}; plants.append(("an extra address", man, o))
        o = copy.deepcopy(over); o["keys"][next(iter(o["keys"]))]["law"] = "exp"; plants.append(("a law outside the set", man, o))
        mm = copy.deepcopy(man); mm["params"][0]["law"] = "lin"; plants.append(("law declared by the manifest itself", mm, over))
        for label, pm, po in plants:
            red = bool(judge(key, pm, po))
            print(f"{'PASS' if red else 'FAIL'}  control: {label}: {'red' if red else 'NOT red'}")
            if not red:
                fails.append(f"control {label!r} did not read red")
    for f in fails:
        print("FAIL  " + f)
    if skipped:
        print("KIT-GATE skipped: " + "; ".join(skipped) + f" (judged {judged} of {len(modules)})")
    print(f"rack_overlay_check: {'RED' if fails else 'PARTIAL' if skipped else 'GREEN'} ({judged} of {len(modules)} modules held to their pinned manifests)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
