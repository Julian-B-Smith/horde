# horde's engine: the spec, from the user's side

**Status.** DRAFT, round 2, for the human's ratification (ADR-223 item 2). Written 2026-10-15 by the horde lead's spec agent from the records; revised 2026-10-16 after the critic's verdict (ROADMAP B494) and the human's ruling on the laws.

**The laws are copied (the human, 2026-10-16: "copy the laws").** Where a formula *is* the sound (the coupling law and its taper, member placement, the detune laws, drift, gravity, the ensemble, every blade law, the spreads, the pan law, the envelope's curve), the formula is normative: Appendix C states each as math with its constants and units. The math is copied, not the code: the builder is free to compute it fast (single precision, tables, SIMD, any loop order). **A law may change only where every anchor that tests it still passes, and the reason is stated in the spec when it does.** Appendix B's "as is" translation of the bank holds because of this.
**Sanctioned by** ADR-223 and ADR-224 (this file is the "ratified spec" ADR-224 item 8 names). **Rows:** B494 step (1), after the B493 audit (`docs/audits/2026-10-15-engine-rebuild-audit.md`).
**Who reads it.** The builder of the new engine reads this file and the acceptance tests written from it, and nothing else. A different agent writes those tests from this file before the engine exists. So everything a requirement needs is in here, and nothing in here describes how today's engine does a thing unless that behaviour is itself the requirement.

**How to read it.**
- Requirements are numbered (`SW-4`, `VO-3`, …). Each is testable and ends with its source (an ADR, a ROADMAP row, or the B493 audit's row number such as `audit S1`).
- A requirement marked **[Qn]** or **[Xn]** depends on an open question in section 11. The text states the recommended answer, so the spec can be built as written; if the human rules otherwise, that requirement changes and nothing else does. `Qn` keeps the audit's numbering; `Xn` are questions this spec adds.
- "Must" is a requirement. "Should" is a recommendation the builder may depart from with a stated reason. Numbers carry units. Times are in seconds unless stated.
- Musicians' words are used where they exist. The glossary (section 2) fixes them.

---

## 1. Purpose and scope

**Purpose.** horde is a synthesizer whose oscillators are swarms: the stacked copies a synthesizer calls unison are coupled oscillators that pull toward one pitch or push apart, drift, settle into just intervals, and carry **blades**, windows that cut each cycle of the wave and replace what lies inside them with another sound (hard sync, FM, noise, fold, ring, crush). This spec defines the engine that makes that sound: everything from a played note to the instrument's stereo output before the effects.

The concept matters more than the old numbers. The new engine need not sound the same as today's at the same settings; it must do what a musician expects each control to do to the waveform, and it must sound good to the human (ADR-223 item 3). The bank of 83 presets must stay recognisable and sound good by the human's ear (section 10).

**In scope.**
- The **voice**: allocation, stealing, mono and legato, the amplitude envelope (ENV 1), the per-voice signal path, the slot the per-voice filters sit in.
- What lives in a voice: **two swarm oscillators** (each with its members, its coupling and its two blades), the **sub** and the **noise oscillator**.
- The sum of the voices and the engine's output stage.
- The engine's interface to the rest of the instrument: notes, pitch, modulation, morph, state, the mixer's meters, the FX rack's input, the labs, the host.

**Out of scope** (each has its own spec or project, and meets this engine only through section 7):
- the FX modules and the rack itself (behind `docs/proposals/rack-slot-contract.md`);
- the filters' own designs (the engine provides their per-voice slot, section 4.3 and IF-5; their types and sound are a separate spec);
- the modulators (ENV 2 to ENV 4, LFOs, the mod matrix), the macros and intent bus, the morph engine, the pitch system (bend lane, note lane and glide, quantizer, MTS-ESP), the arpeggiator: the engine accepts their outputs (section 7) and does not compute them;
- true-stereo processing after the voice (ADR-224 item 3), the mixer's strip and the master stage;
- the GUI, and the host shell beyond the engine's API.

---

## 2. Glossary

These words mean exactly this everywhere in this spec and in every record that cites it (ADR-224 item 7).

| term | meaning |
|---|---|
| **note** | One key press from the host, identified by its **note id** (CLAP's note id, with its channel and key). Two notes may share a key (MPE). |
| **voice** | One sounding note, instrument-wide. Polyphony counts voices: "8 voices" means 8 notes at once. A voice holds both oscillators, the sub, the noise oscillator, ENV 1 and the filter slot. |
| **oscillator** | One of the voice's two swarm oscillators (**osc 1**, **osc 2**). |
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
| **ENV 1** | The voice's amplitude envelope. ENV 2 to ENV 4 are the modulation system's. |
| **control rate** | About 1,500 updates per second (ADR-224 item 5). The interval between updates is the **control interval**. |
| **ramp** | A value travelling linearly from where it stands to its new value across one control interval. |
| **step** | A value arriving in one sample. |
| **quality** | The oversampling setting: Standard (1×) or High (2×). |
| **internal rate** | The sample rate the oscillators run at: the host rate × the quality factor. |
| **per-voice modulation** | An offset to one parameter for one voice, from the modulation system (ADR-224 item 4). |
| **Morphable / Structural / Device** | The three parameter classes (SPEC-INTENT-BUS §3.2; rack contract §4): Morphable values blend between morph corners; Structural values are owned by one corner and flip; Device values stay outside the morph. |
| **bank** | `reference/scalpel/data/presets.json`, 83 presets: the sound reference in character, not in samples. |
| **today's engine** | The composed engine in `h2/engine/` as of `main` 990e4a3, which the new engine replaces. Named only where it is the thing the new engine is compared against or migrates from. |

---

## 3. Scope of the rebuild

The rebuild consolidates as well as rewrites: one home per concept, one source of truth for the parameters, and the apparatus that existed to keep two engines in step retires with the old engine (the human: "I assume this also means consolidating some files, like the blade definitions, etc.").

### 3.1 IN: built new

| item | reason |
|---|---|
| The engine's source tree, organised by concept, one home per concept: the blade and its modes defined in exactly one place (today split across `h2/engine/engine.h`, `h2/engine/blade.h`, the JS twin and the lab); likewise the swarm law, the voice, the sources, the output stage. | Two lineages glued together are the baggage the audit counted (audit §1.8). |
| A single **parameter manifest**, the one source of truth from which the engine's parameter table, the plugin's host parameters (the lockfile, B308 H4), the labs' controls and the manual's parameter list are generated. | Today these are separate tables that drift (audit §3.11; `h2-preview-shell`'s hand-written range table; the lab's own key lists). |
| The engine's API: note ids, per-voice modulation, pitch input, state, observations (section 7). | ADR-224 items 1, 2, 4. |
| The WebAssembly build the labs load. | ADR-223 item 4; needs the toolchain approved first [Q30]. |
| The new engine's own test suite: the invariant suite, the bank A/B, the CPU measurement (section 10). | ADR-223 item 3; ADR-224 item 8. |
| A bank translator: the bank's 83 presets as new-engine patches, generated by the rule in Appendix B. | The bank is the sound reference (ADR-223 item 3); the A/B needs it. |

### 3.2 RETIRED at cutover, after the new engine is accepted (section 10)

Nothing here is removed before its replacement exists and is wired; each removal from `./verify` is the human's gate (charter; audit §7). Paths are from the audit's inventory (§1.2, §1.10, §7).

| item | paths | files | lines | reason |
|---|---|---|---|---|
| today's engine | `h2/engine/engine.h`, `h2/engine/blade.h`, `h2/engine/swarm.h`, `h2/engine/js.h` | 4 | 2,994 | replaced |
| the JS twin | `docs/design/scalpel-horde-engine.js` | 1 | 1,679 | the labs run the C++ (ADR-223 item 4) |
| the frozen earlier ports used as test references | `h2/cores/scalpel/razor_core.h`, `h2/cores/swarm/swarm_core.h`, `h2/cores/swarm/force_core.h`, `h2/cores/swarm/glide_core.h`, `h2/cores/swarm/lift-ledger.json` | 5 | 4,041 | they exist only to hold parity |
| the parity tools | `tools/h2_engine_parity_check.cpp` (and its contraction-on control build), `tools/h2_engine_render.mjs`, `tools/labharness/composed_engine_check.mjs`, `tools/labharness/divergence_ledger_check.mjs`, `tools/parity_floor_check.py`, `tools/h2_libm_count.py`, `h2/engine/libm-calls.darwin-arm64.appleclang-16.txt`, `tools/h2_scalpel_parity_check.cpp`, `tools/h2_scalpel_render.mjs`, `tools/h2_scalpel_stream.h`, `tools/h2_swarm_parity_check.cpp`, `tools/h2_swarm_lift_check.cpp`, `tools/h2_swarm48_check.cpp`, `tools/h2_lift_check.py`, `tools/measure_h2_scalpel.cpp` | 15 | 5,656 | parity is no longer a goal (ADR-223 item 3) |
| the divergence-switch machinery | `docs/port/divergences.json`; the switches D1 to D10 (D9 and D10 parked on their branches, which are left as they are) | 1 | — | D1 to D10 become plain requirements of this spec or are dropped (Appendix A) |
| the twin's check page | `docs/design/composed-engine-check.html` | 1 | — | it checks the twin against its prototypes |
| **removed in all** | | **27** | **≥ 14,370** | |
| the self-digest references (re-baselined, not removed) | `h2/engine/selfdigest.parity.darwin-arm64.appleclang-16.node24.txt`, `h2/engine/selfdigest.product.darwin-arm64.appleclang-16.node24.txt` | 2 | — | they become the new engine's frozen goldens; a protected path, so a sanctioned step (audit M10) |
| lab code that embeds an engine copy (re-pointed to the WebAssembly build, not removed) | the `<script src>` loads of the twin and its prototypes in `docs/design/scalpel-interface-lab.html`, `docs/design/envelope-hierarchy-lab.html`, `docs/design/listening-pass.html`, `docs/design/maw-horde-lab.html`, `docs/design/sluice-horde-lab.html` | 5 | — | one engine, in one language |

**Gates and tools that depend on the twin or the old cores** (each changes at cutover; none is removed before its replacement is wired):

| path | what happens | why |
|---|---|---|
| `tools/golden/gen_goldens_sr.mjs` | cut down, not deleted: its twin-rendering half retires, the half that feeds the legacy `swarm48_check` stays | it serves the legacy shell too |
| `tools/labharness/sandbox_check.mjs` | its rows that copy or plant the twin and `reference/scalpel/prototype/razor-core.js` are re-pointed at the WebAssembly build | it guards the labs' sandboxing |
| `tools/port_legacy_presets.mjs` | re-pointed: it reads the twin to state legacy presets' fates; it reads this spec's manifest instead | the legacy porter outlives the twin |
| `h2/harness/*` (`adapter_composed.h`, `adapters.h`, `control_*.h`, `harness.h`, `harness.mjs`, `render_cli.cpp`, `gen_patches.mjs`, `patches.json`, `selftest.mjs`) | re-aimed at the new engine's API and partly rewritten (the adapters are today's engine's string-keyed interface; `patches.json` is regenerated from Appendix B) | the harness drives the engine; it does not survive unchanged |
| `tools/h2_rules_check.py` rule 2 (every h2 core compiled with `-ffp-contract=off`) | **a gate change that needs the human's sanction**: for the new engine the rule contradicts the precision ruling (ADR-224 item 6) | fused multiply-add is allowed; the rule stays for the legacy cores while they exist |
| `tools/h2_rt_lint_check.py` | its scan root must cover the new tree (it scans `h2/` today) | RT-1 is checked by it |
| `CMakeLists.txt` | its `h2/cores` and `h2/engine` targets are replaced by the new tree's | the old targets build the retired code |

**Kept, re-pointed at the new engine** (audit §7): the behaviour checks `tools/h2_frame_hold_check.mjs`, `tools/h2_steal_fade_check.mjs`, `tools/h2_spread_ramp_check.mjs`; the fuzz (`tools/h2_fuzz_check.mjs`); the soak (`tools/h2_soak_check.mjs`); the RealtimeSanitizer probe and lint (`tools/h2_engine_rtsan_check.py`, `tools/h2_rt_lint_check.py`); the self-digest tool (`tools/h2_engine_selfdigest_check.cpp`); the harness self-test; the A/B tool (`tools/h2_ab.mjs`); the CPU tool (`tools/measure_h2_engine.cpp`); the measurement tools under `tools/patchspace/` that render the twin today (`alias_sources.mjs`, `antialias_scope.mjs`, `calibrate.mjs`, `fidelity.mjs`, `gauntlet_report.mjs`).

### 3.3 UNTOUCHED

| item | reason |
|---|---|
| The legacy plugin (`src/`) | frozen; the charter's invariants still govern it. |
| The prototypes in `reference/` | history and protected; nothing here edits them [Q30]. |
| The FX modules | their own projects, behind the rack-slot contract. |
| The GUI, and the host shell beyond the engine's API | out of scope (section 1). |
| Design labs that do not run the engine (the edge-correction lab, the mixer and stereo labs, and others) | they do not load an engine copy. |

### 3.4 File layout: requirements of the build

- **BD-1.** One concept per file. A file's name is a word a musician would recognise for what it holds (`blade`, `swarm`, `voice`, `envelope`, `sub`, `noise`, `output`), never a lineage (`razor`, `scalpel`, `horde`, `js`). (ADR-223; audit §1.8)
- **BD-2.** No file carries two lineages' versions of the same thing. A concept that both prototypes had (a phase counter, a centre-of-swarm sum, a glide, a drift, a random stream, a 2π) exists once. (audit §1.8)
- **BD-3.** No switch keeps an old behaviour beside a new one. A behaviour this spec drops is deleted, not disabled. (audit §1.5; N18)
- **BD-4.** Test and measurement hooks live outside product code; a test observes the engine through its public observation view (IF-10) or a test build that adds no branch to a product line. (audit §1.4; N10)
- **BD-5.** The parameter table, host parameters, lab controls and manual list are generated from the one manifest; nothing hand-copies it. (B308 H4)

**A proposed layout, which the builder may adapt** (a recommendation, not a mandate). It sits under `h2/` so the existing real-time lint and rules checks cover it; the folder name `h2/core/` is a placeholder:

```
h2/core/
  manifest/   parameters (the one source of truth) and its generators
  api/        the engine's calls, events, state and observation view
  voice/      allocator and stealing, the voice, ENV 1, mono and legato
  swarm/      members, placement, detune laws, coupling, drift, onset, start phase, gravity, pan
  blade/      geometry, modes, waves, FM, mirror, spreads and their laws and rules, interplay
  edges/      the band-limited step table (and the corner table if X3 is ruled in)
  sources/    sub, noise
  output/     level, DC guard, non-finite guard
  wasm/       the WebAssembly entry the labs load
tests/        the invariant suite, the bank A/B, the CPU measurement (under the repo's tools/ convention)
```

---

## 4. Architecture at the user's level

### 4.1 Who owns what (Q1, Q18, Q25: RULED, ADR-224)

- **One instrument-wide voice per played note** owns: the allocation decision, ENV 1, the per-voice filter slot, and inside it osc 1, osc 2, the sub and the noise oscillator. One allocator decides for all of them; there is no per-oscillator voice law. (ADR-224 item 1; audit H1, H2)
- **Each source hands its voice's signal up before any mixing**; the voice's filters and ENV 1 act on it; the voices are summed after. (ADR-224 item 2)
- **Members are mono** sources panned across the stereo field. Nothing renders a member separately per channel. True-stereo processing happens after the voice, outside this engine. (ADR-224 item 3)

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
  │         ─► Σ members × density compensation ─► osc 1 level ──┐          │
  │  osc 2: the same, independent ─────────────► osc 2 level ───┤          │
  │  sub:   one phase, one shape, tone ─► mono to L/R ─► level ─┤          │
  │  noise: per-voice seeded noise, colour ─► mono ─► level ────┤          │
  │                                                              ▼          │
  │                                    per-voice filter slot (stereo-linked)│
  │                                                              ▼          │
  │                                         × ENV 1 × velocity            │
  └──────────────────────────────────────────────────────────────┬─────────┘
                                                                  ▼
                         Σ voices ─► output stage (Level, guards) ─► stereo out ─► FX rack
```

- The oscillators run at the internal rate (host rate × quality) and return to the host rate before the per-voice filter slot; the sub and the noise oscillator may run at the host rate.
- The per-source meters (IF-6) read each source's signal after its level, summed across voices, before the filter slot; the engine meter reads the output.
- The filter slot passes the signal unchanged when no filter is assigned. The filter types, their count and how sources route to them are the filters' spec [X13]; what the slot guarantees is IF-5.

### 4.3 What lives where, in one table

| concern | owner | where specified |
|---|---|---|
| which voice a note gets, stealing, polyphony, mono/legato | the engine (voice) | VO |
| amplitude envelope | the engine (ENV 1) | EN |
| pitch: bend, glide, quantizer, MTS-ESP, transpose | the pitch system, upstream | IF-2 |
| gravity | the engine (voice) | SW-11 |
| the swarm, blades, edges | the engine (oscillator) | SW, BL, ED |
| sub, noise oscillator | the engine (voice) | SN |
| filters' sound | the filters' spec | IF-5 |
| ENV 2 to 4, LFOs, matrix, macros, intents, morph | the modulation and morph systems | IF-3, IF-4 |
| true stereo after the voice, mixer strip, FX, master | the device | IF-6 to IF-8 |

---

## 5. Every function and parameter

### 5.1 How to read the manifest

The manifest is data (BD-5). Its keys are frozen from the first release; integer ids are assigned explicitly by the manifest and are append-only (B308 H4; B428; ADR-197 armor row 7). This section is its content.

- **Key** is the stable address. Rows under 5.3 exist once per oscillator (`osc1.…`, `osc2.…`); rows under 5.4 once per blade per oscillator (`osc1.blade1.…` … `osc2.blade2.…`).
- **Law** is how the normalised form v ∈ [0, 1] maps to the value x in the range [lo, hi]. Every bindable parameter has this normalised form, because macros, intents, morph and modulation all work in it (SPEC-INTENT-BUS §3.3; SPEC-MODULE-MACROS §4–5). The laws, exactly:

  | law | x(v) | notes |
  |---|---|---|
  | `lin` | lo + (hi − lo)·v | |
  | `log` | lo·(hi/lo)^v | lo and hi nonzero, same sign |
  | `pow` | lo + (hi − lo)·v² | finer near lo |
  | `lin, wraps` | lo + (hi − lo)·frac(v) | circular: an offset past either end wraps, never clamps |
  | `log, off zone` | 0 (off) for v ≤ 0.01; else 0.004·(hi/0.004)^((v − 0.01)/0.99) | blade width: off below 0.004 cycle |
  | `dB` | −∞ (silence) at v = 0; else lo_dB + (hi_dB − lo_dB)·v, applied as 10^(dB/20) | levels; lo_dB is the range's −60 |
  | `step` | option index round(v·(n − 1)) of n options | in the table's option order |

  A parameter's display may differ from its law (a pitch in st shown as octave and semitone); the law is what modulation, morph and macros move.
- **Class**: `M` Morphable, `S` Structural (flips at a morph corner boundary), `D` Device (outside the morph). (glossary; Q17)
- **Rate**: `C` control rate, ramped; `A` also an audio-rate destination [X16]; `N` read at note-on and held for the note; `T` stepped, changed by the rule of PA-5.
- **PV**: accepts per-voice modulation. Every continuous parameter does (ADR-224 item 4); stepped ones do not.
- Rows marked **[Q31]** were deferred by the human on 2026-10-02 and are in the table as recommended; the table stays open until Q31 is ruled (audit §5.4).

### 5.2 The voice (once per instrument)

| key | label | unit | range | default | law | class | rate | PV | what it does to the sound |
|---|---|---|---|---|---|---|---|---|---|
| `voice.mode` | Voicing | — | poly, mono, legato | poly | step | S | T | — | Poly gives each note its own voice; mono plays one voice and restarts its envelope on every note; legato restarts only when no key is held (VO-6). |
| `voice.polyphony` | Polyphony | voices | 1..16 | 8 | step | D (Settings) | T | — | How many notes can sound at once, in one pool (Q4, ruled by ADR-224 item 1). |
| `voice.gravity` | Gravity | % | 0..1 | 0 | lin | M | C | yes | Held notes drift into just intervals of one another (SW-11). [Q8] |
| `env1.attack` | Attack | s | 0.0005..10 | 0.004 | log | M | C | yes | Time from note-on to full level. |
| `env1.hold` | Hold | s | 0..5 | 0 | pow | M | C | yes | Time held at full level before the decay. |
| `env1.decay` | Decay | s | 0.005..20 | 0.4 | log | M | C | yes | Time from full level to the sustain level. |
| `env1.sustain` | Sustain | % | 0..1 | 0.85 | lin | M | C | yes | Level held while the key is down. |
| `env1.release` | Release | s | 0.005..20 | 0.28 | log | M | C | yes | Time from key-up to silence. |
| `env1.attackCurve` | Attack curve | — | −1..1 | 0 (linear) | lin | M | C | yes | Bends the attack: 0 straight, positive fast-start, negative slow-start (C.12). [Q31: envelope decision 1] |
| `env1.decayCurve` | Decay curve | — | −1..1 | +1 | lin | M | C | yes | Bends the decay; +1 is the bank's exponential shape (C.12). [Q31] |
| `env1.releaseCurve` | Release curve | — | −1..1 | +1 | lin | M | C | yes | Bends the release; +1 is the bank's shape (C.12). [Q31] |
| `env1.velocity` | Velocity | % | 0..1 | 1 | lin | M | N | yes | How much velocity scales the voice's level; at 1 level is proportional to velocity, at 0 velocity is ignored. [Q31] |
| `mix.osc1` | Osc 1 level | dB | −∞, −60..+6 | 0 | dB | M | C | yes | osc 1's contribution to the voice. |
| `mix.osc2` | Osc 2 level | dB | −∞, −60..+6 | 0 | dB | M | C | yes | osc 2's contribution. |
| `mix.sub` | Sub level | dB | −∞, −60..+6 | −2 | dB | M | C | yes | The sub's contribution (−2 dB ≈ SPEC-SUBOSC's level 0.8). |
| `mix.noise` | Noise level | dB | −∞, −60..+6 | −12 | dB | M | C | yes | The noise oscillator's contribution. |
| `out.level` | Level | dB | −∞, −60..+12 | 0 | dB | M | C | yes | The engine's output level into the rest of the device; 0 dB is calibrated by OU-2. [Q20] |
| `quality` | Quality | — | Standard (1×), High (2×) | Standard | step | D, saved with the patch | T | — | Oversampling: High aliases less on FM, ring and fold with sine carriers, at about 1.5 to 1.7 times the CPU (ADR-191; B445). [Q27] |

Each of osc 1, osc 2, the sub and the noise oscillator also has an on switch: `osc1.on` (default on), `osc2.on` (off), `sub.on` (off), `noise.on` (off): gate, class S, rate T. An oscillator that is off costs nothing (PF-6).

### 5.3 Each oscillator (`osc1.…`, `osc2.…`)

| key | label | unit | range | default | law | class | rate | PV | what it does to the sound |
|---|---|---|---|---|---|---|---|---|---|
| `pitch` | Pitch | st | −48..+48 | 0 | lin | M | A | yes | The oscillator's transposition, continuous so it can be swept across octaves; the GUI may show it as octave and semitone. (audit round 2 `pitchOffset` merge, with the human's caveat that coarse automation across octaves is met) |
| `fine` | Fine | cents | −100..+100 | 0 | lin | M | A | yes | Fine tuning. |
| `unison` | Unison | voices (members) | 1..16 | 7 | step | S | T | — | How many members the swarm has. [X10] |
| `distribution` | Distribution | — | even, JP-8000, Gaussian, Cauchy, golden | JP-8000 | step | S | T | — | How the members are placed around the pitch: evenly, on the classic supersaw curve, or shaped by a seeded draw. |
| `seed` | Seed | — | 0..999 999 | 1234 (osc 2: 1235) | step | S, advanced | T | — | Fixes every random choice of this oscillator (placement, drift, scatter), so a patch is the same swarm every time. Shown under an advanced menu (the human, audit round 1 `seed`). |
| `detuneLaw` | Detune law | — | cents, Hz, ERB, harmonic | cents | step | S | T | — | What detune means: equal cents (beats faster up the keyboard), equal Hz (the same beat everywhere), equal roughness (ERB), or members on harmonics of the pitch. [Q31: tempo grid] |
| `detune` | Detune | % (shown in the law's unit) | 0..1 | 0.28 | lin | M | C | yes | How far the members sit from the pitch (exact laws, C.3): cents law, a member at placement x sits x·detune·range·100 c away; Hz law, x·detune·range·20 Hz; ERB law, x·detune·range·0.35·ERB(f); harmonic law, member i on f·(1 + detune·range·reach·i). |
| `detuneRange` | Range | — | ×1, ×12, ×24 | ×1 | step | S | T | — | Widens detune to about an octave or two. [X11] |
| `harmonicReach` | Harmonic reach | — | 0.25..4 | 1 | log | M | C | yes | Harmonic law only: how far up the series the members reach. |
| `anchor` | Root anchor | % | 0..1 | 0 | lin | M | C | yes | 0 centres the swarm on the pitch; 1 puts its lowest member on the pitch. Advanced. |
| `coupling` | Coupling | % | −1..1 | 0 | lin (the squared taper is in the law, C.1) | M | C | yes | Positive pulls the members toward one locked pitch (cloud, herding, shimmer, lock); negative pushes them into an even splay (a multiplied, hollower tone). Feels the same at any detune (SW-4). (the human named it Coupling, audit round 1) |
| `phaseLag` | Phase lag | ° | −90..+90 | 0 | lin | M | C | yes | Bends where a locked swarm settles; near ±90° opens unstable, part-locked states. |
| `pivot` | Pivot | — | mean field, root member | mean field | step | S | T | — | Mean field locks onto the swarm's average; root member locks onto the member nearest the played pitch, so a locked swarm stays on pitch. |
| `onsetLock` | Onset lock | % | −1..1 | 0 | lin (taper in the law, C.1) | M | N | yes | A burst of locking (or, negative, of splay) at each note's start. [Q31] |
| `dissolve` | Dissolve | s | 0.05..8 | 0.63 | log | M | C | yes | How long the onset burst takes to fade. [Q31] |
| `driftDepth` | Drift | cents | 0..100 | 0 | pow | M | C | yes | How far each member wanders in pitch. |
| `driftRate` | Drift rate | % | 0..1 | 0.4 | lin (the walk's rate is 0.2 + 8·knob, C.4) | M | C | yes | How fast they wander. [Q31: drift mode] |
| `inertia` | Inertia | % | 0..1 | 0 | lin (the swing reads knob^2.5, C.5) | M | C | yes | Members swing into new pitches with weight and overshoot instead of jumping. [Q6, Q31] |
| `startPhase` | Start | — | fresh, settled, continue | fresh | step | S | N | — | Where members start each note: fresh (phase 0, opened by Scatter), settled (already in the state the coupling would reach), or continue (where this oscillator's last voice left off). [X1] |
| `scatter` | Scatter | % | 0..1 | 0 | lin | M | N | yes | How random the fresh start phases are: 0 all aligned, 1 fully random. |
| `onsetScatter` | Onset scatter | ms | 0..80 | 0 | pow | M | N | yes | Members enter up to this much apart, like players in an ensemble. [Q13, Q31] |
| `timingCorrection` | Timing correction | — | 0..1.5 | 0.25 | lin | M | N | yes | How strongly the entries correct toward the ensemble's average, so the swarm does not drift late. [Q31] |
| `width` | Width | % | 0..1 | 0.8 | lin | M | C | yes | How far the members spread across the stereo field; 0 is mono. [Q26] |
| `panImage` | Pan image | — | balanced, pitch fan, spread fan | balanced | step | S | T | — | Balanced places members so no side is brighter; the fans order them by pitch or by placement, a deliberate spectral-stereo effect. |
| `panInvert` | Invert image | — | off, on | off | step | S | T | — | Mirrors the image. Advanced. |
| `panMotion` | Pan motion | % | 0..1 | 0 | lin | M | C | yes | Slow wandering of each member's pan. |
| `bandLimit` | Band-limit | % | 0..1 | 1 | lin | M | C | yes | 1 removes the aliasing of every edge (ED-1); 0 leaves it, as a raw digital texture. |
| `motionGrain` | Motion grain | % | 0..1 | 1 | lin | M | C | yes | How the spread law's values reach the blades: 1 in steps at the control rate (the grain and air the human preferred in a blind A/B of the spread ramp), 0 as smooth ramps. The frame always ramps. [X8] |
| `baseWave` | Base wave | — | sine, triangle, saw, reverse saw, square | saw | step | S | T | — | The wave the members play and the blades cut into. |
| `crossMod` | Cross-mod | % | 0..1 | 0 | pow | M | C | yes | Each member's previous output bends the next member's carrier phase (a ring of members). |
| `feedback` | Feedback | % | 0..1 | 0 | pow | M | C | yes | Each member's own previous output bends its carrier phase. |
| `spreadLaw` | Spread law | — | gradient, random, drift, alternate, swarm | gradient | step | S | T | — | How every spread is fanned across the members (BL-13). |
| `spreadDrift` | Spread drift | Hz | 0.02..12 | 0.5 | log | M | C | yes | Drift law only: how fast the spread values wander. |
| `quantize` | Quantize | — | free, whole | free | step | S | T | — | Whole snaps the cut-rate spreads and rules to whole harmonics. |
| `stacking` | Stacking | — | blade 2 over 1, blade 1 over 2 | blade 2 over 1 | step | S | T | — | Which blade is the upper one when they interact. |
| `upperHears` | Upper hears | % | 0..1 | 0 | lin | M | C | yes | 0: both blades act on the base wave side by side; 1: the upper blade acts on the lower blade's result. |
| `collisionPitch` | Collision → pitch | % | −1..1 | 0 | lin | M | C | yes | Where the blades overlap, the upper carrier's pitch bends up or down, up to ±2 octaves at full overlap. |
| `collisionBite` | Collision → bite | % | 0..1 | 0 | lin | M | C | yes | Where they overlap, the upper blade's FM depth and fold drive rise. |

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
| `position` | Position | cycle | 0..1, circular | 0.875 / 0.35 | lin, wraps | M | C | yes | Where in the cycle the window sits. Wraps: modulation and morph go round, never clamp. |
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
| `cutSpread` | Cut spread | ±harmonics | −24..24 | 0 / 0 | lin | M | C | yes | Fans the cut rate across the members (whole harmonics under Quantize). |
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

From `specs/SPEC-SUBOSC.md` §3 to §7, with its provisional envelope rows struck (its R4: the shipped sub takes ENV 1), the retired sync row omitted, and its seed taken from the voice (its R5).

| key | label | unit | range | default | law | class | rate | PV | what it does |
|---|---|---|---|---|---|---|---|---|---|
| `wave` | Wave | — | sine, triangle, square, saw, pulse, noise, bump | saw | step | S | T | — | The sub's shape (SN-2). |
| `pulseWidth` | Width | % | 0.05..0.95 | 0.5 | lin | M | C | yes | Pulse only; 0.5 is a square. |
| `bumpAmount` | Bump | — | 0..0.6 | 0.35 | lin | M | C | yes | Bump only: the size of the second bump. |
| `bumpPhase` | Bump phase | rad | −π..π | −0.25 | lin | M | C | yes | Bump only: negative leads with the big bump. |
| `octave` | Octave | — | −3, −2, −1, 0 | −1 | step | S | T | — | A sub goes down only. |
| `semitones` | Semitones | st | −12..+12 | 0 | step | S | T | — | |
| `fine` | Fine | cents | −100..+100 | 0 | lin | M | A | yes | |
| `startPhase` | Phase | cycle | 0..1 | 0 | lin | M | N | yes | The sub's phase at note-on, relative to the voice's master phase (SN-1). |
| `keytrack` | Keytrack | — | on, off | on | step | S | T | — | Off pins the sub to C2. |
| `tone` | Tone | Hz | 30..20 000 | 20 000 | log | M | C | yes | A gentle low-pass. |

### 5.6 The noise oscillator (`noise.…`) [X15]

| key | label | unit | range | default | law | class | rate | PV | what it does |
|---|---|---|---|---|---|---|---|---|---|
| `colour` | Colour | — | −1..1 | 1 | lin | M | C | yes | +1 white, 0 pink, −1 brown, continuous between. |

### 5.7 Settled values, not parameters

The human locked or culled these on 2026-10-02 (`docs/audits/2026-10-02-engine-audit-round2-decisions.json`); they are behaviours with one setting, not controls.

| was | now |
|---|---|
| `normExp` locked 0.75 | members sum with density compensation N^−0.75 (SW-19). [X17] |
| `inertiaCurve` locked 2.5 | the inertia knob's response follows w = knob^2.5 (SW-8). |
| `voiceCull` locked −80 dB | a voice under −80 dB counts as faded (VO-7). |
| `dcMode` locked per cycle | no blade leaves DC (BL-20). |
| `absK` locked off; `motionCenter` locked 0; `panMode` locked per-member; `tilt`, `hiTame`, `lpOut` locked | not present. |
| `aaCarrier`, `aaXin`, `aaLoop` locked on | their aims are requirements ED-1 and BL-18. |
| `basin` (round 1 lock 35 c; round 2 deferred) | gravity's capture radius is 35 c [Q31]. |
| `panCurve` (deferred) | the fans use a fixed curve [Q31]. |
| culled: stretch law, `bpm` and `tune` as parameters (they are inputs: IF-2, IF-11), ring reach, poles, `polyGlide`, `panScatter`, the saw-shape family, `pitchEnv` as its own envelope, the engine selector, the blade envelopes (11 keys), the bench-only keys | not present. Blade envelopes become ENV 2 to ENV 4 through the matrix (Appendix B). |
| `rtone` moved | to the filters (a per-note cutoff control, with R as a modulation source, IF-3). |
| `poly` moved | to Settings: `voice.polyphony`. |

### 5.8 Counts

- **Rows:** voice 18 (plus 4 on switches), oscillator 38, blade 30 (plus 2 blade-2-only), sub 10, noise 1: **103 rows**.
- **Host parameters:** 18 + 4 + 2 × (38 + 2 × 30 + 2) + 10 + 1 = **233**.
- Deferred rows carried as recommended [Q31]: six (`onsetLock`, `dissolve`, `driftRate` with its mode, `inertia`, `onsetScatter`, `timingCorrection`), plus the ENV 1 curves and velocity as envelope decision 1.

---

## 6. Behaviours

Each requirement states what the player hears; where a law is the sound, the requirement points to its formula in Appendix C, which is normative.

### 6.1 The swarm (SW)

| id | requirement | source |
|---|---|---|
| SW-1 | Each oscillator of each voice is a swarm of U members (`unison`), each a phase-continuous oscillator at its own frequency around the oscillator's pitch. A member's frequency may change at any time; its phase never jumps because of it. | SPEC §2; audit S1 |
| SW-2 | **Placement** follows `distribution` by C.2 (even, JP-8000, Gaussian, Cauchy, golden). Seeded draws come from the oscillator's `seed`; the same seed gives the same placement. | SPEC §3, §6.7; audit S7 |
| SW-3 | **Detune** follows `detuneLaw`, `detune`, `detuneRange`, `harmonicReach` and `anchor` by C.3. In the cents law with even placement, `detune` 0.1 at range ×1 puts the outermost members exactly ±10 cents from the pitch. | SPEC §4; audit S4, S8 |
| SW-4 | **Coupling** follows C.1: the pull on each member is a frequency offset in Hz, of strength km = 4·K·\|K\| in units of σ, toward sync for K > 0 and toward even splay, with three times the authority, for K < 0; so the same knob position gives the same regime at any detune, register and law. Claim anchors (7 members, JP-8000, cents law, `detune` 0.28 at ×1 (the outermost members ±28 c), A3 = 220 Hz, 44.1 kHz, start aligned, no drift; R read about once a second, values ±0.08): K +1.0 (km 4.0) sustains R ≥ 0.95; +0.85 (km 2.89) holds a steady partial lock; +0.7 (km 1.96) oscillates, R swinging between about 0.25 and 0.8 (read densely, not once a second); +0.6 (km 1.44) from an aligned start falls to no sustained lock within about 3 s; −1.0 after 4 s reads R ≤ 0.10 and R_N ≥ 0.75, with the 7th harmonic at least 5 × its uncoupled level; −0.7 reads R_N 0.44 ± 0.1. | SPEC §3, §5.1–5.2, §6.3; ACCEPTANCE L0-2, L0-3; ADR-007; audit S1 |
| SW-5 | **Phase lag** α enters the sync pull as in C.1: at K +0.9 (km 3.24), α 60° lowers the locked mean frequency at 220 Hz by about 1.8 Hz (direction and monotonic growth are the requirement; the size ±25 %). | SPEC §5.1; ACCEPTANCE L0-9 |
| SW-6 | **Pivot**, root member: by C.1, under positive coupling every member is pulled toward the member nearest the played pitch, so a locked swarm sounds at the played pitch, not at its drifted mean. | audit S14 |
| SW-7 | **Drift**: each member wanders by the walk of C.4, up to `driftDepth` cents, at the rate `driftRate` sets, from the oscillator's seeded stream. [Q31: drift mode] | audit S9 |
| SW-8 | **Inertia** follows C.5: a member's offset from the oscillator's pitch swings toward its target with weight and overshoot; the oscillator's own pitch (the note, bend, glide) is never swung. Lowering the knob to 0 while a swing is in flight lets the swing finish at the fastest setting; the frequency never jumps. [Q6] | SPEC §5.3; audit S11 |
| SW-9 | **Onset lock**: at note-on the burst Kenv = 8·onset·\|onset\| (in units of σ, like km) joins the coupling by C.1 and decays with time constant `dissolve`. At 0 nothing changes. [Q31] | ADR-056; audit S3 |
| SW-10 | **Start phase.** Fresh: member i starts at phase `scatter`·uᵢ, uᵢ a seeded uniform draw in [0, 1). Settled: the members start in the state the coupling law would reach after 1.5 s from fresh at Scatter 0 (K > 0) or Scatter 1 (K ≤ 0), at the note's pitch and settings, so a note does not audibly slide into its steady state. Continue: each member starts at the phase the same member of this oscillator has, at the note-on sample, in the voice most recently started that is still sounding; if none is sounding, at the phase that member had in the voice that stopped sounding last, at the sample it stopped (a taken voice counts as sounding for its whole fade). Every start is independent of the host's block size. [X1] | audit S13; B475; ADR-216 A3 item 11 |
| SW-11 | **Gravity** follows C.6. Claim anchor: 220 Hz and 331 Hz (a fifth 5.2 c sharp) at gravity 0.7 settle within ±0.5 c of 3/2 in about 3 s; a pair outside the 35 c basin moves 0 c; a released voice keeps the pitch it had at release; a newly struck note starts at its played pitch while held notes keep the pitch they settled to. The result is independent of block size and the same at every sample rate (a fixed time grid). Gravity acts on the voice's pitch, so both oscillators and the sub follow it. [Q8, Q9] | ADR-008; ADR-086 A1; ACCEPTANCE L0-8; audit S15, P2 |
| SW-12 | **Onset scatter** follows C.7: at note-on each member waits a seeded offset, silent and with its phase held, then enters by a fade; the offsets are corrected toward the ensemble's mean with gain `timingCorrection` and carried across notes, so the asynchrony has the structure of players correcting each other. Claim anchor (`onsetScatter` 30 ms, many notes): the lag-1 autocorrelation of a member's offset is near +1 at α 0, about +0.7 at 0.25, near 0 at 1 and negative at 1.5; the ordering is the requirement. [Q13] | ADR-077; audit S16 |
| SW-13 | **A member near the band edge fades out**: a member whose frequency rises from 0.40 to 0.48 of the internal rate fades from full to silent (linearly in frequency) and is silent above; never folded back, never a DC offset. (A blade carrier is capped instead, BL-4; the sub is capped, SN-1.) [X2] | B378 F2; audit S17 |
| SW-14 | Every time constant, rate and duration in the engine is defined in seconds or Hz and converted to the running rate, so a patch behaves the same at 44.1, 48, 88.2, 96, 176.4 and 192 kHz. No constant is tuned per tick or per sample. | ADR-009; charter §Domain; audit S2 |
| SW-15 | A note's first output sample already plays its members' correct frequencies, blade settings and DC correction; nothing in the engine warms up over the first control intervals. | B325; audit V5 |
| SW-16 | **Motion grain** steps only the spread law's values: at `motionGrain` 1 each control interval's new spread values reach the blades at once (the grain and air the human preferred in B460's blind A/B, which compared the spread ramp only); at 0 they ramp across the interval (BL-17); between, the step is the fraction m of the change and the rest ramps. The frame's lead always ramps, at every setting (ADR-216 item 1, ruled). [X8] | ADR-224 item 5; B460; ADR-216 item 1; audit B5 |
| SW-17 | Changes of `coupling` and `detune` from any source are smoothed like every other continuous parameter (PA-1): automating either never steps the member frequencies. | audit B6, M1 |
| SW-18 | **Pan** follows C.8: an equal-power law at a seat set by `panImage` and `width`; width 0 is mono; in the balanced image the left-right brightness differs by under 1.5 dB with Width spread 0.5 at 4 to 16 members; a two-member swarm cannot be balanced and is stated so. Pan motion moves seats slowly on a fixed time grid, independent of block size. No member is polarity-inverted between channels. [Q26] | SPEC-SCALPEL §6, §9; ADR-086 amendment; audit V9 |
| SW-19 | The swarm's sum is scaled by U^−0.75 (C.8), the human's lock. [X17] | audit round 2 `normExp` lock; audit V10 |

### 6.2 The blades (BL)

| id | requirement | source |
|---|---|---|
| BL-1 | **Geometry** follows C.9.1: a window of width w (off below 0.004 cycle) centred at c on each member's cycle; inside, a crossfade from the input wave toward the blade's sound by gate × depth, the gate's raised-cosine taper lying inside the window; outside, the input wave unchanged. | SPEC-SCALPEL §4.1; audit B1 |
| BL-2 | **Modes** follow C.9.2. In every carrier mode (Sync, FM reset, FM free, Ring) the carrier's phase restarts at 0 at each window entry; FM reset also restarts its modulator (and the pitch-FM accumulator); FM free's modulator runs on. Noise holds a uniform value in [−1, 1), a new one each whole carrier cycle inside the window and at each entry. Fold is sin(π/2·(1 + (k − 1)/4)·x) with k the cut rate as set (never per blade). Crush holds ⌊k⌋ whole steps and a last partial one when k is not whole (C.9.4). | SPEC-SCALPEL §4.2, §4.4; audit B1 |
| BL-3 | **Waves** follow C.9.3, the sine→saw shape included (its brightness r capped by the carrier's own frequency). | SPEC-SCALPEL §4.3; audit B1 |
| BL-4 | **Cut rate.** "× pitch per cycle": the carrier runs at `cut` × the member's frequency; "per blade": `cut` cycles across the window; Hz: `cutHz` whatever the note. Every carrier rate is capped at 0.45 of the internal rate (a cap, not a fade). | SPEC-SCALPEL §4.1 |
| BL-5 | **FM** follows C.9.2: phase FM offsets the carrier phase by (index/2π)·modulator; pitch FM bends its rate by (1 + index/10)^modulator (±1200·log₂(1 + index/10) c). The modulator runs at `modRate` × the member's pitch or at `modRateHz`. | SPEC-SCALPEL §4.2 |
| BL-6 | **Mirror** follows C.9.5. On a half-wave-symmetric base (sine, triangle, square) twin − removes the even harmonics (at least 100 dB down) and adds no DC. | SPEC-SCALPEL §4.5 |
| BL-7 | **Crush** follows C.9.4: sweeping the cut rate from 2 to 60, every base wave, Edges > 0: no sample-to-sample jump above 0.01. | SPEC-SCALPEL §4.4, §9 |
| BL-8 | **Rotation.** `rotate` turns the window around the cycle at that rate in Hz; "restart per note" starts each note's window at Position; "free-running" uses one clock for every voice, advanced by the engine's sample clock; blade 2 turns with blade 1 or on its own. When a rate returns to zero the window glides home by the shortest way, with a time constant of 30 ms. | SPEC-SCALPEL §6 |
| BL-9 | **Blade 2** is a full second blade. Every "as blade 1" or "blade 1's" option makes it follow blade 1's value of that setting. With blade 2 off, the oscillator sounds exactly as with blade 1 alone. | SPEC-SCALPEL §4.6 |
| BL-10 | **Frame** follows C.13: in swarm frame each member's window sits at its lead from the voice's frame angle; the frame follows ψ where the swarm has a centre and never jumps where it has none (R near 0); where it is locked the frame equals ψ. At a note's start the frame is ψ if R ≥ 0.25, else member 0's phase. The lead always reaches the blade as a ramp. What the frame does after a crossing is [X7]. | ADR-216 item 1, A1; B461; audit B4 |
| BL-11 | **Interplay** follows C.11. With `upperHears` 0 and both collisions 0 the output is the same whichever blade is upper. | SPEC-SCALPEL §13; audit B1 |
| BL-12 | **Cross-mod and feedback** follow C.14: one sample late, in a ring of members, changing timbre only; the loop is low-passed so it never settles into a limit cycle at either quality. | SPEC-SCALPEL §6; ADR-189 A1 (D3); audit B9 |
| BL-13 | **Spreads** follow C.10: each spread's per-member position p comes from the spread law; a single member gets p = 0; blade 2's own spreads use draws independent of blade 1's. | SPEC-SCALPEL §5; audit B2 |
| BL-14 | **A negative spread is the mirror image of the positive**, exactly, Quantize included (a whole-harmonic snap rounds half away from zero), rotation included. | ADR-184 A2; B315; audit B3 |
| BL-15 | **Cut rules** follow C.10.3; ratios match those lists to 0.01. An unreadable custom list is refused and reported, and the previous list stays. | SPEC-SCALPEL §5 |
| BL-16 | **A quantized cut never changes mid-cycle.** Under Quantize (or a snapped rule), when the spread law moves a member to a different ratio the change waits until that blade's carrier next restarts at its window entry, where the carrier's phase is 0, so it is never heard as a jump. The one stated exception: a twin window wider than half a cycle is mid-window when the main window restarts, so its change still lands mid-window; no bank preset uses that case [X9]. Over a long note the time-weighted share of every ratio is the same as without the wait, within ±0.001 (no tone bias). This holds at every `motionGrain`. | B487; B485; audit B5 |
| BL-17 | **No control-rate step reaches the sound**: blade position, cut, width, depth, index, rotation and the sine→saw band limit, as fed to the blade (not only as stored), arrive as ramps across each control interval, at `motionGrain` 0, and always for the frame (SW-16). | ADR-216 A4; B460; B484; audit B5, H4 |
| BL-18 | **Noise.** Each voice, and each member within it, draws its noise from its own seeded stream, started at note-on; one note never changes another note's noise. A noise step is a step of the sound and is band-limited like any other edge (ED-1). [X4, X5] | B469; B481; audit B16, B17 |
| BL-19 | **DC.** No blade leaves a DC offset: with any mode, wave and spread, the output's mean over each second of a held note is within 0.003 of zero, including while the cut rate steps or sweeps. With cross-mod, feedback or collision on, a first-order DC blocker at 8 Hz also runs, since those depend on past samples. | SPEC-SCALPEL §7, §9; audit B13 |

### 6.3 Edges and aliasing (ED)

| id | requirement | source |
|---|---|---|
| ED-1 | **Every hard step the engine makes is band-limited**: base-wave discontinuities, window and twin edges, carrier wraps (also under FM, cross-mod and feedback, where the step's time follows the phase push), Crush steps at Edges 0, noise steps. The correction is a **minimum-phase band-limited step table of 32 taps, read with interpolation** (ruled, ADR-222), scaled by `bandLimit` (1 full, 0 none). Step heights are computed from the waveform's closed form at the step, not by evaluating the whole blade twice. | ADR-222; ADR-189 (D1, D2 aims); SPEC-SCALPEL §8.2; audit B11, B14 |
| ED-2 | The correction adds no latency: it is causal, with no look-ahead, so the cross-mod and feedback taps read finished samples. | ADR-222 |
| ED-3 | The table's own DC offset (it grows with pitch: about 0.23 on a full-scale saw at C8 at 44.1 kHz) is removed by subtracting its known constant at each edge, never by delaying the smooth part. | ADR-222 item 2(b); research note F5 |
| ED-4 | The table's cutoff keeps the top of the audible band: on a saw at C6 the 16 to 20 kHz band is within 3 dB (target 1 dB) of an ideal band-limited saw. The table is stored no finer than its read needs. | ADR-222 item 2(a), (c); research note F3, F4 |
| ED-5 | Each member keeps its pending correction; it is cleared or played out when a voice is reused, never left to leak into a new note; nothing is allocated for it on the audio thread. | ADR-222 item 4 |
| ED-6 | A softened edge (Edges > 0) is a continuous gate and needs no step correction. Corners (a triangle's points, a mirrored window's centre, Crush slew corners) are not steps; whether 1.0 corrects them is [X3]. | ADR-222 item 3; audit B12 |

### 6.4 The voice (VO)

**Levels named once.** A voice's **envelope level** is ENV 1's output, 0 to 1, before velocity and source levels. A voice is **audible** while its envelope level is at or above −60 dB (1e-3); **faded** below it; **freed** when its release has finished (envelope level exactly 0) or, released, below −80 dB (1e-4).

| id | requirement | source |
|---|---|---|
| VO-1 | One allocator serves the whole instrument. Notes are addressed by note id; a note-off or expression for an id reaches only that note's voice. A note-off or expression with note id −1 (CLAP's wildcard) addresses by channel and key: every voice whose note has that channel and key (−1 for either field matches all). Two notes on one key are two voices. | ADR-224 item 1; audit H3 |
| VO-2 | **The voice law (poly).** A new note takes, in order: (1) a free voice, oldest first; (2) a voice whose key is up (a releasing tail), quietest envelope level first, age breaking ties; (3) only when every voice is held, the oldest held voice. A repeated note never reuses its own sounding voice, so the first note's release keeps ringing. The tiers judge a voice by its key: a voice whose key is up is a tail, whatever its envelope is doing; a held voice at sustain 0 that has faded counts as a tail. | ADR-083; B310; ADR-216 A3 item 2; envelope conflicts decision 7; audit V1 |
| VO-3 | **A taken voice fades** (ADR-216 A3, items 1 to 8). When a note takes an audible voice, the old sound fades to silence linearly over 5 ms and the new note starts from silence when the fade ends. Everything about that note is late by its wait, its release included, so it sounds for as long as its key was held. While waiting it counts as held by its key (tiers 1 and 2 never pick it); a retune reaches it (its sounding pitch moves; the struck pitch stays the glide target); a panic or a voicing change cancels it with any pending release; a second take replaces the waiting note without restarting the fade. A culled tail that is taken waits no longer than the shorter of the steal fade and what the cull had left. A zero-length note on a taken voice is silent. If the cull picks a tail whose key is up but whose late release is still pending, it is released at the cull (the pending release cleared); a taken tail's pending release is cancelled by the take. A faded voice restarts at once. Mono and legato are not affected. | ADR-216 item 2, A3 items 1–8; B459; audit V3 |
| VO-4 | **Under load.** The engine accepts a voice cap from the device (an input, never a clock reading). Over it, the quietest releasing tails fade out over 8 ms, never a held voice. A note at a full cap with no tail to take is refused (the default) or, by the device's choice, takes the oldest held voice by VO-3. With no cap the engine never culls. [Q3] | B323; B375; audit V2 |
| VO-5 | `voice.polyphony` sets how many voices exist (1 to 16, one pool). Lowering it while voices sound releases the excess by VO-4's fade, never a cut. | ADR-224 item 1; audit V8 |
| VO-6 | **Mono and legato.** Mono: one voice; every note restarts ENV 1 from its current level, with that note's own velocity. Legato: ENV 1 restarts only when a note arrives with no other key held; otherwise the voice keeps its strike's velocity and envelope. In both, the voice sounds the pitch and the expressions of the newest held note id, and releasing it while others are held returns to the most recent held one without restarting. A mono or legato note change reports which note the voice moves from and to, so the pitch system can glide (IF-2). | SPEC-SCALPEL §6; ADR-084; ADR-208; audit V4; envelope conflicts decision 5 |
| VO-7 | A freed voice outputs exact zeros and holds no state that a later note could hear. | rack contract A5 |
| VO-8 | Velocity (0..1) scales the voice's level by (1 − v + v·velocity), v = `env1.velocity`; at v = 1 level is proportional to velocity. Per-note pressure is a per-voice modulation source (IF-1). | ADR-084; audit V10 |
| VO-9 | **The voice's path** is: each source at its level, summed, through the filter slot, times ENV 1 and velocity. Each source's signal exists separately before the sum (ADR-224 item 2) for the filter slot's routing and the meters. | ADR-224 item 2; B303 |
| VO-10 | Each voice publishes its lifecycle: started (note id), released, taken (old note id, new note id, when the new one will start), freed. | audit §3.1 |

### 6.5 ENV 1 (EN)

| id | requirement | source |
|---|---|---|
| EN-1 | ENV 1 is an attack–hold–decay–sustain–release envelope whose timed stages follow C.12: each stage takes exactly its time and lands exactly on its target (attack on 1, decay on sustain, release on 0), with no step at a stage's end; its shape is set by its curve (0 straight; +1 the bank's exponential shape, the decay and release defaults; −1 its mirror). | B366; B377; audit V11 |
| EN-2 | A note-on from silence starts ENV 1 at exactly 0. A retrigger (mono, legato, a re-struck voice) restarts the attack from the envelope's current level, never from 0 (C.12). A stolen voice's new note starts from silence (VO-3). | ADR-161; envelope conflicts decision 5; VO-3 |
| EN-3 | Changing a time, curve or sustain while a stage runs continues the stage from where the level stands (C.12); the level never jumps. | B377; PA-1 |
| EN-4 | ENV 1 is the same in seconds at every sample rate and independent of block size. | ADR-009; V7 |

### 6.6 Parameter changes and modulation (PA)

| id | requirement | source |
|---|---|---|
| PA-1 | **No zipper.** A change of any continuous parameter from the host, automation, a macro, an intent or the morph glides to its new value through a one-pole smoother with a 12 ms time constant; a full-range jump in one event makes no click by the detector of QS-4. | SPEC-SCALPEL §7; B378 F8; audit B6 |
| PA-2 | **Per-voice modulation, one mechanism.** For every continuous parameter, the engine accepts per voice an offset in the parameter's normalised 0..1 form. The value a voice uses is the parameter's resolved value plus the sum of its offsets, clamped to [0, 1] (wrapped for circular parameters), mapped by the parameter's law. An offset for all voices at once is the same mechanism with every voice addressed. | ADR-224 item 4; audit §3.3 |
| PA-3 | **Control rate.** Offsets arrive and take effect at the control rate: the interval is the whole number of samples nearest 1/1,500 s at the host rate (29 at 44.1 kHz, 32 at 48 kHz, 64 at 96 kHz), each change ramped across one interval. Intervals are counted from the engine's own sample clock since reset, never from the host's block boundaries. | ADR-224 item 5; V7 |
| PA-4 | **Audio rate.** The named destinations also accept a per-sample offset: the voice pitch and the oscillator `pitch` and `fine`, the sub's `fine`, the blades' `cut`, `cutHz` and `fmDepth`. [X16] | ADR-224 item 5 |
| PA-5 | **Stepped keys while a note sounds.** A stepped setting of rate T (mode, wave, mirror, frame, cut-rate and mod-rate units, base wave, the laws and rules, quantize, the pan image, a blade's or source's on switch) changes on sounding voices at once, through an equal-gain crossfade of 5 ms from the old setting's sound to the new; the same holds when the morph flips one (B479). A second change that arrives during a crossfade waits until it ends and then crossfades from the then-current setting to the newest value (changes in between are skipped), so at most two settings ever sound at once. Keys of rate N (`startPhase`) take effect at the next note-on. [Q14] | B479; rack contract RS-3.6, RS-4.3; ADR-218; audit B7 |
| PA-6 | A change of `unison` while a note sounds fades new members in and removed members out over 5 ms; the remaining members continue unbroken. [Q14] | audit B7 |
| PA-7 | **Host values are checked where they enter.** A non-finite value is refused; a value out of range is clamped to it; an unknown option of a stepped key is refused; each refusal is counted and readable. No value the host can send puts the engine in a state no control can reach. | ADR-207 item 2; ADR-213; audit N14 |
| PA-8 | A parameter that cannot affect the sound in the current configuration (a Hz cut rate while the cut is per cycle, Harmonic reach under the cents law) is declared inert by the manifest's dependency rule; an inert parameter never moves the output, and a live one always can. | ADR-187 item 4(b) (the dependency probe) |

### 6.7 Determinism (DT)

| id | requirement | source |
|---|---|---|
| DT-1 | The same state and the same sample-timed event stream give identical output, sample for sample, on one build. Bit-identity across platforms or builds is not required. | ADR-224 item 6; charter §Domain; audit N1 |
| DT-2 | The output is bit-identical however the host splits the same events into blocks (any block sizes from 1 to 4,096, including uneven splits). | module 1.0 bar §3; rack contract RS-7.11; audit V7 |
| DT-3 | At different sample rates the same patch gives the same sound (the same timings, pitches and spectra within the measures of section 8), not the same samples. | SW-14; ADR-197 armor row 4 |
| DT-4 | **Every random draw comes from a mulberry32 stream** named by its purpose (placement, start phases, drift, spread law, noise, onset scatter), seeded from the oscillator's seed, the purpose, the voice's note counter and the member, never from a parameter's position in a list or from how many draws another voice made. **A note-on never changes a voice already sounding**: not its random numbers, not its samples. [Q16] | charter §Domain invariant 1; SPEC-SCALPEL §9; B308 H3; B469; audit N2, N6 |
| DT-5 | The engine reads no clock and depends on nothing outside its state and its inputs. | charter §Domain; SPEC §5.7 |
| DT-6 | After `reset()` the engine renders exactly as a freshly prepared instance with the same state. | rack contract A2 |

### 6.8 Real-time safety (RT)

| id | requirement | source |
|---|---|---|
| RT-1 | Processing, parameter and modulation input, note events, observation reads and `reset()` allocate nothing, take no lock, make no system call or I/O, and run no unbounded loop. Everything is allocated at prepare, sized by the maxima (16 voices, 16 members, both oscillators, both blades). Every lazily built table is built at prepare. | `h2/README.md` rule 5; rack contract RS-7.7, RS-7.10; audit N12 |
| RT-2 | The work per sample is bounded by a constant set by the parameter maxima (the number of edges corrected per member per sample is capped, and the cap is stated). | SPEC-SCALPEL §7 |
| RT-3 | A non-finite output sample is replaced by zero, and the event is counted and latched until cleared, readable from another thread. It is never repaired silently. | ADR-197; rack contract RS-7.9; audit N15 |
| RT-4 | The engine produces no subnormal output samples and no subnormal-driven slowdown: tails reach exact zero; flush-to-zero is set by the shell around processing, and the engine does not depend on it for correctness. | ADR-211; ADR-213 item 3; audit N13 |

### 6.9 The output stage (OU)

**0 dB, defined once.** At `out.level` 0 dB the engine's output is 0.14 × the sum of the voices (0.14 = 1.6 × 0.0875, the small-signal gain of today's engine at its ADR-221 default, into a linear stage), where a single member of a full-scale base wave at envelope level 1, velocity 1, U = 1, contributes amplitude 1 before its pan gains (C.8).

| id | requirement | source |
|---|---|---|
| OU-1 | The output stage scales the sum of the voices by 0.14 × 10^(`out.level`/20) and does nothing else to the sound: no saturation, no limiter, no clamp. Drive is the drive module's. [Q19] | SPEC-SCALPEL §10; ADR-221 item 6; audit V13 |
| OU-2 | **At 0 dB** (a) every bank preset, translated by Appendix B, plays a four-note chord (keys 48, 52, 55, 60) at velocity 0.85 held 3 s with its peak below 0 dBFS (no exceptions); and (b) the bank's loudness is within ±1 dB on average of today's engine at its ADR-221 default, as the level-matched A/B reports. **If (a) and (b) conflict, (a) wins**: the default `out.level` is lowered below 0 dB until (a) holds, and the shortfall in (b) is reported; the definition of 0 dB does not move. | ADR-221 items 1, 2, 4; B489; audit V12 |
| OU-3 | **Migration.** A saved state from today's engine is translated by Appendix B (B.2 for the keys only today's engine has); its stored `gain` g becomes `out.level` = 20·log₁₀(g/0.0875) dB; a state saved without a quality setting opens at High (2×). The sound is translated, not promised identical (ADR-223 item 3). Every later default change carries an ADR and a migration. | ADR-221 item 3; ADR-197; ADR-223 item 3; B445; audit V14, K2 |
| OU-4 | When no voice sounds, the output is exact zero and the engine reports itself silent, so the shell may stop calling it. While silent no voice exists; the only state that moves is the engine's sample clock (which drives free-running rotation and the control and gravity grids), so the shell passes the number of samples it skipped when it resumes, and the engine then renders exactly as if it had been called throughout. [Q24] | rack contract A5, RS-7.6 |

### 6.10 The sub and the noise oscillator (SN)

| id | requirement | source |
|---|---|---|
| SN-1 | **The sub** is one phase, one shape, no members, no coupling, no drift. Its frequency is the voice's published pitch (keytrack on) or C2, 65.41 Hz (off), times 2^(octave + semitones/12) and 2^(fine/1200), capped at 0.45 of the rate (a cap, not a fade). **The sub integrates its own frequency; at note-on its phase is `startPhase` (cycles) and the voice's master phase is 0, so the two start together; it stays phase-continuous under every change** (a glide, a bend, gravity, keytrack, octave). It writes the same sample to both channels. [Q21] | SPEC-SUBOSC §1, §2, §4; audit §3.6 |
| SN-2 | **Sub shapes.** sine (exact); triangle; square and saw and pulse (both edges of the pulse band-limited at every width, by the engine's one edge correction); noise (white, uniform, from the voice's seeded stream, ignoring pitch); bump, y = (sin θ + a·sin(3θ + φ)) / peak(a, φ), peak(a, φ) the true maximum of \|sin θ + a·sin(3θ + φ)\| over a period, so the shape peaks at exactly 1 for every (a, φ), band-limited by construction while 3·f is below Nyquist (at the defaults the trailing bump is 0.882 of the leading one and the trough between them 0.637 of the peak). Sine and triangle start at 0 rising; saw at −1. Level −∞ is exact silence. | SPEC-SUBOSC §3, §5.2, §10.6 |
| SN-3 | The sub's tone is a one-pole low-pass, bilinear-prewarped so its response is the same at every sample rate (30 Hz to 20 kHz, clamped to 0.45 of the rate). The sub's signal goes through the tone, then joins the voice, where ENV 1 acts on it with the rest (the envelope after the tone). [Q21] | SPEC-SUBOSC §5.1, §5.3 |
| SN-4 | **The noise oscillator** is per voice, mono, from the voice's own mulberry32 stream (so one note never changes another's noise): uniform white noise shaped by `colour` (+1 flat, 0 falling 3 dB per octave, −1 falling 6 dB per octave, continuous between, from 20 Hz to Nyquist), at a level independent of colour within 1 dB. It shares the voice's allocation, ENV 1 and filter slot. [X15] | ADR-190 A6; B452; audit §3.6 |

---

## 7. Interfaces to the rest of the instrument

These are the audit's eleven areas (§3). The engine's API is a set of calls and timed events; everything crossing it is in the units of section 5.

| id | area | the engine exposes or accepts | source |
|---|---|---|---|
| IF-1 | **Notes, voices, MPE** | Accepts note-on (note id, channel, key, velocity), note-off (note id, release velocity), per-note expressions (tuning, pressure, timbre, slide, and CLAP's others), each sample-timed. Expressions are addressed like note-offs (VO-1): by note id, or with note id −1 by channel and key, −1 matching all. Per-note tuning goes into that note's pitch (IF-2); pressure, timbre and slide are per-voice modulation sources the modulation system routes; by default pressure scales the voice's level by its value (re-armed to 1 at each note-on, so a host that never sends it hears nothing different). In mono and legato the voice follows the expressions of the newest held note id (VO-6). Velocity is the note-on's, by VO-6 and VO-8. Publishes VO-10's lifecycle events and the load readouts (voices culled, refused, taken). [Q2] | ADR-190 A4; ADR-084; ADR-208; ADR-224 item 1; audit §3.1 |
| IF-2 | **Pitch** | **Pitch is addressed by note id**, like the expressions: the pitch system sends each note's pitch in Hz (bend, note lane and glide, quantizer, MTS-ESP, transpose already applied), at audio rate when it moves. Until a note's pitch is set, the note plays 12-TET from its key (A4 = 440 Hz) plus its per-note tuning. In mono and legato the voice follows the pitch of the newest held note id. Accepts the host tempo as an input (reserved for a tempo-grid law, Q31). Publishes each voice's sounding pitch (the input times gravity's ratio) and master phase (the integral of the sounding pitch from 0 at note-on) for the sub, the noise oscillator and keytracked filters. Gravity's settled ratio rides the input multiplicatively (C.6), so a bend or glide keeps the interval gravity settled. [Q6, Q7, Q8, Q9] | B278; B397; B431; ADR-096; audit P1–P3, §3.2 |
| IF-3 | **Modulation, the intent bus and macros** | Every continuous parameter is a destination by its manifest key and id, with unit, range, law, class and rate (section 5). Per-voice offsets (PA-2) at the control rate (PA-3), audio-rate offsets on the named destinations (PA-4). Every modulation destination is also an automatable host parameter, because wrappers drop CLAP's parameter-modulation events. Sources the engine publishes per voice: R and ψ of each oscillator's swarm, the onset burst's level, gravity's settled offset in cents. Macros and intents bind by key and curve (`lin`, `exp`, `log`) and need nothing more. [Q12] | ADR-224 items 4, 5; ADR-169 A3; ADR-192; B429; audit §3.3 |
| IF-4 | **Morph and state** | The manifest's class column is the morph's input: Morphable values blend; Structural values flip at the morph's seeded point (hysteresis band 0.1) and change on sounding voices by PA-5; Device values stay out. The engine reports the heard value of every stepped key per oscillator, so the GUI shows what is heard where the morph and the stored patch differ. **State** is the parameter values, the `ratios` text and the quality setting: saving, loading and saving again gives byte-identical text and bit-identical renders; floats round-trip exactly; live state (tails, ramps in flight, meters, latches, stream positions) is never saved. A load made while not processing is whole when it returns; a load while voices sound fades them over 5 ms and starts fresh (the engine's memory is reset: streams rewound from their seeds, gravity, the ensemble's carried offsets and the continue-phase snapshot cleared). A saved project restores as saved. [Q15, Q17] | ADR-104; ADR-185; ADR-188; ADR-208; rack contract RS-6.6–6.13, A17–A20; audit §3.4 |
| IF-5 | **The per-voice filter slot** | The voice passes each source's signal and the sources' sum to the filter slot, stereo-linked (the same coefficients on both channels), with the voice's published pitch for keytracking, its per-voice modulation and its lifecycle (a filter's state is cleared when its voice is reused). A filter in the slot must add no latency, or declare its latency so every parallel branch is aligned. The slot passes the signal unchanged when empty. [X13] | ADR-224 items 1, 2; B303 |
| IF-6 | **Mixer and gain staging** | Levels are named by the rule Gain = a stage's multiplier, Level = a contribution to a mix, Volume = the master only. Meters (peak and a latching clip flag that catches a single sample over 0 dBFS) at: each source after its level, summed over voices; the engine's output. Every tap is before any saturation. The master stage (outside) restores loudness; the engine does not. [Q20] | B225; B402; B376; ADR-221; ADR-203 item 5; audit §3.5 |
| IF-7 | **The FX rack** | The engine's output is one stereo pair at the host rate with zero latency, into the rack's first slot. The engine offers no insert point of its own. [Q23] | ADR-222; rack contract RS-2.8, RS-7.1 |
| IF-8 | **True stereo** | Members are mono and panned (SW-18); the stereo field after the voice is the device's. The engine exposes its width per oscillator to whatever width equation the stereo lab settles; it guarantees no member is polarity-inverted between channels. [Q26] | ADR-224 item 3; ADR-190 A9; B408 |
| IF-9 | **Quality and CPU** | `quality` is per patch (saved), Device class, never morphed; changing it while voices sound fades them over 5 ms. Operation counters (members, edges corrected, voices) are readable for the CPU measurement. [Q27] | ADR-191; B445; ADR-187 item 8 |
| IF-10 | **The labs** | A WebAssembly build of the same source, with the same API, which a lab loads in an AudioWorklet. An observation view, read without changing the audio: per voice and oscillator, members' phases, frequencies and levels, R, ψ, the frame angle, gravity's ratios, onset waits, the heard stepped values. No lab carries its own engine. [Q30] | ADR-223 item 4; audit §3.10, N21 |
| IF-11 | **The host (CLAP)** | Through the shell: the manifest generates the host parameter table, committed as a lockfile (id, key, min, max, default, taper, steps), ids explicit, append-only, below 0xB00000; activation refuses a sample rate that is non-finite or outside 8,000 to 768,000 Hz (the certified range is 44.1 to 192 kHz); the engine takes host tempo and transport as inputs. [Q31] | B308 H4; B428; ADR-207 item 2; audit §3.11 |

---

## 8. Sound and quality standards (measurable)

Each is measured by the invariant suite (section 10.1) on the bank and on generated patches. Where a number below is a target rather than a ruling, it says so.

| id | standard | source |
|---|---|---|
| QS-1 | **Aliasing, edges.** At Standard quality, 44.1 kHz, one member, base saw with no blade and with a Sync blade (saw carrier, cut 6, width 0.25): alias power at least 110 dB below harmonic power at C2, C4, C6 and C8 (ADR-222's table measured 112 to 139 dB). Method: a note with an odd number of cycles in a 65,536-sample window, so each harmonic and alias has its own bin; every non-harmonic bin from 20 Hz to Nyquist counted; a naive saw as the must-fail control (it reads 9 to 27 dB). | ADR-222; research note F1; audit B11 |
| QS-2 | **Aliasing, the top of the band.** The edge correction keeps 16 to 20 kHz on a saw at C6 within 3 dB of an ideal band-limited saw (target 1 dB). | ADR-222 item 2(a) |
| QS-3 | **Aliasing, what the table cannot reach**, measured and reported per bank preset at both qualities by the estimator that compares a render with renders at four and eight times the rate (bins classed clean, folding or dynamics): FM, ring and fold with sine carriers at Standard quality alias by design (ADR-191); High quality reduces them. Reported, not judged, until the human rules a floor. | ADR-191; B346; B445 |
| QS-4 | **Clicks.** **The bright-patch click detector is the test author's first ratified deliverable, built and ratified before any engine stage** (it does not exist yet, B490). Its shape is the requirement it must meet: renders are normalised to a stated peak (−6 dBFS) before detection; an order-p linear predictor (p stated, about 8 to 16) fitted over the previous M samples (M stated, about 2 to 5 ms) predicts each sample; a hit is a prediction residual above k times the local residual RMS (k stated); its must-fire control is a step of 0.05 × the render's peak planted in Crushed bells at random positions (caught on every run), with the same plant in the Breathing pad; its must-read-zero control is the bank's clean renders of held notes with no event (no hit). p, M and k are chosen by measurement and ratified with the controls. With it: a note from silence, a stolen voice (VO-3), a culled tail (VO-4), a stepped-key change (PA-5), a morph flip, a full-range parameter jump (PA-1), a quantized cut change (BL-16) and a frame crossing (BL-10) make no detected click; the swarm's own grain (SW-16 at 1) is measured and reported, not judged. | B490; B459; B461; B485; B486; ADR-218 |
| QS-5 | **Headroom.** OU-2 (a): the bank's four-note chords at velocity 0.85 at 0 dB peak below 0 dBFS, on every preset. | ADR-221 item 4; B489 |
| QS-6 | **DC.** On every bank preset and on generated patches, the output's mean over any second of a held note is within 0.003 of zero (BL-19). | SPEC-SCALPEL §9 |
| QS-7 | **Silence and denormals.** After the last voice is freed, every output sample is exact zero; across the bank's tails no output sample is subnormal. | rack contract A5; SPEC-SUBOSC §10.5; RT-4 |
| QS-8 | **Bounded output.** Over the whole parameter space (fuzzed, with extreme values and automation sweeps), every output sample is finite and the output stays bounded (a stated peak bound per configuration class, with the bank's own peaks as the anchor). | ADR-197 armor rows 3, 6; ACCEPTANCE L0-13 |
| QS-9 | **Long-run stability.** A 4-hour soak of reference patches keeps level, DC and spectral centroid within stated bounds. | ADR-197 armor row 5 |
| QS-10 | **Mono fold.** The mono sum of each bank preset is reported against its stereo power (the loss in dB), as the stereo lab's input. | ADR-197 armor row 9; B408 |

---

## 9. Performance standards

| id | standard | source |
|---|---|---|
| PF-1 | **The CPU target.** The heaviest bank patch (Glass horde pad, translated), 8 voices, Standard quality, 44.1 kHz, 128-sample blocks: about **10.7 % of one Apple M3 performance core** for its one swarm oscillator. (Today: 27.2 %.) The arithmetic: the sources' 34 % of a min-spec core, less about 2 % for the sub, shared by two swarm oscillators, is about 16 % each, and 16 % ÷ the assumed ×1.5 = 10.7 % of an M3 core. The B493 audit estimates a clean engine at about 11.5 % (7.8 % best, 15.3 % cautious), and that estimate **excludes** the always-on ramps (D8 measured +2.7 to +9.9 %) and per-voice modulation; with the ramps it is 11.8 to 12.6 %. **A miss is likely**; it is reported with what would close it (10.4), never hidden. | B488; B494; audit §4.2 |
| PF-2 | **The whole device.** Two swarm oscillators each on the heaviest bank patch, the sub on, and per-voice modulation running on every continuous parameter of both oscillators at the control rate (and on the audio-rate destinations at audio rate), 8 voices, Standard: the sources' share is at most 34 % of a min-spec core (the device envelope is under 50 %: sources 34 %, FX 12 %, modulation 2 %, master 1 %, headroom 1 %). Min-spec is an Apple M1 base or a 4-core 2018-class Intel ultrabook, Windows x64 AVX2; an M3 figure converts by an assumed ×1.5 until measured [Q29]. | B439; B488; ACCEPTANCE E-6 |
| PF-3 | **How it is measured.** The frozen protocol of `docs/port/cpu-ledger.md` (44.1 kHz reference, 48 kHz secondary; 128-sample blocks; all notes held before the first block, keys 48 + 3k, velocity 0.85; 0.25 s untimed then 2 s timed; best of 5, interleaved; the calibration loop; the load guard, never forced), as a new protocol version for the new engine (its build flags differ: fused multiply-add is allowed), re-baselined in the same change. 16 voices are one pool of 16, not two engines. CPU is measured and reported, never a verify gate; meeting PF-1 and PF-2 is judged at acceptance (section 10). | `docs/port/cpu-ledger.md` §protocol; ADR-187 item 8 |
| PF-4 | **Beyond the bank.** The CPU is also reported on generated patches (combinations the bank lacks), under per-voice modulation, and across a morph between corners with different stepped keys; no patch falls to a path more than 1.5 times slower than the bank's median per member-sample without that being reported by name. | audit M4 |
| PF-5 | **Polyphony.** Up to 16 voices; the default 8. Cost grows linearly with voices and members. | audit V8; cpu-ledger |
| PF-6 | **Cost when off.** Blade 2, the twins, cross-mod, feedback, interplay, collision, an oscillator that is off, the sub and the noise oscillator when off cost nothing measurable (under 1 % of the patch's CPU each). | SPEC-SCALPEL §8.8 |
| PF-7 | **Memory.** Everything is allocated at prepare (RT-1). The worst-case memory per instance is stated in the manifest and reported (today's engine: about 125 KB for 8 voices of 9 members). | rack contract RS-2.6; audit §9 |
| PF-8 | **Latency.** Zero at both qualities: no stage looks ahead, the decimation at High quality included. | ADR-222; rack contract RS-7.1 |

---

## 10. Acceptance

The new engine replaces today's when all of 10.1 to 10.4 hold and the human says so (ADR-223 item 3; B494 step 4). Passing is not done: the suite green, the bank judged, the CPU reported, and a trace written.

### 10.1 The invariant suite (Layer-0, deterministic, in `./verify`)

Written by a different agent from this spec, before the engine exists (ADR-223 item 2). Every check has a must-fail control (a planted fault that turns it red on every run). The click detector of QS-4 is its first deliverable, ratified before any engine stage is built. It must prove at least:

0. every law of Appendix C, checked directly where it can be observed (placement offsets, detune frequencies, coupling targets, gravity's ratios and pair moves, the onset offsets, the drift walk's statistics, pan gains, the envelope's shape, every blade law on a single member), each against its equation within a stated tolerance;
1. non-finite safety and the output guard's latch (RT-3, QS-8);
2. bit-determinism on one build (DT-1) and one note never changing another's random numbers (DT-4, BL-18);
3. block-size independence (DT-2) and sample-rate independence (DT-3, SW-14, EN-4);
4. DC and peak bounds, and headroom on the bank (QS-5, QS-6, OU-2);
5. silence, exact zeros and no subnormals (QS-7, VO-7, OU-4);
6. long-run stability (QS-9);
7. the aliasing floor (QS-1, QS-2) and the aliasing report (QS-3);
8. real-time safety: no allocation, lock or system call on the audio thread, bounded work (RT-1, RT-2), by a sanitizer run and by inspection rules;
9. clicks, by the detector of QS-4, with its own must-fire control on a bright patch;
10. the claim anchors: coupling's regimes (SW-4), phase lag (SW-5), gravity (SW-11), onset scatter's structure (SW-12), the frame never jumping (BL-10), the quantized cut's timing and no tone bias (BL-16), every spread kind ramping as fed to the blade (BL-17), twin − cancelling even harmonics (BL-6), Crush continuity (BL-7), interplay's parallel identity (BL-11), the negative-spread mirror (BL-14), the cut-rule ratios (BL-15), the steal fade's rules (VO-3), the voice law (VO-2), the sub's bump shape (SN-2);
11. live parameters move the output and inert ones do not (PA-8), for every manifest row;
12. host values checked on entry (PA-7);
13. state round trip, reset to fresh, and migration (IF-4, DT-6, OU-3);
14. the zipper measure: a host staircase of 128-sample steps against a per-sample ramp, high-passed at 250 Hz, on every continuous parameter (PA-1, PA-2, PA-3);
15. the WebAssembly build renders the same sound as the native build, within the measures of section 8 (IF-10).

Its thresholds are ratified with the suite and pinned in the tolerance registry; a threshold changes only on the human's approval (ADR-197).

### 10.2 The frozen goldens

Once the suite is green and the human has accepted the sound, the new engine's renders of the reference scenarios are frozen as its goldens (the self-digest files, re-baselined by their tool, never by hand; a sanctioned step on a protected path). From then on every change to the engine's sound is recorded with an A/B set and the human's word [X6].

### 10.3 The bank, by the human's ear

- The whole bank (83 presets, translated by Appendix B), rendered offline by the A/B tool (ADR-220; `tools/h2_ab.mjs`): today's engine as A, the new engine as B, the same phrases (a held note, a chord, a fast line, a trill), with the harness driving each note's pitch through a glide (the lag law, the preset's glide time, overlapping or always) where the preset glides, since the new engine leaves glide to the pitch system (Q6); level-matched, the level of the difference beside each pair, played looping with a click-free switch and a blind mode.
- **"Sounds good" is a process, not a number.** The human listens in capped listening batches (ADR-197): each batch a short list, never an interruption. For each preset the human marks it recognisable and good, recognisable but needs a change (the preset is edited, not the engine, unless the human says the engine is wrong), or not recognisable (the engine or the translation is wrong for that preset, and the cause is found). The bank passes when the human says it does.
- The batches carry, at least: the ten presets whose sound changed most beyond level under ADR-221 (Clockwork, Orbiting cuts, Ring of sines, Crushed bells, Drifting cuts, Anchored horde, Two-formant vowel, Vowel choir, Golden bells, Cross-mod shimmer), to hear whether the old output saturation's grit mattered; Glass horde pad and the Breathing pad for the grain (SW-16) and the crackle (BL-16); every preset whose start phase translates (X1).

### 10.4 The CPU

PF-1 and PF-2 measured under PF-3 and reported to the human with PF-4's spread. The target is part of the judgement; a miss is reported with what would close it, never hidden.

### 10.5 Build requirements

- **BD-6. Self-documenting and concise** (ADR-223's mandate). Names carry what the code does; comments carry only why: the constraint, the rejected alternative, the trap. No comment narrates history, parity or a switch. The audit's estimate for a clean engine is about 2,100 to 3,000 lines; it is an estimate, not a gate.
- **BD-7. No emulation of another language's number rules**, no random draws taken to keep another engine's order, no second value of a constant. Single precision, approximations and fused multiply-add are allowed throughout, judged by the suite and the ears (ADR-224 item 6). (audit §1.3; N3–N9)
- **BD-8. Built in stages behind the frozen structure**, each stage green on its part of the suite before the next; the parameter table stays open until Q31 is ruled. (audit §5.5)
- **BD-9. Exactly what is tested ships**: the build that the suite and the CPU measurement run is the shipped build's code with no product-line branch added. (audit N10)
- Section 3.4's BD-1 to BD-5 (the file layout and the one manifest).

---

## 11. Open questions, for the human

Six of the audit's 31 were ruled by ADR-224 (Q1, Q10, Q11, Q18, Q25, Q28). Five more are ruled by existing records and are dropped here (11.4). The rest are sorted by when they must be ruled: **before the build** (they shape structure the builder freezes first), **during the build** (before the stage that needs them), **after** (they do not block 1.0's engine). `Qn` keeps the audit's numbering; `Xn` are this spec's. Each has a recommendation and one line of reason; the requirements that depend on it carry its tag.

### 11.1 Must be ruled before the build (10)

| # | question | recommendation | reason | depends |
|---|---|---|---|---|
| X10 | The Unison range. | 1 to 16. | It sizes every per-voice buffer and the SIMD lanes; the bank uses up to 9; 32 doubles the worst case for little. | 5.3, RT-1, PF-5 |
| X16 | Which destinations take audio-rate modulation. | Voice pitch, oscillator pitch and fine, sub fine, blade cut rates and FM depth. | It decides which paths run per sample; these are where audio-rate motion is a sound (FM-like), not a smoothing. | PA-4 |
| X8 | The micro-motion texture (as fixed in round 2). | `motionGrain` per oscillator steps only the spread law's values; default 1 (stepped); the frame always ramps; the quantized-cut latch applies at every setting. | ADR-224 item 5 and B460's blind A/B (which compared the spread ramp only); ADR-216 item 1 ruled the frame. | SW-16, BL-16, BL-17 |
| Q14 | How a stepped key changes while a note sounds. | At once, by a 5 ms equal-gain crossfade; a change during a crossfade waits for it to end (PA-5); Unison changes fade members (PA-6). | It decides whether every oscillator must be able to render twice for a moment, which shapes the oscillator's structure. | PA-5, PA-6 |
| Q16 | Does the position-free draw rule (B308 H3) bind the engine's draws? | Yes: mulberry32 streams keyed by (seed, purpose, note counter, member). | It fixes the random streams' structure, and is what makes one note independent of another (B469). | DT-4 |
| Q21 | Which phase the sub follows, and two departures from SPEC-SUBOSC §2. | The sub integrates its own frequency from `startPhase` at note-on, while the voice's master phase starts at 0, so the two start together (SN-1). **Departure 1:** SPEC-SUBOSC §2 has the voice hand the sub oscillator 1's phase; osc 1 is a swarm with no single phase, its members drift, and an integrated phase stays continuous under every pitch change. **Departure 2:** SPEC-SUBOSC §2 applies the envelope before the tone; here ENV 1 acts after it, with the rest of the voice, so every source has one envelope position (the tone's one-pole rings for at most about 5 ms even at 30 Hz). SPEC-SUBOSC itself is not edited. | It decides the voice's master phase and the sub's place in the voice path. | SN-1, SN-3, IF-2 |
| Q9 | Which pitch the engine publishes to the sub and the filters. | The sounding pitch: the input times gravity's ratio. | The voice's master phase integrates it; the sub must stay in tune with what the oscillators play. | IF-2, SN-1 |
| Q8 | Gravity against MTS-ESP and the quantizer. | The tuning system sets each note's target; gravity acts on top, multiplicatively, only when its knob is up (default 0). | It fixes the order of the pitch path inside the voice; a user who wants fixed tuning leaves gravity at 0. | SW-11, IF-2 |
| X18 | The rules gate's precision rule (`tools/h2_rules_check.py` rule 2, every h2 core built with `-ffp-contract=off`). | Sanction the gate change: the rule no longer applies to the new engine's tree; it stays for the legacy cores while they exist. | The precision ruling (ADR-224 item 6) allows fused multiply-add; the builder's first compile meets this gate, and changing a gate is the human's. | BD-7, 3.2 |
| Q30 (toolchain part) | The WebAssembly toolchain, a new dependency not yet installed. | Approve one pinned toolchain version now. | The labs' stage and the WebAssembly check of 10.1 item 15 need it; approving it early keeps the build in one pass. The prototypes' fate is in 11.2. | IF-10, 3.1 |

### 11.2 To be ruled during the build, before the stage that needs them (25)

| # | question | recommendation | reason | depends |
|---|---|---|---|---|
| Q2 | MPE: which expressions reach the swarm and the blades. | Per-note tuning into the note's pitch; pressure, timbre and slide as per-voice mod sources, pressure to level by default. | The note-id voice already gives MPE its plumbing; routing is the matrix's. | IF-1 |
| Q3 | The voice limit: a fixed count or CPU-adaptive. | A fixed count (`voice.polyphony`) plus the device's cap input, which the device's default does not drive. | Determinism and a voice law you can predict. | VO-4 |
| Q6 | One glide or three. | One note glide, in the pitch system's note lane; keep inertia as the members' weight; cut the per-member frequency glide. | The human: inertia "kind of ends up doing the same thing as Freq Glide"; inertia was asked for by name (B298). | SW-8, IF-2 |
| Q7 | Pitch bend: global or per note, its range, and where its laws live. | Per note under MPE through note expressions, global otherwise; a per-patch bend range, default ±2 st; the laws stay in the pitch system's bend lane. | ADR-097 and B431; the engine consumes resolved pitch. | IF-2 |
| Q12 | Which swarm observables are modulation sources and lab readouts. | Sources: R per voice and oscillator, the onset burst level, gravity's offset in cents. Readouts: those plus ψ, member phases, frequencies and levels, the frame angle. | R is "coherence as a source" (H2-PLAN); the rest is what the phase ring and cycle view draw. | IF-3, IF-10 |
| Q13 | Where onset scatter lives. | In the engine, per oscillator, by C.7. | The human: "an important knob I would like to preserve", without using up an envelope slot (B370). | SW-12 |
| Q15 | Does loading a patch reset the engine's memory. | Yes: streams rewound, gravity, carried onset offsets and the continue snapshot cleared; sounding voices fade over 5 ms. | A loaded patch then sounds the same every time it is loaded (DT-1). | IF-4 |
| Q17 | The morph class of every engine key. | As the class column of section 5. | Continuous rows morph; stepped rows flip; quality, polyphony and seeds' visibility are Device or advanced. | IF-4 |
| Q19 | Does the engine keep a saturating output stage. | No: a linear output stage; drive belongs to the drive module and Shriek. | ADR-221 found chords clipping on 71 of 83 presets; SPEC-SCALPEL §10 said not to copy the clipper. The presets of 10.3 are heard for the grit. | OU-1 |
| Q20 | Volume, gain or level, and which taps. | `out.level` and the four source levels in dB, named by the B376 rule; meters at each source and the output, before any saturation. | B376's naming; B225's meter list. | IF-6, 5.2 |
| Q22 (the rest) | Do the sub and the noise oscillator share gravity and drift. | Gravity yes (it is the voice's pitch); drift and coupling no. | The sub's point is that it is not a swarm. | SN-1, SN-4 |
| Q24 | The engine's sleep rule and tail. | Silent when no voice sounds; a skipped-samples count on resuming (OU-4). | An instrument's tail is its voices; the count keeps free-running clocks exact. | OU-4 |
| Q27 (the rest) | Quality modes: 1× and 2× only, or a heavy-patch policy too. | 1× and 2× only; no automatic thinning. | The member count is the user's own CPU lever. | 5.2, IF-9 |
| Q30 (prototypes part) | Do the prototypes in `reference/` stay. | Keep them as protected history. | They cost nothing and record the design's origin. | 3.3 |
| Q31 | The 19 deferred rows and envelope decision 1. | **Keep:** `onsetLock` and `dissolve` ("the functionality needs to be present"); `inertia` (Q6); `onsetScatter` and `timingCorrection` (Q13). **Drift mode:** one mode, the walk (C.4) ("pick the best one and lock it in"). **Lock:** `basin` 35 c; `panCurve` 0.5. **Cut from 1.0:** `freqGlide` (Q6); `attackScatter`, `relScatter`, `voiceEnv` (the human could not hear them); `topo`, `mu`, `balance` (the human asked to test whether two-cluster is worth it with blades; out until it wins a test); `beatMult` and the tempo-grid law (post-1.0, the host tempo input reserved); `superMode` and `mono` (the stereo lab's; width 0 is mono); `bassMono` (a device output policy). **`vol`:** `out.level` in dB (Q20). **Envelope decision 1:** C.12 (time means time to finish; per-stage curves; decay and release default +1, the bank's shape; a velocity amount). | The human's own notes on each row (2026-10-02); fewer, clearer controls for 1.0. | 5.2, 5.3, SW-7–SW-12, EN-1 |
| X1 | Start phase: the merged row's execution, and how the bank's "settled" translates. | Fresh with Scatter, Settled, Continue (SW-10). Translate the bank's settled (today played as aligned) to fresh, Scatter 0, which is what the human tuned on; random to fresh, Scatter 1. Settled is genuinely settled for new patches. | The human merged the rows and asked to "workshop the execution"; the bank must stay recognisable. | SW-10, App. B |
| X2 | Members near the band edge: fade out or fold. | Fade out between 0.40 and 0.48 of the internal rate (SW-13); carriers and the sub are capped instead. | Folding and DC from members above Nyquist were measured on legal patches (B378 F2). | SW-13 |
| X3 | Corners (triangle points, mirrored centres, Crush slew corners): correct them in 1.0. | Correct triangle base and carrier corners with a corner table built like the step table; leave Crush slew corners. | Uncorrected triangle corners read −66 dB at C4 to −31 dB at C8; a table reads below −149 dB (research note). | ED-6 |
| X4 | Should notes share one noise stream. | No: per voice and member (BL-18, DT-4). | B469; notes interacting confounds every with/without comparison. | BL-18, SN-4 |
| X5 | The noise blade's large steps: sound or click. | Keep the steps as the sound; band-limit them like every edge. | B481 is unheard; a sample-and-hold source steps by design. | BL-18 |
| X7 | The frame after a crossing. | Treat a flip of ψ by exactly half a cycle as no change; turn back at the rate limit otherwise. | ADR-216 A1 left it to the human; a crossing is a symmetry, not a movement. | BL-10, C.13 |
| X11 | Detune's range (the merged `spread` row). | A stepped Range: ×1, ×12, ×24. | The human: "expand detune or add the toggle"; a toggle keeps the fine range fine. | 5.3, SW-3 |
| X12 | The default patch: blade 1 off. | Yes: an untouched patch is the swarm saw. | The design's own rule; a blade is a choice. | 5.4 |
| X13 | The filter slot: how many filters per voice and how sources route to them. | Settled by the filters' spec; the slot supports two filters, each source routed to either, both or neither, serial or parallel. | B303 needs per-voice filters; the mixer's taps already name filter placements (B225). | IF-5, 4.2 |
| X17 | Density compensation, and the bank's balance. | Keep the human's lock (U^−0.75) for new patches; translate each bank preset with a level offset of +5·log₁₀(U) dB, which restores its √U-normalised level exactly, so the bank's balance between presets is kept (ADR-221 item 2) and its headroom is unchanged from today's measurement. | *Corrected from round 1*, which said offsets would break headroom: they restore today's absolute level, which already passes OU-2 (a) (worst 0.815). | SW-19, App. B |

### 11.3 Can be ruled after the build (6)

| # | question | recommendation | reason | depends |
|---|---|---|---|---|
| Q23 | Shriek as a fixed post-stage per oscillator, or a rack slot. | A rack slot; the engine offers no insert. | Keeps the engine's boundary small (IF-7); nothing in the engine changes either way. | IF-7 |
| Q26 | Which width candidates are in 1.0, and the mono-fold guarantee. | In the engine: per-member pan only (SW-18), no polarity inversion; every other candidate is post-voice and the stereo lab's; mono fold reported (QS-10) until the lab sets a bound. | ADR-224 item 3 put true stereo after the voice. | SW-18, IF-8 |
| Q29 | Is an M1 or Intel min-spec measurement a precondition for freezing the CPU row. | Not for building; yes before the CPU row is frozen for release. | Every min-spec figure rests on the assumed ×1.5. | PF-2 |
| X6 | Does a sound-change ledger continue against the new engine's own goldens. | Yes: every change after the goldens freeze is recorded with its A/B and the human's word. | ADR-220 item 1 names the ledger as part of correctness. | 10.2 |
| X9 | A twin wider than half a cycle under the cut latch: give the twin its own latch. | Not in 1.0; the edge stays stated. | No bank preset uses it (B487's critic). | BL-16 |
| X15 | The noise oscillator's design (no lab yet, B452). | Build the minimum of SN-4 now; revisit after its lab. | ADR-190 A6 put a plain noise oscillator in 1.0; its design lab has not started. | SN-4, 5.6 |

### 11.4 Already ruled, dropped (5)

| # | ruled by | the ruling, applied |
|---|---|---|
| Q4 | ADR-224 item 1 (one allocator for the instrument) | One pool of 1 to 16 voices, default 8 (VO-5); "two engines of 8" no longer exists. |
| Q5 | ADR-190 A2 (undecided at the 2026-10-07 freeze goes after 1.0) | Chords as copies of the oscillator are post-1.0. |
| Q27, per-preset part | ADR-191; B445 ("presets that need os 2 can save it") | Quality is saved per preset (5.2). |
| Q22, allocation and ENV 1 | ADR-224 item 1 (the sub and the noise oscillator live inside the one voice, under its one ENV 1) | They share the voice's allocation and ENV 1 (SN-1, SN-4). |
| X14 | B370 (the human: ENV 1 "a global envelope that applies to all pre-FX sound-generating devices") | No per-source opt-out from ENV 1. |

---

## Appendix A. Traceability: the audit's 75 decisions

Every row of the B493 audit's section 2 lands here. KEEP and RULING rows become requirements; DROP rows are artefacts of parity with the JavaScript and are gone; CULLED rows the human already cut.

| row | class | lands in |
|---|---|---|
| S1 coupling law | KEEP | SW-4; C.1; 5.3 `coupling` |
| S2 time constants in seconds | KEEP | SW-14 |
| S3 onset lock and dissolve | RULING | SW-9; C.1; Q31 |
| S4 detune laws | KEEP | SW-3; C.3 |
| S5 stretch law | CULLED | 5.7 |
| S6 tempo grid | RULING | Q31 (post-1.0); IF-2 tempo input reserved |
| S7 placement | KEEP | SW-2; C.2 |
| S8 spread merged into detune; anchor | KEEP | SW-3; X11 |
| S9 pitch drift | RULING | SW-7; C.4; Q31 |
| S10 `motionCenter` | CULLED | 5.7 |
| S11 inertia | RULING | SW-8; C.5; Q6 |
| S12 `freqGlide` | RULING | Q6 (cut) |
| S13 start phase | RULING | SW-10; X1 |
| S14 pivot | KEEP | SW-6 |
| S15 gravity | KEEP | SW-11; C.6 |
| S16 the ensemble | RULING | SW-12; C.7; Q13; Q31 |
| S17 members above Nyquist | RULING | SW-13; X2 |
| B1 the SCALPEL v1 sound set | KEEP | BL-1 to BL-12; C.9, C.11, C.14; 5.4 |
| B2 spreads, laws, rules | KEEP | BL-13, BL-15; C.10 |
| B3 bipolar spreads mirror | KEEP | BL-14 |
| B4 the frame never jumps (D6) | KEEP | BL-10; C.13; X7 |
| B5 no control-rate step; the quantized cut (D8, D9) | KEEP | BL-16, BL-17, SW-16 |
| B6 smoothing, detune and coupling included | KEEP | PA-1, SW-17 |
| B7 stepped keys while sounding | RULING | PA-5, PA-6; Q14 |
| B8 blade envelopes | CULLED | 5.7; Appendix B (ENV 2, ENV 3 routes) |
| B9 cross-mod, feedback, the loop filter (D3) | KEEP | BL-12 |
| B10 D1, D2 as switches | CULLED | their aim is ED-1 |
| B11 edge correction, the minimum-phase table | KEEP | ED-1 to ED-5; QS-1, QS-2 |
| B12 corners | RULING | ED-6; X3 |
| B13 DC | KEEP | BL-19; QS-6 |
| B14 edge heights by two evaluations | DROP | parity artefact; ED-1 states closed-form heights |
| B15 `sin` hash, shared-state overrides, engine-wide morph fields | DROP | parity artefacts (SPEC-SCALPEL §8, §10) |
| B16 noise from the shared stream | RULING | BL-18; X4 |
| B17 noise steps | RULING | BL-18; X5 |
| B18 kernels fitted to the bank | DROP | as a shape; PF-1, PF-4 keep the aim |
| V1 the voice law | KEEP | VO-2 |
| V2 the cap and the cull | KEEP | VO-4; Q3 |
| V3 a taken voice fades (D7) | KEEP | VO-3 |
| V4 mono, legato, glide | KEEP | VO-6; IF-2 (glide in the pitch system, Q6) |
| V5 the first sample is right | KEEP | SW-15 |
| V6 the culled fade rounded through a float | DROP | parity artefact |
| V7 block-size and sample-rate independence | KEEP | DT-2, DT-3, PA-3 |
| V8 the pool | RULING | VO-5 (Q4, ruled by ADR-224 item 1) |
| V9 pan | KEEP | SW-18; C.8 |
| V10 loudness by envelope, velocity, member count | KEEP | VO-8, SW-19; X17 |
| V11 the amplitude envelope | RULING | EN-1 to EN-4; C.12; Q31 envelope decision 1 |
| V12 the default output 12 dB lower | KEEP | OU-2; QS-5 |
| V13 the output `tanh` | RULING | OU-1; Q19 |
| V14 a saved state keeps its level | KEEP | OU-3 |
| N1 determinism | KEEP | DT-1, DT-5 |
| N2 named seeded streams | KEEP | DT-4 |
| N3 JavaScript number semantics | DROP | BD-7 |
| N4 JavaScript `parseFloat` | DROP | BD-7; BL-15 keeps the ratio list |
| N5 `Array.slice` pool semantics | DROP | BD-7 |
| N6 draws thrown away to keep an order | DROP | BD-7; DT-4 |
| N7 the literal 0.08 at 44.1 kHz | DROP | SW-14 |
| N8 two values of 2π | DROP | BD-2, BD-7 |
| N9 doubles everywhere, no fused multiply-add | DROP | BD-7 (ADR-224 item 6) |
| N10 the shipped build is the tested build | KEEP | BD-9, BD-4 |
| N11 parity with the JavaScript | DROP | ADR-223 item 3 |
| N12 real-time safety | KEEP | RT-1, RT-2 |
| N13 flush-to-zero | KEEP | RT-4 |
| N14 host values checked on entry | KEEP | PA-7 |
| N15 non-finite output latched | KEEP | RT-3 |
| N16 the CPU target | KEEP | PF-1 to PF-3 |
| N17 every audible change heard A/B | KEEP | 10.3; X6 |
| N18 each fix behind a switch | DROP | BD-3 |
| N19 divergences mirrored into the lab's JavaScript | DROP | the labs run the engine (IF-10) |
| N20 a divergence ledger | RULING | 10.2; X6 |
| N21 visualiser readouts | KEEP | IF-10 |
| K1 the bank sounds good | KEEP | 10.3; Appendix B |
| K2 oversampling saved per preset | KEEP | 5.2 `quality`; OU-3 |
| P1 pitch bend | RULING | IF-2; Q7 |
| P2 gravity rides the played pitch | KEEP | SW-11, IF-2 |
| P3 pitch as an input | KEEP | IF-2 |

Count: 75 rows. KEEP 40, RULING 17, DROP 14, CULLED 4.

---

## Appendix B. The bank, translated

### B.1 The bank's presets

The bank's presets set SCALPEL-era keys. Each translates to a new-engine patch on osc 1 (osc 2, the sub and the noise oscillator off) by this table; any key a preset omits takes the bank's base value (`reference/scalpel/data/parameters.json` defaults). Because the laws are copied (Appendix C), "as is" means the same number plays the same law. The translator is generated data (3.1), and the A/B (10.3) judges its result.

**Enumerations map by meaning, not by number** (the bank's stored ids are not in display order):

| bank key | stored id → new option |
|---|---|
| `hot`, `hot2`, `base` | 0 sine, 1 triangle, 2 saw, 3 square, 4 reverse saw, 6 sine→saw |
| `mshape`, `mshape2` | 0 sine, 1 triangle, 2 saw, 3 square, 4 reverse saw, 5 smooth noise, 7 S&H noise |
| `law` (spread law) | 0 gradient, 1 random, 2 alternate, 3 swarm, 4 drift |
| `phaseMode` | 0 random, 1 aligned, 2 settled (then by X1, below) |
| `rotSync`; `rot2Follow` | 1 restart per note, 0 free-running; 1 with blade 1, 0 its own |
| `mode`, `mode2` | 0 Sync, 1 FM reset, 2 FM free, 3 Noise, 4 Fold, 5 Ring, 6 Crush |
| `mirror`; `mirror2` | 0 off, 1 reflect, 2 twin −, 3 twin +; −1 as blade 1 |
| `lock`; `lock2` | 0 × pitch per cycle, 1 per blade, 2 Hz; −1 as blade 1 |
| `frame`; `frame2` | 0 member, 1 swarm; −1 as blade 1 |
| `kRule`, `kRule2` | 0 even, 1 harmonic, 2 undertone, 3 octaves, 4 major, 5 minor, 6 fifths, 7 golden, 8 primes, 9 custom |
| `polyMode`; `panOrder`; `aa`; `kq`; `fmType`; `mUnit`; `b2fm`, `b2sp`; `b2order` | 0 poly, 1 mono, 2 legato; 0 balanced, 1 spread fan; 1 → 1, 0 → 0; 0 free, 1 whole; 0 phase, 1 pitch; 0 × pitch, 1 Hz; 0 blade 1's, 1 its own; 0 blade 2 over 1, 1 blade 1 over 2 |

| bank key | new key | rule |
|---|---|---|
| `N` | `osc1.unison` | as is |
| `detune` (cents) | `osc1.detune` | cents ÷ 100, with `detuneLaw` cents, `detuneRange` ×1, `distribution` even (C.3: the outermost members at ±the stored cents) |
| `K` | `osc1.coupling` | as is (the bank was tuned under this law, C.1) |
| `phaseMode` | `osc1.startPhase`, `osc1.scatter` | random → fresh, Scatter 1; aligned and settled → fresh, Scatter 0 (X1: today's engine plays settled as aligned, and the bank was tuned on that) |
| `width`, `panOrder` | `osc1.width`, `osc1.panImage` | as is |
| `aa` | `osc1.bandLimit` | 1 → 1, 0 → 0 |
| `base`, `xm`, `fb`, `law`, `driftRate`, `kq`, `kCustom`, `b2order`, `b2mix`, `colK`, `colB` | `baseWave`, `crossMod`, `feedback`, `spreadLaw`, `spreadDrift`, `quantize`, `ratios`, `stacking`, `upperHears`, `collisionPitch`, `collisionBite` | as is |
| blade 1 keys (`mode`, `hot`, `morph`, `lock`, `k`, `kHz`, `c`, `w`, `hard`, `depth`, `mirror`, `frame`, `rotRate`, `rotSync`, `fmType`, `mshape`, `I`, `mUnit`, `m`, `mHz`, `bspread`, `kspread`, `kRule`, `kRuleAmt`, `wspread`, `dspread`, `mspread`, `ispread`, `rotSpread`) | `osc1.blade1.…` (`mode`, `wave`, `shape`, `cutIn`, `cut`, `cutHz`, `position`, `width`, `edges`, `depth`, `mirror`, `frame`, `rotate`, `rotation`, `fmType`, `modShape`, `fmDepth`, `modIn`, `modRate`, `modRateHz`, `positionSpread`, `cutSpread`, `cutRule`, `ruleDepth`, `widthSpread`, `depthSpread`, `shapeSpread`, `fmIndexSpread`, `rotateSpread`) | as is; `blade1.on` on (every bank preset uses blade 1) |
| blade 2 keys (the same with a `2` suffix, `b2on`, `rot2Follow`, `lock2`, `mirror2`, `frame2`, `b2fm`, `b2sp`) | `osc1.blade2.…` | as is |
| `A`, `D`, `S`, `R` (ms, level) | `env1.attack`, `decay`, `sustain`, `release` | ms ÷ 1000; hold 0; curves 0, +1, +1 (C.12) |
| `polyMode` | `voice.mode` | as is |
| `glide` (ms, time to 95 %), `glideAlways` | the pitch system's note lane (the lag law: exponential in log-frequency, 95 % in the stored time; glide from overlapping notes or always) | the A/B harness supplies it (10.3; Q6) |
| `benvA`, `benvD`, `benvVel`, `benvK`, `benvW` | ENV 2: attack `benvA` ms (curve 0), hold 0, decay `benvD` ms to sustain 0 (curve +1), release equal to decay; its level scaled by (1 − `benvVel` + `benvVel`·velocity); routed per voice to `blade1.cut` (or `cutHz` when the cut is in Hz) by 4·`benvK` octaves at full level, and to `blade1.width` by 3·`benvW` octaves at full level (each converted to its parameter's normalised law); and to blade 2 the same way unless `b2env` is 1 | the human's cull: blades take envelopes from the matrix, "preset translation writes default routes" (audit round 2) |
| `benvA2` … `benvW2` with `b2env` 1 | ENV 3, the same law, routed to `blade2.cut` and `blade2.width` | as above |
| (level) | `out.level` | the bank sets no level of its own, so each preset plays at the ADR-221 default: 0 dB + 5·log₁₀(N) dB, the offset that restores its level under the bank's 1/√N sum (X17) |
| `dcMode`, `cScale`, `os` | — | locked (per cycle), never live, or the device's (`quality` Standard for the bank) |

Until the device's modulation system exists, the A/B harness supplies ENV 2 and ENV 3 itself through the engine's per-voice modulation (PA-2), which also exercises that mechanism on the bank.

### B.2 A saved state of today's engine (OU-3)

A state from today's engine carries the bank's keys (B.1) and these of its own; each translates as stated, and anything dropped is reported to the user by name when the state opens. (States saved by the B458 preview are exempt from migration by ADR-215 item 2; the rule is stated so a saved project is not left to guesswork.)

| today's key | new key | rule |
|---|---|---|
| `gain` | `out.level` | 20·log₁₀(g/0.0875) + 5·log₁₀(N) dB |
| `os` | `quality` | 1 → Standard, 2 → High; absent → High (K2) |
| `poly` | `voice.polyphony` | as is, within 1 to 16 |
| `dist`, `seed`, `harmReach`, `anchor`, `pivotMode`, `driftDepth`, `inertia`, `onsetScatter` | `distribution`, `seed`, `harmonicReach`, `anchor`, `pivot`, `driftDepth`, `inertia`, `onsetScatter` | as is (`dist` 0 even, 1 JP-8000, 2 Gaussian, 3 Cauchy, 4 golden) |
| `h.law` | `detuneLaw` | 0 cents, 1 Hz, 2 ERB, 4 harmonic; 3 (tempo grid) and 5 (stretch), culled, → cents, reported |
| `spread` s | `detuneRange`, `detune` | the range nearest s of ×1, ×12, ×24 (by log distance); `detune` × s/range, clamped to 1 (a clamp is reported) |
| `onset`, `dissolve` | `onsetLock`, `dissolve` | as is |
| `h.driftRate` | `driftRate` | as is (the same knob law, C.4) |
| `onsetAlpha` | `timingCorrection` | as is |
| `grav` | `voice.gravity` | as is |
| `keepPhase` 1 | `startPhase` continue | overrides `phaseMode` |
| `spreadRamp` | `motionGrain` | 1 → 0 (ramped), 0 → 1 (stepped) |
| `driftMode` (non-walk), `freqGlide`, `basin` (≠ 35), `attackScatter`, `relScatter`, `voiceEnv`, `motionCenter`, `inertiaCurve`, `bpm`, `beatMult`, `stretchB`, `cScale`, `dcMode`, the switches `aaCarrier`, `aaXin`, `aaLoop`, `frameHold`, `stealFade`, `cutLatch`, `edgeTable` | — | dropped; the switches' behaviours are requirements of this spec; a non-default dropped value is reported |

---

## Appendix C. Laws (normative)

The math is copied; the code is not. Each law is stated with its constants and units; the builder may compute it any way (single precision, tables, SIMD, any loop order) that keeps every anchor and every Layer-0 check of the law within its tolerance. **A law may change only where every anchor that tests it still passes, and the reason is stated here when it does.** Where this appendix departs from today's engine, it says so.

### C.0 Conventions

- θᵢ is member i's **swarm phase** in cycles; start phases (SW-10), coupling (C.1) and the frame (C.13) read it. Blades and base waves read the **blade phase** φᵢ = frac(θᵢ + ½). So a member started at θ = 0 begins its base saw at −1, and a blade's `position` is measured in φ. (The two origins come from the two prototypes; here they are one phase, offset by a constant.)
- frac(x) = x − ⌊x⌋. Δ is the control interval in seconds (PA-3). f_s is the internal rate, Nyq = f_s/2.
- Each member's phase advances per internal sample by gᵢ(t)/f_s, where gᵢ is its frequency from C.1 and C.5, updated each control interval and, between updates, scaled per sample by f₀(t)/f₀(t_k), f₀ the oscillator's pitch at the sample and t_k the last update, so a glide or bend is smooth inside an interval. A member's frequency is never negative.

### C.1 Coupling

Source: SPEC.md §3 (layer 2), §5.1–5.2, §6.3; ADR-007; ADR-056; the law as today's engine runs it.

- Taper: **km = 4·K·|K|**, K = `coupling`. The taper lives here only; the manifest's law for `coupling` is linear.
- Onset burst: **Kenv(t) = 8·o·|o|·e^(−t / max(0.01 s, `dissolve`))**, o = `onsetLock`, t the time since note-on. Kenv is in the same units as km.
- Spread: **σ = max(0.08 Hz, √((1/U)·Σᵢ (fᵢⁿᵃᵗ − f̄)²))**, fᵢⁿᵃᵗ the natural frequencies of C.3 (after drift, before coupling), f̄ their mean.
- Targets, in Hz: **S\* = (max(0, km) + max(0, Kenv))·σ**; **P\* = 3·(max(0, −km) + max(0, −Kenv))·σ**. The applied S and P follow S\* and P\* through a one-pole of time constant 0.0043512 s, starting from 0 at note-on.
- Mean field: R·e^{iψ} = (1/U)·Σⱼ e^{i·2π·θⱼ}.
- The pull on member i, in Hz, recomputed each control interval:
  - mean field: **cᵢ = S·R·sin(ψ − 2π·θᵢ − α)**;
  - root pivot: **cᵢ = S·sin(2π·(θ_r − θᵢ) − α)**, r the member whose natural frequency is nearest f₀;
  - and when P > 0.001 Hz: **cᵢ += P·sin(2π·(θ_c + (i − c)/U − θᵢ))**, c the centre member of C.2 (mean field) or r (root pivot).
- α = `phaseLag` in radians. *Departure:* today's engine has no α term (phase lag was never composed into it); the term is copied from SPEC.md §5.1, and SW-5 is its anchor. Its use in the root-pivot form is this spec's extension of the same law.
- The member's coupled frequency is gᵢ = fᵢⁿᵃᵗ + cᵢ before inertia (C.5).

### C.2 Placement

Source: today's engine (from SPEC.md §3 and the legacy core). For i = 0 … U − 1 and uᵢ = i/(U − 1) (U = 1: x₀ = 0):

| distribution | xᵢ |
|---|---|
| even | 2uᵢ − 1 |
| JP-8000 | with J = (−1, −0.5715, −0.1774, 0, 0.181, 0.565, 0.9766), s = 6uᵢ, a = ⌊s⌋, b = min(a + 1, 6): J[a] + (J[b] − J[a])·(s − a) |
| Gaussian | clip(√(−2·ln max(r₁, 10⁻⁹))·cos(2π·r₂) / 2.5, −1, 1), two seeded uniforms in [0, 1) per member, in member order |
| Cauchy | clip(tan(π·(r − ½)) / 4, −1, 1), one seeded uniform per member |
| golden | 2·frac((i + 1)·0.6180339887498949) − 1 (no draws) |

The placement depends only on (U, distribution, seed). The centre member c = argmin |xᵢ| (the lowest index on ties); x_min = min xᵢ.

### C.3 Detune laws

With d = `detune` × range (range 1, 12 or 24), x'ᵢ = xᵢ − `anchor`·x_min, and f₀ the oscillator's pitch (the voice's sounding pitch × 2^(`pitch`/12 + `fine`/1200)):

| law | fᵢ |
|---|---|
| cents | f₀·2^(x'ᵢ·d·100/1200) |
| Hz | f₀ + x'ᵢ·d·20 |
| ERB | f₀ + x'ᵢ·d·0.35·ERB(f₀), ERB(f) = 24.7·(4.37·f/1000 + 1) |
| harmonic | f₀·(1 + d·`harmonicReach`·i) (by member index: placement and anchor do not apply) |

Then drift (C.4): fᵢ ×= 2^(Dᵢ·`driftDepth`/1200); and **fᵢⁿᵃᵗ = max(1 Hz, fᵢ)**.

### C.4 Drift

Source: today's engine (the walk mode). *Correction to the brief's transcription:* the formula Tᵢ ← Tᵢ − α(Tᵢ − mean) + N(0, σ) is the ensemble's timing correction (C.7), not the drift; the drift is this walk.

Per member, each control interval: **Dᵢ ← clip(Dᵢ + (r − ½)·2·√(ρ·Δ) − 0.4·Dᵢ·Δ, −1, 1)**, with ρ = 0.2 + 8·`driftRate` (the knob, 0 to 1), r a seeded uniform in [0, 1), Dᵢ = 0 at note-on. It runs only while `driftDepth` > 0.

### C.5 Inertia

Source: SPEC.md §5.3; today's engine. With w = knob^2.5 (the locked taper), ω₀ = 2π·(8·(1 − w) + 0.6) rad/s, and yᵢ member i's offset from f₀ (yᵢ\* = gᵢ − f₀ its target), each control interval:

**mᵢ ← (mᵢ + (yᵢ\* − yᵢ)·ω₀²·Δ)·e^(−0.9·ω₀·Δ);  yᵢ ← yᵢ + mᵢ·Δ**  (damping ratio 0.45). The member's frequency is f₀ + yᵢ.

At note-on yᵢ = yᵢ\* and mᵢ = 0. At knob 0 with no swing in flight yᵢ = yᵢ\*; lowered to 0 mid-swing, the swing continues at the knob-0 ω₀ until |yᵢ − yᵢ\*| < 0.01 Hz, then follows exactly. *Departure:* today's engine swings the whole member frequency, so a new pitch (a legato note, a bend) swings too; here only the offset swings (SW-8).

### C.6 Gravity

Source: ADR-008; ADR-086 A1; today's engine.

- Every grid step T_g = 256/44100 s (round(f_host·T_g) samples), if g = `voice.gravity` ≥ 0.005:
  - take the held voices (key down), each with its settled pitch p_v; sort them by p_v ascending (stable);
  - for each pair (a, b), a < b, in that order (a outer): r = p_b/p_a; r_f = r/2^⌊log₂ r⌋; ρ_k the nearest of the 13 ratios **{1, 16/15, 9/8, 6/5, 5/4, 4/3, 7/5, 3/2, 8/5, 5/3, 16/9, 15/8, 2}** by |1200·log₂(r_f/ρ)|; e = 1200·log₂(r_f/ρ_k) cents; if |e| ≤ 35 c, with m = e·3g·T_g/2: p_b ×= 2^(−m/1200), p_a ×= 2^(m/1200).
- p_v is the played pitch when the voice is struck or first enters gravity; thereafter it rides the played pitch: when the played pitch changes by a factor q, p_v ×= q. A released voice keeps p_v. The voice's sounding pitch is p_v once gravity has moved it, else the played pitch.
- "A new chord": each newly struck note starts at its played pitch; notes already held keep the pitches they settled to.

### C.7 The ensemble (onset scatter)

Source: ADR-077; today's engine. Per oscillator a persistent offset vector Tᵢ (seconds, i = 0 … U − 1), zero at reset, at a seed change and at a load.

- At each fresh note-on with s = `onsetScatter` > 0: μ = mean(T); **Tᵢ ← Tᵢ − α·(Tᵢ − μ) + 0.001·s·zᵢ**, α = `timingCorrection`, zᵢ standard normal from the ensemble stream (in member order); then wᵢ = (Tᵢ − mean(T)) − minⱼ(Tⱼ − mean(T)).
- Member i waits wᵢ seconds, silent, its phase held; then it enters with a gain eᵢ(t) = 1 − e^(−t/τ), τ = max(0.002 s, `env1.attack`), multiplied onto the voice's envelope (every member enters by this fade; the earliest at once). *Departure from round 1 of this spec:* round 1 said the waits "close up by sustain"; that was a lab candidate (B370), not the law, and is withdrawn.

### C.8 Pan and density

Source: SPEC-SCALPEL §6, §9; the legacy core's pitch fan (ADR-070, ADR-073); ADR-064 and ADR-086's amendment.

- Seat sᵢ ∈ [−1, 1]:
  - **balanced:** U ≤ 9 from the table (N = 1: 0; N = 2: −1, 1; N = 3: −1, 1, 0; N = 4: −0.3333, 1, −1, 0.3333; N = 5: −0.5, 1, 0, −1, 0.5; N = 6: −0.2, 0.6, −1, 1, −0.6, 0.2; N = 7: −0.3333, 1, −1, 0.6667, −0.6667, 0.3333, 0; N = 8: −0.1429, 0.4286, −0.7143, 1, −1, 0.7143, −0.4286, 0.1429; N = 9: −0.25, 1, −0.75, −0.5, 0, 0.5, 0.75, −1, 0.25); for U = 10 to 16 a permutation of U evenly spaced seats from −1 to 1, found by search to meet SW-18's brightness bound and recorded in the manifest;
  - **spread fan:** sᵢ = 2·(i/(U − 1) − ½);
  - **pitch fan:** rank the members by xᵢ ascending (stable; by index under the harmonic law); rank k sits at distance d_k = k/(U − 1) for odd U, (⌊k/2⌋ + ½)/(U/2) for even U (U = 1: 0); `panInvert` makes d = 1 − d; the seat is (−1 for even k, +1 for odd k)·d_k.
- Pan motion: each member index has a phase π_i advancing at (0.08 + 0.021·i) Hz on the gravity grid (not reset at note-on); the seat becomes clip(sᵢ + `panMotion`·sin(2π·π_i), −1, 1).
- Gains: aᵢ = (sᵢ·`width` + 1)·π/4; **Lᵢ = cos aᵢ, Rᵢ = sin aᵢ**. The swarm's output is U^−0.75·Σᵢ yᵢ·(Lᵢ, Rᵢ), yᵢ member i's output (C.9).

### C.9 The blade (SPEC-SCALPEL §4.1 to §4.5, verbatim, then what it leaves open)

> **4.1 Geometry**
>
> For a blade with centre `c`, width `w` (bypass below 0.004), cut rate `k`:
>
> - `st = frac(c − w/2)`, `e = frac(φ − st)`; inside when `e < w`.
> - Carrier rate `kk = k/w` in *per blade* units, else `kk = k`. In *Hz* units the per-member ratio is `k = k_Hz/fᵢ`.
> - Reflect mirror: `eʳ = e > w/2 ? w − e : e`, else `eʳ = e`. Hot phase `hp = kk·eʳ`.
> - Edge gate (all modes except crush): `t = hard·w/2`; `g = ½ − ½cos(πe/t)` for `e < t`, `½ − ½cos(π(w−e)/t)` for `e > w−t`, else 1.
> - Centre: `c = position + rotation clock + member rotation + spread offset + (frame = swarm ? leadᵢ : 0)`.
> - Every carrier rate is capped at `0.45·fs·OS/fᵢ`.
>
> **4.2 Modes**
>
> | Mode | hot(φ) inside the blade |
> |---|---|
> | Sync | `wave(blade wave, frac(hp + xin))` — hard sync at blade entry |
> | FM reset | `wave(blade wave, frac(xin + cp))`, modulator phase `m·eʳ` restarts each blade |
> | FM free | same, modulator phase free-running (`modX += mEff·dφ`) |
> | Noise | sample-and-hold uniform noise, new value at each integer `hp` |
> | Fold | `sin(π/2·(1 + (k−1)/4)·base)` |
> | Ring | `base · wave(blade wave, frac(hp + xin))` |
> | Crush | box-averaged hold levels with slew (§4.4) |
>
> **FM phase:** `cp = hp + (I/2π)·mod(shape, x)`. **FM pitch:** `cp = hp + acc`, `acc += ((1 + I/10)^{mod} − 1)·kk·dφ` (exponential deviation, symmetric in cents; depth shown as `±1200·log₂(1 + I/10)` cents); reset mode zeroes `acc` at blade entry. **Mod rate** is `m × f₀` or a fixed Hz (`mEff = mHz/fᵢ`).
>
> **4.3 Waves and modulator shapes**
>
> `sine = sin 2πx`; `tri` (sine-aligned); `saw = 2·frac(x+½) − 1`; `rev saw = −saw`; `square`; **sine→saw** `= −atan2(r·sin θ, 1 − r·cos θ)/asin r`, `θ = 2π(x+½)` — the closed form of Σ rⁿ sin(nθ)/n. `r = 1 − (1 − r_max)^shape`, `r_max = min(0.995, 0.01^{f_h/Nyq_OS})` from the blade's own carrier frequency `f_h`, which makes the morph self-band-limiting. Modulator shapes add *smooth noise* (smoothstep between hashed values at integer x) and *S&H noise* (hash of ⌊x⌋).
>
> Periodic antiderivatives (used by crush and DC): sine `(1 − cos 2πx)/2π`; tri piecewise `2f²`, `2f − 2f² − ¼`, `2f² − 4f + 2`; saw `y² − y` with `y = frac(x+½)`; rev saw `y − y²`; square `f < ½ ? f : 1 − f`.
>
> **4.4 Crush**
>
> Hold level `j` is the **average** of the base wave over `[st + j/kk, st + (j+1)/kk]`, via the antiderivatives — not a point sample. Point sampling flips a whole step by ±2 whenever a grid point slides across a saw discontinuity as k changes; averaging is continuous in k. With *step slew* (the Edges control in crush mode), each step glides in from the previous level over its first `hard` fraction, and the output ramps onto `base(st + w)` over the last `min(hard, kk·w)` hold units, so the blade exit is seamless. Verified: largest jump < 0.002 while sweeping k 2→60 for every base wave.
>
> **4.5 Mirror**
>
> *Reflect* makes the blade a palindrome. *Twin −* adds `−D(φ − ½)`, *twin +* adds `+D(φ − ½)`. With a half-wave-symmetric base (sine, tri, square), twin − removes even harmonics entirely (~120 dB down) and is DC-free by construction.

What §4 leaves open, settled from the prototype's own law (the code of `reference/scalpel/prototype/razor-core.js` is not copied; its math is):

1. **The blend.** Inside the window the output is **x + g·depth·(hot − x)**, x the blade's input (the base wave, or the serial input of C.11); outside, x. A blade's contribution is D = output − x. The gate's taper lies **inside** the window, from its edges inward. Crush has gate 1 throughout and uses `edges` as its slew.
2. **Waves exactly.** tri(x) = 4x for x < ¼, 2 − 4x for x < ¾, else 4x − 4 (sine-aligned: 0 at x = 0, rising); square(x) = 1 for x < ½, else −1; sine→saw with r < 10⁻⁴ is the sine.
3. **The carrier restarts.** In Sync, FM reset, FM free and Ring the carrier phase restarts at 0 at every window entry (hp = 0 at e = 0). The modulator of FM reset is at phase m_eff·eʳ, so it restarts too; that of FM free advances by m_eff·Δφ per sample and never restarts; pitch-FM's accumulator `acc` is zeroed at entry in FM reset only. With collision (C.11) the carrier phase also adds `cacc`, and with cross-mod or feedback `xin` (C.14): cp = hp + xin + cacc (+ the FM term).
4. **Noise.** The held value is uniform in [−1, 1); a new value is drawn whenever ⌊hp⌋ changes and at each window entry, from the member's own stream (BL-18). *Departure:* the prototype draws from one shared stream (B469).
5. **Modulator noise.** Smooth noise is a smoothstep (3f² − 2f³) between per-integer values uniform in [−1, 1); S&H noise is the per-integer value; both from the member's own stream, keyed by the integer, not a `sin` hash (SPEC-SCALPEL §10).
6. **Fold.** hot = sin(π/2·(1 + (k − 1)/4)·b·x), with k the member's effective cut rate after spreads (never divided per blade), x the input, and b = 1 + 2·`collisionBite`·overlap when it collides, else 1.
7. **Crush when k is not whole.** With the hold window w_e = w (w/2 under reflect) the window holds ⌊kk·w_e⌋ whole steps and one last partial step; every step's level is the average over its full interval [st + j/kk, st + (j + 1)/kk], the partial one included. Before the first step (hp < 0) the level is base(st). The first step's slew starts from base(st).
8. **The per-member effective values** (spreads from C.10, p their positions): cut kᵢ = min((Hz ? max(0.05, `cutHz`/fᵢ + kAdd) : max(0.25, `cut` + kAdd))·kMul, 0.9·Nyq/fᵢ), the cap of BL-4; width wᵢ = min(1, `width`·wMul); depth dᵢ = clip(`depth` + dAdd, 0, 1); FM index Iᵢ = `fmDepth`·iMul; shape mᵢ = clip(`shape` + mAdd, 0, 1); the sine→saw r uses f_h = (per blade ? kᵢ/max(wᵢ, 0.001) : kᵢ)·fᵢ and Nyq of the internal rate.
9. **Twins.** A twin's contribution is D evaluated at φ − ½ with the same carrier and modulator state as the main window at that sample; twin − subtracts it, twin + adds it.

### C.10 Spreads (SPEC-SCALPEL §5, verbatim, then what it leaves open)

> **Laws** give each member a position `pn ∈ [−½, ½]` per spread parameter: *gradient* (`i/(N−1) − ½`), *random* (new draws on every note-on; a retriggered sounding note glides to them over 20 ms), *drift* (Ornstein–Uhlenbeck per member and parameter: `θ = 2π·rate`, `σ = 0.289·√(2θ·dt)`, clamped ±0.75), *alternate* (±½), *swarm* (`leadᵢ`). A solo member always gets `pn = 0`.
>
> **Offsets** (blade 1; blade 2's own set uses independent random slots):
>
> | Spread | Effect |
> |---|---|
> | Position | `c += bspread·pn` |
> | Cut rate (even) | `k += kspread·pn` (quantize: whole-harmonic offsets) |
> | Cut rule | `k ×= r(x)^depth`, `x = (pn + ½)(N−1)`, `r` log-interpolated between rule slots or snapped |
> | Width | `w ×= 2^{4·wspread·pn}` |
> | Depth | `depth += 2·dspread·pn` |
> | Shape | `shape += 2·mspread·pn` |
> | FM index | `I ×= 2^{4·ispread·pn}` |
> | Rotate | `rotation rate += 2·rotSpread·pn` Hz |
>
> **Cut rules** (member 0 is the root at k): harmonic `j+1`; undertone `1/(j+1)`; octaves `2^j`; major `{1, 5/4, 3/2}` by octave; minor `{1, 6/5, 3/2}`; fifths `1.5^j`; golden `φ^j`; primes `pⱼ/2`; custom list (fractions or decimals) repeating by octave.

What §5 leaves open:

1. **Independence.** Every (member, spread kind, blade) has its own position draw; the prototype's slot numbering is not kept (§10), only the independence. Alternate: pᵢ = −½ for even i, +½ for odd i. Swarm: pᵢ = leadᵢ (C.13).
2. **Random.** Targets are uniform in [−½, ½) drawn at each note-on; a fresh voice takes them at once; a sounding voice re-struck glides to them through a one-pole of time constant 0.02 s.
3. **Drift, exactly in time.** Each control interval Δ: **p ← clip(p·e^(−θΔ) + 0.289·√(1 − e^(−2θΔ))·z, −0.75, 0.75)**, θ = 2π·`spreadDrift`, z standard normal. *Change, with its reason:* SPEC-SCALPEL's σ = 0.289·√(2θ·dt) is this law's first-order form at a fixed 32-sample dt; the exact form gives the same stationary spread (0.289) and correlation time (1/θ) at any control interval, which PA-3's interval needs. The anchor is the walk's measured stationary spread and correlation time.
4. **Cut spread under Quantize.** The offset is round(s)·p, then rounded half away from zero: kAdd = sign(round(s)·p)·round(|round(s)·p|); free: kAdd = s·p.
5. **Cut rules exactly.** List Lⱼ (j = 0 … U − 1): harmonic j + 1; undertone 1/(j + 1); octaves 2^j; major Set[j mod 3]·2^⌊j/3⌋ with Set = (1, 5/4, 3/2); minor the same with (1, 6/5, 3/2); fifths 1.5^j; golden 1.6180339887^j; primes Pⱼ/2 with P = (2, 3, 5, 7, 11, 13, 17, 19, 23, and, an extension because the prototype stops at nine members, the following primes 29, 31, 37, 41, 43, 47, 53 for U up to 16); custom Cⱼ mod n·2^⌊j/n⌋ over the n parsed ratios. Then x = clip((p + ½)·(U − 1), 0, U − 1); Quantize: r = L[round(x)]; free: r = exp((1 − f)·ln L[a] + f·ln L[b]), a = ⌊x⌋, b = min(U − 1, a + 1), f = x − a; kMul = r^`ruleDepth`. A rule other than even replaces the even cut spread (kAdd = 0). With U = 1 kMul = 1.
6. **Rotate spread.** The rotation-rate offset 2·s·p keeps its sign (BL-14): a negative spread turns the members the other way.

### C.11 Interplay (SPEC-SCALPEL §13.2 to §13.3, verbatim)

> **13.2 Serial composition (normative)**
>
> Let `D_A(φ)` be the lower blade's delta computed from the base (as in v1: `voice − base`), and `s_A, s_B ∈ {−1, 0, +1}` the twin signs (twin −, off/reflect, twin +). Evaluate the lower blade at φ and φ − ½:
>
>   `L(φ) = D_A(φ) + s_A·D_A(φ−½)`  `L(φ−½) = D_A(φ−½) + s_A·D_A(φ)`
>
> The second evaluation is needed whenever *either* blade has a twin (the upper twin must hear the lower blade at φ − ½); it uses the lower blade's twin state.
>
> The upper blade transforms `x = base + λ·L`. Every mode's formula replaces *base* with *x* (§4.2): fold `sin(drive·x)`, ring `x · carrier`, and the lerp `x + g·depth·(hot − x)`; outside the blade the upper returns *x*. Its delta is `D_B = voice_B(x) − x`, and similarly at φ − ½ with `x₂ = base(φ−½) + λ·L(φ−½)`.
>
>   **out = base + L(φ) + D_B(φ) + s_B·D_B(φ−½)**
>
> Consequences, verified to machine precision: at λ = 1 a generator mode on top (sync, FM, noise) *occludes* the lower blade inside its span (`out = (1 − g·d)·(base + L) + g·d·hot`); fold and ring on top process the lower blade's content; at λ = 0 this reduces exactly to the v1 parallel sum.
>
> **Serial crush.** The base part of each hold level stays box-averaged (§4.4). The lower blade's part, `x − base`, is sampled at each step start and held; with slew it glides from the previous held value over the first `slew` fraction of the step, and over the exit ramp it glides onto the live `x − base`, so the blade exit stays continuous. Held state: `idx`, `val`, `pv` per blade state.
>
> **13.3 Collision**
>
> Per member and tick, with blade-local positions `e₁`, `e₂` and the §4.1 edge gates (crush counts as hard-edged, gate 1 inside):
>
>   `ov = gate₁(e₁) · gate₂(e₂)`
>
> *Pitch* is an integrator on the upper blade, so the carrier chirps smoothly rather than jumping: `cacc ← 0` at upper-blade entry; `cacc += (4^(colK·ov) − 1)·kk_upper·Δφ`; carrier phase `cp = hp + xin + cacc` for sync, FM and ring. `cacc` is not wrapped (it is bounded by blade length), which keeps the PolyBLEP carrier-wrap scan valid: the scan adds `cacc − Δcacc` and `cacc` to its tick-start and tick-end offsets.
>
> *Bite* reads `ov` at the current tick. Only the upper blade's state carries `ov`/`cacc`; the lower's stay 0. When both amounts are 0 the fields are cleared once and cost nothing afterwards.

What §13 leaves open: with `upperHears` 0 (λ = 0) and both collision amounts 0 the blades add in parallel, out = base + D₁ + D₂ (+ each twin by its sign), and the result does not depend on `stacking`. "Per member and tick" in §13.3 means per internal sample here (the "PolyBLEP scan" sentence describes the old correction; ED-1's table takes the same carrier-phase offsets).

### C.12 ENV 1's curve

Source: B377 (the human: shapeable curves at every stage); the lead's ruling of the form, 2026-10-16.

- Each timed stage of length T with curve value u ∈ [−1, 1] follows **y(t) = (1 − e^(−c·t/T)) / (1 − e^(−c))** for 0 ≤ t ≤ T, with **c = 4·u**; at c = 0, y(t) = t/T. y(0) = 0 and y(T) = 1 exactly, so no stage leaves a residual step at its end.
- Attack, from the level L₀ it starts at (0 from silence, the current level on a retrigger): level = L₀ + (1 − L₀)·y. Hold: 1 for the hold time. Decay: level = S + (1 − S)·(1 − y). Sustain: S. Release, from the level L_r at key-up: level = L_r·(1 − y).
- Defaults: attack u = 0 (straight); decay and release u = +1, which is the bank's exponential shape (falling by e⁴ across the stage) rescaled to land exactly at T.
- A change of a stage's time or curve while it runs restarts that stage from the current level toward the same target, over (1 − the elapsed fraction) × the new time.

### C.13 The frame

Source: ADR-216 item 1 and A1 (ruled).

- Each voice keeps a frame angle Φ (cycles) per oscillator. Each control interval Δ:
  - advance: Φ ← Φ + the mean of the members' measured phase advances over the interval;
  - measure: d = the wrapped difference ψ/2π − Φ, in (−½, ½];
  - track: g(R) = clip((R − 0.05)/0.2, 0, 1); the fraction F = 1 − (1 − g)^(Δ/Δ_ref), Δ_ref = 32/48000 s; the move is F·d, limited to ±8·Δ cycles; where F = 1 and the move is within the limit, Φ = ψ/2π exactly.
- At a note's start Φ = ψ/2π if R ≥ 0.25, else member 0's phase θ₀; leads are set, not ramped, at note-on and for a member that joins a held frame.
- Each member's lead is **leadᵢ = frac(θᵢ − Φ + ½) − ½** cycles; in swarm frame it adds to the window's centre (C.9 §4.1), and it reaches the blade as a ramp across each control interval at every `motionGrain` (SW-16).
- After a crossing (R through 0, ψ flipping): [X7].

### C.14 Cross-mod and feedback

Source: SPEC-SCALPEL §6; ADR-189 A1 (D3); today's engine.

- Each member's output yᵢ passes through a one-pole low-pass at 5 kHz (time constant 1/(2π·5000) s, per internal sample) to give its loop tap ŷᵢ.
- The carrier phase offset of member i at sample n is **xinᵢ[n] = ½·(`crossMod`·ŷᵢ₊₁[n − 1] + `feedback`·½·(ŷᵢ[n − 1] + ŷᵢ[n − 2]))**, members in a ring (member U − 1's neighbour is member 0). It applies to Sync, FM and Ring carriers on both blades and changes no member frequency.
- *Correction to round 1:* round 1 said "about 4 to 5 kHz"; ADR-189 A1's prose says about 4 kHz, but the ruled build (`LOOP_FC`) and today's engine use 5 kHz, which is copied here.

### C.15 Time constants and grids

| what | value | source |
|---|---|---|
| control interval Δ | round(f_host/1500) samples | ADR-224 item 5 |
| coupling smoother | one-pole, 0.0043512 s | B150; today's engine |
| gravity and pan-motion grid | 256/44100 s | ADR-086 A1 and amendment |
| parameter smoothing | one-pole, 0.012 s | SPEC-SCALPEL §7 |
| random spread glide | one-pole, 0.020 s | SPEC-SCALPEL §5 |
| rotation homing | 0.030 s | SPEC-SCALPEL §6 |
| steal fade; cull fade; stepped-key crossfade | 0.005 s linear; 0.008 s linear; 0.005 s equal-gain | ADR-216; B323; PA-5 |
| DC blocker (with cross-mod, feedback or collision) | first order, 8 Hz | SPEC-SCALPEL §7 |
| loop filter | one-pole, 5 kHz | C.14 |
| High quality's decimation | low-pass at 0.45 × the host rate, at least as steep as a 4th-order Butterworth (two biquads, Q 0.5412 and 1.3066), no look-ahead | SPEC-SCALPEL §7; PF-8 |
