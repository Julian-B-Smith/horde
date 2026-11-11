# b674-roadmap-check — roadmap_check in verify fast, and one records pass over the lead's queue rows

- **Queue item:** B674 (ADR-393 item 4; docs/audits/2026-11-10-fable-audit.md finding 6, step 1)
- **Why:** the queue's status was free prose, so no grep or tool could list what is open. Cell 3 of every row now starts with one word from a closed list, and a check holds it.
- **Evidence consulted:** docs/audits/2026-11-10-fable-audit.md finding 6; ROADMAP.md queue table (header line 7041, rows to line 7750); docs/design/CLAUDE.md "Owns"; tools/design_chain_check.py KNOWN_DUPLICATES; the lead's brief (measured facts: 705 rows).
- **Alternatives rejected:** guessing a status from plain-text words (the brief forbids it; those rows are UNSTATED); renumbering B147/B155 (ADR-346; the pin is reused, read from design_chain_check.py by regex, never imported).
- **Verify:** `nice -n 10 ./verify fast` exit 0 in the worktree (base 60d1b71b plus this change set); roadmap_check GREEN, controls ok.
- **Open questions:** at the end.

## What changed
- **tools/roadmap_check.py** (new, wired in ./verify fast beside design_chain_check; header WIRED). Rules per queue row (header to next heading): unique id except the pinned B147 and B155; exactly three cells (a pipe separates unless escaped with a backslash or inside a backtick span); cell 3 is STATUS, or STATUS then " · " and anything, with STATUS in OPEN, QUEUED, IN PROGRESS, BLOCKED, PROPOSED, PARKED, DONE, SUPERSEDED, WONTDO, UNSTATED. Non-blank text inside the table that is not a row is red. GUI-owned rows (every GB row; B261, B532, B555, B556, B557, B581-B584, B589, B591, B593-B596) may be nonconforming only while listed in tools/roadmap_gui_pending.txt (the list may only shrink; a listed row that now conforms prints "now conforms: remove <id> from the list").
- **Controls** (in memory, every run, judged on new errors): a conforming synthetic row reads green; a duplicated id, a two-cell row, an unknown status word, a status word not at the start of cell 3 and a new nonconforming GB row each read red. Prints "controls ok".
- **tools/roadmap_gui_pending.txt** (new): the 51 GUI-owned rows that do not conform (2 other GUI rows already conform and are not listed).
- **ROADMAP.md**: 652 lead rows touched (650 distinct ids; B147 and B155 each appear twice), cell separators and cell 3 only, plus one text join (B438, below). GUI rows are byte-identical (no GB or GUI-owned id appears in the diff). No handover notes to the GUI session exist in lead rows (the one hit, B656 "GUI needs (handed to the GUI session)", hands over needs, not the row), so none were treated as handed over.
- **verify**: one wiring line plus a comment.

## The rule of the pass (script in the session scratchpad, not committed)
Bold spans in the row whose text starts with a mapped word not followed by a letter: DONE SHIPPED MERGED CLOSED COMPLETE to DONE; OPEN; QUEUED READY PENDING to QUEUED; IN PROGRESS; BLOCKED; PROPOSED; PARKED DEFERRED to PARKED; SUPERSEDED RETIRED REPLACED to SUPERSEDED; WONTDO, WON'T DO, DROPPED to WONTDO. A mapped word at the very start of old cell 3 counts as one more vote (and is normalised, not duplicated). Exactly one class gives the status; none or several give UNSTATED. Three cells: cell 3 becomes "STATUS · old cell 3". Two cells: " STATUS |" appended. More than three: the pipes between the id/item separator and the last two separators are escaped, so the id cell, the item cell and the LAST cell are the three cells; cell 3 is then the last cell.

## Counts (lead rows, 652)
Before: 2 rows (both OPEN, GUI-owned) conformed; 425 rows had no bold status word (the lead's figure). After: DONE 57, OPEN 171 (includes those 2 GUI rows), IN PROGRESS 14, QUEUED 1, PROPOSED 4, PARKED 3, BLOCKED 0, SUPERSEDED 0, WONTDO 0, UNSTATED 404; 51 GUI rows pending. A bare `grep -c '^| G\?B[0-9]* |.*| OPEN'` gives 176, not 171, because B532, B555, B556, B557, B674 and B517 contain "| OPEN" in prose or an escaped pipe; the check's cell-3 count is the true one.
UNSTATED is large (404) because the rule forbids plain-text guessing; 16 of them are multi-class. B674's own row now reads QUEUED (its bold word); the lead updates it.

## Rows that were odd
- Two-cell rows (82): got their status appended (UNSTATED unless a bold word said otherwise).
- More than three cells (53 rows): cell counts {'B77': 7, 'B93': 4, 'B94': 4, 'B95': 4, 'B126': 5, 'B147': 5, 'B146': 5, 'B153': 7, 'B159': 5, 'B225': 7, 'B267': 5, 'B177': 5, 'B172': 5, 'B52': 4, 'B50': 7, 'B46': 5, 'B38': 4, 'B300': 4, 'B315': 4, 'B351': 28, 'B355': 7, 'B405': 9, 'B528': 4, 'B529': 4, 'B530': 4, 'B531': 4, 'B533': 4, 'B534': 4, 'B535': 4, 'B536': 4, 'B537': 4, 'B538': 4, 'B539': 4, 'B540': 4, 'B541': 4, 'B542': 4, 'B543': 4, 'B544': 4, 'B545': 4, 'B546': 4, 'B547': 4, 'B548': 4, 'B549': 4, 'B550': 4, 'B551': 4, 'B552': 4, 'B553': 4, 'B554': 4, 'B558': 4, 'B559': 4, 'B560': 4, 'B561': 4, 'B562': 4}.
- **B351 (28 cells)**: a small markdown table (Measure, AUC, Leave-one-out, Cut; aliasDb, aliasConvDb, aliasTotalDb) pasted inside prose. Its pipes are escaped; the last cell (prose ending "MERGED 2026-09-29 (#853) ...") became cell 3. Multi-class (OPEN and MERGED), so UNSTATED.
- **B405 (9 cells)**: absolute-value bars in prose, escaped. Its last cell is prose ending MERGED; the item and refs went into cell 2.
- B77, B126, B153, B225, B267, B50, B355 and others: pipes in math or a pasted second status; B177 and B172 had an empty fourth cell between two reference cells. All escaped; the last cell is cell 3.
- **B438**: its cell 3 continued after a blank line in a stray paragraph (the records follow-up note on ADR-275), which broke the table. Joined into the row (closing pipe and blank line removed, one space). The only text-level edit beyond separators and cell 3.
- B147 and B155: left as two rows each (the pin); not renumbered.

## Multi-class rows (bold words disagree, so UNSTATED)
B52, B351, B364, B365, B460, B466, B467, B487, B491, B493, B514, B526, B537, B562, B522, B396

## Round-trip proof
For each of the 652 changed rows (scratch script): split the new row, require exactly three cells and a status prefix; removing that prefix (or, for two-cell rows, the appended " STATUS |"), un-escaping the added pipes and comparing the text before and after cell 3 gives the original row. Result: round trip ok 652, bad 0. B438 is checked by diff instead: one insertion (" UNSTATED ·") plus the join. ROADMAP.md diff: 653 insertions, 655 deletions (the extra two deletions are B438's fragment).

## GUI pending list (51, in tools/roadmap_gui_pending.txt)
B261, B532, B555, B556, B557, B581, B582, B583, B584, B589, B591, B593, B594, B595, B596, GB1, GB2, GB3, GB4, GB5, GB6, GB17, GB13, GB19, GB32, GB21, GB22, GB23, GB24, GB14, GB15, GB33, GB34, GB35, GB36, GB37, GB38, GB16, GB25, GB28, GB26, GB27, GB30, GB31, GB29, GB12, GB10, GB9, GB7, GB8, GB11

## UNSTATED rows (404, ids only)
B1, B2, B3, B4, B5, B6, B7, B8, B9, B10, B11, B18, B19, B20, B21, B22, B24, B25, B26, B12, B13, B15, B43, B42, B44, B70, B73, B74, B75, B76, B78, B79, B80, B81, B87, B88, B89, B90, B91, B92, B93, B94, B96, B97, B98, B100, B111, B112, B113, B114, B115, B116, B117, B118, B119, B120, B122, B123, B124, B125, B126, B127, B128, B132, B133, B134, B135, B136, B137, B138, B139, B140, B141, B142, B143, B144, B145, B147, B146, B147, B148, B149, B150, B151, B152, B153, B154, B155, B155, B156, B157, B158, B160, B161, B162, B163, B164, B165, B166, B167, B168, B169, B170, B181, B184, B187, B188, B189, B191, B193, B195, B202, B203, B213, B212, B216, B219, B220, B221, B218, B217, B215, B214, B222, B223, B224, B225, B226, B227, B228, B231, B232, B233, B234, B235, B236, B237, B240, B241, B242, B243, B244, B245, B246, B247, B248, B249, B250, B251, B239, B238, B230, B229, B254, B256, B257, B258, B259, B260, B262, B263, B264, B265, B266, B267, B268, B269, B270, B271, B272, B273, B275, B276, B277, B278, B279, B280, B281, B282, B283, B284, B285, B286, B252, B253, B207, B208, B209, B210, B211, B206, B205, B204, B201, B200, B199, B198, B196, B197, B194, B192, B190, B186, B185, B182, B180, B179, B176, B177, B178, B175, B173, B174, B171, B172, B101, B102, B103, B109, B107, B108, B99, B82, B83, B84, B85, B86, B69, B68, B66, B67, B64, B65, B63, B62, B57, B56, B55, B53, B52, B49, B48, B47, B46, B45, B41, B39, B40, B16, B287, B288, B289, B290, B31, B30, B29, B291, B292, B293, B294, B295, B296, B299, B300, B301, B302, B303, B304, B305, B307, B308, B309, B310, B311, B312, B313, B314, B315, B316, B317, B318, B319, B320, B321, B37, B35, B34, B32, B322, B323, B324, B325, B326, B327, B328, B329, B330, B331, B332, B33, B333, B334, B335, B336, B337, B338, B347, B350, B351, B352, B354, B360, B364, B365, B378, B379, B405, B407, B414, B416, B417, B422, B423, B435, B438, B439, B442, B443, B444, B445, B446, B447, B448, B449, B450, B452, B453, B454, B456, B457, B458, B460, B466, B467, B471, B472, B474, B487, B491, B493, B494, B499, B503, B504, B506, B507, B508, B511, B514, B526, B537, B542, B544, B545, B546, B547, B548, B549, B559, B560, B561, B562, B563, B565, B566, B599, B609, B611, B612, B613, B614, B615, B616, B617, B618, B619, B620, B621, B622, B623, B627, B628, B658, B654, B662, B666, B644, B519, B522, B390, B393, B396, B399, B17

## Verify
roadmap_check: GREEN (705 rows; IN PROGRESS 14, OPEN 171, QUEUED 1, BLOCKED 0, PROPOSED 4, PARKED 3, DONE 57, SUPERSEDED 0, WONTDO 0, UNSTATED 404; 51 GUI rows pending); controls ok. `nice -n 10 ./verify fast` exit 0, with test_table_check, weakening_check, design_chain_check and tolerance_registry_check green in that run.

## Open questions
- 404 UNSTATED rows need a lead or human pass to say what they are (the closed-list words are not in their bold text). Many are probably old DONE rows with plain-text status.
- B517's escaped pipe in its last cell makes a plain grep miss it; not touched.
