# HORDE — Swarm Glitch Modules

**Spec v0.1 — DECOHERE · STALE FIELD · NECROSIS**
Status: validated in browser prototype (`reference/decoherence-lab.html`, Aug 2026). This document is the handoff spec for implementation inside the SWARM engine. These are **state-tier glitches**: they operate on the Kuramoto voice-swarm's dynamical state, not on the audio buffer. They cannot live in the FX rack; they must be implemented where the coupling law runs.

---

## 0. Substrate and conventions

Per-voice state: phase `θ_i`, natural frequency `ω_i = 2π·f0·(1 + spread·d_i) + drift_i`, where `d_i` is the existing detune-distribution shape. Mean field: `r·e^{iψ} = (1/N)·Σ e^{iθ_i}`.

**K convention.** This spec uses HORDE's normalized coupling `K ∈ [−1, +1]`: `+1` fully coupled (sync attractor), `−1` fully anti-coupled, where negative K pulls phases toward **equidistant splay states** rather than acting as naive repulsion. Note: the prototype approximated negative K as raw repulsion (`−K·r·sin(ψ−θ_i)`); the production implementation should use the engine's existing splay-target semantics. Consequence worth preserving deliberately: HORDE's decohere will be an *ordered* shatter (collapse toward splay) rather than the prototype's disordered scatter. If A/B testing shows the disordered variant is musically distinct and valuable, expose it as a mode (`disorder: ordered | chaotic`); otherwise ship ordered only.

**Smoothed observers.** Maintain one-pole-smoothed `r̄`, `ψ̄`, and mean-field angular velocity `dψ̄` at audio rate, τ ≈ 5 ms. STALE FIELD captures from these, never from instantaneous values (instantaneous ψ at capture time is noisy when r is low).

**Interaction ordering per sample:** (1) NECROSIS mutates per-voice `ω_i`/`amp_i`/`θ_i` per its failure models → (2) STALE FIELD selects the field the coupling term sees → (3) DECOHERE overrides the effective K → (4) integrate. All three compose freely.

**Bypass transparency.** Each module disengaged must be a true no-op on the K path and voice state (bit-transparent against the engine with modules absent). This is testable under `./verify`.

---

## 1. DECOHERE — utility event

Coupling is driven to negative territory for a burst; the swarm collapses from lock toward splay, then re-condenses when K restores. The heal is emergent — its duration depends on how much margin the current K has over the detune spread, which is a feature: the same burst reads differently in different patches.

`K_eff = clamp(−K_base · strength, −1, +1)` while active; otherwise `K_eff = K_base`.

| Param | ID | Range | Default | Unit | Notes |
|---|---|---|---|---|---|
| Strength | `dec.strength` | 0.5 – 4.0 | 1.5 | × | clamped post-multiply; >1 only matters when K_base < 1/strength |
| Burst | `dec.burst` | 10 – 1000 | 150 | ms | one-shot duration |
| Trigger | `dec.trig` | event | — | — | MIDI note-on option, manual, mod-matrix event, stochastic rhythm process |
| Hold | `dec.hold` | gate | off | — | gate input; overrides burst timer while high |

Retrigger during an active burst restarts the timer (no stacking). Trigger latency: next sample. The stochastic rhythm process should be a first-class trigger source — probabilistic decoherence on rhythmic hits was the strongest musical use in prototype testing.

**Open question (implement behind a flag, default off):** `dec.healboost` — temporary K multiplier (e.g. 1.5×, 80 ms ramp-down) applied after burst end, for patches near the sync threshold where natural heal time is unbounded. Only needed if sound design demands predictable recovery.

---

## 2. STALE FIELD — flagship

The mean field the voices couple to is replaced by a ghost: magnitude, angle, and rotation rate captured at engage time, advanced at the captured rate thereafter. Voices keep serving a dead attractor. The signature behavior — the reason this module exists — is **note changes while engaged**: natural frequencies pull toward the new pitch while the ghost field pulls at the old note's rotation rate, producing a strained, beating, wrong-but-coherent tone. Preserve this interaction; do not "fix" it by re-capturing on note change.

On engage: `ψ_f ← ψ̄`, `dψ_f ← dψ̄`, `r_f ← max(0.15, r̄)` (floor prevents a null ghost when engaged during low-r moments). Per sample while engaged: `ψ_f += dψ_f`. Coupling term uses `(r_f, ψ_f)`.

**Extension beyond prototype — Haunt.** Continuous blend instead of binary engage: the coupling term becomes `haunt · K·r_f·sin(ψ_f−θ_i) + (1−haunt) · K·r̄·sin(ψ̄−θ_i)` (blend the two *torques*, not the field vectors — blending angles through zero-crossings of r is ill-defined). `haunt = 1` reproduces the prototype. Intermediate values give a swarm partially haunted by its past, which should pair well with inertia on the haunt parameter itself.

| Param | ID | Range | Default | Unit | Notes |
|---|---|---|---|---|---|
| Engage | `stale.engage` | latch/gate | off | — | capture happens on rising edge |
| Haunt | `stale.haunt` | 0 – 1 | 1.0 | — | torque blend; smooth 20 ms; modulatable, inertia-eligible |
| Capture τ | `stale.tau` | 2 – 20 | 5 | ms | observer smoothing; expose in dev builds only |

Disengage is instant (live field resumes; no crossfade needed — the coupling integrator smooths it inherently). Re-engage always re-captures.

---

## 3. NECROSIS — revised: bipolar vitality axis

**Design change from prototype (Julian's direction):** replace the damage-one/heal-all buttons with a single continuous slider that damages voices as it moves one way and heals them as it moves back. Necrosis becomes a modulatable macro rather than an event system — an LFO on it is a breathing sickness; an envelope on it is a build.

Mechanics:

- `necro.vitality ∈ [0, 1]`, default 1.0 (fully healthy). Damaged-voice target count `M = round((1 − vitality) · N)`.
- **Deterministic order.** A seeded permutation of voice indices (seed = patch seed) defines damage order. When M rises, the next voices in the permutation are damaged; when M falls, the most recently damaged are healed first (LIFO). Same slider gesture → same sonic result, every time. The permutation should avoid damaging both extreme-detune voices early (they carry the width); constrain the first ⌈N/4⌉ entries of the permutation to interior voices.
- **Failure assignment** per `necro.failure`: `dropout` (amp decays ~2.2 s⁻¹ exponential with flicker interruptions — random ~50 ms full mutes, p ≈ 0.0012/sample), `drift` (frequency random walk, step variance ∝ age · f0 · 1.2e−4), `rot` (phase jumps, probability and magnitude growing with age; jump ∝ min(3, age)·2 rad), or `random` (per-voice assignment at damage time, seeded).
- **Age** accumulates per damaged voice while damaged; drives severity in all modes.
- **Healing is a recovery, not an undo:** on heal, ramp `amp → 1`, `drift → 0` over 150–300 ms (randomized per voice within that range) and freeze rot jumps immediately. Age resets on full recovery.
- Damaged voices stay in the coupling sum (this is the point — survivors get dragged around the corpses). Dropout voices leave the *audio* mix as amp → 0 but their phase keeps integrating and contributing to the field until healed.

| Param | ID | Range | Default | Unit | Notes |
|---|---|---|---|---|---|
| Vitality | `necro.vitality` | 0 – 1 | 1.0 | — | bipolar action: down damages, up heals; smooth 10 ms; fully modulatable |
| Failure | `necro.failure` | enum | dropout | — | dropout / drift / rot / random |
| Virulence | `necro.virulence` | 0.25 – 4 | 1.0 | × | global multiplier on age-driven severity rates |
| Seed | `necro.seed` | int | patch | — | permutation + random assignments; saved with patch |

Emergent behavior to preserve: at K near the sync threshold, sufficient necrosis pushes the swarm below threshold and the note falls apart on its own. Do not clamp against this.

---

## 4. Implementation notes

Audio-thread only; parameter changes via the existing atomic/event path; no allocation after init. Per-voice added state: `state (u8)`, `age (f32)`, `amp (f32)`, `drift (f32)`, `flickerRemain (i32)` — fits alongside existing voice struct; N ≤ 32 keeps everything trivially in cache. Forward Euler at audio rate is sufficient (prototype-validated); no denormal risk in the phase path, but the observer one-poles and dropout amp decay should use the standard FTZ guard. Zero added latency; no lookahead. Voice-count changes while damaged: clamp M to new N, heal any voice index ≥ N instantly.

Telemetry for UI: expose `r̄`, `ψ̄`, per-voice `θ_i` and state at control rate. The prototype's phase-circle + r-history visualization is the reference UI concept and doubles as the debugging view; strongly recommend building it early — every behavior in this spec is legible on it.

Prototype relationship: `reference/decoherence-lab.html` is the behavioral reference for DECOHERE (chaotic variant), STALE FIELD (haunt = 1), and the three failure models. It is **not** a bit-parity oracle — the K convention differs (§0) and NECROSIS §3 supersedes its button-based interface. Acceptance is behavioral: splay/heal trajectory shape, stale-field note-conflict beating, deterministic necrosis ordering, and bypass bit-transparency.

## 5. Acceptance tests

1. **Bypass null:** all modules disengaged → bit-identical to engine without modules.
2. **Decohere:** from locked state (r̄ > 0.95), burst drives r̄ below 0.3 within 60 ms at strength 1.5, K_base 0.6-equivalent; r̄ recovers to > 0.9 with no discontinuity in output amplitude envelope beyond the splay itself.
3. **Stale field determinism:** engage → note change → disengage, twice with identical timing and seed → identical output.
4. **Stale conflict:** engaged on note A, playing note B, mean beat rate of the output envelope scales monotonically with |f_B − f_A| at fixed K.
5. **Necrosis determinism:** identical vitality gesture + seed → identical output; LIFO healing verified by damaging 4, healing 2, confirming voices 3–4 recovered and 1–2 still damaged.
6. **Threshold collapse:** at K just above sync threshold, vitality sweep to 0.3 with drift mode drives r̄ below lock and it does not recover until vitality returns.

## 6. Out of scope here

Buffer-tier glitch FX (codec sabotage, skip cascade, misread, spectral datamosh) — separate spec, shared FX-rack/standalone-VST territory. Cross-engine state transplants (swarm state driving CHOIR parameters, etc.) — future spec once CHOIR lands.
