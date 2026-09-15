# ORBITAL — N-body gravity modulator
Recommendation spec for integration into a synth plugin mod bank
Working name: ORBITAL (naming provisional). Prototype: `gravity-modulator.html` (browser, Web Audio).

---

## 1. What it is

A modulator module that hosts 2–8 bodies on a unit square. Each body has mass, position, and velocity; the bodies attract each other under softened Newtonian gravity and are integrated in a vacuum (no drag by default). Each body's position — and derived quantities like speed and acceleration — are exposed as modulation sources. The user never "draws" a modulation shape; they set up a physical system and route what it does.

The musical value comes from three regimes the same law produces:

- **Bound orbits** — periodic or quasi-periodic motion: a multi-LFO whose components are phase-locked by physics, not by a clock.
- **Close approaches** — slingshots: sharp, asymmetric spikes in speed and acceleration, exactly where hand-drawn envelopes are hardest to make convincing.
- **Chaos** — three or more free bodies never repeat: an evolving, non-random, non-periodic source with long-range structure.

## 2. Placement in the architecture

In FOUNDATIONS terms, ORBITAL is a **coupling model**, not a plain source. The bodies are not parameters; they are an intermediate state whose observables become sources. This places it squarely in the "matrix = routing, models = coupling" layer:

- The **field** (universe + bodies) is one model instance with its own state.
- Its **observables** (§4) are registered as mod sources in the matrix like any LFO.
- Its own **parameters** (§3) are mod targets, which makes the model reflexive (§7).
- It qualifies as **adaptive state** under the second FOUNDATIONS brief: state persists across notes, has a declared memory shape (*fading* if drag > 0, *total* in a true vacuum), and must satisfy replay determinism (§6).

Scope: instantiate at **global** scope by default (one field per plugin instance). Offer **per-voice** instantiation as an opt-in flag on the model, with a note-on reset policy (§6.3). Per-voice is cheap (§5) but most patches want one shared field so voices share the gesture.

## 3. Parameters (patch state)

### 3.1 Field

| Parameter | Range | Default | Notes |
|---|---|---|---|
| Gravity `G` | 0.001 – 1.0, log | 0.025 | Overall force scale. |
| Softening `ε` | 0.005 – 0.5 | 0.06 | Plummer softening, r² → r² + ε². This is the character knob: small ε = violent slingshots, large ε = lazy swells. Never allow 0. |
| Time scale | 0 – 4×, plus tempo-relative option | 1× | 0 freezes. Tempo-relative mode scales dt by host BPM so orbits track tempo changes proportionally (not sync — there is no clock to sync to). |
| Drag | 0 – 1 | 0 | Velocity decay, exp(−3·drag·dt). 0 is the vacuum. Nonzero makes the system converge to a static configuration — useful as a "settle after a kick" behaviour. |
| Edges | bounce / wrap / open | bounce | See §8. |
| Centre-of-mass lock | on/off | on | Subtracts COM position and velocity each step (free bodies only; disabled automatically when any body is pinned). Prevents the whole system drifting off the pad. |
| Edge cushion | 0 – 1 | 0 (off) | *(Added 2026-09-15, ADR-165 A1/A2.)* A drag band that rises smoothly toward each wall: with d the distance to the nearest wall and w the cushion width, velocity decays by exp(−12·cushion·u²·dt), u = max(0, 1 − d/w). A damping, not a potential — it only touches what reaches the band, so interior orbits are unchanged; it slows orbits that get too extreme before they bounce. Inert in wrap mode. |
| Cushion width | 0.02 – 0.3 | 0.1 | Width of the band, in field units. |
| Max speed | fixed constant | 3.0 units/s | Safety clamp, not user-facing. |
| Body count | 2 – 8 | 3 | |

### 3.2 Body (× N)

| Parameter | Range | Default | Notes |
|---|---|---|---|
| Mass | 0.1 – 16, log | 1 | Modulatable (§7). Mass 0 is not permitted; a massless test particle can be approximated by 0.1 with G scaled. |
| Pinned | on/off | off | Fixed in space; still exerts gravity. A pinned body is a gravity well. |
| Home position (x₀, y₀) | 0 – 1 each | preset | Initial condition. Also the target for "return home" (§7). |
| Initial velocity (vx₀, vy₀) | ±2 each | preset | Initial condition. Store as polar (speed, angle) in the UI; keep Cartesian internally. |
| Output smoothing | 0 – 50 ms | 5 ms | One-pole on the observables at control rate. |

Bodies are indexed 1..N; the matrix addresses sources as `ORBITAL.body[i].x`, etc.

## 4. Observables (mod sources)

Expose per body:

| Source | Range | Meaning |
|---|---|---|
| `x`, `y` | 0..1 unipolar (and ±1 bipolar variant) | Position. y is flipped so up = higher. Clamped in open-edge mode. |
| `speed` | 0..1 | |v| / maxSpeed. Peaks on close approach; the primary "gesture" source. |
| `accel` | 0..1 | |a| normalised by a running maximum with slow decay. Sharper than speed; leads it in time. Use for transients. |
| `near` | 0..1 | 1 − (distance to nearest other body), softened. Proximity as a continuous gate. |
| `angle` | 0..1 wrapping | atan2 of position relative to field centre. A free-running phasor whose rate is physical. |

Expose per field:

| Source | Range | Meaning |
|---|---|---|
| `energy` | 0..1 | Kinetic energy, normalised. A single "how excited is the system" signal. |
| `spread` | 0..1 | RMS distance of bodies from COM. Collapsed vs dispersed. |
| `pair[i,j].dist` | 0..1 | Optional, advanced: distance between any pair. |

Default routing suggestion shipped with the module: `body[1].x → filter cutoff`, `body[1].y → resonance`, `body[2].x → pan`, `body[1].speed → drive` — one body doing timbre, one doing space, and the slingshot doing the accent.

## 5. Numerics

- **Integrator:** velocity Verlet (kick–drift–kick), as in the prototype. Symplectic; energy stays bounded over long runs. Do not use forward Euler — it pumps energy and the system explodes within minutes.
- **Physics step:** fixed `dt_phys = 1/960 s`, independent of sample rate and block size. Accumulate wall time per audio block and run as many fixed steps as fit (cap 32 per block; if exceeded, drop the remainder rather than spiral). This is the parity-critical decision: the prototype uses 1/480; adopt 1/960 in C++ and regenerate goldens rather than inherit the browser value.
- **Precision:** double for positions, velocities, and the force loop. Float is fine for the exported observables.
- **Cost:** O(N²) pair loop, N ≤ 8 → 28 pairs → ~30 sqrt per step, ~1000 steps/s. Negligible; per-voice instantiation at 16 voices is still under 1 % of a core.
- **Output stage:** observables computed once per physics step, one-pole smoothed at control rate (block or sub-block), then delivered to the matrix like any other control-rate source. No audio-rate output in v1.
- **Safety:** ε > 0 guarantees no singularity; max-speed clamp guarantees no ejection blow-up; NaN guard on every step (reset field to home if any coordinate is non-finite); flush denormals in the smoothing filters.

## 6. State, determinism, and lifecycle

### 6.1 State categories
- **Patch state** (§3): field parameters, body definitions, initial conditions. Saved in the preset.
- **Transient state**: current positions and velocities. Not saved by default.
- **Adaptive state**: the same positions/velocities *when the "persist across sessions" flag is set* — the user may want a chaotic patch that picks up where it left off. Store as an opaque blob; version it.

### 6.2 Replay determinism
The system is chaotic: two runs differing in the last bit diverge. Determinism therefore requires (a) fixed dt, (b) identical step count per block, which means wall-time accumulation must be replaced by sample-count accumulation (`steps = floor((samplesElapsed / sampleRate) / dt_phys)` with carried remainder), and (c) no use of `float` in the force loop. With those three, offline render == realtime render for a given start state. Document that a *different sample rate* produces a different (but equally valid) trajectory, since block boundaries shift step timing by up to one step; this is acceptable and should be stated, not hidden.

### 6.3 Reset policies (user-selectable)
- **Free-running** — never resets; the default.
- **Transport** — reset to initial conditions on host play start. Makes song renders repeatable.
- **Note-on (global scope)** — reset on first note after all notes off (the "legato" rule).
- **Note-on (per-voice scope)** — each voice owns a field, reset on its note-on, with optional velocity → initial-speed scaling (velocity kicks the orbit).
- **Manual** — reset only via the panel button or a trigger input.

Reset means: positions ← home, velocities ← initial velocity, trails cleared, smoothing filters primed to the new observable values (no glide from the old position).

## 7. Reflexive and performance modulation

The module's own parameters are legitimate matrix targets. Recommended exposure, in order of usefulness:

1. **Mass** (per body) — most interesting. Another body's `y` → this body's mass makes the field self-referential without going unstable, because Verlet tolerates slowly varying mass. Rate-limit mass changes (one-pole, 20 ms) to avoid impulsive energy injection.
2. **G** — global intensity. Envelope → G turns a static configuration into a collapse on note-on.
3. **Time scale** — freeze / slow-motion as a performance gesture.
4. **Drag** — macro from vacuum to settle.
5. **Softening** — sweeps between slingshot and swell character.

Two trigger inputs, addressable from the matrix or MIDI:

- **Kick** — add an impulse to a chosen body (magnitude × direction; direction random, toward centre, or fixed). Note-on → kick is the single best default for per-note life in a global-scope field.
- **Reset** — as §6.3.

Performance surface: a body can be **grabbed** by a controller. In HORDE this should go through the intent bus — the XY pad writes an intent `ORBITAL.grab[i]` = (x, y); while an intent is active the body follows it (as the prototype's drag), and on release inherits the pad's recent velocity (throw). Do not bind the pad directly to position; the intent layer is what allows the pad's home-offset semantics and corner scoping to apply.

Inertia system: bodies already *are* inertia, so ORBITAL should not sit behind the shared inertia stage; register it as inertia-exempt so the two don't double-lag.

## 8. Edge behaviour

- **Bounce** — elastic reflection at the unit square. Preserves energy, keeps everything visible and in-range. Default because it is the only mode where a beginner can't lose a body.
- **Wrap** — toroidal; gravity is computed on the minimum-image displacement so bodies attract across the seam. Position observables jump at the seam — flag this in the UI, and prefer `angle`/`speed`/`near` in this mode.
- **Open** — no walls; positions unbounded, observables clamped. Needed for true celestial-mechanics presets (figure-eight, escape trajectories). Pair with COM lock; without it the field drifts away. With a pinned body, COM lock is off and escaped bodies are gone until reset — accept this; it's what the mode means.

## 9. UI

The field view is the module. Recommendations from the prototype that should survive:

- Square canvas, subtle 8×8 grid with the centre cross emphasised — the cross is what tells you where 0.5 is.
- Bodies drawn with radius ∝ √mass, each in its own colour, name adjacent. Pinned bodies show a hollow centre.
- Trails (last ~220 steps, alpha ramp) and force lines (alpha ∝ force) — these are not decoration; they are the only way to read the system's state at a glance. Toggle, default on.
- Projection ticks on the bottom and left edges showing each body's current x and y — this is the visible link between the picture and the modulation values.
- *(Added 2026-09-15.)* A small x/y position graph on every body's card — the two observables as strips over the last few seconds (x solid, y dashed, in the body's colour) — so the modulation a body emits is readable beside the orbit that makes it. In the plugin this is the body card's live readout.
- HUD: simulation time and total energy. Total energy drift is the health check.
- Drag / throw / double-click-to-add exactly as prototyped.

Per-body mapping belongs in the matrix, not in per-body dropdowns as the prototype does; the dropdowns were an expedient for a standalone demo. The body card in the mod bank should show mass, pin, home, and live readouts of its observables, and accept drops from the matrix's target list.

## 10. Presets

Ship presets as **initial-condition sets**, scaled to the unit square with velocities in field units:

- **Binary** — two equal masses in a circular orbit. The reference "stable LFO" preset.
- **Sun + planets** — one pinned mass, two bound orbits, one comet. Demonstrates pinning and mixed periods.
- **Figure eight** — Chenciner–Montgomery choreography; three equal masses, open edges, ε ≈ 0.01, G = 0.05, velocities scaled by √(G/s) where s is the spatial scale (0.28). The show-off preset; it also proves the integrator is doing its job.
- **Four-body chaos** — the "never repeats" preset.
- **Collapse** — bodies at rest on a ring, G modulated by an envelope. Demonstrates reflexive modulation.

Preset mass normalisation: when the user changes body count on a loaded preset, new bodies get mass = median of existing masses, positioned on the largest empty region, at rest.

## 11. Deliberate divergences from the prototype

| Prototype | Plugin | Reason |
|---|---|---|
| dt = 1/480 s, wall-clock accumulation | dt = 1/960 s, sample-count accumulation | Determinism (§6.2) and finer slingshot resolution. |
| "Add node" position from `Math.random` | seeded mulberry32 (lab: seeded 2026-09-15, ADR-165's sanctioned edit) | Initial conditions are patch state; SPEC §5.7. |
| Float64 in JS (implicit) | Double, explicit | Parity stability. |
| Per-body target dropdowns | Matrix routing | Architecture (§2). |
| Only `x`, `y` exported | Full observable set (§4) | Speed/accel are where the gestures are. |
| Drag via pointer | Drag via intent bus + throw | Performance surface (§7). |
| No reset policy | §6.3 policies | Host integration. |
| Trail length in steps | Trail length in seconds | Independent of dt. |

Because of the dt change, the prototype is a **behavioural** oracle, not a bit-parity oracle: the same preset should produce the same orbit family, period within ~1 %, and identical qualitative events (bounce order, close-approach count over 30 s). Write the parity test at that level.

## 12. Open questions for Julian

1. **Global vs per-voice as the default.** This spec says global. If the intended sound is "each note has its own little solar system," flip it.
2. **Should `accel` normalise by a running max or by a fixed ceiling?** Running max adapts but makes the first slingshot after reset look bigger than later ones. Fixed ceiling is predictable but may clip on tiny-ε presets.
3. **Is mass-modulation worth the rate limit?** The 20 ms one-pole makes reflexive patches stable but blunts "mass jumps on note-on" effects. Could be a per-body option.
4. **Wrap-mode observables.** Should `x`/`y` be replaced by sin/cos of the toroidal coordinate in wrap mode so nothing jumps? It's the correct answer mathematically but changes the meaning of the source between modes.
5. **Naming.** ORBITAL, WELL, and SYZYGY were all considered. ORRERY is taken by the sequencer.
