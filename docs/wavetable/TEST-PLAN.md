# The wavetable member: the test plan

> **Origin.** Drafted for the horde lead session, 2026-11-08, on ROADMAP B657 and ADR-387 (the human, by poll:
> "Replace, for 1.0" and "In 1.0" for the third oscillator; and: "think up some tests (export wavetables from Serum
> and record the different compositions on both to test fidelity, etc.)"). It collects the gates that
> `specs/SPEC-WAVETABLE.md` and `docs/wavetable/CONTRACT.md` already define, names the rows that have no test,
> adds the end-to-end plug-in tests the 1.0 integration needs (ADR-387; the inventory is B656's
> `docs/wavetable/INTEGRATION.md`, in progress), and sets out a repeatable fidelity protocol against tables exported
> from Serum.

**Status.** PROPOSED, for the lead and the human. Nothing here has run. Every number this plan adds is PROPOSED and
marked so; every number it repeats keeps the status its source gives it. Section 9 lists the questions for the human.

**How to read it.**
- **Layer-0** is deterministic, needs no model and no human, and blocks: a red Layer-0 gate halts the stage
  (CONTRACT §6.2). **Layer-E** is measured and reported: listening batches, the Serum comparisons, timed CPU runs.
  The plan never moves a Layer-E number into a gate without a ruling.
- **Judged** means a horde gate with a threshold. **Reported** means a number written down and looked at, never a
  pass or fail.
- **RATIFIED** numbers are SPEC-ENGINE's. **PROPOSED** numbers wait for the human (CONTRACT §7 C3). Until then
  they are built and judged as written, as the spec says.
- Every gate has a **must-fail plant** and a **must-read-zero control** (spec section 9). A gate whose plant does
  not fire, or whose control does not read zero, is red (CONTRACT §6.2).
- New ids in this file: **TP-n** (a proposed gate for a spec row that has none), **P-n** (a test piece),
  **T-n** (a test table), **M-n** (a comparison measure), **E2E-n** (an end-to-end plug-in test), **RG-n** (a
  regression control), **Q-n** (an open question). The lead folds adopted TP gates into the spec as WT-F15 onward.

**Outline.**
1. Where the tests run
2. Layer-0: the gates, stage by stage, and the gaps
3. Layer-E: listening batches and the human's notes
4. The fidelity protocol against Serum
5. End-to-end plug-in tests for 1.0
6. Regression controls
7. Cadence and caps
8. Competitor material: the handling rules
9. Open questions
- Appendix A: design note, recording in `tools/au-bench`
- Appendix B: the piece script format

---

## 1. Where the tests run

| place | what runs there | who runs it |
|---|---|---|
| **The build repo** (`horde-wavetable`, local, never pushed; CONTRACT §5.1) | the stage gates S0 to S8 (section 2), on a clean `git archive` export of the stage's commit (CONTRACT §6.3) | the builder runs them while working; **only the lead's run on the clean export closes a stage** |
| **horde's `./verify fast`** | engine-free checks over committed horde material: the comparison meters' own controls, the piece scripts, the competitor-material guard (TP-23) | CI and every session |
| **horde's `./verify full`** | the Layer-E host checks it already runs (auval, host loads), plus the test plug-in's from S8 | the lead, macOS only |
| **A quiet night, 2 to 6 am** | the timed CPU cells (WT-P3), the test plug-in's au-bench cells, the Serum CPU reference | a scheduled batch, no model in the loop (ADR-380 A1) |
| **The human's Ableton** | the end-to-end checks only a real host can show (section 5) | the human, from a short checklist |
| **A folder outside every repo** | everything from Serum: tables, presets, renders, the label-to-name map (section 8) | the lead and the human |

---

## 2. Layer-0: the gates, stage by stage

### 2.1 The gate table

Every gate below is proven at **S0** (its detector plants fire and its control reads zero, engine-free) before the
stage that introduces it opens (CONTRACT §5.6), and re-runs at every later stage's close (CONTRACT §6.1). "D" plants
are inputs the meter must catch; "M" plants are builds with one mechanism disabled (spec section 9).

| gate | what it holds | method | must-fail plant | must-read-zero control | threshold, status | from |
|---|---|---|---|---|---|---|
| **WT-41** | generator accuracy | each frame against a 4× finer analysis | (M) the bench's 512-point DFT | a spectral generator against its own coefficients | 10⁻⁶ of peak, PROPOSED | S1 |
| **WT-43** | goldens per generator and law version | spectra hashes on the reference build, coefficients on any | (M) one generator constant nudged by one ulp | the golden build against itself | hash equal; 10⁻⁶ of peak, PROPOSED | S1 |
| **WT-47** | the table gates | no DC; audible-band energy; no near-silent frame; frames differ; WT-F1 static | (D) one table per failure: DC on frame 3, all energy above 20 kHz at C3, a frame at 10⁻⁷ peak, 16 equal frames | the eight factory tables pass | 12 dB band rule PROPOSED; the rest are rules | S1 |
| **WT-F1** | aliasing, a table alone | QS-1's method exactly | (D) naive saw, 9 to 27 dB; (M) the 2048-sample linear read, about 64 dB at C2 | direct additive synthesis reads the 140 dB cap | 110 / 104 / 98 / 93 dB at C2 / C4 / C6 / C8: QS-1's numbers, RATIFIED for edges, PROPOSED for tables; C1 reported | S1 (static), S3 (C1 to C8 at 44.1, 48, 96 kHz), S7 (in the engine) |
| **WT-F2** | the top of the band | QS-2's method per note, against the engine-free design figure | (D) a saw missing its top four harmonics; (M) the pitch crossfade off | a read exactly at a level's top, no crossfade | **OPEN** (CONTRACT §7 C1): judged as the design figure within 0.5 dB at C6 and every semitone C2 to B5 | S3 |
| **WT-F3** | transformations' aliasing | QS-3's method on each sweep, Standard and High | (M) sync's level chosen for fᵢ, not r·fᵢ: worse by ≥ 20 dB at r = 4, C5 | a table alone at both qualities | **reported** (QS-3); the plant's 20 dB is PROPOSED | S5a, S5b |
| **WT-F4** | clicks | QS-4's detector, the moving-phase probe (four sine tables at 0°, 45°, 90°, 135°) | (D) QS-4's 0.05 steps, 10 of 10; (M) the rebuild crossfade off: a phase step of about 34° per rebuild | the same render with the position still | no event, RATIFIED (QS-4) | S2, then S3 (glides), S4, S5a, S5b, S7 |
| **WT-F5** | determinism | two renders; blocks 1, 7, 128, 4,096 and uneven; the load case; `set_table` mid-render | (M) an unseeded draw in the rebuild; (M) a table applied when a worker finishes | one build twice | bit-identical, RATIFIED (DT-1, DT-2) | S1, every stage; S6 adds load and mid-render |
| **WT-F6** | a static patch never rebuilds | the rebuild counter over 10 s | (M) a rebuild forced every boundary | the counter reads 0 | 0 rebuilds, PROPOSED | S2 |
| **WT-F7** | loudness steady across the pad | OU-2's ungated BS.1770, 3 s at C3, one member, 5 × 5 grid | (M) K-weighting replaced by plain RMS (sine and Vowel corners, 2.42 dB apart) | four identical corners: 0.0 dB | ±1.0 dB of the median, PROPOSED; C2 and C5 reported | S2, S4 (U = 9) |
| **WT-F8** | DC | QS-6's method, every transformation swept | (M) WT-25's removal off, asym 0.8 on a saw | a sine table, no transformation | 0.003·10^(L/20), RATIFIED (QS-6) | S1 (static), S5a (every transformation) |
| **WT-F9** | silence and denormals | QS-7's method | (D) a subnormal in the output; (M) one in a member's state with flush-to-zero off | a released note's tail | exact zero; no subnormal, RATIFIED (QS-7, RT-4) | S1 |
| **WT-F10** | the blend's law | engine-free reference blends against the published set and the output | (M) corners A ↔ B swapped; (M) Spectral log against the other WT-3 rule | four identical corners | 10⁻³ per harmonic (corner identity); 10⁻⁵ of peak (Linear equivalence, sweeps); 10⁻³ for WT-15 (iii): PROPOSED | S2 |
| **WT-F11** | capture | round trip through the library and the state | (M) capture at the wrong frame position | a capture of four identical corners | 10⁻⁴ per frame, PROPOSED; bounds refused at the limit | S6 |
| **WT-F12** | bounded and finite | fuzzed parameters, extreme PM and loop | (M) a NaN planted in a published set | the S7 patches | finite, under QS-8's bound, RATIFIED (QS-8, RT-3) | S5a, S5b (every matrix entry) |
| **WT-F13** | real-time safety | RealtimeSanitizer and ThreadSanitizer, Homebrew LLVM | (M) a `malloc` in the rebuild (rtsan); (M) a write to a staged slot after posting (TSan) | the clean build: no report | no report, RATIFIED (RT-1) | S1 (rtsan), S6 (TSan), S8 (the plug-in) |
| **WT-F14** | the matrix's equivalences | renders compared sample by sample; an engine-free two-oscillator reference | (M) a back-edge read in the same sample; (M) the on-fade left out of m | (a) and (b) themselves | (a), (b) bit-identical; (c) 10⁻⁶ of peak; (d) no event: PROPOSED | S5b; (a) again at S7 and in 1.0 (RG-3) |
| **WT-P4** | cost when off | every `member` at `base`: the bank against the pinned commit | (M) a table set prepared and stepped while no member reads it | the pinned commit against itself | 83 of 83 bit-identical, RC-6.4's method | S4 (harness), S7 (the engine) |
| **WT-P6** | memory | the manifest statement at the build's ρ, N and strategy | (M) one table slot allocated twice | the statement against its own arithmetic (CONTRACT §1.5) | 24 MiB per oscillator, 72 MiB for three, PROPOSED (WT-8) | S4, every later stage |

**CPU at every stage.** The instruction count of the stage's cells sits beside the test count at once, and the
stage's own night's timed run must be within its share before the stage closes (CONTRACT §5.4, §6.1; WT-P1 to
WT-P3). This is a **stage-close condition, not a suite gate**: PF-3 keeps CPU out of the suite's pass or fail, and
the contract adds the stage-close rule on top. Its plant is a build that runs the read loop twice (about twice the
count); its control is a render with no notes (the idle floor). The shares are CONTRACT §5.4's, PROPOSED: S1 35 %,
S2 55 %, S3 65 %, S4 85 %, S5a 100 % of 4 %, S5b 100 % of 12 %, S6 within 2 % of S5b, S7 and S8 the cells in the
engine at K = 2.

### 2.2 The rows with no test, and what this plan proposes

The spec's section 9 covers most rows. These have no gate today. Each proposal has a plant and a control; the
lead writes and proves it at S0 if adopted (Q-9).

| row | the claim nobody tests | proposed gate | must-fail plant | control | from |
|---|---|---|---|---|---|
| WT-2 | stored frames are peak-normalised | **TP-1**: every stored frame's peak at 8,192 points is 1.0 within 10⁻⁶ (PROPOSED) | (M) a generator that skips the scale | the Sine → saw table's frame 0 | S1 |
| WT-4, WT-36 | H·r_hi ≤ c for every level, and the level follows the read rate | **TP-2**: (a) engine-free arithmetic over the level table at 44.1, 48, 96 kHz and both qualities; (b) measured: no harmonic above c in a render at every MIDI key, and at sync r = 4 and warp \|(a, b)\| = 1 | (M) one level one step too bright at its boundary | the level table as specified | S1 (a), S3 (b), S5a (warps) |
| WT-5 | "a glide never steps" in brightness | **TP-3**: along P-3's glide, the 16 to 20 kHz energy tracks the engine-free design curve within 0.5 dB at every 10 ms hop, and no hop-to-hop change exceeds the design's largest by more than 0.2 dB (PROPOSED) | (M) the pitch crossfade off: a step at every level boundary | an ideal additive glide under the same ceiling law | S3 |
| WT-9 | every level exists before it is read | **TP-4**: just after a rebuild, a note started at each key 0 to 127, mid-interval, renders bit-identically to the same note in a render where every level was built a boundary earlier | (M) only the levels in use built | two identical renders | S3 |
| WT-14 | the rebuild cadence | **TP-5**: under a moving morph, rebuilds equal boundaries (344.5 per second at 44.1 kHz, 375 at 48: B648), the same at every block split | (M) R = 1 control interval: four times the count | a static patch: 0 (WT-F6) | S2 |
| WT-16 | the peak limit and the gain cap engage, and are counted | **TP-6**: a blend whose scaled peak would exceed 2.6 peaks at 2.6 within 10⁻⁶ and counts one engagement; saw against reverse saw at 0.5 / 0.5 under Linear meets the +24 dB cap and counts it | (M) the limiter removed: peak above 2.6 | the factory frames at their loudness (engine-free peak 2.27): both counters 0 | S2 |
| WT-17 (DT-6) | after `reset()` the set is as freshly prepared | **TP-7**: render, reset, render equals a fresh prepare with the same state, bit for bit | (M) a crossfade in flight survives the reset | two fresh prepares | S2 |
| WT-18 | no silent first R samples after a load | **TP-8**: the first R samples after a load equal the same samples of a render that loaded a boundary earlier | (M) the first set published at the first boundary instead of before the first sample | two loads | S6 |
| WT-20 | the table member keeps the base wave's polarity | **TP-9**: Sine → saw frame 15 at C3 against an engine-free band-limited rising saw 2θ − 1 correlates ≥ 0.99, frame 0 against −sin 2πθ ≥ 0.999 (PROPOSED) | (M) the bench's coefficients un-negated: about −0.99 | the reference against itself | S1 |
| WT-21 | reads per member-sample | **TP-10**: the read counter per member-sample equals the spec's count in each case (1; 2 or 8; doubled by each running crossfade) | (M) a doubled read | a static patch's count | S2, S4 |
| WT-22 | the frame's scope | **TP-11**: (a) under Linear two voices at frames 0.2 and 0.8 sum to the two voices rendered alone; (b) under Spectral log the set's frame is the newest held voice's at every boundary (IF-10's observation), the last value with none held; frame spread inert | (M) the oldest held voice used: differs at the second note-on | one voice | S4 |
| WT-24 | no ED-1 delay unless sync is on | **TP-12**: without sync the output's cross-correlation peak against an engine-free read at the same phase sits at lag 0; toggling `wt.syncIn` gives no WT-F4 event | (M) E always on: lag about 2.4 samples | sync off, no toggle | S5a, S7 |
| WT-34 | the warp is smooth and monotonic | **TP-13**: engine-free over a grid of (a, b), including \|(a, b)\| > 1: slope in [0.1, 1.9], periodic, monotonic | (M) the pair left unscaled at (1, 1) | (0, 0): the identity | S5a |
| WT-39 | each transformation costs under 1 % when neutral | **TP-14**: instruction count with each transformation at its neutral value against a build without it, PF-6's method | (M) a neutral warp that still evaluates its sines | one build twice | S5a, S5b |
| WT-40 | factory tables come from formulas only | **TP-15**: every factory recipe is `kind: gen` with a listed generator id; no factory path opens a file; the test importer's tables (section 4.6) are refused as factory or as saved state | (M) a factory entry of the test-import kind | the factory set | S1, S6 |
| WT-45 | deleting a library table keeps patches playing | **TP-16**: after the library file is deleted, a patch using it renders bit-identically from its recipe, and the observation marks it "not in the library" | (M) a loader reading the spectra from the library instead of the recipe | no delete | S6 |
| WT-53 | the spectral-blend class's rules | **TP-17**: corners holding one recipe pool into one weight; a corner whose oscillator is off weighs 0; Quantum is one-hot and latches; corner A is Exempt | (M) no pooling: Count Two picks differently | a patch with four different tables | S8 |
| WT-62 | a modulator's PA-5 crossfade and unison change reach PM smoothly | **TP-18**: PM from osc j through osc j's PA-5 crossfade and through a unison change: no WT-F4 event | (M) m taken from the live swarm only | no change during the render | S5b |
| WT-65 | the back-edge clamp engages and is counted | **TP-19**: a back-edge depth of 10 meets I_back, counted and observable | (M) the clamp removed: the counter stays 0 while the loop leaves the map | a depth under I_back: 0 | S5b |
| WT-70 | the internal modulator's noise follows DT-4 | **TP-20**: same note counter, same noise; adding a voice changes no other voice's draws | (M) one shared stream | one voice | S5a (if the lab is ruled) |
| WT-60, WT-69 | the manifest at K = 2 and K = 3 | **TP-21**: rows naming a missing oscillator are reserved, never live, never host parameters; counts as CONTRACT §2.1 (in 1.0, as B656 restates them) | (M) a reserved row exposed as a host parameter | the manifest's own counts | S7, S8, 1.0 |
| §2.2 | by-index tests break when rows are inserted | **TP-22**: no test seeds or iterates parameters by index (a scan of the test sources); every expected value regenerated after the insert is regenerated by ruling, not by habit | (D) a planted by-index loop in a scratch test | the suite as it stands | S7 |
| section 8 | competitor material never enters a tracked file | **TP-23**: horde's `./verify fast` refuses a tracked file with an audio, preset or exported-table extension (`.wav .aif .aiff .flac .aupreset .adv .fxp` and the synths' own preset suffixes); no tracked file has one today | (D) a planted `.wav` in a scratch tree | the tree as it stands | now (horde) |

Rows WT-19 ("never a choice on the page"), WT-35 and WT-37 are design statements with no number. WT-19's half is
a GUI check (E2E-11). WT-1, WT-3, WT-6 to WT-8, WT-10 to WT-13, WT-15, WT-23, WT-25, WT-26, WT-31 to WT-33, WT-38,
WT-42, WT-44, WT-46, WT-50 to WT-52, WT-54 to WT-56, WT-61, WT-63, WT-64, WT-66 to WT-68 are covered by the table in
2.1, by S8's morph and modulation rows, or by the stability maps the lead draws engine-free (WT-31, WT-65).

---

## 3. Layer-E: listening batches and the human's notes

**The batches** are CONTRACT §5.6's: at most six clips per stage, rendered by the lead from the clean export, 44.1
kHz, 24-bit, peak-normalised to −1 dBFS with the gain printed beside each clip. The clip list is CONTRACT §5.6's
table (S1 `s1-1-sine-saw-scan` to S8 `s8-4-human-take`); the human may swap any before the stage closes (§7 F2).
Every clip is one oscillator's member, never two engines (GUI-1).

**How the notes come back.**
1. The lead publishes the batch as one page: each clip with its question, a notes field, and a three-way answer
   (keep, change, unsure). The page saves the notes where the lead can read them back (the clean-room progress
   page's pattern), so no note lives only in a chat.
2. Before the next stage starts, each note becomes a row in the build repo's STAGES.md: built into the next stage,
   ruled out with a reason the human accepts, or deferred to a named stage (CONTRACT §5.5). A note that asks for a
   spec change goes to the first unstarted stage (CONTRACT §5.7).
3. A note that names a fault a meter should have caught opens a gate row: the lead writes the missing gate and its
   plant, so the next stage cannot pass the same fault silently (the "detector shares the assumption" lesson,
   LIBRARY L0032).

**The Serum A/B clips** (section 4) join the same cap: they are never a seventh clip. When a stage has Serum
material, the lead trades one of CONTRACT's clips for it and says which. Their page stays **local** (a file opened
from the out-of-tree folder), never published, because publishing the page would publish competitor audio
(section 8).

**What else is Layer-E.** WT-F3 and QS-3's aliasing reports; WT-F1 at C1; WT-F7 at C2 and C5; the timed CPU runs;
every Serum comparison except the importer's own check (section 4.7).

---

## 4. The fidelity protocol against Serum

### 4.1 What it is for

The human's idea: export wavetables from Serum, play the same tables and the same test pieces through both synths,
and compare. It answers three questions horde's own gates cannot: does horde render a table someone else made as
faithfully as the synth that made it; where does horde's design (one level per octave, the brightness ceiling, the
loudness rule, the swarm) sound different, and is that a choice or a fault; and what does a wavetable synth the
human knows cost on this Mac for the same chord. It is **comparative, not imitative** (the competitor-reference
convention, ROADMAP, ratified 2026-08-01): horde's sound is its own (ADR-337 A1), so Serum is a reference point,
never a target horde must equal (section 4.8 says why, measure by measure).

### 4.2 The tables

Two sources, so that half the inputs are ours and can be reproduced from the repo.

| label | what | source | committed? |
|---|---|---|---|
| T-H1 | Sine → saw | horde factory (WT-40), exported as 16 frames × 2,048 samples | the recipe is; the WAV is generated on demand |
| T-H2 | Vowel ah → ee | horde factory | same |
| T-H3 | Hard sync 1 → 6 | horde factory | same |
| T-H4 | Moving comb (dense) | horde factory | same |
| T-S1 | a basic-shapes table (sine, triangle, saw, square across its frames) | the human's export from Serum | **never** |
| T-S2 | a pulse-width style table | Serum export | never |
| T-S3 | a vocal or formant style table | Serum export | never |
| T-S4 | a single bright saw frame | Serum export | never |
| T-S5 | a dense, spectrally rich table, strong harmonics to the top | Serum export | never |
| T-S6 | a table whose frames change abruptly from one to the next | Serum export | never |

- **Frame size 2,048 samples**, so a frame carries harmonics 1 to 1,024, the same count as WT-1.
- **Frame count.** horde plays 16 frames (WT-1). The human exports 16-frame tables where Serum's editor allows it;
  otherwise the importer takes frames at round(f·(N − 1)/15) for f = 0 to 15, records which, and the frame
  comparisons are judged only at those 16 frames (Q-6).
- The T-H tables go the other way: the lead exports them (WT-7's 2,048-sample form) into the out-of-tree folder,
  and the human loads them into Serum. This direction needs no importer on horde's side and makes the inputs of
  half the comparisons reproducible by anyone with the repo.
- The human chooses the Serum tables. Which table a T-S label names is written only in the out-of-tree folder.

### 4.3 The patches

One base patch per synth per table, built to the same rules.

| setting | both synths |
|---|---|
| oscillators | one; the test table; every other oscillator, sub and noise off |
| filter, FX | off |
| amplitude envelope | attack at its minimum (stated), sustain full, release 20 ms (P-6: 10 ms) |
| velocity | velocity to amplitude off; every note at 108/127 (0.85, PF-3's) |
| unison | 1 unless the piece says otherwise; detune 0 |
| start phase | fixed, randomness off |
| warps | off (neutral) unless the piece is P-7 |
| glide | off unless the piece is P-3 |
| polyphony | at least 8 voices (P-2, P-6) |
| pitch | A4 = 440 Hz, equal temperament, bend range unused |
| quality | **Serum:** rendered at its default oscillator quality and at its highest (Q-12). **horde:** Standard; High as a second render |

**horde's side** in addition: `member = table`, the test table in all four corners of osc 1 (so the morph does
nothing), law Linear, frame per the piece, transformations neutral, osc 2 and osc 3 off.

**Warp variants** (P-7 only), each a separate Serum preset and horde patch, where Serum's warp has a horde analogue:

| variant | horde | Serum | matched by |
|---|---|---|---|
| sync | `wt.syncIn` × pitch, `wt.sync` 1 → 4, window 0 | its sync warp | the measured sync ratio at each hold |
| bend | `wt.bend` 0 → 1 | its bend warp | the measured spectral centroid at each hold |
| asymmetry / PWM | `wt.asym` 0 → 1 | its asymmetry or PWM warp | the measured even-to-odd harmonic balance at each hold |
| FM from the second oscillator | PM osc 2 → osc 1, osc 2 a sine at ratio 1, index 0 → 4 | its FM-from-B warp, B a sine at ratio 1 | the measured first-sideband ratio J₁/J₀ at each hold |

The knob values differ between the synths; the holds are matched by measurement, not by knob position, and the
match is reported with the result.

### 4.4 The test pieces

Every piece is a script in Appendix B's format, committed under `tools/wt-fidelity/pieces/` (ours: notes and
times only, nothing of Serum's). Times are in seconds so one script serves 44.1 and 48 kHz. Every render is stereo
float32 at **44.1 kHz** (primary) and **48 kHz** (secondary), 128-sample blocks except where stated.

| piece | notes | lengths | automation | analysis window |
|---|---|---|---|---|
| **P-1 keys** | C1 to C8: keys 24, 36, 48, 60, 72, 84, 96, 108, one at a time | note i on at 3·i s, held 2.5 s; 24 s in all | none; run at frames 0, 7/15 and 1 | [on + 0.5 s, on + 0.5 s + 65,536 samples] per note |
| **P-2 chord** | PF-3's chord: keys 48, 51, 54, 57, 60, 63, 66, 69 | all on at 0, held 3.0 s; record to 3.5 s | none; frame 0.5; U = 1 (P-2a) and U = 7 (P-2b) | [1.0, 3.0] s |
| **P-3 glide** | key 36 on at 0; key 96 on at 1.000 s, key 36 off at 1.001 s (legato); key 36 on at 11.000 s, key 96 off at 11.001 s; key 36 off at 21.0 s; record to 21.5 s | mono, legato, glide time 8 s, linear in pitch, fixed-time law | none; frame 1 (T-H1: a saw) | tracked f₀, 10 ms hops, up and down analysed apart |
| **P-4a frames** | key 48 (C3), 16 notes | note i on at 2·i s, held 1.5 s; 32 s | frame set to i/15 at 2·i − 0.25 s, between notes | [on + 0.25, on + 1.25] s |
| **P-4b sweep** | key 48, one note | on at 0, held 10 s; record to 10.5 s | frame 0 until 1 s, linear 0 → 1 from 1 s to 9 s, held to 10 s; **32-sample blocks**, one value per block | 50 ms frames over [1, 9] s |
| **P-5 unison** | key 48, one note; then P-2's chord | held 4 s | U = 1, 3, 7, 9 in turn; detune set so the outer members sit at ±20 cents (PROPOSED), calibrated by measurement (Q-8); width full | [1.0, 3.5] s |
| **P-6 staccato** | 40 notes: note n on at 0.1·n s, off at 0.1·n + 0.05 s, key [48, 55, 60, 67][n mod 4] | 4.5 s in all | none; U = 1 and U = 7 | per note, and the tail after 4.05 s |
| **P-7 warps** | key 48, one note | on at 0, held 9 s | the warp amount held at 0, 0.33, 0.67 and 1.0 for 2 s each from 1 s, then the matched holds of section 4.3 | the middle 1 s of each hold |

If either synth cannot glide 60 semitones in one move, P-3 runs as two 30-semitone glides (36 → 66, then 66 → 96)
with the same timing per half, and the analysis joins them at f₀ (Q-7). Because P-3 is analysed against the tracked
f₀, the two synths' glide curves need not match.

**P-8 morph** is horde's only and is not compared with Serum: four different corner tables (T-H1 to T-H4), P-2's
chord held 10 s, X and Y on a 0.2 Hz circle; and the S2 diagonal A → D in 8 s. It serves E2E-3 and RG-7.

### 4.5 How each is rendered

| synth | vehicle | when |
|---|---|---|
| **Serum** | `tools/au-bench` with the recording mode of Appendix A, from the human's `.aupreset` or Live `.adv`; the host automates the frame and warp through a local binding file | any time the human has exported the material |
| **horde, before the build** | the JS lab's offline render (`docs/wavetable/lab/wavetable-lab.html`: an OfflineAudioContext running the lab's own worklet, as B652 rendered it), **T-H tables only** (the lab has no importer and needs none) | now: it proves the pipeline and the meters before S1 (Q-15); its numbers are labelled "lab", never the build's |
| **horde, S1 to S6** | the build harness's offline render (CONTRACT §5.3), which must read Appendix B's scripts (an item for the S1 brief), with the test importer (4.6) | from the stage that builds what the piece needs (4.9) |
| **horde, S7** | the clean-room engine headless, at the pinned commit with the seam | S7 |
| **horde, S8 and 1.0** | the test plug-in, **through the same au-bench recording mode as Serum**: the same host, the same event placement, the same block loop | S8 onward; the cleanest comparison, and the one the 1.0 results quote |

Each Serum render is made twice, on fresh instances after one primed render (auhost found the first instance in a
process differs: its header, 2026-10-01). The two must agree in magnitude (every analysed harmonic within 0.1 dB);
if not, the comparison is reported as unstable and not used. Serum's version is read from the component and written
with every result.

### 4.6 The import path: a test-only importer

horde has no wavetable importer: CONTRACT §7 G3 puts import after 1.0, and WT-40 forbids importing into the factory
set. The T-S tables need one. **Recommendation: a test-only importer, never in the shipped plug-in** (Q-2).

- **What it does.** Reads a WAV of whole 2,048-sample frames (a frame-size argument; a file whose length is not a
  whole number of frames is refused; a frame-size marker chunk, if present, is checked against the argument); takes
  a real FFT per frame; writes harmonics 1 to 1,024 as WT-1's complex coefficients (harmonic 1,024 is the frame's
  Nyquist bin, cosine only); removes DC (WT-1); scales each frame to WT-2's peak; picks 16 frames (4.2); and runs
  WT-47's table gates, reporting any failure by name instead of fixing it.
- **Where it lives.** The build harness from S1, and a test-only hook in the test plug-in's build from S8 (a
  compile-time switch, absent from the shipped build). It hands spectra straight to the table set; it makes **no
  recipe**, so nothing imported can be saved into a patch, a preset or the library (TP-15 refuses it).
- **Its own gate (judged, Layer-0).** The importer's coefficients equal the WAV's own DFT after the same DC removal
  and scaling, within 10⁻⁶ of peak, complex (PROPOSED). Plants: (D) the imaginary parts negated (a time-reversed
  table, which a magnitude-only check would miss); (D) a frame offset by one sample. Control: a T-H table exported
  and re-imported reproduces its recipe's spectra within float32's rounding. The CI copy runs on T-H tables only;
  the T-S run is local.
- **After 1.0.** If G3's importer is ruled, this code is its seed, under its own recipe kind, with this gate.

### 4.7 What is compared, and how

| measure | what | method | horde: judged or reported | Serum |
|---|---|---|---|---|
| **M-1a import** | the importer's spectra against the WAV | 4.6's gate | **judged** (it is horde's code) | — |
| **M-1b table fidelity** | each frame's harmonic magnitudes in the render (P-4a at C3 and C1) against the table's own spectrum, times the expected constant gain (WT-16) and the design's level truncation and pitch-crossfade weight | Kaiser-windowed spectrum (β 19, ±16-bin harmonic exclusion: `tools/measure_alias.cpp`'s method), harmonics within 60 dB of the strongest | **judged**: within 0.1 dB per harmonic, and the retained harmonic count equals the design's H (PROPOSED) | reported: the same deviation, its retained count against f₀ (its own top-of-band strategy), and the per-harmonic horde − Serum difference |
| **M-2 aliasing floor** | alias power below harmonic power at C1 to C8 (P-1), one member, a static table | horde in the harness: **QS-1's exact method** (odd whole cycles in 65,536 samples, rectangular window). Both synths through au-bench: MIDI pitches cannot place whole cycles, so the Kaiser method of M-1b, applied identically to both, with f₀ tracked from the render, not assumed | **judged** by WT-F1's floors (horde's own); the Kaiser figure is calibrated against the exact one on horde's harness render (they must agree within 1 dB at C2 to C8, else the Kaiser figure is not used) | reported beside horde's; **a note where Serum is cleaner than horde opens a ROADMAP row** (Q-4) |
| **M-3 level across frames** | BS.1770 ungated loudness of each P-4a note; the P-4b sweep in 400 ms windows | OU-2's measure | **judged** at C3 within ±1.0 dB of the median (WT-F7's number, PROPOSED); C2, C5 reported | reported: shows what WT-16's loudness rule changes |
| **M-4 glide smoothness** | top-band (16 to 20 kHz) energy against the ideal band-limited table at the tracked f₀, per 10 ms hop, over P-3 | f₀ tracked per hop; the band energy against an engine-free ideal at that f₀ | **judged** by TP-3 (the design curve within 0.5 dB, no step beyond the design's) | reported: its ripple per octave, its largest hop-to-hop step, any hysteresis between up and down |
| **M-5 unison and stereo** | member spread (peaks around the fundamental in a 4 s FFT on a sine table at U = 3 and 7), loudness against U = 1, side-to-mid ratio and L/R correlation per 100 ms (the width measure the gauntlet's stage 3 specifies, not yet built), mono fold loss (QS-10) | P-5 | **reported**: horde's spread follows its swarm (coupling, drift, the spread law), which is the instrument's character, not a defect | reported |
| **M-6 CPU** | % of one core on PF-3's chord, U = 1 and U = 7 (Serum) against WT-P2's cells | `tools/au-bench`, PF-3's conditions, in the night window | **judged against WT-BUDGET** (horde's own, WT-P1), not against Serum | reported: the reference reading ADR-380 A1 #5 asks for; WT-BUDGET moves only by ruling (Q-13) |
| **M-7 onsets** | per-note level of the first 10 ms, its spread over P-6's 40 notes; QS-4's detector over the run; the tail | P-6 | **judged**: QS-7 (exact zero after the last release) and DT-1 (two renders bit-identical); the onset spread and click report are reported | reported |
| **M-8 warps** | the brightness range each warp reaches (centroid at each hold), and its aliasing by QS-3's method (renders at 4× and 8× the rate) | P-7 | **reported** (WT-F3 and QS-3 are reported); horde's own sync gate is S5a's QS-1 at r = 2.5 | reported; if Serum cannot render at 4× the rate, its aliasing is not measured (never measured another way instead: the gauntlet's rule) |
| **M-9 determinism** | two renders compared sample by sample | every piece | **judged**: bit-identical (DT-1) | reported (the gauntlet's stage-2 practice: measured, never assumed) |
| **M-10 DC and silence** | QS-6 and QS-7 on every piece | the ratified methods | **judged** | reported |

**Every meter is calibrated before it is trusted** (LIBRARY L0016, L0017): each has a must-read-zero input (a
render against itself; an ideal additive tone) and a must-read-high input (a naive saw for M-2; a hard level switch
for M-4; a mono signal against independent noise for M-5's width), run on **both** synths' signal classes, since a
meter calibrated on horde's renders can miss what Serum's do. The meters live in `tools/wt-fidelity/` (ours,
committed); their controls on synthetic signals join `./verify fast` under ADR-180 §1.

### 4.8 Why horde's gates never hang on Serum's numbers

1. **A gate must be reproducible from the repo.** Serum's renders can never be committed (section 8), so a gate
   that needs them could not run in CI, on a second machine, or for anyone without Serum.
2. **A gate pegged to another product moves when that product updates.** A threshold that shifts with someone
   else's release is a gate that can be weakened without anyone deciding to (charter: gates are never weakened
   without a recorded human decision).
3. **horde's sound is its own** (ADR-337 A1): one level per octave, the brightness ceiling (ADR-384 #8), loudness
   matched to the base saw (WT-16) and the swarm are choices. Where Serum differs, the difference is a question for
   the human, answered by listening, not a defect by definition.
4. **What Serum's numbers do instead:** a Serum figure that beats horde's opens a ROADMAP row (a finding, with the
   numbers); a Serum figure that informs an open ruling (C1's level spacing, WT-BUDGET, ADR-380's aliasing ceiling)
   goes to the human with that ruling. Comparative framing only: "close the gap", never "match".

The one judged comparison that uses a Serum file is M-1a, and it judges horde's importer against the file's own
DFT, never against Serum's rendering.

### 4.9 When each comparison becomes meaningful

| stage | comparisons | why then |
|---|---|---|
| now (the lab) | M-1b, M-2, M-3, M-4 on T-H tables, labelled "lab" | proves the pieces, the recording mode and the meters before any build number exists |
| S1 | M-1a, M-1b, M-2 at a static pitch, M-9, M-10 | the read and the levels exist |
| S2 | M-3 | WT-16's loudness |
| S3 | M-2 at C1 to C8, both rates; M-4 | levels across pitch, the pitch crossfade; feeds C1's spacing ruling |
| S4 | M-5, M-7, P-2's sound | members, voices |
| S5a | M-8 sync, bend, asymmetry | the transformations |
| S5b | M-8 FM from the second oscillator | the PM matrix |
| S8 and 1.0 | everything again through au-bench, both synths in one host; M-6 | the test plug-in: the figures the 1.0 report quotes |

### 4.10 Step by step

**The human, once:**
1. Choose six Serum tables for T-S1 to T-S6 (4.2). Export each as a WAV of 2,048-sample frames, 16 frames where the
   editor allows it, into the out-of-tree folder's `tables/`, named by label only. Write which table each label is
   in a note in that folder.
2. Load the four T-H WAVs the lead put in that folder into Serum, and save them as Serum tables for the patches.
3. Build the base patch (4.3) once in Serum inside Live (Serum loaded **as an Audio Unit**) and save it as an
   `.aupreset` or a Live `.adv` per table, in `patches/`, named `T-xx__base`. Build the four warp variants on T-H1
   and T-S1 the same way (`T-xx__sync`, `__bend`, `__asym`, `__fm`). Note Serum's default and highest oscillator
   quality setting in the folder's note.
4. Approve the session in which the lead loads Serum headless (the third-party latch: `--allow-third-party` is used
   only in a session the human approved; the gauntlet's oversight point 2).

**The lead, each time it runs:**
5. Read Serum's parameter list locally and write the binding file (`bindings/serum.json`: our lane names, frame,
   warp, FM amount, detune, to Serum's parameter ids) in the out-of-tree folder. It is never committed.
6. Calibrate P-5's detune by renders: a one-dimensional bisection on the bound detune parameter until the outer
   members of U = 3 sit at ±20 cents, budget counted in renders (the gauntlet's stage-4 rule); horde's detune set to
   the same measured spread.
7. Record: for each table × piece × rate, Serum through au-bench's recording mode, twice; horde through the stage's
   vehicle (4.5), twice. Under the shared-machine rule: one job at a time, at most 4 threads, `nice`, load checked
   first (CONTRACT §6.5). Recording is untimed, so it may run by day; the CPU cells (M-6) run only in the night
   window.
8. Analyse with `tools/wt-fidelity/` (meters calibrated first, 4.7), and write the numbers to
   `docs/wavetable/fidelity/<date>-<stage>.md`: tables keyed by T, P and M labels, the Serum version, both synths'
   settings in our words, the stability check of 4.5, and curves drawn from the numbers. No spectrogram, waveform
   image or screenshot of a Serum render is committed.
9. Turn every Serum-beats-horde finding into a ROADMAP row, and every number an open ruling needs into that
   ruling's evidence.

**The human, at a stage's close:**
10. Listen to the A/B clips the lead traded into the stage's batch (section 3), on the local page; notes come back
    as in section 3.

---

## 5. End-to-end plug-in tests for 1.0

ADR-387 puts the wavetable member and three oscillators in the 1.0 plug-in. B656's inventory
(`docs/wavetable/INTEGRATION.md`, in progress) decides the ids, the mixer channel, MO-26's pick over five sources,
the destinations, state and the migration; where it has not decided, the test below is written for each option and
the ruled one is kept. **Headless** means the clean-room suite or the test plug-in through au-bench or auhost,
Layer-0. **In Live** means the human's checklist, Layer-E.

| test | what | headless (judged) | in Live (the human) |
|---|---|---|---|
| **E2E-1 three oscillators** | each oscillator alone renders as its single-oscillator reference; osc 3's mixer channel; MO-26's shared pick over five sources (ADR-383) with osc 3 in it; an off source costs nothing | each oscillator soloed by the others' switches, bit-identical to its reference; PF-6's method for osc 3 off; the pick's suite cases with a fifth source | osc 3 on and off while playing: no click, no dropout |
| **E2E-2 the PM matrix** | all six off-diagonal entries and the three diagonals; audio-rate depths (PA-4) | each forward entry alone, sine tables, index ≤ 2: sidebands at the carrier ± k × the modulator within 0.5 dB of the Bessel prediction Jₖ(I) (engine-free, PROPOSED); back-edges reported; WT-F14 through the plug-in's process call; a depth automated sample by sample: no WT-F4 event | an EP and a bell from S5b's clips, rebuilt in the plug-in and played |
| **E2E-3 morph across corner tables** | four different tables; X and Y by host automation and by a global LFO; Blend, Stepped, Quantum | P-8 through the plug-in with automation events: WT-F4, WT-F7, WT-F10's sweep, MS-29's refusal of per-note routes to X and Y | a clip envelope on X and Y, then the LFO; listen for dropouts (the B642 lesson) and watch Live's CPU meter |
| **E2E-4 state** | save and load with recipes; the size bound; the exported preset; fallbacks | (a) save, load into a fresh instance, **render**: bit-identical (a state oracle must render, LIBRARY L0063); (b) the chunk at the worst case (64 recipes, depth 8) within its bound (CONTRACT §3.2: 32 KiB of recipes) and refused at assignment past it (WT-44); (c) an exported preset with embedded spectra at most 1.4 MiB, loading with the library deleted; (d) an unknown generator version falls back and is reported by name | save the Live set with a worst-case patch, quit, reopen: the same sound; Live's log shows no state error (the large-chunk silent-save lesson) |
| **E2E-5 migration of old patches** | patches with blade settings load (ADR-387 #1: mapped or flagged, B656 proposes) | every bank preset and the human's own presets (B312's local output, never committed) load; every flagged patch is named in the load report; blade rows keep their saved values through a re-save (ids frozen, RC-9.1); a base-wave-only patch moved to a table keeps harmonics 1 to 16 within 0.5 dB of its pre-migration render, DT-3's method (judged, PROPOSED, Q-10) | a capped batch of the most-changed patches, before against after (Blind-Spot Armor: every default change carries an ADR and a migration) |
| **E2E-6 the banks** | the factory and demo banks | every preset loads with no refusal and no fallback; QS-5 headroom, QS-6 DC, QS-7 silence, DT-1 determinism; goldens frozen once the factory patch session re-voices the bank | the human's own pass at the factory patch session |
| **E2E-7 randomization** | Tablecaster's rolls into corners through the quality gate (ADR-386 #3) | 1,000 rolls (seeds 1 to 1,000 across the ten generator engines): each passes WT-47 and reaches a corner, or is refused and named, never both; the same seed gives the same recipe id and the same spectra hash; the 16-frame resample is deterministic (ADR-386 #1); the roll's seed comes from the patch's purpose-keyed stream (`wt/random/…`, WT-46); plant: a generator variant with DC on one frame is refused; control: the factory set passes. Rolled patches' CPU: none above 1.5 × the bank median per member-sample without being reported (PF-4's rule) | rolling all four corners while a chord holds: loudness steady (WT-F7 reported), no click |
| **E2E-8 CPU in the host** | the test plug-in against WT-BUDGET and PF-1 | au-bench on WT-P2's cells at K = 3 in the night window: within 12 % (WT-P1, PROVISIONAL); **its worst block under 100 % of the deadline before the human loads it in Live** (a core at 100 % has crashed the human's Live) | Live's CPU meter on the P-2 chord at U = 9, three oscillators, the morph moving; the reading written beside au-bench's |
| **E2E-9 determinism** | an offline bounce twice | through au-bench or auhost: two renders bit-identical; block sizes 32, 128 and 512 bit-identical (DT-2) | export the same clip twice from Live (dither off, normalise off), null them; a mismatch in Live with headless identity points at the host's settings first (Q-11) |
| **E2E-10 real-time safety** | the plug-in's audio path while tables are assigned during playback | rtsan and TSan through Homebrew LLVM on the plug-in render, with a planted allocation and a planted unsynchronised write (WT-F13) | — |
| **E2E-11 one engine on the page** | no page, preset, sample name or lab offers wavetable and blades, or swarm and SCALPEL, as engines to choose (GUI-1, WT-19) | — | the GUI session's checklist at its page reviews |
| **E2E-12 the host's view** | the new rows reach the host; reserved rows do not | the lockfile appended per IF-11; TP-21 | Live's Configure lists the rows; automation lanes for frame and the matrix depths |
| **E2E-13 build hygiene** | the bundle the human loads is valid | auval succeeds on the test plug-in; `codesign --verify --deep` on the installed copy after the post-build re-sign | the plug-in appears in Live's browser |

---

## 6. Regression controls

What must not change through the integration. Each runs on the **full scenario stream** (every bank preset and every
WT-P2 cell, both rates, blocks 1, 7, 128 and 4,096): a control run on a subset under-reports (LIBRARY L0068).

| control | must stay | how | plant | lifetime |
|---|---|---|---|---|
| **RG-1 everything but the oscillators** | bit-identical: every bank preset with both swarm oscillators off (sub, noise, filters, FX, envelopes, modulation sounding), before and after | rendered at the pinned pre-integration commit and at every integration commit (the tier-0 practice) | (M) the noise oscillator's stream purpose renamed: must differ | until 1.0 |
| **RG-2 the base path** | WT-P4: with every `member` at `base`, all 83 bank presets bit-identical to the pinned commit | as WT-P4 | (M) a table set stepped while no member reads it | until the migration ADR retires the base path; that ADR names this control's retirement, so no gate lapses silently (LIBRARY L0065) |
| **RG-3 K = 3, osc 3 off, equals K = 2** | WT-F14 (a), in the real engine: with osc 3 off and every matrix entry naming it at 0, the three-oscillator plug-in renders every bank preset bit-identically to the two-oscillator one | the 1.0 build against the pre-osc-3 build | (M) osc 3 draws once from a shared stream (a DT-4 breach); (M) osc 3's on-fade not zero | permanent |
| **RG-4 the render modes** | WT-F14 (b): every back-edge at 0 renders bit-identically in segment and interleaved mode | as WT-F14 | (M) the order of member arithmetic changed in one mode | permanent |
| **RG-5 state compatibility** | every pre-integration state (the bank, the human's local presets) loads and re-saves with every existing key's value exact (RC-9.2 to RC-9.4) | load, save, compare key by key; then render (L0063) | (M) a key renamed without its alias | permanent |
| **RG-6 by-key seeding** | TP-22: no test depends on manifest order | the scan | (D) a planted by-index test | permanent |
| **RG-7 the morph** | the morph suite's sweeps bit-identical for every key except the table key | the morph suite at the pinned commit and after | (M) the new class applied to a second key | until 1.0 |
| **RG-8 the modulation system** | MS-29's refusals, routes and destinations other than the new ones unchanged | the modulation suite | (M) a per-note route to `morph.x` accepted | until 1.0 |
| **RG-9 CPU elsewhere** | the instruction count of RG-1's renders within ±1 % (reported) | instruction counts, ADR-380 A1 #1 | a build running the noise oscillator twice | until 1.0 |

---

## 7. Cadence and caps

| when | what | limit |
|---|---|---|
| **`./verify fast`** (horde; seconds; CI-blocking) | TP-23; the piece scripts parse, their times fit their lengths, their keys are in range; `tools/wt-fidelity/`'s meters on their synthetic controls; the importer's gate on a T-H table once the importer exists in horde's tools | engine-free only; no model; no plug-in |
| **every builder commit** (the build repo) | every Layer-0 gate of the stages built so far | the builder's run informs; it closes nothing |
| **stage close** (the build repo, the lead) | on the clean export: every gate of this and every earlier stage, every mutation plant firing and every control reading zero in the same run, the sanitizers, the instruction counts beside the test count, the stage's comparisons from 4.9 (local, reported), the listening batch | one heavy job at a time, at most 4 threads, `nice`, load checked first (CONTRACT §6.5) |
| **the stage's night**, 2 to 6 am | the judged CPU cells timed (PF-3's method, best of 5), which re-fixes the instruction-to-time factor; from S8 the test plug-in's au-bench cells; the Serum CPU reference, re-measured only when Serum's version changes | Live and OneDrive closed; the load guard never bypassed (ADR-380 A1 #3) |
| **`./verify full`** (horde, Layer-E, macOS) | from S8: auval and the host load for the test plug-in; E2E-13 | human-paced |
| **at 1.0 integration** | sections 5 and 6 at every integration PR (headless), the Live checklist at each integration milestone | — |

**Token caps** (ADR-380 A1 #4). The nightly batch is scripts: it spends no tokens. A model is used only to write
the morning report from the numbers, and any optimisation attempt a red CPU cell triggers runs under the caps: 2 M
tokens a day, 300 k per attempt, counted from reported usage and written to the ledger. Under the doctrine's model
routing, a verbatim report is a Haiku role and an analysis a Sonnet role, each pinned at spawn; the lead's rulings
stay with the lead.

**CPU caps on tests.** A recording or analysis job runs under the shared-machine rule above; parallel renders are
never used to save time. The 1,000-roll battery of E2E-7 and the full scenario stream of section 6 are the heaviest
jobs and run alone.

---

## 8. Competitor material: the handling rules

The rule (the competitor-reference convention, ROADMAP, ratified 2026-08-01): **never commit competitor-rendered
audio, presets, wavetables or captured data**; framing stays comparative. The Serum gauntlet's protocol
(`docs/design/serum-gauntlet-protocol.md`) adds: no parameter listing and no parameter or preset name in a tracked
file, results keyed by our labels.

- **Where it lives.** One folder outside every repo (recommended: the gauntlet's `~/horde-gauntlet/`, a `wt/`
  folder in it), holding `tables/`, `patches/`, `bindings/`, `renders/` and the label note. The brief that ordered
  this plan names `local/` (gitignored, B312); if the human prefers it, the root checkout's `local/wt-fidelity/`
  only, never a worktree's, which the harness deletes (Q-1).
- **In code, not vigilance.** au-bench's recording mode refuses an output path inside a repo unless the path is
  git-ignored (Appendix A); TP-23 refuses tracked audio and preset files; the third-party latch stays.
- **What is committed:** the piece scripts (notes and times), the meters, the T-H recipes, and results as numbers
  and curves of numbers keyed by our labels, with the Serum version.
- **What is never committed:** Serum's tables, presets, renders, parameter ids or names, the binding file, the
  label-to-table note, spectrograms, waveform images or screenshots of its output.
- **Never published.** The A/B listening page is a local file. An Artifact or any upload of a page carrying Serum
  renders would publish them.

---

## 9. Open questions for the human

| # | question | recommendation |
|---|---|---|
| **Q-1** | Where does Serum material live: the gauntlet's folder outside the repos, or `local/` in the root checkout? | **Outside the repos** (`~/horde-gauntlet/wt/`): the location, not `.gitignore`, is the safeguard (the gauntlet's rule 3), and worktrees are deleted with their `local/`. `local/wt-fidelity/` in the root checkout is the fallback. |
| **Q-2** | Build a test-only importer (4.6)? | **Yes**, in the harness from S1 and as a compile-time hook in the test plug-in from S8, never shipped, making no recipe. Without it the T-S half of the protocol cannot run. |
| **Q-3** | Record with au-bench (Appendix A) or give auhost a preset loader? | **au-bench**, taking auhost's script format so one piece file drives both tools; auhost carries the retired SCALPEL core and stays as it is. |
| **Q-4** | Should "at least as clean as Serum" be a horde gate? | **No**: reported, and a note where Serum is cleaner opens a ROADMAP row. A gate on uncommitted competitor material cannot be reproduced and moves with their releases (4.8). |
| **Q-5** | Use horde's own tables in Serum as well as Serum's in horde? | **Yes**: T-H1 to T-H4 make half the comparisons reproducible from the repo. |
| **Q-6** | Frame counts: ask for 16-frame Serum tables? | **Yes where the editor allows**; otherwise 16 evenly picked frames, judged only at those. |
| **Q-7** | Glides: match the two synths' glide curves, or analyse against the tracked f₀? | **Tracked f₀**: no curve matching needed; the 30-semitone split if a synth cannot glide 60 semitones at once. |
| **Q-8** | Unison: match detune by knob or by measured spread? | **By measured spread** (±20 cents outer, PROPOSED), calibrated by a counted bisection of renders. |
| **Q-9** | Adopt TP-1 to TP-23 into SPEC-WAVETABLE as WT-F15 onward, with their PROPOSED numbers under CONTRACT §7 C3? | **Yes, before S0 closes**, since S0 must prove every S1 gate's plants; TP-3 especially, as WT-5's "never steps" has no meter today. |
| **Q-10** | Migration: hold base-wave patches moved to tables to DT-3's harmonic method (1 to 16 within 0.5 dB) as a judged gate? | **Yes, judged**; patches with blades are flagged by name and heard in a capped batch, not judged by a number. |
| **Q-11** | Live's offline bounce twice: judged or reported? | **Judged headless** (DT-1, DT-2 through the plug-in shell), **reported in Live**: Live's own settings can differ between exports, and a mismatch there is checked against the headless result first. |
| **Q-12** | Serum's oscillator quality: which setting? | **Both** its default and its highest; horde Standard against both, High against the highest. |
| **Q-13** | Does the Serum CPU reading on the wavetable chord revise WT-BUDGET? | **Reported beside WT-BUDGET** at the next night run; WT-BUDGET moves only by the human's ruling with the timed evidence (CONTRACT §5.4 (4)). |
| **Q-14** | Plots: may curves derived from Serum renders be committed? | **Curves of measured numbers, yes; spectrograms, waveforms and screenshots, no.** |
| **Q-15** | Run the protocol now on the JS lab, before S1? | **Yes, on T-H tables only, labelled "lab"**: it proves the pieces, the recording mode and the meters, so S1's first comparison is about the build, not the pipeline. |

---

## Appendix A. Design note: recording in `tools/au-bench`

**Today.** au-bench (B638) times an instrument on PF-3's chord from an `.aupreset` or a Live `.adv`, reads peak and
RMS, and discards the audio (`run_once` in `au_bench.cpp`). auhost (B381) renders note scripts to float32 WAV but
has no preset loader. The protocol needs both in one host, so that Serum and the test plug-in are driven
identically.

**What au-bench would need** (a design, not code):

1. **`--script <piece.json>`**: auhost's script format (Appendix B; auhost's header documents it), read by one
   reader both tools share, so a piece file means the same in each. Note events go in with their sample offset
   (sample-accurate, `MusicDeviceMIDIEvent`'s offset); parameter events at the start of the block that contains them
   (most Audio Units apply them there; auhost's header says so). A piece that automates sets its own block size
   (P-4b: 32).
2. **`--bind <bindings.json>`**: maps a script's lane names (`frame`, `warp`, `fm`, `detune`) to the instrument's
   parameter ids, so committed scripts name no product's parameter. The Serum binding lives out of tree; horde's
   binding (its manifest ids) may be committed.
3. **`--record <dir>`**: a separate, untimed pass. Each recording makes one primed render on a fresh instance and
   throws it away, then two renders on fresh instances, captured into a buffer allocated before the render (no file
   I/O inside the block loop), written afterwards as stereo float32 WAV with no metadata from the plug-in. It prints
   the two renders' maximum difference and first differing frame. It never shares a run with a timed cell, so
   recording never perturbs a CPU figure.
4. **Output names** by our labels only: `<table>__<piece>__<sr>__<synth>__r<n>.wav`.
5. **The location latch.** The tool refuses an output directory inside any git work tree unless `git check-ignore`
   says it is ignored, as the third-party latch refuses a component: the rule held in code.
6. **Tail and length.** The script's own length, plus `--tail <s>` after the last note-off (default 0.5 s).
7. **Its controls**, added to `--controls` on Apple's DLSMusicDevice: (R1) a script's note lands at its sample (an
   onset detector on the recording, within one sample at 128-sample blocks); (R2) two recordings of a deterministic
   unit are bit-identical; (R3) a planted "skip the render" records silence and is flagged; (R4) an output directory
   inside the repo that is not ignored is refused; (R5) a script naming an unbound lane is refused.

Exit codes as today, plus one for a refused output location. It stays UNWIRED: a measurement tool, outside every
gate.

## Appendix B. The piece script format

auhost's format, unchanged, with one addition: a `lane` event for bound parameters and a `ramp` for automation.

```
{ "name": "P-4b", "seconds": 10.5, "block": 32,
  "events": [ {"sec": 0,   "on": 48, "vel": 108},
              {"sec": 0,   "lane": "frame", "value": 0},
              {"sec": 1.0, "lane": "frame", "ramp": {"to": 1, "sec": 8.0}},
              {"sec": 10,  "off": 48} ] }
```

A `ramp` is expanded by the host into one value per block, linear in the parameter's normalised range, so both
synths receive the same staircase; the build harness expands it the same way. A lane's normalised value means the
same thing in both only where the binding says so (frame 0 to 1 is the table's first to last frame in both; the
warp amounts are matched by measurement, section 4.3).
