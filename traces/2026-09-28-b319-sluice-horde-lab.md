# b319-sluice-horde-lab — a design lab for Sluice as it will live inside horde

- **Queue item:** B319 (read verbatim from `origin/lead-records-123:ROADMAP.md`, records PR #811, with B127, B166, B273 and B302 beside it). The human: "Could you also please start agents building the design labs for Maw and Sluice as they'll live in Horde?"
- **Why:** Horde consumes Sluice as one node in its FX rack (ADR-166, A1–A6). Before anything is ported, the human needs to see and hear what that node is inside horde. That means its network, its macros, tempo sync on horde's division names, the fz.h randomiser, and the corners as its slots, all in the new labs' look (B302).

## What changed (commit `d1b1ca2`; `lab-review` meta `B319 · 2026-09-28`)

The only new file is `docs/design/sluice-horde-lab.html`. It is a 980×720 plugin window showing horde's FX page with SLUICE selected in the rack.

- **The engine is our NETWORK reference, loaded and called.** The page fetches `reference/network-lab-v0.html` and slices out `ENGINE_FACTORY`, `PRESETLIB`, `toSlider`/`fromSlider` and `sigOf`. It evaluates them unmodified. The reference file itself is not edited. There are two runtime seams:
  1. **A seeded `Math`.** The S&H at :458 still calls `Math.random`, so the slice is handed a `Math` whose `random` is a seeded stream.
  2. **Two read-only taps.** These add a meter after each module's tick and one on the damped loop bus.
- **One renderer.** `makeRenderer` produces everything heard and everything drawn live (L0064). It runs on the page, or in an AudioWorklet built from its own `toString()`. Its first messages arrive through `processorOptions`.
- **Input.** The default is a horde voice: the composed engine (`scalpel-horde-engine.js`, loaded) playing a riff on the host tempo. Two other inputs are on a switch: an impulse per bar and a seeded noise burst.
- **The face:**
  - four macros, bound by order and shown with Sluice's labels;
  - RANDOMISE: fzRandomize / fzRandomPatch / thresholds / tri-state locks, ported from `fz.h` and `random.h` onto our eight module types. It has one seed, a click counter and undo, with one JSON of the node per undo step (Sluice seq 6);
  - TEMPO: sync by name over horde's 22 divisions, with the 120 bpm fallback and a no-op when the tempo is unchanged;
  - EVERY PARAMETER: the full parameter list one level down, with threshold dots and lock pills.
- **The views.**
  - The network: nodes lit by the tap levels, the loop bus, and the LFO edge.
  - The loop state: DECAY, CRITICAL, FREEZE or OPEN, read from a rendered impulse response.
  - The output: a scope and spectrum, wet over dry.
  - Four corners, each with a rendered IR sparkline, and an XY cursor.
- **Mocked, never heard:** the post limiter, the release gate and TAIL, and the rest of the rack. The loudness is measured but not applied, because our reference has no wet gain stage.
- **Self-checks.** Nine run at load, each with a must-fail control. All nine pass in headless Chrome, read in real time through DevTools on the committed file.

  | Check | What it proves | Result | Must-fail control |
  |---|---|---|---|
  | K1 | 17 patches render finite, ≤ 0.85 and not silent | Pass | Planted NaN input and zero input are both caught |
  | K2 | The bypassed slot is bit-identical to its input | 0 of 51 200 samples differ | Not bypassed at mix 0: 12 800 of 12 800 differ (the reference's output stage still runs) |
  | K3 | 22 names + 7 aliases agree with each name's own arithmetic; the renderer's own resolution is exact for 132 division×bpm pairs; fallback and no-op hold | Rendered 1/4 @ 120 echo lands at 11 025 samples | The same render at 90 bpm gives 14 700, which is rejected |
  | K4 | Same seed gives the same patch; locks hold; unlocked sets are nested | Pass | seed+1 differs; unlocked modules change; a draw-only-when-applied twin is caught |
  | K5 | The worklet's source, evaluated on the page, matches the page renderer (samples and all 8 meter messages) | Identical | A renderer one block ahead: 64 of 64 blocks differ |
  | K6 | The tapped engine matches the untapped reference on 5 presets | Bit-identical | A planted perturbing tap is caught 5 of 5 |
  | K7 | Sluice's G-70: each macro at its stored value reproduces its preset | 8 of 8 | +0.1 changes every macro |
  | K8 | The morph is exact at each corner and inside the corners at the centre | Pass | A split chain is flagged and the nearest corner is heard |
  | K9 | The real AudioWorklet, rendered in an OfflineAudioContext, matches the page renderer | 0 of 12 288 samples differ | One block ahead: all differ |

- **Measured drift.** A 1 kHz sine through our shifter at +80 Hz reads 57.1 dB at 920 Hz and 46.4 dB at 1080 Hz. Our reference shifts **down**, which confirms Sluice's F-008 finding on the v0.1 code we hold.

## Evidence consulted

- **Sluice notices:** `integrations/sluice/*` (all eight, seq 1–10).
- **Decisions:** DECISIONS ADR-166 with A1–A6, ADR-169 with A1–A2, and ADR-172.
- **Reference and spec:** `reference/network-lab-v0.html` and `specs/SPEC-FX-NETWORK.md`.
- **Sluice's own tree, read only:**
  - `lab/sluice-lab.html` (engine, presets, macros);
  - `netcore/include/sluice/{fz,random,defs,macro,patch}.h`.
- **Labs this one borrows from:**
  - `scalpel-interface-lab.html` (tokens, chrome, knob, the seeded-Math / one-renderer routes);
  - `scalpel-horde-engine.js` and `composed-engine-check.html` (the voice API);
  - `fx-design-lab.html` (preset names, macro data, the rack roster, the derived macro position);
  - `shape-lab-mod.html` (DIVS);
  - `fx-page-lab.html` and `fx-chain-morph-lab.html` (read; nothing taken).
- **Tooling:** `tools/labharness/lab_load_check.mjs` and `lab_wheel_scroll_check.mjs`.
- **Lessons:** LIBRARY L0026, L0032 and L0064.

## Alternatives rejected

- **Using Sluice's lab as the engine.** Rejected because the brief says our reference is the oracle horde holds. Drift is listed instead.
- **Copying `ENGINE_FACTORY` into the lab.** Rejected: a copy drifts. Slicing and K6 keep it honest.
- **Drawing from a parallel monitor instance.** Rejected under L0064. It was replaced by one renderer plus K5 and K9.
- **Wheel control on the knobs.** Not added: no wheel listener means `lab_wheel_scroll_check`'s inventory needs no change.

## Verify

- `./verify fast` passed on `d1b1ca2` (exit 0). `.harness/last-verify.json` reads `{"target":"fast","exit":0,"git":"d1b1ca2","ts":"2026-09-28T01:54:49Z"}`.
- `lab_load_check` run alone passes: 55 labs, the new one included.
- The private-name leak check is SKIPPED in this worktree because `.leakcheck-names` is untracked and absent here. The same case-sensitive pattern from the main checkout's list was run by hand against the lab and got 0 hits. There are also 0 `/Users/` paths.

## Open questions

These are also in the lab's side panel.

**For the human:**
1. **New patch vs the corners.** Should a new patch spread to all corners, or be disabled while the corners differ?
2. **Macro tier.** Should macros be corner-tier or global? ADR-169 A1 is still owed.
3. **A macro after a randomise.** The knob is derived from its first binding, so the other N−1 bindings can jump.
4. **Bypass.** Should bypass cut the tail (as here) or let the loop ring out?
5. **fzEnginePatch's global knobs.** How do they map? B273 still has this open.
6. **Divisions.** The division convention follows B213 D3. A notice to Sluice is owed once D3 is ruled.

**For the lead:** a brief to Sluice may be worth filing. Our reference is v0.1: mono, pre-F-008, unseeded (:458, :743, :987, :994) and eight types. Ask which artefact horde should hold as the oracle (their lab or netcore's parity fixtures) and whether to re-ingest.

**Limits of this lab:**
- The loop-state thresholds (FREEZE / CRITICAL) are the lab's own. The reference shimmer fog reads CRITICAL (T60 about 8.5 s) under a single 0.5 impulse, not FREEZE.
- K9 needs real time: a virtual-time headless capture shows it as pending.
