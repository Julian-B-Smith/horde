# Wavetable S0: the five engine-free items

> **Origin.** horde lead session, 2026-11-09, ROADMAP B665: the five pending engine-free items of
> `docs/wavetable/CONTRACT.md` §5.6 (S0), after the spec rework B663 (#1484). Computed in numpy, double precision,
> deterministic, by the scripts in `tools/wavetable_s0/` (one per item; `python3 -I tools/wavetable_s0/<name>.py`
> prints every number quoted here). Nothing here edits a spec: changes the numbers suggest are listed at the end as
> **proposed changes** for the lead.

| # | item | verdict |
|---|---|---|
| 1 | J2's delayed read (WT-24, WT-F42) | **Holds.** DT-3 within 0.17 dB on every case (the bound is 0.5). The restart is as band-limited as ED-1's filter E gives, within 0.4 dB. B-51's control fails by 2 to 40 dB. **Three findings:** WT-F42's own ratios carry no step on these tables; τ must equal −Ā to about 0.001 sample; and the step-and-corner correction reaches only 51 to 71 dB by QS-1's meter on bright tables. |
| 2 | WT-F2's design figures | **Re-derived exactly:** C6 −6.84, −5.30, −5.30 and −2.54 dB at 44.1 kHz. The band is empty at 36 semitones at one level per octave, and 21 at half. The quarter-octave worst is −7.56 dB and the eighth-octave worst −2.53. |
| 3 | The loop stability map, and I_back | **Mapped.** With the table read, the loop is stable up to loopDrive 0.62 on Sine → saw frame 0, 0.34 on frame 15 and 0.14 on Vowel frame 7, taking the lowest pitch-by-pitch limit across C1 to C6. The default 0.3 runs away on Vowel at C6. The 1 ↔ 2 loop with equal depths contracts up to 2.75, so **I_back = 2.5 is proposed** (the spec holds 4). |
| 4 | BB-41's worst-case recipe | **2,081 bytes (2.03 KiB)** on the real 68 rows, under 3 KiB. It is 2,337 bytes with the engine's own 512-byte `ratios` limit. Eight such recipes take 16.4 KiB of the 32 KiB patch bound. **BB-F8's plant as written does not fire:** named keys come to 2,828 bytes. |
| 5 | WT-6's read designs | **Re-derived exactly.** 6-point Lagrange reads the 140 dB cap at every pitch. Linear at 2048 fails at C2 and C4 (the must-fail control). Additive reads the cap (the clean control). The drafter's "4-point cubic" is Catmull-Rom; 4-point Lagrange is 15 to 20 dB better. |

---

## 1. J2's delayed read

**The law rendered** (`j2_delayed_read.py`). One member, one level, s = 0, `bandLimit` 1.
- **The read:** the table is read at θ − τ·f/f_int, with no filter E.
- **The step residual:** each restart adds Δ·(s(x) − [x ≥ τ]). x is the time since the *undelayed* restart. The indicator is taken from the delayed read itself, so the indicator and the read agree on which sample is after the step.
- **The corner residual:** Δσ·(x + Q(x) − (x − τ)₊).

Here s is ED-1's band-limited step, built exactly as the clean-room engine builds it (`core/edges/tables.cpp`: 0.47·f_s, Kaiser β 8.3, 32 taps, 128 grid points per tap, the engine's cubic read), and **τ = −Ā = 2.489797 samples**. Δ = y(0) − y(frac r) and Δσ = (f/f_int)·r·(y′(0) − y′(frac r)), each read at the actual read phases (WT-24).

The reference is the **ideal additive synced read**: the exact Fourier series of one member cycle of y(r·frac θ), with harmonics below 20 kHz.

The other designs rendered beside it:

| design | what it is |
|---|---|
| `E` | ED-1 as written: E applied to the undelayed read, plus r₂ and q₂ |
| `nodelay` | **the must-fail control** (B-51): steps corrected by s − 1 on an undelayed read; curved corners uncorrected, the pre-round-42 law |
| `nodelay+q` | the same, with the corner residual Q(x). This is WT-F42's mutation literally: the read's τ removed |
| `naive` | no correction |

At C4 the level is H = 16 for every case, at every rate. No case is inside WT-5's crossfade band, so DT-3 sees the restart law alone.

### DT-3 (WT-F42)

The table gives the largest level difference between 44.1, 48 and 96 kHz over harmonics 1 to 16 below 18 kHz, by DT-3's method on a held C4. The bound is 0.5 dB.

| case | Δ (step) | delayed | E | nodelay | nodelay+q | naive | delayed vs ideal, worst harmonic |
|---|---|---|---|---|---|---|---|
| Sine → saw 15, r = 2.5 | 0.000 | **0.032** | 0.000 | 0.000 | 18.85 | 0.000 | 0.035 |
| Sine → saw 15, r = 3.5 | 0.000 | **0.042** | 0.000 | 0.000 | 18.92 | 0.000 | 0.045 |
| Sine → saw 15, r = 2.75 | −0.408 | **0.142** | 0.000 | 7.53 | 26.39 | 0.024 | 0.156 |
| Sine → saw 15, r = 3.25 | +0.408 | **0.068** | 0.000 | 3.97 | 21.29 | 0.039 | 0.076 |
| Vowel 7, r = 2.5 | 0.000 | **0.143** | 0.001 | 0.001 | 14.60 | 0.001 | 0.116 |
| Vowel 7, r = 3.5 | 0.000 | **0.054** | 0.001 | 0.000 | 24.04 | 0.000 | 0.057 |
| Vowel 7, r = 2.75 | +0.109 | **0.091** | 0.000 | 3.22 | 40.38 | 0.006 | 0.100 |
| Vowel 7, r = 3.25 | −0.109 | **0.166** | 0.001 | 2.00 | 22.53 | 0.011 | 0.190 |

- **The delayed read holds DT-3 on every case:** worst 0.166 dB, with 0.33 dB to spare.
- **It matches the ideal synced read's first 16 harmonics** within 0.19 dB at all three rates.
- E holds DT-3 more tightly (0.001 dB). E matches the step's whole phase response, while a delay matches only its linear part.

**Finding 1a: WT-F42's ratios carry no step on these tables.** Sine → saw and Vowel are sine series. At frac r = 0.5, the restart jumps between two zero crossings, so Δ = 0 exactly and the restart is a corner only. At those ratios the `nodelay` control equals the naive read (0.000 dB), and B-51's step mismatch never shows. 2.75 and 3.25 were added so that a step is tested. There `nodelay` fails by 2.0 to 7.5 dB, and `nodelay+q` fails by 14.6 to 40.4 dB on every ratio.

**The must-read-zero control** is r = 1 and r = 2. Δ and Δσ are exactly 0, so the corrections are skipped as a branch. The output equals the plain synced read to 8.5 × 10⁻¹³: the residue of evaluating the same sum by two routes, with no correction term. The DT-3 spread is 0.0000 dB on both tables.

### A band-limited restart: QS-1's meter

The meter runs at 44.1 kHz, on an odd whole number of cycles in 65,536 samples, capped at 140 dB. Values are in dB.

| case | delayed | E | nodelay | nodelay+q | naive | ideal |
|---|---|---|---|---|---|---|
| Sine → saw 15, C4, r = 2.5 / 3.5 / 2.75 / 3.25 | 70.5 / 60.9 / 67.4 / 63.3 | 70.9 / 61.3 / 67.8 / 63.7 | 43.2 / 40.0 / 42.0 / 41.2 | 24.3–24.6 | 43.2 / 40.0 / 33.3 / 33.6 | 140 |
| Sine → saw 15, C6 (H = 4) | 65.2 / 55.8 / 59.6 / 56.2 | 65.4 / 56.0 / 59.8 / 56.4 | 37.0 / 33.8 / 35.5 / 35.4 | 20.2–21.4 | 37.0 / 33.8 / 28.0 / 28.2 | 140 |
| Vowel 7, C4 | 60.7 / 50.7 / 55.6 / 51.8 | 61.1 / 51.1 / 56.0 / 52.2 | 38.3 / 34.7 / 37.2 / 36.2 | 24.7–24.8 | 38.3 / 34.7 / 36.1 / 35.3 | 140 |
| Vowel 7, C6 | 68.5 / 58.9 / 61.5 / 58.2 | 68.7 / 59.2 / 61.7 / 58.4 | 43.5 / 40.1 / 45.5 / 43.9 | 21.7–24.5 | 43.5 / 40.1 / 43.6 / 42.4 | 140 |
| Sine → saw 0 (a sine), C4 | **126.0 / 117.2** / 97.7 / 94.8 | 126.3 / 117.6 / 97.9 / 95.1 | 59.6 / 56.8 / 64.5 / 63.1 | 33.4–40.8 | 59.6 / 56.8 / 29.8 / 29.8 | 140 |

- **The delayed read is as band-limited as E**, within 0.4 dB in every case, and 15 to 40 dB cleaner than the uncorrected read.
- **Finding 1b: the step-and-corner correction does not reach QS-1's floors on bright tables.** On Sine → saw 15 and Vowel 7 it reaches 51 to 71 dB, against floors of 104 at C4 and 98 at C6. A sine table reaches 126 dB at C4.
  - The cause is the restart's higher-order discontinuities (curvature and above), which WT-24 does not correct.
  - They are large on a level whose top harmonic sits near c/2 to c, because the cycle is not smooth at the sample scale.
  - **This is true of E too, so it is not a cost of J2.** It bears on S5a's "sync's edges by QS-1's method at r = 2.5": against QS-1's floors, that gate would fail with any law in the spec. See proposed change P3.

### WT-25: the corrections' mean (does τ = −Ā zero it?)

**Yes, on average; the number is +7.1 × 10⁻⁵.** The step residual's area, averaged over the restart's fraction δ, is Ā + τ, which is 0 by construction. Numerically it reads +7.1 × 10⁻⁵ sample per unit step on a 4,096-point δ grid; E's own reads −1.2 × 10⁻⁴.

- **Per restart it is not zero:** it ranges from −0.50 to +0.50 with δ. Over a held note the δ values spread out and the term averages away.
- **The corner residual's mean** is +0.0026 (E's is +0.0001). Nothing pins it, but it is small.
- **Measured in the render** (1 s at 44.1 kHz), the corrections add a mean of −1.8 × 10⁻⁵ to +1.9 × 10⁻⁵. That is −84 to −91 dB re the output's RMS, far under QS-6's 0.003 (about −50 dB).
- **So WT-25's "plus the restart corrections' own mean" term is not needed under the delayed read,** as ED-3 found under E (P4).
- **This needs τ exact.** With τ rounded to 2 samples, the step mean is −0.49 per restart and the corner mean −14.6: a real offset.

### Sensitivity to what the spec leaves open

Each cell is the DT-3 spread (dB) / QS-1 at C6 (dB). The first four columns are Sine → saw 15, the last four Vowel 7.

| variant | 2.5 | 3.5 | 2.75 | 3.25 | 2.5 | 3.5 | 2.75 | 3.25 |
|---|---|---|---|---|---|---|---|---|
| as built (τ = −Ā, 32 samples) | 0.032/65.2 | 0.042/55.8 | 0.142/59.6 | 0.068/56.2 | 0.143/68.5 | 0.054/58.9 | 0.091/61.5 | 0.166/58.2 |
| τ + 0.001 | 0.030/65.0 | 0.037/55.7 | 0.134/59.5 | 0.079/56.1 | 0.284/68.4 | 0.279/58.9 | 0.082/61.5 | 0.271/58.2 |
| τ + 0.01 | 0.246/59.9 | 0.446/53.8 | 0.279/57.0 | 0.285/54.4 | **3.48**/65.2 | **3.06**/57.8 | **0.71**/61.0 | **1.25**/57.7 |
| τ ± 0.25 | 5.2–12.1 / 31–34 | | | | 8.4–25.7 / 38–43 | | | |
| τ = 2 (an integer delay line) | 8.86/28.7 | 14.60/26.0 | 10.08/28.2 | 23.89/26.7 | 23.06/34.8 | 17.88/32.0 | 19.16/36.8 | 18.05/35.4 |
| residuals cut to 24 samples | 0.032/65.2 | 0.042/55.8 | 0.142/59.6 | 0.068/56.2 | 0.147/68.5 | 0.054/58.9 | 0.091/61.5 | 0.164/58.2 |
| residuals cut to 16 samples | 0.033/62.4 | 0.034/54.8 | 0.136/58.3 | 0.090/55.3 | 0.110/66.8 | 0.051/58.3 | 0.091/61.2 | 0.180/57.9 |
| QS-1's reference table (0.4725, β 8; τ = 2.370) | 0.031/65.2 | 0.039/55.8 | 0.136/59.6 | 0.064/56.2 | 0.131/68.5 | 0.052/58.9 | 0.085/61.5 | 0.152/58.2 |

**Finding 1c: τ must be the very table's −Ā, to about 0.001 sample.** The corner residual's tail ends only at Q(32) + τ = 0. An error ε leaves an uncorrected step of ε·Δσ, 32 samples after every restart.
- At ε = 0.01 the Vowel cases fail DT-3, and an integer delay line fails badly.
- In double precision the engine computes τ as E's pinned moment already, so this costs nothing. But it rules out a rounded or float-truncated τ, and τ taken from a different table design.
- The residual's length (24 or 32 samples) and the table design (the engine's or QS-1's) barely matter.

---

## 2. WT-F2's design figures

This is the 16 to 20 kHz energy of a saw read through WT-4's levels and WT-5's crossfade, against an ideal saw (every harmonic below 20 kHz), in dB (`wtf2_top_band.py`).

| spacing | 44.1 kHz: C6 | C6 without crossfade | C2–B5 worst | semitones with an empty band | median | 48 kHz: C6 | C2–B5 worst | empty |
|---|---|---|---|---|---|---|---|---|
| one per octave | **−6.84** | −5.30 | −∞ | 36 of 48 | −9.32 | −6.46 | −∞ | 32 |
| half octave | **−5.30** | −5.30 | −∞ | 21 | −6.30 | −5.30 | −∞ | 16 |
| quarter octave | **−5.30** | −5.30 | **−7.56** | 0 | −3.01 | −5.28 | −7.56 | 0 |
| eighth octave | **−2.54** | −2.54 | **−2.53** | 0 | −1.20 | −2.31 | −2.48 | 0 |

- **The drafter's figures stand to the hundredth:** −6.84, −5.30, −5.30, −2.54, and −7.6 and −2.5 across the semitones.
- The empty notes at one per octave are D♯ to B in every octave. The band empties when 16 kHz falls above a level's top harmonic.
- **Control:** the ideal saw against itself reads 0.00 dB.
- **Detector plant (D):** a saw with its top four harmonics below 20 kHz removed reads −∞ at C6 and −1.11 at C4. It fires.
- **Mutation plant (M):** with the crossfade disabled, C6 at one per octave reads −5.30 against the design's −6.84, outside 0.5 dB. It fires.

---

## 3. The loop stability map (WT-31) and I_back (WT-65)

**The method is B521's** (`loop_stability_map.py`, SPEC-ENGINE BL-12's Measure).
- **The set-up:** 44.1 kHz, one member (its own neighbour), `crossMod` 1 and `feedback` 1, with xin added to the read phase by C.14's law (WT-31). The tap is d·y⁰, with C.16's estimate at 0, since a table has no DC.
- **The perturbation:** a twin's `feedback` steps to 0.999 at 1 s and back at 1.01 s, through PA-1.
- **The verdict:** a setting contracts if the twins' difference over 1.5 to 2.5 s is below 10⁻⁶ of the output RMS, as rendered or shifted by k·P (0 < |k| < 12).
- **The grid:** pitches C1 to C6 (P = 1349 … 42) and loopDrive 0.14 to 1.00 in steps of 0.01. Each table plays at its WT-4 level, with WT-16's loudness gain g (0.872 for frame 0, 1.178 for frame 15, 1.498 for Vowel 7).

### (A) The self-loop on a table member

Each cell is the largest loopDrive with every grid point from 0.14 up contracting. The count of contracting points above the first failure is in brackets (windows, as BL-12 reports for the blades).

| table | C1 | C2 | C3 | C4 | C5 | C6 | lowest across C1–C6 |
|---|---|---|---|---|---|---|---|
| Sine → saw 0 (sine) | 1.00 | 1.00 | 1.00 | 0.73 (11) | 0.64 (0) | 0.62 (0) | **0.62** |
| Sine → saw 15 (saw) | 0.44 (7) | 0.44 (7) | 0.35 (13) | 0.35 (7) | 0.34 (11) | 0.34 (15) | **0.34** |
| Vowel 7 | 1.00 | 1.00 | 0.80 (17) | 0.64 (7) | 0.40 (2) | **0.14** (1) | **0.14** |

- **Controls:**
  - Depth 0 (xin = 0): every pitch and table reads exactly 0. Stable, as it must be.
  - The saw table at loopDrive 1 reads a twin difference of 1.4 × the output RMS at every pitch. Unstable, as it must be.
- **For D2:**
  - The default loopDrive 0.3 stays contracting on Sine → saw (both frames) at every pitch mapped.
  - On **Vowel 7 at C6 it runs away at the default**. Its limit is 0.14, the row's floor, so at C6 no setting above the floor contracts on this table.
  - The saw table's limit (0.34 from C3 up) sits under the blades' saw carrier (0.50, BL-12). The table's loudness gain and its band-limited slope steepen the loop.
  - So BL-12's map does not transfer, as WT-31 says: the human's D2 call needs these per-table limits.

### (B) The 1 ↔ 2 loop on Sine → saw frame 0, and I_back

Osc 2 renders first and reads osc 1 one sample late, averaged (the back-edge, depth D_b). Osc 1 then reads osc 2 at the same sample (the forward edge, D_f). The PM is BL-5's (D/2π)·m, with m = g·T. Both oscillators play the same pitch. Each cell is the largest D_b contracting from 0.25 up, in steps of 0.25.

| forward depth | C1 | C2 | C3 | C4 | C5 | C6 | lowest |
|---|---|---|---|---|---|---|---|
| D_f = D_b (equal indices) | 10.0 | 9.5 | 8.75 | 3.5 | 3.25 | **2.75** | **2.75** |
| D_f = 1 | 10.0 | 10.0 | 10.0 | 7.25 | 6.25 | 4.75 | 4.75 |
| D_f = 2.5 | 4.75 | 4.0 | 3.75 | 3.25 | 3.25 | 3.0 | 3.0 |
| D_f = 5 | 1.5 | 1.75 | 1.5 | 1.25 | 1.0 | 0.75 | 0.75 |
| D_f = 10 | 3.5 | 2.5 | 1.5 | 1.25 | 1.0 | 1.0 | 1.0 |

- **WT-65 defines I_back as the largest index at which the 1 ↔ 2 loop still contracts.** Read with one index on both edges, that is **2.75**, set by C6.
- **The spec's 4 is above that limit at C4, C5 and C6.**
- **Recommendation:** I_back = 2.5. That is one grid step under the map's 2.75, as BL-12 keeps 0.05 under its limits (P5).
- **What the clamp cannot do:** the forward edge is unclamped, so a forward depth of 5 to 10 makes the loop run away from D_b of about 0.75 to 1.0. No back-edge clamp that keeps the musical range open can prevent that.
- The output stays bounded (WT-65), and the human hears it in `s5b-4-feedback`.
- The map stops at C6 (B521's range). C7 and C8 are not mapped, and on this trend they would sit lower.

---

## 4. BB-41's worst-case recipe size

**The rows are read from the clean-room engine's manifest at e72a092** (`bb41_recipe_size.py`, read-only):
- the six blade-only oscillator rows (ids 0x23, 0x28 to 0x2c);
- blade 1's 30 rows (0x100 to 0x11d);
- blade 2's 32 rows (0x200 to 0x21f).

That makes **68**, as CONTRACT §2.4 says: 41 continuous and 27 stepped.

**The text is §3.1's canonical form:** keys sorted, no whitespace, ECMAScript's shortest numbers. The values are worst cases:
- **a continuous row:** any v in [0, 1]. The longest is 24 characters (17 digits in [10⁻⁶, 10⁻⁵): `0.0000012345678901234567`);
- **a stepped row:** its longest index fraction (`0.16666666666666666`);
- **the seed:** uint32's 4294967295;
- **the ratios text:** the spec's 256 bytes;
- **the frame axis:** its largest form;
- **fRef:** 261.63.

| what | bytes | against |
|---|---|---|
| one row, worst (key, colon, value) | 30 | BB-41's "about 32" |
| all 68 rows | 1,570 | BB-41's "about 2.2 KiB" |
| **the worst-case recipe** | **2,081 (2.03 KiB)** | 3 KiB (3,072): **under**, with 991 to spare; BB-41 estimated about 2.6 KiB |
| the same with the engine's own `ratios` limit, 512 bytes | 2,337 | under 3 KiB |
| the same with the seed row's own range (0 to 999,999) | 2,077 | |
| a patch of 8 worst-case bake recipes, in the `tables` dictionary | 16,809 (16.4 KiB); 18,857 with 512-byte `ratios` | 32 KiB: **under**, room for 27 to 31 more 512-byte recipes |
| **detector plant (D), BB-F8:** named keys (`blade1.cutSpread`) | **2,828** canonical; 3,089 pretty-printed | 3 KiB: **does not fire** canonically |
| control: the base wave only | 97 | |

**Choices the format leaves open, each a few bytes:**
- lowercase hex without `0x`;
- the base wave written as its row `23`;
- `"ratios"` and `"fRef"` as keys;
- the frame axis as `{"row","start","swing"}`.

**Finding 4a: BB-F8's plant does not fire.** Named keys in canonical form come to 2,828 bytes, under 3 KiB, so the plant as written would read green. The critic's 2.9 to 3.1 KiB estimate assumed values of 22 characters on every row. P6 proposes a 2.5 KiB bound, which the worst case meets with 479 bytes to spare (223 with 512-byte `ratios`) and which the named-key plant exceeds.

**Finding 4b: the `ratios` bound differs between the spec and the engine.**
- **The spec:** IF-12 and MO-13's text say 256 bytes.
- **The engine:** it accepts 512 (`kRatiosMaxBytes = 512`, `engine2/tests/harness/horde_engine.h`).
- A patch the engine accepted could carry 512 bytes into the bake. It still fits.

**Finding 4c: two number forms.**
- **§3.1's canonical form** is ECMAScript's. It writes 1.2345678901234567 × 10⁻⁶ as `0.0000012345678901234567` (24 characters) and 10⁻⁷ as `1e-7`.
- **The engine's state writer** (`core/state/json.cpp`, `shortest`) uses printf's `%.*g`. It writes the same values as `1.2345678901234567e-06` (22 characters) and `1e-07`.
- Recipe ids hash the canonical text, so the bake must not reuse the state's writer. BB-41's "up to 22 characters" is the printf form.

---

## 5. WT-6's read designs

The table is a 1/n saw at the level WT-4 picks (one per octave, no crossfade). The measure is QS-1's method at 44.1 kHz (`wt6_read_alias.py`), in dB, capped at 140. N, the samples per cycle, is in brackets.

| note | H | 6-pt Lagrange | 4-pt Lagrange [32H] | 4-pt Catmull-Rom [32H] | 8-pt Lagrange | 2-pt linear [2048] | additive | floor |
|---|---|---|---|---|---|---|---|---|
| C1 | 512 | **140.0** [8192] | 135.7 [16384] | 118.1 | 128.1 [4096] | 54.3 | 140.0 | reported |
| C2 | 256 | **140.0** [4096] | 134.9 [8192] | 115.3 | 140.0 [4096] | **63.6** | 140.0 | 110 |
| C4 | 64 | **140.0** [2048] | 129.7 [2048] | 109.4 | 140.0 [2048] | **82.7** | 140.0 | 104 |
| C6 | 16 | **140.0** [2048] | 140.0 | 137.7 | 140.0 | 99.5 | 140.0 | 98 |
| C8 | 4 | **140.0** [2048] | 140.0 | 140.0 | 140.0 | 115.9 | 140.0 | 93 |

- **The drafter's figures stand.** Linear reads 54, 64, 83, 100 and 116. The cubic reads 118, 115, 109, 138 and 140, which is Catmull-Rom. The 6-point reads the cap at C1, C2 and C4. The 8-point reads 128 at C1 and the cap at C2 and C4.
- **Memory per one-per-octave level set:** 144 KiB for the 6-point (largest level 32 KiB) and 112 KiB for the 8-point (largest 16 KiB).
- **The must-fail control fires:** linear at 2048 is below the floor at C2 and C4.
- **The must-read-clean control holds:** additive reads the cap everywhere.
- **Every other design meets every floor.**
- **Finding 5a: "4-point cubic" is ambiguous.** As 4-point Lagrange it reads 129.7 to 140 dB, 15 to 20 dB above Catmull-Rom (the cubic the engine's edge tables use). If WT-6 keeps the cubic as a fallback, it should name the kernel (P7).

---

## Proposed changes, for the lead

These are not applied. Each cites its evidence above.

- **P1 (WT-24, CONTRACT §7 J2):** take J2's delayed read to the human as proven engine-free. It holds DT-3 within 0.17 dB on WT-F42's cases and on step-bearing ratios. It is as band-limited as E (within 0.4 dB by QS-1's meter). It switches nothing.
- **P2 (WT-F42):** add a ratio whose restart has a step, for example **r = 2.75 and 3.25**, to 2.5 and 3.5. On sine-series tables a ratio with frac r = 0.5 restarts between zero crossings, so the step part of B-51 is untested (§1, finding 1a). The mutation plant (the τ delay removed) fires on both kinds, by 14.6 to 40.4 dB.
- **P3 (WT-37, CONTRACT §5.6 S5a, WT-F3):** with WT-24's step-and-corner correction, a synced bright table reads 51 to 71 dB by QS-1's meter at C4 and C6, below QS-1's floors. That holds under E as well. S5a's "sync's edges by QS-1's method at r = 2.5" should be **reported** (QS-3's rule), or judged against a measured floor. QS-1's floors need a correction of the restart's higher-order discontinuities, which the spec does not have. Note that a curvature residual's tail ends only if a second moment is also pinned, which a pure delay cannot do (§1, finding 1b).
- **P4 (WT-25):** drop "plus the restart corrections' own mean" for the delayed read. τ = −Ā zeroes the step residual's δ-averaged area (+7.1 × 10⁻⁵). The measured mean of the corrections is −84 dB re the output RMS or lower. Add instead: **τ must be the read table's own −Ā, to within 10⁻³ sample** (§1, finding 1c).
- **P5 (WT-65, CONTRACT §7 H6):** set **I_back = 2.5**. The 1 ↔ 2 loop with equal indices contracts up to 2.75 at C6. The spec's 4 runs away at C4 to C6. State that a forward depth of 5 to 10 runs the loop away at a back-edge index of 0.75 to 1.0 whatever the clamp, and that the output stays bounded (§3 B).
- **P6 (BB-41, BB-F8, CONTRACT §3.2):** set the `blade-bake` recipe bound to **2.5 KiB**. The worst case is 2,081 bytes, or 2,337 with the engine's 512-byte `ratios`. Then BB-F8's named-key plant (2,828 bytes) fires. At 3 KiB it reads green (§4, finding 4a). Also record:
  - **the `ratios` limit:** the spec says 256 bytes and the engine accepts 512;
  - **the number form:** §3.1's ECMAScript form is not the state writer's printf form.
- **P7 (WT-6):** name the cubic fallback's kernel. 4-point Lagrange at 32·H reads 129.7 dB or more at every pitch; Catmull-Rom reads 109.4 at C4.
- **For D2 (CONTRACT §7 D2, WT-31), not a number change.** The table member's loop limits are 0.62 for Sine → saw 0, 0.34 for Sine → saw 15 and 0.14 for Vowel 7, the lowest of each table's pitch-by-pitch limits. The default 0.3 runs away on Vowel at C6. These go to the human with D2.

## What remains

- **Bank control for BB-F8:** the bank's own bake recipes were not computed. They need the bank translated through the engine's state; this is S6c's control.
- **The loop above C6, and at High:** not mapped.
- **C.16's estimate in the loop tap:** taken as 0. It is not 0 once sync or the warp runs.
- **WT-F2 under the pitch crossfade at other placements:** WT-5's "upper third" is PROPOSED, and only that placement was computed.
