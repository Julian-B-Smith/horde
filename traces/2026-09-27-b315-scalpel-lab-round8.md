# b315-scalpel-lab-round8 — bipolar spreads; gui2's morphing logo; the Specimen under MAIN's XY

- **Queue item:** B315. I read the row verbatim from `origin/lead-records-121:ROADMAP.md` (records PR #807), with B309 beside it. The human, 2026-09-27:
  1. "I want all the "spread" controls in "advanced" (as well as position and cut rate spread) to be bipolar."
  2. "I would like to port over the dynamic morphing logo from the original Horde, as well as Specimen; this, I think, can become an underlay of the surface for the main XY."
- **Why:** This is round 8 of B271's lab, `docs/design/scalpel-interface-lab.html`.
  - Isolation: the lab, plus one engine-side law in `docs/design/scalpel-horde-engine.js`. Nothing in `src/`, `specs/` or `reference/` was touched. `docs/design/index.html` was not regenerated.

## What changed (commit `769e8c4`; `lab-review` meta `B271 + B293–B315 · 2026-09-27`)

1. **Bipolar spreads** (section A, `BIPOLAR`).
   - The fourteen keys (`bspread`, `kspread`, `wspread`, `dspread`, `mspread`, `ispread`, `rotSpread` and their `…2` twins) run −max..+max, with 0 at the centre.
     - The ranges are ±1, ±24 and ±2 Hz.
     - Rotate spread uses a new `bpow` taper: the old `pow` mirrored about the centre.
     - The units are signed.
     - Sliders fill from a full-height zero tick. Knobs already drew bipolar arcs (`knob()`, min < 0 < max).
   - **Per-parameter verdict.** It was read off `reference/scalpel/prototype/razor-core.js` `spreadMember` (:103-136) and `kSpread` (:144-160). Every offset is amount·pn(j), and pn is centred on 0 under every spread law:
     - gradient ±½;
     - alternate ±½;
     - random and drift draws around 0 (clamped ±0.75);
     - the swarm lead in [−½, ½).

     So a negative amount already means the mirror there.

     | Parameter | Blade 1 | Blade 2 | Offset field | Verdict |
     |---|---|---|---|---|
     | Pos spread | `bspread` | `bspread2` | `cOff`, `cOff2` | Oracle-native |
     | Cut spread | `kspread` | `kspread2` | `kAdd`, `kAdd2` | Oracle-native (caveat below) |
     | Width spread | `wspread` | `wspread2` | log2 `wMul` / `wMul2` | Oracle-native |
     | Depth spread | `dspread` | `dspread2` | `dAdd` / `dAdd2` | Oracle-native |
     | Shape spread | `mspread` | `mspread2` | `mor` / `mr2` | Oracle-native |
     | FM idx spread | `ispread` | `ispread2` | log2 `iMul` / `iMul2` | Oracle-native |
     | Rotate spread | `rotSpread` | `rotSpread2` | `rotOff`, `rotOff2` | **Divergence** |

     - **Cut spread caveat.** Under a Cut rule it stays inert either side of 0 (B297; checked bit-identical). With Quantize on, `Math.round` rounds halves up, so the mirror is off by one step at exact half-integers (open question).
     - **Rotate spread divergence.** `rotOff = x·2·pn(2)` is fine, but the render gates member rotation on `t.rotSpread > 0.004` and `t.rotSpread2 > 0.004` (:786, :791). A negative spread therefore homes every member to 0.
     - **Where the divergence lives.** It is in the composed engine. `set()` hands the oracle |x| and keeps the sign. `spread()` negates `rotOff`/`rotOff2` after the oracle's own law, and keeps `rotOff2 = rotOff` when blade 2 does not own its spreads.
     - **Why it is safe for existing patches.** At x ≥ 0 nothing runs, so every existing patch is bit-identical. The worklet bundle (`CORE.toString()`) carries it.
     - **An ADR note is owed.**
   - `parameters.json` still says 0..max. The XY-range check accepts exactly this widening for exactly these keys (`rangeDrifts`). A control checks that the same widening on an undeclared key (Edges) is caught.
   - **Macro and morph depths** on the spreads are halved in taper units, so they reach the same values:
     - Spread: 0.5 → 0.25 and 0.25 → 0.125;
     - Motion's Rotate spread: 0.3 → 0.15;
     - the morph's corner offsets: ×½.
2. **The morphing logo** (section E2). This is a port of gui2's wordmark: the td::Warp engine at `src/gui/gui2.html:6221-6350`, the driver at :6352-6645, and the tokens and lessons at :170-262.
   - **The session.** The human's tool session (blobs and swirls) is kept verbatim.
   - **Placement.** The mark sits in the frame header, replacing the plain "horde" text: gui2's slot, beside the tabs. The page title "scalpel" is lab chrome and stays.
   - **What drives it.** gui2's own lexicon, because every input exists here:
     - morph on: the colour is the morph cursor, a bilinear blend of `--lgA..D` at (MORPH.x, MORPH.y). The brief said the lab has no morph corners, but it does: MAIN's Morph pad.
     - morph off: ADR-150's slow hue drift;
     - R (`MON.rSm`) drives the tremor;
     - Detune (EFF.detune/100) drives the waveAmp.
   - **The one departure.** The clock is the engine clock (MON.clock/SR), not rAF's (B271), so stills freeze it.
   - **gui2's lessons kept:**
     - Display-type corner colours, with an ink outline (`--lg-edge`) in light mode only.
     - A fixed 70×46 box that is no taller than the tabs. It was checked: the header is 47.0 px with the mark and 47.0 without.
     - Its cost is capped by the backing store: a 2× bitmap of 140×92, a quarter of gui2's, painted on at most every other frame.
     - One canvas for the lab's life, re-parented at each rebuild.
3. **The Specimen as the underlay of MAIN's XY** (section E3). This is gui2's CHROME-002: the shader, spring, fronts and mappings verbatim from `gui2.html:5002-5463`; the toggle is param 178, gui2.html:1387.
   - **Which XY.** It sits under MAIN's pad titled "XY" (FREE TO REASSIGN): it is MAIN's XY, its marks are sparse, and it has no data job yet (Q38). The ring is a data well, and a pearl there would compete with what it measures.
   - **How it sits.** It is a WebGL canvas behind the pad's 2D canvas, with `pointer-events: none`.
     - `paintXY` paints a 35% tube veil, opaque tube plates under its three texts, and a tube ring on the carried marker. The puck already had one.
     - The Width bypass band now draws under the labels, because it was tinting the plate.
   - **Inputs from the monitor.** The voices' gates strike the pearl (a fresh note pokes it), the envelopes bloom it, the last strobe's peak makes it quiver, R pulls it taut, and Detune sets the ripple depth.
   - **gui2's lessons kept:**
     - Never grab '2d' on it: it is not in CANV.
     - Create-on-show.
     - One canvas and context for the lab's life, because a context per rebuild would hit the browser's context cap.
     - Why-it-failed is shown on the pad and the pill.
   - **Departures:**
     - It runs on the engine clock, and draws only when that clock moved, every third frame (gui2's 20 Hz).
     - The resolution is fixed at min(dpr, 2), because gui2's adaptive scale read wall-clock frame times.
     - It has no drag-to-orbit or click-to-strike: the pointer is the pad's.
   - **The toggle.** A SPECIMEN pill (default on; `?specimen=0` starts it off). With no WebGL, the pad is its plain opaque surface.
4. **The page text:** the header comment (ROUND 8), the badges, and open questions 54–59.

## Evidence

- **VERIFIED, in-page self-check 66/66, 25 of them controls that must fail** (55/55 with 19 before). It was read from headless Chrome (real GPU, ANGLE Metal on M3) on OSC, on `?page=main&theme=dark`, and in skin C with `direct=1`. The 11 new rows:
  - **Bipolar mirror,** on a seeded scratch of the composed engine (N 5, gradient law, K 0, detune 0). For each of the 14 keys, max |f(−x) + f(+x)| is ≤ 2e-16.
    - `mspread2` has no offset field, so it is held to the across-member mirror, and its error is 0.
    - Rotate spread is also read as rendered rotation after 0.2 s: the error is 2e-13 on both blades.
    - 0 is the default, the centre, and writes no offset.
  - **Cut spread ±7.2 under the harmonic rule** renders bit-identically on both blades.
  - **CONTROL:** the oracle alone does not mirror Rotate spread (error 0.110 cycle).
  - **CONTROL:** a clamped law (max(0, x)) is not a mirror (0.150).
  - **XY ranges.** 53 reachable keys match, 14 of them the declared widening.
    - **CONTROL:** an undeclared widening is drift.
  - **Logo.** The mark is in the header in a 70×46 box, with 4704 ink px. The warp moves 3187 px between t 1.0 and 2.5. With morph at B the colour is #ff2e88 (= `--lgB`). The header is 47.0 px with the mark and 47.0 without.
    - **CONTROL:** the mark in gui2's 96 px box makes the header 96.0 px.
  - **Specimen, light and dark.**
    - The pearl covers 13% / 12% of the underlay, which was read back.
    - The pad is see-through (alpha 89). The 3 text plates are opaque and in the tube colour, and the puck has alpha 255.
    - The underlay has `pointer-events: none`, and `elementFromPoint` finds the pad.
    - A click round-trips Width 0.0202 and Cut rate 12.264 into P, and the engine's `t` agrees.
    - **CONTROL:** the underlay given the pointer takes the click, and nothing moves.
    - **CONTROL:** with the Specimen off, the pad is opaque (alpha 255).
  - **Fallback** (a simulated refusal): the underlay is hidden, the pad has alpha 255, the pill reads "SPECIMEN n/a", and the reason is painted on the pad.
- **Planted faults** (served through a DevTools response rewrite, nothing committed): 5 of 5 CAUGHT.
  - Engine's bipolar rotation disabled: the bipolar row turns red.
  - Underlay given the pointer: the Specimen row turns red.
  - Pad opaque over the Specimen: red.
  - Text plates removed: red.
  - Logo in gui2's 96 px box: 2 red.
- **Frame cost, headless, MAIN, M3 GPU.** Each figure is ms per frame, the mean of 240 frames after 30 warm-up frames. Each frame is monStep + paintCanvases, plus the named part forced every frame; the Specimen is synced by a 1-px readPixels, so its GPU time is counted.

  | Frame | dpr 2 | dpr 1 |
  |---|---|---|
  | Base | 1.37 | 1.34 |
  | + logo | 2.07 | 2.06 |
  | + Specimen | 2.57 | 1.35 (unsynced) |
  | + both | 3.66 | — |

  - **Per draw.** The Specimen is 0.99 ms at 508×212. The logo is 0.90 ms per paint, and one warp alone is 0.80 ms.
  - **At the real cadence** (Specimen every 3rd frame, logo every 2nd), they add about 0.33 ms and 0.45 ms per frame.
- **`lab_load_check`:** GREEN — 54 labs loaded, 0 broken, 1 skipped.
- **`lab_wheel_scroll_check`:** GREEN — 0 failure(s); 6 labs, 2 planted faults. For SCALPEL: 20 wheel elements, 0 plain-wheel changes, alt moved 19/20, and 103 clicks in the scroller, 0 moved.
- **Not verified:**
  - Nobody has listened to a negative spread.
  - No human has looked at the logo or the pearl on a real display.
  - The cost figures come from one machine, headless.

## Screenshots (scratch, not committed; `scratchpad/b315/shots/`)

- 01: Advanced with the fourteen spreads set to mixed signs, the whole frame, light and dark.
- 02: the Advanced spreads table, zoomed, with fills from the zero tick, light and dark.
- 03: OSC's face with Pos spread −0.4 and Cut spread +6 as bipolar knobs, light and dark.
- 04 / 05: the header logo, morph off and morph at corner B, light and dark.
- 06: MAIN's XY over the Specimen, light and dark.
- 07: MAIN, the whole frame, light and dark.
- 08: the XY with the Specimen off.

## Alternatives rejected

- **Clamping negative Rotate spread lab-side, or leaving it native:** the oracle would home the members, so the knob's left half would be dead.
- **Editing the oracle's two comparisons:** `reference/` is protected. The engine wrapper is the brief's sanctioned place.
- **Keeping macro and morph depths unchanged:** every macro would reach twice as far on these keys.
- **Replacing the page's "scalpel" title with the logo:** the product's mark belongs in the product's frame, where gui2 puts it.
- **Keeping gui2's rAF clock and adaptive resolution:** both read the wall clock, which B271 forbids for what is drawn.
- **Putting the Specimen under the ring:** it would compete with the data the ring measures.
- **Specimen off = no context (gui2's rule):** here off hides the underlay and stops drawing. A context is created only on first show, and one per lab lifetime.

## Verify

- `./verify fast`: exit 0 at git 769e8c4 (`.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"769e8c4","ts":"2026-09-28T00:07:09Z"}`).
- `./verify full`: exit 0 at git 769e8c4 (`{"target":"full","exit":0,"git":"769e8c4","ts":"2026-09-28T00:10:57Z"}`). It was run because the engine file changed, and its log includes `composed_engine_check.mjs: GREEN — composed_engine_check: 71 rows, 0 failed`.
- The log notes that the private-name leak check was SKIPPED (`.leakcheck-names` is absent in this worktree).
- This trace is committed on top, and `verify fast` is re-run on that hash.

## Open questions

1. **An ADR note for the Rotate-spread divergence.** Or sanction an edit of the oracle's two gates, and amend `parameters.json`'s ranges.
2. **Snapped Cut spread at exact half-integers.** Math.round rounds halves up, so the mirror is off by one step there. Accept, or round half away from zero.
3. **Is MAIN's "XY" pad the XY the human meant?** And should the pearl take gestures once the pad has its job (Q38)?
4. **The logo's size.** It is 70×46, sized to the tab rows. gui2's is 146×96, which would push the lab's 720 px frame.
