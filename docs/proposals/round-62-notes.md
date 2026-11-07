# Spec round 62: the modulation system — notes for the lead (B637)

> **Origin.** horde lead session, 2026-11-06. The human, by poll: "Write the mod spec round". Drafted by an agent the lead dispatched, from the ruled draft `docs/proposals/modulation-spec-draft.md` (#1424) and ADR-371 (and A1), ADR-372, ADR-375 (and A1), ADR-377 and ADR-378 (the travel stage), with ADR-266, ADR-330, ADR-342, ADR-349 and ADR-374 where they bear. **Revised the same day** after a critic's review of `d8693366` ("rework") and the human's answers to the open questions (to be recorded as ADR-379), on top of round 61 (`origin/b631-round-61` at `7acea9c6`, merged into this branch). ROADMAP B633, B637.

**Second critic pass (approved with notes) and ADR-379 A1, same day.** Merged round 61's `5f8315b6` (MO-22's escaped bars) and applied the notes and the human's three further answers; see §11.

**Status: DRAFT for the lead.** The round adds **Appendix E** to `specs/SPEC-ENGINE.md` and touches the body where modulation crosses the engine and the resolver. Two items stay **[OPEN]**, each decided by a lab that is testing it. Every **[Xn]** is built as written until ruled.

## 1. What the round adds

**Placement.** A new **Appendix E, "The modulation system (normative)"**, after round 61's Appendix D (D.8). Like the resolver, it is a part the clean room builds beside the engine, with its own calls (IF-13).

| rows | what |
|---|---|
| MS-1 to MS-7 (E.0) | three parts and who computes what; clock and cadence ("held from the boundary"); reading sources on the outside path one interval late; the outside path is control rate; determinism; real-time safety; reset and load |
| MS-8 to MS-11 (E.1) | the source table (26 ORBITAL sources with acceleration; ψ relative to the frame); keyboard sources read from the shell; macros, X and Y; aftertouch's default route |
| MS-12 to MS-23, MS-41, MS-42 (E.2) | destinations; the route record; curves; contributions and Control; one route per pair; depth wires; the cap; where routes live; summing with the Control input; rates; quantize by per-key offsets (IF-2 for voice pitch only); undo. **MS-41, the two paths by rule. MS-42, a new voice modulated from its note-on sample.** |
| MS-24 to MS-29 (E.3) | per-corner routes fade with the corner; route weights (the resolver owns every pick; a source's keys use its on corners; own one-pole); parametric, not a sound crossfade; modulator settings are keys of the morph; Exempt uses corner A's routes; the morph position (global LFOs and the 2-D random only) and Save under a position route |
| MS-30 to MS-35, MS-44 (E.4) | the loop setting; the graph, with loops through the engine; refusing, muting the cycle's lightest route, and corner edits that add a pad edge; a preset into a corner; one step late, held from the boundary; the bound, buzzing up to the control rate's Nyquist; **MS-44, the erosion flag the resolver reads** |
| LF-1 to LF-14 (E.5) | four LFOs (one law, run by the modulation system when global and by the engine when per note); retrigger; rate and sync; start phase; the drawn shape and its morph; S&H; Random; the travel stage; streams keyed by VO-10's note counter; envelope mode; performance state |
| MK-1 to MK-5 (E.6) | the Kuramoto LFO, as the lab built it; a modulator of its own, not an LFO kind |
| MB-1 to MB-11 (E.7) | ORBITAL: step (Bounce by MR-4's fold, clamp counted), thermostat, time, mass, safety, observables with acceleration, full-range scales, reset, 26 sources, destinations, **MB-11: the two periodic presets' data** |
| MR-1 to MR-10 (E.8) | the 2-D random, with `xy.triplet` and `xy.dotted` |
| MS-36 to MS-40, MS-43, MS-45 (E.9) | state (`mod` in the shell's document); no hidden state; the FX device; CPU; the host block; **MS-43, what the engine receives for a per-note LFO; MS-45, the observation view** |

**Body edits.**
- The header names Appendix E and the [OPEN Mn] tag.
- §1: the modulation system is in scope, and so are the engine's per-note LFOs and engine path.
- §3 gains a deliverable and a `mod/` folder.
- §4.3: the rows are split three ways (modulation system, engine, intent bus).
- 5.2 and 5.6: `mix.osc1`, `mix.osc2`, `mix.sub`, `mix.noise` and the noise's `cutoff` go to rate A [X78].
- PA-2: a Control input, and the engine path's terms.
- PA-3: the note-on either-order exception now covers the modulation system's events.
- PA-4: the levels and the noise cutoff join [X78].
- VO-10: publishes the note counter at the fresh start.
- IF-3: values are readable at each boundary; velocity and the expressions are read from the shell. X45 is dropped.
- IF-11: the modulators' block.
- IF-12: the position offset; the modulation manifest's keys (MO-11's menu only); the erosion flag; the pick reads.
- **IF-13 (new):** the modulation system's calls.
- RC-4: specified, and "must not require" is amended.
- RC-7 and RC-7.3: the new transport consumers.
- RC-11.1: the modulation state.
- **PF-11 (new):** the modulation CPU.
- BD-8: stage 9, with its engine and resolver impact stated.
- MO-26 (7): "until routes exist" is replaced by the erosion flag.
- **Items 36 to 39 (new).**
- §11.0: M3 and M4. §11.1: X41 to X81, without X45 and X46.

**Engine impact (corrected; the first draft understated it).** With no route and no per-note LFO, every accepted item stays bit-identical (item 36.1). But the round adds to:
- **stage 1's API:** IF-13's events, PA-2's Control input, VO-10's note counter, PA-4's longer list;
- **stage 2's voice:** the per-note LFOs, run by Appendix E's LFO law, and the engine path's per-sample closed form;
- **stage 7's resolver:** the modulation manifest's keys, the erosion flag, the pick reads.

BD-8 stage 9 reopens those stages for those additions only.

## 2. The human's answers (ADR-379) and the lead's calls, and where each is written

| ruling | where |
|---|---|
| 1. Routes respect a source's on corners | MS-25 (a source's keys: MO-26 (2) weights, (7) for the noise, the restricted pick, held while waiting, no glide at a landing); item 36.7 |
| 2. M1 hybrid, chosen automatically | MS-41 (the rule: per-note engine-local source into a PA-4 key, not Control, not quantized; the triples; the closed form in the engine), MS-3, MS-4, MS-43, LF-1, LF-2, PA-2, PA-4, RC-4; item 36.11 |
| 3. M5: global LFOs and the 2-D random only | MS-12 (d), MS-29 (the Kuramoto LFO is its own modulator, not an LFO kind, so it may not; ORBITAL, the wheel and macros may not); E.6's intro; item 36.9 |
| 4. M6 (a): the resolver resolves modulator keys; menu only; every pick a purpose-keyed read | MS-27, MS-25, LF-8, IF-12; item 36.17 |
| 5. M4 (b): acceleration joins; sine and cosine stay [OPEN] | MB-6, MB-7, MB-9, MS-8; [OPEN M4] |
| 6. M3 stays [OPEN], in a lab | LF-8; [OPEN M3] |
| Lead: M2 (b), minimal | PA-2's Control input q = 1 − W ramped by PA-3; MS-20; item 36.4 with "a morph jump under Control moves the value" |
| Lead: B4 | MS-29, [X79] |

## 3. The critic's findings, and where each is fixed

| finding | fix |
|---|---|
| B1: route weights glided from MO-5 (a) | MS-25: each k[c] has its own one-pole [X76]; "MO-5 (a)" and "glided corner weights" are gone from MS-25 and IF-12 |
| B3: MO-26 (7)'s route test | MS-44 and IF-12: a per-corner erosion flag (depth above 0, not refused or muted, Control included); round 61's "until routes exist" is replaced |
| B4: Save under a position route | MS-29, [X79] |
| B5: a new voice's first sample | MS-42, PA-3, LF-2 (phase 0 at the note-on's sample); item 36.13 |
| B6: the loop gate's oracle; ORBITAL's energy | 39.1 (a), an engine-free recurrence; 38.2, relative energy drift with the clamp count asserted 0; both cite Blind-Spot Armor risk row 6 |
| S2: quantize | MS-22: per-key offsets, IF-2 for voice pitch only [X81] |
| S3: loops through the engine | MS-31's swarm nodes; `osc1.R` → `osc1.coupling` in 39.1 (b) and 39.2 |
| S4: corner edits that add a pad edge | MS-32: mute the lightest route and name it |
| S5: which route "Refuse inside a corner" mutes | MS-32: the cycle's lightest route |
| S6: X45 and the engine-impact claims | X45 dropped (velocity and expressions read from the shell, MS-9, IF-3); BD-8 and §1 above |
| S7: tolerances | items 36 to 39 now use 10⁻⁶, 10⁻⁵ or 10⁻⁴ with stated durations |
| S8: ORBITAL's trajectories | 38.2 uses only MB-11's two periodic presets, with their data in the spec |
| S9: the walk across rates | 38.3 compares 44.1 kHz with 96 kHz at σ 0.05 |
| S10: missing acceptance items | 36.14 (MS-16, MS-18), 36.15 (MS-21), 36.16 (MS-7, MS-36, MS-40), 36.17 (MS-27), 36.18 (MS-11), 38.1 (MK-2), 38.2 (MB-8), 36.20 and MS-45 (the observation view on IF-13) |
| S11: the per-note stream's key | LF-12 and VO-10: the note counter's four bytes, least significant first, between the purpose and the seed (MS-5's order) |
| S12: "held from the boundary" | MS-2, MS-3, MS-34 |
| S13: where `mod` lives | MS-36: the shell's document, not IF-4 [X80], because the engine does not own it and most of it is per corner, like the corners |
| S14: one owner for picks | MS-25 and IF-12: the resolver |
| S15: Exempt's routes | MS-28: corner A's |
| S16: `xy.division` | `xy.triplet`, `xy.dotted` |
| S17: Bounce | MB-1: MR-4's fold |
| ψ | `oscN.psi` is relative to the frame (C.13's d); item 36.10 |
| MS-35 | says outright that a loop can buzz up to f/(2Δ): 760 Hz at 44.1 kHz |
| setting-set text | "of the mixture while its swarm blends, SB-4" is gone from MS-8's `oscN.R` (round 61's IF-3 had already dropped it) |

**The ten nits.** The brief named only ψ, and the review itself did not reach this agent. Fixed besides:
- the FX device's per-note LFOs (they run in its own modulation system, since it has no engine);
- MS-40: the block reaches the resolver too;
- ORBITAL's speed clamp is stated in field units;
- MS-41's triples are one per corner that holds the route, not one per route;
- MS-15: one Control route per destination **in a corner**.

**Please send the review's nit list if any of the ten is not among these.**

**Not this round's:** round 61's MO-22 row has unescaped `|` inside its formula (a table-column count of 11 against 3). It is round 61's text and is left as it is.

## 4. Open items, by sound

**[OPEN M3] LFO corners that differ in kind** (LF-8, MS-27), being tested in a lab (mix against switch, ADR-379 r6).
- **(a) Mix:** between a drawn corner and an S&H corner, the wobble fades into the steps, and halfway you hear a wobble with steps riding on it.
- **(b) Switch:** the kind changes at one place on the pad, and either side you hear one kind.

**[OPEN M4] The sine and cosine of each ORBITAL body's angle as sources** (MB-9), being tested in the reopened ORBITAL lab (ADR-379 r5).
- **(a) Not sources:** 26 sources. Under Wrap, a body crossing the seam makes its `x` or `y` route jump.
- **(b) Sources:** 34 sources. A circular orbit drives two smooth, quarter-turn-apart wobbles, and crossing the seam moves nothing abruptly.

## 5. Proposed defaults: [X41] to [X81]

**The lead's (the first brief):**

| tag | the default | otherwise, by ear |
|---|---|---|
| X41 | keytrack (n − 60)/48 per voice, clamped to ±1 | a wider span keytracks more gently |
| X42 | velocity and keytrack per voice | global: every held note follows the newest key |
| X43 | release velocity only as MPE lift | a fast key-up would brighten a non-MPE tail |
| X44 | bend and per-note bend start bipolar; other keyboard sources unipolar | a unipolar bend offsets the sound at rest |
| X47 | Walk smoothing is its own time (0.225 s) | tied to τ, slowing the walk would also round it |
| X48 | spring = tension (Hz) and ζ; overshoot a readout | — |
| X49 | one bias amount; 0 uniform | — |
| X50 | the 2-D random's own seed; a stream per mode | a shared stream replays across modes |
| X51 | spread reflects at the edge | shrinking makes edge-homed spreads smaller |
| X52 | the 16-sample tick | — |
| X53 | one source, two rows | — |
| X54 | travel includes None | — |
| X55 | S&H draws at the first cycle | the shipped law starts a struck note flat |
| X56 | S&H on its own cycle; sample-on-trigger (and ADR-266 r6's S&H on ORBITAL's mappings) after 1.0 | — |
| X57 | STEPS and S&H both stay | — |

(X45 is dropped: velocity and the expressions are read from the shell. X46 is settled by ADR-379 r5 and ADR-165 A1.)

**This round's readings:**

| tag | the default | otherwise, by ear |
|---|---|---|
| X58 | an envelope source before its velocity factor | after it, depth would follow key velocity too |
| X59 | ψ relative to the frame; the burst clamped; gravity 0.5 + cents/70 | other scalings move knobs further or less |
| X60 | the outside path at the control rate | per-sample global modulators would cost about three times the slice |
| X61 | curves exp s², log 1 − (1 − s)², bipolar on the magnitude | curves on 2s − 1 would move a bipolar rest |
| X62 | the 64 routes count distinct pairs over the corners | per corner would allow 256 live routes |
| X63 | route weights follow `morph.law` | a Quantum-mode key's routes would flip while its neighbours fade |
| X64 | position routes patch-wide | per-corner position routes depend on what they move |
| X65 | the lab's loop graph, admission order, one step late everywhere | — |
| X66 | LFO defaults (Global; start phase for free LFOs; sync with retrigger runs from the strike; per-note unstruck in phase) | random start phases would detune a chord's wobbles |
| X67 | per-note streams: Next and Keep from draw 0 of the note's stream; Rewind one shared riff | — |
| X68 | travel laws; S&H's default Time 0.08 s on the SLEW curve | a linear Time is more mechanical |
| X69 | envelope mode falls to s = 0 along osc 1's governing release | falling to y = 0 would leave unipolar routes half on |
| X70 | one seed per LFO | a patch seed ties the LFOs' randomness |
| X71 | Kuramoto details (Drawn from a chosen LFO, global, retrigger after silence, exact 2π) | — |
| X72 | the new modulators' rate-like settings only are host parameters (`kuro.rate`, `kuro.retrig`, `orb.timeScale`, the 2-D random's clock, rate and division), limited as the LFOs' are | every M setting would put ORBITAL's eight masses on the host's list |
| X73 | ORBITAL details (one global field; resets; Kick on the card; v* 0.5, τ 0.5 s; scales measured per preset, a user field at the maxima) | per-voice fields: an orbit per note, at 16× the CPU |
| X74 | the 2-D random's details (Spring default, Lag kept, a pad is two Control routes, rows unipolar) | dropping Lag aligns it with the LFOs' travel set |
| X75 | MPE bend range 48 st until B504 | 2 or 12 st makes a small slide a large move |
| X76 | each route weight its own one-pole at `morph.glide`, none at a landing | glided weights would smear routes on a source's landing |
| X77 | a Control route always takes the outside path | Control on the engine path would need ends and W per sample |
| X79 | Save under a position route stores the unglided target at the unmodulated position | storing the heard value would bake an LFO's sweep into the corner |
| X80 | `mod` lives in the shell's document | inside IF-4, the engine's state would change version for data it never reads |
| X81 | a quantized route always takes the outside path; Quantize on in any corner sends every instance into that key outside | snapping is control-rate by nature (hysteresis, minimum step time) |
| X82 | an Exempt key's routes are corner A's | blending the corners' routes would modulate a key the morph holds still |
| X83 | Kuramoto's Drawn plays Sine when the chosen LFO is not drawn | refusing would silence a patch when an LFO's kind is changed |
| X84 | several 2-D random note triggers on one tick are one | each note of a strummed chord would jump the point separately |
| X85 | the per-note-to-shared setting is device-level, outside every patch | per patch, a preset could forbid what the player allows |
| X86 | its default is Allow (the newest note) | — |
| X87 | under Refuse, a loaded patch's routes of that kind are muted and named, not deleted | deleting would lose them when the setting is switched back |

## 6. Where the draft was overtaken by later rulings

1. MD-33, MD-48, MD-49: ADR-372.
2. MD-44: ADR-371 A1 (start phase).
3. MD-60 and MD-29: ADR-371 A1.
4. MD-41: ADR-378 (the travel stage; SLEW becomes S&H's default travel).
5. MD-40 against ADR-377: [OPEN M3], in a lab.
6. MD-14's "×5 observables" against SPEC-ORBITAL §4: ADR-379 r5 (acceleration joins).
7. MD-19: no longer waits.
8. MD-1 and MD-3 (everything outside the engine): ADR-379 r2. The per-note LFOs and the engine path are now in the engine.
9. MD-61 ("only a global LFO"): ADR-379 r3 adds the 2-D random.
10. The draft's FX table ("ENV 1 also an envelope follower") against X27: left out.
11. ADR-266 r6 (S&H on ORBITAL's mappings): after 1.0 under X56. Please confirm.
12. SPEC-ORBITAL §5's "cap 32 steps per block" would break DT-2: MB-3 counts per sample.
13. SPEC-ORBITAL §12.6: the Van der Pol band is not built.

## 7. For the lead to record

- **ADR-379**, the human's six answers above. The text this round cites as ADR-379 r1 to r6 assumes that numbering.
- **ADR-378**, the travel stage (cited, not yet in DECISIONS).
- **The lead's calls:**
  - M2 (b), minimal: PA-2's Control input;
  - B4 [X79];
  - the critic's should-fix 2 to 17 accepted as written in §3.
- **A DECISIONS line for the engine change:** the engine gains the per-note LFOs, the engine path's closed form, PA-2's Control input, PA-4's longer list (rates C → A on five rows) and VO-10's note counter. This reverses RC-4's "must not require the engine to compute an LFO" for per-note LFOs (ADR-379 r2).
- **An ENGINE-NEEDS check:** whether GUI rows assume per-note LFOs live outside the engine.
- **Clean-room delivery** (memory: clean-room spec sync): after M3 and M4 are decided, deliver to both repos the same turn.
- **Round 61's MO-22** unescaped `|` (§3 above).
- **The structure file:** proposed blocks (§8).

## 8. The structure file

`engine2/structure/engine-structure.json`'s `spec.blob` is updated. It already has a `modulation` item with edges to `morph_resolver`, `parameters` and `voice`. Proposed, not added:
- child items: `modulation.sources`, `modulation.matrix`, `modulation.lfo`, `modulation.kuramoto`, `modulation.orbital`, `modulation.xy_random` and `modulation.loops`. **The separate `voice.lfo` proposal is withdrawn** (critic 2d): the per-note LFOs and the engine path's closed form are the same code as `modulation.lfo` and the matrix's closed form (LF-1, MS-41), so the engine's use is an edge, `voice->modulation.lfo`, not a second block;
- edges:
  - `transport->modulation`;
  - `host->modulation`;
  - `voice->modulation` (published sources, lifecycle with the note counter);
  - `modulation->voice.route_terms`;
  - `modulation->voice.lfo`;
  - `modulation->pitch_system` (voice pitch's quantized contribution);
  - `modulation->fx_rack`;
  - `modulation->morph_resolver` (position offset, erosion flags);
  - `morph_resolver->modulation` (resolved modulator keys, picks);
- cites: IF-13, RC-4, MS-41, MS-43.

## 9. Process notes (not spec rows)

- **Checks.** Each modulator, when ported, gets a `tools/*_check` with parity to its lab and seeded replay. Two deliberate divergences:
  - MS-5's purpose-keyed streams replace the lab's XOR words, so parity is on the law with the draws injected;
  - MK-3 uses an exact 2π where the lab truncates it.
- **Listening.** The [Xn] rows can go to the human as one capped listening batch (Blind-Spot Armor), the travel laws first.

## 10. After 1.0 (named, not built)

Note-on randoms; the trigger timer, its impulses and every trigger source; the MIDI gate and trigger; S&H's sample-on-trigger and the S&H mode on ORBITAL's mappings; ORBITAL's edge cushion; rails; the silent spring; a stepped destination kind; envelopes beyond ENV 1 to ENV 4; waypoints and region locks on a modulator's keys; ORBITAL per-voice fields, grab and throw, persistence, distance scale and pair distances.

## 11. The second pass (ADR-379 A1 and the critic's notes)

**The human's rulings (ADR-379 A1), and where written.**
1. **PA-4's audio-rate destinations, ruled (X78 retired).**
   - The four source levels and the noise filter's `cutoff`.
   - The blades' `position` and `width`, on both blades of both oscillators (rate column C → A; `params.json` and `shell-data.js` regenerated).
   - Forward in PA-4: the voice filters' cutoff and resonance, when RC-1 builds them.
   - PF-2 notes it must be re-measured with the larger set.
2. **A per-note source on a shared destination reads the newest note** (MS-46), and the page warns. MS-8's column is now **scope** (`per-note` or `global`), and MS-12 lists each destination kind's scope (`per-voice` or `shared`). The warning derives from these two columns.
3. **A device setting can refuse such routes** (MS-47): `device.perNoteToShared`, Allow or Refuse. It is device-level and saved outside the patch [X85]; its default is Allow [X86]. Under Refuse, existing routes of that kind are muted and named, not deleted [X87].

**The critic's notes, and where fixed.**
- 2a: MS-20's A counts outside-path instances only; engine-path terms are added in the engine, once.
- 2b: MS-41 chooses the path per corner instance; Quantize on in any corner sends all of a key's instances outside.
- 2c: MS-42 states the starting values. Envelopes and swarm observables take the last published value or their rest value. A per-note LFO starts from the same LFO code run once at phase 0.
- 2d: MS-41 says the closed form is the same code in both places. The `voice.lfo` proposal is withdrawn (§8).
- 2e: MS-43: a per-note LFO's settings are held from the boundary, never glided or ramped.
- 2f: IF-3 publishes each per-note LFO's output.
- 2g: MS-46.
- MS-41 adds that no engine-path route closes a loop within a sample.
- Item 39.1 (b) drops the Nyquist clause, which could not fail.
- MS-28's corner-A routes are tagged [X82].

**Nits fixed.**
- MS-13 cites ADR-340 r7 and the Map Mode spec for depth 0.25.
- MB-3: only the mass smoothing and event timing depend on the rate.
- MK-4: Drawn falls back to Sine [X83].
- MR-6: tagged [X84].
- RC-11.1 lists every way the modulation system reaches the engine.
- IF-13: offsets addressed by note id, as PA-2 does.
- X72: rate-like host parameters only.

**New items.** 36.21 covers the newest note and the device setting. Item 36.13 now also checks starting values.

**For the GUI session (ENGINE-NEEDS or GUI rows; the lead routes them):**
- **The newest-note warning** (MS-46): shown when a route from a `per-note` source to a `shared` destination is made, naming both, and derived from MS-8's scope column and MS-12's scope list.
- **The device setting's home** (MS-47): where "Per-note sources on shared destinations: Allow / Refuse" lives in the device's settings.
- **The muted-and-named notice** when Refuse meets a loaded patch.

**For the lead to record:** ADR-379 A1 (the three rulings above; the text cites "ADR-379 A1"). PA-4's growth and the five further rate changes (the blades' `position` and `width`) join the DECISIONS line for the engine change in §7.

## 12. ADR-379 A2 (the reopened labs), same commit

- **M3 closed: switch.** LF-8. An LFO whose corners differ in kind resolves `lfoN.kind` as a Structural key (its pick and latch) and crosses between kinds over PA-5's 5 ms at the flip. Capturing an in-between shape as its own pattern is in the after-1.0 note. Item 37.2 checks it.
- **M4, first half, closed: the sine and cosine of each body's angle join** (34 sources). MB-6 and MB-9 are updated, MS-8 gains a row, and item 38.2 checks sin² + cos². [X88]: both are (1 ± …)/2 of the unsmoothed angle, and new routes start bipolar.
- **[OPEN M4], narrowed:** which centre the angle is measured about.
  - (a) The field centre, as written. A body circling off-centre wobbles unevenly.
  - (b) The other bodies' centre of mass. A body turns evenly around its partner.
  - (c) The direction of travel. The angle swings hardest at a slingshot.

  The lab builds them for the human's ear. **This is the only [OPEN] left in the round.**
- **The thermostat auto-kicks a stopped field.** MB-2. With the thermostat on, where the free bodies' mean speed is below 10⁻⁹ (the reference's skip threshold, [X89]), every free body gets speed v*. Body j's direction is 2π·u, with u being draw k of `mod/orbital/autokick/<j>` at `orb.seed`, and k counting restarts since reset [X90]. Item 38.2 checks it. Orbit size is not part of this ruling.
- **For the lead to record:** ADR-379 A2 (these three).
