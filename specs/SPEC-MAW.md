# MAW — multi-stage waveshaper / saturation / distortion module for HORDE

**Spec recommendation sheet · prototype-to-agent handoff · 2026-09-16**
**Name status:** MAW is a placeholder. Naming is open. `ball: julian`

This packet contains the browser prototype (`maw-horde-distortion-prototype.html`), the DSP core it runs (`core.js`, a single class usable as an AudioWorklet or by direct instantiation), a property-based fidelity battery (`fidelity.js`, report in `fidelity-report.txt`), a decay-tail test (`tail.js`), and the preset set with its audibility check (`presets.js`, `preset-test.js`). The prototype is the **parity oracle for the memoryless curve math and the routing topologies**; the battery is the **oracle for sonic properties**. Section 9 says which is which, and section 10 lists what in the prototype is incidental and must not be replicated.

---

## 1. Intent

A Roar-class distortion device that lives in HORDE's FX chain (not a voice engine — distinct from WARP), built as three identical stages that can be arranged in five topologies, with each stage owning a morphable pair of transfer curves, a tone filter, and the usual drive/bias/level/mix. On top of that, the HORDE-native behaviours this whole device family shares: **inertia** on every continuous stage parameter, **ecology** (stages modulate each other's drive through their output envelopes), and **flux** (an OU walk on each stage's morph as the quantum-morph mechanism).

The reference for feel is Ableton Roar: drive ahead of the curve, a per-curve "amount" (here: `shape`), a tone filter that can sit before or after the curve, and topologies including a pitch-tracked feedback loop. The reference for math is the prototype.

---

## 2. Signal architecture

```
in ──► [input gain] ──► TOPOLOGY( stage1, stage2, stage3 ) ──► dry/wet ──► [output gain] ──► out
```

### 2.1 Topologies (`route`)

| id | name | flow |
|---|---|---|
| 0 | series | in → S1 → S2 → S3 |
| 1 | parallel | in → S1, S2, S3 → Σ |
| 2 | multiband | in → LR4 split at `xo1`, `xo2` → lo→S1, mid→S2, hi→S3 → Σ |
| 3 | mid/side | M=(L+R)/2 → S1; S=(L−R)/2 → S2; recombine → S3 |
| 4 | feedback | (in + fb·loop) → S1 → S2 → S3 → out; loop tapped after stage `fbTap`, through delay → lowpass damp → highpass → back |

Multiband uses two 4th-order Linkwitz–Riley crossovers (each = two cascaded Butterworth SVFs per side, Q = 0.7071). Low band = LP4(xo1); the residual HP4(xo1) is split again at `xo2 = max(xo2, 1.5·xo1)`. The band sum is measured flat to ±0.1 dB across the spectrum (battery §2) — the small phase-compensation asymmetry (low band does not pass through the second crossover's allpass) is within that and is accepted.

Mid/side recombination is an exact identity on stereo input with stages bypassed (battery §2). Parallel sums to 3× dry with stages bypassed.

### 2.2 Stage (identical ×3)

```
x ──► [envIn follower]
  ──► [tone filter, if fpos = pre]
  ──► u = g·x + bias                         (g from drive, see 2.3)
  ──► floor crossfade: below |x| < floor pass g·x, above use curve  (smoothstep over [floor, 2·floor])
  ──► y = (1−m)·A(u, shape) + m·B(u, shape)   (curve morph; ADAA form when eligible, §5)
  ──► DC blocker (1-pole HP, 10 Hz)
  ──► × comp (measured auto-gain, 2.4) × pk (drive-ref, 2.5)
  ──► [tone filter, if fpos = post]
  ──► out = (dry·(1−mix) + y·mix) · level
  ──► [stage output envelope, for ecology]
```

Tone filter: Zavalishin TPT SVF, `ft` ∈ {off, LP, BP, HP}, cutoff `cut` (Hz, log, clamped 2^4.3 … 2^14.3), `res` 0..1 → Q = 0.7071·20^res. Stage runs stereo (independent filter/DC/OS state per channel; one shared parameter set).

### 2.3 Drive

`drive` is input gain in dB, range −12 … +48 in the UI, clamped −12 … +60 after modulation. Bias is a DC offset added after gain (asymmetry → even harmonics). Both are removed downstream by the DC blocker.

### 2.4 Auto-gain (`auto` 0..1, global)

Not a static 1/√g — that is wrong for saturating curves and was the first bug found. At every control tick each stage runs a 32-point reference sine of amplitude A through its current curve/drive/bias/morph, removes the DC of the result, and computes `comp = (rms_in / rms_out)^auto`, clamped 0.05 … 20, smoothed one-pole 0.25 per tick. A = 0.5 in static mode (`autoMode` = 0) or the tracked input peak (`autoMode` = 1). Measured: static mode holds ten of fourteen curves within 0.3 dB across 0–48 dB drive; tracked mode collapses input-level dependence from 9–25 dB to ~2 dB across a 26 dB input range, at the cost of behaving like a slow AGC (§8, open decision D3).

### 2.5 Drive reference (`driveRef`, global)

Absolute (0): `u = g·x + bias`. Input-peak (1): `u = g·x/pk + bias`, output `× pk`, where `pk = envIn · π/2` (a symmetric 1-pole follower of |x| at coefficient 3e-4/sample, so mean|x| of a sine → its peak). This is the "position on the curve" semantic: 0 dB drive always puts the input peak on ±1, the domain edge, regardless of source level, and because the output is re-scaled by `pk` the note's dynamics are preserved (`pk·f(x/pk)` — a dynamic waveshaper). It exists because cheby's pure-harmonic point and fold's first fold both sit at the domain edge. See D3.

### 2.6 Control rate and interpolation

Stage coefficients (g, bias, morph, comp, pk) are computed at control rate and **linearly interpolated per sample** between ticks. This is required: with block-held coefficients a 10 Hz LFO on drive produced 28× more sample-to-sample energy at block boundaries than inside blocks; interpolated, 1.0× (battery §7). Filter coefficients are updated per tick without interpolation (TPT SVF tolerates it).

---

## 3. Curves

Fourteen curves. `x` is the driven, biased input; `s` is the per-stage `shape` (0..1), whose meaning is per-curve. Every curve is a static function — none can change pitch; what moves with drive is the dominant partial.

| id | name | f(x, s) | shape means | symmetry |
|---|---|---|---|---|
| 0 | soft | (1−s)·tanh x + s·clip(x) | knee hardness | odd |
| 1 | hard clip | brickwall at ±1 with quadratic knee of half-width k = 0.9s: for 1−k<|x|<1+k, sgn·(|x| − (|x|−(1−k))²/4k) | knee width | odd |
| 2 | overdrive | od(x)·(1 + 1.5s·(1−od²)), od = classic piecewise (2u; (3−(2−3|u|)²)/3; 1) at u = x/2 | mid-level bite | odd |
| 3 | tube | 1.5(c − c³/3) + 0.5s·c², c = clip(x) | even-harmonic term | even |
| 4 | diode | x>0: tanh x; x<0: tanh(kx)/k, k = 1+6s | negative-side ceiling | even |
| 5 | fold | (1−s)·sin(πx/2) + s·trifold(x) | ridge hardness (sine ↔ triangle) | odd |
| 6 | rectify | clip(((1−s)x + s|x|)·(1+s)) | half → full wave | even |
| 7 | crush | round(x·L)/L, L = 2^(8−7s) | bit depth 8 → 1 | odd |
| 8 | clip→wrap | (1−s)·clip(x) + s·wrap(x), wrap = ((x+1) mod 2) − 1 | wraparound amount | odd |
| 9 | cheby | cos(n·acos(trifold(x))), n = 1+4s | harmonic index (1…5, continuous) | n-dependent |
| 10 | polynomial | clip(c + 0.45s·sgn·sin(πN·|c|^p)·(1−c⁴)·tanh(6|c|)), N = 1+9s, p = 1−0.75s | wiggle density (crowds toward centre) | odd |
| 11 | fractal | trifold((1−0.5s)·c + 1.6s·Σₖ 0.8^(k−1)·tri(2^(k−1)·c/2 + ¼) / Σ 0.8^(k−1)), K = 1+7s octaves (fractional last octave faded in) | octave count and ridge amplitude | odd |
| 12 | shards | c + Σ_{j<J} hⱼ·raised-cosine((c−pⱼ)/wⱼ), J = round(32s), table SHARDS fixed (LCG seed 1337) | shard count | asym |
| 13 | noise | clip(c + s·n·|c|), n ∈ U(−1,1) from the instance RNG | noise amount | — |

`trifold` folds ℝ onto [−1,1] as a triangle wave of period 4 (continuous). `tri(u)` is a unit triangle wave of period 1. Exact code is in `core.js` and is the parity reference.

Verified properties (battery §3): odd curves have even-harmonic content >160 dB down at zero bias; cheby at integer n from a sine puts >98% of harmonic energy in Hₙ; full-wave rectify removes the fundamental entirely; `morph` is exactly a linear blend of the two curves' outputs (the post-curve path is linear), so any morph position is documentable as a blend, not a new curve.

### 3.1 Per-curve parameter semantics worth stating in the manual

- **fold**: drive = fold count, shape = ridge hardness, bias = asymmetry. Three independent axes (an earlier version had shape multiply into the same gain as drive — one degree of freedom on two knobs; do not regress this).
- **cheby**: shape selects the harmonic. Drive from −12 to 0 dB (with drive-ref on) is a clean intensity ramp that keeps the fundamental dominant; 0 dB is the pure harmonic; above 0 dB the argument folds and the dominant partial hops (H1→H3→H5→H7…). For **even n the fundamental cannot be retained by drive at all** (T₂ = 2x²−1 has no linear term) — the intensity control for even n is the stage `mix`. This is math, not a mapping choice.
- **polynomial**: the wiggle amplitude is tapered by tanh(6|x|). Without the taper the curve produced near-full-amplitude wiggles for tiny inputs and released notes rang on for 400 ms longer than a saturator (tail test). Do not remove the taper.
- **fractal**: octave weights decay at 0.8 and the ridge amplitude grows with shape; H13–40 land 11 dB below H1–4 at full shape (was 27 dB in the first version, which was inaudible).

---

## 4. Stage `floor` (per stage, 0 … 0.3 linear, shown in dB)

Below `floor` the stage passes clean gain `g·x`; between `floor` and `2·floor` it smoothstep-crossfades into the curve. A nonlinearity threshold. Measured on a 300 ms-T60 note at 24 dB drive: shards' release tail comes down from 430/530 ms (−40/−60 dB) to the saturator baseline of 290/390 ms with floor at −40 dB. Floor > 0 disables ADAA on that stage (the blended function has no closed-form antiderivative). Default 0.

---

## 5. Anti-aliasing — the main constraint

HORDE's global 2× oversampling is the ceiling; 4×/8× is untenable plugin-wide. The battery ran an OS ladder (1/2/4/8×) and first-order ADAA per curve so the strategy is measured, not assumed (battery §4, §4a, §4b; worst case = 4.7 kHz sine at 24 dB drive, shape 0.5):

| family | 2× | 2× + ADAA | verdict |
|---|---|---|---|
| soft, hard, overdrive, tube, diode, crush | −26 … −40 dB | soft: −53 dB | 2× is enough; ADAA where available matches what 4× would have bought, at ~3% cost |
| clip→wrap, cheby, polynomial, shards | −16 … −37 dB at 4×; 8× barely better | — | **dirty by design**; discontinuities/dense ridges defeat oversampling; document, don't chase |
| fold | −0 dB at 4×, −22 at 8×, ADAA1 useless (−4.5) at 4.7 kHz; **−121 dB at 234 Hz** with identical settings | — | aliasing scales with f × drive × order; fix is a **pre-fold lowpass or note-following order limit**, not OS |
| feedback route | same as the stages inside it | | loop doesn't worsen aliasing once the stages are oversampled |

**Recommendation:** run the module under HORDE's global 2× (whole-chain, not per-stage — the prototype's per-stage FIR wrapper is incidental, §10). Implement ADAA (first-order, with the midpoint fallback for |Δu| < 1e-4) for every curve with a closed-form antiderivative and enable it by default. Antiderivatives already written and verified: soft `(1−s)·logcosh(x) + s·clipAD(x)`, fold `(1−s)·(−2/π)cos(πx/2) + s·triAD(x)`, clip→wrap `(1−s)·clipAD + s·wrapAD`. Easy additions for the agent: hard clip (piecewise cubic through the knee), tube (quartic), diode (piecewise logcosh), rectify (piecewise quadratic). Ship fold with its tone filter defaulting to **LP pre** (open decision D5).

ADAA caveats to carry into the design: half-sample latency and a mild HF rolloff inherent to first-order ADAA; it is bypassed when `floor` > 0.

---

## 6. Feedback route

- Delay line, 1 s max, **cubic Hermite** interpolation (linear was audibly dull and part of the original "noisy" complaint).
- Time: `fbMode` 0 = pitch-tracked, T = sr / (f_lastNote · 2^(fbSemi/12)); 1 = free, `fbMs`. Time target is slewed per sample at 0.003 (≈7 ms) to avoid zipper.
- Loop: read → 1-pole lowpass `fbDamp` → SVF highpass `fbHp` (Q 0.7071) → × (fbAmt · fbNorm) → summed into stage 1 input. Written into the delay: tanh(tap), tap after stage `fbTap` ∈ {1, 2, 3}.
- **Loop normalisation (ADR-1):** `fbNorm = 1 / Π_{stages ≤ tap, on} (mix·slope·g·comp·level + (1−mix)·level)`, where `slope` is the curve's numeric derivative at the bias point (floor 0.25), product clamped 0.05 … 400. `fbAmt = 1.0` therefore means unity small-signal loop gain regardless of stage drives; the pre-normalisation version had loop gains far above unity at 85% and was the noise the first review flagged. Measured: burst decay differs ~20% between 0 and 40 dB drive (saturation lowers large-signal loop gain), expected and documented.
- Silence in → exact silence out at 120% feedback (battery §6); the loop cannot self-start from nothing. Self-oscillation from a signal at >100% is intended.

---

## 7. HORDE behaviours

**Inertia** (`inertia` 0..1, global): every stage's drive, bias, log-cutoff and morph target passes through a mass-spring integrated at control rate: ω = 2π·30·0.02^I rad/s (30 Hz → 0.6 Hz), ζ = 1 − 0.85·I. Measured step response: 0% overshoot at I = 0.1, 17% at 0.6, 47% at 0.9 with ~2 s settle. I = 0 is instant (spring bypassed). If HORDE already has a canonical inertia primitive, use it and re-derive these constants so the knob feel matches (D8).

**Ecology** (`eco` −1..1, global): each stage's drive is offset by `−eco · 36 dB · mean(other stages' output envelopes)`; envelopes are 1-pole |y| trackers (attack 0.02, release 0.0015 per sample). Positive: a hot stage starves the others. Negative: stages feed each other. Envelope-driven, not signal-cancelling. Still to be judged against a plain compressor on programme material (D6).

**Flux** (`flux` 0..1, global): per stage an OU process on morph, `w += 2·(0−w)·dt + 0.8·flux·√dt·N(0,1)`, clamped ±0.6, added to the morph target. Uses the instance RNG.

**Modulation (prototype-local, replace with HORDE's mod system):** input envelope follower (`envA`/`envR` ms) and a sine LFO (`lfoRate`), with fixed-depth targets: env→drive ±24 dB, env→cutoff ±4 oct, env→morph ±1, lfo→drive ±24 dB, lfo→cutoff ±3 oct, lfo→bias ±0.6. The *targets and depth scalings* are the spec; the sources are whatever HORDE's mod matrix provides.

**Determinism:** one xorshift32 RNG per instance drives flux and the noise curve; the shards table is a fixed LCG table. The prototype seeds at 0x51ED; in HORDE the seed comes from the device's seeding discipline. No wall-clock anywhere.

---

## 8. Open decisions — staged for DECISIONS.md

| # | decision | recommendation | ball |
|---|---|---|---|
| D1 | Name | — | julian |
| D2 | Bus-level vs per-voice instantiation | Bus-level (mid/side, feedback and ecology all assume a summed signal). Per-voice would need envelope followers replaced by voice-envelope-derived levels. | julian |
| D3 | Ship `autoMode` = tracked and `driveRef` = input-peak? | Ship `driveRef` (it is what makes cheby/fold usable and preserves dynamics); drop tracked auto-gain unless a use case appears — inside HORDE the input level is known, so static auto-gain calibrated to nominal level covers it. Note: drive-ref is an envelope-following control; a note onset hits the curve before the follower settles. On a bus this is manageable; seed the follower from the voice envelope if per-voice. | julian |
| D4 | Output safety clamp | None in the C++ build (host has headroom). A hard ±1 clamp after DC removal reintroduced DC on asymmetric signals (battery §6, tube+bias). If a limiter is wanted, it goes *before* the final DC blocker and is documented as a nonlinearity. | horde-agent |
| D5 | Fold defaults | Tone filter LP, pre-curve, ~700 Hz, on by default when curve = fold (either slot). Alternatively an order limit that follows note frequency. Measured justification in §5. | julian |
| D6 | Ecology vs compressor | Keep; needs a programme-material A/B before it earns a front-panel slot. | julian |
| D7 | Feedback tap default | Prototype defaults to after stage 3; the tamed preset taps after stage 1 so stages 2–3 colour without being inside the loop. Recommend default = after stage 1. | horde-agent |
| D8 | Inertia primitive | Reuse HORDE's if it exists; match the overshoot table in §7. | horde-agent |
| D9 | Module presets carrying macros | Per the intent-bus decision (sub-module macros nest into the global intent busses; they never appear/disappear or float with morph). This spec lists the macro-able set: per stage drive/shape/morph/cut/floor/mix; global inertia/eco/flux/fbAmt/wet. Mapping is the intent-bus work, not this module's. | julian |
| D10 | ADAA when `floor` > 0 | Off (no closed form). Acceptable; note in the manual. | horde-agent |

---

## 9. What is the oracle for what

- **Bit-parity oracle (`./verify fast`)**: `core.js` at `os: 1`, `adaa: false`, `flux: 0`, curves 0–12 (noise excluded), all five topologies, inertia 0 and 0.5, drive-ref both modes. Generate goldens with the internal source at `src: 'sample'` fed by a known buffer. Tolerance: 1e-6 absolute (the prototype is float64 internally; if the C++ uses float32 curves, loosen to 1e-5 and record the ADR). **Do not generate parity goldens with the prototype's oversampler on** — its FIR is incidental (§10) and the C++ module runs under HORDE's global 2× instead.
- **Property oracle (`./verify full`)**: port `fidelity.js` sections as-is. The properties are the spec; the numbers are the acceptance thresholds (§11). Add `tail.js` (release-tail timing) and `preset-test.js` (every preset audible, finite, decaying) to `full`.
- **Parameter-connectivity test (battery §7b2)**: perturb every stage parameter individually and require ≥ −30 dB output change. Non-negotiable in `fast` — this class of bug (a parameter silently disconnected) survived two rounds of property tests and a visual preview that kept agreeing with the user.

---

## 10. Incidental in the prototype — do NOT replicate

- The per-stage windowed-sinc FIR oversampler (`OSn`) and its 10.5-sample-per-stage latency. HORDE's global 2× replaces it. The only latency the module should add is ADAA's half sample.
- The internal oscillator source (polyBLEP saw/square/pulse/sine, 3-saw "super", 8-voice allocator), the sample-loop source, computer-keyboard and Web MIDI input, the −80 dB voice kill, the −6 dB default output.
- The AudioWorklet / Blob / data-URL / ScriptProcessor fallback chain; the analyser visuals; the transfer-curve preview (which computes from UI params and is not evidence the audio path works — see §9).
- The 32-sample control block. Use HORDE's control rate; the interpolation requirement (2.6) is what matters, not the block size.
- The fixed-depth mod matrix and its env/LFO sources.
- The RNG seed value, the ±1 output clamp (D4), the −12…+48 dB UI range for drive (HORDE's UI decides).
- The measured auto-gain's 32-point reference sine size — any N ≥ 16 that hits the same RMS is fine; parity goldens are generated at auto = 0.

---

## 11. Acceptance criteria

- [ ] All five topologies bit-match `core.js` at the parity settings in §9.
- [ ] Bypass and `mix = 0` are bit-exact dry; parallel = 3× dry; mid/side stereo identity; multiband sum flat within ±0.15 dB (20 Hz – 20 kHz).
- [ ] Odd curves: even-harmonic energy ≤ −100 dB re odd at zero bias. Cheby integer n: ≥ 98% of harmonic energy in Hₙ. Full-wave rectify: fundamental ≤ −40 dB re H2.
- [ ] Morph = linear blend of A and B outputs, error ≤ 1e-5.
- [ ] Static auto-gain: output RMS within 0.5 dB across 0–48 dB drive for curves 0–8 (sine 0.5 in).
- [ ] Per-sample coefficient interpolation: block-boundary/in-block |Δ| ratio ≤ 1.5 under a 10 Hz LFO on drive at inertia 0.
- [ ] Silence in → silence out (exact zero) for every curve including noise, and for the feedback route at `fbAmt` 1.2.
- [ ] Two runs with the same seed are bit-identical including flux and the noise curve.
- [ ] Every stage parameter individually changes the output by ≥ −30 dB re signal (13 checks).
- [ ] Aliasing at 2× + ADAA, 4.7 kHz sine, 24 dB drive, shape 0.5: soft ≤ −45 dB, hard/overdrive/tube/diode ≤ −30 dB. Fold with LP-pre at 700 Hz ≤ −40 dB. Dirty-by-design curves: reported, not gated.
- [ ] Feedback: silence stays silent at 120%; a 5 ms loop at `fbAmt` 0.6 decays ≥ 100 dB within 240 ms at any stage drive 0–40 dB.
- [ ] Release tail (300 ms T60 note, 24 dB drive): every curve reaches −60 dB re peak within 450 ms; with `floor` at −40 dB, within 400 ms.
- [ ] Inertia step response matches §7 within ±5% overshoot.
- [ ] Every shipped preset is audible, finite and decays after release (`preset-test.js`).
- [ ] DECISIONS.md entries appended for D1–D10 as they close; ADR-1…ADR-8 (below) recorded.

---

## 12. ADR entries to record

- **ADR-1** Feedback loop normalised to unity small-signal gain at `fbAmt` = 1 (see §6).
- **ADR-2** Per-curve parameters are independent: fold = drive/shape/bias on three axes; cheby drive is intensity below the domain edge; even-n cheby intensity is `mix`.
- **ADR-3** Anti-aliasing = HORDE global 2× + ADAA on closed-form curves + pre-fold lowpass; wrap/cheby/polynomial/shards declared dirty-by-design. No OS factor above 2× anywhere.
- **ADR-4** Stage coefficients interpolated per sample between control ticks.
- **ADR-5** Auto-gain is measured (reference-sine RMS ratio), never 1/√g.
- **ADR-6** Per-stage `floor` as a nonlinearity threshold; disables ADAA on that stage.
- **ADR-7** Drive-ref implemented as `pk·f(x/pk)` (dynamics-preserving), not as argument normalisation alone.
- **ADR-8** Seeded per-instance PRNG for flux and noise; fixed shard table.

---

## 13. Findings the agent should not re-derive

The battery caught four things a parity-only approach would have preserved rather than found: static auto-gain that made drive *quieter*; a control-rate staircase under modulation; the polynomial's non-decaying tail; and a single-line comment that silently disconnected morph and bias for two revisions while the curve preview kept agreeing with the user. The last one is why §9 puts the connectivity test in `fast`. Treat the property battery as the definition of "works"; treat parity as the definition of "matches".
