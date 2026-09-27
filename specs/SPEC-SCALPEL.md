# SCALPEL — blade-synthesis engine for HORDE

**Recommendation spec and handoff** · working name SCALPEL (naming staged in DECISIONS) · prototype: *Scalpel bench*
**Oracle:** `prototype/razor-core.js` (verbatim from `prototype/scalpel-bench.html`) · **Verifier:** `verify/verify.js`
**ball:** horde — implementation. Bench stays the reference until parity goldens are regenerated from C++.

---

## 1. Recommendation

Add SCALPEL to HORDE as a new engine type. It is a Waverazor-style *sectional* oscillator: a base waveform is cut by one or two **blades** (windows over the cycle) and each blade replaces its span with a transformation — hard sync, FM, noise, fold, ring, or crush. It sits naturally in HORDE because every voice is a **Kuramoto swarm** of 1–9 members, so the same coupling law HORDE already uses for detune now also governs where the cuts land, how they spread, and how members modulate each other.

The bench validated the design musically and numerically: 76 starting points covering growls, FM sines, leads, pads and moving textures; a trance "crunch" that matches the Operator/Serum idiom; anti-aliasing better than −115 dB between harmonics at A5 within HORDE's 2× oversampling ceiling; per-cycle DC correction accurate to 0.0055 across 240 mode combinations.

**Ship in v1** (recommended scope)

- Blade 1 and blade 2, all seven modes, all blade waves including the closed-form sine→saw morph.
- Per-blade: units, mirror, FM, rotation, frame, envelope, spreads — each with a *same as blade 1* default.
- Swarm integration: coupling, coupling time (seconds/cycles), start phases (settled/random/aligned), blade frame (member/swarm), cross-member modulation, feedback, balanced pan order.
- Spread laws (gradient, random, drift, alternate, swarm) and cut rules (even, harmonic, undertone, octaves, major, minor, fifths, golden, primes, custom).
- Voicing: poly, mono, legato, glide. Blade envelopes.
- Blade interplay (§13, addendum): serial stacking with a continuous "upper hears lower" blend, stacking order, and collision → pitch / bite. Zero cost when λ = 0 and collision is off.
- Anti-aliasing (PolyBLEP + 2×), per-cycle DC with blocker fallback.
- Monitor visuals: cycle view (sum/members), phase ring with cross-mod arrows, spectrum with predicted formant band.

**Defer** (v1.x): Serum wavetable export (bench feature; see §10), soft/reverse sync, more than two blades, cross-sync and inter-blade FM (§13.6), blade-centre coupling (§13.6).

**Do not port**: the bench UI layout, preset buttons and pads as implemented — HORDE's intent bus, quantum-morph corners and macro system replace them (§11).

---

## 2. Signal flow

```
note ─► voice (amp ADSR, blade envelopes 1/2, glide) ─► swarm of N members (Kuramoto phases θᵢ)
            │
            └► member i, per oversampled tick:
                 base(φ)                                   ← base wave at the member's phase
               + D₁(φ)  [+ twin D₁(φ−½)]                   ← blade 1 contribution (and its mirror twin)
               + D₂(φ)  [+ twin D₂(φ−½)]                   ← blade 2 contribution, if on
               + PolyBLEP corrections (1-sample delay)
               − per-cycle DC estimate (smoothed)
            ► equal-power pan (balanced slot order) ► Σ members × amp env × vel / √N
      ► 4th-order Butterworth @ 0.45·fs ► decimate 2:1 ► (8 Hz blocker when required) ► tanh(1.6·gain·y)
```

A blade contribution is `D(φ) = g(e)·depth·(hot − base)`: zero outside the blade, a crossfade toward the transformed signal inside it. Contributions add; overlapping blades are allowed.

---

## 3. Swarm and coupling (normative)

**Member frequencies.** For a voice at `f` (after bend and glide), member `i` of `N` runs at `fᵢ = f·2^(centsᵢ/1200)`, `centsᵢ = detune·(2i/(N−1) − 1)`.

**Coupling strength (normalized K).** `Keff = K·(1.5·Δω_max + 6π·s)` rad/s with `Δω_max = 2πf·(2^(detune/1200) − 1)` and `s = 1` for *coupling time: seconds*, `s = f/110` for *cycles*. K = +1 locks at any detune; K = −1 drives toward splay.

**Mean fields.** `Rₕ = (1/N)·Σⱼ e^{i·h·2πθⱼ}` for `h = 1…H`, with `H = 1` for K ≥ 0 and `H = min(N−1, 6)` for K < 0. Repulsion must cancel the first N−1 moments or members settle into lumpy r = 0 states instead of equidistant splay.

**Update.** Every 32 samples: `ωᵢ = max(0, fᵢ + (Keff/2π)·Σₕ (1/h)·(Im Rₕ·cos 2πhθᵢ − Re Rₕ·sin 2πhθᵢ))`. Phases advance per sample at `ωᵢ·(glide ratio)`.

**Lead.** `ψ = arg R₁ / 2π`; `leadᵢ = frac(θᵢ − ψ + ½) − ½` cycles. Used by the swarm frame and the swarm spread law.

**Settled start (default).** On a fresh note, fast-forward the coupling 150 steps with `dt = min(0.01, 0.15/|Keff|)` (phases advanced by `(ωᵢ − f)·dt`), from random phases (K ≤ 0) or phases uniform in [0, 0.15) (K > 0). New notes then start at the swarm's steady state instead of audibly sliding into it. *Random* and *aligned* remain selectable.

**Coupling time: cycles.** With `s = f/110`, locked lags `sin Δθ = Δω/(Keff·r)` become pitch-independent; the verifier measures a lead-spread range of 0.0000 cycles across A1–A5. Identical to *seconds* at 110 Hz.

---

## 4. Blades (normative)

### 4.1 Geometry

For a blade with centre `c`, width `w` (bypass below 0.004), cut rate `k`:

- `st = frac(c − w/2)`, `e = frac(φ − st)`; inside when `e < w`.
- Carrier rate `kk = k/w` in *per blade* units, else `kk = k`. In *Hz* units the per-member ratio is `k = k_Hz/fᵢ`.
- Reflect mirror: `eʳ = e > w/2 ? w − e : e`, else `eʳ = e`. Hot phase `hp = kk·eʳ`.
- Edge gate (all modes except crush): `t = hard·w/2`; `g = ½ − ½cos(πe/t)` for `e < t`, `½ − ½cos(π(w−e)/t)` for `e > w−t`, else 1.
- Centre: `c = position + rotation clock + member rotation + spread offset + (frame = swarm ? leadᵢ : 0)`.
- Every carrier rate is capped at `0.45·fs·OS/fᵢ`.

### 4.2 Modes

| Mode | hot(φ) inside the blade |
|---|---|
| Sync | `wave(blade wave, frac(hp + xin))` — hard sync at blade entry |
| FM reset | `wave(blade wave, frac(xin + cp))`, modulator phase `m·eʳ` restarts each blade |
| FM free | same, modulator phase free-running (`modX += mEff·dφ`) |
| Noise | sample-and-hold uniform noise, new value at each integer `hp` |
| Fold | `sin(π/2·(1 + (k−1)/4)·base)` |
| Ring | `base · wave(blade wave, frac(hp + xin))` |
| Crush | box-averaged hold levels with slew (§4.4) |

**FM phase:** `cp = hp + (I/2π)·mod(shape, x)`. **FM pitch:** `cp = hp + acc`, `acc += ((1 + I/10)^{mod} − 1)·kk·dφ` (exponential deviation, symmetric in cents; depth shown as `±1200·log₂(1 + I/10)` cents); reset mode zeroes `acc` at blade entry. **Mod rate** is `m × f₀` or a fixed Hz (`mEff = mHz/fᵢ`).

### 4.3 Waves and modulator shapes

`sine = sin 2πx`; `tri` (sine-aligned); `saw = 2·frac(x+½) − 1`; `rev saw = −saw`; `square`; **sine→saw** `= −atan2(r·sin θ, 1 − r·cos θ)/asin r`, `θ = 2π(x+½)` — the closed form of Σ rⁿ sin(nθ)/n. `r = 1 − (1 − r_max)^shape`, `r_max = min(0.995, 0.01^{f_h/Nyq_OS})` from the blade's own carrier frequency `f_h`, which makes the morph self-band-limiting. Modulator shapes add *smooth noise* (smoothstep between hashed values at integer x) and *S&H noise* (hash of ⌊x⌋).

Periodic antiderivatives (used by crush and DC): sine `(1 − cos 2πx)/2π`; tri piecewise `2f²`, `2f − 2f² − ¼`, `2f² − 4f + 2`; saw `y² − y` with `y = frac(x+½)`; rev saw `y − y²`; square `f < ½ ? f : 1 − f`.

### 4.4 Crush

Hold level `j` is the **average** of the base wave over `[st + j/kk, st + (j+1)/kk]`, via the antiderivatives — not a point sample. Point sampling flips a whole step by ±2 whenever a grid point slides across a saw discontinuity as k changes; averaging is continuous in k. With *step slew* (the Edges control in crush mode), each step glides in from the previous level over its first `hard` fraction, and the output ramps onto `base(st + w)` over the last `min(hard, kk·w)` hold units, so the blade exit is seamless. Verified: largest jump < 0.002 while sweeping k 2→60 for every base wave.

### 4.5 Mirror

*Reflect* makes the blade a palindrome. *Twin −* adds `−D(φ − ½)`, *twin +* adds `+D(φ − ½)`. With a half-wave-symmetric base (sine, tri, square), twin − removes even harmonics entirely (~120 dB down) and is DC-free by construction.

### 4.6 Blade 2

A full second blade with its own mode, wave, shape, width, units, cut rate, position, rotation (with blade 1 or its own clock), mirror, edges, depth, FM (blade 1's or its own, with its own modulator phase), envelope, spreads, and frame. Every per-blade setting defaults to *same as blade 1*, so patches that predate blade 2 are unaffected; the verifier confirms sample-identical output against the pre-blade-2 build when blade 2 follows.

---

## 5. Spreads across members

**Laws** give each member a position `pn ∈ [−½, ½]` per spread parameter: *gradient* (`i/(N−1) − ½`), *random* (new draws on every note-on; a retriggered sounding note glides to them over 20 ms), *drift* (Ornstein–Uhlenbeck per member and parameter: `θ = 2π·rate`, `σ = 0.289·√(2θ·dt)`, clamped ±0.75), *alternate* (±½), *swarm* (`leadᵢ`). A solo member always gets `pn = 0`.

**Offsets** (blade 1; blade 2's own set uses independent random slots):

| Spread | Effect |
|---|---|
| Position | `c += bspread·pn` |
| Cut rate (even) | `k += kspread·pn` (quantize: whole-harmonic offsets) |
| Cut rule | `k ×= r(x)^depth`, `x = (pn + ½)(N−1)`, `r` log-interpolated between rule slots or snapped |
| Width | `w ×= 2^{4·wspread·pn}` |
| Depth | `depth += 2·dspread·pn` |
| Shape | `shape += 2·mspread·pn` |
| FM index | `I ×= 2^{4·ispread·pn}` |
| Rotate | `rotation rate += 2·rotSpread·pn` Hz |

**Cut rules** (member 0 is the root at k): harmonic `j+1`; undertone `1/(j+1)`; octaves `2^j`; major `{1, 5/4, 3/2}` by octave; minor `{1, 6/5, 3/2}`; fifths `1.5^j`; golden `φ^j`; primes `pⱼ/2`; custom list (fractions or decimals) repeating by octave.

---

## 6. Modulation, rotation, envelopes, voicing

**Cross-member modulation and feedback.** Each member's blade carrier phase is offset by `xin = ½·(xm·y_{i+1}[n−1] + fb·½·(yᵢ[n−1] + yᵢ[n−2]))` cycles (ring topology, one-sample delay; the two-sample average is the DX-style guard against Nyquist hunting). Applies to Sync, FM and Ring carriers on both blades. It changes timbre only, never member frequencies.

**Rotation.** Bipolar rate (±4 Hz) moves the blade around the cycle. *Restart per note* (default) zeroes the voice clock at note-on so every note starts with the blade at Position; *free-running* uses a shared clock. When a rate returns to zero the offset glides home by the shortest way (30 ms). Blade 2 rotates with blade 1 or on its own clock.

**Blade envelopes.** Per blade, AD: linear attack, exponential decay (`1 − e^{−4/(D·fs)}` per sample), velocity sensitivity `v`: level `= env·(1 − v + v·vel)`. Cut rate `×2^{±4 oct·level}`, width `×2^{±3·level}`. Retrigger follows the amp envelope's voicing rules.

**Voicing.** Poly (6 voices, oldest stolen), mono (retrigger every note), legato (retrigger only when no key is held; releasing returns to the last held key without retrigger). Glide is exponential in log-frequency, glide time ≈ time to 95%; *overlapping* or *always*. Member rates are scaled per sample by `f/f_at_last_coupling` so glide is smooth between 32-sample coupling updates.

**Pan.** Equal power. *Balanced* order (default) permutes the evenly spaced slots so pan is uncorrelated with member order; otherwise every gradient spread tilts brightness toward one side (measured +3.2 dB with width spread 0.5 under fan order). Slot tables found by exhaustive search:

```
N=2  [-1, 1]                       N=6  [-.2, .6, -1, 1, -.6, .2]
N=3  [-1, 1, 0]                    N=7  [-.3333, 1, -1, .6667, -.6667, .3333, 0]
N=4  [-.3333, 1, -1, .3333]        N=8  [-.1429, .4286, -.7143, 1, -1, .7143, -.4286, .1429]
N=5  [-.5, 1, 0, -1, .5]           N=9  [-.25, 1, -.75, -.5, 0, .5, .75, -1, .25]
```

N = 2 cannot be balanced; N = 3 is half-balanced. *Fan* order remains available as a deliberate spectral-stereo effect.

---

## 7. Anti-aliasing and DC

**PolyBLEP, 2-point, one-sample delay.** For an event at fraction τ ∈ (0, 1] of the tick with step height `h`: current sample `−h/2·τ²`, delayed sample `+h/2·(1 − τ)²`. Events handled: base-wave discontinuities (saw, rev saw at ½; square at 0 and ½); blade and twin edges; carrier discontinuities inside each blade, located in carrier-phase space `cp = kk·eʳ + offset` with the FM offset interpolated across the tick (so wraps stay corrected under FM), at most 6 per tick; crush steps when slew is 0.

**Step heights.** The bench measures `h` by evaluating the full composite at `E ± 10⁻⁷`. Keep that only for blade edges. For carrier wraps use the analytic jump × gate × depth (see §8).

**Oversampling.** 2× maximum (HORDE constraint). Decimator: two biquads (Q 0.5412, 1.3066) at 0.45·fs.

**Per-cycle DC (default).** Every 256 samples, per member and blade, estimate the blade's mean contribution and subtract it (smoothed, 0.003 per tick; jumps on a voice's first estimate). Hard or soft sync blades of closed-form waves are exact: `(F(kk·w) − F(0))/kk − (F(st+w) − F(st))`, minus a dense numeric pass over the taper regions when edges are soft. Every other mode is numeric with `J = clamp(32·features, 48, 1024)` points, where features count carrier cycles, crush steps and modulator cycles. Refresh interval scales with `J/64`. Noise blades are estimated with the noise at its mean. *Twin −* is zero by construction; *twin +* doubles. With cross-mod or feedback active, an 8 Hz blocker runs as well, since those depend on previous samples the estimate cannot see. Verified: worst estimate error 0.0055 over 240 cases; output residual < 0.0015 while k steps.

**Parameter smoothing.** 12 ms one-pole. Per sample: cut rate (k, Hz), width, position, depth, FM depth, edges — for both blades. Everything else at control rate (16 samples). Discrete settings apply at block rate.

---

## 8. Performance and port guidance

JS reference cost per member-tick, 6 notes × 9 members at 2×: default ~145 ns, two blades ~285 ns, two blades + twin + cross-mod + envelopes ~550 ns. Most of the default path is JS overhead rather than DSP (bypass alone costs ~80 ns). A scalar C++ port should land roughly at 10–25 ns per member-tick for the default path; that is an estimate, not a measurement. At 8 notes × 9 members × 2×, that is 7–17 % of one core.

Port requirements:

1. **Per-member parameter structs**, not writes into shared state. The bench overrides shared fields per member and restores them; do not replicate.
2. **Analytic carrier step heights** (jump × gate × depth). Numeric evaluation costs two full composites per event, and events can occur several times per tick at high k.
3. **Integer hash** (xorshift/PCG) for noise. The bench uses a `sin`-based hash, which is why noise FM is its most expensive mode.
4. **Harmonic mean fields by powers**: `e^{i·h·2πθ} = (e^{i·2πθ})^h`. One sincos per member instead of H.
5. **Amortize settle** over the first blocks, or seed it from the locked fixed point, so chords don't spike the note-on block.
6. **Specialize the inner loop** per block on (mode, blade wave, mirror, blade 2 on) so the per-sample path does not branch on mode.
7. **Fast approximations** for sine and for the morph's atan2; its `r` cap already bounds bandwidth.
8. **Blade 2, twins, cross-mod as zero-cost when off.**
9. **SIMD note:** 9 members fill 4-wide lanes badly (3 groups, the last ¾ empty); 8 fills them exactly. BLEP events are branchy — vectorize the smooth path, fall back to scalar for events. Staged as a decision.

---

## 9. Parity and verification

The oracle is `prototype/razor-core.js`. It calls `Math.random`; `verify/rng.js` installs a seeded xorshift so JS renders are reproducible.

**Bit-parity targets**: any configuration whose output is independent of random draws — aligned start phases, gradient/alternate/swarm laws, no noise modes. **Statistical targets**: settled or random start phases, the random and drift laws, noise and S&H modulation. The C++ engine must use named, seeded RNG streams; its draw order will not match the JS, and should not try to.

`verify/verify.js` runs the numeric battery below in about 20 s (`--bench` adds cost). `verify/render-goldens.js` renders all 76 presets to float WAVs with a hash manifest (not shipped; ~45 MB).

### Acceptance criteria

- [ ] Coupling: r ≥ 0.90 at K = +1 (7 members, ±40 ct); r ≤ 0.10 at K = −1 (4 members)
- [ ] Coupling time *cycles*: lead-spread range across A1–A5 < 0.01 cycles
- [ ] Swarm frame: (blade centre − lead) identical across members to 1e-6
- [ ] Aliasing at A5, 2×: between-harmonic probe < −100 dB re h2 for saw sync (both blades) and for a rev-saw carrier under harmonic FM
- [ ] Per-cycle DC estimate: worst error < 0.01 over the verifier's 240-case grid; output residual < 0.003 while k steps
- [ ] Crush with slew: largest jump < 0.01 while k sweeps 2→60, all base waves
- [ ] Balanced pan: |L − R| brightness < 1.5 dB with width spread 0.5, N = 4…9
- [ ] Cut rules match §5 lists to 0.01
- [ ] Blade envelopes independent; legato glides without retrigger and settles within 0.05 Hz
- [ ] No non-finite samples across the verifier's 112-configuration sweep; output bounded
- [ ] Blade 2 with every per-blade setting at *same as blade 1* matches the oracle goldens for the Two-blade presets (regression guard for the follow defaults)
- [ ] With every new parameter at its default, existing HORDE sets load and render unchanged (ADR-draft)
- [ ] Interplay: serial fold-over-sync, occlusion and λ = ½ match the §13.2 closed forms to 1e-9
- [ ] Interplay off (λ = 0, collision off): output identical regardless of stacking order
- [ ] Interplay: output DC residual < 0.003 on the verifier's serial patches; aliasing at A5 (fold over sync) < −100 dB re h2
- [ ] Collision pitch: extra carrier phase > 0.5 cycles with overlapping blades, exactly 0 when they don't overlap
- [ ] No non-finite samples over all 7 × 7 mode pairs × both stacking orders with twins and collision on
- [ ] C++ cost at 2×, default path, ≤ 25 ns per member-tick on the reference machine (target; re-baseline after first port)

---

## 10. Incidental details — do NOT replicate

- `Math.random` and its draw order; the 14 random slots' numbering (keep only their independence).
- Shared-state parameter overrides; `RazorCore.mr/mn` static globals for the morph (make them per-blade fields).
- Numeric step heights for carrier wraps (§8.2).
- The `sin`-based hash.
- The three-pass static estimate the *members* view and wavetable export use for cross-mod/feedback. It is a display approximation; audio is exact.
- AudioWorklet/ScriptProcessor plumbing, 30 Hz visualizer messages, the `.zip` wrapper for downloads.
- 6-voice polyphony and the fixed 9-member arrays (use HORDE's voice allocator and its member limit).
- The bench's preset reset bases and UI grouping — HORDE's patch model supersedes them.
- `tanh(1.6·gain·y)` as the final stage — use HORDE's output stage; the bench's pads are hot enough in chords to lean on this clipper.

**Wavetable export** (deferred) is specified by the bench: frames rendered at 8192 points, band-limited to 1023 harmonics by FFT, 2048-sample frames in 32-bit float WAV with a `clm ` chunk (`<!>2048 01000000 wavetable (www.xferrecords.com)` — reconstructed; 2048 is also Serum's default), table-wide normalization, modes snapshot / sweep pad axis (with its second parameter) / record over time.

---

## 11. HORDE integration

- **Coupling law.** Use HORDE's existing Kuramoto implementation and K normalization; SCALPEL adds only *coupling time* and the lead output. Sakaguchi α, when it lands, composes with *swarm frame* and the *swarm* law without changes.
- **Legacy defaults.** Every new parameter defaults to behaviour already in HORDE or to "off": coupling time *seconds*, blade frame *member*, spread law *gradient*, blade 2 *off*, per-blade settings *same as blade 1*.
- **Pads → intent bus.** The bench's two pads with a primary and optional inverted second parameter per axis map directly onto intents with two bindings, one inverted. Keep HORDE's rule that pad offsets live relative to a per-corner home.
- **Quantum morph and inertia.** Candidates for corner-level binding: blade widths, cut rates, positions, spread amounts, envelope amounts. Keep global: voicing, units, frames, laws, coupling time. Inertia on position and cut rate gives physical-feeling blade motion.
- **FOUNDATIONS.** Blade geometry, BLEP event scanning, and per-cycle DC are self-contained and reusable; worth landing as FOUNDATIONS utilities rather than engine-private code.
- **Pan order.** HORDE hit the same brightness tilt; share one balanced-slot table across engines.

---

## 12. Known limitations

- Cross-mod and feedback carriers are not BLEP-exact (their phase offset changes per sample); aliasing is bounded by the 2× stage.
- FM with noise modulation is intentionally broadband between harmonics; there is no meaningful alias metric for it.
- The per-cycle DC estimate for the sine→saw morph is numeric (no closed-form integral).
- Two-member swarms cannot be pan-balanced.
- Rotation and spreads are control-rate; very fast rotate-spread settings step at 32-sample resolution.
- Collision acts on primary cuts only; mirror twins neither cause nor receive it.
- Serial crush point-samples the lower blade's content per step (only the base part is box-averaged); with slew 0 its steps can move when k changes.

---

## 13. Addendum — blade interplay (v1.1)

Added after the initial packet. The bench prototype and oracle in this packet include it; verifier checks 14–18 cover it.

### 13.1 Parameters

| Key | Range | Default | Meaning |
|---|---|---|---|
| `b2order` | 0, 1 | 0 | Stacking: 0 = blade 2 over blade 1 (blade 2 is *upper*), 1 = blade 1 over blade 2 |
| `b2mix` (λ) | 0…1 | 0 | What the upper blade transforms: base (0) → base + lower blade's full contribution (1). Smoothed per sample |
| `colK` | −1…1 | 0 | Collision → pitch: upper carrier rate × 4^(colK·ov), i.e. ±2 octaves at full overlap |
| `colB` | 0…1 | 0 | Collision → bite: upper FM depth × (1 + 4·colB·ov), fold drive × (1 + 2·colB·ov) |

All four only apply with blade 2 on. With λ = 0 and both collision amounts 0 the engine takes the parallel path and is sample-identical to v1.

### 13.2 Serial composition (normative)

Let `D_A(φ)` be the lower blade's delta computed from the base (as in v1: `voice − base`), and `s_A, s_B ∈ {−1, 0, +1}` the twin signs (twin −, off/reflect, twin +). Evaluate the lower blade at φ and φ − ½:

  `L(φ) = D_A(φ) + s_A·D_A(φ−½)`  `L(φ−½) = D_A(φ−½) + s_A·D_A(φ)`

The second evaluation is needed whenever *either* blade has a twin (the upper twin must hear the lower blade at φ − ½); it uses the lower blade's twin state.

The upper blade transforms `x = base + λ·L`. Every mode's formula replaces *base* with *x* (§4.2): fold `sin(drive·x)`, ring `x · carrier`, and the lerp `x + g·depth·(hot − x)`; outside the blade the upper returns *x*. Its delta is `D_B = voice_B(x) − x`, and similarly at φ − ½ with `x₂ = base(φ−½) + λ·L(φ−½)`.

  **out = base + L(φ) + D_B(φ) + s_B·D_B(φ−½)**

Consequences, verified to machine precision: at λ = 1 a generator mode on top (sync, FM, noise) *occludes* the lower blade inside its span (`out = (1 − g·d)·(base + L) + g·d·hot`); fold and ring on top process the lower blade's content; at λ = 0 this reduces exactly to the v1 parallel sum.

**Serial crush.** The base part of each hold level stays box-averaged (§4.4). The lower blade's part, `x − base`, is sampled at each step start and held; with slew it glides from the previous held value over the first `slew` fraction of the step, and over the exit ramp it glides onto the live `x − base`, so the blade exit stays continuous. Held state: `idx`, `val`, `pv` per blade state.

### 13.3 Collision

Per member and tick, with blade-local positions `e₁`, `e₂` and the §4.1 edge gates (crush counts as hard-edged, gate 1 inside):

  `ov = gate₁(e₁) · gate₂(e₂)`

*Pitch* is an integrator on the upper blade, so the carrier chirps smoothly rather than jumping: `cacc ← 0` at upper-blade entry; `cacc += (4^(colK·ov) − 1)·kk_upper·Δφ`; carrier phase `cp = hp + xin + cacc` for sync, FM and ring. `cacc` is not wrapped (it is bounded by blade length), which keeps the PolyBLEP carrier-wrap scan valid: the scan adds `cacc − Δcacc` and `cacc` to its tick-start and tick-end offsets.

*Bite* reads `ov` at the current tick. Only the upper blade's state carries `ov`/`cacc`; the lower's stay 0. When both amounts are 0 the fields are cleared once and cost nothing afterwards.

### 13.4 DC with interplay

Closed-form per-blade estimates no longer hold once the upper hears the lower. For `b2on && λ > 0` the pair is estimated together: the mean of `(out − base)` over one cycle, numerically, with scratch copies of all four blade states, `J = clamp(32·(kk₁·fm₁ + kk₂·fm₂ + 2(m₁ + m₂) + 4), 64, 2048)` points (`fm = 1 + I/2` for FM blades, else 1). Refresh interval scales with J/64 as before. Collision makes the cycle time-varying, so when either collision amount is on the 8 Hz blocker also runs. Verified: output residual ≤ 0.0002 on serial patches; estimate error ≤ 0.0034 against brute force over 48 serial configurations.

### 13.5 Cost

JS reference ratios (absolute numbers vary by machine; see `verify/last-run.txt`): serial ≈ +35 % over parallel two blades (the lower twin evaluation and the pair DC estimate), serial + collision ≈ +55 %. In C++: the upper/lower role swap should be a template or branch-free pointer swap at block rate, not per-sample descriptor arrays (the bench builds small arrays per call; do not replicate); `ov` needs only the two gates the engine already computes.

### 13.6 Staged extensions (not implemented)

- **Cross-sync:** upper-blade entry resets the lower carrier (sync within sync).
- **Inter-blade FM:** the upper output phase-modulates the lower carrier, through the same one-sample delay as cross-member modulation.
- **Blade-centre coupling:** treat the two centres as coupled oscillators on the cycle (attract/repel), so counter-rotating blades lock, slip, or push each other apart. Control rate; pairs naturally with collision.

### 13.7 Presets

The *Showcase / Interplay* row: Fold over sync, Sync over fold (same patch, opposite stacking), Occlusion sweep, Ring on ring, Crushed burst, Collision chirps, Collision bite. All in `data/presets.json`.
