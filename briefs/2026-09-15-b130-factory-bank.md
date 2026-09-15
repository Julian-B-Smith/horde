# Dispatch brief — B130: the factory bank content + bank_check

**Provenance.** HYPERSAW lead organ, 2026-09-15, for a scoped subagent with zero
conversation history. Motivating rows: B102 (the 1.0 definition of done's
"30–50 categorised factory presets built to demonstrate the coupling laws"),
split into B129 (store path + install, another agent, disjoint files) and B130
(this). Read ROADMAP rows B102, B129, B130, B103, ADR-152, ADR-159, ADR-162
(B122 cornerNames) and CLAUDE.md first. The engine you are writing presets for
is the Kuramoto swarm (`src/swarm_core.h`; the walkthrough in
`traces/2026-09-0*-kuramoto*.md` if present); the parameter table is
`kParams` in `src/hypersaw_clap.cpp`.

## Acceptance criteria (verbatim from ROADMAP B130)

> (1) `docs/presets/factory/<category>/<name>.json` — at least 30 and at most 50, categories `lead · bass · pad · pluck · keys · fx · morph · demo`, each file a CURRENT-LAYOUT patch captured through the headless rig (`hypersaw_debug_apply` on a hand-authored parameter set, then `hypersaw_debug_state`, so every file carries `morphLayout 2`, `cornerNames`, the state header and the ADR-152 flatten — self-contained, never hand-typed corner arrays). (2) Required exemplars, each named on the row and asserted by `bank_check`: **cloud-to-lock sweep** (K from a cloud to lock; R rises past 0.9 within 2 s of a strike at K 1, stays under 0.3 at K 0), **consonance-gravity chord** (two notes a sharp fifth settle within ±1 c of 3/2 by 3 s), **splay-interference** (K −1: the even-lattice gap 1/n, R under 0.1), **quantum-morph** (four deliberately different corners: corner A cloud, B lock, C splayed, D two-cluster balance 1 — the four R/RA/RB signatures differ pairwise by ≥ 0.4), plus **one per FX type** that exists today, and **one ORBITAL-shaped mod-matrix demo** is NOT required (the module does not exist). (3) `tools/bank_check.cpp`, standalone and unwired: loads every file (must apply, `statefix`-style bit-identical re-save = load), renders 2 s of a C4 strike — RMS above −60 dBFS, no NaN/Inf, peak under 0 dBFS — and asserts each exemplar's claim above through a new headless export `hypersaw_debug_viz(p, osc, &R, &RA, &RB, &n)` (the same numbers the GUI's phase circle reads); every file's `cornerNames` are either "" or the name of a shipped corner preset in `docs/presets/factory/corners/`. (4) Every preset has a one-line description in `docs/presets/factory/BANK.md` (what it demonstrates, which knob to touch first) — the teaching line is part of the product (B103). (5) No shell change except the one export; no GUI change; the install path is B129's

## Where things are

- Headless exports: `hypersaw_debug_apply`, `hypersaw_debug_state`,
  `hypersaw_debug_cornerapply`, `hypersaw_debug_cornername`,
  `hypersaw_debug_cornervals` near the end of `src/hypersaw_clap.cpp`; add
  `hypersaw_debug_viz` beside them reading `hostIf.getViz()`'s snapshot
  fields (`R`, `RA`, `RB`, `n` — see `VizSnapshot` in `src/gui/hypersaw_gui.h`
  and the producer `hostIf.getViz = ` in the shell).
- Rig: `tools/notefuzz_scaffold.inc` (include `<algorithm>` first for MSVC);
  `tools/preset_probe.cpp`, `tools/penv_check.cpp` and
  `tools/morphlayout_check.cpp` are worked examples of apply → process →
  read; `tools/statefix_check.cpp` shows the bit-identical re-save assertion.
- Authoring: write a small generator `tools/gen_factory_bank.cpp` (or a
  Python + the check binary) that holds the hand-authored parameter sets AS
  CODE (name → {id: value} + corner sets + corner preset names), applies each
  through the rig, captures `hypersaw_debug_state`, and writes the JSON files
  — so the bank is reproducible and a param-table change regenerates it.
  Commit the generated files too (they ARE the bank).
- Corner presets for the morph exemplars: write them as
  `docs/presets/factory/corners/<name>.json` through `hypersaw_debug_cornerapply`
  → the corner-preset writer (`hzMorphCornerJson` semantics — find the shell
  function `cornerJson(k)` and expose a debug export if none reads it).
- What the numbers mean: R is the order parameter (1 = locked, ~0 = cloud /
  splayed); RA/RB are the two-cluster orders; the trajectory oracle
  `tools/trajectory_check.cpp` shows the settle windows and thresholds the
  repo already trusts — reuse them rather than inventing new ones.

## Files in scope

NEW `docs/presets/factory/**` (json + BANK.md + corners/), NEW
`tools/bank_check.cpp`, NEW `tools/gen_factory_bank.cpp` (or `.py`),
`src/hypersaw_clap.cpp` (the ONE export), `CMakeLists.txt` (two targets),
`traces/2026-09-15-b130-factory-bank.md`.

**OUT of scope:** the store path / install / GUI (B129's agent); `ROADMAP.md`
/ `DECISIONS.md`; `./verify` and gates; protected paths; any RT path;
untracked root files. B129's agent touches `CMakeLists.txt` and the GUI —
REBASE onto main before opening your PR, keep both hunks.

## Constraints

Branch from `main` (pull first); absolute build paths; `./verify fast` after
each change set, gate every scripted commit on its exit code; `./verify full`
before done; paste oracle output verbatim; no machine identity or private
sibling names in tracked files (preset names are yours to invent — keep them
about the sound, not about people); MSVC in CI.

## Deliverable

Branch `b130-factory-bank`, pushed, PR via `gh pr create --base main` whose
body lists the bank by category with each teaching line, then `bank_check`
output and the `./verify full` tail pasted from the run. **Never merge.**
Final report: PR URL, the bank list, check output, verify tail, the
ROADMAP/DECISIONS text you would add.
