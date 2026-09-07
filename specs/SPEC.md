# horde — instrument specification

## 1. Thesis

A unison voice stack is a population of coupled oscillators. Everything this instrument does follows from taking that literally, at three nested levels:

1. **Within a note:** N oscillator voices are Kuramoto-coupled. Coupling strength K sweeps the sound through a genuine phase transition: free cloud → herding → shimmering partial lock → static comb → single coherent oscillator. Negative K pulls toward the splay state instead: harmonic multiplication (saw kernel) or interference erasure (sine kernel).
2. **Within a spectrum:** each partial of an additive series carries its own voice cloud; coupling can cascade up the harmonic series.
3. **Within a chord:** held notes are coupled through the same sin(error) restoring force, with simple frequency ratios as attractors — consonance gravity, adaptive just intonation as emergent physics.

One mechanism, three scales. The instrument's identity is that these are *the same knob philosophy* everywhere: an attractor, a strength, a basin, a settling time.

## 2. Unified engine (ADR-001)

There is one engine: a **per-partial swarm over a kernel**.

- `P` partials × `M` cloud voices per partial, each voice a phase accumulator rendering a **kernel**: saw (polyBLEP↔naive crossfade), sine (table), or wavetable (future: the terrain sibling's terrain as kernel — "every preset is somewhere" crossover).
- **HYPERSAW mode** (the engine formerly named SAW; renamed 2026-08-17, ADR-091) is the engine at `P = 1`, saw kernel, `M = 1..32`.
- **SPECTRA mode** is the engine at `P = 4..N_max`, sine kernel, `M = 1..7`, with per-partial amplitude tilt `1/k^t` and stretch inharmonicity `f_k = k·f0·√(1 + B·k²)`.
- Prototype ceiling was P=24 (browser main thread). Target: 64–128 partials via oscillator bank, or unbounded via iFFT rendering — **ADR-006, open, Phase 0 spike**. Note the architecture favors iFFT unusually well: all coupling dynamics already run at control rate, so the swarm state can drive a spectral frame directly.

## 3. Parameter surface — four layers

Every control lives in exactly one layer. This is the UI's information architecture and the preset format's schema.

### Layer 1 — The swarm (who exists)

| Parameter | Range / values | Notes |
|---|---|---|
| Voices (M) | 1–32 (HYPERSAW), 1–7 per partial (SPECTRA) | continuous-density presentation optional (parked) |
| Partials (P) | 1 (HYPERSAW) / 4–128 (SPECTRA) | ceiling per ADR-006 |
| **Distribution** | even · JP-8000 (Szabo curve, interpolated) · Gaussian (seeded) · Cauchy (seeded) · bimodal (two-cluster) · clustered-pairs · drawn-by-hand | *who sits where* in normalized offset space x ∈ [−1,1] |
| **Detune law** | cents-constant · Hz-constant · ERB-flat · **tempo-grid** | *how offsets map to Hz* (see §4). Law and distribution are orthogonal axes — this factorization is a deliberate design position (see PRIOR-ART §2) |
| Detune amount | 0–1, per-law scaling | cents: ±100¢ · Hz: ±20 Hz · ERB: ±0.35·ERB(f0) · grid: cents placement then snap |
| Width tilt (SPECTRA) | k^γ, γ ∈ [−1, 2] | chaos gradient across the series: blizzard-bass↔blizzard-top |
| Amp tilt (SPECTRA) | 1/k^t, t ∈ [0.5, 2] | spectral rolloff |
| Stretch (SPECTRA) | B ∈ [0, 2e-3], squared taper | piano/bell inharmonicity |
| Seed | integer, displayed, dice | governs Gaussian/Cauchy draws, un-retriggered phase scatter, per-note drift streams |

### Layer 2 — The coupling (how they interact)

| Parameter | Range | Notes |
|---|---|---|
| **Pull K** | bipolar −1..+1, squared taper | + = sync attractor, − = splay attractor. Normalized: K_Hz = 4·K·\|K\|·σ_measured (sync); splay gets **3× authority** (ADR-007). Slewed. |
| Topology | mean-field · ring (reach 1–8) · two-cluster (cross-link μ 0–1) | shapes the sync attractor; splay = assigned slots (≡ twisted state on ring) |
| Poles q | 1–4 | Daido q-harmonic mean field (mean-field topology only). q=1 classic sync; q>1 splits the swarm into q clusters (R_q high, R₁ low) with seed-dependent demographics — the split ratio is a fundamental-vs-q·f mix the seed rolls — and bistable full sync from aligned starts (ADR-015). Emergent sibling of assigned-slot splay, which remains the deterministic mechanism (ADR-007) |
| Phase lag α | −90°..+90° | Sakaguchi term sin(ψ−θ−α); bends locked frequency; doorway to broken-symmetry states |
| Inertia | 0–1 | 2nd-order swing dynamics, ζ=0.45, mass ∝ knob; hunting/ringing approach to lock |
| Onset lock / dissolve | 0–1 / 10 ms–8 s | K envelope: locked attack blooming into cloud (or the reverse with retrigger off) |
| Cascade (SPECTRA) | 0–1 | partial k receives coupling gated by smoothstep(R_{k−1}); slews slow with cascade so the zipper is a heard event |
| Retrigger | on/off | aligned vs seeded-scattered phase init |
| Absolute-K mode | advanced toggle | bypass σ-normalization for identical-oscillator experiments (ADR-004) |

### Layer 3 — Forces between notes

| Parameter | Range | Notes |
|---|---|---|
| Consonance gravity | 0–1 | pairwise pull of held fundamentals toward nearest simple ratio; rate ∝ knob; audio-domain force, not MIDI preprocessing (ADR-008) |
| Basin | ±10..±50¢ | capture radius; outside it, harmony untouched |
| Ratio set | default 13 ratios (1/1…2/1, octave-folded) | future: Tonality-supplied priors, context-weighted basins |

### Layer 4 — Output & perception

| Parameter | Notes |
|---|---|
| Density comp | normalization exponent N^p, p ∈ [0.5, 1] — coherent↔incoherent summing audible |
| Stereo width | offset-mapped pan; **auto-narrows per-partial under splay** (cancellation requires per-channel coherence); mono fold-down audition |
| R → mod | order parameter R and spread σ as first-class mod sources; prototype-validated route: R→tone (bipolar one-pole) |
| Digital↔clean | polyBLEP↔naive crossfade; aliasing as material |
| Master | volume, tanh soft-clip guard |

### Performance layer (over all four)

- XY pad: X = detune, Y = bipolar K (validated gesture). Under splay, X becomes "looseness of the multiplied tone" — the axes stay orthogonal.
- MPE map (Phase 5): pressure → K, slide → detune, per-note pads.
- K is a gesture, not a setting: envelopes, macros, and MPE routing to K are first-class, per the finding that its value lives in the transitions.

## 4. Detune laws

- **Cents-constant** (classic): beat rates ∝ f0; the familiar register-dependence.
- **Hz-constant** (beat-flat): identical beat rates everywhere; static bass, subtle top.
- **ERB-flat** (roughness-flat): Δf ∝ ERB(f0) = 24.7·(4.37·f0/1000 + 1); perceived roughness approximately constant across the keyboard. The law that "follows you."
- **Tempo-grid** (ADR-005): voices placed by the distribution as usual, then each Hz offset snapped to the nearest multiple of u = (BPM/60)·cyclesPerBeat. All pairwise beat rates become exact grid multiples; spread stays under the detune knob; rung-sharing voices thicken with zero beating. Unit is **cycles per beat** (¼/beat = one pulse per 4/4 bar; 2/beat = eighths). Host-tempo synced. **Grid × coupling (ADR-016/017):** locking suppresses grid-rate beating (locked voices share one frequency) — intended negotiation, not a defect, but any surface exposing this law MUST show live grid status (unit, rungs occupied) plus a lock warning gated on coupling engaged (Ksm > ~0.05 Hz) AND coherence high (R or R_q > 0.8); grid-forward presets want K subcritical (≲ 1×σ).

## 5. Subsystem specifications

### 5.1 Sync attractor
Mean-field: couple_i = K_slew·R·sin(ψ − θ_i − α), mean field (R, ψ) recomputed per control tick, coupling term held across the tick. Ring: sum over ±reach neighbors, normalized by 2r. Two-cluster: per-cluster mean fields, intra weight 1, inter weight μ, normalized by (1+μ).

### 5.2 Splay attractor (ADR-007)
Assigned-slot, anchored on the center voice (argmin |x_i|): couple_i += K_splay·sin(2π(θ_c + (i−c)/M − θ_i)). Saw kernel: locked splay sums toward a saw at M·f (harmonic multiplication; level drop is physical). Sine kernel: locked splay cancels (interference gate); residual ghost from slot errors asin(Δω/K). Requires 3× sync authority to overcome detune; detune knob becomes splay looseness.

### 5.3 Inertia
Per-voice frequency state with momentum: mom += (target − f_eff)·ω0²·dt; mom ×= exp(−2ζω0·dt); f_eff += mom·dt. ω0 = 2π·(8(1−w)+0.6) rad/s, ζ = 0.45. w→1 = heavy: perpetual hunting (kept deliberately).

### 5.4 Cascade (SPECTRA)
gate_k = 1 − c + c·t², t = clamp((R_{k−1} − 0.4)/0.45, 0, 1). Coupling slew coefficient 0.08/(1 + 120c) per tick so stages take ~0.5 s at c=1 (zipper ≈ 8 s over 12 partials; ≈ 2 s at c=0.5).

### 5.5 Consonance gravity (ADR-008)
Once per render block: fold each held pair's ratio into [1,2), snap to nearest of the ratio set, compute cents error; if |err| ≤ basin, move each note half the correction at rate grav·3/s (exponential approach). Runs on f0cur per note; note-on resets f0cur to equal-tempered pitch, so settling is a per-chord event.

### 5.6 Visualization (first-class, not decoration)
- **Phase circle** (HYPERSAW/DYNAMICS hero) with: order vector R₁·e^{iψ}; **dual order meters** (R₁ sync/clump and the N-th Daido order parameter R_N = |(1/N)·Σ e^{iNθᵢ}| for splay/formation — orthogonal regime indicators: R₁ high = sync, R₁ low + R_N high = splay, both low = free); **seat rings** at assigned splay slots when splay is engaged (opacity ramps with engagement); **formation polygon** connecting voices in index order (collapsed point = sync · regular N-gon = splay · scribble = free). Amended per the splay-legibility change note (2026-07-17, archived at `docs/change-notes/2026-07-17-splay-legibility.md`): R₁ alone cannot distinguish locked splay from free — the instrument's most novel state was illegible. Forward-compatibility: if multi-pole (Daido) coupling lands, meters/formations generalize to an R_q family — design the GUI meter component to take q as a parameter, not hardcode N.
- **Phase carpet** (voice index × phase) — where ring waves and broken-symmetry states are legible
- **Partial strips** (per-partial phase columns + R bars) — where cascades and erasure are legible
- Live readouts: R (per cluster where relevant; R_q under Daido poles), effective pull Hz, measured σ, gravity ratio + cents deviation, and — whenever the tempo-grid law is exposed — grid status with the cause-AND-state lock warning on fixed-height reserve (ADR-016/017)
- The GUI thesis: the user should *see the physics they are hearing.* These are the instrument's face, not debug views.

### 5.7 Provenance & preset identity
Seed + distribution + topology + full parameter state = a nameable, exactly reproducible swarm. Presets embed provenance metadata (per the terrain sibling's provenance discipline). Deterministic guarantee: same seed, same note order → identical output (mulberry32 streams; per-note stream seeded seed + noteAge·7919 + 1; no wall-clock anywhere in the core).

## 6. Implementation notes (hard-won, do not rediscover)

1. Control tick = 16 samples → **2756 ticks/s at 44.1 kHz**. All slew coefficients are per-tick; time constant τ ≈ 1/(coef·tickRate). Getting this wrong by the intuitive ~10× made the cascade zipper invisible once already.
2. Coupling terms held across a tick are fine for first-harmonic coupling (relative phases drift at detune rates); they are *not* fine for higher-harmonic coupling (rotates at N·f) — hence assigned-slot splay rather than N-th-harmonic mean field.
3. σ is measured live from actual per-voice frequencies (post-drift, post-law) each tick, floored at 0.05–0.08 Hz. K in ×σ units puts the phase transition mid-knob at any register/law/detune. Absolute-K mode exists because the normalization collapses at zero detune.
4. Stereo pans break splay cancellation (per-channel sums weight voices unequally). Per-partial width mix w = K_splay/(K_splay + 2σ) narrows toward center as splay engages. Coherence costs width on both ends of the K axis; make it a law, not a bug report.
5. Envelope: AR, ~3–4 ms attack, ~160–180 ms release, per swarm. tanh on the master bus. Voice stealing: free-and-faded first, else oldest by age counter.
6. polyBLEP saw; 4096-entry sine table with linear interp for the additive kernel.
7. JP-8000 normalized offsets (Szabo): [−1, −0.5715, −0.1774, 0, 0.181, 0.565, 0.9766], interpolated for M ≠ 7.
