# HORDE — Formant Engine (working name: CANTO)
## Specification v0.1 — derived from browser prototype `reference/formant-pulsar-fof.html`

**Status:** prototype validated by ear; this document is the handoff to implementation.
**Prototype is the oracle.** As with the SAW engine, correctness of the plugin engine is defined as parity with the prototype's DSP core (`FormantCore` class) at matched sample rate, up to the stochastic sources (masking RNG), which must be seedable for verification.

---

## 1. Concept

A formant-synthesis engine in the FOF / pulsar family, framed in Horde terms: the fundamental is a *firing rate*, formants are *grains*, and every continuous control is a physical state with inertia rather than a value. It doubles as a bass instrument via a single interdependent "register" system that reshapes the whole engine as pitch descends.

Sound targets: vocal / choir / vowel leads; formant-shifted ("chipmunk" / "dark") voices; pulsar buzz and plastic bass; stochastic crackle and pitched stutter from grain masking.

---

## 2. Signal model

### 2.1 Grain scheduler
- One emission cycle per fundamental period. Phase accumulator `φ += f0 · dt`; on wrap, emit one grain per active formant (plus the sub grain, §4).
- Grains overlap freely; grain pool with a hard cap (prototype: 400 live grains, oldest evicted). Plugin should size the pool from worst case: `N_formants × ceil(max_grain_dur × f0_max)`.
- Grain phase starts at 0 each emission (phase-coherent with f0 → clean harmonic series).

### 2.2 Grain models (switchable, shared scheduler)
| model | envelope | duration |
|---|---|---|
| **FOF** | raised-cosine attack of length `tex`, then `exp(−π · bw · t)` | to −60 dB, capped (60 ms; sub grain 600 ms) |
| **pulsar** | Hann window over the pulsaret | `cycles / f_c` (duty cycle emerges from f0 : f_c) |

Grain signal: `amp · env(t) · sin(2π · f_c · t)`.

Parameters: `tex` 0.2–8 ms · `bw scale` 0.2–4× · `cycles` 1–12 (pulsar only).

### 2.3 Formant bank
- 5 formants (F1–F5) per voice; each with centre `f_c`, bandwidth `bw`, level `dB`.
- Vowel tables (bass register, from standard FOF tables): a e i o u; see prototype `VOWELS`.
- Output: sum of grains → `tanh(x · drive · 0.9)`.

---

## 3. Formant physics (inertia + coupling)

Each formant centre is a **mass on a spring in log-frequency**:

```
x_i   = log(f_c,i)               (state)
tgt_i = log(f_vowel,i) + shift + reg.shift
ẍ_i   = ω²(tgt_i − x_i) − 2ζω ẋ_i + K·ω²( mean(x) − x_i )
```
- `morph rate` ω/2π: 0.5–40 Hz
- `bounce` ζ: 0.05–1.5 (< 1 overshoots and rings on vowel/shift changes)
- `coupling K`: −1…+1. Positive collapses formants toward their common centre; negative splays them. (Formant-domain analogue of the SAW engine's K.)
- `formant shift`: ±24 st, added to all targets (decoupled from pitch — this is the chipmunk/dark axis).
- Integrate with semi-implicit Euler at ≤128-sample sub-steps (stability: `ω·h < ~1`).

---

## 4. Register system (single interdependent state)

One hidden state **R ∈ [0,1]** with its own spring:
```
R_target = clamp( pitchFollow · log2(220 / f0) / 3 + manual, 0, 1 )   // A3→0, A0→1
R̈ = ω_R²(R_target − R) − 2ζω_R Ṙ         // shares ζ with formant physics
```
Controls: `pitch follow` 0–1 · `manual` −1…+1 · `lag` ω_R/2π 0.3–30 Hz.

R alone derives every bass-register parameter (no separate user controls):
```
bwk        = 2^(−1.6·R)                     bandwidth scale (narrower = more resonant)
reg.shift  = −0.55·R·ln2                    all formants slide down (≈ −7 st at R=1)
reg.tilt   = 14·R                           dB added across the stack, F1→F5 (i/(N−1))
reg.tex    = 1 + R                          attack multiplier (softer down low)
reg.sub    = (0.08 + 0.9·R^0.7)·(0.55 + 0.45·bwk)   sub grain level, trimmed as bw narrows
```
**Sub grain:** a 6th FOF grain at `f_c = f0`, `tex` 2 ms, `bw` 5 Hz, cap 600 ms — rings across periods and reads as a continuous fundamental, but is subject to masking/bursts like every other grain.

Design intent: R is a *performance state*, not a preset value. Low ζ makes big downward jumps overshoot into "extra bass" and settle. R must be exposed as a mod source and destination.

---

## 5. Vowel field (XY)

- Anchors placed on the vowel chart: `i (0.15,0.15) u (0.85,0.15) e (0.22,0.55) o (0.78,0.55) a (0.5,0.9)`.
- Target table = inverse-distance blend, `w_k ∝ 1/(d⁴ + ε)`, normalised. Frequencies blended in log domain; bandwidths and dB linear.
- Regions rendered by nearest anchor; anchor glyph size/brightness = current weight.
- **Decision:** the XY point is itself a **dragged mass** (position state + velocity, spring toward pointer/MIDI target, sharing ζ), so gesture gets the same inertia as everything else.
- **Decision:** the field is a **quantum-morph surface** — stochastic flips are between anchor tables (always "valid vowels"), not arbitrary parameter states. Flip rate/probability should be one control; flip should target the anchor weights, and the formant masses then chase.

---

## 6. Masking = stochastic rhythm

Applied per grain at emission time:
- `drop prob` 0–0.95: per-formant Bernoulli drop (independent per formant → decorrelated crackle).
- `burst on / off` (1–16 / 0–16): counted in fundamental periods; emissions only during "on".
Both apply to the sub grain too. This is the first instance of the non-sequencer rhythm principle: sub-audio rhythm emerging from an audio-rate process. Later: hazard rate coupled to swarm/formant coherence.

---

## 7. Voice

- Monophonic in prototype (last-note priority) with second-order glide on f0 (`glide` 0.5–40 Hz, ζ 0.95).
- Gate envelope: linear attack (~0.0006/sample) / exponential release (`release` 10–1500 ms).
- Plugin: polyphonic; each voice owns its own formant states, R, and grain pool. Global vowel field; per-voice masses.

---

## 8. Parameter surface (summary)

| group | params |
|---|---|
| grain model | model (FOF/pulsar), tex, bw scale, cycles |
| vowel | XY (x,y), formant shift |
| physics | morph rate, bounce ζ, coupling K |
| register | pitch follow, manual, lag |
| masking | drop prob, burst on, burst off |
| voice | glide, drive, release |

Every continuous parameter above is a mod destination; **R, XY position, XY velocity, and grain-emission events** are mod sources.

---

## 9. Verification hooks
- Deterministic RNG seed for masking.
- Golden renders vs prototype at 48 kHz for: (a) sustained note per vowel, (b) vowel snap i→a at ζ 0.2, (c) octave drop A3→A1 with lag 4 Hz, (d) burst 4/4 at f0 55 Hz.
- Stability sweep: max morph rate × max lag × min ζ, blocks of 32–1024 samples.

## 10. Open items
- Formant table set beyond 5 bass vowels (soprano/tenor tables; consonant/noise formants).
- Grain waveform beyond sine (pulsaret shape morph) — cheap and in-spirit for pulsar mode.
- Whether K should act on bandwidths as well as centres.
- FX chain hand-off point (this engine → all-pass network → shaper → dispersion, see distortion engine spec).
