# horde's engine: the spec

**Status.** The contract for horde's engine, 2026-10-16.

**Who reads it.** The builder reads this spec and the acceptance tests written from it, and nothing else (BD-10). A different agent writes those tests from this spec before the engine exists. Everything a requirement needs is in here.

**Source.** The record behind every requirement and law is kept outside this spec, by requirement id, in `docs/port/spec-engine-provenance.md`. The builder does not need it.

**The laws are normative.** Where a formula *is* the sound (the coupling law and its taper, member placement, the detune laws, drift, gravity, the ensemble, every blade law, the spreads, the pan law, the envelope's curve, DC removal), Appendix C states it as math with its constants and units, **and Appendix D states the morph's laws the same way, and Appendix E the modulation system's (round 62).** The math is binding, not any way of computing it: the builder is free to compute it fast (single precision, tables, SIMD, any loop order). **A law may change only where every anchor that tests it still passes, and the reason is then stated in this spec.**

**How to read it.**
- Requirements are numbered (`SW-4`, `VO-3`, `RC-2`, `MO-6`, `SB-1`, …); laws are numbered `C.0` to `C.16`, the morph's are Appendix D's rows, and the modulation system's Appendix E's (`MS`, `LF`, `MK`, `MB`, `MR`). Each is testable.
- A tag **[Qn]** or **[Xn]** marks an item still open (section 11); **[OPEN Mn]** marks one a lab or the lead rules before its part is built (11.0, round 62). The spec states what is built until it is ruled; if it is ruled otherwise, that requirement changes and nothing else does. **PROPOSED** marks a value the human will confirm by ear; it is built as written.
- "Must" is a requirement. "Should" is a recommendation the builder may depart from with a stated reason. Numbers carry units. Times are in seconds unless stated.
- Musicians' words are used where they exist. The glossary (section 2) fixes them.

---

## 1. Purpose and scope

**Purpose.** horde is a synthesizer whose oscillators are swarms: the stacked copies a synthesizer calls unison are coupled oscillators that pull toward one pitch or push apart, drift, settle into just intervals, and carry **blades**, windows that cut each cycle of the wave and replace what lies inside them with another sound (hard sync, FM, noise, fold, ring, crush). This spec defines the engine that makes that sound: everything from a played note to the instrument's stereo output before the effects.

The engine must do what a musician expects each control to do to the waveform, and it must sound good to the human. The bank of 83 presets (Appendix B) must stay recognisable and sound good by the human's ear.

**In scope.**
- The **voice**: allocation, stealing, the sustain pedal, mono and legato, the envelopes (ENV 1 to ENV 4), the per-voice signal path, the slot the per-voice filters sit in.
- What lives in a voice: **two swarm oscillators** (each with its members, its coupling and its two blades), the **sub** and the **noise oscillator**.
- The sum of the voices and the engine's output stage.
- The engine's interface to the rest of the instrument (section 7), and the reserved contracts for the modules still to come (7.2).
- The **morph resolver** (Appendix D, ADR-325): beside the engine, it turns the corners and a pad position into values, every Structural key following its quantum boundaries (MO-13) and a source's on switch fading as a level under Blend and Stepped (MO-26). Structural setting sets and their rendering in the voice are after 1.0 (ADR-377).
- The **modulation system** (Appendix E, round 62; ADR-371, ADR-372, ADR-375, ADR-379): beside the engine and the resolver, it computes the global modulators (the global LFOs, the Kuramoto LFO, ORBITAL, the 2-D random) and evaluates the matrix's routes. **In the engine** for it: the per-note LFOs, by Appendix E's LFO law, and the per-sample terms of the engine path's routes (MS-41, MS-43).

**Out of scope** (each meets this engine only through section 7):
- the FX modules and the rack itself;
- the filters' own designs (the engine builds only their per-voice slot, RC-1);
- the macros and intent bus (the modulators and the matrix are the modulation system's, Appendix E, which the engine meets through PA-2, IF-3 and IF-13; the engine computes the envelopes and the per-note LFOs itself, RC-4), **the morph's editor, its corner storage and its pad (its resolver is in scope, Appendix D)**, the pitch system (bend lane, note lane and glide, quantizer, MTS-ESP), the arpeggiator: the engine accepts their outputs and does not compute them;
- true-stereo processing after the voice, the mixer's strip and the master stage;
- the GUI, and the host shell beyond the engine's API.

---

## 2. Glossary

| term | meaning |
|---|---|
| **note** | One key press from the host, identified by its **note id** (CLAP's note id, with its channel and key). Two notes may share a key (MPE). |
| **voice** | One sounding note, instrument-wide. Polyphony counts voices: "8 voices" means 8 notes at once. A voice holds both oscillators, the sub, the noise oscillator, ENV 1 to ENV 4 and the filter slot. |
| **oscillator** | One of the voice's two swarm oscillators (**osc 1**, **osc 2**). |
| **source** | osc 1, osc 2, the sub or the noise oscillator. |
| **swarm** | The set of copies inside one oscillator for one voice. |
| **member** | One copy in a swarm. What other synthesizers call a unison voice. The user-facing label is **Unison** and its count is shown as "voices" in that control only (as Serum and Vital do); everywhere else in this spec the word is member. |
| **base wave** | The wave every member plays before any blade acts on it. |
| **blade** | A window over each cycle of a member's wave, with a centre, a width and a cut rate, inside which the base wave is replaced by a transformation (the blade's **mode**). Each oscillator has blade 1 and blade 2. |
| **carrier** | The signal a blade plays inside its window (for Sync, FM and Ring modes), running at the blade's **cut rate**. |
| **spread** | A blade setting fanned out across the members by a **spread law**, so each member's blade differs. |
| **coupling** | The pull between members of one swarm: positive pulls them toward one locked pitch, negative pushes them toward an even spread of phases (**splay**). |
| **θᵢ** | Member i's phase, in cycles (0 to 1). |
| **R**, **ψ** | The swarm's order parameter: R·e^{iψ} = (1/U)·Σⱼ e^{i·2π·θⱼ}. R is how aligned the members' phases are (0 none, 1 locked), ψ the angle of their mean phase (radians). |
| **R_N** | The U-th order parameter, R_N = \|(1/U)·Σⱼ e^{i·2π·U·θⱼ}\|: high when the members sit in an even splay. R high means sync; R low with R_N high means splay; both low means free. |
| **σ** | The swarm's measured spread: the RMS deviation of the members' natural frequencies (after the detune law and drift, before coupling) from their mean, in Hz, floored at 0.08 Hz. Coupling strength is stated in units of σ. |
| **frame** | Where a blade's centre is measured from: the member's own phase (**member frame**) or the swarm's mean phase (**swarm frame**). |
| **gravity** | The pull of held voices toward just-intonation ratios of one another. |
| **ENV 1 to ENV 4** | The voice's four envelopes, one law (C.12, EN). Each source's level follows one of them, its **governing envelope**: ENV 1 unless the source's `env` selects another (RC-6.5). The modulation system also reads them as sources and routes them (RC-4). |
| **control rate** | About 1,500 updates per second. The interval between updates is the **control interval**, Δ. |
| **ramp** | A value travelling linearly from where it stands to its new value across one control interval. |
| **step** | A value arriving in one sample. |
| **quality** | The oversampling setting: Standard (1×) or High (2×). |
| **internal rate** | The sample rate the oscillators run at: the host rate × the quality factor. |
| **per-voice modulation** | An offset to one parameter for one voice, from the modulation system. |
| **Morphable / Structural / Device** | The three parameter classes: Morphable values blend between morph corners or flip, by the morph's law; Structural values follow their quantum boundaries under every law, one value at a time, flipping by PA-5's brief crossfade (MO-13; ADR-377), a source's on switch fading as a level under Blend and Stepped (MO-26); Device values stay outside the morph. |
| **corner**, **pad**, **position** | The morph's four full sets of values (A, B, C, D), the square they sit on, and the point on it that is heard (MO-4). |
| **structural group** | After 1.0 (ADR-377): the Structural keys rendered together (MO-14): a kernel, a swarm, the sub's or the noise's chain, the filter, an FX slot, or the keys that act by gain. |
| **setting set** | After 1.0 (ADR-377): for a structural group that blends, the distinct settings its corners hold that sound at a position, each with an id and a weight, the weights summing to 1 (MO-17). |
| **note counter** | The count of voice starts (VO-6's "fresh start" rows) since the engine's last reset; it keys each note's random streams (DT-4). |
| **bank** | 83 presets in the bank format (Appendix B), given as input data: the sound reference in character, not in samples. |
| **the envelope presets** | 12 further presets in the bank format (Appendix A), each named for the envelope shape it plays. |
| **the reference renders** | A frozen set of renders of the bank with each render's parameter metadata: input data for the test author only (10.1), never for the builder. |

---

## 3. What the build delivers

| deliverable | what it is |
|---|---|
| The engine's source tree | organised by concept, one home per concept: the blade and its modes defined in exactly one place; likewise the swarm law, the voice, the sources, the output stage. |
| The parameter manifest | the one source of truth from which the engine's parameter table, the plugin's host parameters (a lockfile), the labs' controls and the manual's parameter list are generated. |
| The engine's API | as the suite's harness header fixes it from section 7: note ids, pitch input, per-voice modulation, state, observations. |
| The morph resolver | Appendix D's resolver for 1.0 (MO-1 to MO-13 and MO-20 to MO-27; its set rows, MO-14 to MO-19 and MO-20's set half, and the engine's set rendering, SB-1 to SB-13, are after 1.0, ADR-377), beside the engine, with its own calls (IF-12): corners, position, settings, modes, waypoints, region locks and transport in; `Param` events and the sources' morph levels out. |
| The modulation system | Appendix E's system (MS-1 to MS-45, LF, MK, MB and MR rows), beside the engine and the resolver, with its own calls (IF-13): note events, sources, transport, resolved settings and routes in; per-voice offsets, Control inputs, engine-path terms, per-note LFO settings, the morph position's offset, the erosion flags and voice pitch's quantized contribution out (round 62). |
| The WebAssembly build | the same source compiled for the labs (IF-10). |
| The bank translator | the bank's presets and the envelope presets as patches for this engine, generated by the rules of Appendix B. |
| The renders for judging | offline renders of the bank and the envelope presets (10.3). |

### File layout: requirements of the build

- **BD-1.** One concept per file. A file's name is a word a musician would recognise for what it holds (`blade`, `swarm`, `voice`, `envelope`, `sub`, `noise`, `output`).
- **BD-2.** Each concept exists once: one phase counter per member, one centre-of-swarm measurement, one random-stream facility, one value of each constant.
- **BD-3.** No switch keeps two behaviours of one thing side by side. A behaviour this spec does not ask for is not built.
- **BD-4.** Test and measurement hooks live outside product code; a test observes the engine through its public observation view (IF-10), or through a test build that adds no branch to a product line.
- **BD-5.** The parameter table, host parameters, lab controls and manual list are generated from the one manifest; nothing hand-copies it.

**A proposed layout, which the builder may adapt** (a recommendation, not a mandate):

```
core/
  manifest/   parameters (the one source of truth) and its generators
  api/        the engine's calls, events (transport included, RC-7), state, observation view, GUI snapshot (RC-8), history snapshot and gestures (RC-11)
  morph/      the resolver: weights, glide, the draw, the laws, the modes, waypoints, region locks, the sources' levels (Appendix D)
  mod/        the modulation system: sources, routes, the LFO law (one home for the global and the per-note LFOs), the Kuramoto LFO, ORBITAL, the 2-D random, loops (Appendix E)
  voice/      allocator and stealing, the pedal, the voice, the envelopes, mono and legato, the per-note LFOs and the engine path's terms (MS-41, MS-43)
  swarm/      members, placement, detune laws, coupling, drift, onset, start phase, gravity, pan
  blade/      geometry, modes, waves, FM, mirror, spreads and their laws and rules, interplay, DC
  edges/      the band-limited step and corner tables
  sources/    the source slot (RC-6), sub, noise
  output/     level, non-finite guard, clip latch
  wasm/       the WebAssembly entry the labs load
tests/        the invariant suite, the renders for judging, the CPU measurement
```

---

## 4. Architecture at the user's level

### 4.1 Who owns what

- **One instrument-wide voice per played note** owns: the allocation decision, ENV 1 to ENV 4, the per-voice filter slot, and inside it osc 1, osc 2, the sub and the noise oscillator. One allocator decides for all of them; there is no per-oscillator voice law.
- **Each source hands its voice's signal up before any mixing**; its governing envelope scales it (VO-9), the voice's filters act on it, and the voices are summed after. The one exception is the sub in mono (SN-7): one instance for the instrument, summed with the voices after their filter slots.
- **Members are mono** sources panned across the stereo field. Nothing renders a member separately per channel. True-stereo processing happens after the voice, outside this engine.

### 4.2 The per-voice signal path

```
note id, key, velocity, per-note expressions          pitch input (Hz, per note id, audio rate)
        │                                                        │
        ▼                                                        ▼
  ┌──────────────────────────── one voice ─────────────────────────────────┐
  │  voice pitch × gravity ratio ──► published voice pitch (IF-2)           │
  │  envelopes: ENV 1 to ENV 4 (C.12), each × its velocity factor (VO-8)    │
  │                                                                         │
  │  osc 1: swarm of U members ─► each member: base wave + blade 1 + blade 2│
  │         ─► edge correction ─► DC removal ─► pan (mono member → L/R)     │
  │         ─► Σ members × density compensation ─► gate ─► osc 1 level      │
  │         ─► × its envelope (RC-6.5) ────────────────────────────────┐    │
  │  osc 2: the same, independent ─► osc 2 level ─► × its envelope ────┤    │
  │  sub:   one phase, one shape, eroded ─► tone ─► × its envelope         │
  │         ─► drive ─► DC removal ─► pan ─► gate ─► level ────────────┤    │
  │  noise: per-voice seeded source, colour ─► Resonator ─► Whisper         │
  │         ─► Wind ─► noise filter ─► stereo spread ─► pan ─► gate         │
  │         ─► level ─► × its envelope ────────────────────────────────┤    │
  │                                                                    ▼    │
  │                                     per-voice filter slot (stereo-linked)
  └────────────────────────────────────────────────────────────────────┬────┘
                                                                       ▼
  mono sub: the sub, once for the instrument (SN-7) ─► its own envelope
           ─► its own filter-slot instance, driven by its chosen note (RC-1) ─┤
                                                                       ▼
        Σ voices ─► output stage (Level, guards) ─► stereo out ─► make-up gain (the mixer's, RC-2) ─► FX rack
```

- The oscillators run at the internal rate (host rate × quality) and return to the host rate before the per-voice filter slot; the sub and the noise oscillator run at the host rate at both qualities, so their laws (the bump's fade, the caps, the noise's filters) do not change with `quality`; the sub's drive oversamples by its own factor of 2 (SN-5).
- The sub is drawn in its default order; its `order` switch moves the drive and its DC removal ahead of the tone and the envelope (SN-3). Erode reads the noise before its envelope, level and pan (SN-14).
- Each source's governing envelope acts on it before the filter slot (VO-9), because each source may follow its own envelope (RC-6.5) and one envelope after a shared filter could not do that. While the slot is empty this is the same sound as one envelope after it.
- "gate" is each source's mute and solo (IF-6).
- The filter slot passes the signal unchanged until a filter is assigned (RC-1).
- In 1.0 every Structural key has one value at a time (MO-13), so every stage runs once; two settings of a key sound together only during PA-5's crossfade. Setting sets, which would run a stage once per setting, are after 1.0 (ADR-377).

### 4.3 What lives where

| concern | owner | where specified |
|---|---|---|
| which voice a note gets, stealing, the pedal, polyphony, mono/legato | the engine (voice) | VO |
| the envelopes, ENV 1 to ENV 4, and which one each source follows | the engine (voice) | EN, C.12, RC-6.5 |
| pitch: bend, glide, quantizer, MTS-ESP, transpose | the pitch system, upstream | IF-2, RC-3 |
| gravity | the engine (voice) | SW-11, C.6 |
| the swarm, blades, edges, DC | the engine (oscillator) | SW, BL, ED, C.16 |
| sub, noise oscillator | the engine (voice) | SN |
| filters' sound | the filters | RC-1 |
| source levels, pan, mute and solo, meters | the engine, driven by the mixer | IF-6, RC-2 |
| the global modulators, the matrix and its routes (ENV 1 to 4's included) | the modulation system | IF-3, IF-13, RC-4, Appendix E |
| the per-note LFOs; the engine path's per-sample terms | the engine (voice), from the modulation system's settings and terms | MS-41, MS-43 |
| macros, intents | the intent bus | IF-3, RC-11.1 |
| the morph's laws; its structure's rendering | the morph resolver; the engine (voice) | IF-4, IF-12, Appendix D |
| true stereo after the voice, FX, master limiter | the device | IF-7, IF-8, RC-5 |

---

## 5. Every function and parameter

### 5.1 How to read the manifest

The manifest is data (BD-5). Its keys are frozen from the first release; integer ids are assigned explicitly by the manifest and are append-only. This section is its content.

- **Key** is the stable address. Rows under 5.3 exist once per oscillator (`osc1.…`, `osc2.…`); rows under 5.4 once per blade per oscillator (`osc1.blade1.…` … `osc2.blade2.…`). The rows of ENV 2 to ENV 4's table (5.2) exist once per envelope (`env2.…`, `env3.…`, `env4.…`).
- **Law** is how the normalised form v ∈ [0, 1] maps to the value x in the range [lo, hi]. Every parameter has this normalised form, and the host and the modulation system address it by v (IF-11):

  | law | x(v) | notes |
  |---|---|---|
  | `lin` | lo + (hi − lo)·v | |
  | `log` | lo·(hi/lo)^v | lo and hi nonzero, same sign |
  | `pow` | lo + (hi − lo)·v² | finer near lo |
  | `lin, wraps` | lo + (hi − lo)·frac(v) | circular: an offset past either end wraps, never clamps |
  | `log, off zone` | 0 (off) for v ≤ 0.01; else 0.004·(hi/0.004)^((v − 0.01)/0.99) | blade width: off below 0.004 cycle |
  | `dB` | −∞ (silence) at v = 0; else lo_dB + (hi_dB − lo_dB)·v, applied as 10^(dB/20) | levels; lo_dB is the range's −60 |
  | `step` | option index round(v·(n − 1)) of n options | in the table's option order; in blade 2's lists the option that follows blade 1 ("as blade 1", "blade 1's", "with blade 1") comes first, at index 0 |

  A parameter's display may differ from its law; the law is what the host, modulation, morph and macros move.
- **Class**: `M` Morphable, `S` Structural (follows its quantum boundaries under every law, flipped by PA-5, MO-13; a source's on switch fades as a level under Blend and Stepped, MO-26; ADR-377), `D` Device (outside the morph). [Q17]
- **Rate**: `C` control rate, ramped; `A` also an audio-rate destination; `N` read at a voice's start (VO-6) and held, **as its target value, not PA-1's glide**: a value read once cannot zipper, so a note takes the value that was set even when it was set just before the note (the human's ruling, 2026-10-21, round 28; builder-origin, B518); `T` stepped, changed by the rule of PA-5.
- **PV**: accepts per-voice modulation. Every continuous parameter does; stepped ones do not.
- **Live when**: PA-8's table says when each parameter can affect the sound.
- Rows tagged **[Q31]** are built as written until Q31 is ruled.
- A row whose class says **reserved** holds a key and its id for a feature designed but not yet built. In 1.0 its only v is 0, its later default (for a stepped row its first option); any other v in a state is kept unchanged and read as 0, uncounted by PA-7. **A `Param` event addressed to a reserved row's id is refused and counted by PA-7 like an unknown id** (the human's ruling, 2026-10-21, round 27): it is not a host parameter, and only a state carries its value. It is in the manifest and the state, and it is **not a host parameter** until the feature ships, when its other values are added so that v = 0 keeps its meaning (RC-9.4). One mechanism serves every reservation: the pitch modes, the noise's source mode, the sampler's four rows (SN-13) and the sources' stream selectors (RC-6.7). So a 1.0 preset loads unchanged once the feature ships, and the host never shows a control that does nothing (round 26).

### 5.2 The voice (once per instrument)

| key | label | unit | range | default | law | class | rate | PV | what it does to the sound |
|---|---|---|---|---|---|---|---|---|---|
| `voice.mode` | Voicing | — | poly, mono, legato | poly | step | S | T | — | Poly gives each note its own voice; mono plays one voice and restarts its envelope on every note; legato restarts only when no key is held (VO-6). |
| `voice.polyphony` | Polyphony | voices | 1..64 | 16 | step | D (Settings, not a sound parameter) | T | — | How many notes can sound at once, in one pool (VO-5). |
| `voice.gravity` | Gravity | % | 0..1 | 0 | lin | M | C | yes | Held notes drift into just intervals of one another (SW-11). |
| `env1.attack` | Attack | s | 0.0005..10 | 0.004 | log | M | C | yes | Time from a voice's start to full level. |
| `env1.hold` | Hold | s | 0..5 | 0 | pow | M | C | yes | Time held at full level before the decay. |
| `env1.decay` | Decay | s | 0.005..30 | 0.4 | log | M | C | yes | Time from full level to the sustain level: the decay ends exactly then. |
| `env1.sustain` | Sustain | % | 0..1 | 0.85 | lin | M | C | yes | Level held while the key is down. |
| `env1.release` | Release | s | 0.005..30 | 0.28 | log | M | C | yes | Time from key-up to silence: the release ends exactly then. |
| `env1.attackCurve` | Attack curve | — | −2.5..2.5 | 0 (linear) | lin | M | C | yes | Bends the attack: 0 straight, positive fast-start, negative slow-start (C.12). [Q31] |
| `env1.decayCurve` | Decay curve | — | −2.5..2.5 | +1 | lin | M | C | yes | Bends the decay (C.12); +2.30 is the bank's exponential. [Q31] |
| `env1.releaseCurve` | Release curve | — | −2.5..2.5 | +1 | lin | M | C | yes | Bends the release (C.12). [Q31] |
| `env1.velocity` | Velocity | % | 0..1 | 1 | lin | M | N | yes | How much velocity scales the voice's level; at 1 level is proportional to velocity, at 0 velocity is ignored. This is velocity's default mapping (VO-8). |
| `mix.osc1` | Osc 1 level | dB | −∞, −60..+6 | 0 | dB | M | A | yes | osc 1's contribution to the voice. |
| `mix.osc2` | Osc 2 level | dB | −∞, −60..+6 | 0 | dB | M | A | yes | osc 2's contribution. |
| `mix.sub` | Sub level | dB | −∞, −60..+6 | −2 | dB | M | A | yes | The sub's contribution. |
| `mix.noise` | Noise level | dB | −∞, −60..+6 | −12 | dB | M | A | yes | The noise oscillator's contribution. Its bottom, v = 0, is a true off: gain exactly 0, not −60 dB (SN-4). |
| `out.level` | Level | dB | −∞, −60..+26 | 0 | dB | M | C | yes | The engine's output level into the rest of the device; 0 dB is defined in 6.9; the ceiling lets a version-0 state keep its level (OU-3). [Q20] |
| `quality` | Quality | — | Standard (1×), High (2×) | Standard | step | D, saved with the patch | T | — | Oversampling: High aliases less on FM, ring and fold with sine carriers, at about 1.5 to 1.7 times the CPU. [Q27] |

Each source also has an on switch: `osc1.on` (default on), `osc2.on` (off), `sub.on` (off), `noise.on` (off): gate, class S, rate T. A source that is off costs nothing (PF-6).

**ENV 2 to ENV 4** (`env2.…`, `env3.…`, `env4.…`; round 26). Each has the ten rows below, in an id block of its own, outside every source's block, so a later effects-device shell can expose the envelopes unchanged. The engine computes them (EN-5). Their defaults are ENV 1's, so a source switched to ENV 2 in a new patch sounds the same until ENV 2 is edited.

| key | label | unit | range | default | law | class | rate | PV | what it does to the sound |
|---|---|---|---|---|---|---|---|---|---|
| `attack` | Attack | s | 0.0005..10 | 0.004 | log | M | C | yes | As `env1.attack`. |
| `hold` | Hold | s | 0..5 | 0 | pow | M | C | yes | As `env1.hold`. |
| `decay` | Decay | s | 0.005..30 | 0.4 | log | M | C | yes | As `env1.decay`. |
| `sustain` | Sustain | % | 0..1 | 0.85 | lin | M | C | yes | As `env1.sustain`. |
| `release` | Release | s | 0.005..30 | 0.28 | log | M | C | yes | As `env1.release`. |
| `attackCurve` | Attack curve | — | −2.5..2.5 | 0 (linear) | lin | M | C | yes | As `env1.attackCurve` (C.12). [Q31] |
| `decayCurve` | Decay curve | — | −2.5..2.5 | +1 | lin | M | C | yes | As `env1.decayCurve` (C.12). [Q31] |
| `releaseCurve` | Release curve | — | −2.5..2.5 | +1 | lin | M | C | yes | As `env1.releaseCurve` (C.12). [Q31] |
| `velocity` | Velocity | % | 0..1 | 1 | lin | M | N | yes | How much velocity scales this envelope's level (VO-8). |
| `oneShot` | One-shot | — | off, on | off | step | S | T | — | On: a key-up does not end the attack or the decay; the envelope runs them to their end (C.12). The bank's blade envelopes use it (B.1). |

### 5.3 Each oscillator (`osc1.…`, `osc2.…`)

| key | label | unit | range | default | law | class | rate | PV | what it does to the sound |
|---|---|---|---|---|---|---|---|---|---|
| `octave` | Octave | oct | −4..+4 | 0 | step | S | T | — | Whole-octave transposition. |
| `semitone` | Semitone | st | −12..+12 | 0 | step | S | T | — | Whole-semitone transposition. |
| `pitch` | Coarse | st | −48..+48 | 0 | lin | M | A | yes | Continuous transposition that can be swept and automated across several octaves. The oscillator's transposition is octave + semitone/12 + pitch/12 octaves plus fine. |
| `fine` | Fine | cents | −100..+100 | 0 | lin | M | A | yes | Fine tuning. |
| `pan` | Pan | — | −1..+1 | 0 | lin | M | C | yes | Shifts the whole oscillator's image left or right; members' seats shift with it and stop at the edges (C.8). |
| `mute` | Mute | — | off, on | off | step | D | T | — | Silences the oscillator; it keeps running (IF-6). |
| `solo` | Solo | — | off, on | off | step | D | T | — | While any source is soloed, only soloed sources sound (IF-6). |
| `unison` | Unison | voices (members) | 1..16 | 7 | step | S | T | — | How many members the swarm has. |
| `distribution` | Distribution | — | even, JP-8000, Gaussian, Cauchy, golden | JP-8000 | step | S | T | — | How the members are placed around the pitch: evenly, on the classic supersaw curve, or shaped by a seeded draw (C.2). |
| `seed` | Seed | — | 0..999 999 | 1234 (osc 2: 1235) | step | S, advanced | T | — | Fixes every random choice of this oscillator (placement, drift, start phases, spreads), so a patch is the same swarm every time. Shown under an advanced menu. |
| `detuneLaw` | Detune law | — | cents, Hz, ERB, harmonic | cents | step | S | T | — | What detune means: equal cents (beats faster up the keyboard), equal Hz (the same beat everywhere), equal roughness (ERB), or members on harmonics of the pitch. [Q31] |
| `detune` | Detune | % (shown in the law's unit) | 0..1 | 0.28 | lin | M | C | yes | How far the members sit from the pitch (C.3): cents law, a member at placement x sits x·detune·range·100 c away; Hz law, x·detune·range·20 Hz; ERB law, x·detune·range·0.35·ERB(f); harmonic law, member i on f·(1 + detune·range·reach·i). |
| `detuneRange` | Range | — | ×1, ×12, ×24 | ×1 | step | S | T | — | Widens detune to about an octave or two. [X11] |
| `harmonicReach` | Harmonic reach | — | 0.25..4 | 1 | log | M | C | yes | Harmonic law only: how far up the series the members reach. |
| `anchor` | Root anchor | % | 0..1 | 0 | lin | M | C | yes | 0 centres the swarm on the pitch; 1 puts its lowest member on the pitch. Advanced. |
| `coupling` | Coupling | % | −1..1 | 0 | lin (the taper is in the law, C.1) | M | C | yes | Positive pulls the members toward one locked pitch (cloud, herding, shimmer, lock); negative pushes them into an even splay (a multiplied, hollower tone). Feels the same at any detune (SW-4). |
| `phaseLag` | Phase lag | ° | −90..+90 | 0 | lin | M | C | yes | Bends where a locked swarm settles; large angles open unstable, part-locked states. |
| `pivot` | Pivot | — | mean field, root member | mean field | step | S | T | — | Mean field locks onto the swarm's average; root member locks onto the member nearest the played pitch, so a locked swarm stays on pitch. |
| `onsetLock` | Onset lock | % | −1..1 | 0 | lin (taper in the law, C.1) | M | N | yes | A burst of locking (or, negative, of splay) at each note's start. [Q31] |
| `dissolve` | Dissolve | s | 0.05..8 | 0.63 | log | M | C | yes | How long the onset burst takes to fade. [Q31] |
| `driftDepth` | Drift | cents | 0..100 | 0 | pow | M | C | yes | How far each member wanders in pitch. |
| `driftRate` | Drift rate | % | 0..1 | 0.4 | lin (the walk's rate is 0.2 + 8·knob, C.4) | M | C | yes | How fast they wander. [Q31] |
| `inertia` | Inertia | % | 0..1 | 0 | lin (the swing reads knob^2.5, C.5) | M | C | yes | Members swing into new places with weight and overshoot instead of jumping. [Q31] |
| `phase` | Phase | ° | 0..360 | 0 | lin, wraps | M | N | yes | Where in its cycle every member starts when the oscillator retriggers. |
| `phaseRandom` | Random | % | 0..100 | 0 | lin | M | N | yes | How far each member's start is randomised past Phase: 0 all start together, 100 anywhere in the cycle. |
| `retrigger` | Retrigger | — | on, off | on | step | S | T | — | On: phases restart at each note (Phase and Random); off: they resume where the oscillator left off (SW-10). |
| `onsetScatter` | Onset scatter | ms | 0..80 | 0 | pow | M | N | yes | Members enter up to this much apart, like players in an ensemble. [Q13, Q31] |
| `timingCorrection` | Timing correction | — | 0..1.5 | 0.25 | lin | M | N | yes | How strongly the entries correct toward the ensemble's average, so the swarm does not drift late. |
| `width` | Width | % | 0..1 | 0.8 | lin | M | C | yes | How far the members spread across the stereo field; 0 is mono. [Q26] |
| `panImage` | Pan image | — | balanced, pitch fan, spread fan | balanced | step | S | T | — | Balanced places members so no side is brighter; the fans order them by pitch or by placement, a deliberate spectral-stereo effect. |
| `panInvert` | Invert image | — | off, on | off | step | S | T | — | Mirrors the image left for right. Advanced. |
| `panMotion` | Pan motion | % | 0..1 | 0 | lin | M | C | yes | Slow wandering of each member's pan. |
| `bandLimit` | Band-limit | % | 0..1 | 1 | lin | M | C | yes | 1 removes the aliasing of every edge (ED-1); 0 leaves it, as a raw digital texture. |
| `motionGrain` | Motion grain | % | 0..1 | 1 | lin | M | C | yes | How the spread law's values reach the blades: 1 in steps at the control rate (a grain and air in the sound), 0 as smooth ramps. The frame always ramps. |
| `baseWave` | Base wave | — | sine, triangle, saw, reverse saw, square | saw | step | S | T | — | The wave the members play and the blades cut into. |
| `crossMod` | Cross-mod | % | 0..1 | 0 | pow | M | C | yes | Each member's previous output bends the next member's carrier phase (a ring of members). |
| `feedback` | Feedback | % | 0..1 | 0 | pow | M | C | yes | Each member's own previous output bends its carrier phase. |
| `loopDrive` | Loop drive | — | 0.14..1 | 0.3 | log | M | C | yes | How hard cross-mod and feedback drive their loop: stable below about 0.5 with a saw carrier, 0.38 with a triangle and 0.42 with a sine, nearly throughout with a reverse saw or square, about 0.34 with sine→saw from C3 up and at no setting below C3, chaotic above wherever the window is long enough, each setting its own noise (C.14, BL-12; round 20, ADR-244, corrected in round 22, ADR-248, and in round 50, ADR-350; the bank's presets with a live loop carry 1, B.1; the label is a placeholder until the naming workshop). |
| `spreadLaw` | Spread law | — | gradient, random, drift, alternate, swarm | gradient | step | S | T | — | How every spread is fanned across the members (BL-13). |
| `spreadDrift` | Spread drift | Hz | 0.02..12 | 0.5 | log | M | C | yes | Drift law only: how fast the spread values wander. |
| `quantize` | Quantize | — | free, whole | free | step | S | T | — | Whole snaps the cut-rate spreads and rules to whole harmonics. |
| `stacking` | Stacking | — | blade 2 over 1, blade 1 over 2 | blade 2 over 1 | step | S | T | — | Which blade is the upper one when they interact. |
| `upperHears` | Upper hears | % | 0..1 | 0 | lin | M | C | yes | 0: both blades act on the base wave side by side; 1: the upper blade acts on the lower blade's result. |
| `collisionPitch` | Collision → pitch | % | −1..1 | 0 | lin | M | C | yes | Where the blades overlap, the upper carrier's pitch bends up or down, up to ±2 octaves at full overlap. |
| `collisionBite` | Collision → bite | % | 0..1 | 0 | lin | M | C | yes | Where the blades overlap, the upper blade's FM depth rises up to ×5 and its fold drive up to ×3 (C.11). |
| `env` | Envelope | — | ENV 1, ENV 2, ENV 3, ENV 4 | ENV 1 | step | S | T | — | The envelope this oscillator's level follows (RC-6.5). |
| `pitchMode` | Pitch mode | — | normal | normal | step | S, reserved (5.1) | T | — | How Octave, Semitone, Coarse and Fine read; reserved for the 'harmonics' and 'ratio' modes after 1.0. |
| `stream` | Stream | — | All | All | step | S, reserved (5.1) | T | — | Which notes this oscillator plays (RC-6.7): All, every note. Reserved until Arp's core lands, which appends the six other combinations of the Direct, Chord and Arp toggles after All, so v = 0 keeps its meaning (rounds 30, 31). |

The oscillator's `stream` (round 30) takes its block's last id, after its blades' rows (RC-6.1, 5.8).

Not a host parameter, but oscillator state: **`ratios`**, the custom cut-rule ratio list (text, fractions or decimals separated by commas, default `1, 5/4, 3/2`).

### 5.4 Each blade (`…blade1.…`, `…blade2.…`)

| key | label | unit | range | default (blade 1 / blade 2) | law | class | rate | PV | what it does to the sound |
|---|---|---|---|---|---|---|---|---|---|
| `on` | Blade | — | off, on | off / off | step | S | T | — | Turns the blade on. [X12] |
| `mode` | Mode | — | Sync, FM reset, FM free, Noise, Fold, Ring, Crush | Sync / Fold | step | S | T | — | What replaces the wave inside the window (BL-2). |
| `wave` | Blade wave | — | sine, triangle, saw, reverse saw, square, sine→saw | saw / saw | step | S | T | — | The carrier's wave. |
| `shape` | Shape | % | 0..1 | 0.5 / 0.5 | lin | M | C | yes | Sine→saw only: from a sine (0) to a full saw (1). |
| `cutIn` | Cut rate in | — | × pitch per cycle, × pitch per blade, Hz; blade 2 adds "as blade 1" | × pitch per cycle / as blade 1 | step | S | T | — | Whether the cut rate follows the note (per cycle or per blade length) or is a fixed frequency (a formant). |
| `cut` | Cut rate | × | 1..64 | 6 / 3 | log | M | A | yes | Carrier cycles per cycle of the member's wave (or per blade). |
| `cutHz` | Cut rate | Hz | 20..12 000 | 1 320 / 800 | log | M | A | yes | The carrier's fixed frequency, when the cut rate is in Hz. |
| `position` | Position | cycle | 0..1, circular | 0.875 / 0.35 | lin, wraps | M | C | yes | Where in the cycle the window sits. |
| `width` | Width | cycle | 0..1 | 0.25 / 0.2 | log, off zone | M | C | yes | How much of the cycle the window covers. |
| `edges` | Edges | % | 0..1 | 0 / 0 | lin | M | C | yes | Softens the window's edges; in Crush, the glide between steps. |
| `depth` | Depth | % | 0..1 | 1 / 1 | lin | M | C | yes | Blend from the base wave (0) to the blade's sound (1) inside the window. |
| `mirror` | Mirror | — | off, reflect, twin −, twin +; blade 2 adds "as blade 1" | off / as blade 1 | step | S | T | — | Reflect makes the window a palindrome; the twins add a second window half a cycle on, inverted (−) or not (+). |
| `frame` | Frame | — | member, swarm; blade 2 adds "as blade 1" | member / as blade 1 | step | S | T | — | Swarm frame places every member's window relative to the swarm's mean phase, so the windows move together as the swarm locks and drifts. |
| `rotate` | Rotate | Hz | −4..+4 | 0 / 0 | lin | M | C | yes | Moves the window around the cycle at this rate. |
| `rotation` | Rotation | — | blade 1: restart per note, free-running; blade 2: with blade 1, its own | restart per note / with blade 1 | step | S | T | — | Whether each note starts the window at Position, and whether blade 2 turns with blade 1. |
| `fmType` | FM type | — | phase, pitch | phase / phase | step | S | T | — | FM modes: modulate the carrier's phase, or its pitch (symmetric in cents). |
| `modShape` | Mod shape | — | sine, triangle, saw, reverse saw, square, smooth noise, S&H noise | sine / sine | step | S | T | — | The FM modulator's wave. |
| `fmDepth` | FM depth | index | 0..10 | 2 / 2 | pow | M | A | yes | How hard the modulator pushes the carrier. |
| `modIn` | Mod rate in | — | × pitch, Hz | × pitch / × pitch | step | S | T | — | Whether the modulator follows the note or is fixed. |
| `modRate` | Mod rate | × | 0.25..24 | 3.37 / 3.37 | log | M | C | yes | Modulator frequency as a multiple of the member's pitch. |
| `modRateHz` | Mod rate | Hz | 1..20 000 | 660 / 660 | log | M | C | yes | Modulator frequency, fixed. |
| `positionSpread` | Position spread | ±cycle | −1..1 | 0 / 0 | lin | M | C | yes | Fans the window's position across the members. |
| `cutSpread` | Cut spread | harmonics | −24..24 | 0 / 0 | lin | M | C | yes | Fans the cut rate across the members by s·p: up to ±s/2 harmonics at the outermost members (±0.75·s under the drift law), whole harmonics under Quantize; the effective cut never falls below 0.25 (C.9 item 8). |
| `cutRule` | Cut rule | — | even, harmonic, undertone, octaves, major, minor, fifths, golden, primes, custom | even / even | step | S | T | — | Gives each member's cut rate a ratio from a rule (BL-15). |
| `ruleDepth` | Rule depth | % | 0..1 | 1 / 1 | lin | M | C | yes | How far toward the rule's ratios the members go. |
| `widthSpread` | Width spread | ± | −1..1 | 0 / 0 | lin | M | C | yes | Fans the window width: w × 2^(4·spread·p), so up to ×4 either way (×8 under the drift law, whose p reaches ±0.75). |
| `depthSpread` | Depth spread | ± | −1..1 | 0 / 0 | lin | M | C | yes | Fans the depth. |
| `shapeSpread` | Shape spread | ± | −1..1 | 0 / 0 | lin | M | C | yes | Fans the sine→saw shape. |
| `fmIndexSpread` | FM index spread | ± | −1..1 | 0 / 0 | lin | M | C | yes | Fans the FM depth: index × 2^(4·spread·p), up to ×4 either way (×8 under the drift law). |
| `rotateSpread` | Rotate spread | ±Hz | −2..2 | 0 / 0 | lin | M | C | yes | Fans the rotation rate, so windows drift apart. |
| `fmSource` (blade 2 only) | FM | — | blade 1's, its own | blade 1's | step | S | T | — | Blade 2 uses blade 1's FM settings or its own. |
| `spreadSource` (blade 2 only) | Spreads | — | blade 1's, its own | blade 1's | step | S | T | — | Blade 2 uses blade 1's spreads or its own. |

### 5.5 The sub (`sub.…`)

The sub follows its governing envelope (ENV 1 unless `env` selects another, RC-6.5); it has no envelope of its own and draws no random numbers. Its chain has two orders, chosen by `order` (SN-3): by default shape → tone → envelope → drive → DC removal → pan, gate and level, so a decaying note drives less, as an amp does; or shape → drive → DC removal → tone → envelope → pan, gate and level, so the drive keeps its colour all note long and its mean comes out exactly (SN-6). Erode bends the shape's time by the noise (SN-14). Every key the round-25 spec named keeps its id; the rows added in round 26 take the next free ids of the sub's block in the order the first lockfile fixes, append-only from then, `stream` (round 30) last (5.8).

| key | label | unit | range | default | law | class | rate | PV | what it does |
|---|---|---|---|---|---|---|---|---|---|
| `order` | Order | — | tone, env → drive; drive → tone, env | tone, env → drive | step | S | T | — | Where the drive sits: after the tone and the envelope (the lab's order, the default), or ahead of them (SN-3). |
| `voiceMode` | Voice mode | — | per note, mono | per note | step | S | T | — | Per note: a sub in every voice. Mono: one sub for the instrument, at the chosen held note (SN-7). Per note is the default because it is the sound every patch had before (RC-9.2). |
| `priority` | Mono follows | — | lowest, highest, newest | lowest | step | S | T | — | Mono only: which held note the sub plays (SN-7). |
| `wave` | Wave | — | sine, triangle, square, saw, pulse, bump, smooth square | saw | step | S | T | — | The sub's shape (SN-2). The smooth square is appended last, so it moves no index; the removal of the noise wave moved bump from index 6 to 5, harmless in a manifest not yet released. A panel may show the smooth square after the square. The round-25 noise wave is removed (the noise oscillator plays noise); the manifest is not yet released (5.1), so the option is removed, not retired. |
| `pulseWidth` | Width | % | 0.05..0.95 | 0.5 | lin | M | C | yes | Pulse only: the share of each cycle at +1; 0.5 is the square. |
| `softness` | Softness | % | 0..1 | 0.5 | lin | M | C | yes | Smooth square only: 1 is near a sine, 0 near a square (SN-2). |
| `bumpHarmonic` | Harmonic A | — | 2, 3, 4, 5, 6, 7, 8, 9 | 3 | step | S | T | — | Bump only: which harmonic the first extra one is. |
| `bumpAmount` | Amount A | — | 0..0.6 | 0.35 | lin | M | C | yes | Bump only: the first extra harmonic's size (the round-25 bump's amount, same key and id). |
| `bumpPhase` | Phase A | rad | −π..π | −0.25 | lin | M | C | yes | Bump only: the first extra harmonic's phase; negative leads with the big bump (the round-25 bump's phase, same key and id). |
| `bump2Harmonic` | Harmonic B | — | 2, 3, 4, 5, 6, 7, 8, 9 | 5 | step | S | T | — | Bump only: which harmonic the second extra one is. |
| `bump2Amount` | Amount B | — | 0..0.6 | 0 | lin | M | C | yes | Bump only: the second extra harmonic's size; 0 is the one-harmonic bump. |
| `bump2Phase` | Phase B | rad | −π..π | 0 | lin | M | C | yes | Bump only: the second extra harmonic's phase. |
| `octave` | Octave | — | −3, −2, −1, 0 | −1 | step | S | T | — | A sub goes down only. |
| `semitones` | Semitones | st | −12..+12 | 0 | step | S | T | — | |
| `pitch` | Coarse | st | −48..+48 | 0 | lin | M | A | yes | Continuous transposition, as the oscillators' Coarse (SN-1). A pitch envelope reaches the sub here: ENV 2 to ENV 4 route to it through the modulation system, like any destination. |
| `fine` | Fine | cents | −100..+100 | 0 | lin | M | A | yes | |
| `startPhase` | Phase | cycle | 0..1 | 0 | lin | M | N | yes | The sub's phase at a fresh start (SN-1). |
| `keytrack` | Keytrack | — | on, off | on | step | S | T | — | Off pins the sub to C2. |
| `erode` | Erode | % | 0..1 | 0 | lin | M | C | yes | The noise bends the sub's time, a noise-modulated delay; 0 is off and bit-identical (SN-14). |
| `drive` | Drive | % | 0..1 | 0 | lin | M | C | yes | Saturation of the shape (SN-5); 0 is clean. |
| `crunch` | Crunch | % | 0..1 | 0 | lin | M | C | yes | A parallel, level-matched band of hard-clipped upper harmonics: grit without darkening (SN-5). |
| `character` | Character | — | Tube, Diode, Fold-lite | Tube | step | S | T | — | The saturation's curve: soft and asymmetric, a harder knee with a lower negative ceiling, or a gentle fold (SN-5). |
| `tone` | Tone | Hz | 30..20 000 | 20 000 | log | M | C | yes | A gentle low-pass (SN-3). |
| `pan` | Pan | — | −1..+1 | 0 | lin | M | C | yes | Places the sub in the stereo field (C.8). |
| `mute`, `solo` | Mute, Solo | — | off, on | off | step | D | T | — | As the oscillators' (IF-6). |
| `env` | Envelope | — | ENV 1, ENV 2, ENV 3, ENV 4 | ENV 1 | step | S | T | — | The envelope the sub's level follows (RC-6.5; in mono, SN-7). |
| `pitchMode` | Pitch mode | — | normal | normal | step | S, reserved (5.1) | T | — | Reserved for the 'harmonics' and 'ratio' modes after 1.0. |
| `stream` | Stream | — | All | All | step | S, reserved (5.1) | T | — | Which notes the sub plays (RC-6.7): All, every note. Reserved until Arp's core lands, which appends the six other combinations of the Direct, Chord and Arp toggles after All, so v = 0 keeps its meaning (rounds 30, 31). |

### 5.6 The noise oscillator (`noise.…`)

The noise follows its governing envelope (RC-6.5) and has none of its own. Its chain is source and colour → Resonator → Whisper → Wind → noise filter → stereo spread and pan → gate, level and envelope (SN-4, SN-8 to SN-12). Every new row's default leaves the round-25 noise as it was (RC-9.2): Clean, every stage off, the filter off, Pitch 0, spread 0. `colour`, `pan`, `mute` and `solo` keep their ids; the rows added in round 26 take the next free ids of the noise's block, the sampler's four reserved rows after them, `lfsrKeytrack` after those and `stream` after it (append-only, RC-9.1).

| key | label | unit | range | default | law | class | rate | PV | what it does |
|---|---|---|---|---|---|---|---|---|---|
| `type` | Source | — | Clean, Crackle, S&H, LFSR | Clean | step | S | T | — | What makes the noise (SN-8). Clean is the round-25 noise. |
| `density` | Density | /s | 1..2 000 | 30 | log | M | C | yes | Crackle: pops per second. |
| `rate` | Rate | Hz | 50..20 000 | 8 000 | log | M | C | yes | S&H: new values per second. |
| `length` | Length | steps | 7, 15, 31, 63, 127, 511, 2 047, 32 767 | 15 | step | S | T | — | LFSR: the register's period; 63 steps or fewer repeat at the note's pitch, longer ones repeat below the note (127 about an octave down) and become hiss toward 32 767. |
| `lfsrKeytrack` | LFSR keytrack | — | on, off | on | step | S | T | — | LFSR: off pins its clock to C3, as if every note were C3; Pitch still moves it (SN-8). |
| `colour` | Colour | — | −1..1 | 1 | lin | M | C | yes | +1 white, 0 pink, −1 brown, continuous between; on every source (SN-4). |
| `resonator` | Resonator | — | off, on | off | step | S | T | — | A comb tuned to the note (SN-9). |
| `feedback` | Feedback | — | 0..0.98 | 0.9 | lin | M | C | yes | Resonator: how long it rings. |
| `whisper` | Whisper | — | off, on | off | step | S | T | — | Three vowel formants (SN-9). |
| `vowel` | Vowel | — | 0..1 (a, e, i, o, u) | 0 (a) | lin | M | C | yes | Whisper: sweeps a–e–i–o–u. |
| `wind` | Wind | — | off, on | off | step | S | T | — | A band that wanders like wind (SN-9). |
| `gust` | Gust | % | 0..1 | 0.4 | lin | M | C | yes | Wind: how far and how fast the band wanders. |
| `filter` | Filter | — | Off, LP, BP, HP | Off | step | S | T | — | The noise's own filter, ahead of the voice's filters (SN-10). |
| `cutoff` | Cutoff | Hz | 20..20 000 | 8 000 | log | M | A | yes | The noise filter's cutoff. |
| `resonance` | Resonance | % | 0..0.9 | 0.2 | lin | M | C | yes | The noise filter's resonance (Q 0.5 to 5). |
| `keytrack` | Keytrack | — | 0..1 | 0 | lin | M | C | yes | How far the cutoff follows the note: 1 fully, from C4; 0 not at all. |
| `pitch` | Pitch | st | −48..+48 | 0 | lin | M | C | yes | Transposes the noise's pitched parts: the LFSR's clock, the Resonator's tuning and the filter's keytracking (SN-11). |
| `spread` | Stereo spread | % | 0..1 | 0 | lin | M | C | yes | 0 is mono, as the round-25 noise; above 0 the two channels part (SN-12). |
| `pan` | Pan | — | −1..+1 | 0 | lin | M | C | yes | Places the noise in the stereo field (C.8). |
| `mute`, `solo` | Mute, Solo | — | off, on | off | step | D | T | — | As the oscillators' (IF-6). |
| `env` | Envelope | — | ENV 1, ENV 2, ENV 3, ENV 4 | ENV 1 | step | S | T | — | The envelope the noise's level follows (RC-6.5). |
| `pitchMode` | Pitch mode | — | normal | normal | step | S, reserved (5.1) | T | — | Reserved for the 'harmonics' and 'ratio' modes after 1.0. |
| `stream` | Stream | — | All | All | step | S, reserved (5.1) | T | — | Which notes the noise plays (RC-6.7): All, every note. Reserved until Arp's core lands, which appends the six other combinations of the Direct, Chord and Arp toggles after All, so v = 0 keeps its meaning (rounds 30, 31). |
| `mode` | Source mode | — | noise | noise | step | S, reserved (5.1) | T | — | Reserved for the sampler after 1.0, which appends `sample` (SN-13). |
| `sampleLoop` | Loop | — | one-shot | one-shot | step | S, reserved (5.1) | T | — | Sampler: one-shot, or looping once it ships (SN-13). |
| `sampleKeytrack` | Keytrack | — | on | on | step | S, reserved (5.1) | T | — | Sampler: pitched, or a fixed rate once it ships (SN-13). |
| `sampleStart` | Start | % | 0 | 0 | lin | M, reserved (5.1) | C | — | Sampler: the start position (SN-13). |
| `sampleRandom` | Random start | % | 0 | 0 | lin | M, reserved (5.1) | C | — | Sampler: how far the start is randomised (SN-13). |

### 5.7 Fixed behaviours (no control)

| behaviour | value |
|---|---|
| density compensation | a swarm's members sum scaled by U^−0.75 (C.8, SW-19) [X17] |
| inertia's response | w = knob^2.5 (C.5) |
| levels of a voice | audible, faded, freed and sounding as 6.4 defines them |
| DC | removed by C.16; the sub's after its drive, by SN-6 |
| the noise stages' level matching | each stage holds its input's level (SN-9) |
| gravity's capture radius | 35 cents (C.6) [Q31] |
| the pitch fan's curve | as C.8 [Q31] |
| host tempo and transport | inputs, not parameters (IF-2, IF-11) |
| blade envelopes | none: blades take ENV 2 to ENV 4 through the modulation system's routes (RC-4; Appendix B writes the bank's as routes) |

### 5.8 Counts

- **Rows:** voice 18 (plus 4 on switches), ENV 2 to ENV 4 10 (once per envelope), oscillator 48 (`loopDrive` added in round 20; `env` and `pitchMode` in round 26; `stream` in round 30), blade 30 (plus 2 blade-2-only), sub 28, noise 28: **164 table rows** (the sub's and the noise oscillator's `mute` and `solo` share one row each, so they hold 29 and 29 parameters).
- **Manifest parameters:** 18 + 4 + 3 × 10 + 2 × (48 + 2 × 30 + 2) + 29 + 29 = **330**. Thirteen are reserved (5.1: the four sources' `pitchMode` and `stream`, `noise.mode` and the sampler's four rows), so there are **317 host parameters**.
- **Ids** (round 26): every key the earlier rounds named keeps its id. Each new key takes the next free id of its own block: each oscillator's, the sub's and the noise's (RC-6.1, RC-9.1); ENV 2 to ENV 4 take a block of their own. Each source's `stream` (round 30) takes its block's last id: an oscillator's after its blades, the sub's after its other rows, the noise's after `lfsrKeytrack`. A table's row order is its display order, not its id order.

---

## 6. Behaviours

Each requirement states what the player hears; where a law is the sound, the requirement points to its formula in Appendix C.

### 6.1 The swarm (SW)

| id | requirement |
|---|---|
| SW-1 | Each oscillator of each voice is a swarm of U members (`unison`), each a phase-continuous oscillator at its own frequency around the oscillator's pitch. A member's frequency may change at any time; its phase never jumps because of it. |
| SW-2 | **Placement** follows `distribution` by C.2. Seeded draws come from the oscillator's `seed`; the same seed gives the same placement. |
| SW-3 | **Detune** follows `detuneLaw`, `detune`, `detuneRange`, `harmonicReach` and `anchor` by C.3. In the cents law with even placement, `detune` 0.1 at range ×1 puts the outermost members exactly ±10 cents from the pitch. |
| SW-4 | **Coupling** follows C.1: the pull on each member is a frequency offset in Hz, of strength km (C.1's taper; round 51, ADR-352) in units of σ, toward sync for K > 0 and toward even splay, with three times the authority, for K < 0; so the same knob position gives the same regime at any detune, register and law. With one member there is no pull (C.1). Claim anchors (7 members, JP-8000, cents law, `detune` 0.28 at ×1 (the outermost members ±28 c), A3 = 220 Hz, 44.1 kHz, Phase 0 and Random 0, no drift; R read at every control interval over the last 2 s of an 8 s note, values ±0.08 unless stated): K +1.0 (km 16) sustains R ≥ 0.98, ±0.01; +0.85 (km 4.20) holds R 0.99, ±0.02; +0.75 (km 1.91) holds a steady lock, R 0.934; +0.7 (km 1.38) oscillates, R swinging between about 0.26 and 0.87 (its minimum and maximum over those 2 s); +0.6 (km 0.80) from an aligned start falls to no sustained lock within about 3 s; −1.0 (km −16) after 4 s reads R ≤ 0.10 and R_N ≥ 0.90, with the 7th harmonic's POWER about 7 × its uncoupled level (≈ 7·R_N²; the gate stays at least 4.0); −0.7 (km −2.30) reads R_N 0.57 ± 0.1. The near-critical anchors (+0.7, +0.6) are the most sensitive to the control interval and are the first place a change of regime shows. (A simulation of C.1 at these conditions with Δ = 29 samples reads σ = 2.19 Hz; R 1.000 at +1.0, 0.996 at +0.9, 0.990 at +0.85, 0.934 at +0.75, 0.26 to 0.87 at +0.7, 0.14 to 0.75 at +0.6; R_N 0.989 at −1.0, 0.565 at −0.7.) |
| SW-5 | **Phase lag** α enters the sync pull as in C.1. Judged: the direction (positive α lowers a locked swarm's mean frequency, negative raises it) and monotonic growth; the magnitude is S·R·sin α within ±25 % at a lockable K. Anchor: SW-4's conditions at K +1.0 (S = 16σ ≈ 35.1 Hz), α 10°, 20° and 30°: the swarm stays locked (R ≥ 0.9, steady) and its mean frequency falls by S·R·sin α (a simulation of C.1: 6.09, 12.00 and 17.53 Hz against 6.09, 12.00 and 17.55 Hz). At these conditions the lock holds to at least 60° and is lost at 90°, so larger angles are not an anchor. |
| SW-6 | **Pivot**, root member: by C.1, under positive coupling every member is pulled toward the member nearest the played pitch, so a locked swarm sounds at the played pitch, not at its drifted mean. |
| SW-7 | **Drift**: each member wanders by the walk of C.4, up to `driftDepth` cents, at the rate `driftRate` sets, from the voice's seeded drift stream. [Q31] |
| SW-8 | **Inertia** follows C.5: a member's offset from the oscillator's pitch swings toward its target with weight and overshoot; the oscillator's own pitch (the note, bend, glide, audio-rate pitch) is never swung, so detune and the coupling pull swing while glide and bends stay crisp. The swing runs only above w = 0.001 (knob above about 0.063); at or below it members follow their targets, and a swing in flight when the knob drops finishes at the knob-0 law; the frequency never jumps. |
| SW-9 | **Onset lock**: at a voice's start the burst Kenv = 8·onset·\|onset\| (in units of σ, like km) joins the coupling by C.1 and decays with time constant `dissolve`. At 0 nothing changes. [Q31] |
| SW-10 | **Start phase.** With `retrigger` on, at each voice start (VO-6) member i starts at θᵢ = frac(`phase`/360 + (`phaseRandom`/100)·uᵢ), uᵢ a uniform draw in [0, 1) from the note's start-phase stream (DT-4): at Random 0 every member starts at Phase, at 100 anywhere in the cycle. With `retrigger` off, the members resume: each starts at the phase the same member of this oscillator has, at that sample, in the voice most recently started that is still sounding; if none is sounding, at the phase that member had in the voice that stopped sounding last, at the sample it stopped (a taken voice counts as sounding for its whole fade); if no voice has sounded since reset, as with `retrigger` on. Member i resumes from member i of that voice; members beyond its count start as with `retrigger` on. Every start is independent of the host's block size. |
| SW-11 | **Gravity** follows C.6. Claim anchor: 220 Hz and 331 Hz (a fifth 5.2 c sharp) at gravity 0.7 settle within ±0.5 c of 3/2 in about 3 s; a pair outside the 35 c basin moves 0 c; a released voice keeps the pitch it had at release; a newly started note starts at its played pitch while held notes keep the pitch they settled to. The result is independent of block size and the same at every sample rate (a fixed time grid). Gravity acts on the voice's pitch, after the tuning system has set each note's target and only when its knob is up, so both oscillators and the sub follow it. |
| SW-12 | **Onset scatter** follows C.7: at a voice's fresh start each member waits a seeded offset, silent and with its phase held, then enters by a fade; the offsets are corrected toward the ensemble's mean with gain `timingCorrection` and carried across notes, so the asynchrony has the structure of players correcting each other. Claim anchor (`onsetScatter` 30 ms, many notes): the lag-1 autocorrelation of a member's offset is near +1 at α 0, about +0.7 at 0.25, near 0 at 1 and negative at 1.5; the ordering is the requirement. [Q13] |
| SW-13 | **A member near the band edge fades out**: a member whose frequency rises from 0.40 to 0.48 of the internal rate fades from full to silent (linearly in frequency) and is silent above; never folded back, never a DC offset. (A blade carrier is capped instead, BL-4; the sub is capped, SN-1.) [X2] |
| SW-14 | Every time constant, rate and duration in the engine is defined in seconds or Hz and converted to the running rate, so a patch behaves the same at 44.1, 48, 88.2, 96, 176.4 and 192 kHz. No constant is tuned per tick or per sample. |
| SW-15 | A voice's first output sample already plays its members' correct frequencies, blade settings and DC correction (C.16); nothing in the engine warms up over the first control intervals, except what a law states: the coupling's smoothed strengths S and P rise from 0 at each fresh start with their time constant (C.1), which is the coupling's own onset. |
| SW-16 | **Motion grain** steps only the spread law's values: at `motionGrain` 1 each control interval's new spread values reach the blades at once; at 0 they ramp across the interval (BL-17); between, the step is the fraction m of the change and the rest ramps. The frame's lead always ramps, at every setting. |
| SW-17 | Changes of `coupling` and `detune` from any source are smoothed like every other continuous parameter (PA-1): automating either never steps the member frequencies. |
| SW-18 | **Pan** follows C.8: an equal-power law at a seat set by `panImage`, `width`, `pan` and `panInvert`; width 0 is mono. **Brightness balance**: in the balanced image, with blade 1 on and its Width spread 0.5, the default patch otherwise, the energy above 2 kHz of the left channel and of the right channel over 2 s of a held C3 differ by under 1.5 dB, for 4 to 9 members (judged) and 10 to 16 (reported); a two-member swarm cannot be balanced and is stated so. Pan motion moves seats slowly on a fixed time grid, its gains ramped across each grid step, independent of block size. No member is polarity-inverted between channels. [Q26] |
| SW-19 | The swarm's sum is scaled by U^−0.75 (C.8). [X17] |

### 6.2 The blades (BL)

| id | requirement |
|---|---|
| BL-1 | **Geometry** follows C.9.1 and C.9 item 1: a window of width w (off when the member's effective width is below 0.004 cycle) centred at c on each member's cycle; inside, a crossfade from the input wave toward the blade's sound by gate × depth, the gate's raised-cosine taper lying inside the window; outside, the input wave unchanged. |
| BL-2 | **Modes** follow C.9.2 and C.9 items 3 to 6. In every carrier mode (Sync, FM reset, FM free, Ring) the carrier's phase restarts at 0 at each window entry; FM reset also restarts its modulator (and the pitch-FM accumulator); FM free's modulator runs on, and so does its pitch-FM accumulator, which advances every sample inside the window and outside it, as a free-running oscillator does (the human's ruling, round 46, ADR-303; engine-free, the window-only reading misses DT-3 on a sine modulator). Noise holds a uniform value in [−1, 1), a new one each whole carrier cycle inside the window and at each entry. Fold is sin(π/2·(1 + (k − 1)/4)·x) with k the member's effective cut rate after spreads (never divided per blade). Crush holds as many whole steps as fit the window and a last partial one when they do not fit exactly (C.9 item 7). |
| BL-3 | **Waves** follow C.9.3, the sine→saw shape included (its brightness r capped by the carrier's own frequency). |
| BL-4 | **Cut rate.** "× pitch per cycle": the carrier runs at `cut` × the member's frequency; "per blade": `cut` cycles across the window (so at `cut`/width × the member's frequency); Hz: `cutHz` whatever the note. **The carrier's frequency is capped at 0.45 of the internal rate in every mode** (a cap, not a fade; C.9 item 8). |
| BL-5 | **FM** follows C.9.2: phase FM offsets the carrier phase by (index/2π)·modulator; pitch FM bends its rate by (1 + index/10)^modulator (±1200·log₂(1 + index/10) c). The modulator runs at `modRate` × the member's frequency fᵢ, or at `modRateHz`. |
| BL-6 | **Mirror** follows C.9.5 and C.9 item 9. On a half-wave-symmetric base (sine, triangle, square) twin − removes the even harmonics (at least 100 dB down) and adds no DC, in every mode whose blade sound is a fixed function of the cycle (Sync, FM reset, Fold, Ring and Crush, with rotation 0 and the blade's values held); FM free and Noise, whose sound differs from cycle to cycle, are outside the claim. A twin window is corrected for edges (ED-1) and DC (C.16) exactly as the main window. |
| BL-7 | **Crush** follows C.9.4. Judged on the output while the member's phase is inside the window: one member at C3, Standard quality, `position` 0.25 and `width` 0.25 (the window [0.125, 0.375] holds no edge of any base wave), Edges 0.3, depth 1, the cut rate swept exponentially from 2 to 60 over 2 s, every base wave: no sample-to-sample change of the output inside the window exceeds 1.25 × the largest per-sample change of the C.9.4 law itself over the same sweep (the law evaluated on a grid 16 times finer, its largest change scaled to the run's sample interval). The law's own slews are steep (at C3, Edges 0.3, about 0.08 per sample on a sine), so the bound is relative to the law, not absolute |
| BL-8 | **Rotation.** `rotate` turns the window around the cycle at that rate in Hz; "restart per note" starts each voice's window at Position at its fresh start; "free-running" integrates the blade's resolved `rotate` rate (without per-voice offsets) on one clock per blade per oscillator, advanced by the engine's sample clock whether or not a voice sounds, and each voice's own `rotate` offsets and its members' `rotateSpread` add their integrals from the voice's fresh start; blade 2 turns with blade 1 (its window is offset from its own Position by blade 1's rotation angle, member by member, on blade 1's clock, homing included; blade 2's `rotate` and `rotateSpread` are inert) or on its own (blade 2's `rotate`, and the `rotateSpread` its `spreadSource` names, under blade 1's restart / free-running choice) (round 19, ADR-243). When a rate returns to zero the window glides home to Position by the shortest way, with a time constant of 30 ms, in both rotation modes. |
| BL-9 | **Blade 2** is a full second blade. Every "as blade 1" or "blade 1's" option makes it follow blade 1's value of that setting. With blade 2 off, the oscillator sounds exactly as with blade 1 alone. |
| BL-10 | **Frame** follows C.13: in swarm frame each member's window sits at its lead from the voice's frame angle; the frame follows ψ where the swarm has a centre and never jumps where it has none (R near 0); where it is locked the frame equals ψ. At a voice's fresh start the frame is ψ if R ≥ 0.25, else member 0's phase. The lead always reaches the blade as a ramp. What the frame does after a crossing is [X7]. |
| BL-11 | **Interplay** follows C.11. With `upperHears` 0 and both collisions 0 the output is the same whichever blade is upper. Interplay and collision act only with blade 2 on. |
| BL-12 | **Cross-mod and feedback** follow C.14: one host sample late, in a ring of members. **With a saw carrier, by C.14's law the loop contracts** (a disturbance of the loop dies away) at every `loopDrive` below 0.5, at every width, pitch and quality measured, so below 0.5 every setting settles to one stable sound, never noise. At 0.5 the linearised loop turns unstable (the saw carrier's slope of 2 makes the loop gain 2·d). Each carrier wave has its own limit (the engine-free map, B521, redone on this law in round 50, ADR-350): about 0.50 for the saw, 0.38 for the triangle, 0.42 for the sine, 1.00 for the reverse saw and for the square in the FM modes, none for the square in Sync; in Ring, 0.65 for the saw, 0.55 for the triangle and sine and 0.66 for the reverse saw; and for sine→saw 0.34 from C3 up, and no setting at all below C3, where it does not contract even at 0.14. Below C3 that is below the default 0.3, which is kept (the human, rounds 22 and 50); the case is reported. Above the instability the loop is chaotic wherever the window is long enough to keep the disturbance, each setting its own noise. A wide window (0.9 and up in the map) can also hold subharmonic cycles below 0.5; those measured are stable. **Known windows, measured and not judged** (round 25, ADR-253, on the law before round 50, and not re-mapped off the grid since): with a square carrier at High, in the FM modes at P 42, that law had narrow bistable windows just off the judged grid, near `loopDrive` 0.828 and 0.963; a correct engine passes the judged grid, and the long run watches them. The loop stays deterministic at every drive (DT-1, DT-2). These facts come from an engine-free map of the law (round 22, ADR-248, B521; round 50, ADR-350); because the loop reads the member before its edge correction (C.14, round 23), they hold whatever the edge table's design. **Measure** (judged at both qualities, every carrier wave, at `loopDrive` 0.14, at n/100 for every n divisible by 5 from 15 up to L, and at every n/100 from L − 0.05 to L inclusive, L its carrier's judged limit; the full grid of every n/100 from 0.14 to L is a labelled long run, judged by the same criteria, before each stage's acceptance (round 23, ADR-250); L is the carrier's limit less a margin of 0.05: saw 0.45, triangle 0.33, sine 0.37, reverse saw 0.95, square 0.86 in Sync and 0.95 in the FM modes, Ring mode 0.50 for every wave but sine→saw, sine→saw 0.29 in every mode at C3 and above; above those, and sine→saw below C3, measured and reported, not judged): at 44.1 kHz, one member, `crossMod` 1 and `feedback` 1, each carrier mode (Sync, FM reset, FM free, Ring), rotation 0, no drift or noise, blade 1 otherwise at its manifest defaults (cut 6, position 0.875, depth 1, Edges 0, sine→saw at shape 0.5) with the carrier wave under test and `modRate` 2, blade width 0.25 and 1, at P = 1349, 337 and 42 samples (44,100/P Hz). **Start:** the patch is set and settled first, with 0.25 s of silence after it is applied, so every value has finished its PA-1 glide before the note starts at 0; the measures below time from the note's start, so they judge the loop and not its start-up transient (round 24, ADR-252). **Contraction:** a twin render whose `feedback` steps to 0.999 at 1 s and back to 1 at 1.01 s (through PA-1): over 1.5 s to 2.5 s the RMS of the twins' difference is below 10⁻⁶ of the output's RMS, either as rendered or after shifting the twin by a whole number k·P of samples, 0 < k < 12. A stable subharmonic cycle can slip by whole periods after the step and sound identical; a loop that does not contract matches at no shift. **Repetition, at width 0.25, a regression check only** (at a whole-period pitch every window starts identically, so it cannot detect chaos): after 1 s the output repeats with period P, the RMS of y[n] − y[n − P] over the next second below 10⁻⁴ of the output's RMS. |
| BL-13 | **Spreads** follow C.10: each spread's per-member position p comes from the spread law; a single member gets p = 0; blade 2's own spreads use draws independent of blade 1's. |
| BL-14 | **A negative spread is the mirror image of the positive**, exactly, Quantize included (both roundings of C.10 item 4 go half away from zero, so −s gives exactly the negated offsets of s), rotation included. |
| BL-15 | **Cut rules** follow C.10 item 5; ratios match those lists to 0.01. An unreadable custom list is refused and reported, and the previous list stays. |
| BL-16 | **A quantized cut never changes mid-cycle.** Under Quantize (or a snapped rule), when the spread law moves a member to a different ratio the change waits until that blade's carrier next restarts at its window entry, where the carrier's phase is 0, so it is never a jump of the carrier's phase. On a blade with no carrier (Fold, Crush), a change of its quantized cut, from the spread law or the host, waits for the window's next entry instead (the sample where e of C.9.1 wraps, e[n] < e[n − 1] − ½) (round 22, ADR-248). The one stated exception: a twin window wider than half a cycle is mid-window when the main window restarts, so its change still lands mid-window [X9]. Over 60 s of a held C3 (44.1 kHz, U = 7, Quantize on, cut spread 6, the drift law at `spreadDrift` 2 Hz, at `motionGrain` 1 and 0) the time-weighted share of every ratio is within ±0.001 of the share the spread law's values give without the wait, computed from the observation view's p (no tone bias). This holds at every `motionGrain`. And the ratio a member's blade uses changes only at samples where that blade's carrier restarts (on Fold or Crush, its window's entry), as the observation view shows (round 19, ADR-243). |
| BL-17 | **No control-rate step reaches the sound**: blade position, cut, width, depth, index, rotation and the sine→saw band limit, as fed to the blade (not only as stored), arrive as ramps across each control interval, at `motionGrain` 0, and always for the frame (SW-16). Under Quantize the cut is not ramped; it changes by BL-16. |
| BL-18 | **Noise.** Each voice, and each member within it, draws its noise from its own seeded stream (DT-4), started at the voice's fresh start; one note never changes another note's noise. A noise step is a step of the sound and is band-limited like any other edge (ED-1). [X4, X5] |
| BL-19 | **DC.** No blade leaves a DC offset: C.16 removes each blade's cycle mean; with any mode, wave and spread, the output's mean over each second of a held note is within 0.003·10^(L/20) of zero, L the patch's `out.level` in dB (0.003 at 0 dB, with every source level 0 dB and velocity 1), including while the cut rate steps or sweeps. |

### 6.3 Edges and aliasing (ED)

| id | requirement |
|---|---|
| ED-1 | **Every hard step the engine makes is band-limited**: base-wave discontinuities, window and twin edges, carrier wraps (also under FM, cross-mod and feedback, where the step's time follows the phase push), Crush steps at Edges 0 (above 0, a window's edge and a Crush step are short tapers, ED-7), noise steps; the one exception is the noise oscillator's S&H holds and LFSR steps, which stay raw (SN-8); under Erode the sub's edges are placed by SN-14's rule. The correction is built on a **minimum-phase band-limited step table of 32 taps, read with interpolation**, s(x) for x ≥ 0 (0 before); s(x) = 1 for x ≥ 32; its exact design (window, cutoff, storage) is the builder's, chosen to meet QS-1 and QS-2. **Its length is 32 host samples at either quality** (the human's ruling, round 43, ADR-298: B-60): at High, where an oscillator runs at twice the host rate, s spans 64 internal samples, and every count below that is the table's length doubles with it, in internal samples: s(x) = 1 from there on, E's taps (E[0] … E[64]), r₂'s and q₂'s span, E's history and a pending correction's residual part. ED-6's and ED-7's taper bound (T under 32 internal samples) and c(T) are not the table's length and do not change. The sub runs at the host rate, so its table spans 32 samples. (Engine-free, ADR-298: a table of 32 internal samples at High reaches QS-1's floors only by cutting near 19 kHz, 10 to 14 dB under an ideal saw at 16 to 20 kHz; 32 host samples meet QS-1's floors with room at about 3.5 dB, 3.06 dB of it C.15's decimator alone. QS-1 and QS-2 judge Standard; High's readings are reported.) It has two parts (round 42, ADR-289: B-51, B-52). **(1) The filter E.** E[k] = s′(k) for k = 1 … 31, E[0] = ½·s′(0⁺) and E[32] = ½·s′(32⁻) (s′ at a whole sample is the mean of its one-sided slopes), scaled so ΣE = 1, then E[0] and E[1] moved by equal and opposite amounts so that its first moment Σk·E[k] equals τ = −Ā exactly (Ā as ED-3 defines it; ΣE stays 1). Differencing s at whole samples is not E: it is half a sample late, and DT-3 then fails. **E runs per member** (the human's ruling, round 42): on each member's uncorrected output yᵢ⁰, at the source's rate (an oscillator's internal rate; the sub's host rate, SN-1), before C.8's pans and gains, before C.15's decimation, before PA-5's crossfade (one E per setting while two settings crossfade), and before the source's level, gate, governing envelope and every later gain; for the sub, on its shape where SN-2's corrections are added, before its tone, envelope and drive (SN-3). Each member's residuals are added to its E's output, so the member is band-limited before any gain touches it. (One E per channel after the pans is not this law: while a gain moves it skews gain against sound by τ, at most −40 dB during the steepest onset fade, round 42 check d.) E runs at every `bandLimit` (the human's ruling: the filter stays on). **(2) The residuals.** Each step of height h adds h·r₂(x) at each later sample, x its time after the step in samples, with r₂(x) = s(x) − Σ_{0 ≤ k ≤ ⌊x⌋} E[k] on [0, 32) and 0 after (it reaches 0 there, since ΣE = 1); a step on a sample counts from that sample if the sample holds the right-hand value, else from the next, ⌊x⌋ one lower at each later sample; each corner adds ED-6's q₂. The residuals are scaled by `bandLimit` (1 full, 0 none), so at 0 the aliasing stays, as a raw texture, with only E's gentle roll-off at the top of the band. (A consequence, not a second rule: at `bandLimit` 1 the output is E applied to the smooth part plus the band-limited steps, so the two keep the same phase relation at every sample rate, DT-3; the steps alone carried the table's delay before round 42, and a curved smooth part none.) Step heights are computed from the waveform's closed form at the step instant (a short taper's by ED-7), never by rendering the blade twice and differencing. Where several steps fall at one instant (a carrier reset on a window's end, a window edge on a sample, a square base's edge on a window's edge), the blades' contribution D is evaluated from its closed form at that instant's left and right limits, once per instant, so coincident steps are counted once; a step that falls on a sample's boundary is measured against the samples themselves and counted in exactly one interval (round 19, ADR-243). |
| ED-1a | **No base wave is ever rendered naive.** Every discontinuity the base wave itself makes is an edge ED-1 covers, on every member, with or without a blade, in and out of every window: the saw's and reverse saw's reset once per cycle, the square's two edges, and the triangle's two corners (by the corner table of ED-6, X3). The sine has none. At `bandLimit` 1 the plain saw base therefore meets QS-1 exactly as the saw does under a blade; only `bandLimit` below 1 lets the base wave alias, by the player's choice. Judged by QS-1 with no blade on each base wave in turn, and by QS-4's detector with its ratified numbers (its 20 ms local window included), which must read no event on a held plain-saw base at C1 and at C2: one of the detector's must-not-fire controls, which it must pass as ratified. |
| ED-2 | The correction adds no latency: it is causal, with no look-ahead. ED-1's filter E is causal too: it delays the smooth part by τ, about 2.4 samples, as the table delays each step, and adds no latency: a note first sounds where its envelope leaves 0 (PF-8.2), E[0] and E[1] not both 0 (round 42). The cross-mod and feedback taps read the member before it and before E (C.14, round 23, ADR-250). Reading a closed form at phases ahead of the current sample, the phase advanced at the member's current rate and every other input held at its current value (ED-7's heights and slope changes), is not look-ahead: it uses nothing the engine has not yet received. |
| ED-3 | **The table's own DC** (round 42, ADR-289). Each residual h·r₂(n + δ) has an area A₂(δ) = Σₙ r₂(n + δ), whose average over δ is Ā₂ = Ā + Σk·E[k], Ā the step table's own area (Σₙ (s(n + δ) − 1) averaged over δ, a constant of the table). ED-1 pins E's first moment to −Ā, so Ā₂ is 0 and the corrections add no offset: nothing is subtracted for them, and C.16's estimate serves only the blades' cycle mean (BL-19). (Before round 42 the corrections' mean, Ā·(fᵢ/f_s)·Σₑ hₑ, was subtracted; under E there is nothing to subtract, and subtracting it would itself be an offset.) |
| ED-4 | The table's cutoff keeps the top of the audible band (QS-2). The table is stored no finer than its read needs. |
| ED-5 | Each member keeps its pending correction, a span of 32 + 32 samples (a residual's, and a short taper's, ED-7), and each member, and the sub, keeps E's history, 32 samples, with a second history while PA-5's two settings crossfade (ED-1); at a crossfade's start the incoming setting's E history and pending corrections are copies of the outgoing setting's; each is cleared or played out when a voice is reused, never left to leak into a new note, and `reset()` clears them; nothing is allocated for them on the audio thread. |
| ED-6 | A softened edge (Edges > 0) is a short taper, corrected by ED-7 while it is narrower than 32 internal samples. Corners (a triangle's points, a mirrored window's centre, a blade contribution's change of slope at a window's edge) are not steps. A corner whose slope changes by Δσ per internal sample at fractional time δ adds Δσ·q₂(n + δ), with the residual corner table **q₂(x) = x + Q(x) − Σ_{0 ≤ k ≤ ⌊x⌋} E[k]·(x − k)** on [0, 32) and 0 after (it reaches 0 there, by E's pinned moment, ED-1), Q(x) = ∫₀^x (s − 1). **The triangle base's corners** are always corrected. **Crush's window corners** are corrected too (the human's ruling, round 42): inside a Crush window D = depth·(level − x), so the base's slope stops at the entry and resumes at the exit, each a corner of D (engine-free: better at 413 of R's 418 Crush points, by a median 37 dB, and within 0.2 dB at the other five, where the uncorrected long slews set the reading). **A blade's corners** (its carrier's points, a mirror's centre, D's change of slope at a window's edge) are corrected while the blade is in **ED-6's class**: `crossMod` and `feedback` at 0, and the taper T (hard edges are T = 0) under 32 internal samples, with ED-7's weight c(T). This holds for every D, curved or piecewise linear, on every base (the human's ruling, round 42, ADR-289): the rule that left a curved D's corners uncorrected rested on a first-order delay of the smooth part, which ED-1's filter replaces. Δσ is read from D's closed form at the corner. Where a blade enters or leaves the class (a cross-mod or feedback change; a change of T moves c(T) continuously, ED-7), only these corrections switch, and those pending play out (ED-5). Tapered edges and Crush's slews are ED-7's (round 29). [X3] |
| ED-7 | **Short tapers** (B-29a; round 29). **Scope.** A taper that softens a hard step is rendered as its law states and corrected as the step it approaches, convolved with the taper's slope. Such a taper is a window's edge under C.9.1's gate, a twin window's included (BL-6), or a Crush step's slew or exit ramp under C.9.4. **Width.** T is in internal samples, from the member's effective values and frequency fᵢ at the taper's start (C.9 item 8), with f_int the internal rate: a window's edge (C.9.1, every mode but Crush), `edges`·wᵢ/2·f_int/fᵢ; Crush, `edges`/kkᵢ·f_int/fᵢ, and the exit, min(`edges`, kkᵢ·w_e)/kkᵢ, w_e C.9.4's hold window,·f_int/fᵢ, kkᵢ C.9.1's carrier rate from kᵢ and wᵢ (C.9.4's hold unit is 1/kk). **The step part** (round 42, ADR-289: B-52). **At a window's edge** the correction is **c(T)·∫₀ᵀ k(τ)·D(τ)·r₂(s − τ) dτ** at the entry and its negative at the exit, s internal samples from the taper's start, where: r₂ is ED-1's residual table; k is the taper's slope at unit area, over 0 ≤ τ ≤ T (C.9.1, (π/2T)·sin(πτ/T); C.9.4, 1/T); c(T) = 1 for T ≤ 16, 0 for T ≥ 32, linear between; and D(τ), the blade's contribution, is read from the closed form at gate 1, at the blade phase advanced from the taper's start at the member's rate there, every other input (xin, cacc, ov, the held noise) held at its value at the taper's start. **FM's modulator is not held** (round 54, ADR-360): it is part of the closed form, FM reset's at mEff·eʳ and FM free's advancing by mEff per cycle of the blade phase (C.9.2's table); held, D′ would lose the modulator's share of the carrier's slope, and T → 0 would not be ED-6's corner. D is read on both sides of a carrier wrap, base step or point, or a modulator's step or point, inside the taper, so the step part follows it where it falls; the kernel-weighted mean, h·(r₂∗k) with h = ∫k·D, is not this law (B-52: it clicks). The wrap or step itself is still ED-1's, at its rendered height (the gate there times its jump), and a point there is ED-6's corner weighted by the gate. **Its realisation is the builder's** (tables included), held to a reference (the human's ruling, round 42): the integral computed piecewise between r₂'s whole-sample breaks and D's own (its wraps, steps and points, a modulator's included) with 8 Gauss–Legendre points per piece, which converges (16 points move it by at most 0.007 dB). **Any realisation is allowed whose QS-1 falls no more than 0.5 dB below the 8-point reference** at every point of R's grid and R₂'s (item 7.8): with the worst conforming table design's 4.49 dB and float storage's 0.00 dB, that stays inside X = 5. (4 points per piece fall up to 7.64 dB short on R's grid, so they do not qualify; engine-free, round 42 check f.) The same rule serves the corner part's integral and Crush's (r₂∗k). **In Crush**, where D across a slew is the held level, the step part is c(T)·h·(r₂∗k)(s), h the change of level, or the base at the exit less the last level; where a slew overlaps the exit ramp, each height is read from C.9.4's blend at its own midpoint. **The corner part.** ED-6's class is decided by the taper's width, not by hard edges alone: it holds while T < 32, with weight c(T). Each corner of D at the window's edges gets **c(T)·∫₀ᵀ k(τ)·D′(τ)·q₂(s − τ) dτ** from the taper's start (+ at the entry, − at the exit), q₂ ED-6's residual corner table and D′ D's slope per internal sample, read from the closed form inside the taper as D is: the corner's analogue of the step part. (The kernel-weighted mean slope, c(T)·Δσ·(q₂∗k) with Δσ = ∫k·D′, is not this law: it holds only where D′ is nearly constant across the taper, and on a curved D across a long taper it leaves 10 to 17 dB of QS-1 uncorrected, engine-free, round 42 check c.) A corner of the carrier inside the taper is still ED-6's, weighted by the gate there, so the cycle's corrected slope changes sum to 0. Round 29's in-taper remainder and smooth-part delay are gone: the exact step part contains the remainder, and ED-1's filter replaces the delay. **DC.** None is subtracted (ED-3). **Shared rules.** Every part is scaled by `bandLimit`, as ED-1's residuals are; ED-1's filter E is not. A taper's step and its corner at one instant are one edge for RT-2's count of 8. Every part starts at the taper's start, so it adds no latency (ED-2). Each member's pending correction holds 32 + 32 samples (ED-5). The loop reads the member before these corrections and before E, as before ED-1's and ED-6's (C.14's y⁰). At T → 0 this is ED-1 and ED-6 as written, so moving `edges` off 0 changes the aliasing continuously, never by a jump. **Measure** (judged, at Standard and 44.1 kHz, on the engine's own table). The cases are four blades: QS-1's Sync-blade case; the same with a square carrier on a triangle base; the same with a triangle carrier on a saw base (its points fall inside the tapers); and a Crush blade (cut 6, width 0.25, depth 1). Each is measured at every fourth semitone from C2 to C8 (19 pitches, the cut capped by C.9 item 8), with `edges` at 0 and at 2⁻ᵏ for k = 0 to 20. **The rule:** at every point, QS-1's alias ratio is no lower than min(its own ratio at `edges` 0, R) − 5 dB. **R** is the engine-free reference: this row's construction, with ED-1's filter and residual tables, on QS-1's reference design (the 32-tap minimum-phase Kaiser sinc at 0.4725·f_s, β 8.0), at that pitch and `edges` (`engine2/tests/calibration/b29a_reference.csv`, 1,672 rows, regenerated under round 42; its body's SHA-256, 5103df6dee8c175a9778d9f099cefe4b58a3318e46689c6635ee86c3c2a911e7, pinned by the suite). **Controls**, each falling by more than 5 dB below the rule (on the reference design): no correction for a taper (69 to 86 dB); the corners corrected at hard edges only, with no corner part for a taper (28 to 69 dB); the step part as the kernel-weighted mean h·(r₂∗k) (12 to 31 dB); that form with h read as D at the taper's start (24 to 42 dB); the correction cut off at T = 16 instead of faded by c(T), where T lies between 16 and 32 (up to 12.6 dB on Crush and 18.6 dB on Sync). **Unjudged** (named, not measured): FM reset and FM free, Ring, Fold and Noise blades; the reverse-saw carrier; a mirror's centre; twins; High quality; `bandLimit` below 1; `crossMod` and `feedback`; a Sync blade with a sine carrier; curved corners; the sub. The unjudged modes are held by the realisation's tolerance against the reference above. |

**Round 29 (ED-7, B-29a).** The row and R were derived and measured engine-free by the test author, reviewed by two critics, and ruled by the human: B-29 corrects the in-taper remainder ("with the remainder"; superseded in round 42, whose exact step part contains it, and R regenerated); QS-1 keeps its Sync floor of 93 dB at C8, with its reference design switched to the 32-tap Kaiser sinc at 0.4725·f_s, β 8.0 (ADR-256 and its amendments). **X = 5 dB comes from the oracle alone:** the worst design found that meets QS-1 and QS-2 falls 4.49 dB below min(its own Edges 0, R), and an implementation's quadrature and precision add at most 0.08 dB; no engine number enters it, and it is re-derived if a floor or the reference design moves, or the builder's window family differs from the 32-tap Kaiser family the scan covered. **The convolutions' realisation is the builder's, judged by the measure**, as ED-1's table is. **Effects device: none.**

### 6.4 The voice (VO)

**Levels named once.** A voice's **governing envelopes** are the envelopes its on sources follow (RC-6.5; ENV 1 when every source follows it, or when no source is on). Its **envelope level** is the largest of their outputs, 0 to 1, before velocity and source levels; with every source on ENV 1, the default, it is ENV 1's output, as before round 26. A voice is **sounding** from its start until it is freed (a voice taken and fading under VO-3 is sounding for its whole fade). It is **audible** while its envelope level is at or above −60 dB (1e-3), **faded** below it, and **freed** when every governing envelope's release has finished (envelope level exactly 0) or, once released, when its envelope level falls below −80 dB (1e-4), and in either case its filter slot's tail report (RC-1) has fallen below −80 dB too. **A driven sub's envelope:** a voice whose sub is on in the order tone, env → drive with `drive` > 0 is freed only when the envelope the sub follows has finished its release (level exactly 0), because that envelope sits ahead of a drive of up to about +29 dB of small-signal gain, so −80 dB at the envelope is not −80 dB at the output; for the same reason that envelope never counts the voice as faded, so VO-2 takes such a voice in tier 2 (a tail, quietest first), never in tier 1. So a source on a longer envelope keeps its voice until that envelope ends; an envelope no on source follows never holds a voice. The mono sub (SN-7) is in no voice and holds none. A voice is **held** while its note's key is down (a note-on without its note-off), and **sustained** while its key is up and the sustain pedal holds it (VO-12); **gated** means held or sustained.

| id | requirement |
|---|---|
| VO-1 | One allocator serves the whole instrument. Notes are addressed by note id; a note-off or expression for an id reaches only that note's voice. A note-off or expression with note id −1 (CLAP's wildcard) addresses by channel and key: every voice whose note has that channel and key (−1 for either field matches all). Two notes on one key are two voices. A voice records its note's stream (IF-1), kept with its note id for the voice's life; in 1.0 nothing reads it (RC-6.7). |
| VO-2 | **The voice law (poly).** A new note takes, in order: (1) a faded voice that is not held (freed voices included), oldest first (a voice's age runs from its note's start; a voice that has never sounded is free); (2) a voice that is not held (a releasing tail, or a sustained voice), quietest envelope level first, age breaking ties; (3) only when every voice is held, the oldest held voice. A held key is never taken ahead of a tail. A repeated note never reuses its own sounding voice, unless no other voice exists (polyphony 1). |
| VO-3 | **A taken voice fades.** When a note takes an audible voice, the old sound fades to silence over 5 ms (the raised-cosine fade, C.0) and the new note starts from silence when the fade ends. Everything about that note is late by its wait, its release included, so it sounds for as long as its key was held. While waiting it counts as held by its key (tiers 1 and 2 never pick it); a retune reaches it (its sounding pitch moves; the struck pitch stays the glide target); a panic or a voicing change cancels it with any pending release; a second take replaces the waiting note without restarting the fade. A culled tail that is taken waits no longer than the shorter of the steal fade and what the cull had left. A zero-length note on a taken voice is silent. If the cull picks a tail whose key is up but whose late release is still pending, it is released at the cull (the pending release cleared); a taken tail's pending release is cancelled by the take. A faded voice restarts at once. |
| VO-4 | **Under load.** The engine accepts a voice cap from the device: an integer from 1 to `voice.polyphony`, or none (an input, never a clock reading); by default the device sets none, so there is no CPU-adaptive limiting. Over a cap, the quietest releasing tails fade out over 8 ms, never a held voice. A note at a full cap with no tail to take is refused (the default) or, by the device's choice, takes the oldest held voice by VO-3. With no cap the engine never culls. |
| VO-5 | `voice.polyphony` sets how many voices exist, 1 to 64 in one pool (default 16). VO-2 and VO-3 hold unchanged at every setting, including when every voice is in use. Lowering it while voices sound releases the excess, oldest first by VO-2's age, by VO-4's fade, never a cut. |
| VO-6 | **Mono and legato.** Mono: one voice; every new note re-runs the voice's start from its current level, with that note's own velocity, the level moving from the old velocity's to the new over 5 ms (no step). Legato: the voice re-runs its start only when a note arrives with no other key held; otherwise it is a **legato change**: the swarm and blades keep running and the voice keeps its strike's velocity and envelope. In both, the voice sounds the pitch and the expressions of the newest held note id, and releasing it while others are held returns to the most recent held one by a legato change. A note change reports which note the voice moves from and to, so the pitch system can glide (IF-2). What re-runs, by kind of start: see the table below. **A fresh start on a sounding voice.** In mono, a re-run follows `retrigger`: with it on, the table's fresh-start column re-runs and the voice's output crossfades over 5 ms with equal gain from its continuing state to the restarted one, so no waveform jump is heard; with it off, the phases continue, everything continues as on a legato change, and only the envelopes (ENV 1 to ENV 4) re-run (with the new note's velocity, ramped over 5 ms). In legato the phases always continue: legato's first note while the voice's tail still sounds continues everything as a legato change does and re-runs only the envelopes, with the new note's velocity ramped over 5 ms. Every envelope keeps the voice's legato: on a legato change none restarts, whichever sources follow it. |
| VO-7 | A freed voice outputs exact zeros and holds no state that a later note could hear. |
| VO-8 | Velocity (0..1) scales each envelope's level by its **velocity factor** (1 − v + v·velocity), v that envelope's `velocity` row (`env1.velocity` for ENV 1), velocity's default mapping; at v = 1 the level is proportional to velocity. Velocity is also a modulation source (IF-1). |
| VO-9 | **The voice's path** is: each source at its level, pan and gate, times its governing envelope's level and that envelope's velocity factor (VO-8), summed, through the filter slot. Each source's signal exists separately before the sum for the filter slot's routing and the meters. The sub applies its governing envelope inside its own chain (SN-3) and the voice does not apply it again. The envelope acts before the slot so that each source can follow its own (RC-6.5); while the slot is empty the sound is the same as one envelope after it (round 26). |
| VO-10 | Each voice publishes its lifecycle: started (note id, and its note counter, DT-4, for the per-note LFO streams of LF-12; round 62), released, taken (old note id, new note id, when the new one will start), freed. **Its consumers include the pitch system** (RC-3), which needs to know which notes still sound, their release tails included, to choose where a glide starts. |
| VO-11 | **A voice that is not sounding costs nothing.** The engine renders zero member-samples, zero edge corrections and zero envelope steps for every voice that is not sounding; judged on the operation counters (IF-9), deterministically: with `voice.polyphony` 64 and one voice sounding, the counters equal those with `voice.polyphony` 1 and the same voice sounding. |
| VO-12 | **The sustain pedal.** The engine accepts the sustain pedal per channel (MIDI CC 64, pressed at a value of 64 or more; under MPE a zone's master-channel pedal holds every channel of the zone). A key released while the pedal is down keeps its voice sounding at sustain (its envelopes stay in their current stages, not released) and makes that voice a tail for VO-2 (taken before any held key). A key struck again while the pedal holds its earlier note takes a new voice; the earlier one stays a tail. Pedal-up releases every voice held only by the pedal. In mono and legato the pedal holds the one voice's gate: releasing keys under the pedal changes neither its note nor its envelope; a new key still changes the note (mono re-runs, legato glides); at pedal-up with no key held the voice releases, and with keys held it moves to the most recent held key by a legato change. **MPE:** the shell tells the engine whether MPE is on and each zone's master channel (1 for the lower zone, 16 for the upper), from the host's MPE configuration or the device's setting; without MPE each channel's pedal holds only that channel's voices. |
| VO-13 | **All notes off and all sound off.** The engine handles the host's all-notes-off (MIDI CC 123, or CLAP's all-notes note-off) as a key-up of every note and a pedal-up: every voice releases through its envelopes and its tail rings out as usual. It handles all-sound-off (MIDI CC 120) as an immediate stop: every voice, waiting notes included, fades to silence over 5 ms and is freed; the pedal's state is cleared. Neither resets the ensemble's carried offsets or any stream; gravity's settled pitches go with their voices. There is no panel control for either. |
| VO-14 | **Changes while notes sound.** A `voice.mode` change fades every voice over 5 ms and frees it (as all-sound-off). A `quality` change fades every voice over 5 ms, frees it, and continues at the new rate. A `voice.polyphony` change follows VO-5. |

**What re-runs at each kind of voice start** (VO-6):

| state | a fresh start (poly; mono's first note; legato's first note from silence; a mono re-run with `retrigger` on, which crossfades by VO-6) | a legato change (also a mono re-run with `retrigger` off and legato's first note over a tail, except that the envelopes re-run, VO-6) |
|---|---|---|
| start phases (SW-10) | re-run (`retrigger` on) or resume (off) | continue |
| onset lock burst (C.1) | re-run | continue (no new burst) |
| onset scatter (C.7) | re-run (the carried offsets update) | continue (no new waits) |
| rotation clock (BL-8, restart per note) | restarts at Position | continues |
| random spread targets (C.10 item 2) | redrawn; a sounding voice glides to them over 20 ms | kept |
| drift-law spread positions (C.10 item 3) | redrawn from the stationary spread | continue |
| noise streams (BL-18, SN-4) | restarted from the note's seed | continue |
| the sub's phase (SN-1; the mono sub keeps its own, SN-7) | restarts at `startPhase` | continues |
| gravity entry (C.6) | the voice enters at its played pitch | the voice's settled pitch rides the new pitch (C.6) |
| member drift (C.4) | restarts at 0 | continues |
| inertia (C.5) | set to the targets (no swing) | continues swinging toward the new targets |
| the frame (C.13) | set (ψ if R ≥ 0.25, else member 0's phase) | continues |
| the note counter and the note's streams (DT-4) | a new count | the same |
| ENV 1 to ENV 4 (EN-5) | each re-runs its attack from its current level | each continues its stage |
| velocity | the new note's (ramped over 5 ms in mono) | the strike's, kept |

### 6.5 The envelopes (EN)

| id | requirement |
|---|---|
| EN-1 | ENV 1 to ENV 4 are attack–hold–decay–sustain–release envelopes whose timed stages follow C.12: each stage takes exactly its time and lands exactly on its target (attack on 1, decay on sustain, release on 0), with no step at a stage's end; its shape is set by its curve. ENV 2 to ENV 4 add a one-shot mode (C.12). |
| EN-2 | A voice's start from silence starts every envelope at exactly 0. A re-run (mono, legato's first note, a re-struck voice) restarts each envelope's attack from its current level, never from 0 (C.12). A stolen voice's new note starts from silence (VO-3). |
| EN-3 | Changing an envelope's time, curve or sustain while a stage runs continues the stage from where its level stands (C.12); the level never jumps. |
| EN-4 | Every envelope is the same in seconds at every sample rate and independent of block size. |
| EN-5 | **The engine computes ENV 2 to ENV 4** (round 26), for every sounding voice, every sample, from their own rows (5.2), whether or not a source follows them: so a selector change mid-note finds its envelope already running (PA-5), and the modulation system can read every envelope as a per-voice source (IF-3). They are in the engine, not in the modulation system, because the voice law reads them: a voice is freed when its governing envelopes end and stolen by their level (6.4, VO-2), and every envelope must keep the voice's legato (VO-6). An envelope computed outside would put the voice law in another module's hands, and the engine could not be tested alone. IF-9's envelope-step counter counts one step per sounding voice per sample for its four envelopes together. FX device: ENV 2 to ENV 4 are voice-bound modulation sources. The FX device instances the same C.12 envelope once per sidechain note, so the envelope is built with no dependency on the voice; its rows sit in a block of their own. |

### 6.6 Parameter changes and modulation (PA)

| id | requirement |
|---|---|
| PA-1 | **No zipper.** A change of any continuous parameter from the host, automation, a macro, an intent or the morph, **and each source's morph level (MO-26)**, glides to its new value (a row of rate N is still read as its target at a voice's start, §5.1, round 28) through a **second-order smoother: two identical one-pole stages in series, each with a time constant of 6 ms** (critically damped, so a jump starts with zero slope and never overshoots, and the pair passes 1 − 1/e of the way at about 12.8 ms), stepped each sample as the exact discretisation of the continuous pair: with τ = 6 ms, h = Δt/τ and p = e^(−h), the errors to the target go e₁ ← p·e₁ and e₂ ← p·(e₂ + h·e₁), e₂ the heard value's error (round 25, ADR-253; never two separately sampled one-poles, which miss the continuous law early in a jump), in the normalised form v (the shortest way round for circular parameters), snapping to the target once within 10⁻⁶ of it; a full-range jump in one event makes no click by the detector of QS-4. **Except a landing:** a `Param` the resolver marks as landing (MO-26 (4)) takes its value at once, with no glide, since its source was silent (round 61, ADR-377 A2). **A source's morph level** (MO-26) glides by this law at the host rate f_host, whatever the quality, so the resolver's mirror of it (MO-26 (1)) is exact. |
| PA-2 | **Per-voice modulation, one mechanism.** For every continuous parameter, the engine accepts per voice an offset in the parameter's normalised 0..1 form, addressed by note id and cleared at that note's fresh start. The value a voice uses is the parameter's resolved value plus the sum of its offsets, clamped to [0, 1] (wrapped for circular parameters), mapped by the parameter's law. An offset for all voices at once is the same mechanism with every voice addressed. **Round 62 (Appendix E; the lead, M2 (b); ADR-379 r2):** per voice and per key, the engine also takes a **Control input** q in [0, 1] (1 until one is sent), ramped by PA-3, and the value a voice uses is clamp(q·b̃ + the sum of its offsets, 0, 1), b̃ the resolved value after PA-1 (MS-20); and, on PA-4's keys, the **engine path's terms** (MS-41), each evaluated per sample on its source and added to the offsets before the clamp. |
| PA-3 | **Control rate.** Offsets arrive and take effect at the control rate: the interval is the whole number of samples nearest 1/1,500 s at the host rate (29 at 44.1 kHz, 32 at 48 kHz, 64 at 96 kHz), each change ramped across one interval. Intervals are counted from the engine's own sample clock since reset, never from the host's block boundaries; a voice starting mid-interval sets its values at its start and joins the clock's next interval. Events with the same sample time apply in the order received, before that sample renders, except IF-2's pitch and per-note expressions at a note-on's sample, **and the modulation system's offsets, Control inputs and engine-path terms for that note id at its note-on's sample (MS-42; round 62)**, which apply in either order and are set at the voice's start, not ramped. |
| PA-4 | **Audio rate.** These destinations also accept a per-sample offset: the voice pitch, the oscillator `pitch` and `fine`, the sub's `pitch` and `fine` (so a pitch envelope on the sub is smooth), the blades' `cut`, `cutHz` and `fmDepth`, **the four source levels `mix.osc1`, `mix.osc2`, `mix.sub` and `mix.noise`, and the noise filter's `cutoff`** (round 62; ADR-379 r2: "pitch, cutoff, level" [X78]; a per-voice filter's cutoff joins when RC-1 builds it). They are the engine path's destinations (MS-41). Every other parameter is control-rate, ramped. |
| PA-5 | **Stepped keys while a note sounds.** A stepped setting of rate T (mode, wave, mirror, frame, cut-rate and mod-rate units, base wave, distribution, detune law and range, pivot, the spread law and rules, quantize, the pan image, a blade's or source's on switch, a source's envelope selector, the sub's wave and harmonic selectors, the noise's source, length, stage switches and filter type) changes on sounding voices at once, through an equal-gain crossfade of 5 ms from the old setting's sound to the new; the same holds when the morph flips one (every Structural key, MO-13). During the crossfade both settings render from one shared member state (the same phases and swarm), which then continues under the new setting; the engine is sized for this (RT-1). A second change that arrives during a crossfade waits until it ends and then crossfades from the then-current setting to the newest value (changes in between are skipped), so at most two settings of a key ever sound at once, and only during this crossfade (ADR-377). A change of the sub's `order` crossfades its whole chain, the incoming order's chain starting from its fresh-start state (its phase continuing), so the two orders share no filter or drive state. Any other stepped change of the sub or the noise crossfades at the point it acts: the sub's shape ahead of the drive; a noise stage's output against its input; the noise filter's outputs. A noise stage switched on mid-note starts from its fresh-start state. The sub's drive bypass (SN-5) is evaluated on each voice's resolved values and engages or releases by this crossfade, with this rule for a second change during it. Keys of rate N take effect at the next voice start. `octave` and `semitone` change the frequency at once with every phase continuous, so they need no crossfade; `retrigger` acts only at the next voice start, so it needs none; `mute` and `solo` follow IF-6. **Except a landing:** a stepped key whose `Param` is marked as landing (MO-26 (4)), the sub's `order` included, changes at once with no crossfade, and `unison` with no PA-6 fade, ahead of its source's switch-on crossfade, since the source was silent. A landing makes no crossfade of its own, so RT-1's sizing (one crossfade per oscillator, and the sub's and the noise's chain copies) holds (round 61, ADR-377 A2). |
| PA-6 | A change of `unison` while a note sounds fades new members in and removed members out over 5 ms; the remaining members continue unbroken; their seats and the U^−0.75 scale move to the new count's over the same 5 ms. |
| PA-7 | **Host values are checked where they enter.** A non-finite value is refused; a value out of range is clamped to it; an unknown option of a stepped key is refused; **a `Param` event whose id names no engine parameter (an unknown id, a reserved row's, §5.1, or one of the morph's host parameters, IF-11, which the shell routes to the resolver) is refused and nothing is stored** (round 28, the counter `refused_param_id`); **a morph level (MO-26, IF-12) that is not finite is refused, and one outside [0, 1] is clamped to it (round 61, ADR-377); the `SettingSet` event's refusals and their counters are after 1.0 (D.6)**; each refusal and each clamp is counted and readable. No value the host can send puts the engine in a state no control can reach. |
| PA-8 | **Live and inert.** A parameter is **live** when its row's condition (below) holds: then there is a setting of the other parameters, and a note sequence, under which changing it moves the output. When the condition fails it is **inert**: no change of it moves the output, whatever the other parameters and the notes. |

**Live when** (PA-8; a parameter not listed is live whenever its source is on; `phase` and `phaseRandom` are live at every `retrigger` setting, since a start with nothing to resume uses them):

| parameter(s) | live when |
|---|---|
| every blade row | that blade is on; blade 2's "as blade 1" options read blade 1's value |
| a blade's `wave` | that blade is on in a carrier mode (Sync, FM reset, FM free, Ring; BL-2), since Noise, Fold and Crush read no wave (C.9.2) (round 21, ADR-245) |
| `cut` | `cutIn` is per cycle or per blade |
| `cutHz` | `cutIn` is Hz |
| `fmType`, `modShape`, `fmDepth`, `modIn`, `fmIndexSpread` | the blade's mode is FM reset or FM free, and for blade 2, `fmSource` is its own |
| `modRate` / `modRateHz` | as above, with `modIn` × pitch / Hz |
| `shape`, `shapeSpread` | the blade is in a carrier mode and its wave is sine→saw (round 21, ADR-245) |
| `edges` | the blade's width is above 0.004 |
| `ruleDepth` | `cutRule` is not even |
| `cutSpread` | `cutRule` is even |
| `ratios` | `cutRule` is custom |
| `spreadDrift` | `spreadLaw` is drift |
| every spread | U > 1, and for blade 2's spreads, `spreadSource` is its own |
| `upperHears`, `stacking`, `collisionPitch`, `collisionBite` | both blades are on |
| blade 2's `rotate` and `rotateSpread` | blade 2's `rotation` is its own (and, for `rotateSpread`, the spreads' condition) |
| `crossMod` | a blade is on in Sync, FM or Ring mode (at U = 1 the member is its own neighbour, C.14) |
| `feedback` | a blade is on in Sync, FM or Ring mode |
| `loopDrive` | a blade is on in Sync, FM or Ring mode |
| `detune`, `detuneRange`, `coupling`, `onsetLock`, `pivot`, `panImage`, `panInvert`, `motionGrain`, `spreadLaw`, `quantize` | U > 1 |
| `width` | U > 1, or `panMotion` > 0 |
| `panMotion` | `width` > 0 |
| `harmonicReach` | U > 1 and `detuneLaw` is harmonic |
| `anchor`, `distribution` | U > 1 and `detuneLaw` is not harmonic |
| `phaseLag` | U > 1, and `coupling` > 0 or `onsetLock` > 0 (S\* includes the burst, C.1) |
| `dissolve` | U > 1 and `onsetLock` ≠ 0 |
| `timingCorrection` | U > 1 and `onsetScatter` > 0 (`onsetScatter` itself is live at every U: its entry fade, C.7) |
| `driftRate` | `driftDepth` > 0 |
| `inertia` | U > 1 or `driftDepth` > 0 |
| `pulseWidth` | the sub's wave is pulse |
| `bumpHarmonic`, `bumpAmount`, `bumpPhase`, `bump2Harmonic`, `bump2Amount`, `bump2Phase` | the sub's wave is bump |
| `priority` | `sub.voiceMode` is mono |
| `softness` | the sub's wave is smooth square |
| `lfsrKeytrack` | the noise's `type` is LFSR |
| every noise row but `spread`, `pan`, `mute`, `solo` and `env` | the noise is on, or the sub is on with `erode` > 0 (Erode reads the noise's left channel before its envelope, level and pan, SN-14) |
| `character` | `sub.drive` > 0 (Crunch works at every drive) |
| every `env1` row | an on source follows ENV 1 (the default) |
| every `env2`, `env3` and `env4` row | an on source follows that envelope (a route that reads it is the modulation system's, RC-4) |
| a source's `env` | that source is on |
| `density` / `rate` / `length` (noise) | the noise's `type` is Crackle / S&H / LFSR |
| `feedback` / `vowel` / `gust` (noise) | the Resonator / Whisper / Wind is on |
| `cutoff`, `resonance`, `keytrack` (noise) | the noise's `filter` is not Off |
| `pitch` (noise) | the noise's `type` is LFSR, or the Resonator is on, or the filter is not Off and `keytrack` > 0 |
| every reserved row (5.1) | never in 1.0 |

### 6.7 Determinism (DT)

| id | requirement |
|---|---|
| DT-1 | The same state and the same sample-timed event stream give identical output, sample for sample, on one build. Bit-identity across platforms or builds is not required. |
| DT-2 | The output is bit-identical however the host splits the same events into blocks (any block sizes from 1 to 4,096, including uneven splits). |
| DT-3 | At different sample rates the same patch gives the same sound, not the same samples: between renders at 44.1, 48 and 96 kHz, the level of each of the first 16 harmonics of a held note (where below 18 kHz) agrees within 0.5 dB, and every note and envelope event time agrees within 1 ms. Judged on every bank preset with no Noise blade, no noise source, no drift, neither the random nor the drift spread law, and no live cross-mod or feedback loop (`crossMod` and `feedback` both 0; round 50, ADR-350: those run above BL-12's limits, chaotic and rate-dependent) (the others are reported), on a held C4, each harmonic's level the power within ±4 bins of its frequency in a 1 s window (4-term Blackman–Harris) starting 1 s after the note-on. **Judged harmonics** are those within 80 dB of the strongest; quieter ones are reported (round 42, ADR-289): within QS-1's floors, alias power in a harmonic's window can move a harmonic L dB below the strongest by up to 20·log₁₀(1 + 10^((L − F)/20)), F QS-1's floor at that pitch, which reaches 0.5 dB at L = F − 24.8, 79.2 dB at C4 (104 − 24.8), rounded to 80 (engine-free). For a curved D, which QS-1 does not bound, the floor is a rule, not a bound. **A preset with the sub sounding is reported, not judged** (SN-2, round 42). **A looped preset is reported, not judged** (`crossMod` or `feedback` above 0 and live): C.14's loop steps in host samples, so its time differs at each rate, even with no edge correction (the human's ruling, round 42, engine-free, B-51). **A preset with a blade on a sine-to-saw shape carrier is reported, not judged** (C.9.3): its carrier's r_max = min(0.995, 0.01^(f_h/Nyq)) depends on the Nyquist frequency by law, so its harmonics differ between rates (engine-free; coupling, serial interplay and FM free hold; the human's ruling, round 45, ADR-301). |
| DT-4 | **Every random draw comes from a mulberry32 stream** named by its purpose. Per oscillator, persistent until reset, a seed change or a load: placement (C.2) and the ensemble (C.7), seeded from the oscillator's `seed` and the purpose. Per note, started at the voice's fresh start: start phases, drift, the spread law, blade noise, and the noise oscillator's five streams, `noise`, `noise.right`, `noise.crackle`, `noise.lfsr` and `noise.wind` (SN-8, SN-9, SN-12; from osc 1's seed, "the voice's seed"), each seeded from the seed, the purpose, the note counter and the member. The sub draws none (round 26 removed its noise wave). One purpose per use keeps each use's numbers independent of how many draws another use made. Never from a parameter's position in a list, never from how many draws another voice made. **A note-on never changes a voice already sounding**: not its random numbers, not its samples. |
| DT-5 | The engine reads no clock and depends on nothing outside its state and its inputs. |
| DT-6 | After `reset()` the engine renders exactly as a freshly prepared instance with the same state. |

### 6.8 Real-time safety (RT)

| id | requirement |
|---|---|
| RT-1 | Processing, parameter and modulation input, note events, observation reads and `reset()` allocate nothing, take no lock, make no system call or I/O, and run no unbounded loop. Everything is allocated at prepare, sized by the maxima: 64 voices, 16 members, both oscillators, both blades, room for PA-5's second setting on every oscillator, and PA-5's crossfade copies of the sub's and the noise's chains; **no per-setting instances: setting sets are after 1.0 (ADR-377)**; a second voice state for VO-6's mono re-run crossfade, the noise's two chains and its Resonator lines down to 20 Hz (SN-9), and the mono sub (SN-7). Every lazily built table is built at prepare. |
| RT-2 | The work per sample is bounded by a constant set by the parameter maxima. At most 8 edges are corrected per member per internal sample; any further edge in that sample is left uncorrected and counted. A taper's step and its corner at one instant count as one edge (ED-7). |
| RT-3 | **A non-finite value is contained where it arises and reported.** A voice in which any state becomes non-finite (a member's phase or output, a loop tap, a filter or envelope state) has that voice's state reset to a fresh voice's (its sound stops for that note), and the event is counted; without the reset, a NaN in a feedback or cross-mod loop would latch the voice into silence or noise for good. A non-finite output sample is also replaced by zero, counted and latched. Every count and latch is readable from another thread until cleared; nothing is repaired silently. |
| RT-4 | The engine produces no subnormal output samples and no subnormal-driven slowdown: tails reach exact zero; flush-to-zero is set by the shell around processing, and the engine does not depend on it for correctness. |

### 6.9 The output stage (OU)

**0 dB, defined once.** At `out.level` 0 dB the engine's output is 0.14 × the sum of the voices, where a single member of a full-scale base wave at envelope level 1, velocity 1, U = 1, centred, contributes amplitude 1 before its pan gains (C.8).

| id | requirement |
|---|---|
| OU-1 | The output stage scales the sum of the voices by 0.14 × 10^(`out.level`/20) and does nothing else to the sound: no saturation, no limiter, no clamp. A sample over 0 dBFS is caught by the clip latch (IF-6) and reported; the device's master limiter, after the rack, holds the device's peaks (RC-5). Drive is the drive module's. [Q19] |
| OU-2 | **Loudness, then headroom, at the default level** (`out.level` 0 dB for a new patch; for a bank preset its translated level, B.1). **First, loudness:** each bank preset's translated `out.level` is moved so that its chord (below) has the bank's median loudness, where loudness is the ITU-R BS.1770-4 integrated loudness of the chord's 3 s, stereo, ungated, at the engine's output, and the median is taken over the bank at the translated levels before any move; a move that would leave `out.level`'s range stops at its end and is reported by name. **Then, headroom:** every bank preset plays a four-note chord (C4 E4 G4 B4: keys 60, 64, 67, 71) at velocity 0.85 held 3 s with its peak below 0 dBFS (no exceptions). A preset that fails has its translated `out.level` lowered by its shortfall (the chord's peak above 0 dBFS, plus 0.1 dB), and the shortfall is reported by name, so a preset lowered here sits below the median and is listed as such; the definition of 0 dB and a new patch's default of 0 dB do not move. |
| OU-3 | **Migration.** A state in the version-0 format (B.2) opens translated by Appendix B, its level by B.2's formula (which keeps the level it stored, up to `out.level`'s ceiling of +26 dB); the sound is translated, not promised identical. The engine's own state format carries `version` 1 (IF-4). Every later change of a default carries a migration. |
| OU-4 | When no voice sounds, the output is exact zero and the engine reports itself silent, so the shell may stop calling it. While silent no voice exists; the only state that moves is the engine's sample clock (which drives free-running rotation and the control and gravity grids), so the shell passes the number of samples it skipped when it resumes, and the engine then renders exactly as if it had been called throughout. [Q24] |

### 6.10 The sub and the noise oscillator (SN)

Round 26 (ADR-255) writes this section from the human's rulings on the sub and the noise oscillator, made by ear in an audition lab, a browser approximation of these laws: the lab gave each law's form and numbers, not a parity target. **Effects device:** the sources and the mono sub are voice-bound and instrument-only. FX device: ENV 2 to ENV 4 are voice-bound modulation sources. The FX device instances the same C.12 envelope once per sidechain note, so the envelope is built with no dependency on the voice; its rows sit in a block of their own. The human's later rulings of 2026-10-20, from the lab, add the sub's `order` switch, Erode, the smooth square, the LFSR's keytrack and the noise level's true off; Erode's effects-device line is none (it is voice-bound and instrument-only). **Two deviations from ADR-255's stated consequences**, made in this round: the engine computes ENV 2 to ENV 4 (EN-5) instead of taking them as a per-voice input, because the voice law reads them; and their ids sit in a block of their own, not in the sources' blocks, so another shell can expose them.

| id | requirement |
|---|---|
| SN-1 | **The sub** is one phase, one shape, no members, no coupling, no drift, no random draws. Per note there is one in each voice; in mono, one for the instrument (SN-7). Its frequency is its note's pitch (the voice's published sounding pitch; in mono, SN-7's) with keytrack on, or C2, 65.41 Hz, with it off, times 2^(octave + semitones/12 + pitch/12) and 2^(fine/1200), `pitch` being Coarse, capped at 0.45 of the host rate (a cap, not a fade). The sub and the noise oscillator run at the host rate at both qualities (4.2). **The sub integrates its own frequency; at a fresh start its phase is `startPhase` (cycles) and the voice's master phase is 0, so the two start together; it stays phase-continuous under every change** (a glide, a bend, gravity, keytrack, octave, Coarse, the mono sub's change of note). It is panned by C.8's single-source law. |
| SN-2 | **Sub shapes**, with θ its phase in cycles and t = frac(θ): sine sin 2πθ; triangle as C.9 item 2; saw 2t − 1 (starting at −1); **pulse, the classic two-level pulse: +1 while t < w, else −1**, w = `pulseWidth`, so its mean is 2w − 1, removed after the drive (SN-6). The human chose the classic pulse, the lab's; the round-25 zero-mean pulse is gone. **The square is exactly the pulse at w = 0.5**: +1 for t < ½, as C.9's base square (the round-25 sub square began at −1). Every step of the saw, square and pulse (heights ±2) is band-limited by ED-1's correction, the oscillators' own law: ED-1's filter E on the sub's shape and the residual tables r₂ and, for the triangle's corners, q₂ (ED-6), with no offset to subtract (ED-3; the human's ruling, round 42, ADR-289: one law for every source, so the sub's shapes carry E's delay as the oscillators' do). The sub's tone (its cap at 0.45·f_h), its drive's 2× resampling (SN-5) and, at High, its path without the oscillators' decimator (C.15) add rate-dependent phase of their own, so a preset with the sub sounding is reported, not judged, by DT-3 (the human's ruling, round 42; engine-free, check a). Each closed form below, its peak included, is the shape before E. E passes it within 0.001 dB below 0.35·f_h and rolls it off above (on QS-1's reference design about −0.2 dB at 0.42·f_h, −2.1 dB at 0.45·f_h, −7.8 dB at 0.48·f_h), where the bump's fades and the sub's cap lie, so this row's claims on a peak, a level or F(n) are judged on the shape before E (the observation view) and on the rendered sound only where every component lies below 0.35·f_h. Level −∞ is exact silence. **The bump** is the fundamental plus two extra harmonics, A and B, each of a whole order n from 2 to 9 (`bumpHarmonic`, `bump2Harmonic`), an amount a (`bumpAmount`, `bump2Amount`) and a phase φ (`bumpPhase`, `bump2Phase`): **y = (sin 2πθ + a_A·F(n_A)·sin(n_A·2πθ + φ_A) + a_B·F(n_B)·sin(n_B·2πθ + φ_B)) / peak**, peak the true maximum over a period of \|the numerator\| as played, its fades included, so **the shape peaks at exactly 1 for every setting**. **F(n) = clip((0.48 − n·f/f_h)/0.08, 0, 1)**, f the sub's frequency and f_h the host rate: each extra harmonic fades linearly from full at n·f = 0.40·f_h to silence at 0.48·f_h and is silent above, so the bump never aliases (its fundamental is capped by SN-1). The peak is taken after the fades because a bump normalised by its unfaded peak would fall below 1 inside a fade. It follows the pitch: recomputed at least once per control interval, it reaches the sound as a ramp across it (PA-3), so at a steady pitch it is exactly 1, and while the pitch moves through a fade it lags by at most one interval's change. **The defaults reproduce the round-25 bump exactly** wherever 3f < 0.40·f_h: A at n 3 with the round-25 `bumpAmount` 0.35 and `bumpPhase` −0.25, which keep their keys and ids as harmonic A's, and B at n 5 with amount 0 (at the defaults the trailing bump is 0.882 of the leading one and the trough between them 0.637 of the peak). **The smooth square** (appended last to the wave list, so no stored index moves): **y = tanh(k·sin 2πθ)/tanh k**, which peaks at exactly 1 and has mean exactly 0 (odd symmetry) for every k. k = min(k_s, k_max): **k_s = 0.2 + 119.8·(1 − s)²**, s = `softness` (s = 1 is within 0.5 % RMS of a sine, s = 0 within about 3 % RMS of a band-limited square at 55 Hz). Near Nyquist k is lowered, never faded, so that **every harmonic above 0.45·f_h stays at least 68 dB under the fundamental**, by construction: with m = 0.45·f_h/f, **k_max(m) is the largest k whose exact ratio \|c_n/c_1\| (the odd harmonics' Fourier coefficients of tanh(k·sin x)), from a fixed-N quadrature at prepare, is at most 10^(−68/20)** at n, the first odd number at or above m (the ratios fall with n), for each whole m from 1 to 2048. **k_max may fall below k_s's floor of 0.2 near the cap** (k = min(k_s, k_max) as written); **at m = 1**, where n = 1 is the fundamental itself and no k meets the bound, **the shape is its limit as k → 0, a sine** (round 27). (The lab's closed-form pole bound, π·e^(−n·α)/(k·cosh α) with α = asinh(π/(2k)), is wrong at low n, reading −51.0 dB at n = 3 and −60.7 dB at n = 5 where it claims −68, so it is not used.) between whole m it is linearly interpolated, so k moves continuously as the pitch glides; for m ≥ 2048, k_max does not limit. k follows the pitch at the control rate, ramped (PA-3). |
| SN-3 | **The order** (`order`; the human's ruling: keep the switch). The default, **tone, env → drive** (the lab's): shape (with Erode, SN-14) → tone → governing envelope → drive (SN-5) → DC removal (SN-6) → pan, gate and level, so a decaying note drives less, as an amp does; velocity, with the envelope, reaches the drive, so a harder note drives harder and its timbre changes with velocity, which is intended: an amp responds to how hard it is played, and the human chose this order from the lab. The other, **drive → tone, env**: shape (with Erode) → drive → DC removal → tone → governing envelope → pan, gate and level, so the drive keeps its colour all note long. In both the sub applies its governing envelope (and its velocity factor, VO-8) inside its own chain, and the voice does not apply it again (VO-9). A change of `order` crossfades the whole chain (PA-5). **The tone** is a one-pole low-pass, bilinear-prewarped so its response is the same at every sample rate (30 Hz to 20 kHz, clamped to 0.45 of the rate). |
| SN-4 | **The noise oscillator** is per voice, from the voice's own noise streams (so one note never changes another's noise). Its chain is its source (SN-8) with its colour → Resonator → Whisper → Wind (SN-9) → noise filter (SN-10) → stereo spread and pan (SN-12) → gate, level and governing envelope (VO-9). **Colour** shapes the source by `colour` (+1 flat, 0 falling 3 dB per octave, −1 falling 6 dB per octave, continuous between, from 20 Hz to Nyquist; each slope within ±0.5 dB per octave, measured in octave bands from 40 Hz to 16 kHz over 10 s), at a level independent of colour within 1 dB (both judged on Clean). It filters each source's output: Clean's, Crackle's and LFSR's as they are made, and S&H's before the hold (S&H holds the coloured noise). The noise shares the voice's allocation, envelopes and filter slot. **It runs** in a voice while it is on, and also, silent, while the sub is on with the voice's resolved, smoothed `erode` above 0, evaluated at each control interval (so block size never decides it, DT-2), because Erode reads it (SN-14). The silent noise runs its left chain only; started mid-note it takes its fresh-start state, its streams from the note's seed (DT-4); with the sub in mono, every sounding voice's noise runs. There it costs what it computes, the one exception to RC-6.3. **Its level** (`mix.noise`) bottoms out at a true off: at v = 0 the gain is exactly 0. Above it the dB law (5.1) gives 10^((−60 + 66·v)/20), so a fall through PA-1 reaches −60 dB, and the snap within 10⁻⁶ of v = 0 lands on exactly 0, a last step of 10⁻³ of the level. Erode reads the noise before this level, so a noise at a true off still erodes. |
| SN-5 | **The sub's drive** (Drive, Crunch and Character; the three characters and Crunch's parallel form are the human's choices from the lab). It sits where `order` puts it (SN-3) and runs at twice the host rate, f₂ = 2·f_h, up- and down-sampled by exactly C.15's High-quality decimation filter (the 4th-order Butterworth low-pass at 0.45·f_h, two biquads of Q 0.5412 and 1.3066, run at f₂), the same coefficients up (after inserting zeros, with gain 2) and down, **keeping the second of each pair of f₂ samples**: causal, with no look-ahead (PF-8). Each biquad is the analog prototype taken through the bilinear transform prewarped at its cutoff (the standard audio-EQ low-pass form), round 27, and pinned so that item 25's thump bound can be derived from the law. With x its input (SN-3), d = `drive`, m = min(1, 20·d) and g = 10^(d·G/20), G = 30 dB for Tube and Diode and 14 dB for Fold-lite (a fold needs less gain to fold): **the saturator** is **Dr(x) = x + m·(k·S(g·x) − x)**, S the character's curve: Tube S(u) = tanh(u + 0.3) − tanh 0.3 (soft, asymmetric); Diode S(u) = u/(1 + u⁴)^¼ for u ≥ 0, else 0.6·v/(1 + v⁴)^¼ with v = u/0.6 (a harder knee, a lower negative ceiling); Fold-lite S(u) = sin u. **k = min(4, (1/√2)/σ_S)**, σ_S the RMS about its mean of S(g·sin 2πθ) over one cycle: the make-up keeps a full-scale sine near its dry RMS as the drive rises. At d = 0, Dr(x) = x. **Crunch**, c = `crunch`, is a parallel fizz path on the saturator's output y, at f₂: p = tanh(20·y); h = 1.5·(p − LP₇₀₀(p)); q = LP₁₀ₖ(clip(h, −0.55, 0.8)); e follows \|y\|, rising through a one-pole of 1 ms and falling through one of 50 ms, **e ← e + (1 − e^(−1/(τ·f₂)))·(\|y\| − e)** with τ = 1 ms while \|y\| > e, else 50 ms (round 27); the output is **(y + 1.4·c·e·q)/(1 + 0.04·c)**. LP_f is the one-pole low-pass of coefficient 1 − e^(−2π·f/f₂); every state is 0 at a fresh start. The pre-clip makes even harmonics even from a sine, the high-pass keeps only the upper ones, the asymmetric clip turns them to grit and the low-pass tames the top; the follower scales the fizz to the signal, so it adds bite at any level and the dry path is never darkened (the human rejected the lab's first Crunch, a pre- and de-emphasis that cancelled and then darkened the sound). With `drive` and `crunch` both 0 the stage is bypassed exactly and costs nothing (PF-6); it engages and releases by PA-5's 5 ms crossfade, so its resampling filters never switch in with a step. |
| SN-6 | **The sub's DC comes out after the drive** (the human's ruling, as in the lab), **exactly, in both orders**, so QS-6 holds in both (the human: "keep the gate; compute it exactly"). A memoryless curve maps a two-level pulse to a two-level pulse of the same width, so on a steady pulse or square the saturator changes only the two levels, and with them the mean; its timbre changes only at the corrected edges and through Crunch. After the drive the sub subtracts **M, the cycle mean of the drive's output for the shape as it reaches the drive**: **M = (1/N)·Σⱼ Dr(a·yⱼ)**, j = 0 … N − 1, a fixed-N periodic quadrature over one steady-state cycle, N set at prepare (PROPOSED: 256; its tolerance is the test author's to derive). Here xⱼ = s(j/N) is the shape of SN-2 at N phases (the bump's fades and the smooth square's k included; Erode moves the shape in time without changing the values it takes, so it is left out). **In the order drive → tone, env**, yⱼ = xⱼ and a = 1. **In the default order**, yⱼ is the steady-state cycle of xⱼ through the tone's one-pole, in closed form: with p = e^(−λ/N), λ = 2π·f_c/f (f_c the tone's cutoff, f the sub's frequency), y₀ = (1 − p)·Σⱼ p^(N−1−j)·xⱼ/(1 − p^N), then yⱼ₊₁ = p·yⱼ + (1 − p)·xⱼ, which is the periodic response of the analog one-pole the tone prewarps to (its state at the cycle's end equals its start; the digital tone differs from it only near Nyquist, within M's tolerance); and a = the governing envelope's level times its velocity factor **at the interval's first sample**, M then ramping to its new value across the interval (PA-3; round 27). For the pulse at the drive → tone, env order it is closed form, M = w·Dr(+1) + (1 − w)·Dr(−1). **When:** M is recomputed at each control interval while any of its inputs changes (the envelope, the tone, the pitch, the drive, the character, the shape's own controls), reaches the sound as a ramp across it (PA-3), and applies from the sub's first sample (SW-15). The cycle yⱼ depends on the tone, the pitch and the shape, not on a, so while only the envelope moves just the N evaluations of Dr are redone. **Cost:** per voice and control interval at most N shape evaluations, 2N one-pole steps and N evaluations of Dr, none when nothing changed, into a buffer of N values allocated at prepare (RT-1): bounded, allocation-free, measured in PF-4. **Crunch's fizz path is not zero-mean** and has no closed form (its asymmetric clip and level follower depend on the signal's history), so it is not in M. Then **a first-order DC blocker at 2 Hz** (PROPOSED; state 0 at a fresh start; **y[n] = x[n] − x[n−1] + R·y[n−1], R = e^(−2π·2/f_h)**, round 27) takes only what M cannot see: the envelope's movement within an interval, Crunch's fizz path and the corrected edges' residue (their values between the two levels pass the curve too). At 2 Hz it passes a sub at 8 Hz within 0.3 dB. The test author derives any residue bound from this method (item 25). |
| SN-7 | **The mono sub** (`voiceMode` mono; the human's ruling: a mono sub following the lowest held note by default, also the highest or the newest). One sub for the instrument, outside the voices, plays the **chosen note** among the gated notes (held or sustained, 6.4; a note refused under a cap, VO-4, is never gated, and a note waiting for a taken voice, VO-3, joins when it starts, as gravity counts it, C.6): `priority` lowest takes the lowest key, highest the highest key, newest the latest note-on; ties on one key go to the newest. (A later round restricts the choice to the notes of the streams the sub's selector includes, RC-6.7; in 1.0 every source's stream selector is All, so the choice is unchanged.) It chooses by key, not by sounding pitch, so a bend or a glide past another note never flips the choice mid-gesture. Its pitch is the chosen note's: its voice's published sounding pitch while it holds a voice, else its pitch input (IF-2). Kept so, with its consequence: a note with no voice (in `voice.mode` mono or legato, the held notes behind the sounding one) has no gravity ratio, because gravity acts only between voices (C.6), so when the choice moves between a note under gravity and one without a voice the sub's pitch steps by that voice's gravity offset; the voice's published pitch is the only gravity there is to read. When the chosen note changes, the sub moves to the new pitch at once, phase-continuous; **its glide belongs to the pitch system** (RC-3), and until the pitch system exists there is none. It runs the sub's whole chain (SN-1 to SN-6) once, with **its own instance of its governing envelope** (the one `sub.env` selects, by that envelope's rows and C.12), the chosen note's per-voice offsets (PA-2), and the sub's pan, gate and level, and passes its own filter-slot instance, driven by the chosen note's voice (RC-1), and joins the sum of the voices after it. Its starts follow the voice mode's rule for a single voice (VO-6): a **fresh start** when a note is gated with none gated before and its envelope has ended (phase `startPhase`, every state reset); a **re-run** of its envelope from its current level, phases and states continuing, when a note is gated with none gated before while its release still sounds, and, under `voice.mode` mono, at every new note; otherwise a **legato change**, in which its envelope continues its stage. Its velocity is that of the note that last started or re-ran it, moving over 5 ms on a re-run (VO-6). It releases when no note is gated, and outputs exact zeros once its envelope has ended (VO-7). The voice law and the polyphony do not count it. Wherever VO-13, VO-14, IF-4's load, RT-3 or DT-6 fade, free or reset every voice, they do the same to the mono sub. Its slot instance follows X13's routing for the sub, and its tail keeps the mono sub sounding after release until the tail falls below the free threshold (6.4). Erode reads the noise of the voice holding its chosen note, by the same hand-off as its pitch (SN-14). A `voiceMode` change while notes sound crossfades over 5 ms (PA-5) from the old mode's subs to the new mode's, which start fresh. |
| SN-8 | **The noise's source** (`type`; all four kept by the human's ear), each at the level of Clean unless stated. **Clean:** uniform white noise in [−1, 1) from the voice's `noise` stream, one draw per sample (the round-25 noise). **Crackle:** sparse pops, the Clean noise times a pop level e_p times 2.5: each sample, with probability `density`/f_h (a uniform draw from the `noise.crackle` stream), a pop starts, e_p ← 0.4 + 0.6·u (u a second draw); otherwise e_p decays by e^(−1/(0.0015·f_h)) (1.5 ms); its level grows with the density. **S&H:** a phase advances by min(1, `rate`/f_h) per sample, and each time it wraps the held value takes the coloured Clean noise's current sample (at a fresh start the phase is 1, so the first sample takes one). A hold is a raw step, not band-limited (exempt from ED-1). **LFSR:** a Fibonacci shift register of b bits: s ← ((s ≪ 1) \| (bit_{b−1}(s) ⊕ bit_{t−1}(s))) & (2^b − 1), output +1/√3 while bit 0 is set, else −1/√3 (Clean's variance, so colour, level and filter treat it like any source). `length` picks (b, t) from (3, 2), (4, 3), (5, 3), (6, 5), (7, 6), (9, 5), (11, 9), (15, 14): periods 2^b − 1, from 7 to 32 767 steps, each maximal. The register steps each time a clock phase wraps, the phase advancing by min(1, f_n·r·M/f_h) per sample, f_n the voice's published pitch, or C3, 130.81 Hz, when `lfsrKeytrack` is off (as if every note were C3; only the LFSR's clock is pinned, not the Resonator or the filter's keytracking), r the Pitch ratio (SN-11), M = min(2^b − 1, 64), the clock's cap at one step per sample kept: a register of 63 steps or fewer repeats at the note's pitch, a longer one is hiss. At a fresh start the register is 1 + ⌊u·32 767⌋ masked to b bits (1 if that is 0), u from the `noise.lfsr` stream; a `length` change masks the running register the same way. Each output step is raw, exempt from ED-1. S&H and LFSR stay raw, as the lab has them, because their aliasing grit is the sound the human approved, the reason a bit-crush character was dropped from the noise; any aliasing floor for them is reported, not judged. **The streams** `noise.right`, `noise.crackle` and `noise.wind` advance every sample (the Wind walk at its own rate) whatever the source, spread and stage switches, so turning one on mid-note is deterministic in time alone. |
| SN-9 | **The noise stages**, Resonator, Whisper and Wind, each with its own switch, in that fixed order, in series. Resonator and Whisper are linear and time-invariant, so their order is inaudible; Wind's band moves, so it comes last, where it moves the whole sound. **Each stage is level-matched:** its output is scaled by g = √((P_in + 10⁻¹²)/(P_out + 10⁻¹²)), clipped to [0.02, 30], P_in and P_out one-pole averages (time constant 0.3 s) of the squares of its input and of its unscaled output on the left channel, both 1/3 at a fresh start, the same g scaling both channels: a stage changes the colour, not the level. **Resonator:** per channel, a feedback comb tuned to **f_r = clip(f_n·r, 20 Hz, f_h/2.5)**: y[n] = (1 − β²)^¼·x[n] + β·½·(d[n] + d[n − 1]), d[n] the line's output at a delay of f_h/f_r − ½ samples, read by linear interpolation, the line written with y[n], β = `feedback`. The two-tap average damps the highs as a plucked string's loop does, and the half sample it adds makes the loop one period long; the input gain (1 − β²)^¼ holds white noise's level through the loop. The delay follows the smoothed pitch each sample, so a Pitch move glides; the line is cleared at a fresh start. PROPOSED: the 20 Hz floor bounds the line's memory (64 voices × 2 channels × f_h/20 samples, about 4.9 MB at 192 kHz in single precision). **Whisper:** three band-passes, the formants of a vowel, summed: N·Σⱼ Gⱼ·kⱼ·bpⱼ, each bpⱼ the band output of a topology-preserving state-variable filter (g = tan(π·fⱼ/f_h), damping kⱼ), G = (1, 0.7, 0.4), k = (0.18, 0.12, 0.10). The formants (Hz): a (800, 1150, 2900), e (400, 1600, 2700), i (270, 2140, 2950), o (450, 800, 2830), u (325, 700, 2700); `vowel` v ∈ [0, 1] sweeps them at 4v, each formant interpolated in log frequency between the two neighbouring vowels and capped at 0.45·f_h. N makes white noise in give the same RMS out: N = (∫₀^{f_h/2} \|H(f)\|² df / (f_h/2))^(−½), H the three bands' summed response. **Wind:** a band-pass, the same filter with damping 0.25, whose centre wanders: f_c = clip(800 Hz·2^(D·w(t)), 60 Hz, 0.45·f_h), D = 0.4 + 1.4·`gust` octaves; w moves from each target to the next, uniform in [−1, 1) from the `noise.wind` stream, by a smoothstep (3u² − 2u³) over each target's interval, a new target every 1/ρ s, ρ = 0.1·20^`gust` Hz (0.1 to 2 Hz); two targets are drawn at a fresh start, and the walk is shared by both channels. Its output is min(30, (π·0.25·f_c/f_h)^(−½))·0.25·bp, which divides out the band's share of white noise's power so the level holds while the centre wanders. |
| SN-10 | **The noise filter** (`filter`; the human kept it, ahead of the voice's filters): Off passes the signal unchanged; LP, BP and HP are a topology-preserving state-variable filter's low output, its band output times k (unity at its peak) and its high output, with g = tan(π·f_c/f_h), damping k = 2 − 2·`resonance` (Q from 0.5 to 5) and **f_c = clip(`cutoff`·(f_n·r/261.6256 Hz)^`keytrack`, 20 Hz, 0.45·f_h)**: keytrack 1 follows the note fully from C4, 0 not at all. Its coefficients follow the smoothed values, recomputed at least once per control interval and reaching the filter as a ramp across it (PA-3). |
| SN-11 | **The noise's Pitch** (`pitch`, ±48 st, smoothed by PA-1): **r = 2^(`pitch`/12)** multiplies the note's pitch f_n everywhere the noise follows it, and only there: the LFSR's clock (SN-8), the Resonator's tuning (SN-9) and the filter's keytracking (SN-10). Clean, Crackle, S&H, Whisper and Wind follow no pitch and are untouched, so with no LFSR, no Resonator and keytrack 0, Pitch changes nothing (PA-8). |
| SN-12 | **Stereo spread** (`spread`, s; the human kept it). At s = 0 the noise is mono, exactly the round-25 noise: one chain, both channels the same. Above 0 a second chain runs for the right channel, fed by a second white stream under its own purpose, `noise.right`: **the right channel's white draw is (1 − s)·u_L + √(1 − (1 − s)²)·u_R**, u_L the left's draw and u_R the second stream's, so the channels' correlation is 1 − s and the right keeps the left's variance; the LFSR's right register (drawn after the left at a fresh start) mixes the same way, and Crackle's pop level and Wind's walk are shared. When s rises from 0, the right chain's states start as copies of the left's, so the channels begin equal and part. After the noise filter the two channels are panned by C.8's single-source law, the left by L = cos a and the right by R = sin a, and both are scaled by **√(2/(2 − s))**, which keeps the level of the mono sum at centre the same at every spread: decorrelating the channels widens the noise without making it quieter in mono (the lab's 1 + (√2 − 1)·s matched this at s = 0 and 1 and was within 0.4 dB between). |
| SN-13 | **The sampler's reservation.** The human ruled that the noise oscillator will play samples after 1.0, one sample per patch, one-shot or looping, keytracked or at a fixed rate, from a start position or a random one, its colour, stages, filter and envelope still applying; it is designed now so that a 1.0 preset loads unchanged once it ships. Reserved now: (1) `noise.mode` (5.6), whose one 1.0 option is `noise` and to which the sampler appends `sample`; (2) the four reserved rows `noise.sampleLoop`, `noise.sampleKeytrack`, `noise.sampleStart` and `noise.sampleRandom` (5.6), each at its later default in 1.0 (one-shot, keytracked, start 0, random 0), last in the noise's block (any further sampler key appends after them, RC-6.1); (3) the state member `samples` (IF-4), an object keyed by source (`"noise"`), each value naming the sample's file by reference, never holding its audio (a host can fail, silently, to save a large state). In 1.0 a state without these keys loads them at v = 0; a state carrying them at another v keeps them byte for byte (5.1) and plays the noise as 1.0 defines it, and keeps a `samples` member unchanged (RC-9.3). The sampler replaces only the source (SN-8). |
| SN-14 | **Erode** (`erode`, a; the human's ruling: the noise modulates the sub's time). At a > 0 the sub's shape is read at the **eroded phase θ_e = θ − f·τ(t), τ(t) = a × 4 ms × n̂(t)**, f the sub's frequency in Hz and n̂ the noise oscillator's left channel (its only channel at spread 0) after its source, stages and filter, before its envelope, level and pan, scaled by 0.25·√3 so that Clean noise, of RMS 1/√3 here, reads RMS 0.25, the lab's noise level for which its 4 ms was voiced (so a = 1 is a delay of about 1 ms RMS on Clean noise). It is the exact form of a short delay modulated by the noise and applied to a periodic signal, so it needs no delay line: harmonic h moves h times as far, and a higher sub moves further, its sidebands' power rising 6 dB per octave of the sub's pitch while 2π·f·τ_RMS ≪ 1 (beyond it the sidebands saturate). τ is bounded, so θ_e never departs from θ by more than f·max\|τ\|: **Erode never changes the sub's frequency on average.** At a = 0 nothing is read and the sub is bit-identical to one without Erode. The noise's level, mute, pan and envelope do not reach Erode, so a silent noise erodes as a loud one does. **Which noise:** the per-note sub reads its own voice's noise; the mono sub reads the noise of the voice holding its chosen note (if two voices hold its key, the newest), and while that note holds no voice (SN-7's hand-off: in `voice.mode` mono or legato a held note behind the sounding one, or its voice freed) n̂ is 0. With no note gated there is no chosen note and n̂ goes to 0, which would step the sub's phase as it releases. Every change of the voice it reads, and every move to or from n̂ = 0, the release included, crossfades n̂ over 5 ms (C.0), because a jump in n̂ is a jump in the sub's phase (the lab switched at once). **Edges:** ED-1 places each step of the saw, square and pulse, and ED-6 each triangle corner, where the straight line between θ_e at the two samples crosses it, the corner's slope taken from the same step (θ_e's own step, not the pitch step: at a = 1 the eroded phase moves far further than a pitch step); a crossing in either direction counts, with its sign (θ_e moves backward when the noise moves fast), and RT-2's cap of 8 edges per sample holds. Aliasing under Erode is reported, not judged (QS-11). The bump's fades and the smooth square's k read f, not θ_e's rate. Effects device: none. |

---

## 7. Interfaces to the rest of the instrument

### 7.1 Interfaces

The engine's API is a set of calls and timed events; everything crossing it is in the units of section 5. The suite's harness header, written by the test author from this section, fixes the exact signatures; the builder implements that header.

| id | area | the engine exposes or accepts |
|---|---|---|
| IF-1 | **Notes, voices, expressions** | Accepts note-on (note id, channel, key, velocity), note-off (note id, release velocity), the sustain pedal (VO-12), all-notes-off and all-sound-off (VO-13), channel pressure, and per-note expressions, each sample-timed. Expressions are addressed like note-offs (VO-1). CLAP's expressions map so: tuning → that note's pitch (IF-2); volume → a per-voice linear gain on the voice's output (0 to 4, as CLAP sends it); pan → an offset added to every source's pan for that voice; vibrato, expression, brightness (MPE timbre) and pressure (MPE per-note pressure) → per-voice modulation sources. **Velocity and aftertouch** (channel pressure, and per-note pressure under MPE) are modulation sources; velocity's default mapping is `env1.velocity` (VO-8); aftertouch's is [X21]. Pressure never reaches the level unless a route maps it, so a keyboard's aftertouch cannot silence held notes. In mono and legato the voice follows the expressions of the newest held note id (VO-6). Publishes VO-10's lifecycle events and the load readouts (voices culled, refused, taken). [Q2] **The stream tag** (round 30, reserved): a note-on may carry a stream, `direct` (what the player holds), `chord` (what the chord stage layers on a played note) or `arp` (what the arpeggiator generates), `direct` when absent (round 31 added `chord`); note-offs and expressions address the note by its id as before. In 1.0 the engine accepts the tag and records it with the voice (VO-1), and nothing reads it (RC-6.7). |
| IF-2 | **Pitch** | **Pitch is addressed by note id**, like the expressions: the pitch system sends each note's pitch in Hz (bend, note lane and glide, quantizer, MTS-ESP, transpose and per-note tuning already applied), at audio rate when it moves. Until a note's pitch is set, the note plays 12-TET from its key (A4 = 440 Hz) × 2^(tuning/12), tuning the note's tuning expression in semitones. **A pitch sent for a note id at its note-on's sample** (in the same processing call, in either order) applies from that note's first sample, so a glide begins at its start pitch and the note never sounds its 12-TET pitch first; the same holds for every per-note expression of IF-1 (volume, pan and the rest), sent for a note id at its note-on's sample in either order (round 19, ADR-243); a note that waits for a taken voice (VO-3) keeps its pitch input moving while it waits and starts at the pitch it has reached. The order is: the input, then gravity's ratio (the **sounding pitch**), then each oscillator's transposition. In mono and legato the voice follows the pitch of the newest held note id. Accepts the host's tempo and transport (RC-7). Publishes each voice's sounding pitch and **master phase** (the integral of the sounding pitch from 0 at the voice's fresh start) for the sub, the noise oscillator and keytracked filters. Gravity's settled ratio rides the input multiplicatively (C.6), so a bend or glide keeps the interval gravity settled. [Q6, Q7] |
| IF-3 | **Modulation, the intent bus and macros** | Every continuous parameter is a destination by its manifest key and id, with unit, range, law, class and rate (section 5). Per-voice offsets (PA-2) at the control rate (PA-3), audio-rate offsets on the destinations of PA-4. Every modulation destination is also an automatable host parameter, because wrappers drop CLAP's parameter-modulation events. Sources the engine publishes per voice: the levels of ENV 1 to ENV 4 (EN-5), R and ψ of each oscillator's swarm, the onset burst's level, gravity's settled offset in cents, velocity, and the voice's expressions (IF-1). Macros and intents bind by key and curve (`lin`, `exp`, `log`) and need nothing more. **Round 62 (Appendix E; ADR-379 r2):** each published source is readable as it stands at each control-interval boundary (MS-3); the modulation system reads velocity and the expressions from the shell's note events, as the engine does (MS-9). The engine also computes the per-note LFOs (MS-43) and evaluates the engine path's terms on its own per-note sources (MS-41), and takes each key's Control input (PA-2). [Q12] |
| IF-4 | **Morph and state** | The manifest's class column is the morph's input, **and Appendix D is its law**. Morphable values blend or flip by the law and the key's own mode (MO-6 to MO-13). A Structural key resolves to one value, which changes on sounding voices by PA-5 (MO-13: in 1.0 no Structural key blends, and setting sets are after 1.0, ADR-377). Device values stay out. **A source's morph level (MO-26) is an input, never state,** like the transport: a load or a restore leaves it unchanged (IF-12). **Values the morph sends (its `Param` events and levels) are not reported to the host as parameter changes**: they are the morph's output, not edits; the host automates the morph's own parameters instead (IF-11). The engine reports the heard value of every stepped key per oscillator, and each source's heard morph level, so the GUI shows what is heard where the morph and the stored patch differ. **Each corner's `ratios` texts are the morph system's state, beside the corners' numbers (RC-11.1; ADR-327); the engine receives a text only as its `ratios` input from the morph (MO-20), and its state holds only the stored texts.** **State** is a key-value document carrying `version` 1, the parameter values (in their normalised form v), the `ratios` text and the quality setting: saving, loading and saving again gives byte-identical text and bit-identical renders; floats round-trip exactly; live state (tails, ramps in flight, meters, latches, stream positions) is never saved; a document without `version` is the version-0 format (B.2). A load made while not processing is whole when it returns; a load while voices sound fades them over 5 ms and starts fresh (the engine's memory is reset: streams rewound from their seeds, gravity, the ensemble's carried offsets and the resume snapshot cleared). A saved project restores as saved. **The text:** version 1 is a UTF-8 JSON object with no whitespace and exactly the members `version` (1), `params` (manifest key → v, in ascending manifest id order) and `ratios` (`{"osc1": text, "osc2": text}`), with any keys and members this build does not know kept after them (RC-9.3; the member name `samples` is reserved for the sampler, SN-13, and 1.0 treats it as one it does not know), each number written as the shortest decimal that reads back as the same value; version 0 is a JSON object of B.2's keys → numbers (and `kCustom` → text) with no `version` member. A set of version-0 example states is supplied to the test author as input data with the reference renders. [Q15, Q17] |
| IF-5 | **The per-voice filter slot** | RC-1. |
| IF-6 | **Mixer, mute and solo, meters** | Levels are named by the rule Gain = a stage's multiplier, Level = a contribution to a mix, Volume = the master only. **Mute and solo** are each source's gate, driven by the device mixer: a muted source is silent; while any source is soloed, every source that is not soloed is silent; a soloed source that is also muted is silent; the gate opens and closes by a 5 ms ramp, and a gated source keeps rendering, so un-muting is phase-continuous. **Meters** (peak, and a latching clip flag that catches a single sample over 0 dBFS, readable and clearable from another thread): each source after its level, pan and gate, times its governing envelope and velocity factor, summed over voices (the mono sub's own envelope for the mono sub) (a metering tap only; the filters are not in it); and the engine's output. The mixer's make-up gain, between the engine's output and the FX rack's first slot, restores loudness (RC-2); the engine does not. [Q20] |
| IF-7 | **The FX rack** | RC-5. |
| IF-8 | **True stereo** | Members are mono and panned (SW-18); the stereo field after the voice is the device's. The engine exposes each oscillator's width and pan to whatever width equation the device settles; it guarantees no member is polarity-inverted between channels. [Q26] |
| IF-9 | **Quality and CPU** | `quality` is per patch (saved), Device class, never morphed (VO-14 for changes while notes sound). **Operation counters**, deterministic and readable: member-samples rendered, edges corrected, edges left over (RT-2), envelope steps, voices sounding. Structure setting-samples are after 1.0 (D.6). [Q27] |
| IF-10 | **The labs and the observation view** | A WebAssembly build of the same source, with the same API, which a lab loads in an AudioWorklet, built with one pinned toolchain version: **Emscripten 6.0.13** (the human's word, 2026-10-24, at stage 8; ADR-280). The clean room builds the module and its offline render entry, which item 15 judges. The AudioWorklet wrapper that a lab loads it through is built with the labs, outside the clean room (round 37, ADR-283). An observation view, read without changing the audio, per voice and oscillator: members' phases, each member's uncorrected output y⁰ (C.14's, before ED-1's corrections and filter; round 42, the human's ruling), natural and coupled frequencies, σ, S, P, Kenv, drift values D, spread positions p, effective blade values (C.9 item 8), C.16's estimate and subtracted value per member, θ per FM free blade (round 56), pan gains, levels, R, ψ, the frame angle, gravity's ratios, onset waits, and the heard stepped values; per source, the heard morph level (MO-26) (the heard setting sets are after 1.0, D.6); per voice, the four envelopes' levels and stages; the sub's phase, its uncorrected shape y⁰ (SN-2's shape as played, at unit level, before ED-1's corrections and filter, the tone, the governing envelope, the drive and M; round 42), its bump peak and its subtracted mean M (SN-2, SN-6), and the mono sub's chosen note; the noise's stage gains (SN-9). The invariant suite's law checks (10.1 item 0) read it. No lab carries its own engine. |
| IF-11 | **The host (CLAP)** | Through the shell: the manifest generates the host parameter table from its non-reserved rows (5.1), **and the morph's manifest (D.2) appends the morph's host parameters to it: the position's `morph.x` and `morph.y`, `morph.on`, `morph.law`, `morph.count`, `morph.temperature`, `morph.cohesion`, `morph.stepRate`, `morph.stepSync`, `morph.stepDivision`, `morph.stepTriplet`, `morph.stepDotted` and `morph.glide`, never `morph.seed` or `morph.audible` (ADR-326, X39; ADR-327; round 48); each one row of one range, taper and step count**, committed as a lockfile (id, key, min, max, default, taper, steps), ids explicit, append-only, below 0xB00000; **the modulators' block (MS-40) is appended after the morph's (round 62), and reaches the modulation system and the resolver, never the engine**. **The morph's parameters reach the resolver, never the engine (IF-12).** **Every host parameter's value is its normalised v ∈ [0, 1]** (the law maps it; stepped keys in steps of 1/(n − 1)); its display shows the unit, and v = 0 of a dB row shows −∞. Activation refuses a sample rate that is non-finite or outside 8,000 to 768,000 Hz (the certified range is 44.1 to 192 kHz); the engine takes host tempo and transport as inputs (RC-7). [Q31] |
| IF-12 | **The morph** | **The resolver's calls** (MO-1, MO-3): prepare (the host rate); reset; the corners (four arrays of v indexed by manifest id, sized at prepare from the manifest, and each corner's two `ratios` texts, at most 256 bytes each, MO-13); the morph settings (D.2); each key's mode and count (MO-11); **each Morphable key's waypoints, in their stored order, and the patch's waypoint law and clamp (MO-21; round 61, ADR-373); each key's region lock, or none (MO-24)**; the position (x, y), sample-timed; the transport, as RC-7's events; **a read of the current resolution, which changes nothing: each key's resolved value (MO-25) and a key's region shape as a lock would take it now (MO-24)**; and process (a sample count), which advances the resolver's clock and returns its events for those samples, each timed at a control-interval boundary. The shell passes them to the engine in the same block. **The morph level** (MO-26), per source (osc 1, osc 2, the sub, the noise): a parameter the resolver computes, taken by the engine as an input and not a manifest row, a value g in [0, 1], timed like a `Param` event (PA-3), applied as one more factor on the gain of the source's level key (`mix.osc1`, `mix.osc2`, `mix.sub`, `mix.noise`) and glided by PA-1 only; an input, never state (IF-4), like the transport: prepare and `reset()` set it to 1, and a load or a restore leaves it unchanged; checked on entry by PA-7. In 1.0 the engine receives no `SettingSet` event (it is after 1.0, D.6), and every Structural key arrives as a `Param` event. A `Param` event from the resolver may carry a **landing** mark (MO-26 (4)): the engine then applies it at once, without PA-1's glide, PA-5's crossfade or PA-6's fades. Events apply in arrival order (PA-3), whatever their source. While the morph is on, the morph system routes the host's and the GUI's edits of morphing keys to the corners (ADR-109), so the resolver is their only source. The morph's own settings and position are host parameters of the shell (IF-11), never events to the engine. Checked on entry by PA-7. **Round 62 (Appendix E; ADR-379 r1, r4):** the position the laws read is `morph.x` and `morph.y` plus the modulation system's position offset (MS-29), clamped to [0, 1], taken one control interval late (MS-34). The resolver resolves the modulation manifest's keys (E.5 to E.8) by the same law (MS-27), with MO-11's menu only: a waypoint list or a region lock on one is refused; their `Param` events go to the modulation system, never the engine. It takes, per corner, the modulation system's **erosion flag** (MS-44), which MO-26 (7) reads. And it answers, changing nothing, **every pick the modulation system needs** (MS-25, LF-8): a key's own pick, a source's restricted pick (MO-26 (2), (3)), and the pick MO-7 gives a named purpose at the unglided weights, with no latch, so the modulation system implements no part of MO-6 to MO-8. |
| IF-13 | **The modulation system** | **Its calls** (round 62; MS-1, MS-2): prepare (the host rate; MS-6's maxima allocated); reset; the modulation state (MS-36: every corner's routes, the patch-wide routes, the modulators that exist, each LFO's points, `mod.loops` and the seeds); its host parameters (MS-40); the modulation manifest's resolved values and the picks it asks for, from the resolver (MS-25, MS-27); the note events (velocity and release velocity included), the expressions, CC 1, the bend and channel pressure, sample-timed, as the shell passes them to the engine (MS-9); the engine's lifecycle events with each voice's note counter (VO-10) and its published sources at each control-interval boundary (IF-3, MS-3); the transport, as RC-7's events; the macro, X and Y values (MS-10); and process (a sample count), which advances its clock and returns its events for those samples, each timed at a control-interval boundary or, for a new note, at its note-on's sample (MS-42): per-voice offsets by note id and key (PA-2, PA-3), each key's Control input (PA-2), the engine path's triples (MS-41), each per-note LFO's settings, points and modulated settings (MS-43), the morph position's offset and each corner's erosion flag (IF-12), voice pitch's quantized contribution (MS-22) and the FX values' offsets (RC-5). The shell passes them on in the same block. **The engine's sources at a boundary:** the shell runs the engine and the modulation system in turn, one control interval at a time, or the engine hands over each interval's end values; both give the same events, independent of block size (MS-5). **An observation view** (MS-45), read between calls without changing anything. It reads no clock, allocates nothing after prepare (MS-6), and its host parameters never reach the engine. The engine checks what it receives by PA-7, as any input. |

### 7.2 Reserved contracts

Each module still to come meets the engine through a contract that exists now. For each: what the engine provides and accepts now; what each voice hands over or accepts, where it applies; what is deferred to the module; and what the module must not require of the engine. RC-6 to RC-11 also carry numbered requirements (RC-6.1, …), tested by the suite where they can be tested now (10.1 items 18 to 23, 30 and 31).

#### RC-1. The filters

- **Provided now.** A per-voice filter slot after each source's governing envelope (4.2, VO-9), passing the signal unchanged while empty. The builder builds the slot and nothing in it.
- **What each voice hands the slot:** each source's signal after its level, pan, gate and governing envelope (stereo, per voice), and their sum; the voice's sounding pitch (for keytracking), master phase, velocity, per-voice modulation and lifecycle (start, release, take, free). The slot returns one stereo signal per voice, which joins the sum of the voices. **The mono sub (SN-7) has one slot instance of its own** (the human's ruling, 2026-10-21: "Its own filter, following its note"). That instance is handed the mono sub's signal and, from the voice holding the chosen note, that voice's sounding pitch, velocity, per-voice modulation and lifecycle, so it behaves as that note's filter. At a hand-off its own state carries over, with no jump between voices, while its inputs move to the new chosen note. The mono sub's lifecycle (SN-7) resets it and reads its tail report. *Why its own instance, not the chosen voice's:* adding the sub into a voice's filter would hop it between filter states at each hand-off, and the tail of a voice's filter would carry the sub after that voice's note had moved on.
- **Must provide:** a tail report, so a voice frees only after its filters' tails fall below the free threshold (6.4): the envelopes act per source before the slot, and the filters ring out.
- **Must provide, when built:** its Structural keys declared to the morph, which follow their quantum boundaries (MO-13) and change by PA-5's crossfade. After 1.0 (ADR-377): a filter instance per setting of the `voice/filter` group's set (SB-1), each cleared at its start (SB-6), their outputs mixed by SB-2, the keys declared as the `voice/filter` group (MO-14).
- **Deferred:** the filter types, their count and how sources route to them [X13] (built until ruled: two filters per voice, each source routed to either, both or neither, serial or parallel); per-filter cutoff tracking of R.
- **Must not require:** a filter must process each voice's stereo pair with the same coefficients on both channels, add no latency (or declare it so every parallel branch is aligned), clear its state when its voice is reused, and run no unbounded work per sample; it must not need the members separately.

#### RC-2. The mixer's gain staging

- **Provided now.** The four source levels (dB), each source's pan, mute and solo (IF-6), the source and output meters with their clip latches, `out.level`, and the sum of the voices; the meters' readings, for display, in the snapshot (RC-8).
- **The make-up gain sits before the FX rack.** The mixer's **make-up gain**, a user control, takes the engine's output to the rack's first slot (RC-5). Its default is ×4 (+12.04 dB): with it, the rack receives 0.56 × the sum of the voices at `out.level` 0 dB. The engine's output stays linear and unclipped (OU-1), and so does the make-up, so the level reaching the rack may exceed 0 dBFS: the rack's input meter and clip latch show it, and the master limiter after the rack holds the device's peaks.
- **The gain-staging view.** The mixer panel shows the level as it moves through the chain, with a gain control at every stage (each source's level, each filter slot's input and output, `out.level`, the make-up, each FX slot's input and output, the master's input and volume) and, at every stage, a meter with a peak hold and a clip lamp that latches a single sample over 0 dBFS until cleared: each source after its level, pan and gate; each filter slot's input and output (the filters' own meters); the engine's output; the make-up gain's output, which is the rack's input; each FX slot's input and output; and the master stage (limiter in, gain reduction, out). Clicking a lamp clears it. The engine supplies the readings for its own stages (IF-6, RC-8); the filters, the rack and the master stage supply theirs.
- **Deferred:** the mixer panel and its drawing, its faders' display laws, the make-up gain's range and any automatic mode [B499], the filter and FX slots' input and output gains (the filters' and the rack's), and any bus routing beyond the per-voice filter slot.
- **Must not require:** a gain stage inside the engine beyond those listed (the make-up gain is the mixer's); a meter tap after the filter slot (the mixer reads the filters' own meters).

#### RC-3. The pitch system

- **Provided now.** The per-note pitch input (IF-2), the 12-TET fallback, gravity's place (after the tuning, before each oscillator's transposition), the note-change reports of VO-6, and the published sounding pitch and master phase. **The mono sub's glide** (SN-7): the engine reports each change of the mono sub's chosen note (from note id, to note id) and accepts a pitch for the mono sub, in Hz at audio rate, at an address reserved for it beside the note ids; while one is sent it replaces the chosen note's pitch. Until the pitch system exists nothing sends it, and the sub steps to each new note's pitch.
- **Deferred to the pitch system:** the bend lane and its laws, the note lane and glide (including the bank's glide values: each bank preset's `glide` time and `glideAlways` mode, B.1, among them the 12 bank presets that play legato and the 22 poly presets that store a glide time). **What the pitch system is expected to bring, so the engine leaves room for it:** glide in poly as well as mono and legato, each new voice travelling from a previous pitch; the travel laws (constant time, constant rate, lag, and a spring that overshoots and rings); scale quantization as a modifier on any law (the emitted pitch snaps while the law's dynamics run underneath, with a hysteresis and a minimum time between steps, so a quantized glide is a run of notes); where a glide starts from (a key still held, a note still sounding including its release tail, or the last note played even across silence); and in a chord, each voice from its own nearest predecessor. The engine's part is IF-2's per-note pitch at audio rate from a note's first sample and VO-10's lifecycle; the quantizer, MTS-ESP, transposition by the device, and per-patch bend range [Q6, Q7].
- **Must not require:** the engine to glide, bend or quantize; the engine to know a scale; a pitch input slower than control rate when it moves.

#### RC-4. The modulator bank

- **Computed now, by the engine** (round 26): ENV 1 to ENV 4, per voice, by C.12, from their own rows (5.2, EN-5); their levels are published per voice as sources (IF-3). Every source's level follows one of them (RC-6.5). They are the engine's because the voice law reads them (EN-5).
- **Accepted now.** Per-voice offsets by note id (PA-2), at the control rate (PA-3) and at audio rate on PA-4's destinations; sources published per voice (IF-3).
- **One-shot.** ENV 2 to ENV 4 offer a **one-shot** mode (`oneShot`) in which a key-up does not end the attack or decay (the envelope runs its stages to their end). PROPOSED: the bank's blade envelopes translate to ENV 2 and ENV 3 in one-shot mode (B.1), so their swells survive short notes.
- **Default routes** of the init patch and the factory presets: velocity through each envelope's `velocity` row (VO-8); aftertouch [X21].
- **Specified (round 62):** the LFOs, the Kuramoto LFO, ORBITAL, the 2-D random, the matrix and every route (an envelope's route to a parameter included) are Appendix E's modulation system, outside the engine, except what ADR-379 r2 puts inside it: the per-note LFOs (MS-43) and the per-sample terms of the engine path (MS-41).
- **Deferred:** macros and the intent bus themselves.
- **Must not require:** the engine to compute an LFO other than the per-note LFOs (round 62, ADR-379 r2), or an envelope beyond ENV 1 to ENV 4; any route evaluation beyond MS-14 and MS-15's closed form on the engine path's triples (MS-41); a route depth outside the destination's normalised range; any destination not in the manifest.

#### RC-5. The FX rack

- **Provided now.** One stereo pair at the host rate with zero latency, the engine's output after the mixer's make-up gain (RC-2), into the rack's first slot; the silent report and skipped-samples rule (OU-4). The engine offers no insert point of its own [Q23].
- **Deferred:** the rack, its modules, and the device's master limiter (which holds the device's peaks, OU-1).
- **Must provide, when built:** for each slot, its Structural keys (the module patch's structure, as the rack defines it), which follow their quantum boundaries (MO-13): each resolves to one value, and a change is made by the rack's own switching. After 1.0 (ADR-377): the keys as its `fx/N` group (MO-14) and its rendering of a two-setting set (MO-19), two module instances, each holding one setting, both receiving the blended Morphable values, their outputs mixed by SB-2's law at the set's weights, with SB-11's slot rule. **FX device (ADR-254):** the same.
- **Must not require:** a peak guarantee from the engine (OU-1, QS-8), a sidechain from inside a voice, or a latency from the engine.

#### RC-6. A source slot in the voice

- **Provided now.** The voice holds its sources through one contract, the **source slot**. osc 1, osc 2, the sub and the noise oscillator are the slot's first four instances and are built through it, so a later source (a formant source, the blade-to-blade controls of a later blade version, any other engine) is added beside them without restructuring the voice.
- **What each voice hands a source:** its published sounding pitch and master phase (IF-2), to which the source adds its own transposition; the gate (held, sustained, released, taken, freed: 6.4); its governing envelope (RC-6.5); to the sub, the noise oscillator's signal before its envelope, level and pan, for Erode (SN-14); velocity; the note's expressions (IF-1); the per-voice modulation offsets of the source's own parameters (PA-2, PA-4); the control-interval tick (PA-3); the voice's note counter and seeded streams under the source's own purposes (DT-4).
- **What a source hands the voice:** its per-voice stereo signal, panned by its own law, before its level and gate (VO-9); a published phase where it has one (as the sub's, SN-1); its readouts (RC-8) and observation values (IF-10); its operation counters (IF-9, VO-11); its parameters, in the manifest under its own id range.
- **Requirements.**
  - **RC-6.1** The four sources are instances of the slot: the voice's code names no source kind, and each source's parameters occupy a contiguous manifest id range of their own, append-only within it (RC-9.1).
  - **RC-6.2** A source allocates nothing after prepare, reads no clock, and reaches no other source's state except through what the voice hands it: a source that follows another's phase reads the voice's published phase, never the other source.
  - **RC-6.3** A source that is off renders nothing and costs nothing (PF-6, VO-11), except the noise running silently for Erode (SN-4).
  - **RC-6.4** Adding a source changes no other source's output. Tested (10.1 item 18) by a test build that adds a fifth source through the slot (a plain sine at the voice's pitch, switched by `sine.on` and carrying the slot's envelope selector **`sine.env`**, Step, ENV 1 to ENV 4, default ENV 1, round 27): the four existing sources render bit-identically with and without it, and its counters read zero while it is off.
  - **RC-6.5** **The envelope selector** (round 26; the human's ruling: no source carries an envelope of its own, every source's level follows ENV 1 by default, and a selector lets it follow ENV 2, 3 or 4 instead). Each source slot carries `env`: ENV 1 (the default) or ENV 2, 3 or 4. The voice multiplies the source's signal, after its level, pan and gate, by that envelope's level and velocity factor (VO-8, VO-9) (the sub applies it inside its own chain, SN-3). A preset or state that names no selector plays ENV 1 (RC-9.2). Every envelope keeps the voice's legato (VO-6): on a legato change no envelope restarts, whichever source follows it. A later source gets the selector through the slot and brings no envelope of its own.
  - **RC-6.6** **A source instanced once for the instrument.** A source may run once for the instrument instead of once per voice; in 1.0 only the sub does, in mono (SN-7). The instrument hands it what a voice would (the chosen note's pitch, a gate, a velocity, the control tick, the chosen note's offsets) and its own instance of its governing envelope; it joins the sum of the voices after the filter slots and holds no voice.
  - **RC-6.7** **The stream selector, reserved** (round 30; the human's ruling: the arpeggiator reaches chosen sources by a per-source note stream). Each source slot carries `stream`, a reserved row (5.1) whose only 1.0 value is All, index 0; a later source gets it through the slot. Once built, the selector is three toggles, Direct, Chord and Arp, in any combination but none (the human's ruling, round 31): seven values, All (every toggle on) at index 0, the other six appended after it (RC-9.4). **Deferred, reserved:** once built, a voice renders only the sources whose toggles include its note's stream; the others render nothing and cost nothing (RC-6.3); every stream's notes share one voice pool (VO-5), since two pools would double the voice and CPU budget for little a selector cannot do; and the mono sub chooses only among the notes of the streams its toggles include (SN-7); and the sustain pedal (VO-12) holds `direct` voices only, never `chord` or `arp` voices, which the arpeggiator holds by its own input hold (the human's ruling, round 36, ADR-282). Built when the arpeggiator's core lands (ADR-269, ADR-270). In 1.0 every selector is All, which is the behaviour every patch already has. Effects device: none; in the FX device the sidechain's notes are the direct stream.
- **Deferred:** the later sources themselves; a fifth source's place in the mixer (RC-2); the stream selector's behaviour (RC-6.7).
- **Must not require:** a change to the voice law, the envelopes or the filter slot; another source's internal state; an allocation by the voice; a second voice for one note.

#### RC-7. Tempo and transport from the host

- **Accepted now.** The host's tempo (beats per minute), time signature, song position (in quarter notes) and play state (playing or stopped, and the loop's bounds when looping) as **sample-timed transport events**, like note events (IF-1): each event's values hold from its sample on. Nothing reads a clock (DT-5); every tempo-dependent behaviour is a function of these events and the sample count. **In 1.0 its first consumer is the morph resolver's synced steps (MO-10, ADR-325), which also receive its jumps; the modulation system's synced LFOs (LF-4, LF-12), its 2-D random's Tempo trigger (MR-6) and ORBITAL's Transport reset (MB-8) take the same events, through IF-13 (round 62).**
- **Without a host transport:** 120 beats per minute, 4/4, stopped, at position 0, until the first transport event.
- **Jumps and loops:** a position that does not follow from the previous event's position, tempo and the samples elapsed (a locate, a loop's wrap) is a **jump**, delivered to the consumers as a jump at that sample and never interpolated.
- **Requirements.**
  - **RC-7.1** Transport events take effect at their sample time, independent of the host's block size (DT-2).
  - **RC-7.2** Between events, while playing, the position advances as position(n) = position(n₀) + (n − n₀)·tempo/(60·f_host) quarter notes; while stopped it holds.
  - **RC-7.3** In 1.0 no sound of the engine depends on the transport **except through the morph's synced steps (MO-10; ADR-325)** and, given the modulation system's events, nothing of its own; the instrument's sound also depends on it through the modulation system (round 62: LF-4, MR-6, MB-8), whose events reach the engine as any input: **with `morph.stepSync` off, or the morph off,** a render is bit-identical with and without any transport events (10.1 item 19).
  - **RC-7.4** **The standard tempo divisions.** Every part of the instrument that syncs to tempo (the morph's stepped hybrid, and the LFOs, the arpeggiator and the delays when they are specified) offers one set of divisions, the table below, and no part keeps a list of its own. **The table is fixed:** an index always names the same row, whatever the time signature, and the menu lists the rows in the table's order. The order is by length in 4/4. A note value's length is in whole notes; a bar value's length is in bars, a bar being the time signature's (RC-7). So in other meters a row's length may tie or cross its neighbour's (in 3/4 a dotted 1/2 equals 1 bar), and neither the index nor the order changes. **Base** rows are always offered. **Triplet** rows (⅔ of their base) are offered when the control's triplet toggle is on, and **dotted** rows (3/2 of their base) when its dotted toggle is on. The two toggles belong to each synced control and are saved with the patch (ADR-327). They filter the menu only, so a stored index, and the sound, never change with them.

    | index | name | kind | length |
    |---|---|---|---|
    | 0 | 1/32 | base | 1/32 whole note |
    | 1 | 1/16T | triplet | 1/24 whole note |
    | 2 | 1/32D | dotted | 3/64 whole note |
    | 3 | 1/16 | base | 1/16 whole note |
    | 4 | 1/8T | triplet | 1/12 whole note |
    | 5 | 1/16D | dotted | 3/32 whole note |
    | 6 | 1/8 | base | 1/8 whole note |
    | 7 | 1/4T | triplet | 1/6 whole note |
    | 8 | 1/8D | dotted | 3/16 whole note |
    | 9 | 1/4 | base | 1/4 whole note |
    | 10 | 1/2T | triplet | 1/3 whole note |
    | 11 | 1/4D | dotted | 3/8 whole note |
    | 12 | 1/2 | base | 1/2 whole note |
    | 13 | 1 bar T | triplet | 2/3 bar |
    | 14 | 1/2D | dotted | 3/4 whole note |
    | 15 | 1 bar | base | 1 bar |
    | 16 | 2 bars T | triplet | 4/3 bar |
    | 17 | 1 bar D | dotted | 3/2 bar |
    | 18 | 2 bars | base | 2 bars |
    | 19 | 4 bars T | triplet | 8/3 bar |
    | 20 | 2 bars D | dotted | 3 bars |
    | 21 | 4 bars | base | 4 bars |
- **Deferred:** the consumers: the arpeggiator and the shared quantizer (in the 1.0 roster, outside the engine), synced glide, a tempo-grid detune law (Q31).
- **Must not require:** the engine to learn the transport by any path but these events; a clock; a look at events that have not yet arrived.

#### RC-8. A readout path for the GUI

- **Provided now.** A **snapshot** the audio thread publishes at the end of every processing call, and within a longer call at the first control-interval boundary after each 1/60 s of audio, which the GUI thread reads without blocking. It holds: per voice and oscillator, R and ψ, the members' phases and levels, each blade's window (position and width) and the frame angle; per voice, its level and its envelopes' stages; the output's clip and non-finite latches (IF-6, RT-3); the meters: each source meter's and the output meter's peak since the previous frame, and their clip latches (IF-6), from which the GUI draws the meters' ballistics and peak holds (RC-2); the operation counters (IF-9).
- **The mechanism:** three frames allocated at prepare (one being written, the latest, one being read), published by an atomic index exchange and a sequence counter. The writer never waits. The reader takes the latest complete frame; if the counter shows the frame changed while it read, it discards that read and keeps the frame it had. A late or skipped frame is normal: the GUI draws the latest frame it has.
- **Requirements.**
  - **RC-8.1** Publishing allocates nothing, takes no lock and never waits (RT-1).
  - **RC-8.2** Reading never changes the audio: a render while a reader thread reads continuously is bit-identical to one without a reader (10.1 item 20, under the sanitizer run of item 8).
  - **RC-8.3** The snapshot is not the observation view: IF-10's view is read between processing calls, complete and exact, for the tests; the snapshot is asynchronous and lossy, for display, and no test of the sound reads it.
- **Deferred:** the GUI, its drawing, and which readouts each view draws.
- **Must not require:** the audio thread to wait for the GUI, to publish more often than stated, or to keep a history; a readout the engine does not already compute.

#### RC-9. Adding parameters later without breaking saved projects

- **Provided now.** The manifest's ids (5.1, IF-11) and the state format (IF-4).
- **Requirements.**
  - **RC-9.1** Manifest ids are append-only: never reused, renumbered or removed; a retired parameter keeps its id and is inert.
  - **RC-9.2** A new parameter's default reproduces the sound every patch had before it existed.
  - **RC-9.3** A state's keys that the build does not know (in `params`, and top-level members) are kept and written back unchanged, after the known ones in the order read; they are never dropped and never change the sound.
  - **RC-9.4** `version` rises only for a breaking change (a key's meaning or law changed), which carries a migration, as B.2 does, from every earlier version.
- **Tests (10.1 item 21):** a version-1 state with an unknown key round-trips byte-identically and renders as without it; a version-1 state saved before a parameter was added (a test build adds one with a neutral default) loads to the same render, bit for bit.
- **Deferred:** the parameters still to come.
- **Must not require:** a new `version` for an added parameter; a changed meaning or law of an existing key without a migration.

#### RC-10. A quality-tier hook

- **Provided now.** One input, the **tier**, an integer from the device (a setting, or a policy outside the engine), never a clock reading or a load measured inside the engine. Tier 0 is the default and the only tier in 1.0; at tier 0 the engine is exactly as this spec states.
- **What a later tier may vary:** a cap on the members per swarm, a longer control interval, the quality (High to Standard), a cheaper edge table. **What no tier may vary:** the voice law, the steal and cull fades, the pedal, determinism within the tier (DT-1, DT-2), the manifest, the state and the parameter values.
- **Requirements.**
  - **RC-10.1** A tier change while notes sound reaches them through PA-5's 5 ms equal-gain crossfade.
  - **RC-10.2** Within a tier the output is deterministic and independent of block size.
  - **RC-10.3** In 1.0 a tier other than 0 is refused and counted (PA-7) (10.1 item 22).
  - **RC-10.4** The CPU targets (PF-1, PF-2) are measured at tier 0.
- **Deferred:** the tiers themselves and any policy that chooses one (1.0 has no automatic thinning, Q27).
- **Must not require:** a tier chosen inside the engine from measured CPU; a voice law that differs between tiers.

#### RC-11. History support

- **Provided now.** What the shell and the GUI need to build undo, redo and snapshot history on the engine. The history itself (the undo stack, its length, compare) lives in the shell and the GUI, not in the engine.
- **Requirements.**
  - **RC-11.1 No hidden user state.** Every audible value a user can change is a parameter in the manifest (each oscillator's `seed` included) or a member of the saved state (the `ratios` text). The morph position (saved with the patch, D.2; round 61, ADR-376), **the corners (their numbers and their `ratios` texts, ADR-327), the morph settings (D.2), each key's mode and count (MO-11), each key's waypoints in their stored order with the waypoint law and its clamp (MO-21), each key's region lock (MO-24), and the patch's own saved corner presets** (round 61, ADR-373: a corner's preset menu lists the patch's own first, then the user's library of corner presets, which every patch shares and no patch's state holds; saving one asks which), the macro values and the intents, **and the modulation system's state (MS-36: routes, modulators, their points, `mod.loops`, seeds; round 62)**, are the morph, macro, intent and modulation systems' own state (RC-4); they reach the engine only as parameter values and, from the morph, the sources' morph levels (MO-26, IF-12), and a snapshot holds the parameter values. Gravity's settled ratios, tails, ramps, stream positions, meters and latches are performance state, not user state: never saved (IF-4) and left as they stand by a restore (RC-11.4).
  - **RC-11.2 Snapshot and restore.** The engine takes a snapshot of its state (every parameter's v and the `ratios` text) **(no morph level: a level is an input, IF-4; the corners, their `ratios` texts included, are snapshotted and undone by the morph system's own history, never by this snapshot)** into a buffer allocated at prepare, and restores one from such a buffer, both deterministic and real-time safe (RT-1): no allocation, lock or system call, so the shell may take one per undo step and restore one during playback. The snapshot converts to and from IF-4's text outside the audio thread.
  - **RC-11.3 Gestures passed through.** The host's parameter gesture begin and end events (CLAP's param gestures) are accepted sample-timed and reported back unchanged in the engine's output events with their parameter ids, so the shell can group one drag into one undo step. They change nothing in the sound.
  - **RC-11.4 A restore is a change, not a load.** Restoring a snapshot applies its values to the sounding voices as one set of parameter changes: each continuous value glides by PA-1, each stepped value crossfades by PA-5, a `unison` change follows PA-6. Unlike a load (IF-4), it re-triggers no note, frees no voice, resets no voice's phases, streams, gravity or ensemble, and restarts nothing beyond what the restored values imply.
- **Tests (10.1 item 23):** a snapshot taken mid-render and restored later reproduces the parameter state exactly (every v bit-identical, the `ratios` text byte-identical); a restore mid-note, to a snapshot that differs in continuous and stepped values, produces no event by QS-4's detector.
- **Deferred:** the undo UI, the history's length, compare (A/B snapshots).
- **Must not require:** the engine to keep a history itself, to store more than one snapshot buffer per request, or to treat a restore as a load.

---

## 8. Sound and quality standards (measurable)

Each is measured by the invariant suite (10.1) on the bank and on generated patches. Where a number below is a target rather than a bound, it says so.

| id | standard |
|---|---|
| QS-1 | **Aliasing, edges.** At Standard quality, 44.1 kHz, one member, base saw with no blade and with a Sync blade (saw carrier, cut 6, width 0.25): alias power below harmonic power by at least **110 dB at C2, 104 dB at C4, 98 dB at C6 and 93 dB at C8**, counted over every non-harmonic bin from 20 Hz to 20 kHz; aliases between 20 kHz and Nyquist are reported, not judged. Method: the note's frequency puts an odd whole number of cycles in a 65,536-sample window, analysed with a **rectangular window** (the whole cycle count makes it leakage-free, so each harmonic sits in its own bin and every other bin holds only alias), in double precision; **the alias ratio is capped at 140 dB** (the float output's floor); a naive saw is the must-fail control (it reads 9 to 27 dB). **The reference design** (ADR-256.1, as amended by the human's ruling of 2026-10-20; a reference, not a mandate): a 32-tap minimum-phase step table from a windowed sinc with cutoff 0.4725·f_s and a Kaiser window of β 8.0, 1024 phases per tap, read with linear interpolation. It meets every floor of this row and QS-2 at once: the plain saw, 114.2, 114.2, 108.7 and 100.9 dB at C2, C4, C6 and C8; the Sync-blade case, with ED-6's corners corrected, 111.6, 111.3, 105.0 and 93.7 dB; QS-2, −2.2 dB (inside its 3 dB, short of the 1 dB target). **The trade around it:** at cutoff 0.47·f_s and β 8 the plain saw has more room (119.4, 115.6, 107.9, 99.0 dB) and QS-2 reads −2.5 dB, but the Sync-blade case misses C8 (91.7 dB); at 0.45·f_s and β 10 every floor holds with room (Sync-blade case: 134.1, 129.4, 121.8, 110.5 dB), but QS-2 reads −6.2 dB, because the C6 saw's 19th harmonic (19.9 kHz) sits just above the cutoff, where any such sinc is near −6 dB; at 0.48·f_s the dense harmonics of a low note fold back below 20 kHz and C2 falls under its floor; no 32-tap Kaiser design found has more than about 0.7 dB of room on both QS-1 and QS-2 at once. (Corrected in round 17: the 0.45 / β 10 design had been stated as measuring −1.5 dB on QS-2; it reads −6.2 dB. Corrected in round 29: the 0.47 / β 8 design had been stated as meeting all four floors; it does so on the plain saw only.) The floors may be raised on measurement. |
| QS-2 | **Aliasing, the top of the band.** At Standard quality and 44.1 kHz, the edge correction keeps 16 to 20 kHz on a single-member saw at C6 within 3 dB of an ideal band-limited saw (target 1 dB), with QS-1 met at the same time. |
| QS-3 | **Aliasing, what the table cannot reach**, measured and reported per bank preset at both qualities by comparing a render with renders at four and eight times the rate (bins classed clean, folding or dynamics): FM, ring and fold with sine carriers at Standard quality alias by design; High quality reduces them. Reported, not judged, until a floor is ratified. |
| QS-4 | **Clicks.** The click detector is the test author's first deliverable, built and ratified before any engine stage, and **calibrated first on the reference renders** (input data for the test author, 2). **Its method and numbers (ratified):** per channel, the render is low-passed at 0.75 × Nyquist (a causal 8th-order Butterworth) and its first difference is predicted; a short-term linear predictor of order **p = 24** is refitted every 1.25 ms on the **10 ms** before each block (the autocorrelation method with a Hann window and a white-noise correction of 10⁻⁵), crossfaded between fits; a pitch predictor (the lag of the residual's autocorrelation peak, from 24 Hz up; a 33-tap fractional delay fitted over 40 ms, refitted every 10 ms) follows one period; the residual is, per sample, the smaller of the two predictors' residuals; a sample's score is its \|residual\| over the residual's RMS in a **20 ms** local window centred on it (its own footprint excluded), that level floored at 60 dB under the analysed signal's peak; a click is a score above **k = 10**, and hits within **1.5 ms** are one event. It is level-relative: a render scaled by any gain reads the same events. Its remaining constants are the suite's, as ratified. **Controls:** must-fire, steps of 0.05 of the render's peak planted at random positions on a sine, on a band-limited saw at C2 and C6 and on Crushed bells at C4, and steps of 0.05 of full scale on Crushed bells, each caught 10 of 10; must-not-fire, held notes with no event (the reference renders' and the bank's) and a plain-saw base at C1 and C2 (ED-1a), no event. A patch whose own sound is made of control-rate steps as large as the plant (Glass horde pad, the Breathing pad, a Crushed bells chord) is beyond a per-sample detector; its steps are judged by BL-16 and by ear. With it: a voice from silence (its onset is the patch's own attack: **reported, not judged**, ADR-239); likewise **reported, not judged** (ADR-241), because the spec's own ideal law reads as an event: a full-range fall to silence of `out.level` or a source's mix (PA-1 through the dB law, a fast fade, the mirror of an onset), an un-mute from silence (IF-6's gate, a tone entering from silence), a mono legato change with no glide (VO-6's instant pitch step is the note change itself), and a full-range jump down of an oscillator's `pitch` (PA-1's smoother: with ED-1's filter E the law reads as an event, engine-free, on every conforming table design; the jump up stays judged; the human's ruling, round 45, ADR-301). **The probe rule** (round 22, ADR-248; it binds every use of the detector in the suite): every judged no-click case is rendered on a pure-sine probe (osc 1 only, one member, a sine base, no blade, every other source off), and a case the probe cannot host adds only what it needs (the blade or source it changes), because on a rich periodic tone this detector cannot tell a correct change of the waveform within a cycle from a click (an engine-free render of a Fold tone fires on a correct PA-1 level move). So a case whose correct sound is such a change is **reported, not judged**, and is judged instead by its own law test, gated, and only these: a quantized cut change on any blade (BL-16's latch timing), a frame crossing (BL-10's frame law, on a patch whose R falls below 0.01), and a stepped-key change or morph flip whose settings change the waveform (PA-5's crossfade law: C.0's 5 ms raised cosine between the two settings' sounds; ADR-276, round 33). If the engine-free law render of any other judged case fires, the case goes to the human for a ruling; the test author never reclassifies a case alone. (Rounds 19 and 21 reported these per mode; round 22 makes it the rule.) **A note's onset inside a case is reported, not judged** (ADR-239): a case that starts a note judges its own event and reports the new note's attack (round 32). A mono re-run (VO-6) is also **reported, not judged** (ADR-274): the re-run replays the patch's own attack, the onset family, and its ideal law reads as an event at the attack's end. Every other case makes no detected click: a stolen voice (VO-3), a culled tail (VO-4, triggered by lowering the cap below the sounding count with no new note, so the cull is judged alone; ADR-274), any other full-range parameter jump (PA-1), any other mute or solo (IF-6) and all-sound-off (VO-13); the swarm's own grain (SW-16 at 1) is measured and reported, not judged. |
| QS-5 | **Headroom.** OU-2: the bank's four-note chords at velocity 0.85 at the default level peak below 0 dBFS, on every preset. |
| QS-6 | **DC.** On every bank preset and on generated patches, the output's mean over any second of a held note is within 0.003·10^(L/20) of zero, L the patch's `out.level` in dB (BL-19). |
| QS-7 | **Silence and denormals.** After the last voice is freed, every output sample is exact zero; across the bank's tails no output sample is subnormal. |
| QS-8 | **Bounded output, no peak guarantee.** Over the whole parameter space (fuzzed, with extreme values and automation sweeps), every output sample is finite and below a peak bound per configuration class, with the bank's own peaks as the anchor; the bounds are proposed by the test author and ratified with the suite. The engine guarantees no peak at or below 0 dBFS: a sample over it is latched and reported (IF-6), and the device's master limiter holds the device's peaks. |
| QS-9 | **Long-run stability.** A 4-hour soak of reference patches keeps level, DC and spectral centroid within bounds proposed by the test author and ratified with the suite. |
| QS-10 | **Mono fold.** The mono sum of each bank preset is reported against its stereo power (the loss in dB). |
| QS-11 | **Aliasing, the sub** (round 26; every number PROPOSED: before any is judged, the test author derives engine-free, from SN-2, ED-1 and QS-1's reference design, whether it is reachable, and a number that is not goes to the human). By QS-1's method (Standard quality, 44.1 kHz, the whole-cycle rectangular window, every non-harmonic bin from 20 Hz to 20 kHz, capped at 140 dB), the sub alone at `octave` 0, `drive` and `crunch` 0, sounding C2, C4, C6 and C8: saw, square, pulse (w 0.5 and 0.1) and triangle meet QS-1's floors (110, 104, 98 and 93 dB), since their edges and corners take the same correction as a base wave's; the smooth square, at `softness` 0 and 0.5, keeps every harmonic above 0.45 of the rate at least 60 dB under the fundamental (SN-2 builds 68 dB by construction from the exact ratios; 60 is the lab's test gate, PROPOSED); sine and bump (at the defaults, and with both harmonics at n 9 and amount 0.6) read at least 130 dB, since none of their harmonics lies above 0.48 of the rate and only numerical error can alias. With the drive (each character at `drive` 0.25, 0.5 and 1, `crunch` 0 and 1, on the saw and the pulse at w 0.1), and under Erode, the aliasing is measured and reported, not judged, until a floor is ratified (QS-3's rule): a curve's harmonics have no bound the 2× oversampling can promise. A naive saw is the must-fail control. |

---

## 9. Performance standards

| id | standard |
|---|---|
| PF-1 | **The CPU target.** The heaviest bank patch (Glass horde pad, translated), 8 voices, Standard quality, 44.1 kHz, 128-sample blocks: about **10.7 % of one Apple M3 performance core** for its one swarm oscillator. The arithmetic: the sources' 34 % of a min-spec core, less about 2 % for the sub, shared by two swarm oscillators, is about 16 % each, and 16 % ÷ an assumed ×1.5 (M3 to min-spec) = 10.7 % of an M3 core. A miss is reported with what would close it, never hidden. |
| PF-2 | **The whole device.** Two swarm oscillators each on the heaviest bank patch, the sub on, and per-voice modulation running on every continuous parameter of both oscillators at the control rate (and on PA-4's destinations at audio rate), 8 voices, Standard: the sources' share is at most 34 % of a min-spec core (the device's envelope is under 50 %: sources 34 %, FX 12 %, modulation 2 %, master 1 %, headroom 1 %). Min-spec is an Apple M1 base or a 4-core 2018-class Intel ultrabook, Windows x64 AVX2; an M3 figure converts by the assumed ×1.5 until measured [Q29]. |
| PF-3 | **How it is measured.** A Release build of the shipped code; 44.1 kHz reference, 48 kHz secondary; 128-sample blocks; the patch's notes (the k-th note on key 48 + 3·(k mod 8), velocity 0.85; round 60, ADR-368: above 8 notes the chord repeats, so every key stays in range) all held before the first block; 0.25 s untimed, then 2 s timed; best of 5 repeats, interleaved across the cells measured; the result is the time per second of audio as a share of one performance core, with a calibration loop (10⁸ dependent multiply-adds) timed in every repeat to confirm the core; a load guard before each repeat (the one-minute load average at most 3.0 and no other process above half a core, else wait 20 s and re-check, for up to 10 minutes, then refuse), never bypassed. CPU is measured and reported, never a pass-fail gate of the suite. |
| PF-4 | **Beyond the bank.** The CPU is also reported on generated patches (combinations the bank lacks), under per-voice modulation, and across a morph between corners with different stepped keys, **at every corner, every edge's midpoint and the centre** (in 1.0 each Structural key has one value there, MO-13; IF-9's structure setting-samples are after 1.0, ADR-377); no patch falls to a path more than 1.5 times slower than the bank's median per member-sample without that being reported by name. |
| PF-5 | **Polyphony.** Up to 64 voices; default 16. Cost grows linearly with the voices sounding and their members; a voice that is not sounding costs nothing (VO-11). The CPU target (PF-1, PF-2) is measured at 8 voices. |
| PF-6 | **Cost when off.** Blade 2, the twins, cross-mod, feedback, interplay, collision, a source that is off, the sub and the noise oscillator when off, the sub's drive at Drive and Crunch 0, the noise's stages and filter when off and its right chain at spread 0 cost nothing measurable (under 1 % of the patch's CPU each), except the noise running silently for Erode (SN-4). |
| PF-7 | **Memory.** Everything is allocated at prepare (RT-1). The worst-case memory per instance, at 64 voices of 16 members, is stated in the manifest and reported. |
| PF-8 | **Latency.** Zero at both qualities: no stage looks ahead, the decimation at High quality included. |
| PF-9 | **Many voices, reported.** The heaviest bank patch is also measured at 16 and at 64 sounding voices and reported, so the cost of a full pool is known; neither is a target. |
| PF-10 | **The ambitious patch, reported** (round 53, ADR-355). What an experimenter's heaviest use costs, and its worst moment, so that they are known. It is not a target and never a gate. Two swarm oscillators and the sub on, as PF-2 sets them, without its per-voice modulation. **Notes:** PF-3's held chord, and a **note stream**: a note-on every 80 ms (about sixteenths at 187 BPM), keys 48 + 3·(i mod 8) for the i-th note, velocity 0.85, each released 60 ms later, running through the untimed span and the timed one, so tails overlap and voices are stolen by the voice law. **Voices:** a limit of 8, and of 16. **Morph:** none, with the heaviest bank patch on both oscillators. The cells with Blend at the pad's centre at counts Two and Four (MO-17), with the bank's four heaviest presets by PF-4 as corners, are after 1.0, with setting sets (ADR-377). **Read:** PF-3's protocol and load guard, giving the average as PF-3 does, and beside it the **worst block**: the longest single 128-sample block of the timed span, as a share of that block's duration, taking the smallest of the 5 repeats' values so that one disturbance doesn't count. FX, the modulation system and Arp's own work are outside this reading; each has its own slice. |
| PF-11 | **The modulation system, reported** (round 62; MD-54, MS-39; T10). Its CPU under PF-3, at 8 and at 16 voices, on a patch with four LFOs (two per note, one of each kind, Random on Cubic with a Spring travel), 64 routes (8 of them on the engine path, MS-41), the Kuramoto LFO at 8 voices, ORBITAL at 8 bodies with the thermostat on, and the 2-D random in Walk: the modulation system's own share, and the engine's added share for the per-note LFOs and the engine path, reported against PF-2's modulation slice (2 % of a min-spec core, about 1.3 % of an M3 core at PF-1's ×1.5), which the morph, the intent bus and the arpeggiator share, and shown beside the suite's count at every stage that builds it. Never a gate. |

---

## 10. Acceptance

The engine is accepted when 10.1 to 10.4 hold and the human says so. Passing is not done: the suite green, the renders judged, the CPU reported.

### 10.1 The invariant suite (deterministic)

Written by a different agent from this spec, before the engine exists. Every check has a must-fail control (a planted fault that turns it red on every run). **The test author receives the reference renders** (a frozen set of renders of the bank, with each render's parameter metadata) as input data; the builder does not. The click detector of QS-4 is the first deliverable, calibrated on the reference renders and ratified before any engine stage is built. The suite must prove at least:

0. every law of Appendix C, checked directly through the observation view (IF-10) where it can be observed (placement offsets, detune frequencies, coupling targets, gravity's ratios and pair moves, the onset offsets, the drift walk's statistics, pan gains, the envelope's shape, the DC estimate, every blade law on a single member), each against its equation within a stated tolerance;
1. non-finite safety, the voice reset and the output guard's latch (RT-3, QS-8);
2. bit-determinism on one build (DT-1) and a note-on never changing a voice already sounding (DT-4, BL-18);
3. block-size independence (DT-2) and sample-rate independence by DT-3's measure (SW-14, EN-4);
4. DC and peak bounds, and headroom on the bank (QS-5, QS-6, OU-2);
5. silence, exact zeros and no subnormals (QS-7, VO-7, OU-4);
6. long-run stability (QS-9);
7. the aliasing floors (QS-1, QS-2), every base wave band-limited (ED-1a), the aliasing report (QS-3), and short tapers (ED-7), **item 7.7**: at ED-7's 1,672 points, QS-1's ratio no lower than min(the engine's own Edges 0, R) − 5 dB; R's body hash pinned and rows regenerated engine-free; ED-7's five controls each red (no correction for a taper; the corners corrected at hard edges only; the step part as the kernel-weighted mean h·(r₂∗k); that form with h read at the taper's start; the correction cut off at T = 16 instead of faded by c(T)); and the rule's own control, planted readings 0.01 dB either side of the 5 dB margin; **item 7.8** (round 42): the same rule on the modes ED-7's measure leaves unjudged (a Sync blade with a sine carrier, FM reset, FM free, Ring and Fold), against R₂, an engine-free reference of ED-7 on those modes at R's 19 pitches and 22 `edges` values, (`engine2/tests/calibration/r2_reference.csv`, 2,090 rows; one member on a saw base, position 0.875, width 0.25, depth 1; cuts 6, 2, 2, 3, 4; fmDepth 2; FM free at modRate 3, its rows from round 56's start, the item settling its settings before the note (round 59, ADR-365); its body's SHA-256, f6ef83fbf4d982fc28d3075ece73b1871d6b1588d89875b209da0aeba99de127, pinned); X = 5 as R's; controls: 2 Gauss–Legendre points per piece, the unsplit 64-point midpoint rule, and the mean-slope corner, each red;
8. real-time safety: no allocation, lock or system call on the audio thread, bounded work (RT-1, RT-2), by a sanitizer run and by inspection rules;
9. clicks, by the detector of QS-4, with its own must-fire control on a bright patch;
10. the claim anchors: coupling's regimes (SW-4), phase lag (SW-5), gravity (SW-11), onset scatter's structure (SW-12), the frame never jumping (BL-10), the quantized cut's timing and no tone bias (BL-16), every spread kind ramping as fed to the blade (BL-17), twin − cancelling even harmonics (BL-6), Crush continuity (BL-7), interplay's parallel identity (BL-11), the negative-spread mirror (BL-14), the cut-rule ratios (BL-15), the steal fade's rules (VO-3), the voice law at 1, 16 and 64 voices (VO-2, VO-5), the pedal (VO-12), all-notes-off and all-sound-off (VO-13), the start table of VO-6, the sub's shapes and its classic pulse (SN-2), the mono sub's choice of note, its pitch and its starts (SN-7);
11. live parameters move the output and inert ones do not, by PA-8's table, for every manifest row;
12. host values checked on entry (PA-7);
13. state round trip, reset to fresh, and migration from the version-0 format (IF-4, DT-6, OU-3);
14. the zipper measure: a host staircase of 128-sample steps against a per-sample ramp, high-passed at 250 Hz, on every continuous parameter (PA-1, PA-2, PA-3). **Per-voice offsets (PA-2) are judged by PA-3's law, their zipper reported, not judged** (round 39, ADR-285): a per-sample read of a view value under a staircase of offsets must follow PA-3's tick-and-ramp law within the suite's tolerance, and a build with no ramp is its must-fail control. PA-3's own law, rendered engine-free on this measure, cannot meet a zipper bound fit for PA-1, and the measure cannot tell its ramp from none (B-50);
15. the WebAssembly build renders the same sound as the native build: the difference of the two renders, on the held-note and chord phrases (2 s each), at 44.1 and 48 kHz, on both channels, is at least 90 dB below the render's RMS (IF-10); the bound allows the two toolchains' different math libraries and operation orders (BD-7) and is ratified with the suite. In two parts (round 34, ADR-280): **15a**, at stage 8, on states that need no bank translator: the version-0 example states (through B.2's load), the default state, and version-1 fixtures written from the manifest alone that reach what version 0 cannot (every blade mode, the sub's drive and Crunch in both orders, Erode, the noise's sources and stages, the mono sub, ENV 2 to ENV 4); **15b**, at the bank stage, on every bank preset, through the bank translator; where a preset routes B.1's blade envelopes, both builds' renders take those routes as data beside the state and apply them through PA-2, as the render harness does; **A chaotic state is reported, not judged** (round 35, ADR-281): a state whose law, rendered engine-free twice, once as written and once with a one-ulp change to one member's natural frequency or with every update by fused multiply-add, departs from itself by more than −90 dB of its RMS within the 2 s phrase, as a strongly coupled swarm across a wide spread does. No correct pair of builds under BD-7 can match on it. Which states are chaotic is decided by that engine-free render only, never by the engine, and the list is kept with the suite. **A knife-edged state is reported, not judged, likewise** (round 41, ADR-287): a state with a sample-and-hold noise modulator whose rate is set in Hz, where that rate and the host rate, as exact numbers, stand in a ratio p/q in lowest terms with q no more than the phrase's length in samples. Its phase then reaches a whole number exactly on a sample, so the sample on which the held value steps turns on the phase's last bit, which BD-7 leaves free. The rule is structural, read off the state and the rate alone, and the bank's knife-edged states are listed with the chaotic ones;
16. each of the 12 envelope presets (Appendix A), translated by Appendix B, plays the envelope shape it is named for: on a 2 s note its ENV 1 level (IF-10) follows C.12 for the translated values within 10⁻⁴ at every sample, and the shape's defining feature holds: pluck, below −60 dB before the key-up; pad and swell, an attack of at least 0.5 s; gate and the swell cut at the key-off, below −60 dB within 30 ms of the key-up; stab, its sustain reached by the end of the decay and held; tail, audible (−60 dB or above) 4 s after the key-up; steal, under a full pool new notes take the quietest tails first (VO-2) with no detected click, the steal's fade judged and the new note's onset, from the fade's end over at least 10 ms, reported, not judged, as in 9.3 (QS-4's onset rule; round 40, ADR-286: the envelope's own attack corner on a sine reads as an event, engine-free);
17. a voice that is not sounding costs nothing, by the operation counters (VO-11);
18. a fifth source added through the slot by a test build leaves the four sources' renders bit-identical, and costs nothing while off (RC-6.3, RC-6.4);
19. transport events at any sample times and block sizes leave the render bit-identical **with `morph.stepSync` off** (RC-7.1, RC-7.3), **and with it on, synced steps fall on the division boundaries at any block size (MO-10)**;
20. a reader thread reading the GUI snapshot continuously leaves the render bit-identical, with no allocation, lock or wait on the audio thread (RC-8.1, RC-8.2);
21. a state with an unknown key round-trips byte-identically and renders as without it; a state saved before a parameter was added loads to the same render (RC-9.2, RC-9.3);
22. a tier other than 0 is refused and counted (RC-10.3);
23. a snapshot taken mid-render and restored reproduces the parameter state exactly, and a restore mid-note makes no detected click (RC-11.2, RC-11.4);
24. **the bump** (SN-2): at a steady pitch, for every pair of harmonic orders and amounts and phases on a grid, the observation view's peak equals the true maximum of SN-2's numerator computed engine-free by dense evaluation, and the sub's uncorrected shape (the observation view's sub y⁰, before ED-1's filter; round 42) peaks at 1 within a tolerance the test author derives from the sample grid; each extra harmonic's level relative to the fundamental's, by DFT at whole-cycle frequencies of that shape, is a·F(n), following F(n) for n·f from 0.38 to 0.50 of the rate and is silent above 0.48. Must-fail controls: a bump normalised by its unfaded peak (it reads below 1 inside a fade); a fade with its corners swapped. **The smooth square** (read on the sub's y⁰): its peak is exactly 1 and its mean 0 at every softness and pitch; k is k_s where m ≥ 2048 and k_max(m) is lowered by SN-2's rule, checked engine-free against the exact Fourier series; at C8 and softness 0 every harmonic above 0.45 of the rate is at least 60 dB under the fundamental. Must-fail controls: k never lowered (the C8 harmonic above 0.45·f_h reads above the bound); a normalisation by tanh of the unlowered k (the peak is not 1);
25. **the sub's DC after the drive** (SN-6): in both orders, for every wave and character, `drive` on a grid from 0 to 1, `crunch` 0, pulse widths from 0.05 to 0.95, at C1, C3 and C5, the observation view's M equals SN-6's integral computed engine-free, within a tolerance the test author derives; QS-6 holds in both orders, judged, `crunch` on a grid from 0 to 1 included; the output's mean over its first 50 ms and its content below 20 Hz over the first 200 ms (the thump) are under bounds the test author derives from SN-6's method; and a mid-note `order` change makes no QS-4 event (on the probe rule), on PA-5's fresh start, at two pitches of the sub (the human's ruling, round 44, ADR-299), each judged at the item's own change time and with the change moved across one cycle of the sub in 24 equal steps: **at octave −1** (65.41 Hz, the default), with Fold-lite at every `drive`, with Tube up to `drive` 0.80 at both, and with Diode up to 0.50 at the item's time and up to 0.40 across the cycle; **at octave 0** (130.81 Hz), with Fold-lite at every `drive`, with Tube up to 0.80 at the item's time and up to 0.60 across the cycle, and with Diode up to 0.35 at the item's time and up to 0.25 across the cycle. Every drive above those is reported, not judged: there PA-5's own crossfade of the chain reads as an event by the law, engine-free, at the item's time or at some position across the cycle (on all four conforming table designs, at the change's own sample and on PA-3's engine clock; ADR-288, ADR-289, ADR-298, ADR-299; round 43's bounds had been derived at octave −1 only). A held, driven, edged sub (saw, square, pulse) reads as events by its own law, engine-free, so no judged no-click case sounds one (round 42 check b). Must-fail controls: the pulse's mean taken out before the drive (2w − 1, the design the human rejected, which leaves the drive's own mean); the blocker alone with no subtraction;
26. the sub's aliasing floors and the drive's report (QS-11);
27. **the noise's spread** (SN-12): at `spread` 0 the two channels are bit-identical, and the left equals a render with the right chain removed; for s from 0.1 to 1 (Clean, colour +1, no stage, filter Off, 10 s) the channels' correlation coefficient is 1 − s, and the mono sum's level is the same at every s, each within a bound the test author derives from the sample count. Must-fail controls: the right channel drawn from the left's own stream (correlation 1 at every s); the mix without its √(1 − (1 − s)²) term (the right channel's level falls);
28. **the noise's Pitch** (SN-11): with the LFSR at 15 steps, the output repeats with the period of 15 clock steps, its fundamental at f_n·r, for Pitch on a grid from −48 to +48 where f_n·r·M < f_h (below the LFSR clock's cap); the Resonator's fundamental is f_r; the filter's cutoff moves by r^keytrack; with no LFSR, no Resonator and keytrack 0, renders are bit-identical at every Pitch. With `lfsrKeytrack` off the LFSR's fundamental is 130.81 Hz·r on every note, while the Resonator and the filter still follow the note. Must-fail controls: Pitch applied to S&H's rate; keytrack off pinning the Resonator too;
29. **the noise's sources and stages** (SN-8, SN-9): every LFSR length's period is exactly 2^b − 1 steps; Crackle's pops arrive at `density` per second; each stage's level matching holds the chain's long-term RMS to the source's, each within a bound the test author derives engine-free. Must-fail control: a tap pair that is not maximal;
30. **the reservations** (SN-13, RC-6.7): a state with `noise.mode`, the pitch-mode keys and the four `stream` rows absent loads to the same render, bit for bit, as with them present at their defaults, and its re-save then round-trips byte-identically; a state carrying the sampler's four keys, `noise.mode` at the sampler's v and a `samples` member round-trips byte-identically and plays the noise as 1.0 defines it; no reserved row (5.1) is a host parameter. a note-on carrying a stream tag (`direct`, `chord` or `arp`) renders bit-identically to one without (IF-1). Must-fail controls: a build that drops an unknown member (RC-9.3); one that exposes a reserved row to the host; one that silences a source on an `arp`-tagged note;
31. **the envelope selector** (RC-6.5, EN-5): for each source, ENV k holding ENV 1's values renders bit-identically to ENV 1; with its own values the source's level (for the sub, at `drive` 0) follows ENV k by C.12 within 10⁻⁴ at every sample (item 16's tolerance), read through IF-10; a legato change continues every envelope's stage; a voice whose source follows an ENV 2 with a longer release than ENV 1's sounds until ENV 2 ends (6.4); a selector change mid-note makes no detected click (PA-5, on QS-4's probe rule) with the two envelopes' levels within 0.1 at the change, other changes reported (ADR-241: a crossfade between very different levels is the law's own sound); the mono sub's own envelope follows its starts (SN-7). Must-fail controls: a voice freed at ENV 1's end (it cuts ENV 2's tail); an ENV 2 that restarts on a legato change;
32. **the source laws, each against its equation** through the observation view or on a single source: SN-5's three curves S, the make-up k and Crunch's path; Whisper's formant frequencies and its N; Wind's centre walk and gain; the noise filter's LP, BP and HP responses (SN-10); each within a tolerance the test author derives engine-free where the law does not give one. Must-fail controls: Tube's offset 0.3 dropped (S becomes odd, and M on the square reads 0); k computed without removing the mean; a formant table with two vowels swapped; Wind's gain without its 1/√ correction (the level follows the centre); BP without its factor k (its peak is not unity);
33. **Erode** (SN-14): at `erode` 0 the sub is bit-identical to a build without Erode; the sidebands scale with the sub's level (the sub 30 dB lower gives sidebands 30 dB lower); Erode is a time delay, not a phase offset (on a pure sine sub, the sideband power rises 6 dB per octave of the sub's pitch, while 2π·f·τ_RMS ≪ 1); a noise at a true off and at +6 dB erode identically; the sub's mean frequency over 10 s is its uneroded frequency; a per-note chord equals the sum of its notes played alone, each eroded by its own voice's noise; the mono sub is eroded by the noise of the chosen note's voice and crossfades it over 5 ms at a hand-off. Every tolerance is the test author's to derive. Must-fail controls: the noise added to the output instead of bending the time (its level does not follow the sub's); a fixed phase offset in cycles (the sidebands do not rise with the pitch); the noise read after its level; a frequency modulation, integrated (the mean frequency drifts); one noise shared by every voice; the mono sub reading the newest voice's noise; a hand-off with no crossfade (a detected click, on a sine sub eroded by noise low-passed at 100 Hz or below, so the crossfade's absence is the only step).
34. **The morph resolver** (Appendix D, RC-7.4), on its own calls (IF-12). Items 34.6 to 34.11 and 34.15 run at
`morph.glide` 0; 34.12 tests the glide.
   - **34.1 Weights (MO-4).** At 10,000 seeded positions the weights equal the bilinear formula within 10⁻¹² and sum to 1.
     **Must fail:** a transposed corner order (B and C swapped).
   - **34.2 The draw's independence (MO-6).** Picks for every key at 27 positions are identical with and without 200 ids
     appended to the manifest, and with the corners' entries written in reverse order. **Must fail:** a draw taken from one
     stream in list order.
   - **34.3 The draw and the pick, exactly (MO-6, MO-7).** At the position (0.35, 0.6), with T = 1 and cohesion 0.3, the
     keys `osc1.blade1.mode`, `osc1.baseWave`, `osc1.unison`, `osc2.blade2.wave`, `sub.wave`, `noise.type`, `env1.attack`
     and `osc1.coupling` pick, in that order:
     - at seed 1: C C D B A C D D;
     - at seed 1024: C C C B A C C C;
     - at seed 9999: C B C A C D D C.

     The first draw of `morph/shared` at seed 1024 is u = 0.5761699904687703 (its FNV-1a state 0x5EE6354F). **Must fail:**
     a stream that hashes the seed before the purpose's bytes, or that uses an arithmetic shift.
   - **34.4 Temperature and cohesion (MO-7).** At T = 0.02, every key picks the heaviest corner wherever that corner's
     weight is at least 1.9 times the next. This holds for every seed: ln 1.9 / 0.02 ≈ 32 exceeds the largest gap of two
     clamped Gumbel draws, ≈ 23.8. At cohesion 1, every key picks the same corner at every position. **Must fail:** a pick
     without the temperature term.
   - **34.5 The latch (MO-8).** A Structural key dithered across a position where two corners' scores tie, with their
     difference staying within ±m, flips at most once. Carried until the difference exceeds m, it flips. On an edge with
     equal draw terms the band is 0.45 to 0.55. A Morphable key under Quantum follows its pick at every crossing.
     **Must fail:** a Structural key with no latch.
   - **34.6 Blend at count 4 (MO-9).** A Morphable value equals Σ w·v within 10⁻¹². A circular key equals the unit-vector
     mean within 10⁻¹²; at a zero resultant (corners 0 and 0.5 at equal weight) it equals the heaviest corner's value (the
     lower corner on a tie). **Must fail:** a linear sum on a circular key, which fails at the wrap; a sum in the law's
     unit instead of v.
   - **34.7 Count 2 on a Morphable key (MO-9, MO-11).** A key set to Two emits MO-9's two-value blend of its pooled
     distinct corner values, at the unglided weights. It is continuous away from the point where the top three tie and jumps through it.
     **Must fail:** a renormalised top-two law, which jumps on the diagonals.
   - **34.8 Stepped (MO-10, RC-7.4).**
     - The output is constant between steps and equals Blend at the stepped position.
     - φ follows its closed form: steps fall where ⌊φ⌋ changes, at 3 steps per second at 44.1 kHz (every 14,700 samples)
       and at 7 per second (fractional), with no drift over 10⁶ samples.
     - The first held position is the one at reset. A rate change mid-run moves no step already passed.
     - Synced, steps fall on the division boundaries of a scripted transport. A loop's backward jump that changes ⌊φ⌋ is a
       step. While the transport is stopped, steps keep falling at the tempo's division length.
     - Quantum-mode keys and every Structural key read the stepped position.

     **Must fail:** steps counted by a rounded period, which drifts; a Structural key that reads the live position.
   - **34.9 Modes and counts (MO-11 to MO-13).**
     - An Exempt key emits corner A's value at every position and under every law, Structural keys included.
     - A Quantum key under Blend emits its pick. Every Structural key, Follow or Quantum, emits its pick under Blend and
       Stepped and arrives as a `Param` event (MO-13, ADR-377), except the on switches (MO-26).
     - A Follow key under Quantum flips.
     - A Morphable key's Two acts on that key alone; a Structural key's Count changes nothing (MO-11).

     **Must fail:** a mode applied per group; a Structural key that blends under Blend.
   - **34.10 Groups and sets (MO-14, MO-17, MO-19): after 1.0 (ADR-377), not judged for 1.0.** At 2,000 seeded positions and corner layouts:
     - candidates pool identical settings, and an id is the lowest corner holding its setting;
     - at n = 4 the set is every candidate with weight above 0, at its pooled weight;
     - at n = 2 the set has at most two settings, with MO-17's weights and tie-break;
     - the weights sum to 1 within 10⁻¹², and flip-only keys are never in a set;
     - `oscN.ratios` is in `oscN/swarm`, compared as text, and `sub.env` is in `sub/chain`;
     - an `fx/N` set has n = 2 whatever the choice;
     - `oscN/swarm` carries the kernel's keys exactly when the four corners differ in the swarm's keys. On the A–B edge
       (y = 0) of a patch whose A and B agree on the swarm's keys and whose C and D differ from them, the swarm set lists
       the kernel's keys.

     **Must fail:** a merge decided at the position, which lists none on that edge; an equal-power set, which fails the sum.
   - **34.11 Continuity at n = 2 (MO-18): after 1.0 (ADR-377), not judged for 1.0.** With four distinct settings, on an 801 × 801 grid, a setting outside the set
     counts as weight 0. No two neighbouring points farther than 0.1 from the centre differ in any weight by more than
     0.02, and the only jump is at the triple tie. **Must fail:** the renormalised top-two law, which jumps on the
     diagonals.
   - **34.12 The glide (MO-5).** After a step of the position from A to D, with n₀ the first control boundary at or after
     the step (MO-3), at every control boundary n before
     the snap, each Morphable value equals target − (target − start)·e^(−(n − n₀)/(f_host·`morph.glide`))
     within 10⁻⁹. This holds at 44.1, 48 and 96 kHz, at block sizes 1, 64 and 4,096 alike. Quantum's picks read the
     unglided weights. The weights' own glide (MO-5 (a)) is after 1.0 (round 61, the lead (critic S5)). **Must fail:** a coefficient computed
     per block, which fails at uneven blocks.
   - **34.13 Device keys (MO-2).** Over every Device row, at 1,000 positions, under every law and mode, the resolver emits
     no event for a Device key. **Must fail:** a resolver that blends `quality`.
   - **34.14 Block size and transport (MO-3, RC-7.3).** A scripted session gives the same events at the same samples at
     block sizes 1 to 4,096. The session holds a moving position, changes of settings and modes, transport events with a
     loop's jump, and corner edits. With `morph.stepSync` off, it gives the same events with and without the transport
     events. **Must fail:** resolving once per block.
   - **34.15 The events (MO-20; restated for 1.0 by ADR-377).**
     - Every key arrives as a `Param` event, and no `SettingSet` event is sent.
     - When `morph.on` turns off, with the position near D and `morph.audible` A, the resolver emits corner A's values for
       every key and a morph level of 1 for each source, and then nothing. Changing `morph.audible` to C while off
       emits corner C's the same way, its Morphable values gliding by MO-5 (b).
     - `oscN.ratios` arrives as the `ratios` input.

     **Must fail:** a resolver that sends a `SettingSet` event; a resolver that leaves a source's level below 1 at
     morph off.

     The set ordering this item judged before ADR-377 (sets before `Param` events, MO-20's fixed order of groups, the merge
     and un-merge orders, a group's last one-setting set) is after 1.0, with D.5.
   - **34.16 Reset (MO-3).** After reset the events equal a freshly prepared resolver's for the same inputs. **Must fail:**
     a latch kept across reset.
   - **34.17 The standard divisions (RC-7.4).**
     - Every synced control's options are RC-7.4's table: 22 rows, each with its index, name, kind and length, in the
       table's order.
     - The triplet and dotted toggles change only which rows are offered.
     - A stored index keeps its row in 4/4, 2/4, 3/4 and 2/8, and its render is bit-identical whatever the toggles. In 3/4,
       index 14 (1/2D) and index 15 (1 bar) are both 3/4 of a whole note, and they keep their order.
     - `morph.stepDivision` 3 steps every 1/16 note.

     **Must fail:** an option list sorted by length in the current meter, which swaps rows in 2/8; an option list rebuilt
     by the toggles, which moves a stored index.
   - **34.18 Corner exactness (MO-27).** At each of the four corners, under each law and at counts Two and Four, on a patch
     whose corners differ in every class of key, with a circular key at values that do not survive an atan2 round trip, a
     key with waypoints, a locked key and an Exempt key, and with osc 1, osc 2, the sub and the noise each on in some
     corners and off in others: once the glides settle, the values the resolver sends, and each source's level where its
     switch is on there (its switch where it is off), equal, bit for bit, those it sends with the morph off and
     `morph.audible` at that corner. **Must fail:** a circular key computed through atan2
     at the corner; a level computed as W_on over a sum of weights that lands an ulp below 1.
   - **34.19 A source's on switch (MO-26).** On a patch with osc 2, the sub and the noise each on in some corners and off
     in others, whose keys differ between those corners:
     - **The fade.** Under Blend, along a sweep from an off corner to an on corner, each source's level equals W_on at
       every boundary, its switch reads on exactly where W_on is above 10⁻³, and it never flips mid-blend.
     - **The wait.** With W_on dropped to 0 in one step, the switch's off `Param` arrives at the first boundary at which
       the resolver's mirror ĝ, computed from PA-1's law, is at or below 10⁻³. That boundary is the same at block sizes
       1, 64 and 4,096 and at 44.1, 48 and 96 kHz. The source's keys keep their last values until then.
     - **The restriction.** While the source is partly on, every Morphable key equals the on-corner blend, every
       Structural key's pick is an on corner, and no value of an off corner is emitted. Where no weighted corner has it
       off, the values are bit-identical to MO-9's.
     - **Landing.** When a source comes back on, under Blend and under Quantum, its keys' `Param` events are marked
       landing, come before the switch's on `Param` at that boundary, and carry the new values with no MO-5 (b) glide.
     - **Quantum.** Under Quantum, and for a Quantum-mode switch under Blend, the switch flips at its pick and the level
       is 1.
     - **The noise.** With the noise off in every corner and `sub.erode` above 0 in corners B and D only, the noise's
       keys range over B and D and ignore A and C, and its level stays 0. With `sub.erode` 0 everywhere, they take the
       plain blend.

     **Must fail:** a switch-off sent when W_on first crosses the floor, before ĝ does; a mirror stepped per block,
     which moves the off boundary with the block size; a plain four-corner blend while the source is partly on; a
     landing value glided by MO-5 (b); the noise's keys restricted to its on corners alone.
   - **34.20 Waypoints, locks and Save (MO-21 to MO-25).**
     - **MO-21.** A ninth waypoint on a key, a 65th in the patch, one nearer than 0.04 to a corner or another waypoint,
       and one with a non-finite value are each refused whole, and the key keeps its previous list. A value outside
       [0, 1] is clamped first.
     - **MO-22.** Under TPS·Δ and under TIN·Δ: with no waypoints the value is MO-9's, bit for bit; the value passes
       through every waypoint within 10⁻⁹; at the four corners it equals the corner values exactly; with Clamp on it
       never leaves the hull. On a seeded fixture whose waypoints include co-circular ties, TIN·Δ's triangulation equals
       the one MO-22 (4)'s tests give in stored order.
     - **MO-23.** Waypoints act only under Blend, mode Follow and count Four, and are dormant under Stepped, Quantum,
       Exempt and count Two. Back in Blend they act again, unchanged.
     - **MO-24.** A locked key's picks don't move when the seed, Temperature or Cohesion change. Locking as is changes
       no pick anywhere on a 201 × 201 grid. A corner edit moves no region and changes the value the edited corner's
       region emits. An offset beyond 10⁶ is refused.
     - **MO-25.** The resolved values the resolver supplies equal what it last emitted (glided).

     **Must fail:** a waypoint law that recomputes corners, which drifts there by rounding; a triangulation that
     ignores the stored order, which differs on the tie fixture; a lock that keeps reading the temperature.
   - **34.21 A landing in the engine (PA-1, PA-5, PA-6, IF-12).** With a source off and sounding nothing, a boundary's
     `Param` events marked landing (a continuous key, a stepped key, the sub's `order` and `unison`), followed by the
     source's switch-on: from that sample each value is the new one, with no PA-1 glide, no PA-5 crossfade and no PA-6
     fade, and the only crossfade is the switch's own. **Must fail:** the same events without the landing mark, which
     glide, crossfade and fade.
35. **The structure blend** (D.6, SB-1 to SB-13; IF-12's event; PA-7's refusals), given `SettingSet` and `Param`
events. **After 1.0 (ADR-377): not built and not judged for 1.0.** Its sub-items stay as written for when setting sets
return.
   - **35.1 Corner exactness (SB-10).** A one-setting set renders bit-identically to the patch with the morph off and those
     values stored. **Must fail:** a planted weight of 10⁻³ on another setting.
   - **35.2 Weight 0 costs nothing (SB-5, IF-9).** With every set holding one setting, the counters equal the no-morph
     patch's. A setting at weight 0 adds zero setting-samples, and by-gain groups never add any. **Must fail:** a build
     that renders every listed entry.
   - **35.3 The mix law (SB-1, SB-2).** At a static set with settled weights, on a patch with no feedback, no cross-mod and
     an empty filter slot, the output equals Σ wₛ·(the render of setting s alone, with the same values) within 10⁻⁶ of its
     RMS, for 2, 3 and 4 settings. **Must fail:** an equal-power mix.
   - **35.4 By gain (SB-1 item 1).** A set on `oscN/env`, `noise/env` or `oscN/pan` renders one setting (by the counters)
     and meets 35.3's law. **Must fail:** a two-render build, which reads double.
   - **35.5 Weights glide and sum to 1 (SB-2).** Heard weights sum to 1 within 10⁻⁵ at every sample of a moving set, and a
     weight jump follows PA-1's curve. **Must fail:** weights applied as steps.
   - **35.6 No click across a blend (QS-4).** A 1 s sweep of a two-setting set from one setting to the other makes no
     detected click. It runs on the probe rule's patch: a sine probe plus the one blade whose mode differs. **Must fail:**
     a planted bare switch at the midpoint is detected.
   - **35.7 Waking (SB-6).** A setting that enters mid-note equals, from its entry, the same setting started fresh at that
     sample. The same render at block sizes 1, 64 and 4,096 is bit-identical. **Must fail:** a wake that resumes stale state.
   - **35.8 Slot independence and blade noise (SB-6).** A setting rendered in either slot, or alone, gives bit-identical
     samples from the same start. Two kernel settings in the Noise mode draw from separate copies of the blade-noise
     streams: each equals its render alone. **Must fail:** streams keyed by slot; one stream shared by the settings.
   - **35.9 A voice that starts in a blend (SB-7).** With no feedback and no cross-mod, a note started at a static set
     equals Σ wₛ·(the note started in each setting alone), from its first sample. **Must fail:** a voice that starts its
     heaviest setting only.
   - **35.10 Legato in a blend (SB-7).** A legato change during a two-setting set keeps both settings' phases and states
     continuous: the output equals a render in which only the pitch moved. **Must fail:** restarting settings on a legato
     change.
   - **35.11 A flip inside a blend (SB-8).**
     - A one-value `mode` change in a two-setting kernel group crossfades both settings by PA-5's law (QS-4's law test),
       with at most four settings rendering.
     - An `octave` change steps every setting's frequency at once, with phases continuous.
     - A `unison` change follows PA-6 in every instance.
     - A second change waits.

     **Must fail:** flipping one setting only.
   - **35.12 Limits (SB-9).** Never more than four settings per group, two in a count-2 group, and twice those during SB-8,
     by the counters and the observation view. **Must fail:** a planted build that admits a third setting into a count-2
     group before a slot frees.
   - **35.13 The slot rule (SB-11).** Scripted sequences:
     - {A, B} → {C, D};
     - {A: 1} → {D: 1}, MO-18's point;
     - a corner edit replacing one setting's values while four sound;
     - count 4 → 2 while four sound;
     - **the relabel case:** A and B hold X (id 0) and C and D hold Y. Edit A to Z, so X's id becomes 1. X keeps its slot
       with no restart, and Z enters new;
     - a key leaving the key list (Follow to Quantum) while two settings sound: its `Param` applies to both, and neither
       re-enters;
     - a key joining the list (Quantum to Follow): a setting whose `Param`-given value equals the new entry's matches it,
       and does not re-enter;
     - **a merge while the kernel blends:** a corner edit makes `osc1/swarm` differ while `osc1/kernel` holds two
       settings. The kernel's one-setting set, then the merged swarm set, are both accepted. Every old setting of both
       groups fades by PA-1 while the new instances enter by the slot rule, with the limits held;
     - two `ratios` texts that parse alike (`1, 5/4` and `1,5/4`) are one setting.

     Throughout: at most the allowed settings sound at every sample; heard weights sum to 1 within 10⁻⁵; slots free only
     below 10⁻⁴; the output steps by no more than 10⁻⁴ of the settings' difference; and 35.6's detector reads no click on
     the probe patch. **Must fail:** a build that matches by id, which restarts X in the relabel case; a build that frees
     a slot without passing its residual, which breaks the sum; a build that matches across a merge.
   - **35.14 The kernel's shared state (SB-3).** With feedback on, in a two-setting kernel group, the mixed y⁰ that the loop
     reads (IF-10) equals Σ wₛ·y⁰ₛ of the settings' y⁰ within 10⁻⁶ at every sample. **Must fail:** feeding back the
     heaviest setting's output alone.
   - **35.15 Swarm instances (SB-4).**
     - Each instance's start phases and drift equal those of its setting rendered alone.
     - The noise's streams follow the seed of osc 1's heaviest setting at the fresh start, and keep it when the weights
       cross.
     - Two settings with the same seed and different `unison` keep separate ensemble states, each equal to its setting's
       alone.
     - Two settings that differ only in `osc1.ratios` render whole instances, each with its own text.
     - Published R, ψ and the onset level equal the mixture's within 10⁻⁶.
     - An instance's feedback reads its own output.

     **Must fail:** streams shared across instances; ensemble state keyed by seed alone; a published value taken from one
     instance.
   - **35.16 Sets as inputs (IF-4, IF-12, PA-7, RC-11).**
     - The state and the snapshot hold the heaviest setting's values, `ratios` included, and no set, and no corner's texts:
       the state's text has exactly IF-4's members.
     - Handing the same corners (their texts included) back to the resolver after a load (IF-12) renders bit-identically.
       The corners' own save and undo are the morph system's, judged with the shell's acceptance (RC-11.1).
     - A `Param` event naming one of the morph's host ids is refused as an id that names no engine parameter.
     - A save and load mid-blend, with the sets resent at the same samples, renders bit-identically.
     - Each PA-7 case is refused whole and counted, among them an `fx/N` set, an `oscN/kernel` set while merged, and a bad
       `ratios` text.
     - A `Param` for a key in the latest multi-setting set is refused and counted; one that follows a set without that key
       is accepted.
     - No value the morph sends is reported to the host as a parameter change.

     **Must fail:** accepting an oversize set; storing a set, or a corner's text, in the state; refusing on slots still
     fading; a "live" kernel judged by slots still fading, which refuses the merged swarm set.
   - **35.17 Determinism (DT-1, DT-2).** A moving set at block sizes 1 to 4,096 is bit-identical.
   - **35.18 A voice's life while an envelope selector blends (SB-13).** With `osc1/env` blending ENV 1 (release 0.1 s) and
     ENV 2 (release 2 s), the voice is freed only after ENV 2 has finished while ENV 2's setting has weight above 0, and
     after ENV 1 has finished once ENV 2's weight has reached 0. **Must fail:** a build that governs by ENV 1 alone, which
     cuts ENV 2's tail.

36. **The modulation system's matrix** (Appendix E, E.0 to E.4; IF-13; round 62), on its own calls, with the engine
where a test hears the result. Tolerances are reachable in single precision (BD-7); every number below not stated in
Appendix E is the test author's. Durations are stated per item.
   - **36.1 No routes, no change (MS-1, MS-41, MS-43).** A patch with no routes and no per-note LFO, every global
     modulator running, renders bit-identically for 10 s with and without the modulation system attached.
     **Must fail:** a planted offset of 10⁻⁶ on one key.
   - **36.2 Polarity and curve (MS-14, MS-15).** For every polarity and curve, at s ∈ {0, ¼, ½, ¾, 1} and d ∈ {0, 0.25,
     1}, the contribution equals its closed form within 10⁻⁶; a bipolar route at s = ½ contributes exactly 0 on every
     curve. **Must fail:** a curve applied to 2s − 1 directly, which moves a bipolar source's rest.
   - **36.3 Summing and the wall (MS-20).** Opposing routes cancel; the clamp is once, after the sum; a macro pushed past a
     knob's end leaves an LFO's dip audible in full. **Must fail:** a clamp per route.
   - **36.4 Control (MS-15, MS-20, PA-2's Control input).** Under a Control route of weight 1, a jump of the morph's
     resolved value b by 0.5 moves the heard value by less than 10⁻⁶ at every sample of the following 50 ms; removing the
     route restores the knob's value; unipolar routes add on top. **Must fail:** a morph jump under Control moves the value
     (Control as an offset against the unsmoothed b).
   - **36.5 Depth wires (MS-17).** A depth route that would take a depth below 0 stops it at 0, and the route never
     inverts; a route onto a depth route is refused. **Must fail:** a depth that passes zero.
   - **36.6 Fading with the corner (MS-24, MS-25).** At `morph.glide` 0, under Blend, a route only corner B holds
     contributes x·(1 − y)·m(s) at (x, y) within 10⁻⁶; opposite polarities in A and B cancel at x = ½, y = 0 and invert
     past it; under Quantum the contribution follows the resolver's pick for the destination; at `morph.glide` 0.1 s a
     step of the target reaches 1 − 1/e of the way in 0.1 s ± one sample; with the morph off it is corner
     `morph.audible`'s. **Must fail:** a route weight taken from the heaviest corner; route weights glided from MO-5's
     weights instead of their own one-pole.
   - **36.7 A source's on corners (MS-25; ADR-379 r1; MO-26).** With osc 2 on in A and B and off in C and D, and a route
     into `osc2.fine` held only by C, osc 2's `fine` takes nothing from it at every position, under Blend and Quantum;
     with the route in A and C, its weight is A's on-corner weight w[A]/(w[A] + w[B]); at osc 2's landing the route's
     weight takes its target at once. For the noise, a route held by an erosion corner counts. **Must fail:** plain
     corner weights on a source's keys.
   - **36.8 Exempt (MS-28).** A route on an Exempt key acts as on a Follow key, with corner A's routes at every position.
     **Must fail:** an Exempt key that drops its routes; one that blends the corners' routes.
   - **36.9 The morph position (MS-29).** A global LFO's route and a 2-D random's route to `morph.x` move the resolver's
     weights one control interval late, and `morph.x` as a host parameter keeps its value; a route to it from a per-note
     LFO, an envelope, the Kuramoto LFO, ORBITAL, the wheel or a macro is refused. Save current sound under such a route
     stores the unglided target at `morph.x`, `morph.y` (MS-29). **Must fail:** the Kuramoto LFO accepted.
   - **36.10 The sources (MS-8, MS-9).** `key` at keys 0, 12, 60, 108 and 127; `bend` at its ends and centre;
     `mpe.lift` 0 before the note-off and the release velocity after; channel pressure on its channel's voices only;
     `oscN.psi` of a held, locked swarm stays within 0.01 of its value over 1 s; a per-voice source's offset reaches its
     own voice only. **Must fail:** a per-voice source sent to every voice; ψ unreferenced to the frame, which turns at
     the note's frequency.
   - **36.11 Two paths (MS-3, MS-41).** Each route of a table of 20 (sources × destinations × polarity × Quantize) takes
     the path MS-41's rule names. An engine-path route from ENV 2 into `osc1.pitch` matches the closed form on ENV 2's own
     level at every sample within 10⁻⁶ in v, with no delay; the same envelope into `osc1.coupling` (outside path) reads
     the level at the end of the previous control interval. **Must fail:** an engine-path route evaluated one interval
     late; an engine path that evaluates anything but the closed form on the handed triples (a second matrix).
   - **36.12 Quantize (MS-22).** With Quantize on one route into `osc1.pitch`, the sum of every route into it arrives as
     `osc1.pitch`'s offset, snapped to the scale; into voice pitch it reaches the pitch system, and none reaches the
     engine as an offset; with Quantize off on every route, none reaches the pitch system. **Must fail:** only the
     quantized route snapped.
   - **36.13 The first sample (MS-42; PA-3).** A note whose velocity routes into `osc1.pitch` and whose global LFO routes
     into `osc1.coupling` sounds modulated from its first sample, whichever order the note-on and the events arrive in at
     that sample. **Must fail:** the voice's first sample unmodulated.
   - **36.14 Pairs and the cap (MS-16, MS-18).** A second route for a pair in one corner is refused; the 65th distinct
     pair across the corners is refused, while a pair held in all four corners counts once. **Must fail:** a cap counted
     per corner.
   - **36.15 A row of rate N (MS-21).** A route into a rate-N row changes a note only at its start, and a note started
     on the sample of a change takes the changed offset (MS-42). **Must fail:** a rate-N row that moves mid-note.
   - **36.16 Reset, load and state (MS-7, MS-36, MS-40).** After `reset()` the system's events equal a fresh one's for
     10 s, bit for bit; a load resets it; a restore does not. The shell's document round-trips `mod` byte-identically,
     and the engine's IF-4 document carries none of it. The modulators' host block keeps every id, range and order when a
     test build appends a setting. **Must fail:** a phase saved with the patch; an id renumbered by an append.
   - **36.17 Modulator settings morph (MS-27; IF-12).** An LFO's rate blends on its log taper between corners under
     Blend, flips at its quantum boundary under Quantum, and holds corner A's under Exempt; a waypoint list or region
     lock sent for a modulator key is refused. **Must fail:** a modulator key that takes waypoints.
   - **36.18 The defaults (MS-11, MS-13).** The init patch routes `pressure` and `mpe.pressure` to both oscillators'
     `coupling`, unipolar, depth 0.125, linear, in every corner; a new route starts at depth 0.25, linear, at its source's
     natural polarity. **Must fail:** aftertouch on one oscillator only.
   - **36.19 The erosion flag (MS-44; MO-26 (7)).** A corner's flag is set exactly while an admitted route of that corner
     names `sub.erode` with depth above 0 (a Control route included), and clears when that route is refused, muted or set
     to depth 0. **Must fail:** a refused route counted.
   - **36.20 Determinism, safety and the view (MS-5, MS-6, MS-45).** Events are bit-identical at block sizes 1 to 4,096
     and uneven splits over 10 s; appending a route, a modulator or a point leaves an existing patch's events
     bit-identical; under item 8's sanitizer run nothing allocates after prepare; reading the observation view between
     calls changes no event. **Must fail:** a stream taken in list order.
37. **The LFOs** (E.5; LF-1 to LF-14), global ones on the modulation system's calls and per-note ones through the engine.
   - **37.1 The drawn shape (LF-6, LF-7).** Segments follow C.12's curve within 10⁻⁶; a point added in one corner
     leaves the other corners' shapes within 10⁻⁶ of before (the split u·f, u·(1 − f)); a point parked in all four
     corners leaves the list. **Must fail:** a split that keeps each half's bend.
   - **37.2 Travelling points (LF-8).** Under Blend, at 10,000 seeded positions, no list runs backward; under Quantum,
     inserting a point leaves every other point's pick unchanged; at cohesion 1 every point picks one corner.
     **Must fail:** picks keyed by a point's place in the list.
   - **37.3 Rate and sync (LF-4, LF-5).** A free LFO's p is exact in samples (within 10⁻⁶ cycle over 60 s) and a rate
     change never moves it; a synced LFO meets the same bar at the same phase after a locate and after a loop's wrap,
     offset by the start phase. **Must fail:** a synced LFO counted from its own start.
   - **37.4 Global, per note and retrigger (LF-2, LF-3).** A global LFO gives every voice the same y; struck, every
     held note's wave restarts; per note, each note's wave starts at its note-on's sample and a legato change keeps it;
     the engine's per-note LFO and the modulation system's global LFO, given the same settings and start, agree within
     10⁻⁶ over 10 s. **Must fail:** a per-note LFO shared across voices.
   - **37.5 S&H and Random (LF-9, LF-10, LF-12).** Draws by index equal the sequential stream's; the first cycle reads
     draw 0; Linear, Cosine and Cubic pass through y_k at each cycle start; Cubic stays in [−1, 1]; each strike option
     reads the draw LF-12 names; a per-note stream is keyed by the note counter VO-10 publishes; a synced stream meets the
     same draw in the same bar. **Must fail:** S&H that holds 0 in its first cycle (the shipped law).
   - **37.6 Travel (LF-11).** None is the identity; Time arrives at exactly its time (to the sample), and on S&H equals
     the lab's SLEW law; Rate's slope never exceeds its bound; Spring stays in [−1, 1]. **Must fail:** Time as a
     one-pole, which never arrives.
   - **37.7 Envelope mode (LF-13).** The shape plays once and holds its last point; at the note-off the value falls to
     zero along osc 1's governing envelope's release; a global one restarts at every new note. **Must fail:** a shape
     that loops.
38. **The Kuramoto LFO, ORBITAL and the 2-D random** (E.6 to E.8).
   - **38.1 The Kuramoto LFO (MK-1 to MK-5).** Its phases, outputs, R and ψ at every tick match the test author's own
     computation of MK-3 within 10⁻⁴ RMS over the first 2 s, over K ∈ {−1, −0.5, −0.2, 0, 0.2, 0.35, 0.7, 1}, every shape
     and two rates; over 12 s, R is high at strong K (K ≥ 0.7), near the incoherent floor at K = 0, and below it at
     K ≤ −0.5, where the phases spread to an even interleave; a reset and a change of `kuro.voices` set θᵢ = i/n (MK-2).
     **Must fail:** the splay toward ψ + i/n, which settles into two clusters.
   - **38.2 ORBITAL (MB-1 to MB-11).** On the two periodic presets of MB-11 only: the orbit's period within 1 % of the
     test author's own integration of MB-1 at half the step, over 30 s; at drag 0 and the thermostat off, the total
     energy (kinetic plus softened potential) drifts by less than the test author's bound, relative to its start, over
     10 minutes, **with the speed clamp's count 0 throughout** (asserted from the observation view; Blind-Spot Armor risk
     row 6); the thermostat off is bit-identical to MB-1 without it; with it on, the free bodies' mean speed settles at
     v*/√(1 + 2τγ) within the test author's tolerance (γ = 3·drag); every observable, acceleration included, stays in
     [0, 1]; a planted non-finite velocity resets the field and is counted; bodies 5 to 8 are not sources; each reset
     policy resets at its event and no other (MB-8). **Must fail:** forward Euler, which gains energy; steps counted by
     wall time; a run whose clamp engaged, which would hide the drift.
   - **38.3 The 2-D random (MR-1 to MR-10).** Each mode, travel and trigger, run twice from one seed with the same
     triggers, gives the same path exactly; a replay repeats it; every point stays in [0, 1]²; the draws per Walk tick and
     per trigger are the same under every bias; under Tempo, a locate lands on the bar's point; at σ 0.05 from the centre
     (where the fold never acts), the Walk's stationary spread at 44.1 kHz and at 96 kHz agree within the test author's
     tolerance over 600 s. **Must fail:** an unseeded draw (the plant); a bias by rejection, which changes the draw count.
39. **Loops** (E.4; the named gate of ADR-372's armor note; Blind-Spot Armor risk row 6).
   - **39.1 The bound (MS-35).** **(a) Engine-free, first:** two global LFOs, sine, free, each routed bipolar at depth 1
     to the other's rate, read one interval late (MS-34), follow the closed recurrence, computed by the test author alone:
     with Δ samples per interval and R(v) = 0.02·2000^v Hz (`lfoN.rate`'s log law), vᵢ[n] = clamp(bᵢ + (2s_j[n − 1] − 1), 0, 1),
     pᵢ[n + 1] = pᵢ[n] + R(vᵢ[n])·Δ/f, s_i[n] = (sin 2πpᵢ[n] + 1)/2; the system's LFO outputs match it within 10⁻⁵ over
     10 s at the boundaries. **(b) The worst case**, `mod.loops` Allow: LFO 1 to its own rate, bipolar, depth 1; LFO 1 and
     LFO 2 to each other's rates; `kuro.R` to `kuro.K` and `kuro.rate`; `orb.energy` to `orb.G` and `orb.timeScale`; a
     global LFO and the 2-D random to `morph.x` with the LFOs' rates differing between corners; ENV 2 to `env2.decay`; a
     loop **through the engine**, `osc1.R` to `osc1.coupling`, bipolar, depth 1; depth wires on each; run for 600 s at
     block sizes 1, 64 and 4,096 under seeded edits: every value finite and every destination and source in its range at
     every control interval, no oscillation faster than the control rate's Nyquist (MS-35), and the events bit-identical
     across the block sizes. **Must fail:** the clamp removed on a modulator setting's destination, which leaves its range;
     a same-interval read, whose result changes with the order modulators are evaluated in.
   - **39.2 Refusing (MS-31, MS-32).** Under Refuse any possible loop, the route MS-32's order names is refused, a loop
     through the engine included; under Refuse only inside a corner, a loop formed only across corners is admitted, and
     in the mixed state that closes it the cycle's lightest route is muted and named; a corner edit that adds a pad edge
     and closes a loop mutes the lightest route and names it; under both, the pad counts; under Allow, nothing is refused.
     **Must fail:** a pad that does not count; the heaviest route muted.
   - **39.3 A preset into a corner (MS-33).** Under a refusing mode the load completes without the routes that close a
     loop, and each is named. **Must fail:** a load refused whole.
   - **39.4 One step late everywhere (MS-34).** On a loop-free patch the three modes give bit-identical events.
     **Must fail:** a refusing mode that reads same-interval.

Its thresholds are ratified with the suite and pinned; a threshold changes only on the human's approval.

### 10.2 The frozen references

Once the suite is green and the human has accepted the sound, the engine's renders of the suite's reference scenarios are frozen as its own references, by the suite's tool, never by hand. From then on every change to the engine's sound is recorded with a before-and-after render set and the human's word [X6].

### 10.3 Renders for judging

The engine is also judged by the human's ear on the bank. What the build delivers for that:
- An **offline render entry**: given a patch and a phrase (a sample-timed event list: notes, per-note pitch in Hz, expressions, the pedal, parameter changes), the engine renders a stereo file of 24-bit PCM at 44.1 kHz and at 48 kHz, unnormalised, from sample 0 of the event list.
- **The phrase set**: a held note, a chord, a fast line, a trill, supplied with the acceptance tests as event lists. Where a preset glides (B.1: `glide`, `glideAlways`), the phrase's per-note pitch carries the glide (exponential in log-frequency, 95 % in the preset's glide time, from overlapping notes or always), which stands in for the pitch system (RC-3) until it exists. A note glides from the most recently started note. Notes that start on the same sample, a chord's, glide from one another in event order, so each note of a chord after the first glides from the note before it, and the first starts on pitch unless `glideAlways` makes it glide from the last note played (round 49, the human's ruling, ADR-348, reversing round 39).
- **The renders**: every bank preset and every envelope preset, translated by Appendix B, on every phrase.

### 10.4 The CPU

PF-1, PF-2, PF-5 and PF-9 measured under PF-3 and reported to the human with PF-4's spread. A miss is reported with what would close it, never hidden.

### 10.6 "It worked": the judgement, written before the bank (round 38)

Ratified by the human before the bank stage began (ADR-284, B522), so the bar is fixed before any bank render exists. The new engine replaces the current one only when every condition below holds; nothing switches automatically.

- **1. The invariant suite.** Section 10.1 green, with every item the spec marks reported shown as reported.
- **2. The hold-out suite (B517).** Every check run, on a copy of the final tree, and every failure resolved: an engine fixed, or a check shown wrong engine-free and resealed. Results go to the human only.
- **3. The listening judgement.**
  - **The batch.** The human hears a capped batch of about 20 pairs: each one a bank preset rendered by the new engine and by the current one, on a phrase of the phrase set (10.3), level-matched, labelled. The pairs are ranked by a differential report of the two engines over the whole bank (B523; reported, never a gate), so the most different come first.
  - **The rule.** Any pair the human marks wrong blocks the cutover until the engine is fixed or the human rules the difference acceptable for that preset, recorded per preset.
  - **The record.** Each verdict, with the pair it was given on, is kept.
- **4. The CPU.** The heaviest bank patch at 8 voices, measured under the frozen protocol (PF-3) on a quiet machine, against PF-1's target of about 10.7 % of an M3 performance core for its one swarm oscillator (round 52, ADR-354):
  - at or under 10.7 %: **pass**;
  - over 10.7 %: **does not pass**. The figure and what would close it go to the human, and the cutover waits on the human's ruling.
- **5. The size.** Reported at cutover, never a gate. The engine stood at about 10,050 lines (9,056 of code) before the bank stage, against about 14,370 retired from the current engine. The expectation is that it stays under the retired count. BD-6's estimate of 2,100 to 3,000 lines was wrong for this scope and is corrected, not hidden.
- **6. The ruling.** When 1 to 4 hold, the human rules the cutover. The current engine is retired only on that word.

### 10.5 Build requirements

- **BD-6. Self-documenting and concise.** Names carry what the code does; comments carry only why: the constraint, the rejected alternative, the trap. No comment narrates history. A clean engine of this scope was first estimated at about 2,100 to 3,000 lines; that estimate was wrong for this scope (about 10,050 lines before the bank stage) and is corrected in 10.6, item 5. It is reported, never a gate.
- **BD-7. One arithmetic.** No code emulates another language's number semantics, no random draw exists only to preserve an order, and each constant has one value. Single precision, approximations and fused multiply-add are allowed throughout, judged by the suite and the ears.
- **BD-8. Built in stages behind the frozen structure**, each stage green on its part of the suite before the next: (1) the manifest, the API, the harness header and the state document (items 12, 22, and 13's document half: the text round trip and the version-0 parse); (2) the voice, ENV 1, the pedal and the output stage on a plain one-member saw (items 1, 2, 3, 5, 10's voice anchors, 17, 19); (3) the swarm, C.1 to C.8 (items 0, 10's swarm anchors); (4) the edge correction (items 7, 9); (5) the blades, C.9 to C.11, C.14, C.16 (items 0, 4, 10's blade anchors, 14), with a **stage 5 amendment** (round 29): ED-7, built on top of the accepted stage 5, with stage 5's items still green, and judged by item 7.7; stage 6 does not wait on it (the human's ruling, 2026-10-21: in parallel); (6) the sub and the noise oscillator, ENV 2 to ENV 4 and the source slot's envelope selector (item 10's sub anchors, item 18 for the source slot, items 24 to 33, and item 11 over every manifest row, now that every row can sound); (7) modulation, morph and state (item 13's render half: reset to fresh, migration rendered; items 14, 20, 21, 23), with a **stage 7 amendment** (B585, ADR-324, ADR-325; restated by ADR-377): the morph resolver (Appendix D) and the sources' morph levels (MO-26), built on the accepted stages and judged by item 34; every accepted item stays green, and item 19 is judged as RC-7.3 now reads; the bank stage carries on and does not wait on it (ADR-326, X38), and a corner, or the morph off, stays bit-identical. The structure blend (D.5, D.6, item 35) is after 1.0 (ADR-377), so the amendment reopens no stage for it; (8) the WebAssembly build (item 15a); then the bank (items 4, 6, 15b, 16) and the renders for judging; (9) **the modulation system** (Appendix E, round 62), once its [OPEN M] items are ruled, judged by items 36 to 39. It adds to the engine (stage 1's API: IF-13's events, PA-2's Control input, VO-10's note counter, PA-4's longer list; stage 2's voice: the per-note LFOs and the engine path's per-sample terms, MS-41 and MS-43) and to the resolver (stage 7's amendment: the modulation manifest's keys, the erosion flag, the pick reads), and it reopens those stages for those additions only: with no route and no per-note LFO every accepted item stays green, bit for bit (item 36.1), and the bank stage does not wait on it. The parameter table stays open until Q31 is ruled.
- **BD-9. Exactly what is tested ships**: the build that the suite and the CPU measurement run is the shipped build's code with no product-line branch added.
- **BD-10. A clean room.** The builder reads only this spec and the acceptance tests (with the input data they carry for the builder: the bank and the phrase set). The build happens in a tree that contains no earlier engine, no earlier engine's tests, no prototype and no legacy plugin; the tree is provided to the builder.
- BD-1 to BD-5 (section 3).

---

## 11. Items still open

Each is built as written here until it is ruled; a ruling changes only the requirements tagged with it.

### 11.0 To be ruled before the build (2)

**[OPEN M3] and [OPEN M4]** (round 62) are decided by the labs testing them (ADR-379 r5, r6) before the modulation system's stage (BD-8, stage 9). No engine stage waits on them. ADR-379 ruled M1, M2, M5 and M6; their text is in MS-41, MS-20, MS-29 and MS-27.

| # | the item | what is built until it is ruled | tagged |
|---|---|---|---|
| M3 | LFO corners that differ in kind (drawn against S&H or Random): (a) their outputs mix by weight (ADR-349's recommendation; a control signal, parametric); (b) the kind follows its quantum boundaries (ADR-377's principle). Being tested in a lab (mix against switch; ADR-379 r6) | not built until the lab rules | LF-8, MS-27 |
| M4 | the sine and cosine of each ORBITAL body's angle as sources (ADR-165 A1, for Wrap): (a) not sources (26 sources, as written); (b) sources, so a body crossing Wrap's seam moves the sound smoothly (34 sources). Being tested in the reopened ORBITAL lab (ADR-379 r5) | (a) | MB-9, MS-8 |

### 11.1 To be ruled during the build, before the stage that needs them (63)

| # | the item | what is built until it is ruled | tagged |
|---|---|---|---|
| Q2 | MPE: which expressions reach the swarm and the blades | IF-1's mapping; the routes are the modulation system's | IF-1 |
| Q6 | glide | one note glide, in the pitch system; inertia as the members' weight; no per-member frequency glide | IF-2, RC-3 |
| Q7 | pitch bend | per note under MPE through note expressions, global otherwise; a per-patch bend range, default ±2 st; the bend laws in the pitch system | IF-2, RC-3 |
| Q12 | swarm observables as modulation sources and readouts | sources: R per voice and oscillator, the onset burst level, gravity's offset in cents; readouts: IF-10's view | IF-3, IF-10 |
| Q13 | where onset scatter lives | in the engine, per oscillator, by C.7 | SW-12, `onsetScatter` |
| Q15 | loading a patch resets the engine's memory | yes, as IF-4 states | IF-4 |
| Q17 | the morph class of every key | the class column of section 5 | 5.1, IF-4 |
| Q19 | a saturating output stage | none: a linear output stage | OU-1 |
| Q20 | level naming and meter taps | `out.level` and the four source levels in dB; IF-6's meters | `out.level`, IF-6 |
| Q22 | the sub and noise share gravity and drift | gravity yes; drift and coupling no | SN-1, SN-4 |
| Q24 | the sleep rule | OU-4 | OU-4 |
| Q27 | quality modes | Standard and High only; no automatic thinning | `quality`, IF-9 |
| Q31 | the deferred rows | keep `onsetLock`, `dissolve`, `inertia`, `onsetScatter`, `timingCorrection`; one drift mode, the walk (C.4); gravity's basin 35 c; the pitch fan's fixed curve; no tempo-grid law, no per-member frequency glide, no attack or release scatter, no per-member envelopes, no cluster topologies, no super-width or mono-fold control, no bass-mono; `out.level` in dB; ENV 1 as C.12 with its curve range ±2.5 | the rows tagged [Q31], EN-1, IF-11 |
| X2 | members near the band edge | fade out between 0.40 and 0.48 of the internal rate (SW-13) | SW-13 |
| X3 | corners | the triangle base's, the carriers' and every blade contribution's corners corrected by the residual corner table q₂ (round 42); short tapers and Crush's slews by ED-7 | ED-6, ED-7 |
| X4 | one noise stream for all notes, or one per voice | one per voice and member | BL-18 |
| X5 | the noise blade's large steps | kept as the sound, band-limited | BL-18 |
| X7 | the frame after a crossing | a flip of ψ by half a cycle (within 10⁻⁶ cycle) counts as no change; otherwise the frame turns back at its rate limit | BL-10, C.13 |
| X11 | detune's range | a stepped Range: ×1, ×12, ×24 | `detuneRange` |
| X12 | the default patch | blade 1 off: an untouched patch is the swarm saw | `on` |
| X13 | the filters' count and routing | two filters per voice, each source routed to either, both or neither, serial or parallel (RC-1); the mono sub passes its own slot instance, driven by its chosen note (RC-1, SN-7) | RC-1, SN-7 |
| X17 | density compensation and the bank's balance | U^−0.75 for new patches; each bank preset translated with +5·log₁₀(N) dB (B.1) | SW-19, B.1 |
| X21 | aftertouch's default destination | channel pressure and per-note pressure route to `coupling` on both oscillators, +0.25 of its range (+0.125 in v) at full pressure, in the init patch and every factory preset unless the preset says otherwise: a held note locks tighter as it is pressed, the instrument's own gesture | IF-1, RC-4 |
| X22 | the envelopes' translation from the bank (PROPOSED) | B.1's ENV 1 rule (decay and release × 2.30, curve +2.30) and the blade envelopes as one-shot ENV 2 and ENV 3 (RC-4) | B.1, RC-4 |
| X41 | keytrack's value (the lead's proposal) | k = clamp((n − 60)/48, −1, 1), s = (k + 1)/2 | MS-8 |
| X42 | velocity and keytrack per voice (the lead's proposal) | per voice | MS-8 |
| X43 | release velocity (the lead's proposal) | a source only as MPE lift | MS-8, MS-9 |
| X44 | the keyboard sources' starting polarity (ADR-371 A1, the lead's reading) | bend and MPE per-note bend bipolar; every other keyboard source unipolar | MS-8 |
| X47 | the Walk's smoothing (the lead's proposal) | its own time in seconds, two one-poles; default 0.225 s (the lab's 0.3 × 1.5 s / 2) | MR-5 |
| X48 | the spring's controls (the lead's proposal) | tension in Hz and ζ; overshoot a readout | MR-8, LF-11 |
| X49 | the 2-D random's bias (the lead's proposal) | one amount with the bias choice; 0 is uniform | MR-4 |
| X50 | the 2-D random's seed (the lead's proposal) | its own seed; each mode its own stream (by purpose, MS-5, rather than the lab's XOR word) | MR-3 |
| X51 | a spread past the pad (the lead's proposal) | reflects at the edge | MR-4 |
| X52 | the 2-D random's update (the lead's proposal) | the 16-sample tick, no pitch special case | MR-2 |
| X53 | the 2-D random's rows (the lead's proposal) | one source, two rows, `xy.x` and `xy.y` | MR-1, MS-8 |
| X54 | the 2-D random's travel (the lead's proposal) | includes None | MR-8 |
| X55 | S&H's first cycle (the lead's proposal) | draws at every cycle start, the first included | LF-9 |
| X56 | S&H's clock (the lead's proposal) | its own cycle; sample-on-trigger, and ADR-266 r6's S&H mode on ORBITAL's mappings, after 1.0 | LF-9, E.9 |
| X57 | STEPS and S&H (the lead's proposal) | both stay: STEPS is a drawn preset shape (a factory drawing, not a kind), S&H a kind | LF-6, LF-9 |
| X58 | which envelope level is the source (T1; round 62's reading) | the level before its velocity factor (velocity is its own source) | MS-8 |
| X59 | ψ, the onset burst and gravity as values (T2; round 62's reading) | frac(ψ/2π), circular; the burst clamped to [0, 1]; 0.5 + cents/70, clamped | MS-8 |
| X60 | the outside path's rate (T7; round 62's reading) | control rate, ramped, PA-4's keys included; per-sample reaches a key only by the engine path (MS-41) | MS-4 |
| X61 | the route curves' forms (round 62's reading of MQ9) | exp s², log 1 − (1 − s)², bipolar on the magnitude | MS-14 |
| X62 | how the 64 routes count (round 62's reading) | distinct (source, destination) pairs over the four corners and the patch-wide routes | MS-18 |
| X63 | a route's weights (the corner-mapping lab's law) | follow `morph.law`, not the destination key's own mode | MS-25 |
| X64 | routes into the morph position (the corner-mapping lab's law) | patch-wide, not per corner | MS-19, MS-29 |
| X65 | the loop graph and timing (the corner-mapping lab's law) | the pad's edges as MS-31 states; the fixed admission order; one step late under every mode | MS-31, MS-32, MS-34 |
| X66 | LFO defaults (round 62's reading) | Global; start phase also for free LFOs; synced with retrigger runs from the strike; per note unstruck agrees in phase; phases never saved | LF-2 to LF-5, LF-14 |
| X67 | a strike on a per-note stream (round 62's reading of ADR-378) | Next and Keep start each note at draw 0 of its own stream; Rewind plays one shared riff | LF-12 |
| X68 | the travel stage's laws (round 62's reading of ADR-378) | Time along C.12's curve at u = ln(100)/4 (the SLEW law); Rate in full scales per second; Spring clamped to ±1; S&H's default Time 0.08 s | LF-11 |
| X69 | envelope mode's details (round 62's reading of ADR-371 A1) | the source value falls to 0; osc 1's governing envelope; a note-off before the end releases from where it is; global releases when no note is held | LF-13 |
| X70 | the LFOs' seeds (round 62's reading) | one per LFO, `lfoN.seed`, default N; streams by purpose | LF-12 |
| X71 | the Kuramoto LFO's details (round 62's reading of "as is") | Drawn reads a chosen LFO, default LFO 1; global; retrigger at a note after silence; exact 2π (the lab truncates it) | MK-2 to MK-4 |
| X72 | the new modulators' host parameters (round 62's reading of MQ15) | every M setting of E.6 to E.8 is a host parameter in MS-40's block | MS-40 |
| X73 | ORBITAL's details (round 62's reading) | one global field; reset Free, Transport or Note-on; Kick only from the card; v* 0.5, τ 0.5 s; the speed, energy and acceleration scales measured per factory preset over 30 s, a user field at the clamp's and softened gravity's maxima; bodies never saved | MB-2, MB-7 to MB-9 |
| X74 | the 2-D random's details (round 62's reading) | Spring the default travel; Lag kept beside ADR-378's four; mapping to a pad is two Control routes; its rows start unipolar | MR-1, MR-8, MS-8 |
| X75 | the MPE member channels' bend range (B504's per-patch setting, open) | 48 semitones | MS-8 |
| X76 | the route weights' glide (B1, the critic's review; round 62's reading) | each k[c] its own one-pole of `morph.glide` s, per sample, toward an unglided target; none at a landing | MS-25 |
| X77 | a Control route's path (round 62's reading of ADR-379 r2) | always the outside path | MS-41 |
| X78 | PA-4's "cutoff, level" (round 62's reading of ADR-379 r2) | the four source levels and the noise filter's `cutoff` join PA-4, rate A | PA-4, 5.2, 5.6 |
| X79 | Save current sound under a position route (the lead, B4) | a Morphable key's unglided target at `morph.x`, `morph.y` | MS-29 |
| X80 | where the modulation state is saved (round 62's reading) | a `mod` member of the shell's patch document, not the engine's IF-4 | MS-36 |
| X81 | a quantized route's path (round 62's reading) | always the outside path | MS-22, MS-41 |

### 11.2 Can be ruled after the build (9)

| # | the item | what is built until it is ruled | tagged |
|---|---|---|---|
| Q23 | a fixed post-stage per oscillator, or a rack slot, for the hosted distortion | a rack slot; no engine insert | RC-5 |
| Q26 | the width candidates and the mono-fold guarantee | per-member pan and per-source pan only, no polarity inversion; mono fold reported | SW-18, IF-8, `width` |
| Q29 | a min-spec measurement before the CPU row is frozen | not needed to build | PF-2 |
| X6 | the record of sound changes after the references freeze | kept, with a before-and-after render set and the human's word | 10.2 |
| X9 | a twin wider than half a cycle under the cut latch | the edge stays as stated in BL-16 | BL-16 |
| X23 | the bank's coupling rescale | removed by ear, round 51, ADR-352: a preset's K is stored as it is | B.1 |
| X24 | the start phases of the bank's negative-coupling presets | B.1's rule; judged A/B in the preset round | B.1 |
| X27 | an audio input into the voice (an envelope follower as a modulation source, a vocoder-style input) | recorded; no space reserved now | — |
| X26 | a separate toggle for legato's phase continuity (the human: "it could also be a separate toggle") | not built for 1.0 unless ruled: in legato the phases always continue (VO-6) | VO-6 |

---

## Appendix A. The envelope presets (input data)

Twelve presets in the bank format (B.1), each named for the envelope shape it must play (10.1 item 16). Every key not listed takes its base value (B.1).

| name | shape it plays | params |
|---|---|---|
| Pluck · sync blade | pluck | hot 2, w 0.3, c 0.8, k 2, hard 0.05, N 5, detune 12, K 0.5, benvA 1, benvD 220, benvK 0.55, benvW 0.2, benvVel 0.6, A 1, D 260, S 0, R 140 |
| Pluck · FM glass | pluck | mode 1, w 1, k 1, c 0.5, I 1.6, m 2, N 1, detune 12, K 0.5, benvA 2, benvD 600, benvK 0.12, A 1, D 700, S 0, R 400 |
| Pad · slow bloom | pad | mode 1, base 1, w 0.5, k 6, c 0.5, hard 0.7, I 0.9, m 2, N 7, detune 22, K 0.35, width 1, A 900, D 1500, S 0.8, R 2500 |
| Swell · two seconds | swell | w 0.35, k 4, c 0.6, N 7, detune 30, K 0.2, rotRate 0.25, A 2000, D 400, S 1, R 1200 |
| Swell · cut at the key-off | swell cut at the key-off | mode 4, w 0.4, k 3, c 0.5, N 5, detune 18, K 0.3, A 1500, D 10, S 1, R 10 |
| Gated stab · organ | gate | mode 4, w 0.3, k 3, c 0.7, N 3, detune 8, K 0.5, A 1, D 4000, S 1, R 10 |
| Stab · short decay | stab | mode 5, w 0.35, k 5, c 0.4, hard 0.2, N 4, detune 10, K 0.4, A 1, D 150, S 0.25, R 90 |
| Long tail · five seconds | tail | w 0.25, k 6, c 0.875, N 5, detune 18, K 0.35, A 4, D 600, S 0.6, R 5000 |
| Tail pile-up · steal and cull | steal (new notes take the quietest tails; under a cap the cull thins them) | w 0.3, k 4, c 0.8, N 6, detune 20, K 0.3, b2on 1, mode2 4, w2 0.25, k2 3, c2 0.35, A 1, D 300, S 0.7, R 4000 |
| Blade sweep outlives the amp | pluck | w 0.3, k 3, c 0.8, hard 0.1, N 3, detune 10, K 0.5, benvA 1, benvD 5000, benvK 0.8, benvVel 0.5, A 1, D 250, S 0, R 120 |
| Mono retrigger pluck | pluck | polyMode 1, glide 40, w 0.3, k 2.5, c 0.8, N 5, detune 14, K 0.4, benvA 1, benvD 300, benvK 0.5, A 1, D 300, S 0, R 200 |
| Legato swell lead | swell | polyMode 2, glide 150, mode 1, w 0.6, k 1, c 0.5, I 1.2, m 1, N 3, detune 16, K 0.5, A 600, D 400, S 1, R 800 |

---

## Appendix B. Input formats and their translation

### B.1 The bank format

**The file.** A JSON document `{ "note": text, "presets": [ { "category": text, "name": text, "params": { key: value, … } }, … ] }` holding 83 presets. Every value is a number except `kCustom`, which is text.

**Base values.** A preset may omit a key; an omitted key takes its base value: A 4, D 400, I 2, I2 2, K 0.35, N 5, R 280, S 0.85, aa 1, b2env 0, b2fm 0, b2mix 0, b2on 0, b2order 0, b2sp 0, base 0, benvA 2, benvA2 2, benvD 250, benvD2 250, benvK 0, benvK2 0, benvVel 0.5, benvVel2 0.5, benvW 0, benvW2 0, bspread 0, bspread2 0, c 0.875, c2 0.35, cScale 0, colB 0, colK 0, dcMode 2, depth 1, depth2 1, detune 14, driftRate 0.5, dspread 0, dspread2 0, fb 0, fmType 0, fmType2 0, frame 0, frame2 −1, glide 60, glideAlways 0, hard 0, hard2 0, hot 2, hot2 2, ispread 0, ispread2 0, k 6, k2 3, kCustom "1, 5/4, 3/2", kHz 1320, kHz2 800, kRule 0, kRule2 0, kRuleAmt 1, kRuleAmt2 1, kq 0, kspread 0, kspread2 0, law 0, lock 0, lock2 −1, m 3.37, m2 3.37, mHz 660, mHz2 660, mUnit 0, mUnit2 0, mirror 0, mirror2 −1, mode 0, mode2 4, morph 0.5, morph2 0.5, mshape 0, mshape2 0, mspread 0, mspread2 0, panOrder 0, phaseMode 2, polyMode 0, rot2Follow 1, rotRate 0, rotRate2 0, rotSpread 0, rotSpread2 0, rotSync 1, w 0.25, w2 0.2, width 0.7, wspread 0, wspread2 0, xm 0. A key with no listed base value takes the default of the row it translates to.

**Translation.** Each preset becomes a patch on osc 1 (osc 2, the sub and the noise oscillator off). Because the laws of Appendix C are normative, "as is" means the same number plays the same law.

**Enumerations map by meaning, not by number:**

| bank key | stored id → option |
|---|---|
| `hot`, `hot2`, `base` | 0 sine, 1 triangle, 2 saw, 3 square, 4 reverse saw, 6 sine→saw |
| `mshape`, `mshape2` | 0 sine, 1 triangle, 2 saw, 3 square, 4 reverse saw, 5 smooth noise, 7 S&H noise |
| `law` (spread law) | 0 gradient, 1 random, 2 alternate, 3 swarm, 4 drift |
| `phaseMode` | 0 random, 1 aligned, 2 settled |
| `rotSync`; `rot2Follow` | 1 restart per note, 0 free-running; 1 with blade 1, 0 its own |
| `mode`, `mode2` | 0 Sync, 1 FM reset, 2 FM free, 3 Noise, 4 Fold, 5 Ring, 6 Crush |
| `mirror`; `mirror2` | 0 off, 1 reflect, 2 twin −, 3 twin +; −1 as blade 1 |
| `lock`; `lock2` | 0 × pitch per cycle, 1 per blade, 2 Hz; −1 as blade 1 |
| `frame`; `frame2` | 0 member, 1 swarm; −1 as blade 1 |
| `kRule`, `kRule2` | 0 even, 1 harmonic, 2 undertone, 3 octaves, 4 major, 5 minor, 6 fifths, 7 golden, 8 primes, 9 custom |
| `polyMode`; `panOrder`; `aa`; `kq`; `fmType`; `mUnit`; `b2fm`, `b2sp`; `b2order` | 0 poly, 1 mono, 2 legato; 0 balanced, 1 spread fan; 1 → 1, 0 → 0; 0 free, 1 whole; 0 phase, 1 pitch; 0 × pitch, 1 Hz; 0 blade 1's, 1 its own; 0 blade 2 over 1, 1 blade 1 over 2 |

**Keys:**

| bank key | new key | rule |
|---|---|---|
| `N` | `osc1.unison` | as is |
| `detune` (cents) | `osc1.detune` | cents ÷ 100, with `detuneLaw` cents, `detuneRange` ×1, `distribution` even (C.3: the outermost members at ±the stored cents) |
| `K` | `osc1.coupling` | as is (round 51, ADR-352: the rescale proposed as X23 is removed, by ear). `cScale` has no destination. With C.1's taper, a preset whose K is under about 0.72 does not lock (Locked horde at 0.6 and Anchored horde at 0.7 among them); the human's factory patch session re-voices them. The negative-coupling presets' start is X24. |
| `phaseMode` | `osc1.phase`, `osc1.phaseRandom`, `osc1.retrigger` | random → Phase 0, Random 100, Retrigger on; aligned and settled → Phase 0, Random 0, Retrigger on |
| `width`, `panOrder` | `osc1.width`, `osc1.panImage` | as is |
| `aa` | `osc1.bandLimit` | as is |
| `xm`, `fb`, either above 0 | `osc1.loopDrive` | 1 (round 50, ADR-350: the sound the bank was voiced with). A preset with both at 0 takes the default |
| `base`, `xm`, `fb`, `law`, `driftRate`, `kq`, `kCustom`, `b2order`, `b2mix`, `colK`, `colB` | `baseWave`, `crossMod`, `feedback`, `spreadLaw`, `spreadDrift`, `quantize`, `ratios`, `stacking`, `upperHears`, `collisionPitch`, `collisionBite` | as is |
| blade 1 keys (`mode`, `hot`, `morph`, `lock`, `k`, `kHz`, `c`, `w`, `hard`, `depth`, `mirror`, `frame`, `rotRate`, `rotSync`, `fmType`, `mshape`, `I`, `mUnit`, `m`, `mHz`, `bspread`, `kspread`, `kRule`, `kRuleAmt`, `wspread`, `dspread`, `mspread`, `ispread`, `rotSpread`) | `osc1.blade1.…` (`mode`, `wave`, `shape`, `cutIn`, `cut`, `cutHz`, `position`, `width`, `edges`, `depth`, `mirror`, `frame`, `rotate`, `rotation`, `fmType`, `modShape`, `fmDepth`, `modIn`, `modRate`, `modRateHz`, `positionSpread`, `cutSpread`, `cutRule`, `ruleDepth`, `widthSpread`, `depthSpread`, `shapeSpread`, `fmIndexSpread`, `rotateSpread`) | as is; `blade1.on` on |
| blade 2 keys (the same with a `2` suffix, `b2on`, `rot2Follow`, `lock2`, `mirror2`, `frame2`, `b2fm`, `b2sp`) | `osc1.blade2.…` | as is |
| `A` (ms) | `env1.attack` | ms ÷ 1000; curve 0 (the bank's attack is linear) |
| `D`, `R` (ms) | `env1.decay`, `env1.release` | **PROPOSED** [X22]: ms ÷ 1000 × ln(10⁴)/4 ≈ 2.303, and the curve u = ln(10⁴)/4 ≈ 2.303. The bank's decay and release fell as e^(−4t/T), T the stored time, and a release ended where its level fell below 10⁻⁴ (−80 dB) of full scale: from a starting level L at t = (T/4)·ln(L/10⁻⁴), 2.1·T at L = 0.45 and 2.3·T at L = 1. With C.12's curve at c = 4u = ln(10⁴) ≈ 9.21 over T′ = 2.303·T the stage is (e^(−4t/T) − 10⁻⁴)/(1 − 10⁻⁴) of its distance: the bank's shape to within 10⁻⁴ of full scale, landing at 2.303·T. The two tails differ only below −80 dB, so each preset's tail keeps its audible length. |
| `S` | `env1.sustain` | as is; hold 0 |
| `polyMode` | `voice.mode` | as is |
| `glide` (ms, time to 95 %), `glideAlways` | the pitch system (RC-3); in the renders, the phrase's per-note pitch (10.3) | exponential in log-frequency, 95 % in the stored time; from overlapping notes, or always [Q6] Until the pitch system exists, a version-0 state's `glide` and `glideAlways` are **preserved** in the saved document (RC-9.3) and written back unchanged. |
| `benvA`, `benvD`, `benvVel`, `benvK`, `benvW` | **PROPOSED** [X22]: written as ENV 2's rows (5.2): `env2.attack` = `benvA`/1000 (`attackCurve` 0), `env2.hold` 0, `env2.decay` = `benvD`·2.303/1000, `env2.sustain` 0, `env2.decayCurve` +2.303 (as for ENV 1), `env2.velocity` = `benvVel`, `env2.oneShot` on; the routes stay the modulation system's: routed per voice to `blade1.cut` (or `cutHz` when the cut is in Hz) by 4·`benvK` octaves at full level, and to `blade1.width` by 3·`benvW` octaves at full level (each converted to its parameter's normalised law); and to blade 2 the same way unless `b2env` is 1. One-shot keeps the bank's blade envelopes, which ignored key-up, swelling on short notes. | until the modulation system exists, the render harness reads the engine's ENV 2 and ENV 3 (IF-3) and applies the routes through PA-2; item 15b's renders take the same routes as data beside the state, on both builds (round 34). Until the modulation system exists, these keys, their `…2` twins and `b2env` are **preserved** in the saved document (RC-9.3) and written back unchanged. |
| `benvA2` … `benvW2` with `b2env` 1 | ENV 3's rows by the same rule (`env3.…`), routed to `blade2.cut` and `blade2.width` | as above |
| (level) | `out.level` | 0 dB + 5·log₁₀(N) dB [X17] |
| (aftertouch) | the default route of X21 | |
| `dcMode`, `cScale` (beyond the rescale) | — | not used |
| (quality) | `quality` | Standard |

### B.2 The version-0 state format

A version-0 state is a key-value map (no `version` key) with the bank's keys (translated by B.1) and these. **B.2's rows override what B.1 implies.** A dropped key is reported to the user by name when the state opens, whenever its value would have sounded different from this engine's fixed behaviour (5.7) or default.

| key | new key | rule |
|---|---|---|
| `gain` | `out.level` | 20·log₁₀(g/0.0875) + 5·log₁₀(N) dB (a stored 0.35 lands at +12 dB plus the density offset; the +26 dB ceiling keeps every stored gain up to 1 at N ≤ 9) |
| `master` | folded into `out.level` | + 20·log₁₀(master) dB |
| `os` | `quality` | 1 → Standard, 2 → High; absent → High |
| `poly` | `voice.polyphony` | as is, within 1 to 64 |
| `b1on` | `osc1.blade1.on` | as is (overrides B.1's "on") |
| `enable` | `osc1.on` | as is |
| `octave`, `semi`, `fine` | `osc1.octave`, `osc1.semitone`, `osc1.fine` | as is, clamped to the new ranges (a clamp is reported) |
| `dist`, `seed`, `harmReach`, `anchor`, `pivotMode`, `driftDepth`, `inertia`, `onsetScatter` | `distribution`, `seed`, `harmonicReach`, `anchor`, `pivot`, `driftDepth`, `inertia`, `onsetScatter` | as is (`dist` 0 even, 1 JP-8000, 2 Gaussian, 3 Cauchy, 4 golden) |
| `h.law` | `detuneLaw` | 0 cents, 1 Hz, 2 ERB, 4 harmonic; 3 and 5 → cents, reported |
| `spread` s | `detuneRange`, `detune` | the range nearest s of ×1, ×12, ×24 (by log distance); `detune` × s/range, clamped to 1 (a clamp is reported) |
| `onset`, `dissolve` | `onsetLock`, `dissolve` | as is |
| `h.driftRate` | `driftRate` | as is (C.4) |
| `onsetAlpha` | `timingCorrection` | as is |
| `grav` | `voice.gravity` | as is |
| `keepPhase` 1 | `osc1.retrigger` off | overrides `phaseMode` |
| `spreadRamp` | `motionGrain` | 1 → 0, 0 → 1 |
| `driftMode`, `freqGlide`, `basin`, `attackScatter`, `relScatter`, `voiceEnv`, `motionCenter`, `inertiaCurve`, `bpm`, `beatMult`, `stretchB`, `cScale`, `dcMode`, `rtone`, `tempoGrid`, `onsetBipolar`, `ksmPerRate`, `bend`, `aaCarrier`, `aaXin`, `aaLoop`, `frameHold`, `stealFade`, `cutLatch`, `edgeTable` | — | dropped, and reported when the value differs from this engine's behaviour |

---

## Appendix C. Laws (normative)

Each law is stated with its constants and units; the builder may compute it any way (single precision, tables, SIMD, any loop order) that keeps every anchor and every invariant check of the law within its tolerance.

### C.0 Conventions

- θᵢ is member i's **swarm phase** in cycles; start phases (SW-10), coupling (C.1) and the frame (C.13) read it. Blades and base waves read the **blade phase** φᵢ = frac(θᵢ + ½). So a member started at θ = 0 begins its base saw at −1, and a blade's `position` is measured in φ.
- frac(x) = x − ⌊x⌋. Δ is the control interval in seconds (PA-3): coupling, drift, inertia, the spread law and the frame all update once per Δ. f_s is the internal rate, Nyq = f_s/2.
- **Every fade, gate ramp and crossfade has the raised-cosine shape** (ADR-239). That covers VO-3's steal fade, VO-4's cull, VO-6's re-run crossfade, VO-13's all-sound-off, VO-14's voicing and quality fades, IF-4's load fade, IF-6's mute and solo gate, PA-5's stepped-key crossfade, RC-10.1's tier change and RC-11.4's restore. **A source's morph level is a value, not a fade (MO-26): it moves with the pad and glides by PA-1.** A gain rising over a length T follows g(t) = ½·(1 − cos(π·t/T)) for 0 ≤ t ≤ T, and a falling one follows 1 − g(t). A crossfade's two gains sum to 1 at every sample (equal gain). The slope is zero at both ends, so no fade starts or ends with a corner, which the click detector of QS-4 would read as an event.
- Each member's phase advances per internal sample by gᵢ(t)/f_s, where gᵢ is its frequency from C.1 and C.5: each control interval's newly computed value is reached by a linear ramp across the following interval (so no change of coupling, detune, drift or inertia steps a frequency, SW-17), and the ramped value is scaled per sample by f₀(t)/f₀(t_k), f₀ the oscillator's pitch at the sample and t_k the last update, so a glide or bend is smooth inside an interval. **The final member frequency is max(0, ·)**; every conversion of a frequency to a ratio (a cut in Hz, a mod rate in Hz) divides by max(fᵢ, 1 Hz).
- A member waiting under onset scatter (C.7) counts in R, ψ and σ with its held phase and its natural frequency, and advances by zero.

### C.1 Coupling

- Taper (round 51, ADR-352): **km = 2·K²·(1 + 7·K⁸)** for K ≥ 0 and **km = −4·K²·(1 + 3·K⁸)** for K < 0, K = `coupling`, so km is 16 at K +1 and −16 at K −1. The manifest's law for `coupling` is linear; the taper is here.
- Narrowing (round 51): for K > 0, and not under the harmonic detune law, the members' natural frequencies are drawn together: **fᵢ ← f_t + (fᵢⁿᵃᵗ − f_t)·(1 − K/2)**, each control interval, before the pull. f_t is the members' mean under mean field, and the root member's fᵢⁿᵃᵗ under root pivot, so a locked swarm stays on the played pitch (SW-6). σ below is the spread before narrowing.
- Onset burst: **Kenv(t) = 8·o·|o|·e^(−t / max(0.01 s, `dissolve`))**, o = `onsetLock`, t the time since the voice's fresh start. Kenv is in the same units as km.
- Spread: **σ = max(0.08 Hz, √((1/U)·Σᵢ (fᵢⁿᵃᵗ − f̄)²))**, fᵢⁿᵃᵗ the natural frequencies of C.3, f̄ their mean.
- Targets, in Hz: **S\* = (max(0, km) + max(0, Kenv))·σ**; **P\* = 3·(max(0, −km) + max(0, −Kenv))·σ**. The applied S and P follow S\* and P\* through a one-pole of time constant 0.0043512 s, starting from 0 at the voice's fresh start. **The cap** (round 51): after the one-pole, S ≤ max(300 Hz, 4·σ) and P ≤ max(300 Hz, 12·σ), because a pull past about that strength turns unstable at the control interval. The harmonic detune law has no cap: it keeps that chaos by the human's ruling.
- Mean field: R·e^{iψ} = (1/U)·Σⱼ e^{i·2π·θⱼ}.
- The pull on member i, in Hz, recomputed each control interval (with U = 1 there is no pull, cᵢ = 0):
  - mean field: **cᵢ = S·R·sin(ψ − 2π·θᵢ − α)**;
  - root pivot: **cᵢ = S·sin(2π·(θ_r − θᵢ) − α)**, r the member whose natural frequency is nearest f₀ (the lowest index on ties);
  - and when P > 0.001 Hz: **cᵢ += P·sin(2π·(θ_c + (i − c)/U − θᵢ))**, c the centre member of C.2 (mean field) or r (root pivot).
- α = `phaseLag` in radians.
- The member's coupled frequency is gᵢ = fᵢⁿᵃᵗ + cᵢ before inertia (C.5).

### C.2 Placement

For i = 0 … U − 1 and uᵢ = i/(U − 1) (U = 1: x₀ = 0):

| distribution | xᵢ |
|---|---|
| even | 2uᵢ − 1 |
| JP-8000 | with J = (−1, −0.5715, −0.1774, 0, 0.181, 0.565, 0.9766), s = 6uᵢ, a = ⌊s⌋, b = min(a + 1, 6): J[a] + (J[b] − J[a])·(s − a) |
| Gaussian | clip(√(−2·ln max(r₁, 10⁻⁹))·cos(2π·r₂) / 2.5, −1, 1), two uniforms in [0, 1) per member from the placement stream, in member order |
| Cauchy | clip(tan(π·(r − ½)) / 4, −1, 1), one uniform per member from the placement stream |
| golden | 2·frac((i + 1)·0.6180339887498949) − 1 (no draws) |

The placement depends only on (U, distribution, seed). The centre member c = argmin |xᵢ| (the lowest index on ties); x_min = min xᵢ.

### C.3 Detune laws

With d = `detune` × range (range 1, 12 or 24), x'ᵢ = xᵢ − `anchor`·x_min, and f₀ the oscillator's pitch (the voice's sounding pitch × 2^(`octave` + `semitone`/12 + `pitch`/12 + `fine`/1200)):

| law | fᵢ |
|---|---|
| cents | f₀·2^(x'ᵢ·d·100/1200) |
| Hz | f₀ + x'ᵢ·d·20 |
| ERB | f₀ + x'ᵢ·d·0.35·ERB(f₀), ERB(f) = 24.7·(4.37·f/1000 + 1) |
| harmonic | f₀·(1 + d·`harmonicReach`·i) (by member index: placement and anchor do not apply) |

Then drift (C.4): fᵢ ×= 2^(Dᵢ·`driftDepth`/1200); and **fᵢⁿᵃᵗ = max(1 Hz, fᵢ)**.

### C.4 Drift

Per member, each control interval: **Dᵢ ← clip(Dᵢ + (r − ½)·2·√(ρ·Δ) − 0.4·Dᵢ·Δ, −1, 1)**, with ρ = 0.2 + 8·`driftRate` (the knob, 0 to 1), r a uniform in [0, 1) from the note's drift stream, Dᵢ = 0 at the voice's fresh start. It runs only while `driftDepth` > 0.

### C.5 Inertia

With w = knob^2.5, ω₀ = 2π·(8·(1 − w) + 0.6) rad/s, and yᵢ member i's offset from f₀ (yᵢ\* = gᵢ − f₀ its target), each control interval:

**mᵢ ← (mᵢ + (yᵢ\* − yᵢ)·ω₀²·Δ)·e^(−0.9·ω₀·Δ);  yᵢ ← yᵢ + mᵢ·Δ**  (damping ratio 0.45). The member's frequency is max(0, f₀ + yᵢ).

At a voice's fresh start yᵢ = yᵢ\* and mᵢ = 0. The swing runs only for w > 0.001 (knob above about 0.063); at or below it yᵢ = yᵢ\* when no swing is in flight. A swing in flight when w drops to or below 0.001 continues at the knob-0 ω₀ (2π·8.6 rad/s) until |yᵢ − yᵢ\*| < 0.01 Hz, then follows exactly, so the frequency never jumps. Only the offset swings: f₀ itself (the note, a bend, a glide, audio-rate pitch) is never filtered.

### C.6 Gravity

- Every grid step T_g = 256/44100 s (round(f_host·T_g) samples), if g = `voice.gravity` ≥ 0.005:
  - take the held voices, each with its settled pitch p_v; "held" here means gated (held or sustained, 6.4), as the voice law times it (VO-3): a stolen note joins when it starts, after its wait, and leaves when its late release takes effect; sort them by p_v ascending (stable);
  - for each pair (a, b), a < b, in that order (a outer): r = p_b/p_a; r_f = r/2^⌊log₂ r⌋; ρ_k the nearest of the 13 ratios **{1, 16/15, 9/8, 6/5, 5/4, 4/3, 7/5, 3/2, 8/5, 5/3, 16/9, 15/8, 2}** by |1200·log₂(r_f/ρ)|; e = 1200·log₂(r_f/ρ_k) cents; if |e| ≤ 35 c, with m = e·3g·T_g′/2, T_g′ = round(f_host·T_g)/f_host the grid's actual interval: p_b ×= 2^(−m/1200), p_a ×= 2^(m/1200).
- Below g = 0.005 no step runs, and every settled pitch persists as it stands (no snap back), riding the played pitch.
- p_v is the played pitch when the voice starts fresh or first enters gravity; thereafter it rides the played pitch: when the played pitch changes by a factor q, p_v ×= q. A released voice keeps p_v. The voice's sounding pitch is p_v once gravity has moved it, else the played pitch.
- A newly started note starts at its played pitch; notes already held keep the pitches they settled to.

### C.7 The ensemble (onset scatter)

Per oscillator a persistent offset vector Tᵢ (seconds, i = 0 … U − 1), zero at reset, at a seed change and at a load; when U changes, added members get Tᵢ = mean(T) and removed members' entries are dropped.

- At each fresh start (VO-6's table) with s = `onsetScatter` > 0: μ = mean(T); **Tᵢ ← Tᵢ − α·(Tᵢ − μ) + 0.001·s·zᵢ**, α = `timingCorrection`, zᵢ standard normal from the ensemble stream (in member order); then Tᵢ is clipped to μ ± 0.004·s (four standard deviations, which bounds the walk at α = 0); then wᵢ = (Tᵢ − mean(T)) − minⱼ(Tⱼ − mean(T)).
- Member i waits wᵢ seconds, silent, its phase held; then it enters with a gain eᵢ(t) = 1 − e^(−t/τ), τ = max(0.002 s, `env1.attack`), multiplied onto the voice's envelope. At s > 0 every member enters by this fade, the earliest at once (wait 0) but fading; at s = 0 there are no waits and no fade.

### C.8 Pan and density

- Seat sᵢ ∈ [−1, 1], before `pan` and `panInvert`:
  - **balanced:** U ≤ 9 from the table (N = 1: 0; N = 2: −1, 1; N = 3: −1, 1, 0; N = 4: −0.3333, 1, −1, 0.3333; N = 5: −0.5, 1, 0, −1, 0.5; N = 6: −0.2, 0.6, −1, 1, −0.6, 0.2; N = 7: −0.3333, 1, −1, 0.6667, −0.6667, 0.3333, 0; N = 8: −0.1429, 0.4286, −0.7143, 1, −1, 0.7143, −0.4286, 0.1429; N = 9: −0.25, 1, −0.75, −0.5, 0, 0.5, 0.75, −1, 0.25); for U = 10 to 16, member i takes the k-th of the U evenly spaced seats from −1 to 1, where k is i's rank when the members are ordered by frac((i + 1)·0.6180339887498949);
  - **spread fan:** sᵢ = 2·(i/(U − 1) − ½) (U = 1: 0);
  - **pitch fan:** rank the members by xᵢ ascending (stable; by index under the harmonic law); rank k sits at distance d_k = k/(U − 1) for odd U, (⌊k/2⌋ + ½)/(U/2) for even U (U = 1: 0); the seat is (−1 for even k, +1 for odd k)·d_k.
- `panInvert` mirrors every image: sᵢ → −sᵢ.
- Pan motion: each member index has a phase πᵢ advancing at (0.08 + 0.021·i) Hz on the gravity grid (not reset at a voice's start); the seat becomes clip(sᵢ + `panMotion`·sin(2π·πᵢ), −1, 1), and the gains below ramp across each grid step.
- The oscillator's image: **pᵢ = clip(sᵢ·`width` + `pan`, −1, 1)**: `pan` shifts every seat, and a seat that reaches an edge stops there.
- Gains: aᵢ = (pᵢ + 1)·π/4; **Lᵢ = cos aᵢ, Rᵢ = sin aᵢ**. The swarm's output is U^−0.75·Σᵢ yᵢ·(Lᵢ, Rᵢ), yᵢ member i's output (C.9).
- **A single source** (the sub, the noise oscillator) is panned by the same law with p = `pan`.

### C.9 The blade

#### C.9.1 Geometry

For a blade with centre `c`, width `w` (bypass when the effective width is below 0.004), cut rate `k`:

- `st = frac(c − w/2)`, `e = frac(φ − st)`; inside when `e < w`.
- Carrier rate `kk = k/w` in *per blade* units, else `kk = k`. In *Hz* units the per-member ratio is `k = k_Hz/max(fᵢ, 1 Hz)`.
- Reflect mirror: `eʳ = e > w/2 ? w − e : e`, else `eʳ = e`. Hot phase `hp = kk·eʳ`.
- Edge gate (all modes except crush): `t = hard·w/2`; `g = ½ − ½cos(πe/t)` for `e < t`, `½ − ½cos(π(w−e)/t)` for `e > w−t`, else 1.
- Centre: `c = position + rotation clock + member rotation + spread offset + (frame = swarm ? leadᵢ : 0)`.
- Every carrier's frequency is capped at 0.45 × the internal rate (item 8).

#### C.9.2 Modes

| Mode | hot(φ) inside the blade |
|---|---|
| Sync | `wave(blade wave, frac(hp + xin))`: hard sync at blade entry |
| FM reset | `wave(blade wave, frac(xin + cp))`, modulator phase `mEff·eʳ` restarts each blade |
| FM free | same, modulator phase free-running (`modX += mEff·dφ`) |
| Noise | sample-and-hold uniform noise, new value at each integer `hp` |
| Fold | `sin(π/2·(1 + (k−1)/4)·base)` |
| Ring | `base · wave(blade wave, frac(hp + xin))` |
| Crush | box-averaged hold levels with slew (C.9.4) |

**FM phase:** `cp = hp + (I/2π)·mod(shape, x)`. **FM pitch:** `cp = hp + acc`, `acc += ((1 + I/10)^{mod} − 1)·kk·dφ` (exponential deviation, symmetric in cents; depth shown as `±1200·log₂(1 + I/10)` cents); reset mode zeroes `acc` at blade entry. Where the modulator steps within a sample (S&H noise at each whole x; saw, reverse saw and square at their jumps), that sample's increment is integrated exactly: each side of the step contributes its own value of the increment in proportion to the fraction of the sample it holds, so the accumulated phase does not depend on where the host's samples fall (the human's ruling, round 47, ADR-304; engine-free, DT-3 on an S&H pitch-FM blade at index 2 goes from 2.73 dB to 0.06 dB). **Mod rate**: `mEff = modRate` (a multiple of the member's frequency fᵢ), or `mEff = modRateHz/max(fᵢ, 1 Hz)`. Under collision, I is the bitten index of C.11.

#### C.9.3 Waves and modulator shapes

`sine = sin 2πx`; `tri` (sine-aligned); `saw = 2·frac(x+½) − 1`; `rev saw = −saw`; `square`; **sine→saw** `= −atan2(r·sin θ, 1 − r·cos θ)/asin r`, `θ = 2π(x+½)`: the closed form of Σ rⁿ sin(nθ)/n. `r = 1 − (1 − r_max)^shape`, `r_max = min(0.995, 0.01^{f_h/Nyq})` from the blade's own carrier frequency `f_h`, which makes the shape self-band-limiting. Modulator shapes add *smooth noise* (smoothstep between per-integer values) and *S&H noise* (the per-integer value).

Periodic antiderivatives (used by crush and DC): sine `(1 − cos 2πx)/2π`; tri piecewise `2f²`, `2f − 2f² − ¼`, `2f² − 4f + 2`; saw `y² − y` with `y = frac(x+½)`; rev saw `y − y²`; square `f < ½ ? f : 1 − f`.

#### C.9.4 Crush

With the hold window w_e = w (w/2 under reflect): hold level `j` is the **average** of the base wave over `[st + j/kk, st + (j+1)/kk]`, via the antiderivatives, not a point sample. Point sampling would flip a whole step by ±2 whenever a grid point slides across a saw discontinuity as k changes; averaging is continuous in k. With *step slew* (the Edges control in crush mode), each step glides linearly in from the previous level over its first `hard` fraction, and the output ramps linearly onto `base(st + w_e)` over the last `min(hard, kk·w_e)` hold units, so the blade exit is seamless.

#### C.9.5 Mirror

*Reflect* makes the blade a palindrome. *Twin −* adds `−D(φ − ½)`, *twin +* adds `+D(φ − ½)`. With a half-wave-symmetric base (sine, tri, square), twin − removes even harmonics entirely (about 120 dB down) and is DC-free by construction.

#### C.9, further rules

1. **The blend.** Inside the window the output is **x + g·depth·(hot − x)**, x the blade's input (the base wave, or the serial input of C.11); outside, x. A blade's contribution is D = output − x. The gate's taper lies **inside** the window, from its edges inward. Crush has gate 1 throughout and uses `edges` as its slew.
2. **Waves exactly.** tri(x) = 4x for x < ¼, 2 − 4x for x < ¾, else 4x − 4 (0 at x = 0, rising); square(x) = 1 for x < ½, else −1; sine→saw with r < 10⁻⁴ is the sine.
3. **The carrier restarts.** In Sync, FM reset, FM free and Ring the carrier phase restarts at 0 at every window entry (hp = 0 at e = 0). The modulator of FM reset is at phase mEff·eʳ, so it restarts too; that of FM free starts at phase 0 at a voice's fresh start (round 56, ADR-363), advances by mEff·Δφ per sample and never restarts; pitch-FM's accumulator `acc` is zeroed at entry in FM reset only. With collision (C.11) the carrier phase also adds `cacc`, and with cross-mod or feedback `xin` (C.14): cp = hp + xin + cacc (+ the FM term).
4. **Noise.** The held value is uniform in [−1, 1); a new value is drawn whenever ⌊hp⌋ changes and at each window entry, from the member's own stream (BL-18).
5. **Modulator noise.** Smooth noise is a smoothstep (3f² − 2f³) between per-integer values uniform in [−1, 1); S&H noise is the per-integer value; both from the member's own stream, keyed by the integer.
6. **Fold.** hot = sin(π/2·(1 + (k − 1)/4)·b·x), with k the member's effective cut rate after spreads (never divided per blade), x the input, and b = 1 + 2·`collisionBite`·ov under collision (C.11), else 1.
7. **Crush when k is not whole.** The window holds ⌊kk·w_e⌋ whole steps and one last partial step; every step's level is the average over its full interval [st + j/kk, st + (j + 1)/kk], the partial one included. The first step's slew starts from base(st).
8. **The per-member effective values** (spreads from C.10, p their positions): width wᵢ = min(1, `width`·wMul), the blade bypassed when wᵢ < 0.004; cut kᵢ = min((Hz ? max(0.05, `cutHz`/max(fᵢ, 1 Hz) + kAdd) : max(0.25, `cut` + kAdd))·kMul, 0.9·Nyq·(per blade ? max(wᵢ, 0.001) : 1)/max(fᵢ, 1 Hz)), so the carrier's frequency (per blade: kᵢ/wᵢ·fᵢ) never exceeds 0.9·Nyq = 0.45·f_s; depth dᵢ = clip(`depth` + dAdd, 0, 1); FM index Iᵢ = `fmDepth`·iMul; shape mᵢ = clip(`shape` + mAdd, 0, 1); the sine→saw r uses f_h = (per blade ? kᵢ/max(wᵢ, 0.001) : kᵢ)·fᵢ and Nyq of the internal rate.
9. **Twins.** A twin's contribution is D evaluated at φ − ½ with the same carrier and modulator state as the main window at that sample; twin − subtracts it, twin + adds it.

### C.10 Spreads

**Laws** give each member a position `pn ∈ [−½, ½]` per spread parameter: *gradient* (`i/(N−1) − ½`), *random* (new draws at every fresh start; a re-struck sounding voice glides to them over 20 ms), *drift* (Ornstein–Uhlenbeck per member and parameter, item 3, clamped ±0.75), *alternate* (±½), *swarm* (`leadᵢ`). A solo member always gets `pn = 0`.

**Offsets** (blade 1; blade 2's own set uses independent draws):

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

Further rules:

1. **Independence.** Every (member, spread kind, blade) has its own position draw from the note's spread stream. Alternate: pᵢ = −½ for even i, +½ for odd i. Swarm: pᵢ = leadᵢ (C.13).
2. **Random.** Targets are uniform in [−½, ½) drawn at each fresh start; a fresh voice takes them at once; a sounding voice re-struck glides to them through a one-pole of time constant 0.02 s.
3. **Drift.** At a fresh start each p is drawn from the stationary spread (a normal draw × 0.289, clipped to ±0.75); then each control interval Δ: **p ← clip(p·e^(−θΔ) + 0.289·√(1 − e^(−2θΔ))·z, −0.75, 0.75)**, θ = 2π·`spreadDrift`, z standard normal: a stationary spread of 0.289 and a correlation time of 1/θ at any control interval.
4. **Cut spread under Quantize.** With rnd(x) = sign(x)·⌊|x| + ½⌋ (half away from zero): kAdd = rnd(rnd(s)·p); free: kAdd = s·p.
5. **Cut rules exactly.** List Lⱼ (j = 0 … U − 1): harmonic j + 1; undertone 1/(j + 1); octaves 2^j; major Set[j mod 3]·2^⌊j/3⌋ with Set = (1, 5/4, 3/2); minor the same with (1, 6/5, 3/2); fifths 1.5^j; golden 1.6180339887^j; primes Pⱼ/2 with P = (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53); custom Cⱼ mod n·2^⌊j/n⌋ over the n parsed ratios. Then x = clip((p + ½)·(U − 1), 0, U − 1); Quantize: r = L[round(x)]; free: r = exp((1 − f)·ln L[a] + f·ln L[b]), a = ⌊x⌋, b = min(U − 1, a + 1), f = x − a; kMul = r^`ruleDepth`. A rule other than even replaces the even cut spread (kAdd = 0). With U = 1 kMul = 1.
6. **Rotate spread.** The rotation-rate offset 2·s·p keeps its sign (BL-14): a negative spread turns the members the other way.

### C.11 Interplay

Interplay and collision act only with blade 2 on.

**Serial composition.** Let `D_A(φ)` be the lower blade's delta computed from the base (output − base), and `s_A, s_B ∈ {−1, 0, +1}` the twin signs (twin −, off or reflect, twin +). Evaluate the lower blade at φ and φ − ½:

  `L(φ) = D_A(φ) + s_A·D_A(φ−½)`  `L(φ−½) = D_A(φ−½) + s_A·D_A(φ)`

The second evaluation is needed whenever *either* blade has a twin (the upper twin must hear the lower blade at φ − ½); it uses the lower blade's twin state.

The upper blade transforms `x = base + λ·L` (λ = `upperHears`). Every mode's formula replaces *base* with *x* (C.9.2): fold `sin(drive·x)`, ring `x · carrier`, and the blend `x + g·depth·(hot − x)`; outside the blade the upper returns *x*. Its delta is `D_B = output_B(x) − x`, and similarly at φ − ½ with `x₂ = base(φ−½) + λ·L(φ−½)`.

  **out = base + L(φ) + D_B(φ) + s_B·D_B(φ−½)**

Consequences: at λ = 1 a generator mode on top (sync, FM, noise) *occludes* the lower blade inside its span (`out = (1 − g·d)·(base + L) + g·d·hot`); fold and ring on top process the lower blade's content; at λ = 0 this reduces exactly to the parallel sum, out = base + D₁ + D₂ (+ each twin by its sign), which does not depend on `stacking`.

**Serial crush.** The base part of each hold level stays box-averaged (C.9.4). The lower blade's part, `x − base`, is sampled at each step start and held; with slew it glides from the previous held value over the first `slew` fraction of the step, and over the exit ramp it glides onto the live `x − base`, so the blade exit stays continuous.

**Collision.** Per member and sample, with blade-local positions `e₁`, `e₂` and the C.9.1 edge gates (crush counts as hard-edged, gate 1 inside):

  `ov = gate₁(e₁) · gate₂(e₂)`

*Pitch* is an integrator on the upper blade, so the carrier chirps smoothly rather than jumping: `cacc ← 0` at upper-blade entry; `cacc += (4^(colK·ov) − 1)·kk_upper·Δφ`; carrier phase `cp = hp + xin + cacc` for sync, FM and ring. `cacc` is not wrapped (it is bounded by the window's length), so the edge correction's carrier-wrap detection sees its whole phase, taking `cacc` into the carrier phase at the start and end of each sample.

*Bite* reads `ov` at the current sample: the upper blade's FM index is **I_b = I·(1 + 4·colB·ov)** and its fold drive b = 1 + 2·colB·ov (C.9 item 6). Only the upper blade carries `ov` and `cacc`; the lower's stay 0. When both amounts are 0 they cost nothing.

### C.12 The envelopes' curve

- Each timed stage of length T with curve value u ∈ [−2.5, 2.5] follows **y(t) = (1 − e^(−c·t/T)) / (1 − e^(−c))** for 0 ≤ t ≤ T, with **c = 4·u**; at c = 0, y(t) = t/T. y(0) = 0 and y(T) = 1 exactly: a stage's time is exactly when it ends, and no stage leaves a residual step.
- Attack, from the level L₀ it starts at (0 from silence, the current level on a re-run): level = L₀ + (1 − L₀)·y. Hold: 1 for the hold time. Decay: level = S + (1 − S)·(1 − y). Sustain: S. Release, from the level L_r at key-up: level = L_r·(1 − y).
- Defaults for a new patch: attack u = 0 (straight); decay and release u = +1. The bank's exponential is u = ln(10⁴)/4 ≈ 2.303 (B.1).
- A change of a stage's time or curve while it runs restarts that stage from the current level toward the same target, over (1 − the elapsed fraction) × the new time, the elapsed fraction being the STAGE's own (the fraction of its time that has passed, advancing at 1/T per sample, so a gliding change that restarts it at every control interval lands where that fraction reaches 1). A change of sustain during the decay retargets the decay to the new sustain the same way; during sustain the level moves to it by PA-1's smoother. Restarts happen at most once per control interval (changes within an interval take effect together).
- ENV 1 to ENV 4 all follow this law. In one-shot mode (ENV 2 to ENV 4's `oneShot`, RC-4) a key-up is ignored until the decay ends; the envelope then releases if its voice is not gated (6.4), else holds its sustain until it is not.

### C.13 The frame

- Each voice keeps a frame angle Φ (cycles) per oscillator. Each control interval Δ:
  - advance: Φ ← Φ + the mean of the members' measured phase advances over the interval (a waiting member's is zero, C.0);
  - measure: d = the wrapped difference ψ/2π − Φ, in (−½, ½];
  - track: g(R) = clip((R − 0.05)/0.2, 0, 1); the fraction F = 1 − (1 − g)^(Δ/Δ_ref), Δ_ref = 32/48000 s; the move is F·d, limited to ±8·Δ cycles; where F = 1 and the move is within the limit, Φ = ψ/2π exactly.
- At a voice's fresh start Φ = ψ/2π if R ≥ 0.25, else member 0's phase θ₀; leads are set, not ramped, at a fresh start and for a member that joins a held frame.
- Each member's lead is **leadᵢ = frac(θᵢ − Φ + ½) − ½** cycles; in swarm frame it adds to the window's centre (C.9.1), and it reaches the blade as a ramp across each control interval at every `motionGrain` (SW-16).
- After a crossing (R through 0, ψ flipping): [X7].

### C.14 Cross-mod and feedback

- Each member's loop tap is its pre-correction output y⁰ᵢ (below) with C.16's subtracted estimate eᵢ put back, scaled by `loopDrive` d: **ŷᵢ[n] = d·(y⁰ᵢ[n] + eᵢ[n])**, with no filter in the loop (round 50, ADR-350: it reverses ADR-189 A1's filter here). After 1.0, in a blending kernel group, the tap reads the mixture of these, weighted as SB-3 mixes the outputs (ADR-377). **The loop reads the member before its edge correction** (round 23, ADR-250): its input y⁰ᵢ is member i's output yᵢ (C.9, C.8) without the edge and corner corrections of ED-1, ED-6 and ED-7 and before ED-1's filter E, which runs after the tap (round 42); C.16's subtracted estimate is included, and the 8 Hz blocker, which acts on the summed output, is not. So the loop never hears the band-limiting, its dynamics are the same for every table design, and the audible output stays band-limited. **The loop runs at the host rate at both qualities:** the tap, and xin below, step once per host sample, n counting host samples; at High each member's tap reads its y⁰ᵢ at the second of the host sample's two internal samples, and xin is held across both. So the loop takes the same number of steps per window at Standard and High (round 21, ADR-245).
- The carrier phase offset of member i at sample n is **xinᵢ[n] = ½·(`crossMod`·ŷᵢ₊₁[n − 1] + `feedback`·½·(ŷᵢ[n − 1] + ŷᵢ[n − 2]))**, members in a ring (member U − 1's neighbour is member 0). It applies to Sync, FM and Ring carriers on both blades and changes no member frequency.

### C.15 Time constants and grids

| what | value |
|---|---|
| control interval Δ (coupling, drift, inertia, spreads, frame) | round(f_host/1500) samples |
| coupling smoother | one-pole, 0.0043512 s |
| gravity and pan-motion grid | 256/44100 s, round(f_host·256/44100) samples |
| parameter smoothing | two one-pole stages in series, 0.006 s each, in v, snapping when both are within 10⁻⁶ of the target |
| random spread glide | one-pole, 0.020 s |
| rotation homing | 0.030 s |
| DC estimate refresh; its smoothing | every 256/44100 s × max(1, ⌈J/64⌉); one-pole, 0.00756 s (C.16) |
| steal fade; cull fade; stepped-key crossfade; mute and solo; mono velocity | 0.005 s linear; 0.008 s linear; 0.005 s equal-gain; 0.005 s; 0.005 s |
| DC blocker (with cross-mod, feedback, collision or FM free in pitch FM) | first order, 8 Hz (C.16) |
| High quality's decimation | low-pass at 0.45 × the host rate, at least as steep as a 4th-order Butterworth (two biquads, Q 0.5412 and 1.3066), no look-ahead |

### C.16 DC removal

Each member subtracts from its output the cycle mean of its blades' contributions, so no blade leaves a DC offset; the base waves are zero-mean by construction.

- **The estimate.** For each blade (twins included by their sign: twin − contributes 0, twin + twice the main window's), the mean over one cycle of its contribution D(φ) (C.9 item 1) at the member's current effective values (C.9 item 8):
  - **Sync on a closed-form carrier wave** (not sine→saw): exactly, depth·[(F_hot(kk·w) − F_hot(0))/kk − (F_base(st + w) − F_base(st))] (under reflect, 2·(F_hot(kk·w/2) − F_hot(0))/kk for the first term), F the antiderivatives of C.9.3; with Edges above 0, less what the tapers remove, integrated by the midpoint rule over the two taper regions with J_t = clamp(⌈32·kk·t⌉, 32, 512) points each, t = hard·w/2, the carrier read at eʳ (C.9.1) under reflect.
  - **Every other mode and wave**: the midpoint rule over the window with J = clamp(⌈32·F⌉, 48, 1024) points, F = w·kk, or for the FM modes F = w·(kk + I·mEff + mEff) (the features that must be resolved), with a Noise blade's held value taken as 0 (its mean). **FM free**, whose cycle mean depends on where its free-running modulator stands at the window's entry, takes the mean over 8 of the entries the modulator will make: θ + j·Δ for j = 13k mod 32, k = 0 … 7, θ its actual phase at the latest window entry (before the blade's first entry, where it would have entered: its phase less mEff·e, round 56) and Δ the fractional part of its cycles per member cycle at the current values (round 55, ADR-362; every estimate reads afresh, keeping no earlier readings; it read j = 0 … 31 before that round, which gives the same mean when Δ is a multiple of 1/8 and the modulator is not a noise shape). Each reading starts pitch FM's accumulator at 0, and a noise modulator's keys are the floors of its positions there (C.9 item 5), read without drawing. **One reading is enough** when the entries barely move: at Δ = 0, for any modulator shape, one reading at θ; and for a modulator that is not a noise shape, with d the signed distance of Δ from its nearest whole number and |d| below 1/496, one reading at θ + 15.5·d. At a slow beat the estimate follows the beat; otherwise it approaches the long-run mean (round 19, ADR-243: an average over evenly spaced start phases is wrong wherever the modulator's entries do not cover the cycle evenly, as at a whole-number `modRate`). Its cost is reported (PF-4); how it keeps up with a note's opening glide is the builder's, judged by BL-19 and item 4.
  - **Serial interplay** (`upperHears` > 0): the pair together, the mean of (out − base) over one cycle by the midpoint rule with J = clamp(⌈32·(G₁ + G₂ + 4)⌉, 64, 2048) points, G = kk + I·mEff + mEff for an FM blade, else kk. With an FM free blade in the pair, the walk runs once for each j, every FM free blade standing at its own θ + j·Δ, and one reading is enough only when each FM free blade in the pair allows it.
  - **Every J is a minimum:** more points, or exact integration, are allowed.
- **When.** At a voice's fresh start, before its first sample (SW-15), the estimate is computed and applied at once; thereafter it is recomputed every 256/44100 s × max(1, ⌈J/64⌉), J the points that estimate used (for FM free, one modulator start's), and the subtracted value follows each new estimate through a one-pole of time constant 0.00756 s, so it never steps.
- **The blocker.** While `crossMod` or `feedback` is above 0 and live on an oscillator (PA-8: a blade is on in Sync, FM or Ring mode), or a collision amount is above 0 and live (both blades on), or a blade is on in FM free with pitch FM (the human, round 58, ADR-365: its accumulator runs free, where each of the estimate's readings starts it at 0), so that a parameter PA-8 makes inert never changes the sound, the blocker engaging or releasing by a 5 ms crossfade (round 19, ADR-243), that oscillator's summed output per voice also passes a first-order DC blocker at 8 Hz (its state 0 at the voice's fresh start), because those depend on past samples the estimate cannot see.
- The edge corrections add no offset of their own (ED-3, round 42).

## Appendix D. The morph (normative)

The morph holds four **corners**, each a full set of the manifest's values, and a **position** on a square **pad**.
This appendix states how a position becomes the values the engine renders: the morph's clock and glide, its
settings, the quantum draw, the laws (Blend, Quantum, Stepped), each parameter's own morph mode and count, how
structure follows its quantum boundaries (MO-13), a key's waypoints and region lock, and how a source's on switch
fades. Rows MO-1 to MO-13, MO-20 and MO-27 (D.0 to D.4), MO-21 to MO-25 (D.7, round 61, ADR-373) and MO-26 (D.8, round 61,
ADR-376, ADR-377) are the **resolver**, built by the clean room beside the engine (ADR-325). **D.5 and D.6 (MO-14 to
MO-19, MO-20's set half, SB-1 to SB-13) are after 1.0 (round 61, ADR-377):** setting sets, and the engine's rendering
of them, are neither built nor judged for 1.0.

### D.0 Who computes what, and when

| id | requirement |
|---|---|
| MO-1 | **Two halves.** The **resolver** maps the corners, the position, the morph settings (D.2), each key's mode and count (D.4), and each key's waypoints and region lock (D.7) to events for the engine (MO-20): one value for each Morphable key and each Structural key, sent as `Param` events, and each source's morph level (MO-26, IF-12). The engine applies `Param` events as any (PA-1, PA-5, PA-6). **In 1.0 no setting set is sent or rendered (ADR-377):** every Structural key resolves to one value (MO-13), and D.5 and D.6 are after 1.0. The corners, the position, the morph settings, the modes, the waypoints and the region locks are the morph system's state (RC-11.1); the resolver receives them through IF-12 and saves none of them. |
| MO-2 | **Device keys** are outside the morph. The resolver never emits them; they are never blended and never flipped (5.1). |
| MO-3 | **Clock, cadence, prepare and reset.** The resolver keeps a sample clock of its own, advanced by the samples it is asked to process and counted from its reset exactly as PA-3 counts the engine's, so its control intervals coincide with the engine's. It resolves once at the first sample of every control interval. An input (a position, a transport event, a setting, a mode, a corner edit, a key's waypoints, a region lock) takes effect at the first interval boundary at or after its sample time. Its events are timed at that boundary. Prepare (given the host rate) allocates everything it will use (RT-1). Reset sets its clock to 0, clears every latch (MO-8: no corner held), clears the stepped hold (MO-10), snaps the glide (MO-5) to its targets, and sets the step phase to 0. After reset it resolves exactly as a freshly prepared resolver with the same inputs (DT-6's rule), and its first resolution sends every source's level (MO-26, MO-20). It reads no clock (DT-5), allocates nothing after prepare, and its output is independent of block size (DT-2). |

### D.1 The weights and the glide

| id | requirement |
|---|---|
| MO-4 | **Corners and weights.** The corners sit at A = (0, 0), B = (1, 0), C = (0, 1), D = (1, 1). At a position (x, y) in [0, 1]², the **corner weights** are bilinear: w = [(1 − x)(1 − y), x(1 − y), (1 − x)y, xy], in that order, each in [0, 1], summing to 1. Every law reads the position only through these weights, at the stepped position under Stepped (MO-10), except the waypoint law, which also reads the position itself (MO-22). |
| MO-5 | **Morph glide** (ADR-325: on the resolver's output, before PA-1). In 1.0 one glide, (b), a one-pole of time constant `morph.glide` seconds, stepped once per sample on the resolver's clock with the coefficient 1 − e^(−1/(f_host·`morph.glide`)), and bypassed at 0. **(b) The Morphable values:** each Morphable value the resolver emits is glided in its normalised form v (circular keys the shortest way round), and snaps to its target once within 10⁻⁶. Under Blend, for a key whose law is not `lin, wraps`, whose count is 4, whose waypoints are not acting (MO-23) and whose blend MO-26 (2) does not restrict, (b) applied to the blend of fixed corner values gives the same value as the blend at the glided weights; for circular keys and at count 2 it is a glide of the value, not of the weights. Under Quantum it softens a Morphable key's flip. **Never glided:** Quantum's picks (MO-7 reads the unglided weights) and Structural keys (they flip by PA-5). The corner weights' own glide, (a), is after 1.0 (D.5): no 1.0 row reads glided weights, since MO-7, MO-9, MO-22 and MO-26 read the unglided ones (round 61, the lead (critic S5)). |
| MO-27 | **Corner exactness** (1.0; round 61, the lead (critic B3); SB-10's role, which is after 1.0). At an exact corner c (w[c] = 1; the stepped position's under Stepped), every key the resolver sends takes corner c's stored value exactly. A Morphable key sends v[c]: a circular key included, as v[c] itself with no atan2 round trip (MO-9), at either count, and with waypoints (MO-22 reads the corners). A Structural key sends c's value: every other corner scores −10¹², so its pick is c, locked or not. An Exempt key sends its one value, which every corner holds (MO-12). Each source's level is exactly 1 where its switch is on at c, and its switch is off where it is off at c (MO-26). Once MO-5 (b)'s glide and PA-1 have settled, the output is therefore bit-identical to the same patch with the morph off and `morph.audible` at c (MO-20). |

### D.2 The morph settings

The morph's settings belong to the patch, are saved with it by the morph system (RC-11.1), and are never morphed. **That includes the pad's position:** a preset saves `morph.x` and `morph.y` where the puck was, and they stay host parameters (round 61, ADR-376; GN-10).

| key | label | range | default | what it does |
|---|---|---|---|---|
| `morph.on` | Morph | off, on | off | While off, corner `morph.audible` sounds and the resolver emits nothing after MO-20's hand-over (ADR-104: morph ships off; ADR-319 item 1). |
| `morph.audible` | Audible corner | A, B, C, D | A | The corner that sounds while the morph is off, whatever the position (ADR-319 item 1). |
| `morph.law` | Morph mode | Blend, Quantum, Stepped | Quantum | The law every Follow key obeys (D.3). |
| `morph.count` | Count | Two, Four | Four | How many corner values a Morphable key blends at once under Blend and Stepped (MO-9). |
| `morph.temperature` | Temperature | 0.02 to 4, log | 1 | How sharply each corner owns the pad under Quantum (MO-7). |
| `morph.cohesion` | Cohesion | 0 to 1, lin | 0.3 | How far all keys pick the same corner together under Quantum (MO-7). |
| `morph.seed` | Morph seed | 1 to 9,999, step | 1024 | Seeds the quantum draw (MO-6). |
| `morph.stepRate` | Step rate | 0.25 to 32 steps per second, log | 4 per second | How often Stepped takes a new position while `morph.stepSync` is off (MO-10). |
| `morph.stepSync` | Sync | off, on | off | On: steps follow the host's musical position at `morph.stepDivision` (MO-10, RC-7). |
| `morph.stepDivision` | Division | an index into RC-7.4's table, 0 to 21, step | 3 (1/16) | The step's length while `morph.stepSync` is on. |
| `morph.stepTriplet` | Triplets | off, on | off | Offers the table's triplet rows in the Division menu (RC-7.4); changes no sound. |
| `morph.stepDotted` | Dotted | off, on | off | Offers the table's dotted rows in the Division menu (RC-7.4); changes no sound. |
| `morph.glide` | Morph glide | 0 to 5 s, pow | 0.008 s | MO-5. |
| `morph.x` | X | 0 to 1, lin | 0 | The pad position's x: 0 is the A–C side, 1 the B–D side (MO-4). |
| `morph.y` | Y | 0 to 1, lin | 0 | The pad position's y: 0 is the A–B side, 1 the C–D side (MO-4). |

The ranges and defaults of on, law, temperature, cohesion, seed and glide are the legacy shell's (its ids 151, 154 to
158; law 0 was quantum), and so are the position's (its ids 152 and 153: 0 to 1, default 0, so a fresh patch sits on
corner A; ADR-373 item 9); the tapers and the step rate's range are ADR-326's (X36). **Every setting but `morph.seed` and
`morph.audible` is a host parameter**, the position's `morph.x` and `morph.y` included (round 61, ADR-373; GN-51): they come from a morph manifest appended to
the lockfile (IF-11; ADR-326, X39; `morph.audible` is a page choice, ADR-327). Each is one lockfile row,
with one range, taper and step count, which is why the rate and the division are two settings. The Division menu's triplet and dotted toggles are host parameters too (round 48). The engine never receives them; they are the resolver's inputs (IF-12).

### D.3 The draw and the laws

| id | requirement |
|---|---|
| MO-6 | **The quantum draw** (ADR-104; ADR-325 for X32: keyed per key, permanently, which is ADR-186 §5(g)'s ADR; ADR-185's rule then holds by construction). A stream for a purpose p and the seed S = `morph.seed` is a mulberry32 generator whose 32-bit state starts at h, the FNV-1a hash (offset basis 0x811C9DC5, prime 0x01000193, arithmetic modulo 2³²: h ← (h XOR b)·0x01000193 for each byte b) of the UTF-8 bytes of p followed by the four bytes of S, least significant first. Each step does: a ← a + 0x6D2B79F5; t ← (a XOR (a >> 15))·(a OR 1); t ← (t + (t XOR (t >> 7))·(t OR 61)) XOR t; out ← t XOR (t >> 14). Every operation is modulo 2³² on unsigned values, and >> is a logical shift. The draw is u = out / 2³², clamped to [10⁻⁹, 1 − 10⁻⁹]. A key k's draws are gₖ[c] = −ln(−ln u), the first four outputs of the purpose `morph/key/` followed by k's manifest key, for c = A, B, C, D in order. The **shared** draws g*[c] come the same way from the purpose `morph/shared`, which no manifest key can produce, since keys hold no `/`. **No draw depends on a key's position in any list or on how many keys exist**, so appending a key never changes how an existing patch sounds. Blend and Stepped read no draw. |
| MO-7 | **Quantum's pick.** For a key k at the unglided weights w (the stepped position's under Stepped), each corner scores sₖ[c] = λ[c] + (1 − h)·gₖ[c] + h·g*[c], with λ[c] = ln(w[c])/T where w[c] > 10⁻⁹ and −10¹² otherwise; T = max(0.02, `morph.temperature`) and h = `morph.cohesion`. The key's **pick** is the corner with the highest score (the lowest corner on a tie). **A key with a region lock scores by MO-24 instead, and its latch (MO-8) is MO-24's** (round 61, ADR-373). A Morphable key emits the pick's value (glided by MO-5 (b), then PA-1). A Structural key resolves to it as one value, held by MO-8, except a source's on switch under Blend or Stepped (MO-26). A source's keys pick only among its on corners while MO-26 (2) or (3) restricts them (the noise's among its on or erosion corners, MO-26 (7)). |
| MO-8 | **The latch: a band of 0.1** (ADR-325). A Structural key keeps the corner it holds until another corner's score exceeds the held corner's by more than m = ln(0.55/0.45)/T. On an edge where two corners' draw terms are equal, that is a band of exactly 0.1 of the pad, centred on the even split; elsewhere it is the same margin in score. A key holds no corner before its first resolution after reset, and takes its pick then. Morphable keys have no latch. |
| MO-9 | **Blend.** A Morphable key's value at count 4 is the blend of its four corner values at MO-4's weights, unglided (MO-5 (b) then glides the value): Σc w[c]·v[c] in the normalised form v. For a key whose law is `lin, wraps` (a circular key), it is the angle of the weighted mean of unit vectors, frac(atan2(Σc w[c]·sin 2πv[c], Σc w[c]·cos 2πv[c]) / 2π); where that mean's length is below 10⁻⁹, the value of the heaviest corner (the lowest on a tie); and at an exact corner, that corner's v itself, with no atan2 round trip (MO-27). At count 2 (MO-11), the candidates are the key's distinct corner values, each with its **pooled weight** W, the sum of the unglided weights of the corners holding it, and an id, the lowest corner holding it. With at most two candidates, every candidate with W > 0 takes part, at weight W. With three or more, they are ordered by W, highest first, ties to the lower id; with W₁ ≥ W₂ ≥ W₃, the first two take part, at weights (W₁ − W₃)/(W₁ + W₂ − 2W₃) and (W₂ − W₃)/(W₁ + W₂ − 2W₃), and where W₁ + W₂ − 2W₃ = 0 (the top three tie) the first alone, at weight 1. The same sums run over those, with those weights. This two-value law is continuous wherever the top three pooled weights do not all tie (MO-18 states the same law for settings). A Structural key does not blend in 1.0: it follows its quantum boundaries (MO-13; ADR-377). **A Morphable key whose waypoints act (MO-23) takes MO-22's value instead** (round 61, ADR-373). **A source's keys blend only over the corners where it is on, while MO-26 (2) applies (the noise's over its on or erosion corners, MO-26 (7)).** |
| MO-10 | **Stepped** (B424). The resolver keeps a **step phase** φ and a **stepped position**. At reset, φ = 0 and the stepped position is the position at the first resolution. **φ in closed form.** When `morph.stepSync` is off, φ(n) = φ₀ + (n − n₀)·`morph.stepRate`/f_host, where n₀ is the sample of the last change of the rate, of sync, or of the transport's play state, and φ₀ is φ(n₀); so φ never jumps at a rate change, and there is no rounding drift at fractional samples per step. When it is on and the transport is playing, φ(n) = (the host's musical position at n in quarter notes, RC-7.2)/(the length of `morph.stepDivision`'s row of RC-7.4's table, in quarter notes). When it is on and the transport is stopped (ADR-325, X34), φ(n) = φ₀ + (n − n₀)·tempo/(60·f_host·that length). A jump (RC-7) sets φ to the new position's. **A step falls at the first resolution at which ⌊φ⌋ differs from its value at the previous resolution**, so a backward jump (a loop's wrap) that changes ⌊φ⌋ is a step too, and the stepped position takes the current position. Between steps it holds. Stepped is Blend (MO-9) at the stepped position. **Every key reads the stepped position under Stepped: Follow keys, every Structural key's pick (MO-13), and Quantum-mode keys' picks.** Values and picks therefore move in steps, each step's edge glided by MO-5 and PA-1. |

### D.4 Each key's own mode and count (B557); structure in 1.0; what the resolver emits

| id | requirement |
|---|---|
| MO-11 | **The menu.** Every Morphable and Structural key carries two per-patch choices, saved by the morph system and never morphed. **Mode:** Follow (the default: it obeys `morph.law`), Quantum (it flips at its quantum boundaries, MO-7, whatever `morph.law` is) or Exempt (MO-12). **Count:** As the morph (the default: `morph.count`), Two or Four. Count acts only while the key blends: Follow under Blend or Stepped. For a Morphable key it chooses MO-9's count. A Structural key never blends in 1.0, so its Count acts on nothing (ADR-377; what it does after 1.0 is in D.5). |
| MO-12 | **Exempt.** An Exempt key holds corner A's value at every position and never blends or flips. The morph's editor writes the held value into all four corners when a key is made Exempt (ADR-109), so corner A's is the held value. With the morph off it sends, like every key, corner `morph.audible`'s stored value (MO-20), which is the held value wherever the editor has written it into all four corners. Its waypoints and its region lock do not act (MO-23, MO-24). **Exempt concerns the morph only:** the value it holds takes per-voice modulation wherever PA-2 gives its key modulation, as it would under Follow (round 61, ADR-373; GN-40). |
| MO-13 | **Structure follows its quantum boundaries** (round 61, ADR-377; for 1.0 it reverses ADR-317's Blend half). The human, 2026-11-06: "For 1.0, anything that can't blend naturally follows its quantum boundaries." **Every Structural key resolves to one value under every law and every mode but Exempt:** its pick and its latch (MO-7, MO-8; MO-24 for a locked key), read at the stepped position under Stepped (MO-10). So Blend and Stepped blend Morphable keys only. The value changes on sounding voices by PA-5's brief timed crossfade, by PA-6 for `unison` and by VO-14 for `voice.mode`, and at no position do two values of one Structural key sound at once but during that crossfade. A Quantum-mode Structural key does the same, and an Exempt one holds corner A's value (MO-12). A reserved row (5.1) is never emitted, since PA-7 refuses its `Param` (Q-TA-74's reading, ADR-364). This covers the envelope selectors (`oscN.env`, `noise.env`), the pan pair (`panImage`, `panInvert`), every chain's keys and the blades' on switches. **`oscN.ratios`**, each corner's `ratios` text for oscillator N (ADR-326, X40; ADR-327: in the morph system's corner state, RC-11.1), is a Structural key of the morph like any other: two texts are one value when their **parsed** lists are equal, so spacing and spelling (`1, 5/4` and `1,5/4`, or `5/4` and `1.25`) do not matter; a text is at most 256 bytes of UTF-8 (round 48); its pick arrives as the engine's `ratios` input (MO-20). **The exception is the sources' on switches (`osc1.on`, `osc2.on`, `sub.on`, `noise.on`), which fade under Blend and Stepped (MO-26).** Setting sets, in which corners' differing structure would render side by side and mix, are after 1.0 (D.5, D.6). |
| MO-20 | **What the resolver emits** (restated for 1.0 by ADR-377). At each control interval boundary it emits, as needed, a `Param` event for each Morphable key whose glided value changed and for each Structural key whose value changed, and a morph level (IF-12) for each source whose level changed (MO-26). The first resolution after a reset sends every source's level, whatever it is (MO-3). At a boundary where a source lands (MO-26 (4)), its keys' landing `Param` events come before its switch's on `Param`. `oscN.ratios` arrives as the engine's `ratios` text input, the same input IF-4's `ratios` feeds. **When `morph.on` turns off** (ADR-319 item 1), the resolver emits the values of corner `morph.audible`, whatever the position: a `Param` event for every key the morph resolves (the `ratios` texts included), and a morph level of 1 for each source (ADR-123: morph off pins it to 1). A change of `morph.audible` while the morph is off emits the new corner's values the same way. Its Morphable values glide by MO-5 (b), and its Structural values change by PA-5. Otherwise the resolver emits nothing while the morph is off. An FX slot's Structural keys go to the rack (IF-12). How sets and `Param` events were ordered when sets were sent is after 1.0 (D.5). |

### D.5 Structure: after 1.0 (ADR-377)

> **After 1.0.** The human, 2026-11-06: "I don't want any sounds to crossfade except as brief transitions,
> temporally-bound. The whole concept is parametric morph that allows every point to become its own corner preset."
> And: "For 1.0, anything that can't blend naturally follows its quantum boundaries." In 1.0 every Structural key
> follows its quantum boundaries (MO-13), and an oscillator's on switch fades as a level (MO-26). Nothing in D.5 or D.6
> is built or judged for 1.0: no group, no setting set, no `SettingSet` event, no slot. The rows stay as the record of
> the design ADR-317 and ADR-325 ruled, for when waveform morphing returns after 1.0, and their ids stay reserved.

| id | requirement |
|---|---|
| MO-14 | **Structural groups.** The Structural keys that are not flip-only (MO-15) form these groups, each named by its id. Group ids use a `/`, so no id is a manifest key. (1) **By gain:** `oscN/env` (that oscillator's `env`), `noise/env` (the noise's `env`), and `oscN/pan` (`panImage` and `panInvert`). (2) **`oscN/kernel`:** `baseWave`, `stacking`, `spreadLaw`, `quantize`, and every row of 5.4 for both blades. (3) **`oscN/swarm`:** `unison`, `distribution`, `seed`, `detuneLaw`, `detuneRange`, `pivot`, `octave`, `semitone`, and the oscillator's **`ratios` text**, named `oscN.ratios` for the morph (ADR-326, X40: each corner keeps its own; ADR-327: in the morph system's corner state, RC-11.1). Two texts are the same setting when their **parsed** lists are equal: the same count of ratios, each the same number, so spacing and spelling (`1, 5/4` and `1,5/4`, or `5/4` and `1.25`) do not matter. A text is at most 256 bytes of UTF-8 (round 48). `oscN.ratios` carries MO-11's mode and count like any key. If the four corners hold more than one setting of this group's blending keys, the kernel's keys join `oscN/swarm` and `oscN/kernel` does not exist. This is decided over the four corners, so it is fixed for the patch and changes only with a corner edit or a change of mode. It does not change with the position. (4) **`sub/chain`:** the sub's Structural rows but `voiceMode` and `priority`, with `env` included. The sub applies its envelope inside its chain ahead of the drive (SN-3), so `sub.env` cannot act by gain. (5) **`noise/chain`:** the noise's Structural rows but `env` and the reserved rows. (6) **`voice/filter`:** the filter's own Structural keys, when a filter is assigned (RC-1). (7) **`fx/N`:** each FX slot's structure, whose keys the rack defines (RC-5). A **setting** of a group is one value for each of its blending keys (MO-16). |
| MO-15 | **Flip-only keys.** `voice.mode`, `oneShot`, `retrigger`, `sub.voiceMode`, `sub.priority` and every reserved row never blend: they act on the voice law or at a voice's start, where one note cannot hold two values. Each resolves to one value under every law and every mode **but Exempt** (MO-12). That value is its own pick and latch (MO-7, MO-8), at the stepped position under Stepped, so under Blend it is the seeded point of IF-4 before ADR-317. It changes on sounding voices by PA-5, or by VO-14 for `voice.mode`. |
| MO-16 | **Which keys blend.** A key of a group **blends** when its mode is Follow and the law is Blend or Stepped. Every other key of the group resolves to one value (its pick under Quantum, its held value under Exempt), which the resolver emits as a `Param` event. Settings are formed over the blending keys only. |
| MO-17 | **The setting set.** A blending group's candidates are the distinct settings its four corners hold. A candidate's **id** is the lowest corner index (A = 0 … D = 3) holding it. Its **pooled weight** Wₛ is the sum of the glided corner weights (MO-5 (a)) of the corners holding it. The group's **count** n is the smallest Count among its blending keys (As the morph reads `morph.count`; ADR-325), and is 2 for `fx/N` whatever the choice. **At n = 4**, or with at most two candidates, the set is every candidate with Wₛ > 0, at weight Wₛ. **At n = 2** with three or more candidates, the candidates are ordered by Wₛ, highest first, ties broken by the lower id. With W₁ ≥ W₂ ≥ W₃, the set is the first two, at weights (W₁ − W₃)/(W₁ + W₂ − 2W₃) and (W₂ − W₃)/(W₁ + W₂ − 2W₃); where W₁ + W₂ − 2W₃ = 0 (the top three tie), the set is the first alone, at weight 1. A set's weights sum to 1. |
| MO-18 | **Continuity at n = 2.** The two-setting law is continuous wherever the top three pooled weights do not all tie. Where the second and third trade places, both have weight 0. It is discontinuous only at a point where they all tie: the pad's centre when the corners hold four settings, or (0.5, ⅓) when they hold three with C = D. Near that point the weights turn steeply. **A path through that point is a switch there.** With four distinct corners the A–D and B–C diagonals pass through it: that is correct, and it is what the Two setting trades for its cost. The engine's slot rule (SB-11) carries the change. |
| MO-19 | **An FX slot's set** (ADR-324 H10). An `fx/N` group is resolved like any other, with n = 2, so at most two settings of a slot sound at once. Its sets go to the rack, never to the engine (IF-12). How a slot renders its set is RC-5's. |

**MO-20's set half, after 1.0.** As it read before ADR-377: **What the resolver emits, and in what order.** At each control interval boundary it emits, as needed, **first** the `SettingSet` events (IF-12) of the blending groups whose set (key list, values or weights) changed, **then** a `Param` event for each Morphable key whose glided value changed and for each Structural key that resolves to one value whose value changed. A one-value `oscN.ratios` arrives in the `Param` phase as the engine's `ratios` text input, the same input IF-4's `ratios` feeds. So a key that stops blending is already absent from its group's latest set when its `Param` event arrives (PA-7). **The order of several sets at one boundary** is fixed: osc 1's groups, then osc 2's, then `noise/env`, `sub/chain`, `noise/chain` and `voice/filter`. Within an oscillator the order is `oscN/pan`, `oscN/env`, `oscN/kernel`, `oscN/swarm`, except at an un-merge. **A change of merge** (MO-14 item 3, made by a corner edit or a mode change) is sent in an order the engine can accept (PA-7). At a **merge**, `oscN/kernel` first gets a one-setting set (its current heaviest kernel setting, the lower id on a tie), and then comes the merged `oscN/swarm` set, which lists the kernel's keys. At an **un-merge**, the `oscN/swarm` set without the kernel's keys comes first, then the `oscN/kernel` set. A group that stops blending (a mode change, or the corners coming to agree) gets one last set of one setting, which equals `Param` events setting its values. **When `morph.on` turns off** (ADR-319 item 1), the resolver emits the values of corner `morph.audible`, whatever the position: for each blending group a set of one setting equal to that corner's setting, then a `Param` event for every other key the morph resolves (the `ratios` texts included). A change of `morph.audible` while the morph is off emits the new corner's values the same way. Its Morphable values glide by MO-5 (b), and its Structural values change by PA-5. Otherwise the resolver emits nothing while the morph is off. `fx/N` sets go to the rack (IF-12).

**MO-5's set half, after 1.0.** (a) The weights: the four corner weights w are glided together, by the same one-pole as MO-5 (b), before every law, and snap together to their targets once all four are within 10⁻⁶ of them. The glided weights stay in [0, 1] and sum to 1. After 1.0 the setting sets are computed from them (MO-17). **For structure (ADR-325, X31):** at count 4, gliding the corner weights is exactly a glide of the set's weights on the output, since the set's weights are sums of corner weights; at count 2 it is a glide in weight space, because a glide of the output weights is undefined when the pair changes. A setting set's own weights are never glided by the resolver: the engine glides them (PA-1, SB-2).

**MO-11's set half, after 1.0.** A Structural key's Count bounds its group's count (MO-17: the smallest wins, ADR-325).

**MO-25's set half, after 1.0.** **A group that blends at the position** cannot be stored whole: a corner holds one value per key, so it never holds a two-setting sound, and IF-4 is unchanged. The corner then takes the set's heaviest setting (the lower id on a tie), and the morph system reports which groups it could not keep as a mix (ADR-373).

### D.6 Rendering a setting set (the engine's half): after 1.0 (ADR-377)

> **After 1.0**, with D.5: no row here is built or judged for 1.0.

**IF-12's set half, after 1.0.** **The `SettingSet` event**, timed like a `Param` event (PA-3): a group id (MO-14: `osc1/kernel`, `osc1/swarm`, `osc1/pan`, `osc1/env`, the same for `osc2`, `noise/env`, `sub/chain`, `noise/chain`, `voice/filter`, `fx/1`, …); the count n (2 or 4); the group's blending keys, by manifest key, in ascending manifest id order; and up to n entries, each an id (0 to 3, MO-17; a label and a tie-break, SB-11), a weight, and one value for each listed key: v, or for `oscN.ratios` its text. **`fx/N` sets go to the rack (RC-5), never to the engine.** A merged oscillator's `oscN/swarm` set lists its kernel keys too (MO-14 item 3). A key that resolves to one value is never in a set: it arrives as a `Param` event and applies to every setting of its group (SB-8). A `Param` event for a key in its group's latest multi-setting set is refused (PA-7).

**PA-7's set half, after 1.0.** A `SettingSet` event (IF-12) is refused whole, and nothing of it is stored, when it names no group, gives n other than 2 or 4 (or other than 2 for an FX slot), holds more entries than n, repeats an id, names an id outside 0 to 3, lists keys that are not that group's or a key twice, gives a value outside a key's range or options (or a `ratios` text its parser refuses), or gives a weight that is not finite, is negative, or makes the weights' sum differ from 1 by more than 10⁻⁶; or names an `fx/N` group, whose sets go to the rack; or is an `oscN/kernel` set while that oscillator is merged (its latest `oscN/swarm` set lists kernel keys, MO-14 item 3), or an `oscN/swarm` set that lists kernel keys while that oscillator's `oscN/kernel` group is live (its latest received set holds more than one setting; MO-20 sends it one setting first at a merge) (the counter `refused_setting_set`). A `Param` event for a key listed in its group's latest received set, when that set holds more than one setting, is refused (the counter `refused_param_in_set`); "holds" means the latest set received, not settings still fading in slots (SB-11).

**IF-9's set half, after 1.0.** Structure setting-samples: for each voice, each sample and each structural group that renders per setting (every kind but by gain, SB-1), the number of settings rendered beyond the first, kept per group kind, with a swarm instance's members counted as member-samples.

**IF-10's set half, after 1.0.** Per voice and structural group, the heard setting set (each setting's id, values, target and heard weight, its slot, SB-11, and the sample at which it entered that slot, `started_at`: the voice's fresh start for a setting SB-7 loads, else the sample at which SB-11 (b) placed it, so a setting that keeps running keeps its `started_at` (round 61, ADR-373); it counts on the engine's sample clock from reset, PA-3, as a voice's start sample does: the lead's reading, for the test author to confirm), each kernel setting's uncorrected output y⁰ beside the mixed y⁰ that the loop reads (SB-3), and while a swarm blends, the member values above per instance with its setting id (SB-4).

| id | requirement |
|---|---|
| SB-1 | **One render per setting.** For each setting of a group's set, the engine renders the group once. (1) **By gain:** no render. The stage applies Σₛ wₛ·gainₛ, which is the same sound as mixing renders because the stage is linear in the signal. (2) **`oscN/kernel`:** the member kernel (the base wave, both blades and their edge corrections), once per setting, from the oscillator's one member state. (3) **`oscN/swarm`:** a whole oscillator instance per setting (SB-4). (4) and (5) **`sub/chain`, `noise/chain`:** the chain, once per setting, at the points PA-5 names for each. (6) **`voice/filter`:** one filter instance per setting (RC-1). (7) **`fx/N`:** by RC-5. A set is never rendered as a product of groups (MO-14 item 3). |
| SB-2 | **The mix.** A group's output is Σₛ wₛ·yₛ, with equal gain (RS-3.6 of the rack's contract, ADR-324 H11). Each weight is a value glided by PA-1, per voice. The glided weights sum to 1 within 10⁻⁵. |
| SB-3 | **The kernel's one member state.** Every setting of a kernel group renders from the oscillator's one member state, as PA-5's crossfade does. Where the member state reads the output (feedback and cross-mod, C.14), it reads the mixed output Σₛ wₛ·yₛ. |
| SB-4 | **Several swarm instances.** While `oscN/swarm` holds several settings, each setting is a whole oscillator instance, with its own members (SW-1), its own coupling, drift and frame, and its own feedback and cross-mod loop, which reads that instance's output only. (a) Its per-note streams (DT-4) are seeded from that setting's `seed`. The noise oscillator's seed ("the voice's seed", osc 1's) is the `seed` of osc 1's heaviest setting at the voice's fresh start (the lower id on a tie), and is kept for the note. (b) The ensemble's persistent state (DT-4, C.7) is kept once per distinct swarm setting among the four corners (its full tuple of values; at most four per oscillator), each with its own stream seeded from that setting's `seed`; placement (C.2) is computed per instance from its own `unison`, `distribution` and `seed`. An instance uses its setting's state; a corner edit that makes a new setting starts its state fresh. (c) The sources the engine publishes for the oscillator (IF-3) are the mixture's: R·e^{iψ} = Σₛ wₛ·Rₛ·e^{iψₛ}, and the onset burst's level is Σₛ wₛ·levelₛ. (d) The observation view (IF-10) reports members per instance, with the instance's setting id. |
| SB-5 | **A setting at weight 0 costs nothing.** A setting whose heard weight is exactly 0 renders nothing and counts nothing (IF-9). Where a group's set holds one setting, and at every corner, exactly one setting renders. |
| SB-6 | **Waking.** A setting that enters a slot (SB-11) while a voice sounds starts from its fresh-start state, at the weight PA-1 raises from 0. For a swarm that is a fresh start of its members (ADR-324 H9). Its per-note streams are keyed by purpose, note counter, member and (for the swarm) its seed, never by the slot that renders it, so a setting renders the same in any slot. **Each kernel setting holds its own copy of the per-note blade-noise streams** (BL-18), started from the note's seed at the setting's start, so settings never consume each other's draws. |
| SB-7 | **A voice that starts in a blend.** A voice's fresh start loads every setting of each group's current set into its slots, each from its fresh-start state, with its weight set to the target, not glided. The note is the blend from its first sample. **A legato change** (VO-6) keeps every setting running, with its phases and states continuous, as it keeps one setting today. |
| SB-8 | **A flip inside a blend.** When a key of a blending group that resolves to one value (MO-16) changes, every live setting of the group changes with it at once: by PA-5's 5 ms raised-cosine crossfade, except that `octave` and `semitone` change the frequency at once with every phase continuous (PA-5), and `unison` fades members in and out by PA-6. During a PA-5 crossfade, at most twice the group's settings render. A second change waits until it ends, as PA-5 says. |
| SB-9 | **Limits.** At most four settings per group sound, at most two for a group whose count is 2, and twice those during SB-8's crossfade. The engine is sized for this (RT-1). |
| SB-10 | **Corner exactness.** At each corner, and wherever every group's set holds one setting, the output is bit-identical to the same patch with the morph off and those values stored. |
| SB-11 | **The slot rule.** Each voice keeps, per blending group, as many **slots** as the group's count n. At each control interval the engine applies the group's current set as follows. (a) **Matching settings, by value.** An entry matches a slot's setting when their values agree on every key the entry lists, the setting's value for a key being its value in the set that brought it, or, for a key it received since by a `Param` event (a key that has joined the list, Quantum to Follow), that `Param`'s value. `oscN.ratios` compares parsed lists (MO-14). The id is only a label. A matched setting takes the entry's id, and its weight as the target. Where several slot settings match one entry, the one with the largest heard weight takes it (the lower slot on a tie) and the others take target 0; where one slot setting matches several entries, the entry with the lower id takes it. An entry that matches no slot setting is a new setting, and a slot setting that matches no entry takes target 0. So a corner edit that only relabels a setting (A and B hold X with id 0, and A is edited, so X's id becomes 1) keeps it running, with no restart. **A key that leaves a group's key list** (a mode change from Follow) arrives as its `Param` event and applies to every live setting by SB-8; the settings keep their slots and match on the remaining keys, so none re-enters. **A change of merge** (an `oscN/swarm` set whose key list gains or loses the kernel's keys) replaces both groups' settings: every slot setting of `oscN/kernel` and `oscN/swarm` counts as matching no entry, so the old settings fade by PA-1 and the new sets' settings enter by (b) and (c), with no matching across the change. (b) **Entering.** A setting not in a slot enters a **free** slot: an empty one, or one whose setting has target 0 and a heard weight below 10⁻⁴. It starts by SB-6. A freed setting's remaining heard weight r is added to the heard weight of the slot setting with the largest heard weight (the lower id on a tie): each of PA-1's two stages of that setting takes the freed setting's value at the same stage, its first stage rising by the freed setting's first stage and its second by r (round 57, ADR-364), so nothing is lost and it glides back to its target along PA-1's curve. So the weights still sum to 1, and the output steps by under 10⁻⁴ of the two settings' difference (below −80 dB). (c) **Waiting.** While a setting of the set waits for a slot, its weight goes to the slot settings that are in the set, in proportion to their set weights. If no slot setting is in the set, the slot setting with the largest heard weight (the lower id on a tie) takes target 1 and the others 0, so the others empty and free their slots. (d) **A lower count.** When a group's count falls, slots above the new count admit no setting and are removed once freed. So any change of set, including the path through MO-18's point, a corner edit, a chimera that Quantum-mode keys make, and a change of count, law or mode, is a sequence of PA-1 glides, with never more settings sounding than SB-9 allows. |
| SB-12 | **Per voice, and the mono sub.** Sets are the instrument's (one pad). Slots, heard weights and settings' states are per voice. The mono sub (SN-7, RC-6.6) keeps its `sub/chain` slots once for the instrument. |
| SB-13 | **A voice's life while an envelope selector blends.** While a source's `env` group (`oscN/env`, `noise/env`) or the sub's chain holds several settings, the source follows each envelope its settings name, and each counts among the voice's governing envelopes (6.4) while its setting's heard weight is above 0. So the voice is freed only when every such envelope has finished (6.4's rule), and an envelope whose setting has faded to weight 0 no longer holds the voice. |

### D.7 Waypoints, region locks and Save current sound (round 61, ADR-373)

A **waypoint** (a pin) is a value pinned at a pad position, so a key's blend passes through it (B235, B272). A **region
lock** keeps the shape of the regions a key's corners own under its pick (B269). Both are per key and per patch, saved by
the morph system (RC-11.1), and never morphed. The human's rulings of 2026-11-06 fix their limits, their dormancy and
what a lock keeps. Rows MO-21 to MO-24 are the resolver's; MO-25 states what the resolver supplies to Save current
sound and what the morph system writes.

| id | requirement |
|---|---|
| MO-21 | **Waypoints: what a key holds** (round 61, ADR-373; GN-7). Only a Morphable key whose law is not `lin, wraps` holds waypoints. A circular key (law `lin, wraps`), a Structural key, a Device key and `oscN.ratios` hold none (ADR-373). A waypoint is a position (x, y) in [0, 1]² and a value in the key's normalised form v, in [0, 1]. **A key holds at most 8 waypoints, and the patch at most 64 across all its keys, dormant ones included (MO-23), stored in order.** The order is part of the patch: it breaks TIN·Δ's ties and fixes TPS·Δ's arithmetic (MO-22). Each waypoint lies at least 0.04 (Euclidean, in pad units) from every corner and from every other waypoint of its key (B235's editor: nearer than that, the value is the corner's or the other waypoint's). A key's waypoints reach the resolver as one ordered list (IF-12), which replaces the key's previous list. As PA-7 checks host values, a position or value outside [0, 1] is clamped to it, and the spacing below is then checked on the clamped positions (the editor lab clamps a pin's position before its spacing test). **The list is refused whole, and the key keeps its previous list, when** any position or value in it is not finite, it names a key that cannot hold waypoints (above), holds more than 8 waypoints, places one nearer than 0.04 to a corner or to another of the list, or would bring the patch's total above 64. So a ninth waypoint on a key, or a 65th in the patch, is refused, and nothing changes. **The patch's waypoint law** is TPS·Δ (the default) or TIN·Δ, and its **Clamp to set values** is on (the default) or off (B272; GN-7). Both are page choices of the morph system, like MO-11's mode and count: saved with the patch, never morphed, and **never host parameters** (ADR-373), so they are not D.2 settings and IF-11 does not list them. |
| MO-22 | **The waypoint law** (B235, B272; the Δ form). For a key with corner values v[A], v[B], v[C], v[D] and n ≥ 1 waypoints (xⱼ, yⱼ, vⱼ), j = 1 … n in stored order, at the position (x, y), unglided, as MO-9 reads the weights: **(1) The base** b(x, y) = Σc w[c]·v[c], MO-9's count-4 blend at MO-4's weights of (x, y); for a source's key while MO-26 (2) applies at (x, y), that on-corner blend instead (the noise's on-or-erosion blend, MO-26 (7); ADR-377 A1: the pins bend the on corners' blend; the editor lab's block W gates its base the same way, at the same 10⁻³). **(2) The nodes** are P₁ = A (0, 0), P₂ = B (1, 0), P₃ = C (0, 1), P₄ = D (1, 1) and P₄₊ⱼ = (xⱼ, yⱼ). Each carries a **residual** r: 0 at a corner, and vⱼ − b(xⱼ, yⱼ) at a waypoint. **(3) TPS·Δ:** R(x, y) = a₀ + a₁·x + a₂·y + Σᵢ ωᵢ·φ(‖(x, y) − Pᵢ‖) over all n + 4 nodes, with φ(d) = d²·ln d (natural log) and φ(0) = 0, where ω and a are the unique solution of Σᵢ ωᵢ·φ(‖Pₖ − Pᵢ‖) + a₀ + a₁·xₖ + a₂·yₖ = rₖ at every node k, with Σᵢ ωᵢ = Σᵢ ωᵢ·xᵢ = Σᵢ ωᵢ·yᵢ = 0 (the thin-plate spline through the residuals). **(4) TIN·Δ:** R is linear on each triangle of a triangulation of the nodes: in the triangle that holds (x, y), the residuals of its three vertices weighted by the barycentric coordinates of (x, y). The triangulation starts as the triangles (A, B, D) and (A, D, C), split along A–D, and takes the waypoints one at a time in stored order (Bowyer–Watson): every triangle whose circumcircle holds the waypoint strictly inside is removed (the test below), and the waypoint is joined to each edge of the hole's boundary (an edge of exactly one removed triangle), except an edge it lies on, which forms no triangle. A waypoint on a circumcircle, not inside it, removes nothing, so a tie goes to the earlier waypoint. On an edge two triangles share, both give the same value. **The tests, in double precision as the editor lab computes them:** every triangle is stored counter-clockwise, orient(a, b, c) = (bₓ − aₓ)(c_y − a_y) − (b_y − a_y)(cₓ − aₓ) > 0 (the first two are, with A = (0, 0), B = (1, 0), C = (0, 1), D = (1, 1)); with a′ = a − p, b′ = b − p, c′ = c − p, the point p is strictly inside the circumcircle of (a, b, c) when |a′|²·(b′ₓc′_y − c′ₓb′_y) − |b′|²·(a′ₓc′_y − c′ₓa′_y) + |c′|²·(a′ₓb′_y − b′ₓa′_y) > 10⁻¹³; a boundary edge (a, b) and the waypoint p form a triangle only when |orient(a, b, p)| > 10⁻¹⁴, stored as (a, b, p) when orient > 0 and as (b, a, p) otherwise. **(5) The raw value** is u(x, y) = b(x, y) + R(x, y). Under both laws R is 0 at every corner and rⱼ at every waypoint, so u passes through every waypoint. **(6) The value.** At the four exact corners it is the corner's own value v[c], with no clamp, so a waypoint never changes a corner. Elsewhere, with Clamp to set values on, u is clamped to [lo, hi], the smallest and the largest of its waypoint values and the corner values of the corners its base ranges over: all four, or for a source's key while MO-26 (2) applies its on corners, and for the noise's its on or erosion corners (MO-26 (7)); then, under either choice, it is clamped to [0, 1]. MO-5 (b) glides it, and MO-20 emits it as the key's value. A key with no waypoints is MO-9's blend, bit for bit. **(7) When it is computed** is the builder's. The coefficients change only when a corner value, the key's waypoints, the law or the clamp changes, never with the position, and computing them is within MO-3: nothing allocated after prepare, and the output independent of block size. |
| MO-23 | **When waypoints act** (round 61, ADR-373; GN-7, GN-29). A key's waypoints act only while the morph is on, `morph.law` is Blend, the key's mode is Follow and its count is Four, so that it blends all four corners (MO-9). **Under Quantum or Stepped they are dormant: kept and unused, and they act again when the law returns to Blend.** So under Stepped a Follow key steps through MO-9's blend at the stepped position, without its waypoints. A key whose own mode is Quantum keeps its waypoints dormant the same way under every law (MO-11, MO-13), and they act again when it returns to Follow under Blend. **Under Exempt they are dormant too: kept and unused, and they act again when the key returns to Follow under Blend** (MO-12; ADR-373). **At count Two they are dormant**, and the key plays MO-9's two-value blend without them; they act again at count Four (ADR-373). With the morph off they do not act (MO-20). A dormant waypoint still counts toward MO-21's limits. A corner edit changes no waypoint: each keeps its position and its value, so the key still passes through it, and its residual follows the new base (MO-22 (2)). Waypoints starting or stopping to act change the key's target value, which MO-5 (b) and PA-1 glide. |
| MO-24 | **The region lock** (B269; round 61, ADR-373; GN-6). A key that picks (MO-7: every Structural key in 1.0, MO-13, except a source's on switch under Blend or Stepped, MO-26; a Morphable key under Quantum or whose mode is Quantum) gives each corner the **region** of the pad where that corner wins its pick. **A region lock keeps the shape of those regions, and nothing else.** It is three finite numbers, the key's **offsets** δ[B], δ[C] and δ[D], with δ[A] = 0, each of magnitude at most 10⁶: a lock with a larger or a non-finite offset is refused whole, as PA-7 refuses, and the key keeps its previous lock. A locked key scores each corner sₖ[c] = ℓ[c] + δ[c], where ℓ[c] = ln w[c] if w[c] > 10⁻⁹ and −10¹² otherwise, at the weights MO-7 reads (the stepped position's under Stepped); its pick is the corner with the highest score (the lowest corner on a tie). `morph.temperature`, `morph.cohesion`, `morph.seed` and the draws (MO-6) do not reach it, so reseeding, or moving Temperature or Cohesion, moves no locked region (B269). A locked Structural key's latch (MO-8) is a margin of ln(0.55/0.45) on these scores: the band of 0.1 of the pad on an edge where two corners' offsets are equal. **Locking as is** takes δ[c] = T·(oₖ[c] − oₖ[A]), where oₖ[c] = (1 − h)·gₖ[c] + h·g*[c] is the key's draw term and T and h are MO-7's at that moment, so the key picks the same corner at every position before and after (the resolver supplies these offsets on request, IF-12). How the editor turns a boundary drag into offsets is the editor's (B269). **The lock keeps no value:** a corner edit changes no lock, so the regions stay where they are, and the key emits the edited corner's new value wherever that corner's region holds the position (round 61, ADR-373). A lock acts wherever its key picks. Where its key blends (a Morphable Follow key under Blend or Stepped), or is Exempt, it does not act and is kept: **making a key Exempt keeps its lock dormant, and it acts again when the key picks once more** (ADR-373). |
| MO-25 | **Save current sound** (ADR-319 item 1; round 61, ADR-373; GN-35). The morph system's Save current sound into a corner stores what is heard: every key, blended at the position, with the offsets baked in, so that the corner alone sounds as the position did. **The resolver supplies the values** on request (IF-12), changing nothing: for each Morphable key and each Structural key, its **resolved value**, the value MO-4 to MO-24 and MO-26 give it at the current position under the law, its mode and count, its waypoints and its lock (with the morph off, corner `morph.audible`'s, MO-20), and for `oscN.ratios` its text. In 1.0 every Structural key has one value (MO-13), so a corner can hold exactly what is heard of it (Save during a structural blend, after 1.0, is in D.5). **A Morphable value is the glided value heard now**, the one last emitted after MO-5 (b), not its target (ADR-373). **The morph system writes them:** to each value it adds the offsets the macros and the pad's X and Y intents give that key (the intent bus's, outside this spec), and writes the result into the corner in place of its value. Device keys are not written. **Only the macro and X and Y offsets are baked:** route modulation (an LFO's or an envelope's contribution) is not frozen into the corner (ADR-373). **A source fading** (MO-26, its switch on) is heard at its morph level g, which no key holds, so Save bakes the g last sent (round 61, the lead (critic S5)) into the corner's level key (ADR-377 A1): the key takes the v whose gain, by its dB law, is the heard gain (with its offsets baked) times that g, so the corner sounds as the position did. **Where that gain falls below the law's −60 dB, Save stores the source off** (ADR-377 A2: "The source off"): the corner's switch is off, and its level key takes the heard gain without g. **An Exempt key** (one value in all four corners, MO-12) whose offsets move it takes the pushed value in all four corners, so the four never part (ADR-373). Once the corner changes, the sound at the position changes with it, unless the position is that corner. |

### D.8 A source's on switch (round 61, ADR-376, ADR-377 A1)

The human, 2026-11-06, of the legacy rule: "it fades in, but with the values of the corner it's headed to instead of
the arbitrary values of the off-oscillator in the corner it's coming from." The legacy rule is three records: the level
ramp (ADR-123, B48, built 2026-08-26), the human's "weighted average of only the on corners" (B65, 2026-08-27), and its
build as the off-corner blend rule (ADR-183, B232, 2026-09-24). The human, 2026-11-06 (ADR-377 A1): the floor as
legacy; a fading source's Structural keys pick among its on corners; Save bakes its level; its waypoints bend its on
corners' blend; "Sub and noise too"; and under Quantum the sources switch. And (ADR-377 A2): a landing skips PA-1's
glide too ("Skip it"); Save below the floor stores the source off; the noise's keys range over its erosion corners as
well; and Save bakes the g last sent.

| id | requirement |
|---|---|
| MO-26 | **A source's on switch: it fades under Blend and Stepped, and switches under Quantum** (round 61, ADR-376; ADR-377, A1 and A2). The **sources** are osc 1, osc 2, the sub and the noise. Each has its **switch** (`osc1.on`, `osc2.on`, `sub.on`, `noise.on`), its **level key**, the manifest's gain row for it (`mix.osc1`, `mix.osc2`, `mix.sub`, `mix.noise`; 5.2), and its **keys**: every key whose manifest key begins with its prefix (`oscN.`, with 5.4's blade rows and `oscN.ratios`; `sub.`; `noise.`), and its level key. A source's **on-weight** W_on = Σc w[c]·on_c is the summed weight of the corners where its switch is on, at MO-4's unglided weights (the stepped position's under Stepped, MO-10). The **floor** is W_on = 10⁻³, about −60 dB (ADR-123, ADR-183; ADR-377 A1). **(1) The fade, under Blend and Stepped** (the switch's mode Follow). The resolver sends the source a **morph level** g = W_on (IF-12). It is a parameter the resolver computes and the engine takes as an input, not a manifest row: ADR-123 rejected writing the ramp into the level key, whose own morph target it would fight, and a row would be a host parameter and saved state, which a morph output is not. The engine applies g as one more factor on the level key's gain, glided by PA-1 only, as the legacy shell's smoother was, so the source fades and never flips in a blend. For the noise that factor comes after the point the sub's Erode reads (SN-14), so g never reaches Erode. The switch resolves to on where W_on is above the floor, read from the same unglided W_on as g's target, and the engine turns it on at once (PA-5). **The wait for off is the resolver's** (ADR-377 A2, critic NB1). Per source, it keeps a mirror ĝ of the engine's heard g, two stages s₁ and s₂ (ĝ = s₂) stepped once per host sample on its own clock (MO-3), at f_host whatever the quality (High's 2× engine rate included), by PA-1's law exactly: with τ = 6 ms, h = 1/(f_host·τ) and p = e^(−h), the errors to the g last sent, e₁ = s₁ − g and e₂ = s₂ − g, go e₁ ← p·e₁ and e₂ ← p·(e₂ + h·e₁), and ĝ snaps to g once within 10⁻⁶ of it; each g it sends takes effect in the mirror at its event's sample, as in the engine, and prepare and reset set both stages to 1. Where W_on is at or below the floor, the resolver sends the switch's off `Param` at the first control-interval boundary at which ĝ is at or below the floor, and not before; the engine applies it at once by PA-5. So a source never cuts out while its heard level is above the floor, and the wait is deterministic and independent of block size (DT-2). Its count does not matter: it has two values. **(2) Its keys follow its on corners.** While W_on is above the floor and some corner that carries weight has the switch off, the source's Morphable keys blend only over the corners where it is on, with the weights w[c]·on_c / W_on (a circular key's mean likewise; at count Two the candidates are the on corners' values, each pooled weight divided by W_on before MO-9's two-value law); its Structural keys pick only among the corners where it is on, an off corner scoring −10¹² in MO-7 (and in MO-24 for a locked key); and its waypoints bend that on-corner blend, clamped to the on corners' hull (MO-22 (1), (6)). So an off corner's stored values never sound. Where no weighted corner has it off, all of this is MO-7, MO-9 and MO-22 unchanged, bit for bit. **While the resolver waits** to send the switch off (1), the source's keys hold the values they last took under this clause (critic NB2). Once the switch is off, its keys take the plain blend and plain picks (ADR-183's fallback), so a corner where it is off reads back its own values once all has settled (MO-27). **(3) Under Quantum** (the law, or the switch's own Quantum mode under any law): the switch flips at its quantum boundary by PA-5, like any Structural key (MO-13), and g is 1 (ADR-377 A1). While it is on and some weighted corner has it off, its keys pick only among the corners where it is on, as in (2). **(4) Landing.** When a source comes back on, under any law (the switch's on `Param` after it was off, from a fade or a Quantum flip), every one of its keys takes its new value outright at that boundary, with no glide and no crossfade, since what it held was never heard (B232's landing; under Quantum, the defect B250 recorded; ADR-377 A2: "Skip it"). The resolver skips MO-5 (b)'s glide for them and marks their `Param` events as **landing** (IF-12); the engine applies a landing `Param` at once, without PA-1's glide, PA-5's crossfade or PA-6's fades (the sub's `order` included), ahead of the switch's own crossfade in, which those events precede at the boundary (MO-20). **(5) Each switch's own mode.** An Exempt switch holds corner A's value (MO-12), and its on-weight reads corner A's switch for every corner, whatever the others store: W_on is 1 everywhere if A has it on and 0 if not, so g is 1 where it is on, no corner counts as off, and (2) changes nothing. A Quantum-mode switch follows (3) under every law, and under Blend or Stepped (2) still restricts its source's Morphable keys to the on corners while it is on. An Exempt key of the source holds its one value (MO-12). **(6) Morph off.** g is 1 (ADR-123), and the switch is corner `morph.audible`'s (MO-20). **(7) The noise's exception: its erosion corners** (ADR-377 A2). The human: "noise settings influence erosion even if the noise osc is off … This is the exception to the off-oscillator blend rule." The noise feeds the sub's Erode while it is off (SN-4, SN-14), so its keys range over more corners than its switch does. A corner is an **erosion corner** where its `sub.on` is on and its `sub.erode` is above 0 or is the destination of a modulation route in that corner (ADR-377 A2; the human: "any corner where noise is influencing the erosion of its respective sub"; B637: "the destination of a modulation route in that corner" is the modulation system's erosion flag for that corner, MS-44, round 62, which IF-12 takes). For the noise's keys, (2) and its waiting and fallback use the corners where the noise is on **or** that are erosion corners in place of its on corners, with their summed weight W_E in place of W_on (the weights w[c]·e_c / W_E, the picks, the waypoints' base and hull, and the floor test on W_E); a corner with neither is ignored. The noise's level g still follows W_on, and its switch the floor on W_on, as in (1). When the noise is off in every corner, so that it sounds only through Erode, and W_E falls to the floor, its keys hold their last values until the resolved `sub.erode` has settled (MO-5 (b)'s snap to its target), and only then take the plain blend (round 61, the lead). |

## Appendix E. The modulation system (normative)

The **modulation system** computes the global modulators, evaluates every route, and hands the engine per-voice
offsets (PA-2), the per-sample route terms of E.2's engine path (MS-41), and the resolver an offset to the morph
position. It is built by the clean room beside the engine and the resolver (round 62; ADR-371, ADR-372, ADR-375,
ADR-379; B633, B637). This appendix states its sources, its routes and the two paths they take, its modulators and their
laws, where it meets the morph, how loops are bounded, and what it saves. Rows MS-1 to MS-45 are the system and the
matrix; LF-1 to LF-14 the LFOs; MK-1 to MK-5 the Kuramoto LFO; MB-1 to MB-11 ORBITAL; MR-1 to MR-10 the 2-D random.
Items 36 to 39 (10.1) test them.

**Notation.** s is a source's value in [0, 1] (bipolar sources: 0.5 at rest); y = 2s − 1 for a bipolar source; v a key's
normalised value (5.1); Δ the control interval (PA-3), in samples; f the host rate. A **modulator** is an LFO, the
Kuramoto LFO, ORBITAL or the 2-D random. A **route** is one record of the matrix (MS-13). **The modulation manifest**
holds the modulators' settings (E.5 to E.8), as the morph's manifest holds D.2's. A **source's keys** and its
**on-weight** are MO-26's.

### E.0 Who computes what, and when

| id | requirement |
|---|---|
| MS-1 | **Three parts** (RC-4; MD-1, MD-3; ADR-371; ADR-379 r2). The engine computes ENV 1 to ENV 4 and the per-note LFOs (LF-2), and evaluates the routes of the **engine path** (MS-41). The resolver resolves the corners, the modulation manifest's keys among them (MS-27), and answers every pick the modulation system needs (MS-25). The modulation system computes the global modulators (the global LFOs, the Kuramoto LFO, ORBITAL, the 2-D random), reads the sources (MS-8, MS-9), and evaluates every route of the **outside path**. It sends: to the engine, per-voice offsets in v addressed by note id (PA-2), an offset for every voice being the same mechanism with every voice addressed, each Control-carrying key's (1 − W) (MS-20), and the engine path's route terms (MS-41); to the resolver, an offset to the morph position (MS-29) and each corner's erosion-route flag (MS-44); to the pitch system, voice pitch's quantized contribution (MS-22); to the rack, the offsets of the FX modules' declared mod values (MS-12). It computes no envelope of ENV 1 to ENV 4 and implements no part of MO-6 to MO-8. It meets the others only through IF-12 and IF-13. |
| MS-2 | **Clock and cadence.** It keeps a sample clock of its own, advanced by the samples it is asked to process and counted from its reset exactly as PA-3 counts the engine's, so its control intervals coincide with the engine's and the resolver's (MO-3). At the first sample of every control interval it (1) applies every input whose sample time is at or before that sample; (2) advances every global modulator to that sample (a modulator with its own tick, MK-3 and MR-2, runs its ticks on this clock); (3) evaluates every outside-path route there, and every engine-path route's terms; (4) emits each changed offset and term, timed at that boundary. **Every value it reads at a boundary is held from that boundary until the next**, and the engine ramps each offset and term across one interval (PA-3). An input takes effect at the first boundary at or after its sample time, except a note-on (MS-42). |
| MS-3 | **Reading sources on the outside path** (T3; ADR-379 r2). An outside-path route whose source a voice holds (ENV 1 to ENV 4, velocity, a per-note LFO, R, ψ, the onset burst, gravity's offset) reads the value at the end of the previous control interval, held from the boundary (MS-2): it acts one control interval late (29 samples at 44.1 kHz, 0.66 ms). The engine publishes those values at each boundary (IF-3). The note's velocity and expressions are read from the shell's note events, as the engine reads them (IF-1, MS-9). |
| MS-4 | **The outside path is control rate** [X60]. Every offset of the outside path is a control-rate offset, ramped by PA-3, on PA-4's keys too: no global modulator runs above 40 Hz (LF-4), so a ramp at 1,500 per second is a linear interpolation of it (T7), and evaluating the global modulators per sample would cost about three times the system's CPU slice (MD-54). PA-4's per-sample offset input is not used by the modulation system in 1.0; the engine path (MS-41) is how a per-note source reaches a per-sample key. |
| MS-5 | **Determinism** (charter; DT-1, DT-2, DT-5; MD-5). Every random draw comes from a mulberry32 stream whose 32-bit state starts at MO-6's FNV-1a hash of a **purpose**, then any further bytes a row names, then a 32-bit seed, all least significant byte first, stepped as MO-6 states. Each purpose begins `mod/` and holds a `/`, so no manifest key can produce it, and no draw depends on a key's or a modulator's position in any list, so appending a modulator, a route or a point never changes an existing patch (ADR-185). Draw k (k = 0, 1, …) of a stream is its (k + 1)-th output, read by index: the state after k + 1 steps is h + (k + 1)·0x6D2B79F5 modulo 2³², so draw k needs no earlier draw (the modulator lab's identity, B237). u is the output / 2³². Tempo and position come only from RC-7's events. No clock is read. The same state, inputs and event stream give identical offsets and terms, sample for sample, and they are bit-identical however the host splits the blocks (DT-2). |
| MS-6 | **Real-time safety and time constants** (RT-1; MD-6; ADR-009). Processing, every input and reset allocate nothing, take no lock and make no system call. Prepare allocates for the maxima: 64 voices, 4 LFOs (each per voice, in the engine, when per note), 32 points per shape, 64 routes and their per-voice terms, the Kuramoto LFO's 8 phases, ORBITAL's 8 bodies, the 2-D random's state. Every time constant is stated in seconds and turned into a coefficient per step of the clock it runs on (per sample, per 16-sample tick or per control interval), never stated per tick. |
| MS-7 | **Reset and load.** Reset sets the clock to 0 and every modulator to its fresh state: each global LFO to its start phase and draw 0, the Kuramoto LFO's phases to MK-2's, ORBITAL to its home (MB-8), the 2-D random to its replay (MR-9), every route weight to its target unglided (MS-25), every loop read (MS-34) to the value its source has at the first boundary. After reset it runs exactly as a freshly prepared system with the same inputs (DT-6). A load resets it (IF-4). A restore (RC-11.4) is a change, not a load: every modulator keeps running. |

### E.1 Sources

| id | requirement |
|---|---|
| MS-8 | **The sources.** The table below lists every routable source in 1.0, its polyphony, its value s and the polarity a new route from it starts with (MS-13). A per-voice source reaches only its own voice's offsets. A **global** source reaches every voice's alike. (ADR-371 MQ11, MQ12, MQ13; ADR-371 A1; ADR-375 A1; ADR-379; IF-1, IF-3.) |

| source id | what | polyphony | s | new route starts | ruling |
|---|---|---|---|---|---|
| `env1` … `env4` | the envelope's level, **before its velocity factor** [X58] | per voice | its level (C.12) | unipolar | IF-3, EN-5; T1 |
| `vel` | the note's velocity, from its note-on (IF-1), held for the note; in mono and legato the newest held note's, ramped over 5 ms as VO-8 ramps it | per voice [X42] | velocity, 0 to 1 | unipolar | IF-1; VO-6, VO-8 |
| `key` | keytrack: k = clamp((n − 60)/48, −1, 1), n the note's key (IF-1) | per voice [X42] | (k + 1)/2 [X41] | unipolar [X44] | ADR-371 MQ11; B82 |
| `wheel` | the mod wheel (MIDI CC 1), c/127 | global | c/127 | unipolar [X44] | ADR-371 MQ11 |
| `bend` | the pitch bend, b in [−1, 1] (14-bit, centre 0); it still bends pitch (RC-3) | global | (b + 1)/2 | bipolar [X44] | ADR-371 MQ11, A1 |
| `pressure` | channel pressure (aftertouch) | every voice on its channel (T9) | 0 to 1 | unipolar | IF-1; X21 |
| `mpe.pressure` | MPE per-note pressure | per voice | 0 to 1 | unipolar | ADR-371 MQ11; IF-1 |
| `mpe.slide` | MPE slide, CC 74 (CLAP's brightness expression) | per voice | 0 to 1 | unipolar | ADR-371 MQ11; IF-1 |
| `mpe.bend` | MPE per-note bend: the note's tuning expression t in semitones (IF-1) | per voice | clamp(0.5 + t/(2·B), 0, 1), B the member channels' bend range [X75] | bipolar [X44] | ADR-371 MQ11, A1; B504 |
| `mpe.lift` | MPE lift: the note's release velocity (IF-1's note-off), 0 until its note-off, then held [X43] | per voice | 0 to 1 | unipolar | ADR-371 MQ11; IF-1 |
| `expr.vibrato`, `expr.expression` | CLAP's vibrato and expression expressions | per voice | 0 to 1 | unipolar | IF-1 |
| `oscN.R` | R of oscillator N's swarm | per voice | R | unipolar | IF-3; Q12 |
| `oscN.psi` | ψ of oscillator N's swarm **relative to its frame**: d = the wrapped difference ψ/2π − Φ in (−½, ½] (C.13's d), so it does not turn at the note's frequency | per voice | d + ½, circular [X59] | bipolar | IF-3; C.13; Q12; T2 |
| `oscN.onset` | the onset burst's level | per voice | clamp(level, 0, 1) [X59] | unipolar | IF-3; Q12; T2 |
| `oscN.gravity` | gravity's settled offset, g cents | per voice | clamp(0.5 + g/70, 0, 1), 35 c the basin (Q31) [X59] | bipolar | IF-3; C.6; T2 |
| `lfo1` … `lfo4` | LFO 1 to 4, output y in [−1, 1] (E.5) | per LFO: global or per voice (LF-2) | (y + 1)/2 | bipolar | ADR-371 MQ1, MQ2, MQ13 |
| `kuro.v1` … `kuro.v8` | the Kuramoto LFO's voice outputs yᵢ (E.6); a voice above the count reads yᵢ = 0 | global | (yᵢ + 1)/2 | bipolar | ADR-375 |
| `kuro.R` | the Kuramoto LFO's coherence R | global | R | unipolar | ADR-375 |
| `orb.b1.x` … `orb.b4.accel` | ORBITAL's bodies 1 to 4, each `x`, `y`, `speed`, `accel`, `near`, `angle` (MB-6); `angle` is circular | global | 0 to 1 | unipolar | ADR-375 A1; ADR-379 r5 |
| `orb.energy`, `orb.spread` | ORBITAL's field (MB-6) | global | 0 to 1 | unipolar | ADR-375 A1 |
| `xy.x`, `xy.y` | the 2-D random's point (E.8) | global | its coordinate | unipolar [X74] | ADR-375; [X53] |

| id | requirement |
|---|---|
| MS-9 | **The keyboard's sources are read from the shell** (MD-13; ADR-379 review). The shell hands the modulation system the note events (with velocity and release velocity), the expressions, CC 1, the bend and channel pressure, as it hands the engine (IF-13); the engine publishes none of them as a source, and IF-3 publishes no keytrack. The wheel and the bend act on every voice, channel pressure on the voices of its channel. **Release velocity is a source only as MPE lift** (ADR-371 MQ11). |
| MS-10 | **Macros, X and Y** are the intent bus's. Their binding to a manifest key reaches the engine as the key's resolved value, never as an offset (MD-7; RC-11.1). The modulation system reads a macro's value, or X's or Y's, only as a route's source into a destination outside the engine's manifest: a modulator's setting or another route's depth (MS-12). The page lists them beside the modulators (GUI-9). |
| MS-11 | **Aftertouch's default route** stays X21's: in the init patch and every factory preset, unless the preset says otherwise, `pressure` and `mpe.pressure` to `coupling` on both oscillators, unipolar, depth 0.125, linear, in every corner. Velocity's default mapping stays each envelope's `velocity` row (VO-8), not a route (MD-11). |

### E.2 Routes

| id | requirement |
|---|---|
| MS-12 | **Destinations** (ADR-371 MQ8; ADR-379 r3; MD-16, MD-19). A route may move: (a) every continuous row of the engine's manifest (PV yes, 5.1); (b) a continuous setting of a modulator (E.5 to E.8: the rows marked M); (c) an FX module's declared mod value (RC-5; ADR-330, B591: how they draw is the GUI's design item); (d) the morph position's x or y, from a global LFO or the 2-D random only (MS-29); (e) another route's depth (MS-17). It may not move a stepped row, a reserved row, a morph setting of D.2 other than the position, or a stepped modulator setting. Each route keeps a destination-kind field (engine key, modulator setting, FX value, morph position, route depth), so a stepped kind can be added after 1.0 without a format change (MD-17). |
| MS-13 | **A route** (ADR-266 r2; MD-20 to MD-22, MD-29, MD-30). One record: its source, its destination, its **depth** d in [0, 1] (a magnitude; ADR-168's signed depth is superseded), its **polarity** (unipolar, bipolar, inverse or Control), its **curve** (linear, exponential or logarithmic), a Control's two ends lo and hi in [0, 1], and on a pitch destination its Quantize switch. A new route starts at depth 0.25, linear (ADR-371 A1), with Quantize off, and at its source's natural polarity (MS-8). |
| MS-14 | **The curve** (ADR-371 MQ9) [X61]. Applied to s before the polarity: linear c(s) = s; exponential c(s) = s²; logarithmic c(s) = 1 − (1 − s)². On a bipolar polarity it acts on the magnitude of y = 2s − 1, symmetrically: the term reads sgn(y)·c(\|y\|), so a curve never moves a bipolar source's rest. |
| MS-15 | **The contribution m(s)** of a route, in the destination's v (ADR-266 r2; MD-21): unipolar d·c(s); bipolar d·sgn(y)·c(\|y\|); inverse −d·c(s). **Control** (MD-22; GUI-17): the source sweeps the destination between lo and hi, lo + (hi − lo)·c(s), and the knob's own value is ignored while it acts (MS-20). At most one Control route per destination in a corner. |
| MS-16 | **One route per pair** (MD-24). In one corner, at most one route per (source, destination); a second is refused, and mapping an already-mapped source only selects its route. A route's identity is (source, destination) in its corner (MD-51, B72). |
| MS-17 | **Depth wires** (ADR-371 MQ7; ADR-372 r2; MD-28). A route's destination may be another route's depth, one level only: nothing lands on a depth route. The depth the route plays is d_eff = clamp(d + Σ the depth routes' contributions m(s) (MS-15, Control excluded), 0, 1), each depth route in the same corner as the route it moves; so a depth wire adds to the depth and stops at zero, and it never turns the modulation upside down. Deleting a route deletes its depth routes. Depth routes run on the outside path. |
| MS-18 | **The route cap** (ADR-371 MQ14): 64 routes, counted as distinct (source, destination) pairs over the union of the four corners and the patch-wide routes (MS-19) [X62]. A route that would exceed it is refused. |
| MS-19 | **Where routes live** (ADR-371 MQ17; ADR-372). Each corner holds its own set of routes (E.3). Routes into the morph position are **patch-wide**: they belong to no corner, since the position chooses the corners (the corner-mapping lab's law) [X64]. |
| MS-20 | **Summing a destination for a voice** (ADR-371 MQ10; MD-26, MD-27; the lead, M2 (b)). For an engine key, with b̃ the resolved value b (the morph's, with the macros and X and Y already applied by the intent bus) as PA-1 has smoothed it in the engine, W = min(1, the weighted sum of the Control routes on the key, MS-24), P the weighted sum of their Control positions and A the weighted sum of every other route's contribution, the value heard is **clamp((1 − W)·b̃ + P + A, 0, 1)**, wrapped on a circular key, then mapped by the key's law (PA-2). The modulation system sends P + A as the offset (with the engine path's terms, MS-41, added in the engine) and (1 − W) as the key's Control input (PA-2), so a morph move of b under a Control route moves nothing (item 36.4). The clamp is once, after the sum, so opposing routes can cancel near an end. A knob's end is a wall: a macro past it does nothing more and no headroom is stored. A modulator's setting, an FX value and the morph position sum the same way in the modulation system, with b unsmoothed, clamped to [0, 1] in their own v. |
| MS-21 | **Rate of a destination** (MD-18). A route into a row of rate C acts at the control rate (MS-4) or by the engine path (MS-41); a row of rate N reads its offset once, at a voice's start (§5.1), which MS-42 makes that note's own. |
| MS-22 | **Pitch quantize** (ADR-290; MD-30; RC-3; the critic's review). Each route into a pitch destination (voice pitch, each oscillator's `pitch` and `fine`, the sub's `pitch` and `fine`) carries Quantize, and a quantized route runs on the outside path [X81]. While any route into a destination has Quantize on, the sum of every route into it is snapped with the pitch system's quantizer (the global scale, the glide quantizer's hysteresis and minimum step time; ADR-290): **for voice pitch**, the pitch system takes the routes' contribution in semitones and the snapped pitch reaches the engine through IF-2; **for any other pitch key**, the snapped total reaches the engine as that key's per-voice offset (PA-2), so IF-2 carries voice pitch only (T8). While none has, they are plain offsets. |
| MS-23 | **Routes are not host parameters** (MD-32). Making, removing, or changing a route's depth, polarity, curve, ends or Quantize is one undo step per gesture. Removing a modulator removes its routes as one undo step, with a line naming them and Undo (MD-50). A modulator with no route keeps its settings until it is removed (ADR-371 MQ18). A route the Random page draws (ADR-374 r5: ENV 4 often to pitch; under ADR-342's lean toward simpler patches) is an ordinary route, and the draw is one edit. |
| MS-41 | **Two paths, chosen by rule** (ADR-379 r2, the critic's narrow form; no user switch). A route takes the **engine path** when all of these hold: its source is per-note and engine-local (ENV 1 to ENV 4, `vel`, or an LFO whose `lfoN.voice` is Per note); its destination is a key PA-4 lists for a per-sample offset; its polarity is not Control [X77]; and Quantize is off on every route into that key [X81]. Every other route takes the **outside path** (MS-2 to MS-4): from a global source, from keytrack, MPE, expressions or the swarm's observables, into a control-rate key, a modulator's setting, an FX value, the morph position or a depth, a Control route, a quantized route. **On the engine path** the modulation system evaluates the matrix (the route weights k, MS-25; d_eff, MS-17; the loop rules, E.4) and hands each voice, per destination key and per engine-local source, one triple (k[c]·d_eff, polarity, curve) for each corner c that holds the route with k[c] > 0, in that corner's depth, polarity and curve (MS-24's sum, term by term), timed like an offset, with k[c]·d_eff ramped by PA-3; the engine evaluates only MS-14 and MS-15's closed form on the source's own value at every sample and adds the result to that key's offset before PA-2's clamp. So the matrix has one home, the modulation system, and the source's value is never late. |
| MS-42 | **A new voice is modulated from its first sample** (IF-2's rule, ADR-243; the critic's review). At a note-on's sample the modulation system evaluates that note's outside-path routes (its per-voice sources at their starting values, every global source as it stands) and hands their offsets, its Control inputs and its engine-path triples for that note id **at the note-on's sample**, not the next boundary; the engine applies them in either order with the note-on (PA-3) and sets them, not ramped, at the voice's start. A per-note LFO's phase 0 is the note-on's sample (LF-2). So no note sounds a sample unmodulated (item 36.13). |

### E.3 Where routes meet the morph

| id | requirement |
|---|---|
| MS-24 | **Each corner's routes fade with the corner** (ADR-372 r1). For a destination, each corner c has a **route weight** k[c] (MS-25). Every route of corner c on that destination contributes k[c]·m(s), m from MS-15 at that corner's depth, polarity, curve and ends, and a route that a corner lacks contributes nothing there. So a route only some corners hold fades in with them; and where corners disagree in polarity, the contributions cancel in the middle and the route comes back inverted past it. A Control route of corner c adds k[c] to W and k[c]·(lo + (hi − lo)·c(s)) to P (MS-20). |
| MS-25 | **The route weights** (the corner-mapping lab's law; ADR-371 MQ5; ADR-379 r1, r4) [X63]. Each destination has a target t[c] per corner, read at each boundary and held from it: **Blend and Stepped**, MO-4's unglided weights (the stepped position's under Stepped, MO-10); **Quantum**, 1 at the destination's pick and 0 elsewhere. **The resolver owns every pick** (IF-12): an engine key's or a modulator setting's pick is that key's own (MO-7, MO-8, MO-13); any other destination's is the resolver's answer for the purpose `morph/key/` followed by its destination id (`mod/fx/N/` and the value's name for an FX value; `mod/depth/`, the source and the destination for a depth route), read at the unglided weights, with no latch. The weights follow `morph.law`, whatever the destination key's own mode. **A source's keys** (MO-26; ADR-379 r1): for a destination that is one of a source's keys, t is MO-26 (2)'s on-corner weights w[c]·on_c / W_on (for the noise, MO-26 (7)'s erosion-or-on corners, weighted by W_E), and under Quantum the restricted pick the resolver answers (an off corner scoring −10¹²); while MO-26 (1) waits to send the switch off, t holds; once the switch is off, t is the plain weights above. So an off corner's routes never touch the source. **Morph off:** t is 1 at corner `morph.audible`. **The glide** [X76]: each k[c] follows t[c] through its own one-pole of time constant `morph.glide` seconds, stepped per sample with the coefficient 1 − e^(−1/(f·`morph.glide`)) (equal, since t holds between boundaries, to one step per interval with 1 − e^(−Δ/(f·`morph.glide`))), snapping to t once within 10⁻⁶, bypassed at 0. **At a landing** (MO-26 (4)) the routes on the landing source's keys take k = t at once, with no glide. |
| MS-26 | **This is a parametric blend, not a crossfade of sounds.** ADR-377 forbids two sounds crossfading except through PA-5's timed flip. A route's contribution blending across corners moves one parameter's value, as a Morphable key's blend does (MO-9); it renders nothing twice, so ADR-377 holds. |
| MS-27 | **A modulator's settings are keys of the morph** (ADR-371 MQ15; ADR-379 r4; MD-45, MD-49). The resolver resolves the modulation manifest's keys (E.5 to E.8) with Appendix D's one law, as it resolves the engine's: the rows marked **M** are Morphable (MO-9, MO-5 (b)), the rows marked **S** are Structural and follow their quantum boundaries under every law (MO-13; ADR-377). Its `Param` events for them go to the modulation system, never the engine (IF-12). **In 1.0 they take MO-11's menu (Follow, Quantum, Exempt; Count) only:** waypoints and region locks on a modulator's key are after 1.0 (ADR-379 r4), and IF-12 refuses them. A drawn LFO's points are corner state (LF-7), never host parameters; their picks under Quantum are the resolver's answers (LF-8). |
| MS-28 | **An Exempt key still takes modulation** (ADR-371 A1, GN-40; MD-59; MO-12 as round 61 states it). Exempt takes the key out of the morph only: routes on it act as on any key. **Its routes are corner A's**, t one at A, as MO-12 holds corner A's value; with the morph off, corner `morph.audible`'s (MO-20). |
| MS-29 | **The morph position as a destination** (ADR-371 MQ8; ADR-379 r3; MD-61). Only a **global** LFO (LF-2) and the **2-D random** (E.8) may target the morph position's x or y. A per-note LFO, an envelope, any per-voice source, the Kuramoto LFO (a modulator of its own, E.6, not one of the four LFOs' kinds), ORBITAL, the mod wheel, the bend, pressure and the macros may not. Such a route is patch-wide (MS-19). Its sum (MS-20) is added to `morph.x` or `morph.y` and clamped to [0, 1]; the result is the position the resolver reads (IF-12), one control interval late (MS-34). The host parameters `morph.x` and `morph.y` keep their own values. **Save current sound under a position route** (the lead, B4; consistent with ADR-373 r6: modulation is not frozen) [X79]: for a Morphable key, Save stores the key's unglided target at the position `morph.x`, `morph.y`, the unmodulated one, not the value heard at the modulated position; every other part of MO-25 is unchanged. |

### E.4 Loops

| id | requirement |
|---|---|
| MS-30 | **The loop setting** (ADR-372 r3, r5). `mod.loops`: Refuse any possible loop, Refuse only inside a corner, or **Allow, one step late** (the default for a new patch). Saved per patch, so a preset carries its own; a page choice, never morphed, never a host parameter. **FX device:** the same setting and default (ADR-372). |
| MS-31 | **The graph** (the corner-mapping lab's law, extended to the engine; the critic's review) [X65]. **Nodes:** every modulator, ENV 1 to ENV 4, each oscillator's swarm (the node of its sources R, ψ, onset and gravity), and the pad. **Edges**, each a route's: a route whose destination is a modulator's setting gives an edge from its source to that modulator; one whose destination is a row of ENV 1 to ENV 4 (5.2) to that envelope; one whose destination is a key of oscillator N (`oscN.…`, its blades' rows included) to oscillator N's swarm; one whose destination is a voice row that the swarms read (5.2: pitch, gravity's rows) to both swarms; a depth route gives an edge from its source to the node its route's destination reaches; a patch-wide route (MS-29) gives an edge from its source to the pad. So `osc1.R` routed to `osc1.coupling` is a loop **through the engine**, closed by the engine's own law. **Under the two refusing modes the pad counts** (ADR-372 r5): it has an edge to each node one of whose settings differs between corners, or into whose settings the routes differ between corners. A **loop** is a cycle in this graph. |
| MS-32 | **Refusing.** Under **Refuse any possible loop**, the edges are taken in a fixed order (the pad's, then the patch-wide routes, then corner A's routes, B's, C's and D's, each corner's in the order stored), and a route whose edge closes a loop with the edges kept so far is refused. Under **Refuse only inside a corner**, the same is done for each corner on its own, with the pad's and the patch-wide edges; and at each boundary the live edges (a corner's route whose route weight k on its destination is at least 10⁻³, MS-25) are checked in the same order, and where they close a loop in that mixed state, **the cycle's lightest route** (the smallest k; on a tie, the later in the fixed order) is muted for that interval. **A corner edit that changes the pad's edges** (a setting that comes to differ between corners) and so closes a loop under a refusing mode goes through, and the cycle's lightest route is muted until the loop opens again; the page names it. A refused route is never created by the gesture that would make it, and the page says why. Under **Allow, one step late**, nothing is refused or muted. |
| MS-33 | **A preset loaded into a corner that would close a loop** (ADR-372 r4), under a refusing mode: the load goes through without the routes that close it, refused in MS-32's order, and the page names each route it dropped. |
| MS-34 | **One step late, under every mode** (ADR-372 r3; the corner-mapping lab) [X65]. A modulator reads the summed offsets of its own settings, the resolver reads the morph position's offset, and an outside-path route reads a swarm's or an envelope's value, as they were computed or published at the previous boundary, **held from the boundary** to the next. So no outside-path read sees a value of its own interval, the order in which modulators are evaluated changes nothing, and the three modes differ only in what they refuse. No engine-path route can close a loop within a sample: its destinations are PA-4's keys (MS-41), and no engine-local source reads one of them. |
| MS-35 | **The bound, for an allowed loop** (ADR-372's armor note; Blind-Spot Armor risk row 6). Every destination is clamped to its own range after its sum (MS-20); every source's s lies in [0, 1] by its own law (E.5 to E.8, MS-8), every depth in [0, 1] (MS-17), and a loop is read one control interval late (MS-34). So, whatever the loop, every value is finite and in its range at every interval, and nothing can run away within an interval. **An allowed loop can oscillate, and its fastest oscillation is a buzz at the control rate's Nyquist frequency**, f/(2Δ): 760 Hz at 44.1 kHz (Δ = 29), 750 Hz at 48 kHz. Item 39 holds it as a named gate. |
| MS-44 | **The erosion flag** (B3; MO-26 (7); the critic's review). For each corner, the modulation system tells the resolver (IF-12), at each boundary, whether any **admitted** route of that corner has `sub.erode` as its destination: a route with depth above 0 (d_eff above 0 for a wired route), neither refused (MS-32, MS-33) nor muted that interval, Control routes included. MO-26 (7) reads it as "the destination of a modulation route in that corner". |

### E.5 The LFOs

Each LFO's settings (MD-45; ADR-371 MQ15; ADR-371 A1; ADR-378). Rows marked **host** are host parameters in the
modulators' own append-only block (MS-40); every other setting is patch state, saved by the morph system per corner.

| key | label | range | default | class | what it does |
|---|---|---|---|---|---|
| `lfoN.voice` | Voice | Global, Per note | Global [X66] | S | LF-2. |
| `lfoN.kind` | Kind | Drawn, S&H, Random | Drawn | S | LF-6, LF-9. |
| `lfoN.smooth` | Smoothing | Linear, Cosine, Cubic | Linear | S | Random's interpolation (LF-10). |
| `lfoN.travel` | Travel | None, Time, Rate, Spring | None; S&H: Time (ADR-378) | S | LF-11. |
| `lfoN.travelTime` | Travel time | 0.001 to 4 s, log | 0.08 s [X68] | M | Time's arrival time. |
| `lfoN.travelRate` | Travel rate | 0.05 to 50 per s, log | 2 per s | M | Rate's speed, in full scales (Δy = 2) per second. |
| `lfoN.springHz` | Tension | 0.2 to 12 Hz, log | 3 Hz | M | Spring's natural frequency. |
| `lfoN.springZeta` | Damping | 0.02 to 1.5, lin | 0.35 | M | Spring's ζ; the page shows the overshoot e^(−πζ/√(1−ζ²)) for ζ < 1. |
| `lfoN.strike` | Strike | Next draw, Keep held value, Rewind | Next draw | S | What a strike does to the random stream (LF-12; ADR-378). |
| `lfoN.rate` | Rate | 0.02 to 40 Hz, log | 1 Hz | M, host | LF-4. |
| `lfoN.sync` | Sync | off, on | off | S, host | LF-4. |
| `lfoN.division` | Division | RC-7.4's table | index 9 (1/4) | S, host | LF-4. |
| `lfoN.triplet`, `lfoN.dotted` | Triplets, Dotted | off, on | off | S, host | RC-7.4's menu filter; changes no sound. |
| `lfoN.retrig` | Retrigger | off, on | off | S, host | LF-3. |
| `lfoN.phase` | Start phase | 0 to 360° | 0° | M | LF-5. |
| `lfoN.envMode` | Envelope mode | off, on | off | S | LF-13. |
| `lfoN.seed` | Seed | 0 to 2³² − 1 | N | S | LF-12 [X70]. |

| id | requirement |
|---|---|
| LF-1 | **Four LFOs**, `lfo1` to `lfo4` (ADR-371 MQ1). Each outputs y in [−1, 1]. The law of this section has one home (BD-2): the modulation system runs it for a global LFO, the engine for a per-note one (LF-2), the same code. |
| LF-2 | **Global or per note** (ADR-371 MQ2; MD-35). **Global:** one wave for the instrument, the same y in every voice's routes. **Per note:** one wave per voice, computed by the engine (ADR-379 r2) from the resolved settings and points the modulation system hands it (MS-43), started at the voice's fresh start with phase 0 at the note-on's sample (MS-42), with its own phase and stream; a legato change keeps it running (VO-6). Only a global LFO may target the morph position (MS-29). |
| LF-3 | **Retrigger** (ADR-266 r7; MD-35). A **strike** restarts the wave at its start phase. Global with retrigger on: every note-on that makes a voice's fresh start strikes the one wave, for every held note. Per note with retrigger on: each note's wave is struck at its fresh start; off, each per-note wave starts at the phase an unstruck wave has at that sample (LF-4), so the notes run free and agree in phase while their streams differ [X66]. Global with retrigger off is never struck. |
| LF-4 | **Rate and sync** (ADR-371 MQ3, MQ6; RC-7.4; MD-42, MD-43). The LFO's position p is a count of cycles; k = ⌊p⌋ is the cycle and x = p − k the place in it. **Free:** p advances by `lfoN.rate`/f per sample from the last strike, or from reset (a change of rate never moves p). **Synced, retrigger off:** p(n) = P(n)/L + φ₀, P(n) the host's song position in quarter notes at sample n (RC-7.2; while stopped, it advances at the tempo from the last position, as MO-10 runs), L the length of `lfoN.division`'s row in quarter notes, φ₀ = `lfoN.phase`/360°. So the same bar always meets the same part of the wave, and a loop's wrap or a locate (RC-7's jump) puts it there. **Synced, retrigger on** [X66]: a strike sets p to φ₀, and p then advances at 1/L cycles per quarter note of the tempo. The rate is Morphable and blends on its log taper; the division is Structural and flips. |
| LF-5 | **The start phase** (ADR-371 A1). Synced, it offsets the wave against the song's grid (LF-4). Free or struck, it is the phase a strike and a reset start the wave from [X66]. |
| LF-6 | **The drawn shape** (ADR-349 r1, r4; MD-36). One point list shared by the four corners, at most 32 points, each point carrying a stable id that is never reused. Each corner holds every point's time t in cycles (0 to 1, the first point at 0), level y in [−1, 1] and bend u in [−2.5, 2.5]. The segment from point i to point i + 1 follows C.12's curve, y = yᵢ + (yᵢ₊₁ − yᵢ)·(1 − e^(−c·τ))/(1 − e^(−c)), c = 4·uᵢ, τ the fraction of the segment elapsed (at c = 0, τ itself); equal times make a vertical step; after the last point the wave holds its level to the cycle's end and, in a loop, steps back to the first point's level. |
| LF-7 | **Adding a point changes no sound in the other corners** (ADR-349). In each other corner it lands parked on that corner's segment, at its fraction f of the segment's time, with the segment's bend u split into u·f before it and u·(1 − f) after (C.12's exact split). A point parked in all four corners leaves the list. Pairing points across corners is a one-time edit action, never run while the sound plays. |
| LF-8 | **The shape under the morph** (ADR-349 r1, r2; ADR-371 MQ5; MD-38, MD-39). **Blend and Stepped:** each point's time, level and bend are blended over the corners at MO-4's weights (at the stepped position under Stepped), times linearly in cycles; the list is then ordered by time (a point's id breaking a tie), so it never runs backward. **Quantum (on trial):** each point picks its corner by MO-7, with the draws of the purpose `morph/lfoN/point/` followed by its id in decimal, and its time, level and bend are that corner's, each glided by MO-5 (b) at `morph.glide`. At cohesion 1 every point picks one corner, which is the whole-shape flip. Corners that differ in `lfoN.kind`: **[OPEN M3]**. |
| LF-9 | **S&H** (ADR-371 MQ4; ADR-266 r6; ADR-378). Draw k of the LFO's stream (LF-12), y_k = 2·u_k − 1, is held for cycle k: the shape's value is y_k for all x. It draws at every cycle start, the first included [X55], and it is clocked by its own cycle (LF-4) [X56]. |
| LF-10 | **Random** (ADR-371 MQ4; B237; ADR-378). The same stream, interpolated between the draws, each passing through y_k at x = 0 of cycle k and heading for y_{k+1} (the look-ahead stays on the beat: a seeded stream knows draw k + 1). **Linear:** y_k + (y_{k+1} − y_k)·x. **Cosine:** y_k + (y_{k+1} − y_k)·(1 − cos πx)/2. **Cubic** (Catmull–Rom through y_{k−1} … y_{k+2}, clamped): y = y_k + ½·x·(y_{k+1} − y_{k−1} + x·(2y_{k−1} − 5y_k + 4y_{k+1} − y_{k+2} + x·(3(y_k − y_{k+1}) + y_{k+2} − y_{k−1}))), then clamped to [−1, 1], which clips the run (−1, +1, +1, −1)'s peaks of 1.25. Draw −1 is the hash of the unstepped state h, by MS-5's identity, as the lab reads it. |
| LF-11 | **Travel** (ADR-378). One travel stage follows each random shape (S&H, Linear, Cosine, Cubic); the drawn kind has none. It turns the shape's value r(t) into the output y(t), per sample. **None:** y = r. **Time** [X68]: at each cycle start the stage notes y₀, the output there, and for t seconds since that cycle start y = y₀ + (r(t) − y₀)·Y(t/T) while t < T, then r(t); T = `lfoN.travelTime`, Y C.12's curve with u = ln(100)/4 (the modulator lab's SLEW law, which arrives exactly at T); a travel longer than a cycle never arrives, and the next cycle starts from where it is. On S&H this is the lab's SLEW exactly. **Rate:** y moves toward r by at most 2·`lfoN.travelRate`/f per sample. **Spring:** a mass on a spring, ẍ = ω²(r − y) − 2ζω·ẏ, ω = 2π·`lfoN.springHz`, ζ = `lfoN.springZeta`, stepped per sample by semi-implicit Euler (velocity first), y clamped to [−1, 1] with its velocity zeroed at the clamp [X68]. Times are counted in samples, so a change of rate stretches no travel. **Defaults:** None on Linear, Cosine and Cubic; S&H keeps its shipped SLEW law, Time at 0.08 s (ADR-378). |
| LF-12 | **The random stream and a strike** (ADR-378; the labs facilitator) [X67, X70]. A global LFO's stream is the purpose `mod/lfoN/global` at `lfoN.seed`; a per-note LFO's is `mod/lfoN/note`, followed by the four bytes of the voice's note counter (DT-4's, which VO-10 publishes at the fresh start), least significant first, then `lfoN.seed` (MS-5's order). Free, the cycle index k counts cycles since reset, offset by the strikes below; synced (retrigger off), k = ⌊p⌋ from the song position, so the same bar meets the same draw. A strike starts a new cycle and, per `lfoN.strike`: **Next draw**, the new cycle reads the first draw not yet heard; **Keep held value**, the new cycle reads the draw the struck cycle read (S&H holds its value; the next wrap reads the following draw); **Rewind**, the new cycle reads draw 0, so every strike replays the same sequence. A per-note LFO's first cycle is draw 0 of its own stream under Next draw and Keep, and draw 0 of the `mod/lfoN/global` stream under Rewind, so every note plays the same riff. |
| LF-13 | **Envelope mode** (ADR-371 MQ16, A1; MD-46, MD-60). The LFO plays one cycle from its start phase at its rate (or division) and stops, holding its last point's value. At its note-off, the value it holds or has reached falls to zero along the release of osc 1's governing envelope (RC-6.5), C.12's release law with that envelope's release time and curve: s(t) = s_r·(1 − Y(t/T_r)), s the source value [X69]. **Per note:** the cycle starts at the voice's fresh start and releases at that voice's release (the pedal's included, VO-12). **Global** (ADR-371 A1): any new note restarts it, and it releases when no note is held [X69]. No envelope beyond ENV 1 to ENV 4 exists in 1.0; envelope mode stands in for one. |
| LF-14 | **Phases and streams are performance state** (MD-52; T6) [X66]. An LFO's phase, its stream position and its travel state are never saved and restart at a load (IF-4). |

### E.6 The Kuramoto LFO

In 1.0 as the modulator lab built it (ADR-375: "Yes, as is"; B226; ADR-053). One global instance. It is a modulator of its own, not one of the four LFOs' kinds, so it may not move the morph position (MS-29; ADR-379 r3).

| key | label | range | default | class |
|---|---|---|---|---|
| `kuro.voices` | Voices | 1 to 8 | 4 | S |
| `kuro.K` | Coupling K | −1 to 1, lin | 0.35 | M |
| `kuro.rate` | Rate | 0.05 to 8 Hz, log | 1 Hz | M |
| `kuro.detune` | Detune | 0 to 1, lin | 0.3 | M |
| `kuro.anchor` | Anchor | Mean, Slowest, Fastest | Mean | S |
| `kuro.shape` | Shape | Sine, Triangle, Saw, Square, Drawn | Sine | S |
| `kuro.drawn` | Drawn from | LFO 1 to 4 | LFO 1 [X71] | S |
| `kuro.retrig` | Retrigger | off, on | off | S |

| id | requirement |
|---|---|
| MK-1 | **Rates.** With n = `kuro.voices`, voice i (0 to n − 1) has xᵢ = 2i/(n − 1) − 1 (0 when n = 1) and ratio ρᵢ = 2^(2·xᵢ·`kuro.detune`). Its rate is rᵢ = `kuro.rate`·ρᵢ/N, N the geometric mean of the ρᵢ (Mean), their minimum (Slowest) or their maximum (Fastest). |
| MK-2 | **Phases.** Each voice has a phase θᵢ in [0, 1). At reset, and at a change of `kuro.voices`, θᵢ = i/n. With retrigger on, every note-on that makes a voice's fresh start while no other note is held resets them so [X71]. |
| MK-3 | **The law, per sample** (the lab's KuroSwarm). κ = sgn(K)·K²·3 Hz. **K ≥ 0:** θᵢ ← θᵢ + (rᵢ + κ·R·sin(ψ − 2πθᵢ))/f, R and ψ the last tick's. **K < 0 (splay):** the voices are ranked by phase, ascending, a tie broken by the lower index; a = the phase of rank 0; m̄ = the mean of the rᵢ; the voice of rank q moves by θᵢ ← θᵢ + (rᵢ + (m̄ − rᵢ)·\|K\|)/f + \|κ\|·sin(2π(a + q/n − θᵢ))/f, every voice from the phases before this sample's moves. Then each θᵢ ← θᵢ − ⌊θᵢ⌋. **The tick:** at the first sample of every 16 samples on the system's clock, before that sample's move, R = \|Σ e^(i2πθᵢ)\|/n, ψ = arg Σ e^(i2πθᵢ), and each output yᵢ = shape(θᵢ) are computed and held for the tick. 2π is exact [X71]. |
| MK-4 | **The shapes.** Sine sin 2πθ; Triangle 4θ − 1 for θ < ½, else 3 − 4θ; Saw 2θ − 1; Square 1 for θ < ½, else −1; Drawn: the chosen LFO's shape (LF-6) as resolved now, at θ. |
| MK-5 | **Sources:** `kuro.v1` to `kuro.v8` and `kuro.R` (MS-8). Its phases are performance state (LF-14). |

### E.7 ORBITAL

An N-body gravity field whose observables are sources (ADR-165; SPEC-ORBITAL as amended by ADR-165 A1 and ADR-375 A1;
B126). One global field [X73].

| key | label | range | default | class |
|---|---|---|---|---|
| `orb.G` | Gravity | 0.001 to 1, log | 0.025 | M |
| `orb.eps` | Softening ε | 0.005 to 0.5, log | 0.06 | M |
| `orb.timeScale` | Time × | 0 to 4, lin | 1 | M |
| `orb.drag` | Drag | 0 to 1, lin | 0 | M |
| `orb.edges` | Edges | Bounce, Wrap, Open | Bounce | S |
| `orb.com` | Centre lock | off, on | on | S |
| `orb.smooth` | Smoothing | 0 to 0.05 s, lin | 0.005 s | M |
| `orb.thermo` | Thermostat | off, on | off | S |
| `orb.thermoSpeed` | Target speed v* | 0.01 to 2, log | 0.5 [X73] | M |
| `orb.thermoTau` | Thermostat time τ | 0.05 to 5 s, log | 0.5 s [X73] | M |
| `orb.reset` | Reset on | Free-running, Transport, Note-on | Free-running | S |
| `orb.seed` | Seed | 0 to 2³² − 1 | 1024 | S |
| per body j (2 to 8 bodies) | mass m (0.1 to 16, log; M), pinned (S), instant mass (S), home x₀, y₀ (0 to 1), initial velocity (vx₀, vy₀) | | the preset's | |

| id | requirement |
|---|---|
| MB-1 | **The step** (SPEC-ORBITAL §5; the lab's step). Velocity Verlet at a fixed dt = 1/960 s (SPEC-ORBITAL §11's divergence from the prototype's 1/480 s). The accelerations: for each pair (a, b), d = position_b − position_a (under Wrap, each component wrapped to [−½, ½], the minimum image), r² = \|d\|² + ε², and a gains G·m_b·d/r³ while b gains −G·m_a·d/r³. Then, for each body not pinned: v += ½·a·dt; position += v·dt; the accelerations again; v += ½·a·dt; v ← v·e^(−3·drag·dt). Then the thermostat (MB-2), then the speed clamp: \|v\| above 3.0 field units per second is scaled to 3.0, and each such clamp is counted (MS-45). Then the edges: **Bounce** puts each coordinate through MR-4's fold F (the reflection into [0, 1]) and negates that velocity component when F reflected it an odd number of times, so a step past both walls is still a reflection; **Wrap** takes each coordinate modulo 1; **Open** does nothing. Then, with the centre lock on, edges not Wrap and no body pinned, the mass-weighted mean position is moved to (½, ½) and the mass-weighted mean velocity is subtracted from every body. |
| MB-2 | **The thermostat** (ADR-375 A1; SPEC-ORBITAL §3.1, §5). When on, after the drag and before the speed clamp: v̄ = the arithmetic mean speed of the free bodies (not pinned); λ = √(1 + (dt/τ)·((v*/v̄)² − 1)), clamped to [0.5, 1.5] (and 1 when v̄ is 0); every free body's velocity is multiplied by λ. It keeps orbit shapes and regulates only their pace. Off, the step is bit-identical to MB-1 without it. The Van der Pol band is not built (SPEC-ORBITAL §12.6: the regulator not kept is deleted). |
| MB-3 | **Time.** Each sample adds `orb.timeScale`/f to an accumulator; while it holds at least dt, and at most 8 times per sample, a step runs and dt is taken from it; if 8 steps ran, the accumulator is set to 0. Steps are counted in samples, never wall time (SPEC-ORBITAL §6.2), so a render is the same offline and in real time; a different sample rate gives a different, equally valid trajectory (stated, not hidden). |
| MB-4 | **Mass** (ADR-165 A1). A change of a body's mass, from a knob or a route, reaches the force loop through a one-pole of 0.020 s per sample, unless the body's instant flag is on. |
| MB-5 | **Safety.** ε > 0 always (its range). If any position or velocity becomes non-finite, the field is reset to its home (MB-8) and the event counted. Positions and velocities are double precision. |
| MB-6 | **The observables**, from the state after each step, held between steps. Per body: x = clamp(x, 0, 1); y = clamp(1 − y, 0, 1) (up is higher); speed = min(1, \|v\|/S); accel = min(1, \|a\|/A), \|a\| the body's acceleration from the step's second force evaluation (MB-1) (SPEC-ORBITAL §4, on ADR-165 A1's fixed ceiling, not a running maximum; ADR-379 r5); near = 1 − min(1, √(d_min² + ε²)), d_min the distance to the nearest other body (0 with one body); angle = frac(atan2(½ − y, x − ½)/2π + 1). Per field: energy = min(1, Σ ½·m·\|v\|²/E); spread = clamp(√(Σ m·\|p − p̄\|²/M)/√½, 0, 1), p̄ the centre of mass, M the total mass. Every observable but angle is smoothed by a one-pole per sample of time constant `orb.smooth` (bypassed at 0), primed to the observable at a reset (no glide from the old values). |
| MB-7 | **The full-range scales** (ADR-375: "Yes, per preset"; ADR-165 A1 and ADR-379 r5 for acceleration) [X73]. S, E and A above are each preset's own, stored with it, so its speed, energy and acceleration outputs span a fixed full range and are audible without extra gain. For a factory preset each is the peak of \|v\| (of Σ ½·m·\|v\|², of \|a\|) over the first 30 s from home at time scale 1, rounded up to two significant figures. A field the user builds has S = 3.0, E = ½·M·3.0², the most the clamp allows, and A = 2·G·M/(3√3·ε²), the most softened gravity can give, until it is saved as a preset, which measures them. |
| MB-8 | **Reset** (SPEC-ORBITAL §6.3, as B135 amended it). Reset puts every body at its stored home and initial velocity (a pinned body at rest) and primes the smoothing. `orb.reset`: **Free-running** never resets but at the system's reset and a load; **Transport** resets at the host's play start (RC-7); **Note-on** resets at the first note-on after all notes are off [X73]. The card's Reset and Return home buttons, and its Kick (a magnitude to a chosen body, its direction θ = 2π·u from the next draw of the purpose `mod/orbital/kick` at `orb.seed`), are performance events (ADR-267 A1): they make no history. Adding a body draws its home from the purpose `mod/orbital/add` at `orb.seed` (ADR-165 A2). |
| MB-9 | **Sources: bodies 1 to 4** (ADR-375 A1; ADR-379 r5). Each body's x, y, speed, acceleration, near and angle, and the field's energy and spread: 26 sources. Bodies 5 to 8 still move and act on the others, but are not routable. Whether the sine and cosine of each body's angle join them (ADR-165 A1, for Wrap) is **[OPEN M4]**, which the reopened ORBITAL lab decides. The bodies' positions and velocities are performance state, never saved [X73]. |
| MB-10 | **Its settings as destinations** (ADR-371 MQ8; SPEC-ORBITAL §7): G, ε, time scale, drag, each body's mass, the smoothing and the thermostat's two values (MS-12 (b)). Not in 1.0: the edge cushion (ADR-375 A1: after 1.0, not dropped), a per-voice field, the intent bus's grab and throw, persistence across sessions, the distance scale and its bake (B135), pair distances. |
| MB-11 | **Two presets' data** (the modulator lab's, SPEC-ORBITAL §10; for item 38.2). Coordinates are the field's, y downward as the integrator holds it (MB-6 flips it). **Binary:** G 0.025, ε 0.06, Bounce, centre lock on; two bodies of mass 3 at (0.38, 0.5) with velocity (0, −0.28) and at (0.62, 0.5) with (0, 0.28). **Figure eight** (Chenciner–Montgomery): s = 0.28, v = √(0.05/s); G 0.05, ε 0.01, Open, centre lock on; three bodies of mass 1 at (0.5 + 0.97000436·s, 0.5 − 0.24308753·s) and (0.5 − 0.97000436·s, 0.5 + 0.24308753·s), each with velocity (0.46620369·v, 0.43236573·v), and at (0.5, 0.5) with (−0.93240737·v, −0.86473146·v). Both are periodic. |

### E.8 The 2-D random

One point-valued source for the XY pads, three modes, seeded (ADR-266 r8; ADR-375; B540). One global instance.

| key | label | range | default | class |
|---|---|---|---|---|
| `xy.mode` | Mode | Walk, Jump, Scatter | Walk | S |
| `xy.homeX`, `xy.homeY` | Home | 0 to 1 each | 0.5 | M |
| `xy.region` | Spread | Disc, Box | Box | S |
| `xy.radius` | Radius | 0.02 to 1, lin | 0.5 | M |
| `xy.bias` | Bias | None, Corners, Edges, Centre | None | S |
| `xy.biasAmount` | Bias amount | 0 to 1, lin | 0.6 | M |
| `xy.tau` | Walk speed τ | 0.05 to 20 s, log | 1.5 s | M |
| `xy.sigma` | Walk range σ | 0.02 to 1.5, log | 0.45 | M |
| `xy.smoothTime` | Walk smoothing | 0 to 5 s, pow | 0.225 s [X47] | M |
| `xy.clock` | Trigger | Note, Rate, Tempo | Rate | S |
| `xy.rate` | Rate | 0.1 to 20 Hz, log | 1.5 Hz | M |
| `xy.division` | Division | RC-7.4's table | index 9 (1/4) | S |
| `xy.triplet`, `xy.dotted` | Triplets, Dotted | off, on | off | S | 
| `xy.travel` | Travel | None, Time, Rate, Lag, Spring | Spring [X74] | S |
| `xy.travelTime` | Time | 0.005 to 4 s, log | 0.25 s | M |
| `xy.travelRate` | Rate | 0.05 to 20 pads per s, log | 2 | M |
| `xy.lag` | Lag τ | 0.002 to 2 s, log | 0.12 s | M |
| `xy.springHz` | Tension | 0.2 to 12 Hz, log | 3 Hz | M |
| `xy.springZeta` | Damping ζ | 0.02 to 1.5, lin | 0.35 | M |
| `xy.seed` | Seed | 0 to 2³² − 1 | 540 | S |

| id | requirement |
|---|---|
| MR-1 | **One source, two rows** [X53]. The point (x, y) in [0, 1]² is the sources `xy.x` and `xy.y` (MS-8). Mapping it to a pad in one gesture makes two Control routes, x to the pad's X and y to its Y, with ends 0 and 1 [X74]. |
| MR-2 | **The tick** [X52]. The law runs at the first sample of every 16 samples on the system's clock, with dt = 16/f s, and the point holds between ticks. No destination, pitch included, gets a faster path. |
| MR-3 | **Streams** [X50]. The source has its own seed, `xy.seed`, and each mode its own stream: the purposes `mod/xy/walk`, `mod/xy/jump` and `mod/xy/scatter` (MS-5), so switching mode changes the numbers. |
| MR-4 | **The map from a unit point** c in [−1, 1]² to the pad. (1) **Bias** [X49]: γ = 1 + 3·`xy.biasAmount`, so amount 0 is uniform under every bias. Corners: each axis c → sgn(c)·(1 − (1 − \|c\|)^γ). Edges: with m = max(\|cx\|, \|cy\|), both axes scaled by (1 − (1 − m)^γ)/m (by 0 at m = 0). Centre: both scaled by m^(γ − 1). (2) **Region:** Box keeps the point; Disc maps it by the Shirley–Chiu concentric map (\|a\| > \|b\|: r = a, φ = (π/4)·(b/a); else r = b, φ = π/2 − (π/4)·(a/b); then (r cos φ, r sin φ); (0, 0) stays). (3) **Position and fold** [X51]: x = F(`xy.homeX` + `xy.radius`·a), y = F(`xy.homeY` + `xy.radius`·b), with F the reflection into [0, 1], F(z) = z − 2⌊z/2⌋, then 2 − that if above 1. So a spread past the pad's edge reflects back in. |
| MR-5 | **Walk** (ADR-266 r8). Each tick n draws u₁ = 1 − u_{2n} and u₂ = u_{2n+1} of the walk stream; g = √(−2 ln u₁)·(cos 2πu₂, sin 2πu₂); with a = e^(−dt/τ), the unit point moves c ← G(a·c + σ·√(1 − a²)·g), G reflecting each axis into [−1, 1] (G(z) = 2F((z + 1)/2) − 1): the Ornstein–Uhlenbeck process's exact transition, so σ holds at any tick rate. **Smoothing** [X47]: two one-poles in series on c, each of time constant `xy.smoothTime` s (coefficient 1 − e^(−dt/t_s); off at 0, when both follow c). The smoothed point is mapped by MR-4. |
| MR-6 | **Jump and Scatter: the triggers** (ADR-375). **Note:** each note-on is a trigger; several on one tick are one. **Rate:** a phase advancing by `xy.rate`·dt per tick fires a trigger when it reaches 1 (and on the first tick after a replay). **Tempo:** the trigger index is j = ⌊P/L⌋, P the song position in quarter notes from the transport's bar 1 and L the division's length (RC-7.4), so it locks to the bar, not to its own replay; a trigger fires at each tick where j changes, a jump included (RC-7). Under Note and Rate, j counts the triggers since the replay, from 0. |
| MR-7 | **Jump and Scatter: the target.** **Jump:** trigger j's unit point is (2u_{2j} − 1, 2u_{2j+1} − 1) from the jump stream. **Scatter:** the R2 sequence, ((2·frac(u₀ + j/ρ) − 1), (2·frac(u₁ + j/ρ²) − 1)), ρ = 1.324717957244746 (the plastic number, the real root of x³ = x + 1), u₀ and u₁ the scatter stream's draws 0 and 1. Each is mapped by MR-4 to a target. |
| MR-8 | **Travel** to the target (ADR-375: a zero beside the glide and spring laws) [X54, X48]. **None:** the point is the target at the trigger's tick. **Time:** linear from the point at the trigger to the target, arriving at exactly `xy.travelTime` s. **Rate:** along the straight line at `xy.travelRate` pads per second, landing on the target. **Lag:** a one-pole of `xy.lag` s (it never arrives). **Spring:** per tick, v ← v + (ω²·(target − p) − 2ζω·v)·dt, then p ← p + v·dt, ω = 2π·`xy.springHz`, ζ = `xy.springZeta`, reflecting a coordinate past 0 or 1 back in with that velocity component negated; the page shows the overshoot e^(−πζ/√(1−ζ²)) for ζ < 1 as a readout. Every law but the spring stays on the segment, so inside the pad; only the spring carries velocity, and a change to it starts from rest. |
| MR-9 | **Replay.** At reset, at a load, and at a change of `xy.mode` or `xy.seed`, the source replays: the walk's c is (0, 0) and its smoothers too, the point is home (MR-4 of (0, 0)), the trigger count is 0, and every stream reads from draw 0. The card's Replay button does the same as a performance event (ADR-267 A1). So the same seed, settings and triggers give the same path. |
| MR-10 | **Global, in 1.0.** One point for the instrument. Its path is performance state, never saved (LF-14). |

### E.9 State, the FX device, the CPU, and what follows 1.0

| id | requirement |
|---|---|
| MS-36 | **What is saved** (MD-47; GN-10; T5). Saved with the patch, by the morph system per corner: each corner's routes; each modulator setting's corner values and each LFO's points (MS-27, LF-6). Per patch: the patch-wide routes, the modulators that exist, `mod.loops` and the seeds. **Where it lives** [X80]: the modulation state is a member `mod` of the **shell's** patch document, beside the morph system's corners, never inside the engine's IF-4 document. The engine does not own it (RC-11.1 lists it as another system's state, as it lists the corners), most of it is per corner as the corners are, and the engine's state then needs no change and no new version (RC-9.4). Never saved: phases, streams, ORBITAL's bodies, the 2-D random's path, the summed offsets (LF-14, MB-9, MR-10). |
| MS-37 | **No hidden user state** (RC-11.1). Every audible value a user can change in the modulation system is a host parameter of its block (MS-40) or a member of `mod`. History: the modulation system's own undo covers its edits (MS-23); firing a performance event makes none (ADR-267 A1). |
| MS-38 | **The FX device** (ADR-254; ADR-371, ADR-372, ADR-375; MD-55 to MD-58). It hosts the same matrix, modulators and laws over its audio, with the same loop setting and default. A route there reaches only the rack's declared mod values, and Quantize applies on any FX module's pitch destination. Its notes exist only when MIDI is routed to it: per-note LFOs (one per sidechain note, else none, computed by the device's own modulation system, since it has no engine), keytrack, velocity, MPE and every engine source have nothing to follow without it, and global LFOs are its default. ENV 1 to ENV 4 are EN-5's sidechain-note instances. The modulators' host-id block is append-only, so the device exposes it unchanged. The 2-D random's bar lock and ORBITAL's scales carry over unchanged. The Mod page and the matrix assume no oscillator panel exists. |
| MS-39 | **The CPU** (MD-54; PF-2; T10). The modulation system's cost is reported against its slice (PF-2's modulation 2 % of a min-spec core, which the morph, the intent bus and the arpeggiator share), never a gate: PF-11. |
| MS-43 | **What the engine receives for a per-note LFO** (ADR-379 r2; IF-13). For each LFO whose `lfoN.voice` is Per note, the modulation system hands the engine its resolved settings (MS-27) and its resolved point list (LF-8) whenever they change, timed like a `Param` event, and per voice the modulated values of its settings (MS-20, one step late, MS-34). The engine runs LF-1 to LF-14's law per voice, per sample, and the engine path (MS-41) reads its output. |
| MS-45 | **The observation view** (IF-13; the critic's review). Read between processing calls without changing anything, for the tests: each source's s (per voice where it is per voice), each route's k[c], d_eff and contribution, each destination's offset, P, A and (1 − W) per voice, each engine-path triple handed, the refused and muted routes, the erosion flags (MS-44), the morph position's offset, each modulator's state (an LFO's p, k and travel; the Kuramoto phases, R and ψ; ORBITAL's bodies, observables and clamp count; the 2-D random's point, c and trigger count). |
| MS-40 | **The modulators' host parameters** (ADR-371 MQ15; MD-55). The rows marked host in E.5, and [X72] every M row of E.6 to E.8, form one block of host parameters appended to the lockfile after the morph's (IF-11), ids explicit and append-only, each one row of one range, taper and step count. They reach the resolver and the modulation system (MS-27), never the engine. |

**After 1.0** (ADR-371 MQ12; ADR-375 A1; ADR-378; the lead's proposed defaults). Not built in 1.0, and named so that no
row above forecloses them: note-on randoms; the trigger timer, its impulses and every trigger source; the MIDI gate and
trigger; S&H's sample-on-trigger, and the S&H mode on ORBITAL's mappings (ADR-266 r6) [X56]; ORBITAL's edge cushion;
rails; the silent spring; a stepped destination kind (MS-12); envelopes beyond ENV 1 to ENV 4 (envelope mode stands in); waypoints and region locks on a modulator's keys (MS-27).
