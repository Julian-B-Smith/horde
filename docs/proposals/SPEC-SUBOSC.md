# SUB OSC — Source-Module Specification **(DRAFT)**

> **THIS IS A DRAFT, NOT A SPEC.** It lives in `docs/proposals/` and is therefore
> NOT a protected path: strike lines, rewrite them, delete whole sections. It
> becomes protected — an edit becoming a spec change — only when the human moves
> it to `specs/SPEC-SUBOSC.md`. Until then the lab
> (`docs/design/subosc-lab.html`) is the thing that is real and this file is the
> proposal about it.
>
> **Project:** HORDE (source-module type) · **Queue item:** B155
> **Written:** 2026-09-18, from the human's request: *"a Sub Osc source module
> with simple parameters (a small suite of shapes, pitch controls, etc.)"*
> **Reference prototype:** `docs/design/subosc-lab.html` (`SubOscCore`)
> **Harness:** `tools/labharness/subosc_check.mjs` — 25 properties, hand-run
> **Status:** proposal. Nothing here is ratified and nothing is in `src/`.

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
    master phase (osc 1) ──► hard sync (optional) ──► phase accumulator
                                    │
                            one of six shapes
              sine · triangle · square · saw · pulse(width) · noise(seeded)
                          (polyBLEP on saw/square/pulse)
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

- Octave is **{−2, −1, 0}** — a sub goes down, never up. Default −1.
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
phase at note-on and is also the value hard sync resets to. Velocity scales
level linearly.

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

## 6. Hard sync

When enabled and a master phase is supplied, a **wrap in the master phase resets
the oscillator phase to the start-phase value** (not to 0 — the same convention
as `specs/SPEC-STATION.md` §3.4, so a start-phase control still means something
under sync).

**Named limit (L2):** the reset discontinuity is **not** band-limited in v0.
Sync is the classic aliasing generator and a correct treatment needs a BLEP at
the reset instant, whose amplitude is the step the reset causes. It is left out
because the honest sub's job is the bottom octave, where sync is rare; the cost
is unmeasured and the harness does not currently cover it. ← **a ruling: ship
without, or price the BLEP before the port?**

---

## 7. Parameter table

Class is ADR-173's derivation, not a new column: **stepped ⇒ structural,
continuous ⇒ morphable**. Numeric ids are deliberately absent — the id layout
waits on the per-oscillator sources increment and the human's id-layout ruling.

| Address | Core key | Range / values | Default | Class | Mod | Notes |
|---|---|---|---|---|---|---|
| `subosc.wave` | `wave` | sine, triangle, square, saw, pulse, noise | saw | structural | – | stepped; morphs atomically |
| `subosc.width` | `width` | 0.05 – 0.95 | 0.5 | morphable | ✓ | pulse only; 0.5 ≡ square |
| `subosc.octave` | `octave` | −2, −1, 0 | −1 | structural | – | a sub goes down only |
| `subosc.semis` | `semis` | ±12 st | 0 | structural | – | stepped by definition |
| `subosc.fine` | `fine` | ±100 c | 0 | morphable | ✓ | |
| `subosc.level` | `level` | 0 – 1 | 0.8 | morphable | ✓ | 0 is exact silence |
| `subosc.phase` | `phase` | 0 – 1 | 0 | morphable | ✓ | note-on phase **and** the sync reset target |
| `subosc.keytrack` | `keytrack` | on / off | on | structural | – | off ⇒ C2 |
| `subosc.tone` | `tone` | 30 – 20000 Hz | 20000 | morphable | ✓ | one-pole TPT LP |
| `subosc.sync` | `sync` | on / off | off | structural | – | needs a master phase |
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

1. **Intent bus / morph:** the six morphable rows are the morph surface. The
   structural rows (waveform, octave, semitones, keytrack, sync, seed) morph
   atomically at a corner boundary like every other structural parameter.
2. **Mod matrix:** every morphable row is a destination. The module is a matrix
   **source row** in its own right once the id layout lands.
3. **Inertia:** `fine` and `tone` are inertia-eligible. `octave`/`semis` are not
   — a mass-slewed stepped parameter is a glide with extra steps.
4. **Adaptive state:** none. Declared absent at zero cost.

---

## 9. Parity, deliberate divergences, and named limits

`docs/design/subosc-lab.html` is the parity oracle for: the six shapes, the
polyBLEP correction, the pulse construction, the pitch law, the TPT tone
coefficient, the sync reset convention, the noise stream, and the clamps.

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
- **L2 — hard sync is not band-limited.** §6.
- **L3 — noise does not respond to pitch or keytrack.** It is white, full-band,
  and only the tone control shapes it.
- **L4 — the aliasing numbers are the MODULE's, not the oscillator's.** The tone
  filter is in the measured path, because it is in the signal path.
- **L5 — the subnormal count is measured on the OUTPUT in float32.** That is the
  contract the host sees; it is not evidence about CPU stalls, which this
  machine cannot show either way (the audit records the same limit at §3.7).

---

## 10. Acceptance — MEASURED, 2026-09-18

All numbers from `node tools/labharness/subosc_check.mjs` on the lab core at
commit-time; 25 properties, every one paired with a must-fail control that
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

### 10.2 Determinism

| property | result |
|---|---|
| two instances, same seed, 16384 samples | **bit-identical** |
| five notes of history + `allOff()` vs fresh | **bit-identical** — no stream survives the reset |
| control: different seed | differs from sample 0 |
| control: history *without* `allOff()` | differs from sample 0 (env/filter carry over, by design) |

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
samples, two legs (pulse + tone + hard sync; seeded noise + tone):
**bit-identical, tolerance 0.0 exactly.** Control: a per-render-call filter
reset diverges at sample 64.

### 10.5 Silence and denormals

- Level 0, all six waveforms, 8192 samples: **every sample exactly 0**
  (control: level 0.001 is not silent — 8192 non-zero samples).
- 2.0 s of release tail: **0 float32-subnormal samples**, last non-zero sample
  at 1951 (control: flush-to-zero removed ⇒ **584** subnormals).

### 10.6 Still to measure before a port

- CPU per voice (no budget claimed here; the SAW figures are in
  `docs/MEASUREMENTS.md`).
- Aliasing under hard sync (L2), and under audio-rate modulation of `fine`.
- Behaviour at 192 kHz — the harness covers 44.1 / 48 / 96 only.

---

## 11. Open rulings for the human

| # | Question | Lead's recommendation |
|---|---|---|
| R1 | Keytrack off pins to C2 (§4), or add a `freeHz` parameter? | pin to C2; one fewer control, and a free-running sub is a rare case |
| R2 | Triangle naive (L1), or pay for the BLAMP now? | naive for v0; it is the worst row but it is 30 dB down from where it would matter, and the sub range is where this module lives |
| R3 | Ship hard sync without a BLEP at the reset (L2)? | yes for v0, with L2 recorded; a sync BLEP is worth a queue row of its own |
| R4 | Do `attack`/`release` (§5.3) survive the port at all? | no — strike both rows when the voice envelope is wired |
| R5 | Is `seed` a per-module parameter or does the voice seed reach it? | prefer the voice's seed; a per-module seed is one more thing a preset must carry |
| R6 | `noise` as a waveform, or is a sub-range noise source a different module? | keep it here; it costs one switch case and no state |
