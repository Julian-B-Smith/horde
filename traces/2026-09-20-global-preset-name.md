# global-preset-name — the global preset's NAME becomes shell state, so it persists, forks and restores

- **Queue item:** B174 (human, 2026-09-20: "I would like for the global preset
  to persist when you reload the GUI, like the corner presets do … and it
  should have an asterisk if you've edited the patch without saving"; then
  "Let's make sure the global preset's name is stored").

- **Why:** B186's gauntlet (PR #703) proved the STATE was never contaminated by
  the human's branch report — the literal scenario plus 120 seeds restore
  byte-identically. What moved was the DISPLAY: the global preset's name was
  not shell state at all, so no history snapshot could carry it and the page
  simply showed whatever was loaded last, from whichever branch you stood on.
  Putting the name in the shell fixes a reported, user-visible defect and
  delivers the persistence and the asterisk in the same mechanism.

  The B122 corner mechanism is the model, mirrored rather than re-invented:
  (1) the name goes to the SHELL, so it rides the patch chunk, the host chunk
  and every history snapshot; (2) the GUI READS it back instead of remembering
  it (localStorage is unavailable under the plugin's opaque origin, ADR-105
  A2); (3) the asterisk is the shell's own answer to "would applying this
  preset change the patch?", so dirty means one thing and cannot disagree with
  a load.

  **How the naming window was closed.** PR #703 found that a corner preset's
  name could be LOST from history when a GUI frame landed between the load and
  the naming, because `setCornerName` only amends a mark that is still
  *pending*. The global name is not set by a second call at all: it is an
  ARGUMENT to `applyStateJson`, set before that function's own `undoMark`, so
  the state `undoService` will snapshot already carries the name however late
  the pump arrives. An empty argument defers to the patch's own `presetName`
  key — which is how a history restore (whose json IS a snapshot) recovers its
  branch's name — and a patch with no key is unnamed, never "whatever was
  loaded before" (a load is a load, the same rule the routes/intent chunks
  already state). `tools/undo_check.cpp` gates it: a named load followed
  immediately by a pump must produce a node that carries the name.

  **Reuse, and where it stopped.** `jsonNumber` is now ONE reader of "the
  number this JSON gives, or the default when it names no such key", shared by
  `applyStateJson` (what a load does) and `presetMatches` (whether a load would
  change anything) — the two must agree down to the absent-key rule. The
  comparison itself could NOT be shared with `cornerMatches`: that function's
  every line is the morph slot array and its ADR-159 layout remap, while a
  global patch is a parameter key set. What is shared is the law — a key the
  preset does not carry loads as its default, so it must READ as its default to
  match (B124's uncarried-slot rule, one level up).

  **Stated cost.** Saving names the patch but marks no history node (a save
  changes no values), so the node you are standing on keeps the name it was
  captured under. Undoing across a save therefore shows the older name. That is
  arguably honest — the node records a state where the patch was called
  something else — but it is a deliberate choice, not an oversight.

- **Evidence consulted:** `src/hypersaw_clap.cpp` (B122 `setCornerName` /
  `cornerName[4]` / `cornerMatches` / `applyMorphChunk` / `morphJson`;
  `stateJson` / `applyStateJson`; `state_save` / `state_load`; `undoMark` /
  `undoService` / `undoGoTo`), `src/gui/gui2.html` (`syncCornerNames`, the
  preset save/load handlers, `syncFromEngine`'s 500 ms poll, the `.cdirty`
  class and the `.row` three-track grid), `src/gui/hypersaw_gui.h` +
  `hypersaw_gui_common.h` (the corner name trio's bindings),
  `src/hypersaw_debug.h` (the export-declaration rule and the "an export with
  no owner is deleted" law), `tools/statefix_common.h`, `tools/undo_check.cpp`
  on main, and PR #703's layer 3 for the gauntlet idiom
  (`git show origin/history-fidelity-gauntlet:tools/undo_check.cpp`, read
  only — this branch is based on `origin/main`).

- **Alternatives rejected:**
  - *A `setPresetName` call after `applyState`, mirroring `setCornerName`
    exactly* — rejected: it reproduces the very window PR #703 just found.
  - *Changing `hypersaw_debug_apply`'s C ABI to take a name* — rejected: a
    dozen tools hold that prototype, and `extern "C"` turns a drifted prototype
    into a clean link and garbage at runtime (the header's own warning). A new
    `hypersaw_debug_apply_named` with a declared owner instead.
  - *Emitting `"presetName"` unconditionally* — rejected: an empty key would
    change the bytes of every stored chunk and break bank_check's re-save
    identity. Written only when non-empty, the `modRoutes`/`intent`/`routing`
    rule.
  - *A GUI-side "edited since load" flag* — rejected: it is a second opinion
    that can disagree with a load, which is exactly what the corner asterisk
    was designed not to be.
  - *Marking history on save so the current node carries the new name* —
    rejected: it puts non-sonic nodes in a history the human asked not to
    overload. Recorded as the stated cost above.

- **RED before the change:** with the one line that writes `presetName` into
  `stateJson` removed (the pre-B174 world: the name is not in the snapshot),
  rebuilt and re-run, `undo_check` reports
  `RED (6 failures)` — including
  `FAIL scenario: back on the FIRST branch the patch reports Squids, NOT Grackle`
  and `FAIL identity: the load's OWN node carries the name (no naming window)`.
  Restored, the same binary reports `GREEN (0 failures)`.

- **Visual:** `docs/img/b174-preset-name.png` — the Presets cluster after a
  load (name shown, no asterisk) and after one edit (same name, asterisk),
  rendered from `src/gui/gui2.html` with the three new shell bindings stubbed.

- **Verify:** `./verify full`, exit 0, git `3292dc8` — the code commit, per
  `.harness/last-verify.json` (`{"target":"full","exit":0,"git":"3292dc8"}`).
  Re-run on the follow-up commit that filled this line in; that commit touches
  this file only.

- **Open questions:**
  1. The asterisk is a PARAMETER comparison; the morph corners have their own
     asterisks (B122) and a corner edit therefore does not raise the global
     one. Deliberate, but the human may expect otherwise.
  2. `presetMatches` compares the live values against the preset's. A parameter
     the loader clamps or quantises would read as permanently dirty — B186
     reported exactly that shape for `o1.tilt`'s alias clamp (out of scope
     here). The oracle's patches are taken to a load/save fixed point so this
     change's rows do not depend on that defect, but a player loading such a
     preset would see a spurious asterisk until it is fixed.
  3. Saving under a name refuses a `/` (the factory tier is read-only), so the
     shell holds `category/name` for a factory preset while the box shows the
     tail. That is the existing prefill behaviour, made explicit; worth a
     human's eye.
  4. `docs/img/b174-preset-name.png` was committed because the brief names a
     screenshot as acceptance and `docs/img/` is this repo's screenshot home —
     but it was not in the brief's file list.
