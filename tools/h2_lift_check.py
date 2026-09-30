#!/usr/bin/env python3
"""h2_lift_check -- a lifted h2 core is its legacy source plus its ledger, byte for byte.

WIRED: ./verify fast

WHY. ADR-186 item 4 lifts a legacy core into h2/ by COPYING it, and ADR-187's
consequence L3 makes a byte-identical lift the core's parity proof: the copy
inherits the legacy chain only as long as it IS the legacy code plus a known,
listed set of edits. A single stray edit in the copy would carry the proof's
name without its substance, and nothing else would notice: the build stays
green and the audio still plays. ROADMAP B379 step 1, and its critic rework.

WHAT IT CHECKS, for every h2/cores/<core>/lift-ledger.json:
  - each lifted file (a key of `files`, repo-relative) is rebuilt from its
    CURRENT src original by applying the ledger's entries for it, in order, and
    the result must equal the lifted file BYTE FOR BYTE. Files are read as
    bytes, never as newline-translated text, so a CRLF or a stray carriage
    return is a difference like any other;
  - the source has not moved since the lift: its git blob must equal the
    ledger's `src_blob`. If it moved, the verdict is "legacy moved since the
    lift; re-lift deliberately", and the copy is NOT blamed for it;
  - every entry applies: a 'replace' line, an 'insert_after' anchor and a
    'patch' block must each occur exactly once in the text being rebuilt;
  - every *.h in the ledger's directory is a key of `files` (a header dropped
    in beside the lift would otherwise escape the gate);
  - a divergence entry (kind 'divergence') names its ADR-187 ledger id, and that
    id is defined in the directory's divergences.json. This is the one path for
    later edits: divergences live here as hunks too (h2/README.md rule 8).
  So the ONLY differences a lift can carry are the ledgered ones, and nothing
  else is normalised (no whitespace, comment or namespace folding).

THE RULES CALIBRATE THEMSELVES ON EVERY RUN (selftest()): an unledgered edit, a
stale entry, an ambiguous anchor, a stray carriage return, a moved source, an
unlisted header, a multi-line patch applied and misapplied, and a divergence
hunk without its ledger id must each be judged as intended.
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from h2_rules_check import blob_hash  # noqa: E402  (one implementation of git's blob id)

KINDS = ("namespace", "edit", "divergence")


def _b(s):
    return s.encode("utf-8")


def _block(v):
    """A ledger block: a string (one line) or a list of lines -> list of byte lines."""
    return [_b(x) for x in (v if isinstance(v, list) else [v])]


def _find(lines, block):
    n = len(block)
    return [i for i in range(len(lines) - n + 1) if lines[i:i + n] == block]


def rebuild(src_bytes, entries, div_ids):
    """src bytes + this file's ledger entries -> (expected lifted bytes, failures)."""
    lines = src_bytes.split(b"\n")
    fails = []
    for e in entries:
        eid, op, kind = e.get("id", "?"), e.get("op"), e.get("kind")
        if kind not in KINDS:
            fails.append(f"{eid}: unknown kind {kind!r} (one of {', '.join(KINDS)})")
            continue
        if kind == "divergence" and e.get("divergence") not in div_ids:
            fails.append(f"{eid}: a divergence hunk must carry an id defined in divergences.json (got {e.get('divergence')!r})")
            continue
        if op == "replace":
            old, new = _block(e["src"]), _block(e["h2"])
        elif op == "insert_after":
            old = _block(e["after"])
            new = old + _block(e["h2"])
        elif op == "patch":
            old, new = _block(e["old"]), _block(e["new"])
        else:
            fails.append(f"{eid}: unknown op {op!r}")
            continue
        if op in ("replace", "insert_after") and len(old) != 1:
            fails.append(f"{eid}: '{op}' takes one line; use 'patch' for a block")
            continue
        hits = _find(lines, old)
        if len(hits) != 1:
            what = "line" if len(old) == 1 else f"{len(old)}-line block"
            fails.append(f"{eid}: its source {what} occurs {len(hits)} times (must be exactly 1): {old[0].decode('utf-8', 'replace')!r}")
            continue
        i = hits[0]
        lines[i:i + len(old)] = new
    return b"\n".join(lines), fails


def first_diff(a, b):
    al, bl = a.split(b"\n"), b.split(b"\n")
    for i in range(max(len(al), len(bl))):
        x = al[i] if i < len(al) else b"<end of file>"
        y = bl[i] if i < len(bl) else b"<end of file>"
        if x != y:
            return i + 1, x, y
    return None


def check_file(name, src_bytes, h2_bytes, entries, src_blob, div_ids=()):
    now = blob_hash(src_bytes)
    if src_blob != now:
        return [f"{name}: legacy moved since the lift at {src_blob}; re-lift deliberately (the source is now {now}; the h2 copy is not blamed)"]
    want, fails = rebuild(src_bytes, entries, set(div_ids))
    if not fails and want != h2_bytes:
        n, x, y = first_diff(want, h2_bytes)
        fails.append(f"{name}: an UNLEDGERED difference at lifted line {n}:\n        expected {x!r}\n        found    {y!r}")
    return fails


def check_listed(core_dir_rel, headers, files):
    """Every *.h in the ledger's directory must be a lifted file the ledger lists."""
    return [f"{core_dir_rel}/{h}: a header in a ledgered core directory that the ledger's `files` does not list"
            for h in headers if f"{core_dir_rel}/{h}" not in files]


def selftest():
    src = b"a\nnamespace old\nb\nc\n"
    ns = {"id": "NS", "kind": "namespace", "op": "replace", "src": "namespace old", "h2": "namespace new::old"}
    ed = {"id": "E", "kind": "edit", "op": "insert_after", "after": "b", "h2": "x"}
    good = b"a\nnamespace new::old\nb\nx\nc\n"
    blob = blob_hash(src)
    ok = lambda s, h, es, bl=blob, d=(): not check_file("t", s, h, es, bl, d)
    cases = [
        (ok(src, good, [ns, ed]), "a conforming lift was rejected"),
        (not ok(src, good.replace(b"c\n", b"c2\n"), [ns, ed]), "an unledgered edit was accepted"),
        (not ok(src, b"a\nnamespace old\nb\nx\nc\n", [ns, ed]), "a ledgered replace the copy does not carry was accepted"),
        (not ok(src, good, [ns, ed, {"id": "S", "kind": "edit", "op": "replace", "src": "gone", "h2": "y"}]), "a stale entry was accepted"),
        (not ok(b"b\nb\n", b"b\nx\nb\n", [ed], blob_hash(b"b\nb\n")), "an ambiguous anchor was accepted"),
        # L1: bytes, not text. A CRLF copy of an LF source, and one stray \r, are differences.
        (not ok(src, good.replace(b"\n", b"\r\n"), [ns, ed]), "a CRLF copy of an LF source was accepted"),
        (not ok(src, good.replace(b"b\n", b"b\r\n"), [ns, ed]), "a stray carriage return was accepted"),
        # M1: a moved source is its own verdict, and the copy is not blamed.
        (not ok(src + b"d\n", good, [ns, ed]) and "legacy moved" in check_file("t", src + b"d\n", good, [ns, ed], blob)[0],
         "a source that moved since the lift was not reported as moved"),
        # M2: a multi-line patch applies exactly, and a misapplied one is caught.
        (ok(src, b"a\nnamespace old\nP\nQ\nR\n", [{"id": "P", "kind": "edit", "op": "patch", "old": ["b", "c"], "new": ["P", "Q", "R"]}]),
         "a multi-line patch was rejected"),
        (not ok(src, b"a\nnamespace old\nP\nQ\nc\n", [{"id": "P", "kind": "edit", "op": "patch", "old": ["b", "c"], "new": ["P", "Q", "R"]}]),
         "a misapplied multi-line patch was accepted"),
        (not ok(src, b"a\nnamespace old\nb\nZ\n", [{"id": "D", "kind": "divergence", "op": "patch", "old": ["c"], "new": ["Z"]}]),
         "a divergence hunk without a ledgered id was accepted"),
        (ok(src, b"a\nnamespace old\nb\nZ\n", [{"id": "D", "kind": "divergence", "divergence": "D1", "op": "patch", "old": ["c"], "new": ["Z"]}], blob, ("D1",)),
         "a divergence hunk with its ledgered id was rejected"),
        # L2: a header beside the lift that the ledger does not list.
        (len(check_listed("h2/cores/x", ["a.h", "b.h"], {"h2/cores/x/a.h": {}})) == 1, "an unlisted header was not flagged"),
    ]
    for passed, why in cases:
        if not passed:
            return why
    return None, len(cases)


def main():
    st = selftest()
    if not isinstance(st, tuple):
        print("h2_lift_check: FAILED (the rule itself is broken) — selftest: " + st, file=sys.stderr)
        return 1
    ledgers = sorted(ROOT.glob("h2/cores/*/lift-ledger.json"))
    if not ledgers:
        print("h2_lift_check: FAILED — no h2/cores/*/lift-ledger.json found", file=sys.stderr)
        return 1
    fails, report = [], []
    for lp in ledgers:
        core_dir = lp.parent
        core_rel = str(core_dir.relative_to(ROOT))
        led = json.loads(lp.read_text())
        files = led["files"]
        divp = core_dir / "divergences.json"
        div_ids = [d["id"] for d in json.loads(divp.read_text()).get("divergences", [])] if divp.is_file() else []
        fails += check_listed(core_rel, sorted(p.name for p in core_dir.glob("*.h")), files)
        for rel, spec in files.items():
            h2p, srcp = ROOT / rel, ROOT / spec["src"]
            if not h2p.is_file() or not srcp.is_file():
                fails.append(f"{rel}: missing ({rel} or {spec['src']})")
                continue
            mine = [e for e in led["entries"] if e["file"] == rel]
            fails += check_file(rel, srcp.read_bytes(), h2p.read_bytes(), mine, spec["src_blob"], div_ids)
            report.append(f"{rel} = {spec['src']}@{spec['src_blob'][:7]} + [{', '.join(e['id'] for e in mine)}]")
        unknown = [e["id"] for e in led["entries"] if e["file"] not in files]
        if unknown:
            fails.append(f"{lp.relative_to(ROOT)}: entries for files the ledger does not list: {unknown}")
    if fails:
        print(f"h2_lift_check: FAILED — {len(fails)} problem(s):", file=sys.stderr)
        for f in fails:
            print("    " + f, file=sys.stderr)
        return 1
    print(f"h2_lift_check: GREEN ({'; '.join(report)}; selftest {st[1]}/{st[1]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
