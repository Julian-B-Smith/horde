# The wavetable member in the plug-in: the integration inventory

> **Origin.** Drafted for the horde lead session, 2026-11-08, on ROADMAP B656 and ADR-387 (the human, by poll:
> "Replace, for 1.0" and "In 1.0"; and: "let's run through everything that will have to be figured out for a C++
> version to wire into the plugin (morph & mod contracts, randomization, gui needs)"). ADR-387 sits on branch
> `lead-records-421` (commit 5bf3738b) at the time of writing, not yet merged.
> Companions: `specs/SPEC-WAVETABLE.md` (what the oscillator does), `docs/wavetable/CONTRACT.md` (the seam and the
> build), and B657's `docs/wavetable/TEST-PLAN.md` (the tests, in progress on its own branch). This file does not
> repeat them; it lists what else has to change for the wavetable member to play in the plug-in, who changes it, in
> what order, and what the human still has to rule. **Section 12 is the one list of decisions.**

**Status.** DRAFT for the lead and the human. Nothing here is ruled by being written down. Claims about code cite a
file and line at the commit named; "unverified" marks what was not checked.

**What was read.** SPEC-ENGINE (sections 4, 5, 6.6 to 6.8, 7, 9, Appendices B, D, E) and SPEC-WAVETABLE on
origin/main (c85c5e03); CONTRACT in full; DECISIONS ADR-247, ADR-254, ADR-374 (A1, A2), ADR-376 to ADR-387, GUI-30,
GUI-38, GUI-55; the lab `docs/wavetable/lab/wavetable-lab.html` and its traces (B650, B651, B652); the B648 cost bench
traces; the clean-room engine at `e72a092` (`~/Documents/Claude/cleanroom-engine`, read only) and its `ANSWERS.md`;
the test plug-in `tools/engine-preview/` (README, `bridge_core.h`, `preview_plugin.cpp`, `build.sh`,
`demo_bank.json`); `docs/design/gui3/` (`layout.json`, `params.json`, `ENGINE-NEEDS.md`); the random roles lab and
its generator; `reference/scalpel/data/presets.json`.

**Owners named below.** *Lead*: the horde lead session (specs, ROADMAP, the bridge and the test plug-in, gates,
measurements). *GUI session*: chartered by `docs/design/CLAUDE.md` (owns `docs/design/**`, GUI-n rulings, GN rows).
*Wavetable builder*: the one builder of `~/Documents/Claude/horde-wavetable` (ADR-385). *Clean-room builder* and
*clean-room test author*: the two sessions of `~/Documents/Claude/cleanroom-engine` and `~/Documents/Claude/cleanroom-tests`.
*The human* rules.

---

## 1. What ADR-387 changes, in one page

ADR-387 closed the two questions the wavetable contract left for later: the wavetable member **replaces** the blades
for 1.0 (CONTRACT §7 A1), and the plug-in has **three oscillators** in 1.0 (CONTRACT §7 H9). Everything below follows
from those two rulings and from the lab the human played.

**The five things that matter most:**

1. **The budget is not settled, and the numbers on file disagree.** WT-BUDGET is 4 % of an M3 core per oscillator
   (ADR-385, provisional). SPEC-WAVETABLE WT-P7 quotes the first bench run at 3.9 %. The B648 follow-up, which
   measured the design as specified, counts **13.3 % per oscillator** (Spectral log, 6-point read, moving; 10.2 %
   for Linear with a per-voice frame), by an uncalibrated instruction count, and **was never timed**
   (`traces/2026-11-07-b648-quadrant-cost-bench-followup.md`, results table and caveats). ADR-387 cites "about 13 %".
   On top of that, PF-2's envelope (the sources' 34 % of a min-spec core) spread over **three** oscillators instead of
   two leaves about **7.1 % of an M3 core per whole oscillator** instead of PF-1's 10.7 % (section 2.6). The first
   timed run at S0 decides how hard this is (decision 7).
2. **Every sound that exists today is a blade sound.** All 83 factory presets have blade 1 on (`w` > 0 in every
   preset of `reference/scalpel/data/presets.json`; 42 have blade 2 on). 14 of the 16 demo presets use blades
   (`tools/engine-preview/demo_bank.json`; only Bamboo Flute and Rainglass have no blade rows). PF-1's reference
   patch, Glass horde pad, is a blade patch. So the bank, the CPU anchor and the clean room's bank tests all have to
   move, and the blade code cannot leave the engine until they have (section 6.5, decisions 21 and 22).
3. **horde 2 has no "revision gate" of the legacy kind.** ADR-387 says the blade rows "retire behind the revision
   gate". The revision gate is the legacy shell's (ADR-157, `engine_revision`). SPEC-ENGINE's equivalent is the state
   document's `version` and a migration (IF-4, RC-9.4). Blades going inert changes the meaning of existing keys, so it
   is a version change with a migration from version 1 (decision 4).
4. **The lab's simpler Spectral-log system cannot be ported as it stands.** The human liked the lab's pre-blended
   frames (B651: "There must be a simpler system for spectral log than recalculating the tables on every note"),
   which gives every voice its own frame under both laws. The lab builds those frames on a worker thread and lets a
   voice read the nearest built frame until its own arrives. That is not deterministic (DT-1, DT-2, WT-18), and under
   motion it builds up to 16 frames per rebuild, each with its own inverse FFTs. It needs a deterministic design on
   the audio thread and a cost measurement before it is adopted (decision 6).
5. **Every table generator must exist in C++.** A saved patch stores tables as recipes (WT-42), and the engine
   rebuilds them from the recipe. So the eight factory generators and every one of TABLECASTER's ten generator
   engines (ADR-386) need a C++ version with frozen goldens per version (WT-43). TABLECASTER is a port, not a
   hook-up (decision 18).

**What is unchanged.** The swarm's laws (SW, C.1 to C.8), the voice (VO), the envelopes (EN, C.12), the sub and the
noise (SN), the output stage (OU), the morph resolver's laws for every other key, and the modulation system's laws.
Members stay mono and panned (IF-8). There is still one engine (GUI-1): nothing anywhere offers "wavetable" and
"blades" as engines to choose between.

---

## 1a. Where the lab and the spec part ways

The lab (`docs/wavetable/lab/wavetable-lab.html`, B650 to B652) is what the human played and likes. Where it differs
from the spec, this table says which way the C++ should go. A lab choice the ear can hear is evidence of what the
human wants; a lab choice made for browser CPU is not.

| lab choice (where) | spec today | which way the C++ should go | ruling needed |
|---|---|---|---|
| Spectral log pre-blends all 16 frames, so the frame is per voice under both laws; a "lazy" mode builds only the frames voices read (B651; `wavetable-lab.html` lines 505–541, the switch at 1476) | WT-22: under Spectral log the frame is global, the newest held voice's | **The lab's model**, built deterministically on the audio thread, if S4's timed run affords it; WT-22 as the fallback. It removes CONTRACT §7 B2 (no per-note route to a shared frame remains) | decision 6 |
| FM section: one **source** selector (Internal, Osc 2, Self) with one index (lines 1003–1013) | ADR-385 A1's matrix: a depth per other oscillator, plus the internal modulator's own depth (WT-61, WT-70) | **The spec's matrix**: ruled by the human after the lab, and it lets osc 2 and the internal modulator act at once | none (ruled) |
| **FM mix**: a second, unmodulated read mixed in (SCALPEL's blade depth; Operator jitter uses 60 %) | absent | **Add it** as `oscN.wt.modMix` (0..1, default 1, Morphable); the second read costs only below 1 | decision 8 |
| Mod ratio ×1/16..×32, Hz 0.1..20 000 | WT-70 proposed ×0.25..24 (the blades' range), 1..20 000 Hz | **The lab's ranges**: its presets were voiced on them, and 0.1 Hz reaches vibrato rates | decision 8 |
| Per member: Decorrelated or Shared (a two-way switch) | `wt.modDecorrelate`, continuous 0..1 | **Continuous**, so it morphs (ADR-337 A1); A/B against the lab's switch at S5a | decision 8 |
| Self: PM from the member's own last two outputs, index clamped at 1.6 rad | the diagonal is C.14's `feedback` (WT-61, CONTRACT §7 H4) | **The spec's** (one loop concept); `s5b-4-feedback` lets the human compare | decision 10 |
| Linear pre-blends 16 frames by weighted sums of the corners' level sets (strategy (b)) | CONTRACT §7 C9 recommends strategy (a), read-time | **Measured at S4**, as planned: inaudible, so the lab's choice is not evidence | none (C9) |
| 4-point cubic read by default (line 1478), for browser CPU | 6-point Lagrange (WT-6) | **The spec's**: WT-F1 judges | none |
| 2× FM oversampling switch (line 1479) | `quality` High (WT-37); no opt-out for table members (CONTRACT §7 E3) | **The spec's**: it is the same 2× | none |
| Sync restart corrected by a polyBLEP; Structural flips step at a block edge | ED-1/ED-6 corrections; PA-5's 5 ms crossfade | **The spec's**: the lab's are browser shortcuts | none |
| Two oscillators only (the lab predates B654) | three, with the matrix | **The spec's**: S5b's clips are where the human first hears three | none |
| Every setting a corner value; drawn LFOs whose points travel (ADR-349, LF-8); only LFOs move the pad (MS-29) | the same | the same | none |

---

## 2. SPEC-ENGINE changes

All rows here are drafted by the lead into one or more spec rounds (section 11, step 9), reviewed by a critic and
ratified by the human; the clean-room builder implements them and the clean-room test author tests them. CONTRACT §8
lists the rows a port amends; this section adds what ADR-387 brings and what CONTRACT §8 missed.

### 2.1 The member chain

| row | what changes | where | owner | depends on | open / recommendation |
|---|---|---|---|---|---|
| §1, §2, §4.2 | "two swarm oscillators … carry blades" becomes three swarm oscillators whose members read wavetables; the signal-path picture loses "base wave + blade 1 + blade 2" and gains the table read, its transformations and the PM matrix | SPEC-ENGINE lines 19–80, 130–160 | lead | ADR-387 | none |
| C.0 | unchanged: its raised cosine is reused by WT-5, WT-15 and WT-62 | Appendix C | — | — | none |
| C.9 to C.11 (the blade, spreads' blade parts, interplay) | **retired**: kept as history in the spec, marked "retired, 1.0 (ADR-387)", read by no new patch | Appendix C | lead | decision 22 for when the code leaves | the laws stay in the text until the code is deleted, so the bank tests that still run against blades have their law |
| C.10 (spreads) | kept for the frame spread: `wt.frameSpread` uses the oscillator's spread law (WT-22, BL-13) | C.10 | lead | — | none |
| C.13 (the frame) | kept: `oscN.psi` is measured against it (MS-8) | C.13 | — | — | none |
| C.14 (cross-mod and feedback) | xin pushes a table member's read phase (WT-31); a new row for the PM matrix (WT-61 to WT-66) | C.14 | lead | D2's stability map (CONTRACT §7 D2) | as CONTRACT §8 |
| C.16 (DC) | the loop's blocker condition includes table members; the means of WT-25 replace the blades' estimate | C.16 | lead | — | as CONTRACT §8 |
| BL-1 to BL-19 | **retired** with C.9; BL-13's spread law stays (frame spread) and BL-5's FM laws are quoted by WT-61 and WT-70, so those two are restated in the new rows rather than cited | §6.2 | lead | — | none |
| ED-1 to ED-7 | kept: the sync restart is an edge (WT-24, WT-32); the base wave's own edges go with the base wave | §6.3 | lead | — | none |
| SW-13 | unchanged (the band-edge fade applies to table members, WT-24) | §6.1 | — | — | none |
| VO-9, RC-6.1, RC-6.2 | **not in CONTRACT §8.** RC-6.1 says the voice's code names no source kind; RC-6.2 says a source reaches no other source's state except through what the voice hands it. The PM matrix makes the voice render oscillators in a fixed order and hand each one the others' member outputs (WT-62 to WT-66). Amend RC-6.2 so the voice hands an oscillator source the PM taps of the other oscillator sources, and RC-6.1 so the render order and the PM buffers belong to the voice, keyed by slot index, not by source kind | SPEC-ENGINE lines 745–746 | lead | — | approve (no ruling needed beyond the round) |
| RC-6.4 | its test build's "fifth source" (a sine at 0x6000) becomes a sixth | §7.2 RC-6.4, item 18 | lead; test author | decision 1 | none |

### 2.2 The manifest (§5)

**New rows on osc 1 and osc 2** (CONTRACT §2.1): eleven `wt.` rows at offsets 0x030 to 0x03A, the internal
modulator's at 0x03B onward once ruled. With three oscillators in 1.0, `osc1.wt.pm3` (0x103A) and `osc2.wt.pm3`
(0x203A) are **live**, not reserved as WT-69 has them at K = 2.

**Osc 3's block.** The oscillator formula `pid::osc(o, row) = 0x1000·(o + 1)` (clean-room
`engine2/core/manifest/manifest.h`, comment lines 6–8) gives 0x3000 for o = 2, which is the sub's. Osc 3 needs a
block of its own. **0x8000 is free**, re-checked for this inventory:

- the clean-room manifest's blocks are voice 0x0001–0x0012, osc 1 and osc 2 at 0x1000 and 0x2000 (with blades at
  +0x100 and +0x200 and `stream` at +0x300), sub 0x3000, noise 0x4000, ENV 2 to 4 at 0x5000 to 0x5209, and two
  test-build blocks, `sine` at 0x6000 and `kTestAdded` at 0x7000 (`manifest.h` lines 71 and 77);
- `git grep` of 0x8000 to 0x8FFF over horde's `tools`, `docs`, `engine2` and `specs` finds only CONTRACT's own
  mention, and a grep of the clean-room `core` and `tests/harness` finds none;
- `docs/design/gui3/params.json` holds 330 rows, none at or above 0x6000.

**Proposed layout** (decision 2): osc 3 keeps osc 1's offsets so one row has one offset in every oscillator block:
`osc3.on` 0x8000 (osc 1's `on` is offset 0x00, `kOscOn`), its oscillator rows at their usual offsets, the `wt.`
rows at 0x8030 to 0x803A (`osc3.wt.pm1` at 0x8039, `osc3.wt.pm2` at 0x803A), the internal modulator from 0x803B,
`stream` at 0x8300 (so item 12.8's "stream is its block's highest id" holds). **Osc 3 gets no blade blocks and none
of the six oscillator rows only blades use** (`baseWave`, `quantize`, `stacking`, `upperHears`, `collisionPitch`,
`collisionBite`) and no `ratios` text: their offsets stay empty. Reserving rows for a feature osc 3 never had would
be inventing. **`mix.osc3`** takes the voice block's next free id, 0x0013 (`kQuality` is 0x12, `manifest.h`).
Before the round claims 0x8000 it must fix the morph's and the modulators' lockfile blocks, which IF-11 and MS-40
append after the engine's with no numbers yet (CONTRACT §10).

**Counts, the lead's arithmetic, re-derived by the round** (decision 3 assumed):

| | today (5.8) | after |
|---|---|---|
| manifest rows | 330 | about 428 (431 with `wt.modMix`): + 22 `wt.` rows on osc 1 and 2, + 14 internal modulator rows, + 61 for osc 3 (its `on`, 42 oscillator rows, 11 `wt.`, 7 modulator), + `mix.osc3` |
| reserved | 13 | 15 (osc 3's `pitchMode` and `stream`) |
| retired (new class word) | 0 | 136 (osc 1 and 2: 62 blade rows each, and the six blade-only oscillator rows each) |
| host parameters | 317 | about 277 (280) |

**A consequence to know** (CONTRACT §2.2): `manifest_param(i)` is in id order, so the rows at 0x1030 and 0x2030
shift the index of every later row. Osc 3 at 0x8000 comes after the test blocks and shifts nothing of theirs. Any
clean-room test that seeds or iterates by index is reseeded by key (S7's gate).

### 2.3 The retired blade rows

| question | recommendation |
|---|---|
| Delete, reserve, or a new class? | ADR-387: "ids are frozen and never deleted" (RC-9.1). **A new class word, `retired`, that shares reserved's mechanism** (5.1): not a host parameter, kept in a state byte for byte and read as inert, a `Param` addressed to it refused and counted by PA-7. "Reserved" means "designed, not yet built" and its v = 0 is a later default; a retired row is the opposite, so it gets its own word and its own count in 5.8 (decision 3). |
| Shown? | No. Not in any GUI page, the Random lab, or the host's list. A load report names any retired row a patch set away from its default (section 6.4). |
| Which rows | both blades of osc 1 and osc 2 (5.4, 62 rows each), the six blade-only oscillator rows, and the `ratios` text (5.3's oscillator state). `crossMod`, `feedback`, `loopDrive`, `spreadLaw`, `spreadDrift`, `motionGrain` and `bandLimit` stay live for table members (CONTRACT §2.4). |
| PA-8 | the three loop rows' condition "a blade is on in Sync, FM or Ring" (SPEC-ENGINE lines 590–592) becomes "the oscillator is on" (WT-31) |
| The `ratios` path | `oscN.ratios` reaches the engine as a text and the resolver as each corner's two texts (IF-12; `set_corner` in clean-room `morph/resolver.h` line 22). Keep the inputs (append-only), stop using them. The bridge's ratios plumbing (`preview_plugin.cpp` lines 177, 551, 575, 826, 1385, 2379) is the template for the corner tables, which also travel per corner per oscillator. |

### 2.4 Mixer and sources

| row | change | owner | open / recommendation |
|---|---|---|---|
| 5.2 | `mix.osc3` (0x0013, dB, −∞..+6, default 0, M, A, PV); `osc3.on` default **off** (CONTRACT §7 H3) | lead | approve H3 |
| IF-6 | osc 3 gets mute, solo, a meter and a clip latch. Clean-room `api/engine.h` line 137 holds `gui_peak_[5]` (four sources and the output): six | lead; clean-room builder | none |
| VO-9, RC-6 | osc 3 is the slot's fifth instance (RC-6.1); RC-6.3 holds: off costs nothing | lead | none |
| RC-2 | "a fifth source's place in the mixer" (RC-6's Deferred list) is now osc 3's | lead | none |
| PA-4 | `osc3.pitch`, `osc3.fine`, `mix.osc3` audio-rate as their siblings | lead | none |
| the clean-room code | every array sized for two oscillators or four sources: `OscSettings osc[2]` (`voice/voice.h` line 137), `Swarm osc_[2]` and `xf_[2]` (lines 253, 271), the per-source arrays sized 4 in `api/engine.h` (lines 89–125), the resolver's `enum Source { kOsc1, kOsc2, kSub, kNoise, kSources }` (`morph/resolver.h` line 53), the harness header's `kSourceOsc1 … kSourceNoise, kSources = 4` (line 127) and `OscView osc[2]` (line 369). **Osc 3 is appended as source 4** (`kSourceOsc3 = 4`, `kSources = 5`), never inserted, so the existing source numbers keep their meaning | clean-room builder; test author (header) | none |

### 2.5 True stereo (IF-8)

No change. A table member is mono and panned by C.8, as every member is. The PM taps are member outputs before the
pan (WT-62), so no modulator reads a channel. FX device: none.

### 2.6 Quality and CPU (IF-9, PF-1, PF-2, PF-7)

| row | change | owner | open / recommendation |
|---|---|---|---|
| IF-9 | new counters (CONTRACT §1.9): rebuilds, levels built, table reads, sync edges, peak-limit and gain-cap engagements, tables refused, swaps applied, **back-edge clamp engagements (WT-65) and interleaved segments (WT-64)** | lead | none |
| `quality` | table members run at the internal rate like every member, no opt-out (CONTRACT §7 E3) | — | approve E3 |
| PF-1 | its anchor, Glass horde pad, is a blade patch and retires with the bank. Re-anchor PF-1 to WT-P2's judged cells (cell 2 for one oscillator, cell 5 for three), whole oscillator, swarm laws included | lead | decision 7 |
| PF-2 | "two swarm oscillators" becomes three. **The arithmetic:** PF-1 is (34 % − 2 % for the sub) ÷ 2 oscillators ÷ 1.5 (M3 to min-spec) = 10.7 % of an M3 core. Over three oscillators it is 32 % ÷ 3 ÷ 1.5 = **7.1 % per whole oscillator**, of which WT-BUDGET's 4 % is the table member. The B648 follow-up counts the table member alone at 10 to 13 % | lead; human | decision 7: keep PF-2 judged with **two** oscillators on (as today's wording), report all three on beside it as PF-10 reports the ambitious patch, and rule PF-2 again with S5b's timed numbers |
| PF-2 (round 62) | PA-4's list grows again (`wt.sync`, `wt.syncHz`, the matrix rows, `wt.modDepth`), so PF-2 is re-measured as ADR-379 A1 already requires | clean-room builder | approve CONTRACT §7 E1 |
| PF-6 | the transformations at neutral cost under 1 % (WT-39); osc 3 off costs nothing | — | none |
| PF-7, RT-1 | the table sets in prepare's maxima. Today the clean-room engine measures **19.8 MB** per instance (PF-7.1, 19,792,840 bytes, clean-room `ANSWERS.md`, the acceptance of 6cd98b5). Three table sets add **26.0 MiB** (strategy (b), one level per octave) to **59.7 MiB** (strategy (a)), plus 144 KiB of PM scratch and 48 KiB of interleave buffers (CONTRACT §1.5). The lab's Spectral-log model adds published 16-frame sets: about 6.75 MiB per oscillator under strategy (b)'s figure (CONTRACT §1.5's table, "Linear published sets") | lead; human | CONTRACT §7 C4 (the cap) gains this line |

### 2.7 Determinism and real time (DT, RT)

- **DT-4.** New purposes: `wt/mod/…` (the internal modulator's noise, WT-70), `wt/random/…` (generators' own draws,
  WT-46), and the Random page's draws for tables (section 5). The internal modulator's start phases come from
  mulberry32 streams keyed by note and member (the lab already did: B652 trace, "the only change from SCALPEL").
- **DT-5.** No wall clock in a recipe or its id (CONTRACT §3.1).
- **DT-1, DT-2.** Every rebuild, swap and lazy frame build happens at a sample the event stream fixes (WT-14, WT-18).
  The lab's worker-thread model does not (decision 6).
- **DT-3.** Holds: the level chosen depends on the internal rate, so the top of the band differs between 44.1 and
  96 kHz, but DT-3 judges the first 16 harmonics below 18 kHz.
- **RT-1, RT-2.** Main-thread builds, the slot handoff (CONTRACT §1.6), bounded rebuild work. A lazy Spectral-log
  builder must bound its frames per boundary.

### 2.8 Every SPEC-ENGINE row a 1.0 port touches

CONTRACT §8's list, plus what ADR-387 and this inventory add (in bold):

| row | change |
|---|---|
| §1, §2, §4.1, §4.2 | **three oscillators; table members; the blade text retired** |
| 5.2 | **`mix.osc3`, `osc3.on`** |
| 5.3 | the `wt.` rows; the internal modulator; **`osc1.wt.pm3`, `osc2.wt.pm3` live; osc 3's block** |
| 5.4 | **every row retired** |
| 5.1, 5.8 | **the `retired` class; the counts** |
| C.9 to C.11, BL | **retired (BL-5's FM law and BL-13's spreads restated where reused)** |
| C.14, C.16 | the loop on table members; the PM matrix row |
| PA-4, PA-5, PA-8 | as CONTRACT §2.3; **PA-8's loop rows; `osc3.*` rows** |
| **RC-6.1, RC-6.2, RC-6.4** | **the voice hands PM taps between oscillator sources; osc 3 the fifth instance** |
| IF-4 | the engine document's `wt` member; **`version` 2 and its migration from 1 (section 6.4)** |
| **IF-6** | **osc 3's gate and meter** |
| IF-9, IF-10 | counters and observations; **osc 3's views** |
| IF-12 | `set_corner_tables`; the `Tables` output; **osc 3's morph level** |
| **IF-13, MS-8, MS-11, MS-12** | **osc 3's four swarm sources; the new destinations (section 4)** |
| MO-13, Appendix D | the spectral-blend class; **MO-26, D.8 with five sources; MO-27's one-hot rule (section 3)** |
| RT-1, PF-7 | the table sets; **three of them** |
| **PF-1, PF-2** | **re-anchored (section 2.6)** |
| **B.1** | **kept as a test fixture until the retirement round (section 6.5)** |
| the structure file | `instrument.table_set`; the member's blocks; **osc 3** |

**FX device (ADR-254).** None for the member chain. Osc 3's block is an engine block, below IF-11's ceiling and away
from the rack's, modulators' and macros' blocks, which stay their own. The PM matrix and the table member are
per-voice processing: marked instrument-only.

---

## 3. The morph contract (Appendix D, IF-12)

| item | what changes | where | owner | depends on | open / recommendation |
|---|---|---|---|---|---|
| The spectral-blend class (WT-53) | a new key class in Appendix D: `oscN.wt.table`'s weights follow `morph.law` (MO-4's unglided weights under Blend and Stepped, pooled identical recipes, Count Two by MO-9, glided by MO-5 (b), restricted to on corners), Quantum one-hot at the pick, Exempt corner A, morph off one-hot at `morph.audible` | MO-13, a new D row (CONTRACT §4.2) | lead | ADR-377 A3 | none: ruled |
| The resolver's output | `set_corner_tables(corner, osc, table id, at)` in, a `Tables` event out (per oscillator, four ids and four weights, at control boundaries when they change), **for three oscillators** | IF-12; clean-room `morph/resolver.h`; the harness header | lead; clean-room builder; test author | — | none |
| Mode and count (MO-11) | the table key carries a mode and a count like any key; the bridge's `modes` message already names `osc1.ratios` and `osc2.ratios` as keys (`bridge_core.h` lines 76–77): `oscN.wt.table` takes that path | MO-11; `bridge_core.h` | lead | — | none |
| MO-26 and ADR-383's shared switch pick | **five switches**: `osc3.on` joins the one switch pick. Osc 3's keys are every `osc3.` key and `mix.osc3`, and its table key; they blend and pick only over its on corners. The resolver's source list, morph level per source and its mirror ĝ grow to five | MO-26, D.8; `resolver.h` line 53 | lead; clean-room builder | decision 1 | decision 14 (confirm: the ruled rule's plain extension) |
| Quantum | one-hot at the key's pick and latch; a flip is WT-15's rebuild crossfade, not PA-5 (the table set is per instance, shared by PA-5's copy) | WT-38, CONTRACT §1.8 | — | — | none |
| Stepped | MO-4's weights at the stepped position, the same as Blend otherwise | WT-53 | — | — | none |
| Waypoints and region locks | the table key holds no waypoints (as `oscN.ratios` holds none, MO-21) and no region lock in 1.0 (WT-53, PROPOSED); a list or lock sent for it is refused and counted (MO-21's counter, MO-24's) | MO-21, MO-24 | lead | — | decision 12: approve as proposed |
| Save current sound (MO-25) | where the corners' tables differ at the position, Save writes a capture recipe of the heard blend into the corner (WT-55); the resolver supplies each oscillator's weights and ids, the morph system makes the capture on the main thread | MO-25, IF-12's read | lead | WT-44's bounds | decision 13: if the capture would break WT-44's bounds, **refuse the whole save, named**, never write a partial corner |
| MO-27 corner exactness | at an exact corner the published set must equal the corner's own table. Linear's Σ w·c is exact with one weight 1 and the rest 0; **Spectral log is not**: max(0, exp(ln(\|c\| + ε)) − ε) does not return \|c\| bit for bit in floating point. Add: a one-hot weight set publishes the corner's own level sets under both laws, with no blend arithmetic | WT-53, MO-27 | lead | — | decision 11: approve |
| Morph off | the table set is one-hot at `morph.audible` (MO-20) | WT-50 | — | — | none |

**FX device (ADR-254).** None: the FX device hosts no oscillator. FX-module structure still follows its quantum
boundaries (ADR-377 A3).

---

## 4. The modulation contract (IF-13, Appendix E)

| item | what changes | where | owner | open / recommendation |
|---|---|---|---|---|
| New destinations (MS-12 (a)) | every continuous `wt.` row on all three oscillators: `wt.frame`, `wt.frameSpread`, `wt.sync`, `wt.syncHz`, `wt.window`, `wt.asym`, `wt.bend`, the six matrix rows, the internal modulator's continuous rows; scope **per-voice** | MS-12; CONTRACT §4.1 | lead | none |
| The engine path (MS-41) | `wt.sync`, `wt.syncHz`, the matrix rows and `wt.modDepth` join PA-4, so ENV 1 to 4, velocity and per-note LFOs reach them sample-exact; the rest take the outside path | PA-4, MS-41 | lead | approve CONTRACT §7 E1 |
| Not destinations | `wt.law`, `wt.syncIn`, `wt.modShape`, `wt.modIn`, `wt.modType` (stepped, MS-12), the table keys (corner state), `member` (state) | MS-12 | — | none |
| MS-29 | stands (ADR-385): only global LFOs and the 2-D random move `morph.x`, `morph.y`. Per-note timbre motion comes from the per-voice frame | MS-29 | — | none |
| MS-46, MS-47 | under WT-22 as written, `wt.frame` under Spectral log is shared, so a per-note route to it reads the newest note, the page warns, and Refuse mutes it (CONTRACT §7 B2). **Under the lab's model (decision 6) the frame is per voice under both laws and none of this arises** | MS-46, MS-47 | lead; GUI session (the warning) | decision 6 |
| The internal modulator as a source | it runs per member at audio rate inside the read (WT-70); making it routable would mean publishing a per-member value no other source has | E.1 | lead | decision 9: **not a source** |
| Osc 3's swarm sources | `osc3.R`, `osc3.psi`, `osc3.onset`, `osc3.gravity` join MS-8 (per-note) | MS-8, IF-3 | lead | none |
| MS-11 | aftertouch's default route goes "to `coupling` on both oscillators" | MS-11 | lead | decision 15: all three, so a patch that turns osc 3 on behaves like the others |
| ADR-379 A1 rule 1 | routes respect a source's on corners: osc 3 included | E.3 | — | none |
| Round 62's queue | the modulation system is the clean room's stage 9, still waiting on the test author's items 36 to 39 (clean-room `ANSWERS.md`). The wavetable destinations are added by a later round, never folded into 62 | — | lead | none |

**FX device (ADR-254).** Osc 3's four swarm sources are voice-bound; their FX-device stand-in is none. The internal
modulator is not a source, so it declares nothing.

---

## 5. Randomization

**Today.** The random roles lab (`docs/design/random-roles-lab.html`, B644) embeds the manifest as
`docs/design/gui3/params.json` carries it (330 rows), nested by PA-8's live-when table, through
`tools/gen_random_roles_data.py`. The generator hard-codes two oscillators and two blades (`OSCS`, `BLADES`, line
49), an oscillator's sections (`OSC_SECTIONS`, line 54), the loop and blade-pair rows (lines 64–65), the "Carrier
loop" group (live when a blade is on in Sync, FM or Ring) and the "Both blades" group (lines 175–178). The ruled rules pre-filled there are ADR-374 (r1 to r5, A1, A2), GUI-55 and
GUI-38. The human's notes in the lab ask for: roll scope (Never, Once per patch, Per corner), transposition ladders
unlocked along the chaos curve, a Discrete switch that locks quantum- or stepped-only rows ("It would be nice to be
able to roll all corners and get a smooth result everywhere"), and per-feature group toggles.

| item | what changes | where | owner | depends on | open / recommendation |
|---|---|---|---|---|---|
| The generator | three oscillators; a "Wavetable" section per oscillator (tables, frame, law, sync, warp), a "PM" section (the matrix row and the internal modulator); `crossMod`, `feedback`, `loopDrive` move out of "Carrier loop" (their condition becomes "the oscillator is on"); the blade groups go | `tools/gen_random_roles_data.py` | lead | the spec round; GUI session regenerating `params.json` and `layout.json`'s `live_when` | decision 24 (who regenerates the page) |
| Blade roles | every blade row's role, and ADR-374 r4 / GUI-55 r4 ("At low chaos, blades are rare"), retire. The human's exported annotations for blade rows become history | the lab, the master's Randomize stand-ins | GUI session | the spec round | none |
| Corner tables | **drawn by a chaos ladder**: low chaos, one factory table for all four corners, drawn **once per patch** (so the pad does not blend tables); middle, a table **per corner** from the factory set and the user library; high, **TABLECASTER rolls** per corner. A blend of four tables is smooth under Blend, so per-corner tables meet the human's "smooth result everywhere" wish; they flip only under Quantum | the Random page; the generator's roles | GUI session; human | decision 18 | decision 19 |
| The law | Structural, drawn once per patch; Linear at low chaos | — | GUI session | — | decision 19 |
| `wt.frame`, spread, warp, window | continuous per corner, leaning to neutral at low chaos | — | GUI session | — | decision 19 |
| `wt.syncIn` | Structural; off at low chaos, unlocked by a threshold | — | GUI session | — | decision 19 |
| PM matrix | zero at low chaos; forward edges (3 → 2 → 1) unlock first; back-edges only at high chaos and capped below I_back (WT-65) | — | GUI session | H6's map | decision 19 |
| Osc 3 on/off | ADR-374 r1 to r3 extended: randomizing one oscillator never flips its own switch; osc 3 is never the only source; it is off at low chaos and unlocked by a threshold; its settings are left alone while it is off | GUI-55's rules | GUI session; human | decision 1 | decision 17 |
| ADR-374 A2 | "fully randomized patches should heavily bias toward attaching every osc volume to Osc 1": osc 3's `env` selector included | GUI-55 A2 | GUI session | — | none |
| TABLECASTER as a "Tables" tab in Random (ADR-384's "a tab in Random that renders tables to corners") | ADR-386's five integration points: **(1)** corners play 16 frames resampled from up to 256, the library keeps the full roll for capture and export; **(2)** its peak/RMS/table normalisation is display and export only, the engine applies WT-16; **(3)** a roll reaches a corner only through WT-47's gate; **(4)** each generator engine frozen per version by goldens; **(5)** zero phase by default for tables headed to corners | the Random page; SPEC-WAVETABLE WT-46 | GUI session (page); lead (spec); wavetable builder (C++ generators) | decision 18 | approve all five |
| The quality gate (WT-47) on a roll | a refused roll is re-drawn from the next draw of the same purpose stream, at most 8 tries, then refused and named; deterministic, so a seed always lands on the same table | WT-47, WT-46 | lead | — | decision 18 |
| Seeded purpose streams | the Random page draws a roll's 32-bit seed from its own stream (`random/wt/oscN/corner/c`); the recipe stores the seed, never the page's stream position, so a recipe rebuilds alone (WT-42, WT-46). The generator's own draws use `wt/random/…` | DT-4, WT-46 | lead | — | none |
| The Discrete switch and table keys | the table key is not a manifest row but behaves as a choice: when Discrete is off, the corner tables stay locked | the Random page | GUI session | — | decision 19 |

**FX device (ADR-254).** None: the FX device has no oscillators and no tables. ADR-342's FX-side rules stand.

---

## 6. State, presets and migration

### 6.1 Recipes in the host chunk (CONTRACT §3)

- The chunk carries a `tables` dictionary {id: recipe} with every table a corner of **any** oscillator references
  and every capture's sources (CONTRACT §3.2 says "either oscillator": three now). Bounds: 64 recipes of at most 512
  bytes, so 32 KiB, capture depth 8, enforced when a table is assigned or captured, never at save (WT-44).
- **The chunk's ceiling is not ruled.** GN-37 estimates the morph system's document at about 60 KB for four corners
  (about 120 KB with four saved presets), against ADR-351's 64 KiB paste cap and ADR-359 r2's open ceiling. The
  tables add up to 32 KiB. Decision 16: rule one ceiling for the whole chunk before S6, the tables counted in it.
- In the test plug-in the chunk is the bridge's text (`bridge_core.h` lines 123–130: version 2 for a plain preset,
  3 and 4 with the morph). The tables become **version 5**: a `tables` line and each corner's table ids, written only
  when they differ from what a load rebuilds by itself, as the morph's lines are.

### 6.2 The library on disk

The user library is device-wide (WT-45, ADR-384): one file per table (recipe and spectra) in the user's
application-support folder, under the company folder the branding convention names. A table's id is its recipe's
hash, never a random draw. Deleting a library file never silences a patch: the patch rebuilds from its recipe and
the page marks the table "not in the library", with a restore. The lab already behaves so (B650 trace, "a two-step
delete of a table in use left the corner playing"). Owner: the wavetable builder (S6) for the store; the lead for
the bridge's messages (section 8.3); the GUI session for the browser.

### 6.3 Preset tiers (ADR-382)

| tier | what it carries with tables | open / recommendation |
|---|---|---|
| oscillator | one oscillator's rows and **one table**: its corner table where the corners agree, otherwise a capture of the heard blend; the dictionary closure of that recipe. Slot-agnostic (it loads into osc 1, 2 or 3); globals never travel (B641's starting rule) | decision 20 (ADR-382 left "what an oscillator preset carries" open) |
| corner | one corner: each oscillator's table id for that corner and the closure | none |
| global | the whole patch, its dictionary | none |
| exported file | the dictionary plus the rendered spectra of every played table that is not a factory generator (CONTRACT §3.2): with 12 corner tables, at most about 2.1 MiB | none |

### 6.4 How old patches load

**What exists.** No horde 2 patch has been released. What exists is: the factory bank (83, Appendix B's format),
version-0 states, the 16 demo presets, and the human's Live sessions saved with test plug-in builds (chunk versions
2 to 4). All of them use blades.

**The mechanism.** IF-4's `version` rises to 2 with a migration from version 1 (RC-9.4), as B.2 is migrated from
version 0. The test plug-in's chunk gets the same migration when it reads an older chunk. That is horde 2's revision
gate.

**Mapped or flagged** (ADR-387 left it to this plan):

| | flagged, with the base wave mapped (recommended) | mapped by a blade-cycle generator |
|---|---|---|
| what loads | each corner gets a factory table that is its base wave (a "Basic shapes" generator: `factory/basic` with `{"shape": "saw"}` and so on, 16 identical frames); every blade row is kept byte for byte, inert; the load report names every retired row the patch set away from its default ("blade 1: Sync, width 0.25 … retired") | a new generator renders the patch's blade cycle into 16 frames across one blade parameter (B652's paper estimate: about a third of the bank reproduced closely, a third roughly, the rest not at all) |
| what the human hears | the patch's swarm on its plain base wave: the unison, coupling, envelopes and pan as before, without the blade's formant, sync or fold colour. Corners whose base waves differed now **blend** their waves under Blend where they used to flip; under Quantum they flip as before | close at rest for static blades; moving, random and cross-mod blades approximate or lost |
| cost | small: one generator, the migration, the report | the blade law ported into a generator (C.9 to C.11), its goldens, and a mapping per family |
| when | the 1.0 spec round | only if the factory session wants old blade sounds back |

Decision 4 recommends **flagged with the base wave mapped**. The factory bank is re-voiced anyway (ADR-387), and the
only other blade patches are the human's own test sessions.

**`member` (WT-19).** WT-19 keeps `member = base` for old patches, which is the old engine path with its blades.
Under ADR-387 every state migrates to `table`, so `member` has one job left: the oracle switch that lets S7 prove the
seam changes nothing (WT-P4: all 83 bank presets bit-identical with every `member` at `base`). Decision 5: keep it
as engine state through S8 only, and remove `base` in the 1.0 round, so 1.0 has no hidden second path.

### 6.5 The banks

| bank | today | recommendation |
|---|---|---|
| factory (83) | all blade patches; the clean room's bank items, WT-P4 and PF-1 rest on it | **not translated.** Re-voiced on tables at the factory patch session (ADR-387; the memory "factory patch session later"). B.1 stays as a test fixture in the clean room until the retirement round (decisions 21, 22) |
| demo (B645, 16, `tools/engine-preview/demo_bank.json`) | 14 use blades; approved for build 10 | **at S8, add a "Demo (wavetable)" section seeded from the lab's 24 presets** (B652's bank: the jitter trio, the FM electric piano, bell and growl, the classic sounds, the motion patches), written into `demo_bank.json`'s format. Keep the 16 blade demos playable until the retirement round, then retire them (decision 21) |
| the lab's 24 | the human's evidence of taste | the seed for the demo section, not a factory bank (ADR-382 r3: every global preset richer than one oscillator; the lab's have no filter and no noise) |

**FX device (ADR-254).** The FX chain stays a separable preset section with its own version (ADR-254's checklist);
tables are oscillator state and never part of it. The library is device-wide storage the FX device could read if it
ever hosts a table-driven module.

---

## 7. GUI needs: a handover list for the GUI session

The GUI session owns `docs/design/**` and decides how every item looks; nothing here is a design. Each item says what
the engine has to supply. New GN rows would follow `docs/design/gui3/ENGINE-NEEDS.md`'s format (`| ID | Need |
Decided by | The shell today |`, plus the spec reference in its later sections), numbered by the GUI session from
its next free id (GN-56 at the time of writing).

### 7.1 Pages and parts to retire

| what | where today | note |
|---|---|---|
| each blade's section on the Osc pages (`osc1/blade-1`, `osc1/blade-2`, the same for osc 2), the blades in their title bars (GUI-30) | `layout.json` pages `osc1`, `osc2`; `sections.json` | retire with the rows |
| the blade rows in Main's deck tabs | `layout.json`, page `main`, tabs `osc1` and `osc2` (`osc1.blade1.on` … `osc1.blade2.edges`) | — |
| the waveform's blade drag roles | `layout.json` `blade_view` (GUI-30; the SCALPEL lab's XY roles) | the waveform in three views stays and draws the table member's cycle (GN-41) |
| the ring's blade rim arcs | the Osc pages' ring (the SCALPEL bench's ring, harvested at B544) | the ring itself stays: it is the swarm |
| `baseWave` and the other retired oscillator rows | every page | hidden, as reserved rows are |
| the Random page's blade levels and the "blades are rare" stand-ins | GUI-38, GUI-55 r4 | section 5 |

### 7.2 New and changed pages

| need | engine data it needs | proposed GN row |
|---|---|---|
| **Osc 3**: a third Osc page, a Main deck tab, a Dashboard card, a Mix strip, Mod page destinations; off by default | osc 3's rows, gate, meter (IF-6), swarm views (GN-41 to GN-43 extended to osc 3) | GN-56: osc 3 everywhere osc 1 and 2 appear |
| **The wavetable section** of each Osc page: the table's 16 frames (a waterfall or stack), the frame and spread, the law, sync and window, asym and bend | each corner's table frames and the published set's frames, as cycles (a frame's 2048-sample image, WT-7) | GN-57: table and published-set frame cycles in the frame (`want.tables`) |
| **The PM matrix**: a 3 × 3 grid, rows the modulated, columns the modulator, the diagonal `feedback` (WT-61); clamp engagements shown, never silent (WT-65); named algorithms as page presets that write ordinary matrix values (CONTRACT §7 H1) | the clamp counter per voice and edge, interleaved-mode flag | GN-58: PM observation (clamp, interleave) |
| **The internal modulator** with the lab's scope (the modulator and the resulting pitch deviation in cents for the newest voice, B652) | the newest held voice's first member's modulator ring and deviation | GN-59: modulator scope feed |
| **Corner tables on the Morph page**: a picker per oscillator per corner (12), each with a preview; the blended table at the puck; capture; Save current sound naming the capture it will write; per-key mode and count for each table key | each corner's id and frames; the resolver's weights; the capture result and refusals (WT-44) | GN-60: corner tables, weights and capture across the bridge |
| **The library browser**: factory, user and rolled tables; two-step delete; "not in the library" with restore; export | the library list and its operations (section 8.3) | GN-61: the library's list and operations |
| **The Tables tab in Random** (TABLECASTER): roll, roll all, mutate, locks, history; send a roll to corners; refusals by WT-47 named | the C++ generators' results and the gate's verdict | GN-62: roll, gate result, assign |
| **Observation**: each member's level (the lab's mip ladder, with "FM moved it from H n"), rebuild count, the loudness gain and its peak-limit and gain-cap engagements, the rebuild crossfade | IF-10's per-member level and weight, IF-9's counters (CONTRACT §1.9) | GN-63: table observation |
| **The load report**: retired rows a patch used, and fallback tables (CONTRACT §3.3), by name | the migration's report | GN-64: the load report |
| **Spectral log's warning** (MS-46) | none new | only if decision 6 keeps WT-22 |

**FX device (ADR-254).** FX screens and the mod matrix must not assume these oscillator pages exist (the checklist's
"screens" rule). Nothing above is shared with the FX device except the library browser, which stays oscillator-side
in 1.0.

---

## 8. The clean-room engine and the bridge

### 8.1 Where the clean room stands

- The engine is built to round 61 (6cd98b5, accepted: "the bank is 83 of 83 bit-identical with morph off",
  clean-room `ANSWERS.md`). The spec is at round 64 (`e72a092`). Round 62 (Appendix E, the modulation system) is BD-8's
  stage 9 and waits on the test author's items 36 to 39; rounds 63 (unison 9) and 64 (the shared switch pick) are
  queued with it. PF-3's timed run waits on the quiet window.
- The seam CONTRACT §1 read is at `e72a092`, so S7 re-reads it at its own pinned commit, which should be after stage 9
  lands (CONTRACT §9 item 10).

### 8.2 How the wavetable lands there (CONTRACT §5.8)

| piece | what | owner | rule |
|---|---|---|---|
| the spec round(s) | SPEC-ENGINE drafted from sections 2 to 6 of this file and CONTRACT §8, critic-reviewed, ratified; **delivered to both clean-room repos in the same turn** (the clean-room spec-sync practice) | lead | the round carries **no number from the wavetable builder** (ADR-247: the test author never sees builder numbers). Its PROPOSED figures come from SPEC-WAVETABLE's engine-free probes or the lead's own engine-free analysis |
| the patch series | the table modules and the seam, from `seam/` against the pinned commit | wavetable builder | how it lands is CONTRACT §7 F3: decision 23 |
| the harness header | `set_table`, the `Tables` event, `set_corner_tables`, the observation and counter fields, **`kSourceOsc3 = 4`, `kSources = 5`, `OscView osc[3]`** | test author (the header is the test side's) | the lead files the change request with the round, same turn |
| the tests | written from the round, blind to the build | test author | the build's gates (`horde-wavetable/tests`) are the lead's and never cross to the test author |
| gate moves | only with engine-free evidence and the human's ruling (ADR-247), recorded as an ADR amendment | lead; human | — |

### 8.3 The test plug-in's bridge (`tools/engine-preview/`)

| item | change | where |
|---|---|---|
| the engine | `build.sh` already takes `HORDE_ENGINE2_REPO` (line 6): at S8 it builds from the wavetable build's `seam/` repo at a commit, since the clean-room repo holds none of the build until F3 lands. The build label names the repo and the commit, so a build in Live is never a guess | `build.sh` |
| host parameters | every new engine row is a host parameter under its own id, as today (B632); `morph.x`, `morph.y` stay 0xA00001, 0xA00002 (`preview_plugin.cpp` line 78); the corner tables are **not** host parameters (WT-51) | `preview_plugin.cpp` |
| new messages (section 3's names, the GUI session's to confirm) | `table {seq, osc, corner, id}` (assign from the dictionary or the library); `capture {seq, osc, frames: "all" \| f}`; `library {seq, op: list \| delete \| restore \| export, id}`; `roll {seq, osc, corners, generator, settings, seed}`; and `want {tables: true}` for frames (GN-57). Each acknowledged in the frame that completes it, as `hello` and `preset` are | `bridge_core.h`; `preview_plugin.cpp` |
| the two-oscillator assumptions | `want.osc` (`bridge_core.h` line 51, `osc[2]`), `kRatiosKey` (line 77), `unison[2]` (line 170), the corners' texts (`preview_plugin.cpp` lines 177–178, 261–262, 308), the per-oscillator loops (575, 587, 594, 602, 825), `kSwitch` and `kLevelKey` (803–804, four sources) | the same files |
| the chunk | version 5 with the tables (section 6.1); versions 2 to 4 migrate (section 6.4) | `bridge_core.h` |
| the main-thread builds | `set_table` builds on the main thread and posts a `Tables` event at the next processing call (WT-18); the plug-in's swap of engines on a preset load waits for the builds | `preview_plugin.cpp` |
| the oracles | `bridge_test.cpp`: the new messages parse, version 5 round-trips, a version 4 chunk migrates; `smoke.cpp`: a `set_table` reaches the engine at its sample and the output equals the offline render bit for bit, with a control that applies it a block late. Each with a control that must fail, as the README requires | `bridge_test.cpp`, `smoke.cpp` |
| the recorded frames | `frames_dump.cpp`'s fixtures change; **re-recording them is a human gate** | `tools/labharness/fixtures/` |
| install | outside the sandbox; re-sign after the build (the seal gotcha); `killall -9 AudioComponentRegistrar`; `auval` (the global build notes) | `build.sh --install` |

**FX device (ADR-254).** None: the bridge is the instrument's. The FX device would have no `table`, `capture` or
`roll` messages.

---

## 9. FX device (ADR-254), item by item

| item | consequence |
|---|---|
| member chain, PM matrix | none; per-voice processing, marked instrument-only |
| osc 3's ids | an engine block (0x8000), clear of the rack's, modulators' and macros' own blocks |
| mixer | osc 3 is one more source input to the rack (GN-50); the FX device's input stays the DAW's pair |
| morph | none; FX-module structure still follows its quantum boundaries (ADR-377 A3) |
| modulation | osc 3's swarm sources are voice-bound, with no stand-in; the internal modulator is not a source |
| randomization | none; the FX device has no oscillators or tables |
| state, presets | the FX chain stays a separable section; tables are oscillator state |
| library | device-wide; readable by the FX device if it ever hosts a table-driven module |
| screens | FX screens and the mod matrix never assume the oscillator pages |
| bridge | none |

---

## 10. Owners at a glance

| owner | work |
|---|---|
| the human | decisions in section 12; the listening batches (CONTRACT §5.5); the factory patch session |
| lead | SPEC-WAVETABLE and CONTRACT rework for ADR-387; the SPEC-ENGINE round(s); S0 and every gate, measurement and listening page; `gen_random_roles_data.py`; the bridge and the test plug-in; the demo section; delivering rounds to both clean-room repos |
| GUI session | every page in section 7; the Random roles and the Tables tab; GN rows; the random roles lab page |
| wavetable builder | S1 to S8 in `horde-wavetable`, including the C++ generators (S6b) and the patch series |
| clean-room builder | stage 9 (rounds 62 to 64); then the wavetable round and the patch series (F3); later the retirement round |
| clean-room test author | items 36 to 39; the harness header change; tests from the wavetable round, blind to the build |

---

## 11. Sequencing: from today to the wavetable in the test plug-in

CONTRACT's stages S0 to S8 are the spine. The clean room's queue runs beside it and joins at S7.

| step | what | owner | depends on | runs in parallel with | the human rules here |
|---|---|---|---|---|---|
| 0 | The human rules the "now" decisions of section 12 (1 to 5, 7, 14) in one poll batch | human; lead | this file | — | decisions 1–5, 7, 14 |
| 1 | Rework SPEC-WAVETABLE and CONTRACT for ADR-387: replace (A1 answered); K = 3 at the seam; osc 3's real block; `member`'s sunset; the lab-derived rows once ruled; MO-27's one-hot rule; WT-P7 restated from the B648 follow-up; critic review | lead (Opus critic) | step 0 | 2, 3, 4 | — |
| 2 | **S0**: the repo, harness, alias meter, click detector, instruction counter, plants and controls; **the B648 follow-up cells timed** in the 2–6 am window; the conversion factor | lead | CONTRACT §5 (not step 1) | 1, 3, 4 | WT-BUDGET (decision 7) with the timed figure |
| 3 | Clean room: stage 9 (round 62), rounds 63 and 64, PF-3's timed run | clean-room builder and test author | items 36 to 39 | everything up to step 10 | — |
| 4 | GUI session: design labs for section 7's pages; GN-56 onward | GUI session | step 0 for osc 3 and the table pickers | 1 to 11 | GUI rulings |
| 5 | **S1 to S4** (one stage at a time, each closed by CONTRACT §6.1) | wavetable builder; lead | step 2 | 3, 4, 6 | after S1: G1 (the factory set, with "Basic shapes"); after S2: C6, C7, both laws; after S3: C1 (spacing); after S4: C9, C4 and **decision 6** |
| 6 | TABLECASTER's generators specified for C++ (ADR-386's five points), its JS outputs frozen as goldens per engine | lead | decision 18 | 5 | decision 18 |
| 7 | **S5a, S5b** | wavetable builder; lead | S4; decision 8 before S5a | 3, 4 | D1, D2, E3, H1, H2, H6; decisions 9, 10; PF-2 (decision 7, again) |
| 8 | **S6**, and **S6b** (the C++ generators: eight factory, "Basic shapes", TABLECASTER's ten) | wavetable builder | S5b; step 6 | 4, 9 | decisions 13, 16, 20 |
| 9 | The SPEC-ENGINE round(s): sections 2 to 6 of this file; critic; ratified; both clean-room repos the same turn; the harness header change request | lead; test author; human | step 1; S5b (engine-free numbers only) | 8 | ratification; decisions 11, 12, 15, 17, 19 |
| 10 | **S7**: the seam at the clean-room commit after stage 9, **K = 3**, both render modes, the loop amendment; WT-P4 (the 83 bit-identical with `member` at `base`) | wavetable builder; lead | 3, 8, 9 | 4; the bridge's preparation (11a) | — |
| 11a | The bridge prepared against S7's seam: three oscillators, the messages, version 5, the oracles | lead | 10 | 10, 4 | — |
| 11 | **S8**: the spectral-blend class, `set_corner_tables`, the destinations, the `wt` state and migration, osc 3's sources; the test plug-in built from `seam/`; the "Demo (wavetable)" section | wavetable builder (engine); lead (bridge, demos); GUI session (pages) | 10, 11a, 4 | — | **the wavetable in the test plug-in: the human plays it** (`s8-4-human-take`); decision 21 |
| 12 | After: the patch series lands in the clean-room engine (F3); the test author's tests from the round | clean-room builder and test author | 11 | — | decision 23 |
| 13 | After: the factory patch session; then one retirement round removes the blade code, retires B.1's bank tests, re-anchors PF-1 | human; lead; clean room | 11, 12 | — | decision 22 |

**What can start today without any ruling:** step 2's S0 tooling and the timed follow-up cells, step 3, and the
GUI session's design work for the parts that do not depend on osc 3.

---

## 12. The decisions

### 12.1 CONTRACT §7 after ADR-387

| item | status |
|---|---|
| A1 replace or beside | **answered: replace** (ADR-387) |
| A2 how base and table appear | answered in part; the rest is decisions 3 to 5 below |
| B2 Spectral log's frame under Refuse | moot if decision 6 adopts the lab's model |
| C1, C3 to C10 | unchanged; ruled at their stages (section 11, step 5) |
| D1 to D3, E1, E3 | unchanged; E1 recommended (section 4) |
| F1, F2 | unchanged |
| F3 how the patch lands | decision 23 |
| G1 the factory set | unchanged, plus "Basic shapes" (decision 4) |
| G2 the random generator's slot | superseded by TABLECASTER (decision 18) |
| G3 import | unchanged (after 1.0) |
| H1 to H8 | unchanged; ruled at S5a and S5b |
| H9 the real engine's third oscillator | **answered: in 1.0** (ADR-387); decisions 1 and 2 |

### 12.2 New decisions, each with a recommendation

Numbered once; **Needed** says by when. Step 0 of section 11 puts the "now" ones to the human in one batch.

1. **Osc 3 at the seam.** *Needed: now.* Build S7 and S8 at K = 3, rather than K = 2 and a later round over the
   same code (the voice loops, the per-source arrays, the resolver's source list, the harness header).
   *Recommendation: K = 3.* WT-60's "the clean-room seam at K = 2" changes.
2. **Osc 3's ids.** *Needed: now.* Block 0x8000 with osc 1's offsets (`osc3.on` 0x8000, `wt.` rows 0x8030–0x803A,
   the modulator from 0x803B, `stream` 0x8300), no blade blocks and none of the six blade-only rows; `mix.osc3` at
   0x0013; the morph's and modulators' lockfile blocks fixed first. *Recommendation: approve.*
3. **The blade rows.** *Needed: now.* A new class word, `retired`, sharing reserved's mechanism (not host parameters,
   kept in a state, refused as `Param`s, hidden), for both blades, the six blade-only oscillator rows and the
   `ratios` text. *Recommendation: approve.*
4. **Old patches.** *Needed: now.* Flagged, with the base wave mapped to a "Basic shapes" factory table per corner,
   every retired setting named in the load report; IF-4 `version` 2 with a migration. Not a blade-cycle generator in
   1.0. *Recommendation: approve.* What the human hears: the swarm on its plain base wave, without the blade's
   colour; corners whose base waves differed blend under Blend where they flipped before.
5. **`member` (WT-19).** *Needed: now.* An oracle switch through S8 only; the 1.0 round migrates every state to
   `table` and removes `base`. *Recommendation: approve.*
6. **Spectral log's frame.** *Needed: before S4 closes.* The lab's model (all 16 frames pre-blended, the frame per
   voice under both laws), built deterministically on the audio thread with a bounded number of frames per
   boundary, adopted if S4's timed run affords it; WT-22's shared frame as the fallback. *Recommendation: the lab's
   model, on the timed evidence.* It makes CONTRACT §7 B2 moot.
7. **The budget.** *Needed: now, then again after S5b.* Time the B648 follow-up cells at S0 before S1 opens, then
   rule WT-BUDGET with that figure (it moves only by the human's ruling, ADR-385). Re-anchor PF-1 to WT-P2's cells.
   Keep PF-2 judged with two oscillators on and report three on beside it, ruling PF-2 again after S5b.
   *Recommendation: approve.*
8. **The internal modulator's rows.** *Needed: before S5a.* WT-70's set plus `wt.modMix` (the lab's FM mix), the
   lab's ranges (ratio ×1/16..×32, 0.1..20 000 Hz), decorrelation continuous with an A/B against the lab's
   two-way switch. *Recommendation: approve; finalise the ids then (0x03B onward, `wt.modMix` next).*
9. **The internal modulator is not a modulation source.** *Needed: before S5a.* *Recommendation: approve.*
10. **Self-PM.** *Needed: at S5b.* `feedback` as the matrix's diagonal (CONTRACT §7 H4), judged against the lab's Self
    in `s5b-4-feedback`. *Recommendation: H4.*
11. **MO-27 for tables.** *Needed: the SPEC-ENGINE round.* One-hot weights publish the corner's own level sets under
    both laws, with no blend arithmetic. *Recommendation: approve.*
12. **Waypoints and region locks on table keys.** *Needed: the round.* Neither in 1.0; refused and counted.
    *Recommendation: approve (WT-53 as proposed).*
13. **Save current sound past the bounds.** *Needed: the round.* Refuse the whole save, named. *Recommendation:
    approve.*
14. **Five sources in the shared switch pick.** *Needed: now.* `osc3.on` joins ADR-383's one pick; MO-26 lists five
    sources. *Recommendation: confirm (the ruled rule, extended).*
15. **Aftertouch's default route** (MS-11) on all three oscillators. *Needed: the round.* *Recommendation: approve.*
16. **The chunk's ceiling.** *Needed: before S6.* One ruled ceiling for the host chunk, the tables (up to 32 KiB)
    counted in it, with GN-37. *Recommendation: rule it before S6.*
17. **Osc 3 in Random.** *Needed: the round (the GUI session builds it).* Never alone; off at low chaos and unlocked
    by a threshold; its settings untouched while off; ADR-374 A2's lean to osc 1's envelope includes it.
    *Recommendation: approve.*
18. **TABLECASTER.** *Needed: before S6 (step 6 of section 11).* ADR-386's five points as proposed; its ten engines
    ported to C++ as recipe generators at S6b with goldens per version; a refused roll re-drawn up to 8 times from
    its own stream, then refused, named. *Recommendation: approve.*
19. **Random's wavetable roles.** *Needed: the round (the GUI session builds it).* Corner tables on a chaos ladder (one
    factory table once per patch at low chaos, per-corner factory and library tables in the middle, TABLECASTER rolls
    at the top); the law once per patch; sync off at low chaos; the matrix zero at low chaos with forward edges
    unlocking before back-edges, capped below I_back; the Discrete switch locks the corner tables.
    *Recommendation: approve as the starting point for the chaos-curve tuning session (ADR-374 r6).*
20. **The oscillator preset tier.** *Needed: before S6.* One oscillator's rows and one table (a capture where the
    corners differ), with its recipe closure; slot-agnostic. *Recommendation: approve.*
21. **The banks.** *Needed: at S8.* A "Demo (wavetable)" section seeded from the lab's 24 presets at S8; the 16 blade
    demos kept until the retirement round; the factory bank not translated, re-voiced at the factory patch session,
    B.1 kept as a test fixture meanwhile. *Recommendation: approve.*
22. **When the blade code leaves the engine.** *Needed: after S8.* One retirement round after the factory patch
    session: the blade code deleted, B.1's bank items retired, PF-1 re-anchored. Until then WT-P4 needs the blades.
    *Recommendation: approve.*
23. **How the patch series lands** (CONTRACT §7 F3). *Needed: before S8 ends.* Merged, the clean-room builder
    reviewing it, with the round, so the test author writes tests from the spec and never sees the build's numbers.
    *Recommendation: (i), merged.*
24. **Who regenerates the random roles lab.** *Needed: with the round.* The generator is the lead's (B644); the page
    sits in the GUI session's scope. *Recommendation: the lead updates the generator; the GUI session regenerates the
    page, or hands the page back to the lead by a note on its row.*

---

*Last verified 2026-11-08 against origin/main c85c5e03, `lead-records-421` 5bf3738b and the clean-room engine
`e72a092`.*
