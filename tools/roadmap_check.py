#!/usr/bin/env python3
"""roadmap_check — every ROADMAP queue row has a unique id, three cells and a status word (B674).

WIRED: ./verify fast

WHY. The queue is one table of 700+ rows whose status used to be free prose, so no tool and no
grep could say what is open (Fable audit 2026-11-10, finding 6; ADR-393 item 4). Now cell 3
starts with one word from STATUSES, optionally ` · ` and references, so `grep '| OPEN'` is true.

RULES (each queue-table row, `| B<n> |` or `| GB<n> |`, between the `| # | Item | Status |` header
and the next heading):
  * the id is unique, except the pinned duplicates (shared with design_chain_check.KNOWN_DUPLICATES);
  * exactly three cells, where `|` separates unless escaped `\\|` or inside a backtick code span;
  * cell 3 is `<STATUS>` or `<STATUS> · <anything>`, STATUS from the closed list below.
GUI-owned rows (every GB row and the B rows docs/design/CLAUDE.md lists, ADR-336) are not the
lead's to edit, so a nonconforming one is allowed only while its id is in
tools/roadmap_gui_pending.txt. That list may only shrink: a listed row that now conforms prints
`now conforms: remove <id> from the list` (not a failure); a nonconforming GUI row not listed is red.

MUST-FAIL CONTROLS run every time on in-memory copies: a duplicated id, a two-cell row, an unknown
status word, a status word not at the start of cell 3 and a new nonconforming GUI row each read
red, a conforming synthetic row reads green, or the check itself is red.
"""
import ast
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
STATUSES = ["IN PROGRESS", "OPEN", "QUEUED", "BLOCKED", "PROPOSED", "PARKED", "DONE",
            "SUPERSEDED", "WONTDO", "UNSTATED"]
STATUS_RE = re.compile(r"^(" + "|".join(STATUSES) + r")( · .*)?$")
HEADER = "| # | Item | Status |"
ROW_RE = re.compile(r"^\| (G?B\d+) \|")
# docs/design/CLAUDE.md "Owns" (ADR-336); every GB row is GUI-owned as well.
GUI_B_ROWS = {"B261", "B532", "B555", "B556", "B557", "B581", "B582", "B583", "B584", "B589",
              "B591", "B593", "B594", "B595", "B596"}


def known_duplicates():
    src = (ROOT / "tools/design_chain_check.py").read_text(encoding="utf-8")
    m = re.search(r"^KNOWN_DUPLICATES = (\{.*\})$", src, re.M)  # read the pin, never import a checker
    return ast.literal_eval(m.group(1)) if m else {}


def is_gui(rid):
    return rid.startswith("GB") or rid in GUI_B_ROWS


def split_cells(line):
    """Return (cells, seps): cell strings between unescaped, non-code-span pipes (outer pipes
    dropped) and the index of every separator pipe in `line`."""
    seps, i, n = [], 0, len(line)
    while i < n:
        c = line[i]
        if c == "\\":
            i += 2
            continue
        if c == "`":
            j = i
            while j < n and line[j] == "`":
                j += 1
            run = j - i
            k = j
            while k < n:  # a span closes at the next run of exactly the same length
                if line[k] == "`":
                    e = k
                    while e < n and line[e] == "`":
                        e += 1
                    if e - k == run:
                        break
                    k = e
                else:
                    k += 1
            i = (k + run) if k < n else j  # unclosed run is literal
            continue
        if c == "|":
            seps.append(i)
        i += 1
    cuts = [-1] + seps + ([n] if not seps or seps[-1] != n - 1 else [])
    cells = [line[a + 1:b] for a, b in zip(cuts, cuts[1:])]
    return cells[1:] if cells and cells[0].strip() == "" and seps and seps[0] == 0 else cells, seps


def table_rows(text):
    """(lineno, line) of each queue row, plus stray non-blank non-row lines inside the table."""
    lines = text.split("\n")
    try:
        h = lines.index(HEADER)
    except ValueError:
        return [], ["queue table header not found"]
    rows, stray = [], []
    for k in range(h + 2, len(lines)):
        l = lines[k]
        if l.startswith("#"):
            break
        if ROW_RE.match(l):
            rows.append((k + 1, l))
        elif l.strip():
            stray.append(f"line {k + 1}: text inside the queue table that is not a row")
    return rows, stray


def check(text, pending, dups):
    errs, notes, counts, seen, gui_pending = [], [], {}, {}, 0
    rows, stray = table_rows(text)
    errs += stray
    for ln, l in rows:
        rid = ROW_RE.match(l).group(1)
        seen[rid] = seen.get(rid, 0) + 1
        cells, _ = split_cells(l)
        why = None
        if len(cells) != 3:
            why = f"{len(cells)} cells, want 3"
        else:
            m = STATUS_RE.match(cells[2].strip())
            if not m:
                why = "cell 3 does not start with a status word"
            else:
                counts[m.group(1)] = counts.get(m.group(1), 0) + 1
        if why is None:
            if is_gui(rid) and rid in pending:
                notes.append(f"now conforms: remove {rid} from the list")
        elif is_gui(rid):
            if rid in pending:
                gui_pending += 1
            else:
                errs.append(f"line {ln}: GUI row {rid}: {why}, and it is not in roadmap_gui_pending.txt")
        else:
            errs.append(f"line {ln}: {rid}: {why}")
    for rid, c in seen.items():
        if c > dups.get(rid, 1):
            errs.append(f"{rid} appears {c} times (allowed {dups.get(rid, 1)})")
    return errs, notes, counts, len(rows), gui_pending


def load_pending():
    p = ROOT / "tools/roadmap_gui_pending.txt"
    if not p.exists():
        return set()
    return {l.strip() for l in p.read_text(encoding="utf-8").split("\n")
            if l.strip() and not l.lstrip().startswith("#")}


def controls(text, pending, dups):
    rows, _ = table_rows(text)
    last = rows[-1][1]
    base = text.replace(last, last + "\n" + "{}", 1)
    cases = [  # (name, row appended after the last real row, must be red?)
        ("conforming row", "| B9001 | item | OPEN · § x |", False),
        ("duplicated id", None, True),
        ("two-cell row", "| B9002 | item |", True),
        ("unknown status word", "| B9003 | item | FINISHED · § x |", True),
        ("status not at start", "| B9004 | item | § ADR-1 · DONE |", True),
        ("new nonconforming GUI row", "| GB9999 | item | whenever |", True),
    ]
    bad, base_errs = [], set(check(text, pending, dups)[0])  # a control is judged on NEW errors
    for name, row, want_red in cases:
        if row is None:
            row = last
        errs = set(check(base.replace("{}", row), pending, dups)[0]) - base_errs
        if bool(errs) != want_red:
            bad.append(f"control '{name}' read {'red' if errs else 'green'}, want {'red' if want_red else 'green'}")
    return bad


def main():
    text = (ROOT / "ROADMAP.md").read_text(encoding="utf-8")
    pending, dups = load_pending(), known_duplicates()
    errs, notes, counts, n, gp = check(text, pending, dups)
    errs += controls(text, pending, dups)
    for x in notes:
        print(x)
    for x in errs:
        print(f"roadmap_check: {x}", file=sys.stderr)
    per = ", ".join(f"{s} {counts.get(s, 0)}" for s in STATUSES)
    print(f"roadmap_check: {'RED' if errs else 'GREEN'} ({n} rows; {per}; {gp} GUI rows pending)")
    if not errs:
        print("roadmap_check: controls ok")
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
