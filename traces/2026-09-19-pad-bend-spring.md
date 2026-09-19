# pad-bend-spring — a MAIN pad axis assigned to Pitch Bend returns to centre on release, under the bend law

- **Queue item:** unqueued: the human's ruling of 2026-09-19, verbatim "Bend
  should spring to center", dispatched by the horde lead as one queue item. It
  answers open question 1 of `traces/2026-09-19-pad-assign-bend-wheel.md`
  (ROADMAP B160, PR #677), which left pad release unchanged and flagged the
  spring as a design call. The broader ruling ("all macros should have the
  option of springing to origin", "macros settable to unipolar or bipolar") is
  a separate design row and was explicitly out of scope.

- **Why:** The pad's bend axis is position-absolute — `mainMap` already defines
  the pad's centre (u = 0.5) as zero bend — so "springs to centre" and "writes
  the bend target back to 0" are the same sentence. The implementation is
  therefore a WRITE, not an animation: on `pointerup`, while the gesture
  bracket is still open, the axis sends param 38 → 0 down the same three lines
  (`bridge.setParam` / `lastParams` / `setControl`) the drag used. Param 106's
  bend law (spring / lag / constant time / constant rate, carried by
  `glide_core`) then moves the sound home exactly as it moves a released MIDI
  wheel home. A pad-local easing would have been a second travel law in a
  design whose entire point (the #677 trace, "C — bend and wheel on the pad")
  was that bend gets no second write path.

  Pad Latch (param 268) is the opt-out rather than a new flag: its contract at
  `src/hypersaw_clap.cpp:3244` already reads "latched, the puck does not
  return". Unlatched (the default, `{268, ..., 0, ...}` at
  `src/hypersaw_clap.cpp:721`) springs.

  GUI-side, not shell-side, per the brief's preference: the release is a
  pointer gesture the GUI owns end to end, and the shell writes neither 179 nor
  180 (`tools/intent_check.cpp:2153`). Nothing in the shell needed to learn
  about it.

- **Evidence consulted:**
  - `src/gui/gui2.html` — `ASN_BEND`/`ASN_WHEEL` (3455), `asnPid` (3967),
    `mainAsnOf`/`mainMacroIds` (3969-3970), `BEND_WHEEL_ST` (3977), `mainMap`
    (3980-3985), `PADS` and the `perf` flag (3987-3994), `padPuck`
    (4001-4008 — reads `lastParams[id]`, i.e. the LIVE param, never a drag
    cache), `wirePad` (4009+), `padSideWrite` (the Mod Wheel's non-parameter
    path), and the simulated bend wheel strip (2517-2549).
  - `src/hypersaw_clap.cpp` — param row 268 (721), `intentLatchOn` (3244),
    `padStep`'s latch argument (3569), the 268 apply/read sites (5680, 5802).
  - `traces/2026-09-19-pad-assign-bend-wheel.md` — the whole of part C, and
    open question 1, which this row closes.
  - `tools/labharness/lab_load_check.mjs` (the gui2 load gate).

- **Alternatives rejected:**
  - *Spring to the PRE-GESTURE value, as the Wheel strip does (ADR-112: "38
    lives in the morph field, so a corner may legitimately hold a nonzero
    Pitch, and a wheel that springs to zero would stomp it").* Rejected because
    it contradicts the ruling and the pad's own geometry: the pad is absolute,
    so returning to the pre-gesture value would return the puck to where the
    drag STARTED, not to centre. Recorded as an open question below rather than
    silently reconciled — it is a real divergence between two surfaces that
    write the same parameter.
  - *A pad-local animation of the return (requestAnimationFrame easing toward
    0).* Rejected outright: a second travel law, and it would diverge from the
    host wheel the moment param 106 changed.
  - *A new "spring" parameter for the pad.* Rejected: param 268 already exists
    with exactly this contract, and the brief bars new parameters.
  - *Adding a `pointercancel` handler to the pad.* Rejected as out of scope: the
    pad has never had one, so adding it would change the macro axis's release
    path, which the acceptance criteria require to be unchanged. Noted below.
  - *Doing the spring shell-side.* Rejected: the GUI sees the gesture end
    directly; the shell does not write 179/180 at all.

- **Verify:** `./verify full` — **exit 0**, git `0122f92`
  (`.harness/last-verify.json`: `{"target":"full","exit":0,"git":"0122f92","ts":"2026-09-19T23:22:08Z"}`),
  and green again on the commit that carries this corrected paragraph — a
  trace-text-only amendment whose `src/` tree is byte-identical to `0122f92`
  (`git diff 0122f92 HEAD -- src/` is empty). Green earlier on `b1154f4` and on
  the uncommitted working tree as well: three full runs, same result.
  All fifteen gates GREEN, including `parity_check`, `glide_check`,
  `time_check`, `presentation_check`, `gen_gui_controls --check`, `gui_reach`,
  `paramclass` (246 rows — this change adds no row), `preset_check`,
  `state_check`, `rtsafety_probe`, and `lab_load` (`GREEN — 44 labs loaded, 0
  broken, 1 skipped`, with `OK gui2.html`). The identical run was green on the
  working tree before the commit.

  **Extra evidence, outside `./verify` (scratch probe, not committed).** The
  probe lifts the SHIPPED text of the pad block out of `gui2.html` by anchor
  lines (`const ASN_NONE` … `PADS.forEach(wirePad);`) and runs it in a `vm`
  context with stubs, so it tests the file rather than a copy. 15 assertions,
  all PASS:
  - 179 = 9 / 180 = 9 / both, latch off → exactly `[[38, 0]]` (and `[[38,0],
    [38,0]]` for both axes); `lastParams[38]` ends at 0.
  - latch on (268 = 1) → zero writes, `lastParams[38]` left at its drag value.
  - `padPuck` on a bend axis tracks param 38: 2 st → 1.0, 0 st → 0.5, −1 st →
    0.25 (the puck follows the live parameter, the acceptance criterion).
  - **Four must-not-fire controls** (L0033): a macro axis, a Mod Wheel axis
    (also asserted to make no `setModWheelUI` call), a None axis, and the OSC
    pad all produce zero writes.

  **Calibrated by mutation** — the assertions are load-bearing, not tautologies:

      centre1 (centre 0.5 -> 1.0)              RED - 4 failed
      nolatch (drop the 268 guard)             RED - 2 failed
      allaxes (drop the ASN_BEND guard)        RED - 2 failed
      noperf  (drop the pad.perf guard)        RED - 1 failed

  The `noperf` mutant initially stayed GREEN because the stub `effId` returned
  −1, which neutralised the OSC control for the wrong reason (the
  detector-shares-the-assumption trap). Fixed by giving `effId` real-ish ids;
  the control then fires. Recorded because a control that passes for the wrong
  reason is worse than no control.

- **Open questions:**
  1. **Two surfaces that write param 38 now spring to different places.** The
     Wheel strip springs to its PRE-GESTURE value on ADR-112's stated reasoning
     (a morph corner may hold a nonzero Pitch, and springing to zero stomps it);
     the pad now springs to zero, because the human ruled "spring to center" and
     the pad's centre IS zero bend. Both are defensible, but a patch whose
     corner holds Pitch = +1 st will have its pad release write that away. Not
     resolved here — it needs the human, and it may be what the broader
     "springing to origin" row is for.
  2. **The puck does not visibly TRAVEL home; it snaps to centre while the
     sound travels.** `padPuck` reads `lastParams[38]`, which is the bend
     TARGET, and the target goes to 0 immediately by design. The shell exposes
     no live post-law bend value to the GUI (searched: no `bendNow` /
     `bendViz` readback exists), so the GUI cannot draw the travel without a new
     readback — a shell interface change, out of scope. The acceptance
     criterion "reads the live param, not the last drag position" IS met and is
     what makes the puck follow automatically the day such a readback lands.
  3. **`pointercancel` on the pad is still unhandled** (pre-existing): a
     cancelled pointer leaves the gesture open and now also skips the spring.
     Out of scope here because fixing it changes the macro axis's release path.
  4. **Both axes assigned to Pitch Bend writes 38 twice** on release (and during
     the drag). Harmless and pre-existing from #677, but it means a
     two-bend-axis pad is a degenerate configuration the UI does not forbid.
  5. **No CI-blocking oracle covers the pad's release semantics.** `lab_load`
     proves gui2 still loads; the 15 assertions above are a manual scratch run,
     not a gate. Same L0031 boundary #677 flagged.
