# gui-roadmap-conform — the GUI session's 51 ROADMAP rows take ADR-394's status cell

- **Queue item:** none of its own. It is the follow-up ADR-394 asks of the GUI session (B674; ADR-393 item 4): conform the GUI-owned rows pinned in `tools/roadmap_gui_pending.txt`, emptying the list. The lead asked by session message on 2026-11-11, after #1497. The human said "Yes, after it merges".
- **Why:** `tools/roadmap_check.py` (in `./verify fast`) wants every queue row to read `| id | item | STATUS · references |`. The lead could not edit GUI rows (ADR-336), so it pinned them.
- **Evidence consulted:** origin/main at 129dca9b (the merge of #1497); `tools/roadmap_check.py`; `traces/2026-11-11-b674-roadmap-check.md` (the lead's transform rule); each row's own text; DECISIONS.md for GUI-10, GUI-16, GUI-19 and GUI-60, all on main.
- **Verify:** `nice -n 10 ./verify fast`; the result is in the PR.

## What changed
- **ROADMAP.md:** 51 rows. Each has only its separators and cell 3 changed. No lead row is touched.
- **tools/roadmap_gui_pending.txt:** all 51 ids removed. The header stays, and the list is now empty.
- After the change, roadmap_check reads GREEN (706 rows; IN PROGRESS 13, OPEN 210, PROPOSED 5, PARKED 4, DONE 68, UNSTATED 406; 0 GUI rows pending), with its controls ok.

## The rule
Every row becomes `STATUS ·` followed by its old cell 3 unchanged, the same shape as the lead's rows.

- **A bold word at the start of the old cell 3** gives the status, using the lead's map:
  - OPEN gives OPEN (36 rows);
  - DONE gives DONE (5);
  - IN PROGRESS gives IN PROGRESS (1, GB26);
  - DEFERRED gives PARKED (1, GB14).
- **Four-cell rows B532, B555, B556 and B557** read `| id | item | references | OPEN date … |`. The references go into cell 3 first, then ` · ` and the old fourth cell without its leading OPEN. For example: `OPEN · § B531 · § B392 · 2026-10-21 **…**`.
- **B584 and B594** each had one bare pipe in their prose: B584's "Grid | Table", B594's "Macros | Mods". Each is escaped as `\|`, and the bold OPEN gives the status.
- **RULED and BUILT** are not on the closed list. The lead's rule would leave these UNSTATED. As the rows' owner I read them instead, each on its text alone: OPEN where the row names anything still to do, DONE where it names nothing and its ruling is on main.
  - **OPEN:**
    - B581 ("Needed from the lead (B89)");
    - GB19 ("To build");
    - GB23, GB15 and GB25 ("Next: the master");
    - GB3 ("until the engine's is wired");
    - GB32, GB34 and GB30 ("Open: …");
    - GB36 ("Waits on the human's hands").
  - **DONE:**
    - GB5: GUI-16 and GUI-19 are on main, and the row names nothing open;
    - GB35: GUI-60 is on main; the row reads only "BUILT (GUI-60)".
  - **UNSTATED:**
    - GB17: its 1.0 part is built, and its rail is "after 1.0", so it reads as both DONE and PARKED;
    - B261: its cell 3 holds references and dated notes, with no status word.
- Rows whose bold word is OPEN were not re-read, even where the text suggests the work may be finished: GB2 and GB21 read "awaiting the human's merge" or "Built". The lead's rule takes the written word, and this pass does too. Correcting them is a separate pass of reading.

## Round-trip proof
For each changed row, a scratch script removes the `STATUS ·` prefix, puts back the four-cell rows' fourth cell and its OPEN, and unescapes the one added `\|`. The result matches the row on origin/main byte for byte: 51 ok, 0 bad. No row outside the GUI's set changed.

## Engine and FX device
- **Engine impact:** none. This is records only.
- **FX device:** none.

## Open questions
- GB17 and B261 read UNSTATED. Their status is the human's to state.
- Some OPEN rows may be stale; GB2 and GB21 are examples. A reading pass like the lead's B677 could cover the GUI rows too.
