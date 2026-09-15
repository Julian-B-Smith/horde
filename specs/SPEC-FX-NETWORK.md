# SPEC — Morphable FX Network (working title: NETWORK / name TBD)

Status: draft v0.1 · 2026-08-28
Origin: browser experimentation studio (`network-lab-v0.html`), validated hands-on.
Governance: autonomous-paradigm doctrine (spin-up survey, `./verify`, phase-gated
ROADMAP, DECISIONS.md, seeded determinism). This document is the pre-spin-up spec.

---

## 1 · Identity

A single device family built on one idea: **a network of frequency/phase/time
operators — shifters, delays, allpasses, diffusers, combs, filters — that can be
freely composed, reordered, placed inside or outside feedback, and *morphed as a
whole*.** Bode, Scrumulator, freeverb, Disperser-style dispersion, and shimmer
are not features; they are *points in the patch space* this device spans, shipped
as presets.

Differentiators (each already demonstrated in the lab):
1. Topology as a playable parameter — same module, different loop position,
   categorically different behavior (static EQ ↔ damping; blur ↔ per-pass smear).
2. Whole-patch morphing between saved states as a first-class, automatable control.
3. Continuous *algorithm* morphs, not just parameter morphs (filter LP→BP→HP
   today; linear-shift↔ratio-shift and drawn spectral remap on the roadmap).
4. Deliberate access to the critical regime: loop nonlinearity turns the
   decay/sustain boundary into a bifurcation (freeze-fog). Expose it, don't hide it.

## 2 · Product surfaces

| Surface | What | Notes |
|---|---|---|
| S1 `netcore` | C++ DSP core library: modules + rack VM + patch schema | No UI, no host deps. FOUNDATIONS-conformant module ABI. This is the FX-module deliverable other synths consume. |
| S2 Plugin | Standalone effect plugin | CLAP-native first (HYPERSAW precedent); VST3 via clap-wrapper; AU later. GUI phase-gated. |
| S3 Lab | The browser studio, maintained as the R&D twin | Same patch JSON, same module semantics. Prototyping + parity oracle, never feature-lags on DSP semantics (UI may lag). |

## 3 · Architecture

### 3.1 Rack VM
- One processor owns the whole network; modules execute per-sample in graph
  order. **No host-graph nodes inside the network** — per-sample feedback is the
  device's reason to exist.
- Routing tiers (phase-gated):
  - **R0 (shipped in lab):** serial chain inside one global feedback bus.
  - **R1:** parallel branches with per-branch gain + sum nodes (unlocks true
    8-comb freeverb, wet/dry per branch, mid/side later).
  - **R2:** arbitrary directed graph with feedback edges; each feedback edge gets
    an implicit 1-sample delay and a declared loop policy (see 3.4).
- Stereo: R0 core is mono-processed dual or true-stereo per module (module
  declares channel behavior); R1 introduces stereo divergence (comb detune,
  decorrelated diffusers).

### 3.2 Patch = data (schema v1)
```json
{ "v": 1,
  "chain|graph": [ {"id":"m3","type":"shifter","enabled":true,"params":{...}} ],
  "fb": {"gain":0.5,"damp":0.3,"clip":1},
  "mix": 0.7,
  "mods": [ {"type":"lfo","rate":0.3,"depth":0.2,"shape":"tri","target":"m3:shift"} ] }
```
- Stable per-module `id`s; all references (mod targets, wires) are id-based.
  *(Empirically forced: positional targets silently detached on reorder.)*
- Presets stored id-free with positional targets, translated to ids on load.
- Versioned; loaders migrate old versions forward, never sideways.
- DAW state chunk = this JSON verbatim. Human-readable state is a feature.

### 3.3 Module ABI (FOUNDATIONS-conformant)
```
prepare(sr, maxBlock)      // allocation here only
reset()                    // clear state, keep params
tick(x) / process(block)   // RT-safe, no alloc, no locks
paramSpec()                // static: key, range, scale, default, smoothing class
setTarget(key, value)      // thread-safe target write; smoothing internal
latency() -> samples       // 0 for all v1 modules; PV modules will report
channels() -> mono|stereo|mono-to-stereo
```
- Smoothing classes standardized: `fast` (~3 ms), `time` (~50 ms, for delay
  lengths/window sizes), `snap` (stepped ints). One implementation, library-wide.
- Every param is a mod target by construction. Max exposed mod surface is
  ecosystem doctrine.

### 3.4 Feedback plumbing (hard requirements, all empirically earned)
- **DC/infrasonic blocker in every feedback path** (~20–25 Hz one-pole HP).
  Finding: doppler maps DC→DC, comb-bank DC gain ≈ 1/(1−decay) ≈ 5–6×; any
  global loop pumps DC to the rails without this. Non-optional, non-exposed.
- **NaN/Inf flush + hard clamp (±4)** per loop sample. Engine must be
  un-killable under any patch; "stability fuzzing" is a verify gate (see §10).
- **Loop nonlinearity is a declared element**, not a safety afterthought: clip
  off = linear loop (decays or clamps); clip on = tanh in loop → bifurcation
  into self-sustain (freeze). Ship as a proper saturator element with drive,
  and document the critical-point behavior. Future: soft-knee, wavefolder,
  bit-crush as alternate loop nonlinearities.
- Denormal policy: FTZ/DAZ on the audio thread (C++); +tiny-noise fallback.

## 4 · Module roster

### v1 (ported from lab, all validated)
| Module | Guts | Notes |
|---|---|---|
| freq shift | Hilbert pair (Niemitalo allpass cascades) + SSB | Phase relaxes to 0 at rest (deterministic zero-state). Known: at shift 0 + parallel mix it *is* an 8-pole phaser — document as feature. |
| ratio shift | Dual-tap sin²-crossfade doppler delay | `mix` param = ladder-send (shimmer requires partial send — empirical). |
| delay | Interp line, internal damped fb, mix | |
| allpass | Single Schroeder stage, ±g | |
| diffuser | 4 × series Schroeder, size + blur(g) | = freeverb diffusion half |
| comb bank | 4 × parallel damped comb | = freeverb comb half; R1 expands to 8 + stereo detune |
| filter | TPT SVF, cutoff/res, continuous mode LP→BP→HP | Response verified (Goertzel) |
| disperser | up to 32 × series biquad allpass | |

### Committed roadmap modules
- **Spectral remapper** — PV; every bin maps through a drawn curve `f→f'`.
  Unifies shift (f+s), pitch (r·f), folding, inversion. The device's flagship.
  Latency-reporting; quality tiers (FFT size).
- **FDN reverb core** — orthogonal-matrix FDN (reverb-station lineage). Finding:
  comb banks in outer loops have peaky unity-crossings → knife-edge decay;
  graceful long tails require uniform sub-unity loop gain. This module is the
  answer, and reverb-station becomes a consumer/donor.
- **Loop saturator** (promoted from fb-bus flag, see 3.4).
- **Envelope follower + S&H mod sources**; **pitch-tracking mod source**
  (Tonality-core seam).
- Candidates (unscheduled): granular pitch cloud, barberpole phaser macro,
  spring/dispersion hybrid, spectral freeze.

## 5 · Modulation & morph

- **N LFOs** (v1: 2–4), each id-targeted, shapes incl. S&H; depth in
  spec-relative units (portable across param ranges).
- **Laws / coupling models**: macro → param via breakpoint curves, and
  param↔param coupled models. This is FOUNDATIONS' "intermediate models are
  first-class" brief applied here; the shift-mode macro (linear → musical ratio
  → extreme) is the canonical example and ships as a factory law.
- **Morph is a modulator**: morph position is one automatable plugin parameter.
  - Matched structures: per-param interpolation, log-domain for log-scaled params.
  - Mismatched structures (roadmap): run both patches, equal-power crossfade
    wet — "structural morph" as a distinct, honest mode.
  - Slots A/B v1; XY four-corner (quantum-morph seam) explicitly deferred.
- Preset save/slot/read-only-morph interaction model carried over from lab
  (proven interface; see K-1 for open client bug).

## 6 · Analysis & instrumentation

Lab (full suite): live spectrogram, offline IR of current patch, tap-any-wire
(R1), **batch renderer** — sweep any param across N values against deterministic
test signals (impulse / sweep / loop), render spectrogram contact sheet. The
shimmer tuning session (27-point grid search with energy-envelope criteria) is
the template: *automated search with declared perceptual criteria is a core lab
workflow, not a nicety.*

Plugin (minimal): output spectrogram, loop-level meter with
decay/critical/freeze indicator (the bifurcation is user-facing; show which side
they're on).

## 7 · Presets

- Factory: `bode`, `freeverb` (+ exploded variant), `scrumulator-ish`
  (labeled impression), `shimmer fog` (freeze-fog; documented supercritical),
  `metal veil`, plus one per roadmap module on landing.
- Preset = full patch JSON + name + description + tags. User banks are files.
- Every factory preset carries a one-line topology rationale (the lab's card
  anatomy lines, extended). Teaching is part of the product's identity.

## 8 · Plugin deliverable

- **Format:** CLAP-native; VST3 via clap-wrapper (validate in Live + Bitwig +
  Reaper); AU when demand exists. Standalone app via host wrapper, low priority.
- **Params exposed to host:** mix, fb gain/damp, morph position, 8 macro slots
  (law-mapped). Module params host-exposed via dynamic param pool (CLAP handles
  this well; VST3 fallback = fixed pool of 128).
- **State:** patch JSON chunk (schema-versioned).
- **Perf budget:** full v1 chain (8 modules + fb bus) ≤ 3% of one core @48k/128
  on the dev machine (define machine in manifest); PV remapper budgeted
  separately. Per-module budgets in `paramSpec` metadata; CI perf gate.
- **Latency:** 0 for v1 roster; reported when PV modules enter a patch (host
  PDC). Patch latency = max path latency; document the R2 implication.
- **Sample-rate independence:** all times in ms/Hz internally; golden renders at
  44.1/48/96 must match within tolerance envelopes.

## 9 · FOUNDATIONS seams (exhaustive, per seam-audit format)

1. Module ABI ↔ FOUNDATIONS extension-point contract (this project pressure-tests it).
2. Smoothing classes → library-wide standard (donate).
3. Mod system: LFO/targets/laws ↔ FOUNDATIONS matrix + intermediate-model layer
   (consume once it exists; lab implementation is the stopgap and the brief).
4. Patch schema / preset hierarchy ↔ scoped-preset R&D (this device is a
   single-scope consumer; multi-scope arrives via synth integration).
5. Loop plumbing (DC blocker, NaN policy, clamp, loop-nonlinearity element) →
   donate as library primitives; reverb-station and any feedback synth consume.
6. FDN core ↔ reverb-station (shared implementation, two products).
7. Tonality-core ↔ pitch-quantized shift amounts, key-aware ratio laws.
8. Adaptive-state brief ↔ future performance-responsive patches (declared
   memory shapes for e.g. input-level-adaptive fb). Deferred, seam named now.
9. netcore as importable FX in synth ecosystems (the bass sibling, the canvas
   sibling's regions, the terrain sibling) — the "drop a reverb I like into an
   existing synth" doctrine test. *(Intake edit 2026-09-14, ADR-014 alias rule:
   private siblings are aliased in tracked files; the map is local.)*

## 10 · Verification (`./verify`)

- **fast:** build + unit DSP (filter/shift response via Goertzel, allpass
  flatness, comb tuning, DC-blocker efficacy), schema round-trip, id-retarget
  invariants, morph endpoint/midpoint math. *(All of these already exist as the
  lab's Node harness — port, don't reinvent.)*
- **full:** golden renders (per preset × per test signal, tolerance envelopes);
  **browser↔C++ parity** on shared patches (the lab is the reference
  implementation until C++ overtakes it, then roles swap — decision logged);
  stability fuzz (≥10⁴ random patches × random automation, zero NaN/Inf, output
  bounded); perf gate; 6-second energy-envelope classification per factory
  preset (decay / sustain / runaway must match its declared class).
- Determinism: seeded noise sources; no wall clock in core; S&H LFO seeded.

## 11 · Known issues at spec time

- **K-1** Morph slider inert on Julian's mobile client despite full headless
  pass of the interaction path. Plan: on-page debug readout (state + last-event
  log) to diagnose in situ; suspect webview event/render quirk, not logic.
  Blocking nothing on S1/S2; must be resolved before lab is called the
  reference UI.
- **K-2** Main-thread fallback engine can starve on heavy chains — worklet path
  is canonical; fallback is a compatibility courtesy.
- **K-3** `scrumulator-ish` fidelity unknown (topology inferred from a video
  description). Revisit if source becomes available; keep the honest label.

## 12 · Milestones

- **M0 — Lab hardening:** K-1 debug readout; batch renderer; R1 parallel
  branches; loop-saturator element; preset export as file. Gate: lab can run a
  declared parameter search end-to-end and reproduce it from its own logs.
- **M1 — netcore:** C++ port of v1 roster + rack VM + schema; Node-harness tests
  ported into `./verify`; parity oracle green against lab goldens.
- **M2 — Plugin alpha:** CLAP shell around netcore, R0+R1 routing, morph param,
  factory presets, headless-safe GUI-less operation + minimal generic editor.
- **M3 — Flagship DSP:** spectral remapper + FDN core + structural morph.
- **M4 — Product:** GUI (framework decision logged first), VST3 wrapper
  validation, preset browser, manual (topology-teaching format).
- Each milestone = phase gate: green verify + acceptance criteria + trace.

## 13 · Open decisions (to DECISIONS.md at spin-up)

1. Name / brand.
2. GUI framework (CLAP GUI + ?: JUCE-for-UI-only vs. imgui vs. web-view).
   Constraint: must render the topology as the hero, not a knob grid.
3. PV implementation (own vs. library) for the remapper.
4. Oversampling policy for loop nonlinearities (likely 2× on the fb bus only).
5. Repo layout: `netcore` + `plugin` + `lab` mono-repo vs. split (cross-repo
   doctrine says writes-stay-home; mono-repo likely wins for parity testing).
6. Whether the lab's engine factory remains the schema source of truth
   post-M1, or the C++ paramSpec generates the lab's defs.
