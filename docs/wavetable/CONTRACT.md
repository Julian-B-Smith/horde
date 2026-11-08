# The wavetable member: how it wires in, and how the build runs

> **Origin.** Drafted for the horde lead session, 2026-11-07, on ROADMAP B649 and ADR-385 (the human, by poll: a
> new local repo with one builder; the lead owns the gates, the measurements and the listening pages; the
> oscillator first, then wired in; every stage ends with a CPU check, the fidelity gates and a listening batch of at
> most 6 clips). Reworked the same day on an independent critic's review, the human's further rulings (blending
> tables across the pad is in 1.0; MS-29 stands; WT-BUDGET 4 % provisional) and the cost bench's findings (B648).
> Companion to `specs/SPEC-WAVETABLE.md`, which says what the oscillator does. This file says where it plugs in and
> how it gets built without repeating the clean-room build's mistakes. **§7 is the one list of open decisions.**

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
  transformation values; C.14's xinᵢ; under PM, the modulator m from the voice's buffer (§1.8); while sync is on,
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

**Real time.** Nothing on the audio thread allocates, locks, makes a system call or waits (RT-1); a rebuild's
work is bounded by the level sizes (RT-2). The inverse FFT is the builder's own code or a dependency the human
approves (charter).

### 1.6 Tables in: slots, ownership and the handoff

Built at S6; S1 to S5 assign tables only at load, synchronously (WT-18).

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

- **PM render order and buffer.** With `osc1.wt.pm` live, the voice renders osc 2 first and keeps, per member, its
  output after PA-6's fade and entry gain, times osc 2's governing envelope and **its on-fade** (`osc_fade_[1]`),
  in a buffer osc 1 reads (WT-30): 9 members × 512 samples of doubles, 36 KiB at Standard, **72 KiB at High**
  (the internal rate), one shared scratch since voices render one at a time. During osc 2's PA-5 crossfade the
  buffer holds the copy's and the live swarm's member outputs mixed by the crossfade's raised cosine. The order
  changes no other output: the sources sum.
- **Only one direction.** PM both ways needs the two swarms interleaved per sample, a restructuring of
  `Swarm::render`'s loop; `osc2.wt.pm` is reserved.
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

Each oscillator gains ten rows (SPEC-ENGINE 5.1's format), keys `oscN.wt.…`, at the next free offsets of its
block: osc 1's ids 0x1030 to 0x1039, osc 2's 0x2030 to 0x2039.

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
| 0x039 | `wt.pm` | PM from osc 2 | index | 0..10 | 0 | pow | M | A | yes | osc 2 modulates osc 1's read phase (WT-30). **osc 1 only; `osc2.wt.pm` reserved (5.1).** |

**Not host parameters:** WT-19's `member` (engine state, never shown as a choice) and each corner's
`oscN.wt.table` (morph corner state, WT-51).

**Counts.** 20 parameters, one reserved: the manifest grows from 330 to 350 rows, reserved rows from 13 to 14,
host parameters from 317 to 336.

### 2.2 The ids: verification and placement

- **Free.** The clean-room manifest at `e72a092` was compiled on its own (`core/manifest/manifest.cpp` and
  `memory.cpp` against the harness header) into a scratch program that printed every row: **330 rows**, as 5.8
  says. Per oscillator block the used offsets are 0x000 to 0x02F (48 rows), 0x100 to 0x11D (blade 1), 0x200 to
  0x21F (blade 2) and 0x300 (`stream`). 0x030 to 0x0FF is free in both. The other blocks are the voice (0x0001 to
  0x0012), the sub (0x3000), the noise (0x4000), ENV 2 to 4 (0x5000 to 0x5209), and two test-build blocks (0x6000,
  0x7000). Horde's tree has no use of these ids (searched `specs`, `docs`, `engine2`, `tools`). The morph's and the
  modulators' host parameters are appended to the lockfile after the engine's and never reach it (IF-11, MS-40).
- **Placement.** `manifest/manifest.h` (line 10) and 5.8 say a new key takes **the next free offset of its block**,
  so 0x030 onward, not a high range; `stream` stays its block's last id (0x300) and 5.8's wording holds.
- **A consequence to know:** `manifest_param(i)` is in ascending id order, so inserting rows at 0x1030 shifts the
  manifest **index** of every later row (by 10 after osc 1's block, by 20 after osc 2's). Ids, states (keyed by
  key) and renders are unaffected; a test that seeds or iterates parameters by index sees a different order and
  must seed by key, or have its expected values regenerated. Any placement inside the product blocks shifts the
  indices of the blocks after it; only a block above every product row would not, which is what the test builds
  did to avoid it.

### 2.3 SPEC-ENGINE rows these touch

- **PA-4** gains `wt.sync`, `wt.syncHz` and `wt.pm` as audio-rate destinations (an envelope sweeps them sample by
  sample on the engine path, MS-41); PF-2 is then re-measured, as round 62 did (ADR-379 A1).
- **PA-5** gains `wt.syncIn` and `member`. **It does not gain the law or the tables:** those change by WT-15's
  rebuild crossfade (§1.8), and WT-F4 judges them with differing corners.
- **PA-8** gains the live conditions below, and **its rows for `crossMod`, `feedback` and `loopDrive`** change
  (WT-31).
- **C.14** and **C.16** change for the loop on table members (WT-31).

### 2.4 Live and inert (PA-8)

**The new rows** are live only while the oscillator's `member` is `table`. `wt.frameSpread` needs Linear and
`unison` > 1; `wt.sync` needs `wt.syncIn` at × pitch, `wt.syncHz` at Hz, `wt.window` not off; `osc1.wt.pm` needs
osc 2 on.

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
| `wt.sync`, `wt.syncHz`, `wt.pm` | per-voice | engine path from ENV 1 to 4, velocity and per-note LFOs once PA-4 lists them; outside otherwise | |
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
  9 members, the morph moving.
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
| S5 | cell 3 | ≤ 100 % |
| S6 | cells 1 to 3: no audio-path change | within 2 % of S5 |
| S7, S8 | the cells in the clean-room engine; the swarm's own cost reported apart (PF-1 holds the whole oscillator) | ≤ 100 % |

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
| **S5** | sync and window, asym and bend, PM from osc 2 with its fades, the loop on table members, DC removal, High | growls, sync leads, formants | WT-F3, WT-F8 with every transformation, WT-F12, WT-F4 with each amount swept; sync's edges by QS-1's method at r = 2.5; the loop's stability map (WT-31) | ≤ 100 % |
| **S6** | recipes, the dictionary and its bounds, capture, the library, the slot handoff (§1.6), state, the random slot | captures against live blends | WT-F11, WT-F5's load and mid-render cases, WT-F13 (TSan plant); recipe round trip byte-identical; bounds refused at assignment; fallbacks reported | within 2 % |
| **S7** | the seam in `seam/` (§5.8): the table member in `Swarm::render`, the table set in `HordeEngine`, the voice's order and PM buffer, the loop amendment | the table member in the real swarm | WT-P4 (every `member` at `base`: all 83 bank presets bit-identical to the pinned commit); the clean-room suite green, by-index tests reseeded by key (§2.2); WT-F1, WT-F4, WT-F5 in the engine | cells in the engine |
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
| S5 | `s5-1-sync-lead` | sync swept by an envelope, × pitch | Does it replace a Sync blade lead? |
| | `s5-2-formant` | sync in Hz, window 1, swept 300 → 3,000 Hz, a chord | Does it replace a formant blade? |
| | `s5-3-pm-growl` | osc 2 sine → osc 1 Vowel, index by envelope | Growl worth having? |
| | `s5-4-swarm-fm` | feedback and cross-mod on a 9-member table swarm | Usable, or chaos? |
| | `s5-5-warps` | asym and bend on Pulse and Sine → saw | Distinct enough to keep both? |
| | `s5-6-quality` | a high-index PM lead, Standard then High | Is High needed? |
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
- **C4. The memory cap**, 48 MiB for both oscillators' table sets. Recommendation: approve; it allows strategy (a)
  at one level per octave (39.8 MiB) and strategy (b) at half-octave (29.5 MiB).
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
  hear S5.
- **D2. Reuse `crossMod`, `feedback` and `loopDrive`** for table members (amends C.14, C.16, PA-8). Put to the
  human **only after the lead's engine-free stability map** (WT-31). Recommendation: approve then.
- **D3. No Normalise toggle.** Recommendation: approve.

### E. Transformations

- **E1. Audio-rate `wt.sync`, `wt.syncHz`, `wt.pm`** (PA-4). Recommendation: approve; PF-2 re-measured.
- **E2. PM from osc 1 to osc 2** needs the swarms interleaved per sample. Recommendation: after 1.0 unless S5's
  growls show it is needed.
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

## 8. SPEC-ENGINE rows a port amends

Not edited here; the lead drafts the round (§5.8).

| row | change |
|---|---|
| 5.3 | the ten `wt.` rows (§2.1) |
| 5.8 | counts (350 rows, 14 reserved, 336 host) |
| C.14 | xin also pushes a table member's read phase (WT-31) |
| C.16 | the blocker's "loop live" condition includes table members; the table member's means (WT-25) |
| PA-4 | `wt.sync`, `wt.syncHz`, `wt.pm` audio-rate |
| PA-5 | `wt.syncIn`, `member`; not the law or the tables (WT-15) |
| PA-8 | the new rows' live conditions; `crossMod`, `feedback`, `loopDrive` live for table members |
| IF-4 | the engine document's `wt` member |
| IF-9, IF-10 | counters and observations (§1.9) |
| IF-12 | `set_corner_tables`; the `Tables` output |
| MO-13, a new Appendix D row | the spectral-blend class (§4.2) |
| ADR-377's post-1.0 bullet | amended for corner tables (ADR-377 A3 / ADR-385) |
| RT-1, PF-7 | the table sets in prepare's maxima; the memory statement |
| the structure file | `instrument.table_set`; the member's blocks |

---

## 9. What made the seam harder than expected

1. **The blades read the base wave in closed form everywhere** (§1.7): beside is a port, not a switch.
2. **The loop exists only when a blade is on** (`n_blades_ > 0`, §1.1; PA-8's rows say so): reusing it amends
   C.14, C.16 and PA-8.
3. **The voice renders each oscillator over a whole segment:** PM between oscillators works one way only, and its
   modulator must carry osc 2's on-fade and PA-5 mix explicitly (§1.8).
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
