# history-fidelity-gauntlet — a seeded pair-wise regime over the real plugin's history, and the three defects it found

- **Queue item:** B186 (history fidelity gauntlet; human report 2026-09-20 —
  "loading a different preset on a second branch switched the original branch
  over to the new preset and made the changes on top of it instead of on top
  of the initial preset")
- **Why:** Layers 1 and 2 of `undo_check` each check ONE node at a time, which
  is structurally why neither could see the reported bug: it is a claim about
  a PAIR of branches. Layer 3 builds a tree of real depth and breadth from a
  seeded random sequence (edits, global-preset loads, corner-preset loads,
  step-backs, undo/redo) and then asserts, for every ordered pair (a, b), that
  visiting a → b → a lands on a's exact stored state and that visiting b never
  mutates what a stores. Extended `tools/undo_check.cpp` rather than adding a
  sibling: it is already wired (`verify:239`), it IS the history oracle, and a
  sibling would have required editing `./verify` — permitted by ADR-180 §1 but
  strictly more invasive than reusing the wired gate.
- **Evidence consulted:** `src/undo_tree.h`; `src/hypersaw_clap.cpp`
  (`applyStateJson` ~5785, `undoService`/`undoGoTo` ~6052-6092, `setCornerName`
  ~5488, `cornerApply` ~5443, the FX-rack type branch ~6654, the param table
  ~238/~293); `src/fx_rack.h` (`typeAllowed` 275, `kSlotMaxInstances` 270, the
  B117 shadow 292, `fadeLeft` decay 581); `src/swarm_core.h` 1438/1443 (the
  tilt/toneTilt alias); `src/gui/gui2.html` ~2956 (the corner load's two
  awaits); `src/gui/hypersaw_gui_common.h` 487 (hzFrame services the mark
  first) and 549/563 (the bridge verbs are the same doors the probe uses);
  `tools/test_table_check.py` 82; `tools/statefix_common.h`.

## The reported bug: NOT reproduced in state

Run literally (`reportedScenario`): load X, edit, step back, load Y on the
fork, return to the first branch, edit again. Every state assertion is green,
including that loading Y rewrote neither the X node nor the edit-on-X node,
and that returning to the first branch is byte-identical to what that node
recorded. The randomised regime agrees: 120 distinct seeds, ~30 nodes and 132
ordered pairs each, zero cross-branch contamination.

What IS true is the display half, and it is B174's: the **global** preset's
name is not shell state at all, so no snapshot carries it and no restore can
put it back. After the scenario the parameters are X-plus-edits while the
editor's header still reads Y. The player sees the reported bug; the state
under it is correct. Printed by the gauntlet, deliberately not gated — B174
is a different item and building it was out of scope.

## Three real defects the gauntlet did find

1. **FIXED HERE — a corner preset's name could be lost from history.**
   `setCornerName` only amended a *pending* mark. The GUI sets the name in a
   second awaited round trip after the load (`gui2.html` ~2956) and `hzFrame`
   services the pending mark first thing (`hypersaw_gui_common.h:487`), so a
   frame landing between the two awaits snapshotted the corner's new VALUES
   beside its OLD name: the instrument then stood on a node that did not hold
   its state, and the first navigation away silently reverted the dropdown to
   unnamed. A corner name is patch state (B122), so a name change with nothing
   to amend now marks like the edit it is. Three `corner reference` rows were
   RED before the fix and GREEN after, with a must-read-zero control (the root
   node names no preset) green throughout.

2. **REPORTED, NOT FIXED — `o1.tilt` (id 1045) does not survive any state
   round trip.** `swarm_core.h` 1438 and 1443 map `"tilt"` and `"toneTilt"` to
   the same field with different declared ranges ([0.5, 2] vs [-1, 1]), so an
   osc-2 Amp Tilt above 1 is written out as an out-of-range `o1.toneTilt` and
   clamped to 1 on load. Preset load, session reload and history restore lose
   it identically. Excluded from the gauntlet's edit pool behind
   `kAliasGapId`, with a row that re-earns the exclusion every run and goes
   RED the day it is fixed.

3. **REPORTED, NOT FIXED — a state load that must MOVE COMB between slots
   loses COMB entirely.** COMB is the rack's only singleton
   (`fx_rack.h:270`); during the load the slot vacating COMB arms an 80 ms
   crossfade whose shadow still holds the type (`fx_rack.h:292`), so the
   incoming slot's write is refused at the cap
   (`hypersaw_clap.cpp:6664`) microseconds later and nobody retries. Measured
   with audio actually processed, both directions, ending with no COMB at all.
   Excluded behind `kCombType` (no edit writes type 5; the two factory presets
   that place it are held back and the count is printed), with the same
   self-retiring evidence row and an uncapped-type control.

Both exclusions are named, printed and re-proved every run rather than
silently skipped; a gap nobody re-checks is how a gap becomes permanent.

- **Alternatives rejected:**
  - A sibling `tools/history_check.cpp` — would have needed a `./verify` edit
    and a second CMake target to buy nothing the wired gate does not already
    give.
  - Fixing (2) and (3) here — both are rulings about parameter routing and FX
    rack semantics (which engine owns `o1.tilt`; what a load may do to a
    capped type under an armed crossfade), not about history fidelity, and the
    brief puts anything not about history fidelity out of scope.
  - Making the gauntlet process audio so it sees (3) natively — the crossfade
    shadow never decays headlessly (`fadeLeft` only drops in `renderCrossfade`),
    so a non-processing gauntlet reports the harness rather than the product.
    Settling fades everywhere would have cost seconds of audio per seed for a
    defect that is out of scope anyway; only the evidence row processes audio,
    which is where the claim about the product is made.
- **Verify:** `./verify full`, exit 0, git `fda4d19` per
  `.harness/last-verify.json`. `undo_check` GREEN, 110 rows, 3.1 s; green on
  120 distinct seeds and byte-identical across repeat runs of the same seed.

## Open questions

- **Not fixed, needs a ROADMAP row:** the `tilt`/`toneTilt` alias (2) and the
  COMB relocation loss (3). Both are user-visible on ordinary preset and
  session loads, not only in history.
- **Not fixed, not gated, narrow:** `undoGoTo` calls `undoService()` first, but
  `undoService` returns early when the param queue has not drained and
  `undoGoTo` then clears `undoPending` — so a mark taken while nothing drains
  (transport stopped, editor closed) is DISCARDED rather than collapsing into
  the next node, contrary to that function's own comment ("the edit in hand
  becomes a node BEFORE we walk away from it"). Observed headlessly; in a
  processing DAW the window is one audio block wide. Not gated because the
  honest fix needs a main-thread way to know the queue drained, which is an
  RT-discipline question for the human.
- The gauntlet keeps its walk under the 200-node cap on purpose, so slot
  recycling is untested here; eviction remains layer 1's subject.
- The gauntlet never uses the CLAP chunk transport (host save/load) and never
  interleaves `undoMark("host load")` with the GUI's marks. Both are
  reachable; neither was in the human's description.
