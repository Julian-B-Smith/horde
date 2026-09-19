# pad-assign-bend-wheel — two layout gaps closed, macro names single-sourced, and Pitch Bend / Mod Wheel added as MAIN pad destinations

- **Queue item:** unqueued: the human's two rulings of 2026-09-19, dispatched by
  the horde lead as one queue item ("the MAIN pad's assignment surface"),
  three parts / three commits / one PR. Verbatim: (1) "There isn't enough
  padding above the routing matrix on the fx page or within the performance
  section of the set page." (2) "Let's make pitch bend and mod wheel accessible
  to the MAIN XY as well as the macros, and let's include the intent bus names
  in the mapping dropdowns (i.e. 'Macro 1: Space')."

- **Why:** Three changes that each remove a place where one fact was spelled
  twice or reached by a mechanism that does not actually reach it.

  **A — the two gaps.** Both are one defect wearing two hats: a box whose
  spacing came from a layout mechanism that skips it. `#mxPane` carries
  `column-span:all`, which ENDS the multicol flow, so the 10px `column-gap` does
  not apply across that break and the pane butted the clusters above it. SET's
  Performance panel holds generated `.cluster` children, which are in normal
  flow and not in a column track, so they had no gap at all.

  **B — one name per macro.** The macro KNOBS already wore their intent names;
  every MAPPING surface still spelled the macros itself (static `<option>` text,
  the `MOD_SRC_NAMES` const). Two copies of one key chain drift (L0005), and the
  drift here is nasty because it is silent and semantic: rename M3 and the knob
  says "Air" while the dropdown that AIMS at it still says "Macro 3".

  **C — bend and wheel on the pad.** The point of the design is that NEITHER
  destination gets a new write path. Pitch bend is already a parameter (38) that
  the host wheel, the GUI wheel strip and now the pad all write, which is exactly
  why the bend travel law shapes all three identically; a pad-only bend path
  would have been a second place for the law to be forgotten. The Mod Wheel is
  not a parameter at all — it is matrix source 15 behind `hzModWheel` — so the
  pad drives the GUI's own Mod slider and the two become one control.

- **Evidence consulted:**
  - `src/gui/gui2.html` — `#mxPane` / `#mxBody` CSS (the 2026-09-17 pass that
    padded the pane's INSIDE and left the outside gap at zero), the ADR-098 flow
    rule `.ctlcol > *, #pg-FX.on > * { margin: 0 0 10px }` (specificity 1,1,0 —
    why a bare `#mxPane` rule would have silently done nothing), `.cluster`
    padding 8px 10px, `h2 { margin-bottom:6px }`, `paintIntentNames`,
    `MOD_SRC_NAMES`, `asnPid` / `mainMacroIds` / `PADS` / `wirePad` /
    `padRouteLine` / `modWheelCtl`, the bend wheel strip (`bridge.setParam(38, v)`).
  - `src/hypersaw_clap.cpp` — param rows 179/180; `intentPadId`; the assignment
    apply site and read-back; `applyParam(38, ...)`; the channel-0 wheel decode
    `applyParam(38, (v14 - 8192) * (2.0 / 8192.0))`, which is where the ±2 st
    constant is already spelled; `hostIf.setModWheel` → `srcWheel` → `mod.src[15]`.
  - `tools/gen_gui_controls.py` — confirmed the SET block's nested clusters are
    GENERATED (`<div class="cluster"><h2>{group}</h2>`), so the fix had to be CSS
    keyed on nesting, never markup.
  - A DOM parse of gui2.html for nested clusters: exactly two, both in SET's
    Performance panel — so `.cluster .cluster` fixes the class and today touches
    only the reported instance.
  - `tools/intent_check.cpp:2153` — the T8 commentary that ids 179/180 hold the
    pad's pointer and the shell writes NEITHER.
  - LIBRARY L0005 (duplicated key chains), L0023 (a widened range without its UI
    control), L0041/L0026 (TDZ), L0032/L0033 (controls that must not fire),
    L0051/L0056 (run verify, read the log, then commit).

- **Alternatives rejected:**
  - *A pad-only pitch-bend path.* Rejected: it would have been a second place the
    bend travel law could be forgotten. Returning param 38 from the same decoder
    every other assignment already goes through is the whole implementation.
  - *A pad-only bend RANGE.* Rejected: the shell already spells ±2 st (MIDI 1.0 /
    MPE-manager default) at the channel-0 decode. `BEND_WHEEL_ST` borrows that
    number and says so; if a bend-range parameter ever lands, both sites read it
    and the constant goes.
  - *Calling `hzModWheel` directly from the pad.* Rejected: the Mod slider's
    readout would sit stale while the pad moved the source underneath it. Routing
    through `setModWheelUI` makes the slider and the pad one control.
  - *Inserting bend/wheel into the enum near the macros.* Rejected outright —
    every stored patch's 179/180 would change meaning. Appended at 9/10.
  - *Hand-editing SET's nested cluster markup.* Rejected: it is generated, so the
    next `gen_gui_controls.py` run erases it. CSS survives regeneration.
  - *Renaming params 179/180* (they still read "Main X > Macro" although 9/10 are
    not macros). Rejected as out of scope: a host-facing parameter name is public
    interface and needs the human's gate. Noted in the code comment.
  - *Wiring `intent_check` into `./verify`.* Rejected: editing `./verify` is
    out of scope and human-gated. Run manually instead; see below.

- **Verify:** `./verify full` — **exit 0**, git `c822eb8`
  (`.harness/last-verify.json`: `{"target":"full","exit":0,"git":"c822eb8","ts":"2026-09-19T20:35:01Z"}`).
  Every gate GREEN, including `parity_check: 156/156 scenarios within eps=1e-06`,
  `presentation_check`, `gen_gui_controls --check`, `gui_reach`, `paramclass`
  (246 rows unchanged — a widened RANGE adds no row), `preset_check`,
  `bank_check`, `state_check`, `paramscope_check`, `rtsafety_probe`. `./verify
  fast` was additionally run green on each of the three intermediate commits.

  **The gate that earned its keep.** Mid-change, with the shell's range widened
  and the selects not yet extended, `presentation_check` failed:

      param 179: select offers [0, 1, 2, 3, 4, 5, 6, 7, 8], declared range 0..10 -- MISSING [9, 10]
      param 180: select offers [0, 1, 2, 3, 4, 5, 6, 7, 8], declared range 0..10 -- MISSING [9, 10]
      presentation_check: FAILED -- 2 select(s) do not cover their param's range

  That is L0023 exactly — a widened range shipping an invisible feature — caught
  by a gate rather than by care. Recorded because the gate firing FOR REAL, in
  the situation it was written for, is evidence about the gate, not just about
  this change.

  **Extra evidence, outside `./verify`.** A scratch probe (not committed) lifted
  the pure assignment logic out of gui2.html and asserted 30 properties: all
  0..8 decodes unchanged, a stored `179=3 / 180=1` still resolving to
  `[169, 167]` (Macro 4 / Macro 2), the bend map's centre/ends/clamp, `asnPid(9)
  === 38`, `asnPid(10) === -1`, and the naming under both flag states. All PASS.
  It carries **two must-not-fire controls** (a macro axis and a wheel axis must
  both show unit passthrough, i.e. the bend map must NOT engage) and was
  **calibrated against a mutated copy** — `BEND_WHEEL_ST 2→3` plus swapping
  `ASN_BEND`/`ASN_WHEEL` in the decoder made it read `RED - 5 failed`, so the
  assertions are load-bearing rather than tautological. Per L0033, one
  assertion did NOT fire under that mutation and this is the coverage boundary,
  not a miss: "pad centre = zero bend" is `(0.5-0.5)*2*K == 0` for every K, so it
  is structurally insensitive to the range constant. The range is pinned by the
  two endpoint assertions instead.

  `tools/labharness/lab_load_check.mjs` run directly: `GREEN — 44 labs loaded, 0
  broken, 1 skipped`, with `OK gui2.html` — this is the gui2 load check `verify
  fast` runs (its sweep covers the shipping GUIs, not only the design labs).

  `build-release/intent_check` (built by the full build but **deliberately
  unwired from `./verify`** — see `docs/proposals/b89-phase2-intent-resolver.md`
  §2a, "intent_check standalone; ./verify full exactly unchanged") was run by
  hand because it is the only oracle that exercises the pad's pointer. Its whole
  T8 block — pointer, gesture bracket, spring return, the home that flips, the
  latch control — is `OK`. Its one reported failure is `FAIL no manifest at
  build-golden/intent/intent-manifest.tsv`: the golden fixtures were never
  generated in this fresh worktree. That is a pre-existing environment gap, not a
  regression; nothing in this change touches the resolver.

- **The CSS deltas, described** (no `.claude/launch.json` in this repo, so no dev
  server and no screenshots — the brief's stated fallback):
  - FX: `#pg-FX.on > #mxPane { margin-top:20px }`. Was effectively 0 (the
    ADR-098 flow rule's `margin:0 0 10px` sets top to 0 and the column gap does
    not cross a `column-span:all` break). 20px is deliberately DOUBLE the 10px
    inter-cluster column gap, per the brief's "visibly larger ... roughly
    double": the pane is a page-width break in the flow, not another tile in it,
    and matching 10px would have said the opposite. The selector carries the page
    because `#mxPane` (1,0,0) loses to `#pg-FX.on > *` (1,1,0) — a bare id rule
    would have looked applied and done nothing.
  - SET: `.cluster .cluster { margin-top:10px }`. Was 0 between the two nested
    panels and 6px under the outer `<h2>` (the h2's own `margin-bottom`). Now
    10px between panels, and 10px under the h2 for free — adjacent sibling
    margins collapse to `max(6,10)`. 10px is the page's existing tile spacing
    (`column-gap` and the flow rule's bottom margin are both 10px), so the nested
    panels now sit on the same rhythm as top-level clusters.

- **Open questions:**
  1. **Should a Pitch Bend axis spring to centre on release?** Pad release
     behaviour is UNCHANGED — the pad does for a bend axis exactly what it does
     for a macro, i.e. the value stays where the drag left it. A real wheel
     springs back, and the GUI's own Wheel strip springs back to its rest value
     on `pointerup`; a pad that does not is a defensible different instrument but
     it is a design call, not an implementation detail. **Flagged for the lead /
     human, not decided here.** (Related: the LATCH parameter 268 already exists
     for the intent pad, so "springs back unless latched" has a precedent to
     borrow if the answer is yes.)
  2. **The acceptance criterion "every option label for a macro in a mapping
     dropdown equals the macro knob's label text" cannot hold literally**, and it
     contradicts the brief's own Part B spec in the same document. The knob reads
     `Space` / `M1`; the dropdown reads `Macro 1: Space` / `Macro 1` — which is
     the human's own worked example. Resolved in favour of the human's example
     and the explicit Part B text, and implemented so the two CANNOT diverge:
     both renderings read `intentName(i)` and share one fallback condition, the
     strongest satisfiable form of the criterion. Flagged rather than silently
     reinterpreted.
  3. **Params 179/180 are still named "Main X > Macro" to the host** although 9
     and 10 are not macros. Renaming a shipped parameter is a public-interface
     change and needs the human's gate; left for that ruling and marked in the
     table comment.
  4. **`intent_check` is not run by `./verify`** and its goldens are absent from
     a fresh worktree, so the pad's pointer semantics have no CI-blocking oracle.
     Pre-existing and out of scope (editing `./verify` is human-gated), but it
     means the T8 evidence above is a manual run, not a guaranteed gate — L0031's
     distinction between what is gated and what was merely checked once.
  5. **The Mod Wheel axis is not captured by the morph or by a corner**, because
     it is a matrix source and not a parameter. That is consistent with how the
     wheel already behaves, but it means an assignment-10 axis is the one pad axis
     whose value a patch does not store. Stated, not fixed.
