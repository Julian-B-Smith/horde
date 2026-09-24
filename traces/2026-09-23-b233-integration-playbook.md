# b233-integration-playbook: a playbook for wiring a new source through every seam, and the check that keeps its citations honest

- **Queue item:** B233. The row was read with `git show origin/lead-records-84:ROADMAP.md | grep '^| B233 |'`, and B232 and B238 were read beside it.
  - B233's deliverable, verbatim: "`docs/playbooks/integrating-a-source.md`: every seam a new source or parameter block must be wired through, each with the file:line of the existing mechanism, the invariant it must keep, the check that proves it, and the incident that taught it … Written from THIS repo's evidence, not generic advice, and paired with a machine-checkable list where one is cheap."
- **Why:** The human asked for the playbook directly, and B238 (the swarm extension) will be its first use. The document is only worth anything if it is accurate, so it was written from the code at `origin/main` `e07acac`. A check was added that re-reads its citations on every `./verify fast`, so it cannot silently rot.

## What changed

- **`docs/playbooks/integrating-a-source.md`** (new). It covers ten seams:
  1. ids and engine blocks
  2. class
  3. the morph field: order, three passes, resolution, level ramps, exempt vs absent, corner identity, B222 adoption, and B232 as PROPOSED
  4. the mod matrix
  5. history
  6. presets and state
  7. presentation and GUI
  8. RT safety and determinism
  9. parity
  10. checks

  Each seam gives (a) the mechanism, (b) the invariant, (c) the check and (d) the incident. The document also contains:
  - a B238 pre-read;
  - 14 places where the docs and the code disagree;
  - 6 open questions;
  - a checklist for the PR description.
- **`tools/playbook_check.py`** (new, `WIRED: ./verify fast`). It has three rules:
  - Every `` `path:line anchor` `` citation resolves. A missing file, a vanished anchor or an unanchored citation FAILS. A line that has only moved is printed as drift and counted.
  - The four `morphLayout` writers agree, and there are exactly four of them.
  - `src/*.h` and `src/*.cpp` contain no clock or unseeded-RNG tokens, with comments stripped.

  An in-run selftest feeds each rule inputs that must fail and inputs that must pass.
- **`verify`**: one line in `fast()`, with its reason (ADR-180 §1: adding a check is not gated).
- **`README.md`**: a Map row for `docs/playbooks/`. There is no `CODEMAP.md`, and nothing else lists playbooks, so the README map is the nearest index.

## Evidence consulted

- **Shell, read directly:** `src/hypersaw_clap.cpp` (the param table, the engine blocks, `paramClassOf`, `morphInit`, `morphStep`/`intentApply`, the B222 adoption, `modAddRoute`/`modStep`, the undo seam, `stateJson`/`initState`/`applyStateJson`/`historyJson`, `applyParam`/`readParam`, host param text, `state_save`/`state_load`).
- **Other source:** `src/mod_core.h`, `src/undo_tree.h`, `src/depends_graph.h`, `src/gui/gui2.html` (`gestureFor`, `NO_HISTORY_IDS`, `MOD_SRC_NAMES`, `modDestOptions`, the `data-when` evaluator), `src/param_presentation.tsv`.
- **Tools and gates:** `verify`, and the tool headers and code for `presentation_check`, `depends_check`, `gen_depends_header`, `gen_gui_controls`, `gui_reach`, `test_table_check`, `compact_lab_table_check`, `gui_history_check.mjs`, `morphlayout_check` (T1, T1b, T10b), `paramclass_check` (T1a pin 266), `undo_check`, `paramscope_check`, `rtsafety_probe`, `bank_check` (row E), `gen_factory_bank`, `subosc_check`.
- **Records:** ROADMAP rows B48, B100, B187, B191, B192, B193, B195, B203, B213, B219, B220, B222, B232, B233 and B238 (from `origin/lead-records-84`). DECISIONS ADR-108, ADR-109 and ADR-173. INDEX in full. LIBRARY L0063.

## Findings the lead should see (all also in the playbook, §12 and §13)

1. **A new per-osc or global instrument row cannot reach the morph field's tail today.**
   - Adding it to the table puts it inside the frozen prefix.
   - Adding it to `kMorphLateIds` inserts it before the routing block.
   - `morphlayout_check` T1 or T1b goes RED either way.

   B238 needs a ruling on the append site first.
2. **The engine blocks' three passes insert rather than append for any future engine row.** That includes a new block (STATION) and a new SUB row. T10b cannot see it. Raising `kRoutingNSrc` inserts routing cells mid-block in the same way. Both are read from the code, not measured.
3. **The depends graph flattens AND to OR** in `depends_graph.h`, and it cannot name engine keys. B232's "by declaration" needs tooling work and a ruling on the declaration channel before it can cover the SUB.
4. **`readParam` returns before the ADR-136 base intercept for engine, routing and shell-owned ids.** So a modulated SUB row probably reads back its modulated value. This is a hypothesis; a one-route probe would settle it.
5. **`rtsafety_probe` never switches morph on and never adds a route.** Those paths are outside its window.
6. **Some `baseIdOf` sites are not guarded against engine ids:** the host text for rows 35–37, the morph enable test for row 150, and the GUI destination menu for rows 161+. They are safe today only because the SUB block is 20 rows long.

## Calibration

- **In-run selftest:**
  - Citations: 5 cases, plus one "an ordinary code span is not a citation" control.
  - Marker: agree, split and vanished.
  - Determinism: 6 must-catch cases and 5 must-pass cases, the latter including comment prose and `operand(`.
- **Planted faults on the real corpus** (scratch script, not committed):
  - Renaming a real anchor gave 1 FAIL.
  - Moving a real citation's line by 2 gave 0 FAIL and 1 drift, which reported the true line.
  - Bumping one of the shell's three marker writers to 10 FAILED with a list of all four writers.
  - Inserting `#include <chrono>` into a copy of `subosc_core.h` FAILED at the right line.
  - The unplanted corpus read clean in all three rules.

## Alternatives rejected

- **Failing on citation drift.** Every edit above a cited line would turn an unrelated PR red. That is a gate cost the human did not ask for.
- **Computing `morphIds` order statically in Python** to freeze the whole layout. That would be a second copy of `morphInit`'s rule, and it belongs as a binary-driven row in `morphlayout_check`, which is proposed as Q2.
- **Creating `CODEMAP.md`.** It would invent an index nobody reads. The README map already exists.

## Verify

- `./verify fast` on `5a68812`: exit 0, according to `.harness/last-verify.json` (`{"target":"fast","exit":0,"git":"5a68812"}`).
- `./verify full` was NOT run. The change adds no C++ and touches no binary, and the fast leg is where the new check lives. Before closing B233 the lead should run full on the merge commit, per the charter.

## Open questions

These are Q1–Q6 in the playbook's §13:

- **Q1.** Where do new instrument rows append in the morph field, and should the T1b band move to admit them?
- **Q2.** Should the whole layout-9 order be frozen as a fixture?
- **Q3.** What channel does B232 use to declare which switch a parameter belongs to, and does AND become real in the engine header?
- **Q4.** Should readback report the base for modulated engine and shell-owned ids?
- **Q5.** Should `rtsafety_probe`'s window be widened to include morph and the mod matrix?
- **Q6.** Should `gui_reach` and `presentation_check` cover engine and routing ids, or should their docstrings say they do not?
