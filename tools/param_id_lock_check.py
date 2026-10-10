#!/usr/bin/env python3
"""param_id_lock_check -- host-visible parameter ids are APPEND-ONLY (B448 C2, ADR-197 risk row 7).

WIRED: ./verify fast (static extraction); ./verify full adds --runtime (the real plugin).

WHY. A CLAP parameter id is what a host stores in a saved session and in every
automation lane. Renumber one, reuse one, or change its stepping or range, and an
old project silently drives a different knob. No audio oracle sees that: the sound
is "plausible" either way. ADR-082 froze the ids and a ROADMAP line says "already
true", but nothing failed when the rule broke -- this file is the thing that fails.

WHAT ALREADY EXISTED, AND WHY IT IS NOT ENOUGH. paramclass_check / paramscope_check
classify and scope parameters, morphlayout_check pins the morph field's layout,
state_check / statefix_check prove saved state reloads. None compares the id SET and
each id's identity against a committed record, so a deleted or re-pointed id passes
all of them. registry_dump (FOUNDATIONS' emitter) enumerates ids but aborts today
(it zips state keys against params and they have drifted, 371 vs 397) and checks
nothing. Horde 2 has no parameter table yet (B308 H4 plans one), so its section of
the lock is a marked stub.

THE LOCK. tools/param_id_lock.json: one entry per parameter the legacy shell shows a
host: id, display name, stepped, automatable, min, max. Defaults are NOT locked (a
default is a different contract: ADR-197 requires its own ADR plus a migration).

WHAT IT CHECKS (every run).
  RED  an id in the lock is missing from the tree (REMOVED, or RENUMBERED when the
       same unique name now carries another id);
  RED  an id's name, stepping, automatable flag or range differs from the lock;
  RED  two parameters share an id (in the tree or in the lock);
  RED  an id the human retired (--approve-change on a removal) is back in the tree;
  RED  the tree has ids the lock lacks. They are printed, and `--append` adds them.
       ALLOWED to exist, REQUIRED to be recorded in the same PR.
MODES.
  (none)                   static check against src/ -- no build, ~0.1 s (`fast`).
  --runtime <param_id_dump>  also run the real plugin's enumeration and require it to
                           equal the static extraction field for field, and the lock.
                           This is what proves the static extractor is honest.
  --append                 add the tree's new ids to the lock. Refuses (writes
                           nothing) if any existing entry is missing or differs.
  --approve-change <id> <ref>  a HUMAN-approved break of one id: re-pins it to the
                           tree (or retires it if the tree dropped it). <ref> (an ADR
                           or ROADMAP row, non-empty) is recorded in the lock.
SELF-CALIBRATING: selftest() plants a removed id, a renumbered id, a retyped id, a
changed range, a renamed id, a duplicate id, a reused retired id and an --append that
tries to alter an entry, plus an empty-ref approval; each must be refused, and the
untouched tree must be accepted. A checker that cannot fail proves nothing.
"""
import copy
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "hypersaw_clap.cpp"
FXRACK = ROOT / "src" / "fx_rack.h"
LOCK = pathlib.Path(__file__).resolve().parent / "param_id_lock.json"
FIELDS = ("name", "stepped", "automatable", "min", "max")


def strip_comments(s):
    """Block comments first: a `//` inside /* ... */ must not start a line comment,
    and the kGlobalIds list carries ADR numbers in its comments (registry_decl.py's
    lesson: \\d+ over raw text slurped digits out of 'ADR-082' and '2026-08-11')."""
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


def _const(src, name, cast=int):
    m = re.search(r"\b%s\s*=\s*([0-9]+)" % re.escape(name), src)
    if not m:
        sys.exit(f"param_id_lock_check: cannot find constant {name} in the source (the layout moved; "
                 "update this extractor and let the --runtime row confirm it)")
    return cast(m.group(1))


def _rows(src, array_name):
    """Rows `{id, "key", "Name", min, max, default, stepped, ...}` of a ParamDef array."""
    m = re.search(r"%s\[\]\s*=\s*\{(.*?)\n\};" % re.escape(array_name), src, re.S)
    if not m:
        sys.exit(f"param_id_lock_check: array {array_name} not found")
    row = re.compile(r'\{\s*(\d+)\s*,\s*"[^"]*"\s*,\s*"([^"]*)"\s*,\s*([-+0-9.eE]+)\s*,'
                     r'\s*([-+0-9.eE]+)\s*,\s*[-+0-9.eE]+\s*,\s*(true|false)\s*,')
    return [(int(i), n, float(lo), float(hi), st == "true") for i, n, lo, hi, st in row.findall(m.group(1))]


def extract_static():
    """-> {id: {name, stepped, automatable, min, max}} computed from the source alone.

    It re-derives, in ~40 lines, what params_get_info computes at run time. That is a
    deliberate duplicate (the table is file-static and the routing/engine blocks are
    built at load time, so only a build can enumerate them, and `fast` has no build).
    The duplicate is policed by `--runtime` in `full`, which demands field-for-field
    equality with the real plugin -- if this drifts, that row goes red, not this one."""
    src = strip_comments(SRC.read_text(encoding="utf-8"))
    fx = strip_comments(FXRACK.read_text(encoding="utf-8"))
    out = {}

    def put(pid, name, stepped, lo, hi):
        if pid in out:
            sys.exit(f"param_id_lock_check: the source declares id {pid} twice ({out[pid]['name']!r} "
                     f"and {name!r}) -- two parameters share an id")
        out[pid] = {"name": name, "stepped": bool(stepped), "automatable": True,
                    "min": lo, "max": hi}

    stride, nosc = _const(src, "kOscStride"), _const(src, "kNumOsc")
    gblk = re.search(r"constexpr clap_id kGlobalIds\[\]\s*=\s*\{(.*?)\};", src, re.S)
    if not gblk:
        sys.exit("param_id_lock_check: kGlobalIds not found")
    globals_ = {int(x) for x in re.findall(r"\b\d+\b", gblk.group(1))}
    base = _rows(src, "static const ParamDef kParams")
    if not base:
        sys.exit("param_id_lock_check: kParams parsed to nothing -- the row shape changed")
    for pid, name, lo, hi, st in base:
        put(pid, name, st, lo, hi)
    for osc in range(1, nosc):                      # per-oscillator twins: id + 1000*osc
        for pid, name, lo, hi, st in base:
            if pid not in globals_:
                put(pid + osc * stride, f"Osc{osc + 1} {name}", st, lo, hi)

    for pid, name, lo, hi, st in _rows(src, "static constexpr ParamDef kSubOscParams"):
        put(pid, name, st, lo, hi)                  # the only engine block (kEngineBlocks)

    # ADR-088 routing block: ids are COMPUTED from the cell. Ranges and name formats
    # are retyped from makeRoutingTable(); the edge rule from routing_core.h's
    # edgeForward (a slot reaches a LATER slot, a source reaches any).
    nsrc, maxsrc = _const(src, "kRoutingNSrc"), _const(src, "kRoutingMaxSrc")
    nslot = _const(fx, "kRackSlots")
    fstride = _const(src, "kRoutingFromStride")
    cbase, obase = _const(src, "kRoutingCoeffBase"), _const(src, "kRoutingOutBase")
    ibase, sbase = _const(src, "kRoutingInitBase"), _const(src, "kRoutingSrcOutBase")
    for f in range(maxsrc + nslot):
        mi = f if f < nsrc else (nsrc + f - maxsrc if maxsrc <= f < maxsrc + nslot else -1)
        if mi < 0:
            continue
        for t in range(nslot):
            if mi < nsrc or (mi - nsrc) < t:
                lhs = f"Src{f + 1}" if f < maxsrc else f"Slot{f - maxsrc + 1}"
                put(cbase + f * fstride + t, f"Route {lhs} > Slot{t + 1}", False, -2.0, 2.0)
    for t in range(nslot):
        put(obase + t, f"Out Slot{t + 1}", False, 0.0, 2.0)
    for t in range(nslot):
        put(ibase + t, f"Init Slot{t + 1}", False, -1.0, 1.0)
    for s in range(nsrc):
        put(sbase + s, f"Out Src{s + 1}", False, 0.0, 2.0)
    return out


def parse_dump(text):
    """param_id_dump's TSV -> {id: entry}. A duplicate id is a hard error, not a merge."""
    out = {}
    for ln in text.splitlines():
        if not ln.strip():
            continue
        p = ln.split("\t")
        if len(p) != 6:
            sys.exit(f"param_id_lock_check: malformed dump line {ln!r}")
        pid = int(p[0])
        if pid in out:
            sys.exit(f"param_id_lock_check: the plugin enumerates id {pid} twice")
        out[pid] = {"name": p[1], "stepped": p[2] == "1", "automatable": p[3] == "1",
                    "min": float(p[4]), "max": float(p[5])}
    return out


# ---- the lock ---------------------------------------------------------------

def entry_of(pid, p):
    return {"id": pid, "name": p["name"], "stepped": p["stepped"],
            "automatable": p["automatable"], "min": p["min"], "max": p["max"]}


def lock_params(lock):
    """-> ({id: entry}, [duplicate ids]). Duplicates are reported, never merged."""
    out, dups = {}, []
    for e in lock["legacy"]["params"]:
        if e["id"] in out:
            dups.append(e["id"])
        out[e["id"]] = e
    return out, dups


def compare(lock, tree):
    """The whole rule, pure. -> (failures, new_ids). `tree` is {id: entry} (extract/dump)."""
    fails = []
    locked, dups = lock_params(lock)
    fails += [f"id {i} appears twice in the lock" for i in dups]
    retired = {r["id"]: r for r in lock["legacy"].get("retired", [])}
    by_name = {}
    for i, p in tree.items():
        by_name.setdefault(p["name"], []).append(i)
    for i, e in sorted(locked.items()):
        t = tree.get(i)
        if t is None:
            moved = [j for j in by_name.get(e["name"], []) if j not in locked]
            if len(by_name.get(e["name"], [])) == 1 and moved:
                fails.append(f"id {i} {e['name']!r} RENUMBERED: the same name is now id {moved[0]} "
                             "(a host's saved sessions and automation lanes still address the old id)")
            else:
                fails.append(f"id {i} {e['name']!r} REMOVED from the tree (ids are append-only; "
                             "a human retires one with --approve-change)")
            continue
        for k in FIELDS:
            if t[k] != e[k]:
                fails.append(f"id {i} {e['name']!r}: {k} changed {e[k]!r} -> {t[k]!r}")
    for i in sorted(retired):
        if i in tree:
            fails.append(f"id {i} was RETIRED ({retired[i]['ref']}) and is back in the tree: "
                         "a retired id is never reused")
    new = sorted(i for i in tree if i not in locked and i not in retired)
    return fails, new


def append_new(lock, tree):
    """--append: -> (new_lock, added_ids) or raises ValueError. Only ever ADDS: if any
    existing entry is missing or differs, it refuses, so `--append` can never be the
    way a break gets laundered into the lock."""
    fails, new = compare(lock, tree)
    if fails:
        raise ValueError("refusing to --append; existing entries are not intact:\n  " + "\n  ".join(fails))
    out = copy.deepcopy(lock)
    out["legacy"]["params"] += [entry_of(i, tree[i]) for i in new]
    out["legacy"]["params"].sort(key=lambda e: e["id"])
    return out, new


def approve_change(lock, tree, pid, ref):
    """--approve-change: a human-approved break of ONE id -> new_lock, or ValueError."""
    if not (ref or "").strip():
        raise ValueError("--approve-change needs a non-empty <ref> (the ADR or ROADMAP row that approves it)")
    out = copy.deepcopy(lock)
    rows = out["legacy"]["params"]
    idx = next((k for k, e in enumerate(rows) if e["id"] == pid), None)
    if idx is None:
        raise ValueError(f"id {pid} is not in the lock (new ids are added with --append, not approved)")
    t = tree.get(pid)
    if t is None:                                   # approved removal: retire the id for good
        gone = rows.pop(idx)
        out["legacy"].setdefault("retired", []).append({"id": pid, "name": gone["name"], "ref": ref})
        return out
    if all(t[k] == rows[idx][k] for k in FIELDS):
        raise ValueError(f"id {pid} already matches the tree; nothing to approve")
    rows[idx] = dict(entry_of(pid, t), approved=list(rows[idx].get("approved", [])) + [ref])
    return out


def new_lock(tree):
    return {"schema": 1,
            "legacy": {"source": "tools/param_id_dump.cpp enumerates the real plugin; "
                                 "tools/param_id_lock_check.py extracts the same list statically",
                       "params": [entry_of(i, tree[i]) for i in sorted(tree)], "retired": []},
            "h2": {"status": "STUB",
                   "note": "horde 2 has no parameter table or manifest yet (h2/ holds cores and one "
                           "engine, no host-visible ids). ROADMAP B308 H4 plans the generated table "
                           "committed as a lockfile; when it exists, its ids lock here the same way."}}


def dump_lock(lock):
    """One parameter per line: the lock is reviewed as a diff, and seven lines per id
    would bury an appended id in noise. Everything else is ordinary indented JSON."""
    rows = lock["legacy"]["params"]
    head = dict(lock, legacy=dict(lock["legacy"], params=["@@"]))
    text = json.dumps(head, indent=1, ensure_ascii=False)
    body = ",\n".join("   " + json.dumps(e, ensure_ascii=False) for e in rows)
    return text.replace('   "@@"', body) + "\n"


# ---- self-calibration ---------------------------------------------------------

def selftest(tree):
    """Plant each fault on a COPY of the real tree/lock; every one must be refused and
    the untouched pair accepted. -> (problems, number of expect_red plants); empty problems = calibrated."""
    bad, planted = [], [0]
    lock = new_lock(tree)
    names = [p["name"] for p in tree.values()]
    X = min(i for i, p in tree.items() if names.count(p["name"]) == 1)   # a uniquely named id

    def expect_red(label, lk, tr, needle):
        planted[0] += 1
        fails, _ = compare(lk, tr)
        if not any(needle in f for f in fails):
            bad.append(f"control '{label}' was NOT refused (wanted {needle!r}, got {fails or 'green'})")

    def mutated(fn):
        t = copy.deepcopy(tree)
        fn(t)
        return t

    planted[0] += 1
    fails, new = compare(lock, tree)                # the must-read-GREEN control
    if fails or new or len(tree) < 100:
        bad.append(f"control 'untouched tree' read RED/empty: {fails[:2]} new={new[:2]} n={len(tree)}")
    expect_red("removed id", lock, mutated(lambda t: t.pop(X)), "REMOVED")
    expect_red("renumbered id", lock,
               mutated(lambda t: t.__setitem__(X + 5000, t.pop(X))), "RENUMBERED")
    expect_red("changed range", lock, mutated(lambda t: t[X].__setitem__("max", t[X]["max"] + 1)), "max changed")
    expect_red("changed min", lock, mutated(lambda t: t[X].__setitem__("min", t[X]["min"] - 1)), "min changed")
    expect_red("retyped (stepping)", lock,
               mutated(lambda t: t[X].__setitem__("stepped", not t[X]["stepped"])), "stepped changed")
    expect_red("retyped (automatable)", lock,
               mutated(lambda t: t[X].__setitem__("automatable", False)), "automatable changed")
    expect_red("renamed id", lock, mutated(lambda t: t[X].__setitem__("name", "Voices2")), "name changed")
    dup = copy.deepcopy(lock)
    dup["legacy"]["params"].append(dict(dup["legacy"]["params"][0]))
    expect_red("duplicate id in the lock", dup, tree, "appears twice")
    planted[0] += 1
    try:
        parse_dump("1\ta\t0\t1\t0\t1\n1\tb\t0\t1\t0\t1\n")
        bad.append("control 'duplicate id in the plugin's enumeration' was NOT refused")
    except SystemExit:
        pass
    retired = copy.deepcopy(lock)
    gone = retired["legacy"]["params"].pop(0)
    retired["legacy"]["retired"].append({"id": gone["id"], "name": gone["name"], "ref": "ADR-test"})
    expect_red("retired id reused", retired, tree, "RETIRED")

    planted[0] += 3                                 # the three append controls below
    grown = mutated(lambda t: t.__setitem__(99999, dict(t[X], name="Newcomer")))
    fails, new = compare(lock, grown)
    if fails or new != [99999]:
        bad.append(f"control 'appended id' should be NEW only, got fails={fails} new={new}")
    try:
        merged, added = append_new(lock, grown)
        if added != [99999] or compare(merged, grown) != ([], []):
            bad.append("control 'append' did not produce a lock that matches the grown tree")
        if compare(lock, grown)[1] != [99999]:
            bad.append("control 'append' mutated its input lock")
    except ValueError as e:
        bad.append(f"control 'append' was refused: {e}")
    tampered = mutated(lambda t: (t.__setitem__(99999, dict(t[X], name="Newcomer")),
                                  t[X].__setitem__("max", t[X]["max"] + 1)))
    try:
        append_new(lock, tampered)
        bad.append("control '--append that alters an existing entry' was NOT refused")
    except ValueError:
        pass
    for ref in ("", "   ", None):
        planted[0] += 1
        try:
            approve_change(lock, mutated(lambda t: t[X].__setitem__("max", 99)), X, ref)
            bad.append(f"control 'approve with empty ref {ref!r}' was NOT refused")
        except ValueError:
            pass
    planted[0] += 2
    ranged = mutated(lambda t: t[X].__setitem__("max", 99))
    ok_lock = approve_change(lock, ranged, X, "ADR-test")
    if compare(ok_lock, ranged) != ([], []):
        bad.append("control 'approved range change' did not leave the lock matching the tree")
    retire = approve_change(lock, mutated(lambda t: t.pop(X)), X, "ADR-test")
    if compare(retire, mutated(lambda t: t.pop(X))) != ([], []) or not retire["legacy"]["retired"]:
        bad.append("control 'approved removal' did not retire the id cleanly")
    return bad, planted[0]


# ---- driver -------------------------------------------------------------------

def load_lock():
    if not LOCK.exists():
        sys.exit(f"param_id_lock_check: {LOCK.relative_to(ROOT)} is missing -- a deleted lock is a "
                 "RED, not a reset (a first lock is made with --init)")
    return json.loads(LOCK.read_text(encoding="utf-8"))


def main(argv):
    runtime = None
    args = list(argv)
    if "--runtime" in args:
        i = args.index("--runtime")
        runtime = args[i + 1] if i + 1 < len(args) else sys.exit("--runtime needs the param_id_dump path")
        del args[i:i + 2]
    tree = extract_static()
    bad, planted = selftest(tree)
    if bad:
        print("param_id_lock_check: SELFTEST FAILED -- the checker cannot be trusted:\n  " + "\n  ".join(bad))
        return 1
    try:
        if args[:1] == ["--init"]:
            if LOCK.exists():
                raise ValueError("the lock exists; --init only makes a first one")
            LOCK.write_text(dump_lock(new_lock(tree)), encoding="utf-8")
            print(f"param_id_lock_check: wrote {LOCK.relative_to(ROOT)} ({len(tree)} ids)")
            return 0
        if args[:1] == ["--append"]:
            lock, added = append_new(load_lock(), tree)
            LOCK.write_text(dump_lock(lock), encoding="utf-8")
            print(f"param_id_lock_check: appended {len(added)} id(s): {added}")
            return 0
        if args[:1] == ["--approve-change"]:
            if len(args) != 3 or not args[1].isdigit():
                raise ValueError("usage: --approve-change <id> <ref>")
            LOCK.write_text(dump_lock(approve_change(load_lock(), tree, int(args[1]), args[2])), encoding="utf-8")
            print(f"param_id_lock_check: re-pinned id {args[1]} under {args[2]}")
            return 0
    except ValueError as e:
        print(f"param_id_lock_check: {e}")
        return 1
    if args:
        print(__doc__.split("MODES.")[1].split("SELF-CALIBRATING")[0])
        return 2
    problems = []
    if runtime:
        r = subprocess.run([runtime], capture_output=True, text=True, timeout=120)
        if r.returncode != 0:
            print(f"param_id_lock_check: {runtime} exited {r.returncode}: {r.stderr.strip()}")
            return 1
        live = parse_dump(r.stdout)
        if live != tree:
            diff = sorted(set(live) ^ set(tree)) + sorted(i for i in live if i in tree and live[i] != tree[i])
            problems.append(f"the static extraction and the real plugin disagree on {len(diff)} id(s), "
                            f"first {diff[:6]}: fix the extractor (the plugin is the truth)")
        tree = live
    fails, new = compare(load_lock(), tree)
    problems += fails
    if new:
        problems.append(f"{len(new)} new id(s) not in the lock: {new[:12]}{' ...' if len(new) > 12 else ''} "
                        "-- append-only is fine, but record them: python3 tools/param_id_lock_check.py --append")
    if problems:
        print("param_id_lock_check: RED")
        for p in problems:
            print("  " + p)
        return 1
    ids = sorted(tree)
    print(f"param_id_lock_check: GREEN ({len(tree)} host-visible ids {ids[0]}..{ids[-1]} all in the lock "
          f"unchanged; {'real plugin == static extraction; ' if runtime else ''}{planted} controls fired as expected)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
