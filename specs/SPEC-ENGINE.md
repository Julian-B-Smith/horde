# horde's engine: the spec

**Status.** The contract for horde's engine, 2026-10-16.

**Who reads it.** The builder reads this spec and the acceptance tests written from it, and nothing else (BD-10). A different agent writes those tests from this spec before the engine exists. Everything a requirement needs is in here.

**Source.** The record behind every requirement and law is kept outside this spec, by requirement id, in `docs/port/spec-engine-provenance.md`. The builder does not need it.

**The laws are normative.** Where a formula *is* the sound (the coupling law and its taper, member placement, the detune laws, drift, gravity, the ensemble, every blade law, the spreads, the pan law, the envelope's curve, DC removal), Appendix C states it as math with its constants and units. The math is binding, not any way of computing it: the builder is free to compute it fast (single precision, tables, SIMD, any loop order). **A law may change only where every anchor that tests it still passes, and the reason is then stated in this spec.**

**How to read it.**
- Requirements are numbered (`SW-4`, `VO-3`, `RC-2`, …); laws are numbered `C.0` to `C.16`. Each is testable.
- A tag **[Qn]** or **[Xn]** marks an item still open (section 11). The spec states what is built until it is ruled; if it is ruled otherwise, that requirement changes and nothing else does. **PROPOSED** marks a value the human will confirm by ear; it is built as written.
- "Must" is a requirement. "Should" is a recommendation the builder may depart from with a stated reason. Numbers carry units. Times are in seconds unless stated.
- Musicians' words are used where they exist. The glossary (section 2) fixes them.

---

## 1. Purpose and scope

**Purpose.** horde is a synthesizer whose oscillators are swarms: the stacked copies a synthesizer calls unison are coupled oscillators that pull toward one pitch or push apart, drift, settle into just intervals, and carry **blades**, windows that cut each cycle of the wave and replace what lies inside them with another sound (hard sync, FM, noise, fold, ring, crush). This spec defines the engine that makes that sound: everything from a played note to the instrument's stereo output before the effects.

The engine must do what a musician expects each control to do to the waveform, and it must sound good to the human. The bank of 83 presets (Appendix B) must stay recognisable and sound good by the human's ear.

**In scope.**
- The **voice**: allocation, stealing, the sustain pedal, mono and legato, the amplitude envelope (ENV 1), the per-voice signal path, the slot the per-voice filters sit in.
- What lives in a voice: **two swarm oscillators** (each with its members, its coupling and its two blades), the **sub** and the **noise oscillator**.
- The sum of the voices and the engine's output stage.
- The engine's interface to the rest of the instrument (section 7), and the reserved contracts for the modules still to come (7.2).

**Out of scope** (each meets this engine only through section 7):
- the FX modules and the rack itself;
- the filters' own designs (the engine builds only their per-voice slot, RC-1);
- the modulators (ENV 2 to ENV 4, LFOs, the mod matrix), the macros and intent bus, the morph engine, the pitch system (bend lane, note lane and glide, quantizer, MTS-ESP), the arpeggiator: the engine accepts their outputs and does not compute them;
- true-stereo processing after the voice, the mixer's strip and the master stage;
- the GUI, and the host shell beyond the engine's API.

---

## 2. Glossary

| term | meaning |
|---|---|
| **note** | One key press from the host, identified by its **note id** (CLAP's note id, with its channel and key). Two notes may share a key (MPE). |
| **voice** | One sounding note, instrument-wide. Polyphony counts voices: "8 voices" means 8 notes at once. A voice holds both oscillators, the sub, the noise oscillator, ENV 1 and the filter slot. |
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
| **ENV 1** | The voice's amplitude envelope. ENV 2 to ENV 4 are the modulation system's (RC-4). |
| **control rate** | About 1,500 updates per second. The interval between updates is the **control interval**, Δ. |
| **ramp** | A value travelling linearly from where it stands to its new value across one control interval. |
| **step** | A value arriving in one sample. |
| **quality** | The oversampling setting: Standard (1×) or High (2×). |
| **internal rate** | The sample rate the oscillators run at: the host rate × the quality factor. |
| **per-voice modulation** | An offset to one parameter for one voice, from the modulation system. |
| **Morphable / Structural / Device** | The three parameter classes: Morphable values blend between morph corners; Structural values are owned by one corner and flip; Device values stay outside the morph. |
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
  voice/      allocator and stealing, the pedal, the voice, ENV 1, mono and legato
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

- **One instrument-wide voice per played note** owns: the allocation decision, ENV 1, the per-voice filter slot, and inside it osc 1, osc 2, the sub and the noise oscillator. One allocator decides for all of them; there is no per-oscillator voice law.
- **Each source hands its voice's signal up before any mixing**; the voice's filters and ENV 1 act on it; the voices are summed after.
- **Members are mono** sources panned across the stereo field. Nothing renders a member separately per channel. True-stereo processing happens after the voice, outside this engine.

### 4.2 The per-voice signal path

```
note id, key, velocity, per-note expressions          pitch input (Hz, per note id, audio rate)
        │                                                        │
        ▼                                                        ▼
  ┌──────────────────────────── one voice ─────────────────────────────────┐
  │  voice pitch × gravity ratio ──► published voice pitch (IF-2)           │
  │                                                                         │
  │  osc 1: swarm of U members ─► each member: base wave + blade 1 + blade 2│
  │         ─► edge correction ─► DC removal ─► pan (mono member → L/R)     │
  │         ─► Σ members × density compensation ─► gate ─► osc 1 level ─┐   │
  │  osc 2: the same, independent ───────────────────► osc 2 level ───┤   │
  │  sub:   one phase, one shape, tone ─► pan ─► gate ─► level ───────┤   │
  │  noise: per-voice seeded noise, colour ─► pan ─► gate ─► level ───┤   │
  │                                                                   ▼    │
  │                                     per-voice filter slot (stereo-linked)
  │                                                                   ▼    │
  │                                              × ENV 1 × velocity      │
  └───────────────────────────────────────────────────────────────────┬────┘
                                                                      ▼
        Σ voices ─► output stage (Level, guards) ─► stereo out ─► make-up gain (the mixer's, RC-2) ─► FX rack
```

- The oscillators run at the internal rate (host rate × quality) and return to the host rate before the per-voice filter slot; the sub and the noise oscillator may run at the host rate.
- "gate" is each source's mute and solo (IF-6).
- The filter slot passes the signal unchanged until a filter is assigned (RC-1).

### 4.3 What lives where

| concern | owner | where specified |
|---|---|---|
| which voice a note gets, stealing, the pedal, polyphony, mono/legato | the engine (voice) | VO |
| amplitude envelope | the engine (ENV 1) | EN, C.12 |
| pitch: bend, glide, quantizer, MTS-ESP, transpose | the pitch system, upstream | IF-2, RC-3 |
| gravity | the engine (voice) | SW-11, C.6 |
| the swarm, blades, edges, DC | the engine (oscillator) | SW, BL, ED, C.16 |
| sub, noise oscillator | the engine (voice) | SN |
| filters' sound | the filters | RC-1 |
| source levels, pan, mute and solo, meters | the engine, driven by the mixer | IF-6, RC-2 |
| ENV 2 to 4, LFOs, matrix, macros, intents, morph | the modulation and morph systems | IF-3, IF-4, RC-4 |
| true stereo after the voice, FX, master limiter | the device | IF-7, IF-8, RC-5 |

---

## 5. Every function and parameter

### 5.1 How to read the manifest

The manifest is data (BD-5). Its keys are frozen from the first release; integer ids are assigned explicitly by the manifest and are append-only. This section is its content.

- **Key** is the stable address. Rows under 5.3 exist once per oscillator (`osc1.…`, `osc2.…`); rows under 5.4 once per blade per oscillator (`osc1.blade1.…` … `osc2.blade2.…`).
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
- **Class**: `M` Morphable, `S` Structural (flips at a morph corner boundary), `D` Device (outside the morph). [Q17]
- **Rate**: `C` control rate, ramped; `A` also an audio-rate destination; `N` read at a voice's start (VO-6) and held; `T` stepped, changed by the rule of PA-5.
- **PV**: accepts per-voice modulation. Every continuous parameter does; stepped ones do not.
- **Live when**: PA-8's table says when each parameter can affect the sound.
- Rows tagged **[Q31]** are built as written until Q31 is ruled.

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
| `mix.osc1` | Osc 1 level | dB | −∞, −60..+6 | 0 | dB | M | C | yes | osc 1's contribution to the voice. |
| `mix.osc2` | Osc 2 level | dB | −∞, −60..+6 | 0 | dB | M | C | yes | osc 2's contribution. |
| `mix.sub` | Sub level | dB | −∞, −60..+6 | −2 | dB | M | C | yes | The sub's contribution. |
| `mix.noise` | Noise level | dB | −∞, −60..+6 | −12 | dB | M | C | yes | The noise oscillator's contribution. |
| `out.level` | Level | dB | −∞, −60..+26 | 0 | dB | M | C | yes | The engine's output level into the rest of the device; 0 dB is defined in 6.9; the ceiling lets a version-0 state keep its level (OU-3). [Q20] |
| `quality` | Quality | — | Standard (1×), High (2×) | Standard | step | D, saved with the patch | T | — | Oversampling: High aliases less on FM, ring and fold with sine carriers, at about 1.5 to 1.7 times the CPU. [Q27] |

Each source also has an on switch: `osc1.on` (default on), `osc2.on` (off), `sub.on` (off), `noise.on` (off): gate, class S, rate T. A source that is off costs nothing (PF-6).

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
| `coupling` | Coupling | % | −1..1 | 0 | lin (the squared taper is in the law, C.1) | M | C | yes | Positive pulls the members toward one locked pitch (cloud, herding, shimmer, lock); negative pushes them into an even splay (a multiplied, hollower tone). Feels the same at any detune (SW-4). |
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
| `spreadLaw` | Spread law | — | gradient, random, drift, alternate, swarm | gradient | step | S | T | — | How every spread is fanned across the members (BL-13). |
| `spreadDrift` | Spread drift | Hz | 0.02..12 | 0.5 | log | M | C | yes | Drift law only: how fast the spread values wander. |
| `quantize` | Quantize | — | free, whole | free | step | S | T | — | Whole snaps the cut-rate spreads and rules to whole harmonics. |
| `stacking` | Stacking | — | blade 2 over 1, blade 1 over 2 | blade 2 over 1 | step | S | T | — | Which blade is the upper one when they interact. |
| `upperHears` | Upper hears | % | 0..1 | 0 | lin | M | C | yes | 0: both blades act on the base wave side by side; 1: the upper blade acts on the lower blade's result. |
| `collisionPitch` | Collision → pitch | % | −1..1 | 0 | lin | M | C | yes | Where the blades overlap, the upper carrier's pitch bends up or down, up to ±2 octaves at full overlap. |
| `collisionBite` | Collision → bite | % | 0..1 | 0 | lin | M | C | yes | Where the blades overlap, the upper blade's FM depth rises up to ×5 and its fold drive up to ×3 (C.11). |

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

The sub takes ENV 1 and the voice's seed; it has no envelope or seed of its own.

| key | label | unit | range | default | law | class | rate | PV | what it does |
|---|---|---|---|---|---|---|---|---|---|
| `wave` | Wave | — | sine, triangle, square, saw, pulse, noise, bump | saw | step | S | T | — | The sub's shape (SN-2). |
| `pulseWidth` | Width | % | 0.05..0.95 | 0.5 | lin | M | C | yes | Pulse only; 0.5 is a square. |
| `bumpAmount` | Bump | — | 0..0.6 | 0.35 | lin | M | C | yes | Bump only: the size of the second bump. |
| `bumpPhase` | Bump phase | rad | −π..π | −0.25 | lin | M | C | yes | Bump only: negative leads with the big bump. |
| `octave` | Octave | — | −3, −2, −1, 0 | −1 | step | S | T | — | A sub goes down only. |
| `semitones` | Semitones | st | −12..+12 | 0 | step | S | T | — | |
| `fine` | Fine | cents | −100..+100 | 0 | lin | M | A | yes | |
| `startPhase` | Phase | cycle | 0..1 | 0 | lin | M | N | yes | The sub's phase at a voice's start (SN-1). |
| `keytrack` | Keytrack | — | on, off | on | step | S | T | — | Off pins the sub to C2. |
| `tone` | Tone | Hz | 30..20 000 | 20 000 | log | M | C | yes | A gentle low-pass. |
| `pan` | Pan | — | −1..+1 | 0 | lin | M | C | yes | Places the sub in the stereo field (C.8). |
| `mute`, `solo` | Mute, Solo | — | off, on | off | step | D | T | — | As the oscillators' (IF-6). |

### 5.6 The noise oscillator (`noise.…`) [X15]

| key | label | unit | range | default | law | class | rate | PV | what it does |
|---|---|---|---|---|---|---|---|---|---|
| `colour` | Colour | — | −1..1 | 1 | lin | M | C | yes | +1 white, 0 pink, −1 brown, continuous between. |
| `pan` | Pan | — | −1..+1 | 0 | lin | M | C | yes | Places the noise in the stereo field (C.8). |
| `mute`, `solo` | Mute, Solo | — | off, on | off | step | D | T | — | As the oscillators' (IF-6). |

### 5.7 Fixed behaviours (no control)

| behaviour | value |
|---|---|
| density compensation | a swarm's members sum scaled by U^−0.75 (C.8, SW-19) [X17] |
| inertia's response | w = knob^2.5 (C.5) |
| levels of a voice | audible, faded, freed and sounding as 6.4 defines them |
| DC | removed by C.16 |
| gravity's capture radius | 35 cents (C.6) [Q31] |
| the pitch fan's curve | as C.8 [Q31] |
| host tempo and transport | inputs, not parameters (IF-2, IF-11) |
| blade envelopes | none: blades take ENV 2 to ENV 4 through the modulation system (RC-4; Appendix B writes the bank's as routes) |

### 5.8 Counts

- **Rows:** voice 18 (plus 4 on switches), oscillator 44, blade 30 (plus 2 blade-2-only), sub 12, noise 3: **109 table rows** (the sub's and the noise oscillator's `mute` and `solo` share one row each, so they hold 13 and 4 parameters).
- **Host parameters:** 18 + 4 + 2 × (44 + 2 × 30 + 2) + 13 + 4 = **251**.

---

## 6. Behaviours

Each requirement states what the player hears; where a law is the sound, the requirement points to its formula in Appendix C.

### 6.1 The swarm (SW)

| id | requirement |
|---|---|
| SW-1 | Each oscillator of each voice is a swarm of U members (`unison`), each a phase-continuous oscillator at its own frequency around the oscillator's pitch. A member's frequency may change at any time; its phase never jumps because of it. |
| SW-2 | **Placement** follows `distribution` by C.2. Seeded draws come from the oscillator's `seed`; the same seed gives the same placement. |
| SW-3 | **Detune** follows `detuneLaw`, `detune`, `detuneRange`, `harmonicReach` and `anchor` by C.3. In the cents law with even placement, `detune` 0.1 at range ×1 puts the outermost members exactly ±10 cents from the pitch. |
| SW-4 | **Coupling** follows C.1: the pull on each member is a frequency offset in Hz, of strength km = 4·K·\|K\| in units of σ, toward sync for K > 0 and toward even splay, with three times the authority, for K < 0; so the same knob position gives the same regime at any detune, register and law. With one member there is no pull (C.1). Claim anchors (7 members, JP-8000, cents law, `detune` 0.28 at ×1 (the outermost members ±28 c), A3 = 220 Hz, 44.1 kHz, Phase 0 and Random 0, no drift; R read at every control interval over the last 2 s of an 8 s note, values ±0.08): K +1.0 (km 4.0) sustains R ≥ 0.95; +0.85 (km 2.89) holds a steady partial lock; +0.7 (km 1.96) oscillates, R swinging between about 0.25 and 0.8 (its minimum and maximum over those 2 s); +0.6 (km 1.44) from an aligned start falls to no sustained lock within about 3 s; −1.0 after 4 s reads R ≤ 0.10 and R_N ≥ 0.75, with the 7th harmonic at least 5 × its uncoupled level; −0.7 reads R_N 0.44 ± 0.1. The near-critical anchors (+0.7, +0.6) are the most sensitive to the control interval and are the first place a change of regime shows. (A simulation of C.1 at these conditions with Δ = 29 samples reads σ = 2.19 Hz; R 0.965 at +1.0, 0.924 at +0.85, 0.23 to 0.80 at +0.7, 0.04 to 0.92 at +0.6.) |
| SW-5 | **Phase lag** α enters the sync pull as in C.1. Judged: the direction (positive α lowers a locked swarm's mean frequency, negative raises it) and monotonic growth; the magnitude is S·R·sin α within ±25 % at a lockable K. Anchor: SW-4's conditions at K +1.0 (S = 4σ ≈ 8.78 Hz), α 10°, 20° and 30°: the swarm stays locked (R ≥ 0.9, steady) and its mean frequency falls by S·R·sin α (a simulation of C.1: 1.42, 2.76 and 3.94 Hz against 1.47, 2.88 and 4.16 Hz). Above about 35° the cos α weakening of the pull loses the lock at these conditions, so larger angles are not an anchor. |
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
| BL-2 | **Modes** follow C.9.2 and C.9 items 3 to 6. In every carrier mode (Sync, FM reset, FM free, Ring) the carrier's phase restarts at 0 at each window entry; FM reset also restarts its modulator (and the pitch-FM accumulator); FM free's modulator runs on. Noise holds a uniform value in [−1, 1), a new one each whole carrier cycle inside the window and at each entry. Fold is sin(π/2·(1 + (k − 1)/4)·x) with k the member's effective cut rate after spreads (never divided per blade). Crush holds as many whole steps as fit the window and a last partial one when they do not fit exactly (C.9 item 7). |
| BL-3 | **Waves** follow C.9.3, the sine→saw shape included (its brightness r capped by the carrier's own frequency). |
| BL-4 | **Cut rate.** "× pitch per cycle": the carrier runs at `cut` × the member's frequency; "per blade": `cut` cycles across the window (so at `cut`/width × the member's frequency); Hz: `cutHz` whatever the note. **The carrier's frequency is capped at 0.45 of the internal rate in every mode** (a cap, not a fade; C.9 item 8). |
| BL-5 | **FM** follows C.9.2: phase FM offsets the carrier phase by (index/2π)·modulator; pitch FM bends its rate by (1 + index/10)^modulator (±1200·log₂(1 + index/10) c). The modulator runs at `modRate` × the member's frequency fᵢ, or at `modRateHz`. |
| BL-6 | **Mirror** follows C.9.5 and C.9 item 9. On a half-wave-symmetric base (sine, triangle, square) twin − removes the even harmonics (at least 100 dB down) and adds no DC, in every mode whose blade sound is a fixed function of the cycle (Sync, FM reset, Fold, Ring and Crush, with rotation 0 and the blade's values held); FM free and Noise, whose sound differs from cycle to cycle, are outside the claim. A twin window is corrected for edges (ED-1) and DC (C.16) exactly as the main window. |
| BL-7 | **Crush** follows C.9.4. Judged on the output while the member's phase is inside the window: one member at C3, Standard quality, `position` 0.25 and `width` 0.25 (the window [0.125, 0.375] holds no edge of any base wave), Edges 0.3, depth 1, the cut rate swept exponentially from 2 to 60 over 2 s, every base wave: no sample-to-sample change of the output inside the window exceeds 1.25 × the largest per-sample change of the C.9.4 law itself over the same sweep (the law evaluated on a grid 16 times finer, its largest change scaled to the run's sample interval). The law's own slews are steep (at C3, Edges 0.3, about 0.08 per sample on a sine), so the bound is relative to the law, not absolute |
| BL-8 | **Rotation.** `rotate` turns the window around the cycle at that rate in Hz; "restart per note" starts each voice's window at Position at its fresh start; "free-running" integrates the blade's resolved `rotate` rate (without per-voice offsets) on one clock per blade per oscillator, advanced by the engine's sample clock whether or not a voice sounds, and each voice's own `rotate` offsets and its members' `rotateSpread` add their integrals from the voice's fresh start; blade 2 turns with blade 1 or on its own. When a rate returns to zero the window glides home to Position by the shortest way, with a time constant of 30 ms, in both rotation modes. |
| BL-9 | **Blade 2** is a full second blade. Every "as blade 1" or "blade 1's" option makes it follow blade 1's value of that setting. With blade 2 off, the oscillator sounds exactly as with blade 1 alone. |
| BL-10 | **Frame** follows C.13: in swarm frame each member's window sits at its lead from the voice's frame angle; the frame follows ψ where the swarm has a centre and never jumps where it has none (R near 0); where it is locked the frame equals ψ. At a voice's fresh start the frame is ψ if R ≥ 0.25, else member 0's phase. The lead always reaches the blade as a ramp. What the frame does after a crossing is [X7]. |
| BL-11 | **Interplay** follows C.11. With `upperHears` 0 and both collisions 0 the output is the same whichever blade is upper. Interplay and collision act only with blade 2 on. |
| BL-12 | **Cross-mod and feedback** follow C.14: one sample late, in a ring of members, changing timbre only; the loop is low-passed so it never settles into a limit cycle at either quality. **Measure:** one member, `crossMod` 1 and `feedback` 1, each carrier mode (Sync, FM reset, FM free with a whole-number `modRate`, Ring), rotation 0, no drift or noise, at a pitch whose period is a whole number of samples P (44,100/336 Hz at 44.1 kHz), both qualities: after 1 s the output repeats with period P, the RMS of y[n] − y[n − P] over the next second below 10⁻⁴ of the output's RMS (a loop in a limit cycle, or chaotic, does not repeat). |
| BL-13 | **Spreads** follow C.10: each spread's per-member position p comes from the spread law; a single member gets p = 0; blade 2's own spreads use draws independent of blade 1's. |
| BL-14 | **A negative spread is the mirror image of the positive**, exactly, Quantize included (both roundings of C.10 item 4 go half away from zero, so −s gives exactly the negated offsets of s), rotation included. |
| BL-15 | **Cut rules** follow C.10 item 5; ratios match those lists to 0.01. An unreadable custom list is refused and reported, and the previous list stays. |
| BL-16 | **A quantized cut never changes mid-cycle.** Under Quantize (or a snapped rule), when the spread law moves a member to a different ratio the change waits until that blade's carrier next restarts at its window entry, where the carrier's phase is 0, so it is never heard as a jump. The one stated exception: a twin window wider than half a cycle is mid-window when the main window restarts, so its change still lands mid-window [X9]. Over 60 s of a held C3 (44.1 kHz, U = 7, Quantize on, cut spread 6, the drift law at `spreadDrift` 2 Hz, at `motionGrain` 1 and 0) the time-weighted share of every ratio is within ±0.001 of the share the spread law's values give without the wait, computed from the observation view's p (no tone bias). This holds at every `motionGrain`. |
| BL-17 | **No control-rate step reaches the sound**: blade position, cut, width, depth, index, rotation and the sine→saw band limit, as fed to the blade (not only as stored), arrive as ramps across each control interval, at `motionGrain` 0, and always for the frame (SW-16). Under Quantize the cut is not ramped; it changes by BL-16. |
| BL-18 | **Noise.** Each voice, and each member within it, draws its noise from its own seeded stream (DT-4), started at the voice's fresh start; one note never changes another note's noise. A noise step is a step of the sound and is band-limited like any other edge (ED-1). [X4, X5] |
| BL-19 | **DC.** No blade leaves a DC offset: C.16 removes each blade's cycle mean; with any mode, wave and spread, the output's mean over each second of a held note is within 0.003·10^(L/20) of zero, L the patch's `out.level` in dB (0.003 at 0 dB, with every source level 0 dB and velocity 1), including while the cut rate steps or sweeps. |

### 6.3 Edges and aliasing (ED)

| id | requirement |
|---|---|
| ED-1 | **Every hard step the engine makes is band-limited**: base-wave discontinuities, window and twin edges, carrier wraps (also under FM, cross-mod and feedback, where the step's time follows the phase push), Crush steps at Edges 0, noise steps. The correction is a **minimum-phase band-limited step table of 32 taps, read with interpolation**, scaled by `bandLimit` (1 full, 0 none); its exact design (window, cutoff, storage) is the builder's, chosen to meet QS-1 and QS-2. Step heights are computed from the waveform's closed form at the step, not by evaluating the whole blade twice. |
| ED-1a | **No base wave is ever rendered naive.** Every discontinuity the base wave itself makes is an edge ED-1 covers, on every member, with or without a blade, in and out of every window: the saw's and reverse saw's reset once per cycle, the square's two edges, and the triangle's two corners (by the corner table of ED-6, X3). The sine has none. At `bandLimit` 1 the plain saw base therefore meets QS-1 exactly as the saw does under a blade; only `bandLimit` below 1 lets the base wave alias, by the player's choice. Judged by QS-1 with no blade on each base wave in turn, and by QS-4's detector with its ratified numbers (its 20 ms local window included), which must read no event on a held plain-saw base at C1 and at C2: one of the detector's must-not-fire controls, which it must pass as ratified. |
| ED-2 | The correction adds no latency: it is causal, with no look-ahead, so the cross-mod and feedback taps read finished samples. |
| ED-3 | **The table's own DC.** Each edge's correction, h·r(n + δ) on the 32 samples after it (h the edge's height, δ its fractional time, r the table minus the unit step), has a nonzero area A(δ) = Σₙ r(n + δ), so the corrections add an offset that grows with pitch (about 0.63 on a ±1 saw at C8 at 44.1 kHz). It is removed by subtracting from each member's output the mean the corrections add, Ā·(fᵢ/f_s)·Σₑ hₑ, Ā the table's area averaged over δ (a constant of the table) and the sum over the member's edges in one cycle at its current values, computed and smoothed with C.16's estimate; never by delaying the smooth part |
| ED-4 | The table's cutoff keeps the top of the audible band (QS-2). The table is stored no finer than its read needs. |
| ED-5 | Each member keeps its pending correction; it is cleared or played out when a voice is reused, never left to leak into a new note; nothing is allocated for it on the audio thread. |
| ED-6 | A softened edge (Edges > 0) is a continuous gate and needs no step correction. Corners (a triangle's points, a mirrored window's centre, Crush slew corners) are not steps; triangle base and carrier corners are corrected by a corner table built like the step table; Crush slew corners are not. [X3] |

### 6.4 The voice (VO)

**Levels named once.** A voice's **envelope level** is ENV 1's output, 0 to 1, before velocity and source levels. A voice is **sounding** from its start until it is freed (a voice taken and fading under VO-3 is sounding for its whole fade). It is **audible** while its envelope level is at or above −60 dB (1e-3), **faded** below it, and **freed** when its release has finished (envelope level exactly 0) or, once released, when its envelope level falls below −80 dB (1e-4). A voice is **held** while its note's key is down (a note-on without its note-off), and **sustained** while its key is up and the sustain pedal holds it (VO-12); **gated** means held or sustained.

| id | requirement |
|---|---|
| VO-1 | One allocator serves the whole instrument. Notes are addressed by note id; a note-off or expression for an id reaches only that note's voice. A note-off or expression with note id −1 (CLAP's wildcard) addresses by channel and key: every voice whose note has that channel and key (−1 for either field matches all). Two notes on one key are two voices. |
| VO-2 | **The voice law (poly).** A new note takes, in order: (1) a faded voice that is not held (freed voices included), oldest first; (2) a voice that is not held (a releasing tail, or a sustained voice), quietest envelope level first, age breaking ties; (3) only when every voice is held, the oldest held voice. A held key is never taken ahead of a tail. A repeated note never reuses its own sounding voice, unless no other voice exists (polyphony 1). |
| VO-3 | **A taken voice fades.** When a note takes an audible voice, the old sound fades to silence linearly over 5 ms and the new note starts from silence when the fade ends. Everything about that note is late by its wait, its release included, so it sounds for as long as its key was held. While waiting it counts as held by its key (tiers 1 and 2 never pick it); a retune reaches it (its sounding pitch moves; the struck pitch stays the glide target); a panic or a voicing change cancels it with any pending release; a second take replaces the waiting note without restarting the fade. A culled tail that is taken waits no longer than the shorter of the steal fade and what the cull had left. A zero-length note on a taken voice is silent. If the cull picks a tail whose key is up but whose late release is still pending, it is released at the cull (the pending release cleared); a taken tail's pending release is cancelled by the take. A faded voice restarts at once. |
| VO-4 | **Under load.** The engine accepts a voice cap from the device: an integer from 1 to `voice.polyphony`, or none (an input, never a clock reading); by default the device sets none, so there is no CPU-adaptive limiting. Over a cap, the quietest releasing tails fade out over 8 ms, never a held voice. A note at a full cap with no tail to take is refused (the default) or, by the device's choice, takes the oldest held voice by VO-3. With no cap the engine never culls. |
| VO-5 | `voice.polyphony` sets how many voices exist, 1 to 64 in one pool (default 16). VO-2 and VO-3 hold unchanged at every setting, including when every voice is in use. Lowering it while voices sound releases the excess by VO-4's fade, never a cut. |
| VO-6 | **Mono and legato.** Mono: one voice; every new note re-runs the voice's start from its current level, with that note's own velocity, the level moving from the old velocity's to the new over 5 ms (no step). Legato: the voice re-runs its start only when a note arrives with no other key held; otherwise it is a **legato change**: the swarm and blades keep running and the voice keeps its strike's velocity and envelope. In both, the voice sounds the pitch and the expressions of the newest held note id, and releasing it while others are held returns to the most recent held one by a legato change. A note change reports which note the voice moves from and to, so the pitch system can glide (IF-2). What re-runs, by kind of start: see the table below. **A fresh start on a sounding voice.** In mono, a re-run follows `retrigger`: with it on, the table's fresh-start column re-runs and the voice's output crossfades over 5 ms with equal gain from its continuing state to the restarted one, so no waveform jump is heard; with it off, the phases continue, everything continues as on a legato change, and only ENV 1 re-runs (with the new note's velocity, ramped over 5 ms). In legato the phases always continue: legato's first note while the voice's tail still sounds continues everything as a legato change does and re-runs only ENV 1, with the new note's velocity ramped over 5 ms. |
| VO-7 | A freed voice outputs exact zeros and holds no state that a later note could hear. |
| VO-8 | Velocity (0..1) scales the voice's level by (1 − v + v·velocity), v = `env1.velocity`, velocity's default mapping; at v = 1 level is proportional to velocity. Velocity is also a modulation source (IF-1). |
| VO-9 | **The voice's path** is: each source at its level, summed, through the filter slot, times ENV 1 and velocity. Each source's signal exists separately before the sum for the filter slot's routing and the meters. |
| VO-10 | Each voice publishes its lifecycle: started (note id), released, taken (old note id, new note id, when the new one will start), freed. |
| VO-11 | **A voice that is not sounding costs nothing.** The engine renders zero member-samples, zero edge corrections and zero envelope steps for every voice that is not sounding; judged on the operation counters (IF-9), deterministically: with `voice.polyphony` 64 and one voice sounding, the counters equal those with `voice.polyphony` 1 and the same voice sounding. |
| VO-12 | **The sustain pedal.** The engine accepts the sustain pedal per channel (MIDI CC 64, pressed at a value of 64 or more; under MPE a zone's master-channel pedal holds every channel of the zone). A key released while the pedal is down keeps its voice sounding at sustain (its ENV 1 stays in its current stage, not released) and makes that voice a tail for VO-2 (taken before any held key). A key struck again while the pedal holds its earlier note takes a new voice; the earlier one stays a tail. Pedal-up releases every voice held only by the pedal. In mono and legato the pedal holds the one voice's gate: releasing keys under the pedal changes neither its note nor its envelope; a new key still changes the note (mono re-runs, legato glides); at pedal-up with no key held the voice releases, and with keys held it moves to the most recent held key by a legato change. **MPE:** the shell tells the engine whether MPE is on and each zone's master channel (1 for the lower zone, 16 for the upper), from the host's MPE configuration or the device's setting; without MPE each channel's pedal holds only that channel's voices. |
| VO-13 | **All notes off and all sound off.** The engine handles the host's all-notes-off (MIDI CC 123, or CLAP's all-notes note-off) as a key-up of every note and a pedal-up: every voice releases through ENV 1 and its tail rings out as usual. It handles all-sound-off (MIDI CC 120) as an immediate stop: every voice, waiting notes included, fades to silence over 5 ms and is freed; the pedal's state is cleared. Neither resets the ensemble's carried offsets or any stream; gravity's settled pitches go with their voices. There is no panel control for either. |
| VO-14 | **Changes while notes sound.** A `voice.mode` change fades every voice over 5 ms and frees it (as all-sound-off). A `quality` change fades every voice over 5 ms, frees it, and continues at the new rate. A `voice.polyphony` change follows VO-5. |

**What re-runs at each kind of voice start** (VO-6):

| state | a fresh start (poly; mono's first note; legato's first note from silence; a mono re-run with `retrigger` on, which crossfades by VO-6) | a legato change (also a mono re-run with `retrigger` off and legato's first note over a tail, except that ENV 1 re-runs, VO-6) |
|---|---|---|
| start phases (SW-10) | re-run (`retrigger` on) or resume (off) | continue |
| onset lock burst (C.1) | re-run | continue (no new burst) |
| onset scatter (C.7) | re-run (the carried offsets update) | continue (no new waits) |
| rotation clock (BL-8, restart per note) | restarts at Position | continues |
| random spread targets (C.10 item 2) | redrawn; a sounding voice glides to them over 20 ms | kept |
| drift-law spread positions (C.10 item 3) | redrawn from the stationary spread | continue |
| noise streams (BL-18, SN-4) | restarted from the note's seed | continue |
| the sub's phase (SN-1) | restarts at `startPhase` | continues |
| gravity entry (C.6) | the voice enters at its played pitch | the voice's settled pitch rides the new pitch (C.6) |
| member drift (C.4) | restarts at 0 | continues |
| inertia (C.5) | set to the targets (no swing) | continues swinging toward the new targets |
| the frame (C.13) | set (ψ if R ≥ 0.25, else member 0's phase) | continues |
| the note counter and the note's streams (DT-4) | a new count | the same |
| ENV 1 | re-runs its attack from the current level | continues its stage |
| velocity | the new note's (ramped over 5 ms in mono) | the strike's, kept |

### 6.5 ENV 1 (EN)

| id | requirement |
|---|---|
| EN-1 | ENV 1 is an attack–hold–decay–sustain–release envelope whose timed stages follow C.12: each stage takes exactly its time and lands exactly on its target (attack on 1, decay on sustain, release on 0), with no step at a stage's end; its shape is set by its curve. The modulator bank's envelopes follow the same law (RC-4). |
| EN-2 | A voice's start from silence starts ENV 1 at exactly 0. A re-run (mono, legato's first note, a re-struck voice) restarts the attack from the envelope's current level, never from 0 (C.12). A stolen voice's new note starts from silence (VO-3). |
| EN-3 | Changing a time, curve or sustain while a stage runs continues the stage from where the level stands (C.12); the level never jumps. |
| EN-4 | ENV 1 is the same in seconds at every sample rate and independent of block size. |

### 6.6 Parameter changes and modulation (PA)

| id | requirement |
|---|---|
| PA-1 | **No zipper.** A change of any continuous parameter from the host, automation, a macro, an intent or the morph glides to its new value through a one-pole smoother with a 12 ms time constant, in the normalised form v (the shortest way round for circular parameters), snapping to the target once within 10⁻⁶ of it; a full-range jump in one event makes no click by the detector of QS-4. |
| PA-2 | **Per-voice modulation, one mechanism.** For every continuous parameter, the engine accepts per voice an offset in the parameter's normalised 0..1 form, addressed by note id and cleared at that note's fresh start. The value a voice uses is the parameter's resolved value plus the sum of its offsets, clamped to [0, 1] (wrapped for circular parameters), mapped by the parameter's law. An offset for all voices at once is the same mechanism with every voice addressed. |
| PA-3 | **Control rate.** Offsets arrive and take effect at the control rate: the interval is the whole number of samples nearest 1/1,500 s at the host rate (29 at 44.1 kHz, 32 at 48 kHz, 64 at 96 kHz), each change ramped across one interval. Intervals are counted from the engine's own sample clock since reset, never from the host's block boundaries; a voice starting mid-interval sets its values at its start and joins the clock's next interval. Events with the same sample time apply in the order received, before that sample renders. |
| PA-4 | **Audio rate.** These destinations also accept a per-sample offset: the voice pitch, the oscillator `pitch` and `fine`, the sub's `fine`, the blades' `cut`, `cutHz` and `fmDepth`. Every other parameter is control-rate, ramped. |
| PA-5 | **Stepped keys while a note sounds.** A stepped setting of rate T (mode, wave, mirror, frame, cut-rate and mod-rate units, base wave, distribution, detune law and range, pivot, the spread law and rules, quantize, the pan image, a blade's or source's on switch) changes on sounding voices at once, through an equal-gain crossfade of 5 ms from the old setting's sound to the new; the same holds when the morph flips one. During the crossfade both settings render from one shared member state (the same phases and swarm), which then continues under the new setting; the engine is sized for this (RT-1). A second change that arrives during a crossfade waits until it ends and then crossfades from the then-current setting to the newest value (changes in between are skipped), so at most two settings ever sound at once. Keys of rate N take effect at the next voice start. `octave` and `semitone` change the frequency at once with every phase continuous, so they need no crossfade; `retrigger` acts only at the next voice start, so it needs none; `mute` and `solo` follow IF-6. |
| PA-6 | A change of `unison` while a note sounds fades new members in and removed members out over 5 ms; the remaining members continue unbroken; their seats and the U^−0.75 scale move to the new count's over the same 5 ms. |
| PA-7 | **Host values are checked where they enter.** A non-finite value is refused; a value out of range is clamped to it; an unknown option of a stepped key is refused; each refusal and each clamp is counted and readable. No value the host can send puts the engine in a state no control can reach. |
| PA-8 | **Live and inert.** A parameter is **live** when its row's condition (below) holds: then there is a setting of the other parameters, and a note sequence, under which changing it moves the output. When the condition fails it is **inert**: no change of it moves the output, whatever the other parameters and the notes. |

**Live when** (PA-8; a parameter not listed is live whenever its source is on; `phase` and `phaseRandom` are live at every `retrigger` setting, since a start with nothing to resume uses them):

| parameter(s) | live when |
|---|---|
| every blade row | that blade is on; blade 2's "as blade 1" options read blade 1's value |
| `cut` | `cutIn` is per cycle or per blade |
| `cutHz` | `cutIn` is Hz |
| `fmType`, `modShape`, `fmDepth`, `modIn`, `fmIndexSpread` | the blade's mode is FM reset or FM free |
| `modRate` / `modRateHz` | as above, with `modIn` × pitch / Hz |
| `shape`, `shapeSpread` | the blade's wave is sine→saw |
| `edges` | the blade's width is above 0.004 |
| `ruleDepth` | `cutRule` is not even |
| `cutSpread` | `cutRule` is even |
| `ratios` | `cutRule` is custom |
| `spreadDrift` | `spreadLaw` is drift |
| every spread | U > 1 |
| `upperHears`, `stacking`, `collisionPitch`, `collisionBite` | both blades are on |
| `crossMod` | a blade is on in Sync, FM or Ring mode (at U = 1 the member is its own neighbour, C.14) |
| `feedback` | a blade is on in Sync, FM or Ring mode |
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
| `bumpAmount`, `bumpPhase` | the sub's wave is bump |
| `semitones`, `octave`, `fine`, `startPhase`, `keytrack` (sub) | the sub's wave is not noise |
| `env1.hold` | always (ENV 1 is always on) |

### 6.7 Determinism (DT)

| id | requirement |
|---|---|
| DT-1 | The same state and the same sample-timed event stream give identical output, sample for sample, on one build. Bit-identity across platforms or builds is not required. |
| DT-2 | The output is bit-identical however the host splits the same events into blocks (any block sizes from 1 to 4,096, including uneven splits). |
| DT-3 | At different sample rates the same patch gives the same sound, not the same samples: between renders at 44.1, 48 and 96 kHz, the level of each of the first 16 harmonics of a held note (where below 18 kHz) agrees within 0.5 dB, and every note and envelope event time agrees within 1 ms. Judged on every bank preset with no Noise blade, no noise source, no drift and neither the random nor the drift spread law (the others are reported), on a held C4, each harmonic's level the power within ±4 bins of its frequency in a 1 s window (4-term Blackman–Harris) starting 1 s after the note-on. |
| DT-4 | **Every random draw comes from a mulberry32 stream** named by its purpose. Per oscillator, persistent until reset, a seed change or a load: placement (C.2) and the ensemble (C.7), seeded from the oscillator's `seed` and the purpose. Per note, started at the voice's fresh start: start phases, drift, the spread law, blade noise, and the noise oscillator and the sub's noise (both from osc 1's seed, "the voice's seed"), each seeded from the seed, the purpose, the note counter and the member. Never from a parameter's position in a list, never from how many draws another voice made. **A note-on never changes a voice already sounding**: not its random numbers, not its samples. |
| DT-5 | The engine reads no clock and depends on nothing outside its state and its inputs. |
| DT-6 | After `reset()` the engine renders exactly as a freshly prepared instance with the same state. |

### 6.8 Real-time safety (RT)

| id | requirement |
|---|---|
| RT-1 | Processing, parameter and modulation input, note events, observation reads and `reset()` allocate nothing, take no lock, make no system call or I/O, and run no unbounded loop. Everything is allocated at prepare, sized by the maxima: 64 voices, 16 members, both oscillators, both blades, room for PA-5's second setting on every oscillator, and a second voice state for VO-6's mono re-run crossfade. Every lazily built table is built at prepare. |
| RT-2 | The work per sample is bounded by a constant set by the parameter maxima. At most 8 edges are corrected per member per internal sample; any further edge in that sample is left uncorrected and counted. |
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

| id | requirement |
|---|---|
| SN-1 | **The sub** is one phase, one shape, no members, no coupling, no drift. Its frequency is the voice's published pitch (keytrack on) or C2, 65.41 Hz (off), times 2^(octave + semitones/12) and 2^(fine/1200), capped at 0.45 of the rate (a cap, not a fade). **The sub integrates its own frequency; at a voice's fresh start its phase is `startPhase` (cycles) and the voice's master phase is 0, so the two start together; it stays phase-continuous under every change** (a glide, a bend, gravity, keytrack, octave). It is panned by C.8's single-source law. |
| SN-2 | **Sub shapes**, with θ its phase in cycles: sine sin 2πθ (exact); triangle as C.9 item 2; saw 2·frac(θ) − 1 (starting at −1); pulse, with width w: 2w − 2 while frac(θ) < w, else 2w (zero-mean; w = 0.5 is the square ±1); noise (white, uniform in [−1, 1), from the voice's noise stream, ignoring pitch); bump, y = (sin 2πθ + a·sin(3·2πθ + φ)) / peak(a, φ), peak(a, φ) the true maximum of \|sin x + a·sin(3x + φ)\| over a period, so the shape peaks at exactly 1 for every (a, φ), band-limited by construction while 3·f is below Nyquist (at the defaults the trailing bump is 0.882 of the leading one and the trough between them 0.637 of the peak). Every step of the saw and pulse is band-limited by the engine's one edge correction (ED-1). Level −∞ is exact silence. |
| SN-3 | The sub's tone is a one-pole low-pass, bilinear-prewarped so its response is the same at every sample rate (30 Hz to 20 kHz, clamped to 0.45 of the rate). The sub's signal goes through the tone, then joins the voice, where ENV 1 acts on it with the rest (the envelope after the tone). |
| SN-4 | **The noise oscillator** is per voice, mono, from the voice's own noise stream (so one note never changes another's noise): uniform white noise shaped by `colour` (+1 flat, 0 falling 3 dB per octave, −1 falling 6 dB per octave, continuous between, from 20 Hz to Nyquist; each slope within ±0.5 dB per octave, measured in octave bands from 40 Hz to 16 kHz over 10 s), at a level independent of colour within 1 dB, panned by C.8's single-source law. It shares the voice's allocation, ENV 1 and filter slot. [X15] |

---

## 7. Interfaces to the rest of the instrument

### 7.1 Interfaces

The engine's API is a set of calls and timed events; everything crossing it is in the units of section 5. The suite's harness header, written by the test author from this section, fixes the exact signatures; the builder implements that header.

| id | area | the engine exposes or accepts |
|---|---|---|
| IF-1 | **Notes, voices, expressions** | Accepts note-on (note id, channel, key, velocity), note-off (note id, release velocity), the sustain pedal (VO-12), all-notes-off and all-sound-off (VO-13), channel pressure, and per-note expressions, each sample-timed. Expressions are addressed like note-offs (VO-1). CLAP's expressions map so: tuning → that note's pitch (IF-2); volume → a per-voice linear gain on the voice's output (0 to 4, as CLAP sends it); pan → an offset added to every source's pan for that voice; vibrato, expression, brightness (MPE timbre) and pressure (MPE per-note pressure) → per-voice modulation sources. **Velocity and aftertouch** (channel pressure, and per-note pressure under MPE) are modulation sources; velocity's default mapping is `env1.velocity` (VO-8); aftertouch's is [X21]. Pressure never reaches the level unless a route maps it, so a keyboard's aftertouch cannot silence held notes. In mono and legato the voice follows the expressions of the newest held note id (VO-6). Publishes VO-10's lifecycle events and the load readouts (voices culled, refused, taken). [Q2] |
| IF-2 | **Pitch** | **Pitch is addressed by note id**, like the expressions: the pitch system sends each note's pitch in Hz (bend, note lane and glide, quantizer, MTS-ESP, transpose and per-note tuning already applied), at audio rate when it moves. Until a note's pitch is set, the note plays 12-TET from its key (A4 = 440 Hz) × 2^(tuning/12), tuning the note's tuning expression in semitones. The order is: the input, then gravity's ratio (the **sounding pitch**), then each oscillator's transposition. In mono and legato the voice follows the pitch of the newest held note id. Accepts the host's tempo and transport (RC-7). Publishes each voice's sounding pitch and **master phase** (the integral of the sounding pitch from 0 at the voice's fresh start) for the sub, the noise oscillator and keytracked filters. Gravity's settled ratio rides the input multiplicatively (C.6), so a bend or glide keeps the interval gravity settled. [Q6, Q7] |
| IF-3 | **Modulation, the intent bus and macros** | Every continuous parameter is a destination by its manifest key and id, with unit, range, law, class and rate (section 5). Per-voice offsets (PA-2) at the control rate (PA-3), audio-rate offsets on the destinations of PA-4. Every modulation destination is also an automatable host parameter, because wrappers drop CLAP's parameter-modulation events. Sources the engine publishes per voice: R and ψ of each oscillator's swarm, the onset burst's level, gravity's settled offset in cents, velocity, and the voice's expressions (IF-1). Macros and intents bind by key and curve (`lin`, `exp`, `log`) and need nothing more. [Q12] |
| IF-4 | **Morph and state** | The manifest's class column is the morph's input: Morphable values blend; Structural values flip at the morph's seeded point (hysteresis band 0.1) and change on sounding voices by PA-5; Device values stay out. The engine reports the heard value of every stepped key per oscillator, so the GUI shows what is heard where the morph and the stored patch differ. **State** is a key-value document carrying `version` 1, the parameter values (in their normalised form v), the `ratios` text and the quality setting: saving, loading and saving again gives byte-identical text and bit-identical renders; floats round-trip exactly; live state (tails, ramps in flight, meters, latches, stream positions) is never saved; a document without `version` is the version-0 format (B.2). A load made while not processing is whole when it returns; a load while voices sound fades them over 5 ms and starts fresh (the engine's memory is reset: streams rewound from their seeds, gravity, the ensemble's carried offsets and the resume snapshot cleared). A saved project restores as saved. **The text:** version 1 is a UTF-8 JSON object with no whitespace and exactly the members `version` (1), `params` (manifest key → v, in ascending manifest id order) and `ratios` (`{"osc1": text, "osc2": text}`), with any keys and members this build does not know kept after them (RC-9.3), each number written as the shortest decimal that reads back as the same value; version 0 is a JSON object of B.2's keys → numbers (and `kCustom` → text) with no `version` member. A set of version-0 example states is supplied to the test author as input data with the reference renders. [Q15, Q17] |
| IF-5 | **The per-voice filter slot** | RC-1. |
| IF-6 | **Mixer, mute and solo, meters** | Levels are named by the rule Gain = a stage's multiplier, Level = a contribution to a mix, Volume = the master only. **Mute and solo** are each source's gate, driven by the device mixer: a muted source is silent; while any source is soloed, every source that is not soloed is silent; a soloed source that is also muted is silent; the gate opens and closes by a 5 ms ramp, and a gated source keeps rendering, so un-muting is phase-continuous. **Meters** (peak, and a latching clip flag that catches a single sample over 0 dBFS, readable and clearable from another thread): each source after its level, pan and gate, times its voice's ENV 1 and velocity, summed over voices (a metering tap only; the filters are not in it); and the engine's output. The mixer's make-up gain, between the engine's output and the FX rack's first slot, restores loudness (RC-2); the engine does not. [Q20] |
| IF-7 | **The FX rack** | RC-5. |
| IF-8 | **True stereo** | Members are mono and panned (SW-18); the stereo field after the voice is the device's. The engine exposes each oscillator's width and pan to whatever width equation the device settles; it guarantees no member is polarity-inverted between channels. [Q26] |
| IF-9 | **Quality and CPU** | `quality` is per patch (saved), Device class, never morphed (VO-14 for changes while notes sound). **Operation counters**, deterministic and readable: member-samples rendered, edges corrected, edges left over (RT-2), envelope steps, voices sounding. [Q27] |
| IF-10 | **The labs and the observation view** | A WebAssembly build of the same source, with the same API, which a lab loads in an AudioWorklet, built with one pinned toolchain version (the exact version is chosen with the human's word at the lab stage). An observation view, read without changing the audio, per voice and oscillator: members' phases, natural and coupled frequencies, σ, S, P, Kenv, drift values D, spread positions p, effective blade values (C.9 item 8), pan gains, levels, R, ψ, the frame angle, gravity's ratios, onset waits, and the heard stepped values. The invariant suite's law checks (10.1 item 0) read it. No lab carries its own engine. |
| IF-11 | **The host (CLAP)** | Through the shell: the manifest generates the host parameter table, committed as a lockfile (id, key, min, max, default, taper, steps), ids explicit, append-only, below 0xB00000. **Every host parameter's value is its normalised v ∈ [0, 1]** (the law maps it; stepped keys in steps of 1/(n − 1)); its display shows the unit, and v = 0 of a dB row shows −∞. Activation refuses a sample rate that is non-finite or outside 8,000 to 768,000 Hz (the certified range is 44.1 to 192 kHz); the engine takes host tempo and transport as inputs (RC-7). [Q31] |

### 7.2 Reserved contracts

Each module still to come meets the engine through a contract that exists now. For each: what the engine provides and accepts now; what each voice hands over or accepts, where it applies; what is deferred to the module; and what the module must not require of the engine. RC-6 to RC-11 also carry numbered requirements (RC-6.1, …), tested by the suite where they can be tested now (10.1 items 18 to 23).

#### RC-1. The filters

- **Provided now.** A per-voice filter slot between the sources and ENV 1 (4.2), passing the signal unchanged while empty. The builder builds the slot and nothing in it.
- **What each voice hands the slot:** each source's signal after its level, pan and gate (stereo, per voice), and their sum; the voice's sounding pitch (for keytracking), master phase, velocity, per-voice modulation and lifecycle (start, release, take, free). The slot returns one stereo signal per voice, which ENV 1 then scales.
- **Deferred:** the filter types, their count and how sources route to them [X13] (built until ruled: two filters per voice, each source routed to either, both or neither, serial or parallel); per-filter cutoff tracking of R.
- **Must not require:** a filter must process each voice's stereo pair with the same coefficients on both channels, add no latency (or declare it so every parallel branch is aligned), clear its state when its voice is reused, and run no unbounded work per sample; it must not need the members separately.

#### RC-2. The mixer's gain staging

- **Provided now.** The four source levels (dB), each source's pan, mute and solo (IF-6), the source and output meters with their clip latches, `out.level`, and the sum of the voices; the meters' readings, for display, in the snapshot (RC-8).
- **The make-up gain sits before the FX rack.** The mixer's **make-up gain**, a user control, takes the engine's output to the rack's first slot (RC-5). Its default is ×4 (+12.04 dB): with it, the rack receives 0.56 × the sum of the voices at `out.level` 0 dB. The engine's output stays linear and unclipped (OU-1), and so does the make-up, so the level reaching the rack may exceed 0 dBFS: the rack's input meter and clip latch show it, and the master limiter after the rack holds the device's peaks.
- **The gain-staging view.** The mixer panel shows the level as it moves through the chain, with a gain control at every stage (each source's level, each filter slot's input and output, `out.level`, the make-up, each FX slot's input and output, the master's input and volume) and, at every stage, a meter with a peak hold and a clip lamp that latches a single sample over 0 dBFS until cleared: each source after its level, pan and gate; each filter slot's input and output (the filters' own meters); the engine's output; the make-up gain's output, which is the rack's input; each FX slot's input and output; and the master stage (limiter in, gain reduction, out). Clicking a lamp clears it. The engine supplies the readings for its own stages (IF-6, RC-8); the filters, the rack and the master stage supply theirs.
- **Deferred:** the mixer panel and its drawing, its faders' display laws, the make-up gain's range and any automatic mode [B499], the filter and FX slots' input and output gains (the filters' and the rack's), and any bus routing beyond the per-voice filter slot.
- **Must not require:** a gain stage inside the engine beyond those listed (the make-up gain is the mixer's); a meter tap after the filter slot (the mixer reads the filters' own meters).

#### RC-3. The pitch system

- **Provided now.** The per-note pitch input (IF-2), the 12-TET fallback, gravity's place (after the tuning, before each oscillator's transposition), the note-change reports of VO-6, and the published sounding pitch and master phase.
- **Deferred to the pitch system:** the bend lane and its laws, the note lane and glide (including the bank's glide values: each bank preset's `glide` time and `glideAlways` mode, B.1, among them the 12 bank presets that play legato), the quantizer, MTS-ESP, transposition by the device, and per-patch bend range [Q6, Q7].
- **Must not require:** the engine to glide, bend or quantize; the engine to know a scale; a pitch input slower than control rate when it moves.

#### RC-4. The modulator bank

- **Accepted now.** Per-voice offsets by note id (PA-2), at the control rate (PA-3) and at audio rate on PA-4's destinations; sources published per voice (IF-3).
- **The modulator bank's envelopes** (ENV 2 to ENV 4) follow C.12's law, as ENV 1 does, and offer a **one-shot** mode in which a key-up does not end the attack or decay (the envelope runs its stages to their end). PROPOSED: the bank's blade envelopes translate to ENV 2 and ENV 3 in one-shot mode (B.1), so their swells survive short notes.
- **Default routes** of the init patch and the factory presets: velocity through `env1.velocity` (VO-8); aftertouch [X21].
- **Deferred:** the LFOs, the matrix, macros and the intent bus themselves.
- **Must not require:** the engine to compute an envelope or LFO other than ENV 1; a route depth outside the destination's normalised range; any destination not in the manifest.

#### RC-5. The FX rack

- **Provided now.** One stereo pair at the host rate with zero latency, the engine's output after the mixer's make-up gain (RC-2), into the rack's first slot; the silent report and skipped-samples rule (OU-4). The engine offers no insert point of its own [Q23].
- **Deferred:** the rack, its modules, and the device's master limiter (which holds the device's peaks, OU-1).
- **Must not require:** a peak guarantee from the engine (OU-1, QS-8), a sidechain from inside a voice, or a latency from the engine.

#### RC-6. A source slot in the voice

- **Provided now.** The voice holds its sources through one contract, the **source slot**. osc 1, osc 2, the sub and the noise oscillator are the slot's first four instances and are built through it, so a later source (a formant source, the blade-to-blade controls of a later blade version, any other engine) is added beside them without restructuring the voice.
- **What each voice hands a source:** its published sounding pitch and master phase (IF-2), to which the source adds its own transposition; the gate (held, sustained, released, taken, freed: 6.4); velocity; the note's expressions (IF-1); the per-voice modulation offsets of the source's own parameters (PA-2, PA-4); the control-interval tick (PA-3); the voice's note counter and seeded streams under the source's own purposes (DT-4).
- **What a source hands the voice:** its per-voice stereo signal, panned by its own law, before its level and gate (VO-9); a published phase where it has one (as the sub's, SN-1); its readouts (RC-8) and observation values (IF-10); its operation counters (IF-9, VO-11); its parameters, in the manifest under its own id range.
- **Requirements.**
  - **RC-6.1** The four sources are instances of the slot: the voice's code names no source kind, and each source's parameters occupy a contiguous manifest id range of their own, append-only within it (RC-9.1).
  - **RC-6.2** A source allocates nothing after prepare, reads no clock, and reaches no other source's state except through what the voice hands it: a source that follows another's phase reads the voice's published phase, never the other source.
  - **RC-6.3** A source that is off renders nothing and costs nothing (PF-6, VO-11).
  - **RC-6.4** Adding a source changes no other source's output. Tested (10.1 item 18) by a test build that adds a fifth source through the slot (a plain sine at the voice's pitch): the four existing sources render bit-identically with and without it, and its counters read zero while it is off.
- **Deferred:** the later sources themselves; a fifth source's place in the mixer (RC-2).
- **Must not require:** a change to the voice law, ENV 1 or the filter slot; another source's internal state; an allocation by the voice; a second voice for one note.

#### RC-7. Tempo and transport from the host

- **Accepted now.** The host's tempo (beats per minute), time signature, song position (in quarter notes) and play state (playing or stopped, and the loop's bounds when looping) as **sample-timed transport events**, like note events (IF-1): each event's values hold from its sample on. Nothing reads a clock (DT-5); every tempo-dependent behaviour is a function of these events and the sample count.
- **Without a host transport:** 120 beats per minute, 4/4, stopped, at position 0, until the first transport event.
- **Jumps and loops:** a position that does not follow from the previous event's position, tempo and the samples elapsed (a locate, a loop's wrap) is a **jump**, delivered to the consumers as a jump at that sample and never interpolated.
- **Requirements.**
  - **RC-7.1** Transport events take effect at their sample time, independent of the host's block size (DT-2).
  - **RC-7.2** Between events, while playing, the position advances as position(n) = position(n₀) + (n − n₀)·tempo/(60·f_host) quarter notes; while stopped it holds.
  - **RC-7.3** In 1.0 no sound of the engine depends on the transport: a render is bit-identical with and without any transport events (10.1 item 19).
- **Deferred:** the consumers: the arpeggiator and the shared quantizer (in the 1.0 roster, outside the engine), tempo-synced LFOs, synced glide, a tempo-grid detune law (Q31).
- **Must not require:** the engine to learn the transport by any path but these events; a clock; a look at events that have not yet arrived.

#### RC-8. A readout path for the GUI

- **Provided now.** A **snapshot** the audio thread publishes at the end of every processing call, and within a longer call at the first control-interval boundary after each 1/60 s of audio, which the GUI thread reads without blocking. It holds: per voice and oscillator, R and ψ, the members' phases and levels, each blade's window (position and width) and the frame angle; per voice, its level and ENV 1's stage; the output's clip and non-finite latches (IF-6, RT-3); the meters: each source meter's and the output meter's peak since the previous frame, and their clip latches (IF-6), from which the GUI draws the meters' ballistics and peak holds (RC-2); the operation counters (IF-9).
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
  - **RC-11.1 No hidden user state.** Every audible value a user can change is a parameter in the manifest (each oscillator's `seed` included) or a member of the saved state (the `ratios` text). The morph position, the macro values and the intents are the morph, macro and intent systems' own state (RC-4); they reach the engine only as parameter values, which a snapshot holds. Gravity's settled ratios, tails, ramps, stream positions, meters and latches are performance state, not user state: never saved (IF-4) and left as they stand by a restore (RC-11.4).
  - **RC-11.2 Snapshot and restore.** The engine takes a snapshot of its state (every parameter's v and the `ratios` text) into a buffer allocated at prepare, and restores one from such a buffer, both deterministic and real-time safe (RT-1): no allocation, lock or system call, so the shell may take one per undo step and restore one during playback. The snapshot converts to and from IF-4's text outside the audio thread.
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
| QS-1 | **Aliasing, edges.** At Standard quality, 44.1 kHz, one member, base saw with no blade and with a Sync blade (saw carrier, cut 6, width 0.25): alias power below harmonic power by at least **110 dB at C2, 104 dB at C4, 98 dB at C6 and 93 dB at C8**, counted over every non-harmonic bin from 20 Hz to 20 kHz; aliases between 20 kHz and Nyquist are reported, not judged. Method: the note's frequency puts an odd whole number of cycles in a 65,536-sample window, analysed with a **rectangular window** (the whole cycle count makes it leakage-free, so each harmonic sits in its own bin and every other bin holds only alias), in double precision; **the alias ratio is capped at 140 dB** (the float output's floor); a naive saw is the must-fail control (it reads 9 to 27 dB). **A reference design that meets all four floors and QS-2** (a reference, not a mandate): a 32-tap minimum-phase step table from a windowed sinc with cutoff 0.45·f_s and a Kaiser window of β 10, read with interpolation, which measures 134.8, 130.0, 123.7 and 115.4 dB on the plain saw at C2, C4, C6 and C8, and −1.5 dB on QS-2. The floors may be raised on measurement. The Sync-blade case has not yet been measured against them; the suite measures it first |
| QS-2 | **Aliasing, the top of the band.** At Standard quality and 44.1 kHz, the edge correction keeps 16 to 20 kHz on a single-member saw at C6 within 3 dB of an ideal band-limited saw (target 1 dB), with QS-1 met at the same time. |
| QS-3 | **Aliasing, what the table cannot reach**, measured and reported per bank preset at both qualities by comparing a render with renders at four and eight times the rate (bins classed clean, folding or dynamics): FM, ring and fold with sine carriers at Standard quality alias by design; High quality reduces them. Reported, not judged, until a floor is ratified. |
| QS-4 | **Clicks.** The click detector is the test author's first deliverable, built and ratified before any engine stage, and **calibrated first on the reference renders** (input data for the test author, 2). **Its method and numbers (ratified):** per channel, the render is low-passed at 0.75 × Nyquist (a causal 8th-order Butterworth) and its first difference is predicted; a short-term linear predictor of order **p = 24** is refitted every 1.25 ms on the **10 ms** before each block (the autocorrelation method with a Hann window and a white-noise correction of 10⁻⁵), crossfaded between fits; a pitch predictor (the lag of the residual's autocorrelation peak, from 24 Hz up; a 33-tap fractional delay fitted over 40 ms, refitted every 10 ms) follows one period; the residual is, per sample, the smaller of the two predictors' residuals; a sample's score is its \|residual\| over the residual's RMS in a **20 ms** local window centred on it (its own footprint excluded), that level floored at 60 dB under the analysed signal's peak; a click is a score above **k = 10**, and hits within **1.5 ms** are one event. It is level-relative: a render scaled by any gain reads the same events. Its remaining constants are the suite's, as ratified. **Controls:** must-fire, steps of 0.05 of the render's peak planted at random positions on a sine, on a band-limited saw at C2 and C6 and on Crushed bells at C4, and steps of 0.05 of full scale on Crushed bells, each caught 10 of 10; must-not-fire, held notes with no event (the reference renders' and the bank's) and a plain-saw base at C1 and C2 (ED-1a), no event. A patch whose own sound is made of control-rate steps as large as the plant (Glass horde pad, the Breathing pad, a Crushed bells chord) is beyond a per-sample detector; its steps are judged by BL-16 and by ear. With it: a voice from silence, a stolen voice (VO-3), a culled tail (VO-4), a stepped-key change (PA-5), a morph flip, a full-range parameter jump (PA-1), a quantized cut change (BL-16), a frame crossing (BL-10), a mono re-run (VO-6), a mute or solo (IF-6) and all-sound-off (VO-13) make no detected click; the swarm's own grain (SW-16 at 1) is measured and reported, not judged. |
| QS-5 | **Headroom.** OU-2: the bank's four-note chords at velocity 0.85 at the default level peak below 0 dBFS, on every preset. |
| QS-6 | **DC.** On every bank preset and on generated patches, the output's mean over any second of a held note is within 0.003·10^(L/20) of zero, L the patch's `out.level` in dB (BL-19). |
| QS-7 | **Silence and denormals.** After the last voice is freed, every output sample is exact zero; across the bank's tails no output sample is subnormal. |
| QS-8 | **Bounded output, no peak guarantee.** Over the whole parameter space (fuzzed, with extreme values and automation sweeps), every output sample is finite and below a peak bound per configuration class, with the bank's own peaks as the anchor; the bounds are proposed by the test author and ratified with the suite. The engine guarantees no peak at or below 0 dBFS: a sample over it is latched and reported (IF-6), and the device's master limiter holds the device's peaks. |
| QS-9 | **Long-run stability.** A 4-hour soak of reference patches keeps level, DC and spectral centroid within bounds proposed by the test author and ratified with the suite. |
| QS-10 | **Mono fold.** The mono sum of each bank preset is reported against its stereo power (the loss in dB). |

---

## 9. Performance standards

| id | standard |
|---|---|
| PF-1 | **The CPU target.** The heaviest bank patch (Glass horde pad, translated), 8 voices, Standard quality, 44.1 kHz, 128-sample blocks: about **10.7 % of one Apple M3 performance core** for its one swarm oscillator. The arithmetic: the sources' 34 % of a min-spec core, less about 2 % for the sub, shared by two swarm oscillators, is about 16 % each, and 16 % ÷ an assumed ×1.5 (M3 to min-spec) = 10.7 % of an M3 core. A miss is reported with what would close it, never hidden. |
| PF-2 | **The whole device.** Two swarm oscillators each on the heaviest bank patch, the sub on, and per-voice modulation running on every continuous parameter of both oscillators at the control rate (and on PA-4's destinations at audio rate), 8 voices, Standard: the sources' share is at most 34 % of a min-spec core (the device's envelope is under 50 %: sources 34 %, FX 12 %, modulation 2 %, master 1 %, headroom 1 %). Min-spec is an Apple M1 base or a 4-core 2018-class Intel ultrabook, Windows x64 AVX2; an M3 figure converts by the assumed ×1.5 until measured [Q29]. |
| PF-3 | **How it is measured.** A Release build of the shipped code; 44.1 kHz reference, 48 kHz secondary; 128-sample blocks; the patch's notes (keys 48 + 3k, velocity 0.85) all held before the first block; 0.25 s untimed, then 2 s timed; best of 5 repeats, interleaved across the cells measured; the result is the time per second of audio as a share of one performance core, with a calibration loop (10⁸ dependent multiply-adds) timed in every repeat to confirm the core; a load guard before each repeat (the one-minute load average at most 3.0 and no other process above half a core, else wait 20 s and re-check, for up to 10 minutes, then refuse), never bypassed. CPU is measured and reported, never a pass-fail gate of the suite. |
| PF-4 | **Beyond the bank.** The CPU is also reported on generated patches (combinations the bank lacks), under per-voice modulation, and across a morph between corners with different stepped keys; no patch falls to a path more than 1.5 times slower than the bank's median per member-sample without that being reported by name. |
| PF-5 | **Polyphony.** Up to 64 voices; default 16. Cost grows linearly with the voices sounding and their members; a voice that is not sounding costs nothing (VO-11). The CPU target (PF-1, PF-2) is measured at 8 voices. |
| PF-6 | **Cost when off.** Blade 2, the twins, cross-mod, feedback, interplay, collision, a source that is off, the sub and the noise oscillator when off cost nothing measurable (under 1 % of the patch's CPU each). |
| PF-7 | **Memory.** Everything is allocated at prepare (RT-1). The worst-case memory per instance, at 64 voices of 16 members, is stated in the manifest and reported. |
| PF-8 | **Latency.** Zero at both qualities: no stage looks ahead, the decimation at High quality included. |
| PF-9 | **Many voices, reported.** The heaviest bank patch is also measured at 16 and at 64 sounding voices and reported, so the cost of a full pool is known; neither is a target. |

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
7. the aliasing floors (QS-1, QS-2), every base wave band-limited (ED-1a) and the aliasing report (QS-3);
8. real-time safety: no allocation, lock or system call on the audio thread, bounded work (RT-1, RT-2), by a sanitizer run and by inspection rules;
9. clicks, by the detector of QS-4, with its own must-fire control on a bright patch;
10. the claim anchors: coupling's regimes (SW-4), phase lag (SW-5), gravity (SW-11), onset scatter's structure (SW-12), the frame never jumping (BL-10), the quantized cut's timing and no tone bias (BL-16), every spread kind ramping as fed to the blade (BL-17), twin − cancelling even harmonics (BL-6), Crush continuity (BL-7), interplay's parallel identity (BL-11), the negative-spread mirror (BL-14), the cut-rule ratios (BL-15), the steal fade's rules (VO-3), the voice law at 1, 16 and 64 voices (VO-2, VO-5), the pedal (VO-12), all-notes-off and all-sound-off (VO-13), the start table of VO-6, the sub's bump shape (SN-2);
11. live parameters move the output and inert ones do not, by PA-8's table, for every manifest row;
12. host values checked on entry (PA-7);
13. state round trip, reset to fresh, and migration from the version-0 format (IF-4, DT-6, OU-3);
14. the zipper measure: a host staircase of 128-sample steps against a per-sample ramp, high-passed at 250 Hz, on every continuous parameter (PA-1, PA-2, PA-3);
15. the WebAssembly build renders the same sound as the native build: on every bank preset's held-note and chord phrases (2 s each), the difference of the two renders is at least 90 dB below the render's RMS (IF-10); the bound allows the two toolchains' different math libraries and operation orders (BD-7) and is ratified with the suite;
16. each of the 12 envelope presets (Appendix A), translated by Appendix B, plays the envelope shape it is named for: on a 2 s note its ENV 1 level (IF-10) follows C.12 for the translated values within 10⁻⁴ at every sample, and the shape's defining feature holds: pluck, below −60 dB before the key-up; pad and swell, an attack of at least 0.5 s; gate and the swell cut at the key-off, below −60 dB within 30 ms of the key-up; stab, its sustain reached by the end of the decay and held; tail, audible (−60 dB or above) 4 s after the key-up; steal, under a full pool new notes take the quietest tails first (VO-2) with no detected click;
17. a voice that is not sounding costs nothing, by the operation counters (VO-11);
18. a fifth source added through the slot by a test build leaves the four sources' renders bit-identical, and costs nothing while off (RC-6.3, RC-6.4);
19. transport events at any sample times and block sizes leave the render bit-identical (RC-7.1, RC-7.3);
20. a reader thread reading the GUI snapshot continuously leaves the render bit-identical, with no allocation, lock or wait on the audio thread (RC-8.1, RC-8.2);
21. a state with an unknown key round-trips byte-identically and renders as without it; a state saved before a parameter was added loads to the same render (RC-9.2, RC-9.3);
22. a tier other than 0 is refused and counted (RC-10.3);
23. a snapshot taken mid-render and restored reproduces the parameter state exactly, and a restore mid-note makes no detected click (RC-11.2, RC-11.4).

Its thresholds are ratified with the suite and pinned; a threshold changes only on the human's approval.

### 10.2 The frozen references

Once the suite is green and the human has accepted the sound, the engine's renders of the suite's reference scenarios are frozen as its own references, by the suite's tool, never by hand. From then on every change to the engine's sound is recorded with a before-and-after render set and the human's word [X6].

### 10.3 Renders for judging

The engine is also judged by the human's ear on the bank. What the build delivers for that:
- An **offline render entry**: given a patch and a phrase (a sample-timed event list: notes, per-note pitch in Hz, expressions, the pedal, parameter changes), the engine renders a stereo file of 24-bit PCM at 44.1 kHz and at 48 kHz, unnormalised, from sample 0 of the event list.
- **The phrase set**: a held note, a chord, a fast line, a trill, supplied with the acceptance tests as event lists. Where a preset glides (B.1: `glide`, `glideAlways`), the phrase's per-note pitch carries the glide (exponential in log-frequency, 95 % in the preset's glide time, from overlapping notes or always), which stands in for the pitch system (RC-3) until it exists.
- **The renders**: every bank preset and every envelope preset, translated by Appendix B, on every phrase.

### 10.4 The CPU

PF-1, PF-2, PF-5 and PF-9 measured under PF-3 and reported to the human with PF-4's spread. A miss is reported with what would close it, never hidden.

### 10.5 Build requirements

- **BD-6. Self-documenting and concise.** Names carry what the code does; comments carry only why: the constraint, the rejected alternative, the trap. No comment narrates history. A clean engine of this scope is expected at about 2,100 to 3,000 lines; that is an estimate, not a gate.
- **BD-7. One arithmetic.** No code emulates another language's number semantics, no random draw exists only to preserve an order, and each constant has one value. Single precision, approximations and fused multiply-add are allowed throughout, judged by the suite and the ears.
- **BD-8. Built in stages behind the frozen structure**, each stage green on its part of the suite before the next: (1) the manifest, the API, the harness header and the state document (items 12, 22, and 13's document half: the text round trip and the version-0 parse); (2) the voice, ENV 1, the pedal and the output stage on a plain one-member saw (items 1, 2, 3, 5, 10's voice anchors, 17, 19); (3) the swarm, C.1 to C.8 (items 0, 10's swarm anchors); (4) the edge correction (items 7, 9); (5) the blades, C.9 to C.11, C.14, C.16 (items 0, 4, 10's blade anchors, 14); (6) the sub and the noise oscillator (item 10's sub anchor, item 18 for the source slot, and item 11 over every manifest row, now that every row can sound); (7) modulation, morph and state (item 13's render half: reset to fresh, migration rendered; items 14, 20, 21, 23); (8) the WebAssembly build (item 15); then the bank (items 4, 6, 16) and the renders for judging. The parameter table stays open until Q31 is ruled.
- **BD-9. Exactly what is tested ships**: the build that the suite and the CPU measurement run is the shipped build's code with no product-line branch added.
- **BD-10. A clean room.** The builder reads only this spec and the acceptance tests (with the input data they carry for the builder: the bank and the phrase set). The build happens in a tree that contains no earlier engine, no earlier engine's tests, no prototype and no legacy plugin; the tree is provided to the builder.
- BD-1 to BD-5 (section 3).

---

## 11. Items still open

Each is built as written here until it is ruled; a ruling changes only the requirements tagged with it.

### 11.0 To be ruled before the build (0)

| # | the item | what is built until it is ruled | tagged |
|---|---|---|---|
| — | none | | |

### 11.1 To be ruled during the build, before the stage that needs them (25)

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
| X3 | corners | triangle base and carrier corners corrected by a corner table; Crush slew corners not | ED-6 |
| X4 | one noise stream for all notes, or one per voice | one per voice and member | BL-18 |
| X5 | the noise blade's large steps | kept as the sound, band-limited | BL-18 |
| X7 | the frame after a crossing | a flip of ψ by half a cycle (within 10⁻⁶ cycle) counts as no change; otherwise the frame turns back at its rate limit | BL-10, C.13 |
| X11 | detune's range | a stepped Range: ×1, ×12, ×24 | `detuneRange` |
| X12 | the default patch | blade 1 off: an untouched patch is the swarm saw | `on` |
| X13 | the filters' count and routing | two filters per voice, each source routed to either, both or neither, serial or parallel (RC-1) | RC-1 |
| X15 | the noise oscillator's design | the minimum of SN-4 | SN-4, 5.6 |
| X17 | density compensation and the bank's balance | U^−0.75 for new patches; each bank preset translated with +5·log₁₀(N) dB (B.1) | SW-19, B.1 |
| X21 | aftertouch's default destination | channel pressure and per-note pressure route to `coupling` on both oscillators, +0.25 of its range (+0.125 in v) at full pressure, in the init patch and every factory preset unless the preset says otherwise: a held note locks tighter as it is pressed, the instrument's own gesture | IF-1, RC-4 |
| X22 | the envelopes' translation from the bank (PROPOSED) | B.1's ENV 1 rule (decay and release × 2.30, curve +2.30) and the blade envelopes as one-shot ENV 2 and ENV 3 (RC-4) | B.1, RC-4 |

### 11.2 Can be ruled after the build (9)

| # | the item | what is built until it is ruled | tagged |
|---|---|---|---|
| Q23 | a fixed post-stage per oscillator, or a rack slot, for the hosted distortion | a rack slot; no engine insert | RC-5 |
| Q26 | the width candidates and the mono-fold guarantee | per-member pan and per-source pan only, no polarity inversion; mono fold reported | SW-18, IF-8, `width` |
| Q29 | a min-spec measurement before the CPU row is frozen | not needed to build | PF-2 |
| X6 | the record of sound changes after the references freeze | kept, with a before-and-after render set and the human's word | 10.2 |
| X9 | a twin wider than half a cycle under the cut latch | the edge stays as stated in BL-16 | BL-16 |
| X23 | the bank's coupling rescale (PROPOSED) | B.1's rule; confirmed by ear in the preset round | B.1 |
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
| `K`, `cScale` | `osc1.coupling` | **PROPOSED rescale** [X23]: the bank's K was voiced with a pull of K·(1.5·Δf_max + 3·s) Hz, Δf_max = f·(2^(detune/1200) − 1) the outermost member's offset and s = 1 (s = f/110 when `cScale` is 1); C.1 pulls 4K′\|K′\|·σ. At the reference pitch f = 110 Hz the two pulls are equal when **K′ = sign(K)·min(1, √(\|K\|·(1.5·Δf_max + 3·s) / (c·σ)))**, c = 4 for K ≥ 0 and c = 12 for K < 0 (splay's three-fold authority), σ the swarm's spread at 110 Hz under this translation (C.3, without drift). C.1's regime then holds at every pitch; the bank's own pull, with its 3 Hz floor, loosened above the reference and tightened below it (with `cScale` 1 it was proportional to f and the match holds at every pitch). Checked by simulating C.1 (7 or fewer members as set, start aligned, no drift): Locked horde K′ 0.907, R 0.946; Anchored horde 0.996, R 0.965; Undertone cathedral 1.0 (clamped), R 0.965; Screamer (3 members) 1.0, R 0.966: each locks, at 110, 220 and 440 Hz. The negative-coupling presets' start is X24. |
| `phaseMode` | `osc1.phase`, `osc1.phaseRandom`, `osc1.retrigger` | random → Phase 0, Random 100, Retrigger on; aligned and settled → Phase 0, Random 0, Retrigger on |
| `width`, `panOrder` | `osc1.width`, `osc1.panImage` | as is |
| `aa` | `osc1.bandLimit` | as is |
| `base`, `xm`, `fb`, `law`, `driftRate`, `kq`, `kCustom`, `b2order`, `b2mix`, `colK`, `colB` | `baseWave`, `crossMod`, `feedback`, `spreadLaw`, `spreadDrift`, `quantize`, `ratios`, `stacking`, `upperHears`, `collisionPitch`, `collisionBite` | as is |
| blade 1 keys (`mode`, `hot`, `morph`, `lock`, `k`, `kHz`, `c`, `w`, `hard`, `depth`, `mirror`, `frame`, `rotRate`, `rotSync`, `fmType`, `mshape`, `I`, `mUnit`, `m`, `mHz`, `bspread`, `kspread`, `kRule`, `kRuleAmt`, `wspread`, `dspread`, `mspread`, `ispread`, `rotSpread`) | `osc1.blade1.…` (`mode`, `wave`, `shape`, `cutIn`, `cut`, `cutHz`, `position`, `width`, `edges`, `depth`, `mirror`, `frame`, `rotate`, `rotation`, `fmType`, `modShape`, `fmDepth`, `modIn`, `modRate`, `modRateHz`, `positionSpread`, `cutSpread`, `cutRule`, `ruleDepth`, `widthSpread`, `depthSpread`, `shapeSpread`, `fmIndexSpread`, `rotateSpread`) | as is; `blade1.on` on |
| blade 2 keys (the same with a `2` suffix, `b2on`, `rot2Follow`, `lock2`, `mirror2`, `frame2`, `b2fm`, `b2sp`) | `osc1.blade2.…` | as is |
| `A` (ms) | `env1.attack` | ms ÷ 1000; curve 0 (the bank's attack is linear) |
| `D`, `R` (ms) | `env1.decay`, `env1.release` | **PROPOSED** [X22]: ms ÷ 1000 × ln(10⁴)/4 ≈ 2.303, and the curve u = ln(10⁴)/4 ≈ 2.303. The bank's decay and release fell as e^(−4t/T), T the stored time, and a release ended where its level fell below 10⁻⁴ (−80 dB) of full scale: from a starting level L at t = (T/4)·ln(L/10⁻⁴), 2.1·T at L = 0.45 and 2.3·T at L = 1. With C.12's curve at c = 4u = ln(10⁴) ≈ 9.21 over T′ = 2.303·T the stage is (e^(−4t/T) − 10⁻⁴)/(1 − 10⁻⁴) of its distance: the bank's shape to within 10⁻⁴ of full scale, landing at 2.303·T. The two tails differ only below −80 dB, so each preset's tail keeps its audible length. |
| `S` | `env1.sustain` | as is; hold 0 |
| `polyMode` | `voice.mode` | as is |
| `glide` (ms, time to 95 %), `glideAlways` | the pitch system (RC-3); in the renders, the phrase's per-note pitch (10.3) | exponential in log-frequency, 95 % in the stored time; from overlapping notes, or always [Q6] |
| `benvA`, `benvD`, `benvVel`, `benvK`, `benvW` | **PROPOSED** [X22]: ENV 2 in one-shot mode (RC-4): attack `benvA` ms (curve 0), hold 0, decay `benvD` ms × 2.303 to 0 (curve +2.303, as for ENV 1), its level scaled by (1 − `benvVel` + `benvVel`·velocity); routed per voice to `blade1.cut` (or `cutHz` when the cut is in Hz) by 4·`benvK` octaves at full level, and to `blade1.width` by 3·`benvW` octaves at full level (each converted to its parameter's normalised law); and to blade 2 the same way unless `b2env` is 1. One-shot keeps the bank's blade envelopes, which ignored key-up, swelling on short notes. | until the modulation system exists, the render harness supplies ENV 2 and ENV 3 through PA-2 |
| `benvA2` … `benvW2` with `b2env` 1 | ENV 3, the same rule, routed to `blade2.cut` and `blade2.width` | as above |
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
- Each member's phase advances per internal sample by gᵢ(t)/f_s, where gᵢ is its frequency from C.1 and C.5: each control interval's newly computed value is reached by a linear ramp across the following interval (so no change of coupling, detune, drift or inertia steps a frequency, SW-17), and the ramped value is scaled per sample by f₀(t)/f₀(t_k), f₀ the oscillator's pitch at the sample and t_k the last update, so a glide or bend is smooth inside an interval. **The final member frequency is max(0, ·)**; every conversion of a frequency to a ratio (a cut in Hz, a mod rate in Hz) divides by max(fᵢ, 1 Hz).
- A member waiting under onset scatter (C.7) counts in R, ψ and σ with its held phase and its natural frequency, and advances by zero.

### C.1 Coupling

- Taper: **km = 4·K·|K|**, K = `coupling`. The manifest's law for `coupling` is linear; the taper is here.
- Onset burst: **Kenv(t) = 8·o·|o|·e^(−t / max(0.01 s, `dissolve`))**, o = `onsetLock`, t the time since the voice's fresh start. Kenv is in the same units as km.
- Spread: **σ = max(0.08 Hz, √((1/U)·Σᵢ (fᵢⁿᵃᵗ − f̄)²))**, fᵢⁿᵃᵗ the natural frequencies of C.3, f̄ their mean.
- Targets, in Hz: **S\* = (max(0, km) + max(0, Kenv))·σ**; **P\* = 3·(max(0, −km) + max(0, −Kenv))·σ**. The applied S and P follow S\* and P\* through a one-pole of time constant 0.0043512 s, starting from 0 at the voice's fresh start.
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

**FM phase:** `cp = hp + (I/2π)·mod(shape, x)`. **FM pitch:** `cp = hp + acc`, `acc += ((1 + I/10)^{mod} − 1)·kk·dφ` (exponential deviation, symmetric in cents; depth shown as `±1200·log₂(1 + I/10)` cents); reset mode zeroes `acc` at blade entry. **Mod rate**: `mEff = modRate` (a multiple of the member's frequency fᵢ), or `mEff = modRateHz/max(fᵢ, 1 Hz)`. Under collision, I is the bitten index of C.11.

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
3. **The carrier restarts.** In Sync, FM reset, FM free and Ring the carrier phase restarts at 0 at every window entry (hp = 0 at e = 0). The modulator of FM reset is at phase mEff·eʳ, so it restarts too; that of FM free advances by mEff·Δφ per sample and never restarts; pitch-FM's accumulator `acc` is zeroed at entry in FM reset only. With collision (C.11) the carrier phase also adds `cacc`, and with cross-mod or feedback `xin` (C.14): cp = hp + xin + cacc (+ the FM term).
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

### C.12 ENV 1's curve

- Each timed stage of length T with curve value u ∈ [−2.5, 2.5] follows **y(t) = (1 − e^(−c·t/T)) / (1 − e^(−c))** for 0 ≤ t ≤ T, with **c = 4·u**; at c = 0, y(t) = t/T. y(0) = 0 and y(T) = 1 exactly: a stage's time is exactly when it ends, and no stage leaves a residual step.
- Attack, from the level L₀ it starts at (0 from silence, the current level on a re-run): level = L₀ + (1 − L₀)·y. Hold: 1 for the hold time. Decay: level = S + (1 − S)·(1 − y). Sustain: S. Release, from the level L_r at key-up: level = L_r·(1 − y).
- Defaults for a new patch: attack u = 0 (straight); decay and release u = +1. The bank's exponential is u = ln(10⁴)/4 ≈ 2.303 (B.1).
- A change of a stage's time or curve while it runs restarts that stage from the current level toward the same target, over (1 − the elapsed fraction) × the new time. A change of sustain during the decay retargets the decay to the new sustain the same way; during sustain the level moves to it by PA-1's smoother. Restarts happen at most once per control interval (changes within an interval take effect together).
- The modulator bank's envelopes follow this law; their one-shot mode (RC-4) ignores key-up until the decay ends.

### C.13 The frame

- Each voice keeps a frame angle Φ (cycles) per oscillator. Each control interval Δ:
  - advance: Φ ← Φ + the mean of the members' measured phase advances over the interval (a waiting member's is zero, C.0);
  - measure: d = the wrapped difference ψ/2π − Φ, in (−½, ½];
  - track: g(R) = clip((R − 0.05)/0.2, 0, 1); the fraction F = 1 − (1 − g)^(Δ/Δ_ref), Δ_ref = 32/48000 s; the move is F·d, limited to ±8·Δ cycles; where F = 1 and the move is within the limit, Φ = ψ/2π exactly.
- At a voice's fresh start Φ = ψ/2π if R ≥ 0.25, else member 0's phase θ₀; leads are set, not ramped, at a fresh start and for a member that joins a held frame.
- Each member's lead is **leadᵢ = frac(θᵢ − Φ + ½) − ½** cycles; in swarm frame it adds to the window's centre (C.9.1), and it reaches the blade as a ramp across each control interval at every `motionGrain` (SW-16).
- After a crossing (R through 0, ψ flipping): [X7].

### C.14 Cross-mod and feedback

- Each member's output yᵢ passes through a one-pole low-pass at 5 kHz (time constant 1/(2π·5000) s, per internal sample) to give its loop tap ŷᵢ.
- The carrier phase offset of member i at sample n is **xinᵢ[n] = ½·(`crossMod`·ŷᵢ₊₁[n − 1] + `feedback`·½·(ŷᵢ[n − 1] + ŷᵢ[n − 2]))**, members in a ring (member U − 1's neighbour is member 0). It applies to Sync, FM and Ring carriers on both blades and changes no member frequency.

### C.15 Time constants and grids

| what | value |
|---|---|
| control interval Δ (coupling, drift, inertia, spreads, frame) | round(f_host/1500) samples |
| coupling smoother | one-pole, 0.0043512 s |
| gravity and pan-motion grid | 256/44100 s, round(f_host·256/44100) samples |
| parameter smoothing | one-pole, 0.012 s, in v, snapping within 10⁻⁶ |
| random spread glide | one-pole, 0.020 s |
| rotation homing | 0.030 s |
| DC estimate refresh; its smoothing | every 256/44100 s × max(1, ⌈J/64⌉); one-pole, 0.00756 s (C.16) |
| steal fade; cull fade; stepped-key crossfade; mute and solo; mono velocity | 0.005 s linear; 0.008 s linear; 0.005 s equal-gain; 0.005 s; 0.005 s |
| DC blocker (with cross-mod, feedback or collision) | first order, 8 Hz (C.16) |
| loop filter | one-pole, 5 kHz |
| High quality's decimation | low-pass at 0.45 × the host rate, at least as steep as a 4th-order Butterworth (two biquads, Q 0.5412 and 1.3066), no look-ahead |

### C.16 DC removal

Each member subtracts from its output the cycle mean of its blades' contributions, so no blade leaves a DC offset; the base waves are zero-mean by construction.

- **The estimate.** For each blade (twins included by their sign: twin − contributes 0, twin + twice the main window's), the mean over one cycle of its contribution D(φ) (C.9 item 1) at the member's current effective values (C.9 item 8):
  - **Sync on a closed-form carrier wave** (not sine→saw): exactly, depth·[(F_hot(kk·w) − F_hot(0))/kk − (F_base(st + w) − F_base(st))] (under reflect, 2·(F_hot(kk·w/2) − F_hot(0))/kk for the first term), F the antiderivatives of C.9.3; with Edges above 0, less what the tapers remove, integrated by the midpoint rule over the two taper regions with J_t = clamp(⌈32·kk·t⌉, 32, 512) points each, t = hard·w/2, the carrier read at eʳ (C.9.1) under reflect.
  - **Every other mode and wave**: the midpoint rule over the window with J = clamp(⌈32·F⌉, 48, 1024) points, F = w·kk, or for the FM modes F = w·(kk + I·mEff + mEff) (the features that must be resolved), with a Noise blade's held value taken as 0 (its mean). **FM free**, whose cycle mean depends on where its free-running modulator stands at the window's entry, takes the mean over 32 evenly spaced modulator start phases.
  - **Serial interplay** (`upperHears` > 0): the pair together, the mean of (out − base) over one cycle by the midpoint rule with J = clamp(⌈32·(G₁ + G₂ + 4)⌉, 64, 2048) points, G = kk + I·mEff + mEff for an FM blade, else kk.
  - **Every J is a minimum:** more points, or exact integration, are allowed.
- **When.** At a voice's fresh start, before its first sample (SW-15), the estimate is computed and applied at once; thereafter it is recomputed every 256/44100 s × max(1, ⌈J/64⌉), J the points that estimate used, and the subtracted value follows each new estimate through a one-pole of time constant 0.00756 s, so it never steps.
- **The blocker.** While `crossMod`, `feedback` or a collision amount is above 0 on an oscillator, that oscillator's summed output per voice also passes a first-order DC blocker at 8 Hz (its state 0 at the voice's fresh start), because those depend on past samples the estimate cannot see.
- The edge correction's own offset is removed separately (ED-3).
