# B668: the C2a bench checks its own timing for contamination (ADR-391, A1)

**What changed.** `tools/c2a_remainder/main.cpp`: PF-3's load guard is replaced, for this bench only, by a contamination check.
Each render is bracketed by PF-3's calibration loop; a repeat counts only if both calibrations are within 2 % of the run's fastest
(after a 30 s warm-up, A1) and its wall time is within 2 % of its thread CPU time. Best of the first 5 clean repeats; 20 rounds, then
refuse (exit 2). Two must-fail controls every run (exit 3 if blind). Load is printed, never gating. `kCleanTol` (1.02) registered on
the human's poll; the retired guard's tolerance entry removed. README, ROADMAP B667/B668/B669, DECISIONS ADR-391 + A1, design-ledger
not_design entry.

**Why.** The night of 2026-11-10: PF-3's guard refused all ten attempts (02:01 to about 05:40). `ps` at each attempt's start:
`corespotlightd` (Spotlight) up to 1.7 cores and `suggestd` (Siri Suggestions) about 1 core until 03:30, then `suggestd` alone
at about one core to 05:30. The Mac slept during attempt 10 and woke at 10:22; that attempt was stopped (outside the window).

**Evidence consulted.**
- SPEC-ENGINE PF-3 (line 868): the guard is PF-3's own text; hence ADR-391 item 4 and B669 (the spec round, both clean-room repos).
- PF-3 and this bench time wall clock (steady_clock), not thread CPU; the first poll said otherwise and the second corrected it.
- Efficiency-core plant: a calibration at QOS_CLASS_UTILITY, QOS_CLASS_BACKGROUND and under PRIO_DARWIN_BG all ran at 3.57–3.62 GHz,
  4.03 cycles per iteration (scratch probe), i.e. on performance cores. So control (1) became a calibration beside 16 spinning threads.
- The boost (scratch probe, 40 calibrations each followed by 2.5 s of work): 0.1007 s at 3.98 GHz for the first two, then
  0.107 s at 3.73–3.75 GHz from the ninth on (x1.06–1.07). The first run under item 1 rejected all 20 repeats in each of 8 rounds.
- Smoke (`--timed --smoke`): control (1) x2.676, control (2) wall/CPU 2.841, both rejected.

**The timed run** (2026-11-10 11:59–12:04, engine 6cd98b5, apps running, load 2.9–3.7; full output kept locally):
19 rounds to reach 5 clean repeats of every cell. Control (1) x3.054 REJECTED, control (2) wall/CPU 2.592 REJECTED. Fastest
calibration after warm-up 0.110 s (about 3.66 GHz, below the 3.74 GHz the machine holds alone, so these figures may read about 2 % high).

| cell (share of one core) | 44.1 kHz | 48 kHz |
|---|---|---|
| A as is (PF-1 anchor) | 66.15 % | 69.27 % |
| C blades off, 9 members | 13.59 % | 14.06 % |
| D blades off, 1 member | 4.85 % | 4.85 % |
| G silent | 0.44 % | 0.58 % |
| J sine base, blades off, 9 | 14.85 % | 15.55 % |
| remainder C − G | **13.14 %** | **13.48 %** |
| per member (8 voices) | 1.09 % | 1.15 % |
| per-voice intercept (8 voices) | 3.81 % | 3.71 % |
| blades' share of B | 79.5 % | 79.7 % |

Two corrections to what the counts suggested (B667): time puts about 3/4 of the remainder per member, not per voice (the lead
told the human "about two-thirds per voice" on 2026-11-09, from a misreading of the counts: the counts' own intercept is about 27 %);
and the sine base costs 13–15 % more per member than the triangle in time, though the counts showed them level.

**Verify.** `./verify fast` exit 0 on this tree before the warm-up edit; rerun after it and the records, exit 0 again. Base: origin/main bbdea650.

## ADR-391 A2: the AU bench gets the same check

`tools/au-bench/au_bench.cpp` (B638, the Serum 2 reference): PF-3's load guard replaced by the same contamination check, so the
reference and horde's figures are read alike. `measure()` takes `strict`: a 30 s warm-up, then rounds until every cell has 5
clean runs (calibration within 2 % of the fastest, the timed span's wall time within 2 % of the rendering thread's CPU time), 20
rounds then exit 2; `contamination_controls()` after every measurement (exit 4 if blind). `--controls` C1 (the guard rule) is
replaced by the two spinner plants. `kCleanTol` registered and the file's inline-float-compare entry re-approved without the
guard's 3.0 and 0.5, both on the human's "Yes, give the AU bench the same check." README and B668 updated.

`--controls` (sandbox off, Live running, load 5.96): CONTROL (1) x3.052 REJECTED, CONTROL (2) wall/thread CPU 1.109 REJECTED;
C1 to C9 ok; "CONTROLS PASSED 0 control(s) failed". `./verify fast` exit 0 on this tree before the trace edit.

## The Serum 2 reference and ADR-392 (C2a)

`tools/au-bench` (this branch's build), sandbox off, Live closed, 2026-11-10 12:33–12:39, Serum 2 v2.1.5 (aumu Xf2X XFER) as an
Audio Unit, the human's patch saved from Live as an `.aupreset` (kept under `~/horde-gauntlet/`, never committed), 8 notes, 128-sample blocks:

| patch | 44.1 kHz | 48 kHz | rounds | controls |
|---|---|---|---|---|
| the human's: 3 wavetable oscillators x 9 unison, no filter, no FX | 12.61 % | 13.68 % | 11 | x2.972, wall/CPU 2.348: both REJECTED |
| Serum 2's default patch (did the preset load?) | 2.05 % | 2.21 % | 11 | x3.383, wall/CPU 1.043: both REJECTED |

Process CPU (all threads) equals the wall share (12.43 / 13.47 %): Serum renders on the calling thread. Peaks at +9.6 / +10.0 dBFS
(the patch clips at 8 notes; it does not change the timing). About 4.6 % per Serum oscillator against about 18.5 % per horde
wavetable oscillator (5 % + 13.5 %): about 4x. Put to the human with targets of 1.5x (PF-2's 7.1 %), 2x and 3x; ruled "Decide after
the probe" (ADR-392): baseline recorded, S1 opens, B670 probes the swarm's per-member path. CONTRACT §7 C2a answered in part.
