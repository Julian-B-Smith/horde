# b309-scalpel-lab-round7 — the waveform stays in view; the lab plays the composed engine; interplay drawn as heard

- **Queue item:** B309. I read the row verbatim from `origin/lead-records-115:ROADMAP.md` (records PR #798), with B298, B300 and B310 beside it. The human, 2026-09-27:
  1. "Could we have the waveforms stay visible at the top of the controls panel when scrolling to the advanced controls? It's hard to estimate effects without visuals."
  2. On horde's swarm laws being greyed out: "Can they be reintroduced?"
- **Why:** This is round 7 of B271's lab, `docs/design/scalpel-interface-lab.html`.
  - The lab played the SCALPEL oracle alone. So it greyed onset, dissolve, drift, inertia and the detune laws, and it played SCALPEL's coupling law.
  - It drew v1.1's interplay as a parallel sum, and B300's notice admitted it.
  - Isolation: one file plus this trace. Nothing in `src/`, `specs/`, `reference/` or `docs/design/scalpel-horde-engine.js` was touched (B310 edits the engine in parallel, and this swap picks it up on merge). `docs/design/index.html` was not regenerated.

## What changed (commit `9d37b60`; `lab-review` meta `B271 + B293–B309 · 2026-09-27`)

1. **The pin** (buildOsc, `.pin`).
   - The view row, the cycle view and the beam form one `position: sticky` block at the top of OSC's Controls scroller. The XY mapping row folds open inside it.
   - The strip above (Osc on, Members, Base, Volume) scrolls away. One level down and Advanced scroll beneath.
   - The pin has an opaque panel ground and z-index 5, above the knobs' own z-index 2.
   - `rebuild()` sets the scroller's `scroll-padding-top` to the pin's height, so anything the browser brings into view lands below the pin.
   - MAIN has no scroller, so its cycle views were already always in view. MAIN is unchanged.
2. **The lab plays the composed engine** (section A, `loadEngine`).
   - **Building it.** `CORE` is B298's `makeComposedEngine(RazorCore, swarmSrc)`. It is built at boot from the oracle class and `reference/swarmsaw.html`'s DSP section, fetched and sliced by the engine module's own `swarmSourceFromHtml`, which is B298's page route.
   - **Why it cannot be one line.** The top-level `const CORE` became `let CORE = null`, built at boot, because only a fetch can read the swarm source.
   - **Defaults.** `RAZOR` (the oracle) still supplies the SCALPEL defaults synchronously, so the controls build first. The engine's own `d` keys join `ORK` and `DEF` at their constructor values.
   - **One engine.** The monitor, the audio worklet and every self-check build the composed engine. The worklet takes `CORE.toString()`, the engine's self-contained bundle, unchanged.
   - **Voiced controls at ACCOUNTING's tiers:**
     - T2: Onset lock, Dissolve and Drift.
     - T3: the detune law (`h.law`), Octave spread, Root anchor, Harmonic reach, Stretch B, Drift rate (`h.driftRate`), Inertia and Inertia curve.
     - G6/G7 have no tier column, and §2's T2 list omits Inertia. They go to T3 by "every surviving specialist control" (open question).
   - **Inert marking.** A sub-parameter the current law does not read is drawn inert with the reason (`HORDE_INERT`, from `swarmsaw.html` controlTick). This is B297's idiom.
   - **Tempo grid.** Law 3 is listed and disabled, with the reason "not in the JS reference".
   - **Onset below 0.** It carries a tooltip note always, and a caution box while `onset < 0` (B298 gap 1).
   - **Presets.** The preset banner, the BENCH VOICING tag and MAIN's patch line say the presets **lock less here**, citing B298's R 0.42 against 0.87.
   - **The voice map's target** is the swarm's own `vf[i]` (natural frequency: law, sub-parameters and drift), not couple()'s cents law.
   - **Label.** SCALPEL's `driftRate` takes ACCOUNTING F2's label, "Spread drift", because horde's Drift rate is now on the same page. Its id and key are unchanged.
   - **Serving.** A lab that cannot fetch (opened from the filesystem, or under a load checker) plays and draws nothing and says why. It does not fall back to the oracle alone.
3. **Interplay drawn as heard** (`memberCycleStepped`, `steppedCycle`).
   - **Where it applies.** It is used wherever the oracle takes interplay: `b2on` with `b2mix > 1e-6`, or `colK`/`colB` ≠ 0.
   - **How each member is drawn.** The oracle's own per-sample step, `RazorCore.prototype.stepM` (gate, collide, then out → outSerial), runs on a scratch member.
     - It steps once per drawn point, with BLEP off, so `m.prev` is `out()` at the point.
     - It runs two cycles and keeps the second, so a window that wraps enters with its steady-state chirp.
     - It uses the base class's stepM, not the composed engine's override, which would tick the swarm.
   - **CONTRIBUTIONS** is what each blade adds as heard. A second run with the upper blade bypassed gives the lower's contribution; the upper's is the rest.
   - **The label.** B300's notice is replaced by a factual label: "INTERPLAY · ② over ① · upper hears λ 0.50 · collision → pitch +0.90 (drawn as heard)".
   - **XY roles.** Upper hears, Collision → pitch and Collision → bite (`XY_PLAY`: pad-assignable in `parameters.json`, bladeless) are roles on both XY surfaces, the waveform and the ring. MAIN's free XY pad is unchanged: what it holds is open question 38.
4. **Citations.** The lab's `razor-core.js` citations are v1.1's, and each was verified against both files:
   - `:280, :289, :294` → `:332, :341, :346`;
   - `:360` → `:412`;
   - `:271, :360` → `:323, :412`;
   - `:704, :737` → `:788, :821`.

   The B300 trace named one number per line. The companions on the same lines were stale too, so I refreshed them as well.
5. **The page text:**
   - the header comment (a ROUND 7 paragraph);
   - the badges, tagline, audit line, side panel and audio status;
   - open questions 43–53;
   - Q10 and Q17 are marked as superseded.

## Evidence

- **VERIFIED, in-page self-check 55/55, 19 of them controls that must fail** (44/44 with 14 on main). It was read from headless Chrome over DevTools in light and dark, on OSC, on `?page=main`, in skin C and with `direct=1`. The 11 new rows:
  - **Engine.** CORE extends the oracle with horde's swarm on. The worklet's OWN source (`workletSource()`, evaluated with stand-in worklet globals) renders **bit-identically** (max |Δ| 0 over 5120 samples) to a page-side instance of the monitor's class.
    - **CONTROL:** the oracle alone differs by 0.7172.
  - **Voiced.** Over 0.25 s at N 5, 20 c, K 0.3:
    - Onset lock changes the render by 0.576, Inertia by 0.488, and the harmonic detune law by 0.916.
    - The voice map's top target moves from 20.0 ¢ to 1017.6 ¢ (1200·log2 1.8), and the row asserts both values.
    - **CONTROL:** the oracle alone reads max |Δ| 0 for all three.
  - **Interplay, exact per sample**, on the 7 Interplay presets: max |heard − drawn| is 3.3e-8 to 6.6e-8, which is float32's limit, as in the first drawn = heard check.
    - **The voicing.** One member, 1×, raw, DC off, f = sr/384, rotation off.
    - **Grid offsets.** Positions move +½ step, so no window edge sits on a sample. The base saw's jump sample is skipped, 4 samples in all. Both are ambiguous to a rounding, and the check says why.
    - **Parked presets.** Collision chirps and Collision bite only meet under rotation, so blade 2 is parked on blade 1's Position.
    - **CONTROL:** round 6's parallel drawing misses by 2.49 / 0.69 / 0.09 / 1.23 / 1.79 / 2.07 / 1.10.
  - **Interplay, as played** (section K2's measurement, AT PHASE against the period ending on each of 8 viz posts): the error falls on all 7.

    | Preset | Now | Was |
    |---|---|---|
    | Fold over sync | 0.09 | 0.55 |
    | Sync over fold | 0.09 | 0.25 |
    | Occlusion sweep | 0.15 | 0.51 |
    | Ring on ring | 0.15 | 0.32 |
    | Crushed burst | 0.16 | 0.71 |
    | Collision chirps | 0.19 | 0.25 |
    | Collision bite | 0.12 | 0.37 |

  - **XY roles.** On the waveform, Upper hears lands at 0.3000, → pitch at 0.1000 and → bite at 0.2000, each exactly its taper step. A ring click lands → pitch at −0.500 and Upper hears at 0.750. The engine's `t` agrees.
    - **CONTROL:** the default mappings move none of them.
  - **Pin.** With both tiers open and the scroller at its end, the cycle view is in view and so is Advanced:
    - skin A pins 215 of 595 px, leaving 380 beneath;
    - skin C pins 317, leaving 278;
    - 122/122 controls can each be brought fully below the pin.
    - **CONTROL:** the same scroll unpinned takes the cycle view out of view in both skins.
- **Unchanged rows still green on the new engine.** The numbers moved with the law, which shows the swap is real:
  - ring clump r 0.965, splay 0.051;
  - carpet r 0.975 / 0.048;
  - beam hue R 0.969 / 0.048;
  - AT PHASE 0.103 against SUM 1.072;
  - rotation drawn = heard 0.0188 ≤ 0.0212;
  - presets 83/83 clean and playing through the engine;
  - XY ranges: 53 keys match `parameters.json`.
- **Planted faults** (served through a DevTools response rewrite, nothing committed): 6 of 6 CAUGHT.
  - Interplay drawn as parallel: 2 rows red.
  - CORE = the oracle alone: 4 red.
  - Pin not sticky: 1.
  - Voice-map target back to couple()'s law: 1.
  - Collision's integrated carrier dropped in the drawing: 1.
  - Interplay roles not bladeless: 1.
- **Real mouse.** With the scroller at 1435 px (Advanced in view), a 42 px drag on the PINNED cycle view in empty space moved Position 0.875 → 0.9395 (42/651 of a cycle). Both blades moved, the engine's `t` agrees, and neither the scroll nor the pin moved.
- **The drawing's cost.** On Collision chirps (N 3), headless Chrome, measured while `verify full` was compiling beside it, so the numbers are noisy: `cycleFrom` takes 2.19 ms per viz post with the stepped interplay path, against 0.62 ms for the parallel sum. There is one post per 1600 samples, and the result is cached per post.
- **Real AudioWorklet** (`?probe=1`): "offline worklet render, 0.5 s of A3: RMS -13.4 dBFS, peak 0.713, non-finite 0 — AUDIBLE PATH OK".
- **`lab_load_check`:** GREEN — 54 labs loaded, 0 broken, 1 skipped.
- **`lab_wheel_scroll_check`:** GREEN — 0 failure(s); 6 labs, 2 planted faults. For SCALPEL: 20 wheel elements, 0 plain-wheel changes, alt moved 19/20, and 103 clicks inside the scroller, 0 moved. Under the checker's fake DOM the engine does not load (no fetch). The SCALPEL defaults still come from the oracle, so the cycle view's wheel site is not blind.
- **Not verified:**
  - Nobody has listened.
  - The as-played interplay numbers are K2's normalised measure, not a proof.
  - The exact row covers one member on the drawing grid; the swarm and rotation are covered only by the as-played row.

## Screenshots (scratch, not committed; `scratchpad/b309/shots/`)

- 01 / 02: OSC at the top, and scrolled to Advanced with the pin, light and dark.
- 03 / 04 / 05: Collision chirps at the same frozen frame (`still=58`), AT PHASE and SUM, the pin and the whole frame, light and dark.
- 06: the same frame drawn with round 6's parallel sum (a planted copy, for comparison), light and dark.
- 07 / 08: the swarm controls live (N 7, 30 c, harmonic law, onset −0.5, inertia 60 %, drift 12 c) in One level down (with the onset caution) and in Advanced (with the inert sub-parameters), light and dark.
- 09: skin C, scrolled to Advanced, pinned.
- 10: MAIN on Collision chirps.
- 11: the audit line.

## Alternatives rejected

- **A synchronous XHR for `swarmsaw.html`,** to keep `const CORE`. It is deprecated, and it still fails from the filesystem.
- **Falling back to the oracle alone when the engine cannot load.** That puts a second engine behind the same controls: the lab would draw and play something other than what it claims.
- **Drawing collision by re-deriving it in lab code** (gate, collide, entry reset). The brief asked for the oracle's own path. `stepM` is that path, and the re-derivation is what B300's notice warned about.
- **A per-point `out()` without stepping.** Collision is integrated state, so no per-point formula can draw it.
- **The interplay roles in `XY_SWARM`.** The optgroup would then call them "the swarm". They get their own group, and they are bladeless, like the swarm roles.
- **Building a compact pin for skin C now.** Its cycle view is the editor. Proposed as an option instead (open question 44).
- **Putting `onset` and `dissolve` in RESTRIKE,** so the monitor shows the burst on a knob turn. The audio is not re-struck on a patch edit, so the drawing would show a burst nobody hears (L0064).

## Verify

- `./verify fast`: exit 0 at git 9d37b60 (`.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"9d37b60","ts":"2026-09-27T22:41:49Z"}`).
- `./verify full`: exit 0 at git 9d37b60 (`{"target":"full","exit":0,"git":"9d37b60","ts":"2026-09-27T22:47:53Z"}`). Its log includes `composed_engine_check.mjs: GREEN — composed_engine_check: 60 rows, 0 failed`.
- The log notes that the private-name leak check was SKIPPED (`.leakcheck-names` is absent in this worktree).
- This trace is committed on top, and `verify fast` is re-run on that hash before the push.

## Open questions

1. **Inertia's tier.** T3 as placed, or T2 beside Onset and Dissolve, since the human asked for it by name?
2. **Onset below 0.** Amend the protected JS reference (a human gate), or override the engine's control tick (about 170 copied lines, B298)?
3. **A compact pin for skin C.** This means the cycle view at 132 px while the pin is stuck, which gives 346 px beneath. It is proposed, not built.
4. **Re-voicing the 83 presets' K for horde's law** (Q B3). They lock less in the lab now.
5. **MAIN's free XY pad** (question 38) is still the human's call. The interplay roles could live there.
6. **B310's voice law** lands through the engine file. Once both merge, the lab's same-note repeat keeps its release, with no lab change.
7. **The lab needs serving** (it fetches `swarmsaw.html`). Is that acceptable? The presets already needed it.
