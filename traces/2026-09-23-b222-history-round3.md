# b222-history-round3 — morph toggle label, morph-on keeps the patch, routing + lossless corners in history

- **Queue item:** B222 (ROADMAP on `lead-records-82`, PR #729); folds in the history half of B193.
- **Why:** The human heard three history defects. The brief asked for diagnosis first, with every cause shown by a check that fails before the fix. Each cause below was first measured by a scratch probe with a must-read-zero control, then gated in `tools/undo_check.cpp` layer 5 and `tools/labharness/gui_history_check.mjs`. Each fix was then reverted on its own and its rows went red (the L0059 plants).
  1. *Morph on had no history entry of its own.* The shell made exactly one node per toggle, but the label was wrong. Since B191, gui2 brackets the value change as begin, set, END. The END's `undoMarkParam(151)` then overwrote guiSetParam's "morph on"/"morph off" with the display name "Morph". The rail folds same-label chains into one row (`histRowsOf`), so on-then-off drew as "Morph ×2". Red before: `got: Morph / Morph / Morph / Morph`.
  2. *Morph on reset the routing matrix.* The lead's hypothesis was right about the mechanism: stale corners. It was wrong about the scope: this is not specific to routing. After ANY state load (the Init patch, a preset, or a history restore), morph on reverted every morphable edit made since. Red before: `cell 1.000000, detune 0.280000` against the edited 0.25/0.777. Every load sets `morphCornersAuthored`, because every stateJson carries a morph chunk. So the seed adoption never ran, even when all four corners were identical.
  3. *A restore depended on the road taken.* Every snapshot restored byte-identically, which is why the B186 gauntlet stayed green. But the routing matrix was in no snapshot (B193), and a node with morph on rewrites the matrix from its corners as soon as audio runs. Red before: the engine's routing readout and the audio both differed by path, while the same road taken twice rendered identically. A second cause surfaced once the first was fixed: the node writer rounded morph corners to %.6g.
- **Fixes (src/hypersaw_clap.cpp):**
  - `undoMarkParam` keeps a pending "morph o…" label on 151.
  - `morphAdoptUncontested`: on the EDITOR's off→on only (the new `editorWrite` flag, set for queue kind 0), each morph group whose four corners agree adopts the live values. Groups whose corners differ stay the field's. Host automation of 151 is unchanged, and a row pins that.
  - `historyJson` is what a node now stores: `stateJson(lossless)` plus an always-present `"routing"` key. `applyStateJson` queues routing (kind 3) only when that key is present. Preset JSON, the host chunk, and preset loads are byte-for-byte unchanged.
  - The routing chunk parse is shared (`routingChunkCells`).
  - `guiSetParam` is one function with two callers: the bridge and the new headless `setmorph` op. A `live` op was also added.
- **Evidence consulted:** ROADMAP rows B222/B193/B189/B186 (origin/lead-records-82). Code read: `src/hypersaw_clap.cpp` (applyParam 151, morphInit, initState, applyStateJson, undoService/undoGoTo, state_save/state_load, morphStep helpers), `src/undo_tree.h`, `src/swarm_core.h` (ensemble timing), `tools/undo_check.cpp`, `tools/labharness/gui_history_check.mjs`, and INDEX (L0032, L0033, L0036, L0059, L0061).
- **Alternatives rejected:**
  - Putting routing in stateJson (B193's literal fix). That changes the preset format and what every preset load does to the matrix, which is a human gate.
  - Clearing `morphCornersAuthored` on loads whose corners agree. That would re-open the seed adoption mid-load, over half-applied values.
  - Snapping `masterVolSm` or the morph glide on activate. That changes existing renders.
  - A tolerance on audio rows. It would hide the %.6g rounding the corner-readout row now catches.
- **Verify:** `./verify full` exit 0 on `ea5aa6b` (read from `.harness/last-verify.json`). Plants: each fix reverted in isolation turns its own rows red (D1 1 row, D2 3, routing 7, lossless 3).
- **Open questions:**
  - (a) The `ens=` ensemble-timing stream is not in history. It is pinned as a BOUNDARY row: whether a restore should rewind the phrase is the human's call.
  - (b) The morph glide cache (`morphCur`) and the master-volume declick are in-flight motion, not state. A node recorded mid-glide restores already landed on its target (up to 0.063 peak difference under a long Morph Glide). The fixed render settles 0.25 s for the declick. Neither is fixed.
  - (c) Host automation of Morph after a load still reverts live edits. That behaviour is pinned, pending a human ruling.
  - (d) `modRoutes` depth is now lossless in history. This follows from the code but is unverified, because no headless door creates a generic route.
  - (e) The op list in `src/hypersaw_debug.h` (out of scope) does not yet name `live`/`setmorph`.
  - (f) B189 is untouched. It is a recording defect and not a cause here.
