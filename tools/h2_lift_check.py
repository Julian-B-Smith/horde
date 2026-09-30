#!/usr/bin/env python3
"""h2_lift_check -- a lifted h2 core is its legacy source plus its ledger, byte for byte.

WIRED: ./verify fast

WHY. ADR-186 item 4 lifts a legacy core into h2/ by COPYING it, and ADR-187's
consequence L3 makes a byte-identical lift the core's parity proof: the copy
inherits the legacy chain (parity_check, 156/156 against SwarmSynth) only as
long as it IS the legacy code. A single stray edit in the copy would carry the
proof's name without its substance, and nothing else would notice: the build
stays green and the audio still plays. ROADMAP B379 step 1.

WHAT IT CHECKS. For every core directory with an h2/cores/<core>/lift-ledger.json:
  - each lifted file is rebuilt from its src/ original by applying the ledger's
    entries (a 'replace' swaps one source line that must occur exactly once; an
    'insert_after' adds one line after a source line that must occur exactly
    once), and the result must equal the lifted file BYTE FOR BYTE;
  - every entry must apply (a stale entry is red: the ledger may not claim an
    edit the tree no longer has);
  - every file the ledger names exists on both sides.
  So the ONLY differences a lift can carry are the ledgered ones, and the diff
  is normalised by nothing else (no whitespace, comment or namespace folding).

THE RULE CALIBRATES ITSELF ON EVERY RUN (selftest()): an unledgered edit, a
stale entry and an ambiguous anchor must each be rejected, and a conforming
synthetic lift accepted.
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def rebuild(src_text, entries):
    """src text + ledger entries for this file -> (expected lifted text, failures)."""
    lines = src_text.split("\n")
    fails = []
    for e in entries:
        key = e["src"] if e["op"] == "replace" else e.get("after")
        if e["op"] not in ("replace", "insert_after") or key is None:
            fails.append(f"{e['id']}: unknown op {e['op']!r}")
            continue
        hits = [i for i, l in enumerate(lines) if l == key]
        if len(hits) != 1:
            fails.append(f"{e['id']}: its source line occurs {len(hits)} times in the original (must be exactly 1): {key!r}")
            continue
        i = hits[0]
        if e["op"] == "replace":
            lines[i] = e["h2"]
        else:
            lines.insert(i + 1, e["h2"])
    return "\n".join(lines), fails


def first_diff(a, b):
    al, bl = a.split("\n"), b.split("\n")
    for i in range(max(len(al), len(bl))):
        x = al[i] if i < len(al) else "<end of file>"
        y = bl[i] if i < len(bl) else "<end of file>"
        if x != y:
            return i + 1, x, y
    return None


def check_file(name, src_text, h2_text, entries):
    want, fails = rebuild(src_text, entries)
    if not fails and want != h2_text:
        n, x, y = first_diff(want, h2_text)
        fails.append(f"{name}: an UNLEDGERED difference at lifted line {n}:\n        expected {x!r}\n        found    {y!r}")
    return fails


def selftest():
    src = "a\nnamespace old\nb\nc\n"
    ns = {"id": "NS", "op": "replace", "src": "namespace old", "h2": "namespace new::old"}
    ed = {"id": "E", "op": "insert_after", "after": "b", "h2": "x"}
    good = "a\nnamespace new::old\nb\nx\nc\n"
    if check_file("t", src, good, [ns, ed]):
        return "a conforming lift was rejected"
    if not check_file("t", src, good.replace("c\n", "c2\n"), [ns, ed]):
        return "an unledgered edit was accepted"
    if not check_file("t", src, "a\nnamespace old\nb\nx\nc\n", [ns, ed]):
        return "a ledgered replace that the copy does not carry was accepted"
    if not check_file("t", src, good, [ns, ed, {"id": "S", "op": "replace", "src": "gone", "h2": "y"}]):
        return "a stale entry was accepted"
    if not check_file("t", "b\nb\n", "b\nx\nb\n", [ed]):
        return "an ambiguous anchor was accepted"
    return None


def main():
    err = selftest()
    if err:
        print("h2_lift_check: FAILED (the rule itself is broken) — selftest: " + err, file=sys.stderr)
        return 1
    ledgers = sorted(ROOT.glob("h2/cores/*/lift-ledger.json"))
    if not ledgers:
        print("h2_lift_check: FAILED — no h2/cores/*/lift-ledger.json found", file=sys.stderr)
        return 1
    fails, report = [], []
    for lp in ledgers:
        core_dir = lp.parent
        led = json.loads(lp.read_text())
        for name, src_rel in led["files"].items():
            h2p, srcp = core_dir / name, ROOT / src_rel
            if not h2p.is_file() or not srcp.is_file():
                fails.append(f"{core_dir.name}/{name}: missing ({h2p.relative_to(ROOT)} or {src_rel})")
                continue
            mine = [e for e in led["entries"] if e["file"] == name]
            fails += [f"{core_dir.name}/" + f for f in check_file(name, srcp.read_text(), h2p.read_text(), mine)]
            report.append(f"{core_dir.name}/{name} = {src_rel} + {len(mine)} ledgered line(s) [{', '.join(e['id'] for e in mine)}]")
        unknown = [e["id"] for e in led["entries"] if e["file"] not in led["files"]]
        if unknown:
            fails.append(f"{lp.relative_to(ROOT)}: entries for files the ledger does not list: {unknown}")
    if fails:
        print(f"h2_lift_check: FAILED — {len(fails)} problem(s):", file=sys.stderr)
        for f in fails:
            print("    " + f, file=sys.stderr)
        return 1
    print("h2_lift_check: GREEN (" + "; ".join(report) + "; selftest 5/5)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
