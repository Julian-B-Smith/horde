# SUB OSC — Source-Module Specification

> **Status.** Ingested as a protected reference 2026-09-19 (ADR-178) on the
> human's ruling of the same day; `reference/subosc.html` (`SubOscCore`) is the
> parity oracle and an edit there is a spec change. The one sanctioned edit is
> spent: BUMP is peak-normalised per (a, φ) (R7, landed 2026-09-19). The human
> edits this table.

---

## 1. Identity and role

SUB OSC is the honest sub: **one oscillator, one voice, no swarm and no
coupling.** It sits beside the two SWARM oscillators as a third per-voice
source, and its entire value is that it is the thing they are not — a single
phase, a single amplitude, a predictable fundamental you can put under a
detuned stack without the stack's beating reaching down into the bottom octave.

It is a **source**, not a hosted FX module: it enters the routing matrix as a
source row, and it does **not** get a four-knob face (ADR-172's faces are for
hosted modules).

**Explicit non-goals.**

1. **No unison, no detune, no coupling.** That is SWARM SAW's whole ontology.
   A second SUB OSC is engine-layering at the HORDE level, not a voice count here.
2. **No filter beyond the one-pole tone.** Filtering is the shared downstream chain.
3. **No per-module envelope in the shipped build.** §5.3.
4. **No sub-specific FX, drive or saturation.** MAW is FX-C (ADR-092 amendment).

---

## 2. Signal architecture

```
  note (midi, vel) ──► pitch: octave · semitones · fine · keytrack
                                    │
                            phase accumulator
                                    │
                           one of seven shapes
        sine · triangle · square · saw · pulse(width) · noise(seeded) · bump
                  (polyBLEP on saw/square/pulse; bump needs none)
                                    │
                        × envelope × level × velocity
                                    │
                        one-pole TPT lowpass  (tone)
                                    │
                            mono → voice bus
```

- **Mono by construction.** The module writes the same sample to both channels;
  pan and width belong to the voice/mixer layer, not to a sub.
- **The envelope is applied BEFORE the tone filter**, so what rings after a note
  ends is the filter's own decay and nothing else.
- **The master phase is an INPUT**, not an internal oscillator. In the device the
  voice hands the sub oscillator 1's phase. The lab fakes it with a saw at a
  lab-only ratio; that control does not survive the port.

---

## 3. Waveforms

| id | shape | band-limiting |
|---|---|---|
| 0 | sine | analytic — exact |
| 1 | triangle | **naive** (see §9 limit L1) |
| 2 | square | polyBLEP, as the difference of two BLEP saws |
| 3 | saw | polyBLEP |
| 4 | pulse (width) | polyBLEP, both edges |
| 5 | noise | seeded mulberry32, white |
| 6 | bump | **none needed** — two partials (see below, and limit L6) |

**Bump (B155, the human's request of 2026-09-19 — ADR-177 §5 — from Mr. Bill):**

```
  y = ( sin(θ) + a·sin(3θ + φ) ) / peak(a, φ)
```

a sine with a slightly phase-shifted third harmonic above it, so one period
reads as **a big bump followed by a slightly smaller bump**. `a` = `bumpAmt`,
`φ` = `bumpPhase`, and `peak(a, φ)` is the measured maximum of
`|sin θ + a·sin(3θ + φ)|` over one period, computed once per parameter set so
that every (a, φ) peaks at exactly 1 — the ADR-178 sanctioned edit, ruling R7,
landed 2026-09-19.

- The two bumps appear when the third harmonic's crest lands inside the sine's,
  which needs `a > 1/9`; **φ breaks the symmetry between them, and its sign
  decides which comes first** — φ < 0 leads with the big one. At the defaults
  (a = 0.35, φ = −0.25 rad) the trailing lobe is **0.8823** of the leading one
  and the trough between them sits at **0.637** of the leading peak (§10.6).
- **No BLEP, and none is needed: the shape is band-limited by construction.** It
  is literally two partials, so there is nothing above 3·f0 to fold — measured
  at the additive floor, −166.8 … −163.6 dB, indistinguishable from the sine
  row (§10.1). That guarantee holds while 3·f0 < Nyquist and lapses above
  f0 = sr/6: limit **L6**.
- **Normalised by the measured peak, not by the analytic bound 1/(1+a)**:
  max|y| = 1 for every (a, φ), bought with a 282-transcendental bracketed search
  once per parameter set instead of one divide — the trade ruling **R7** made,
  because the bound left the shape up to 3 dB quieter than every other waveform.
  (§9 L7 and §10.6 still carry the pre-change measurement; re-measuring them is
  the human's.)
- Half-wave antisymmetry is structural here (`y(θ+π) = −y(θ)` for any a, φ), so
  the shape is DC-free by construction and the negative half always mirrors the
  positive pair. A "bump" asymmetric between the halves is **not** reachable
  with two odd partials and would need a different formula.

- The polyBLEP is **the repo's existing one**, copied from
  `reference/swarmsaw.html:626-633` (ported at `src/swarm_core.h:974-981`), not a
  second implementation. A second BLEP in the tree is a second thing to keep in
  agreement, and the alias numbers in §10 are only comparable to the SAW
  engine's because the correction is the same one.
- The pulse is built as `saw(ph) − saw(ph−w)`, so **both** edges carry the same
  correction at every width — the construction `src/swarm_core.h:1010-1029`
  already uses for its shape morph. The square is the w = 0.5 case and is
  therefore not a separate code path.
- Sine and triangle start at zero rising; the saw starts at −1 (the classic
  shape, and what the reference does).
- Noise ignores pitch entirely. It is in the set because a sub-range noise floor
  is a sound designer's tool, not because it is an oscillator.

---

## 4. Pitch

`freq = mtof(base + 12·octave + semitones) · 2^(fine/1200)`, where
`base = midi` if keytrack is on and **C2 (MIDI 36)** if it is off.

- Octave is **{−3, −2, −1, 0}** — a sub goes down, never up. Default −1.
  (−3 added 2026-09-20, B181 note 1; §7 records why the widening is bit-inert.)
- Semitones ±12, fine ±100 cents.
- The frequency is capped just under Nyquist so no combination of offsets can
  run the phase backwards.
- Keytrack off pins the module to C2. ← **a line to strike or re-point**; the
  alternative is a `freeHz` parameter, which is one more control for a case
  that may not exist.

---

## 5. Tone, level, envelope

### 5.1 Tone

A single one-pole **TPT (bilinear-prewarped) lowpass**, cutoff 30 Hz – 20 kHz,
additionally clamped to 0.45·sr.

The prewarping is not a refinement, it is the fix for a defect the SAW engine
still carries: the naive `1 − exp(−2π·fc/sr)` one-pole warps its magnitude
response with the sample rate — measured −1.34 dB at Nyquist at 44.1 kHz versus
−5.53 dB at 96 kHz, and 0.40 dB of audible-band difference at 10 kHz
(`docs/audits/2026-09-18-saw-engine-audit.md` §3.6, finding A7). Measured here:
§10.

### 5.2 Level, start phase, velocity

Level 0–1, and **level 0 is exact silence** (§10). Start phase 0–1 sets the
phase at note-on. Velocity scales level linearly.

### 5.3 Envelope — PROVISIONAL, expected to be deleted

The lab carries a linear AR in seconds (attack 0.5 ms – 0.5 s, release 2 ms –
2 s) **only so that notes do not click while the module has no voice around
it.** The shipped module takes the voice envelope; these two parameters are
expected to be struck from this table at the port. They are documented so that
the lab's behaviour is not mistaken for a design claim.

Linear rather than exponential on purpose: a linear ramp reaches exactly 0 and
exactly 1 in exactly the stated number of seconds at every sample rate, with no
time-constant tail to drift and no denormal floor to flush.

---

## 6. Hard sync — RETIRED 2026-09-20 (B184)

**The ruling this section used to ask for is answered by deleting the feature.**
The human, on being told the sub's Hard Sync toggle was wired off and inert:
*"Please remove hard sync from the sub. I can't imagine a scenario in which it
would be useful, and I didn't realize it had entered the spec."* So R3 ("ship
hard sync without a BLEP at the reset?") is neither yes nor no — there is
nothing to ship, and the BLEP is never priced.

What remains is the shape of the retirement, because it is load-bearing: the
`sync` parameter's **id is reserved, not reclaimed** (`subosc.sync` → id 4011).
The sub's shell ids are positional — `id − 4000` IS `SubOscCore::Param` — so
deleting the row would slide 4012…4019 down and move the host automation lanes
of parameters that do sound. The slot therefore still stores, reports and
restores a value; nothing reads it. The retirement is pinned by a test
(`subosc_check` 11e) rather than remembered: writing 4011 must leave the render
bit-identical, with a control proving the comparison can fail.

Hard sync is **not** dead as an idea for the instrument — it is queued for the
SWARM oscillators (B185). It is dead for the sub.

The section number is kept, and §7 onward are **not** renumbered: other files
cite §7, §8.1 and §10 by number, and a silent renumber would point them at the
wrong text.

---

## 7. Parameter table

Class is ADR-173's derivation, not a new column: **stepped ⇒ structural,
continuous ⇒ morphable**. Numeric ids are deliberately absent — the id layout
waits on the per-oscillator sources increment and the human's id-layout ruling.

| Address | Core key | Range / values | Default | Class | Mod | Notes |
|---|---|---|---|---|---|---|
| `subosc.wave` | `wave` | sine, triangle, square, saw, pulse, noise, bump | saw | structural | – | stepped; morphs atomically |
| `subosc.width` | `width` | 0.05 – 0.95 | 0.5 | morphable | ✓ | pulse only; 0.5 ≡ square |
| `subosc.bumpAmt` | `bumpAmt` | 0 – 0.6 | 0.35 | morphable | ✓ | bump only; the third harmonic's level `a`. Two lobes need a > 1/9; above 0.6 the harmonic dominates and it stops reading as a sub. **A shape control, not a level control** — peak normalisation (R7) holds the output at full scale across the whole range |
| `subosc.bumpPhase` | `bumpPhase` | −π – π rad | −0.25 | morphable | ✓ | bump only; the third harmonic's phase `φ`. **The sign decides which bump leads** — negative leads with the big one |
| `subosc.octave` | `octave` | −3, −2, −1, 0 | −1 | structural | – | a sub goes down only. **−3 added 2026-09-20** on the human's request (B181 note 1); the default is unchanged, so the widening is bit-inert for every stored patch. At −3 the floor is MIDI 0 → 1.02197 Hz — inaudible, but the maths is clean there (phase increment 2.3e-5 at 44.1 kHz, six orders above the f32 normal floor; the tone stage and the BUMP peak search do not depend on f0) |
| `subosc.semis` | `semis` | ±12 st | 0 | structural | – | stepped by definition |
| `subosc.fine` | `fine` | ±100 c | 0 | morphable | ✓ | |
| `subosc.level` | `level` | 0 – 1 | 0.8 | morphable | ✓ | 0 is exact silence |
| `subosc.phase` | `phase` | 0 – 1 | 0 | morphable | ✓ | the phase at note-on |
| `subosc.keytrack` | `keytrack` | on / off | on | structural | – | off ⇒ C2 |
| `subosc.tone` | `tone` | 30 – 20000 Hz | 20000 | morphable | ✓ | one-pole TPT LP |
| ~~`subosc.sync`~~ | `sync` | on / off | off | structural | – | **RETIRED 2026-09-20 (§6).** The id (4011) is reserved and must not be reused — the block's map is positional. Stored and restored; read by nothing |
| `subosc.seed` | `seed` | uint32 | 1 | structural | – | noise stream; re-drawn per note-on |
| `subosc.attack` | `attack` | 0.5 ms – 0.5 s | 5 ms | morphable | ✓ | **PROVISIONAL, §5.3** |
| `subosc.release` | `release` | 2 ms – 2 s | 80 ms | morphable | ✓ | **PROVISIONAL, §5.3** |

**The table is the only writer.** `setParam` clamps every known key and
**throws** on an unknown one. This is not defensive style, it is the fix for
audit finding A2: the SAW core's `n` has no cap anywhere but the shell's
parameter row, so every tool that drives the core directly can walk it past
`kMaxV` — `n = 33` silently corrupts memory and `n ≥ 40` segfaults.

---

## 8. House-tenet integration

1. **Intent bus / morph:** the eight morphable rows are the morph surface. The
   structural rows (waveform, octave, semitones, keytrack, seed) morph
   atomically at a corner boundary like every other structural parameter.
2. **Mod matrix:** every morphable row is a destination. The module is a matrix
   **source row** in its own right once the id layout lands.
3. **Inertia:** `fine` and `tone` are inertia-eligible. `octave`/`semis` are not
   — a mass-slewed stepped parameter is a glide with extra steps.
4. **Adaptive state:** none. Declared absent at zero cost.

---

## 9. Parity, deliberate divergences, and named limits

`reference/subosc.html` is the parity oracle for: the seven shapes, the
polyBLEP correction, the pulse construction, the pitch law, the TPT tone
coefficient, the noise stream, and the clamps.

**Deliberate divergences from the SAW lab's habits — the point of this module
being born after the audit rather than before it:**

| # | The SAW lab does | SUB OSC does | Audit |
|---|---|---|---|
| D1 | hand-tuned per-tick constants (`0.08`) | every time constant in SECONDS, converted per sample at the current rate | A1, §1.1 |
| D2 | caps only in the shell's param row | one clamped table, `setParam` throws on unknown keys | A2, §1.2 |
| D3 | an RNG stream no reset touches | mulberry32 seeded from `seed`, re-drawn at every note-on, and `allOff()` is a total reset | A3, §1.3 |
| D4 | naive one-pole, response warps with rate | TPT prewarped one-pole | A7, §3.6 |
| D5 | a per-render-call integrator (pan motion) | nothing in `render` accumulates per call | A10 |

**Named limits (pinned, not hidden — LIBRARY L0036):**

- **L1 — the triangle is naive.** No BLEP, no BLAMP. Measured floor −81.0 dB at
  MIDI 60, ~30 dB worse than the band-limited shapes and the worst row in the
  set. It is acceptable in the sub range and indefensible if this module is ever
  played at MIDI 84. The fix is a BLAMP on the two slope discontinuities.
  ← **a ruling: accept for v0, or pay for the BLAMP now?**
- **L2 — RETIRED with hard sync itself (2026-09-20, §6).** The limit was that
  the sync reset was not band-limited; there is no reset. Kept as a numbered
  entry so L3…L7 keep their names in the files that cite them.
- **L3 — noise does not respond to pitch or keytrack.** It is white, full-band,
  and only the tone control shapes it.
- **L4 — the aliasing numbers are the MODULE's, not the oscillator's.** The tone
  filter is in the measured path, because it is in the signal path.
- **L5 — the subnormal count is measured on the OUTPUT in float32.** That is the
  contract the host sees; it is not evidence about CPU stalls, which this
  machine cannot show either way (the audit records the same limit at §3.7).
- **L6 — the bump is band-limited only below f0 = sr/6.** Two partials fold
  nothing while 3·f0 < Nyquist; above 7350 Hz at 44.1 kHz the third partial
  aliases. Measured: −164.1 dB at MIDI 100 (3·f0 = 7911 Hz, still clean) and
  **−10.7 dB at MIDI 120** (3·f0 = 25116 Hz → folds to 18984 Hz). That is three
  octaves above where a sub lives, so it is accepted rather than fixed — but it
  is **pinned by a harness control that must SEE the fold** (L0036), not left as
  prose. The fix, if it is ever wanted, is to gate or fade the third partial
  above sr/6; a hard gate clicks on a sweep through the boundary, which is the
  reason it is not in v0.
- **L7 — the bump's normaliser is the analytic bound, not the true peak.**
  Dividing by 1 + a guarantees |y| ≤ 1 for every (a, φ) in one operation;
  measured over an 8 × 9 (a, φ) grid the worst peak is **0.999999**. The cost is
  the headroom it does not claim: the output sits at **0.750 of full scale at
  the defaults** and as low as 0.707 across the grid (**up to 3.01 dB** quieter
  than the other shapes, which peak at 1). Peak-exact normalisation has no
  closed form for a two-partial sum and would put a ~180-transcendental scan in
  `_recalc`, which is a parameter path the mod matrix drives. Ruling R7.

---

## 10. Acceptance — MEASURED, 2026-09-18

All numbers from `node tools/labharness/subosc_check.mjs` on the lab core at
commit-time; 37 properties, every one paired with a must-fail control that
fires. Re-measure, never relax (`specs/ACCEPTANCE.md` house rule).

### 10.1 Aliasing floor — worst inharmonic bin, dB below the fundamental

44.1 kHz, octave 0, tone wide open, Kaiser β = 19, 65536-point FFT, bins within
±12 of a harmonic excluded. Gate limits are the measurement rounded up ~5 dB.

| waveform | MIDI 24 | 36 | 48 | 60 | gate limits | no-BLEP control |
|---|---|---|---|---|---|---|
| sine | −167.2 | −164.1 | −166.7 | −164.4 | −155 (all) | not reachable |
| triangle | −116.9 | −105.0 | −92.8 | **−81.0** | −110 / −99 / −87 / −76 | not reachable |
| square | −69.6 | −63.9 | −57.6 | −51.9 | −64 / −58 / −52 / −46 | −58.7 … −40.9 |
| saw | −69.3 | −63.9 | −57.6 | −51.9 | −64 / −58 / −52 / −46 | −57.3 … −40.9 |
| pulse (w 0.3) | −67.9 | −62.4 | −56.3 | −50.3 | −62 / −57 / −51 / −45 | −55.7 … −39.5 |
| bump (defaults) | −166.8 | −163.6 | −166.5 | −164.4 | −155 (all) | naive fold: −102.8 … −67.2 |

- **Separation from the naive control is 10.7 – 12.5 dB at every row.** That is
  what polyBLEP buys and it is the honest figure; the gate sits inside a ~5 dB
  band between the measurement and the control, so it is a **regression**
  detector, not a headroom claim.
- The sine row is the **detector's own floor** (the Kaiser skirt around the
  fundamental), not the signal's — which is why it is the must-read-~zero
  control for every other row.
- **Coverage boundary, recorded rather than retried (LIBRARY L0033):** the
  no-BLEP plant moves the sine and triangle rows by **0.0 dB**, because neither
  shape goes through the correction. Those two rows have a threshold but no
  must-fail control.
- **The bump row's control is a NAIVE FOLD, not the no-BLEP plant** (which
  cannot reach it either). A sine overdriven past unity and reflected at ±1
  produces the same two-humped silhouette from a nonlinearity instead of from
  two partials, and reads **−102.8 … −67.2 dB** — 64 to 97 dB above the shape
  it imitates. Without it, "the bump sits at the additive floor" would be a
  claim about the detector, not about the construction (LIBRARY L0032).
- **L6 pinned in both directions:** MIDI 100 (3·f0 = 7911 Hz, under Nyquist)
  reads **−164.1 dB**; MIDI 120 (3·f0 = 25116 Hz) reads **−10.7 dB** at
  18984 Hz. The band-limiting guarantee is real and it has an edge, and the
  harness asserts both halves of that sentence.

### 10.2 Determinism

| property | result |
|---|---|
| two instances, same seed, 16384 samples | **bit-identical** |
| five notes of history + `allOff()` vs fresh | **bit-identical** — no stream survives the reset |
| control: different seed | differs from sample 0 |
| control: history *without* `allOff()` | differs from sample 0 (env/filter carry over, by design) |
| bump, two instances, same params, 16384 samples | **bit-identical** |
| bump, five notes of history + `allOff()` vs fresh | **bit-identical** — the shared filter/envelope state resets too |
| control: `bumpPhase` −0.25 vs −0.24 | differs from sample 0 |

The bump draws on no stream, so its determinism claim is the narrower one: the
output is a pure function of (parameters, note). Its must-differ control is the
phase, not a seed.

### 10.3 Sample-rate independence, 44.1 / 48 / 96 kHz

| quantity | 44.1 k | 48 k | 96 k | drift | gate |
|---|---|---|---|---|---|
| attack to 0.9 (attack = 50 ms) | 45.0000 ms | 45.0000 ms | 45.0000 ms | **0.0000 %** | ≤ 0.5 % |
| tone τ, fc 200 Hz | 795.67 µs | 795.68 µs | 795.75 µs | **0.0105 %** | ≤ 0.5 % |
| \|H(987.8 Hz)\|, fc 500 Hz | 0.451309 | 0.451417 | 0.452023 | **0.1581 %** | ≤ 0.5 % |

- Closed-form anchor (`g/√(g²+tan²)`, no reference implementation needed):
  worst error **0.1081 %**. Compare the SAW engine's K smoother at **54.9 %**
  drift and its output pole at 0.40 dB of audible-band change over the same
  rates.
- Control: the same attack measurement on a core with a hand-tuned per-sample
  increment instead of the seconds-derived one drifts **68.20 %**
  (40.8 / 37.5 / 18.8 ms) — i.e. the ADR-009 trap, visible.

### 10.4 Block-size independence

Chunks **1 / 7 / 64 / 256 / 333** against one whole-buffer render, 20000
samples, two legs (pulse + tone; seeded noise + tone):
**bit-identical, tolerance 0.0 exactly.** Control: a per-render-call filter
reset diverges at sample 64.

### 10.5 Silence and denormals

- Level 0, all seven waveforms, 8192 samples: **every sample exactly 0**
  (control: level 0.001 is not silent — 8192 non-zero samples).
- 2.0 s of release tail: **0 float32-subnormal samples**, last non-zero sample
  at 1951 (control: flush-to-zero removed ⇒ **584** subnormals).

### 10.6 Bump shape — MEASURED 2026-09-19

Defaults **a = 0.35, φ = −0.25 rad**, derived by rendering one period over an
(a, φ) grid and measuring the two positive lobes; the derivation is in
`traces/2026-09-19-b155-subosc-bump.md`.

| MIDI | f0 | leading lobe | trailing lobe | ratio | trough / leading peak | peak |
|---|---|---|---|---|---|---|
| 24 | 32.70 Hz | 0.75002 | 0.66175 | **0.88231** | 0.63694 | 0.75002 |
| 36 | 65.41 Hz | 0.75001 | 0.66174 | **0.88232** | 0.63697 | 0.75002 |
| 48 | 130.81 Hz | 0.74996 | 0.66173 | **0.88237** | 0.63709 | 0.75002 |
| 60 | 261.63 Hz | 0.74973 | 0.66155 | **0.88238** | 0.63752 | 0.74998 |

Gate: ratio and trough fraction within **±5 %** of 0.8823 / 0.6369 at every
note. Analytic value for the same (a, φ): **0.882307** — the core reproduces it
to five decimals, so the shape is the formula and not an approximation of it.

- **Why 0.8823 is "slightly smaller":** 1.09 dB down. Larger \|φ\| drops it fast
  (φ = −0.35 → 0.839) and smaller \|φ\| makes the difference imperceptible
  (φ = −0.15 → 0.928). **Why a = 0.35:** the trough between the bumps sits at
  0.637 of the leading peak, deep enough to read as two bumps at a glance;
  a = 0.3 gives 0.712 (a nick in one crest), a = 0.4 gives 0.566 (a deeper
  notch, and more third-harmonic buzz than a sub wants).
- **Controls.** φ = 0 must give exactly equal lobes — measured **1.00000**, so
  the measurement responds to φ rather than returning a constant. φ = +0.25 must
  **invert** the order — measured **1.13339** ≈ 1/0.8823, which is what pins
  "big bump first" to the sign of the default rather than to the indexing.
- **The lobes are read from a rising zero crossing**, not from the start of the
  render window. Without that anchor the "first" lobe is whichever one the
  buffer opened on, and the ordering claim silently inverted at MIDI 48 in the
  first draft of the measurement.
- **Normalisation, swept not spot-checked:** over an 8 × 9 (a, φ) grid the worst
  peak is **0.999999** (at a = 0, the pure sine) and the quietest is 0.707262 —
  so the bound costs up to **3.01 dB** (limit L7). Control: with the normaliser
  removed the same grid reaches **1.599994**, so "peak ≤ 1" is the normaliser's
  doing and not an artifact of a quiet shape.

### 10.7 Still to measure before a port

- CPU per voice (no budget claimed here; the SAW figures are in
  `docs/MEASUREMENTS.md`).
- Aliasing under audio-rate modulation of `fine`.
- Behaviour at 192 kHz — the harness covers 44.1 / 48 / 96 only.

---

## 11. Open rulings for the human

| # | Question | Lead's recommendation |
|---|---|---|
| R1 | Keytrack off pins to C2 (§4), or add a `freeHz` parameter? | pin to C2; one fewer control, and a free-running sub is a rare case |
| R2 | Triangle naive (L1), or pay for the BLAMP now? | naive for v0; it is the worst row but it is 30 dB down from where it would matter, and the sub range is where this module lives |
| R3 | ~~Ship hard sync without a BLEP at the reset (L2)?~~ | **ANSWERED 2026-09-20 by retirement (§6):** hard sync leaves the sub entirely, so the BLEP is never priced. The human: "I can't imagine a scenario in which it would be useful." Sync for the SWARM oscillators is B185 |
| R4 | Do `attack`/`release` (§5.3) survive the port at all? | no — strike both rows when the voice envelope is wired |
| R5 | Is `seed` a per-module parameter or does the voice seed reach it? | prefer the voice's seed; a per-module seed is one more thing a preset must carry |
| R6 | `noise` as a waveform, or is a sub-range noise source a different module? | keep it here; it costs one switch case and no state |
| R7 | The bump's normaliser: keep the analytic bound 1/(1+a) (L7, up to 3.01 dB unclaimed), or pay for a peak-exact scan in `_recalc`? | keep the bound; a level offset is one `level` turn away, while a ~180-transcendental scan sits in a path the mod matrix drives per block — and the bound is provable for every (a, φ), which a scan is only approximately |
| R8 | Are `bumpAmt`/`bumpPhase` two controls, or one "bump" macro that walks a curve through (a, φ)? | two, for the lab: the lab is where the axis is being learnt, and a macro chosen before the human has played the two axes is a guess. Revisit at the port |
