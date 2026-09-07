<!-- Ingested 2026-07-19 (ADR-048). ADR cross-refs corrected on ingest per the
     external-packet discipline (L0009): Daido = ADR-015 (was "ADR-010"); the
     ingest/design ADR is ADR-048 (packet said "ADR-046", which is the notch
     swarm here); prior-art posture = ADR-044 (was "ADR-018"). Protected path. -->

# horde — swarmalator engine specification (position ↔ phase coupling)

Companion to `SPEC.md` and `SPEC-EFFECTS.md`. The first engine to couple **two** quantities *to each other* rather than coupling one quantity across a population. Each unit carries an audio **phase** θ (→ the tone) and a **spatial angle** ξ on a ring (→ stereo position); a single cross-coupling law makes each pull on the other, so the swarm is simultaneously a spectral object and a stereo-spatial object and the two are one structure, not two. Ingested and ported 2026-07-19 (ADR-048); reference/swarmalator.html is the parity oracle. Marked EXPERIMENTAL — may not survive testing.

Reference implementation: `reference/swarmalator.html` (`Swarmalator` core, headless-testable).

## 1. Thesis

Every other horde engine couples a single population variable — oscillator phase (SAW), partial frequency (SPECTRA), filter/notch center (effects), delay time (time). The swarmalator couples phase **and** position, to each other. The consequence is the one behavior the instrument could not previously produce: **moving in space changes the timbre, and changing the timbre moves the sound in space.** Pan is not a post-hoc placement knob here; it is a state variable of the same dynamical system that makes the tone. This is the stereo/spectral unification, and it is on-thesis — the supersaw's spatial image taken as seriously as its spectrum, both emergent from one coupled system.

## 2. Model

Each unit i holds an audio phase θ_i (cycles, advanced per sample) and a spatial angle ξ_i (radians, updated per control tick). The mean-field-reducible **ring swarmalator** model:

```
ξ̇_i = ν_i + (J/2)[ R₊ sin(ψ₊ − ξ_i − θ_i) + R₋ sin(ψ₋ − ξ_i + θ_i) ]
θ̇_i = ω_i + K·(ordinary Kuramoto sync) + (J/2)[ R₊ sin(ψ₊ − ξ_i − θ_i) − R₋ sin(ψ₋ − ξ_i + θ_i) ]
```

with the two compound order parameters

```
W₊ = ⟨e^{i(ξ + θ)}⟩ = R₊ e^{iψ₊}      W₋ = ⟨e^{i(ξ − θ)}⟩ = R₋ e^{iψ₋}
```

**Derivation note (why it stays in the hood).** The pairwise ring couplings sin(ξ_j−ξ_i)cos(θ_j−θ_i) and sin(θ_j−θ_i)cos(ξ_j−ξ_i) reduce, via sin·cos = ½[sin(a+b)+sin(a−b)], to the W± terms above — so the engine computes **two extra complex accumulators per control tick** and is otherwise O(N), exactly like every other engine. No pairwise O(N²) loop.

**Timescale / tick-holding.** θ advances at audio rate; ξ and the coupling update at control rate (16-sample tick). The compound phases ψ± spin at audio rate, but the coupling arguments ψ± − ξ_i ∓ θ_i are detune-slow — the same tick-holding argument that validated Daido coupling (ADR-015) applies, so holding the W± terms across a tick is correct.

**Two coupling roles (design decision, ADR-048).** K is kept as **ordinary Kuramoto sync** (drives R via the standard R·sin(ψ−θ) term) so the phase axis behaves identically to SAW when J=0; J carries the full swarmalator cross-coupling (space↔phase, both directions). This keeps K's meaning stable across engines and isolates the novel behavior in one knob. Pure compound-only coupling was rejected because at J=0 it fails to sync (measured R≈0.17 before the fix; R=0.97 after).

## 3. Mappings

- **Phase θ → tone.** Per-unit oscillator (saw via polyBLEP, or sine kernel), summed. Identical kernel treatment to SAW.
- **Position ξ → stereo pan.** Pan coordinate p = cos(ξ_i)·width ∈ [−1,1]; constant-power gains gL = cos((p+1)π/4), gR = sin((p+1)π/4). Units on opposite arcs of the ring pan hard L/R.
- **ν_i (spatial natural velocity)** seeded, scaled by the *space drift* knob — the spatial analogue of oscillator drift; nonzero ν gives idle rotation/wander even at J=0.
- **ω_i (audio frequency)** from f0 × seeded detune distribution (same distribution machinery as SAW).

## 4. Parameters (Layer surface)

| Param | Range | Role |
|---|---|---|
| units | 6–18 | population size (prototype; VST may raise) |
| kernel | saw / sine | per-unit waveform |
| detune | 0–1 (→ ±70¢) | spread of ω_i; also sets σ for K normalization |
| space drift | 0–1 | scale of seeded ν_i (idle spatial motion) |
| seed | int | determinism |
| **K** | −1..+1 | phase coupling: + sync (tone), − splay |
| **J** | −1..+1 | cross coupling: + sort (rainbow), − scatter |
| width | 0–1 | ξ→pan depth |
| volume | 0–1 | — |

K normalization reuses ADR-004: K_Hz = 4·K·|K|·σ (squared taper, σ = frequency spread), so the sync transition sits mid-knob regardless of detune. J is scaled by a fixed angular rate (prototype 7 rad/s at |J|=1); the VST should expose or tune this against the spatial timescale.

## 5. Measured behaviors (acceptance anchors — reference `reference/swarmalator.html`, 12 units, 220 Hz)

| State | Setting | R | R₊ | R₋ | Note |
|---|---|---|---|---|---|
| Sync | K=+0.9, J=0 | 0.97 | 0.25 | 0.31 | phase axis == SAW when J off |
| Splay | K=−0.9, J=0 | 0.09 | — | — | phases spread |
| **Rainbow** | K=0, J=+0.9 | 0.26 | 0.39 | 0.48 | space-phase sorting with phases still incoherent |
| **Sync + rainbow** | K=+0.6, J=+0.9 | 0.69 | 0.85 | 0.78 | tone and stereo image are one structure |
| **Active phase wave** | K=−0.5, J=+0.9 | — | 0.27 | 0.30 | sustained ξ drift ≈ 0.46 rad/s — units chase round the ring |
| Stereo widening | J=+0.9, K=+0.5 | — | — | — | L/R correlation drops from ~1 toward 0.89 under the rainbow |
| Stability | K=J=drift=1 | — | — | — | peak 0.92, NaN-clean |

Acceptance criteria for the port (Layer-E-style, to be numbered on ingest): K-sync reproduces the SAW sync transition when J=0 (R ≥ 0.9 at K=+0.9); J alone raises max(R₊,R₋) ≥ 0.35 from a scattered start; K+J raises both R and max(R₊,R₋) simultaneously; active-wave regime shows nonzero mean |Δξ| over 1 s; L/R correlation decreases monotonically with J at fixed K; bounded + NaN-clean at extremes. All seed-deterministic.

## 6. Visualization contract (extends SPEC §5.6)

The ring is the display and the signature image: a circle, each unit a dot at its spatial angle ξ_i, **coloured by phase θ_i (hue = θ·360°)**. The four states read at a glance — sync = one colour spread around the ring; splay/async = scattered colours; rainbow = a smooth hue gradient around the ring; active wave = that gradient rotating. L/R axis labelled (pan). Two centre vectors show W₊ (cyan) and W₋ (magenta) — length = R±, angle = ψ±. Readout: phase R, and space-phase order R₊/R₋. Phase→hue is the one place the palette yields to a full colour wheel, by necessity — the rainbow *is* the information.

## 7. Integration & relationships

- **Shared force-core:** consumes the same distribution machinery and the ADR-004 K-normalization; adds the W± accumulators and the ξ state. Under the ADR-045 axes (PROPOSED) it is a concrete **spatial topology** (distance-on-a-ring coupling) crossed with a **two-term coupling function** — i.e. it is a point in the (Γ, W) space, not a separate framework.
- **Sibling to the parked grain swarm:** the grain engine's position/pan swarm is the same ξ dynamics without the audio-phase coupling; building the swarmalator delivers that spatial behavior as a special case (J acting on position alone).
- **Mod sources:** R₊, R₋, and the mean spatial angle join R/σ/collapse as modulation sources.

## 8. Parked (register)

- **2D swarmalators** (full plane, not ring) for quad/surround or an XY spatial pad — the ring is the stereo reduction; the plane is the immersive version.
- **Distance-weighted phase coupling** (the original OHS 1/|x−x| kernel) for stronger spatial-locality effects, with the softening/singularity care its kernel needs.
- **Chimera-on-the-ring:** localized coherent arc coexisting with a turbulent arc (nonlocal ξ coupling) — chimera you can point to in the stereo field.
- **Swarmalator drift shaper / onset lock** on ξ (spatial attack gesture: image gathers to a point then blooms across the field), mirroring the phase-domain onset lock.

## 9. Prior art

Swarmalators: O'Keeffe, Hong & Strogatz, *Nature Communications* 2017. Ring reduction and solvable 1D variants: Sar, Ghosh et al. This engine's contribution is the **audio instrument mapping** (θ→tone, ξ→stereo, the four states as playable timbre/space regimes, seeded determinism, the shared-core integration) — cite the lineage per the prior-art posture (ADR-044); claim the instrumentation and architecture, not the base model.
