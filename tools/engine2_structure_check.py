#!/usr/bin/env python3
"""engine2_structure_check -- the new engine's declared structure says only what its spec says
(ROADMAP B500; loupe's response integrations/hypersaw/response-engine-structure.md, in loupe's tree).

WIRED: ./verify fast

WHY. engine2/structure/engine-structure.json is the structure of the new engine (its blocks, the
reserved slots, the boundary, and the edges between them), declared as data so that loupe draws the
diagram from that file alone, with no inference. A diagram that cites a requirement the spec does not
have, or silently drops a stage of the spec's signal path, is a wrong picture that looks right, so the
file is gated against the spec itself. The builder never sees the file (spec BD-10; the clean-room
recipe excludes engine2/structure/): it is a reading of the spec, and the builder reads the spec.

WHAT IT CHECKS (each failure carries its tag, which the controls assert):
  format        the format string is "horde.engine-structure/1", the edge kinds are the closed list,
                and every entry has exactly its fields (engine2/structure/README.md lists them);
  ids           every item and edge id is unique; an edge's id is "<from>-><to>";
  dangling      every parent and every edge endpoint names an item;
  cycle         the nesting has no cycle;
  scope         an item is "engine" exactly when the engine item is it or an ancestor of it;
  edge-kind     every edge kind is in the closed list;
  unknown-cite  every cite exists in the spec: a requirement id from its tables, bullets and headings
                (SW-4, BD-10, RC-6.1, Q31, ...), a law (C.0 to C.16, C.9.1 ...), a family tag from a
                section heading (SW, BL, ...) or a numbered section ("§4.2", "§B.1"), all parsed from
                the spec's own text, never listed here;
  signal-path   every stage of §4.2's diagram, parsed from the diagram itself and in its order, is
                mapped to at least one declared item or edge, and nothing else is;
  rc-missing    every reserved contract RC-n that the spec heads is cited by an item;
  counts        the declaration's own counts (items by kind and scope, edges by kind, stages) are true;
  deny-list     no file under engine2/structure/ carries an old-engine identifier, and no id or file
                name is named like the old engine's files (tools/engine2_cleanroom_check.py's lists,
                imported, so there is one deny list);
  spec-changed  spec.blob is the git blob hash of specs/SPEC-ENGINE.md as it is now. Every spec edit
                turns this red on purpose: the declaration is reviewed against the new spec, then
                spec.blob is updated.
  canonical     the file is its canonical form: JSON with sorted keys and two-space indents, items and
                edges ordered by id, cites in natural order (`--canonical` prints it).

MUST-FAIL CONTROLS, every run, on in-memory copies of the declaration: an unknown spec id, a missing
§4.2 stage, a missing RC slot, a dangling edge, a parent cycle, a stale blob, a wrong count, an old
identifier and an unknown edge kind must each read red with their own tag; the declaration's canonical
form must read back to the same bytes, and two evaluations must print the same report.

  python3 tools/engine2_structure_check.py              the gate
  python3 tools/engine2_structure_check.py --stages     print §4.2's stages as parsed
  python3 tools/engine2_structure_check.py --canonical  print the declaration in canonical form
"""
import copy
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
# the one deny list: the clean-room check owns it, this check imports it
from engine2_cleanroom_check import OLD_IDENTIFIERS, OLD_NAME

SPEC = "specs/SPEC-ENGINE.md"
STRUCTURE_DIR = "engine2/structure"
DECLARATION = f"{STRUCTURE_DIR}/engine-structure.json"
FORMAT = "horde.engine-structure/1"
EDGE_KINDS = ["audio", "control", "event", "modulation", "observation"]
ITEM_KINDS = ("block", "slot")
SCOPES = ("engine", "external")
ENGINE_ID = "engine"
TOP_KEYS = ("counts", "edge_kinds", "edges", "format", "items", "signal_path", "spec")
ITEM_KEYS = ("cites", "id", "kind", "label", "multiplicity", "parent", "scope")
EDGE_KEYS = ("cites", "from", "id", "kind", "label", "to")
STAGE_KEYS = ("ids", "stage")


def git_blob(data: bytes) -> str:
    """git's blob hash, computed here so the check needs no git (the file has no filters)."""
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def natural(s: str):
    return [int(p) if p.isdigit() else p for p in re.split(r"(\d+)", s)]


def canonical(doc) -> str:
    return json.dumps(doc, indent=2, ensure_ascii=False, sort_keys=True) + "\n"


# ---- what the spec contains ------------------------------------------------------------------

def spec_ids(spec_text: str) -> set:
    """Every citable id, read off the spec's own headings, requirement tables and bullets."""
    ids = set()
    for line in spec_text.splitlines():
        m = re.match(r"^\| ([A-Z]{2}-\d{1,2}[a-z]?|[QX]\d{1,2}) \|", line)  # SW-1, ED-1a, IF-11, Q31, X13
        if m:
            ids.add(m.group(1))
        ids.update(re.findall(r"\*\*(BD-\d+)\.\*\*", line))               # - **BD-1.** ...
        ids.update(re.findall(r"\*\*(RC-\d+\.\d+)\b", line))            # **RC-6.1** ..., **RC-11.1 No ...**
        m = re.match(r"^#### (RC-\d+)\. ", line)                          # #### RC-1. The filters
        if m:
            ids.add(m.group(1))
        m = re.match(r"^#{3,4} (C\.\d+(?:\.\d+)?)[ ,]", line)              # ### C.6 Gravity, #### C.9.1
        if m:
            ids.add(m.group(1))
        m = re.match(r"^## (\d+)\. ", line)                               # ## 4. Architecture
        if m:
            ids.add("§" + m.group(1))
        m = re.match(r"^### (\d+\.\d+|[AB]\.\d+) ", line)                 # ### 4.2 ..., ### B.1 ...
        if m:
            ids.add("§" + m.group(1))
        m = re.match(r"^## Appendix ([A-C])\. ", line)                    # ## Appendix C. Laws
        if m:
            ids.add("§" + m.group(1))
        m = re.match(r"^### \d+\.\d+ .*\(([A-Z]{2})\)$", line)            # ### 6.1 The swarm (SW)
        if m:
            ids.add(m.group(1))
    return ids


def spec_contracts(spec_text: str) -> list:
    return re.findall(r"^#### (RC-\d+)\. ", spec_text, re.MULTILINE)


ROW_LABEL = re.compile(r"^([a-z0-9 ]+):\s+(.+)$")


def signal_path_stages(spec_text: str) -> list:
    """The stages of §4.2's diagram, in reading order, each "row: stage" inside a source's row.

    Read off the fenced diagram: a line's tokens are what lies between arrows (─►) or wide gaps; a
    line whose first token is "<label>: ..." opens that label's row, a line that starts with an arrow
    continues it, any other line stands alone; "a + b" is two stages; a later token's own "label: "
    (as in "each member: base wave") names a scope the row already carries and is dropped.
    """
    section = spec_text.split("\n### 4.2 ", 1)[1].split("\n### ", 1)[0]
    diagram = section.split("```", 2)[1]
    stages, row = [], None
    for line in diagram.splitlines():
        text = re.sub(r"─+ (one voice) ─+", r"   \1   ", line)
        text = re.sub(r"─+[┐┤┘]", " ", text)
        text = re.sub(r"[│┌┐└┘├┤┬┴▼]", " ", text)
        continues = text.lstrip().startswith("─")
        if not continues:
            opened = ROW_LABEL.match(text.strip())
            row = opened.group(1) if opened else None
            if opened:
                text = opened.group(2)
        # a border line is all dashes once its corners are gone: it holds no stage
        tokens = [t.strip() for t in re.split(r"─*►|\s{3,}", text) if t.strip(" ─")]
        for index, token in enumerate(tokens):
            label = ROW_LABEL.match(token)
            if index > 0 and label:
                token = label.group(2)
            for stage in token.split(" + "):
                stages.append(f"{row}: {stage.strip()}" if row else stage.strip())
    return stages


# ---- the declaration -------------------------------------------------------------------------

def true_counts(doc) -> dict:
    items, edges = doc.get("items", []), doc.get("edges", [])
    return {
        "edges": {k: sum(1 for e in edges if e.get("kind") == k) for k in EDGE_KINDS},
        "items": {k: sum(1 for i in items if i.get("kind") == k) for k in ITEM_KINDS},
        "scope": {s: sum(1 for i in items if i.get("scope") == s) for s in SCOPES},
        "stages": len(doc.get("signal_path", [])),
    }


def problems_of(doc, spec_text: str, spec_blob: str, files: dict, text: str = None) -> list:
    """-> [(tag, message)], sorted. Pure: the same inputs give the same list."""
    out = []

    def bad(tag, msg):
        out.append((tag, msg))

    # The fields are fixed, so loupe's adapter can read every item the same way and a typo'd key
    # (which json would carry silently) reads red here.
    if sorted(doc) != sorted(TOP_KEYS):
        bad("format", f"top-level keys are {sorted(doc)}, not {sorted(TOP_KEYS)}")
    for group, keys in (("items", ITEM_KEYS), ("edges", EDGE_KEYS), ("signal_path", STAGE_KEYS)):
        for entry in doc.get(group, []):
            if not isinstance(entry, dict) or sorted(entry) != sorted(keys):
                bad("format", f"{group} entry {entry.get('id', entry.get('stage')) if isinstance(entry, dict) else entry!r}: "
                              f"keys are not exactly {sorted(keys)}")
    if doc.get("format") != FORMAT:
        bad("format", f"format is {doc.get('format')!r}, not {FORMAT!r}")
    if doc.get("edge_kinds") != EDGE_KINDS:
        bad("format", f"edge_kinds is {doc.get('edge_kinds')!r}, not the closed list {EDGE_KINDS}")
    spec = doc.get("spec", {})
    if spec.get("path") != SPEC:
        bad("format", f"spec.path is {spec.get('path')!r}, not {SPEC!r}")
    if spec.get("blob") != spec_blob:
        bad("spec-changed", f"the spec changed: review the declaration, then update spec.blob "
                            f"(declared {spec.get('blob')}, {SPEC} is now {spec_blob})")

    items, edges = doc.get("items", []), doc.get("edges", [])
    seen = {}
    for thing in items + edges:
        seen[thing.get("id")] = seen.get(thing.get("id"), 0) + 1
    for name, n in sorted(seen.items(), key=lambda kv: str(kv[0])):
        if n > 1:
            bad("ids", f"id {name!r} is used {n} times")
    by_id = {i.get("id"): i for i in items}

    for item in items:
        iid = item.get("id")
        if item.get("kind") not in ITEM_KINDS:
            bad("format", f"item {iid}: kind {item.get('kind')!r} is not one of {ITEM_KINDS}")
        if item.get("scope") not in SCOPES:
            bad("format", f"item {iid}: scope {item.get('scope')!r} is not one of {SCOPES}")
        if not isinstance(item.get("label"), str) or not item["label"]:
            bad("format", f"item {iid}: no label")
        parent = item.get("parent")
        if parent is not None and parent not in by_id:
            bad("dangling", f"item {iid}: parent {parent!r} is not an item")

    for item in items:   # walk to the root; a revisit is a cycle
        chain, here = [], item.get("id")
        while here is not None and here in by_id and here not in chain:
            chain.append(here)
            here = by_id[here].get("parent")
        if here is not None and here in chain:
            bad("cycle", f"item {item.get('id')}: its parents loop ({' -> '.join(chain + [here])})")
        elif here is None:
            inside = ENGINE_ID in chain
            want = "engine" if inside else "external"
            if item.get("scope") in SCOPES and item.get("scope") != want:
                bad("scope", f"item {item.get('id')}: scope {item.get('scope')!r}, but it is "
                             f"{'inside' if inside else 'outside'} the engine")

    for edge in edges:
        eid = edge.get("id")
        for end in ("from", "to"):
            if edge.get(end) not in by_id:
                bad("dangling", f"edge {eid}: {end} {edge.get(end)!r} is not an item")
        if eid != f"{edge.get('from')}->{edge.get('to')}":
            bad("ids", f"edge {eid}: its id is not \"<from>-><to>\"")
        if edge.get("kind") not in EDGE_KINDS:
            bad("edge-kind", f"edge {eid}: kind {edge.get('kind')!r} is not in the closed list {EDGE_KINDS}")

    known = spec_ids(spec_text)
    for thing in items + edges:
        cites = thing.get("cites")
        if not isinstance(cites, list) or not cites:
            bad("unknown-cite", f"{thing.get('id')}: cites nothing; every item and edge rests on the spec")
            continue
        for cite in cites:
            if cite not in known:
                bad("unknown-cite", f"{thing.get('id')}: cites {cite!r}, which the spec does not have")
        if cites != sorted(set(cites), key=natural):
            bad("canonical", f"{thing.get('id')}: cites are not unique and in natural order")

    for rc in spec_contracts(spec_text):
        if not any(rc == c or c.startswith(rc + ".") for i in items for c in i.get("cites", [])):
            bad("rc-missing", f"{rc} is a reserved contract of the spec and no item cites it")

    stages = signal_path_stages(spec_text)
    path = doc.get("signal_path", [])
    declared = [entry.get("stage") for entry in path]
    for stage in stages:
        if stage not in declared:
            bad("signal-path", f"§4.2 stage {stage!r} is not in signal_path")
    for stage in declared:
        if stage not in stages:
            bad("signal-path", f"signal_path stage {stage!r} is not a stage of §4.2")
    if sorted(declared) == sorted(stages) and declared != stages:
        bad("signal-path", "signal_path is not in §4.2's reading order (or a stage is listed twice)")
    known_ids = set(by_id) | {e.get("id") for e in edges}
    for entry in path:
        ids = entry.get("ids")
        if not isinstance(ids, list) or not ids:
            bad("signal-path", f"stage {entry.get('stage')!r} maps to nothing")
            continue
        for name in ids:
            if name not in known_ids:
                bad("dangling", f"stage {entry.get('stage')!r} maps to {name!r}, which is not an item or edge")

    if doc.get("counts") != true_counts(doc):
        bad("counts", f"counts {doc.get('counts')} are not the declaration's own {true_counts(doc)}")

    serial = canonical(doc)
    for name, data in sorted(files.items()):
        if OLD_NAME.search(Path(name).name):
            bad("deny-list", f"{name}: named like the old engine's files")
        hit = OLD_IDENTIFIERS.search(data)
        if hit:
            bad("deny-list", f"{name}: {hit.group(0).decode()!r}, an old-engine identifier")
    hit = OLD_IDENTIFIERS.search(serial.encode("utf-8"))
    if hit:
        bad("deny-list", f"the declaration: {hit.group(0).decode()!r}, an old-engine identifier")
    for name in sorted(known_ids, key=str):
        for part in re.split(r"[.>-]+", str(name)):
            if part and OLD_NAME.search(part):
                bad("deny-list", f"id {name!r}: named like the old engine's files")

    if [i.get("id") for i in items] != sorted(by_id, key=str) or \
       [e.get("id") for e in edges] != sorted((e.get("id") for e in edges), key=str):
        bad("canonical", "items and edges are not ordered by id")
    if text is not None and text != serial:
        bad("canonical", f"{DECLARATION} is not in canonical form (print it with --canonical)")
    return sorted(set(out))


# ---- the run ---------------------------------------------------------------------------------

def report(doc, spec_text, spec_blob, files, text):
    """The gate's whole output, as lines, with the controls. Pure: run twice, it prints the same."""
    lines, failures = [], []

    def expect(label, ok, detail=""):
        lines.append(f"engine2_structure_check: {'ok  ' if ok else 'FAIL'} {label}" + ("" if ok else f"\n    {detail}"))
        if not ok:
            failures.append(label)

    found = problems_of(doc, spec_text, spec_blob, files, text)
    expect(f"{DECLARATION} holds against {SPEC}", not found, "\n    ".join(f"[{t}] {m}" for t, m in found))

    def plant(label, tag, change):
        planted = copy.deepcopy(doc)
        change(planted)
        tags = {t for t, _ in problems_of(planted, spec_text, spec_blob, files)}
        expect(f"control: {label} reads red [{tag}]", tag in tags, f"expected [{tag}], got {sorted(tags)}")

    def first(kind_of, key, value):
        return next(x for x in kind_of if x.get(key) == value)

    def drop_rc10(d):
        gone = {i["id"] for i in d["items"] if any(c.startswith("RC-10") for c in i["cites"])}
        d["items"] = [i for i in d["items"] if i["id"] not in gone]
        d["edges"] = [e for e in d["edges"] if e["from"] not in gone and e["to"] not in gone]

    def cycle(d):
        d["items"][0]["parent"] = d["items"][-1]["id"]
        d["items"][-1]["parent"] = d["items"][0]["id"]

    plant("an unknown spec id", "unknown-cite", lambda d: d["items"][0]["cites"].append("SW-99"))
    plant("a missing §4.2 stage", "signal-path",
          lambda d: d["signal_path"].remove(first(d["signal_path"], "stage", "osc 1: DC removal")))
    plant("a missing RC slot", "rc-missing", drop_rc10)
    plant("a dangling edge", "dangling", lambda d: d["edges"][0].update(to="nowhere"))
    plant("a parent cycle", "cycle", cycle)
    plant("a stale blob", "spec-changed", lambda d: d["spec"].update(blob="0" * 40))
    plant("a wrong count", "counts", lambda d: d["counts"]["edges"].update(audio=d["counts"]["edges"]["audio"] + 1))
    plant("an old identifier", "deny-list", lambda d: d["items"][0].update(label="the SwarmSynth core"))
    plant("an unknown edge kind", "edge-kind", lambda d: d["edges"][0].update(kind="sidechain"))
    expect("the canonical form reads back to the same bytes", canonical(json.loads(canonical(doc))) == canonical(doc))

    counts = doc.get("counts", {})
    lines.append(f"engine2_structure_check: {'FAILED' if failures else 'ok'} "
                 f"({counts.get('items', {}).get('block')} blocks, {counts.get('items', {}).get('slot')} slots, "
                 f"{sum(counts.get('edges', {}).values())} edges, {counts.get('stages')} §4.2 stages, "
                 f"9 must-fail controls)" + (f": {'; '.join(failures)}" if failures else ""))
    return lines, failures


def main():
    spec_bytes = (ROOT / SPEC).read_bytes()
    spec_text = spec_bytes.decode("utf-8")
    if "--stages" in sys.argv:
        print("\n".join(signal_path_stages(spec_text)))
        return 0
    text = (ROOT / DECLARATION).read_text(encoding="utf-8")
    doc = json.loads(text)
    if "--canonical" in sys.argv:
        sys.stdout.write(canonical(doc))
        return 0
    files = {p.relative_to(ROOT).as_posix(): p.read_bytes()
             for p in sorted((ROOT / STRUCTURE_DIR).rglob("*")) if p.is_file()}
    lines, failures = report(doc, spec_text, git_blob(spec_bytes), files, text)
    again, _ = report(doc, spec_text, git_blob(spec_bytes), files, text)
    if again != lines:
        lines.append("engine2_structure_check: FAIL two evaluations printed different reports")
        failures.append("determinism")
    print("\n".join(lines))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
