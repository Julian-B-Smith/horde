# The new engine's spec: provenance

**The builder of the new engine does not read this file (ADR-223: the build is fresh from the spec).** It is for the human, the lead, the critic and the judge: where each requirement and law of `specs/SPEC-ENGINE.md` came from, where the spec departs from today's engine and its prototypes and why, what the rebuild retires at cutover, how the new engine is judged against today's, and the history of the open questions.

**Origin.** Written 2026-10-16 by the horde lead's spec agent, splitting round 4 of PR #1097 at the human's instruction, and extended in round 5 with the verification round's rulings (ADR-226): "I think the new engine spec should entirely elide reference to the existing engine files and JS prototypes. The whole idea is the build should be fresh from the spec." Records: ADR-223 to ADR-229; ROADMAP B493, B494; the engine audit `docs/audits/2026-10-15-engine-rebuild-audit.md`.

**How to use it.** Every requirement and law in the spec has an id (`SW-4`, `C.6`, …). Section 1 gives each id its sources. Section 2 lists every place the spec departs from today's engine or its prototypes. Sections 3 to 6 hold what the spec no longer carries.

"Today's engine" below means the composed engine in `h2/engine/` as of `main` 990e4a3; "the prototypes" means `reference/scalpel/prototype/razor-core.js` (the blade oracle), `reference/swarmsaw.html` (SwarmSynth), and `docs/design/scalpel-horde-engine.js` (the JS twin that composes them).

---

## 1. Sources, by requirement id

The records each requirement or law rests on. "SPEC" is `specs/SPEC.md`; "SPEC-SCALPEL" `specs/SPEC-SCALPEL.md`; "SPEC-SUBOSC" `specs/SPEC-SUBOSC.md`; "ACCEPTANCE" `specs/ACCEPTANCE.md`; "audit Sn/Bn/Vn/Nn/Kn/Pn" the B493 audit's §2 rows; "rack contract" `docs/proposals/rack-slot-contract.md`; "research note" `docs/research/2026-10-13-edge-table-design.md`; "envelope conflicts" `docs/design/scalpel-envelope-conflicts.md`.

Where ADR-225 or ADR-226 ruled a question a requirement depends on, the ruling's item is added after the row's sources. "Dry run 2" marks a finding of the second clean-room dry run (of `2f1a389`, round 6). "ADR-226 B" marks a finding of the verification round that needed no ruling (the five agents' reports on `7d5f7b6`, as the lead listed them for round 5). Code anchors are `file:line` on `main` as merged in round 5 (`31ce152`).

| id | sources |
|---|---|
| SW-1 | SPEC §2 (one engine), §3 layer 1 (who exists); the audit §1.8 (one phase counter per member: departure 1) |
| SW-2 | SPEC §3, §6.7; audit S7 |
| SW-3 | SPEC §4; audit S4, S8 |
| SW-4 | SPEC §3, §5.1–5.2, §6.3; ACCEPTANCE L0-2, L0-3; ADR-007; audit S1; ADR-226 item 12 (the bank's rescale, departure 35) |
| SW-5 | SPEC §5.1; ACCEPTANCE L0-9 |
| SW-6 | audit S14 |
| SW-7 | audit S9 |
| SW-8 | SPEC §5.3; audit S11; ADR-225 item 11; departure 40 |
| SW-9 | ADR-056; audit S3 |
| SW-10 | audit S13; B475; ADR-216 A3 item 11 (resume); ADR-226 item 2 (Phase, Random and Retrigger replace the start modes; the settled start removed); today's start draw `h2/engine/swarm.h:121-131`; dry run 2 (#25): resume when the source voice had another member count |
| SW-11 | ADR-008; ADR-086 A1; ACCEPTANCE L0-8; audit S15, P2; ADR-225 item 8; ADR-226 item 1 (a key held by the pedal is held) |
| SW-12 | ADR-077; audit S16; ADR-226 B (C.7's definitions) |
| SW-13 | B378 F2; audit S17 |
| SW-14 | ADR-009; charter §Domain; audit S2 |
| SW-15 | B325; audit V5; dry run 2 (#18): S and P rise from 0 by C.1's smoother, stated as the law's own onset |
| SW-16 | ADR-224 item 5; B460; ADR-216 item 1; audit B5; ADR-225 item 3 |
| SW-17 | audit B6, M1; dry run 2 (#19): C.0 now ramps each new gᵢ across the following interval (departure 46) |
| SW-18 | SPEC-SCALPEL §6, §9; ADR-086 amendment; audit V9; the legacy core's pan laws (`h2/cores/swarm/swarm_core.h:1570-1660`: the balanced seats, the pitch fan, its invert) and its pan motion (`swarm_core.h:886-918`: per-member rates 0.08 + 0.021·i Hz on the grid, added after width: departure 30); the prototype's balanced table and spread fan (`reference/scalpel/prototype/razor-core.js:62-65, 733-737, 904`); ADR-226 item 5 (per-oscillator pan); ADR-226 B (the brightness measure and the U 10 to 16 seat rule); dry run 2: the test patch has blade 1 on |
| SW-19 | audit round 2 `normExp` lock; audit V10 |
| BL-1 | SPEC-SCALPEL §4.1; audit B1 |
| BL-2 | SPEC-SCALPEL §4.2, §4.4; audit B1 |
| BL-3 | SPEC-SCALPEL §4.3; audit B1 |
| BL-4 | SPEC-SCALPEL §4.1 |
| BL-5 | SPEC-SCALPEL §4.2 |
| BL-6 | SPEC-SCALPEL §4.5; dry run 2 (#29): the claim's mode scope |
| BL-7 | SPEC-SCALPEL §4.4, §9; ADR-226 B (measured on the crush blade's contribution alone, at a stated pitch, Edges and sweep); dry run 2: round 5's measure failed a correct crush (D = output − x includes the base's own jumps: at the default 0.875 / 0.25 the window ends on the square's edge, max \|ΔD\| 2.0; and the law's own slews exceed 0.01 per sample: at C3, Edges 0.3, sine 0.079, triangle 0.056, saw 0.028). Now judged on the output inside a window that holds no base edge, against 1.25 × the law's own largest step |
| BL-8 | SPEC-SCALPEL §6; dry run 2 (#40): one free-running clock per blade per oscillator against per-voice `rotate` offsets |
| BL-9 | SPEC-SCALPEL §4.6 |
| BL-10 | ADR-216 item 1, A1; B461; audit B4 |
| BL-11 | SPEC-SCALPEL §13; audit B1; ADR-226 B (the collision-bite law restored: SPEC-SCALPEL §13.1's `colB` row; `razor-core.js:189, 199`) |
| BL-12 | SPEC-SCALPEL §6; ADR-189 A1 (D3); audit B9; dry run 2 (#42): the limit-cycle measure (the output repeats with a whole-sample period) |
| BL-13 | SPEC-SCALPEL §5; audit B2 |
| BL-14 | ADR-184 A2; B315; audit B3 |
| BL-15 | SPEC-SCALPEL §5; the ratio lists `razor-core.js:77-100`; the refusal of an unreadable custom list is a departure (row 38: the prototype falls back to [1], `razor-core.js:77`) |
| BL-16 | B487; B485; audit B5; ADR-225 item 3. **Evidence (the click detector, PR #1103):** Glass horde pad's steps at the control tick come from blade 1's cut (they vanish with `kRule` 0 or with its depth `kRuleAmt` 0) and persist with the spread ramp on (D8): the quantized cut changing mid-cycle, which this requirement's latch addresses; dry run 2 (#42): the duration, rate and pitch of the no-tone-bias measure |
| BL-17 | ADR-216 A4; B460; B484; audit B5, H4; ADR-225 item 3. Under Quantize the cut is set, not ramped (ADR-216 A4 item 2), as BL-17's last sentence states |
| BL-18 | B469; B481; audit B16, B17 |
| BL-19 | SPEC-SCALPEL §7, §9; audit B13; C.16's sources; dry run 2: the 0.003 bound stated relative to `out.level` |
| ED-1 | ADR-222; ADR-189 (D1, D2 aims); SPEC-SCALPEL §8.2; audit B11, B14; ED-1a's source: PR #1103's click detector (departure 43) |
| ED-2 | ADR-222 |
| ED-3 | ADR-222 item 2(b); research note F5. The note's 0.23 is for the lab's ±0.36 saw; on the spec's ±1 saw the same offset is about 0.63 (0.23/0.36), as the verification round found (ADR-226 B); dry run 2 (#30): the mechanism stated as a formula: subtract the corrections' mean, Ā·(fᵢ/f_s)·Σhₑ, with C.16's estimate. A per-edge zero-area table was considered and rejected: its 32-sample compensation would itself add unband-limited steps at both ends |
| ED-4 | ADR-222 item 2(a), (c); research note F3, F4 |
| ED-5 | ADR-222 item 4 |
| ED-6 | ADR-222 item 3; audit B12 |
| VO-1 | ADR-224 item 1; audit H3 |
| VO-2 | ADR-083; B310; ADR-216 A3 item 2; envelope conflicts decision 7; audit V1; ADR-226 item 11 (the faded-held-note rule deleted; tier 1 reads "faded"); ADR-226 B (polyphony 1) |
| VO-3 | ADR-216 item 2, A3 items 1–8; B459; audit V3; ADR-226 item 1 (a pedal-held key counts as held for the wait) |
| VO-4 | B323; B375; audit V2; ADR-225 item 13 (no CPU-adaptive limiting by default, Q3); departure 41 (the STEAL path) |
| VO-5 | ADR-224 item 1; audit V8; ADR-225 item 13 |
| VO-6 | SPEC-SCALPEL §6; ADR-084; ADR-208; audit V4; envelope conflicts decision 5; ADR-226 item 3 (the table; a legato change keeps the strike's velocity: departure 27; the mono velocity ramp: departure 28); dry run 2: a mono re-run reset the start phases, the rotation clock, the sub's phase and the frame mid-note, a waveform jump against QS-4; the lead's recommendation, PROPOSED as X25 pending the human (departure 47) |
| VO-7 | rack contract A5 |
| VO-8 | ADR-084; audit V10 |
| VO-9 | ADR-224 item 2; B303 |
| VO-10 | audit §3.1 |
| EN-1 | B366; B377; audit V11; ADR-226 item 10 (one envelope law for ENV 1 and the modulator bank) |
| EN-2 | ADR-161; envelope conflicts decision 5; VO-3 |
| EN-3 | B377; PA-1 |
| EN-4 | ADR-009; V7 |
| PA-1 | SPEC-SCALPEL §7; B378 F8; audit B6; ADR-226 B (snapping within 10⁻⁶; smoothing in v, the shortest way round on circular parameters) |
| PA-2 | ADR-224 item 4; audit §3.3; ADR-226 B (offsets by note id, cleared at that note's fresh start) |
| PA-3 | ADR-224 item 5; V7; ADR-226 B (the order of same-sample events; the first partial interval) |
| PA-4 | ADR-224 item 5; ADR-225 item 2 |
| PA-5 | B479; rack contract RS-3.6, RS-4.3; ADR-218; audit B7; ADR-225 item 4; ADR-226 B (both settings render from one shared member state; RT-1 sized for it) |
| PA-6 | audit B7; ADR-225 item 4; ADR-226 B (the seats and U^−0.75 ramp over 5 ms) |
| PA-7 | ADR-207 item 2; ADR-213; audit N14 |
| PA-8 | ADR-187 item 4(b) (the dependency probe); ADR-226 B (the live-when table); dry run 2: `phaseLag` live through the onset burst at coupling 0; `phase` and `phaseRandom` live with `retrigger` off (a start with nothing to resume); `crossMod` live at U = 1 (its own neighbour); the sub's `startPhase` and `keytrack` inert under the noise wave; "can" defined. Found while correcting them: `onsetScatter` is live at U = 1 (its entry fade), `panMotion` and `width` at U = 1 depend on each other, and `onsetLock`, `dissolve`, `motionGrain`, `spreadLaw`, `quantize`, `anchor`, `distribution` and `harmonicReach` are inert at U = 1 |
| DT-1 | ADR-224 item 6; charter §Domain; audit N1 |
| DT-2 | module 1.0 bar §3; rack contract RS-7.11; audit V7 |
| DT-3 | SW-14; ADR-197 armor row 4; ADR-226 B (the metric named: harmonic levels within 0.5 dB, event times within 1 ms); dry run 2: the patches and the analysis window named |
| DT-4 | charter §Domain invariant 1; SPEC-SCALPEL §9; B308 H3; B469; audit N2, N6; ADR-225 item 5; ADR-226 item 2 (the Random draw); ADR-226 B (which streams are per note and which per oscillator; the voice's seed; the note counter) |
| DT-5 | charter §Domain; SPEC §5.7 |
| DT-6 | rack contract A2 |
| RT-1 | `h2/README.md` rule 5; rack contract RS-7.7, RS-7.10; audit N12; ADR-225 items 1 and 13 (16 members, 64 voices) |
| RT-2 | SPEC-SCALPEL §7; ADR-226 B (the per-sample edge cap and its overflow count) |
| RT-3 | ADR-197; rack contract RS-7.9; audit N15 |
| RT-4 | ADR-211; ADR-213 item 3; audit N13 |
| OU-1 | SPEC-SCALPEL §10; ADR-221 item 6; audit V13; ADR-226 item 8 (a clip warning plus the master limiter; no peak guarantee) |
| OU-2 | ADR-221 items 1, 2, 4; B489; audit V12. The chord is the bank headroom row's: keys 60, 64, 67, 71 at velocity 0.85, held 3 s (`tools/h2_output_headroom_check.mjs:49, 55`); dry run 2 (#61): a failing preset's translated level is lowered by its shortfall; the 0 dB definition and the new-patch default stand |
| OU-3 | ADR-221 item 3 (a saved state keeps its level); ADR-197 (every default change carries a migration); B445; audit V14, K2; ADR-226 B (B.2's added rows and the `out.level` ceiling of +26 dB, which keeps every stored gain up to 1 at N ≤ 9: 20·log₁₀(1/0.0875) + 5·log₁₀(9) = 21.2 + 4.8 = 26.0 dB) |
| OU-4 | rack contract A5, RS-7.6 |
| SN-1 | SPEC-SUBOSC §1, §2, §4; audit §3.6; ADR-225 items 6, 7; ADR-226 item 5 (the sub's pan) |
| SN-2 | SPEC-SUBOSC §3, §5.2, §10.6; ADR-226 B (the shape formulas) |
| SN-3 | SPEC-SUBOSC §5.1, §5.3; ADR-225 item 6 |
| SN-4 | ADR-190 A6; B452; audit §3.6; ADR-226 item 5 (the noise oscillator's pan); dry run 2 (#42): the slope tolerance |
| IF-1 | ADR-190 A4; ADR-084; ADR-208; ADR-224 item 1; audit §3.1; ADR-226 items 1 (the pedal), 4 (velocity and aftertouch as sources; pressure never to level unless mapped), 6 (all notes off, all sound off); ADR-226 B (CLAP's other expressions); dry run 2: CLAP's volume expression is a linear gain, 0 to 4 |
| IF-2 | B278; B397; B431; ADR-096; audit P1–P3, §3.2; ADR-225 items 7, 8; ADR-226 B (the order: input, gravity, transposition; the 12-TET fallback) |
| IF-3 | ADR-224 items 4, 5; ADR-169 A3; ADR-192; B429; audit §3.3; ADR-226 item 4 |
| IF-4 | ADR-104; ADR-185; ADR-188; ADR-208; rack contract RS-6.6–6.13, A17–A20; audit §3.4; ADR-226 B (the state format named, `version` 1); dry run 2: the state's text syntax for version 1 and version 0; the version-0 example states are supplied by the lead with the reference renders |
| IF-5 | ADR-224 items 1, 2; B303; ADR-226 items 5, 7 (each voice's signal to the filters; RC-1) |
| IF-6 | B225; B402; B376; ADR-221; ADR-203 item 5; audit §3.5; ADR-226 item 5 (mute and solo in the engine); ADR-226 item 8 (the clip latch) |
| IF-7 | ADR-222; rack contract RS-2.8, RS-7.1; ADR-226 item 7 (RC-5) |
| IF-8 | ADR-224 item 3; ADR-190 A9; B408 |
| IF-9 | ADR-191; B445; ADR-187 item 8 |
| IF-10 | ADR-223 item 4; audit §3.10, N21; ADR-225 item 10; ADR-226 B (the observation view's list for suite item 0) |
| IF-11 | B308 H4; B428; ADR-207 item 2; audit §3.11; ADR-226 B (the normalised domain, −∞ at v = 0, the sample-rate guard) |
| QS-1 | ADR-222; research note F1; audit B11; ADR-226 B (per-pitch floors over 20 Hz to 20 kHz: departure 34); dry run 2: round 5's 4-term Blackman–Harris window read an ideal saw as failing (each harmonic spreads about ±3 bins; an ideal float64 additive saw read 0.0 dB at C2 to C8, a rectangular window 233 to 251 dB), and "below −140 dB counts as −140" summed over about 30,000 bins floored near −95 dB; now a rectangular window and a 140 dB cap. The reference design (0.45·f_s, Kaiser β 10: 134.8, 130.0, 123.7, 115.4 dB; QS-2 −1.5 dB) is the dry run's measurement |
| QS-2 | ADR-222 item 2(a) |
| QS-3 | ADR-191; B346; B445 |
| QS-4 | B490; B459; B461; B485; B486; ADR-218; ADR-225 item 12 (the method ratified; p, M and k proposed by the test author with both controls); ADR-226 item 9 (calibrated first on the reference renders). The details beyond the ratified method (the −6 dBFS normalisation, the planted step and its two presets, the must-not-fire set) are the spec author's proposal. Round 4's "0.05 × the render's peak" and "0.05 of full scale" are reconciled as 0.05 of full scale after normalising each render to a 0.5 peak, so a tenth of the normalised peak; ADR-227 (the detector's numbers RATIFIED, the human, 2026-10-16: k 10, p 24, a 10 ms fit window, a 20 ms local window, a 1.5 ms merge, the rest as proposed; `tools/click_detector.mjs` and `docs/research/2026-10-16-click-detector.md` on the `click-detector` branch). The must-fire set follows the research note: round 5 named the Breathing pad, on which the detector cannot see a 0.05 plant (its own sound is made of steps that large), so it is replaced by the note's judged set; those patches are named as beyond a per-sample detector |
| QS-5 | ADR-221 item 4; B489 |
| QS-6 | SPEC-SCALPEL §9; dry run 2: as BL-19 |
| QS-7 | rack contract A5; SPEC-SUBOSC §10.5; RT-4 |
| QS-8 | ADR-197 armor rows 3, 6; ACCEPTANCE L0-13; ADR-226 item 8. **ACCEPTANCE L0-13's "master peak ≤ 1.0" does not bind the new engine:** the engine guarantees no peak; the device's master limiter holds it. Scoping L0-13 is the lead's records task with the human's sanction, as L0-1 was scoped (ADR-187) |
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
| C.1 (Coupling) | SPEC.md §3 (layer 2), §5.1–5.2, §6.3; ADR-007; ADR-056; `h2/engine/swarm.h:134-227` (σ at 198, the targets and smoother at 202-205, the pulls at 207-227); the α term from the legacy core (departure 3) |
| C.2 (Placement) | SPEC.md §3 layer 1; `h2/engine/swarm.h:76-108` (the five distributions, the placement stream) |
| C.4 (Drift) | `h2/engine/swarm.h:144-158` (mode 2, the walk). *Correction to the brief's transcription:* the formula Tᵢ ← Tᵢ − α(Tᵢ − mean) + N(0, σ) is the ensemble's timing correction (C.7), not the drift; the drift is this walk |
| C.5 (Inertia) | SPEC.md §5.3; `h2/engine/swarm.h:228-246`; ADR-225 item 11 (offsets only, the skip kept); the knob-0 finish is departure 40 |
| C.6 (Gravity) | ADR-008; ADR-086 A1; `h2/engine/engine.h:1532-1576`; ADR-225 item 8 (gravity after the tuning, only when its knob is up); dry run 2: the pair step uses the grid's actual interval at every rate |
| C.7 (The ensemble (onset scatter)) | ADR-077; `h2/engine/engine.h:1578-1678`; ADR-226 B (the fresh start, Tᵢ when U changes, the α = 0 bound at μ ± 4 standard deviations, the entry fade at s > 0 on every member and none at s = 0) |
| C.8 (Pan and density) | SPEC-SCALPEL §6, §9; the legacy core's pitch fan (ADR-070, ADR-073; `swarm_core.h:1570-1660`); ADR-064 and ADR-086's amendment; `razor-core.js:733-737, 904` (the balanced table, the spread fan); ADR-226 item 5 (per-oscillator pan as an image shift); departures 29 (`panInvert`) and 30 (the pan-motion order); ADR-226 B (U = 1, the U 10 to 16 rule) |
| C.12 (ENV 1's curve) | B377 (the human: shapeable curves at every stage); the spec author's proposal of the form (round 2), now ADR-226 item 10 (a stage's time is exactly when it ends). The curve range ±2.5 (±2 until round 5) holds the bank's translation at u = 2.303 (departure 36); the Hold stage is new (departure 39). Today's envelope: `h2/engine/engine.h:2190-2232` |
| C.13 (The frame) | ADR-216 item 1 and A1 (ruled); `h2/engine/engine.h:1440-1515` (holdFrame); the start rule is departure 9 |
| C.14 (Cross-mod and feedback) | SPEC-SCALPEL §6; ADR-189 A1 (D3); `h2/engine/engine.h:1896-1912`, `kLoopFc` at 2076 |
| BD-1 | the lead's "Scope of the rebuild" (round 1: one concept per file, named in a musician's words); the audit §1.8 (two phase counters and two centre measurements: the split the rule prevents) |
| BD-2 | audit §1.8 |
| BD-3 | audit §1.5; N18 |
| BD-4 | audit §1.4; N10 |
| BD-5 | B308 H4 |
| BD-6 | B494 (the rebuild's brief: self-documenting and concise); the audit's size estimate (§4.1) |
| BD-7 | ADR-224 item 6; audit §1.3; audit N3–N9 |
| BD-8 | audit §5.5; ADR-226 B (the stage order and the suite-to-stage map); dry run 2: item 11 moved to stage 6 and item 13's render half to stage 7 (nothing renders at stage 1) |
| BD-9 | audit N10; `h2/README.md` rule 7's purpose |
| BD-10 | ADR-223 item 2 (a clean-room build from the spec and the tests alone); the human, 2026-10-16 ("the build should be fresh from the spec"); ADR-226 item 9 (the reference renders go to the test author, not the builder) |
| VO-11 | ADR-225 item 13 (the human asked about polyphony; "voices not sounding cost no CPU"); ADR-226 B (judged on operation counters, not timing) |
| PF-9 | ADR-225 item 13 |
| RC-6 to RC-10 | ADR-229 (round 8): the human asked "are there any other things you anticipate we still need to add to the engine after this which can have proper space left for?" and ruled "add 1–5" on the lead's list: RC-6 the source slot (a formant source, a later blade version's blade-to-blade controls), RC-7 tempo and transport (the arpeggiator and shared quantizer on the 1.0 roster, tempo-synced LFOs, synced glide), RC-8 the GUI's readout path, RC-9 adding parameters without breaking projects (ADR-197's migration rule), RC-10 the quality-tier hook (Q27 kept: no automatic thinning in 1.0). The lead's sixth item, an audio input into the voice, was agreed to be not for now: X27 |
| RC-11 | ADR-229 A1 (round 8): the human asked "should there be any history hooks?" (undo, redo and snapshot history; the module 1.0 bar lists undo under "usable"). The lead's brief listed the morph position, macro values, gravity's settled ratios and the seed as non-parameter state members; in the spec the seed is a manifest row, the morph and macro values are their own systems' state reaching the engine as parameter values, and gravity's settled ratios stay performance state (IF-4 never saves them), so RC-11.1 says so. The brief's "crossfaded as a patch load is" and "never resets voices' phases" are reconciled as RC-11.4: a restore is a change set (PA-1, PA-5, PA-6), not a load |
| ED-1a | PR #1103's click detector (the lead, round 5): today's plain saw base (`base` 2) is rendered naive (36 and 40 detector events at C1 and C2; the band-limited sine→saw base reads none); ADR-222 (every hard step band-limited); X3 (the triangle's corners); departure 43; dry run 2: the detector's ratified numbers must pass this control as they stand |
| VO-12 | ADR-226 item 1 (the human: "Let's add the sustain pedal to the spec"; a sustained note is stolen before held keys); dry run 2: one term for the pedal (held, sustained, gated, 6.4); how the engine knows MPE mode and its master channels |
| VO-13 | ADR-226 item 6 (no panel control; the host's all-notes-off and all-sound-off handled); today's only form is the prototype's panic (`razor-core.js:354`: every voice released) |
| VO-14 | ADR-226 B (voicing, quality and polyphony changed while notes sound) |
| RC-1 to RC-5 | ADR-226 item 7 (the human: "leave room in the spec for the things we know still need to be added … Gain staging in the mixer panel might qualify too, as well as the bend laws"); RC-1 ADR-224 items 1, 2, B303, X13; RC-2 B225, B376, ADR-221; RC-3 Q6, Q7, ADR-225 items 7, 8, the bank's `glide` and `glideAlways`; RC-4 ADR-224 items 4, 5, ADR-226 items 4, 10; RC-5 the rack contract, ADR-203 (the master limiter), ADR-226 item 8 |
| RC-2, RC-5, RC-8, IF-6 (round 9) | ADR-230: the human, answering Sluice's question (its notice seq 2, `integrations/sluice/notice-engine-rebuild-audit.md`): "Let's add the makeup back in before the FX." The default ×4 is exactly the factor ADR-221 took away (0.35 → 0.0875), so 0.14 × 4 = 0.56 = 0.35 × 1.6, today's level ahead of its `tanh`; the level is restored, not the `tanh`'s ceiling. Then: "I want gain staging UI to give up robust control and visuals of the level as it moves through stages, as you included in the mixer lab": the gain-staging view follows `docs/design/mixer-lab.html` (B225, B402: a meter, a peak hold and a latching clip lamp at every stage, the gain-staging line) |
| RC-2 (round 10) | ADR-230 A1: the human, after round 9: "I want makeup to be a control, go with your recommendation on default; the gain staging should allow granular control at all stages. Maybe we can tweak the presets individually, or even add an adaptive makeup option if that isn't too costly." The per-preset and adaptive options are B499 |
| OU-2 (round 12) | ADR-231: the human, "Per-preset make-up for B499, go with your recommendation"; B499's option (a). The measure (BS.1770-4 integrated, ungated, the chord's 3 s at the engine output) is the lead's proposal |
| IF-2, VO-10, RC-3 (round 13) | ADR-232: the human on the glide system and poly glide (2026-10-17). The legacy design this leaves room for: `src/glide_core.h` (the four laws; quantize as a modifier; `qhyst`, `qTime`), `docs/design/bend-lab.html` (its reference), ADR-076, ADR-102, ADR-103 (poly glide; Glide From), ADR-111, ADR-112 (anchored and movable-do quantize) |
| C.0 (Conventions) | the audit §1.8; today's one-offset bridge `h2/engine/engine.h:1342` (φ_S = frac(φ_H + ½)); the control interval ADR-224 item 5; the frequency guards ADR-226 B; dry run 2 (#19): the ramp of gᵢ. Checked by simulating C.1 at SW-4's conditions with the ramp against the step: every anchor reads the same (R 0.965, 0.924, 0.23 to 0.79, 0.04 to 0.92; R_N 0.44 at −0.7 and 0.84 at −1.0) |
| C.3 (Detune laws) | SPEC.md §4; `h2/engine/swarm.h:163-198` (the four laws, ERB at 251, σ at 198); the merged range X11; the natural-frequency floor of 1 Hz ADR-226 B |
| C.9 (The blade) | SPEC-SCALPEL §4.1–4.5 (copied with the edits of departure 23); `reference/scalpel/prototype/razor-core.js:13-60` (waves, modulator shapes, antiderivatives, crush), `163-256` (FM step, the voice, the gate); items 1 to 9: the verification of round 2 against the same lines; item 8's caps departure 6 |
| C.10 (Spreads) | SPEC-SCALPEL §5; `razor-core.js:77-162` (the rule lists, spreadMember, kSpread), `482-502` (the spread laws and the OU step); departures 7 and 8 |
| C.11 (Interplay) | SPEC-SCALPEL §13.1–13.3; `razor-core.js:189, 199` (bite), `239-303` (collide, out, outSerial); the bite law restored in round 5 (ADR-226 B) |
| C.15 (Time constants and grids) | `h2/engine/engine.h:2073-2090` (the steal fade, the loop filter, the frame's rates); ADR-224 item 5 (the control interval); C.16's refresh and smoothing from `razor-core.js:842-860` |
| C.16 (DC removal) | SPEC-SCALPEL §7, §13.4; `razor-core.js:542-600` (dcEst, dcPair), `842-860` (the refresh every 256/44100 s × ⌈J/64⌉, the smoothing at 0.003 per sample, about 7.56 ms at 44.1 kHz), `871-876` (the 8 Hz blocker); ADR-226 B (the BLOCKER: the law stated); departures 32 and 33; dry run 2: FM free's mean over 32 modulator start phases (its cycle mean depends on the modulator's phase); the FM point count F = w·(kk + I·mEff + mEff) (round 5's under-resolved, an error up to 0.0099 per member); every J a minimum; the Sync taper read at eʳ under reflect |

ADR-225's items, as appended to the rows above (the list kept for reading by item): X10 → `unison`, RT-1 (item 1); X16 → PA-4, rate column `A` (item 2); X8 → SW-16, BL-16, BL-17, `motionGrain` (item 3); Q14 → PA-5, PA-6 (item 4); Q16 → DT-4 (item 5); Q21 → SN-1, SN-3 (item 6); Q9 → IF-2, SN-1 (item 7); Q8 → SW-11, IF-2 (item 8); X18 → the rules gate (item 9; section 3 below); Q30 → IF-10 (item 10); X19 → SW-8, C.5 (item 11); X20 → QS-4, 10.1 (item 12); polyphony → `voice.polyphony`, VO-4, VO-5, VO-11, RT-1, PF-5, PF-9 (item 13; Q3 and Q4 with it).

ADR-226's items, likewise: the pedal → VO-12, VO-2, VO-3, SW-11, IF-1 (item 1); phase → `phase`, `phaseRandom`, `retrigger`, SW-10, DT-4, B.1 (item 2); legato → VO-6's table (item 3); velocity and aftertouch → IF-1, IF-3, RC-4, X21 (item 4); pan, mute and solo → 5.3, 5.5, 5.6, C.8, IF-6, SN-1, SN-4 (item 5); panic → VO-13 (item 6); reserved contracts → 7.2, RC-1 to RC-5 (item 7); output peaks → OU-1, QS-8, IF-6 (item 8); the reference renders → 10.1, QS-4, BD-10 (item 9); envelope timing → C.12, EN-1, RC-4, B.1, X22 (item 10); the stealing rule → VO-2 (item 11); the bank's coupling → B.1, X23 (item 12); the negative-coupling start → X24 (item 13).

---

## 2. Departures from today's engine and its prototypes

Each is stated in the spec as plain law or requirement, without this history. Reason first, then where.

| # | the spec says | today's engine or the prototype | why | where |
|---|---|---|---|---|
| 1 | One phase per member, θ, with the blades reading φ = frac(θ + ½) | two phase counters per member, φ_horde and φ_SCALPEL, advanced separately | the two prototypes put the saw's jump at different places; one phase with a constant offset keeps the bank's blade positions and start phases (audit §1.8) | C.0 |
| 2 | Coupling, drift, inertia, spreads and the frame update once per control interval, about 1/1,500 s (29 samples at 44.1 kHz) | swarm tick 16 samples; spreads and frame 32 samples | ADR-224 item 5; every law is in seconds, so only a near-critical regime could move, which SW-4's +0.7 and +0.6 anchors catch (a simulation of C.1 at Δ = 29 samples still reads them) | C.0, C.15, SW-4 |
| 3 | The sync pull carries a phase lag α, in both the mean-field and root-pivot forms | today's engine has no α term (phase lag was never composed: `swarm.h:207-227`); SPEC.md §5.1 states it in the mean-field form; the legacy core has it in both forms | the `phaseLag` row is a keep (audit round 2); its anchor is SW-5; both forms are the legacy core's (corrected in round 5: round 4 called the root-pivot form an extension) | C.1, SW-5 |
| 4 | SW-5's phase-lag anchor at K +1.0, α 10°–30° | ACCEPTANCE L0-9 (60°, 1.8 Hz) | L0-9 was measured in the dynamics lab (24 members, detune 0.2); at SW-4's conditions a 60° lag loses the lock (a simulation of C.1: R swings to 0.29 at K +0.9); at K +1.0, α 10°, 20°, 30° the swarm stays locked and shifts by 1.42, 2.76, 3.94 Hz against S·R·sin α = 1.47, 2.88, 4.16 Hz | SW-5 |
| 5 | Inertia swings members' offsets from the oscillator's pitch only | today's engine swings the whole member frequency, so a glide, a bend or audio-rate pitch is low-passed below about 9 Hz whenever inertia is on | ADR-225 item 11; the skip at knob ≤ about 0.063 is kept; no bank preset sets inertia | C.5, SW-8 |
| 6 | The carrier's frequency is capped at 0.45 × the internal rate in every mode | the prototype caps the cut ratio at 0.45·f_s/fᵢ before the per-blade division, so a narrow per-blade window can run its carrier past the band edge | "never folded" (X2's principle) | BL-4, C.9 item 8 |
| 7 | Quantize: both roundings half away from zero | today's engine already does so (`engine.h:1106-1111`, ADR-184 A2 (2)); the prototype rounds the spread s half up (toward +∞), then the offset half away from zero | the exact mirror of a negative spread (ADR-184 A2) at half-integer spreads; a departure from the prototype only | C.10 item 4, BL-14 |
| 8 | The drift spread law in exact time: p ← p·e^(−θΔ) + 0.289·√(1 − e^(−2θΔ))·z | σ = 0.289·√(2θ·dt) at a fixed 32-sample dt | the same stationary spread (0.289) and correlation time (1/θ) at any control interval; the prototype's form is its first-order approximation | C.10 item 3 |
| 9 | The frame starts each note at ψ if R ≥ 0.25, else at member 0's phase | ADR-216's constants (decided under its delegation) start it at ψ always | below R 0.25, ψ is the ill-conditioned angle the frame exists to avoid | C.13, BL-10 |
| 10 | Each voice and member draws noise from its own stream | the prototype's noise blade draws from the one shared stream, so any note-on changes every sounding voice's noise (B469) | DT-4: a note-on never changes a voice already sounding | C.9 item 4, BL-18, DT-4 |
| 11 | Modulator noise from the member's own stream keyed by the integer | the prototype's `sin` hash | SPEC-SCALPEL §10 ("do not replicate") | C.9 item 5 |
| 12 | The cross-mod and feedback loop filter at 5 kHz | ADR-189 A1's prose says about 4 kHz; the ruled build (`LOOP_FC`) and today's engine use 5 kHz | copied from the build | C.14 |
| 13 | The frame never jumps (D6), a taken voice fades (D7), spread values ramp at `motionGrain` 0 (D8), a quantized cut waits for its cycle start (D9), the edge table (D10), the carrier and scan aims of D1 and D2, the loop filter of D3 | each was an off-by-default switch beside the old behaviour | ADR-223 item 5; the switches retire, the behaviours are requirements; D8's "off" (the human's blind preference, B460) is `motionGrain` 1, the default | SW-16, BL-10, BL-16, BL-17, VO-3, ED-1, C.14 |
| 14 | A linear output stage: 0.14 × 10^(level/20) × the voice sum, no saturation | `tanh(1.6 · gain · y)` at the default gain 0.0875 (ADR-221) | the clipper made chords clip on 71 of 83 presets (B489); 0.14 = 1.6 × 0.0875 is today's small-signal gain, so 0 dB matches today's default level for small signals | 6.9, OU-1 |
| 15 | Members sum with density compensation U^−0.75; the bank translates with +5·log₁₀(N) dB | the bank was voiced with 1/√N | the human's lock (`normExp` 0.75, 2026-10-02); the offset restores each bank preset's level exactly, so its balance (ADR-221 item 2) and its headroom (worst four-note chord 0.815, B489) are today's | C.8, SW-19, B.1 |
| 16 | ENV 1's stages land exactly at their time (C.12) | the prototype's linear attack and exp(−4t/T) decay and release never land and free the voice under 1e-4 | B377's shapeable curves; ADR-226 item 10 (a stage's time is exactly when it ends). New patches default to +1; the bank translates at u = 2.303 with its times × 2.303, which keeps its tails' audible length (departure 36) | C.12, EN-1 |
| 17 | *Withdrawn in round 5.* Round 4's genuinely settled start (a pre-run) is gone | today's engine plays the bank's `phaseMode` 2 (settled) as aligned | ADR-226 item 2 replaced the start modes with Phase, Random and Retrigger (departure 26); the pre-run's CPU spike goes with it | SW-10, B.1 |
| 18 | A member near the band edge fades out | members above Nyquist alias and add DC (B378 F2, never ruled) | "never folded" | SW-13 |
| 19 | Pitch arrives per note id, already bent and glided; glide is the pitch system's | today's engine keys voices by MIDI note and glides inside the engine | ADR-224 item 1; the audit's merge of `glide` into the note lane | IF-2, VO-6 |
| 20 | The sub integrates its own phase; ENV 1 acts after the sub's tone | SPEC-SUBOSC §2: the voice hands the sub oscillator 1's phase; the envelope before the tone | ADR-225 item 6 ratifies both: a swarm has no single phase; one envelope position for every source. SPEC-SUBOSC itself is not edited | SN-1, SN-3 |
| 21 | Polyphony 1 to 64, default 16, preallocated; voices not sounding cost nothing | a pool of 8 (default `poly` 6), 16 voices as two engines | ADR-225 item 13 | 5.2, VO-5, VO-11, RT-1, PF-5, PF-9 |
| 22 | Unison 1 to 16 | 9 members fixed in the prototype; 1 to 32 in the legacy swarm | ADR-225 item 1 | 5.3, RT-1 |
| 23 | The blade text of Appendix C: SPEC-SCALPEL §4.1–4.5, §5 and §13.2–13.3 copied with these edits: section numbers renumbered to C.9.1–C.9.5; §4.1's carrier-cap sentence restated per item 6; §4.4's "Verified: largest jump < 0.002" sentence dropped (BL-7 holds the bound); §5's drift σ restated per item 8; §13.2's "as in v1" and "the v1 parallel sum" reworded; §13.3's "PolyBLEP carrier-wrap scan" reworded to the edge correction's carrier-wrap detection. **Also (listed in round 5, from a sentence-level diff against SPEC-SCALPEL):** §4.1's bypass reads the effective width, and its Hz ratio divides by max(fᵢ, 1 Hz); §4.2's modulator phase `m·eʳ` reads `mEff·eʳ`, its mod-rate sentence is restated as `mEff = modRate` (a multiple of fᵢ, as BL-5) or `modRateHz/max(fᵢ, 1 Hz)`, and "under collision, I is the bitten index" is added; §4.3's `Nyq_OS` reads `Nyq` (C.0 defines it at the internal rate), "morph" reads "shape", and the hash wording reads "per-integer values" (departure 11); §4.4 gains w_e = w (w/2 under reflect) and "flips" reads "would flip"; §4.5's "~120 dB" reads "about 120 dB"; §5's "note-on" reads "fresh start", "a retriggered sounding note" reads "a re-struck sounding voice", "random slots" reads "draws"; §13.1's "all four only apply with blade 2 on" is moved into C.11; §13.2's "verified to machine precision" is dropped (BL-11 holds it), "lerp" reads "blend", λ is named `upperHears`, "voice" reads "output", and the parallel sum is written out; §13.2's "Held state: idx, val, pv per blade state" is dropped (an implementation note); §13.3's "tick" reads "sample", its bite sentence carries the §13.1 law **I_b = I·(1 + 4·colB·ov)** and fold drive 1 + 2·colB·ov (dropped in round 4, restored in round 5), and "the fields are cleared once" reads "they cost nothing" | the protected SPEC-SCALPEL text | the spec states laws as themselves, without attribution (the human, 2026-10-16) | C.9, C.10, C.11 |
| 24 | The ensemble's waits do not "heal by sustain" | — | round 1 of the spec said they did; that was a lab candidate (B370), not the law; withdrawn in round 2 | C.7 |
| 25 | The bank's enumerations map by meaning: waves 0 sine, 1 triangle, 2 saw, 3 square, 4 reverse saw, 6 sine→saw; spread law 0 gradient, 1 random, 2 alternate, 3 swarm, 4 drift; `phaseMode` 0 random, 1 aligned, 2 settled | the prototype's stored ids (`parameters.json` lists `phaseMode`'s options out of order) | read from the prototype's code and the bench's controls | B.1 |
| 26 | Per oscillator: Phase (degrees), Random (%, each member's start the offset plus a uniform draw spread by the amount) and Retrigger (on: restart at each note; off: resume) | the bank's `phaseMode` (0 random: a draw per note from the swarm's stream, `swarm.h:121-131`; 1 aligned and 2 settled: phase 0) and `keepPhase` | ADR-226 item 2 (the human: "replace phase scatter with random % on each oscillator as in other synths like serum. Maybe also add a degree control"). The bank: random → Random 100; aligned and settled → Random 0; Retrigger on; `keepPhase` 1 → Retrigger off (B.2) | 5.3, SW-10, DT-4, B.1, B.2 |
| 27 | A legato change keeps the strike's velocity | today's engine takes the new note's velocity (`engine.h:1341`, `v.vel = vel` on every start, legato included) | ADR-226 item 3, as the records rule it | VO-6 |
| 28 | A mono re-run moves the level from the old velocity's to the new over 5 ms | today's engine steps it (`engine.h:1341`) | a level jump is a click (ADR-226 B) | VO-6, C.15 |
| 29 | `panInvert` mirrors every image left for right | the legacy core's invert reverses the pitch fan's distances (centre and edges swap, `swarm_core.h:1630`) and acts only there | one definition across every pan law (ADR-226 B) | C.8 |
| 30 | Pan motion moves the seat before width scales it, so width 0 stays mono | the legacy core adds the motion after width (`swarm_core.h:909`) | SW-18's "width 0 is mono" | C.8, SW-18 |
| 31 | Pan per oscillator (an image shift, seats stopping at the edges), pan on the sub and the noise oscillator, mute and solo per source | none in today's engine (one swarm, no per-source gate) | ADR-226 item 5 (the human: "Per-oscillator pan is necessary"; "We definitely need to give each of the oscillators a mute/solo control") | 5.3, 5.5, 5.6, C.8, IF-6 |
| 32 | The DC blocker runs per oscillator per voice, from 0 at the voice's fresh start, only with cross-mod, feedback or a collision amount | the prototype runs one blocker on the instrument's summed output (`razor-core.js:871-876`), and `dcMode` 1 keeps it always on | a blocker on the sum couples the voices (a note-on would move every voice's offset, against DT-4); `dcMode` is not translated | C.16 |
| 33 | The DC estimate's smoothing is a time constant, 0.00756 s | the prototype's 0.003 per sample (`razor-core.js:858`), 7.56 ms at 44.1 kHz only | SW-14: no per-sample constants | C.16, C.15 |
| 34 | QS-1's floors per pitch (110, 104, 98, 93 dB at C2, C4, C6, C8), judged over 20 Hz to 20 kHz; 20 kHz to Nyquist reported | round 4: 110 dB at every pitch, over 20 Hz to Nyquist | the builder's dry run (ADR-226 B) swept 32-tap minimum-phase designs and reached at best 112.5, 106.5, 100.1, 95.9 dB with QS-2 met; QS-2's cutoff parks aliases above 20 kHz, which a Nyquist-wide count would judge. The floors sit about 2.5 dB under the best design, to be raised on measurement | QS-1, ED-1 |
| 35 | The bank's coupling is rescaled in translation | the bank was voiced under a pull of K·(1.5·Δf_max + 3 Hz) (linear in K, a 3 Hz floor; `cScale` 1 makes the floor f/110), where C.1 pulls 4K\|K\|·σ; as is, the locked presets would not lock | ADR-226 item 12 (PROPOSED; confirmed by ear in the preset round, X23). K′ = sign(K)·min(1, √(\|K\|·(1.5·Δf_max + 3·s)/(c·σ))) at f = 110 Hz, c = 4 (12 for K < 0). Simulated (C.1 against the voiced pull, by the spec agent's scratch script, not committed; start aligned, no drift, 110, 220 and 440 Hz): Locked horde K′ 0.907, R 0.946 (as is 0.36); Anchored horde 0.996, R 0.965 (0.40); Undertone cathedral 1.0, R 0.965 (0.14); Screamer, three members, 1.0, R 0.966 (0.897); Hollow square lead 1.0, R 0.966. At other pitches C.1 keeps the regime; the bank's 3 Hz floor had loosened the pull above 110 Hz and tightened it below | B.1 |
| 36 | The bank's ENV 1 decay and release × 2.303 with curve +2.303 | the bank's e^(−4t/T) stages never land; a release frees the voice at 10⁻⁴ absolute (`engine.h:2232`), at (T/4)·ln(L/10⁻⁴): 2.1·T from L = 0.45, 2.3·T from L = 1 (the lead's "about 2.1 ×") | ADR-226 item 10 (PROPOSED, X22): C.12 at c = ln(10⁴) over 2.303·T is the bank's curve within 10⁻⁴ of full scale, landing at 2.303·T; the tails differ only below −80 dB. C.12's curve range widened from ±2 to ±2.5 to hold it | B.1, C.12 |
| 37 | The bank's blade envelopes become ENV 2 and ENV 3 in a one-shot mode | they ignored key-up: attack then decay to their end whatever the gate (`engine.h:2207-2224`) | ADR-226 item 10 (PROPOSED, X22): one envelope law (C.12) for ENV 1 and the bank; one-shot keeps their swells on short notes | RC-4, B.1 |
| 38 | An unreadable custom ratio list is refused and reported; the previous list stays | the prototype falls back to [1] (`razor-core.js:77`), so a typo silently flattens the rule | PA-7: values checked where they enter | BL-15 |
| 39 | ENV 1 has a Hold stage (default 0) | today's ENV 1 is attack, decay, sustain, release (`engine.h:2190-2232`) | B377's envelope work (the human's shapeable stages); 0 reproduces today's | 5.2, C.12 |
| 40 | A swing in flight when the inertia knob drops to the skip finishes at the knob-0 law | today's engine snaps each member to its target at w ≤ 0.001 (`swarm.h:230-232`): a frequency jump when the knob is turned down mid-swing | SW-8: the frequency never jumps | C.5, SW-8 |
| 41 | At a full cap the device may let a note take the oldest held voice by the steal fade (VO-3): the old sound fades 5 ms, the note waits | today's STEAL releases the oldest held voice into the cull's 8 ms fade and starts the note at once in a free slot; a third policy replaces a voice at once (`engine.h:207-245`) | one fade rule for every take (VO-3); replace-at-once is a click | VO-4 |
| 42 | The sustain pedal; all notes off releases, all sound off fades in 5 ms | neither exists; the prototype's panic releases every voice (`razor-core.js:354`) | ADR-226 items 1 and 6 | VO-12, VO-13 |
| 43 | Every discontinuity of the base wave is band-limited (ED-1a) | **the click detector (PR #1103) found today's plain saw base (`base` 2) rendered naive:** 36 and 40 detector events at C1 and C2, where the band-limited sine→saw base reads none | the lead's round-5 addition; ADR-222's every-hard-step aim | ED-1a, ED-1 |
| 44 | A held key is never taken ahead of a tail | round 4's VO-2 let a faded held note at sustain 0 count as a tail | ADR-226 item 11 (the human: "delete the stealing rule") | VO-2 |
| 45 | Velocity and aftertouch are modulation sources mapped by default; pressure never reaches the level unless mapped | today's engine scales level by velocity only (`benvVel` and the voice's `vel`); no aftertouch | ADR-226 item 4; the destinations are X21 | IF-1, RC-4 |
| 46 | Each control interval's new member frequency is reached by a linear ramp across the following interval | today's engine sets `eff` once per 16-sample tick (`swarm.h:228-246`) and holds it | SW-17: automating coupling or detune never steps a frequency (dry run 2, #19); every SW-4 anchor reads the same under the ramp (C.0's source row) | C.0, SW-17 |
| 47 | Ruled (X25, ADR-228): a mono re-run follows `retrigger`: on, it restarts and crossfades over 5 ms from the old state; off, only ENV 1 re-runs; in legato the phases always continue | today's mono retrigger keeps the swarm and the ensemble draws and re-enters each member's envelope (`engine.h:1352-1356`): the retrigger-off behaviour | the lead's recommendation, the human being asked; a restart without the crossfade is a waveform jump (dry run 2) | VO-6, RT-1 |

**Corrections made to the lead's transcription of the laws (round 2):** the composed engine has no α term (row 3); σ is the RMS spread of the natural frequencies (after the detune law and drift, before coupling); the formula given as the drift walk is the ensemble's timing correction (C.7), and the drift is a leaky walk (C.4); the root pivot has no R factor and anchors the splay on the root member; gravity's "new chord" is no chord reset (each newly struck note starts at its played pitch; held notes keep theirs), with |e| ≤ 35 c and pairs applied in sorted order; the ensemble's entry fade is a one-pole of τ = max(2 ms, attack) on every member; the loop filter is 5 kHz (row 12). In round 3 the critic corrected SW-5 (row 4).

**Corrections made in round 5 (ADR-226 B):** the collision-bite law, dropped from C.11 in round 4, restored (I_b = I·(1 + 4·colB·ov)); C.9.4 under reflect uses w_e = w/2, as C.9 item 7; ED-3's figure 0.63 (round 4 carried the lab's 0.23); row 3's framing (the legacy core has α in both forms); row 7's framing (today's engine already rounds half away); the departures' row 17 withdrawn; the source rows ED-3, OU-3, BD-1, BD-6, SW-1, SW-10, C.12 and QS-4 corrected as above.

**Corrections made in round 6 (dry run 2):** QS-1's measuring rule (rectangular window, a 140 dB cap: round 5's rule failed an ideal saw); BL-7's measure (round 5's failed a correct crush); PA-8's rows; C.16's FM free and FM point count; BD-8's stage map; QS-4's must-fire set (round 5's named a patch the ratified detector cannot judge); the detector's numbers now ratified (ADR-227). Departure 34's floors are unchanged; with the corrected rule they sit well under the reference design's measurement and may be raised.

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
| the frozen earlier ports used as test references | `h2/cores/scalpel/razor_core.h`, `h2/cores/swarm/swarm_core.h`, `h2/cores/swarm/force_core.h`, `h2/cores/swarm/glide_core.h`, `h2/cores/swarm/lift-ledger.json` | 5 | 4,117 | they exist only to hold parity (round 4 read 4,041: the ledger's 76 lines were left out) |
| the parity tools | `tools/h2_engine_parity_check.cpp` (and its contraction-on control build), `tools/h2_engine_render.mjs`, `tools/labharness/composed_engine_check.mjs`, `tools/labharness/divergence_ledger_check.mjs`, `tools/parity_floor_check.py`, `tools/h2_libm_count.py`, `h2/engine/libm-calls.darwin-arm64.appleclang-16.txt`, `tools/h2_scalpel_parity_check.cpp`, `tools/h2_scalpel_render.mjs`, `tools/h2_scalpel_stream.h`, `tools/h2_swarm_parity_check.cpp`, `tools/h2_swarm_lift_check.cpp`, `tools/h2_swarm48_check.cpp`, `tools/h2_lift_check.py`, `tools/measure_h2_scalpel.cpp` | 15 | 5,656 | parity is no longer a goal (ADR-223 item 3) |
| the engine-coupled tools round 4 left out | `tools/h2_engine_stream.h` (586), `tools/h2_engine_hooked_replay.h` (154), `tools/h2_engine_fz_probe.cpp` (339), `tools/h2_engine_fz_probe.py` (138), `tools/swarm_sr_parity.h` (158) | 5 | 1,375 | they drive or compare today's engine or its ports; found by the verification round (ADR-226 B) |
| the divergence-switch machinery | `docs/port/divergences.json`; the switches D1 to D10 (D9 and D10 parked on their branches, which are left as they are) | 1 | — | D1 to D10 become plain requirements of the spec or are dropped (section 5) |
| the twin's check page | `docs/design/composed-engine-check.html` | 1 | — | it checks the twin against its prototypes |
| **removed in all** | | **32** | **≥ 15,821** | |
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
- **The batches carry at least:** the ten presets whose sound changed most beyond level under ADR-221 (Clockwork, Orbiting cuts, Ring of sines, Crushed bells, Drifting cuts, Anchored horde, Two-formant vowel, Vowel choir, Golden bells, Cross-mod shimmer), to hear whether the old output saturation's grit mattered; Glass horde pad and the Breathing pad for the grain (SW-16) and the crackle (BL-16; the detector's attribution is under BL-16 in section 1); every plain-saw-base preset at low notes (ED-1a); the coupling-rescaled presets (Locked horde, Anchored horde, Undertone cathedral, Screamer, Hollow square lead: X23) and the negative-coupling presets' start, A/B (X24, ADR-226 item 13); the tails under the envelope translation (X22).
- **The reference renders (ADR-226 item 9).** A frozen set of renders of the bank from today's engine, with each render's parameter metadata, is produced by the lead for the test author (the spec's 10.1 and QS-4 call it "the reference renders"); the builder never receives it. The click detector is calibrated on it first.
- **Loudness (the spec's OU-2 wins over this):** the bank at its translated levels is within ±1 dB on average of today's engine at its ADR-221 default, as the level-matched A/B reports; a shortfall is reported.
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
| S13 start phase | RULING | SW-10; X1, ruled by ADR-226 item 2 (Phase, Random, Retrigger) |
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

Rows that ADR-225 settled since: V8 the pool (item 13: 1 to 64, default 16); S11 inertia (item 11); B5's micro-motion (item 3); B7 stepped keys (item 4). ADR-226 since: S13 the start phase (item 2); V1 the voice law (item 11, and the pedal, item 1); V4 legato (item 3); V13 the output (item 8); B8 the blade envelopes' translation (item 10).

---

## 6. The open questions' history

The audit (§3.12) listed 31 interface questions; the spec added X1 to X24 (X21 to X24 in round 5).

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

**Ruled by ADR-226 (13)**, the verification round's questions, in the human's words where the records give them:

| # | question | ruling | ADR-226 item |
|---|---|---|---|
| (new) | the sustain pedal | in the engine; a key released under the pedal keeps sounding and is a tail (taken before held keys); pedal-up releases ("Let's add the sustain pedal to the spec") | 1 |
| X1 | the start phase | Phase (degrees), Random (%) and Retrigger per oscillator; scatter and the settled start replaced ("replace phase scatter with random % on each oscillator as in other synths like serum") | 2 |
| (new) | legato note changes | the swarm and blades keep running; the strike's velocity is kept | 3 |
| (new) | velocity and aftertouch | modulation sources, mapped by default in the init patch and the factory presets unless a preset has a reason not to; the destinations proposed (X21) | 4 |
| (new) | pan, mute and solo | per-oscillator pan ("necessary"); per-oscillator mute and solo in the engine; each voice's signal to the filters once they exist | 5 |
| (new) | panic | no panel control; the host's all-notes-off and all-sound-off handled | 6 |
| (new) | room for what is coming | reserved contracts for the filters, the mixer's gain staging, the pitch system with its bend laws and glide, the modulator bank, the FX rack | 7 |
| (new) | output peaks | "clip warning plus master limiter"; no peak guarantee in the engine | 8 |
| (new) | test data | the reference renders with their parameter metadata, for the test author only | 9 |
| (new) | envelope timing | a stage's time is exactly when it ends; the bank's translation and the modulator bank's envelopes explained to the human before they are fixed (X22) | 10 |
| (new) | the faded-held-note stealing rule | deleted ("delete the stealing rule") | 11 |
| (new) | the bank's coupling | rescaled in translation, provisionally ("probably rescale"; X23) | 12 |
| (new) | the negative-coupling presets' start | deferred to the preset round, A/B (X24) | 13 |

**Dropped as already ruled by earlier records (6):** Q4 (one pool, by ADR-224 item 1; since ADR-225 item 13 the pool is 1 to 64, default 16); Q5 (chords as copies, post-1.0 by ADR-190 A2); Q27's per-preset part (quality saved per preset, ADR-191 and B445); Q22's allocation and ENV 1 part (ADR-224 item 1); X14 (no per-source ENV 1 opt-out, B370); Q3 (a fixed count, no CPU-adaptive limiting by default, ADR-225 item 13).

**Still open, with the reasons the recommendations rest on** (the spec's section 11 carries only what is built until each is ruled). Since round 8 the spec lists 0 before the build (X25 ruled by ADR-228), 25 during and 9 after (X26 in round 7, X27 in round 8) (round 5: X1 ruled; X21 and X22 added during, X23 and X24 after), and this file lists them the same way; the spec does not carry Q30's prototypes part, which concerns the repository, not the build (keep `reference/` as protected history; it is listed here only).

**The spec's appendix letters since round 4:** Appendix A is the 12 envelope presets as input data, Appendix B the bank and version-0 state formats with their translation, Appendix C the laws; the round-3 traceability appendix is this file's section 5.
### 6.0 To be ruled before the build (0)

**X25, ruled by the human (ADR-228):** "I think the mono thing should be a toggle. To me, legato implies the phase continues but it could also be a separate toggle." The lead's reading: in mono `retrigger` governs a re-run (on, a 5 ms equal-gain crossfade to the restarted state; off, the phases continue and only ENV 1 re-runs, the new velocity ramped); in legato the phases always continue (ADR-226 item 3). A separate legato-phase toggle is not added now: X26, after the build, with no recommendation to add it. The row below is kept as the history.

| # | question | recommendation | reason | depends |
|---|---|---|---|---|
| X25 (RULED, ADR-228) | A fresh start on a sounding voice (a mono re-run; legato's first note over a tail): what re-runs, and how without a click. | The lead's: `retrigger` governs it; on, the restart crossfades over 5 ms with equal gain from the old state; off, the phases run on and only ENV 1 re-runs. | Dry run 2: the round-5 table reset the phases, the rotation clock, the sub's phase and the frame mid-note, which QS-4 forbids. | VO-6, RT-1 |

### 6.1 To be ruled during the build, before the stage that needs them (25, plus Q30's repository part)

| # | question | recommendation | reason | depends |
|---|---|---|---|---|
| Q2 | MPE: which expressions reach the swarm and the blades. | Per-note tuning into the note's pitch; pressure, timbre and slide as per-voice mod sources; pressure never to level unless a route maps it (ADR-226 item 4; its default route is X21). | The note-id voice already gives MPE its plumbing; routing is the matrix's. | IF-1 |
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
| Q31 | The 19 deferred rows and envelope decision 1. | **Keep:** `onsetLock` and `dissolve` ("the functionality needs to be present"); `inertia` (Q6); `onsetScatter` and `timingCorrection` (Q13). **Drift mode:** one mode, the walk (C.4) ("pick the best one and lock it in"). **Lock:** `basin` 35 c; `panCurve` 0.5. **Cut from 1.0:** `freqGlide` (Q6); `attackScatter`, `relScatter`, `voiceEnv` (the human could not hear them); `topo`, `mu`, `balance` (the human asked to test whether two-cluster is worth it with blades; out until it wins a test); `beatMult` and the tempo-grid law (post-1.0, the host tempo input reserved); `superMode` and `mono` (the stereo lab's; width 0 is mono); `bassMono` (a device output policy). **`vol`:** `out.level` in dB (Q20). **Envelope decision 1:** C.12 (time means time to finish; per-stage curves; decay and release default +1 for new patches; the bank at +2.303 with its times × 2.303 (X22); a Hold stage; a velocity amount; the curve range ±2.5). | The human's own notes on each row (2026-10-02); fewer, clearer controls for 1.0. | 5.2, 5.3, SW-7–SW-12, EN-1 |
| X2 | Members near the band edge: fade out or fold. | Fade out between 0.40 and 0.48 of the internal rate (SW-13); carriers and the sub are capped instead. | Folding and DC from members above Nyquist were measured on legal patches (B378 F2). | SW-13 |
| X3 | Corners (triangle points, mirrored centres, Crush slew corners): correct them in 1.0. | Correct triangle base and carrier corners with a corner table built like the step table; leave Crush slew corners. | Uncorrected triangle corners read −66 dB at C4 to −31 dB at C8; a table reads below −149 dB (research note). | ED-6 |
| X4 | Should notes share one noise stream. | No: per voice and member (BL-18, DT-4). | B469; notes interacting confounds every with/without comparison. | BL-18, SN-4 |
| X5 | The noise blade's large steps: sound or click. | Keep the steps as the sound; band-limit them like every edge. | B481 is unheard; a sample-and-hold source steps by design. | BL-18 |
| X7 | The frame after a crossing. | Treat a flip of ψ by exactly half a cycle as no change; turn back at the rate limit otherwise. | ADR-216 A1 left it to the human; a crossing is a symmetry, not a movement. | BL-10, C.13 |
| X11 | Detune's range (the merged `spread` row). | A stepped Range: ×1, ×12, ×24. | The human: "expand detune or add the toggle"; a toggle keeps the fine range fine. | 5.3, SW-3 |
| X12 | The default patch: blade 1 off. | Yes: an untouched patch is the swarm saw. | The design's own rule; a blade is a choice. | 5.4 |
| X13 | The filter slot: how many filters per voice and how sources route to them. | Settled by the filters' spec; the slot supports two filters, each source routed to either, both or neither, serial or parallel. | B303 needs per-voice filters; the mixer's taps already name filter placements (B225). | IF-5, 4.2 |
| X17 | Density compensation, and the bank's balance. | Keep the human's lock (U^−0.75) for new patches; translate each bank preset with a level offset of +5·log₁₀(U) dB, which restores its √U-normalised level exactly, so the bank's balance between presets is kept (ADR-221 item 2) and its headroom is unchanged from today's measurement. | *Corrected from round 1*, which said offsets would break headroom: they restore today's absolute level, which already passes OU-2 (a) (worst 0.815). | SW-19, App. B |
| X15 | The noise oscillator's design (no lab yet, B452). | Build the minimum of SN-4 now; revisit after its lab. | ADR-190 A6 put a plain noise oscillator in 1.0; its design lab has not started. | SN-4, 5.6 |
| X21 | The default destinations of velocity and aftertouch (ADR-226 item 4). | Velocity → level through `env1.velocity` (1, level proportional to velocity). Aftertouch (channel and per-note pressure) → `coupling` on both oscillators, +0.25 of its range at full pressure. | Coupling is the instrument's own gesture (a held note locks tighter as it is pressed) and cannot silence a note; level is the one destination pressure must not reach unmapped. Alternatives for the human: blade 1's depth, or the cut. | IF-1, RC-4 |
| X22 | The bank's envelopes in translation (ADR-226 item 10, PROPOSED). | ENV 1 decay and release × 2.303 with curve +2.303; the blade envelopes as ENV 2 and ENV 3 in a one-shot mode. | Keeps each tail's audible length and each blade swell on short notes (departures 36, 37); the lead explains it to the human first. | B.1, RC-4 |

### 6.2 Can be ruled after the build (9)

| # | question | recommendation | reason | depends |
|---|---|---|---|---|
| Q23 | Shriek as a fixed post-stage per oscillator, or a rack slot. | A rack slot; the engine offers no insert. | Keeps the engine's boundary small (IF-7); nothing in the engine changes either way. | IF-7 |
| Q26 | Which width candidates are in 1.0, and the mono-fold guarantee. | In the engine: per-member pan only (SW-18), no polarity inversion; every other candidate is post-voice and the stereo lab's; mono fold reported (QS-10) until the lab sets a bound. | ADR-224 item 3 put true stereo after the voice. | SW-18, IF-8 |
| Q29 | Is an M1 or Intel min-spec measurement a precondition for freezing the CPU row. | Not for building; yes before the CPU row is frozen for release. | Every min-spec figure rests on the assumed ×1.5. | PF-2 |
| X6 | Does a sound-change ledger continue against the new engine's own goldens. | Yes: every change after the goldens freeze is recorded with its A/B and the human's word. | ADR-220 item 1 names the ledger as part of correctness. | 10.2 |
| X9 | A twin wider than half a cycle under the cut latch: give the twin its own latch. | Not in 1.0; the edge stays stated. | No bank preset uses it (B487's critic). | BL-16 |
| X23 | The bank's coupling rescale (ADR-226 item 12, provisional). | B.1's rule at 110 Hz. | It restores the lock on the locked presets (departure 35); the human confirms it by ear in the preset round. | B.1 |
| X27 | An audio input into the voice (an envelope follower as a modulation source, a vocoder-style input) (ADR-229). | Recorded; no space reserved now. | The human agreed it is not for now. | — |
| X26 | A separate toggle for legato's phase continuity (ADR-228). | None: not built for 1.0 unless ruled. | The human: "it could also be a separate toggle"; legato already keeps the phases running (ADR-226 item 3). | VO-6 |
| X24 | The start of the bank's negative-coupling presets (ADR-226 item 13). | B.1's phase rule (Random 0 or 100 as stored). | Deferred by the human to the preset round, judged A/B. | B.1 |

---

## 7. The spec's rounds

| round | commit | what moved |
|---|---|---|
| 1 | `20cd9b2` | the first draft from the B493 audit and ADR-224 |
| 2 | `8d3b792` | the human: "copy the laws"; the laws became normative (Appendix C); the critic's HIGH and MEDIUM findings closed; the questions sorted before / during / after the build |
| 3 | `78214e9` | the critic's re-read: SW-5's anchor at a lockable K; inertia's scope and the detector's method put to the human |
| 4 | `7d5f7b6` | ADR-225's thirteen rulings folded in; the spec split from this file, so the builder's text names no old file, prototype or engine (the human: "the build should be fresh from the spec") |
| 5 | `2f1a389` | ADR-226's thirteen rulings and the verification round's findings: the pedal, Phase/Random/Retrigger, the legato table, velocity and aftertouch as sources, pan, mute and solo, all notes off and all sound off, the reserved contracts (7.2), output peaks, the reference renders, the envelope timing and translation, the stealing rule, the coupling rescale; the DC law (C.16) and the collision bite restored; every base wave band-limited (ED-1a, from PR #1103's detector); this file's corrections and departures 26 to 45 |
| 6 | `b277634` | the second clean-room dry run: QS-1's measuring rule and a reference table design; BL-7's measure; X25 (the re-run crossfade, PROPOSED, before the build); PA-8's rows; C.16's FM laws; BD-8's stage map; the detector's ratified numbers (ADR-227); the state's text syntax; the earlier open items #18, #19, #25, #29, #30, #38, #40, #42, #51, #52, #61; departures 46 and 47 |
| 7 | `22ec5ea` | X25 ruled (ADR-228): in mono `retrigger` governs a re-run; in legato the phases always continue; X26 (a separate legato-phase toggle) after the build |
| 8 | `c9068cb`, `6798521` | ADR-229: five more reserved contracts, RC-6 (the source slot), RC-7 (tempo and transport), RC-8 (the GUI's readout path), RC-9 (adding parameters later), RC-10 (the quality-tier hook), with requirements RC-6.1 to RC-10.4 and suite items 18 to 22; ADR-229 A1: RC-11 (history support), RC-11.1 to RC-11.4 and suite item 23; X27 (an audio input) after the build |
| 9 | `ad6e898` | ADR-230: the make-up gain before the FX rack (RC-2, RC-5, IF-6); the gain-staging view (RC-2); the meters in the snapshot (RC-8) |
| 10 | `035b299` | ADR-230 A1: the make-up gain is a user control (default ×4); a gain control at every stage (RC-2) |
| 11 | `2a96714` | §4.2's diagram shows ADR-230's make-up gain between the output and the FX rack (found while scoping B500) |
| 12 | `ee90144` | ADR-231: OU-2 matches each bank preset's chord to the bank's median loudness (BS.1770-4, ungated), then applies its headroom rule |
| 13 | `9fcfb8a` | ADR-232: room for the glide system: IF-2 (a pitch at the note-on's sample applies from the first sample; a waiting note's pitch keeps moving), VO-10 (the pitch system consumes the lifecycle), RC-3 (what the pitch system brings) |
| 14 | `3ebb9f6` | ADR-237 items 5 and 9: C.12's elapsed fraction is the stage's own; VO-2's age runs from the note's start; VO-5 releases the oldest first; B.1's spread comes from B.1 alone; old patches' glide and blade envelopes are preserved (RC-9.3) |
| 15 | `ceb0e74` | ADR-238: SW-4's 7th harmonic as power, gated at 4.0; the sub's square = its pulse at 0.5 (SN-2); PA-8's blade-2 rows under `fmSource`/`spreadSource` |
| 16 | this change | ADR-239: raised-cosine fades and crossfades everywhere (C.0); PA-1's smoother second-order (two 6 ms one-poles); a note's onset reported, not judged (QS-4) |
