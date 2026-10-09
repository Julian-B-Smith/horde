#!/usr/bin/env python3
"""manual_scaffold_check -- the user manual's placeholders and its figure registry agree.

WIRED: ./verify fast

WHY (B451). The user manual starts as scaffolding: one markdown file per chapter
(docs/manual/NN-slug.md) carrying a section outline and placeholder lines for
every figure, diagram, table, screenshot and audio example, plus a registry
(docs/manual/figures.json) that says what each one must show, where its truth
comes from and what it waits on. Two lists that describe the same set drift
apart silently: a placeholder nobody registered is a figure nobody will make,
and a registry entry nobody places is work done for a page that no longer asks
for it. Neither is visible to any audio oracle, so a check holds them together.

WHAT IT CHECKS (both directions, plus the shape of each side):
  * every `[[FIG:id]]`, `[[TAB:id]]`, `[[AUD:id]]` in a chapter has a registry
    entry, and every registry entry has exactly one placeholder;
  * no id appears twice, in the registry or in the chapters;
  * every entry has every field, a known `kind` and `status`, a boolean
    `generated`, a `chapter` that names the file holding its placeholder, and a
    kind that matches the placeholder prefix (FIG = figure, diagram or
    screenshot; TAB = table; AUD = audio);
  * every `depends_on` id exists: an H2-PLAN part id (the parts table between
    its `parts-table` markers), a ROADMAP `| Bxxx |` row, or a DECISIONS
    `## ADR-xxx` heading;
  * every H2/H3 section of a chapter carries exactly one line
    `Status: PENDING — <ids>` or `Status: READY-TO-WRITE — <ids>`, citing at
    least one ROADMAP row or ADR, each of which must exist.
Chapters are the files named NN-slug.md; README.md is the conventions page and
is not scanned, because it shows the placeholder syntax as an example.

THE KNOWN-ID SOURCES ARE NOT A SECOND LIST. They are parsed from H2-PLAN.md,
ROADMAP.md and DECISIONS.md on every run. If any of the three parses to zero ids
the check is red as BLIND: a pattern that stops matching must not read as
"every id is unknown" (red for the wrong reason) or, worse, be special-cased.

MUST-FAIL CONTROLS (LIBRARY L0032), every run, on planted copies of
docs/manual in a temp dir, never touching the repo's files: an orphan
placeholder, an orphan registry entry, a duplicate id, a bad status, an unknown
`depends_on`, a missing field, and a section with its status line removed. Each
must read red WITH the error that names its fault (a control that goes red for
some other reason proves nothing). A positive control (the untouched copy reads
green) keeps the judge from being always-red. Any control that goes the wrong
way makes this check red.
"""
import copy
import json
import re
import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANUAL = "docs/manual"
REGISTRY = "figures.json"

CHAPTER_RE = re.compile(r"^\d\d-[a-z0-9]+(?:-[a-z0-9]+)*\.md$")
PLACEHOLDER_RE = re.compile(r"\[\[(FIG|TAB|AUD):([^\]]*)\]\]")
ID_RE = re.compile(r"^[a-z0-9]+(?:[.-][a-z0-9]+)*$")
HEADING_RE = re.compile(r"^(#{1,6})\s+(.*\S)\s*$")
STATUS_RE = re.compile(r"^Status:\s+(PENDING|READY-TO-WRITE)\s+—\s+(.*\S)\s*$")
# Anything that LOOKS like a status line but is malformed must not pass as prose.
STATUS_LOOSE_RE = re.compile(r"^\s*\**Status\**\s*:", re.IGNORECASE)
CITE_RE = re.compile(r"\b(B\d+|ADR-\d+)\b")

FIELDS = ("id", "kind", "chapter", "purpose", "content", "source",
          "depends_on", "status", "generated")
KINDS = {"figure", "diagram", "table", "screenshot", "audio"}
STATUSES = {"needed", "pending-design", "ready-to-make"}
PREFIX_KINDS = {"FIG": {"figure", "diagram", "screenshot"},
                "TAB": {"table"},
                "AUD": {"audio"}}


def known_ids(root):
    """(parts, rows, adrs) parsed from the three record files under root."""
    plan = (root / "docs/H2-PLAN.md").read_text(encoding="utf-8")
    m = re.search(r"<!-- parts-table:start -->(.*?)<!-- parts-table:end -->", plan, re.S)
    parts = set()
    if m:
        for line in m.group(1).splitlines():
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if len(cells) >= 2:
                pm = re.fullmatch(r"`([a-z0-9]+)`", cells[1])
                if pm:
                    parts.add(pm.group(1))
    roadmap = (root / "ROADMAP.md").read_text(encoding="utf-8")
    rows = set(re.findall(r"^\| (B\d+) \|", roadmap, re.M))
    decisions = (root / "DECISIONS.md").read_text(encoding="utf-8")
    adrs = set(re.findall(r"^## (ADR-\d+)\b", decisions, re.M))
    return parts, rows, adrs


def scan_chapter(name, text, rows, adrs, errors):
    """Placeholders [(prefix, id, line)] of one chapter; section errors appended."""
    found = []
    h1 = h2 = 0
    sections = []          # [heading, line, [status lines]]
    in_fence = False
    for n, line in enumerate(text.splitlines(), 1):
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
            continue
        for pm in PLACEHOLDER_RE.finditer(line):
            found.append((pm.group(1), pm.group(2), n))
        if in_fence:
            continue
        hm = HEADING_RE.match(line)
        if hm:
            level = len(hm.group(1))
            if level == 1:
                h1 += 1
            if level == 2:
                h2 += 1
            if level in (2, 3):
                sections.append([hm.group(2), n, []])
            elif level == 1:
                sections.append([None, n, []])
            continue
        if STATUS_LOOSE_RE.match(line):
            if not sections or sections[-1][0] is None:
                errors.append(f"{name}:{n}: status line outside any H2/H3 section")
            else:
                sections[-1][2].append((n, line))
    if h1 != 1:
        errors.append(f"{name}: has {h1} H1 titles, needs exactly 1")
    if h2 == 0:
        errors.append(f"{name}: has no H2 sections")
    for heading, n, statuses in sections:
        if heading is None:
            continue
        if not statuses:
            errors.append(f"{name}:{n}: section '{heading}' lacks a status line")
            continue
        if len(statuses) > 1:
            errors.append(f"{name}:{n}: section '{heading}' has {len(statuses)} status lines")
        for sn, sline in statuses:
            sm = STATUS_RE.match(sline)
            if not sm:
                errors.append(f"{name}:{sn}: malformed status line (want "
                              f"'Status: PENDING|READY-TO-WRITE — <ids>'): {sline.strip()}")
                continue
            cites = CITE_RE.findall(sm.group(2))
            if not cites:
                errors.append(f"{name}:{sn}: status cites no ROADMAP row or ADR")
            for c in cites:
                if c not in rows and c not in adrs:
                    errors.append(f"{name}:{sn}: status cites unknown id {c}")
    return found


def check(manual_dir, known):
    """All errors for a manual directory against known (parts, rows, adrs)."""
    parts, rows, adrs = known
    errors = []
    for label, ids in (("H2-PLAN parts", parts), ("ROADMAP rows", rows), ("DECISIONS ADRs", adrs)):
        if not ids:
            errors.append(f"BLIND: parsed zero {label}; the id source moved or its pattern broke")
    chapters = sorted(p for p in manual_dir.iterdir() if CHAPTER_RE.match(p.name))
    if not chapters:
        errors.append(f"BLIND: no NN-slug.md chapters in {manual_dir}")
    stems = {p.stem for p in chapters}

    placed = {}            # id -> (prefix, chapter stem, line)
    for p in chapters:
        for prefix, pid, n in scan_chapter(p.name, p.read_text(encoding="utf-8"), rows, adrs, errors):
            if not ID_RE.match(pid):
                errors.append(f"{p.name}:{n}: malformed placeholder id '{pid}'")
                continue
            if pid in placed:
                errors.append(f"duplicate id '{pid}': placeholder at {p.name}:{n} "
                              f"and {placed[pid][1]}.md:{placed[pid][2]}")
                continue
            placed[pid] = (prefix, p.stem, n)

    reg_path = manual_dir / REGISTRY
    try:
        reg = json.loads(reg_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as e:
        return errors + [f"{REGISTRY}: unreadable: {e}"]
    entries = reg.get("entries") if isinstance(reg, dict) else None
    if not isinstance(entries, list):
        return errors + [f"{REGISTRY}: needs a top-level object with an 'entries' list"]

    seen = set()
    for i, e in enumerate(entries):
        if not isinstance(e, dict):
            errors.append(f"{REGISTRY}: entry {i} is not an object")
            continue
        eid = e.get("id", f"<entry {i}>")
        missing = [f for f in FIELDS if f not in e]
        if missing:
            errors.append(f"entry '{eid}': missing field(s) {', '.join(missing)}")
        extra = sorted(set(e) - set(FIELDS))
        if extra:
            errors.append(f"entry '{eid}': unknown field(s) {', '.join(extra)}")
        if eid in seen:
            errors.append(f"duplicate id '{eid}' in {REGISTRY}")
            continue
        seen.add(eid)
        for f in ("id", "chapter", "purpose", "content", "source"):
            if f in e and (not isinstance(e[f], str) or not e[f].strip()):
                errors.append(f"entry '{eid}': field '{f}' must be a non-empty string")
        if "kind" in e and e["kind"] not in KINDS:
            errors.append(f"entry '{eid}': unknown kind '{e['kind']}'")
        if "status" in e and e["status"] not in STATUSES:
            errors.append(f"entry '{eid}': unknown status '{e['status']}'")
        if "generated" in e and not isinstance(e["generated"], bool):
            errors.append(f"entry '{eid}': 'generated' must be true or false")
        deps = e.get("depends_on")
        if "depends_on" in e:
            if not isinstance(deps, list) or not deps:
                errors.append(f"entry '{eid}': depends_on must be a non-empty list")
            else:
                for d in deps:
                    if d not in parts and d not in rows and d not in adrs:
                        errors.append(f"entry '{eid}': unknown depends_on id '{d}'")
        if "chapter" in e and e["chapter"] not in stems:
            errors.append(f"entry '{eid}': chapter '{e['chapter']}' is not a chapter file")
        if eid not in placed:
            errors.append(f"orphan entry '{eid}': no placeholder in any chapter")
            continue
        prefix, stem, n = placed[eid]
        if "chapter" in e and e["chapter"] != stem:
            errors.append(f"entry '{eid}': chapter '{e['chapter']}' but its placeholder is in {stem}.md")
        if e.get("kind") in KINDS and e["kind"] not in PREFIX_KINDS[prefix]:
            errors.append(f"entry '{eid}': kind '{e['kind']}' does not match placeholder [[{prefix}:...]]")
    for pid, (prefix, stem, n) in sorted(placed.items()):
        if pid not in seen:
            errors.append(f"orphan placeholder [[{prefix}:{pid}]] at {stem}.md:{n}: no registry entry")
    return errors


# ---- must-fail controls -------------------------------------------------------

def _plant(tmp, fault):
    """Apply one fault to the copied manual in tmp."""
    reg_path = tmp / REGISTRY
    reg = json.loads(reg_path.read_text(encoding="utf-8"))
    first = reg["entries"][0]
    chapter = sorted(p for p in tmp.iterdir() if CHAPTER_RE.match(p.name))[0]
    if fault == "orphan placeholder":
        chapter.write_text(chapter.read_text(encoding="utf-8") + "\n[[FIG:control.orphan-placeholder]]\n",
                           encoding="utf-8")
        return
    if fault == "section lacks status":
        lines = chapter.read_text(encoding="utf-8").splitlines(keepends=True)
        k = next(i for i, ln in enumerate(lines) if STATUS_RE.match(ln.rstrip("\n")))
        del lines[k]
        chapter.write_text("".join(lines), encoding="utf-8")
        return
    if fault == "orphan entry":
        e = copy.deepcopy(first)
        e["id"] = "control.orphan-entry"
        reg["entries"].append(e)
    elif fault == "duplicate id":
        reg["entries"].append(copy.deepcopy(first))
    elif fault == "bad status":
        first["status"] = "done"
    elif fault == "unknown depends_on":
        first["depends_on"] = ["B99999"]
    elif fault == "missing field":
        del first["source"]
    reg_path.write_text(json.dumps(reg, indent=2), encoding="utf-8")


CONTROLS = (
    ("orphan placeholder", "orphan placeholder [[FIG:control.orphan-placeholder]]"),
    ("orphan entry", "orphan entry 'control.orphan-entry'"),
    ("duplicate id", "duplicate id"),
    ("bad status", "unknown status 'done'"),
    ("unknown depends_on", "unknown depends_on id 'B99999'"),
    ("missing field", "missing field(s) source"),
    ("section lacks status", "lacks a status line"),
)


def controls(src, known):
    """Failures of the controls themselves (an empty list means all went the right way)."""
    bad = []
    with tempfile.TemporaryDirectory() as d:
        clean = Path(d) / "clean"
        shutil.copytree(src, clean)
        errs = check(clean, known)
        if errs:
            bad.append(f"positive control: the untouched copy reads red ({errs[0]})")
        for fault, expect in CONTROLS:
            tmp = Path(d) / fault.replace(" ", "-")
            shutil.copytree(src, tmp)
            _plant(tmp, fault)
            errs = check(tmp, known)
            if not any(expect in e for e in errs):
                bad.append(f"control '{fault}': did not read red with \"{expect}\" "
                           f"(got {len(errs)} error(s){': ' + errs[0] if errs else ''})")
    return bad


def main():
    manual = ROOT / MANUAL
    if not manual.is_dir():
        print(f"manual_scaffold_check: RED — {MANUAL} is missing", file=sys.stderr)
        return 1
    known = known_ids(ROOT)
    errors = check(manual, known)
    # Controls run every time, red tree or not. On a red tree the positive control
    # is red too; that is redundant but honest, and it never hides a blind control.
    bad = controls(manual, known)
    if errors or bad:
        for e in errors + bad:
            print(f"manual_scaffold_check: {e}", file=sys.stderr)
        print(f"manual_scaffold_check: RED — {len(errors)} fault(s), {len(bad)} control failure(s)",
              file=sys.stderr)
        return 1
    reg = json.loads((manual / REGISTRY).read_text(encoding="utf-8"))
    n_ch = sum(1 for p in manual.iterdir() if CHAPTER_RE.match(p.name))
    print(f"manual_scaffold_check: GREEN — {n_ch} chapters, {len(reg['entries'])} registry entries, "
          f"{len(CONTROLS)} must-fail controls red + positive control green")
    return 0


if __name__ == "__main__":
    sys.exit(main())
