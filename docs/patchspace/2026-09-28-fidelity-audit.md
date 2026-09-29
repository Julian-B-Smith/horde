> **Aliasing REGENERATED (2026-09-29, B346).** §1a's aliasing bullet and §5's aliasing paragraph below were re-measured by `node tools/patchspace/fidelity_audit.mjs` with its B346 chord pass, on the same presets, chord hold, window and seed, with B345's fixed aliasing metric (PR #843) and B346's os-convergence estimator beside it; the pre-B345 figures they replace were taken with a metric that averaged the whole 0.5 s window before comparing (B342(1)). Everything else in this report (underruns, the neutral case, steals, beating, mechanisms) does not depend on the aliasing metric and is unchanged from B325.

# B325 — DSP fidelity audit: are the noise and clicks real?

HYPERSAW, 2026-09-28, ROADMAP B325 (records PR #821, branch `lead-records-128`). The human: "I'm also starting to notice more noise and clicks that I'm not certain are supposed to be part of the waveforms. Let's make sure the DSP fidelity is holding up. Maybe that's a better challenge for the C++ port, but it would be worth running some tests to make sure I'm not crazy."

The engine is the composed SCALPEL × horde engine (`docs/design/scalpel-horde-engine.js`) and its oracle is RazorCore (`reference/scalpel/prototype/razor-core.js`, protected: read and called, never edited). The code is at `origin/main` e368564 (the lab and engine as of B317). B323's tail culling had not landed, so it is not in anything measured here. The numbers only, no audio; the WAVs are in `local/patchspace/fidelity/` (git-ignored, see §9).

## Verdict

**You are not crazy, and most of what you hear is playback, not the waveform.**

1. **Playback underruns (the clicks and crackle).** The lab's own AudioWorklet ran in headless Chrome 153 for a 2.9 s chord-then-arpeggio phrase per preset. **33 of 83 presets underran**: 8125 underrun events, which is 47.2 s of output replaced by silence (Chrome's `AudioContext.playbackStats`, the device-side count). The other 50 presets had none. Every underrun event is about 5.8 ms of silence, so a sustained overload sounds like a buzz of clicks and a thin, crackling tone.
2. **The render is correct.** The samples the worklet produced are **bit-identical** to an offline Node replay of the same session for 65 of 83 presets. For the other 18 they are within 1e-5 (≤ −100 dBFS), which is float-library rounding between Chrome's and Node's V8 builds; the largest (1e-5) is in *Cross-mod ring (watch)*, a chaotic cross-modulation preset. The transient detector finds **0 events in the tap and 0 in the replay** for all 83. So the dropouts are CPU (the render arrives late), not DSP.
3. **The render has three real artefacts: one was the composed engine's own (now fixed), two are law.**
   - **Plumbing (FIXED).** Where the two swarm laws coincide, the composed engine must equal the oracle sample for sample. At `origin/main` it did so in 241 of 332 renders (83 presets × 4 phrases); with this PR's fix, 308. The one defect was that the blades read the UNDETUNED pitch as every member's frequency until the swarm's first tick reached them. It caused a per-note DC offset of up to 2.5e-2 lasting up to ~85 ms on lock-2 presets (a soft thump; *Formant pluck*, *Chord of formants* and five more), and a modulator phase offset on `mUnit` presets. The other 24 renders differ by design: the swarm steps a mono glide at its 16-sample tick (§4).
   - **Stolen voices (ORACLE-inherent mechanism, exposed more by B310's law).** A stolen voice restarts from its current envelope with its phases reset. The mechanism is the oracle's, and horde's C++ does the same (B310). The composed engine meets it more often because of B310's voice law: 1306 phase-resetting steals against the oracle's 213 on these phrases, **172 of them cutting more than −40 dBFS (the oracle: 40)**, and the worst cut is −15.5 dBFS (the oracle's: −17.8). It is proposed, not fixed (§7).
   - **Beating, not noise (by design).** The composed engine's "noisier" spectra (flatness up to +0.076) are horde's coupling law. At a preset's own K, SCALPEL's law locks the swarm and horde's lets it beat (ACCOUNTING row 6, "Horde law wins").
4. **One fix, in the composed engine only:** the first-tick look-ahead (§4). It comes with five new rows in `composed_engine_check`, two of them must-fail controls. The 71 earlier rows print byte-identical.
5. **A standing check guards this.** `tools/patchspace/fidelity_scan_check.mjs` is wired into `./verify full`. Its sample-exact layer catches the planted defects that the audit's own transient detector could NOT see (§8).

## 1. Method: playback separated from render first

### 1a. Offline render (Node, deterministic)

Tools: `tools/patchspace/fidelity.mjs` (shared) and `tools/patchspace/fidelity_audit.mjs` (the passes).

- **Coverage.** All 83 presets × 4 phrases × both engines, at 48 kHz, oversampling 2 (the engine default). Math.random is seeded with mulberry32 around every instance, and events land on 128-sample boundaries (the worklet quantum).
- **Phrases** (`fidelity.mjs` PHRASES):
  - *chord*: C3 G3 C4 E4 held 1.2 s, then 0.8 s of tail.
  - *repeat*: A3 struck 8×, 120 ms on and 80 ms off. This is B310's report: each strike lands in the last one's release.
  - *arp*: 16 notes/s over C3..C5 with 50 ms gates, 24 notes. This puts pressure on the pool: steals begin once tails outlast 6 slots.
  - *legato*: six overlapping notes, 350 ms each every 300 ms. It glides on the mono presets.
- **Per render:**
  - transient events (§3);
  - every steal, with the amplitude it cuts;
  - on the chord, spectral flatness (0.25..1.15 s); and aliasing, regenerated by B346 (`fidelity_audit.mjs` chord pass): B345's fixed metric against the same engine at 4× the oversampling (B316's method and caveat), B346's os-convergence estimator (the preset at 1×..16× its oversampling), the output stage every os shares (decimator leak, output-rate tanh) and the total against an oversampled truth, over the chord hold's 0.1..0.6 s;
  - non-finite and float32-subnormal output samples.
- **Hotspots.** The gauntlet's hotspots are reproduced from B316's own seeds via its sampler (`samplePatch(0xB316, i, mode)`, run `p3`'s 2000 broad + 1000 edge). Those that clicked or read noisy go through the same four phrases (§6).

### 1b. What the lab's worklet actually played (headless Chrome 153)

- **Setup.** Chrome headless with `--mute-audio` and the autoplay gesture waived. It served the lab from this worktree (`tools/serve_labs.py 8325`) and started audio through the lab's own `audioStart()`.
- **The tap.** The worklet is the lab's `workletSource()`: `CORE.toString()`, the seeded Math.random and `B271Razor`, extended at runtime by a subclass. The subclass:
  1. logs every port message with the `currentFrame` it landed on, from construction onwards;
  2. copies the rendered output of each capture window;
  3. times each `process()` with `Date.now()` (1 ms grain: a load estimate, not a DSP clock).
- **Load.** The lab's monitor, Specimen and logo ran normally, so this is the lab's real load.
- **Per preset:** panic, `applyPreset()`, and 1.1 s to settle. Then a `playbackStats` snapshot and the phrase through the lab's own `noteOn`/`noteOff` (a held chord C3 G3 C4 E4 for 1.2 s, then 16 arpeggio notes at 16/s). Then 1.1 s more and a second snapshot. `playbackStats` refreshes at about 1 Hz, which is why the snapshots are 1.1 s apart from the phrase.
- **Machine.** The Mac was shared with other sessions: its 1-minute load average was 7 to 15 during the capture (8 cores), and later as high as 48. So these are numbers under load, and a quiet machine would do better. Stated, not hidden.
- **Replay.** `Math.random = mulberry32(0xB271)`, a new composed engine with the worklet's `processorOptions` params, and every logged message applied at its logged frame before that quantum is rendered. Frame-exact alignment was confirmed by the bit-exact match (a 0-frame shift).

**Why not an AudioWorklet tap or MediaRecorder alone** (the brief's suggestion, and the memory note's warning): both sit INSIDE the audio graph. They receive every quantum the graph renders, late or not, so they cannot see an underrun, which happens at the device FIFO after the graph. The tap is still essential, because it proves what was rendered. The underruns come from `playbackStats`, which Chrome counts at the device side. This separation is the audit's method: tap = render, `playbackStats` = playback.

## 2. Playback vs render, per preset (full lab)

The 33 presets that underran, worst first. "Render load" is the fraction of each 2.667 ms quantum that the worklet's `process()` took, measured inside the worklet. It is given as the mean over the window and the worst 100 ms (> 1 means the audio thread cannot keep up at all). "Rendered ≡ offline" is the tap against the Node replay.

| # | preset | render load (mean · worst 100 ms) | underrun events | silence inserted (ms) | of total (s) | rendered ≡ offline |
|---|---|---|---|---|---|---|
| 72 | Breathing pad | 1.19 · 1.45 | 689 | 4005 | 7.18 | exact |
| 81 | Crushed bells | 1.38 · 1.67 | 639 | 3714 | 7.56 | exact |
| 82 | Clockwork | 1.31 · 1.68 | 573 | 3330 | 9.17 | exact |
| 48 | Smear and strike | 1.39 · 1.93 | 566 | 3290 | 7.29 | exact |
| 52 | Cross-mod pair | 1.30 · 1.71 | 517 | 3005 | 7.01 | exact |
| 43 | Stacked sync | 1.24 · 1.67 | 417 | 2424 | 6.43 | ≤ 5e-10 |
| 50 | Chord against scatter | 1.21 · 1.73 | 378 | 2197 | 6.20 | exact |
| 61 | Two clocks | 0.87 · 1.24 | 345 | 2005 | 6.01 | exact |
| 62 | Drift and lock | 1.12 · 1.53 | 319 | 1854 | 5.30 | exact |
| 51 | Mirror-image spreads | 1.13 · 1.69 | 309 | 1796 | 5.15 | exact |
| 69 | Glass horde pad | 0.94 · 1.14 | 297 | 1726 | 5.54 | ≤ 7e-15 |
| 80 | Feedback choir | 0.88 · 1.26 | 293 | 1703 | 5.46 | ≤ 4e-12 |
| 41 | Chasing blades | 1.00 · 1.27 | 273 | 1587 | 5.59 | exact |
| 40 | Breath and bite | 0.90 · 1.26 | 267 | 1552 | 5.56 | exact |
| 76 | Ring on ring | 0.81 · 1.23 | 251 | 1459 | 5.46 | exact |
| 45 | Formant over sync | 0.86 · 1.29 | 248 | 1441 | 5.45 | exact |
| 5 | Harmonic stack | 0.84 · 1.09 | 235 | 1366 | 5.37 | exact |
| 71 | Cross-mod shimmer | 0.78 · 1.04 | 207 | 1203 | 4.96 | exact |
| 60 | Ring of sines | 0.79 · 1.07 | 191 | 1110 | 5.03 | exact |
| 16 | Jitter swarm | 0.71 · 1.07 | 188 | 1093 | 5.10 | ≤ 3e-8 |
| 70 | Undertone cathedral | 0.83 · 0.88 | 180 | 1046 | 5.24 | exact |
| 78 | Collision chirps | 0.77 · 1.06 | 173 | 1006 | 5.01 | exact |
| 63 | Orbiting cuts | 0.83 · 1.05 | 171 | 994 | 5.00 | exact |
| 36 | Two-formant vowel | 0.81 · 1.16 | 154 | 895 | 4.90 | exact |
| 49 | Split envelopes | 0.84 · 1.10 | 92 | 535 | 4.54 | ≤ 5e-10 |
| 77 | Crushed burst | 0.73 · 1.06 | 66 | 384 | 4.39 | exact |
| 20 | Sine-to-saw fan | 0.71 · 1.05 | 57 | 331 | 4.33 | exact |
| 79 | Collision bite | 0.74 · 1.02 | 11 | 64 | 4.07 | exact |
| 29 | Frozen noise FM | 0.71 · 1.03 | 8 | 47 | 4.06 | exact |
| 6 | Golden bells | 0.78 · 1.02 | 7 | 41 | 4.05 | exact |
| 4 | Chord of formants | 0.63 · 0.82 | 2 | 12 | 4.01 | exact |
| 11 | Blade pluck | 0.42 · 0.66 | 1 | 6 | 4.01 | exact |
| 26 | Splayed blades | 0.44 · 0.66 | 1 | 6 | 4.01 | exact |

**The 50 clean presets.** Their mean render load is 0.38 at the median and 0.67 at most. The underrunning presets' median is 0.84. Every preset with more than 300 ms of silence ran above 0.7 mean load, with a worst 100 ms of 0.88 or more. "Total (s)" grows with the underruns: Chrome's `totalDuration` counts the inserted silence, so the 4.0 s window stretched to as much as 9.2 s.

**Frozen display, same machine, 27 presets** (the 25 worst plus 2 clean). The lab was loaded with `?specimen=0&still=1` (Specimen off, monitor frozen): a second capture, taken right after the first.
- Total silence was 52.1 s frozen against 45.2 s with the full display.
- A few presets went quiet: *Harmonic stack* 1366 → 17 ms, *Breath and bite* 1552 → 0, *Collision chirps* 1006 → 12, *Jitter swarm* 1093 → 0.
- Others got worse as the machine's own load rose from 11 to 13 during the second capture: *Undertone cathedral* 1046 → 4004 ms, *Glass horde pad* 1726 → 3662.
- The lab's display is therefore not the dominant term. **The audio thread's own render cost against a loaded machine is.** Every preset that underran heavily in both runs has a mean render load near or above 0.8. This is B313's finding, now measured in the playing lab. It goes to B323 (§7).

The detector cannot be the evidence for the dropouts either. Run on the modeled as-heard signal of *Harmonic stack*, *Smear and strike*, *Breathing pad* and *Crushed bells*, with 377 to 826 modeled gaps each, the 20 dB transient detector reads **0 events**. The gaps are so dense they become the baseline, and each gap's edges look like the saw's own. That is why `playbackStats` is the witness.

The modeled signal is a K-quantum FIFO drained every 2.667 ms and fed by the measured per-quantum render cost. With K = 1 it gives 38.3 s of silence against the measured 47.2 s; deeper FIFOs give ~7 s. The WAVs use K = 1 and are labeled "model". This is the only way I could place the gaps in time: `playbackStats` counts them but refreshes at about 1 Hz.

## 3. Render: transients, and what the detector can and cannot see

(pending: summary tables)

**The detector** (`fidelity.mjs` clickEvents) is B316's residual (the second difference, 512-sample frames, hop 256), with a LOCAL baseline: the median of the frames within ±85 ms, excluding the frame's own ±2. A click frame is more than 20 dB over its baseline, with a residual RMS over −90 dBFS. The same detector at 10 dB is reported as the "sensitive" count.

**Its limit, measured (must-read controls):**

- A planted DC step of 0.5 (−6 dBFS) in the loud part of the *Quarter sync* chord is not detected. The chord's own saw edges put about 13 dB more energy in each frame's residual.
- A 0.03 step 0.26 s into the release is not detected either.
- A 0.001 step after the tail has ended is detected (116 dB over its baseline).
- In the standing check, a voice starting at full envelope, a split-integrator phase slip and a ±0.1% frequency jitter all read 0 events.

So "0 events" means **no transient that stands 20 dB out of its surroundings**. It does not mean "no discontinuity". This is why the audit does not rest on it: §4 (sample-exact neutral case), §5 (steals measured at the engine) and §6 (mechanisms by sample difference) do the sensitive work, and the standing check's controls run through the sample-exact layer.

## 4. Render: the neutral case — the composition's plumbing, and the one defect it found

**Where the two swarm laws coincide** the composed engine must equal RazorCore sample for sample. That is ACCOUNTING row 4 ("old patch hears the same") and `composed_engine_check`'s O3: K 0, dist 0, law 0, no drift, the aligned start. The pairing:
- the oracle's start is moved by §1.6.6's ½;
- the composed engine allocates by the oracle's voice law. B310's law differs by design, and the VL rows own it.

Every preset and every phrase went through this pairing: 332 renders (`fidelity_audit.mjs neutral`). This is the audit's SENSITIVE layer: a difference of one sample shows, however masked it would be in the mix.

| engine | renders exactly equal to the oracle (max abs Δ = 0) | the rest |
|---|---|---|
| composed, at `origin/main` e368564 (the lab as the human played it) | 241 / 332 | 91 |
| composed, with this PR's fix | **308 / 332** | 24: the 12 mono presets with glide, on the two phrases that glide (chord, legato) |

**The defect (COMPOSED-ENGINE-introduced, FIXED).** Until the swarm's first tick reached `couple()` (its 32-sample pass), the composed engine handed RazorCore the UNDETUNED pitch as every member's frequency `m.inc`. The code comment said "no swarm tick yet this note: the pitch". `m.inc` feeds:
- the Hz-unit cut rate (lock 2);
- the Hz-unit modulator (`mUnit`);
- the per-cycle DC estimate taken on the note's first sample.

That produced two artefacts, measured in the neutral case against the oracle:
- **A slow offset.** On lock-2 presets the note's DC estimate was taken with the wrong cut rate, which read as an offset of up to 2.5e-2 (a 256-sample mean; about −32 dBFS). It lasted until the estimate's next refresh, up to ~85 ms, on every note. The worst was *Formant pluck* on the arpeggio, max 3.2e-2 (so a soft thump per note). The same class reached 1e-3 to 6e-3 on *Chord of formants*, *Fixed formant*, *Two-formant vowel*, *Formant over sync*, *Chord against scatter*, *Occlusion sweep* and *Glass horde pad*.
- **A modulator phase offset that never recovered.** On `mUnit 1` presets the modulator integrates `mHz/fi`. This read up to 1.0e-1 on *Crunch horde* and 1e-3 on *Two FM voices* (blade 2 `mUnit2`). It is a constant phase shift of the modulator; it is not a click.

**The fix** (`docs/design/scalpel-horde-engine.js`, `startVoice`, `lookAhead`, `couple`, `stepM`):
- The note's first swarm tick is taken as a LOOK-AHEAD in `startVoice`, so `m.inc` is the swarm's own frequency from note-on. The swarm is snapshotted first.
- `stepM` restores the snapshot and takes the real first tick exactly where it always did. So the swarm's trajectory is unchanged: `composed_engine_check`'s 71 earlier rows and all 97 lines of its output outside the new section print **byte-identical** before and after.
- `couple()` re-takes the look-ahead if the pitch moves before the first sample. This covers a chord struck on a mono voice, where four note-ons land in one block.

The first attempt kept the look-ahead tick and skipped the scheduled one. The neutral case caught it at once: it froze the swarm at the first of four same-block note-ons on *Zap bass*, where the pre-fix engine had been exact, and read Δ 0.79. That attempt is now a must-fail control in `composed_engine_check` (§8).

**The 24 remaining differences are the glide (by design).** The composed engine steps a mono glide at the swarm's 16-sample tick, where RazorCore glides per sample (the composition's G3: "the swarm reads it as f0 on its 16-sample tick, which is where horde steps it"; horde's C++ steps it at its control tick too, ADR-096). With glide off (1 ms), the same presets are exact on the legato phrase. The difference is a slow phase drift, not a transient.

## 5. Render: stolen voices, beating, aliasing, subnormals

**Stolen voices: the one real render-side click source (ORACLE-inherent mechanism, exposed more by B310's law).** A fresh start on a slot that is still sounding keeps its envelope (`razor-core.js:397`: `if (!v.active) v.env = 0;`, so an active slot keeps `env`) and resets every member's phase and BLEP state (`:399`, `m.prev = 0`). Whatever the old voice was outputting is thrown away in one sample. horde's C++ `initVoice` does the same (B310's open note). The table gives the cut: env × vel × norm × the members' last outputs × gain × 1.6 (small-signal, before the tanh), logged at every steal. All 83 presets, the four phrases:

| engine | starts on a sounding slot | phase-resetting (fresh) | cut > −40 dBFS | cut > −20 dBFS | worst cut | median / p95 cut |
|---|---|---|---|---|---|---|
| RazorCore | 2243 | 213 (all in the arpeggio) | 40 | 3 | −17.8 dBFS | −50.5 / −26.1 dBFS |
| composed | 1762 | 1306 (1278 arpeggio, 28 repeat) | **172** | **14** | **−15.5 dBFS** | −54.1 / −29.7 dBFS |

The oracle's other 2030 starts on a sounding slot are its same-note REUSE, which keeps the phases and so cuts nothing. That reuse is exactly what B310 replaced ("the release of the first note doesn't continue"). Under horde's law, a recurring arpeggio note takes a new slot, and when the pool is full, tier 2 restarts the quietest tail. So the composed engine trades the oracle's cut-off release for more, quieter phase resets. Four times as many cross −40 dBFS. The worst are *Crushed bells* and *FM bell* on the arpeggio (env ≈ 0.39–0.41 at the steal).

The 20 dB transient detector reads 0 of these in context, because the arpeggio's own edges mask each one (§3). They are real discontinuities, heard as a tick when the pool is full. **Proposed, not fixed here** (§7): a steal fade. It would diverge from both references (RazorCore and horde's C++), so it is a human call and a port divergence, not a lab patch.

**"Noise" that is beating (COMPOSED-ENGINE, by design).** On the held chord the composed engine's spectral flatness exceeds the oracle's:

| | p5 | p50 | p95 | max |
|---|---|---|---|---|
| composed − oracle flatness | −0.001 | +0.005 | +0.066 | +0.076 |

The largest excesses, oracle → composed:

| preset | oracle | composed |
|---|---|---|
| *Mirror-image spreads* | 0.010 | 0.086 |
| *Harmonic stack* | 0.006 | 0.076 |
| *Cross-mod horde* | 0.037 | 0.106 |
| *Counter-rotation* | 0.006 | 0.072 |
| *Breath and bite* | 0.037 | 0.103 |

Decomposed on four of them (both starts aligned, the preset's own K), the excess survives unchanged: *Harmonic stack* 0.0057 → 0.0756, *Mirror-image spreads* 0.0040 → 0.0856, *Two blades* 0.0028 → 0.0397, *Cross-mod horde* 0.0401 → 0.1061. At K 0 it falls to 0.022 / 0.033 / 0.003 / 0.013, the remainder being the start law: the composed engine plays "settled" (phaseMode 2) as the aligned retrig start (ACCOUNTING Q B4); the oracle settles.

So the excess is **the coupling law**. At these presets' K (0.45–0.6, ±10–14 cents), SCALPEL's law locks the swarm to one clean partial set, while horde's lets the members beat. ACCOUNTING row 6 says horde's law wins, so this is by design. It is still worth hearing: presets voiced for SCALPEL's lock will sound busier on horde's law. That is a preset-translation question (§10), not a defect.

**Aliasing: none added (regenerated by B346).** On the held chord (83 presets, both engines, the preset's own oversampling, 0.1..0.6 s):

| engine | B345 aliasDb p5 / p50 / p95 | os-convergence excess p5 / p50 / p95 | total vs the oversampled truth p5 / p50 / p95 | decimator leak p95 | output tanh fold p95 |
|---|---|---|---|---|---|
| oracle | −120.0 / −120.0 / −21.6 | −97.7 / −53.6 / −19.5 | −96.1 / −50.1 / −16.4 | −48.2 | −54.8 |
| composed | −120.0 / −120.0 / −19.4 | −95.6 / −52.0 / −19.8 | −96.2 / −50.5 / −16.7 | −48.2 | −54.8 |

- **Composed − oracle, p5 / p50 / p95:** B345 aliasDb −6.4 / 0.0 / +0.3 dB; os-convergence excess −5.9 / +1.5 / +8.0 dB; total −11.9 / −0.6 / +5.7 dB. The composition adds no aliasing at the median. The spread either way is, as a hypothesis, the two swarm laws' different trajectories (§5 beating), not a new folding source; it was not decomposed.
- **Largest composed − oracle total:** *Crunch (audio-rate PM)* −50.8 → −44.2 · *Drifting cuts* −72.7 → −66.7 · *Ring saw* −65.9 → −59.9 · *Drift and lock* −56.2 → −50.5 · *Metal pair* −70.4 → −64.7. All are 40 dB or more under the signal.
- **Where the chord aliases at all (composed):** *Feedback snarl* −6.1, *Feedback screech* −8.3, *Screamer* −10.5, *Feedback choir* −16.1 (all classed rate-dependent dynamics; all four have feedback on, fb 0.35 to 0.75: B346's feedback-loop finding), *Breathing pad* −16.7, and *Cross-mod roar* −17.5, *Cross-mod pair* −18.0, *Cross-mod horde* −22.1 (cross-member modulation on, xm 0.45 to 0.6: the phase input the PolyBLEP scanner does not track, B346), three presets the B345 metric reads as −120 because its noise floor hid them.
- The pre-B345 paragraph read "−1.2 / 0.0 / 0.0 dB; the largest *Hollow square lead* −61.7 → −59.6". The conclusion (the composed engine adds no aliasing) stands; the absolute levels it implied did not: under the fixed metric and the estimator, the feedback and cross-mod presets alias at −6 to −22 dB in both engines alike.

**Non-finite samples: none.** 0 over 664 renders.

**Float32 subnormal output samples: 4745**, split 2363 oracle and 2382 composed. They sit in decaying tails, from filter and blocker states running down toward zero. JS shows no audible or CPU effect from them; a C++ port without flush floors would carry them (§7).

## 5b. The candidate mechanisms, by sample difference

**Method** (`fidelity_audit.mjs mech`). Each mechanism is removed from the engine by an in-memory text patch of RazorCore, or by a subclass of the composed engine. The file on disk is never touched, and every patch asserts its anchor occurs exactly once. The difference between the render and the ablated render then IS that mechanism's contribution.

**Coverage.** All 83 presets, on the chord and the arpeggio. No preset plays reflected Crush or Crush under collision, so those two mechanisms are measured on constructed subjects: the nearest preset with the one switch that engages the path (named in the table).

**How to read the columns.** Peak Δ is the size of the contribution. The one-sample step of Δ measures a click only where Δ is otherwise smooth (crush exit, scanner, release cut). Where the ablation changes the whole sound, the renders diverge and the step column measures that divergence instead (noSteal, tick1, the DC freeze).

| mechanism (source line) | engine | where it acts | peak Δ | largest step of Δ | reading |
|---|---|---|---|---|---|
| reflected Crush exit jump (`razor-core.js:207-213`: the slew lands on the base at st + w/2, and the palindrome returns to the entry's level at the exit) | oracle | *Slewed crush* + mirror 1; *Fixed crush* + mirror 1 | −3.7 dBFS (slewed), −22 dBFS (hard) | −11.1 / −25.4 dBFS, repeated every cycle (20–30 k samples over −40 dBFS) | A PERIODIC edge: a timbral buzz, band-limited by the exit-edge BLEP (`tryE(st + w)`), not a one-off click. It is large under slew, where the design intends a smooth landing. ORACLE-inherent. |
| the PolyBLEP scanner reads the collision accumulator for a Crush blade (`razor-core.js:633`) | oracle | *Crushed burst* + hard2 0, colK 0.8 | −28 to −30 dBFS | −26.6 dBFS, 3.4–5.1 k samples over −40 dBFS | Misplaced BLEP corrections on every hard Crush step while the blades overlap: ALIASING and grit (the "noise" class), not a click. ORACLE-inherent. |
| per-cycle DC refresh (`razor-core.js:843-857`; the estimate also reads the modulator rate, `:577`) | oracle | 103 / 172 renders | up to −5.3 dBFS (*Clockwork*) | (divergence: see note) | Freezing the estimate at note-on moves the output's operating point, and through the tanh the whole waveform, so the step column is not a click measure here. The refresh is slewed (0.003 per sample), so it cannot step by itself. It is not a click source. Whether the estimate JITTERS (B316's SC_DCRES) is §10's open item. |
| release cut at env < 1e-4 (`razor-core.js:779`) | oracle | 131 / 172 | median −82.8 dBFS, max −47.5 dBFS | max −51.5 dBFS (divergence after the voice is freed included) | Inaudible under anything but silence. ORACLE-inherent; keep. |
| stolen-voice restart (`razor-core.js:397-401`) | both | 74 / 172 | (divergent) | (divergent) | Measured instead at the engine (§5: the cut at every steal). |
| the composed engine's two phase integrators | composed | 171 / 172 | presets without xm/fb: median −163 dBFS, max −144.5 dBFS; xm/fb presets: max −19.7 dBFS | — | Rounding (1e-12 cycles), amplified only by the chaos of cross-modulation and feedback, which amplifies ANY rounding. Not an artefact. |
| the swarm's 16-sample control tick | composed | 149 / 172 | (a different control law, see note) | (divergent) | Ticking every sample changes the swarm's dynamics, so the difference is a different sound, not a removed click. Its noise share is in the attribution (§7). |

## 6. Gauntlet hotspots

(pending)

## 7. Attribution

(pending)

## 8. The standing check and the new composed_engine_check rows

### `tools/patchspace/fidelity_scan_check.mjs` (WIRED: `./verify full`, ~30 s idle)

On a fixed, seeded preset subset, it checks that the composed engine adds no click and no noise over RazorCore beyond a stated tolerance. It has two layers, because §3 showed that one layer is not enough.

**NEU, the sensitive layer (10 presets × chord/legato/repeat, cut short).** In the neutral case (K 0, the aligned start, the oracle's start moved by ½, the oracle's voice law), the composed engine must equal RazorCore **exactly**: tolerance 0, as measured. The presets are *Quarter sync*, *Two blades*, *Crush vs FM*, *Reflected sync*, *Slewed crush*, *Blade pluck*, *Collision chirps*, *Harmonic stack*, *Formant pluck* (lock 2) and *Crunch horde* (mUnit 1). The mono glide presets are left out, because of the glide (§4).

**MIX, the coarse layer (6 presets), at the presets' own settings.** Two conditions:
- no more 20 dB transient events than the oracle;
- chord flatness excess no larger than the excess the audit RECORDED for that preset (the coupling law's share) + 0.02.

A flat tolerance above the worst law excess (0.12) was tried first and could not see control C4, so it was replaced.

**Controls** (each must fail, through the same comparison):

| id | planted defect | detector reading |
|---|---|---|
| C1 | a split-integrator phase slip at 0.25 s | NEU Δ 1.4 |
| C2 | a fresh voice starting at full envelope | NEU Δ 0.53 |
| C3 | ±0.1% seeded increment jitter per tick | NEU Δ 0.29 |
| C4 | the swarm's drift stuck at 60 cents | MIX flatness Δ 0.045 against a limit of 0.018 |
| Z | must-read-zero: the oracle against itself | 0 everywhere |

The audit's own 20 dB transient detector reads 0 events on C1, C2 and C3. That is the reason the NEU layer exists.

### `tools/labharness/composed_engine_check.mjs`: five new rows

| row | what it shows | reading |
|---|---|---|
| B325 | after `noteOn`, before any render: max abs(m.inc − S.eff) | 0 Hz |
| B325 | neutral case exact on *Formant pluck* and *Crunch horde* | 0 |
| B325c | CONTROL: the old placeholder rebuilt | 220 Hz; neutral Δ 3.9e-3 / 5.5e-2 |
| B325 | a mono chord (four note-ons in one block) exact on *Zap bass* and *Screamer* | 0 |
| B325c | CONTROL: the fix's own first attempt | Δ 0.84 |

- **VL2/VL3 re-strike evidence rewritten, not relaxed.** The rows' reading of a re-struck swarm changed from `vfInit === 0` to *the swarm holds the new note as SwarmSynth's newest strike, and its first tick was taken at the strike or is still pending*. The fix takes the first tick at the strike, so `vfInit === 0` no longer holds after a steal. The new condition is stricter, not looser. **Flagged for the lead.**
- **Everything else prints byte-identical.** All 71 earlier rows, and all 97 lines of output outside the new section, are the same as on `origin/main`'s engine. I checked this by running the same check file against both engines.

## 9. Listening (git-ignored WAVs)

(pending)

## 10. Open questions

(pending)
