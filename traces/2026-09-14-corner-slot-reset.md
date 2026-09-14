# 2026-09-14 — B124: uncarried corner slots reset to defaults on every load

**Report (human).** Load a global preset, change a corner to a corner preset,
go back in history: that corner in the original preset is messed up until the
plugin is reloaded.

**Repro.** A headless probe (load P from the human's files, corner B ← Q, undo,
reload P) was CLEAN through undo. `morphlayout_check` T8 then reproduced the
mechanism: a 224-entry corner with oscPitch 7.25, followed by a 222-entry
corner preset, left oscPitch at 7.25 (FAIL before the fix). The human's
store mixes 224/222/202-entry arrays.

**Fix.** `src/hypersaw_clap.cpp`: `cornerSlotDefault(i)` / `resetCorner(k)`;
called before filling in `applyMorphChunk` (per corner; exempt cleared) and
`cornerApply`; `cornerMatches` requires uncarried slots at default.

**Oracle.** `morphlayout_check` T8a–c PASS; `./verify full` in the PR body
(`statefix_check` fixtures load into fresh instances, so their goldens are
unaffected by construction).
