# horde 2's modulation system: a draft spec for the human's ruling (B630)

> **Origin.** horde lead session, 2026-11-06, ROADMAP B630 and DECISIONS ADR-370 (the human: "yes to all, build the morph lab and start everything"). Drafted by an agent the lead dispatched; the lead reviews it. Nothing here reaches the clean room until the human rules it (ADR-370).

**Status: PROPOSAL, 2026-11-06.** Not a spec. Every clause cites the ruling it comes from. Where the record is silent, the clause is a **QUESTION** and points to section 9. Last verified against `origin/main` at `5f16cf60` and the records branch `lead-records-410` (ADR-370, B630).

**How to read it.**
- Ids `MD-1` … are proposed. No tracked file uses `MD-` with a hyphen; ADR-340 ruling 7 names a map gesture "MD1", a GUI lab label, not a requirement.
- Sources are cited as SPEC-ENGINE ids (`PA-2`), ADR and GUI rulings (`ADR-266 r2` is ruling 2), ROADMAP rows (`B70`), the GUI session's engine needs (`GN-27`, `docs/design/gui3/ENGINE-NEEDS.md`) and the two protected specs (`SPEC-INTENT-BUS`, the Map Mode spec `docs/design-system/HORDE-Map-Mode-Spec.md`).
- "Legacy behaviour (file:line)" is what the legacy plug-in does. It is evidence, never a ruling.
- "Reading" marks a GUI session's or the lead's reading the human has not ruled.
- This draft's questions are `MQ1` … (section 9); SPEC-ENGINE's own open items are written "spec Q12".
- Once ruled, the clauses become a spec round in SPEC-ENGINE's style for both clean-room repositories.

---

## 1. Scope

The engine already accepts modulation and publishes sources. Everything that computes a modulator or evaluates a route is outside it (SPEC-ENGINE §1 out of scope; 4.3; RC-4 Deferred). This draft specifies that outside part.

```
 host automation, GUI edits ──┐
 morph resolver (Appendix D) ─┼──► each key's resolved value: Param events and setting sets (IF-4, IF-12)
 intent bus: macros, X, Y ────┘      (macros and intents reach the engine only as values: RC-11.1, IF-3)
                                          │
                                          ▼  the engine smooths it (PA-1)
 THE MODULATION SYSTEM (this draft)       │
   sources ─ published by the engine per voice (IF-3)
           ─ its own: LFOs; MIDI sources the engine does not take
   routes  ─ depth, polarity ─► Σ per-voice offsets in v ──► engine: + offsets, clamp or wrap to [0, 1],
                                (PA-2; control rate PA-3;          map by the key's law (PA-2) ─► the voice
                                 per sample on PA-4's keys)
   pitch routes with Quantize ─► the pitch system (ADR-290, RC-3) ─► per-note pitch in Hz (IF-2)
```

| id | requirement | source |
|---|---|---|
| MD-1 | The modulation system computes the LFOs and any later modulator, evaluates every route, and hands the engine per-voice offsets in each key's normalised form v, addressed by note id (an offset for all voices is the same mechanism with every voice addressed). | RC-4 Accepted now and Deferred; PA-2 |
| MD-2 | Offsets reach the engine at the control rate, 1/1,500 s rounded to whole samples, ramped by the engine across one interval; PA-4's destinations also take a per-sample offset. | PA-3, PA-4; ADR-224 items 4, 5 |
| MD-3 | It reads the engine's published per-voice sources (MD-10) and computes no envelope the engine computes. | RC-4 Must not require; EN-5 |
| MD-4 | It never requires the engine to compute an LFO or an envelope beyond ENV 1 to 4, a depth outside the destination's normalised range, or a destination outside the manifest. | RC-4 Must not require |
| MD-5 | It is deterministic: seeded mulberry32 streams only, no wall-clock read, tempo and position only from RC-7's events. | charter's domain invariants; RC-7; ADR-267 r1 (the timer's determinism) |
| MD-6 | Its audio-thread work allocates nothing; every count (modulators, routes, points) is a cap fixed at prepare. | charter's domain invariants; ADR-349 r4 (32 points); B601's acceptance |
| MD-7 | Macros, X and Y are the intent bus's: their result reaches the engine as each key's resolved value, never as PA-2 offsets. The GUI lists them beside the modulators. | RC-11.1; IF-3; GUI-9 r3, r5; ADR-340 r11 |
| MD-8 | Scale quantize on a pitch route is the pitch system's: a route into a pitch with Quantize on is summed there, and the snapped pitch reaches the engine through IF-2. | ADR-290; RC-3 |
| MD-9 | This draft changes no SPEC-ENGINE requirement. Where a spec round may need one, section 8 says so (T1, T2, T4, T5). | ADR-349 r3 (ENV 1 to 4 stay as SPEC-ENGINE has them); RC-4 |

---

## 2. Sources

s is a source's raw value, in [0, 1], before a route's polarity (Map Mode spec, Model). "Per voice" sources reach only their own voice's offsets.

**Published by the engine** (IF-3; the source of MD-10):

| source | polyphony | rate | range | ruling |
|---|---|---|---|---|
| ENV 1 to ENV 4, each level | per voice | computed every sample (EN-5) | 0 to 1 (C.12 lands on 1 and 0) | IF-3, EN-5, RC-4; ENV 1 always listed (GUI-7 r1) |
| R, each oscillator (of the mixture while its swarm blends) | per voice | engine's | 0 to 1 | IF-3, SB-4; spec Q12; B253 |
| ψ, each oscillator | per voice | engine's | a phase; its mapping to s is T2 | IF-3; spec Q12 |
| the onset burst's level | per voice | engine's | its mapping to s is T2 | IF-3; spec Q12 |
| gravity's settled offset | per voice | engine's | cents; its mapping to s is T2 | IF-3; C.6 |
| velocity | per voice, held for the note (mono: the newest note's, ramped 5 ms) | at the note's start | 0 to 1 | IF-1, IF-3, VO-6, VO-8 |
| vibrato, expression, brightness, pressure (MPE) | per voice | as the host sends | 0 to 1 (`libs/clap/include/clap/events.h:200-203`) | IF-1, IF-3; spec Q2 |
| channel pressure (aftertouch) | per channel: every voice on it | as the host sends | 0 to 1 (IF-1 states none; legacy behaviour `src/hypersaw_clap.cpp:3280`) | IF-1; X21; see T9 |

| id | requirement | source |
|---|---|---|
| MD-10 | The sources above are routable, at the polyphony and range the table gives. | IF-1, IF-3 |
| MD-11 | Aftertouch's default route, in the init patch and every factory preset unless the preset says otherwise: channel and per-note pressure to `coupling` on both oscillators, +0.125 in v at full pressure. Velocity's default mapping stays each envelope's `velocity` row, not a route. | X21, RC-4 Default routes, VO-8; ADR-226 item 4 |
| MD-12 | Pressure never reaches a level unless a route maps it. | IF-1 |
| MD-13 | **QUESTION MQ11.** The mod wheel, pitch bend as a source, keytrack (note number) and release velocity. The engine takes none of them as a source: IF-1 accepts no CC1, the bend is the pitch system's, and IF-3 lists no keytrack or release velocity. | GN-14; RC-3; B82; legacy behaviour: wheel slot 15, bend slot 17 bipolar (`src/hypersaw_clap.cpp:3078-3080`) |

**The modulation system's own:**

| source | polyphony | rate | range | ruling |
|---|---|---|---|---|
| LFO 1 … n | **QUESTION MQ2** | control rate (MD-2); per sample is T7 | s in [0, 1] | GUI-37 (mock-up); ADR-349; section 4 |
| macros 1 to 8, X and Y | global | the intent bus's | the intent bus's (bipolar, rest at `macroRest`) | MD-7; ADR-192; GUI-9; SPEC-INTENT-BUS 4.4 |

| id | requirement | source |
|---|---|---|
| MD-14 | **QUESTION MQ12.** Which candidate modulators join 1.0: ORBITAL (CANDIDATE), the Kuramoto LFO, the 2-D random source, note-on randoms, the trigger timer with its impulses, the silent spring, the MIDI gate and trigger. Each is designed or prototyped; none is ruled into 1.0. | B126, ADR-165; B226; ADR-266 r8, B540; B264; ADR-267 and A1, B541; B57 |
| MD-15 | A trigger is a performance event: firing it makes no history and changes no saved state. Applies to any trigger source MQ12 admits. | ADR-267 A1 |

---

## 3. The matrix

**A route** is one record `{source, destination, depth, polarity}`, plus a Control's two ends and a pitch route's Quantize (Map Mode spec, Model; ADR-266 r2; GN-5; ADR-290).

| id | requirement | source |
|---|---|---|
| MD-16 | **Destinations:** every continuous parameter of the manifest (a row with PV "yes"). A stepped row is refused, and so is a key the resolver owns (the morph's host parameters). | IF-3, PA-2, §5.1 PV column; ADR-136; IF-11; `device.js` routeFault (reading) |
| MD-17 | Each route keeps a destination-kind field, so a stepped kind can be added after 1.0 without a format change. | ADR-267 A2 Room left in 1.0; B542 |
| MD-18 | A destination's rate is its row's: control rate, or per sample for PA-4's keys (voice pitch, each oscillator's `pitch` and `fine`, the sub's `pitch` and `fine`, the blades' `cut`, `cutHz`, `fmDepth`). A row of rate N reads its offset only at a voice's start. | PA-3, PA-4; §5.1 rate N |
| MD-19 | **QUESTION MQ8.** Destinations beyond the engine's rows: the hosted FX modules' parameters, a modulator's own settings, the morph position. | ADR-266 r3; ADR-323 r4; SPEC-INTENT-BUS 6.1; B396; B591 |
| MD-20 | **Depth** is a magnitude from 0 to 1 in the destination's normalised units; polarity carries the sign. ADR-168's signed depth is superseded. | Map Mode spec, Model; ADR-266 r2 |
| MD-21 | **Polarity**, four states. With depth d: unipolar adds s·d; bipolar adds (2s − 1)·d; inverse adds −s·d. | ADR-266 r2; Map Mode spec, Model |
| MD-22 | **Control**, the fourth: the source sweeps the parameter between a low and a high end, and the knob's own value is ignored ("controlled by" the source). At most one Control per parameter. Other routes add on top. Removing it restores the knob's value. Option+Shift-drag moves the high end. | ADR-266 r2; GUI-17; GN-5; see T4 |
| MD-23 | A new route starts at depth 25 %. Its polarity is **QUESTION MQ13** (unipolar per the Map Mode spec; the shell gives LFOs and X, Y bipolar). | ADR-261 r3; Map Mode spec, Model; `device.js` SOURCES (reading) |
| MD-24 | One route per (source, destination): a second is refused, and mapping an already-mapped source only selects its route. | Map Mode spec, Click table; GUI-3 reading; GN-20 |
| MD-25 | **Scaling:** a route moves the key's v, and the key's law maps it (PA-2). So on a `log` row a symmetric bipolar route sweeps evenly in octaves, the lopsided sweep B218 found in the legacy linear-Hz law. | PA-2; B218 (the lead's reading that PA-2 closes it) |
| MD-26 | **Order of summing** for a voice's value of a key: the resolved value (the morph's, with the intent bus's macros and X, Y already applied, clamped by it) is smoothed by PA-1; the sum of every route's contribution for that voice is added; the result is clamped to [0, 1] once, or wrapped on a circular key; the law maps it. | PA-1, PA-2; RC-11.1; SPEC-INTENT-BUS 4.5; Map Mode spec, Model ("clamp once after summing"); **QUESTION MQ10** |
| MD-27 | Opposing routes can cancel near an end, since the clamp follows the sum. | Map Mode spec, Model |
| MD-28 | **Depth routes (via sources):** a route's destination may be another route's depth, one level only: nothing lands on a depth route. Deleting a route deletes the depth routes on it. How a depth route changes a depth is **QUESTION MQ7**. | B70; ADR-340 r10; ADR-341 r5; GUI-3 r5, r7; GN-30 |
| MD-29 | **Curves per route:** **QUESTION MQ9.** Macros and intents bind with a curve (`lin`, `exp`, `log`); routes have none in the record. | IF-3; ADR-169 A3 |
| MD-30 | **Pitch quantize:** each route into a pitch destination carries Quantize. On, the pitch's result (the note and all its modulation) snaps to the global scale, with the glide quantizer's hysteresis and minimum step time. | ADR-290; B554 |
| MD-31 | **Route cap:** **QUESTION MQ14.** | legacy behaviour: 64 routes, 24 sources (`src/mod_core.h:43-44`) |
| MD-32 | Routes are not host-automatable. Making, removing, or changing a route's depth or polarity is one undo step per gesture. | Map Mode spec, Host constraints; ADR-261 review |
| MD-33 | No modulation loop can form: with depth routes one level deep and no modulator setting as a destination, there is none by construction. MQ8 reopens this if it admits modulator settings. | GUI-3 r7; legacy behaviour (`src/mod_core.h:24-28`); B396 |

---

## 4. LFOs

| id | requirement | source |
|---|---|---|
| MD-34 | **Count:** **QUESTION MQ1.** | GUI-37 reading ("four is this session's number"); legacy behaviour: 2 (`src/hypersaw_clap.cpp:3199`) |
| MD-35 | **Polyphony and retrigger:** each LFO has a retrigger switch. What it does depends on **QUESTION MQ2**. | ADR-266 r7; GN-23 |
| MD-36 | **The drawn shape:** one point list shared by the four corners, at most 32 points; each corner holds its own time, level and bend per point. Times are in cycles; a segment's bend follows C.12's curve law. | ADR-349 r1, r4; GUI-37 |
| MD-37 | Adding a point in one corner changes no sound in the others: it lands there parked, by C.12's exact split (a segment of bend u split at f becomes bends u·f and u·(1 − f)). A point parked in all four corners leaves the list. | ADR-349 recommendation (accepted with r1); GUI-37 |
| MD-38 | Under Blend and Stepped, points travel; an LFO's times blend linearly in cycles, and a list never runs backward at any position. | ADR-349 r1 and recommendation; B601 acceptance |
| MD-39 | Under Quantum, each point picks its corner (on trial; the whole-shape flip is the fallback). What smooths the output at a point's flip is **QUESTION MQ5**. | ADR-349 r2 and Open list; B601 |
| MD-40 | Corners whose shapes differ in kind (drawn against S&H or random) mix by output. Pairing points across corners is a one-time edit action, never run while the sound plays. | ADR-349 recommendation; GUI-37 |
| MD-41 | **Kinds:** drawn (MD-36), and sample and hold: one seeded value drawn each cycle and held. Further kinds are **QUESTION MQ4**. | ADR-266 r6; B237; legacy behaviour (`src/hypersaw_clap.cpp:4685`) |
| MD-42 | **Rate:** free in Hz, or synced to tempo by RC-7.4's divisions with their triplet and dotted toggles (saved per control). A free rate blends continuously between corners on a log taper; a division is stepped and flips. The Hz range is **QUESTION MQ3**. | B396 (the human: "speed - continuously if in Hz, stepped if in sync mode"); GUI-37; RC-7.4; ADR-327; B590 |
| MD-43 | **Sync and position:** whether a synced LFO's phase follows the song position and its jumps is **QUESTION MQ6**. Tempo comes only from RC-7's events. | RC-7, RC-7.2; legacy behaviour: rate from tempo, phase free (`src/hypersaw_clap.cpp:4676`) |
| MD-44 | **Start phase:** **QUESTION MQ6.** | legacy behaviour: a start phase 0 to 1 (`src/hypersaw_clap.cpp:765`, row 274) |
| MD-45 | **Settings per corner:** an LFO's settings are per corner and morph. Its points are corner state in the morph system, never host parameters. Whether its other settings (rate, sync, division, retrigger) are host parameters is **QUESTION MQ15**. | GUI-37; ADR-349 Storage; GN-49 |
| MD-46 | No envelope beyond ENV 1 to 4 in 1.0 unless **QUESTION MQ16** adds one; an added envelope would take MD-36 to MD-40. | ADR-349 r3; B229; B370 |

---

## 5. State, presets and the morph

| id | requirement | source |
|---|---|---|
| MD-47 | Routes and the X, Y assignment save with the patch. IF-4's state has no member for them; one is owed (T5). | GUI-2 r2, GUI-9 r1; GN-10; B616 |
| MD-48 | **Mappings are global for 1.0:** one route set, whatever corner owns its destination. Confirming that is **QUESTION MQ17**. | ADR-261 r3; ADR-266 r4; Map Mode spec, Open decisions; GN-27 (open) |
| MD-49 | A modulator's settings morph between corners (MD-42, MD-45); its routes do not, under MD-48. | GUI-37; ADR-349; MD-48 |
| MD-50 | A modulator with no route keeps its settings or loses them: **QUESTION MQ18**. Removing a modulator removes its routes as one undo step, with a line naming them and Undo. | GN-39; ADR-338 r9 (M-R2) |
| MD-51 | A route's identity, if the morph ever keys routes per corner, is (source, destination). **Proposal, not ruled.** | B72 (the lead's proposal); GN-20 |
| MD-52 | An LFO's phase and random stream are performance state: never saved, reset at a load. **Reading**, by analogy to IF-4; see T6. | IF-4 (live state never saved); RC-11.1; B198 |
| MD-53 | Firing a trigger makes no history (MD-15); editing a modulator or a route is an edit, one undo step per gesture. | ADR-267 A1; Map Mode spec, Host constraints |

---

## 6. CPU

**The budget.** PF-2 gives modulation 2 % of a min-spec core, beside sources 34 %, FX 12 %, master 1 %. The morph and the intent bus share that slice (`docs/proposals/module-1.0-bar.md` line 104), and so does Arp (ADR-269 r2). At PF-1's assumed ×1.5 that is about **1.3 % of an M3 core**: one eighth of the **10.7 %** ADR-354 gives one swarm oscillator. The engine's own cost of applying offsets is in the sources' 34 %, since PF-2 measures with per-voice modulation running on every continuous parameter of both oscillators.

**The cost model. An estimate: every unit cost below is assumed and nothing is measured.**

| term | count per second | assumed unit cost (M3) | share of an M3 core |
|---|---|---|---|
| per-voice LFOs: 4 × 8 voices, at 1,500 Hz | 48,000 | 30 ns (segment search, one exp for the bend) | 0.14 % |
| routes: 64 × 8 voices, at 1,500 Hz | 768,000 | 3 ns | 0.23 % |
| audio-rate routes on PA-4's keys, interpolated: 4 × 8 voices × 44,100 | 1.4 M | 3 ns | 0.42 % |
| the same 4 LFOs evaluated per sample instead | 1.4 M | 30 ns | 4.2 % |

- Under these assumptions the first three rows total about 0.8 % of an M3 core, inside the 1.3 % before the morph, the intent bus and Arp take theirs.
- **Evaluating LFOs per sample would be about three times the whole slice.** So T7 matters: an LFO into an audio-rate key should be interpolated between control points, not evaluated per sample.
- Global LFOs cost 1/8 of the per-voice row. PF-1 counts 8 voices; at 16 (PF-5's default) every per-voice row doubles.
- PF-10 leaves the modulation system out ("each has its own slice"). A reported cell for it is the lead's to propose (T10).

| id | requirement | source |
|---|---|---|
| MD-54 | The modulation system's CPU is reported against its slice, never a gate. | PF-2, PF-3; ADR-269 r2 |

---

## 7. The FX device

The FX device hosts the same matrix over the DAW's audio (ADR-370; B528). Every modulation source declares its kind: voice-bound, audio-derivable or free (ADR-254 checklist).

| source | kind | FX-device stand-in | ruling |
|---|---|---|---|
| LFOs | free | retrigger by a sidechain note-on; sync from host tempo | `docs/audits/2026-10-21-fx-device-audit.md` (b); ADR-349 (same law) |
| ENV 1 to 4 | voice-bound | one C.12 envelope per sidechain note (EN-5 builds it with no voice dependency); ENV 1 also an envelope follower | EN-5; audit (b) |
| velocity, MPE expressions, pressure | voice-bound | the sidechain's (RS-4.11 carries no expression events yet) | audit (b) |
| R, ψ, onset burst, gravity offset | voice-bound | none (onset: a transient detector) | audit (b) |
| macros, X, Y | free | none needed | audit (b) |
| mod wheel, pitch bend (if MQ11 admits them) | free (MIDI) | sidechain CC and bend | audit (b) |

| id | requirement | source |
|---|---|---|
| MD-55 | The modulators' and the routes' host ids, if any (MQ15), form a block of their own, append-only, so a second shell exposes them unchanged. | ADR-254 checklist |
| MD-56 | The Mod page and the matrix assume no oscillator panel exists. | ADR-254 checklist (Screens) |
| MD-57 | In the FX device a route reaches only the rack's parameters, through declared mod values; Quantize applies on any FX-module pitch destination. | ADR-254 checklist; ADR-290 FX device |
| MD-58 | Per-voice LFOs (if MQ2 makes them) are instrument-only: in the FX device an LFO runs once, or once per sidechain note. | ADR-254 checklist ("per-voice processing is marked instrument-only") |

---

## 8. Technical gaps for the lead (not for the human's poll)

| # | gap | where |
|---|---|---|
| T1 | IF-3 publishes "the levels of ENV 1 to ENV 4" and VO-8 scales each level by its velocity factor. Which level is the source: before or after velocity? | IF-3, VO-8 |
| T2 | ψ (a phase), gravity's offset (cents) and the onset burst need a stated map to s in [0, 1]. R already lies in it. | IF-3, spec Q12 |
| T3 | A route reading an engine source (an envelope, R) and writing that voice's offset: in the same control interval, or one late? The legacy core recorded a one-tick delay for its control graph. ENV → the sub's pitch at audio rate (PA-4's "so a pitch envelope on the sub is smooth") needs per-sample reads and writes inside one block. | PA-4; `src/mod_core.h:24-28`; ADR-255 r7 |
| T4 | A Control route under PA-2: the offset must be (target − the resolved value). PA-1 smooths the resolved value over about 12.8 ms while the offset ramps over one interval, so a base change under Control leaves a transient unless the modulation system tracks PA-1's smoother exactly, or the engine gains a per-voice override. | ADR-266 r2; PA-1, PA-2 |
| T5 | State members for routes, the X, Y assignment, and modulator settings and points (IF-4 knows only `version`, `params`, `ratios`). RC-9.3 keeps unknown members, so a later reader is safe. B616's state round. | IF-4, RC-9.3; GN-10, GN-37 |
| T6 | MD-52 is a reading. Legacy saved its LFO streams in the chunk (`lfo=`), and B198 asked whether "a load is a load" covers a stream's identity. | IF-4; B198 |
| T7 | An LFO into a PA-4 key: interpolate control-rate values per sample (cheap) rather than evaluate per sample (section 6). Legacy stepped at its tick, a recorded limit (`src/hypersaw_clap.cpp:3188-3198`). | PA-4; B171 |
| T8 | Pitch has two paths: a Quantize route sums in the pitch system (ADR-290), an unquantized one is a PA-4 offset on voice pitch. ADR-290 snaps the whole pitch when any route into it has Quantize on, so the system needs one place that knows every pitch route. | ADR-290, PA-4, IF-2 |
| T9 | Channel pressure without MPE is per channel; PA-2 is per voice. Every voice on that channel takes the same offset. | IF-1, PA-2 |
| T10 | A reported CPU cell for the modulation system, like PF-10: the section-6 load at 8 and 16 voices. | PF-10; ADR-370 (the cost kept visible as features land) |

---

## 9. Questions for the human

Each has a recommendation only where the record gives evidence. "You'd hear" says what the choice does to the sound or the playing.

**MQ1. How many LFOs?**
- (a) Two, as the legacy plug-in has. Fewest controls; patches with three moving things need an envelope or a macro.
- (b) **Four** (recommended). The GUI 3 mock-up already shows four in the Mod page and the bank (GUI-37), and the CPU estimate fits (section 6).
- (c) Eight. Room for dense motion; the bank and the CPU slice get tighter.

**MQ2. Does each note get its own LFO wave, or do all notes share one?**
- (a) One shared wave for all notes (legacy). A chord wobbles together. With retrigger on, every new note restarts the wave for the notes already held.
- (b) Each note its own wave. With retrigger on, each note's wobble starts with that note; off, each runs free. Costs 8 to 16 times more CPU than one shared wave (section 6).
- (c) **A switch per LFO** (recommended): shared, or per note. Evidence: the legacy shared envelope that restarted on every strike made held notes "blip", which ADR-162 fixed by making it per note; the engine already takes per-note offsets (PA-2) and its CPU budget assumes per-note modulation (PF-2).

**MQ3. How fast can a free LFO run?**
- (a) **0.02 to 40 Hz** (recommended), the legacy range: from slow sweeps to fast wobble.
- (b) Wider, into audio rate (growls and FM-like buzz). Smooth only on the keys that take audio-rate modulation (pitch, blade cut, FM depth: PA-4); elsewhere the engine follows it only 1,500 times a second, which smears it. Costs more.

**MQ4. Which LFO kinds besides drawn shapes and sample and hold?**
- (a) Those two only.
- (b) **Also a smoothed random**, as built in the modulator lab: stepped, linear, cosine or cubic glides between random values (B237). Recommended: you asked for it on 2026-09-23 and it was built, but you haven't heard and ruled it.
- (c) Also the legacy shapes (sine, triangle, saw up, saw down, square) as ready-made drawings to start from. Can go with (a) or (b).

**MQ5. Under Quantum, when an LFO point jumps to another corner's shape, what smooths the jump?**
- (a) Nothing beyond the engine's 0.67 ms ramp. A large jump can click.
- (b) **The morph's own glide** (`morph.glide`, IF-11) (recommended). It's the setting that already smooths every other Quantum flip.

**MQ6. Where does a synced LFO's wave start?**
- (a) Free: the rate follows the tempo, but where the wave sits against the bar depends on when you started (legacy).
- (b) **Locked to the song position**, so the same bar always meets the same part of the wave, and a loop or locate restarts it there (recommended: the host's position and its jumps already arrive as events, RC-7, and the morph's synced steps use them).
- Either way: a **start phase** knob (legacy had one), yes or no?

**MQ7. A depth wire (one modulator turning another's amount up and down): how does it act?**
- (a) **It adds to the amount, and stops at zero** (recommended). Mod wheel into a vibrato's amount set at 0 gives "vibrato as you push the wheel". Keeps the rule that an amount is a size and the polarity carries the direction (ADR-266 r2).
- (b) It adds, and past zero the modulation comes back upside down.
- (c) It scales the amount: wheel at 0 silences the vibrato whatever its amount, full gives the amount you set.

**MQ8. What else can a route move, beyond the engine's own knobs?**
- (a) Nothing else in 1.0.
- (b) Also the FX modules' knobs. ADR-254 already expects declared mod values into the rack; how they draw is B591.
- (c) Also a modulator's own settings (an envelope speeding up an LFO). This can form loops; the corner-mapping lab's loop rules (B396) must be ruled first.
- (d) Also the morph pad's position (an LFO sweeping the morph). The lab's question 5 (does the pad count in a loop) must be ruled first.
- Recommendation: (b) for 1.0. Your note on B396 and ADR-323 r4 both expect (c) eventually; it waits on the loop rules.

**MQ9. Can a route bend its response?**
- (a) No: a route moves the knob straight along its own scale.
- (b) Yes, `lin`, `exp`, `log`, the same three curves macros already have (IF-3). An envelope into a cutoff can then bite early or late.

**MQ10. When a macro pushes a knob to its end, can an LFO still pull it back?**
- (a) **The knob's end is a wall for macros; routes then move from there** (recommended). A macro past the end does nothing more, and an LFO's dip is always heard. The record forces this today: a macro reaches the engine as the knob's value, in range (RC-11.1).
- (b) One shared headroom: a macro past the end stores the excess, so an LFO dip is heard only once it exceeds it. Needs RC-11.1 changed.

**MQ11. Which of the keyboard's controls are modulation sources?** Mod wheel, pitch bend (it still bends pitch as well), keytrack (which key you played), release velocity.
- (a) All four.
- (b) **Mod wheel, pitch bend and keytrack** (recommended: the legacy plug-in had the wheel and the bend, and the bank's macro rows already show both, ADR-340 r11; keytrack was planned for per-note modulation, B82).
- (c) None in 1.0.

**MQ12. Which of the new modulators join 1.0?** ORBITAL (gravity bodies), the Kuramoto LFO, the 2-D random source for XY pads, note-on randoms, the trigger timer and impulses, the silent spring, the MIDI gate and trigger.
- (a) None: they follow after 1.0.
- (b) Some, which you name. Each needs its own lab ruling first (ORBITAL's velocity and the 2-D random's modes are still in the lab; the linked note-on random is undefined, B264).
- No recommendation: the record has no ruling on 1.0's modulator roster.

**MQ13. A new mapping starts as:**
- (a) Always unipolar, at 25 % (the Map Mode spec's default).
- (b) **The source's natural shape** (recommended): LFOs and the XY axes bipolar, so a new mapping wobbles around where the knob is and an XY axis at rest moves nothing (GUI-9); envelopes, velocity and macros unipolar.

**MQ14. How many routes at most?**
- (a) 32. (b) **64** (recommended: the legacy plug-in's cap; nothing in the record asks for more). (c) 128.

**MQ15. Can the host automate an LFO's rate and other settings?**
- (a) **Yes: they are host parameters in a block of their own, and morph like any knob** (recommended: ADR-254 already plans a modulators' id block; GUI-37's rate blends between corners like a knob).
- (b) No: they are patch state only, like the drawn points (ADR-349).

**MQ16. Envelopes beyond ENV 1 to 4 in 1.0?**
- (a) **No** (recommended: ENV 2 to 4 already reach any knob, and ADR-349 r3 kept them as they are).
- (b) Yes, drawn envelopes that morph like the LFOs (ADR-349's law).

**MQ17. Mappings while morph is on.**
- (a) **One set of mappings for all corners in 1.0, as ruled** (ADR-261 r3, ADR-266 r4) (recommended). Per-corner mappings come after 1.0 with the corner-mapping lab's five questions.
- (b) Per corner in 1.0. The lab's five questions (B396: blending a mapping only some corners have, depth past zero, loops between corners, a preset that would close a loop, the pad in loops) must be ruled first.

**MQ18. A modulator with no mappings: does the patch keep its settings?**
- (a) **Yes, until you remove it** (recommended: removal is already an explicit act, M-R2, ADR-338 r9; dropping a shaped LFO when its last wire goes would lose the drawing).
- (b) No: an unmapped modulator is dropped when the patch saves.

---

## 10. Where the record contradicts itself

1. **ADR-330's drafted rows don't exist.** ADR-330 says the LFOs, the matrix and device settings "are drafted for B586's one table"; ADR-370 and B630 cite those drafted rows. No such rows are on `origin/main`: not in `docs/design/gui3/params.json`, `layout.json`, `tools/gen_gui3_controls.py`, `engine2/structure/` or `docs/port/`; `port-tab.json` still says "B586 drafts one". This draft is written from the other sources.
2. **Mapping scope.** ADR-261 r3, ADR-266 r4 and the Map Mode spec make mappings global for 1.0. SPEC-INTENT-BUS 6.1 and 6.2 make corner scope the default, and ADR-192 binds named macros per corner. GN-27 and GUI-9 list it open; B396's lab designs per corner and leaves 1.0 open. (MQ17.)
3. **Where the clamp sits.** The Map Mode spec clamps once after summing. SPEC-INTENT-BUS 4.5 clamps at the corner's range, adds global routings, then clamps to [0, 1], with an optional re-clamp per routing. SPEC-ENGINE RC-11.1 delivers macros as values before PA-2's own clamp. So there are at least two clamps. (MQ10.)
4. **A new mapping's polarity.** The Map Mode spec defaults to unipolar; `device.js` gives the source's own (LFOs and XY bipolar). The second is a shell reading, not a ruling. (MQ13.)
5. **Two routes, one pair.** The legacy SUM law kept one canonical entry per (source, destination) (ADR-138); ADR-168 split it to one entry per route to carry polarity. The Map Mode spec and GUI-3's reading allow only one route per pair, and B72's proposed identity (source, destination) assumes one. (MD-24.)
6. **A modulator's settings as a destination.** Legacy refuses them (`src/hypersaw_clap.cpp:4118-4134`). ADR-323 r4 says "a modulator of a modulator shows on the page where its target lives", so it presumes they can be. SPEC-INTENT-BUS 6.1 allows it at global scope only. (MQ8.)
7. **A synced LFO and RC-7.3.** RC-7.3 says no sound of the engine depends on the transport except the morph's synced steps, tested by a bit-identical render. A synced LFO is outside the engine, so the engine test stands, but the instrument's sound will depend on the transport. A spec round should say so.

---

## 11. Counts

| kind | count | clauses |
|---|---|---|
| cited to a ruling or the spec | 36 | the rest |
| cited to a reading or proposal, open to overrule | 3 | MD-25, MD-51, MD-52 |
| a ruled part plus an open QUESTION | 11 | MD-23, MD-26, MD-28, MD-35, MD-39, MD-41, MD-42, MD-43, MD-45, MD-48, MD-50 |
| a QUESTION only | 8 | MD-13, MD-14, MD-19, MD-29, MD-31, MD-34, MD-44, MD-46 |
| **total** | **58** | MD-1 to MD-58 |

Section 9 puts **18 questions** to the human; section 8 lists **10 technical gaps** for the lead.
