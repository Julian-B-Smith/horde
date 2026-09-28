# b318-maw-horde-lab — MAW as it will live in horde: FX-C in the rack, its face and panel, its own core playing a horde voice

- **Queue item:** B318. I read the row verbatim from `origin/lead-records-123:ROADMAP.md` (records PR #811), with B50, B92, B169, B170 and B302 beside it. The human, 2026-09-28: "Could you also please start agents building the design labs for Maw and Sluice as they'll live in Horde?"
- **Why:** MAW is horde's FX-C (the ADR-092 amendment "Maw eats it"; ADR-170). No lab showed it the way it will sit in horde: in the rack, with its role macros over its full panel, heard on a horde voice. This is that lab, in the new labs' look, which is the basis for GUI 3 (B302).
  - New file: `docs/design/maw-horde-lab.html`.
  - Nothing in `src/`, `specs/`, `reference/`, ROADMAP, DECISIONS, LIBRARY or INDEX was touched. `docs/design/index.html` was not regenerated. No other lab changed.

## What the lab is

- **One plugin window**, 980×720 (the SCALPEL lab's frame), on horde's FX page.
  - **Rack strip.** Voice Σ → **1 · MAW · FX-C** → 2 FILTER → 3 DELAY → 4 ROOM → OUT.
    - Slots 2–4 are today's `FxType`s (`src/fx_rack.h:82-99`). They are context and are not rendered.
    - Slot 1's bypass is the header's MAW IN / BYPASS button.
  - **Face.** MAW's preset select, then the four role slots in fixed order (SPEC-MODULE-MACROS §9): Amount "Drive", Tone "Tone", Motion "Motion", Regen "Feedback".
    - Each slot has a tier toggle. **▲ GLOBAL** puts it on horde's face and in the DAW's list as `FX1 <Role>`, ids 300–303 (ADR-169 A1). **◇ CORNER** makes it morph-owned and not DAW-exposed.
    - A readout under each slot says what it moves.
    - Regen is drawn IDLE, in caution, outside the feedback route (fx-design-lab's rule).
  - **Topology.** Five route pills and the route diagram.
  - **In · out · behaviour.** input, wet, output, auto-gain, inertia, ecology, flux, drive-ref and ADAA.
    - A note lists what horde drops: tracked auto-gain (D3) and per-stage oversampling (SPEC-MAW §10).
  - **What MAW rendered** (pinned at the top of the right column):
    - the transfer curve per stage, with the band where the signal sits;
    - input and output spectra, plus a scope;
    - a drive meter per stage over the loop: gain into the curve, auto-gain makeup and stage level, with a playhead cursor.
  - **Inside the module** (scrolls under the pinned visualisers). Every internal parameter, per ADR-169 A2 ("the face summarises, never hides"): three stages, the crossover, the feedback route and the prototype-local modulation.
    - A violet arc runs from the authored value to the value after the macros.
- **Side column:**
  - the rack-input source switch;
  - "the DAW sees": the global-tier slots, and a count of the 70 internals that are never exposed;
  - MAW's fidelity report (31 PASS · 1 FAIL, the FAIL being §7's deliberate control), with its §4 aliasing figure printed under each stage's curve.
- **Open questions:** nine MAW-in-horde decisions for the human, on the page.

## Decisions taken in the lab (each is on the page)

1. **Input: the composed engine, plus a switch.**
   - Sources:
     - **SAW SWARM:** `docs/design/scalpel-horde-engine.js` at blade depth 0 with a saw base.
     - **SCALPEL PATCH:** any of the 83 bench presets.
     - **NOISE** and **DRUM:** seeded, engine-free.
   - The engine is loaded exactly as `scalpel-interface-lab.html:6119-6135` loads it.
   - Measured (scratch probe, node): depth 0 + base Saw renders harmonics H2..H8 at −6.1 … −18.2 dB re H1. That is within 0.2 dB of 1/n, a saw.
   - **Why the engine:** FX-C sees the voice sum, and a swarm's beating is what a saturator's curve, ecology and drive-ref react to.
   - **Why the switch as well:** a drum is where drive-ref, the floor and ecology show themselves. The two engine-free sources also work from the filesystem.
   - Every source is normalised to 0.5 peak, SPEC-MAW §2.4's reference level (open question 5).
   - Every source is written as `v + 0`, so no sample is IEEE −0. See "Bypass" below.
2. **Rendered offline, then looped.**
   - The source and MAW render a 2.005 s buffer (94 × 1024 samples, note held to 1.408 s), which is played by an AudioBufferSourceNode.
   - A parameter change re-renders the buffer.
   - Every picture is therefore a measurement of the buffer you hear (L0064).
   - The only clock read is the AudioContext's, and it only moves the playhead cursor.
3. **The curve is the instance's, not a UI twin.**
   - `mawTransfer()` evaluates `Stage.shapeDriven()` on the rendering instance at its control targets (`tick(1)`), times auto-gain and drive-ref, then applies mix and level. That is `process()`'s arithmetic in its order (`core.js:145-149`), minus the DC blocker and the tone filter.
   - The noise curve reads a silent RNG, not the instance's stream.
   - Every field it touches is restored, so drawing never changes the audio.
   - Snapshots are taken every 1024 samples during the render. While playing, the curves move with the env/LFO routes, inertia, ecology and flux.
4. **The macro distillation is the lab's.**
   - MAW's presets carry no macros. Roles follow ADR-170 A1. Bindings are relative (ADR-169 A2), with the home reproducing the preset (the G-70 rule Sluice's presets follow):
     - Amount: every stage's drive, ±18 dB (offset).
     - Tone: shape ±0.5 (offset) and cutoff ×/÷4 (ratio).
     - Motion: inertia → 1 and flux → 0.8 (toward), home 0.
     - Regen: fbAmt ±0.5 (offset).
   - Open question 3 asks who should own this.
5. **fbAmt runs 0..1.2.** SPEC-MAW §6 intends >100%. fx-design-lab stops at 1.
6. **No wheel-driven controls** (drag, shift-drag, double-click to reset). `lab_wheel_scroll_check`'s inventory is therefore untouched, and its static layer passes this lab (no wheel registration anywhere).
7. **No morphing logo:** it is ~200 lines and a later round unifies chrome (the brief: "only if cheap").

## Taken from other labs (cited in the file header)

- `fx-design-lab.html:705-795`: MAW's parameter rows, ranges and ghost rules; the four-role declaration; Regen IDLE outside the feedback route; the D3/§10 ghost rows.
- `fx-design-lab.html:1752-1790`: the topology layout.
- `scalpel-interface-lab.html:164-230`: tokens for both themes, plus its chrome, knob, pill and window CSS, TOK/TOKA/fit, `el`, `mulberry32`, and the engine load route.
- `reference/maw/fidelity.js:4-5`: the FFT.
- **NOT taken:** fx-design-lab's `drawMaw`/`mawCurve`. It computes from UI params, which SPEC-MAW §10 says is not evidence. Here that method is the must-fail control of self-check 2.
- `fx-page-lab.html` and `fx-chain-morph-lab.html` hold no MAW material (`grep -ci maw` = 0 for both).

## Evidence

- **VERIFIED, in-page self-check 5/5, with 6/6 must-fail controls caught.** Read from headless Chrome's dumped DOM (served on 8318; the default URL, saw swarm). The same rows are GREEN on every screenshot state below.
  1. **Finite and audible for every preset.**
     - 10 presets × (own intake source + 3 lab sources) = 40 renders, with 0 non-finite samples and no silent preset.
     - **CONTROL:** one NaN planted at input sample 100 is caught. 12188 of the 12188 following samples are non-finite, which shows the intake core's NaN poisoning (ADR-170 A3).
  2. **Drawn curve = rendered transfer.**
     - Setup: 25 stage curves over 10 presets, 161 points each, drawn vs the stage's own `process()` one sample at a time. Conditions: os 1, ADAA off, tone filter off, a fresh DC blocker per sample, the same seeded RNG on both sides.
     - Result: max error **0.0e+0** (bit-exact).
     - **CONTROL:** the UI-param preview is caught on 10/10 presets (error > 0.01).
  3. **Bypass is bit-identical.**
     - 33/33 compared bit for bit (Uint32 views): the rack slot's bypass on 3 sources, plus MAW at wet 0 / output 0 dB on 10 presets × 3 sources (SPEC-MAW §11 row 2).
     - **CONTROLS:** the slot engaged differs 3/3. Wet 0 at the prototype's −6 dB output differs 30/30.
  4. **Every preset key exists in MAW's parameter set.**
     - 182 keys over 10 presets are all in `defaultParams()`/`defaultStage()`, and every curve name resolves.
     - **CONTROL:** planted `stages[0].drvie`, curve `"tubee"` and `wett` are caught 3/3.
  5. **Macro homes reproduce the preset.**
     - 10/10 presets come through `resolve()` bit-identical with all four slots at home.
     - **CONTROL:** Amount 0.05 off home moves an internal on 10/10.
- **Found while building:**
  - The −0 case. A seeded noise gate starts at `(r·2−1)·0`, which can be −0. MAW's dry sum returns +0 for it, and a bit-compare would report the sign of zero. Sources are canonicalised with `+ 0` at their one writer, and the comment says why.
  - Under Node's `vm` with the references loaded (a scratch harness, not the load checker, which loads no `<script src>`), the boot takes ~42 s: runChecks 27.9 s, one engine source 11.2 s, one MAW render 1.5 s. The same MAW render takes 60–130 ms in plain Node. My hypothesis is global-lookup interception in the contextified sandbox; it was not profiled. Chrome's boot was not timed. This is why the self-checks are not run headless in `./verify` this round (open question below).
- **`lab_load_check`:** GREEN, 55 labs loaded, 0 broken, 1 skipped (the full sweep, including this lab).
- **Verify:** `./verify fast` exit 0 on the lab commit. `.harness/last-verify.json` reads `{"target":"fast","exit":0,"git":"3604f70","ts":"2026-09-28T01:47:35Z"}`. `lab_load_check` is quiet on green inside verify; run standalone it is GREEN (see above). `lab_wheel_scroll_check`: GREEN, 0 failure(s); 6 labs, 2 planted faults. `private_name_gate` was SKIPPED (`.leakcheck-names` is absent in this worktree). This trace's own commit was re-verified; the hash is in the PR.
- **Not verified:**
  - Nobody has listened. Audio playback was read in the code, not heard.
  - Knob-drag cost in a real browser was not measured (a full re-render per change, ~100 ms estimated from Node timings of MAW's 1.5 s render at 60–130 ms).

## Screenshots (scratch, not committed; `scratchpad/b318/shots/`)

- 01 light: saw swarm on A2, preset init, all slots at home.
- 02 dark: the same state.
- 03 light: SCALPEL "Two blades" through "roar growl", with Amount 0.7 and Tone at corner tier (so the DAW list drops Tone). Knobs show the post-macro drive, and Regen shows IDLE.
- 04 dark: DRUM through "pitch-tracked feedback · tamed", with Regen 0.62 and Motion 0.3. The feedback route is drawn with its tap and pitch.
- 05 light: saw chord (45/52/57) through "fractal grit · multiband", with Tone 0.8. The crossover is drawn, and the dirty-by-design curves are visible.
- 06 dark: SCALPEL "Golden bells" on E2 through "cheby ladder" (drive-ref on, ADAA on).

## Alternatives rejected

- **A toy oscillator as the input:** the brief prefers a horde voice, and the composed engine is already the lab-standard route.
- **Drawing curves from UI params** (fx-design-lab's method): SPEC-MAW §10 says it is not evidence. It is the control instead.
- **A live AudioWorklet of MAW:** pictures would then be of a different run than the one heard. Offline render-and-loop makes them the same buffer.
- **Absolute macro spans:** a slot's home would not reproduce the preset, and ADR-169 A2 allows relative bindings.
- **Adding the lab to the wheel inventory with alt-wheel knobs:** it is not needed for the brief and it widens the scope into `tools/`.

## Open questions

- The nine on the page. In short: fixed post-stage vs matrix slot; MAW as B92's second note-hearing consumer; who authors MAW's macro distillation; Regen outside the feedback route; the input level into FX-C; switching to MAW's current core when its ABI lands; the prototype OS vs horde's 2×; mod-matrix reach into internals; chrome.
- Should this lab's five self-checks become a headless `./verify` gate like `fxlab_check.mjs`? This round did not add one. In a `vm` sandbox with the references loaded, the boot took ~42 s, so it would need a lighter harness (evaluate `reference/maw/core.js` in the main realm).
