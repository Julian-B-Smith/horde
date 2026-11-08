# The wavetable member: how it wires in, and how the build runs

> **Origin.** Drafted for the horde lead session, 2026-11-07, on ROADMAP B649 and ADR-385 (the human, by poll: a
> new local repo with one builder; the lead owns the gates, the measurements and the listening pages; the
> oscillator first, then wired in; every stage ends with a CPU check, the fidelity gates and a listening batch of at
> most 6 clips). Reworked the same day on an independent critic's review, the human's further rulings (blending
> tables across the pad is in 1.0; MS-29 stands; WT-BUDGET 4 % provisional) and the cost bench's findings (B648).
> Companion to `specs/SPEC-WAVETABLE.md`, which says what the oscillator does. This file says where it plugs in and
> how it gets built without repeating the clean-room build's mistakes. **§7 is the one list of open decisions.**
> Amended 2026-11-08 on B654 and ADR-385 A1 (the human, by poll: "Plan 3 with a PM matrix"): three oscillators in
> the build, a PM matrix, 12 % for three; the real engine's third oscillator is a separate ruling (§10).

**Status.** DRAFT for the lead and the human. The seam was read from the clean-room engine at commit `e72a092`
(spec round 64, 2026-11-07) in `~/Documents/Claude/cleanroom-engine/engine2/`. Line numbers below are at that
commit. That engine still has rounds 62 to 64 queued, so S7 re-reads the seam at its own pinned commit.

---

## 1. The seam

### 1.1 Where a member's sample is made today

All paths are under `engine2/core/` in the clean-room engine.

- **`Swarm::render`** (`swarm/swarm.cpp`) renders one oscillator of one voice for a span of host samples at the
  internal rate. Per member, per internal sample, it:
  1. ramps the member's frequency `m.f` across the control interval (C.0) and sets SW-13's fade `m.level`;
  2. skips a member still waiting to enter (C.7);
  3. evaluates the base wave, **`wave(base_, θ + ½)`** (`blade/waves.h`: the saw is 2θ − 1, rising);
  4. adds the blades' contribution `d` through the direct path (`direct.h`), the wide path (`wide.h`) or
     `blade_sample` (`blades.cpp`), less C.16's estimate, **all inside `if (n_blades_ > 0)`**;
  5. forms **`y0 = (x + d) · m.level`**, runs ED-1's FIR `E` over the member's history `e_hist`, and adds the
     correction owed from its ring `pending`;
  6. multiplies by PA-6's `uni_gain`, the entry gain and `density_`, and adds into left and right by the pan gains;
  7. takes C.14's loop tap from `y0` (only with blades on: lines 483–493);
  8. finds the base wave's own edges before the next sample and adds their corrections through
     `EdgeTables::add_edge`.
- **C.14's xin** is computed once per host sample in the same `n_blades_ > 0` block (lines 419–434) and read only by
  the blade paths (line 456). With no blade, there is no loop.
- **`Voice`** (`voice/voice.cpp`) renders `osc_[0]` then `osc_[1]`, each over a whole segment (at most 512
  samples). A PA-5 change copies the swarm (`xf_[o] = osc_[o]`, line 426) and mixes the copy's render and the
  live one by C.0's raised cosine. After that, each oscillator's signal is scaled by its level, gate, envelope and
  its **on-fade** `osc_fade_[o]` (lines 606–622).
- **`VoiceContext`** (`voice/voice.h`) carries what a voice reads at a tick: parameters, envelopes, both
  oscillators' `OscSettings`, the clock.

### 1.2 What the table member replaces, and what it reuses

In horde's structure file (`engine2/structure/engine-structure.json`) a member is `member.phase → member.base_wave
→ member.blade1 → member.blade2 → member.edges → member.dc → member.pan`, with `member.loop` beside it. Under the
**replace** reading, for an oscillator whose `member` is `table` (WT-19):

| block | what happens | how |
|---|---|---|
| `member.phase` | kept | θᵢ and fᵢ as today (WT-20) |
| `member.base_wave` | **replaced** | step 3 becomes the table read and its transformations |
| `member.blade1`, `member.blade2` | **inert** | step 4 skipped; their rows stay (§2.4) |
| `member.edges` | **only with sync** | steps 5's `E`/`pending` and 8 run only while `wt.syncIn` is not off, for the restart (WT-24, WT-32) |
| `member.dc` | kept, new source | WT-25's means replace the blades' estimate |
| `member.loop` | kept, **amended** | the tap and xin must run for table members though no blade is on (WT-31; C.14, C.16, PA-8 amended, §8) |
| `member.pan` | kept | unchanged |

Under **beside** (§1.7) only `member.base_wave` is replaced.

### 1.3 The member's interface

A new concept, `TableMember`, in its own file (BD-1): a per-member state record and a per-sample call, described
here as an interface (the code is the builder's).

- **State**, added to the swarm's `Member` so PA-5's copy carries it: the sync phase; the member's frame and spread
  positions; its loudness gain under Linear (WT-16); its level and pitch-crossfade weight (WT-5); its DC estimate.
  About 64 bytes.
- **Per sample it receives:** θᵢ and fᵢ/f_int; the published set's read-only view and its crossfade position
  (WT-15), or under Linear's read-time strategy the four corners' level sets and the ramped weights; the ramped
  transformation values; C.14's xinᵢ; under the PM matrix, each column's modulator m (WT-62) from the voice's buffer or, interleaved, from the modulator's previous samples (§1.8); while sync is on,
  the `EdgeTables` and the member's `pending` ring.
- **It returns** the member's output before SW-13's fade, adds the restart's corrections to `pending` while sync is
  on, and counts its reads, edges and level changes.
- **At each control tick** the swarm hands it the frame position (the voice's resolved `wt.frame` plus offsets,
  plus C.10's spread), and it picks its level for the interval's peak read rate (WT-36) and its Linear gain.
- **For beside** it also offers value, slope and antiderivative reads (§1.7), unused under replace.

### 1.4 What stays untouched

The swarm's laws (C.1 to C.8, SW), the voice (VO), the envelopes (EN, C.12), the filter slot (RC-1), the sub and
the noise (SN), the mixer and output stage (OU), the morph resolver's laws apart from §4.2's class, and the
modulation system apart from §2's destinations. With every oscillator's `member` at `base`, the engine renders
bit-identically to the engine without the table code (WT-P4, tested as RC-6.4 tests a fifth source).

### 1.5 The table set: a new block, per oscillator, per instance

A new concept, `TableSet`, owned by the engine (`HordeEngine`), two of them. Voices read it through a const view
in `VoiceContext`. Proposed structure id `instrument.table_set`, multiplicity 2 (the lead adds it).

| call | thread, when | what |
|---|---|---|
| `prepare(host rate, quality, control interval, maxima)` | main, prepare | allocates everything for the maxima (RT-1): eight table slots, the level sets, the rebuild scratch, the FFT plan |
| `reset()` | audio | WT-17: tables kept, the published set rebuilt at once, no crossfade in flight |
| `stage(slot, recipe)` | main (inside `set_table` or a load) | builds the table's spectra and level sets into a free slot, completely, before returning (WT-18) |
| `swap(corner, slot)` | audio, at a `Tables` event's sample | makes the staged slot live, retires the old one (§1.6) |
| `set_weights(w[4], law, frame)` | audio, each control boundary | the morph's weights (§4.2), the law, Spectral log's instance frame (WT-22) |
| `boundary()` | audio, every R = 4 control intervals | rebuilds if an input changed (WT-14), starts a crossfade (WT-15), counts |
| `view()` | audio, per voice per segment | the read-only published set(s), crossfade position, gains |

**Memory.** The reference design: the 6-point read capped at 8,192 samples (WT-6), one level per octave, so a
level set is 36,864 samples, **144 KiB**. Each oscillator has eight table slots (four live, four staging, so a load
can replace all four corners at once). Per oscillator:

| part | Linear strategy (a), read-time | strategy (b), pre-blend by inverse FFT |
|---|---|---|
| spectra and log cache, 8 slots × (128 + 64) KiB | 1.5 MiB | 1.5 MiB |
| Spectral log published sets, 3 | 0.42 MiB | 0.42 MiB |
| corner level sets, 8 slots × 16 frames | 18 MiB | — |
| Linear published sets, 3 × 16 frames | — | 6.75 MiB |
| **per oscillator / per instance** | **19.9 / 39.8 MiB** | **8.7 / 17.3 MiB** |

By level spacing (same read and cap), per instance:

| spacing ρ | level set | strategy (a) | strategy (b) |
|---|---|---|---|
| 2 (built) | 144 KiB | 39.8 MiB | 17.3 MiB |
| 2^½ | 266 KiB | 71.1 MiB | 29.5 MiB |
| 2^¼ | 511 KiB | 133.8 MiB | 53.9 MiB |
| 2^⅛ (what QS-2 would need, WT-F2) | 1,001 KiB | about 262 MiB | about 103 MiB |

Under WT-8's proposed 48 MiB cap: strategy (a) fits only at one level per octave; strategy (b) fits at one and
half per octave, but costs 16 inverse FFTs per rebuild while the weights move (B648: inverse FFTs were already 38 %
of the worst cell with one frame). Strategy (b) built from corner level sets instead of FFTs needs both columns.
For comparison, the clean-room manifest states 43.2 MB for the whole engine today (`manifest_memory_bytes`).

**Three oscillators (ADR-385 A1; WT-60).** Three table sets, per instance, same read and cap:

| spacing ρ | strategy (a) | strategy (b) |
|---|---|---|
| 2 (built) | 59.7 MiB | 26.0 MiB |
| 2^½ | 106.6 MiB | 44.3 MiB |
| 2^¼ | 200.7 MiB | 80.9 MiB |
| 2^⅛ | about 393 MiB | about 155 MiB |

Add the PM scratch (144 KiB at High) and interleaving's per-oscillator work buffers (48 KiB) (WT-66). WT-8's cap,
24 MiB per oscillator (72 MiB for three), allows strategy (a) only at one level per octave and strategy (b) at one
and half per octave.

**Real time.** Nothing on the audio thread allocates, locks, makes a system call or waits (RT-1); a rebuild's
work is bounded by the level sizes (RT-2). The inverse FFT is the builder's own code or a dependency the human
approves (charter).

### 1.6 Tables in: slots, ownership and the handoff

Built at S6; S1 to S5b assign tables only at load, synchronously (WT-18).

- **Slots.** Per oscillator, eight table slots, each holding a table's spectra, its log cache and (strategy (a))
  its 16 frames' level sets. Each slot is in one state: **Free**, **Staged**, **Live** or **Retired**.
- **Ownership.** The main thread owns Free slots: it builds into one (`stage`), and when the slot is complete it
  posts a `Tables` event naming the corner and the slot, timed at the first sample of the next processing call and
  recorded in the event stream (WT-18). From then the slot is Staged, and **the main thread never writes it
  again**. At the event's sample the audio thread makes it Live and the corner's previous slot Retired, pushing
  that slot's index onto a single-producer single-consumer return ring. **The audio thread never writes a slot's
  contents.** The main thread drains the ring before it picks a Free slot.
- **Exhaustion.** If no slot is Free (four swaps posted and not yet applied), `set_table` waits on the main thread
  until the audio thread's next processing call returns one. While the shell is not processing, it may apply the
  posted swaps itself through a non-real-time call, since no audio thread runs.
- **Synchronisation.** A slot's contents are published by a release store of its state and read after an acquire
  load. WT-F13's ThreadSanitizer plant writes a Staged slot after posting it, and must be reported.

### 1.7 The beside reading: what the blades would need

If the human rules that the blades cut a table member, the blades must read the table where they read the base
wave in closed form: about twenty call sites in `swarm/blades.cpp`, `swarm/direct.h`, `swarm/wide.h` and
`blade/dc.cpp`, using:

| today | what a table must supply | exact from the spectra? |
|---|---|---|
| `wave(base, x)` | the value | yes |
| `wave_slope`, `deriv` | the slope at a window edge (ED-6, ED-7) | yes: harmonic n × 2πn·i, a second level set |
| `jumps_of`, `edges_of` | the wave's own edges | none: a table has none |
| `mean_over(base, a, b)` | the mean over a span (Crush, C.16) | yes: the antiderivative, a third level set |
| `dc_estimate(stack, base, …)` | the blades' cycle mean | yes, from the antiderivative |

Beside costs about three times the level-set memory and an interface in place of the `Wave` enum at every site.
This build does not do it unless the human rules beside before S7.

### 1.8 What the voice must change

- **The PM matrix's render order** (WT-61 to WT-66; ADR-385 A1). The voice renders its oscillators in the fixed
  order osc K, …, osc 1 (today it renders `osc_[0]` then `osc_[1]`, voice.cpp). Every modulator's taps carry its
  governing envelope and **its on-fade** (`osc_fade_[j]`) and, during its PA-5 crossfade, the copy's and live
  swarm's outputs mixed (WT-62); the order changes no other output, since the sources sum.
- **Segment mode** (no back-edge live): each oscillator renders its whole segment in that order; forward
  modulators' member outputs wait in a shared scratch buffer, at most (K − 1) × 9 members × the segment at the
  internal rate: **144 KiB at High for K = 3** (72 KiB at Standard).
- **Interleaved mode** (a back-edge live, WT-64): the K swarms advance one internal sample at a time. That needs
  `Swarm::render` split into a segment set-up, a one-sample step for all members, and a segment end (High's
  decimation, so each oscillator keeps its own work buffers, K × 2 × 1,024 doubles, 48 KiB). A back-edge reads its
  modulator's last two outputs (WT-63); a forward edge reads the same sample. **Which edges carry the delay:** only
  back-edges (j < i: 1 → 2, 1 → 3, 2 → 3) and the diagonal (C.14). Cost: PROPOSED +10 to 25 % on the oscillators'
  instructions while a back-edge is live (WT-64), measured at S5b.
- **The mode is chosen per voice per segment** and changes nothing audible: a patch with every back-edge at 0 is
  bit-identical in both (WT-F14). The alternative, always interleaving, was rejected: it would charge every patch
  the interleave's cost for a feature most patches do not use.
- **Spectral log's frame.** The engine finds the newest held voice (VO-10's note counter, MS-46's rule) at each
  boundary and hands its frame to the table set.
- **PA-5.** `wt.syncIn` and WT-19's `member` join the stepped keys (`stepped_equal`, `with_stepped`). **The law, a
  corner's table and a Quantum flip of the tables cannot use PA-5**: the table set is per instance, so `xf_[o]` and
  the live swarm (line 426) read the same set; they change by WT-15's rebuild crossfade.

### 1.9 The engine's API, observation and counters

- **Tables in.** `set_table(osc, corner, recipe)`, beside `set_ratios`: main thread, builds and stages (§1.6),
  then posts the `Tables` event. The resolver's weights arrive as a sample-timed event per oscillator (§4.2).
- **State.** The engine's IF-4 document gains a top-level member `wt`: per oscillator `member` (`base` or `table`)
  and the four corner recipe ids it last received, as it keeps the `ratios` text. Added without a version change
  (RC-9.2 to RC-9.4). The recipes live in the patch's table dictionary (§3).
- **Observation (IF-10).** Per oscillator: weights, law, the published set's levels, crossfade position, rebuild
  count. Per member: frame position, level, pitch-crossfade weight, output before the fade. No product branch is
  added for them (BD-4).
- **Counters (IF-9).** Rebuilds, levels built, table reads, sync edges, peak-limit and gain-cap engagements,
  tables refused, swaps applied.

---

## 2. Parameters

### 2.1 The new rows

Each oscillator gains eleven rows (SPEC-ENGINE 5.1's format), keys `oscN.wt.…`, at the next free offsets of its
block: osc 1's ids 0x1030 to 0x103A, osc 2's 0x2030 to 0x203A; and seven PROPOSED rows held at 0x03B to 0x041 for
the internal modulator, pending the lab (below). Amended 2026-11-08 (ADR-385 A1): the single PM row became the
matrix rows 0x039 and 0x03A. **These ids are proposals in a draft, not ratified:** nothing ratified is
renumbered.

| offset | key | label | unit | range | default | law | class | rate | PV | what it does |
|---|---|---|---|---|---|---|---|---|---|---|
| 0x030 | `wt.law` | Blend | — | Linear, Spectral log | Linear | step | S | T | — | How the corner tables combine (WT-11, WT-12). |
| 0x031 | `wt.frame` | Frame | — (shown 1–16) | 0..1 | 0 | lin | M | C | yes | Where in each table the members read (WT-22). |
| 0x032 | `wt.frameSpread` | Frame spread | ± | −1..1 | 0 | lin | M | C | yes | Fans the frame across the members (WT-22). |
| 0x033 | `wt.syncIn` | Sync | — | off, × pitch, Hz | off | step | S | T | — | Turns sync on, following the note or fixed (WT-32). |
| 0x034 | `wt.sync` | Sync ratio | × | 1..16 | 2 | log | M | A | yes | Table cycles per member cycle (WT-32). |
| 0x035 | `wt.syncHz` | Sync | Hz | 20..12 000 | 880 | log | M | A | yes | The fixed sync frequency: a formant (WT-32, WT-33). |
| 0x036 | `wt.window` | Window | % | 0..1 | 0 | lin | M | C | yes | Hard sync (0) to windowed bursts (1) (WT-33). |
| 0x037 | `wt.asym` | Asym | ± | −1..1 | 0 | lin | M | C | yes | Moves the half-cycle point (WT-34). |
| 0x038 | `wt.bend` | Bend | ± | −1..1 | 0 | lin | M | C | yes | Bends time toward the cycle's start or away (WT-34). |
| 0x039 | `wt.pmA` (see below) | PM from the first other oscillator | index | 0..10 | 0 | pow | M | A | yes | The matrix entry from the lowest-numbered other oscillator (WT-61). |
| 0x03A | `wt.pmB` (see below) | PM from the second other oscillator | index | 0..10 | 0 | pow | M | A | yes | The matrix entry from the other one (WT-61). |

**The matrix rows' keys are absolute** (the layout rule is "the other oscillators in ascending order"; the
diagonal is each oscillator's existing `feedback`, WT-61):

| block | 0x039 | 0x03A | diagonal |
|---|---|---|---|
| osc 1 | `osc1.wt.pm2` (the draft's `osc1.wt.pm`: same id, same meaning, key renamed) | `osc1.wt.pm3` | `osc1.feedback` |
| osc 2 | `osc2.wt.pm1` (the draft's reserved `osc2.wt.pm`: same id, same meaning, **now live**, by interleaving) | `osc2.wt.pm3` | `osc2.feedback` |
| osc 3 (the build only) | `osc3.wt.pm1` | `osc3.wt.pm2` | `osc3.feedback` |

**Reservations, honestly kept.** In the real engine (two oscillators, WT-69) `osc1.wt.pm3` and `osc2.wt.pm3` name
an oscillator that does not exist: **reserved** (5.1) until the third-oscillator round (§10) makes them live.
Osc 3's rows exist only in the build's own manifest, at build-local ids that are not engine ids (§10).

**The internal modulator: PROPOSED, pending the lab** (B652; WT-70). Ids held, not finalised: the lab may change
the set, and a row it adds takes 0x042 onward.

| offset | key | label | range | default | law | class | rate | PV |
|---|---|---|---|---|---|---|---|---|
| 0x03B | `wt.modShape` | Mod shape | sine, triangle, saw, reverse saw, square, smooth noise, S&H noise | sine | step | S | T | — |
| 0x03C | `wt.modIn` | Mod rate in | × pitch, Hz | × pitch | step | S | T | — |
| 0x03D | `wt.modRate` | Mod rate | × 0.25..24 | 1 | log | M | C | yes |
| 0x03E | `wt.modRateHz` | Mod rate | 1..20 000 Hz | 5 | log | M | C | yes |
| 0x03F | `wt.modType` | Mod type | phase, pitch | phase | step | S | T | — |
| 0x040 | `wt.modDepth` | Mod depth | index 0..10 | 0 | pow | M | A | yes |
| 0x041 | `wt.modDecorrelate` | Decorrelate | 0..1 | 0 | lin | M | C | yes |

**Not host parameters:** WT-19's `member` (engine state, never shown as a choice) and each corner's
`oscN.wt.table` (morph corner state, WT-51).

**Counts, in the real engine** (two oscillators): 22 rows, 2 reserved, so the manifest grows from 330 to 352
rows, reserved rows from 13 to 15, host parameters from 317 to 337. With the internal modulator's proposed rows,
36 rows: 366, 15 reserved, 351 host parameters.

### 2.2 The ids: verification and placement

- **Free.** The clean-room manifest at `e72a092` was compiled on its own (`core/manifest/manifest.cpp` and
  `memory.cpp` against the harness header) into a scratch program that printed every row: **330 rows**, as 5.8
  says. Per oscillator block the used offsets are 0x000 to 0x02F (48 rows), 0x100 to 0x11D (blade 1), 0x200 to
  0x21F (blade 2) and 0x300 (`stream`). 0x030 to 0x0FF is free in both. The other blocks are the voice (0x0001 to
  0x0012), the sub (0x3000), the noise (0x4000), ENV 2 to 4 (0x5000 to 0x5209), and two test-build blocks (0x6000,
  0x7000). Horde's tree has no use of these ids (searched `specs`, `docs`, `engine2`, `tools`). **Re-checked
  2026-11-08 for 0x03A to 0x041:** no row of the dump falls in 0x1030 to 0x104F or 0x2030 to 0x204F, and the tree's
  only use of 0x1030 to 0x1041 and 0x2030 to 0x2041 is this file. The morph's and the
  modulators' host parameters are appended to the lockfile after the engine's and never reach it (IF-11, MS-40).
- **Placement.** `manifest/manifest.h` (line 10) and 5.8 say a new key takes **the next free offset of its block**,
  so 0x030 onward, not a high range; `stream` stays its block's last id (0x300) and 5.8's wording holds.
- **A consequence to know:** `manifest_param(i)` is in ascending id order, so inserting rows at 0x1030 shifts the
  manifest **index** of every later row (by 11 after osc 1's block, by 22 after osc 2's; 18 and 36 with the
  internal modulator). Ids, states (keyed by
  key) and renders are unaffected; a test that seeds or iterates parameters by index sees a different order and
  must seed by key, or have its expected values regenerated. Any placement inside the product blocks shifts the
  indices of the blocks after it; only a block above every product row would not, which is what the test builds
  did to avoid it.

### 2.3 SPEC-ENGINE rows these touch

- **PA-4** gains `wt.sync`, `wt.syncHz`, the matrix rows (`wt.pm…`) and, if the lab keeps it, `wt.modDepth` as audio-rate destinations (an envelope sweeps them sample by
  sample on the engine path, MS-41); PF-2 is then re-measured, as round 62 did (ADR-379 A1).
- **PA-5** gains `wt.syncIn` and `member`. **It does not gain the law or the tables:** those change by WT-15's
  rebuild crossfade (§1.8), and WT-F4 judges them with differing corners.
- **PA-8** gains the live conditions below, and **its rows for `crossMod`, `feedback` and `loopDrive`** change
  (WT-31).
- **C.14** and **C.16** change for the loop on table members (WT-31).

### 2.4 Live and inert (PA-8)

**The new rows** are live only while the oscillator's `member` is `table`. `wt.frameSpread` needs Linear and
`unison` > 1; `wt.sync` needs `wt.syncIn` at × pitch, `wt.syncHz` at Hz, `wt.window` not off; a matrix row needs
its modulator oscillator on (a reserved one is never live); the internal modulator's rows need `wt.modDepth` above 0
(`wt.modRate` at × pitch, `wt.modRateHz` at Hz).

**The existing rows for a table member:**

| rows | replace | beside |
|---|---|---|
| `baseWave` | inert | inert |
| every blade row, `stacking`, `upperHears`, `collisionPitch`, `collisionBite`, `quantize`, the `ratios` text | **inert** | live |
| `crossMod`, `feedback`, `loopDrive` | live on the read phase (WT-31; amends PA-8's "a blade is on in Sync, FM or Ring") | live |
| `spreadLaw`, `spreadDrift`, `motionGrain` | live through `wt.frameSpread` | live |
| `bandLimit` | live while sync is on | live |
| every swarm, pitch, pan and envelope row | live | live |

No row is deleted (RC-9.1).

---

## 3. State

### 3.1 The recipe, format version 1

A recipe is a JSON object. **Its id** is the 64-bit FNV-1a hash of its canonical text (offset basis
0xcbf29ce484222325, prime 0x100000001b3, arithmetic modulo 2⁶⁴, over the UTF-8 bytes), written as 16 lowercase hex
digits. **The canonical text:** `name` removed; keys sorted by their UTF-8 bytes; no whitespace; strings with JSON's
minimal escapes; integers in plain decimal; other numbers in the shortest decimal that reads back to the same IEEE
754 double (ECMAScript's number-to-string form). Two recipes with the same content have the same id everywhere.

```
generator: {"v":1, "kind":"gen", "gen":"<generator id>", "genV":<int>, "set":{…}, "seed":<uint32>, "name":"…"}
capture:   {"v":1, "kind":"cap", "src":["<id A>","<id B>","<id C>","<id D>"], "x":<0..1>, "y":<0..1>,
            "frame":"all" | <0..1>, "law":"linear" | "spectral-log", "lawV":<int>, "name":"…"}
```

- Factory ids: `factory/sine-saw`, `factory/pulse`, `factory/vowel`, `factory/sync`, `factory/fm`, `factory/fold`,
  `factory/comb`, `factory/organ`; the random generator's is `random`.
- `lawV` pins the blend law's version (WT-42, WT-43); every law version stays buildable.
- A capture's sources are ids resolved in the patch's dictionary, so captures form a graph and a shared source is
  stored once. No wall-clock value is in a recipe or its id (DT-5).

### 3.2 What goes where, and the bounds

| place | holds | bound |
|---|---|---|
| **the host's state chunk** | a `tables` dictionary {id: recipe} with every table a corner of either oscillator references and every capture's sources, transitively; corners hold ids | **64 recipes, 512 bytes each, so 32 KiB**; capture depth 8. **Enforced when a table is assigned or captured** (WT-44), never at save. |
| **an exported preset file** | the dictionary plus the rendered spectra of every played table that is not a factory generator, float32 in base64 | about 171 KiB per table, at most 8 corner tables: ≤ 1.4 MiB per file |
| **the user library** (device-wide, WT-45) | one file per table: recipe and spectra | — |

The host chunk never carries table data (ADR-384 #2; the lesson that a host silently keeps an old chunk when a
large one fails to save).

### 3.3 Loading

1. The main thread rebuilds every recipe before the load returns (WT-18).
2. An unknown generator or law version: the embedded spectra, else the library copy by id, else the corner's
   default factory table. **Every fallback is reported by name** in the load report (PA-7's rule).
3. A capture whose sources cannot all be rebuilt: the same order, reported.

---

## 4. Modulation and morph

### 4.1 Destinations

| destination | scope (MS-12) | path (MS-41) | notes |
|---|---|---|---|
| `wt.frame` | per-voice | outside | under Spectral log the newest held voice's (WT-22); the page warns as MS-46 has it; muted under MS-47's Refuse while Spectral log (WT-56) |
| `wt.frameSpread`, `wt.window`, `wt.asym`, `wt.bend` | per-voice | outside | |
| `wt.sync`, `wt.syncHz`, the matrix rows, `wt.modDepth` | per-voice | engine path from ENV 1 to 4, velocity and per-note LFOs once PA-4 lists them; outside otherwise | |
| `morph.x`, `morph.y` | shared | outside | global LFOs and the 2-D random only: **MS-29, ruled by ADR-385**; ADR-384 #9 is moot |
| `wt.law`, `wt.syncIn`, `member`, `oscN.wt.table` | — | — | not destinations (stepped or state) |

### 4.2 The morph's side

**The design (ADR-377 A3 / ADR-385):** `oscN.wt.table` is a key of a new **spectral-blend class** (WT-53). Every
other Structural key keeps following its quantum picks under ADR-377.

| | the table key |
|---|---|
| Blend, Stepped | MO-4's unglided weights (the stepped position's under Stepped), identical tables pooled, Count Two by MO-9, glided by MO-5 (b), restricted to on corners (MO-26 (2)) |
| Quantum | one-hot at the pick and latch (MO-7, MO-8) |
| a change of pick, a law or table change | WT-15's rebuild crossfade (R) |
| Exempt | corner A |
| morph off | `morph.audible`, one-hot |
| Save current sound (MO-25) | a capture recipe of the heard blend where the corners differ (WT-55) |

**The resolver's API (IF-12).** Today a corner arrives as `set_corner(corner, v, ratios_osc1, ratios_osc2, at)`
(`morph/resolver.h` line 22, and the harness header), which carries the two `ratios` texts. The tables arrive by a
**new call** `set_corner_tables(corner, osc, table id, at)`, so the existing signature is unchanged
(append-only). The resolver gains one output kind, `Tables`: per oscillator, the four corner ids and four weights,
sent at control boundaries when they change (MO-20's timing). The engine routes it to the table set (§1.5).

**ADR-254 (the FX device).** None while it hosts no oscillator; the user library is device-wide (ADR-384).

---

## 5. The build process

### 5.1 The repo and the roles

- **Repo:** `~/Documents/Claude/horde-wavetable`, a new local git repository, **never pushed** (ADR-385).
- **One builder session**, under every clean-room builder rule except blindness.
- **The horde lead owns** the gates (written and proven, plants and controls, before each stage opens), the CPU
  and fidelity measurements, the listening pages, every ruling put to the human, and the conversion factor.
- **The human** listens at each stage's end and rules §7.

### 5.2 What the builder reads

- `specs/SPEC-WAVETABLE.md` and this file, copied into the repo at the stage's start with the horde commit they
  came from.
- The QUADRANT bench as a listening reference and source of test cases, not a parity target; its DFT loops and its
  2048-sample linear read are not ported (WT-6, WT-41).
- The SPEC-ENGINE sections the spec cites, copied.
- From S7: the clean-room engine at a pinned commit (§5.8).

### 5.3 The layout

```
horde-wavetable/
  BRIEF.md       the lead's brief for the current stage: scope, gates, CPU share, clips
  QUESTIONS.md   the builder's questions W-1, W-2, …  (never guess: ask)
  ANSWERS.md     the lead's answers, from the spec and the human's rulings only
  REPORT.md      the builder's report per stage: commit, what was built, gate output verbatim, instruction counts
  STAGES.md      the stage table, each stage's state, the lead's verified numbers, the human's notes as rows
  specs/         SPEC-WAVETABLE.md, CONTRACT.md, the SPEC-ENGINE extract, each with its source commit
  core/          table/  blend/  level/  member/  store/  api/
  harness/       the swarm stand-in, one envelope, the offline render, the WAV writer
  tests/         per stage: its gates, each with its plants and its control
  tools/         the alias meter (QS-1's method), the click detector (QS-4's, copied unchanged), the instruction
                 counter, the clip renderer
  goldens/       generator and law goldens, per version (WT-43)
  listening/S1/ … the clips and the notes page
  vendor/engine2/ (S7 on) the clean-room engine at the pinned commit: a read-only reference
  seam/          (S7 on) a git clone of the clean-room engine at the same commit, on a branch: where the wiring is done
```

**The harness** gives sound from S1: a swarm stand-in (U members placed by C.2's JP-8000 curve at a fixed detune,
seeded start phases, no coupling or drift), one envelope by C.12, an offline render to WAV at 44.1 and 48 kHz, using
the clean-room phase convention (WT-20).

### 5.4 The CPU discipline

The clean-room build measured CPU only at the end and read 7 to 10 times its target (ADR-354; Glass horde pad
61.89 %, REPORT.md §7). Here:

- **WT-BUDGET = 4 % of one core per oscillator, PROVISIONAL** (ADR-385), on WT-P2's judged cells: PF-3's chord,
  9 members, the morph moving. **12 % for three oscillators** with the PM matrix (ADR-385 A1), judged on cells 4
  and 5 (K = 3, the matrix active; cell 5 interleaved).
- **Every stage reports two numbers beside its test count:** the instruction count of its cells, at once, and its
  share of WT-BUDGET.
- **Every stage's own night.** The judged cells built so far run timed (PF-3's method, load guard, best of 5) in
  the 2 to 6 am window of the night after the stage's gates go green (ADR-380 A1). **A stage closes only after that
  run is within its share.** That night's run also **re-fixes the conversion factor** from instructions to time, for
  the next stage's counts.
- **The shares** (cumulative, PROPOSED; the lead fixes them):

| stage | judged cells, as built so far | share of WT-BUDGET |
|---|---|---|
| S1 | the read alone, scaled to cell 2's member-samples (8 reads per member-sample under strategy (a)) | ≤ 35 % |
| S2 | cell 1's rebuilds and crossfades, one member per voice | ≤ 55 % |
| S3 | cells 1 and 2 with the pitch crossfade | ≤ 65 % |
| S4 | cells 1 and 2 at U = 9, the morph moving: the first full measurement | ≤ 85 % |
| S4 | also K = 3 without the matrix, reported | reported |
| S5a | cell 3 (one oscillator) | ≤ 100 % of 4 % |
| S5b | cells 4 and 5 (K = 3, the matrix; segment and interleaved) | ≤ 100 % of 12 % |
| S6 | cells 1 to 5: no audio-path change | within 2 % of S5b |
| S7, S8 | the cells in the clean-room engine at K = 2 (8 %); the swarm's own cost reported apart (PF-1 holds the whole oscillator) | ≤ 100 % |

- **When a timed run exceeds its share** (likely: B648 measured a cheaper design at 3.9 %, and the critic's rough
  estimate for this one is near 7 %, spec WT-P7): (1) the stage does not close; (2) the builder applies cheaper
  schemes that keep every render within −120 dBFS of the previous build (ADR-380 A1's tier 1): WT-15 (ii) or
  (iii), the other Linear strategy, the read's loop order, SIMD; (3) a trade that changes the sound more than that
  (a coarser spacing, a shorter read, fewer frames, a cheaper law) goes to the human with a listening A/B;
  (4) **WT-BUDGET moves only by the human's ruling**, with the timed evidence.
- **Instruction counts miss memory stalls** (B648), so a count that passes can still fail its night.
- **Every CPU check has a plant and a control:** a build running the read loop twice reads about twice the read's
  count; a render with no notes reads the idle floor.

### 5.5 The listening discipline

- At each stage's end the lead renders **at most 6 clips** from a clean export of the stage's commit (§6.3) on one
  page with a notes field per clip and the questions below.
- The human's notes become **rows in STAGES.md before the next stage starts**: built into the next stage, ruled
  out with a reason the human accepts, or deferred to a named stage. A stage does not close with a note unanswered.
- Clips are `sN-k-<what>.wav`, 44.1 kHz, 24-bit, peak-normalised to −1 dBFS with the gain on the page.

### 5.6 The stages

A stage starts only when its gates are written and proven (detector plants fire, controls read zero) and its brief
is in BRIEF.md. **A stage closes only by §6.1.**

| stage | scope | the human hears | gates (spec section 9) | CPU |
|---|---|---|---|---|
| **S0** (lead) | repo, harness skeleton, alias meter, click detector, instruction counter; WT-BUDGET's first timed confirmation; the conversion factor | — | every S1 gate's detector plants and controls proven engine-free | conversion fixed |
| **S1** | the data model, the factory generators, levels, the read; one member, a static table | factory tables | WT-F1, WT-F5, WT-F8 (static), WT-F9, WT-F13 (rtsan); WT-41, WT-43, WT-47 | ≤ 35 % |
| **S2** | the four-corner blend, both laws and WT-3's frame rule, rebuild and crossfade (scheme (ii), with (iii) measured), WT-16's loudness | morph sweeps | WT-F4 (the moving-phase probe, differing corners, law switch, Quantum flip), WT-F6, WT-F7, WT-F10, WT-F5 at every block split | ≤ 55 % |
| **S3** | levels across pitch, the pitch crossfade, WT-36's level choice; ρ a constant | glides at three spacings | WT-F1 at C1 to C8 at 44.1, 48, 96 kHz; WT-F2 (the design figure); WT-F4 over glides | ≤ 65 % |
| **S4** | U members of the stand-in, per-voice frame under Linear (strategy (a); (b) measured), frame spread, Spectral log's newest-voice frame, 8 voices | unison and scanned plucks | WT-F4, WT-F5, WT-F7 at U = 9; WT-P4; memory against WT-8 | ≤ 85 % |
| **S5a** | the internal modulator (if the lab has ruled, else it moves to the first unstarted stage after the ruling), sync and window, asym and bend, the loop on table members, DC removal, High; one oscillator | sync leads, formants, jitter | WT-F3, WT-F8 with every transformation, WT-F12, WT-F4 with each amount swept; sync's edges by QS-1's method at r = 2.5; the loop's stability map (WT-31) | ≤ 100 % of 4 % |
| **S5b** | K = 3; the PM matrix, every modulator's taps and fades (WT-62), the two render modes (WT-64), the back-edge clamp (WT-65), the PM buffers | operator stacks: EP, bell, growl, feedback | WT-F14; WT-F4 with depths swept and a modulator switched off; WT-F12 with every entry fuzzed; WT-F3 for the matrix; the back-edge stability map (WT-65) | ≤ 100 % of 12 % |
| **S6** | recipes, the dictionary and its bounds, capture, the library, the slot handoff (§1.6), state, the random slot | captures against live blends | WT-F11, WT-F5's load and mid-render cases, WT-F13 (TSan plant); recipe round trip byte-identical; bounds refused at assignment; fallbacks reported | within 2 % |
| **S7** | the seam in `seam/` (§5.8): the table member in `Swarm::render`, the table set in `HordeEngine`, the voice's render order, both render modes at K = 2 (osc 1 → osc 2 a back-edge) and PM buffers, the loop amendment | the table member in the real swarm | WT-P4 (every `member` at `base`: all 83 bank presets bit-identical to the pinned commit); the clean-room suite green, by-index tests reseeded by key (§2.2); WT-F1, WT-F4, WT-F5 in the engine | cells in the engine |
| **S8** | the spectral-blend class in the resolver, `set_corner_tables`, the modulation destinations, the engine's `wt` state, the test plug-in | morphing tables, and the human playing it | the morph rows the class touches; MS-29 refusals; RC-9 round trips; rtsan and TSan on the plug-in render | cells, timed |

**The listening batches** (≤ 6 clips each):

| stage | clip | what it demonstrates | asked |
|---|---|---|---|
| S1 | `s1-1-sine-saw-scan` | Sine → saw through its 16 frames at C3 | Does the scan sound like the bench? |
| | `s1-2-factory-tour` | all eight factory tables, 2 s each, C3 | Which earn their place; what is missing? |
| | `s1-3-vowel-register` | Vowel at C2, C3, C4, C5 | Does the vowel hold across registers? |
| | `s1-4-bass-read` | the bench's read against the build's, saw, C1 to C2 | Can you hear the difference? |
| | `s1-5-sync-table-low` | Hard sync 1 → 6 at C1 | Is the low end clean? |
| S2 | `s2-1-linear-diagonal` | A → D over 8 s, Linear | Is the path musical? |
| | `s2-2-log-diagonal` | the same, Spectral log | Keep both laws? |
| | `s2-3-opposed-centre` | saw and reverse-saw corners through the centre | Does it fade gracefully where it cancels? |
| | `s2-4-orbit` | a 0.2 Hz orbit, Linear then Spectral log | Any grain from the rebuilds? |
| | `s2-5-log-phase` | Spectral log, the linear-sum phase, then a fixed phase | Which is steadier? |
| | `s2-6-log-frame-scan` | Vowel scanned under Spectral log, log-domain frames, then exact (WT-3) | Which in-between frames sound right? (C6) |
| S3 | `s3-1-glide-octave` | saw glide C1 → C8 over 12 s, ρ = 2 | Do you hear brightness rise and fall per octave? |
| | `s3-2-glide-half` | the same, ρ = 2^½ | Better enough for the memory? |
| | `s3-3-glide-quarter` | the same, ρ = 2^¼ | Better enough for the memory? |
| | `s3-4-bass-line` | a C1–C2 line on Hard sync | Low-end detail and weight? |
| | `s3-5-top-lead` | a C5–C7 line on Vowel | Any harshness at the top? |
| | `s3-6-vibrato-boundary` | ±50 cent vibrato across a level boundary at C6 | Any brightness wobble? |
| S4 | `s4-1-unison-pad` | PF-3's chord, U = 9, Linear | Does it sound like horde's swarm? |
| | `s4-2-scanned-plucks` | ENV 2 sweeping the frame per note | Worth its cost? |
| | `s4-3-frame-spread` | frame spread 0 → 1 on a held chord | New texture or mud? |
| | `s4-4-log-newest-note` | Spectral log, frame by velocity, a legato line | Is the shared frame acceptable? |
| | `s4-5-unison-1-9` | U = 1 then U = 9 | Density and balance? |
| | `s4-6-full-shape` | XY under an LFO plus per-voice scanning | Does it feel like discovery? |
| S5a | `s5a-1-sync-lead` | sync swept by an envelope, × pitch | Does it replace a Sync blade lead? |
| | `s5a-2-formant` | sync in Hz, window 1, swept 300 → 3,000 Hz, a chord | Does it replace a formant blade? |
| | `s5a-3-jitter` | the internal modulator's noise shapes, decorrelated across 9 members | Does it carry SCALPEL's jitter? |
| | `s5a-4-swarm-fm` | feedback and cross-mod on a 9-member table swarm | Usable, or chaos? |
| | `s5a-5-warps` | asym and bend on Pulse and Sine → saw | Distinct enough to keep both? |
| | `s5a-6-quality` | a high-index internal-modulator lead, Standard then High | Is High needed? |
| S5b | `s5b-1-ep` | an electric piano: osc 3 sine at ratio 14 (coarse +45.7 st) → osc 1 for the tine, osc 2 sine at ratio 1 → osc 1 for the body, each index on its own decaying envelope | Is it a convincing EP? |
| | `s5b-2-bell` | a bell: 3 → 2 → 1, ratios 3.5 and 1, a long decay | Bell-like, or metallic noise? |
| | `s5b-3-growl` | a growl: 3 → 2 → 1 with osc 1's feedback at 0.4, Vowel on osc 1, a bass line | Does it beat the blade growls? |
| | `s5b-4-feedback` | one sine operator's feedback 0 → 1, then the back-edge loop 1 → 3 → 2 → 1 | Where does it turn to noise, and is the clamp in the right place? |
| | `s5b-5-swarm-stack` | the EP of clip 1 on 9-member swarms, detuned | Is a swarm of operators new ground? |
| | `s5b-6-morph-matrix` | matrix depths differing per corner, a morph sweep | Does morphing the algorithm feel fluid? |
| S6 | `s6-1-capture-vs-live` | a blend, then its capture in all four corners | Identical? |
| | `s6-2-blend-of-captures` | a sweep across four captures | New ground? |
| | `s6-3-portable-preset` | a preset reloaded after its library table is deleted | Identical? |
| S7 | `s7-1-coupling` | coupling −1 → +1 on a table swarm | Does the swarm's physics still read? |
| | `s7-2-drift-inertia` | drift and inertia on a table pad | Same character? |
| | `s7-3-gravity` | gravity on a held table chord | Still settles to just intervals? |
| | `s7-4-in-engine-sweep` | an S2 sweep in the engine | Same as S2's? |
| S8 | `s8-1-blend-four-tables` | a morph across four different tables, Blend | Fluid? |
| | `s8-2-quantum-tables` | the same corners, Quantum: tables flip at their boundaries | Do the flips feel like transitions? |
| | `s8-3-lfo-x-env-frame` | a global LFO on X, an envelope on the frame | Does modulation feel fluid? |
| | `s8-4-human-take` | the human playing the test plug-in | The human's notes |

Every clip is one oscillator's member, never two engines (GUI-1).

### 5.7 A change in the middle of the build

A spec change ruled while the build runs goes to **the first stage not yet started**, never into a finished one.
A finished stage reopens only by the human's ruling, and then as a row in the next stage.

### 5.8 The end state: how the code reaches the clean-room engine

- **`vendor/engine2/`** is a read-only reference export of the clean-room engine at the pinned commit, for reading
  only. **`seam/`** is a git clone of the same commit, on a branch, where S7 and S8 wire the table member in.
- **S8 delivers three things:** (1) a SPEC-ENGINE round, drafted by the lead from §8 and reviewed by a critic, so
  the engine's spec states what its code does; (2) a patch series against the pinned commit (the table modules and
  the seam), from `seam/`; (3) a change request for the harness header `engine2/tests/harness/horde_engine.h`
  (`set_table`, the `Tables` event, `set_corner_tables`, the observation and counter fields), which belongs to the
  clean-room test side; the lead files it there, with the round, in the same turn.
- **How the patch lands is OPEN** (§7 F3). The clean-room engine's repo stays its owner's: nothing is written there
  from this build.

---

## 6. Gate rules

### 6.1 What closes a stage

All of these, recorded in STAGES.md with the commit:
1. **every gate of this stage and of every earlier stage green, on the clean export (§6.3)**, run by the lead,
   each gate's mutation plants firing and its control reading zero in the same run;
2. **the instruction count within the stage's share** (§5.4), beside the test count;
3. **the stage's own night's timed run within its share**, and the conversion factor re-fixed from it;
4. **the listening batch heard and every note answered** (§5.5);
5. **REPORT.md** naming the commit, the gate output verbatim, the counts and anything left open.

### 6.2 What happens on red

- **A red gate changes the engine**, never the gate.
- **A gate moves only with engine-free evidence and the human's ruling** (ADR-247), recorded in STAGES.md and as
  a horde ADR amendment; the evidence never includes the builder's numbers.
- **Red halts forward work** (charter): no stage starts on a red one; no change stacks on red.
- **A plant that does not fire, or a control that does not read zero, is a red gate**; the lead fixes the gate
  before the builder touches the engine (a gate that cannot fail proves nothing).
- **Over budget:** §5.4.

### 6.3 Verify from a clean export

The lead runs every gate on `git archive <commit>` extracted into a fresh scratch directory, never on the builder's
working tree. REPORT.md's numbers are the builder's; STAGES.md's are the lead's, and only those close a stage.

### 6.4 Real-time checks

RealtimeSanitizer and ThreadSanitizer through Homebrew LLVM (Apple's clang lacks rtsan and its TSan fails on this
machine), from S1 for the read and at every later stage for every new audio-thread path, each with a planted
allocation (rtsan) or unsynchronised write (TSan) and the clean build as control.

### 6.5 The machine is shared

One heavy job at a time, at most 4 threads, under `nice`, the load checked first. Timed runs only from 2 to 6 am,
with Live and OneDrive closed (ADR-380 A1).

---

## 7. Decisions: ruled, and open

**Ruled (ADR-385, 2026-11-07):** a new local repo with one builder, the lead owning gates, measurement and
listening; the oscillator first, then wired in; every stage ends with CPU, fidelity gates and ≤ 6 clips;
**blending different tables across the pad is in 1.0** (ADR-377 A3 / ADR-385: the spectral-blend class, §4.2);
**MS-29 stands** (ADR-384 #9 moot); **WT-BUDGET 4 % per oscillator, provisional.**
**Ruled (ADR-385 A1, 2026-11-08, "Plan 3 with a PM matrix"):** the build plans three oscillators, the count a build
parameter from S1; PM any oscillator to any, self included, with feedback; 12 % for three. The real engine's third
oscillator is a separate later ruling and round (§10).

**Open**, each with the drafter's recommendation:

### A. Direction

- **A1. Replace or beside?** Recommendation: **build replace's seam (S7) and rule after the filters and modulators
  land**, as the human suggested. Beside is a port (§1.7), replace a member swap.
- **A2. How base and table appear, and when new patches read tables.** WT-19's `member` is state the page never
  offers as a choice (GUI-1). Recommendation: under replace, the base waves become one factory table (sine,
  triangle, saw, square, as frames), so the page shows one wave menu; new patches take `member = table` by an ADR
  and a migration (Blind-Spot Armor; RC-9.2), not before the factory patch session.

### B. Morph and modulation

- **B2. Spectral log's shared frame under MS-47's Refuse.** Recommendation: **mute per-note routes to `wt.frame`
  while the law is Spectral log**, named on the page, unmuted under Linear: it serves ADR-379 A1 #3's wish to
  disallow per-note sources on universal parameters.

### C. Fidelity, memory, CPU

- **C1. Level spacing, and ADR-384 #8 against QS-2.** QS-2's 3 dB at C6 cannot be met under #8's ceiling with
  WT-5's crossfade: engine-free, C6 reads −6.84 dB at one level per octave, −5.30 at half and quarter, −2.54 at an
  eighth, and only an eighth meets 3 dB across C2 to B5, at about 103 MiB per instance even with strategy (b).
  Recommendation: **judge WT-F2 as the design's own figure** (conformance within 0.5 dB, as built), **hear S3's
  three glides, then rule the spacing**; half-octave is the one that fits the memory cap, with strategy (b).
- **C3. The PROPOSED numbers** in the spec's section 9. Recommendation: approve; S1 and S2 measure them.
- **C4. The memory cap**, 24 MiB per oscillator's table set: 48 MiB for two, 72 MiB for three. Recommendation:
  approve; for three it allows strategy (a) at one level per octave (59.7 MiB) and strategy (b) at half-octave
  (44.3 MiB), and refuses (a) at half-octave (106.6 MiB).
- **C5. The rebuild interval:** 4 control intervals for ADR-384 #7's 128 samples. Recommendation: approve.
- **C6. Spectral log's frame interpolation** (WT-3). Recommendation: **the log domain**, the test written to match.
- **C7. The rebuild crossfade's scheme** (WT-15). Recommendation: build the fused read; keep the once-per-interval
  set if it passes WT-F4's moving-phase probe.
- **C8. The level size cap** (WT-6). Recommendation: **8,192 samples with the 6-point read**; 8-point at 4,096 if
  the timed runs show cache misses.
- **C9. Linear's strategy** (WT-23). Recommendation: **(a), read-time**, at one level per octave: no Linear
  rebuilds while the morph moves, which is where B648 found the cost; measure (b) at S4.
- **C10. Loudness** (WT-16): K-weighted at C3, level-matched to the base saw, peak limit 2.6, gain cap +24 dB.
  Recommendation: approve; WT-F7 at ±1.0 dB.

### D. The sound's controls

- **D1. The warps:** asym and bend (one smooth warp), sync with its window; mirror out. Recommendation: approve;
  hear S5a.
- **D2. Reuse `crossMod`, `feedback` and `loopDrive`** for table members (amends C.14, C.16, PA-8). Put to the
  human **only after the lead's engine-free stability map** (WT-31). Recommendation: approve then.
- **D3. No Normalise toggle.** Recommendation: approve.

### E. Transformations

- **E1. Audio-rate `wt.sync`, `wt.syncHz` and the matrix rows** (PA-4). Recommendation: approve; PF-2 re-measured.
- **E2. Superseded** by ADR-385 A1: PM goes any oscillator to any, back-edges by interleaving (WT-63, WT-64).
- **E3. High has no opt-out** for table members. Recommendation: approve.

### F. Process

- **F1. The repo name** `horde-wavetable`, local only. Recommendation: approve.
- **F2. The clips** in §5.6. The human may swap any before a stage closes.
- **F3. How the patch series lands in the clean-room engine** (§5.8). (i) **merged**, the clean-room builder
  reviewing it, with the spec round so the test side can write tests from the spec; or (ii) the clean-room builder
  re-implements from the round, using this build's gates as the oracle. Recommendation: **(i)**: ADR-385's "drop it
  into the member seam" reads as a merge, and (ii) repeats the work the clean room is behind on.

### G. Tables

- **G1. The factory set:** the bench's eight at 1,024 harmonics, Sine → saw negated to the base wave's polarity.
  Recommendation: approve; S1's tour asks what is missing.
- **G2. The random generator** gets a recipe slot only (WT-46).
- **G3. Import of wavetable files made elsewhere.** Recommendation: after 1.0, as a user table with its own recipe
  kind.

---

### H. Three oscillators and the PM matrix (ADR-385 A1)

- **H1. A matrix, or fixed algorithms?** The human ruled the matrix. Open is only whether the page offers a few
  named algorithms (a stack 3 → 2 → 1, two modulators into one carrier, three carriers, a loop) as **presets that
  fill the matrix**. Recommendation: yes, as page-level starting points that write ordinary matrix values, never as
  a mode the engine knows.
- **H2. The feedback law on back-edges** (WT-63): a plain one-sample delay, or averaged over the last two samples
  as C.14's feedback is. Recommendation: **averaged**: it damps the period-2 ringing a one-sample loop makes, and it
  is the law the diagonal already uses, so one loop law serves both.
- **H3. Osc 3 starts off in a new patch.** Recommendation: **off** (WT-60): two-oscillator patches keep their
  sound and their CPU (an off source costs nothing, PF-6), and osc 3 is one switch away.
- **H4. The diagonal is C.14's `feedback`**, not a new self-PM row (WT-61). Recommendation: reuse (one concept,
  BD-2); if S5b's feedback clip shows its range or its control rate too narrow, make `feedback` audio-rate (PA-4)
  rather than add a second self-PM.
- **H5. The fixed render order** osc 3 → osc 2 → osc 1, so the classic stack has no delay (WT-63).
  Recommendation: approve.
- **H6. The back-edge clamp** I_back (WT-65): 4 until the lead's stability map, then the map's limit.
  Recommendation: approve; the human hears it in `s5b-4-feedback`.
- **H7. Segment mode by default, interleaving only while a back-edge is live** (WT-64). Recommendation: approve:
  always interleaving would charge every patch for a feature most do not use.
- **H8. The internal modulator's rows** are held at 0x03B to 0x041, PROPOSED (WT-70). Recommendation: finalise
  them when the lab (B652) is ruled, before S5a starts; if the lab is later, S5a builds without it.
- **H9. When to rule the real engine's third oscillator** (§10). Recommendation: after S5b's operator-stack clips,
  so the human rules with the sound in hand; until then the seam is built at two (S7, S8).

---

## 8. SPEC-ENGINE rows a port amends

Not edited here; the lead drafts the round (§5.8).

| row | change |
|---|---|
| 5.3 | the ten `wt.` rows (§2.1) |
| 5.8 | counts (350 rows, 14 reserved, 336 host) |
| C.14 | xin also pushes a table member's read phase (WT-31) |
| C.16 | the blocker's "loop live" condition includes table members; the table member's means (WT-25) |
| PA-4 | `wt.sync`, `wt.syncHz`, the matrix rows, `wt.modDepth` (if kept) audio-rate |
| PA-5 | `wt.syncIn`, `member`; not the law or the tables (WT-15) |
| PA-8 | the new rows' live conditions; `crossMod`, `feedback`, `loopDrive` live for table members |
| IF-4 | the engine document's `wt` member |
| IF-9, IF-10 | counters and observations (§1.9) |
| IF-12 | `set_corner_tables`; the `Tables` output |
| MO-13, a new Appendix D row | the spectral-blend class (§4.2) |
| ADR-377's post-1.0 bullet | amended for corner tables (ADR-377 A3 / ADR-385) |
| RT-1, PF-7 | the table sets in prepare's maxima; the memory statement |
| C.14, a new row | the PM matrix (WT-61 to WT-66): taps with on-fade, the render order, back-edges, the clamp, both render modes |
| 5.3 | the matrix rows at 0x039 and 0x03A, the reserved rows naming osc 3, the internal modulator's rows when ruled |
| the structure file | `instrument.table_set`; the member's blocks |

---

## 9. What made the seam harder than expected

1. **The blades read the base wave in closed form everywhere** (§1.7): beside is a port, not a switch.
2. **The loop exists only when a blade is on** (`n_blades_ > 0`, §1.1; PA-8's rows say so): reusing it amends
   C.14, C.16 and PA-8.
3. **The voice renders each oscillator over a whole segment:** the PM matrix's back-edges need `Swarm::render`
   split so the swarms can be interleaved per sample, and every modulator must carry its on-fade and PA-5 mix
   explicitly (§1.8; WT-62 to WT-64).
4. **PA-5's copy shares the per-instance table set**, so the law and table changes need the rebuild crossfade, not
   PA-5 (§1.8).
5. **Every member passes through ED-1's filter E**, which delays it about 2.4 samples: sync's on/off is stepped
   so E switches only inside PA-5's crossfade, and the warps were made smooth so they never need E.
6. **The resolver's corner call carries two `ratios` texts** (`resolver.h` line 22): tables need a new call and a
   new output kind.
7. **New rows shift manifest indices** (§2.2): any by-index test must be reseeded by key.
8. **ADR-384 #1, #8 and QS-2 cannot all hold at 44.1 kHz** (§7 C1), and the bench's read fails QS-1 below C6.
9. **The cost bench (B648) found three more:** Spectral log's cached frame lerp is not the law the corner test
   assumed; the rebuild crossfade always runs under motion; the pitch crossfade fades the top octave early.
10. **The clean-room engine is three rounds behind its spec** (62 to 64), and its harness still says 16 members:
    S7 pins a commit and re-reads the seam.

---

## 10. The real engine's third oscillator: not decided here

The build plans three oscillators (ADR-385 A1). **The real engine's third oscillator is a separate later ruling
and spec round** (§7 H9). This build does not decide its ids, its mixer channel, its place among the sources, its
morph keys, its modulation destinations or its GUI pages. What that round would have to change:

**SPEC-ENGINE rows.**
- §1, §2 (the glossary's "one of the voice's two swarm oscillators"), §4.1 and §4.2's signal path.
- 5.2: a level row `mix.osc3` (the voice block's next free offset is 0x13) and `osc3.on`; 5.3 and 5.4 once more
  per oscillator; 5.8's counts.
- VO-9; RC-6.1 (the sources as the slot's instances) and RC-6.4, whose test build adds "a fifth source" at 0x6000:
  osc 3 would be the fifth source, the test's sine a sixth.
- MO-26 and D.8: a fifth source, and ADR-383's **one shared pick over the sources' on switches** extended to five;
  IF-12's morph levels per source; the resolver's source list.
- IF-3 and MS-8 (each oscillator's swarm sources R and ψ), MS-12's destinations, PA-4 (`osc3.pitch`, `osc3.fine`,
  `mix.osc3`), PA-5, PA-8, IF-6's meters, IF-9 and IF-10's observation.
- PF-1 and PF-2: the sources' 34 % share of a min-spec core was set for two swarm oscillators; PF-6; RT-1's maxima.
- B.1: the bank translates with osc 3 off.
- The GUI pages, which are the GUI session's rows (ADR-336).

**The clean-room engine's code** (at `e72a092`), sized for two oscillators and four sources:
`OscSettings osc[2]` (`voice/voice.h` line 137), `Swarm osc_[2]` and `xf_[2]` (lines 253, 271), the voice's
`for (int o = 0; o < 2; ++o)` loops; the engine's per-source arrays sized 4 (`level_`, `gate_`, `morph_`,
`source_l_`, the morph-level glides; `api/engine.h` lines 89 to 125), `gui_peak_[5]` (line 137) and
`observe_morph_level`'s `source < 4` (line 52); the resolver's `enum Source { kOsc1, kOsc2, kSub, kNoise,
kSources }` (`morph/resolver.h` line 53) and `set_corner`'s two `ratios` texts (line 22); the harness header's
`kSourceOsc1 … kSourceNoise` (line 127) and `OscView osc[2]` (line 369).

**The manifest block.** `pid::osc(o, row)` is 0x1000·(o + 1) (`manifest/manifest.h`), so **o = 2 gives 0x3000,
the sub's block**: the formula cannot simply extend, and osc 3 needs a block of its own. **0x8000 appears free**
for it (its blades at 0x8100 and 0x8200 and its `stream` at 0x8300, by the oscillators' layout): the clean-room
manifest's highest product id is 0x5209, the test builds use 0x6000 and 0x7000, horde's `specs`, `docs`, `engine2`
and `tools` hold no id in 0x8000 to 0x8FFF (the one match, 0x8040 in `tools/user_patch_bench.cpp`, is an FTZ/DAZ
register mask, not an id), and IF-11's ceiling is 0xB00000. The morph's and the modulators' lockfile blocks are
appended after the engine's with no numbers fixed yet (IF-11, MS-40), so that round must fix them before it claims
0x8000. **Nothing is assigned here.**

**In the build meanwhile,** osc 3's rows live at build-local ids in the build's own manifest, which are not engine
ids and never reach a saved patch outside the build.

