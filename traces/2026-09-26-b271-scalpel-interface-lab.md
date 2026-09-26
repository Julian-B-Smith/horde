# b271-scalpel-interface-lab — horde's OSC and MAIN pages with SCALPEL at the centre, driven by the real oracle

- **Queue item:** B271. The row is carried in the records PR on branch `lead-records-94`, and I read it verbatim from `origin/lead-records-94:ROADMAP.md`. The human, 2026-09-26: "Let's also start another agent building a post-scalpel interface/aesthetics lab for the updated engine".
- **Why:** SCALPEL is to be horde's sound-design centre (B252: "the interface needs to feel really intuitive and clear"). This lab shows what that looks like before any port:
  - the blade core on the face, in the accounting's priority order;
  - visualizers that make blades legible;
  - marked aesthetic alternatives, so the human can choose rather than inherit a default.
- **What changed:** one new file, `docs/design/scalpel-interface-lab.html` (`lab-review` meta `B271 · 2026-09-26`), plus `docs/design/index.html` regenerated.
  - **OSC page** (980×720). The left column holds the Coupling × Detune pad, the phase ring, the spectrum and Advanced. Controls holds:
    - a strip: tab power, Members, Base wave, Volume;
    - the cycle view;
    - blade 1 and blade 2 stacked. Each blade reads: mode, wave, width, cut rate, position, depth, edges, mirror, FM (contextual), and position/cut spreads;
    - "One level down" (T2): blade detail as a blade 1 | blade 2 comparison grid, swarm motion, and the surviving swarm controls;
    - "Advanced" (T3).

    Each collapsed tier heads a summary strip that names every hidden control off its default.
  - **Blade 2 "same as blade 1".** A following control shows blade 1's value with a dashed cap and an =1 mark. Touching it copies blade 1's group across (copy-on-write). An =1 pill re-links without erasing blade 2's own values. This is stored in the existing sentinel and switch rows, so no new parameter.
  - **MAIN page** (the human's B227 wireframe):
    - Morph pad: four corners. A is the patch; B–D are seeded mulberry32 variations of the blade core. Position blends on the shortest arc, and a CIRCULAR toggle shows today's linear blend.
    - XY (Width × Cut rate).
    - Macros, 2 × 4.
    - Mix | Global.
    - FX strip: two racks on X, drawn but not voiced.
    - Osc Controls: Sub / Swarm 1 / Swarm 2 tabs, a 1 | 2 blade selector, and the mini visualizers.
  - **The oracle:**
    - `reference/scalpel/prototype/razor-core.js` is loaded by `<script src>` and never edited.
    - Visuals read a seeded monitor instance through the oracle's own viz feed (`post({t:'viz', mem})`) and its static functions (`voice`, `fillG2`, `fmStep`, `wave`).
    - The monitor advances a whole number of note periods per frame, counted in samples.
    - Sound comes from a second instance in an AudioWorklet built from `RazorCore.toString()`, which is the bench's own route. A ScriptProcessor fallback is stated on screen.
    - `Math.random` is replaced by a seeded mulberry32 before every rendered instance.
  - **Aesthetic alternatives**, each marked STANDARD or DEPARTURE:
    - SKIN: A knobs (standard) · B sliders (standard) · C direct manipulation on the cycle view (departure).
    - WINDOW: I band · II gate. The gate draws g(e)·depth; both use standard tokens.
    - BLADE 2: hatch (standard) · hue (departure; a proposed `--b2`/`--scr-b2` token).
    - CYCLE EDITS on/off, for A and B.
  - **Twelve open questions**, each with a recommendation, in the page.
- **Evidence consulted:**
  - ROADMAP B212, B227, B252, B263, B271 (`origin/lead-records-94`), and B223, B224, B265, B266.
  - `specs/SPEC-SCALPEL.md`; `docs/scalpel/ACCOUNTING.md` §0, §1.0, §1.1, §1.4, §1.7, §1.8, §2, §6 I; `reference/scalpel/data/parameters.json` and `presets.json`.
  - `reference/scalpel/prototype/razor-core.js` in full. `scalpel-bench.html:1940-2272`, where the formant-band rule and the ring drawing come from.
  - `reference/scalpel/verify/{rng,verify,render-goldens}.js`.
  - `docs/design/station-page-lab.html`: the tokens at :50-134 are spliced in verbatim.
  - `docs/design/compact-lab.html`: the knob CSS and the MAIN frame.
  - `docs/design/fx-design-lab.html`: the `<script src>` pattern and TOK.
  - `src/gui/gui2.html` tokens and tabs; `tools/labharness/lab_load_check.mjs`; `tools/gen_lab_index.py`; `tools/serve_labs.py`.
- **Verified vs entailed:**
  - **VERIFIED, in-page self-check 9/9, 3 of them controls that must fail** (read from headless Chrome via DevTools on the working tree that became 17d4719):
    - **Drawn = heard, two configurations.** Configuration 1 is sync + twin −. Configuration 2 is crush + reflect with a blade 2 fold. With 1× oversampling, raw band-limiting and no DC fix, the oracle's output sample, inverted through tanh and gain, equals the painter's per-point composite at the member's phase. Max error 3.3e-8 in both. Control: the drawing shifted ¼ cycle errs by 1.72.
    - **Window.** The window is exactly zero outside itself and 1.90 inside. Control: a bypassed blade reads 0 everywhere.
    - **Ring.** The ring's r reaches 0.966 at Coupling +1 and 0.026 at −1, against verify.js's own thresholds.
    - **Seed.** One seed gives one bit-identical sound. Control: another seed differs.
  - **VERIFIED, real-DOM interaction, 9/9** (scratch CDP harness; real PointerEvent and WheelEvent):
    - a cycle-view drag moves Position by exactly the dragged fraction, and the monitor oracle's target reads the same value;
    - a vertical drag lowers Depth;
    - an edge drag sets Width;
    - a wheel notch multiplies cut rate by 2^(1/12);
    - copy-on-write takes ownership of blade 2's FM;
    - the =1 pill re-links and keeps blade 2's own FM depth (5);
    - the circular morph of 0.95 and 0.05 gives 0.0, where the linear blend gives 0.5;
    - a knob drag writes exactly fromPos(pos + 0.1);
    - Blade 1 off sends width 0 to the oracle.

    The harness found two bugs, both fixed before the commit:
    - the =1 pill captured its state at build time, so a stale click copied blade 1 over blade 2's new values;
    - `os` was never sent to the oracle.
  - **VERIFIED, audible path.** An OfflineAudioContext rendered 0.5 s of A3 through the same worklet: RMS −16.7 dBFS, peak 0.512, 0 non-finite. **Not verified by ear**: no one listened.
  - Tier placement is read from the OSC page's own DOM. 92 rows sit at their accounting tier and 0 are demoted or unplaced. 8 are promoted by the brief (mirror, mirror2, bspread, kspread, bspread2, kspread2, b2sp, b2fm).
  - `lab_load_check`: GREEN on the lab, and the full sweep reads 53 loaded, 0 broken.
- **Alternatives rejected:**
  - **Copying presets or parameters.json into the lab.** It would drift. Presets are fetched when served, and defaults are read from a live oracle instance. The tier/id table IS a copy of ACCOUNTING §1.4 (at f749faa), and it says so in-file.
  - **A lab-local blade renderer.** It would be a second law. The page calls the oracle's own static functions instead.
  - **Wall-clock animation.** The monitor steps in whole note periods.
  - **A new named colour for blade 2 as the default.** Every named token is a status, so blade 2's hue is offered only as a marked departure.
- **Verify:** `./verify fast`, exit 0, git 17d4719 (`.harness/last-verify.json`, 2026-09-26T18:22:36Z). This trace and the index regeneration are committed on top.
- **Screenshots** (scratch, not committed; `scratchpad/b271/shots/`):
  - 01/02: OSC in the standard, light and dark.
  - 03/04: MAIN with the morph on, light and dark.
  - 05/06: skin B.
  - 07/08: skin C.
  - 09/10: window II gate.
  - 11/12: blade 2 hue.
  - 13/14: the depths open.
  - 15: the full page.
- **Open questions:**
  1. The embedded tier/id table copies ACCOUNTING §1.4 and will drift if the accounting is revised. A drift check, like `compact_lab_table_check.py`, is owed. It is not added here, because another agent is editing `./verify` in parallel and the brief kept it out of scope.
  2. The in-page self-check is not gated by `./verify` (the same state as B211/B235/B268).
  3. The Coupling pad voices the oracle's law, not horde's (ACCOUNTING §1.6). The feel will change once the law is ruled.
  4. Swarm 2 and Sub are drawn but not voiced: the lab runs one oracle swarm.
