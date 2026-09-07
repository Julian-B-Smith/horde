# HORDE — Distortion Engine (working name: WARP)
## Specification v0.1 — derived from browser prototype `reference/distortion-engine.html`

**Status:** prototype validated by ear (known issue: intermittent clicks, see §10); this document is the handoff to implementation.
**Prototype is the oracle.** Correctness of the plugin stage is defined as parity with the prototype's `WarpCore` class at matched sample rate, up to stochastic sources (quantum walk RNG), which must be seedable.

---

## 1. Concept

Distortion as an *engine* rather than an effect: a chain in which every stage has memory or inertia, and in which the timbre can be moved without touching the input spectrum. Three ideas fused:

1. **Phase-then-shape** (the EQ3 trick): flat-magnitude all-pass rotation *before* a nonlinearity changes the waveform shape and therefore the distortion product, while the pre-shaper spectrum is unchanged. Modulating the all-passes is a distortion-timbre LFO orthogonal to drive.
2. **Simplex-blended transfer** with a **continuous polynomial fold** (Roar-style sine→poly sweep generalised): weights sum to one, LUT baked at control rate only when weights/order change; quantum-morph = stochastic walk over the weights.
3. **State-dependent transfer (hysteresis)**: three separable memory mechanisms so the same input sounds different depending on where it has been.

Followed by **dispersion** (cascaded first-order all-passes) as the "watery" post stage, and a single shared **inertia** so all moving targets settle as one body.

Reference sounds: Roar polynomial growl, SOPHIE rubber/latex bass and metallic snares, screech leads, watery neuro basses.

---

## 2. Signal chain (per voice / per instance)

```
source ──► drive (×sweep LFO ×drive-memory) ──► phase network (3× 2nd-order APF)
       ──► play operator (backlash) ──► + bias memory ──► shaper LUT ──► bias update
       ──► dispersion (N× 1st-order APF) ──► DC block ──► tanh(out·gain·1.2)
```
Envelopes `eFast` (4 ms) and `eSlow` (250 ms) are taken **pre-drive** (post phase network) — measuring post-drive creates a positive feedback loop with drive memory (bug found and fixed in prototype; keep this invariant).

Prototype source: mono sine/square/saw with one-pole glide, octave range down to A −1 (≈33 Hz). In Horde the source is the upstream engine.

---

## 3. Phase network (pre)

Three RBJ second-order all-pass biquads at `f_c/spread`, `f_c`, `f_c·spread`, shared Q.
- `centre` 40–6000 Hz · `spread` 1–6 · `Q` 0.2–6 · `enable`
- LFO: `rate` 0.02–8 Hz, `depth` 0–3 octaves (sinusoidal, applied to centre in log domain)
- Coupling: `env→phase` −3…+3 octaves × `eOct`, where `eOct = min(2, 2.5·eFast)`
- Coefficients recomputed at control rate from the **inertia-smoothed** centre (§8). Prototype recomputes every block; plugin should recompute only on change and interpolate or crossfade to avoid zipper artefacts (likely source of the clicks, §10).

---

## 4. Shaper — simplex blend

Six basis curves, weights `w0..w5 ≥ 0`, normalised to sum to one; LUT of 4097 points over `x ∈ [−6, 6]`, linear interpolation, clamped at ends.

| k | name | basis |
|---|---|---|
| 0 | soft | `tanh(x)` |
| 1 | hard | `clamp(x, −1, 1)` |
| 2 | chebyshev | `c = clamp(x)`; `0.6·T1(c) + 0.3·T3(c) + 0.15·T5(c)` |
| 3 | sine fold | `sin(½π·x)` |
| 4 | asymmetric | `x>0 ? tanh(1.6x) : tanh(0.7x)` |
| 5 | **poly fold** | `sin(½π · n · u(x))`, `u(x) = (1−c)·x + c·tanh(x)` |

Poly fold parameters: `fold order` n **continuous** 0.5–32; `fold centre` c 0–1 (0 = uniform fold spacing, 1 = folds crowd toward zero and the curve plateaus at the extremes). `n=1, c=0` reproduces basis 3 exactly. Order and centre are mod destinations; both are meant to be swept.

Bake policy: re-bake only when the normalised-weight/order/centre key changes (prototype: weights to 3 dp, order and centre to 2 dp). Never bake per block unconditionally.

**Quantum walk** (`walk` 0–1): every 12 blocks, each raw weight += `walk·0.12·U(−1,1)`, clamped ≥ 0, floor on total mass; then re-bake. Seedable RNG.

---

## 5. Memory (hysteresis) — three mechanisms, all independent

1. **Backlash** — play operator (Preisach hysteron): `w = play·drive_eff`; `if x > y+w: y = x−w; elif x < y−w: y = x+w; out = y`. `play` 0–0.6. Dead-band that only moves when dragged; opens the in/out orbit into a parallelogram loop. Cheap enough to stack many at different widths later (true Preisach model — open item).
2. **Bias memory** — the shaper's operating point follows a leaky average of its own output: `bias += (y − bias)·(1 − e^{−1/(τ·sr)})`, input to shaper is `x + hyst·bias`. `hyst` −1.5…+1.5 (negative = curve runs away from the signal, more interesting), `bias time` τ 1–800 ms.
3. **Drive memory** — `drive_eff = drive · sweep · clamp(1 + dMem·(eFast − eSlow)·3, 0.25, 4)`. `dMem` −1…+2. Hardens after transients, relaxes into sustains. Envelopes pre-drive (§2).

Diagnostic (keep in the plugin UI if there is one): the **in/out orbit view** — memoryless shaper = single line; each mechanism opens it into a differently shaped loop.

---

## 6. Dispersion (post)

Cascade of `stages` (0–48) identical first-order all-passes, coefficient `c = (1 − tan(πf/sr)) / (1 + tan(πf/sr))`, `y = c·x + x₁ − c·y₁`.
- `freq` 40–8000 Hz · LFO `rate` 0.02–8 Hz, `depth` 0–3 oct · coupling `env→dispersion` −3…+3 oct × eOct.
- Frequency is inertia-smoothed (§8). Same zipper caveat as §3.

---

## 7. Drive

`drive` 0.1–12 · `sweep rate` 0.05–8 Hz · `sweep depth` 0–1 (raised-cosine LFO between `drive·(1−depth)` and `drive`) · then drive memory (§5.3). Result is inertia-smoothed (§8).

---

## 8. Interdependence / inertia

One `inertia` (0.5–40 Hz, one-pole per block) smooths **every** moving target: phase-network centre, dispersion frequency, effective drive. Coupling terms (`env→phase`, `env→dispersion`) feed the same targets. Design intent: the chain lags and settles as one body; nothing snaps.

Plugin: consider promoting the one-pole to the second-order (mass–spring, shared ζ) form used in the formant engine so overshoot is available here too.

---

## 9. Parameter surface (summary)

| group | params |
|---|---|
| drive | drive, sweep rate, sweep depth |
| phase network | enable, centre, spread, Q, lfo rate, lfo depth |
| shaper | w0–w5, fold order, fold centre, quantum walk |
| memory | backlash, bias memory, bias time, drive memory |
| dispersion | stages, freq, lfo rate, lfo depth |
| interdependence | env→phase, env→dispersion, inertia, output |

Mod sources exposed: eFast, eSlow, bias, effective drive, current weight vector.

Presets validated in prototype: *Roar* (sine → chebyshev + fold, drive sweep, drive memory), *EQ3 trick* (square → phase network LFO → soft/hard/asym, backlash + bias, env→phase), *watery* (32-stage dispersion, env→dispersion), *deep poly fold* (order 4.5, centre 0.65, drive sweep, bias + drive memory).

---

## 10. Known issues / open items
- **Clicks** reported at the end of prototyping. Most likely: all-pass coefficient recompute at block rate without interpolation (§3, §6), and/or LUT swap on re-bake without crossfade. Plugin: interpolate biquad coefficients per sample or crossfade LUTs over a few ms; verify against a click detector in the golden tests.
- Poly-fold LUT resolution at high order and high drive: 4097 over ±6 is ~340 points/unit; at n=32 that is ~10 points per half-fold — acceptable, but the plugin should compute basis 5 analytically (it is one `sin`) rather than via LUT if CPU allows, or raise LUT resolution.
- Stack multiple play operators → real Preisach model (weight distribution as a control).
- Oversampling: prototype has none; folds and hard clip alias. Plugin: 2–4× around the shaper.
- Second-order (bouncy) inertia (§8).
- Hand-off from the formant engine and the SAW engine: WARP is the shared post-stage; parameter surface should be identical regardless of source.

## 11. Verification hooks
- Deterministic RNG seed for the walk.
- Golden renders vs prototype at 48 kHz for each of the four presets on a 55 Hz sine and a 110 Hz square, 4 s, with the LFOs frozen at phase 0.
- Feedback-safety test: `dMem = 2`, `drive = 12`, sustained square — effective drive must stay within `[0.25·drive, 4·drive]`.
