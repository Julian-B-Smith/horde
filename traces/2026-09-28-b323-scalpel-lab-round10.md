# b323-scalpel-lab-round10 — overload handled (load meter, tail cull, shedding); a bigger logo; the Specimen's freeze, veil and centring; lime; coherence made legible

- **Queue item:** B323. I read the row verbatim from `origin/lead-records-126:ROADMAP.md` (records PR #819), with B313, B315 and B317 beside it, and ADR-184 A2 on the same branch. The human, 2026-09-28:
  1. "I'm noticing the issue [overload] on many other patches now. Maybe the browser is just overloaded or something, or the device is getting more complex."
  2. "the logo does need to be bigger."
  3. On the Specimen: "it freezes when audio turns on until I turn it off and on again", "the film over it should be more transparent so it can be more vibrant", and "sometimes it floats down low enough that it gets cut off by the bounding box; is there a way we can keep it centered to the space?"
  4. "can we make the green that's used in the waveform (and a few other places) in the light screen modes a little more vibrant and lime-tinted?"
  5. "in all color versions, the coherence line on the phase circle should probably be the same color as the coherence number to make it clearer what it's measuring. Maybe there could even be another vertical bar or something tracking it."
  - A mid-task addendum from the lead, after the human reported "more noise and clicks that I'm not certain are supposed to be part of the waveforms": a culled tail must fade out (5-10 ms, in seconds), never cut; prove it with B316's click metric against an uncapped render, with an instant cut as the must-fail control; say whether the lab's clicks are playback underruns, with evidence.
- **Why:** This is round 10 of B271's lab. Files: `docs/design/scalpel-interface-lab.html`, `docs/design/scalpel-horde-engine.js`, and new rows in `tools/labharness/composed_engine_check.mjs` (already WIRED in `verify full`). Nothing in `src/`, `specs/` or `reference/` was touched. `docs/design/index.html` was not regenerated.
- **Evidence consulted:** the B313/B315/B317/B323 rows; ADR-184 A2 and ADR-083 (DECISIONS); `reference/scalpel/prototype/razor-core.js` (read only: `kSpread` :144-160, `startVoice`/`noteOff` :384-433, `render` :724-906); `tools/patchspace/metrics.mjs` (`clicks`); the round 8 and 9 traces.

## Measured first (before any change)

Headless Chrome (the installed Google Chrome), real GPU, M3, dpr 2. The worklet timed from inside with `Date.now()`, because `performance` is **undefined** in Chrome's AudioWorkletGlobalScope (measured: `typeof performance` → "undefined", `Date.now` present). The window length is counted in samples. Main thread: `performance.now()` around frame()'s four parts. The "before" build was `origin/main`'s lab and engine, served by a CDP response rewrite, with the timer planted. Chrome's `AudioContext.playbackStats` (underrunEvents, underrunDuration) gives the device's own count of gaps.

- **The machine's load moves a lot between sessions.** The same Crushed bells chord read 148% in the first session and 71% an hour later. So every before/after comparison below is interleaved in one session (before, after, before, after).
- **Chord** (six notes held 1.2 s, released, re-struck 0.3 s into the release):

  | Preset | before: DSP load, underruns | after: DSP load, underruns |
  |---|---|---|
  | Crushed bells | 70-71%, 1 | 72-75%, 1 |
  | Glass horde pad | 99%, 758-759 (4.41 s of 5.6 s) | 76-82%, 228-280 (1.3-1.6 s) |
  | Harmonic stack | 35%, 0-1 | 35-46%, 0-1 |
  | Quarter sync | 11%, 0 | 9-10%, 0 |

- **Line** (24 short notes 150 ms apart, 100 ms each: the tails pile up; the human's "crushed bells keeps crashing" case):

  | Preset | before: DSP load, underruns, audio clock vs wall | after: DSP load, underruns |
  |---|---|---|
  | Crushed bells | 145-175%, 951-957 (5.5 s of 6.4 s), 0.54-0.66× | 68-72%, 20 (0.12 s); caps 1-4, 5-10 tails culled |
  | Glass horde pad | 193-239%, 989-990 (5.75 s), 0.41-0.50× | 66-82%, 47-99 (0.27-0.58 s) |
  | Harmonic stack | 48-61%, 11-78 | 53%, 14-17 |
  | Blade pluck | 38-48%, 0-3 | 41-42%, 1-4 |

- **Main thread, per frame:** 0.8-7 ms before (most of it the monitor, which renders one voice at real time); 1.7-8 ms after. The "before" frames are cheaper for a wrong reason: the Specimen and the logo were frozen after audio start (the freeze below), and under overload the monitor followed an audio clock running at 0.4-0.7× wall.
- **The clicks are playback underruns, not render artefacts.**
  - A capture AudioWorklet tapped the lab's output inside the graph (every rendered sample), during the line on the before build.
  - Crushed bells: B316's click metric on the 4.15 s actually rendered reads **0 clicks** (worst frame 7.5 dB over the median). The device reported **951 underruns** in the same span. The same preset and notes rendered OFFLINE through the lab's own worklet: 0 clicks (7.4 dB).
  - Glass horde pad: 0 clicks rendered, **1002 underruns**; offline 0 clicks.
  - After: rendered 0 clicks; underruns 32 (Crushed bells) and 17 (Glass horde pad).
  - So what the human hears as clicks is the device playing gaps while the render thread falls behind. The samples are clean.
- **Chunked render cost** (Node, Crushed bells, six voices): one call per 4096 samples 1.019× RT; one call per sample 1.031× RT. The per-call overhead is ~1-3%, which is what let the cull fade run per sample.

## What changed

1. **The load loop** (lab section J2; engine `cull()`).
   - The worklet shell times each render. Every 32 blocks (4096 samples, 85 ms) it computes `load` and applies the pure cap law `capFor`:
     - over 0.9, the cap falls at once to live × 0.75 / load (at least 1);
     - otherwise it rises one voice per window while the measured cost per voice says one more still fits under 0.9;
     - at the pool size it switches off.
   - The shell posts the window to the page (`loadIn`).
   - **The engine:** `voiceCap` is an input (msg `{t:'cap'}`), never a clock read, so the DET row still holds. Over the cap it culls by ADR-083's tiers 1 and 2 (`tierPick`, now shared with B310's note-on), never a gated voice. At the cap, a note-on replaces a sounding voice: a tail, else the oldest held (tier 3, as at a full pool).
   - **The fade:** a linear gain ramp to 0 over `CULL_FADE` = 0.008 s. It is applied every sample: while a tail fades, the block is rendered one sample per call into two reused buffers, so nothing is allocated. The voice is freed at the ramp's end.
   - **The meter:** a third header line: DSP %, cap, culls, xruns (playbackStats), UI ms, and shed. It is one line, nowrap; the long form is in its title.
   - **Shedding** (DSP > 0.85 or UI > 8 ms on; both under 0.6 / 5 ms for 30 frames off):
     - the Specimen draws every 12th frame (was every 3rd);
     - the logo every 8th (was every 2nd);
     - half the monitor's viz posts are let go.
   - **The monitor** has a per-frame time budget (8 ms; 3 ms while shedding), and it skips a lag beyond ¼ s instead of chasing it.
   - These clocks decide how often and how much is drawn, never what is drawn.
2. **The logo** is 91×60, gui2's aspect (was 70×46, 1.7× the area).
   - MAIN's header is 47 px and its body has no slack (B317), so the mark takes 7 of the frame's 8 px top padding and the 6 px gap below, with negative margins.
   - The header (47.0), the body (top 62.0, 649.0 px) and the frame (720) are unchanged: checked against the 70×46 box.
   - It also takes no pointer now.
3. **The Specimen.**
   - **Freeze (the cause):** audio on calls `monStrike('audio')`, which resets `MON.clock` to 0 (B299). The pearl and the logo computed dt = max(0, clk − lastClk) = 0, so they drew nothing until the new clock passed the old one. Measured before the fix: draws stuck at 119 for 2 s after audio on, and the logo the same.
     - The fix: a clock that goes backwards re-anchors (`S.clk`, `LOGO.clk`).
     - After the fix: 118 → 137 → 157 → 177 draws a second.
   - **Veil:** `XY_VEIL` 0.35 → 0.12. The plates and rings are unchanged.
   - **Centred:** `specimenFit` computes the three metaballs' projected bounds each frame. Each ball is grown by SPEC_GROW 0.12 plus any outward dent, and the per-axis tangent extent is exact. Then:
     - `uShift`, an off-axis lens shift, centres the box;
     - `uZoom` shrinks only if the box would not fit inside the pad's SPEC_MARGIN.
     - A `uMask` uniform draws a hit/no-hit mask for the check.
4. **Lime.** The pale schemes' `--scr-meter` is #007A73 → **#4A9400** (hue 90°, saturation 100%). Contrast: **3.28:1** on ORCHID #F3EBFF, **3.48:1** on FROST #E6F8FF, **3.41:1** on EMBER #FFF0E4 (teal read 4.50-4.77). TUBE and DUSK are unchanged (#B8F227, #C8FF3D).
5. **Coherence.**
   - The needle, the readout and the new **R bar** draw in one ink, `ringCohInk()` = the readout's `--scr-meter`. The needle was `--scr-physics`.
   - The bar sits at the ring canvas's right edge: a track in the grid ink, filled to the readout's R, with a ½ tick outside it.
6. **ADR-184 A2 (2).**
   - The composed engine's `spread()` hands the oracle Cut spread rounded half away from zero (`roundAway`) under Quantize, then restores the smoothed values.
   - Every value but a negative half is bit-identical to the oracle.
   - The header note for A2 (1) now cites the ADR.
7. **Page text:** the ROUND 10 header comment, the meta `B271 + B293–B323 · 2026-09-28`, the badge, and open questions 64-68.

## Evidence

- **VERIFIED, in-page self-check 91/91, with 37 controls** (was 72/72 with 28). Run on OSC, `?page=main&theme=dark`, `?skin=c&direct=1` and `?scheme=dusk&theme=light`: 91/91 on each. The new rows (`checkB323`):
  - **Load meter:** in the header on both pages, with the header 47.0/72.0 with and without it.
    - The worklet shell, run through `workletSource()`, posted 2 windows of 4096 samples.
    - A 132% window reads "DSP 132% cap 3 cull 4 · UI 0.0 · shed", with the bar at 100%, flagged over, and shedding.
    - The longest line fits untruncated and leaves the header at 47.0.
    - **CONTROL:** a 30% window flags nothing.
  - **Cull on CORE:** with tails 63/64/65 at 0.308/0.175/0.100:
    - cap 5 culls 65 only;
    - cap 2 culls all three tails, and held 60-62 are kept.
    - capFor on four windows gives 3 / 4 / 3 / 0.
    - **CONTROL:** a tier-blind culler takes a held note.
  - **Shedding:** over 60 frames, the Specimen 20 → 5 draws, the logo 30 → 8 paints, viz posts 16 → 8.
    - **CONTROL:** the calm cadence is 20 and 30.
  - **Audio toggle:** the pearl's clock was 84584 samples ahead. After the reset it made 9 draws and 15 logo paints; after a second toggle, 9 and 14.
    - **CONTROL (must read zero):** with the clock standing, 0 and 0.
  - **Centred:** 80 views (pitch ±1.1 in 5 steps, 4 yaws, 4 moments), read back as a mask at 254×125:
    - 0 touching the edge, the closest 13 px;
    - the bounds' centre within 2.3% of the pad height.
    - **CONTROL:** B317's fixed camera drifts up to 18.5% off centre, with the closest 2 px.
  - **Logo:** 91×60; header, body and frame unmoved.
  - **Lime:** the three pale screens are lime (hue 70-110°, saturation ≥ 0.8) at ≥ 3:1; the dark screens are unchanged.
    - **CONTROL:** gui2's teal is hue 177°.
  - **Ring:** 20 rings (5 schemes × 2 themes × OSC and MAIN). The styles are equal in all 20, and the needle's and bar's pixels are within 0 of the readout ink.
    - **CONTROL:** `--scr-physics` equals the readout ink in 0 of 20.
  - **R bar:** locked, R 0.965 and bar 0.965 of 198 px; splayed, R 0.051 and bar 0.051.
    - **CONTROL:** each bar read against the other state's R fails.
  - **A2 (2) on CORE:** mirror error 0 at ±0.5, 1.5, 2.5, 7.5.
    - **CONTROL:** the oracle alone reads 1.
- **Planted faults** (served rewrites, nothing committed): **9 of 9 CAUGHT**:
  - the re-anchor removed;
  - centring off;
  - the needle back to `--scr-physics`;
  - the bar at 1 − R;
  - teal back on ORCHID;
  - no shed cadence;
  - the logo without negative margins (2 red);
  - the engine cull taking gated voices;
  - `roundAway` removed.
- **`composed_engine_check`:** 85 rows, GREEN (was 71). The new rows:
  - **CULL:**
    - quietest tail first, with the survivors bit-identical to an uncapped twin;
    - never a held note;
    - the fade's gain follows 1 − t/8 ms within 7.4e-15, with the largest step 1/384 a sample, freed after 385 samples;
    - tier 1 before tier 2;
    - note-on at the cap takes a sounding tail;
    - a non-binding cap and cap 0 are bit-identical.
  - **Clicks** (B316 metric, the addendum):
    - a pure sine with the cull forced reads 0 clicks at 6.5 dB against 0 at 6.6 dB uncapped;
    - Crushed bells reads 0 at 7.0 against 0 at 6.2.
    - **CULLc:** the same cull as an instant cut reads 2 clicks at 41.9 dB.
    - **CULLc:** a tier-blind culler.
  - **KQ:** exact mirror at 10 magnitudes on both blades; bit-identical to the oracle at 15 values including every positive half; −2.5 reads 2 (the oracle reads 1).
    - **KQc:** the oracle's Math.round is 1 step off.
- **`lab_load_check`:** GREEN — 56 labs loaded, 0 broken, 1 skipped.
- **`lab_wheel_scroll_check`:** GREEN — 0 failure(s); 6 labs, 2 planted faults (from the verify fast log).
- **Not verified:**
  - Nobody has listened. The before and after figures come from one machine, headless, whose load varied about 2× between sessions.
  - `playbackStats` is a Chrome API; other browsers show no xrun count.

## Screenshots (scratch, not committed; `scratchpad/b323/shots/`)

- 01: the load meter under Crushed bells (light, dark; notes 12 and 16 of the line).
- 02: MAIN under load, the whole frame.
- 03: the header with the 91×60 logo, MAIN and OSC, light and dark (00-before-header-* for comparison).
- 04: the Specimen at the XY's four extremes (pitch ±1.1), light and dark.
- 05: lime on ORCHID, FROST and EMBER (OSC), and ORCHID on MAIN.
- 06: the ring with its matched needle and R bar in ORCHID, TUBE, DUSK and FROST, plus MAIN's mini ring.

## Alternatives rejected

- **A wall-clock read inside the engine:** the DET row forbids it (SPEC §5.7). The shell measures, and the engine takes a cap.
- **An exponential cull fade in 8-sample chunks** (my first build): superseded by the lead's addendum. It stepped −2.9 dB every 8 samples, where a linear per-sample ramp has no step larger than 1/384.
- **Culling held notes when held voices alone exceed the budget:** the brief says never.
- **Re-centring by easing the camera:** it would lag. Exact bounds each frame need no easing.
- **Growing the header for the logo:** it would push MAIN's body, which has no slack.
- **Smoothing the ring's readout to MON.rSm:** the bar follows the printed number instead. Listed as open.

## Verify

- `./verify fast`: exit 0 at git 2a61c66, the code commit (`.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"2a61c66","ts":"2026-09-28T07:04:22Z"}`).
  - The log includes `lab_wheel_scroll_check: GREEN — 0 failure(s); 6 labs, 2 planted faults` and `test_table_check: GREEN (204 tests …; 72 check files declaring WIRED and verified so, 1 declaring UNWIRED and verified so)`.
  - The private-name leak check was SKIPPED (`.leakcheck-names` is absent in this worktree).
- `./verify full`: exit 0 at git 2a61c66 (`{"target":"full","exit":0,"git":"2a61c66","ts":"2026-09-28T07:14:07Z"}`). It was run because the engine file changed, and its log includes `composed_engine_check.mjs: GREEN — composed_engine_check: 85 rows, 0 failed`.
- This trace is committed on top, and `verify fast` is re-run on that hash.

## Open questions

1. **A held chord that alone exceeds the machine still underruns,** because a held note is never culled. At the cap, a new note replaces a sounding tail, or the oldest held note (tier 3). Should the cap instead refuse new notes?
2. **The Coherence readout is the per-frame R,** not the smoothed R (MON.rSm) the logo and the Specimen use. The bar follows the number. Smooth all three?
3. **The shed thresholds** (DSP 0.85/0.6, UI 8/5 ms) and **the cap law's** 0.9/0.75 are first guesses, measured only on this machine.
4. **The ScriptProcessor fallback** is neither metered nor capped. The meter says "fallback, not metered".
