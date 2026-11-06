# Spec round 62: the modulation system — notes for the lead (B637)

> **Origin.** horde lead session, 2026-11-06. The human, by poll: "Write the mod spec round". Drafted by an agent the lead dispatched, from the ruled draft `docs/proposals/modulation-spec-draft.md` (#1424) and ADR-371 (and A1), ADR-372, ADR-375 (and A1), ADR-377, ADR-378 (the travel stage, recorded by the lead next), with ADR-266, ADR-330, ADR-342, ADR-349 and ADR-374 where they bear. ROADMAP B633, B637.

**Status: DRAFT for the lead.** The round adds **Appendix E** to `specs/SPEC-ENGINE.md` and touches the body where modulation crosses the engine. Nothing here is ruled by this file. Every **[OPEN Mn]** is the lead's to rule before the modulation system's stage (BD-8, stage 9). Every **[Xn]** is built as written until ruled.

## 1. What the round adds

**Placement.** A new **Appendix E, "The modulation system (normative)"**, after Appendix D. Like the resolver, it is a part the clean room builds beside the engine, with its own calls. Its rows:

| rows | what |
|---|---|
| MS-1 to MS-7 (E.0) | who computes what; the clock; reading the engine's sources; control rate only; determinism; real-time safety; reset and load |
| MS-8 to MS-11 (E.1) | the source table (ids, polyphony, value, starting polarity); the keyboard's sources; macros, X and Y; aftertouch's default route |
| MS-12 to MS-23 (E.2) | destinations; the route record; curves; contributions and Control; one route per pair; depth wires; the cap; where routes live; the summing order and the wall; rates; pitch quantize; undo |
| MS-24 to MS-29 (E.3) | per-corner routes fade with the corner; route weights; why that is not a sound crossfade (ADR-377); modulator settings morph; Exempt still modulated; the morph position as a target |
| MS-30 to MS-35 (E.4) | the loop setting; the graph; refusing; a preset into a corner; one step late; the bound |
| LF-1 to LF-14 (E.5) | four LFOs: global or per note, retrigger, rate and sync, start phase, the drawn shape and its morph, S&H, Random, the travel stage, the stream and strikes, envelope mode, performance state |
| MK-1 to MK-5 (E.6) | the Kuramoto LFO, as the lab built it |
| MB-1 to MB-10 (E.7) | ORBITAL: step, thermostat, time, mass, safety, observables, full-range scales, reset, sources, destinations |
| MR-1 to MR-10 (E.8) | the 2-D random: rows, tick, streams, the map, Walk, triggers, targets, travel, replay |
| MS-36 to MS-40 (E.9) | state; no hidden user state; the FX device; the CPU; the modulators' host-parameter block; and a short after-1.0 note |

**Body edits.** The header (Appendix E named; the [OPEN Mn] tag); §1 (the modulation system is in scope, the modulators leave the out-of-scope list); §3 (a deliverable row and a `mod/` folder in the proposed layout); §4.3 (the row split: modulation system, intent bus); IF-3 (per-note bend and lift published, [X45]; sources readable at each boundary); IF-11 (the modulators' block after the morph's); IF-12 (the position offset, the weights and picks read, [OPEN M6]); **IF-13, new** (the modulation system's calls); RC-4 (the deferral ends: "Specified (round 62)"); RC-7 (its new consumers; RC-7.3 says the instrument's sound now depends on the transport through them, the draft's §10 item 7); RC-11.1 (the modulation state); **PF-11, new** (the reported CPU cell, T10); BD-8 (stage 9); **items 36 to 39, new** in 10.1; §11.0 (six OPEN rows) and §11.1 (X41 to X75).

**Engine impact.** None while a patch has no routes: item 36.1 holds it bit for bit, so stage 9 reopens no accepted stage. The engine's half stays PA-2's offsets and IF-3's sources, plus IF-3's two new published values (per-note bend, lift).

## 2. Open items for the lead: [OPEN M1] to [OPEN M6]

Each option is described by what it does to the sound or the playing.

**[OPEN M1] Where per-voice routes are evaluated** (MS-1, MS-3, RC-4, IF-13; the draft's T3, T7). The record decides that the LFOs and the matrix are outside the engine (RC-4's "must not require the engine to compute an LFO"). It does not decide what that costs a route from an engine source.
- **(a) Everything in the modulation system (as written).** Envelope, velocity, R and expression routes read the engine's value from the end of the previous control interval: 0.66 ms late at 44.1 kHz. You would hear it only on a very fast pitch envelope into the sub's pitch (PA-4 wanted that "smooth"), where the envelope's first 0.66 ms arrives late and is then followed as a 1,500 Hz piecewise-linear ramp: a fast "zap" loses a little of its very top. The engine is untouched, and the shell runs the engine and the modulation system in turn per control interval.
- **(b) The engine evaluates the routes whose source it publishes, and the per-note LFOs, inside each voice.** An envelope into pitch is sample-exact and on time; a pitch zap is as sharp as the envelope's curve. Costs: RC-4's "must not require" is reversed; the engine gains a route evaluator and LFOs (a bigger engine and clean-room scope), and the matrix is split across two parts that must agree on curves, polarities and depth wires.
- **(c) A middle way:** (a), plus the engine publishing each envelope's per-sample trajectory for the interval, so a route into PA-4's keys can be applied per sample one interval late. Sharper than (a) on pitch, still late.

**[OPEN M2] A Control route against the engine's smoothing** (MS-20, PA-2; T4). Under Control the knob is ignored, but the engine still smooths the knob's resolved value by PA-1 (about 12.8 ms).
- **(a) As written:** the offset is computed against the unsmoothed value. Whenever the base moves under a Control route (a morph move, automation), the controlled parameter wobbles away from where Control holds it for up to 12.8 ms, then returns: a small swell or dip on every fast morph gesture.
- **(b) The engine takes a per-voice Control input** that replaces the base. Control holds exactly; a small engine change (PA-2 gains one input per voice per key).

**[OPEN M3] LFO corners that differ in kind** (LF-8, MS-27). One corner's LFO is a drawn shape, another's S&H.
- **(a) Mix their outputs by the corner weights** (ADR-349's recommendation). Moving between those corners, the drawn wobble fades into the stepping random one: halfway, a wobble with steps riding on it. It is a blend of two control signals, not of two sounds.
- **(b) The kind follows its quantum boundaries** (ADR-377's principle for structure). The LFO switches from drawn to S&H at one place on the pad; on either side you hear one kind, never a mix.

**[OPEN M4] ORBITAL's observables** (MB-6, MB-7, MB-9). ADR-375 A1 counts "4 bodies × 5 observables"; the lab's five are x, y, speed, near and angle. SPEC-ORBITAL §4 also lists acceleration, and ADR-165 A1 ratified a fixed per-preset ceiling for it and sine and cosine observables in Wrap.
- **(a) The lab's five** (as written): 22 sources. No acceleration: the sharpest slingshot accents come from speed, a little softer.
- **(b) Six, with acceleration** on a per-preset ceiling ([X46]): 26 sources. Acceleration spikes before speed peaks, so accents land earlier and sharper on close approaches.
- **(c) (a) or (b) plus sine and cosine of each position in Wrap,** so a body crossing the seam moves the sound smoothly instead of jumping.

**[OPEN M5] May the 2-D random move the morph position?** (MS-12, MS-29). ADR-371 MQ8: "Only an LFO set to global can move the morph position."
- **(a) No** (as written): the 2-D random drives other pads (the Specimen XY, FX pads) and any knob, but the morph pad moves only from a global LFO. A random walk across the morph corners needs an LFO drawn as one.
- **(b) Yes:** it is global by construction, and the human's limit was aimed at per-note sources. Then "a random walk through the four corners" is one gesture, which is the use B540 was ruled for ("automating the traversal of XY pads").

**[OPEN M6] How a modulator's per-corner settings are resolved** (MS-27, IF-12, IF-13). MQ15 says they "morph like any knob", so something must blend an LFO's rate between corners.
- **(a) The resolver takes the modulation manifest as keys** beside the engine's: one law, one draw scheme, one glide; the LFO rate glides, picks and steps exactly as a knob would. The resolver's IF-12 grows a second output stream.
- **(b) The modulation system blends them itself** from the resolver's weights and picks. The resolver is unchanged, but two parts implement the morph law, and they can disagree by a hair (a rate that flips a moment apart from the knob beside it).

## 3. Proposed defaults, built as written until ruled: [X41] to [X75]

**The lead's (from the brief):**

| tag | the default | what you would hear otherwise |
|---|---|---|
| X41 | keytrack k = (n − 60)/48 per voice, clamped to ±1 (C4 is the centre, four octaves full scale) | a wider span makes keytracking gentler per key; a narrower one saturates at the keyboard's ends |
| X42 | velocity and keytrack per voice | global would make every held note follow the newest key or velocity |
| X43 | release velocity a source only as MPE lift | a non-MPE release velocity would let a fast key-up brighten a release tail |
| X44 | bend and MPE per-note bend start bipolar; every other keyboard source unipolar | a unipolar bend route would offset the sound with the wheel at rest |
| X45 | IF-3 publishes per-note bend and lift | without it, per-note bend and lift cannot be routed |
| X46 | acceleration's ceiling per preset, like speed and energy (if [OPEN M4] admits it) | a running maximum makes the first slingshot after a reset loudest |
| X47 | the Walk's smoothing is its own time in seconds (default 0.225 s, the lab's) | tied to τ, slowing the walk would also round it more |
| X48 | the spring is tension (Hz) and damping ζ; overshoot is a readout | an overshoot knob instead would hide the tension/damping pair |
| X49 | one bias amount beside the bias choice; 0 is uniform | — |
| X50 | the 2-D random's own seed; each mode its own stream | a shared stream would make switching mode replay the same numbers |
| X51 | a spread past the pad reflects | shrinking it to fit would make edge-homed spreads smaller |
| X52 | the 16-sample tick, no pitch special case | an interpolated path would smooth a pitch destination at extreme depth |
| X53 | one source, two rows (`xy.x`, `xy.y`) | — |
| X54 | travel includes None (jump straight) | (ruled in substance by ADR-375) |
| X55 | S&H draws at every cycle start, the first included | the shipped law holds 0 for the first cycle: a struck note starts flat |
| X56 | S&H clocked by its own cycle; sample-on-trigger after 1.0 (and, with it, ADR-266 r6's S&H mode on ORBITAL's mappings) | — |
| X57 | STEPS (a drawn preset) and S&H both stay | — |

**This round's own readings (the agent's, listed so they can be overruled):**

| tag | the default | the alternative, by ear |
|---|---|---|
| X58 | an envelope source is its level before its velocity factor | after it: a route's depth would also follow how hard the key was struck |
| X59 | ψ as frac(ψ/2π), circular; the onset burst clamped to [0, 1]; gravity as 0.5 + cents/70 | other scalings change how far those sources move a knob |
| X60 | all offsets at the control rate in 1.0, PA-4's keys included | per-sample evaluation: about three times the modulation CPU slice for no audible gain below 40 Hz |
| X61 | curves: exp s², log 1 − (1 − s)², bipolar on the magnitude | a geometric "log" is undefined at 0; curves on 2s − 1 would move a bipolar source's rest |
| X62 | the 64 routes count distinct pairs over all corners | 64 per corner: up to 256 live routes, four times the CPU |
| X63 | route weights follow `morph.law`, not the destination key's own mode | a Quantum-mode key's routes would flip while its neighbours' fade |
| X64 | routes into the morph position are patch-wide; Save current sound ignores them | per-corner position routes depend on the position they move |
| X65 | the corner-mapping lab's loop graph, admission order, and one step late under every mode | same-interval reads under the refusing modes would make loop-free chains a step quicker (inaudible) but evaluation-order dependent |
| X66 | LFOs default Global; start phase also for free LFOs; synced with retrigger runs from the strike; per-note unstruck waves agree in phase; phases never saved | random start phases per note would detune a chord's wobbles from each other |
| X67 | a per-note random stream: Next and Keep start each note at draw 0 of its own stream; Rewind plays one riff | — |
| X68 | travel Time along C.12's curve at the SLEW shape; Rate in full scales per second; Spring clamped to ±1; S&H's default Time 0.08 s | a linear Time glide is more mechanical; an unclamped spring overshoots past the LFO's range |
| X69 | envelope mode: the source falls to 0 along osc 1's governing envelope's release; a global one releases when no note is held | falling to the centre (y = 0) would leave a unipolar route half on after release |
| X70 | one seed per LFO | a patch seed would tie the four LFOs' randomness together |
| X71 | Kuramoto Drawn reads a chosen LFO (default LFO 1); global; retrigger after silence; exact 2π | the lab truncates 2π to 6.283185307, a 10⁻¹⁰ difference |
| X72 | the new modulators' continuous settings are host parameters | patch state only: no automation of ORBITAL's gravity or the walk's speed |
| X73 | ORBITAL: one global field; reset Free, Transport or Note-on; Kick only from its card; thermostat v* 0.5, τ 0.5 s; full-range scales measured per factory preset over 30 s | per-voice fields would give each note its own orbit, at 16× the CPU |
| X74 | the 2-D random: Spring the default travel (the lab's); Lag kept beside ADR-378's four LFO travels; mapping to a pad is two Control routes; its rows start unipolar | dropping Lag aligns it with the LFO's four; bipolar rows would centre on 0.5, not on home |
| X75 | the MPE member channels' bend range 48 semitones until B504 rules it | 2 or 12 semitones would make a small finger slide a large pitch move into a route |

## 4. Where the draft is contradicted by a later ruling

1. **MD-33, MD-48, MD-49 (B396's five questions, open in the draft)** are ruled by ADR-372: fade with the corner, stop at zero, a per-patch loop setting defaulting to Allow, a preset into a corner loads minus the mapping with a warning, the pad follows the loop setting. Written as MS-24, MS-17, MS-30 to MS-35.
2. **MD-44 (start phase, open)** is ruled by ADR-371 A1: a synced LFO has a start phase, 0 to 360°. LF-5.
3. **MD-60's details and MD-29's starting curve (T11)** are ruled by ADR-371 A1: envelope mode holds its last point and falls along osc 1's release; a global LFO may use it; a new route starts linear. LF-13, MS-13.
4. **MD-41's S&H "one seeded value drawn each cycle and held"** is extended by ADR-378: a travel stage follows every random shape, and S&H's default is its SLEW law. The lab's five smoothing modes (NONE, LINEAR, COSINE, CUBIC, SLEW) become three interpolations plus a travel stage. LF-9 to LF-11.
5. **MD-40 (corners of different kinds mix by output, ADR-349's recommendation)** sits against **ADR-377** ("anything that can't blend naturally follows its quantum boundaries"). ADR-377 is about sounds, and this is a control signal, so the record does not settle it: [OPEN M3].
6. **MD-14 / ADR-375's "8 bodies × 5 observables"** against **SPEC-ORBITAL §4 and ADR-165 A1**, which name acceleration (with a per-preset ceiling) and Wrap's sine and cosine observables, which the lab never built: [OPEN M4]. ADR-375 A1 then narrows the sources to bodies 1 to 4.
7. **MD-19 ("modulator settings and the morph position wait on B396")** no longer waits: ADR-372 ruled the loop rules.
8. **MD-2 (per-sample offsets on PA-4's keys)** is set aside by [X60]: in 1.0 no modulator runs above 40 Hz, so PA-3's ramp is the interpolation.
9. **The draft's §7 FX table ("ENV 1 also an envelope follower")** sits beside X27, which reserves no space for an envelope follower. MS-38 leaves it out.
10. **ADR-266 r6 (S&H also a mode on ORBITAL's mappings, with its own rate and sync)** is placed by no later ruling. This round puts it after 1.0 with sample-on-trigger ([X56]); the lead should confirm.
11. **ADR-266 r4 and ADR-261 r3 (mappings global for 1.0)** are reversed by ADR-371 MQ17; the Map Mode spec was edited to match (GUI-51, ADR-371 A1).
12. **SPEC-ORBITAL §5's "cap 32 steps per block"** would break DT-2 (block-size independence). MB-3 counts steps per sample, at most 8 per sample, as the lab does; at dt = 1/960 s and time ×4 that is under one step per sample at any certified rate.
13. **SPEC-ORBITAL §12.6:** whichever energy regulator is kept, the other is deleted. ADR-375 A1 keeps the thermostat, so MB-2 states that the Van der Pol band is not built.

## 5. Round 61's Appendix D

The round was written against Appendix D as `origin/b631-round-61` (`e9f28d97`) leaves it, with ADR-377's redraft (structure follows quantum) read from DECISIONS. It cites only MO rows that round 61 keeps (MO-4 to MO-12, MO-20, MO-25) and states nothing about setting sets. MS-28 restates MO-12's round-61 sentence (Exempt still modulated). MS-26 says why a route's depth blending is parametric under ADR-377. When round 61 merges, the conflict is the spec's blob line in `engine2/structure/engine-structure.json` and possibly the header lines both rounds touch; Appendix E sits after Appendix D's last section either way.

## 6. The structure file

`engine2/structure/engine-structure.json`'s `spec.blob` is updated. It already has a `modulation` item with edges to `morph_resolver`, `parameters` and `voice`. Proposed, not added:
- child items under `modulation`: `modulation.sources`, `modulation.matrix`, `modulation.lfo`, `modulation.kuramoto`, `modulation.orbital`, `modulation.xy_random`, `modulation.loops`, each citing its rows (MS, LF, MK, MB, MR);
- edges: `transport->modulation` (RC-7: LF-4, MR-6, MB-8), `host->modulation` (IF-13: notes, CC 1, bend, pressure; MS-40's host block), `voice->modulation` (IF-3's published sources and VO-10's lifecycle), `modulation->pitch_system` (MS-22), `modulation->fx_rack` (MS-12 (c));
- cites on the existing `modulation` item: IF-13, RC-4, Appendix E's family tags.

## 7. Process notes (not spec rows)

- **Checks.** Each modulator, when ported, gets a `tools/*_check` with parity to its lab and seeded replay (the lead's process note). Two divergences the checks must allow, both deliberate: MS-5's purpose-keyed streams replace the lab's XOR words (`lfoSeed`, the 2-D random's salts), so parity is on the law with the draws injected; and MK-3 uses an exact 2π where the lab truncates it.
- **Clean-room delivery.** The round reaches the clean room only after the [OPEN M] items are ruled (memory: clean-room spec sync — both repos the same turn).
- **The lab's facilitator.** The [Xn] rows marked as the lead's can go to the human as one capped listening batch (Blind-Spot Armor), the 2-D random's travel and the S&H travel first.

## 8. After 1.0 (named, not built)

Note-on randoms; the trigger timer, its impulses and every trigger source; the MIDI gate and trigger; S&H's sample-on-trigger and the S&H mode on ORBITAL's mappings; ORBITAL's edge cushion; rails; the silent spring; a stepped destination kind; envelopes beyond ENV 1 to ENV 4; ORBITAL per-voice fields, grab and throw, persistence, distance scale and pair distances.
