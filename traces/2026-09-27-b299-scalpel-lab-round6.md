# b299-scalpel-lab-round6 — Rotate keeps turning; the waveform gestures on MAIN

- **Queue item:** B299. I read the row verbatim from `origin/lead-records-108:ROADMAP.md` (records PR #787). The branch is stacked on round 5's `lab-scalpel-5` (PR #786). The human, 2026-09-27:
  1. "Noticing that rotate currently isn't continuous; it jumps back to its beginning position at a rate I can't seem to control anywhere."
  2. "Please implement the scalpel waveform click/drag interactions on the main page to leave the XY pad clear for different" (the message ends there).
- **Why:** This is round 6 of B271's lab, `docs/design/scalpel-interface-lab.html`.
  - I measured the jump before fixing it, and it was the lab's, not the patch's.
  - Isolation: the change is one file plus this trace. `docs/design/index.html` is not regenerated. Nothing in `src/`, `specs/` or `reference/` changed, and the oracle and its defaults are untouched.

## 1. The jump, measured

- **Method.** Scratch harness, headless Chrome over DevTools, on the lab as committed at `9e2a72b`.
  - Rotate was 0.3 and 0.5 Hz, and the Rotation mode was restart per note (the default) or free-running.
  - The display rate was 60 or 120 Hz, run as `frame()`'s one `monStep` per paint.
  - Each run covered 10 s of wall time.
  - **The audio side** is a twin of the worklet's instance: the same class, seeded 0xB271 as `workletSource()` seeds it, the same params, HOLD's note-on, and 128-frame quanta at real time.
  - **What was logged:** the rotation clock each instance uses (`v.rot` under restart per note, `gRot` when free-running; `razor-core.js:704`, `:737`), once per paint.
- **Audio: continuous.** In every one of the eight runs the audio had 0 resets, and its rate was exactly the Rotate value (0.3 or 0.5).
- **Monitor: re-struck every 2 s of its own engine time** (`MON.t > 2 * SR` → `monStrike()`, round 5's line 1342).
  - Every strike built a **new** `RazorCore`, and a new instance starts `gRot`, `v.rot` and `m.rot` at 0. So the drawn blade went back to Position in **both** modes, free-running included.
  - Engine time was one stroboscope frame per paint (`strobeLen(110 Hz)` = 436 samples). The re-strike therefore landed at **3.667 s and 7.333 s at 60 Hz**, and at **1.833, 3.667, 5.5, 7.333 and 9.167 s at 120 Hz**. That is the display's rate, which no control reaches.
  - **At 0.3 Hz each jump was 0.401 cycle.** At exactly 0.5 Hz the jump hid: 2 s is one whole turn, so the reset landed 0.0045 cycle from where it was.
- **The drawn rotation ran at the wrong speed.** Rate against the heard 0.5 Hz:
  - 0.2727 Hz at 60 Hz, which is 0.545×;
  - 0.5454 Hz at 120 Hz, which is 1.09×.

  At 0.3 Hz it was 0.1635 and 0.327 Hz. Even without the jump, the drawing and the audio disagreed.
- **Other candidates, ruled out:**
  - **Homing** (`RazorCore.home`, `razor-core.js:703`, `:712`) acts only while |Rotate| < 0.004. It is a single 30 ms glide when the knob crosses 0, not a periodic jump.
  - **The RESTRIKE set** (Members, Start phases, …) strikes only on an edit.
  - **The cycle view** reads `mem.c` from the viz post (`razor-core.js:802`). It agrees with the monitor instance to 1.6e-15 at each post's own sample (the check below), so it was not redrawing from Position on its own.
- **`parameters.json` (the lead's third point).** It lists rotSync's options as [restart per note, free-running] with default 1.
  - That is display order, not value order. 1 = restart per note, as the bench's `seg()` defines it (`scalpel-bench.html:1238`) and as ACCOUNTING §1.8.1 already records. The lab's table (`R('rotSync', …)`, line ~893) maps it correctly, so it is not a data bug.
  - `rot2Follow` has the same pattern ([with blade 1, its own], default 1), and §1.8.1's list of affected rows omits it.
- **Bench presets.** 8 of the 76 rotate: Crush vs FM, Chasing blades, Counter-rotation, Formant growl, Wobble jaw, Two clocks, Undertone cathedral, Clockwork. None sets `rotSync` 0.

## 2. The fix (section D, the face, the labstrip)

- **The monitor keeps its own clock.** `MON.clock` counts every rendered sample and is never reset by a strike.
- **With the audio on, the monitor follows the audio's clock.**
  - `audioClock()` reads `AudioContext.currentTime·SR − a0`: the audio's own count of rendered samples.
  - `monAdvance()` → `monFollow()` renders whole strobes, at most 32 per paint, until the monitor is within half a strobe of that count.
  - `audioStart` anchors `a0` and starts the monitor fresh (`monStrike('audio')`), because the worklet's instance starts every clock at 0 too.
  - With the audio off, the monitor is round 5's deterministic stroboscope.
- **Strikes carry the rotation the audio would still have** (`monStrike(kind, what)`, `rotCarry`):
  - `'note'` (a key, or HOLD) carries `gRot` and `gRot2`, which the audio's instance ticks on every sample whether a voice sounds or not;
  - `'patch'` (the RESTRIKE set, a preset, INIT) carries every clock, the voice's and each member's, because the audio's voice is not struck at all.
- **The 2 s re-strike is the RE-STRIKE pill**, under the keys and **off by default**, so the note is held.
  - When it is on, it re-triggers the held note **on the same instance** (`monRetrigger`, the oracle's own `noteOn`: fresh = false). While HOLD is on, it sends the same message to the audio, so the drawing and the audio re-strike together.
  - While the audio plays keys, the monitor waits for those instead.
- **Every strike names itself** for one second of monitor time: `↻ Members · re-struck, rotation kept`, `↻ key · rotation restarts (per note)`, `↻ RE-STRIKE (2 s) · …`, bottom right of the cycle view.
- **The face.** Rotate is a scrub on each blade's WAVE line, with Rotation ("per note" / "free-running", the full words in the tooltip) beside it on both blades.
  - Blade 2 adds its clock switch ("with ①" / "its own"). This is `linkPill` with names, so its copy-on-write is kept.
  - The face does not grow. The tier audit reports rotRate, rotRate2, rotSync and rot2Follow as promoted to T1.
- **Measured after the fix** (scratch `after.json`, the same 10 s):
  - **Audio on, no RE-STRIKE:** 0 resets heard or drawn in either mode, at either display rate. The drawn rate equals the heard: 0.5 / 0.3 Hz at both 60 and 120 Hz.
  - **max |drawn − heard|:** 0.0167 cycle at 0.5 Hz and 0.01 at 0.3 Hz, which is exactly one viz post of rotation (1600 samples).
  - **RE-STRIKE on with HOLD, restart per note, 0.3 Hz:** heard and drawn both reset at 2.0, 4.0, 6.0 and 8.0 s, from 0.6 cycle back to Position. The drawing catches up at the next viz post, within 33 ms.
  - **RE-STRIKE on, free-running:** 0 resets.
  - **Audio off:** 0 resets. The drawing still runs at the display's pace (0.545× at 60 Hz). That is open question 39.

## 3. MAIN's waveform gestures

- **One wiring for all three cycle views.** OSC's view and MAIN's two views (the Visualizers cell's, 254 × 150, and Osc Controls' mini, 148 × 90) are all wired by `cycleSurface(cv, sum)`, which calls `xyWire` (the XY) or, with CYCLE EDITS on, `cycleEdits` (the grips).
  - That gives MAIN OSC's gestures exactly: ↔ Position (wraps), ⇧↔ Width, ↕ Cut rate, alt = fine, the window under the pointer or every blade from empty space.
  - The readout and outline are drawn on whichever view is under the hand (`XY.cv`). The readout breaks into two lines under 420 px and one line per key under 200 px.
  - `XY.map` is the one mapping. MAIN's XY pill opens the same row, in the narrow layout.
- **Hit-testing on what is drawn.** MAIN draws SUM (it has no view pills). `cycleView(cv)` makes its hit-test and outline read SUM's windows while OSC shows AT PHASE, and `xyWindows(view)` takes the view.
- **The XY pad** is now a dashed card captioned **FREE TO REASSIGN**, still today's Width × Cut rate. The ring (Coupling × Detune, absolute) is untouched.

## Verified vs entailed

- **VERIFIED, in-page self-check 44/44, 14 of them controls that must fail** (38/38 with 12 on #786). Read from headless Chrome in light and dark, OSC and MAIN, skin C, and `direct=1`. New:
  - **Rotate free-running, continuous over 10 s held.** The audio twin gave 3750 quanta, 0 resets, monotonic, and 0 resets when the note is struck again at 3.3 s. Rotation sits beside Rotate on both blades, and blade 2's clock switch is present on the built face.
  - **CONTROL:** under restart per note, the same re-strike gives 1 reset, from 0.649 cycle back to Position.
  - **Drawn rotation = heard, through a patch strike (Members at 4.2 s) and a key strike (6.7 s).**
    - 600 paints with the monitor following the twin's clock: max |drawn − heard now| is 0.0188 cycle, against a limit of 0.0212 (one viz post, plus half a strobe either side).
    - At each of the 302 posts' own sample the error is 1.6e-15.
  - **CONTROL:** round 5's pacing (a strobe per paint) is off by 0.500 cycle: 5.48 s rendered against 10.00 s.
  - **MAIN, a grab alone moves nothing, and the target is the drawn window.**
    - The patch was splayed with Rotate 0.5: the grab at blade 1's SUM window (0.567) is one the static window misses.
    - The SUM and AT PHASE windows differ, and MAIN reads SUM.
  - **MAIN drag round trip:** c, k and w each equal the value the mapping predicts; blade 2 is untouched; the oracle's `t` agrees.
  - **Mutation runs (scratch, not committed), each caught:**
    - `rotCarry` as a no-op: the drawn check fails at 0.341.
    - `monFollow` as one strobe: the drawn check fails.
    - `cycleView` returning LAB.view: the MAIN target check fails, "SUM and AT PHASE windows AGREE".
- **VERIFIED, real mouse on MAIN** (`Input.dispatchMouseEvent`):
  - A drag across blade 1's window moved Position 0.875 → 0.975 with the oracle agreeing, and blade 2 held.
  - ⇧ + drag from empty space moved Width ① 0.25 → 0.3512 and Width ② 0.20 → 0.2810, and Cut rate ① 6 → 7.497 and Cut rate ② 3 → 3.749.
  - A click with no movement moved nothing.
  - The mini view from empty space moved both Positions.
  - With `direct=1`, CYCLE EDITS on MAIN moved the window.
- **`lab_load_check`:** GREEN (53 labs, 0 broken, 1 skipped).
- **Not verified:**
  - Nobody has listened.
  - I did not run the real AudioWorklet's clock against the drawing; the twin stands in for it. `AudioContext.currentTime` is assumed to count rendered frames, as the Web Audio spec defines it.
  - The 32-strobe catch-up after a hidden tab was not exercised.

## Screenshots

These are scratch files, not committed, in `scratchpad/b299/shots/`.

- 00: before (OSC and MAIN, light).
- 02: OSC after, light and dark.
- 03: the blade rows at 2× (Rotation on the face), light and dark.
- 04: free-running, with blade 2 on its own clock at −0.30 Hz (2×, light).
- 05: the strike named on the cycle view, light and dark.
- 06: the RE-STRIKE pill.
- 07: MAIN mid-drag with the readout: light (↔ Position, blade 1), dark (⇧↔ Width + ↕ Cut rate, both blades), and the light mini view.
- 08: MAIN dark, and MAIN with the mapping row open.
- 09: MAIN with CYCLE EDITS, and skin C.
- 30: rotation against time, before and after (plot).

## Alternatives rejected

- **Setting a free-running monitor's `gRot` from the audio by message.** It would need the worklet to post its state, and it would fix rotation but not the monitor's pace (the beam and the carpet would still run at the display's speed).
- **Pacing the monitor by the wall clock** (the rAF timestamp). It breaks B271's no-clock rule and the reproducible stills. The audio's own sample count is used only while there is audio.
- **Keeping the 2 s loop on by default.** It is the jump the human reported.
- **A knob for Rotate in a new face group.** The blade row is `nowrap` and full, so it would overflow. The scrub rides the WAVE line.
- **Repurposing MAIN's ring.** The brief forbids it.
- **Emptying the XY pad.** What it holds is the human's call; it is marked, not removed.

## Verify

`./verify fast`, exit 0, git a890ce6 (`.harness/last-verify.json`, 2026-09-27T18:17:35Z). The log notes "private-name leak check SKIPPED" (`.leakcheck-names` is absent in this worktree). This trace is committed on top, and verify is re-run on that hash before the push.

## Open questions

1. **The default Rotation.**
   - The lab recommends keeping restart per note, in the lab and in horde.
   - The jump was the monitor's.
   - Restart per note makes each attack start at Position.
   - All 8 rotating bench presets rely on it.
   - It is the human's call.
2. **What MAIN's XY pad should hold.** The human's message is truncated.
3. **The monitor's pace with the audio off.** It is 0.545× at a 60 Hz display, and the lab accepts it.
4. **Is reading `AudioContext.currentTime` within the lab's "no clock" rule?** The lab says yes: it is the audio's sample count.
5. **The ROADMAP row's wording.** It calls the ring "MAIN's XY pad" in the brief ("MAIN's XY pad (the ring)"). The pad the human means is the XY cell (Width × Cut rate): that is the one the waveform gestures make redundant, and it is the one marked. The ring is unchanged either way.
6. **The mapping row on MAIN** overflows the Visualizers cell when open: the Advanced button goes under the fold, a settings moment.
7. **ACCOUNTING §1.8.1** should add `rot2Follow` to its list of display-order rows.
8. **The self-check is still not gated by `./verify`**, as in B271 and B293–B297.
