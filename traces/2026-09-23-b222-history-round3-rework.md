# b222-history-round3-rework — critic's review of PR #732: saved-precision agreement, field-granular adoption, route depths gated, routing read on restore only

- **Queue item:** B222 (rework on PR #732). This corrects `traces/2026-09-23-b222-history-round3.md`, which it cites rather than edits.
- **Why:** The critic's review found one blocker and three further items.
  1. **B1 (blocker).** `morphAdoptUncontested` split a group on exact `!=`. The host chunk and presets store corners at %.6g, while a live capture keeps full precision. After an ordinary save and reopen, a cell nobody morphed held 0.123457 in one corner and 0.123456789 in another. That handed the whole routing block to the field and reverted the player's routing edit: the human's defect again, through a session reload.
     - **Fix:** corners agree when `|a-b| <= 5e-6*max(|a|,|b|)`, the largest move %.6g rounding can make at any magnitude.
     - **Why a tolerance and not a %.6g text compare:** this runs on the audio thread, and snprintf does not belong there.
     - **Why not the suggested `5e-7*max(1,|a|)`:** that bound is too tight above 1. 1234.5678 saves as 1234.57, off by 2.2e-3, while the bound allows only 6.2e-4.
  2. **NOTE 4 (blend-mode granularity).** Chose to follow the field's own granularity rather than keep the group rule everywhere.
     - In blend, a continuous slot is its own weighted sum and the lead map is never consulted (`morphStep`), so it adopts per slot.
     - In quantum, and for stepped slots in blend, the field picks per group, so it adopts per group. Quantum keeps the group rule for the reason the group exists: a corner table with one routing cell swapped for a live value is a topology no corner authored (ADR-175's cycle hazard).
  3. **S3 (mod-route depths).** Lossless route depths were claimed but untested. A new row loads a route at depth 0.123456789 through the preset door's `modRoutes` key and asserts the node carries the exact text. The critic's plant (`modRoutesChunk(false)` in `stateJson`) now turns it red.
  4. **S4 (preset routing).** `applyStateJson` read `"routing"` from any JSON, which would have silently decided B193's key and a preset-load behaviour. It now reads the key only with `historyRestore=true`, which only `undoGoTo`/`undoStep` pass. The "restore IS a load" row was adjusted to compare the two doors on everything except that one deliberate difference.
- **Evidence consulted:** the critic's probes (the session scratchpad `plant/probe.inc`, `probe2.inc`); `tools/gen_state_fixtures.cpp:258` (the `modRoutes` door); `morphStep` (blend vs quantum target computation).
- **Rows, failing before and passing after** (undo_check). The "before" results were taken on `c868e73` for B1 and S4, and by reverting each fix alone for NOTE 4 and S3:
  - `B1: after an ordinary save + reopen, the routing edit STILL survives morph-on`: FAIL before, OK after. Its control (no reopen) is OK on both builds.
  - `S4: a preset carrying a routing key ... leaves the matrix untouched (cell 0.250000, want 0.7)`: FAIL before, OK after. Its control (a history restore does move the matrix) is OK.
  - `NOTE 4, BLEND ... (X 0.000000 want 0.6)`: FAIL with the group rule. `NOTE 4, QUANTUM` (the stated rule, and the must-fire) is OK on both.
  - `S3: the history node holds the route depth EXACTLY`: FAIL under the plant, OK after.
  - The earlier plants (label, adopt, routing, lossless) still fire.
- **Alternatives rejected:** a %.6g text compare (needs snprintf on the audio thread); the group rule in blend (it discards a live cell for another cell's disagreement, which the field never couples); reading `routing` from presets (human-gated).
- **Verify:** see the PR comment. `./verify full` was run on the committed hash, and the exit code was read from `.harness/last-verify.json`.
- **Open questions:** unchanged from the cited trace. The items the lead is recording as human rulings (automation morph-on, the dropped record-time audio row, the `ens=` exemption, the 0.25 s settle) were not acted on.
