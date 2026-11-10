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
