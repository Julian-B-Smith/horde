# b293-scalpel-lab-round2 — a quieter light mode, the waveform as an XY, and the bench's 76 presets

- **Queue item:** B293. The row is carried in the records PR on branch `lead-records-101`, and I read it verbatim from `origin/lead-records-101:ROADMAP.md`. The human, 2026-09-27, on B271's lab:
  1. "this interface is beautiful and amazing. Only issue is the color scheme can get a little overwhelming in light mode; can we make the blades in light mode a little less opaque, and maybe do something to make the signal pop more?"
  2. "I would like to turn the waveform itself into an XY; horizontal drag should increase the blade width, vertical drag should increase the cut rate, and shift + horizontal drag should drag the position (position is frequently less important than width, thus it not getting primary spot); maybe these commands could be settable in the interface, as in the XY grid of the bench you ingested."
  3. "Can we recreate some of the presets from the bench in here?"
- **Why:** This is round 2 of B271's lab, `docs/design/scalpel-interface-lab.html`, and it answers exactly those three asks. It does not change the design language.
- **What changed:** one file, the lab (`lab-review` meta `B271 + B293 · 2026-09-27`), plus `docs/design/index.html` regenerated.
  1. **Light mode.**
     - Four numeric screen weights sit beside `--scr-alpha`: `--scr-blade-a`, `--scr-sig-w`, `--scr-halo-w` and `--scr-member-a`. Only the light tube changes them. Dark keeps round 1's values, so dark is unchanged.
     - In light, blade fills drop to 0.4 of their old alpha, and window edges drop by the square root of that.
     - The heard trace goes from 1.8 px to 2.6 px, over a 2 px tube-coloured halo on each side.
     - The member traces drop to 0.6 of their old alpha.
     - The trace's ink stays `--scr-meter`: teal, which is not the blade hue. No colour changes job and no token is added.
     - The spectrum's formant bands thin with the blades.
     - Cause: `--scr-alpha` 1.6, which lifts faint strokes on the orchid tube, was also lifting the blade fills. So 5 overlapping member windows went solid magenta.
  2. **The waveform is an XY** (section I2, OSC page, whenever CYCLE EDITS is off).
     - **Default mapping:** horizontal = Width, vertical (up) = Cut rate, shift + horizontal = Position.
     - **Settable:** the XY MAP pill opens the bench's axis row (`scalpel-bench.html:158-159`, `:1411-1425`) for each of three slots: X, Y and ⇧X. Each slot has a primary, a "+ second…" and an inv for the second.
     - **What an axis can drive:** 20 blade roles and 6 swarm roles. These are the bench's pad-assignable keys, with each blade-1/blade-2 pair folded into one role. `k`/`kHz` and `m`/`mHz` resolve by the blade's own "Cut rate in" / "Mod rate in", which is the job the bench's swapAxis() does.
     - **Relative drags.** Nothing moves on grab. Each move adds pixels to a per-key accumulator in taper units: 420 px crosses a whole taper. Position moves one cycle per canvas width and wraps. Alt is fine (×¼).
     - **Target:** the blade whose window, as drawn, is under the pointer at grab. Empty space grabs every blade that is on. The target is outlined in `--scr-marker` on hover and while dragging, and a readout shows every value being moved.
     - **One writer.** A new `writeMany` runs the same copy-on-write and a single `afterWrite`, so the knobs, the summaries and the oracle read one `P`.
  3. **Presets.**
     - All 76 from `reference/scalpel/data/presets.json` (fetched, never copied) sit in a side-column browser. They are grouped by the file's `category`, which is the bench's own grouping: Starting points 36, Two-blade 17, and Showcase Growls 4 / FM sines 4 / Movement 4 / Leads 4 / Pads 4 / Oddities 3.
     - They play exactly as the oracle defines them.
     - A caution-ink banner, a BENCH VOICING tag on the patch name, and MAIN's preset line all say they are not re-voiced for horde's coupling law (ACCOUNTING §1.6, Q B3).
     - `cScale` (Coupling time) joined the lab's table as ACCOUNTING §1.4 row 30 (MERGES → absK 31, T3). It was the one preset key B271's table lacked, and 5 presets set it.
     - The lab has no undo history, so a load is not a history step.
- **Evidence consulted:**
  - ROADMAP B252, B271, B293 (`origin/lead-records-101`).
  - The B271 trace.
  - `docs/scalpel/ACCOUNTING.md` §1.4 (rows 23, 30, and Position's circular note), §1.6/§1.6.1, B3, J2 (`:1061`).
  - `reference/scalpel/prototype/scalpel-bench.html:140-185, 1015-1060, 1395-1440, 1515-1700, 2155-2185`.
  - `reference/scalpel/prototype/razor-core.js:271, 295-320` (set() silently ignores unknown keys, which is why the key check exists).
  - `reference/scalpel/data/parameters.json` and `presets.json`.
- **Verified vs entailed:**
  - **VERIFIED, in-page self-check 17/17, 4 of them controls that must fail.** Read from headless Chrome via DevTools on the committed lab. The 9 from B271 are unchanged. New:
    - **The grab alone moves nothing.**
    - **XY round-trip.** The gesture functions a pointer drives move Width, then Cut rate, then Position (with a wrap). Reading back gives w 0.3493 = expected, k 7.387 = expected, and c 1.15 → 0.150. Blade 2 is untouched, and the monitor oracle's target `t` agrees with `P`.
    - **Empty space moves both blades,** each by the same taper delta.
    - **A remapped axis works:** Depth + inverted Edges gives depth 0.9 and edges 0.2, with width held.
    - **Preset keys:** 76/76 are clean against the lab table, the oracle's keys and parameters.json (107).
    - **CONTROL:** an invented key is caught by all three sets.
    - **Every preset plays:** 76/76 are finite and audible in 2048 samples at A3.
    - **The XY's 50 reachable keys match parameters.json's ranges.** `hard2` is not pad-assignable in the bench; this is reported, not failed.
  - **VERIFIED, real mouse** (a scratch CDP harness using Input.dispatchMouseEvent), on the Two clocks preset:
    - Grabbing blade 2's window and dragging right and up moved w2 0.18 → 0.4273 and k2 9 → 12.478. Blade 2's knobs read 43% and 12.5×, the oracle's `t` read the same, and blade 1 was untouched.
    - A shift-drag on empty space targeted both blades and moved c 0.3 → 0.4 and c2 0.7 → 0.8, with the knob reading 144°.
    - A click without motion left every value bit-identical.
    - **The harness found one bug, fixed before the final commit.** The hit-test used the static Position, but Rotate (0.3 Hz in Two clocks) had moved the drawn window from 0.20–0.40 to 0.31–0.51. So a grab on the visible window reported "empty space". Hit-testing and the outline now read the per-member windows from the oracle's viz feed.
  - `lab_load_check`: GREEN on the lab.
  - Tier audit from the page's DOM: 93 rows sit at their accounting tier (B271's 92 plus `cScale`), with the same 8 promotions and 0 demoted or unplaced.
  - **Not verified by ear.** Nobody listened to the presets in this session.
- **Alternatives rejected:**
  - **An absolute XY, as on the bench's pad.** The waveform's x axis is phase, so there is no puck to grab, and an absolute map jumps the sound on every grab.
  - **A new colour token for the signal.** Every named colour is a status, and teal already contrasts with the blade hue. Weight and a halo were enough.
  - **Hit-testing the static window.** It is wrong under rotation and spreads; the bug above.
  - **Copying presets.json into the page.** It would drift.
  - **Adding undo history so a preset load could be one step.** The brief says "if the lab has history", and it does not. Adding it would invent scope.
- **Verify:** `./verify fast`, exit 0, git 3ebf73b (`.harness/last-verify.json`, 2026-09-27T00:30:34Z). This trace and the index regeneration are committed on top.
- **Screenshots** (scratch, not committed; `scratchpad/b293/shots/`):
  - 01–04: before (light default, light Two clocks, light gate, dark Two clocks).
  - 05–07: light after.
  - 08, 15: dark after.
  - 09: XY mid-drag on blade 2.
  - 10: XY mid shift-drag on both blades.
  - 11: the mapping row (Y second = Depth, inverted).
  - 12: the preset browser.
  - 13: the stage with a preset loaded.
  - 14: MAIN, light.
- **Open questions:**
  1. The drag-target rule: the window under the pointer, else all blades. It is in the page as Q13. With a wide Position spread, blade 2's scattered windows cover much of the cycle, so "empty space" gets rare.
  2. Should MAIN's mini cycle view and its absolute XY pad take the same mapping? They are left as B271 built them.
  3. The mapping is lab state, reset on reload. The page recommends a global UI preference.
  4. `hard2` is missing from the bench's pad-assignable set, while `hard` is in it. This looks like a packet omission; the lab allows it.
  5. The brief says horde's "Cohesion law". ACCOUNTING calls it horde's coupling law (§1.6), and the lab uses ACCOUNTING's wording.
  6. The in-page self-check is still not gated by `./verify`, the same state as B271.
