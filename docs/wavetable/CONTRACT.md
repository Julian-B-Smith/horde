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

> Reworked 2026-11-09 on B663: ADR-387 (the table member replaces the blades; three oscillators in 1.0), ADR-388
> (blade baking, `docs/wavetable/BLADE-BAKE.md`; the budget after the timed run; the `retired` class; the seam at
> three oscillators; osc 3's ids; the base/table switch test-only), ADR-389 (sync always on, softness), the
> integration inventory's open decisions (`docs/wavetable/INTEGRATION.md` §12, now §7 group I) and GN-57 to GN-59.

> Amended 2026-11-09 on B666: ADR-390 (WT-BUDGET 5 % per oscillator at crossfade scheme (iii); §7's
> recommendations adopted; S0's proposals P1 to P7, `docs/wavetable/S0-RESULTS.md`) and the GUI session's GN-60.

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
→ member.blade1 → member.blade2 → member.edges → member.dc → member.pan`, with `member.loop` beside it. The table
member replaces the base wave and the blades (ADR-387):

| block | what happens | how |
|---|---|---|
| `member.phase` | kept | θᵢ and fᵢ as today (WT-20) |
| `member.base_wave` | **replaced** | step 3 becomes the table read and its transformations |
| `member.blade1`, `member.blade2` | **retired** | step 4 skipped; their rows retire (§2.4); the code stays for WT-19's oracle until the retirement round (§7 I22) |
| `member.edges` | **only for a sync restart with a step** | no filter E on a table member (WT-24); a restart with a step adds its corrections to `pending` (step 8's mechanism with a plain residual, WT-24, WT-32) |
| `member.dc` | kept, new source | WT-25's means replace the blades' estimate |
| `member.loop` | kept, **amended** | the tap and xin must run for table members though no blade is on (WT-31; C.14, C.16, PA-8 amended, §8) |
| `member.pan` | kept | unchanged |


### 1.3 The member's interface

A new concept, `TableMember`, in its own file (BD-1): a per-member state record and a per-sample call, described
here as an interface (the code is the builder's).

- **State**, added to the swarm's `Member` so PA-5's copy carries it: the sync phase; the member's frame and spread
  positions; its loudness gain under Linear (WT-16); its level and pitch-crossfade weight (WT-5); its DC estimate.
  About 64 bytes.
- **Per sample it receives:** θᵢ and fᵢ/f_int; the published set's read-only view and its crossfade position
  (WT-15), or under Linear's read-time strategy the four corners' level sets and the ramped weights; the ramped
  transformation values; C.14's xinᵢ; under the PM matrix, each column's modulator m (WT-62) from the voice's buffer or, interleaved, from the modulator's previous samples (§1.8); for a sync restart,
  the step residual tables and the member's `pending` ring.
- **It returns** the member's output before SW-13's fade, adds a stepped restart's corrections to `pending`
  (WT-24), and counts its reads, edges and level changes.
- **At each control tick** the swarm hands it the frame position (the voice's resolved `wt.frame` plus offsets,
  plus C.10's spread), and it picks its level for the interval's peak read rate (WT-36) and its Linear gain.

### 1.4 What stays untouched

The swarm's laws (C.1 to C.8, SW), the voice (VO), the envelopes (EN, C.12), the filter slot (RC-1), the sub and
the noise (SN), the mixer and output stage (OU), the morph resolver's laws apart from §4.2's class, and the
modulation system apart from §2's destinations. With every oscillator's `member` at `base`, the engine renders
bit-identically to the engine without the table code (WT-P4, tested as RC-6.4 tests a fifth source).

### 1.5 The table set: a new block, per oscillator, per instance

A new concept, `TableSet`, owned by the engine (`HordeEngine`), one per oscillator: K of them, three in 1.0. Voices read it through a const view
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

### 1.7 History: the beside reading

Until ADR-387 the contract kept a second reading open, in which the blades would cut a table member. It would have
replaced the base wave's closed forms (`wave`, `wave_slope`, `deriv`, `jumps_of`, `mean_over`, `dc_estimate`) at about
twenty call sites in `swarm/blades.cpp`, `swarm/direct.h`, `swarm/wide.h` and `blade/dc.cpp` with table reads of the
value, the slope and the antiderivative, at about three times the level-set memory. ADR-387 ruled replace; beside is
not a build path. The blade law survives only frozen inside the blade-bake generator (BLADE-BAKE.md BB-60).

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
- **PA-5.** `wt.syncIn` (× pitch ↔ Hz) and WT-19's test switch `member` join the stepped keys (`stepped_equal`, `with_stepped`). **The law, a
  corner's table and a Quantum flip of the tables cannot use PA-5**: the table set is per instance, so `xf_[o]` and
  the live swarm (line 426) read the same set; they change by WT-15's rebuild crossfade.

### 1.9 The engine's API, observation and counters

- **Tables in.** `set_table(osc, corner, recipe)`, beside `set_ratios`: main thread, builds and stages (§1.6),
  then posts the `Tables` event. The resolver's weights arrive as a sample-timed event per oscillator (§4.2).
- **State.** The engine's IF-4 document gains a top-level member `wt`: per oscillator the four corner recipe ids it
  last received, as it keeps the `ratios` text, and through S8 the test switch `member` (WT-19). Retiring the blade
  rows and baking old patches is a breaking change, so IF-4's `version` rises to 2 with a migration from 1 (RC-9.4):
  the migration runs the blade bake (BLADE-BAKE.md BB-50) and keeps every retired row's value byte for byte (§2.4).
  The recipes live in the patch's table dictionary (§3).
- **Observation (IF-10).** Per oscillator: weights, law, the published set's levels, crossfade position, rebuild
  count. Per member: frame position, level, pitch-crossfade weight, output before the fade. No product branch is
  added for them (BD-4).
- **Counters (IF-9).** Rebuilds, levels built, table reads, sync restarts corrected, peak-limit and gain-cap
  engagements, tables refused, swaps applied, back-edge clamp engagements (WT-65), interleaved segments (WT-64).
- **The GUI's observations** (RC-8's snapshot; the GUI session's GN-57 to GN-60). Each is **computed only while a
  page asks** for it (a flag the page sets, read atomically by the engine), published at the GUI frame rate (RC-8:
  the first control boundary after each 1/60 s), and costs nothing otherwise.

  | need | what is published | where it is computed | cost while asked |
  |---|---|---|---|
  | **GN-57** (as GUI-63 narrows it) | per oscillator, the table read at the puck as its 16 frames, each a cycle of 128 points (PROPOSED), **always**; and each corner's 16 frames **only while the morph is on** | **on the main thread**, from the canonical spectra, the weights and the law the audio thread last published in the snapshot; never on the audio thread. **Only the harmonics 128 points can show (1 to 63) are blended**: Spectral log over all 1,024 harmonics at 60 frames a second would cost about 20 % of a core (the critic's estimate) | per oscillator 16 × 63 harmonic blends (× 4 corners with the morph on) and 16 inverse transforms of 128 points per frame, at most 60 a second: well under 1 % of one core, off the audio thread |
  | **GN-58** | per voice, per back-edge, the clamp's engagement count (WT-65; forward edges never clamp), and per voice the interleaved flag of its latest segment (WT-64) | counted on the audio thread always (they are IF-9's counters, per voice); copied into the snapshot only while asked | 64 voices × 3 back-edges × 4 bytes plus 64 flags: under 1 KiB a frame |
  | **GN-59** (its text's "window" reads "sync softness" since ADR-389) | per oscillator, for the voice the page names (default: the newest held), its output after every transformation and PM, before the filter: one cycle of the oscillator's mono member sum at the voice's pitch, with the pitch; and member 0's y⁰ over the same span (IF-10's) | **a gated mono accumulator** per oscillator, written by the audio thread only while asked: it sums the named voice's members (before pan) into a ring of 16,384 samples, enough for a whole cycle down to about 2.7 Hz at 44.1 kHz (a 4,096-sample ring would cut cycles below about 10.8 Hz); the snapshot takes the latest whole cycle; the page resamples it and takes its spectrum | one add per member-sample for one voice while asked, and a copy of up to 3 × 2 × 16,384 floats per GUI frame (about 23 MiB/s at 60 frames, PROPOSED; the copy may run at 30 frames) |
  | **GN-60** (GUI-69) | per oscillator, per voice as IF-10's view is, the frame position after modulation at the display rate, beside its frame row (the selected frame); it relates to GN-57 (the frames drawn) and GN-59 (the output read at this position) | not yet designed; IF-10 already observes each member's frame position (above) | not yet estimated |

---

## 2. Parameters

### 2.1 The new rows

**Osc 1 and osc 2** gain eleven rows each (SPEC-ENGINE 5.1's format), keys `oscN.wt.…`, at the next free offsets of
their blocks; **osc 3** gets the same rows at the same offsets in its block, 0x8000 (ADR-388 #4b; §10). Ids below are
offsets: osc 1's are 0x1000 + offset, osc 2's 0x2000 + offset, osc 3's 0x8000 + offset. **They are proposals in a
draft, not ratified;** nothing ratified is renumbered. Amended 2026-11-09 (ADR-389): `wt.syncIn` loses "off",
`wt.sync` defaults to 1, `wt.window` is withdrawn and `wt.syncSoft` added.

| offset | key | label | unit | range | default | law | class | rate | PV | what it does |
|---|---|---|---|---|---|---|---|---|---|---|
| 0x030 | `wt.law` | Blend | — | Linear, Spectral log | Linear | step | S | T | — | How the corner tables combine (WT-11, WT-12). |
| 0x031 | `wt.frame` | Frame | — (shown 1–16) | 0..1 | 0 | lin | M | C | yes | Where in each table the members read (WT-22). |
| 0x032 | `wt.frameSpread` | Frame spread | ± | −1..1 | 0 | lin | M | C | yes | Fans the frame across the members (WT-22). |
| 0x033 | `wt.syncIn` | Sync | — | × pitch, Hz | × pitch | step | S | T | — | Sync follows the note or a fixed frequency (WT-32); always on (ADR-389). |
| 0x034 | `wt.sync` | Sync ratio | × | 1..16 | **1** (no sync) | log | M | A | yes | Table cycles per member cycle (WT-32). |
| 0x035 | `wt.syncHz` | Sync | Hz | 20..12 000 | 880 | log | M | A | yes | The fixed sync frequency, r = max(1, `wt.syncHz`/f) (WT-32). |
| 0x036 | — | — | | | | | | | | **Withdrawn** (the draft's `wt.window`, superseded by ADR-389). Never ratified, but left empty and never reused, so no draft or lab file that named it can be misread. |
| 0x037 | `wt.asym` | Asym | ± | −1..1 | 0 | lin | M | C | yes | Moves the half-cycle point (WT-34). |
| 0x038 | `wt.bend` | Bend | ± | −1..1 | 0 | lin | M | C | yes | Bends time toward the cycle's start or away (WT-34). |
| 0x039 | `wt.pmA` (below) | PM from the first other oscillator | index | 0..10 | 0 | pow | M | A | yes | The matrix entry from the lowest-numbered other oscillator (WT-61). |
| 0x03A | `wt.pmB` (below) | PM from the second other oscillator | index | 0..10 | 0 | pow | M | A | yes | The matrix entry from the other one (WT-61). |
| 0x042 | `wt.syncSoft` | Sync softness | % | 0..1 | 0 | lin | M | C | yes | The taper over the synced cycle's ends; nothing at 1×, engaged by ×1.25 (WT-33; ADR-389). |

**The matrix rows' keys are absolute** (the layout rule is "the other oscillators in ascending order"; the
diagonal is each oscillator's `feedback`, WT-61). **All six are live in 1.0** (three oscillators, ADR-387):

| block | 0x039 | 0x03A | diagonal |
|---|---|---|---|
| osc 1 | `osc1.wt.pm2` (the first draft's `osc1.wt.pm`: same id, same meaning) | `osc1.wt.pm3` | `osc1.feedback` |
| osc 2 | `osc2.wt.pm1` (the first draft's `osc2.wt.pm`: same id, same meaning) | `osc2.wt.pm3` | `osc2.feedback` |
| osc 3 | `osc3.wt.pm1` | `osc3.wt.pm2` | `osc3.feedback` |

**The internal modulator: PROPOSED, pending the lab** (B652; WT-70; §7 I8). Ids held at 0x03B to 0x041, not
finalised. The lab's FM mix, if adopted (§7 I8), takes **0x043**; a further row takes 0x044 onward.

| offset | key | label | range | default | law | class | rate | PV |
|---|---|---|---|---|---|---|---|---|
| 0x03B | `wt.modShape` | Mod shape | sine, triangle, saw, reverse saw, square, smooth noise, S&H noise | sine | step | S | T | — |
| 0x03C | `wt.modIn` | Mod rate in | × pitch, Hz | × pitch | step | S | T | — |
| 0x03D | `wt.modRate` | Mod rate | × 0.25..24 (the lab: ×1/16..×32, §7 I8) | 1 | log | M | C | yes |
| 0x03E | `wt.modRateHz` | Mod rate | 1..20 000 Hz (the lab: 0.1..20 000, §7 I8) | 5 | log | M | C | yes |
| 0x03F | `wt.modType` | Mod type | phase, pitch | phase | step | S | T | — |
| 0x040 | `wt.modDepth` | Mod depth | index 0..10 | 0 | pow | M | A | yes |
| 0x041 | `wt.modDecorrelate` | Decorrelate | 0..1 | 0 | lin | M | C | yes |
| 0x043 | `wt.modMix` (if adopted) | FM mix | 0..1 | 1 | lin | M | C | yes |

**Osc 3's block, 0x8000** (ADR-388 #4b): `osc3.on` at 0x8000 (osc 1's `on` is offset 0x00); its oscillator rows at
osc 1's offsets, **without the six blade-only rows** (`baseWave`, `quantize`, `stacking`, `upperHears`,
`collisionPitch`, `collisionBite`) and without blade blocks or a `ratios` text, their offsets left empty; the `wt.`
rows above at 0x8030 to 0x8042; the internal modulator from 0x803B; `stream` at 0x8300; its defaults are osc 1's except `seed`, **1236** (osc 1's 1234, osc 2's 1235, clean-room `manifest.cpp` line 100; §7 J11). **`mix.osc3`** is voice
offset **0x0013** (`kQuality` is 0x0012). `osc3.on` defaults to off (§7 H3).

**Not host parameters:** WT-19's test switch `member` (engine state through S8, gone in 1.0) and each corner's
`oscN.wt.table` (morph corner state, WT-51).

**Counts in 1.0** (three oscillators; the round re-derives them; they agree with INTEGRATION §2.2):

| | today (5.8) | 1.0 without the internal modulator | with its 7 rows per oscillator | with `wt.modMix` too |
|---|---|---|---|---|
| manifest rows | 330 | **407** = 330 + 22 (`wt.` rows on osc 1 and 2) + 43 (osc 3's oscillator rows with `on` and `stream`) + 11 (osc 3's `wt.` rows) + 1 (`mix.osc3`) | 428 | 431 |
| reserved | 13 | 15 (+ osc 3's `pitchMode` and `stream`) | 15 | 15 |
| retired (new, §2.4) | 0 | 136 | 136 | 136 |
| host parameters (rows − reserved − retired) | 317 | 256 | 277 | 280 |

### 2.2 The ids: how they were verified free

Three sources, each checked for the new ranges (osc 1 and osc 2: offsets 0x030 to 0x043, ids 0x1030 to 0x1043 and
0x2030 to 0x2043; osc 3: 0x8000 to 0x8FFF; the voice: 0x0013):

1. **The clean-room manifest** at `e72a092` (the engine is still at that commit), compiled on its own
   (`core/manifest/manifest.cpp` and `memory.cpp` against the harness header) into a scratch program that printed
   every row: **330 rows**, as 5.8 says. Voice 0x0001 to 0x0012; each oscillator block 0x000 to 0x02F (48 rows),
   0x100 to 0x11D (blade 1), 0x200 to 0x21F (blade 2), 0x300 (`stream`); sub 0x3000; noise 0x4000; ENV 2 to 4 0x5000
   to 0x5209; the test builds' 0x6000 and 0x7000 (`manifest.h`). No row in any new range.
2. **Horde's tree:** `git grep` over `specs`, `docs`, `engine2` and `tools` finds 0x1030 to 0x1043, 0x2030 to 0x2043,
   0x0013 and 0x8000-range ids only in this file and INTEGRATION.md; the other 0x8… matches are hash constants of
   eight hex digits (FNV-1a's 0x811C9DC5, a mixer's 0x85EBCA6B) and an FTZ/DAZ register mask (0x8040 in
   `tools/user_patch_bench.cpp`), not ids.
3. **The lockfiles and the host ids already given out:** `tools/param_id_lock.json`'s `h2` section is a stub with no
   ids, so **the morph's and the modulators' blocks are not numbered yet** (IF-11 appends them after the engine's,
   MS-40 after the morph's). The test plug-in's own host ids are 0xA00001 and 0xA00002 (the morph's X and Y,
   `preview_plugin.cpp` line 81), 0xA00010 to 0xA00015 (B660's filter, `filter_core.h` line 20) and 0xB00001
   (Preset, `preview_gui.h` line 13): none in the new ranges. ADR-388 #4b's condition ("after the morph's and
   modulators' lockfile blocks are checked") is met as far as it can be today; **the round that numbers those
   blocks must keep them out of 0x8000 to 0x8FFF and off 0x0013**.

**Placement.** `manifest/manifest.h` (line 10) and 5.8 say a new key takes the next free offset of its block: 0x030
onward on osc 1 and 2, and 0x042 for `wt.syncSoft`, the first offset not held by the internal modulator's proposal.
`stream` stays its block's last id. **A consequence:** `manifest_param(i)` is in ascending id order, so the rows at
0x1030 and 0x2030 shift the index of every later row; osc 3 at 0x8000 sits above the test blocks and shifts nothing
of theirs. A test that seeds or iterates by index must seed by key (WT-F36).

### 2.3 SPEC-ENGINE rows these touch

- **5.1** gains the `retired` class (§2.4); **5.8** its counts.
- **PA-4** gains `wt.sync`, `wt.syncHz`, the matrix rows and, if kept, `wt.modDepth` as audio-rate destinations
  (the engine path, MS-41); PF-2 is then re-measured (ADR-379 A1).
- **PA-5** gains `wt.syncIn` (× pitch ↔ Hz) and the test switch `member`. **Not the law or the tables:** they
  change by WT-15's rebuild crossfade (§1.8).
- **PA-7** refuses and counts a `Param` addressed to a retired row, as it does a reserved one.
- **PA-8** gains the live conditions below; its rows for `crossMod`, `feedback` and `loopDrive` change (WT-31).
- **C.14** and **C.16** change for the loop on table members (WT-31).

### 2.4 The `retired` class, and live and inert (ADR-388 #3)

**The class.** A **retired** row is a parameter the instrument once had and no longer plays. It shares the reserved
class's mechanism (5.1) and differs in what its value means:

| | reserved (5.1) | retired |
|---|---|---|
| meaning | designed, not yet built | built, no longer played |
| id | frozen | frozen, never reused or deleted (RC-9.1) |
| in the manifest | yes | yes, its own count in 5.8 |
| host parameter | no | **no** |
| value in a saved state | kept unchanged, read as 0 (its later default) | **kept byte for byte, any value**, read by nothing but the blade bake (BLADE-BAKE.md BB-1) |
| a `Param` event to it | refused and counted (PA-7) | **refused and counted** (PA-7) |
| on a page, in Random, in the host's list | no | **no**; a load report names those an old patch set away from default and the bake did not carry (BB-30) |
| the morph | not resolved | **not resolved**: no corner value is sent, since nothing plays it |
| live (PA-8) | never | never |

**Which rows.** On osc 1 and osc 2: both blades' rows (5.4: 30 and 32), and the six blade-only oscillator rows
`baseWave`, `quantize`, `stacking`, `upperHears`, `collisionPitch` and `collisionBite`: **68 per oscillator, 136**.
The `ratios` text (oscillator state, not a row) is retired the same way: kept in state, read only by the bake. Osc 3
has none of them.

**The new rows** are live while their oscillator is on. `wt.frameSpread` needs Linear and `unison` > 1; `wt.sync`
needs `wt.syncIn` at × pitch, `wt.syncHz` at Hz; `wt.syncSoft` needs a ratio above 1; a matrix row needs its
modulator oscillator on; the internal modulator's rows need `wt.modDepth` above 0 (`wt.modRate` at × pitch,
`wt.modRateHz` at Hz).

**The rows that stay live for table members:** `crossMod`, `feedback`, `loopDrive` (on the read phase, WT-31;
PA-8's "a blade is on in Sync, FM or Ring" becomes "the oscillator is on"); `spreadLaw`, `spreadDrift`,
`motionGrain` (through `wt.frameSpread`); `bandLimit` (it scales a sync restart's correction); every swarm, pitch,
pan and envelope row.

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
  `factory/comb`, `factory/organ`; the random generator's is `random` (TABLECASTER's engines, ADR-386, §7 I18); the
  blade bake's is `blade-bake` (BLADE-BAKE.md BB-40).
- `lawV` pins the blend law's version (WT-42, WT-43); every law version stays buildable.
- A capture's sources are ids resolved in the patch's dictionary, so captures form a graph and a shared source is
  stored once. No wall-clock value is in a recipe or its id (DT-5).

### 3.2 What goes where, and the bounds

| place | holds | bound |
|---|---|---|
| **the host's state chunk** | a `tables` dictionary {id: recipe} with every table a corner of any oscillator references and every capture's sources, transitively; corners hold ids | **64 recipes, 512 bytes each (2.5 KiB for a `blade-bake` recipe, BB-41; ADR-390, P6), 32 KiB in all**; capture depth 8. **Enforced when a table is assigned or captured** (WT-44), never at save; a migrated old patch needs at most 8 bake recipes, 20 KiB. The whole chunk's ceiling is §7 I16. |
| **an exported preset file** | the dictionary plus the rendered spectra of every played table that is not a factory generator, float32 in base64 | about 171 KiB per table, at most 12 corner tables: ≤ 2.1 MiB per file |
| **the user library** (device-wide, WT-45) | one file per table: recipe and spectra | — |

The host chunk never carries table data (ADR-384 #2; the lesson that a host silently keeps an old chunk when a
large one fails to save).

**The bake bound** (ADR-390, P6; S0-RESULTS §4): the worst-case `blade-bake` recipe is 2,081 bytes on the real 68
rows (2,337 with a 512-byte `ratios`), so 2.5 KiB holds it, and BB-F8's named-key plant (2,828 bytes) exceeds it,
which 3 KiB did not. **Open for the next spec round, not resolved here:**
- **the `ratios` limit:** the spec says 256 bytes (IF-12, MO-13), the engine accepts 512 (`kRatiosMaxBytes`);
- **the number form:** §3.1's canonical form is ECMAScript's shortest form, the engine's state writer prints
  printf's `%.*g` form; recipe ids hash the canonical text, so the two differ where they meet.

### 3.3 Loading

1. The main thread rebuilds every recipe before the load returns (WT-18). A version-1 state is migrated first:
   its blades are baked (BLADE-BAKE.md BB-50), its retired rows kept byte for byte (§2.4).
2. An unknown generator or law version: the embedded spectra, else the library copy by id, else the corner's
   default factory table. **Every fallback is reported by name** in the load report (PA-7's rule).
3. A capture whose sources cannot all be rebuilt: the same order, reported.

---

## 4. Modulation and morph

### 4.1 Destinations

| destination | scope (MS-12) | path (MS-41) | notes |
|---|---|---|---|
| `wt.frame` | per-voice | outside | under Spectral log the newest held voice's (WT-22); the page warns as MS-46 has it; muted under MS-47's Refuse while Spectral log (WT-56). Moot if §7 I6 adopts the lab's per-voice model |
| `wt.frameSpread`, `wt.syncSoft`, `wt.asym`, `wt.bend`, the internal modulator's continuous rows | per-voice | outside | |
| `wt.sync`, `wt.syncHz`, the matrix rows, `wt.modDepth` | per-voice | engine path from ENV 1 to 4, velocity and per-note LFOs once PA-4 lists them; outside otherwise | |
| `morph.x`, `morph.y` | shared | outside | global LFOs and the 2-D random only: **MS-29, ruled by ADR-385**; ADR-384 #9 is moot |
| `wt.law`, `wt.syncIn`, the modulator's stepped rows, `member`, `oscN.wt.table`, every retired row | — | — | not destinations (stepped, state, or retired; MS-12) |
| osc 3's rows, `mix.osc3`; osc 3's swarm sources `osc3.R`, `osc3.psi`, `osc3.onset`, `osc3.gravity` (MS-8) | as osc 1's | as osc 1's | ADR-387; INTEGRATION §4 |

### 4.2 The morph's side

**The design (ADR-377 A3 / ADR-385):** `oscN.wt.table` is a key of a new **spectral-blend class** (WT-53). Every
other Structural key keeps following its quantum picks under ADR-377.

| | the table key |
|---|---|
| Blend, Stepped | MO-4's unglided weights (the stepped position's under Stepped), identical tables pooled, Count Two by MO-9, glided by MO-5 (b), restricted to on corners (MO-26 (2)) |
| Quantum | one-hot at the pick and latch (MO-7, MO-8) |
| an exact corner | one-hot publishes that corner's own level sets under both laws, with no blend arithmetic (MO-27; §7 I11) |
| a change of pick, a law or table change | WT-15's rebuild crossfade (R) |
| Exempt | corner A |
| morph off | `morph.audible`, one-hot |
| Save current sound (MO-25) | a capture recipe of the heard blend where the corners differ (WT-55) |

**The resolver's API (IF-12).** Today a corner arrives as `set_corner(corner, v, ratios_osc1, ratios_osc2, at)`
(`morph/resolver.h` line 22, and the harness header), which carries the two `ratios` texts. The tables arrive by a
**new call** `set_corner_tables(corner, osc, table id, at)`, so the existing signature is unchanged
(append-only). The resolver gains one output kind, `Tables`: per oscillator, the four corner ids and four weights,
sent at control boundaries when they change (MO-20's timing). The engine routes it to the table set (§1.5).

**Five sources** (ADR-388 #4d): `osc3.on` joins ADR-383's one shared pick of the sources' on switches, which then
has five sources; osc 3's keys (every `osc3.` key, `mix.osc3` and its table key) blend and pick only over its on
corners (MO-26). The resolver's source list, morph level per source and its mirror grow to five, osc 3 appended
as source 4 so the existing numbers keep their meaning (INTEGRATION §2.4).

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

- **WT-BUDGET: 5 % of one core per oscillator** (ADR-390) on WT-P2's judged cells, under the 2026-11-09 timed run's
  conditions (one oscillator, PF-3's chord, H = 1,024, 9 members, the morph moving, 48 kHz, thread CPU time, best
  of 5, the 2 to 6 am window; `traces/2026-11-09-b648-timed-run.md`), **15 % for three** with the PM matrix, judged
  on cells 4 and 5. The rebuild crossfade is scheme (iii) (WT-15, §7 C7), which the run read at 4.80 %. The
  provisional 4 % and 12 % (ADR-385, A1) are retired. The shares below are fractions of it.
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
| S5a | cell 3 (one oscillator) | ≤ 100 % of 5 % |
| S5b | cells 4 and 5 (K = 3, the matrix; segment and interleaved) | ≤ 100 % of 15 % |
| S6 | cells 1 to 5: no audio-path change | within 2 % of S5b |
| S6b, S6c | the generators and the bake run at load, off the audio thread: cells 1 to 5 unchanged; the bake's own time reported (BB-51) | within 2 % of S5b |
| S7, S8 | the cells in the clean-room engine at K = 3 (ADR-388 #4a); the swarm's own cost reported apart (PF-1, re-anchored to cells 2 and 5, holds the whole oscillator; PF-2 judged at two oscillators, reported at three, ruled again after S5b) | ≤ 100 % |

- **When a timed run exceeds its share** (likely: B648 measured a cheaper design at 3.9 %, its follow-up counts
  this design at about 10 to 13 % uncalibrated, spec WT-P7): (1) the stage does not close; (2) the builder applies cheaper
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
| **S0** (lead) | repo, harness skeleton, alias meter, click detector, instruction counter; WT-BUDGET's first timed confirmation; the conversion factor; **the engine-free items** (below; done, B665) | **`s0-1-unison-at-rest`**: no wavetable build needed: four bank presets in the old engine at U = 1 at rest (what the bake will give) against the original at U = 9 | every S1 gate's detector plants and controls proven engine-free | conversion fixed |
| **S1** | the data model, the factory generators, levels, the read; one member, a static table | factory tables | WT-F1, WT-F5, WT-F8 (static), WT-F9, WT-F13 (rtsan); WT-41, WT-43, WT-47 | ≤ 35 % |
| **S2** | the four-corner blend, both laws and WT-3's frame rule, rebuild and crossfade (scheme (iii), ADR-390; WT-F4's moving-phase probe judges it), WT-16's loudness | morph sweeps | WT-F4 (the moving-phase probe, differing corners, law switch, Quantum flip), WT-F6, WT-F7, WT-F10, WT-F5 at every block split | ≤ 55 % |
| **S3** | levels across pitch, the pitch crossfade, WT-36's level choice; ρ a constant | glides at three spacings | WT-F1 at C1 to C8 at 44.1, 48, 96 kHz; WT-F2 (the design figure); WT-F4 over glides | ≤ 65 % |
| **S4** | U members of the stand-in, per-voice frame under Linear (strategy (a); (b) measured), frame spread, Spectral log's newest-voice frame, 8 voices | unison and scanned plucks | WT-F4, WT-F5, WT-F7 at U = 9; WT-P4; memory against WT-8 | ≤ 85 % |
| **S5a** | the internal modulator (if the lab has ruled, else it moves to the first unstarted stage after the ruling), always-on sync with its softness taper and the plain restart correction (WT-24, WT-32, WT-33), asym and bend, the loop on table members, DC removal, High; one oscillator | sync leads, soft formants, jitter | WT-F3, WT-F8 with every transformation, WT-F12, WT-F4 with each amount swept, WT-F26, WT-F38 to WT-F41; sync's edges by QS-1's method at r = 2.5, **reported** (QS-3's rule), not judged against QS-1's floors (ADR-390, P3: S0 read 51 to 71 dB on bright tables under either correction, E included); the loop's stability map (WT-31) | ≤ 100 % of 5 % |
| **S5b** | K = 3; the PM matrix, every modulator's taps and fades (WT-62), the two render modes (WT-64), the back-edge clamp (WT-65), the PM buffers | operator stacks: EP, bell, growl, feedback | WT-F14; WT-F4 with depths swept and a modulator switched off; WT-F12 with every entry fuzzed; WT-F3 for the matrix; the back-edge stability map (WT-65) | ≤ 100 % of 15 % |
| **S6** | recipes, the dictionary and its bounds, capture, the library, the slot handoff (§1.6), state, the random slot | captures against live blends | WT-F11, WT-F5's load and mid-render cases, WT-F13 (TSan plant), WT-F22, WT-F30; recipe round trip byte-identical; bounds refused at assignment; fallbacks reported | within 2 % |
| **S6b** | the C++ generators (INTEGRATION §11 step 8): the eight factory tables, "Basic shapes", TABLECASTER's ten engines (§7 I18), each frozen by goldens | factory and rolled tables | WT-F15, WT-F29, WT-41, WT-43, WT-47 for every generator | within 2 % |
| **S6c** | the blade bake (BLADE-BAKE.md): the focused port and its oracle, the render, the frame axis, the mappings, the report | old patches baked into tables (migration A/B) | BB-F1, F2, F3 (a), F5 to F8, F10, F12 | within 2 % |
| **S7** | the seam in `seam/` (§5.8) **at K = 3** (ADR-388 #4a): the table member in `Swarm::render`, the table sets in `HordeEngine`, osc 3 (its block, `mix.osc3`, the five-source arrays), the voice's render order, both render modes and PM buffers, the loop amendment, the retired class | the table member in the real swarm, three oscillators | WT-P4 (every `member` at `base`: all 83 bank presets bit-identical to the pinned commit); the clean-room suite green, by-index tests reseeded by key (WT-F36); WT-F1, WT-F4, WT-F5, WT-F14 (a), WT-F35 in the engine | cells in the engine |
| **S8** | the spectral-blend class in the resolver, `set_corner_tables`, five sources in the shared switch pick, the modulation destinations, the engine's `wt` state and IF-4 version 2, the GUI observations (GN-57 to GN-60), the test plug-in | morphing tables, old patches loading, the human playing it | the morph rows the class touches (WT-F31); MS-29 refusals; RC-9 round trips; IF-4 version 2's migration with BB-F3 (b), BB-F4, BB-F9, BB-F11; rtsan and TSan on the plug-in render | cells, timed |

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
| | `s5a-2-formant` | sync in Hz, softness 1, swept 300 → 3,000 Hz, a chord; then a ratio swept 1 → 3 with softness 0.5 (always-on sync's continuous morph) | Does it replace a formant blade? Does the sweep out of 1× sound continuous? |
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
| S6c | `s6c-1-static-blades` | four static-blade bank presets, old engine then baked | Close enough? |
| | `s6c-2-blade-envelopes` | two blade-envelope presets (Blade pluck, Zap bass), old then baked with the frame route | Does the frame sweep carry the pluck? |
| | `s6c-3-rotation` | a rotating-blade preset, old then baked with the frame LFO | Is 16 positions per turn acceptable? |
| | `s6c-4-jitter` | Trance and Operator jitter, old then baked with the internal modulator | Same jitter? |
| | `s6c-5-sync-formant` | a whole-cycle Sync blade and a fixed-Hz formant, old then baked as sync | Same? |
| | `s6c-6-not-carried` | a cross-mod and a noise-blade preset, old then baked, with the load report on the page | Is the loss acceptable, and is the report clear? |
| S7 | `s7-1-coupling` | coupling −1 → +1 on a table swarm | Does the swarm's physics still read? |
| | `s7-2-drift-inertia` | drift and inertia on a table pad | Same character? |
| | `s7-3-gravity` | gravity on a held table chord | Still settles to just intervals? |
| | `s7-4-in-engine-sweep` | an S2 sweep in the engine | Same as S2's? |
| S8 | `s8-1-blend-four-tables` | a morph across four different tables, Blend | Fluid? |
| | `s8-2-quantum-tables` | the same corners, Quantum: tables flip at their boundaries | Do the flips feel like transitions? |
| | `s8-3-lfo-x-env-frame` | a global LFO on X, an envelope on the frame | Does modulation feel fluid? |
| | `s8-4-human-take` | the human playing the test plug-in | The human's notes |

Every clip is one oscillator's member, never two engines (GUI-1). **The one exemption is a migration A/B** (`s0-1`
and S6c's clips): the old engine and the baked patch played one after the other so the human can judge what an old
patch keeps. It compares a patch before and after its migration, not two engines a patch could choose between, and
is never offered on a page.

**S0's engine-free items** (computed by the lead at S0, never by the builder; methods fixed here). **Done**
(B665, `docs/wavetable/S0-RESULTS.md`; adopted by ADR-390 as P1 to P7):
1. **J2's delayed read** (WT-24): with the table read delayed by τ on every path, show engine-free that a synced
   table member's step and corner residuals sum to a band-limited restart and that DT-3 holds (WT-F42), on Sine →
   saw frame 15 and Vowel frame 7 at r = 2.5 and 3.5, s = 0, at 44.1, 48 and 96 kHz; and whether E's pinned moment
   leaves the residuals' mean at zero (WT-25's term). Method: an engine-free render of the law in double precision
   against an ideal band-limited synced read (additive, every harmonic of the synced cycle below 20 kHz).
2. **WT-F2's design figures** per spacing (re-derive the drafter's −6.84 / −5.30 / −2.54 dB at C6).
3. **The loop's stability map** (WT-31) and **I_back** (WT-65), by B521's method.
4. **BB-41's worst-case recipe size** on the real row keys.
5. **WT-6's read designs** (the drafter's numpy alias figures), re-derived by QS-1's method.

### 5.7 A change in the middle of the build

A spec change ruled while the build runs goes to **the first stage not yet started**, never into a finished one.
A finished stage reopens only by the human's ruling, and then as a row in the next stage.

### 5.8 The end state: how the code reaches the clean-room engine

- **`vendor/engine2/`** is a read-only reference export of the clean-room engine at the pinned commit, for reading
  only. **`seam/`** is a git clone of the same commit, on a branch, where S7 and S8 wire the table member in.
- **S8 delivers three things:** (1) a SPEC-ENGINE round, drafted by the lead from §8 and reviewed by a critic, so
  the engine's spec states what its code does; (2) a patch series against the pinned commit (the table modules, the
  seam at three oscillators, osc 3's block and the five-source arrays, the retired class, the blade bake and IF-4's
  version-2 migration), from `seam/`; (3) a change request for the harness header
  `engine2/tests/harness/horde_engine.h` (`set_table`, the `Tables` event, `set_corner_tables`, `kSourceOsc3 = 4`
  and `kSources = 5`, `OscView osc[3]`, the observation and counter fields, the `retired` class), which belongs to
  the clean-room test side; the lead files it there, with the round, in the same turn.
- **How the patch lands is OPEN** (§7 F3, I23). The clean-room engine's repo stays its owner's: nothing is written there
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

This is the one decision list. Ids are never renumbered; an answered item keeps its id and names the ADR that
answered it.

**Ruled.**
- **ADR-385 (2026-11-07):** a new local repo with one builder, the lead owning gates, measurement and listening; the
  oscillator first, then wired in; every stage ends with CPU, fidelity gates and ≤ 6 clips; **blending different
  tables across the pad is in 1.0** (ADR-377 A3 / ADR-385: the spectral-blend class, §4.2); **MS-29 stands**
  (ADR-384 #9 moot).
- **ADR-385 A1 (2026-11-08):** three oscillators in the build, the count a build parameter from S1; PM any
  oscillator to any, self included, with feedback.
- **ADR-387 (2026-11-08):** the table member replaces the blades for 1.0; three oscillators in the plug-in for 1.0.
- **ADR-388 (2026-11-08):** old patches bake their blades into tables (BLADE-BAKE.md); WT-BUDGET ruled after the
  timed run (ruled by ADR-390, WT-P1); the `retired` class (§2.4); the seam at three oscillators from S7; osc 3 at 0x8000 with
  osc 1's offsets and no blade blocks, `mix.osc3` at 0x0013; the base/table switch test-only through S8; `osc3.on`
  in the five-source shared switch pick.
- **ADR-389 (2026-11-09):** sync always on (× pitch or Hz; ratio 1 is no sync); a restart corrected only when it has
  a step; no filter on a member at 1×; the softness taper with depth fading in by ×1.25; `wt.syncSoft` a row.
- **ADR-390 (2026-11-09):** WT-BUDGET 5 % of one core per oscillator, 15 % for three, under the timed run's
  conditions, with the rebuild crossfade at scheme (iii) (C2, C7); this table's recommendations adopted row by row,
  except where S0's proposals P1 to P7 replace them (J2, C3i, H6; WT-F42, WT-25, WT-37, WT-6); J9 as recommended.
  Not ruled by it: C2a, D2a, and C1, C9 and I6 as their rows say.

**The decisions, in one table.** Each row: its id (never renumbered), the question, the drafter's recommendation
(or the ADR that answered it), and the stage before which it is needed. Duplicates are merged under one id: F3 is
I23, H4 is I10, H8 is I8. The PROPOSED numbers are listed by gate (C3a to C3k), never approved as a block.

| id | question | recommendation | needed by |
|---|---|---|---|
| A1 | Replace the blades, or sit beside them? | **Answered: replace** (ADR-387). | — |
| A2 | One wave menu on the page? | **Answered** (ADR-390): The base waves as one factory table, "Basic shapes" (sine, triangle, saw, square as frames), beside the eight (G1). The base/table switch itself is answered: test-only, gone in 1.0 (ADR-388 #4c). | S6b |
| B2 | Spectral log's shared frame under MS-47's Refuse | **Answered** (ADR-390): Moot if I6 adopts the per-voice frame; otherwise mute per-note routes to `wt.frame` while the law is Spectral log, named on the page (ADR-379 A1 #3). | S4 |
| C1 | Level spacing, and ADR-384 #8 against QS-2 at C6 (WT-F2) | Judge WT-F2 as the design's own figure (within 0.5 dB, as built); hear S3's three glides; then rule the spacing. Half-octave fits the cap with strategy (b); an eighth would meet QS-2 at about 155 MiB for three oscillators. | S3 |
| C2 | WT-BUDGET | **Answered** (ADR-390): **5 % of one core per oscillator, 15 % for three**, at scheme (iii) (C7), under the timed run's conditions (§5.4); the provisional 4 % retired. To be ruled after the 2026-11-09 timed run (ADR-388 #2). | S1 |
| C2a | PF-1's re-anchored figure (cells 2 and 5, whole oscillator) | **Answered in part** (ADR-392): the baseline is about 18.5 % of an M3 core per whole oscillator at 48 kHz, 8 voices, 9 members (WT-BUDGET's 5 % plus the 13.5 % remainder, B668), about 4x Serum 2's measured 4.6 % per oscillator on the same Mac and conditions. **The target is set after the swarm speed-up probe (B670)**, in Serum-ratio terms; PF-2 is still judged at two oscillators, reported at three, ruled again after S5b (ADR-388 #2). S1 opens. | S1; S5b; B670 |
| C3a | QS-1's floors for a table alone: 110, 104, 98, 93 dB at C2, C4, C6, C8 (WT-F1) | **Answered** (ADR-390): Approve: the drafter's engine-free 6-point read reaches the 140 dB cap at C1, C2, C4 (S0 item 5 re-derives it). | S1 |
| C3b | Arithmetic tolerances: 10⁻⁶ of peak (WT-F14 (c), WT-F15, WT-F39), 10⁻⁵ (WT-F10's equivalence and sweeps, WT-F41), 10⁻⁴ (WT-F11), 10⁻³ (WT-F10's corner identity, and its sweep under WT-15 (iii)) | **Answered** (ADR-390): Approve: float32 tables against double-precision references. | S1 to S6 |
| C3c | Loudness across the pad: ±1.0 dB at C3 (WT-F7) | **Answered** (ADR-390): Approve; C2 and C5 reported. | S2 |
| C3d | A glide never steps: 0.5 dB tracking, 0.2 dB hop excess (WT-F17) | **Answered** (ADR-390): Approve. | S3 |
| C3e | Short tapers: no more than 1 dB worse than the corrected hard cut (WT-F40) | **Answered** (ADR-390): Approve with J3's minimum width. | S5a |
| C3f | Polarity: correlations 0.99 and 0.999 (WT-F23) | **Answered** (ADR-390): Approve. | S1 |
| C3g | A static patch: 0 rebuilds in 10 s (WT-F6) | **Answered** (ADR-390): Approve. | S2 |
| C3h | The table gate's band rule: energy under 20 kHz at C3 within 12 dB of full (WT-47) | **Answered** (ADR-390): Approve. | S1 |
| C3i | State bounds: capture depth 8, 64 recipes, 512 bytes each, 3 KiB for a bake recipe, 32 KiB in all (WT-44, BB-41) | **Answered** (ADR-390, P6): approved with **2.5 KiB** for a bake recipe, not 3 KiB (S0 item 4: the worst case 2,081 bytes; BB-F8's plant fires only under 2.5); the `ratios` limit and the number form are open for the next round (§3.2). Approve; S0 item 4 checks the bake's worst case. | S6 |
| C3j | WT-F3's plant: sync's level chosen for fᵢ worsens the alias report by at least 20 dB at r = 4, C5 | **Answered** (ADR-390): Approve. | S5a |
| C3k | The bake's tolerances: BB-F2 10⁻⁵, BB-F3 10⁻⁵ and DT-3's 0.5 dB, BB-F10 1 dB per harmonic, BB-F12 10⁻⁶ | **Answered** (ADR-390): Approve. | S6c, S8 |
| C4 | The memory cap: 24 MiB per oscillator, 72 MiB for three (WT-8) | **Answered** (ADR-390): Approve: it allows strategy (a) at one level per octave (59.7 MiB) and strategy (b) at half-octave (44.3 MiB). | S4 |
| C5 | The rebuild interval: 4 control intervals (WT-14) | **Answered** (ADR-390): Approve. | S2 |
| C6 | Spectral log's frame interpolation (WT-3) | **Answered** (ADR-390): The log domain, the test written to match. | S2 |
| C7 | The rebuild crossfade's scheme (WT-15) | **Answered** (ADR-390): **scheme (iii)**, one interpolated set per interval, still subject to WT-F4's moving-phase probe. The fused read; the once-per-interval set if it passes WT-F4's moving-phase probe. | S2 |
| C8 | The level size cap (WT-6) | **Answered** (ADR-390): 8,192 samples with the 6-point read; 8-point at 4,096 if the timed runs show cache misses. | S1 |
| C9 | Linear's strategy (WT-23) | (a), read-time, at one level per octave; measure (b) at S4. | S4 |
| C10 | Loudness (WT-16): K-weighted at C3, level-matched to the base saw, peak limit 2.6, gain cap +24 dB | **Answered** (ADR-390): Approve. | S2 |
| D1 | The warps: asym and bend; sync softness (the law ruled, ADR-389); mirror out | **Answered** (ADR-390): Approve; hear S5a. | S5a |
| D2 | Reuse `crossMod`, `feedback`, `loopDrive` for table members (C.14, C.16, PA-8 amended) | **Answered** (ADR-390): reuse approved; the default `loopDrive` on tables is D2a. Put to the human only after S0 item 3's stability map; then approve. | S5a |
| D2a | The loop default on tables: S0's map shows the default `loopDrive` 0.3 runs away on Vowel 7 at C6 (table limits 0.62 / 0.34 / 0.14 for Sine → saw 0, 15, Vowel 7). Keep 0.3 with the output bounded, lower the default, or a per-table limit? | To be put to the human. | S5a |
| D3 | No Normalise toggle | **Answered** (ADR-390): Approve. | S2 |
| E1 | Audio-rate `wt.sync`, `wt.syncHz` and the matrix rows (PA-4) | **Answered** (ADR-390): Approve; PF-2 re-measured. | S5a |
| E2 | PM osc 1 → osc 2 | **Superseded** by ADR-385 A1 (any to any). | — |
| E3 | High has no opt-out for table members | **Answered** (ADR-390): Approve. | S5a |
| F1 | The repo name `horde-wavetable`, local only | **Answered** (ADR-390): Approve. | S0 |
| F2 | The clips (§5.6) | **Answered** (ADR-390): The human may swap any before its stage closes. | each stage |
| G1 | The factory set: the bench's eight at 1,024 harmonics, Sine → saw negated, plus "Basic shapes" (A2) | **Answered** (ADR-390): Approve. | S1 |
| G2 | The random generator's slot | **Superseded** by TABLECASTER (ADR-386): I18. | — |
| G3 | Import of wavetable files made elsewhere | **Answered** (ADR-390): After 1.0, as a user table with its own recipe kind. | after 1.0 |
| H1 | Named algorithms (stacks, loops) | **Answered** (ADR-390): As page presets that fill the matrix, never an engine mode. | S5b |
| H2 | The back-edge law (WT-63) | **Answered** (ADR-390): Averaged over two samples, C.14's law. | S5b |
| H3 | Osc 3 in a new patch | **Answered** (ADR-390): Off. | S5b |
| H5 | The fixed render order 3 → 2 → 1 | **Answered** (ADR-390): Approve. | S5b |
| H6 | The back-edge clamp I_back | **Answered** (ADR-390, P5): **I_back = 2.5**, one grid step under S0's 2.75 (equal indices, set by C6; the map stops at C6); a forward depth of 5 to 10 runs the loop away at a back-edge index of 0.75 to 1.0 whatever the clamp, and the output stays bounded (WT-65). 4 until S0 item 3's map, then the map's limit. | S5b |
| H7 | Segment mode by default, interleaving only while a back-edge is live | **Answered** (ADR-390): Approve. | S5b |
| H9 | The real engine's third oscillator | **Answered: in 1.0** (ADR-387); the seam at three from S7 (ADR-388 #4a). | — |
| I1–I5, I7, I14 | INTEGRATION §12.2's seam at K = 3, osc 3's ids, the retired class, old patches, the switch, the budget, five sources | **Answered** by ADR-388 (#4a, #4b, #3, #1 (baked, not flagged), #4c, #2, #4d). | — |
| I6 | Spectral log's frame per voice (the lab's model) | Adopt on S4's timed evidence; WT-22's shared frame otherwise. | S4 |
| I8 (= H8) | The internal modulator's rows: WT-70's set, `wt.modMix` at 0x043, the lab's ranges (×1/16..×32, 0.1..20,000 Hz), decorrelation continuous with an A/B against the lab's switch | **Answered** (ADR-390): Approve. | S5a (and S6c, J4) |
| I9 | The internal modulator is not a modulation source | **Answered** (ADR-390): Approve. | S5a |
| I10 (= H4) | Self-PM is C.14's `feedback`, the matrix's diagonal | **Answered** (ADR-390): Reuse; make `feedback` audio-rate if `s5b-4` shows it too narrow. | S5b |
| I11 | MO-27 for tables: one-hot weights publish the corner's own level sets | **Answered** (ADR-390): Approve. | the round |
| I12 | No waypoints or region locks on table keys in 1.0 | **Answered** (ADR-390): Approve; refused and counted. | the round |
| I13 | Save current sound past the bounds | **Answered** (ADR-390): Refuse the whole save, named. | the round |
| I15 | Aftertouch's default route on all three oscillators | **Answered** (ADR-390): Approve. | the round |
| I16 | The host chunk's ceiling, the tables counted in it | Rule one figure. | S6 |
| I17 | Osc 3 in Random | **Answered** (ADR-390): Never alone; off at low chaos; untouched while off; ADR-374 A2 includes it. | the round |
| I18 | TABLECASTER as recipe generators | **Answered** (ADR-390): ADR-386's five points; engines with goldens; a refused roll re-drawn up to 8 times, then refused and named. | S6b |
| I19 | Random's wavetable roles | **Answered** (ADR-390): The chaos ladder as INTEGRATION §5 proposes; "sync off at low chaos" now reads "ratio 1". | the round |
| I20 | The oscillator preset tier | **Answered** (ADR-390): One oscillator's rows and one table, slot-agnostic. | S6 |
| I21 | The banks | **Answered** (ADR-390): A "Demo (wavetable)" section at S8; the blade demos kept until the retirement round; the factory bank re-voiced; the bank and demos also load baked. | S8 |
| I22 | When the blade code leaves the engine | **Answered** (ADR-390): One retirement round after the factory patch session; the bake's port stays (BB-60). | after S8 |
| I23 (= F3) | How the patch series lands (§5.8) | **Answered** (ADR-390): Merged, the clean-room builder reviewing it, with the round. | S8 |
| I24 | Who regenerates the random roles lab | **Answered** (ADR-390): The lead updates the generator; the GUI session the page. | the round |
| J1 | The bake's frame axis (BB-20) | **Answered** (ADR-390): Approve: static corners single-frame; one moving parameter swept across the frames, its route or LFO moved to `wt.frame` at depth 1. | S6c |
| J2 | **The sync restart's smooth part** (WT-24). ED-1 delays each step's correction by τ (about 2.4 samples) while an undelayed curved smooth part between steps gets none: that is B-51, which round 42's filter E fixed for DT-3 (ADR-289), not for QS-1. A synced table read is a curved smooth part with steps; and before round 42, ED-6 left curved corners uncorrected because a corner residual's tail ends only on a delayed smooth part. So a correction with no delay of the smooth part fails DT-3 at every ratio that is not whole. | **Answered** (ADR-390, P1): the delayed read is the restart correction, proven engine-free by S0 item 1 (DT-3 within 0.17 dB on every case; as band-limited as E within 0.4 dB by QS-1's meter; B-51's control fails by 14.6 to 40.4 dB; r = 1 and 2 exact). **Delay the table read itself by τ** (read at θᵢ − τ·fᵢ/f_int, on every path, at every ratio): exact for a table, not a filter, nothing switches as the ratio crosses an integer, so ADR-389 holds; the step and corner residuals then meet the delayed smooth part ED-1 assumes (the B661 lab already runs every member a sample late). WT-F42 judges the build. Not recommended: E with a PA-5 crossfade at integer crossings (switching ADR-389 removed, combing during the fade, a 33- to 65-tap filter per member-sample). | S0, then S5a |
| J3 | Short tapers (WT-37) | **Answered** (ADR-390): A minimum taper half-width of 2 internal samples; ED-7's correction of the leftover step as the alternative; measured by WT-F40. | S5a |
| J4 | The bake needs the internal modulator for jitter presets (BB-32) | **Answered** (ADR-390): Rule I8 before S6c, so `genV` 1 carries jitter. | S6c |
| J5 | The bake's reference pitch and rate (BB-13) | **Answered** (ADR-390): C4 and 44.1 kHz for every pitch- or rate-dependent law. | S6c |
| J6 | The stages: S6b the C++ generators, S6c the bake | **Answered** (ADR-390): Approve. | S6 |
| J7 | `wt.syncSoft` at control rate | **Answered** (ADR-390): Approve. | S5a |
| J8 | What the bake ships (BB-12, BB-60) | **Answered** (ADR-390): A focused C++ port of the single-member blade path, in its own namespace, sized for one voice; the frozen clean-room engine stays in the build repo as its oracle (BB-F12). Not the whole engine in the plug-in (never ruled; a dependency gate; publishes the clean-room source in a public tree; symbol clashes; 43 MB per bake). | S6c |
| J9 | **Swarm presets:** does "the nearest wavetable sound" (ADR-388 #1) mean the single-member cycle, or must the bake carry per-member texture (blade spreads, cut rules, `frame` = swarm)? | **Answered** (ADR-390): Carry what is cheap: when a corner's frame axis is unused, bake its frames at spread positions −1 to 1 and map the spread to `wt.frameSpread` (BB-20 (a)); report the rest; BB-F11 compares U = 9 renders (reported); `s0-1` lets the human hear the loss before anything is built. | S0 (by ear), S6c |
| J10 | `wt.syncSoft`'s offset | **Answered** (ADR-390): 0x042, with 0x036 (the withdrawn `wt.window`) left empty, so no draft or lab file that named 0x036 can be misread; 0x036 would also be legal, since nothing is ratified. | the round |
| J11 | Osc 3's defaults | **Answered** (ADR-390): Osc 1's, except `seed` 1236 and `on` off. | the round |
| J12 | The migration A/B clips as an exemption from "never two engines" (§5.6) | **Answered** (ADR-390): Approve: they compare one patch before and after its migration. | S0 |

---

## 8. SPEC-ENGINE rows a port amends

INTEGRATION §2.8 lists every SPEC-ENGINE row the 1.0 port touches (three oscillators, the retired blades, the
manifest, the morph and modulation contracts, PF-1 and PF-2). This contract adds, or fixes the detail of:

| row | change |
|---|---|
| 5.1, 5.8 | the `retired` class as §2.4 defines it; the counts of §2.1 |
| 5.2, 5.3 | `mix.osc3` at 0x0013; the `wt.` rows of §2.1 on every oscillator, `wt.syncSoft` at 0x042, offset 0x036 left empty |
| PA-4, PA-5, PA-7, PA-8 | §2.3 |
| ED-1 | a table member's sync restart takes the plain residual, never filter E (WT-24; §7 J2) |
| C.14, C.16 | the loop on table members (WT-31); the PM matrix (WT-61 to WT-66); the means of WT-25 |
| IF-4 | `version` 2, the `wt` member, the migration that bakes blades (BLADE-BAKE.md) |
| IF-9, IF-10, RC-8 | the counters and observations of §1.9, GN-57 to GN-60 included |
| IF-12, MO-13, MO-26, MO-27, Appendix D | `set_corner_tables`, the `Tables` output, the spectral-blend class, five sources (§4.2) |
| RT-1, PF-7 | the table sets in prepare's maxima; the memory statement (§1.5) |
| PF-1, PF-2 | re-anchored as WT-P2 says |
| the structure file | `instrument.table_set`; the member's blocks; osc 3 |

---

## 9. What made the seam harder than expected

1. **The blades read the base wave in closed form everywhere** (§1.7): beside would have been a port, which is
   part of why replace was ruled.
2. **The loop exists only when a blade is on** (`n_blades_ > 0`, §1.1; PA-8's rows say so): reusing it amends
   C.14, C.16 and PA-8.
3. **The voice renders each oscillator over a whole segment:** the PM matrix's back-edges need `Swarm::render`
   split so the swarms can be interleaved per sample, and every modulator must carry its on-fade and PA-5 mix
   explicitly (§1.8; WT-62 to WT-64).
4. **PA-5's copy shares the per-instance table set**, so the law and table changes need the rebuild crossfade, not
   PA-5 (§1.8).
5. **Every member passes through ED-1's filter E**, which delays it about 2.4 samples. With sync always on
   (ADR-389), switching E as the ratio crosses an integer would shift a sounding member; so a table member never
   runs E, and its restart takes a plain residual (WT-24, §7 J2). The warps were made smooth so they never need a
   correction.
6. **The resolver's corner call carries two `ratios` texts** (`resolver.h` line 22): tables need a new call and a
   new output kind.
7. **New rows shift manifest indices** (§2.2): any by-index test must be reseeded by key.
8. **ADR-384 #1, #8 and QS-2 cannot all hold at 44.1 kHz** (§7 C1), and the bench's read fails QS-1 below C6.
9. **The cost bench (B648) found three more:** Spectral log's cached frame lerp is not the law the corner test
   assumed; the rebuild crossfade always runs under motion; the pitch crossfade fades the top octave early.
10. **The clean-room engine is three rounds behind its spec** (62 to 64), and its harness still says 16 members:
    S7 pins a commit and re-reads the seam.
11. **The engine is sized for two oscillators and four sources everywhere** (§10), and `pid::osc(2)` lands on the
    sub's block: osc 3 needs its own block and every per-source array grows.

---

## 10. The real engine's third oscillator (ADR-387, ADR-388 #4)

The plug-in has three oscillators in 1.0 (ADR-387), and the seam is built at three from S7 (ADR-388 #4a). Osc 3's
block is **0x8000**, with osc 1's offsets, no blade blocks and none of the six blade-only rows; `mix.osc3` is voice
offset **0x0013** (ADR-388 #4b; the rows in §2.1; how they were verified free in §2.2); `osc3.on` joins ADR-383's
shared switch pick, of five sources (#4d). The 1.0 round changes:

**SPEC-ENGINE rows.**
- §1, §2 (the glossary's "one of the voice's two swarm oscillators"), §4.1 and §4.2's signal path.
- 5.2: `mix.osc3` (0x0013, dB, −∞..+6, default 0, M, A, PV) and `osc3.on` (default off, §7 H3); 5.3 once more for
  osc 3; 5.8's counts.
- VO-9; RC-6.1 and RC-6.2 (the voice hands PM taps between oscillator sources; the render order and PM buffers
  belong to the voice, keyed by slot); RC-6.4, whose test build's "fifth source" at 0x6000 becomes a sixth.
- MO-26 and D.8: five sources and one shared pick; IF-12's morph levels per source; the resolver's source list.
- IF-3 and MS-8 (osc 3's swarm sources), MS-11, MS-12's destinations, PA-4 (`osc3.pitch`, `osc3.fine`, `mix.osc3`),
  PA-5, PA-8, IF-6's meters, IF-9 and IF-10's observation.
- PF-1, PF-2 (WT-P2), PF-6, RT-1's maxima.
- B.1: kept as a test fixture until the retirement round (§7 I21, I22).
- The GUI pages, which are the GUI session's rows (ADR-336).

**The clean-room engine's code** (at `e72a092`), sized for two oscillators and four sources:
`OscSettings osc[2]` (`voice/voice.h` line 137), `Swarm osc_[2]` and `xf_[2]` (lines 253, 271), the voice's
`for (int o = 0; o < 2; ++o)` loops; the engine's per-source arrays sized 4 (`level_`, `gate_`, `morph_`,
`source_l_`, the morph-level glides; `api/engine.h` lines 89 to 125), `gui_peak_[5]` (line 137) and
`observe_morph_level`'s `source < 4` (line 52); the resolver's `enum Source { kOsc1, kOsc2, kSub, kNoise,
kSources }` (`morph/resolver.h` line 53) and `set_corner`'s two `ratios` texts (line 22); the harness header's
`kSourceOsc1 … kSourceNoise` (line 127) and `OscView osc[2]` (line 369). **Osc 3 is appended as source 4**
(`kSources = 5`), never inserted, so the existing source numbers keep their meaning.

**The block formula.** `pid::osc(o, row)` is 0x1000·(o + 1) (`manifest/manifest.h`), so o = 2 would give 0x3000, the
sub's block: osc 3 is a named block of its own, not the formula's next value.
