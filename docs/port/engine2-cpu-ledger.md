# The new engine's CPU ledger

Origin: horde lead session, 2026-11-03, ADR-347 and ADR-354. The figures are the builder's commit `5442272`, before any CPU pass, on an Apple M3. This file stays out of the test author's reach (ADR-247).

## The official figure

PF-3's protocol and load guard, the unmodified tool, load 1.5 to 1.6:

| Cell | Share of a core | Target |
|---|---|---|
| Glass horde pad, 8 voices, 44.1 kHz | **217.31 %** | 10.7 % |
| Glass horde pad, 8 voices, 48 kHz | 212.95 % | |

## Against the current engine

Measured, not PF-3: scratch copies of both tools with the load limit at 5 (the human, 2026-11-02). The current engine runs at `--os 1`, 44.1 kHz, 8 voices. Load read 2.35 to 4.6 in its three rounds, whose Glass horde pad figures agree within 0.1 %.

| Preset | Current | New | New ÷ current |
|---|---|---|---|
| Quarter sync | 1.66 % | 9.53 % | 5.7× |
| Harmonic stack | 9.88 % | 52.69 % | 5.3× |
| Fold over sync | 6.92 % | 52.32 % | 7.6× |
| Crushed bells | 15.06 % | 104.21 % | 6.9× |
| Breathing pad | 19.71 % | 147.90 % | 7.5× |
| Glass horde pad | 26.73 % | 218.14 % | 8.2× |
| Glass horde pad, `edges` 0 on both blades (new only) | | 93.65 % | |

The lead's estimate before the run, recorded 2026-11-02: about 250 % (225 to 300) against about 29 %. The result is below that range.

## Where the time goes

A sample of `pf1` on a `-O3 -g` copy, 23,116 samples. Line numbers are `5442272`'s `swarm/blades.cpp`.

| Where | Share |
|---|---|
| `blade_sample` and what it calls | 88 % |
| ED-7's taper integral, lines 941 to 1018 | at least 48 % |
| of which line 1014, the 32-tap write inside the eight nodes | 44 % |
| Math-library calls | about 12 % |
| `effective` | 4 % |
| `window_point` | 3 % |

The `edges` 0 control agrees: tapers are 57 % of this patch. 83 presets set blade 1's edge, with a median of 0.15, and the heavy ones set it high (Glass horde pad 0.7 and 0.6).

## The ambitious patch, scratch

The lead's own reading of PF-10's wording, not the test author's cells, and no morph yet. Two oscillators and the sub on Glass horde pad, 44.1 kHz. Worst block is the smallest of five repeats' worst, as a share of the block's duration.

| Notes | Voices | Average | Worst block |
|---|---|---|---|
| Held chord | 8 | 439 % | 483 % |
| Note stream | limit 8 | 411 % | 480 % |
| Held chord | 16 | 1,110 % | 1,169 % |
| Note stream | limit 16 | 688 % | 936 % |

## Voices

Glass horde pad: 218.58 % at 8 voices, 552.21 % at 16, 19,557.54 % at 64, so per voice 27.3, 34.5 and 305.6 %. PF-5 says cost grows linearly. PF-3's chord adds keys upward, reaching key 237 at 64 voices. The gauntlet shows higher notes do cost more (1.4× an octave up, 1.7× at three), which covers the 16-voice step at most. It doesn't explain 64 voices, whose cause isn't known. That figure says little about real playing.

The sanity control read 0.830 between unison 16 and unison 1, within its factor of 2.

## The cost gauntlet

The human's request, 2026-11-03. One change at a time, scratch tool, not PF-3, load about 1.7, 8 voices, 44.1 kHz, best of 5, spreads under 1.4 %. Two runs of the unchanged patch read 218.2 and 221.1 %, so differences under about 3 points aren't real.

**Taking away from Glass horde pad (221 %):**

| Change | Reads |
|---|---|
| All edge correction off (`bandLimit` 0) | 77 % |
| Edges 0 on both blades | 96 % |
| ED-7's correction off (code) | 96 % |
| Blade 1 off | 106 % |
| ED-7's integral at 1 point (code) | 116 % |
| ED-7's integral at 2 points (code) | 131 % |
| Blade 2 off | 150 % |
| Both blades off | 14 % |
| FM reset to Sync on blade 1 | 201 % |
| Sine→saw carrier to saw on blade 2 | 205 % |
| The per-member filter E off (code) | 212 % |
| Spread law, spread drift, cut rule, quantize, coupling, detune, width, FM depth 0 | 218 to 221 % |
| Cross-mod 0 | 255 %: more, since a live loop skips the corners' correction |
| FM reset to FM free on blade 1 | 310 % |
| High quality | 335 % |

**Adding to a plain swarm of 9 members:** plain 13.9 %; coupling 13.9 %. One blade at Edges 0: Noise 49.5, Sync 51.0, Fold 51.1, Ring 52.8, Crush 54.5, FM reset 63.9, FM free 150.1 %. Sync with: a sine→saw carrier 70.9, twins 57.1, Edges 0.1 86.0, Edges 0.3 137.6, Edges 0.7 147.9, cross-mod 51.8, feedback 51.7 %. Two Sync blades 81.1 %, with serial 82.0, with collision 82.1 %.

**Members on Glass horde pad:** 1, 29.4 %; 3, 77.4 %; 5, 125.5 %; 9, 221 %; 16, 389.4 %. About 24 points a member at 8 voices.

**Height on Glass horde pad:** up 1 octave 313.7 %; 2, 338.3 %; 3, 385.0 %.

**Profiles of single blades:** Sync at Edges 0 has no one hot spot: 46 % in `blade_sample` spread over many lines, `effective` 9.5 %, `render` 12 %. FM free: about 33 % in `sin` and the math library, `hot` 10 %, `dc_estimate` 5 %.

**The soft-edge A/B, against the exact version, on Glass horde pad and Breathing pad.** A control of exact against exact reads below −550 dB.

| Phrases | 2 points | Off |
|---|---|---|
| Held, chord, fast line (keys up to about 69) | −77 to −93 dB | −59 to −75 dB |
| Fast line up 2 octaves | −55 to −62 dB | −37 to −38 dB |
| Held note up 3 octaves | −48 to −57 dB | −23 to −32 dB |

The lower phrases alone would have made “off” look safe. The suite hasn't judged any of these versions, and nobody has heard them yet.

## The per-member round: what's free, what's heard, what's left (2026-11-03)

All scratch, not PF-3: 8 voices, 44.1 kHz, patched copies of `5442272`, with other jobs running. Figures are the percentage of a core. “Untouched” is the scratch reading above, on the unpatched engine; the last three columns come from one interleaved run, and the “Fixes” column from an earlier one.

**Fixes with no change to the sound.** Removing repeated work, a NEON E, skipping quiet samples and prepared rows read no differing samples at float precision on 8 bank patches and the widened list: saw and square bases, mirrors, FM free and pitch, Noise, collision, feedback, rotation, High, a fast attack with scatter and motion. A plant in the rows fired wherever rows run (not on Crush at Edges 0.3, or at High, where they don't). A critic reviewed them: the first figures were inflated by the patch's own overhead, so measure against the untouched binary before quoting a saving. The suite's two reds at these sets match the unpatched run, and are the builder's known 3.b and 9.b1.

| Patch | Untouched | Fixes | + 2-point soft edges | + 8 taps | or 2 samples |
|---|---|---|---|---|---|
| Plain saw swarm, 9 members | 13.9 | 12.9 | | 11.0 | 9.8 |
| One Sync blade, Edges 0 | 51 | 39 | | 35 | 34 |
| Fold over sync | 52 | 36 | 25 | 23 | 22 |
| Crushed bells | 104 | 72 | 55 | 48 | 45 |
| Breathing pad | 148 | 101 | 62 | 56 | 53 |
| Glass horde pad | 218 | 123 | 95 | 89 | 87 |

**Soft edges: what the gates say.** On items 7.7 and 7.8 (Sync, alias dB): the exact law reads 96 to 113; 2 points 48 to 64 at sharp edges, rising to 100 or more at wide edges on low notes; off 18 to 34. Off is the must-fail control and reads red. 1 point reads about 5 dB better than off and saves 4 more points on Glass, so it was dropped. Every failure of 2 points is under ED-7's 5 dB rule: it's a much lower floor, and the human couldn't hear it on these clips.

**Hard edges: a reading, not QS-1's estimator.** Saw, C2 to C8, a Blackman-Harris window with the note's harmonics masked; no correction reads 28 to 9, QS-1's own control values: 32 taps at least 53 to 89 (this reading stops short of the suite's 115), 8 taps 46 to 31, 2 samples 44 to 26. Today's filter delays the sound about 2.7 samples, so 2 samples comes out about 1.7 samples earlier than today, and 8 taps about 0.8.

**What the listening covers.** Harsh rising runs from C5 to C8, one and nine members, one fast line on Reverse cut, Glass and Breathing at high pitches, one listener on one playback system. The first nine-member clips had K at −1 and 6 members, the lead's mistake with normalised values, which the human heard as hollow; they were replaced. Much of the difference is above 14 kHz, where many ears and speakers fade, so a second listener is worth having. The combined clip, 8 taps or 2 samples with 2 points against today: a slight difference at the very top of the rising runs, more prominent on one member, which the human accepted for the CPU (ADR-356 A1).

**Where the rest goes.** Glass stays near 90 % because the cost is the blade machinery on every member and sample. Sync spends about 50 % of its time in `blade_sample` and 9 % in the math library, and FM free about a third in the system `sin`, with a 48-step bisection per reset. The general routine reads D's limits and slopes numerically where Sync's are known in closed form. Direct Sync and a cheaper sine are under test.

**The current engine as a 1.0 fallback.** A read-only check, nothing built or run. Main's plugin builds the legacy `src/hypersaw_clap.cpp`; `h2/engine` plays only in the local preview shell, 720 commits behind, last touched 2026-10-14. All 83 presets use only its keys. Shipped nearly as is (about 8 items) it keeps the swarm, the blades and the bank at 5 to 8 times less CPU, and reverses the roster: one oscillator, 8 voices, no sub, noise, ENV 2 to 4, MPE, per-voice modulation or poly glide, with today's aliasing. Brought up to the roster (about 20 items) it's a core rewrite that likely spends the lead (a hypothesis). Its heaviest patch still misses 10.7 %. Confidence in the sizes is low to moderate. The new engine's remaining work is about 6 items, with the CPU as the one big unknown.

## Moments, tables and direct Sync (2026-11-03)

All scratch, not PF-3, on a machine other projects were using. Percent of a core, 8 voices, 44.1 kHz.

**Moments (ADR-358).** Legendre moments on whole samples, swept over prepared rows, wider rows on uncut runs, and nodes only on partial samples. About 200 lines and 27 KB of rows plus the existing 68 KB, built at prepare in about 5 ms. Items 7.7 and 7.8 pass at −135 dB on the final binary, all of items 07 and 10 pass at −120 dB, and the rest of the suite wasn't run. Against today's 8 nodes the sound reads −140 to −157 dB. Against 2 nodes, moments costs about the same on Glass and 2 to 4 % less on the others. A possible existing bug: on FM free with a square or held-noise modulator, a finite-difference slope may straddle a step and spike; the 8-node path didn't spike in that render, and no gate covers it.

**Direct Sync.** One Sync blade from 38 % to 20 %, saw on saw from 39 % to 18 %, two Sync blades from 51 % to 25 %. Worst difference −137 dB in float, one float's rounding; plants fired wherever it ran; 7.7, 7.8 and item 00 are unmoved. Only 18 of 83 bank patches qualify, and the heavy ones are unchanged, since reading the blades per sample is 52 to 60 % of their time and the edge search 5 to 11 %. With Glass's blades off, its settings read 13.6 %, so 10.7 % is out of reach for it by this route; an estimate puts a direct Glass at 55 % plus or minus 10.

**Tables, Crushed bells.** One table per member: held chord 47.6 % to 6.5 %, ready in 70 ms, reads about 2 ns, chord build about 3 to 4 ms. Against exact −64 dB on the chord, −31 to −33 dB at C6 and C7, where Crush and Ring no longer fold. One table per voice drops 4 to 16 kHz by 25 dB. The human heard no loss per member in the three examples, and clearly heard one table per voice. A moving cut returns the voice to full cost, with no spike; keeping tables through a sweep cost up to 33 % and read −6 to −14 dB. So tables don't reduce the worst case. Roughly 17 patches qualify as written, 30 with a better table builder. The suite and real-time checks haven't run; the pool was 12.7 MB, and worst-case memory is an estimate.

## The first batch, official (2026-11-03)

PF-3 on Glass horde pad, 8 voices, from the builder's run, commit `f1ca0ba` in the builder's repository, not pushed. The lead's clean-export rerun is still to come.

| Run | 5442272, 44.1 / 48 kHz | Batch, 44.1 / 48 kHz | Gain at 44.1 kHz |
|---|---|---|---|
| `pf1 pf9 sanity` (the figure to carry) | 218.63 / 213.61 % | 96.29 / 101.77 % | 2.27× |
| `pf1` alone | 221.81 / 216.95 % | 92.88 / 98.37, then 92.10 / 96.85 % | 2.39× to 2.41× |

293 of 295. PF-9 at 8, 16, 64 voices: 96.55, 212.23, 6519.86 %. Why the batch reads about 4 % lower when `pf1` runs alone isn't known, and 5442272 goes the other way. Past ADR-354's 2× line, so the pass continues; still far from 10.7 %, and not safe in Live.

What each part gained, the tool on variant builds, `pf1` alone, the builder's own comparisons: items 1 to 4 with moments off 205.10 % (1.08×); the batch 92.10 % (moments 2.23× on top); the batch with the quiet skip off 90.90 %, so the skip gains nothing on this patch. pf4 was not run, so other patches have no PF-3 figure here.

Worst block, the builder's scratch tool, not PF-3, % of the block, least of 5 runs, 5442272 then batch: Glass horde pad 253.5 then 111.2; Glass notes up 36 423.1 then 154.9; Two FM voices 333.4 then 265.1, with its mean 137.7 then 71.0.

## The combination, scratch (2026-11-03)

The ruled moments and 32 taps, direct Sync, the wide path, closed slope, Carson width and a gate that keeps some FM on the node loop. Not PF-3, load 4 to 5, bank of 83, share of a core, before then combined: at or under 10.7 %, 16 then 20 patches; over 100 %, 5 then 0; median 0.382 then 0.206; total 35.74 then 16.95 cores. Glass horde pad 1.397 then about 0.64 to 0.67. The bank renders within −133.2 dB of today, 1 s per patch, 8 voices. Not yet ruled: the three additions (closed slope, Carson width, the gate). The gate holds the click's first cause exactly as it is today, and the builder's cuts may replace it.

## Why blocks overrun (2026-11-03, B614)

Scratch tree, not PF-3, not the builder's engine, machine load 3 to 6, heights inflated by it; which block and what work are the reliable part. Thread CPU per block, share of 128 samples at 44.1 kHz, the lead's recomputation from the raw traces.

| Patch | Set | Chord-on (block 0) | Worst held, 3 repeats | Median | Blocks over 1 in 2 s |
|---|---|---|---|---|---|
| Crunch horde | 149 | 10.45 to 10.53 | 6.88 to 6.96 | 0.32 | 212 |
| Crunch horde | combined | 2.19 to 2.24 | 1.47 to 1.50 | 0.21 | 28 to 32 |
| Jitter swarm | 149 | 4.90 to 4.91 | 4.92 to 6.02 | 0.33 | 115 |
| Jitter swarm | combined | 1.32 to 1.36 | 1.34 to 3.79 | 0.22 | 115 |
| Two FM voices | 149 | 3.35 to 3.63 | 3.34 to 3.39 | 0.63 | 230 to 232 |
| Two FM voices | combined | 1.16 to 1.55 | 1.54 to 1.65 | 0.24 | 31 to 46 |
| Fold over sync (control) | combined | 0.46 | 0.43 to 0.45 | 0.16 | 0 |
| Plain 9-member saw (control) | combined | 0.17 to 0.18 | 0.14 to 0.18 | 0.13 | 0 |

**The cause.** C.16's mean estimate runs for many members in one block, at the end-of-block tick, and FM free's is 32 walks long. Members of voices started together share a countdown, so they fire together. Estimates land only in odd blocks on the grid, and the chord block runs them all before the first sample (SW-15).

**The evidence.** The predictions were fixed before any run, and the block pattern and point counts matched (Jitter swarm exactly, Crunch horde 2.4 % under). Switching the estimate off, for timing only, takes the three to 0.24 to 0.29 on a chord and held, best repeats. A sine modulator keeps the spike, so noise isn't the cause; FM reset removes it, which points to the 32 starts. Repeat 1 matches repeats 2 and 3, and a warm chord matches the cold one, so lazy statics and first touches aren't the cause. Plants: a 6 ms spin read in CPU and wall at its block only, a 6 ms sleep in wall only, and a planted estimate at its block only. The plain saw read 0 points in every block.

**Limits.** The estimate is about 40 % of these patches' average at the combined set. Larger buffers are inferred from summing blocks, no host was tested: at 256 samples Crunch horde's chord is still about 1.2, and at 512 everything at the combined set is under 1. Nothing was heard, and the candidate fix (spreading the 32 starts over grid steps) hasn't been built.

## The click's fix, official (2026-11-04)

PF-3 on Glass horde pad, 8 voices, builder's `02aa43f`, 293 of 295, figures reported in `cab1c83`, not pushed. The lead's clean-export rerun is still to come.

| Run | 5442272 | Batch `f1ca0ba` | Fix `02aa43f` |
|---|---|---|---|
| `pf1 pf9 sanity`, 44.1 / 48 kHz | 218.63 / 213.61 % | 96.29 / 101.77 % | 94.78 / 99.61 % |
| `pf1` alone, one straight after the other | | 96.29 / 99.61 % | 91.94 / 99.24 % |

PF-9 at 8, 16, 64 voices: batch 96.55, 212.23, 6519.86; fix 93.71, 207.01, 6755.87. The batch alone has read 92.10 to 96.29 %, and one binary moves about 4.5 % between runs for a reason nobody knows, so PF-1 can't read a change that small yet. The fix's run was noisy: calibration 0.103 to 0.178 s, the guard waited 41 times, a busiest other process up to 2.62 cores, part of that probably the lead's browser tests. The guard refused all 16 attempts between 03:45 and 07:04 while `suggestd` held a core; that process calmed by itself.

## FM free's 8-entry estimate, scratch (2026-11-04)

Ruled in ADR-362, which carries the figures. Not PF-3, without the click fix. Glass horde pad has no FM free and isn't touched.
