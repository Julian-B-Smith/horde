# Engine rebuild audit: what in horde 2's engine is baggage, and what a new engine must keep (B493)

**Audit date:** 2026-10-15.
**`main`:** `990e4a3` (origin/main). **Records read at:** `47ae038` (origin/lead-records-258, PR 1093: ADR-223, B493, B494). `h2/engine/` is the same on both (`git diff origin/main origin/lead-records-258 -- h2/engine` is empty).
**Cause:** the lead's call (ADR-223 item 1, ROADMAP B493), not the seven-day cadence.
**Scope:** read-only on the code. This file is the only change. The CPU figures below come from the repo's own measuring tool (`tools/measure_h2_engine.cpp`), compiled with the release flags into a scratch directory outside the repo.
**Branches read, never checked out:** `origin/h2-cut-latch-d9` (D9), the local `h2-edge-table-d10` (D10), `origin/h2-output-headroom` (ADR-221) and the local `h2-preview-shell` (the B458 preview the human plays).

---

## One-page summary

**What the engine is today.** horde 2's engine is two older engines stitched together: the swarm (horde's coupled oscillators) and SCALPEL's blades (windows that cut each cycle). That stitching was first done in JavaScript, by making one prototype a subclass of the other and patching the other's source text as it loads. Then it was copied into C++ line by line, because the rule was that the C++ must match the JavaScript to the last bit (ADR-187). Every fix since has been added as a switch that is off by default, with the old behaviour kept beside it, so the match could still be checked.

**How much of it is baggage.** Of the engine's 2,994 lines:

| what | lines | share |
|---|---|---|
| fixes kept with both behaviours (six off-by-default switches, D1 to D8) | 552 | 18.4 % |
| speed scaffolding shaped by the bit-match rule | 231 | 7.7 % |
| copies of JavaScript's number rules | 125 | 4.2 % |
| test and measurement hooks built into the engine | 120 | 4.0 % |
| features the human already culled or locked (2026-10-02) | 32 | 1.1 % |
| **total not about the sound as you hear it** | **1,060** | **35.4 %** |

Around it sit another **11,376 lines** whose only job is the match: the JavaScript twin (1,679), two frozen earlier ports kept as test references (4,041), and the parity tools (5,656). They are retired by ADR-223 only when the new engine and its own tests replace them.

**What it costs in time.** Measured today: the work the two engines duplicate (two phase counters and two "centre of the swarm" sums per member) is **under 2 %** of the CPU. The copied JavaScript number rules are an estimated **2 to 9 %** (not measurable without a new tool). The real cost of the match is that it forbids the methods that would make the engine two to three times faster (single precision, lookup tables, several members at once); ADR-220 already lifted that ban for the CPU work.

**What a new engine must keep.** I reduced **75** decisions from the records to plain requirements. **40** are user-level requirements to keep, **17** more are requirements whose exact form waits on a ruling, **14** are artefacts of the match and can be dropped, and **4** were already culled by the human in the 2026-10-02 audit but are still in the code (three more locked settings sit inside rows that stay).

**What the rest of the instrument needs from it.** Eleven interface areas (voices and MPE, pitch, modulation, morph and state, mixer and gain staging, the sub and the noise oscillator, the FX rack, true stereo, quality modes, the labs, the host). **31 interface questions are still open for the human.** Four of them decide the engine's shape: who owns a voice, who owns the amplitude envelope and the per-voice signal, whether true stereo is per voice, and which parameters take per-voice modulation and how fast.

**What a clean engine would gain.** Size: about **2,100 to 3,000 lines**, against about 3,950 for today's engine once the parked D9, D10 and range-check work lands: **24 to 47 % smaller**, while also carrying the new interfaces. The bigger reduction is around it: the 11,376 lines of twin, frozen ports and parity tools retire with it. CPU: the heaviest bank patch (Glass horde pad, 8 voices, the default 1× oversampling) measures **27.2 % of an Apple M3 performance core today**. My estimate for a clean engine with the same features is **about 11.5 % (central)**, 11.8 to 12.6 % with the always-on smoothing fixes, 15.7 to 16.8 % cautious and 8 % at best. The target is 10.7 %: reachable only at the central-to-best end, and only if true stereo is not per member. Most of that gain is also open to B488 on today's engine (ADR-220); what only a rebuild gives is a further 1.1 to 1.25× in the parts today's structure makes slow, and an end to the parity round on every change (ADR-221's one-value default change stopped twice and re-pinned 670 digest rows in each of two files).

**When to build it.** **Now, once four structural rulings are made** (with two smaller ones that go with them), not after FX, modulation and the mixer are wired. Those four rulings (voice ownership, envelope and per-voice signal ownership, true stereo's shape, per-voice modulation's reach and rate) are what wiring would teach us about the engine, and each can be ruled from a lab or a page of options without building anything. Wiring FX, modulation and the mixer to today's engine first would mean wiring them twice, against an engine that misses its CPU target by 2.5 times. Section 5 gives the case for and against both timings.

**What I could not determine** is listed in section 9: the run-time share of the JavaScript number rules, the cost of the switches when off, the CPU of D6 and D7 when on, the engine's exact memory, and anything about the sound that only the human's ears can settle.

---

## 1. The inventory (B493 acceptance 1)

### 1.1 Method

Every line of the four engine files was assigned to one category by hand, from a read of each block. The ranges are in appendix A; a script applied them in priority order (test hooks, then switches, then JavaScript rules, then kernel scaffolding, then culled), so no line counts twice. "Remainder" is the engine doing what it sounds like. Comments are counted with the code they explain: 591 of `engine.h`'s 2,386 lines (24.8 %) are comment-only, and most of the comments in the switch ranges narrate the switches' rules. Run time comes from the stage-skipping build `measure_h2_engine_stages` (section 1.9). Where a figure needs a tool that does not exist, the estimate and its method are stated.

### 1.2 The count

| file | lines | test hooks | switches D1–D8 | JS number rules | kernel scaffold | culled or locked | remainder |
|---|---|---|---|---|---|---|---|
| `h2/engine/engine.h` | 2,386 | 100 | 480 | 42 | 231 | 17 | 1,516 |
| `h2/engine/blade.h` | 244 | 0 | 72 | 0 | 0 | 5 | 167 |
| `h2/engine/swarm.h` | 261 | 3 | 0 | 12 | 0 | 10 | 236 |
| `h2/engine/js.h` | 103 | 17 | 0 | 71 | 0 | 0 | 15 |
| **all four** | **2,994** | **120** | **552** | **125** | **231** | **32** | **1,934** |

By switch: D1 `aaCarrier` 70 lines, D2 `aaXin` 5, D3 `aaLoop` 22, D6 `frameHold` 121, D7 `stealFade` 228, D8 `spreadRamp` 106.

Not in the main tree but waiting to land on it: D9 (`origin/h2-cut-latch-d9`: +161 lines in `engine.h`, +17 in `blade.h`), D10 (`h2-edge-table-d10`: a new 280-line `edge_table.h`, +86 in `engine.h`) and the preview's range table (`h2-preview-shell`: a new 238-line `domain.h`, +249/−74 in `engine.h`). With those, the engine would be about 3,950 lines.

### 1.3 JavaScript emulation (125 lines; 217 call sites)

The engine calls `js::` 217 times (`engine.h` 197, `swarm.h` 17, `blade.h` 3): `truthy` 77, `max` 61, `min` 27, `pow` 24, `round` 10, `sel` 8, `toInt32` 6, `frac` 3, `sign` 1. Each copies a JavaScript rule the engine does not need once the match is gone:

- `js::max`/`js::min` propagate NaN and order ±0 (`js.h:36-47`). In the release binary that inlines into **570 call sites of `nan`**, 42 % of the engine's 1,353 libm call sites (`h2/engine/libm-calls.darwin-arm64.appleclang-16.txt`). They never run inside the parameter domain; they are code size and branches.
- `js::round` is `floor(x + 0.5)` (`js.h:35`); `js::sel` makes a `switch` refuse a non-integral selector (`js.h:56-58`); `js::truthy` reads a number as a flag (`js.h:54`). Every discrete parameter is a `double` read through these (`blade.h:44-70`): a typed engine has enums and bools.
- `jsParseFloat` re-implements JavaScript's `parseFloat` for the custom ratio list (`engine.h:897-918`).
- `poolSize` copies `Array.slice`'s negative-index rule, so `poly = -2` means "all but two" (`engine.h:1159-1163`).
- A culled voice's fade is rounded through a float because the JavaScript used a `Float32Array(1)` (`engine.h:2019-2021`, `:2037-2038`). ADR-187 A2 left it open: it makes the output depend on the host's block size by up to 1.47e-8.
- Each fresh note takes 9 draws from the host stream for start phases that the swarm then overwrites (`engine.h:1329-1333`, `:1363`), and each swarm draws 32 start phases to keep 9 (`swarm.h:33-36`, `:121-130`), because skipping them would shift every later random number.
- The coupling smoother keeps the literal 0.08 at 44.1 kHz because the seconds formula lands three ULP short (`swarm.h:10-11`, `:38-39`, `:72`).
- Two values of 2π: `TAU` = 6.283185307179586 in `engine.h:103`, the swarm's truncated 6.283185307 in `swarm.h:93, 149, 150, 207, 222, 225, 235` and `engine.h:1582`, because each prototype had its own.
- Built with `-ffp-contract=off` everywhere (`h2/README.md` rule 7), so the compiler may not fuse a multiply and an add. Measured in phase 1a: **+6 % CPU** on the heavy class at `-O3` (`h2/README.md` rule 7, `docs/port/scalpel-phase-1a.md`).

### 1.4 Test and measurement hooks inside the engine (120 lines)

Five `#ifdef` families live in product code: `H2_ENGINE_FAULTS` (21 must-fail controls; `engine.h:106-126`, `:1211-1225`, `:1228`, `:1232-1235`, `swarm.h:246-248`), `H2_ENGINE_STAGES` (8 stage skips; `engine.h:127-148`), `H2_ENGINE_KERNELS` (kernel hit counters; `engine.h:82-98`, `:538-546`), the event log for parity (`js.h:85-101`, `engine.h:105`, `:1854`, `:2138-2140`) and their `#undef`s (`engine.h:2379-2384`). They compile to nothing in the shipped build, so they cost nothing at run time, but every reader of the engine reads them, and each fault site is a second meaning on a production line. D9's branch adds seven more faults (15 to 21) and a "tap" for its probe.

### 1.5 The six switches kept with both behaviours (552 lines)

| switch | what it fixes | ruled | lines | CPU when on | the records' source |
|---|---|---|---|---|---|
| D1 `aaCarrier` | carrier anti-aliasing by first-order ADAA, narrowed | built, off; **locked** in the 2026-10-02 audit | 70 | about 1.0× | ADR-189 A1; audit round 1 |
| D2 `aaXin` | the edge scanner follows the feedback phase push | built, off; **locked** | 5 | not measured | ADR-189 |
| D3 `aaLoop` | a 5 kHz one-pole in the feedback loop (no limit cycle) | built, off; **locked** | 22 | not measured | ADR-189 A1 |
| D6 `frameHold` | the blade frame never jumps | built, off; on in the preview | 121 | not measured | ADR-216, A1 |
| D7 `stealFade` | a taken voice fades over 5 ms | built, off; on in the preview | 228 | 1.03–1.04× during takes (one run) | ADR-216, A3 |
| D8 `spreadRamp` | spread values reach the blades as ramps | built, off; on in the preview | 106 | **+2.7 % to +9.9 %**, even when nothing moves | ADR-216, A4; B482 |

Two things in this table matter more than the line counts:

- **D1, D2 and D3 were locked by the human on 2026-10-02** (`docs/audits/2026-10-02-engine-audit-round2-decisions.json`, rows `aaCarrier`, `aaXin`, `aaLoop`: "lock"), while ADR-189 says their instrument default "is ruled when horde 2's shell is built". The two records disagree about whether a user ever sees these switches. Either way, a user never chooses between two behaviours, so a new engine keeps one.
- **D6, D8 and D9 are one fault, met three times.** Values the engine computes every 32 samples (the frame angle, the member spreads, the quantized cut) reach the audio as steps. SPEC-SCALPEL §12 listed it before any of them: "Rotation and spreads are control-rate; very fast rotate-spread settings step at 32-sample resolution." Each was fixed with its own mechanism, its own off position and its own check (`h2_frame_hold_check.mjs`, `h2_spread_ramp_check.mjs`, D9's `h2_cut_latch_check.mjs`). In a clean engine they are one rule: no control-rate value reaches the audio as a step (requirement B5 in section 2).

The fixes' process cost, from their traces: D7 took five rounds and two independent critics (`traces/2026-10-13-b459-steal-fade-d7.md`, 489 lines); D8 three rounds (`traces/2026-10-13-b460-spread-ramp-d8.md`, 423 lines); D6 three traces; D9 stopped before push on a ledger approval; ADR-221's one-value default change (the default `gain` from 0.35 to 0.0875, one line in each engine) stopped twice: first because three checks compared the engine with its prototype at each one's own default (ADR-221 A1), then because the self-digest's re-pin control could not fire on a row the change moves (A2); its branch re-pins 670 rows in each self-digest file (`git diff --stat origin/main...origin/h2-output-headroom`).

### 1.6 Kernel scaffolding shaped by the match (231 lines)

B441's C1 to C3b made the heavy class about 1.5 times faster without moving a bit (Glass horde pad, 8 voices, 2×: 72.3 % of an M3 core at 48 kHz on 2026-10-04, about 66 % at 44.1 kHz, against 43.0 % measured today), by compiling 17 specialised copies of the inner loop (`engine.h:443-580`, the dispatch at `:2147-2173`) and replaying per-member overrides once a sample (the 18-line proof that this is bit-identical is at `engine.h:2274-2291`). Specialising the inner loop per block is itself a SPEC-SCALPEL requirement (§8 item 6), so the idea stays. What is baggage is its shape:

- **The kernel set is fitted to the current bank.** "The kernels, chosen from what the scenario set and the ledger presets render" (`engine.h:495-500`). A preset with a combination the bank lacks falls to the generic path, which is slower, and nothing tells its designer.
- **The templates exist to keep "every expression the generic path evaluates, in the same order"** (`engine.h:450-455`): one templated body plus a generic wrapper for each of `wave`, `mod`, `voiceOut`, `voicePlain`, `out`, `ev` and `outSerial`.
- **They assume parameters hold still within a render call** (B488, 2026-10-14: the savings "assume parameters that hold still within a block and may not survive modulation"). Nobody has measured them under modulation.

### 1.7 Dead, unreachable or already culled

| item | evidence | class |
|---|---|---|
| `detune` and `K` are smoothed every 16 samples and never read smoothed | in the smoother table (`engine.h:405`); the swarm reads the raw targets `t.detune`, `t.K` (`engine.h:1384-1385`) | dead work; and a missing smoother (detune and coupling zipper under automation) |
| `s.mUnit2` and `s.law` are written and never read | `engine.h:2180-2181`; readers use `d.mUnit2` (`:2308`) and `d.law` (`:1115`, `:1135`) | dead writes |
| `cScale` is accepted and never read | `blade.h:48`, `engine.h:422`; zero reads; the twin says "cScale is ignored" (`docs/design/scalpel-horde-engine.js:57`) | a parameter with no effect |
| `phaseMode` 1 ("aligned") and 2 ("settled", the default) sound the same | both set retrig on (`engine.h:1394`); the only difference is 9 host-stream draws per note (`:1332-1333`), overwritten (`:1363`); the twin: "*settled* ... is NOT voiced here" (`docs/design/scalpel-horde-engine.js:52-54`) | a control whose only effect is to reshuffle other random numbers |
| the blade envelopes (11 keys) | culled by the human 2026-10-02 (round 1, `s.benv*`, `s.b2env`: "cull"); still computed every sample with two `pow` per voice (`engine.h:2207-2208`, `:2216-2228`, `blade.h:132-136`) | culled, still built |
| detune law 5 (stretch) | culled ("it doesn't offer anything harmonic doesn't"); still in `swarm.h:173-176` | culled, still built |
| `motionCenter`, `inertiaCurve`, `dcMode` | locked 2026-10-02; still free parameters (`swarm.h:181`, `:191-195`; `engine.h:1390-1391`, `:2370`) | locked, still variable |
| the global DC refresh countdown is reset by any voice's retrigger | `dcCnt = 0` (`engine.h:1350`), one counter for all voices | an inherited quirk: one note's start moves every voice's DC refresh time |

### 1.8 Duplication between the swarm and blade lineages

This is the "indebted to prior states" the human named. Each row is a place where the C++ still has two of something because there were two prototypes.

| duplicate | where | run time today |
|---|---|---|
| two phase counters per member: the swarm's φ_H and the blade's φ_S = frac(φ_H + ½), advanced by the same frequency | `swarm.h:42`; `engine.h:1363`, `:1872-1881`, `:1926` | swarm phase advance 0.5–0.6 % (measured) |
| two "centre of the swarm" sums: the swarm's R and ψ every 16 samples, the blade's lead and frame every 32 | `swarm.h:206-210`; `engine.h:1509-1518` | part of couple + spread, below |
| two parameter stores for the swarm, copied per voice per tick (`syncSwarm`, 22 lines; with a `pow` and an `exp` each time) | `blade.h:51-55` → `swarm.h:55-61` via `engine.h:1376-1397`, called from `:1399` | inside the swarm tick, 1.2–1.5 % |
| colliding keys renamed with an `h.` prefix: `h.law` beside the spread `law`, `h.driftRate` beside `driftRate`; SCALPEL's `detune` in cents feeding the swarm's knob as `/100` | `engine.h:427-430`, `:1384` | none |
| four clocks: the swarm tick (16 samples, on a global grid at member 0's first step), the couple tick (32), the smoother tick (16, its own counter), the DC tick (256); plus gravity's grid | `engine.h:1867`, `:2205`, `:2196`, `:2235`, `:2124` | none directly |
| a look-ahead swarm tick at note-on (B325) and a saved copy of the swarm per voice, because the two clocks disagree about when a note's member frequencies exist | `engine.h:1404-1418`, `:1521`; `blade.h:139` (800 bytes per voice) | small |
| two pitch glides: SCALPEL's `glide` (log glide of the voice pitch) and the swarm's `freqGlide` (two-stage per member), plus `inertia`, which the human called "kind of the same thing as Freq Glide" | `engine.h:2211-2214`; `swarm.h:140-141`, `:183-186`, `:244`; `engine.h:1873` | small |
| two drifts: the swarm's pitch drift and the spread law's drift walk (14 Gaussian draws per member every 32 samples) | `swarm.h:144-161`; `engine.h:1118-1129` | most of couple + spread on Glass horde pad (9.1 %) |
| two member orderings: the swarm's placement x[] (dist, seed) and the blades' spread slots (14 random slots per member) | `swarm.h:78-110`; `engine.h:981-984`, `:1349` | none |
| four random streams, and the noise blade drawing from the shared host stream, so any note-on changes every sounding voice's noise (B469) | `engine.h:583-584`, `:2111`, `:2127`; `swarm.h:120` | none |

**Measured run-time share of the duplication: under 2 %** (the swarm tick 1.2–1.5 % plus the phase advance 0.5–0.6 %, section 1.9). So removing it is a clarity gain, not a speed gain. The exception is the spread law's drift walk, which is not duplication but is costly (B441's ledger: 14 Gaussians per member every 32 samples, each a `log`, a `sqrt` and a `cos`).

### 1.9 Where the time goes today (measured 2026-10-15)

`measure_h2_engine_stages --stages`, the repo's frozen protocol (44.1 kHz, 128-sample blocks, the presets' own 2× oversampling, best of 5, interleaved, the load guard on; calibration 180.4 ms; load 2.43 to 4.61; head `47ae038`). A stage's share is the time it saves when skipped, so shares are not strictly additive (the sums are 102 to 104 %).

| stage | Glass horde pad, 8 voices | Crushed bells, 8 voices |
|---|---|---|
| full render, % of one M3 core | **42.95** | **24.31** |
| the same at 1× oversampling (the device default) | **27.17** | **15.29** |
| blade evaluation | 47.0 % | 49.9 % |
| per-member-step frame (the remainder: overrides, smoothers, envelopes, pan) | 27.7 % | 28.5 % |
| edge correction (PolyBLEP search and probes) | 11.7 % | 12.0 % |
| couple + spread (every 32 samples) | 9.1 % | 3.3 % |
| DC estimate | 5.7 % | 6.2 % |
| swarm tick (every 16 samples) | 1.2 % | 1.5 % |
| swarm phase advance | 0.5 % | 0.6 % |
| output filters, DC blocker, tanh | under 1 % | under 1 % |

Op count (one voice, 2 s): 0.0060 edges per member-step on both patches. The edge search runs on every step and finds an edge on 0.6 % of them; each edge it finds costs two full evaluations of the blade (`hAt`, `engine.h:1707-1728`). Compared with the 2026-10-04 attribution (`docs/port/cpu-ledger.md` §3, before C1 to C3b), the blade's share fell from 54–62 % to 47–50 % and the remainder's rose slightly; the order of the cost centres is unchanged.

### 1.10 The JavaScript twin and the apparatus around it

`docs/design/scalpel-horde-engine.js` is 1,679 lines: a 275-line header, 294 lines of switch code (D1 to D8 and M1 to M3), 98 lines that patch SwarmSynth's source text at load time (`patchSwarm`, `SWARM_PATCHES`), 38 of visualiser readouts, and the composition. It cannot run alone: it subclasses `reference/scalpel/prototype/razor-core.js` (906 lines) and evaluates SwarmSynth out of `reference/swarmsaw.html` (its DSP section, lines 203 to 717). **Defining one engine in JavaScript takes about 3,100 lines across three files**, two of them protected. ADR-223 item 4 retires the twin; whether the two protected prototypes stay as history is left to the spec.

The apparatus that exists only to hold the match:

| what | lines | gate |
|---|---|---|
| `tools/h2_engine_parity_check.cpp` (and its contraction-on control build) | 933 | `verify full` |
| `tools/h2_engine_render.mjs` (renders the twin's goldens) | 761 | via the parity check |
| `tools/labharness/composed_engine_check.mjs` (the twin against its two prototypes) | 1,578 | `verify full` |
| `tools/labharness/divergence_ledger_check.mjs` | 466 | `verify fast` |
| `tools/parity_floor_check.py` | 287 | `verify full` |
| `tools/h2_libm_count.py` and the libm ledger (`h2/engine/libm-calls.*.txt`) | 154 | context only |
| phase 1a and the swarm lift: `h2_scalpel_parity_check.cpp`, `h2_scalpel_render.mjs`, `h2_scalpel_stream.h`, `h2_swarm_parity_check.cpp`, `h2_swarm_lift_check.cpp`, `h2_swarm48_check.cpp`, `h2_lift_check.py`, `measure_h2_scalpel.cpp` | 1,477 | `verify fast` and `full` |
| `h2/cores/` (the two frozen ports, "test references, not product code", `h2/README.md`) | 4,041 | the checks above |
| **total, with the twin (1,679)** | **11,376** | |

The parity check runs 659 scenarios today (702 on D9's branch). The two self-digest files (668 lines each) and their tool are not parity: they are the engine against itself, the "frozen goldens" ADR-187 item 4 asks for, so that mechanism stays and is re-baselined for the new engine.

### 1.11 SPEC-SCALPEL already said what not to copy

The protected SCALPEL spec has a section titled "Incidental details — do NOT replicate" (§10) and port requirements (§8). The parity rule (ADR-187, 2026-09-28) overrode them, so today's engine replicates most of what the spec told it not to. A new engine built from a user-level spec can follow its own source:

| SPEC-SCALPEL says | today's engine | where |
|---|---|---|
| §8.1 per-member parameter structs, not writes into shared state | writes `s.w`, `s.depth`, `s.I`, `s.mEff` per member and restores them | `engine.h:2315-2318`, `:2346` |
| §10 make the morph's `mr`/`mn` per-blade fields, not static globals | two engine-wide fields swapped around each blade 2 call | `engine.h:2109`, `:811-825`, `:838` |
| §8.2 analytic carrier step heights, not two full evaluations per edge | `hAt` evaluates the whole blade twice per edge | `engine.h:1707-1728` |
| §8.3, §10 an integer hash for noise, not the `sin` hash | the `sin` hash | `blade.h:185` |
| §9 "named, seeded RNG streams; its draw order will not match the JS, and should not try to" | the JavaScript's draw order, to the draw | `engine.h:1329-1333`; `swarm.h:33-36` |
| §10 do not replicate `Math.random` and its draw order | replicated (the host stream is the JS's `Math.random`) | `engine.h:171`, `:583` |
| §10 do not replicate "6-voice polyphony and the fixed 9-member arrays (use HORDE's voice allocator and its member limit)" | `poly` defaults to 6 (the preview shipped with six voices); members fixed at 9, where the audit manifest gives horde's `n` a range of 1..32 | `blade.h:45`; `swarm.h:32` |
| §10 do not replicate `tanh(1.6·gain·y)` as the final stage; "the bench's pads are hot enough in chords to lean on this clipper" | the final stage, and the reason ADR-221 had to lower the default 12 dB | `engine.h:2374-2375` |
| §8.7 fast approximations for sine and the morph's atan2 | double-precision libm | `engine.h:599-610` |
| §8.9 vectorise the smooth path across members | scalar, double | the member loop, `engine.h:2269-2342` |

---

## 2. Every behavioural decision, reduced (B493 acceptance 2)

Each row is a behaviour the records decided or the code carries. **becomes** says what it is from the player's side. **class**:

- **KEEP**: a user-level requirement the new engine must meet.
- **RULING**: a requirement whose exact form waits on the human; the open question is named.
- **DROP**: an artefact of the match with the JavaScript; nothing a player can hear depends on it.
- **CULLED**: the human already culled or locked it (the 2026-10-02 engine audit, `docs/audits/2026-10-02-engine-audit-round2-decisions.json`), but it is still in the code.

A requirement is stated as behaviour, never as today's arithmetic. "Sounds good, not the same" (ADR-223 item 3) applies to every KEEP row: the numbers in it are the records' own targets, not today's output.

### The swarm

| # | decision | becomes | class | source |
|---|---|---|---|---|
| S1 | horde's coupling law, not SCALPEL's: a bipolar knob, a squared taper, the pull in units of the swarm's measured spread | Coupling pulls the members toward one locked pitch as it rises and toward an even spread (splay) below zero, and feels the same at any detune | KEEP | twin header row 6 ("horde law wins"); ADR-184; SPEC-SCALPEL §3, §11; `swarm.h:199-205` |
| S2 | the coupling smoother's coefficient from a time constant in seconds (M1) | Every time constant is stated in seconds, so a patch behaves the same at 44.1, 48, 96 and 192 kHz | KEEP | B150; ADR-009; B382 M1; charter §Domain |
| S3 | the onset lock, bipolar, with dissolve (M2) | A note can begin with a burst of locking (or, negative, of splay) that fades over a time in seconds | RULING: onset and dissolve were deferred "test as recommended" | ADR-056; B382 M2; audit rows `onset`, `dissolve` |
| S4 | detune laws: cents, Hz, ERB, harmonic | Four ways to place the members' pitches around the note | KEEP | audit rows `law`, `harmReach`: keep |
| S5 | detune law 5, stretch | — | CULLED ("doesn't offer anything harmonic doesn't") | audit row `stretchB`; `swarm.h:173-176` |
| S6 | detune law 3, the tempo grid: every pairwise beat a multiple of the tempo (M3) | Detuning measured in beats of the host's tempo | RULING: `beatMult` deferred ("a part of me does want to make it work"), `bpm` culled, the lab's real tempo open | ADR-022; B382 M3; audit rows `beatMult`, `bpm` |
| S7 | member placement (even, JP, Gaussian, Cauchy, golden) from a seed | The spread of members can be even or shaped, reproducibly per patch | KEEP | audit rows `dist`, `seed`: keep |
| S8 | `spread` scales every law; `anchor` puts the lowest member on the root | One detune amount; an advanced anchor | KEEP (spread merged into detune) | audit rows `spread` (merge), `anchor` (keep) |
| S9 | pitch drift per member: walk, sine, sample and hold | Members wander in pitch, seeded, at a depth and rate | RULING: `driftMode` deferred; "the sine option would need a seeded randomness for its start offset" | audit row `driftMode` |
| S10 | `motionCenter`: drift scaled by each member's distance from the root | — | CULLED (locked) | audit row `motionCenter`; `swarm.h:181`, `:191-195` |
| S11 | inertia: a damped second-order swing toward each member's target, with a taper curve | Members can swing into place with weight instead of jumping | RULING: `inertia` deferred, `inertiaCurve` locked; overlaps `freqGlide` | audit rows `inertia`, `inertiaCurve` |
| S12 | `freqGlide`: each member glides to its new frequency | Members can slide independently to new pitches | RULING: deferred, "run tests on both" (with inertia) | audit row `freqGlide` |
| S13 | start phase: random, aligned, settled, keep-phase, scatter | One start-phase control; a note's start never depends on the host's block size | RULING: merged by the human, "we'll want to workshop the execution" | audit rows `phase`, `scatter`, `keepPhase` (merge); B475 |
| S14 | pivot: the swarm folds onto the member nearest the root | The swarm can lock onto the played pitch instead of its drifting average | KEEP | audit row `pivotMode`: keep |
| S15 | gravity: held voices settle toward just ratios on a fixed time grid, inside a basin | Held chords drift into just intonation (about a third of a second at full gravity); release tails keep their pitch; independent of block size | KEEP (`basin` deferred) | B335; ADR-008; ADR-086 A1; `engine.h:1536-1576` |
| S16 | the ensemble: onset scatter with timing correction, attack and release scatter, per-partial envelopes | Members can enter at slightly different times, corrected so the ensemble does not drift late | RULING: all four deferred "test"; attack and release scatter: "I don't think I could ever really hear the effect"; onset scatter: "an important knob I would like to preserve" | ADR-077; ADR-078; B335; B370; audit rows `onsetScatter`, `attackScatter`, `relScatter`, `voiceEnv` |
| S17 | a member above Nyquist is rendered | A member pitched above Nyquist is silent, never folded back or turned into DC | RULING: the B378 audit's F2, proposed for the h2 copy, never ruled | B378 F2 |

### The blades

| # | decision | becomes | class | source |
|---|---|---|---|---|
| B1 | seven blade modes (sync, FM reset, FM free, noise, fold, ring, crush), every blade wave including the closed-form sine→saw, mirror and twins, blade 2, serial interplay and collision | The SCALPEL v1 sound set | KEEP | SPEC-SCALPEL §1, §4, §13; ADR-184 |
| B2 | spreads across members (position, cut, rotation, width, depth, index, morph); spread laws (gradient, random, drift, alternate, swarm); cut rules (harmonic … custom) | Each blade setting can fan out across the members by a law | KEEP | SPEC-SCALPEL §5 |
| B3 | every spread bipolar; a negative spread mirrors the positive exactly, Quantize included | A negative spread is the mirror image of the positive | KEEP | ADR-184 A2; B315 |
| B4 | D6: the frame holds its course where the swarm's centre is undefined | The blades follow the swarm's centre where it has one and never jump where it has none | KEEP | ADR-216 item 1, A1; B461 |
| B5 | D8: spread values ramp across the tick; D9: a quantized cut changes at its blade's next cycle start | No value computed at control rate ever reaches the sound as a step; a quantized cut changes only where its blade's carrier restarts | KEEP (one requirement, three of today's mechanisms) | ADR-216 A4; B460; B487; SPEC-SCALPEL §12 |
| B6 | 12 ms smoothing: 12 keys every sample, the rest every 16; `detune` and `K` read unsmoothed | Every continuous parameter glides over a stated time; nothing zippers under automation or modulation, detune and coupling included | KEEP | SPEC-SCALPEL §7; B378 F8; `engine.h:405`, `:1384-1385` |
| B7 | discrete settings change at block rate, as a jump | Changing a blade mode, wave or the member count while a note sounds does not click | RULING: at once, at the next note, or crossfaded, and how long (Q14) | B479; ADR-218 (the modules' rule) |
| B8 | blade envelopes 1 and 2 (11 keys) | Blades take their envelopes from the mod matrix (ENV 2 to 4, default routes written by preset translation) | CULLED | audit rows `s.benv*`, `s.b2env`; B366; `engine.h:2207-2228` |
| B9 | cross-member modulation and feedback, with D3's loop filter | Members can modulate each other and themselves; the loop never settles into a limit cycle at any oversampling | KEEP | SPEC-SCALPEL §6; ADR-189 A1 (D3) |
| B10 | D1 (carrier ADAA) and D2 (the scanner follows the feedback push) as switches | — (their aim is B11's) | CULLED (locked; ADR-189 says "ruled when the shell is built", see finding H6) | audit rows `aaCarrier`, `aaXin`, `aaLoop`; ADR-189 A1 |
| B11 | edge correction: 2-point PolyBLEP plus 2×; then a 32-tap minimum-phase table (D10); 1× the default, 2× HQ | Blade edges and carriers alias at least about 110 dB under the harmonics from C2 to C8 at the default quality, with no added latency, keeping the top of the band within 1 to 3 dB | KEEP | ADR-222; ADR-191; B383; `docs/research/2026-10-13-edge-table-design.md` |
| B12 | corners (triangle bases, a mirrored blade's centre, crush with slew) are not corrected by D10 | Corners alias no more than a stated level | RULING: whether 1.0 needs the next-order correction (ADR-222 item 3) | ADR-222 item 3 |
| B13 | DC: a per-cycle estimate, an 8 Hz blocker as fallback, `dcMode` locked | No blade leaves a DC offset (residual under 0.003) | KEEP | SPEC-SCALPEL §7, §9; audit row `s.dcMode` (lock) |
| B14 | edge heights measured by two full evaluations of the blade | — | DROP (SPEC-SCALPEL §8.2: analytic heights) | `engine.h:1707-1728` |
| B15 | the `sin` hash for noise, shared-state parameter overrides, the morph's `mr`/`mn` as engine-wide fields | — | DROP (SPEC-SCALPEL §8.1, §8.3, §10) | `blade.h:185`; `engine.h:2315-2318`, `:2109` |
| B16 | the noise blade draws from the shared host stream | One note never changes another note's sound | RULING: B469's first acceptance, the human rules whether notes share one noise stream | B469 |
| B17 | a noise blade can step from −0.9 to +1 in one sample | Noise steps are part of the sound or are softened, by ear | RULING: B481, a listening item | B481 |
| B18 | 17 compiled kernels chosen from the bank's presets | — (the requirement is N16's CPU target on any patch) | DROP as a shape (SPEC-SCALPEL §8.6 keeps specialisation as a method) | `engine.h:495-500` |

### Voices and output

| # | decision | becomes | class | source |
|---|---|---|---|---|
| V1 | the voice law: a free slot, else the quietest releasing tail, else the oldest held; a repeated note never reuses its sounding voice | A repeated note's release keeps ringing; a held note is the last to go | KEEP | ADR-083; B310 |
| V2 | the voice cap is an input; over it the quietest tails fade over 8 ms; at a full cap refuse, steal or replace | Overload sheds release tails without a click and never cuts a held note | KEEP (the cap's source is Q3) | B323; B375 |
| V3 | D7: a taken voice fades over 5 ms and the new note starts when the fade ends, late in every respect, its release included; the voice law reads the key | Taking a sounding voice never clicks, and a note sounds for as long as its key was held | KEEP | ADR-216 item 2, A3; B459 |
| V4 | mono, legato, glide between overlapping notes, glide always | Mono and legato play modes with portamento | KEEP (polyMode merged) | audit rows `polyMode`, `glide` (merge) |
| V5 | a look-ahead swarm tick at note-on (B325) | A note's first sample already plays its members' pitches | KEEP (the mechanism is an artefact of two clocks) | B325; `engine.h:1404-1418` |
| V6 | a culled voice's fade rounded through a float | — | DROP (ADR-187 A2's open item) | `engine.h:2019-2021`, `:2037-2038` |
| V7 | block-size and sample-rate independence | The same notes render the same at any host block size and the same sound at any sample rate | KEEP | module 1.0 bar §3; charter §Domain; B475 |
| V8 | a pool of 8 voices; 16 voices means two engines; `poly` defaults to 6 | A stated polyphony, at the CPU target | RULING (Q4) | `docs/port/cpu-ledger.md` open question 2; B458 ("six voices") |
| V9 | equal-power pan on a balanced slot table, a pan order, a width | Members spread across the stereo field without a brightness tilt; width 0 is mono | KEEP | SPEC-SCALPEL §6, §9, §11 |
| V10 | each voice scaled by envelope × velocity / √N | Loudness does not jump when the member count changes | KEEP | SPEC-SCALPEL §2 |
| V11 | the amp ADSR inside the engine (linear attack, exponential decay and release) | A master amplitude envelope with a shapeable curve at every stage | RULING: envelope decision 1, and whether ENV 1 is global to every pre-FX source (Q18) | B366; B377; B370; audit rows `attack` to `release` (merge) |
| V12 | the default output 12 dB lower | A four-note chord at velocity 0.85 does not clip on any bank preset at the default level | KEEP | ADR-221 items 1, 4; B489 |
| V13 | the output `tanh(1.6 · gain · y)` | — | RULING: whether the engine saturates at all, or drive is its own control (Q19) | SPEC-SCALPEL §10; ADR-221 item 6 |
| V14 | a saved state keeps the level it stored | Every changed default comes with a migration, so an old project sounds as it did | KEEP | ADR-221 item 3; ADR-197 |

### Numbers, determinism and real time

| # | decision | becomes | class | source |
|---|---|---|---|---|
| N1 | same seed and note order, identical output; mulberry32; no clock in the core | Deterministic: a patch and a performance always render the same | KEEP | charter §Domain; SPEC §5.7 |
| N2 | four seeded streams, each in the JavaScript's draw order | Named seeded streams per purpose, whose order is the engine's own | KEEP (the order is N6's) | SPEC-SCALPEL §9 |
| N3 | JavaScript number semantics (`js.h`) | — | DROP | ADR-187 item 6; `js.h:1-71` |
| N4 | JavaScript's `parseFloat` for the custom ratio list | — (the custom ratio list itself is B2's) | DROP | `engine.h:897-918` |
| N5 | `Array.slice` semantics for the voice pool | — | DROP | `engine.h:1159-1163` |
| N6 | draws taken and thrown away to keep the JavaScript's order | — | DROP | `engine.h:1329-1333`; `swarm.h:33-36` |
| N7 | the literal 0.08 at 44.1 kHz | — | DROP | `swarm.h:10-11`, `:72` |
| N8 | two values of 2π | — | DROP | `engine.h:103`; `swarm.h:93` |
| N9 | doubles everywhere; no fused multiply-add | — | DROP (ADR-223 item 3; Q28 for the human) | `h2/README.md` rule 7 |
| N10 | the shipped build is the tested build | Exactly what is tested ships | KEEP (rule 7's purpose) | `h2/README.md` rule 7; ADR-187 A1 |
| N11 | parity: RMS and max-abs under 1e-6, identical blade events, a bit-exact floor, an FMA control | — | DROP | ADR-187 items 6, A2; ADR-223 item 3 |
| N12 | `render()` allocates nothing, locks nothing, reads no clock; parameter and note calls between renders | Real-time safe | KEEP | `h2/README.md` rule 5; module bar §3 |
| N13 | flush-to-zero in the shell, not a divergence | No denormal stalls | KEEP | ADR-211; ADR-213 |
| N14 | host values checked where they enter: refuse a non-finite, clamp a range, refuse an unknown option | A wrong host value can never put the engine in a state no control can reach | KEEP | ADR-207 item 2; `domain.h` on `h2-preview-shell` |
| N15 | a non-finite output sample is zeroed, latched and counted | A fault is reported, never silently hidden | KEEP | ADR-197; B458 acceptance 6 |
| N16 | the heaviest bank patch at about 10.7 % of an M3 performance core per swarm, 8 voices, 1×; CPU measured, never a gate | The CPU target | KEEP | B488; B494; ADR-187 item 8 |
| N17 | every audible change heard A/B before it is ratified | The human's ears judge sound changes | KEEP | ADR-220 item 2 |
| N18 | each fix behind its own switch with both behaviours kept, all-off bit-identical | — | DROP | ADR-187 A1; ADR-189 |
| N19 | every divergence mirrored into the lab's JavaScript | — | DROP (the labs run the C++, ADR-223 item 4) | ADR-187 ratification ("Mirror") |
| N20 | a divergence ledger against the JavaScript | A record of every change to the new engine's sound after its goldens freeze | RULING: whether the ledger continues against the new engine's own goldens (ADR-220 item 1 names "the divergence ledger" as part of correctness) | ADR-187 item 5; ADR-220 item 1 |
| N21 | the visualiser readouts the twin posts (the swarm's R and ψ, member phases, gravity pairs, onsets) | The engine exposes what the labs and GUI draw, without touching the audio path | KEEP | `scalpel-horde-engine.js:1648-1664`; B323 |

### The bank and pitch

| # | decision | becomes | class | source |
|---|---|---|---|---|
| K1 | the 83 bench presets and the 12 envelope presets; the Breathing pad at detune 8, coupling −0.3 | Every bank preset translates to the new engine and sounds good, judged by ear, not identical | KEEP | ADR-223 item 3; ADR-217 A3; B478 |
| K2 | `os` saved per preset; a state saved before the 1× default loads at 2× | Old projects keep their oversampling | KEEP | B445; ADR-197 |
| P1 | one global `bend` in semitones; no MIDI pitch bend in the preview | Pitch bend with a stated range and law, per note under MPE | RULING (Q7) | B397; B388; B458 ("no pitch bend") |
| P2 | gravity's settled pitch rides the played pitch multiplicatively | A bend or glide keeps the interval gravity settled | KEEP | `engine.h:1419-1427` |
| P3 | pitch arrives in Hz per note, not as a MIDI key | Pitch is an input, not hard-coded 12-TET (room for MTS-ESP and Tonality) | KEEP | H2-PLAN `tonality` row; B431 |

**Count:** 75 rows. **KEEP 40, RULING 17, DROP 14, CULLED 4** (S5, S10, B8, B10). Three more culls or locks sit inside rows that otherwise stay: `bpm` (S6), `inertiaCurve` (S11) and `dcMode` (B13).

---

## 3. What the rest of the instrument needs from the engine (B493 acceptance 3)

**The first fact is that the engine's boundary is not decided.** Today's engine is a whole synthesizer: it allocates voices (`engine.h:211-257`), runs the amplitude envelope (`:2229-2232`), pans (`:2183-2188`), sums the voices, blocks DC and saturates (`:2343-2375`). The ratified and planned designs put several of those outside any one oscillator:

- **ENV 1 is a global envelope "that applies to all pre-FX sound-generating devices (including filters)"** (the human, B370);
- **filters run per voice**, "for the keytracking to work how I want it" (B303), so they need each voice's signal before the sum;
- **the sub takes oscillator 1's phase as an input** and writes "mono → voice bus" (SPEC-SUBOSC §2);
- **1.0 has two composed oscillators** (ADR-190), a sub and a noise oscillator, and today each `Engine` instance has its own voice law, so two instances can disagree about which note to refuse or steal.

So the interface tables below are written for an engine that is one oscillator inside a device-level voice, which is what those designs imply. Whether that is right is question Q1, the first of the open questions after the tables.

### 3.1 Notes, voices and MPE

| the engine must expose or accept | ratified where | open |
|---|---|---|
| note on, off and retune by a **note id** (CLAP's id, channel and key), not by MIDI key; today `noteOff(int note)` releases every gated voice with that key (`engine.h:258-289`) | MPE is in 1.0, through CLAP per-note expressions (ADR-190 A4) | Q1, Q2 |
| per-note pitch, pressure, timbre and slide as per-voice inputs | ADR-190 A4; B388 | Q2 |
| the voice law, the cap and its policy, the steal fade, mono and legato (V1 to V4) | ADR-083; B310; B323; B375; ADR-216 | Q3 |
| voice lifecycle events (started, released, taken, freed) for the sources and filters that follow a voice | implied by B303, SPEC-SUBOSC §2 | Q1, Q22 |
| polyphony and the load readouts (culled, refused, stolen; `engine.h:337-340`) | B323 | Q3, Q4, Q5 |

### 3.2 Pitch

| the engine must expose or accept | ratified where | open |
|---|---|---|
| a per-voice pitch input in Hz (bend, glide, quantizer and MTS-ESP applied upstream or inside, by rule) | B278 (one published per-voice pitch, classified a defect); B397; B431 | Q6, Q7, Q8 |
| **a published per-voice pitch** for the sub, the noise oscillator and keytracked filters | B278; FOUNDATIONS' manifest answer Q3 (cited in B397) | Q9 |
| gravity's settled pitch, riding the played pitch (P2) | B335; ADR-086 | Q8 |

### 3.3 Modulation, envelopes and the intent bus

| the engine must expose or accept | ratified where | open |
|---|---|---|
| every continuous parameter as a destination, by integer id, with its unit, range, law, morph class and rate class (the manifest shape); today parameters are set by name through two `strcmp` tables of 64 and 70 entries (`engine.h:411-441`) | module 1.0 bar §1; B376 manifest; B308 H4 (the lockfile) | Q10, Q11 |
| **per-voice (polyphonic) modulation offsets.** The blade envelopes were culled in favour of the matrix ("blades use ENV 2–4 via the matrix", B366), and the matrix's envelopes are per voice, so the engine must take per-voice offsets at least for what those envelopes drove (cut rate and width of both blades) | B376 audit (round 1, `s.benv*` cull); B366 | Q10, Q11 |
| sources: coherence R per voice ("coherence R as a source", the modulators part of H2-PLAN), and any other swarm observable a modulator reads | H2-PLAN `modulators`; B323 ("coherence made legible") | Q12 |
| the intent bus and macros bind to parameters by id and curve; the engine needs nothing more | ADR-192; ADR-169 A3, A4 | — |
| onset scatter's home | B370 | Q13 |

### 3.4 Morph and state

| the engine must expose or accept | ratified where | open |
|---|---|---|
| continuous keys blend between corners; stepped and structural keys (mode, wave, mirror, N, placement) flip at a seeded point with hysteresis | ADR-104; ADR-109; ADR-185 (cited by H2-PLAN `morph`); ADR-188 (modules) | Q14, Q17 |
| a click-free change of a stepped key while a note sounds (B479: the morph corners switch a blade's mode) | for modules, an equal-gain crossfade or a declared gate (RS-3.6, RS-4.3; ADR-218) | Q14 |
| state is the parameter values; `reset()` returns the engine to exactly a fresh instance (the preview branch adds one) | rack contract A2 and A20 (modules); ADR-208 ("a load made while not processing is whole") | Q15 |
| seeded draws that do not depend on position, keyed by (seed, key) | B308 H3 (an ADR owed before the first horde 2 patch is saved) | Q16 |

### 3.5 The mixer and gain staging

| the engine must expose or accept | ratified where | open |
|---|---|---|
| its output: today one stereo pair per engine after the saturator; the mixer wants each source after its own gain, with a meter and a latching clip warning | B225; B402; B435 (the I/O gain standard, for modules) | Q18, Q20 |
| headroom: no clipping on a four-note chord at velocity 0.85 at the default level | ADR-221 | Q19 |
| the master stage restores loudness; the engine does not | ADR-221 (the recommendation it ruled); ADR-195; ADR-203 | Q19 |

### 3.6 The sub and the noise oscillator

| the engine must expose or accept | ratified where | open |
|---|---|---|
| a per-voice master phase for the sub ("the voice hands the sub oscillator 1's phase") | SPEC-SUBOSC §2 | Q21 |
| the same voice, pitch and envelope for the sub and the noise oscillator | SPEC-SUBOSC §1, §5.3 ("the shipped module takes the voice envelope"); ADR-190 A6 | Q22 |

### 3.7 The FX rack

| the engine must expose or accept | ratified where | open |
|---|---|---|
| zero latency (the minimum-phase table was chosen for it: ADR-222) | ADR-222 | — |
| a tail and a sleep rule: when the shell may stop calling it | rack contract RS-7.x (modules; the engine has no declaration) | Q24 |
| an insert point if FX-C (Shriek) is a fixed post-stage per oscillator | ADR-092 amendment; B318 | Q23 |

### 3.8 True stereo

| the engine must expose or accept | ratified where | open |
|---|---|---|
| a width control that drives whichever of per-voice pan and per-member L/R offsets (phase, drift seed, detune skew) the stereo lab proves, with a mono-fold guarantee | ADR-190 A9; B408; module bar §3 ("mono-safe") | Q25, Q26 |

### 3.9 Quality modes and CPU

| the engine must expose or accept | ratified where | open |
|---|---|---|
| a quality setting: 1× by default, 2× HQ, saved per preset | ADR-191; B445 | Q27 |
| the CPU target on the heaviest bank patch, and on the "whole device" cell (two swarms, the sub, modulation running) | B488; B494; module bar appendix | Q28, Q29 |

### 3.10 The labs

| the engine must expose or accept | ratified where | open |
|---|---|---|
| a WebAssembly build the labs run in an AudioWorklet | ADR-223 item 4 | Q30 |
| an observation view (member phases, R, ψ, per-member levels) for the phase ring, cycle view and logo, read without disturbing the audio | the twin's `hordeViz` (`scalpel-horde-engine.js:1648-1664`); B323 | Q12 |

### 3.11 The host and the parameter table

| the engine must expose or accept | ratified where | open |
|---|---|---|
| a parameter table generated from the manifest into a committed lockfile, keys frozen from the first release, host values checked on entry | B308 H4; B428; ADR-207 item 2; `domain.h` on `h2-preview-shell` (114 keys, 64 on the short list) | Q31 |

### 3.12 The open questions, for the human

Engine shape (these four decide the spec's structure):

1. **Q1. Who owns a voice?** One device-level voice (allocation, ENV 1, per-voice filters, the sub and the noise oscillator) with each composed oscillator inside it, or each oscillator allocating its own voices as today?
2. **Q18. Who owns the per-voice signal and the amplitude envelope?** Does the engine output each voice before the sum, so the per-voice filters (B303) and a global ENV 1 (B370) can follow it, or one summed bus as today?
3. **Q25. True stereo: per voice or after the sum (B408)?** Per-member left and right offsets mean two blade evaluations per member: up to about twice the blade stage, which is 47 to 50 % of today's CPU.
4. **Q10. Which parameters take per-voice modulation?** At least the blades' cut and width, because their envelopes moved to the matrix.

Voices and pitch:

5. **Q2.** The MPE design (B388): which expressions reach the swarm and the blades, and how the voice law treats two notes on one key.
6. **Q3.** The voice limit: a fixed count or a CPU-adaptive mode (B323).
7. **Q4.** Sixteen voices: one pool of 16, or two engines of 8?
8. **Q5.** Chords as copies of the oscillator, with optional coupling between copies: a 2026-08-18 proposal (`docs/proposals/engine-slots-and-chord-copies.md`), never ruled. In or out of 1.0?
9. **Q6.** One glide or three: SCALPEL's `glide`, the swarm's `freqGlide` and `inertia` (the last two deferred in the audit).
10. **Q7.** Pitch bend: global or per note, its range, and whether the bend-lane laws (ADR-096) live in the engine or the pitch seam.
11. **Q8.** Gravity against MTS-ESP and the quantizer: gravity retunes held voices toward just ratios; which wins?
12. **Q9.** Which pitch the engine publishes to the sub and the filters: played, bent and glided, or gravity's settled pitch.

Modulation, morph and state:

13. **Q11.** The fastest modulation rate per parameter class (B275's "maximum viable modulation speed"). It decides whether per-member values may be computed once a sample, as today.
14. **Q12.** Which swarm observables are modulation sources and lab readouts: R per voice, ψ, the onset burst, per-member levels.
15. **Q13.** Where onset scatter lives: an engine feature, or part of the envelope system (B370).
16. **Q14.** How a stepped key changes while a note sounds: at once, at the next note, or crossfaded, and over how long.
17. **Q15.** Does loading a patch reset the engine's memory (the ensemble's carried offsets, ADR-077; the keep-phase snapshot; the random streams; gravity)?
18. **Q16.** Does the position-free draw ADR (B308 H3) bind the engine's seeded draws too?
19. **Q17.** The morph class of every engine key, ratified with the manifest.

Mixer, sources and FX:

20. **Q19.** Does the engine keep a saturating output stage, or is drive its own control or module (SPEC-SCALPEL §10; ADR-221 item 6)?
21. **Q20.** Volume, gain or level for the engine's output, and which mixer taps it feeds (B376 `vol`, deferred; B225).
22. **Q21.** Which phase the sub follows: the root member, the swarm's centre ψ, or the played pitch's own phase.
23. **Q22.** Do the sub and the noise oscillator share the voice's allocation, envelope, gravity and drift, or only the note?
24. **Q23.** FX-C (Shriek) as a fixed post-stage per oscillator, or a rack slot (B318)?
25. **Q24.** The engine's sleep rule and the tail it reports.
26. **Q26.** Which width candidates are in 1.0, and the mono-fold guarantee.

Quality, CPU and labs:

27. **Q27.** Quality modes: 1× and 2× only, or also edge-table length, member count and voice count under a heavy-patch policy (B441 phase 3; R2 and R3, B357)? Per preset, per instance, or global?
28. **Q28.** May the new engine use single precision, approximations and fused multiply-add throughout, judged by the invariant suite and the ears? (ADR-220 granted this for the CPU campaign on today's engine; ADR-223 item 3 implies it; neither says it for the new engine in so many words.)
29. **Q29.** Is a measurement on an M1 or the Intel min-spec a precondition for freezing the CPU row? Every min-spec figure rests on an assumed ×1.5 (`docs/port/cpu-ledger.md` §1).
30. **Q30.** The WebAssembly toolchain is not installed on this machine (`emcc` is absent; B372, H2-PLAN open decision 39), which is a new dependency and a human gate; and do the protected prototypes in `reference/` stay as history (ADR-223 item 4)?
31. **Q31.** The parameter list: the 19 rows the audit deferred (`beatMult`, `onset`, `dissolve`, `driftMode`, `inertia`, `freqGlide`, `topo`, `mu`, `balance`, `basin`, `onsetScatter`, `attackScatter`, `voiceEnv`, `relScatter`, `vol`, `panCurve`, `superMode`, `bassMono`, `mono`) and envelope decision 1 (B366, B377).

**31 open questions.** Q1, Q10, Q18 and Q25 shape the engine; the other 27 shape its parameters and edges.

---

## 4. What a clean engine would gain (B493 acceptance 4)

### 4.1 Size

**Method.** Start from the lines that are the sound (section 1.2's remainder, 1,934), take out what a single-lineage engine does not need, and add what the new engine must carry that today's does not. Each figure is an estimate from today's code, with its basis.

| item | lines | basis |
|---|---|---|
| today's core behaviour (the remainder) | 1,934 | section 1.2 |
| minus the swarm/blade duplication | −60 to −100 | section 1.8's rows |
| minus comment that narrates parity and history (today 24.8 % of `engine.h` is comment) | −150 to −250 | B494 asks for "self-documenting and concise"; the charter's rule is that comments say why, not what changed |
| D6, D8 and D9 as one smoothing rule, instead of 405 lines of three mechanisms | +80 to +150 | section 1.5 |
| D7, the steal fade, as the only behaviour (228 lines today, half of them rules for a switched-off state and its transitions) | +120 to +180 | section 1.5; ADR-216 A3 |
| D3's loop filter | +10 | `engine.h:1904-1912` |
| D10, the minimum-phase table (`edge_table.h` on the parked branch is 280) | +200 to +280 | `h2-edge-table-d10` |
| the range table: generated from the manifest, or hand-written as on the preview branch (238) | 0 to +240 | `domain.h` on `h2-preview-shell` |
| the new interfaces: note ids, per-voice outputs, per-voice modulation inputs, the observation view | +150 to +300 | section 3 |
| a specialised inner loop without the bit-identity templates (231 today) | +60 to +120 | section 1.6 |
| **a clean engine** | **about 2,100 to 3,000** | |

**Against what?** Today's engine is 2,994 lines without D9, D10, the range table or any of the new interfaces. With D9, D10 and the range table landed it would be about 3,950. So a clean engine is **24 to 47 % smaller than today's would become**, and does more. Against today's 2,994 as it stands it is about the same size or up to 30 % smaller. **The large reduction is around the engine: 11,376 lines of twin, frozen ports and parity tools retire with it** (section 1.10), and the labs stop needing a second engine in another language.

Error bars: the comment line (±100) and the new interfaces (±150, since Q1, Q10, Q18 and Q25 are open) dominate. If true stereo is per member (Q25) the engine grows by the per-channel path; if the voice layer moves out (Q1, Q18) the engine shrinks and the shell grows by the same amount.

### 4.2 CPU

**Today, measured** (section 1.9; 44.1 kHz, 8 voices): Glass horde pad **27.2 %** of one M3 performance core at the default 1×, 43.0 % at 2×; Crushed bells 15.3 % at 1×. The target is **about 10.7 % per swarm** on the heaviest bank patch at 1× (B488, B494). That is 2.5 times faster than today.

**Method.** The stage shares were measured at 2× (the stage tool runs the presets' own oversampling). At 1× the per-step stages halve and the per-sample ones do not, so I rescaled them using the measured 2× → 1× saving (36.7 %): at 1× the blade is about 36 % of the render, the per-step frame about 31 %, couple and spread about 14 %, edge correction and DC about 9 % each, the swarm about 3 %. Each stage then gets a speed-up factor with a precedent from this repo where one exists:

| stage (share at 1×) | cautious | central | best | basis |
|---|---|---|---|---|
| blade evaluation (36 %) | 2× | 2.5× | 3.5× | single precision: 1.40–1.56× on the swarm core (B378 F11); tables for sin, cos and atan2 (B488 step 2, not measured); members four at a time, 9 members in three 4-lane groups at 75 % use (SPEC-SCALPEL §8.9) |
| per-step frame (31 %) | 1.5× | 2× | 3× | per-member parameter structs instead of shared-state overrides (SPEC §8.1); no `js::` rules (2–9 %, estimated in section 9); fused multiply-add allowed (forbidding it measured +6 % on the blade port, rule 7) |
| couple and spread (14 %) | 2× | 3× | 4× | draw only the spread slots a blade reads; one centre sum instead of two (section 1.8) |
| edge correction (9 %) | 3× | 5× | 10× | 99.4 % of steps search and find no edge (section 1.9): schedule the next edge per member instead of testing every step; the table adds under 1 % (ADR-222) |
| DC estimate (9 %) | 2× | 3× | 5× | closed forms where they exist, a cached estimate while its inputs hold still |
| swarm (3 %) | 1× | 1× | 1.5× | already cheap |
| **combined** | **1.8×** | **2.4×** | **3.5×** | Amdahl over the rows |
| **Glass horde pad, 8 voices, 1×** | **15.3 %** | **11.5 %** | **7.8 %** | 27.2 % divided by the combined factor |

**What the new engine adds back.** D8's ramp costs +2.7 to +9.9 % even when nothing moves (B482, measured by a critic); D6 and D9 have not been measured; D10 is under 1 %. With those always on, the central figure is about **11.8 to 12.6 %** and the cautious one 15.7 to 16.8 %. Two unknowns can move it much further: per-voice modulation at audio-like rates (B488's "whole device" cell, never measured) and per-member true stereo (Q25: up to about twice the blade stage).

**Error bars.** Each factor is ±30 to 50 %: two have measured precedents on a different core, the rest are engineering estimates. Read the result as "a clean engine reaches the 10.7 % target only at the central-to-best end, and only if true stereo is not per member". That is the same conclusion B488 reached for today's engine (a 2.7× target).

**What only a rebuild gives.** ADR-220 already lets B488 use tables, single precision and SIMD on today's engine, so most of the CPU gain is not unique to a rebuild. My estimate of the part that is unique (the per-step frame and the spread path, where today's structure is the cost) is 1.1 to 1.25× on top of what B488 can reach. The rebuild's larger gain is in time: today a one-value default change (ADR-221) stopped twice on parity rows and re-pinned 670 digest rows in each of two files; a sound fix (D7) took five rounds and two critics.

---

## 5. When to build it (B493 acceptance 5)

**Recommendation: build now, once four structural rulings are made (with the two smaller rulings that go with them: the fastest modulation rate and the precision rule). Leave the parameter table open until the deferred rows are ruled. Do not wait for FX, modulation and the mixer to be wired.**

### 5.1 The case for building now

- **Wiring first means wiring twice.** Every interface in section 3 would be built against today's engine (string-keyed parameters, one allocator per oscillator, a voice keyed by MIDI note, a built-in saturator, a JavaScript twin the labs must keep matching), then rebuilt against the new one.
- **The engine is not on the critical path.** H2-PLAN's longest chain is `engine → audit → seams → bend → sub → mix → rack → sluice → fxmorph → history → presets → release`, and its own reading is that "the C++ is no longer what the path waits on": the long pole is sources, routing and the rack, and the sub workshop (B399) and the mixer lab (B402) have not started. A rebuild now runs beside the work that is late, not in front of it.
- **Today's engine misses its CPU target by 2.5 times.** A whole-device measurement on it (B488's "whole device" cell) would measure an engine that is about to be replaced.
- **The parity tax keeps growing.** D9 and D10 are parked (ADR-223 item 5) because each new fix costs a switch, a mirrored JavaScript change, digest re-pins and a ledger row.
- **The rulings that matter are decisions, not builds.** What wiring would teach about the engine is the four shape questions (Q1, Q10, Q18, Q25). Each can be ruled from a page of options or a short lab: none needs the rack or the FX built.

### 5.2 The case for waiting

- **Unknown unknowns.** Wiring may surface requirements nobody has thought of: the modulation rate the matrix really uses, how morph flips feel on a blade mode, what the mixer's gain staging wants from a source.
- **Cheaper rulings.** The human rules from experience: a mod matrix running against a real engine is easier to judge than a page of options.
- **Today's engine works.** It plays in the preview (B458: "It loads and plays"), and the human has tuned the bank on it (ADR-217 A3, B478); a rebuild risks the presets' character (ADR-223 accepts this: "they don't need to sound the same").

### 5.3 Why "now, after four rulings" answers both

The engine's outside is small: notes in, per-voice modulation in, pitch in, per-voice or per-oscillator audio out, observations out. What the waiting case fears is a requirement on that outside that nobody saw. The four shape questions are exactly where such a requirement would change the engine's structure; everything else (a parameter's range, a modulation depth, a meter tap) can be added without a refresh, provided the spec makes the parameter table generated data rather than structure (B308 H4's lockfile already points that way). So rule the four, freeze the structure, and leave the table open until the deferred rows are ruled.

### 5.4 What must be known before the spec is frozen

| | build now | build after FX, modulation and the mixer |
|---|---|---|
| **must be ruled first** | Q1 voice ownership; Q18 envelope and per-voice signal ownership; Q25 true stereo's shape; Q10 per-voice modulation's reach; Q11 the fastest modulation rate; Q28 precision | the same six; wiring onto today's engine lets the human rule them from experience, at the price of building those interfaces twice |
| **must be ruled before the parameter table freezes, not before the build** | Q31 the 19 deferred rows and envelope decision 1; Q6 glide; Q14 stepped-key changes; Q19 the saturator | the same |
| **can stay open** | Q2 MPE's expression routing (the note-id interface is enough); Q23 FX-C's place; Q24 sleep; Q20 the meter taps; Q27 the heavy-patch policy | — |
| **must be measured** | the CPU target's ×1.5 factor (Q29), or the spec states it as an assumption | the whole-device cell on the old engine, which then has to be measured again on the new |
| **needs a dependency approved** | the WebAssembly toolchain (Q30), before the labs can play the new engine | the same, later |

### 5.5 A staging that keeps one refresh

1. The human rules Q1, Q10, Q11, Q18, Q25 and Q28 (a single options page, one recommendation each).
2. The spec is written from section 2's KEEP and RULING rows and section 3's interfaces, reviewed by a critic, ratified (ADR-223 item 2).
3. A different agent writes the acceptance tests from the spec. ADR-187 item 4(b)'s Layer-0 list (non-finite values, bit-determinism, block-size and sample-rate independence, DC and peak bounds, long-run stability, denormals, an aliasing floor, real-time safety, live parameters move the output and inert ones do not, per-parameter anchors) is a ready starting list; B404's aliasing and zipper suites were never built and belong in it.
4. The engine is built in stages behind the frozen structure. The parameter table stays open until Q31 is ruled.
5. FX, modulation and the mixer are wired once, to the new engine.

---

## 6. Ranked findings

Each is a proposal for a ROADMAP row, never a change made here. Classes use the auditor's nine shapes where one fits.

### HIGH

**H1. The engine's boundary is undecided, and the ratified designs pull four of its parts outside it.**
Evidence: the engine allocates voices, runs the amp envelope, pans, sums and saturates (`engine.h:211-257`, `:2229-2232`, `:2183-2188`, `:2343-2375`); ENV 1 is to be global to every pre-FX source (B370), filters are per voice (B303), the sub takes oscillator 1's phase and writes to a voice bus (SPEC-SUBOSC §2). Class: one rule with two implementations, waiting to happen. Minimal delta: rule Q1 and Q18 before the spec freezes; the spec draws the line.

**H2. Two oscillators would mean two voice laws.** Each `Engine` owns its pool and its allocator (`engine.h:2095`, `:211-257`); 1.0 has two composed oscillators (ADR-190), so at a full pool the two can refuse or steal different notes. Not live yet: the preview runs one engine. Class: one rule with two implementations. Delta: a device-level voice in the spec (Q1).

**H3. A voice is keyed by MIDI note, which MPE cannot use.** `noteOff(int note)` releases every gated voice with that key (`engine.h:285-288`); `retune` likewise (`:290-295`). Under MPE two notes can share a key on two channels (ADR-190 A4). Class: reachable but unrecorded (the note id never reaches the engine). Delta: note-id calls in the spec (section 3.1).

**H4. D6, D8 and D9 are one fault fixed three ways.** Control-rate values reach the audio as steps: the frame (`engine.h:1428-1502`), the spreads (`:1025-1096`), the quantized cut (D9's branch, +161 lines). SPEC-SCALPEL §12 named the fault before any of them. Each has its own switch, rules for joined members and mid-note flag changes, and its own check. Class: second copies. Delta: requirement B5 in the spec; D9 stays parked (ADR-223 item 5).

**H5. The charter still defines C++ correctness as parity with the JavaScript.** CLAUDE.md §Domain (the second invariant, scoped by ADR-187), `h2/README.md` rule 3 ("Correctness is parity first"), `specs/ACCEPTANCE.md` L0-1, `docs/port/h2-engine.md` ("the parity target") and H2-PLAN's `engine` row ("the golden the C++ is ported against") all say so; ADR-223 item 3 says parity "is no longer a goal" for the new engine. Class: prose asserting a relationship. Delta: an amendment to the charter's invariant and to `specs/ACCEPTANCE.md` L0-1, a sanction the human would have to give (both are human-gated); the other three are the lead's edits.

**H6. The human's culls and locks of 2026-10-02 are not in the code, and two records disagree about the locks.** The audit export culls the 11 blade-envelope keys and the stretch law and locks `motionCenter`, `inertiaCurve`, `dcMode`, `aaCarrier`, `aaXin` and `aaLoop` (`docs/audits/2026-10-02-engine-audit-round2-decisions.json`); the engine still computes the blade envelopes every sample (`engine.h:2207-2228`), still has law 5 (`swarm.h:173-176`) and still takes every locked key as a parameter. ADR-189 says D1 to D3's default "is ruled when horde 2's shell is built"; the audit says "lock". Class: an exclusion with no expiry; two sources disagree. Delta: the spec takes the audit export as its parameter input (section 2 does); one line in the records reconciles ADR-189 with the audit.

### MEDIUM

**M1. Detune and coupling are read unsmoothed, while smoothed copies are computed and discarded.** Both are in the smoother table (`engine.h:405`) and the swarm reads the raw targets (`engine.h:1384-1385`). Automating either steps the member frequencies every 16 samples. Parity cannot see it: the JavaScript does the same ("Read unsmoothed", `docs/design/scalpel-horde-engine.js:37-38`). Class: a fault the oracles cannot see; work computed and discarded. Delta: requirement B6.

**M2. `phaseMode` 1 and 2 sound the same, and 2 is the default.** Both set retrig on (`engine.h:1394`); 2's only effect is 9 more host-stream draws per note (`:1332-1333`), overwritten (`:1363`), which reshuffles the spread law's and the noise blade's random numbers. Class: a control that cannot fire. Delta: the merge the human already ruled (S13).

**M3. `cScale` is accepted and never read** (`blade.h:48`, `engine.h:422`). Class: reachable but inert. Delta: not in the spec.

**M4. The CPU savings are fitted to the bank and to still parameters.** The 17 kernels were "chosen from what the scenario set and the ledger presets render" (`engine.h:495-500`), and B488 notes the per-sample overrides assume parameters hold still within a block. No figure exists for a non-bank patch, a morph between kernels, or modulation. Class: a number nobody measured. Delta: B488's whole-device cell, and the new engine's CPU row judged on generated patches as well as the bank.

**M5. The noise blade draws from the shared host stream, so notes interact.** `nsRnd` → `rnd()` → the host stream (`engine.h:583-584`), the same stream note-ons draw from (B469). It also confounds every with-and-without measurement on such a patch (B488's A/B: the Breathing pad differs by +1.3 to +2.3 dB from draw shifts alone). Class: a fault the oracles cannot see. Delta: B469's ruling, then per-voice streams in the spec.

**M6. The JavaScript `min`/`max` rule inlines 570 `nan` call sites** into the release binary, 42 % of its libm call sites (`h2/engine/libm-calls.darwin-arm64.appleclang-16.txt`). They never run in the domain; they are code size and branches. Class: cost. Delta: none in today's engine (it would break parity); gone in the new one.

**M7. The culled-voice fade still depends on the host's block size** (up to 1.47e-8; `engine.h:2037-2038`), open since ADR-187 A2. Class: a fault the oracles cannot see. Delta: requirement V7.

**M8. The new engine's acceptance suite has a ready list that nobody has wired.** ADR-187 item 4(b) lists the Layer-0 invariants a demoted core needs; B404's aliasing and zipper suites were deferred at B385 and never built. ADR-223 retires the JavaScript without restating item 4's same-PR condition (frozen goldens plus the wired invariant suite). Class: uncovered behaviour. Delta: B494's tests start from that list (section 5.5 step 3).

**M9. The labs' new engine needs a toolchain that is not installed.** `emcc` and `em++` are absent; B372 and H2-PLAN open decision 39 ("install the toolchain") are unruled; ADR-223 item 4 depends on it. Class: a dependency nobody approved. Delta: the human's decision (Q30), before the build stage that targets the labs.

**M10. The frozen goldens are a protected path, so the new engine's re-baseline needs a sanction.** The self-digest files call themselves "A golden fixture (protected)" (noted by the 2026-10-10 repo audit), and the charter lists golden fixtures as protected. Class: a gate on the route. Delta: a sanction the human would have to give, in the same ruling that ratifies the spec.

### LOW

- **L1. Two values of 2π** (`engine.h:103` against `swarm.h:93, 149, 150, 207, 222, 225, 235` and `engine.h:1582`). Second copies; gone with the rebuild.
- **L2. One DC refresh countdown for every voice, reset by any retrigger** (`engine.h:1350`). An inherited quirk; gone with the rebuild.
- **L3. Two dead writes**, `s.mUnit2` and `s.law` (`engine.h:2180-2181`).
- **L4. The twin repeats `extract_core.mjs`'s slicing rule** "because the page and the worklet cannot import a Node module" (`scalpel-horde-engine.js:1667-1677`). Second copies; retires with the twin.
- **L5. H2-PLAN's engine and port rows predate ADR-220 and ADR-223** ("Last verified: 2026-10-09"; the `port` row's "the JS demotion ruling ... once the Layer-0 suite lands" is overtaken). Doc drift; the next plan refresh.
- **L6. CLAUDE.md still calls SCALPEL "an eighth CANDIDATE" whose "integration shape ... is the open question"**, already reported by the 2026-10-10 repo audit and unfixed. Doc drift; a charter edit the human would have to sanction.

---

## 7. Reduction budget

What the findings and the rebuild would remove and add. "Retire" means retired with today's engine, after the new engine and its own acceptance suite are wired: a gate is never removed before its replacement exists, and every change to `./verify` is the human's gate.

| | removed | added | net |
|---|---|---|---|
| engine lines (`h2/engine/`) | 2,994 today; about 3,950 with D9, D10 and the range table | about 2,100 to 3,000 | −950 to −1,850 against 3,950 |
| the JavaScript twin | 1,679 lines, 1 file | 0 (the labs run the C++ as WebAssembly) | −1,679 |
| the frozen ports (`h2/cores/`) | 4,041 lines, 5 files (`razor_core.h`, `swarm_core.h`, `force_core.h`, `glide_core.h`, `lift-ledger.json`) | 0 | −4,041 |
| parity tooling | 5,656 lines in 15 files (section 1.10) | 0 | −5,656 |
| gate invocations in `./verify` | about 12: the engine parity check and its contraction control, `composed_engine_check`, `divergence_ledger_check`, two `parity_floor_check` runs, the SCALPEL parity check and its control, the swarm parity, swarm48 and lift checks, `h2_lift_check` | the spec's acceptance suite, written by a different agent: estimated 10 to 15 checks (one per requirement group of section 2), plus a WebAssembly build check and the lab load check | about −12, +12 to +17 |
| behaviour checks kept and re-aimed at the new engine | `h2_frame_hold_check`, `h2_steal_fade_check`, `h2_spread_ramp_check`, the fuzz, the soak, the RealtimeSanitizer probe and lint, the self-digest pair, the harness self-test (4,208 lines across the first five) | re-targeted, not rewritten | 0 |
| new test code | — | estimated 3,000 to 5,000 lines, by analogy with those five checks | +3,000 to +5,000 |
| **total lines** | **about 14,400 to 15,300** (the engine as it is or as it would become, the twin, the ports, the parity tools) | **about 5,100 to 8,000** | **about −6,400 to −10,200** |

---

## 8. What changed since the earlier engine audits

There is no earlier report of this kind. The three it builds on:

- **The swarm core audit (2026-09-30, B378)** recommended lifting the legacy swarm core and listed F1 to F16 as fixes for the h2 copy. Since then the composed engine replaced the lift as the product (B385), F1's idea was applied to the composed engine as C1 to C3b, and F3 (the edge correction) was ruled as D10 (ADR-222). F2 (members above Nyquist) and F8 (unsmoothed parameters) were never carried to the composed engine; they are rows S17 and B6 here.
- **The engine audit decisions (2026-10-02, B376)** culled 36 rows, locked 13, merged 13 and deferred 19. Thirteen culled or locked keys are still in the code (H6); the 19 deferred rows are still deferred (Q31).
- **The repo audit (2026-10-10)** found the engine's eight function-like macros (`engine.h:78-133` then, `:82-148` now) and CLAUDE.md's stale SCALPEL sentence; both stand (section 1.4, L6).
- **What moved under all three:** ADR-220 (2026-10-14) lifted bit-identity for the CPU work and ADR-223 (2026-10-15) lifted it for the engine. The audits before them had to treat the match as fixed; this one does not.

---

## 9. What I could not determine

- **The run-time share of the JavaScript number rules.** It needs a variant engine without them, which is a new instrument. Estimated at 2 to 9 %: about 30 `js::` operations per member-sample on Glass horde pad (counted in `renderCallK`'s per-sample block, `stepMember` and `stepBlade`), against about 350 cycles per member-sample (27.2 % of a 4.05 GHz core over 44,100 × 72 member-samples a second), at 0.25 to 1 extra cycle each once out-of-order overlap is counted. At a full 1 to 3 cycles each it would be 9 to 26 %; I think that unlikely, because the blade's libm calls hold the critical path.
- **The cost of the six switches when off.** Each is a branch per sample or per tick; not measured.
- **D6's, D7's and D9's CPU when on.** Only D8 (B482) and D7 (one critic run) have figures.
- **The libm share after C1 to C3b.** The ledger's 26 to 27 % predates them; re-measuring needs the sampling profiler, which I did not run.
- **The engine's exact memory.** No tool prints it. From the struct definitions: about 1.3 KB per member, 14 KB per voice, about 125 KB per engine (B385's figure was 114 KB before D6 to D8).
- **Whether the min-spec factor ×1.5 holds** (Q29).
- **Anything only the ears can settle:** whether the dropped artefacts (phaseMode's two settings, the float-rounded fade, the draw order) were ever heard, and whether the new engine "sounds good". ADR-223 gives that to the human's A/B.
- **What the four shape rulings will be.** The size and CPU estimates move with them (sections 4.1 and 4.2 say how).

---

## Appendix A. The line classification

Line ranges as applied (priority order: test hooks, switches, JavaScript rules, kernel scaffold, culled), on `47ae038`:

- `engine.h` test hooks: 82-98, 105, 106-126, 127-148, 538-546, 1211-1225, 1228, 1232-1235, 1854, 2138-2140, 2379-2384.
- `engine.h` D1: 39-40, 673, 744-781, 1801, 1809, 2006-2007, 2009-2012, 2015, 2137. D2: 1802-1805, 1833. D3: 1337, 1691, 1896-1897, 1899-1901, 1904-1912, 2074-2076, 2136. D6: 41-43, 1130-1136, 1326, 1428-1502, 1511, 1513, 1883, 2079-2087, 2353-2360. D7: 44-50, 180-186, 229-232, 237-240, 245, 248-252, 270-283, 286, 291-293, 296, 300-335, 360-372, 1164-1166, 1170-1173, 1177-1197, 1237-1240, 1263-1321, 1324, 2022-2025, 2030, 2050-2061, 2070-2073. D8: 51-54, 1025-1096, 1101-1105, 1148, 1327, 2361-2366.
- `engine.h` JavaScript rules: 56-60, 897-918, 1159-1163, 1329-1333, 2019-2021, 2037-2038.
- `engine.h` kernel scaffold: 443-537, 547-580, 587-595, 615, 626-629, 665-670, 677-680, 784-788, 829-841, 844-854, 866-869, 2147-2173, 2274-2291.
- `engine.h` culled (blade envelopes): 1343-1344, 2207-2208, 2216-2228.
- `blade.h` D1: 56-57, 73-75, 81-82, 230-241. D3: 114-115. D6: 58-60, 116-118, 148-156. D7: 61-63, 163-178. D8: 64-67, 91-93, 119-122, 157-162. Culled: 132-136.
- `swarm.h` test hooks: 246-248. JavaScript rules: 10-11, 15-17, 33-36, 38-39, 72. Culled: 173-176 (stretch), 181, 191-195 (`motionCenter`).
- `js.h` JavaScript rules: 1-71. Test hooks: 85-101.

## Appendix B. The stage measurement, verbatim summary

```
stages: calibration 180.4 ms; load 2.43..4.61; head 47ae038; best of 5, interleaved
Glass horde pad, 1 voice(s): full 5.308 % of an M3 core   os 1: 3.378 %   remainder 29.0 %
Glass horde pad, 8 voice(s): full 42.949 % of an M3 core  os 1: 27.167 %  remainder 27.7 %
  tick 1.2 · phase 0.5 · blade 47.0 · PolyBLEP 11.7 · DC 5.7 · biquads -0.2 · blocker+tanh -0.5 · couple+spread 9.1
Crushed bells, 1 voice(s): full 2.981 % of an M3 core     os 1: 1.884 %   remainder 33.3 %
Crushed bells, 8 voice(s): full 24.305 % of an M3 core    os 1: 15.293 %  remainder 28.5 %
  tick 1.5 · phase 0.6 · blade 49.9 · PolyBLEP 12.0 · DC 6.2 · biquads 0.0 · blocker+tanh 0.2 · couple+spread 3.3
op counts: Glass horde pad 10630 edges over 1786050 member-steps; Crushed bells 7060 over 1190700 (0.0060 and 0.0059 per step)
```

Built from `tools/measure_h2_engine.cpp` at `47ae038` with `-std=c++20 -O3 -DNDEBUG -ffp-contract=off` (the CMake target's flags) and `-DH2_ENGINE_STAGES`, into a scratch directory outside the repo; the preset stream from `node tools/h2_engine_render.mjs --only '^P/.* :: chord$'`. The load guard waited out another agent's processes before each repeat and was never bypassed.
