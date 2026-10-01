# horde 2's composed engine, `h2/engine/`: design and parity plan

ROADMAP B385 (phase 1b re-scoped; B379, B332). Rulings: ADR-186 item 4 (copy
forward), ADR-187 with A1 (parity strength, the divergence ledger, CPU as
Layer-E), ADR-184 A2, ADR-189 with A1. Written 2026-10-01 as checkpoint 1 of the
B385 PR series. This document is the plan. The checkpoints after it either
follow it or amend it here, in place, with the reason.

## Why one engine

The human, 2026-10-01: "Why would there need to be two scalpels drifting apart?
Wouldn't we simply combine the working parts of each engine into a fresh golden,
run a suite of tests against it to ensure it passes the quality standard ... and
turn that into the C++ golden the device is built against?" Then: "Yes,
re-scope phase 1b that way … Confirming clean-port path."

So horde 2 gets ONE C++ engine, the composed engine, written fresh against the
composed JS. The two proven C++ ports stay where they are, untouched, as test
references:

- `h2/cores/scalpel/razor_core.h`, the blade oracle's port, proven by phase 1a;
- `h2/cores/swarm/`, the lifted legacy swarm core, proven by B379.

Proven code is copied out of them and cleaned. Nothing includes them. There is no
subclassing, so B379's blocker (`razor_core.h` has no extension points,
`docs/port/phase-1b.md`) does not arise: the engine is one class that owns the
whole render.

## The golden (the parity target, ADR-187 item 3)

The composed engine as the lab runs it, at main `c64cfdb` (2026-10-01). The
renderer streams each file's git blob, and the check fails on a blob it was not
pinned to.

| file | role | blob |
|---|---|---|
| `docs/design/scalpel-horde-engine.js` | the composed engine (`makeComposedEngine`) | `581d7942684c91245e4a6637dd40d137335b5d69` |
| `reference/scalpel/prototype/razor-core.js` | the blade oracle it extends | `0ce6a713410d89c65bf55f761f1dc791fae61b16` |
| `reference/swarmsaw.html` | SwarmSynth, the swarm it drives (DSP section) | `e47da6c9e0b4a058e18d79f62d71ab31c3d3b1b0` |
| `reference/scalpel/data/presets.json` | the 83 bench presets | `44b48d72a9bed4717edd0ac5cf9ef4b8d7b0de93` |
| `docs/design/scalpel-interface-lab.html` | the 12 B366 envelope presets (`ENV_PRESETS`) | `6abf848e91065bc33275724a6967a7399ba40a07` |

The engine's flags at their ledgered defaults (`docs/port/divergences.json`):

- **M1 `ksmPerRate`, M2 `onsetBipolar`, M3 `tempoGrid`: ON** (B382, ruled).
- **D1 `aaCarrier`, D2 `aaXin`, D3 `aaLoop`: OFF** (ADR-189: the instrument's default
  is ruled when horde 2's shell is built). Both positions of each are built and
  parity-checked.

What the golden contains, each with its site in the composed JS:

- **The swarm:** SwarmSynth's `noteOn` and `controlTick` (coupling law, onset and
  dissolve, drift, inertia with the shell's taper, every detune law, freqGlide,
  keepPhase, pivot), patched by M1–M3 (`SWARM_PATCHES`, :309–356).
- **The blades:** RazorCore inherited unchanged, apart from the overrides below.
- **φ_S = frac(φ_H + ½)** (`startVoice`, :699) and the two phase integrators
  (`stepM`, :960).
- **B310's voice law** (`noteOn`, :604; `tierPick`, :642).
- **B323's cap and cull, with B375's capPolicy** (`noteOn` :611–630; `cull` :663;
  `renderPlain` :1152).
- **B325's first tick** (`lookAhead` :860; `couple` :878; `stepM` :967).
- **ADR-184 A2:** bipolar Rotate spread (`set` :519, `spread` :530), and the exact
  Cut-spread mirror under Quantize (`roundAway`, :537).
- **B335:** gravity (`gravityStep` :921, the fixed-time grid in `render` :1106),
  and onset scatter, timing correction, attack and release scatter and the
  per-partial envelopes (`armMembers` :764, `memberStep` :817, the member gain in
  `stepM` :990).
- **ADR-189 D1–D3** (`voiceAA` :406, `d1Takes` :399, `scan` :1047, `bladeStep`
  :1003, `loopIn` :1024).

## Structure

Header-only, namespace `horde2::engine`, four files under `h2/engine/`:

| file | contents |
|---|---|
| `js.h` | JS number semantics (`js::round` = `floor(x + 0.5)`, NaN-propagating `min`/`max`, `truthy`, `sel`, `pow`, `toInt32`) and `Mulberry32`, the repo's one RNG. |
| `swarm.h` | `Swarm`, one per voice: SwarmSynth's per-swarm state. `SwarmField`, the one SwarmSynth the engine owns: the member placement `x[]`, `centerIdx`, `xmin` (its `rebuild`, placement only), the note counter, the keep-phase snapshot, `noteOn` and `controlTick`, with M1–M3. |
| `blade.h` | the types: `Blade`, `SP`, `DP`, `NS`, `BX`, `Member`, `Voice`, `Biquad`; and the PURE blade functions: `hash`, `F`, `crushAvg`, `isFM`, `panSlot`, `gate`, `collide`, `d1Takes`. |
| `engine.h` | `Engine`: the stateful blade evaluation (`wave` and `mod`, which read the per-member sine->saw band-limit; `crushLevel`, `fmStep`, `voiceOut` with D1 inline, `out`, `outSerial`, `fillG2`), the spreads with A2, the voice law, cap and cull, the first tick, gravity, the ensemble stream and member envelopes, the BLEP scanner with D2, the loop filter D3, the DC estimators and the render. |

**Data, all preallocated in the object** (rule 5 of `h2/README.md`):

- 8 `Voice`s, each with 9 `Member`s (blade state: phase, modulator phase, the four
  blade states, spreads, DC estimate, loop taps, entry wait and envelope) and its
  own `Swarm`.
- Each `Swarm` keeps its arrays at 9 entries, the member limit. SwarmSynth sizes
  them at its own `MAXV` of 32, but it only ever reads the first `N`. One thing
  of the 32 is kept: `noteOn` draws a start phase for all 32 slots when retrig is
  off, so the C++ draws 32 times and stores 9, or the per-swarm stream would
  shift.
- The B325 look-ahead snapshot is a second `Swarm` per voice (a struct copy
  without `phase`; 800 bytes, against the 39 KB `Voice` snapshot the lifted core
  would have needed).
- Gravity's sort buffer is a fixed array of 8. The fade path's one-sample
  buffers are members.

**The render, in order** (one call of `render(L, R, n)`):

1. `cull()` if a cap is set.
2. Gravity off: one block. Gravity on: segments that end on the fixed-time grid,
   with `gravityStep` between them.
3. Each block: D1's state reset when D1 has just turned on. Then one inner call,
   or, while a culled voice fades, one inner call per sample, rounded to float32
   as the JS's `Float32Array(1)` does, with the ramp applied after each sample.
4. The inner call is RazorCore's `render`, with the composed glue in place:
   - the per-call setup (pan law, envelope rates, the B335 snapshot `preCall`);
   - per sample: the smoothers; `couple` and `spread` every 32 samples; the
     voice envelopes and rotation;
   - per oversampled step, per voice, per member:
     - the loop input (D3);
     - the member's parameter overrides;
     - at member 0's first step of a sample: the swarm tick on the 16-sample grid
       (undoing the look-ahead first), then the member entries and envelopes;
     - the swarm phase advance;
     - the member gain;
     - the blade step (BLEP corrections, D2's scanner);
     - the loop filter (D3), the DC estimate and the pan sum;
   - the output biquads, the DC blocker and the tanh.
5. The keep-phase snapshot of the newest sounding swarm.

**The two classes the JS needs, here as one.** The JS composes by subclassing and
by swapping a static (`RazorCore.voice = voiceAA`, :1148). The C++ engine is one
class, so each override becomes the code at its call site:

- D1 is a branch in `voiceOut` on a per-render flag;
- the swarm drive and the member gain are in the member loop;
- `couple` computes only what the composed engine reads (see "Dropped").

## What is copied from where

| from | what | how |
|---|---|---|
| `h2/cores/scalpel/razor_core.h` (phase 1a, parity-proven against razor-core.js) | `js::` helpers, `Mulberry32`, `EventLog`; the parameter structs and key tables; `wave`, `mod`, `hash`, `F`, crush, `fmStep`, `voiceOut`, `gate`, `collide`, `out`, `outSerial`, `fillG2`; the custom-ratio parser, `ruleList`, `kSpread`, `spreadMember`; `startVoice`, `monoOn`, `noteOff`, `retune`, `panic`; the biquads; `hAt`, `dcEst`, `dcPair`, `scan`, `addE`, `tryE`, `stepM`; the render loop | copied, then cleaned: phase-1a history and quirk narration out, the WHY kept; the fault hooks rebuilt for this engine's controls |
| `h2/cores/swarm/swarm_core.h` (lifted, parity-proven against SwarmSynth) | the M1 coefficient (`kKsmTauSeconds`, :152; `ksmC`, :389); law 3 (:1803–1812); `ensembleSeed` (:1389); `gaussT` (:2126); `kRatios` (:304); the bipolar onset routing (:1894) | copied as expressions. The rest of the lifted core is NOT copied: the composed engine's swarm is SwarmSynth's law, and the lifted core's extras (absK, topologies, Daido poles, glide travel, the per-voice ADSR, ITD) are not in the golden |
| `reference/swarmsaw.html` SwarmSynth (via the composed JS) | `noteOn`, `controlTick`, `rebuild` (placement), `rngS`, `rngG` | transcribed fresh, expression for expression |
| `docs/design/scalpel-horde-engine.js` | every composed override (the list above) | transcribed fresh |

The lifted swarm core cannot be called as it stands: its `controlTick` is private
and differs from SwarmSynth outside the golden's scope. And the legacy parity
chain is within 1e-6, not bit-exact (113 of 156, B378 audit §2.9). The composed
engine needs the swarm's member frequencies far tighter than that: phase 1a's
detection floor shows a sync blade turning a 1e-10 relative pitch error into
6.5e-6 max-abs (`docs/port/scalpel-phase-1a.md`, "Detection floor"). So the swarm
is SwarmSynth's own expression order, and the C++ core is used for its constants
and as the check on M1–M3, which originated there.

## Dropped (dead in the golden, or test fixtures of the JS)

Each item is listed so a reviewer can check that nothing audible left with it.

- **The `razor` swarm source and `origin`** (:207–211, :464). These are fixtures
  for the JS check's O2 and controls. The product is `horde`, with origin ½.
- **The viz and readouts:** `post`, `hordeViz`, `gravRatio`/`gravOct`/`gravErr`,
  `onsD0`, `v.r` (the `hypot` the oracle posts and never renders), the `vc` viz
  countdown, and `toString`. The counters `culled`, `refused` and `stolen` stay:
  they are host-facing load readouts, and the parity check compares them with the
  golden's after every scenario (the `CNT` record).
- **SwarmSynth's output path:** `renderSeg`, pan, tone tilt, hi-tame, roundness,
  the R→tone filter, and the pan layout in `rebuild`. The composed engine never
  calls `renderSeg`, and their state is never read: the output stage is
  RazorCore's.
- **The oracle's coupling arithmetic in `couple()`:** the keff law, the
  higher-harmonic sums and the member increments it computes and the composed
  engine overwrites (:892). Kept: the h = 1 mean field, which gives each member's
  `lead` (frames and spread law 3).
- **`settle()`** (a no-op in the composed engine, :874) and **RazorCore's poly
  `noteOn`**, its same-note reuse superseded by B310. RazorCore's `startVoice`
  draws (start phase, modulator phase, the random-law slots) are KEPT, though the
  start phase is overwritten: they are consumed from the host-seeded stream, and
  dropping them would shift every later draw.
- **M1–M3's OFF positions (a proposal, open question 1).** In the JS, flag 0
  restores SwarmSynth's expression, for lab A/B. The human ruled the lab should
  match the C++, so the C++ carries the ON law only. Setting `ksmPerRate`,
  `onsetBipolar` or `tempoGrid` to 0 is ignored, and no parity scenario does it.
  D1–D3 keep both positions, because their instrument default is still unruled.

## Rules the code follows

- **Parity build:** doubles, `-O2 -ffp-contract=off` (h2 rule 7).
  `tools/h2_rules_check.py` is widened, additively, to treat `h2/engine/` as an
  h2 core for the contraction rule and the no-TU-sees-both rule.
- **JS semantics:** `Math.round` → `floor(x + 0.5)`. The one exception is A2's
  `roundAway` and M3's snap, which ARE half away from zero. `min`/`max` propagate
  NaN; truthiness; strict-equality `switch`; `slice` semantics for the poly pool;
  ToInt32 for the seeds.
- **Randomness: four mulberry32 streams, each in the JS's draw order:**
  - the host-seeded stream (the JS's `Math.random`: RazorCore's draws);
  - `grng` (member placement, dist 2 and 3);
  - one per swarm (start phases, drift);
  - the ensemble stream (B335).
- **Real time:** `render` allocates nothing, locks nothing and reads no clock. The
  cap is an input. `set`, `setString`, the note calls and the cap calls run on the
  render thread BETWEEN `render()` calls (a CLAP host delivers parameter events
  inside `process()`), never concurrently with one, and none allocates.
- **Domain clamps** (inert inside the domain, as in phase 1a): N within 1..9, a
  poly pool of 0 ignored, notes within 0..127.
- **Comments say WHY.** No history, no "was", no measured-then narratives; those
  live in the ADRs and traces this document cites.

## The parity harness (as built; reworked after the critic's review at e54b70b)

**`tools/h2_engine_render.mjs`.** Phase 1a's renderer stays `razor_core.h`'s gate.
The scenario language and the 125 blade rows are in `tools/h2_scenarios.mjs`, which
both renderers share. Phase 1a's stream is byte-identical before and after the move
(sha256 `d3d7897e…`, both runs).

- It loads the golden from the five pinned files and seeds `Math.random` with
  mulberry32 around every instance (the composed engine's seeded wrapper, ADR-187
  item 3). Per scenario it streams:
  - the script;
  - the PRISTINE engine's float64 samples;
  - the blade-event digest;
  - the golden's load readouts after the script (`CNT`: tails culled, notes
    refused, voices stolen).

  The stream header also carries the golden's Node version (`NODE`).
- **Events** come from an INSTRUMENTED scratch copy built in memory, with phase 1a's
  seven insertions into razor-core.js's text plus one in the composed engine's own
  `scan` (D2's BLEP site). Its samples equal the pristine engine's bit for bit on
  every scenario (NONINV).
- **Commands:** phase 1a's, plus `cap <n>` and `capPolicy <n>` (the engine's
  `msg`).
- **`--nudge K`** moves every note-on and retune frequency IN THE SCRIPT K doubles
  up, so both sides render the nudged inputs (L0071's probe).

**`tools/h2_engine_parity_check.cpp`** replays each script through
`horde2::engine::Engine`. Per scenario it requires:

- RMS < 1e-6 and max-abs < 1e-6 (ruled 2026-09-30, B332);
- identical event counts and times;
- identical load readouts;
- no key the engine lacks, unless the check's whitelist names it (it is empty).

The five ORACLE blobs must each appear in `h2/README.md` as `<path>@<blob>`.

**The bit-exact floor is on the MEAN share, keyed per platform, compiler and Node
major** (a proposal; open question 3). The L0071 probe rules out a per-scenario
floor:

- One scenario's share of bit-identical samples moves by up to **57 points** when
  every note-on frequency moves one ULP. Zap bass :: chord reads 5.79%, 62.36% and
  3.55% at nudges 0, 1 and 2. The share is the time before the first libm
  disagreement (L0066) reaches the output, so it is a property of the inputs.
- **The mean over the 540 scenarios held to parity is stable:** 37.00%, 38.34% and
  37.61% at nudges 0, 1 and 2.

  | nudge | P/ | E/ | T/ | C/ |
  |---|---|---|---|---|
  | 0 | 32.87% | 46.33% | 43.39% | 36.28% |
  | 1 | 33.66% | 46.78% | 43.42% | 40.47% |
  | 2 | 33.53% | 47.01% | 43.32% | 37.46% |

- The proposed floor is a mean of at least **30%**, pinned for darwin-arm64, Apple
  clang 16 and Node 24. That is 7.0 points under the lowest. Any other key reports
  the mean and SKIPs the floor, never passes it.
- **Inside the gate:** the FMA-fused build (`h2_engine_fma_control`) fires only if
  parity misses AND, where the floor is pinned, its mean falls under the floor. It
  reads **17.48%** (per family P/ 14.65, E/ 20.41, T/ 22.84, C/ 17.04) and fires,
  with 24 of 543 scenarios missing parity.
- Five scenarios are at 0.0% bit-exact while at parity (≤ 1e-10). These are the
  hash-noise FM patches.

**Chaotic exclusions.** The three Cross-mod ring (watch) rows are pinned BY NAME.
Each must hold identical events and readouts, and a strict **1e-6 max-abs bound
over its first 128 frames**, before the ring amplifies (measured: 5e-11 to 8e-11).
Then one of two rules applies:

- **1a, the default:** phase 1a's rule as briefed. RMS < 1e-6, and the golden's
  own one-ULP divergence must reach at least the C++'s max-abs.
- **b, selectable with `--chaotic-rule b [--chaotic-k K]`:** RMS is exempt too.
  The golden's own one-ULP divergence must be at least K × the C++'s, in RMS and
  in max. This weakens 1a, so it is the human's to rule; it is built, NOT the
  default, and the check stays unwired.

The run prints each listed row's ratios. **Control X1** scales the ring's xm by
1 + 1e-9 inside the engine and must make every listed row "NOT justified" under
the selected rule. It does: under 1a, the first-128 bound alone sees 2.6e-3 to
4.7e-3, and the ratios fall to 0.18–0.42×.

**Must-fail controls, planted under `H2_ENGINE_FAULTS`** (each fires; values at
the rework head):

| control | what it plants | row it must turn red | when planted |
|---|---|---|---|
| A2a | the oracle's `Math.round` on a Quantized Cut spread | C/A2 Quantized Cut spread -2.5 | rms 7.8e-2 |
| A2b | Rotate spread's sign dropped | C/A2 negative Rotate spread | rms 1.6e-1 |
| F2 | the start-phase and modulator draws swapped | T/fm mode 2 type 0 :: chord | rms 1.7e-1 |
| F3 | the blade-entry BLEP skipped | T/mode 0 :: chord, samples AND events | rms 1.1e-3 |
| F5 | every event one tick late | T/mode 0 :: chord, events ALONE | samples bit-identical |
| V1 | the oracle's same-note reuse | C/VL repeat, the first release rings | rms 2.9e-1 |
| T1 | the swarm tick one sample late | C/M1 K 1 at 48000 | rms 3.4e-5 |
| L1 | B325's look-ahead dropped | C/FT chord on a mono voice in one block | rms 4.9e-5 |
| K1 | M1 reverted (0.08 per tick at 48 kHz) | C/M1 K 1 at 48000 | rms 9.4e-4 |
| C1 | the cull a no-op | C/CAP cull, the cap lowered mid-phrase | rms 2.7e-1, readouts disagree |
| C1 | the cull a no-op | C/CAP cull per-partial voices, the cap lowered mid-phrase | rms 2.2e-1, readouts disagree |
| C2 | the cull takes the oldest voice, held or not | C/CAP cull, the cap lowered mid-phrase (the held note is the oldest) | rms 2.8e-1 |
| X1 | the ring's xm × (1 + 1e-9) | every listed chaotic row | 3 of 3 not justified |

F1 (`std::round` for `Math.round`) cannot be planted in this engine. A2's
half-away-from-zero pre-rounding makes `Math.round`'s negative half unreachable,
so A2a takes its place.

Also run every time:

- **Detection floor:** a relative error eps on the swarm's pitch each tick, printed
  and not judged. It is 1e-9.
- **Determinism, AMENDED (critic H2).** Three fixed scripts, rendered twice in this
  process and once in a CHILD process (the binary itself with `--det-digest`), at
  host blocks of 1, 37, 128 and 512: (1) the swarm with the ensemble and gravity;
  (2) the mid-phrase cull; (3) D1–D3 with feedback at os 4.
  - Every block size is bit-identical across the three runs.
  - Scripts 1 and 3 are bit-identical across block sizes.
  - Script 2 differs across block sizes by float32 rounding alone (max |Δ|
    1.47e-8 against a 2^-23 bound). This is the golden's own law: while a culled
    voice fades, `renderPlain` renders one sample per call through a
    `Float32Array(1)` to the end of the host's block, so how many samples pass
    through float32 depends on where the host's blocks end. It is a divergence
    CANDIDATE (the fade could render in doubles), not ledgered here.
- **No arguments (B384):** with no arguments the check spawns the renderer itself
  from the repo root.

**WIRING: NOT YET.** The check carries `UNWIRED:` with the reason. Under rule 1a,
the arp row is red at the scripts' own inputs. Rule b is a weakening of 1a, so
the human rules on the ring criterion; wiring follows in one line per binary.
`h2_rules_check` treats `h2/engine/` as h2 code. Its source detector is a plain
substring test for `h2/cores/` or `h2/engine/`, never narrower than the original.
Self-cases cover `"h2/cores/x.h"`, `<h2/engine/engine.h>`, `"../h2/engine/engine.h"`
and a tab-separated include.

## Scenario families (as built: 543 scenarios)

All at 48 kHz in 128-sample blocks unless a row says otherwise. The seeds are per
family.

1. **P/, the 83 bench presets** × chord, repeat and arp (+ legato for the 12 mono
   presets): 261.
2. **E/, the 12 B366 lab presets** × chord, repeat and arp (+ legato for the 2
   mono ones): 38. They are read from the lab's `ENV_PRESETS` text.
3. **T/, phase 1a's 125 blade rows**, re-run through the composed engine.
4. **C/, 119 composed rows:**
   - **VL 3:** repeat, tiers 2 and 3, tier 1.
   - **CAP 7:**
     - the cull with the cap lowered mid-phrase while three tails release and the
       OLDEST note is held;
     - the same with per-partial voices (the per-member fade);
     - refuse, steal, replace, steal with no free slot, and not binding.
   - **FT 4:** lock 2 in 37-sample blocks; a Hz modulator in 100-sample blocks; a
     chord on a mono voice in one block; a retune before the first sample.
   - **GRAV 8:** a sharp fifth, a triad, a twelfth, a pair outside the basin,
     switched on mid-note, 100-sample blocks, a bend while settled, and 44.1 kHz.
   - **ONS 12:**
     - alpha 0, 0.25 and 1;
     - attack and release scatter;
     - per-partial envelopes, chord and arp, and both together;
     - a seed change; a mono retrigger; 44.1 kHz;
     - onset scatter switched OFF mid-note, and per-partial envelopes switched OFF
       mid-note.
   - **D1–D3 37:**
     - each alone, and all on, over Zap bass, Frozen noise FM, Trance jitter,
       Feedback snarl and Cross-mod roar;
     - D1 with Band-limit off (sync and ring carriers), pitch FM, an S&H
       modulator, on and off mid-note, width-locked, mirrored, under collision, on
       a serial input, and on serial twins;
     - D2 on a blade-2 carrier under feedback, under phase FM, width-locked,
       mirrored, and under collision;
     - D3 on a cross-mod ring, and switched on mid-note.
   - **M 17:**
     - M1: K .35, 1 and -.6 at 44.1, 48 and 96 kHz;
     - M2: onset ±.5 at 44.1 and 48;
     - M3: law 3 at 120 bpm and at 140 ×2, at both rates.
   - **SW 24:** the swarm's own parameters.
   - **A2 6.**
   - **OS 1.**

## Results (darwin-arm64, Apple clang 16, Node 24.10, `-O2 -ffp-contract=off`)

| family | scenarios | at parity | worst rms | worst max-abs | mean bit-exact |
|---|---|---|---|---|---|
| P/ | 261 | 258, plus 2 excluded with evidence; **1 red** | 1.1e-12 | 9.9e-11 | 32.87% |
| E/ | 38 | 38 | 1.9e-14 | 4.5e-13 | 46.33% |
| T/ | 125 | 125 | 2.0e-12 | 1.4e-10 | 43.39% |
| C/ | 119 | 119 | 2.1e-12 | 1.8e-10 | 36.28% |

- Events and readouts are identical on all 543, the ring rows included.
- No scenario sets a key the engine lacks.

**Not at parity under rule 1a: P/Starting points / Cross-mod ring (watch) :: arp**
(rms 1.69e-6, max 1.6e-4; the golden against itself, 1 ULP apart: rms 2.1e-3, max
9.2e-2). This is ADR-065's case, and ADR-187 item 6 inherits ADR-065's evidence
rule.

**The ring ratio over 25 input nudges (0 to 24 ULP; critic M2).** Each cell is the
golden's own one-ULP divergence over the C++ error, min / median / max:

| row | RMS ratio | max ratio | C++ rms range | first-128 max | passes 1a (rms < 1e-6) | passes b at K = 10 |
|---|---|---|---|---|---|---|
| chord | 2 / 2430 / 35546 | 2 / 1423 / 17021 | 1.1e-7 … 1.1e-3 | 6.0e-11 | 14 of 25 | 23 of 25 |
| repeat | 2 / 2392 / 33614 | 2 / 1778 / 16768 | 2.0e-7 … 1.9e-3 | 8.1e-11 | 9 of 25 | 23 of 25 |
| arp | 3 / 999 / 17929 | 2 / 564 / 9498 | 1.2e-7 … 6.7e-4 | 4.9e-11 | 12 of 25 | 23 of 25 |

- At nudges 16 and 19, all three rows diverge macroscopically: C++ rms 4e-4 to
  1.9e-3, ratios 2–3, events still identical. The C++ and the golden take
  different branches of the attractor, as the golden does against itself.
- At nudge 15 the ratios are 19–126.
- No K above about 2 holds on every input. The strict first-128 bound holds on all
  75 (≤ 8.1e-11, against 1e-6), and it is what catches X1 by 4 orders of
  magnitude.
- So option (b) at the critic's K = 10 is still input-fragile on 2 of 25 inputs
  (8%), where rule 1a is on 40–64% of them.

## Deferred: the quality suites (a named checkpoint; the lead's decision, 2026-10-01)

Aliasing (B346's estimator on the engine) and zipper (parameter-motion sidebands,
the B378 F8 method) are NOT part of this PR series. They are a DEFERRED checkpoint
of their own, recorded by the lead as its own ROADMAP row. B385's row names them;
this series delivers parity, determinism and CPU only.

## CPU (checkpoint 4, Layer-E, never a gate)

Measured 2026-10-01 on this Mac (Apple M3; the machine was loaded, load average 4–7;
the calibration loop of 1e8 dependent multiply-adds took 175–205 ms, against 111–119
ms in phase 1a's runs, so read the RATIOS). Release build, `-O3 -ffp-contract=off`
(h2 rule 7). One voice where not stated; 48 kHz; each preset at its own os.

**Against the golden and against the blade port alone** (`build-release/measure_h2_engine`,
new, unwired, best of 5, three interleaved runs that agreed to ±0.01 points;
`build-release/measure_h2_scalpel`; the JS halves are `node tools/h2_engine_render.mjs
--bench` and `node tools/h2_scalpel_render.mjs --bench`, best of 3 after a warm-up):

| preset (one voice held 4 s) | golden JS | `h2/engine` C++ | C++ speed-up | blade JS | `razor_core.h` C++ | engine over the blade port |
|---|---|---|---|---|---|---|
| Crushed bells (N 6, two blades; heavy) | 18.01 % RT | 5.90 % | 3.1× | 11.13 % | 5.22 % | +13 % |
| Glass horde pad (heavy) | 25.81 % | 8.69 % | 3.0× | — | — | — |
| Quarter sync (N 1, one blade; light) | 3.39 % | 0.68 % | 5.0× | 2.12 % | 0.545 % | +25 % |

The C++ renders these at rms ≤ 3.8e-16 from the golden in the release build.

**Through `tools/auhost` (B381 stage 1, PR #886, not merged). MEASURED WITH AN
UNCOMMITTED auhost PATCH; reproducible after #886 merges and the `--engine` mode is
committed (the lead's decision: after #886, not now).** Its `--horde` mode times
`razor_core.h`. To point it at the engine, a scratch copy of `auhost.cpp` (from
`origin/auhost-stage1` at `23bc4fe`) adds an `--engine` mode: the same `cmdHorde` loop
with `horde2::engine::Engine` in place of `RazorCore` and double block buffers (the
engine renders doubles). Nothing else in the host changed, so the two modes share the
script reader, the block loop, the clock and the statistics. That edit belongs in #886's
file, so it is NOT in this PR; it is a ~10-line follow-up once #886 merges. Median % of
real time per block at 128-sample blocks, per voice (median / voices), 5 repeats, all
bit-identical across repeats:

| patch | voices | `razor_core.h` (`--horde`) | `h2/engine` (`--engine`) | engine / blade port |
|---|---|---|---|---|
| the oracle's defaults (N 5, sync blade, os 2) | 1 / 8 / 16 | 1.81 / 1.63 / 1.67 | 2.09 / 1.95 / 1.99 | 1.16–1.20× |
| Crushed bells | 1 / 8 / 16 | 5.05 / 4.93 / 4.99 | 5.72 / 5.68 / 5.74 | 1.13–1.15× |
| Glass horde pad | 1 / 8 / 16 | 7.62 / 7.55 / 7.62 | 8.61 / 8.80 / 8.88 | 1.13–1.17× |
| Quarter sync | 1 / 8 / 16 | 0.55 / 0.38 / 0.39 | 0.68 / 0.45 / 0.46 | 1.18–1.23× |

At 16 voices of Crushed bells the engine is 92 % of real time per block (median) on one
core of this loaded machine; Glass horde pad at 16 voices is 142 %.

**What the engine adds over the blade port:** 13–25 %, the swarm's control tick (per voice
every 16 samples: a `pow` per member under law 0, a `sin`/`cos` pair per member for the
mean field, `atan2`, `exp`), and the per-sample swarm phase advance. This is the B378
audit's territory (F1 specialised kernels, F9 the K = 0 guard): output-neutral work that
must leave every parity digest unchanged, measured against this table.
## Out of scope for this PR series

- The B378 fixes: after parity, one ledgered divergence each.
- The table edges (B380/B383).
- The plugin shell.
- The parameter manifest (B376).
- `src/`, `reference/`, `specs/`, and `h2/cores/**`.
- **The quality suites (aliasing, zipper):** a named DEFERRED checkpoint (above).
- The auhost `--engine` mode: it is committed after #886 merges.

## Open questions

1. **M1–M3's OFF positions.** The C++ carries the ON law only. `ksmPerRate`,
   `onsetBipolar` and `tempoGrid` are unknown keys to it (`set` returns false, and
   the check turns any such key red). Keep it that way?
2. **Where the target pins are enforced.** The razor-core.js pin is in `verify
   fast`; the engine's five are in the parity check (`verify full`, once wired).
   Proposed: leave them there.
3. **The floor's form (the human's).** A per-scenario floor is fragile by
   measurement. The proposal: a mean share of at least 30%, keyed on darwin-arm64,
   Apple clang 16 and Node 24; the baseline is 37.00–38.34%, and FMA reads 17.48%.
4. **The ring criterion (the human's; it blocks the wiring).** Choose one:
   - (a) remove the three rows, as ADR-065 did;
   - (b) rule b, built and selectable: RMS exempt, events, readouts and the
     strict first-128 bound held, and the golden's self-divergence at least K ×
     the C++'s. At K = 10 it holds on 23 of 25 nudged inputs; no K above about 2
     holds on all 25;
   - (c) rule 1a, the default: the arp row is red at the scripts' own inputs.

   A variant worth the human's eye: rule b with K = 1 (the C++ no further from the
   golden than the golden is from itself). It is ADR-065's literal statement, and
   with the first-128 bound it still catches X1.
