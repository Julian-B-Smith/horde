# The wavetable member: how it wires in, and how the build runs

> **Origin.** Drafted for the horde lead session, 2026-11-07, on ROADMAP B649 and ADR-385 (the human, by poll: a
> new local repo with one builder; the oscillator first, then wired in; every stage ends with a CPU check, the
> fidelity gates and a capped listening batch; the CPU budget from the cost bench B648). Companion to
> `specs/SPEC-WAVETABLE.md`, which says what the oscillator does. This file says where it plugs in and how it gets
> built without repeating the clean-room build's mistakes.

**Status.** DRAFT for the lead and the human. The seam was read from the clean-room engine at commit `e72a092`
(spec round 64, 2026-11-07) in `~/Documents/Claude/cleanroom-engine/engine2/`. That engine still has rounds 62
to 64 queued, so S7 re-reads the seam at its own pinned commit before it starts.

---

## 1. The seam

### 1.1 Where a member's sample is made today

All paths below are under `engine2/core/` in the clean-room engine at `e72a092`.

- **`Swarm::render`** (`swarm/swarm.cpp`) renders one oscillator of one voice for a span of host samples, at the
  internal rate, members inside the sample loop. Per member, per internal sample, it:
  1. ramps the member's frequency `m.f` across the control interval (C.0) and sets SW-13's band-edge fade
     `m.level`;
  2. skips the member while it waits to enter (C.7);
  3. evaluates the base wave at the member's phase, **`wave(base_, θ + ½)`** (`blade/waves.h`), so the saw resets
     at θ = 0;
  4. adds the blades' contribution `d` through one of three paths: the direct path for Sync blades (`direct.h`),
     the wide path (`wide.h`), or the general `blade_sample` (`blades.cpp`), less C.16's DC estimate;
  5. forms the uncorrected output **`y0 = (x + d) · m.level`**, stores it in the member's history `e_hist`, runs
     ED-1's FIR `E` over that history, and adds the correction owed to this sample from the member's ring
     `pending`;
  6. multiplies by PA-6's fade `uni_gain`, the entry gain `(1 − entry_rest)` and the density scale `density_`,
     and adds into the left and right buffers by the member's pan gains;
  7. takes C.14's loop tap from `y0`;
  8. finds the base wave's own edges crossed before the next sample (`edges_of(base_)`) and adds their corrections
     to `pending` through `EdgeTables::add_edge`.
- **`Voice`** (`voice/voice.cpp`) calls `osc_[o].render(...)` for o = 0 then 1, each over a whole segment (at most
  512 samples, `Scratch::kMaxSegment`), and for a PA-5 crossfade also renders a full copy of the swarm, `xf_[o]`,
  and mixes the two by C.0's raised cosine.
- **`VoiceContext`** (`voice/voice.h`) carries everything voice-wide a voice reads at a tick: the parameters, the
  envelopes, both oscillators' settings (`OscSettings`, `swarm/swarm.h`), the clock.

### 1.2 What the wavetable member replaces, and what it reuses

In the structure file (`engine2/structure/engine-structure.json` in horde), a member is
`member.phase → member.base_wave → member.blade1 → member.blade2 → member.edges → member.dc → member.pan`, with
`member.loop` beside it. Under the **replace** reading:

| block | while `wt.on` | how |
|---|---|---|
| `member.phase` | kept | θᵢ and fᵢ exactly as today (WT-20) |
| `member.base_wave` | **replaced** | step 3 becomes the table member's read (WT-21, section 6 of the spec) |
| `member.blade1`, `member.blade2` | **inert** | step 4 is skipped; their rows stay in the manifest (§2.4) |
| `member.edges` | **only with sync** | steps 5's `E`/`pending` and 8 run only while `wt.syncIn` is not off; the sync restart's step and corner go through the same `add_edge` (WT-24, WT-32); otherwise the read goes straight to step 6 |
| `member.dc` | kept, new source | the table's transformations' cycle mean replaces the blades' estimate (WT-25) |
| `member.loop` | kept | C.14's tap from the member's output, its `xin` added to the read phase (WT-31) |
| `member.pan` | kept | unchanged |

Under the **beside** reading (§1.7) only `member.base_wave` is replaced, and every blade path reads the table.

### 1.3 The member's interface

A new concept, `TableMember`, in its own file (BD-1), with one per-member state record and one per-sample call. It
is described here as an interface; its code is the builder's.

**Per-member state** (added to the swarm's `Member` record, so PA-5's copy carries it): the sync phase; the
member's frame position and its spread position; its loudness gain under Linear (WT-16); its level index and
pitch-crossfade weight (WT-5); its DC estimate. About 64 bytes per member.

**Per sample, it receives:**
- θᵢ and the phase increment fᵢ/f_int (the swarm's);
- the published set's read-only view and the crossfade position between the outgoing and incoming sets (WT-15);
- the member's ramped transformation values: sync ratio, window, asym, bend (control rate), PM index (per sample
  when routed at audio rate);
- C.14's `xinᵢ` from the loop (the swarm computes it as today);
- under PM, the modulator: osc 2's member (i mod U₂) output times osc 2's governing envelope, from a buffer the
  voice fills (§1.8);
- the `EdgeTables` and the member's `pending` ring, while sync is on.

**It returns** the member's output before SW-13's fade (the swarm multiplies by `m.level` as today), and, while
sync is on, the corrections it adds to `pending` for the restart; it counts its reads, edges and level changes
into the swarm's tally.

**At each control tick** (`Swarm::control`), the swarm hands it the member's frame position (the voice's resolved
`wt.frame` plus offsets, plus the frame spread by C.10's positions), and the table member picks its level for its
peak read rate (WT-36) and its Linear gain (WT-16).

### 1.4 What stays untouched

The swarm's laws (placement, detune, coupling, drift, inertia, onset, start phase, gravity, pan: C.1 to C.8, SW),
the voice (allocation, stealing, the pedal, mono and legato: VO), the envelopes (EN, C.12), the per-voice filter
slot (RC-1), the sub and the noise (SN), the mixer and the output stage (OU), the morph resolver's laws (Appendix
D) apart from the one branch in §4, and the modulation system (Appendix E) apart from the destination rows in §2.
With `wt.on` off on both oscillators, the engine renders bit-identically to the engine without the table code
(WT-P4, tested as RC-6.4 tests a fifth source).

### 1.5 The table set: a new block, per oscillator, per instance

A new concept, `TableSet`, owned by the engine (`HordeEngine`), two of them (osc 1, osc 2). Voices read it through
a const view in `VoiceContext`. Proposed structure id: `instrument.table_set`, multiplicity 2, scope engine,
citing WT-10 to WT-17 (the lead adds it to the structure file; this draft does not edit it).

**Calls** (all on the audio thread unless marked):

| call | when | what |
|---|---|---|
| `prepare(host rate, quality factor, control interval, maxima)` | prepare | allocates everything for the maxima (RT-1): the canonical slots, the level sets, the rebuild scratch, the FFT plan. Not real-time. |
| `reset()` | reset | as WT-17: tables kept, published set rebuilt at the first boundary, no crossfade in flight |
| `accept(handoff)` | each control boundary | takes any table the store has finished preparing (§1.6), without waiting |
| `set_weights(w[4], law, frame)` | each control boundary | the morph's weights (§4), the law, the instance frame (Spectral log: the newest held voice's, WT-22) |
| `boundary()` | every R = 4 control intervals | rebuilds if an input changed (WT-14), starts a crossfade (WT-15), counts |
| `view()` | per voice, per segment | the read-only published set(s) and the crossfade position, for the members |

**Inputs:** the four corner tables (canonical spectra, from the store), the four weights, the law, the instance
frame. **Outputs:** the published level sets (one per law's need: one cycle under Spectral log, one per frame
under Linear's pre-blend, or none under Linear's read-time blend where the members read the corner sets
directly, WT-23), plus the loudness gain.

**Memory, the reference design** (the capped 6-point read of WT-6, N = min(8192, max(2048, 16·H)), one level per
octave, H = 1 to 1,024: a level set is 36,864 samples, **144 KiB** in float32, the largest level 32 KiB):

| part | per oscillator |
|---|---|
| canonical spectra: 4 corner slots + 4 staging slots, 128 KiB each, plus the log-magnitude cache (64 KiB each) | 1.5 MiB |
| Spectral log: published sets, three (heard, heard, building) | 0.42 MiB |
| Linear, strategy (a): pre-built level sets for every corner frame, 4 × 16, plus one staging corner (16) | 11.25 MiB |
| **total, strategy (a)** | **≈ 13.2 MiB**, so **≈ 26.3 MiB per instance** |
| Linear, strategy (b) instead: three pre-blended sets of 16 frames | 6.75 MiB per oscillator in place of 11.25 |

**By level spacing** (WT-4, WT-5; the same read and cap):

| spacing ρ | levels | level set | strategy (a), per instance | strategy (b), per instance |
|---|---|---|---|---|
| 2 (one per octave, built) | 11 | 144 KiB | ≈ 26.3 MiB | ≈ 17.5 MiB |
| 2^½ (half-octave) | 21 | 266 KiB | ≈ 46.1 MiB | ≈ 28.5 MiB |
| 2^¼ (quarter-octave) | 41 | 511 KiB | ≈ 85.8 MiB | ≈ 51.5 MiB |

Half-octave levels fit under WT-8's proposed 48 MiB cap with either Linear strategy, barely with (a); quarter-octave
levels fit with neither. The uncapped read (N = max(2048, 16·H)) costs about 22 % more at every spacing. For
comparison, the clean-room manifest states 43.2 MB for the whole engine today (`manifest_memory_bytes` at
`e72a092`). These are allocations; what the cache sees is the levels in use, which the timed runs judge (WT-P1a).

**Real time.** Nothing on the audio thread allocates, locks, makes a system call or waits (RT-1). A rebuild's work
is bounded by the level sizes (RT-2). The inverse FFT is the builder's own code or a dependency the human approves
(charter: adding a dependency is a human gate).

### 1.6 Tables in: the store and the handoff

Recipes (WT-42) are turned into spectra **off the audio thread**: generators, captures and library reads run on a
worker the shell owns. Each finished table is written into a free staging slot preallocated by the table set,
then published by one atomic store; the audio thread picks it up at its next control boundary with one atomic
load (single producer, single consumer). The audio thread never waits for a table: until the new one arrives, the
corner keeps playing its old table. Linear strategy (a)'s pre-built level sets for the new corner are built by the
worker too, into the staging corner. A second table change for the same corner while one is staged replaces the
staged one (the newest wins, as WT-15's sets do).

### 1.7 The beside reading: what the blades would need

If the human rules that the blades cut a table member, the blades must read the table where they now read the
base wave in closed form. At `e72a092` that is about twenty call sites in `swarm/blades.cpp`, `swarm/direct.h`,
`swarm/wide.h` and `blade/dc.cpp`, using five things:

| today (`blade/waves.h` and friends) | what a table must supply | available exactly from the spectra? |
|---|---|---|
| `wave(base, x)` | the value at a phase | yes (the level read) |
| `wave_slope(base, x)`, `deriv(...)` | the slope (and its change) at a phase, for ED-6 and ED-7 corrections at window edges | yes: harmonic n × 2πn·i, a second level set |
| `jumps_of(base)`, `edges_of(base)` | the wave's own edges | none: a table has no edges |
| `mean_over(base, a, b)` | the mean over a span, for Crush's held steps and C.16 | yes: the antiderivative, harmonic n ÷ 2πn·i, a third level set |
| `dc_estimate(stack, base, …)` | the blades' cycle mean | yes, from the antiderivative |

So beside costs about **three times the level-set memory** (value, slope, integral) and an interface in place of
the `Wave` enum at every one of those sites. It is a real port, not a switch. **This build does not do it** unless
the human rules beside before S7; the contract keeps the value, slope and integral reads in the table member's
interface so it stays possible.

### 1.8 What the voice must change

- **Render order for PM.** With `osc1.wt.pm` above 0 and live, the voice renders osc 2 before osc 1 and keeps
  osc 2's member outputs, each times osc 2's governing envelope, in a per-member buffer for osc 1 to read
  (9 members × 512 samples of doubles, 36 KiB, one shared scratch since voices render one at a time). The order
  changes no other output: the sources sum.
- **Why only one direction.** The voice renders each oscillator over a whole segment (§1.1). PM both ways at once
  needs the two swarms interleaved sample by sample. That is a restructuring of `Swarm::render`'s loop, not a
  wiring change, so `osc2.wt.pm` is reserved (§2).
- **Spectral log's frame.** The engine finds the newest held voice (VO-10's note counter, MS-46's rule) at each
  control boundary and hands its frame to the table set.
- **PA-5.** `wt.on`, `wt.law` and `wt.syncIn` join the oscillator's stepped keys (`stepped_equal`, `with_stepped`
  in `swarm/swarm.h`); a flip renders the outgoing setting on the swarm's copy, as today. The table set is per
  instance, so the copy does not duplicate it.

### 1.9 The engine's API, observation and counters

- **Tables in.** A call beside `set_ratios`: `set_table(osc, slot, recipe)` (not real-time; the shell's worker
  path, §1.6), and a sample-timed event carrying each oscillator's four weights (§4).
- **State.** The engine's IF-4 document gains a top-level member `wt`: per oscillator, the four slots' recipe ids
  the engine last received, as it keeps the `ratios` text. Added without a version change (RC-9.2, RC-9.3, RC-9.4).
  The recipes themselves live in the patch's table dictionary (§3).
- **Observation (IF-10).** Per oscillator: the weights, the law, the published set's levels and sizes, the
  crossfade position, the rebuild count. Per member: its frame position, level index and pitch-crossfade weight,
  its output before the fade. The gates read these; no product branch is added for them (BD-4).
- **Counters (IF-9).** Rebuilds, level sets built, table reads, sync edges corrected, peak-limit engagements,
  tables refused, handoffs taken.

---

## 2. Parameters

### 2.1 The new rows

Each oscillator's block gains these rows (SPEC-ENGINE 5.1's format). Keys are `oscN.wt.…`. Ids are the oscillator's
block base (0x1000·(o + 1)) plus the offset shown, so osc 1's are 0x1400 to 0x140A and osc 2's 0x2400 to 0x240A.

| offset | key | label | unit | range | default | law | class | rate | PV | MS-12 scope | what it does |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 0x400 | `wt.on` | Table | — | off, on | off | step | S | T | — | — | The members read the table set instead of the base wave (WT-20). Off reproduces every existing patch (RC-9.2). |
| 0x401 | `wt.law` | Blend | — | Linear, Spectral log | Linear | step | S | T | — | — | How the corner tables combine (WT-11, WT-12). |
| 0x402 | `wt.frame` | Frame | — (shown 1–16) | 0..1 | 0 | lin | M | C | yes | per-voice; the newest note under Spectral log (WT-22) | Where in each table the members read. |
| 0x403 | `wt.frameSpread` | Frame spread | ± | −1..1 | 0 | lin | M | C | yes | per-voice | Fans the frame across the members by the spread law (WT-22). |
| 0x404 | `wt.syncIn` | Sync | — | off, × pitch, Hz | off | step | S | T | — | — | Turns sync on, following the note or at a fixed frequency (WT-32). |
| 0x405 | `wt.sync` | Sync ratio | × | 1..16 | 2 | log | M | A | yes | per-voice | Table cycles per member cycle (WT-32). |
| 0x406 | `wt.syncHz` | Sync | Hz | 20..12 000 | 880 | log | M | A | yes | per-voice | The fixed sync frequency: a formant (WT-32, WT-33). |
| 0x407 | `wt.window` | Window | % | 0..1 | 0 | lin | M | C | yes | per-voice | From hard sync (0) to windowed bursts (1) (WT-33). |
| 0x408 | `wt.asym` | Asym | ± | −1..1 | 0 | lin | M | C | yes | per-voice | Moves the half-cycle point (WT-34). |
| 0x409 | `wt.bend` | Bend | ± | −1..1 | 0 | lin | M | C | yes | per-voice | Bends time toward the cycle's start or away (WT-34). |
| 0x40A | `wt.pm` | PM from osc 2 | index | 0..10 | 0 | pow | M | A | yes | per-voice | osc 2 modulates osc 1's read phase (WT-30). **osc 1 only; `osc2.wt.pm` is reserved (5.1)** until per-sample interleave is built. |

**Not a host parameter, corner state:** `oscN.wt.table`, each corner's table recipe reference (WT-51), held by the
morph system beside `oscN.ratios`.

**Counts.** 11 rows per oscillator: 22 parameters, one reserved. The manifest grows from 330 to 352; reserved rows
from 13 to 14; host parameters from 317 to 338.

### 2.2 How the free id range was verified

- The clean-room manifest at `e72a092` was compiled on its own (`core/manifest/manifest.cpp` and `memory.cpp`
  against the harness header) into a scratch program that printed every row's id and key. It lists **330 rows**,
  as SPEC-ENGINE 5.8 says.
- Per oscillator block, the used offsets are 0x000 to 0x02F (48 oscillator rows), 0x100 to 0x11D (blade 1's 30),
  0x200 to 0x21F (blade 2's 32, with its two blade-2-only rows) and 0x300 (`stream`). Nothing uses 0x400 to
  0xFFF in either block. The other blocks are the voice (0x0001 to 0x0012), the sub (0x3000), the noise (0x4000),
  ENV 2 to 4 (0x5000 to 0x5209), and two test-build-only blocks (0x6000 the fifth source, 0x7000 the added
  parameter, `manifest/manifest.h`).
- Horde's tree has no use of 0x1400 or 0x2400 (searched `specs`, `docs`, `engine2`, `tools`). The morph's and the
  modulators' host parameters are appended to the lockfile after the engine's (IF-11, MS-40) and never reach the
  engine, so they do not share these blocks.
- **One wording change:** SPEC-ENGINE 5.8 says each oscillator's `stream` "takes its block's last id". With these
  rows it no longer is. The ids do not move (RC-9.1); only that sentence changes, to "takes the id after its blades'
  rows".

### 2.3 Rows that become audio-rate or stepped (SPEC-ENGINE amendments)

- **PA-4** gains `wt.sync`, `wt.syncHz` and `wt.pm` as audio-rate destinations (so an envelope can sweep sync or
  the PM index sample by sample on the engine path, MS-41). PF-2 is then measured again, as round 62 did when PA-4
  grew (ADR-379 A1).
- **PA-5** gains `wt.on`, `wt.law`, `wt.syncIn` and a change of a corner's table (under outcome A) among the
  stepped changes that crossfade by 5 ms.
- **PA-8** gains the live conditions below.

### 2.4 Live and inert (PA-8)

**The new rows:** every `wt.` row except `wt.on` is live only while `wt.on` is on. `wt.frameSpread` is live while
the law is Linear and `unison` > 1. `wt.sync` is live while `wt.syncIn` is × pitch, `wt.syncHz` while it is Hz,
`wt.window` while it is not off. `osc1.wt.pm` is live while osc 2 is on.

**The existing rows while `wt.on` is on**, under each reading:

| rows | replace | beside |
|---|---|---|
| `baseWave` | inert | inert |
| every blade row of both blades (5.4), `stacking`, `upperHears`, `collisionPitch`, `collisionBite`, `quantize`, the `ratios` text | **inert** | live, as today |
| `crossMod`, `feedback`, `loopDrive` | live: they push the read phase (WT-31) | live, on the carriers and the read |
| `spreadLaw`, `spreadDrift`, `motionGrain` | live through `wt.frameSpread` | live |
| `bandLimit` | live while sync is on (it scales the restart's corrections) | live |
| every swarm, pitch, pan and envelope row | live, unchanged | live, unchanged |

No row is deleted. Every id stays (RC-9.1).

---

## 3. State

### 3.1 The recipe, format version 1

A recipe is a small JSON object. Its **id** is a 64-bit FNV-1a hash (MO-6's constants, extended to 64 bits) of its
canonical text: sorted keys, no whitespace, without `name`. A library file, a patch and a preset all name a table
by that id. Two recipes with the same content have the same id on every machine.

```
generator: {"v":1, "kind":"gen", "gen":"<generator id>", "genV":<int>, "set":{…}, "seed":<uint32>, "name":"…"}
capture:   {"v":1, "kind":"cap", "src":["<id A>","<id B>","<id C>","<id D>"], "x":<0..1>, "y":<0..1>,
            "frame":"all" | <0..1>, "law":"linear" | "spectral-log", "name":"…"}
```

- `gen` ids for the factory set are `factory/sine-saw`, `factory/pulse`, `factory/vowel`, `factory/sync`,
  `factory/fm`, `factory/fold`, `factory/comb`, `factory/organ` (WT-40); the random generator's is `random`
  (WT-46).
- A capture's sources are ids, resolved in the patch's dictionary, so a capture of captures is a graph, not a
  copy: shared sources are stored once.
- No wall-clock value is part of a recipe or its id (DT-5). A creation time, if the library shows one, is the
  shell's file metadata.

### 3.2 What goes where

| place | holds | bound |
|---|---|---|
| **the host's state chunk** (the patch) | a `tables` dictionary {id: recipe} with every table any corner of either oscillator references, and every capture's sources, transitively; the corners hold ids | at most **64 recipes** and **32 KiB of recipe text** per patch; a capture depth of 8 (WT-44). A save that would exceed it is refused, named, before anything is written. |
| **an exported preset file** | the same dictionary, **plus the rendered spectra** of every table a corner plays that is not a factory generator (captures, random tables): float32, base64 | about 171 KiB per embedded table; at most 8 corner tables, so ≤ 1.4 MiB per file. A file, never a host chunk. |
| **the user library** (device-wide, the application-support folder, WT-45) | one file per table: its recipe and its rendered spectra | — |

The host chunk never carries table data, only recipes (ADR-384 #2; the global lesson that a host silently keeps an
old chunk when a large one fails to save).

### 3.3 Loading

1. Each recipe is rebuilt from its generator and version (on the worker, §1.6).
2. A generator version this build does not know: the embedded spectra if the file carries them, else the library
   copy by id, else the default factory table for that corner. **Every fallback is reported by name** in the
   load report (PA-7's rule: nothing is fixed silently).
3. A capture whose sources cannot all be rebuilt: the same order (embedded, library, default), reported.
4. Rebuilt factory tables are checked against their goldens in the test suite, not at load.

---

## 4. Modulation and morph

### 4.1 Destinations

| destination | scope (MS-12) | path (MS-41) | notes |
|---|---|---|---|
| `wt.frame` | per-voice | outside, control rate | Under Spectral log the table set reads the newest held voice's value (WT-22). The page warns, as MS-46 has it warn for a per-note source on a shared destination, whenever the law is Spectral log. |
| `wt.frameSpread`, `wt.window`, `wt.asym`, `wt.bend` | per-voice | outside | |
| `wt.sync`, `wt.syncHz`, `wt.pm` | per-voice | engine path for ENV 1 to 4, velocity and per-note LFOs, once PA-4 lists them (§2.3); outside otherwise | |
| `morph.x`, `morph.y` (the table set's X and Y) | shared | outside | from a global LFO or the 2-D random only (MS-29) |
| `wt.on`, `wt.law`, `wt.syncIn`, `oscN.wt.table` | — | — | not destinations: stepped or corner state (MS-12) |

**Per-note sources to shared destinations (MS-46, MS-47).** No `wt.` row is `shared`, so MS-47's Refuse setting
mutes none of them. X and Y are shared, and MS-29 already refuses every per-note source on them. So ADR-384 #9
(per-note sources to X and Y by MS-46 and MS-47) does not arise while MS-29 stands. The one place a per-note value
drives a shared thing is Spectral log's frame; see §7 B2.

### 4.2 The morph's side

The resolver gains one output, sent at control-interval boundaries when it changes (MO-20's timing): **per
oscillator, the four corners' table ids and four weights**. The engine routes it to the table set (§1.5).

| | outcome (A): Structural (WT-52) | outcome (B): spectral-blend class (WT-53) |
|---|---|---|
| Blend, Stepped | one-hot at the key's quantum pick and latch (MO-7, MO-8, MO-13) | MO-4's unglided weights (stepped position under Stepped), identical tables pooled, Count Two by MO-9, glided by MO-5 (b), restricted to the oscillator's on corners (MO-26 (2)) |
| Quantum | one-hot at the pick | one-hot at the pick |
| a change of pick | PA-5's 5 ms crossfade | PA-5's 5 ms crossfade |
| Exempt | corner A | corner A |
| morph off | `morph.audible`, one-hot | `morph.audible`, one-hot |
| Save current sound (MO-25) | the pick's id | a capture recipe of the heard blend when the corners differ (WT-56) |

The engine is the same under both: it blends four tables at whatever weights arrive. The difference is a branch in
the resolver for one key class, and the GUI's corner cards.

**ADR-254 (the FX device).** None while it hosts no oscillator. The user library is device-wide; if the FX device
ever hosts a table-driven module it reads the same library (ADR-384).

---

## 5. The build process

### 5.1 The repo and the roles

- **Repo:** `~/Documents/Claude/horde-wavetable`, a new local git repository, **never pushed** (ADR-385 ruling 1).
- **One builder session** builds. It is a builder under every clean-room builder rule except blindness: it reads
  this contract, the spec and the inputs in §5.2.
- **The horde lead owns** the gates (writes them, proves each one's plant and control engine-free before the stage
  opens), the CPU and fidelity measurements, the human's listening pages, and every ruling. No blind test author
  (ADR-385 ruling 1).
- **The human** approves WT-BUDGET before S1, listens at the end of every stage, and rules the open questions.

### 5.2 What the builder reads

- `specs/SPEC-WAVETABLE.md` and this file, copied into the repo at the stage's start, each with the horde commit
  it came from.
- The QUADRANT bench (`reference/quadrant/quadrant-wavetable.html`) as a listening reference and source of test
  cases, **not** a parity target; its DFT loops and its 2048-sample linear read are not to be ported (WT-6, WT-41).
- SPEC-ENGINE's sections the spec cites (copied).
- From S7: the clean-room engine at a pinned commit, as a read-only `git archive` export in `vendor/engine2/`.

### 5.3 The layout

```
horde-wavetable/
  BRIEF.md       the lead's brief for the current stage (scope, gates, CPU share, clips to render)
  QUESTIONS.md   the builder's questions, numbered W-1, W-2, …  (never guess: ask)
  ANSWERS.md     the lead's answers, from the spec and the human's rulings only
  REPORT.md      the builder's report per stage: commit, what was built, gate results verbatim, instruction counts
  STAGES.md      the stage table, each stage's state, and the human's notes as rows (§5.5)
  specs/         SPEC-WAVETABLE.md, CONTRACT.md, the cited SPEC-ENGINE extract, each with its source commit
  core/          table/ (spectra, generators, recipes)  blend/ (laws, rebuild)  level/ (levels, the read)
                 member/ (the table member, transformations)  store/ (the handoff)  api/
  harness/       the swarm stand-in, one envelope, the offline render, the WAV writer
  tests/         one directory per stage: its gates, each with its plant and its control
  tools/         the alias meter (QS-1's method), the click detector (QS-4's, copied from horde unchanged),
                 the instruction counter, the clip renderer
  goldens/       the generators' goldens, per version (WT-43)
  listening/S1/ … the clips, and the page for the human's notes
  vendor/        (S7 on) the clean-room engine export, read-only
```

**The harness** gives the human sound from S1: a swarm stand-in (U members placed by C.2's JP-8000 curve at a
fixed detune, start phases from a seeded stream, no coupling, no drift), one envelope by C.12's law, and an
offline render to WAV at 44.1 and 48 kHz. It uses the clean-room engine's phase convention (WT-20), so nothing
changes meaning at S7.

### 5.4 The CPU discipline

The clean-room build measured CPU only at the end and read 7 to 10 times its target (ADR-354: 10.7 % of a core
targeted; Glass horde pad at 61.89 %, REPORT.md §7). Here:

- **Every stage reports two numbers beside its test count:** the instruction count of the stage's cells (the
  Mac's retired instructions for a fixed offline render, ADR-380 A1), at once, and its share of WT-BUDGET.
- **The conversion** from instructions to a share of a core is fixed once, at S0, by a timed PF-3 run in the
  quiet window, and kept for the build; the lead re-fixes it only if a timed run disagrees by more than 10 %.
- **The cost bench's evidence** (B648; spec WT-P1a): one oscillator's worst cell is about 3.9 % of a core and
  moving costs 2 to 8 times static, so every stage's cells include one with the morph moving. **Instruction counts
  miss memory stalls**: a stage whose count passes can still fail its timed run, which is why a timed result over
  the share reopens the stage.
- **Timed runs** (PF-3's method, its load guard, best of 5) only between 2 and 6 am (ADR-380 A1), queued by the
  lead, one batch a night. A timed result over the stage's share reopens the stage.
- **The shares** of WT-BUDGET (PROPOSED; the lead fixes them with the budget):

| stage | cells measured | cumulative share of WT-BUDGET |
|---|---|---|
| S1 | one member reading a static table, scaled to cell 1's member-samples | ≤ 35 % |
| S2 | cell 2 (rebuilds under a moving XY), one member per voice | ≤ 55 % |
| S3 | cells 1 and 2 with the pitch crossfade | ≤ 65 % |
| S4 | cells 1, 2, 3 at U = 7 (the first full measurement) | ≤ 85 % |
| S5 | cell 4 | ≤ 100 % |
| S6 | cells 1 to 4 again: no audio-path change | within 2 % of S5 |
| S7, S8 | PF-3's cells in the clean-room engine, the swarm's own cost reported apart | ≤ 100 % |

- **Every CPU check has a plant and a control:** a build with the read loop run twice must read about twice the
  read cost (the counter sees the work); a render with no notes must read the idle floor (the counter reads zero
  work as zero).

### 5.5 The listening discipline

- At the end of every stage the lead renders **at most 6 clips** from a clean export of the stage's commit
  (§6.3), and puts them on one page with a notes field per clip and the questions below.
- The human's notes become **rows in STAGES.md before the next stage starts**: each is built into the next
  stage's scope, ruled out with a reason the human accepts, or deferred to a named stage. A stage does not close
  with an unanswered note.
- Clips are named `sN-k-<what>.wav`, 44.1 kHz, 24-bit, peak-normalised to −1 dBFS with the gain stated on the page,
  so loudness differences between clips are the sound's, not the render's.

### 5.6 The stages

Each stage is small enough to be heard. A stage starts only when its gates are written and proven by the lead
(plant fires, control reads zero) and its brief is in BRIEF.md.

| stage | scope | the human hears | gates (each with plant and control, §9 of the spec) | CPU |
|---|---|---|---|---|
| **S0** (lead) | repo, harness skeleton, the alias meter, the click detector, the instruction counter; WT-BUDGET proposed and approved; the instruction-to-time conversion fixed | — | every S1 gate proven engine-free | conversion fixed |
| **S1** | the data model, the eight factory generators, levels, the read; one member, a static table, one frame | factory tables | WT-F1, WT-F5, WT-F8 (static), WT-F9, WT-F13; generator accuracy (WT-41) and goldens (WT-43); table gates (WT-47) | ≤ 35 % |
| **S2** | the four-corner blend, both laws (Spectral log's frame rule by WT-3), rebuild and crossfade with the two cheaper schemes measured (WT-15), loudness normalisation, the weights input | morph sweeps | WT-F4 (sweeps on the sine probe), WT-F6, WT-F7, WT-F10, WT-F5 at every block split | ≤ 55 % |
| **S3** | levels across pitch, the pitch crossfade, the read-rate level choice; ρ as a constant | glides across the range | WT-F1 at C1 to C8 at 44.1, 48 and 96 kHz; WT-F2; WT-F4 over glides | ≤ 65 % |
| **S4** | U members from the stand-in swarm, per-voice frame (Linear), frame spread, Spectral log's newest-voice frame, 8 voices | unison and scanned plucks | WT-F4, WT-F5, WT-F7 at U = 7; WT-P4 (cost when off); memory against WT-8 | ≤ 85 % (first full) |
| **S5** | sync and window, asym and bend, PM from osc 2, the loop reuse, DC removal; High quality | growls, sync leads, formants | WT-F3 (reported), WT-F8 with every transformation, WT-F12, WT-F4 on the probe with each amount swept; sync's edge correction by QS-1's method at r = 2.5 | ≤ 100 % |
| **S6** | recipes, the dictionary, capture, the library, state, the random generator's slot | captures against their live blends | WT-F11; recipe round trip byte-identical; size bounds (§3.2) refused at the limit; fallbacks reported | within 2 % |
| **S7** | the seam in the clean-room engine (vendored, pinned): the table member inside `Swarm::render`, the table set in `HordeEngine`, the voice's order and PM buffer | the table member in the real swarm | WT-P4 (with `wt.on` off, all 83 bank presets render bit-identically to the pinned commit); the clean-room suite green on the export; WT-F1, WT-F4, WT-F5 again in the engine | PF-3 cells in the engine |
| **S8** | the morph branch (§4.2, both outcomes behind the one open ruling), the modulation destinations, the engine's state member, the test plug-in | the morph across tables, and the human playing it | MO rows the branch touches; MS-29 refusals; RC-9 round trips; rtsan and TSan on the plug-in's render | PF-3 timed in the window |

**The listening batches** (each ≤ 6 clips; what each shows; what the human is asked):

| stage | clip | what it demonstrates | asked |
|---|---|---|---|
| S1 | `s1-1-sine-saw-scan` | Sine → saw stepped through its 16 frames at C3 | Does the scan sound like the bench? |
| | `s1-2-factory-tour` | all eight factory tables, 2 s each, frame 0.5, C3 | Which earn their place, and what is missing? |
| | `s1-3-vowel-register` | Vowel ah → ee at C2, C3, C4, C5 | Does the vowel hold across registers? |
| | `s1-4-bass-read` | the bench's read against the build's on the saw, C1 to C2 | Can you hear the difference? (the WT-F1 plant, by ear) |
| | `s1-5-sync-table-low` | Hard sync 1 → 6 at C1 | Is the low end clean? |
| S2 | `s2-1-linear-diagonal` | A → D over 8 s, Linear, the default corners | Is the path musical? |
| | `s2-2-log-diagonal` | the same, Spectral log | How does it differ from Linear? Keep both? |
| | `s2-3-opposed-centre` | saw and reverse saw corners, the centre, normalisation on | Does the centre hold its level? |
| | `s2-4-orbit` | a 0.2 Hz orbit, Linear then Spectral log | Any grain or zipper from the rebuilds? |
| | `s2-5-log-phase` | Spectral log with the linear-sum phase, then a fixed phase | Which is steadier? (QUADRANT's open question) |
| | `s2-6-log-frame-scan` | Vowel scanned 0 → 15 under Spectral log, log-domain frame interpolation, then exact (WT-3) | Which in-between frames sound right? (C6) |
| S3 | `s3-1-glide-octave` | saw glide C1 → C8 over 12 s, ρ = 2 | Do you hear the brightness rise and fall per octave? |
| | `s3-2-glide-half` | the same, ρ = 2^½ | Better enough to pay memory for? |
| | `s3-3-glide-quarter` | the same, ρ = 2^¼ | Better enough to pay memory for? |
| | `s3-4-bass-line` | a C1–C2 line on Hard sync | Low-end detail and weight? |
| | `s3-5-top-lead` | a C5–C7 line on Vowel | Any harshness or alias at the top? |
| | `s3-6-vibrato-boundary` | ±50 cent vibrato across a level boundary at C6 | Any brightness wobble? |
| S4 | `s4-1-unison-pad` | PF-3's chord, U = 7, Linear, static | Does it sound like horde's swarm? |
| | `s4-2-scanned-plucks` | ENV 2 sweeping the frame per note, an arpeggio | Is per-note scanning worth its cost? |
| | `s4-3-frame-spread` | frame spread 0 → 1 on a held chord | Is the spread a new texture or mud? |
| | `s4-4-log-newest-note` | Spectral log, frame by velocity, a legato line | Is the shared frame acceptable under Spectral log? |
| | `s4-5-unison-1-9` | the same patch at U = 1 and U = 9 | Density and balance? |
| | `s4-6-full-shape` | XY under an LFO plus per-voice scanning | Does it feel like discovery? |
| S5 | `s5-1-sync-lead` | sync ratio swept by an envelope, × pitch | Does it replace a Sync blade lead? |
| | `s5-2-formant` | sync in Hz, window 1, sync swept 300 → 3,000 Hz, a chord | Does it replace a formant blade? |
| | `s5-3-pm-growl` | osc 2 sine → osc 1 Vowel, index by envelope, bass | Growl worth having? |
| | `s5-4-swarm-fm` | feedback and cross-mod on a 7-member table swarm | Usable, or chaos? |
| | `s5-5-warps` | asym and bend swept on Pulse and Sine → saw | Are the warps distinct enough to keep both? |
| | `s5-6-quality` | a high-index PM lead, Standard then High | Is High needed for this? |
| S6 | `s6-1-capture-vs-live` | a live blend, then its capture in all four corners | They should sound identical. Do they? |
| | `s6-2-blend-of-captures` | a sweep across four captures | Do blends of blends open new ground? |
| | `s6-3-portable-preset` | a preset reloaded after its library table is deleted | Identical to before? |
| S7 | `s7-1-coupling` | coupling −1 → +1 on a table member swarm | Does the swarm's physics still read? |
| | `s7-2-drift-inertia` | drift and inertia on a table pad | Same character as with the base wave? |
| | `s7-3-gravity` | gravity on a held table chord | Still settles to just intervals? |
| | `s7-4-in-engine-sweep` | an S2 sweep, now in the engine | Same as S2's? |
| S8 | `s8-1-outcome-a` | a morph across corners with different tables, tables flipping (A) | — |
| | `s8-2-outcome-b` | the same morph, tables blending (B) | Which outcome for 1.0? (the open ruling, by ear) |
| | `s8-3-lfo-x-env-frame` | a global LFO on X and an envelope on the frame, in the engine | Does modulation feel fluid? |
| | `s8-4-human-take` | the human playing the test plug-in in Live | The human's own notes |

S8's two outcome clips are always played as **one oscillator's member, morphing**, never as two engines (GUI-1).

### 5.7 A change in the middle of the build

A spec change ruled while the build runs goes to **the first stage not yet started**, never into a finished one.
A finished stage reopens only by the human's ruling, and then as a row in the next stage. The spec is small and
staged, so the build is never more than one stage behind it (the clean-room build ended rounds 62 to 64 behind).

---

## 6. Gate rules

### 6.1 What closes a stage

All four, recorded in STAGES.md with the commit:
1. **every gate of the stage green**, run by the lead on a clean export (§6.3), each gate's plant firing and its
   control reading zero in the same run;
2. **the instruction count within the stage's share** of WT-BUDGET (§5.4), beside the test count;
3. **the listening batch heard and every note answered** (§5.5);
4. **REPORT.md** naming the commit, the gate output verbatim, the counts, and anything the builder left open.

### 6.2 What happens on red

- **A red gate changes the engine.** Never the gate.
- **A gate moves only with engine-free evidence and the human's ruling** (ADR-247), recorded in the build's
  STAGES.md and as a horde ADR amendment. The evidence never includes the builder's numbers.
- **Red halts forward work** (charter): no stage starts on a red one, and no change stacks on red.
- **A plant that does not fire, or a control that does not read zero, is a red gate**, and the lead fixes the gate
  before the builder touches the engine (the detector-shares-assumption trap: a gate that cannot fail proves
  nothing).
- **Over budget:** the stage does not close. The builder reports what would close the gap; the lead takes it to
  the human only if the gap needs a sound-changing trade (ADR-380 A1: changes under −120 dBFS may proceed, louder
  ones go to the human).

### 6.3 Verify from a clean export

The lead runs every gate on `git archive <commit>` extracted into a fresh scratch directory, never on the builder's
working tree, so in-progress edits cannot leak into a verified result. REPORT.md's numbers are the builder's;
STAGES.md's are the lead's from the export, and only those close a stage.

### 6.4 Real-time checks

RealtimeSanitizer and ThreadSanitizer run through Homebrew LLVM (Apple's clang lacks rtsan and its TSan fails on
this machine), from S1 for the read path and at every stage after for every new audio-thread path. Each run has a
plant (an allocation planted in the stage's new path) and a control (the clean build reports nothing).

### 6.5 The machine is shared

One heavy job at a time on the machine, at most 4 threads, under `nice`, with the load checked first. Timed runs
only in the 2 to 6 am window, with Live and OneDrive closed (ADR-380 A1).

---

## 7. Open questions for the human

Each with the drafter's recommendation. Grouped by what they decide.

### A. Direction

- **A1. Replace or beside?** Recommendation: **build replace's seam (S7) and decide after the filters and
  modulators land**, as the human suggested. Beside is a real port (§1.7: about twenty call sites, three level sets
  per table); replace is a member swap.
- **A2. When `wt.on` becomes the default** (under replace), it is a default change: an ADR and a migration
  (Blind-Spot Armor; RC-9.2). Recommendation: not before the factory patch session.

### B. Morph and modulation

- **B1. Tables Structural (A) or a spectral-blend class (B) for 1.0?** Recommendation: **(B)**. Blending tables
  across the pad is the oscillator's point; it renders one cycle per member, never two sounds, so it keeps
  ADR-377's rule in its letter. S8 renders both so the human can choose by ear.
- **B2. Spectral log's frame is shared.** Under it, a per-note route to the frame reads the newest note. Should
  `device.perNoteToShared = Refuse` (MS-47) mute such routes while the law is Spectral log? Recommendation: **no**;
  the frame's scope stays per-voice, and the page shows the newest-note reading while the law is Spectral log.
- **B3. ADR-384 #9 and MS-29.** X and Y are the morph position (#5), and MS-29 lets only global LFOs and the 2-D
  random move it, so #9's per-note routes to X and Y cannot be made. Recommendation: **keep MS-29**; per-note motion
  lives in the frame and frame spread.

### C. Fidelity, memory, CPU

- **C1. Level spacing** (WT-4). One level per octave (ADR-384 #1) dips the top harmonic to about 9.9 kHz at the
  bottom of each octave at 44.1 kHz; quarter-octave levels keep it at 16.7 to 19.8 kHz but need about 86 MiB per
  instance with Linear's read-time strategy; half-octave levels halve the dip and the pitch crossfade's early fade
  (WT-5) for about 46 MiB. A third lever: let harmonics run past c up to f_int − 20 kHz, where
  their aliases land above 20 kHz. Recommendation: **build the constant, hear S3's three glides, then rule**; if
  one per octave is audibly lacking, half-octave is the one that fits the memory cap.
- **C2. WT-BUDGET** (from B648): the lead proposes, the human approves before S1. The bench's evidence (spec
  WT-P1a): about 3.9 % of a core for one oscillator in the worst cell (H = 1,024, Spectral log, 9 members, the
  morph moving), 1.2 % static, at an uncalibrated conversion; instruction counts miss memory stalls.
- **C6. Spectral log's frame interpolation** (WT-3; found by B648). Lerping the cached log-magnitudes between frames
  is a geometric lerp, not the magnitude of the complex lerp, and fails the corner-identity test by 4.7 × 10³ to
  1.6 × 10⁴ at fractional frames. Recommendation: **define it in the log domain** (the same law on both axes; uses
  the cache; moves formant peaks between frames rather than crossfading them) and write the test to match. The
  alternative, exact, costs a log and a hypot per harmonic per corner per rebuild. S2 plays both.
- **C7. The rebuild crossfade's scheme** (WT-15; found by B648). Under motion a crossfade is always running, so every
  member read touches two sets. Recommendation: **measure a fused read and a once-per-control-interval interpolated
  set at S2**; keep the set if it passes the click gate, else the fused read.
- **C8. Level size cap** (WT-6, WT-6a). Recommendation: **cap at 8,192 samples with the 6-point read** (144 KiB per
  level set, the 140 dB cap at C1, C2 and C4 engine-free); the 8-point read capped at 4,096 is the fallback if the
  timed runs show cache misses on the large levels.
- **C3. The PROPOSED numbers** in the spec's section 9 (level steadiness ±1.5 dB; the static-patch, corner-identity
  and capture tolerances; QS-1's floors applied to the table member). Recommendation: approve as written; S1 and S2
  measure them.
- **C4. The memory cap** (WT-8: 48 MiB for both oscillators' table sets). Recommendation: approve; it allows the
  reference design (about 26.3 MiB) and half-octave levels (about 46.1 MiB), and refuses quarter-octave levels.
- **C5. The rebuild interval:** 4 control intervals (116 samples at 44.1 kHz) for ADR-384 #7's 128 samples, so it
  is defined in time. Recommendation: approve.

### D. The sound's controls

- **D1. The warps:** asym and bend (one smooth warp, two axes), sync with its window; mirror left out (WT-35).
  Recommendation: approve; hear S5.
- **D2. Reuse `crossMod`, `feedback` and `loopDrive`** for the table read (WT-31), rather than new rows.
  Recommendation: approve.
- **D3. No Normalise toggle** (WT-16, ADR-384 #10). Recommendation: approve.

### E. Transformations

- **E1. Audio-rate `wt.sync`, `wt.syncHz`, `wt.pm`** (PA-4, §2.3). Recommendation: approve; PF-2 is re-measured.
- **E2. PM osc 1 → osc 2,** reserved: it needs the two swarms interleaved per sample. Recommendation: after 1.0
  unless S5's growls show it is needed.
- **E3. High quality** has no opt-out for table members (WT-37). Recommendation: approve.

### F. Process

- **F1. The repo name** `horde-wavetable`, local only. Recommendation: approve.
- **F2. The 6 clips per stage** and the questions in §5.6. The human may swap any clip before a stage closes.

### G. Tables

- **G1. The factory set:** the bench's eight formulas at 1,024 harmonics. Recommendation: approve; S1's tour asks
  what is missing.
- **G2. The random generator** joins as a generator with a recipe slot only (WT-46); its integration is a later
  brief.
- **G3. Import of wavetable files made elsewhere** (user WAV tables). Not in this build. Recommendation: after 1.0,
  by ruling; any import stores the file's spectra as a user table with its own recipe kind.

---

## 8. SPEC-ENGINE rows a port would amend

Not edited here; listed so the lead can draft the round when the direction is ruled.

| row | change |
|---|---|
| 5.3 | the eleven `wt.` rows (§2.1) |
| 5.8 | counts (352 rows, 14 reserved, 338 host); the `stream` id sentence (§2.2) |
| PA-4 | `wt.sync`, `wt.syncHz`, `wt.pm` audio-rate (§2.3) |
| PA-5 | `wt.on`, `wt.law`, `wt.syncIn` and a corner's table change |
| PA-8 | the live conditions (§2.4) |
| IF-4 | the engine document's `wt` member (§1.9) |
| IF-9, IF-10 | the counters and observations (§1.9) |
| IF-12, MO-13 / a new D-row | the resolver's table output and the chosen outcome (§4.2) |
| RT-1 | the table sets in prepare's maxima |
| PF-7 | the memory statement (§1.5) |
| the structure file | `instrument.table_set`; the member's blocks under replace (§1.2) |

---

## 9. What made the seam harder than expected

1. **The blades read the base wave in closed form everywhere** (§1.7). Beside is a port of the blade code to a
   table interface, not a switch.
2. **The voice renders each oscillator over a whole segment** (§1.1). Cross-oscillator PM per member works one way
   only without restructuring the swarm's loop.
3. **Every member passes through ED-1's filter E**, which delays its smooth part by about 2.4 samples (ED-2).
   Turning E on and off with a continuous control would shift a sounding member in time; so sync's on/off is a
   stepped row whose change rides PA-5's crossfade, and the warps were made smooth so they never need E.
4. **The resolver sends one value per key.** Four corner tables and four weights is a new kind of output under
   either outcome.
5. **MS-29 and ADR-384 #9 disagree** once X and Y are the morph position (§7 B3).
6. **ADR-384 #1 and #8 pull against each other** at 44.1 kHz (§7 C1), and the bench's 2048-sample linear read
   fails QS-1's floors at low notes (WT-6: 64 dB at C2 against 110).
7. **The cost bench (B648) found three more** in this spec's first draft: Spectral log's cached frame lerp is not
   the law the corner-identity test assumed (WT-3, C6); the rebuild crossfade is always running under motion and
   doubles the member reads (WT-15, C7); and the pitch crossfade toward the darker level fades the top octave early
   (WT-5). Each is now stated, with a recommendation.
8. **The clean-room engine is three rounds behind its spec** (rounds 62 to 64: PA-4's new rows, unison 1 to 9);
   the harness header still says 16 members. S7 pins a commit and re-reads the seam.
