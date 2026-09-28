# b317-scalpel-lab-round9 — the logo's colour-shifting edge; a bigger Specimen the XY turns; gui2's screen schemes

- **Queue item:** B317. I read the row verbatim from `origin/lead-records-123:ROADMAP.md` (records PR #811), with its amendment and B315 beside it. The human, 2026-09-28:
  1. "please replace the black border with the version that shifts color. The specimen also needs to be bigger, and could you make it so moving the XY moves it like it's being clicked and rotated? Also realizing I would like to see the different display color schemes from the original plugin."
  2. Then: "Actually maybe the size of the logo is alright for now".
- **Why:** This is round 9 of B271's lab, `docs/design/scalpel-interface-lab.html`, and that file is the only one changed apart from this trace. Nothing in `src/`, `specs/` or `reference/` was touched, and neither was `docs/design/index.html` or the engine file.
- **Evidence consulted:**
  - `src/gui/gui2.html`:
    - the logo's tokens: :170-212;
    - the logo's driver and its two edges: :6352-6470 (the ink `--lg-edge` and the hue-test `hueRot(lastTint, 120)` at :6384-6420);
    - the Specimen's drag and easing: :5213-5230 and :5370-5378;
    - its ground: :5443-5458;
    - `SCHEMES` and `MODES`: :1846-1867;
    - the scheme CSS: :74-81 and :105-150;
    - the SCR chip: :2102-2212.
  - The round 8 trace, `traces/2026-09-27-b315-scalpel-lab-round8.md`.

## What changed (`lab-review` meta `B271 + B293–B317 · 2026-09-28`)

1. **The logo's edge** (E2, `logoOutline`, `logoEdgeColour`). The box stays 70×46, per the amendment.
   - gui2 has two edges. Its **default** is the ink `--lg-edge`: `#191521` on the light chassis and none on the dark. B315 ported that one, and it is "the black border".
   - The **colour-shifting** one is gui2's hue-test, the human's audition of 2026-08-29. Its edge is the fill's colour turned **120°** (`hueRot(lastTint, 120)`, :6414).
     - The code's comment says 90° was auditioned first, and the human asked for 120.
     - I ported the 120° variant, the one the code ships. The fill already shifts (the morph cursor, or ADR-150's drift), so the edge shifts with it.
   - It is ported as gui2's code path runs it, in **both themes**. gui2's `if (brandHueTest && lastTint) edge = …` overrides dark's `none` too.
   - `--lg-edge` stays in the copied token set but is no longer read.
2. **A bigger Specimen** (buildMain, E3).
   - **Measured first:** MAIN's left column had no slack. The Visualizers cell's content fills it to within 5 px (Advanced ends at 2200 in a cell ending at 2207 less padding).
   - So the Morph cell's three pills move into its title bar, the idiom of the XY's own SPECIMEN pill. The XY pad takes their row: **108 → 127 px** (`XY_PAD_H`).
     - The column stays 649 px and the Visualizers cell is unchanged (304 px).
     - The Morph pad keeps its 138 px.
   - The camera's focal length is **1.25×** gui2's 2.1 (`SPECIMEN_ZOOM`, uniform `uZoom`). This is the lab's only shader edit.
3. **The XY turns the Specimen** (E3, `specimenFollowXY`).
   - gui2's drag gains apply to the XY marker's travel in pad CSS pixels: yaw target +0.006 rad/px, pitch target +0.005 rad/px, pitch clamped ±1.1.
   - The view eases to the target with gui2's own `min(1, dt·7)` on the engine clock. gui2 has damping and no momentum, so no momentum was added.
   - It reads the marker (`xyMarker()`, now also the mapping `paintXY` draws the puck by), not pointer events. So any move of the marker turns the pearl.
   - The baseline is dropped while the Specimen is not showing, so moves made elsewhere never land as one jolt on return.
   - Visual only: nothing reads the orbit back, and no wall clock is read.
4. **gui2's screen schemes** (CSS A, `SCHEMES`, the `screen` chip, `?scheme=`).
   - gui2 ships five (TUBE, ORCHID, FROST, EMBER, DUSK), with values verbatim. Its comment says "four", which predates DUSK; the array has five rows, so all five are here.
   - **Chassis and screen are separate axes.** Neither chassis block sets a `--scr-*` role any more. The theme picks the default pair (light ORCHID, dark TUBE, gui2's `MODES`), and the chip then moves the scheme alone, so crossed pairs can be seen.
   - **B293's pale-tube weights and `--scr-b2`** now key off the pale grounds (ORCHID, FROST, EMBER) rather than off the light chassis. Every paired combination is unchanged.
   - **What repaints:** rebuild() repaints every screen. The Specimen's ground already followed `--scr-tube`, as gui2's does.
5. **Page text:** the ROUND 9 header comment, the badges, and open questions 60–63.

## Evidence

- **VERIFIED, in-page self-check 72/72, with 28 controls** (it was 66/66 with 25 before). The run was headless Chrome (real GPU, ANGLE Metal, M3) on OSC. The count is 72/72 on each of these as well:
  - `?page=main&theme=dark`
  - `?skin=c&direct=1`
  - `?scheme=dusk&theme=light`
  - `?b2=hue`
- **The 6 new rows (`checkB317`):**
  - **Logo edge** (light and dark, morph off). The edge pixels are those opaque in the mark and empty in its fill, averaged.
    - Light: 296 px, #26f6d5. After 5 s of engine time: 185 px, #2498f4 (Δ 127).
    - Dark: #2498f4 → #261dec (Δ 133).
    - Each matches `logoEdgeColour()` within 0.1.
    - The fill turned 120° is true in both themes.
    - Light is 353 away from the ink #191521.
  - **CONTROL: the clock standing,** 20 paints change the edge colour by 0.01 / 0.00.
  - **Orbit.** A real pointer drag on the pad (64, −25 px) moves the marker 63.70, −24.47 px.
    - The yaw target moves +0.3822 (= 0.006/px) and the pitch target −0.1223 (= 0.005/px).
    - After 0.5 s of engine time, the view is 0.3770 yaw and −0.1207 pitch from an undragged run.
    - There were 10 draws.
    - Width and Cut rate round-trip into P and the engine, and the marker ends under the pointer.
  - **CONTROL: the XY still,** the XY share is 0 and 0. The targets moved 0.015 yaw, which is the turntable alone, and 0 pitch.
  - **Schemes.** For all five, in both themes:
    - every OSC screen's modal pixel is gui2's tube (6/6: ring, spectrum, carpet, voice map, cycle view, beam);
    - the tube token equals gui2's value;
    - the Specimen's ground follows it (on MAIN);
    - the roles are identical across themes;
    - the chip names it.

    The five grounds are distinct.
  - **CONTROL: DUSK with its class stripped,** 0 of 6 screens are on #2a2140.
- **Planted faults** (served rewrites, nothing committed): **7 of 7 CAUGHT.**
  - The edge back to the ink.
  - A fixed edge colour.
  - The XY follow removed.
  - Half the yaw gain.
  - No easing.
  - The scheme class never applied.
  - FROST's ground wrong.
- **The Specimen's reach at dpr 2,** from 60 moments across the pearl's motion and turntable. Main and this branch were served in one session.

  | Build | Pad bitmap | Span across | Max cover | Top edge touched |
  |---|---|---|---|---|
  | main | 508×212 | 164..340 px (~88 CSS px) | 11.7% | never |
  | b317 | 508×250 | 120..382 px (~131 CSS px) | 21.7% | never at the default tilt; 3/60 and 1/60 at the ±1.1 tilt limits |

  - The pearl is about 1.5× wider, and its area is about 2.2×.
  - At the default tilt its top clears the pad by 21 bitmap rows (~11 CSS px).
  - The floor shadow reaches the pad's foot on both builds.
- **Frame cost**, headless on MAIN (M3, dpr 2), with main and this branch interleaved in one session, twice each. Each figure is ms per frame, the mean of 240 after 30 warm-up frames; the Specimen is synced by a 1-px readPixels.
  - **This session's baseline runs ~2.3× round 8's figures on the same page.** main's base frame reads 3.18–3.53 here, against b315's 1.37. So compare within a row, not against b315's table.

  | Build | Base | + logo | + Specimen | + both | Specimen / draw | Logo / paint |
  |---|---|---|---|---|---|---|
  | main | 3.18 / 3.53 | 5.03 / 5.13 | 4.89 / 5.33 | 7.16 / 7.94 | 1.213 / 1.218 | 2.024 / 2.078 |
  | b317 | 3.22 / 3.15 | 5.09 / 4.83 | 5.74 / 4.80 | 7.78 / 7.39 | 1.443 / 1.392 | 2.066 / 2.090 |

  - **The Specimen draw is +15–19%.** The bitmap has 18% more pixels, and the pearl is bigger, so more rays hit it. At the real cadence (every 3rd frame) that is 0.46–0.48 ms per frame, against 0.40.
  - **The logo is unchanged within noise:** the edge colour is one `logoHueRot` per paint.
  - At dpr 1 (this session): Specimen 1.06 ms/draw at 254×125, logo 2.08 ms/paint.
- **`lab_load_check`:** GREEN — 54 labs loaded, 0 broken, 1 skipped.
- **`lab_wheel_scroll_check`:** GREEN — 0 failure(s); 6 labs, 2 planted faults. For SCALPEL:
  - 20 wheel elements, 0 plain-wheel changes, and alt moved 19/20;
  - 103 clicks in the scroller, 0 moved.
- **Not verified:**
  - No human has seen the edge, the pearl or the schemes on a real display.
  - The cost figures come from one machine, headless, under a load that doubled the baseline.

## Screenshots (scratch, not committed; `scratchpad/b317/shots/`)

- 01 / 02: the logo alone at engine t0 and t0 + 6 s, light and dark.
- 03: the frame header, light and dark.
- 04 / 05: MAIN's XY cell, the Specimen before and after an XY drag, light and dark.
- 06: MAIN, the whole frame, light and dark.
- 10: OSC, all five schemes in both themes.
- 11: MAIN, all five schemes in both themes.

## Alternatives rejected

- **Shrinking the Morph pad or the cycle view to grow the XY:** both are working surfaces. The pills' row was the only height that carried no information of its own.
- **Momentum on the orbit:** gui2's drag has none (eased follow only), and inventing it would not be "gui2's feel".
- **Driving the orbit from the pointer events:** it would miss every other way the marker moves. The ask was "moving the XY".
- **Scheme weights and `--scr-b2` left on the chassis:** a pale screen in the dark chassis would have drawn the dark weights on a pale ground.
- **gui2's 90° edge:** superseded in gui2's own code by the human's 120°. It is listed as open.

## Verify

- `./verify fast`: exit 0 at git 3dd1f7f, the lab commit (`.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"3dd1f7f","ts":"2026-09-28T01:44:20Z"}`).
  - The log includes `lab_wheel_scroll_check: GREEN — 0 failure(s); 6 labs, 2 planted faults` and `test_table_check: GREEN (204 tests … 72 check files declaring WIRED and verified so …)`.
  - It notes that the private-name leak check was SKIPPED (`.leakcheck-names` is absent in this worktree).
- `./verify full` was not run: nothing outside the lab page changed (no engine, `src/` or tool file). The brief's target is fast.
- This trace is committed on top, and `verify fast` is re-run on that hash (the PR carries the result).

## Open questions

1. **The dark theme now has an edge.** Is that wanted, or should dark drop it, as gui2's ink edge did? Is 120° the turn, or 90°?
2. **The edge's legibility on cream.** The ink edge carried the mark's contrast. A turned edge can be pale too: a yellow fill gets a cyan edge, both pale on cream. Should the light ground darken the edge to keep 3:1?
3. **Knobs and presets turn the pearl.** They move the marker, so they rotate it too. Should that happen, or should only the pad's own gestures turn it?
4. **The pearl grazes the pad's top edge at the pitch limits.** It does in 3 and 1 of 60 moments. Trim `SPECIMEN_ZOOM` if that bothers you.
5. **The scheme count.** gui2 ships five schemes where the brief said four; all five are here.
