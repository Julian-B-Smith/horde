# The new engine's spec: provenance

**The builder of the new engine does not read this file (ADR-223: the build is fresh from the spec).** It is for the human, the lead, the critic and the judge: where each requirement and law of `specs/SPEC-ENGINE.md` came from, where the spec departs from today's engine and its prototypes and why, what the rebuild retires at cutover, how the new engine is judged against today's, and the history of the open questions.

**Origin.** Written 2026-10-16 by the horde lead's spec agent, splitting round 4 of PR #1097 at the human's instruction: "I think the new engine spec should entirely elide reference to the existing engine files and JS prototypes. The whole idea is the build should be fresh from the spec." Records: ADR-223, ADR-224, ADR-225; ROADMAP B493, B494; the engine audit `docs/audits/2026-10-15-engine-rebuild-audit.md`.

**How to use it.** Every requirement and law in the spec has an id (`SW-4`, `C.6`, …). Section 1 gives each id its sources. Section 2 lists every place the spec departs from today's engine or its prototypes. Sections 3 to 6 hold what the spec no longer carries.

"Today's engine" below means the composed engine in `h2/engine/` as of `main` 990e4a3; "the prototypes" means `reference/scalpel/prototype/razor-core.js` (the blade oracle), `reference/swarmsaw.html` (SwarmSynth), and `docs/design/scalpel-horde-engine.js` (the JS twin that composes them).

---

## 1. Sources, by requirement id

The records each requirement or law rests on. "SPEC" is `specs/SPEC.md`; "SPEC-SCALPEL" `specs/SPEC-SCALPEL.md`; "SPEC-SUBOSC" `specs/SPEC-SUBOSC.md`; "ACCEPTANCE" `specs/ACCEPTANCE.md`; "audit Sn/Bn/Vn/Nn/Kn/Pn" the B493 audit's §2 rows; "rack contract" `docs/proposals/rack-slot-contract.md`; "research note" `docs/research/2026-10-13-edge-table-design.md`; "envelope conflicts" `docs/design/scalpel-envelope-conflicts.md`.

Where ADR-225 ruled a question a requirement depends on, the ruling's item is added after the row's sources.

| id | sources |
|---|---|
| SW-1 | SPEC §2; audit S1 |
| SW-2 | SPEC §3, §6.7; audit S7 |
| SW-3 | SPEC §4; audit S4, S8 |
| SW-4 | SPEC §3, §5.1–5.2, §6.3; ACCEPTANCE L0-2, L0-3; ADR-007; audit S1 |
| SW-5 | SPEC §5.1; ACCEPTANCE L0-9 |
| SW-6 | audit S14 |
| SW-7 | audit S9 |
| SW-8 | SPEC §5.3; audit S11 |
| SW-9 | ADR-056; audit S3 |
| SW-10 | audit S13; B475; ADR-216 A3 item 11 |
| SW-11 | ADR-008; ADR-086 A1; ACCEPTANCE L0-8; audit S15, P2 |
| SW-12 | ADR-077; audit S16 |
| SW-13 | B378 F2; audit S17 |
| SW-14 | ADR-009; charter §Domain; audit S2 |
| SW-15 | B325; audit V5 |
| SW-16 | ADR-224 item 5; B460; ADR-216 item 1; audit B5 |
| SW-17 | audit B6, M1 |
| SW-18 | SPEC-SCALPEL §6, §9; ADR-086 amendment; audit V9 |
| SW-19 | audit round 2 `normExp` lock; audit V10 |
| BL-1 | SPEC-SCALPEL §4.1; audit B1 |
| BL-2 | SPEC-SCALPEL §4.2, §4.4; audit B1 |
| BL-3 | SPEC-SCALPEL §4.3; audit B1 |
| BL-4 | SPEC-SCALPEL §4.1 |
| BL-5 | SPEC-SCALPEL §4.2 |
| BL-6 | SPEC-SCALPEL §4.5 |
| BL-7 | SPEC-SCALPEL §4.4, §9 |
| BL-8 | SPEC-SCALPEL §6 |
| BL-9 | SPEC-SCALPEL §4.6 |
| BL-10 | ADR-216 item 1, A1; B461; audit B4 |
| BL-11 | SPEC-SCALPEL §13; audit B1 |
| BL-12 | SPEC-SCALPEL §6; ADR-189 A1 (D3); audit B9 |
| BL-13 | SPEC-SCALPEL §5; audit B2 |
| BL-14 | ADR-184 A2; B315; audit B3 |
| BL-15 | SPEC-SCALPEL §5 |
| BL-16 | B487; B485; audit B5 |
| BL-17 | ADR-216 A4; B460; B484; audit B5, H4 |
| BL-18 | B469; B481; audit B16, B17 |
| BL-19 | SPEC-SCALPEL §7, §9; audit B13 |
| ED-1 | ADR-222; ADR-189 (D1, D2 aims); SPEC-SCALPEL §8.2; audit B11, B14 |
| ED-2 | ADR-222 |
| ED-3 | ADR-222 item 2(b); research note F5 |
| ED-4 | ADR-222 item 2(a), (c); research note F3, F4 |
| ED-5 | ADR-222 item 4 |
| ED-6 | ADR-222 item 3; audit B12 |
| VO-1 | ADR-224 item 1; audit H3 |
| VO-2 | ADR-083; B310; ADR-216 A3 item 2; envelope conflicts decision 7; audit V1 |
| VO-3 | ADR-216 item 2, A3 items 1–8; B459; audit V3 |
| VO-4 | B323; B375; audit V2 |
| VO-5 | ADR-224 item 1; audit V8 |
| VO-6 | SPEC-SCALPEL §6; ADR-084; ADR-208; audit V4; envelope conflicts decision 5 |
| VO-7 | rack contract A5 |
| VO-8 | ADR-084; audit V10 |
| VO-9 | ADR-224 item 2; B303 |
| VO-10 | audit §3.1 |
| EN-1 | B366; B377; audit V11 |
| EN-2 | ADR-161; envelope conflicts decision 5; VO-3 |
| EN-3 | B377; PA-1 |
| EN-4 | ADR-009; V7 |
| PA-1 | SPEC-SCALPEL §7; B378 F8; audit B6 |
| PA-2 | ADR-224 item 4; audit §3.3 |
| PA-3 | ADR-224 item 5; V7 |
| PA-4 | ADR-224 item 5 |
| PA-5 | B479; rack contract RS-3.6, RS-4.3; ADR-218; audit B7 |
| PA-6 | audit B7 |
| PA-7 | ADR-207 item 2; ADR-213; audit N14 |
| PA-8 | ADR-187 item 4(b) (the dependency probe) |
| DT-1 | ADR-224 item 6; charter §Domain; audit N1 |
| DT-2 | module 1.0 bar §3; rack contract RS-7.11; audit V7 |
| DT-3 | SW-14; ADR-197 armor row 4 |
| DT-4 | charter §Domain invariant 1; SPEC-SCALPEL §9; B308 H3; B469; audit N2, N6 |
| DT-5 | charter §Domain; SPEC §5.7 |
| DT-6 | rack contract A2 |
| RT-1 | `h2/README.md` rule 5; rack contract RS-7.7, RS-7.10; audit N12 |
| RT-2 | SPEC-SCALPEL §7 |
| RT-3 | ADR-197; rack contract RS-7.9; audit N15 |
| RT-4 | ADR-211; ADR-213 item 3; audit N13 |
| OU-1 | SPEC-SCALPEL §10; ADR-221 item 6; audit V13 |
| OU-2 | ADR-221 items 1, 2, 4; B489; audit V12 |
| OU-3 | ADR-221 item 3; ADR-197; ADR-223 item 3; B445; audit V14, K2 |
| OU-4 | rack contract A5, RS-7.6 |
| SN-1 | SPEC-SUBOSC §1, §2, §4; audit §3.6 |
| SN-2 | SPEC-SUBOSC §3, §5.2, §10.6 |
| SN-3 | SPEC-SUBOSC §5.1, §5.3 |
| SN-4 | ADR-190 A6; B452; audit §3.6 |
| IF-1 | ADR-190 A4; ADR-084; ADR-208; ADR-224 item 1; audit §3.1 |
| IF-2 | B278; B397; B431; ADR-096; audit P1–P3, §3.2 |
| IF-3 | ADR-224 items 4, 5; ADR-169 A3; ADR-192; B429; audit §3.3 |
| IF-4 | ADR-104; ADR-185; ADR-188; ADR-208; rack contract RS-6.6–6.13, A17–A20; audit §3.4 |
| IF-5 | ADR-224 items 1, 2; B303 |
| IF-6 | B225; B402; B376; ADR-221; ADR-203 item 5; audit §3.5 |
| IF-7 | ADR-222; rack contract RS-2.8, RS-7.1 |
| IF-8 | ADR-224 item 3; ADR-190 A9; B408 |
| IF-9 | ADR-191; B445; ADR-187 item 8 |
| IF-10 | ADR-223 item 4; audit §3.10, N21 |
| IF-11 | B308 H4; B428; ADR-207 item 2; audit §3.11 |
| QS-1 | ADR-222; research note F1; audit B11 |
| QS-2 | ADR-222 item 2(a) |
| QS-3 | ADR-191; B346; B445 |
| QS-4 | B490; B459; B461; B485; B486; ADR-218 |
| QS-5 | ADR-221 item 4; B489 |
| QS-6 | SPEC-SCALPEL §9 |
| QS-7 | rack contract A5; SPEC-SUBOSC §10.5; RT-4 |
| QS-8 | ADR-197 armor rows 3, 6; ACCEPTANCE L0-13 |
| QS-9 | ADR-197 armor row 5 |
| QS-10 | ADR-197 armor row 9; B408 |
| PF-1 | B488; B494; audit §4.2 |
| PF-2 | B439; B488; ACCEPTANCE E-6 |
| PF-3 | `docs/port/cpu-ledger.md` §protocol; ADR-187 item 8 |
| PF-4 | audit M4 |
| PF-5 | audit V8; cpu-ledger |
| PF-6 | SPEC-SCALPEL §8.8 |
| PF-7 | rack contract RS-2.6; audit §9 |
| PF-8 | ADR-222; rack contract RS-7.1 |
| C.1 (Coupling) | SPEC.md §3 (layer 2), §5.1–5.2, §6.3; ADR-007; ADR-056; the law as today's engine runs it. |
| C.2 (Placement) | today's engine (from SPEC.md §3 and the legacy core). For i = 0 … U − 1 and uᵢ = i/(U − 1) (U = 1: x₀ = 0): |
| C.4 (Drift) | today's engine (the walk mode). *Correction to the brief's transcription:* the formula Tᵢ ← Tᵢ − α(Tᵢ − mean) + N(0, σ) is the ensemble's timing correction (C.7), not the drift; the drift is this walk. |
| C.5 (Inertia (PROPOSED: the scope is (ADR-225 item 11))) | SPEC.md §5.3; today's engine. With w = knob^2.5 (the locked taper), ω₀ = 2π·(8·(1 − w) + 0.6) rad/s, and yᵢ member i's offset from f₀ (yᵢ\* = gᵢ − f₀ its target), each control interval: |
| C.6 (Gravity) | ADR-008; ADR-086 A1; today's engine. |
| C.7 (The ensemble (onset scatter)) | ADR-077; today's engine. Per oscillator a persistent offset vector Tᵢ (seconds, i = 0 … U − 1), zero at reset, at a seed change and at a load. |
| C.8 (Pan and density) | SPEC-SCALPEL §6, §9; the legacy core's pitch fan (ADR-070, ADR-073); ADR-064 and ADR-086's amendment. |
| C.12 (ENV 1's curve) | B377 (the human: shapeable curves at every stage); the lead's ruling of the form, 2026-10-16. |
| C.13 (The frame) | ADR-216 item 1 and A1 (ruled). |
| C.14 (Cross-mod and feedback) | SPEC-SCALPEL §6; ADR-189 A1 (D3); today's engine. |
| BD-1 | ADR-223; audit §1.8 |
| BD-2 | audit §1.8 |
| BD-3 | audit §1.5; N18 |
| BD-4 | audit §1.4; N10 |
| BD-5 | B308 H4 |
| BD-6 | ADR-223 (the "self-documenting and concise" mandate); the audit's size estimate (§4.1) |
| BD-7 | ADR-224 item 6; audit §1.3; audit N3–N9 |
| BD-8 | audit §5.5 |
| BD-9 | audit N10; `h2/README.md` rule 7's purpose |
| BD-10 | ADR-223 item 2 (a clean-room build from the spec and the tests alone); the human, 2026-10-16 ("the build should be fresh from the spec") |
| VO-11 | ADR-225 item 13 (the human asked about polyphony; "voices not sounding cost no CPU") |
| PF-9 | ADR-225 item 13 |

Requirements that depend on ADR-225's rulings: X10 → `unison`, RT-1 (item 1); X16 → PA-4, rate column `A` (item 2); X8 → SW-16, BL-16, BL-17, `motionGrain` (item 3); Q14 → PA-5, PA-6 (item 4); Q16 → DT-4 (item 5); Q21 → SN-1, SN-3 (item 6); Q9 → IF-2, SN-1 (item 7); Q8 → SW-11, IF-2 (item 8); X18 → the rules gate (item 9; section 3 below); Q30 → IF-10 (item 10); X19 → SW-8, C.5 (item 11); X20 → QS-4, 10.1 (item 12); polyphony → `voice.polyphony`, VO-4, VO-5, VO-11, RT-1, PF-5, PF-9 (item 13; Q3 and Q4 with it).

---

## 2. Departures from today's engine and its prototypes

Each is stated in the spec as plain law or requirement, without this history. Reason first, then where.

| # | the spec says | today's engine or the prototype | why | where |
|---|---|---|---|---|
| 1 | One phase per member, θ, with the blades reading φ = frac(θ + ½) | two phase counters per member, φ_horde and φ_SCALPEL, advanced separately | the two prototypes put the saw's jump at different places; one phase with a constant offset keeps the bank's blade positions and start phases (audit §1.8) | C.0 |
| 2 | Coupling, drift, inertia, spreads and the frame update once per control interval, about 1/1,500 s (29 samples at 44.1 kHz) | swarm tick 16 samples; spreads and frame 32 samples | ADR-224 item 5; every law is in seconds, so only a near-critical regime could move, which SW-4's +0.7 and +0.6 anchors catch (a simulation of C.1 at Δ = 29 samples still reads them) | C.0, C.15, SW-4 |
| 3 | The sync pull carries a phase lag α, in both the mean-field and root-pivot forms | today's engine has no α term (phase lag was never composed); SPEC.md §5.1 and the legacy core have it in the mean-field form | the `phaseLag` row is a keep (audit round 2); its anchor is SW-5; the root-pivot form is this spec's extension | C.1, SW-5 |
| 4 | SW-5's phase-lag anchor at K +1.0, α 10°–30° | ACCEPTANCE L0-9 (60°, 1.8 Hz) | L0-9 was measured in the dynamics lab (24 members, detune 0.2); at SW-4's conditions a 60° lag loses the lock (a simulation of C.1: R swings to 0.29 at K +0.9); at K +1.0, α 10°, 20°, 30° the swarm stays locked and shifts by 1.42, 2.76, 3.94 Hz against S·R·sin α = 1.47, 2.88, 4.16 Hz | SW-5 |
| 5 | Inertia swings members' offsets from the oscillator's pitch only | today's engine swings the whole member frequency, so a glide, a bend or audio-rate pitch is low-passed below about 9 Hz whenever inertia is on | ADR-225 item 11; the skip at knob ≤ about 0.063 is kept; no bank preset sets inertia | C.5, SW-8 |
| 6 | The carrier's frequency is capped at 0.45 × the internal rate in every mode | the prototype caps the cut ratio at 0.45·f_s/fᵢ before the per-blade division, so a narrow per-blade window can run its carrier past the band edge | "never folded" (X2's principle) | BL-4, C.9 item 8 |
| 7 | Quantize: both roundings half away from zero | the prototype rounds the spread s half up (toward +∞), then the offset half away from zero | the exact mirror of a negative spread (ADR-184 A2) at half-integer spreads | C.10 item 4, BL-14 |
| 8 | The drift spread law in exact time: p ← p·e^(−θΔ) + 0.289·√(1 − e^(−2θΔ))·z | σ = 0.289·√(2θ·dt) at a fixed 32-sample dt | the same stationary spread (0.289) and correlation time (1/θ) at any control interval; the prototype's form is its first-order approximation | C.10 item 3 |
| 9 | The frame starts each note at ψ if R ≥ 0.25, else at member 0's phase | ADR-216's constants (decided under its delegation) start it at ψ always | below R 0.25, ψ is the ill-conditioned angle the frame exists to avoid | C.13, BL-10 |
| 10 | Each voice and member draws noise from its own stream | the prototype's noise blade draws from the one shared stream, so any note-on changes every sounding voice's noise (B469) | DT-4: a note-on never changes a voice already sounding | C.9 item 4, BL-18, DT-4 |
| 11 | Modulator noise from the member's own stream keyed by the integer | the prototype's `sin` hash | SPEC-SCALPEL §10 ("do not replicate") | C.9 item 5 |
| 12 | The cross-mod and feedback loop filter at 5 kHz | ADR-189 A1's prose says about 4 kHz; the ruled build (`LOOP_FC`) and today's engine use 5 kHz | copied from the build | C.14 |
| 13 | The frame never jumps (D6), a taken voice fades (D7), spread values ramp at `motionGrain` 0 (D8), a quantized cut waits for its cycle start (D9), the edge table (D10), the carrier and scan aims of D1 and D2, the loop filter of D3 | each was an off-by-default switch beside the old behaviour | ADR-223 item 5; the switches retire, the behaviours are requirements; D8's "off" (the human's blind preference, B460) is `motionGrain` 1, the default | SW-16, BL-10, BL-16, BL-17, VO-3, ED-1, C.14 |
| 14 | A linear output stage: 0.14 × 10^(level/20) × the voice sum, no saturation | `tanh(1.6 · gain · y)` at the default gain 0.0875 (ADR-221) | the clipper made chords clip on 71 of 83 presets (B489); 0.14 = 1.6 × 0.0875 is today's small-signal gain, so 0 dB matches today's default level for small signals | 6.9, OU-1 |
| 15 | Members sum with density compensation U^−0.75; the bank translates with +5·log₁₀(N) dB | the bank was voiced with 1/√N | the human's lock (`normExp` 0.75, 2026-10-02); the offset restores each bank preset's level exactly, so its balance (ADR-221 item 2) and its headroom (worst four-note chord 0.815, B489) are today's | C.8, SW-19, B.1 |
| 16 | ENV 1's stages land exactly at their time (C.12) | the prototype's linear attack and exp(−4t/T) decay and release never land and free the voice under 1e-4 | B377's shapeable curves; +1 is the prototype's shape rescaled to land at T | C.12, EN-1 |
| 17 | "Settled" start is genuinely settled | today's engine plays the bank's `phaseMode` 2 (settled) as aligned | the merged start-phase row (audit round 2); the bank translates settled to aligned, which is what the human tuned on (X1) | SW-10, B.1 |
| 18 | A member near the band edge fades out | members above Nyquist alias and add DC (B378 F2, never ruled) | "never folded" | SW-13 |
| 19 | Pitch arrives per note id, already bent and glided; glide is the pitch system's | today's engine keys voices by MIDI note and glides inside the engine | ADR-224 item 1; the audit's merge of `glide` into the note lane | IF-2, VO-6 |
| 20 | The sub integrates its own phase; ENV 1 acts after the sub's tone | SPEC-SUBOSC §2: the voice hands the sub oscillator 1's phase; the envelope before the tone | ADR-225 item 6 ratifies both: a swarm has no single phase; one envelope position for every source. SPEC-SUBOSC itself is not edited | SN-1, SN-3 |
| 21 | Polyphony 1 to 64, default 16, preallocated; voices not sounding cost nothing | a pool of 8 (default `poly` 6), 16 voices as two engines | ADR-225 item 13 | 5.2, VO-5, VO-11, RT-1, PF-5, PF-9 |
| 22 | Unison 1 to 16 | 9 members fixed in the prototype; 1 to 32 in the legacy swarm | ADR-225 item 1 | 5.3, RT-1 |
| 23 | The blade text of Appendix C: SPEC-SCALPEL §4.1–4.5, §5 and §13.2–13.3 copied with these edits: section numbers renumbered to C.9.1–C.9.5; §4.1's carrier-cap sentence restated per item 6; §4.4's "Verified: largest jump < 0.002" sentence dropped (BL-7 holds the bound); §5's drift σ restated per item 8; §13.2's "as in v1" and "the v1 parallel sum" reworded; §13.3's "PolyBLEP carrier-wrap scan" reworded to the edge correction's carrier-wrap detection | the protected SPEC-SCALPEL text | the spec states laws as themselves, without attribution (the human, 2026-10-16) | C.9, C.10, C.11 |
| 24 | The ensemble's waits do not "heal by sustain" | — | round 1 of the spec said they did; that was a lab candidate (B370), not the law; withdrawn in round 2 | C.7 |
| 25 | The bank's enumerations map by meaning: waves 0 sine, 1 triangle, 2 saw, 3 square, 4 reverse saw, 6 sine→saw; spread law 0 gradient, 1 random, 2 alternate, 3 swarm, 4 drift; `phaseMode` 0 random, 1 aligned, 2 settled | the prototype's stored ids (`parameters.json` lists `phaseMode`'s options out of order) | read from the prototype's code and the bench's controls | B.1 |

**Corrections made to the lead's transcription of the laws (round 2):** the composed engine has no α term (row 3); σ is the RMS spread of the natural frequencies (after the detune law and drift, before coupling); the formula given as the drift walk is the ensemble's timing correction (C.7), and the drift is a leaky walk (C.4); the root pivot has no R factor and anchors the splay on the root member; gravity's "new chord" is no chord reset (each newly struck note starts at its played pitch; held notes keep theirs), with |e| ≤ 35 c and pairs applied in sorted order; the ensemble's entry fade is a one-pole of τ = max(2 ms, attack) on every member; the loop filter is 5 kHz (row 12). In round 3 the critic corrected SW-5 (row 4).

---

## 3. Scope of the rebuild: what retires, what is re-pointed, what stays

Nothing here is removed before its replacement exists and is wired; each removal from `./verify` is the human's gate.

**The rules gate (ADR-225 item 9).** `tools/h2_rules_check.py` rule 2 (every h2 core built with `-ffp-contract=off`) does not apply to the new engine; it stays for the legacy and frozen cores. The change to the gate is sanctioned by ADR-225 and is made in the build's own PR.

**The WebAssembly toolchain (ADR-225 item 10).** One pinned version, approved in principle; installed at the lab stage with the human's word on the exact version.

**The clean-room tree (spec BD-10).** The lead provides the builder a sparse checkout that does not contain today's engine, the twin, the prototypes or the legacy plugin; the bank (`reference/scalpel/data/presets.json`) is provided as input data with the acceptance tests.
### 3.1 Retired at cutover, after the new engine is accepted

Nothing here is removed before its replacement exists and is wired; each removal from `./verify` is the human's gate (charter; audit §7). Paths are from the audit's inventory (§1.2, §1.10, §7).

| item | paths | files | lines | reason |
|---|---|---|---|---|
| today's engine | `h2/engine/engine.h`, `h2/engine/blade.h`, `h2/engine/swarm.h`, `h2/engine/js.h` | 4 | 2,994 | replaced |
| the JS twin | `docs/design/scalpel-horde-engine.js` | 1 | 1,679 | the labs run the C++ (ADR-223 item 4) |
| the frozen earlier ports used as test references | `h2/cores/scalpel/razor_core.h`, `h2/cores/swarm/swarm_core.h`, `h2/cores/swarm/force_core.h`, `h2/cores/swarm/glide_core.h`, `h2/cores/swarm/lift-ledger.json` | 5 | 4,041 | they exist only to hold parity |
| the parity tools | `tools/h2_engine_parity_check.cpp` (and its contraction-on control build), `tools/h2_engine_render.mjs`, `tools/labharness/composed_engine_check.mjs`, `tools/labharness/divergence_ledger_check.mjs`, `tools/parity_floor_check.py`, `tools/h2_libm_count.py`, `h2/engine/libm-calls.darwin-arm64.appleclang-16.txt`, `tools/h2_scalpel_parity_check.cpp`, `tools/h2_scalpel_render.mjs`, `tools/h2_scalpel_stream.h`, `tools/h2_swarm_parity_check.cpp`, `tools/h2_swarm_lift_check.cpp`, `tools/h2_swarm48_check.cpp`, `tools/h2_lift_check.py`, `tools/measure_h2_scalpel.cpp` | 15 | 5,656 | parity is no longer a goal (ADR-223 item 3) |
| the divergence-switch machinery | `docs/port/divergences.json`; the switches D1 to D10 (D9 and D10 parked on their branches, which are left as they are) | 1 | — | D1 to D10 become plain requirements of the spec or are dropped (section 5) |
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

**Kept, re-pointed at the new engine** (audit §7): the behaviour checks `tools/h2_frame_hold_check.mjs`, `tools/h2_steal_fade_check.mjs`, `tools/h2_spread_ramp_check.mjs`; the fuzz (`tools/h2_fuzz_check.mjs`); the soak (`tools/h2_soak_check.mjs`); the RealtimeSanitizer probe and lint (`tools/h2_engine_rtsan_check.py`, `tools/h2_rt_lint_check.py`); the self-digest tool (`tools/h2_engine_selfdigest_check.cpp`); the harness self-test; the A/B tool (`tools/h2_ab.mjs`); the CPU tool (`tools/measure_h2_engine.cpp`); everything under `tools/patchspace/` that imports `space.mjs` (which loads the twin today): `space.mjs` itself, `gauntlet.mjs`, `gauntlet_report.mjs`, `alias_sources.mjs`, `antialias_scope.mjs`, `calibrate.mjs`, `fidelity.mjs`, `fidelity_audit.mjs`, `gen_dependency_tree.mjs`, `listening_sample.mjs`, `os_quality.mjs`, and the gated checks `metrics_check.mjs`, `dependency_tree_check.mjs`, `fidelity_scan_check.mjs` (through `fidelity.mjs`) and `listening_pass_check.mjs`.

### 3.2 Untouched

| item | reason |
|---|---|
| The legacy plugin (`src/`) | frozen; the charter's invariants still govern it. |
| The prototypes in `reference/` | history and protected; nothing edits them (Q30, prototypes part, still open: keep them). |
| The FX modules | their own projects, behind the rack-slot contract. |
| The GUI, and the host shell beyond the engine's API | out of the spec's scope. |
| Design labs that do not run the engine (the edge-correction lab, the mixer and stereo labs, and others) | they do not load an engine copy. |

---

## 4. Judging the new engine against today's

The spec tells the builder what to deliver for judging (its 10.3: offline renders of the bank and the envelope presets in the A/B tool's input format). The judging itself is here.

- **The A/B.** The whole bank (83 presets) and the 12 envelope presets of B366 (`ENV_PRESETS` in `docs/design/scalpel-interface-lab.html`), translated by the spec's Appendix B, rendered offline by the A/B tool (ADR-220; `tools/h2_ab.mjs`): today's engine as A, the new engine as B, the same phrases (a held note, a chord, a fast line, a trill), the harness driving each note's pitch through a glide (the lag law, the preset's glide time) where the preset glides; level-matched, the level of the difference beside each pair, played looping with a click-free switch and a blind mode.
- **"Sounds good" is a process, not a number.** The human listens in capped listening batches (ADR-197). For each preset: recognisable and good; recognisable but needs a change (the preset is edited, not the engine, unless the human says the engine is wrong); or not recognisable (the engine or the translation is wrong for that preset, and the cause is found). The bank passes when the human says it does.
- **The batches carry at least:** the ten presets whose sound changed most beyond level under ADR-221 (Clockwork, Orbiting cuts, Ring of sines, Crushed bells, Drifting cuts, Anchored horde, Two-formant vowel, Vowel choir, Golden bells, Cross-mod shimmer), to hear whether the old output saturation's grit mattered; Glass horde pad and the Breathing pad for the grain (SW-16) and the crackle (BL-16); every preset whose start phase translates (X1).
- **Loudness (the spec's OU-2 (a) wins over this):** the bank at its translated levels is within ±1 dB on average of today's engine at its ADR-221 default, as the level-matched A/B reports; a shortfall is reported.
- **CPU, against today's.** Today's engine measures Glass horde pad, 8 voices, 1×, at 27.2 % of an M3 performance core (the audit, §1.9; the ledger reads 26.9 to 27.2 %). The audit estimates a clean engine at about 11.5 % (7.8 % best, 15.3 % cautious), excluding the always-on ramps (D8 measured +2.7 to +9.9 %) and per-voice modulation; 11.8 to 12.6 % with the ramps. The target (the spec's PF-1, 10.7 %) is reachable only at the central-to-best end; **a miss is likely**, and is reported with what would close it.
- **Memory, against today's:** today's engine is about 125 KB for 8 voices of 9 members (the audit, §9).
- **The frozen goldens** are the self-digest files (`h2/engine/selfdigest.*.txt`), re-baselined by their tool for the new engine, never by hand: a protected path, so a sanctioned step (audit M10). After they freeze, every change to the engine's sound is recorded with its A/B and the human's word (X6).

---

## 5. Traceability: the audit's 75 decisions
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
| K1 the bank sounds good (the 83 presets and the 12 envelope presets) | KEEP | 10.3; 10.1 item 16; Appendix B |
| K2 oversampling saved per preset | KEEP | 5.2 `quality`; OU-3 |
| P1 pitch bend | RULING | IF-2; Q7 |
| P2 gravity rides the played pitch | KEEP | SW-11, IF-2 |
| P3 pitch as an input | KEEP | IF-2 |

Count: 75 rows. KEEP 40, RULING 17, DROP 14, CULLED 4.

Rows that ADR-225 settled since: V8 the pool (item 13: 1 to 64, default 16); S11 inertia (item 11); B5's micro-motion (item 3); B7 stepped keys (item 4).

---

## 6. The open questions' history

The audit (§3.12) listed 31 interface questions; the spec added X1 to X20.

**Ruled by ADR-224 (6):** Q1 voice ownership, Q10 per-voice modulation's reach, Q11 the modulation rate, Q18 the per-voice signal, Q25 true stereo's shape, Q28 precision.

**Ruled before the build by ADR-225 (13)**, the human: "go with your recommendations".

| # | question | ruling | ADR-225 item |
|---|---|---|---|
| X10 | the Unison range | 1 to 16 members per oscillator | 1 |
| X16 | audio-rate destinations | voice pitch, oscillator pitch and fine, sub fine, the blades' cut rates, FM depth; everything else control-rate, ramped | 2 |
| X8 | the micro-motion texture | `motionGrain` steps only the spread law's values, default stepped; the frame always ramps; the quantized-cut latch at every setting | 3 |
| Q14 | stepped settings while a note sounds | at once, through a 5 ms equal-gain crossfade; a second change waits | 4 |
| Q16 | random draws | position-free mulberry32 streams keyed by seed, purpose, note counter and member | 5 |
| Q21 | the sub's phase | the sub integrates its own phase from `startPhase`, starting with the voice's master phase; both departures from SPEC-SUBOSC §2 ratified | 6 |
| Q9 | the published pitch | the sounding pitch (input × gravity's ratio) | 7 |
| Q8 | tuning and gravity | the tuning sets the target; gravity on top only when its knob is up | 8 |
| X18 | the rules gate | rule 2 does not apply to the new engine; changed in the build's own PR | 9 |
| Q30 (toolchain) | the WebAssembly toolchain | one pinned version, approved in principle; installed at the lab stage with the human's word on the version | 10 |
| X19 | inertia's scope | members' offsets only; the skip kept | 11 |
| X20 | the click detector | the method ratified now; p, M, k proposed by the test author with must-fire and must-not-fire controls, ratified then; the test author's first deliverable | 12 |
| (new) | polyphony | up to 64 voices, preallocated; default 16; in Settings; voices not sounding cost no CPU; the voice law and the steal fade unchanged past the limit; no CPU-adaptive limiting by default (Q3); the CPU budget measured at 8 voices | 13 |

**Dropped as already ruled by earlier records (6):** Q4 (one pool, by ADR-224 item 1; since ADR-225 item 13 the pool is 1 to 64, default 16); Q5 (chords as copies, post-1.0 by ADR-190 A2); Q27's per-preset part (quality saved per preset, ADR-191 and B445); Q22's allocation and ENV 1 part (ADR-224 item 1); X14 (no per-source ENV 1 opt-out, B370); Q3 (a fixed count, no CPU-adaptive limiting by default, ADR-225 item 13).

**Still open, with the reasons the recommendations rest on** (the spec's section 11 carries only what is built until each is ruled). The spec lists 24 during the build and 5 after: it carries X15 among the during-the-build items (the minimum noise oscillator is built now), and does not carry Q30's prototypes part, which concerns the repository, not the build (keep `reference/` as protected history; it is listed here only).

**The spec's appendix letters since round 4:** Appendix A is the 12 envelope presets as input data, Appendix B the bank and version-0 state formats with their translation, Appendix C the laws; the round-3 traceability appendix is this file's section 5.
### 6.1 To be ruled during the build, before the stage that needs them (24)

| # | question | recommendation | reason | depends |
|---|---|---|---|---|
| Q2 | MPE: which expressions reach the swarm and the blades. | Per-note tuning into the note's pitch; pressure, timbre and slide as per-voice mod sources, pressure to level by default. | The note-id voice already gives MPE its plumbing; routing is the matrix's. | IF-1 |
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

### 6.2 Can be ruled after the build (6)

| # | question | recommendation | reason | depends |
|---|---|---|---|---|
| Q23 | Shriek as a fixed post-stage per oscillator, or a rack slot. | A rack slot; the engine offers no insert. | Keeps the engine's boundary small (IF-7); nothing in the engine changes either way. | IF-7 |
| Q26 | Which width candidates are in 1.0, and the mono-fold guarantee. | In the engine: per-member pan only (SW-18), no polarity inversion; every other candidate is post-voice and the stereo lab's; mono fold reported (QS-10) until the lab sets a bound. | ADR-224 item 3 put true stereo after the voice. | SW-18, IF-8 |
| Q29 | Is an M1 or Intel min-spec measurement a precondition for freezing the CPU row. | Not for building; yes before the CPU row is frozen for release. | Every min-spec figure rests on the assumed ×1.5. | PF-2 |
| X6 | Does a sound-change ledger continue against the new engine's own goldens. | Yes: every change after the goldens freeze is recorded with its A/B and the human's word. | ADR-220 item 1 names the ledger as part of correctness. | 10.2 |
| X9 | A twin wider than half a cycle under the cut latch: give the twin its own latch. | Not in 1.0; the edge stays stated. | No bank preset uses it (B487's critic). | BL-16 |
| X15 | The noise oscillator's design (no lab yet, B452). | Build the minimum of SN-4 now; revisit after its lab. | ADR-190 A6 put a plain noise oscillator in 1.0; its design lab has not started. | SN-4, 5.6 |

---

## 7. The spec's rounds

| round | commit | what moved |
|---|---|---|
| 1 | `20cd9b2` | the first draft from the B493 audit and ADR-224 |
| 2 | `8d3b792` | the human: "copy the laws"; the laws became normative (Appendix C); the critic's HIGH and MEDIUM findings closed; the questions sorted before / during / after the build |
| 3 | `78214e9` | the critic's re-read: SW-5's anchor at a lockable K; inertia's scope and the detector's method put to the human |
| 4 | this change | ADR-225's thirteen rulings folded in; the spec split from this file, so the builder's text names no old file, prototype or engine (the human: "the build should be fresh from the spec") |
