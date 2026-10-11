# b448-p3-fuzz-reads-above-the-rail — the fuzz plays full-velocity chords again, reads the bus above the rail, and tells loud from growing

- **Queue item:** B448 (Blind-Spot Armor), phase 2 Wave 1, package P3 (ADR-209, ADR-211). A rework ordered by the lead on PR #1059, same branch.
- **Corrects** `traces/2026-10-11-b448-p3-parameter-fuzz.md`. That entry's point 2 (chords share one note's velocity budget), its thresholds table, its three-control list and its run-time numbers describe commit `2167a04` and no longer hold. Its points 1 and 3 (the sampler is restated, 21 engine keys have no declared range) still do.
- **Why:** the first build divided each velocity by the note count so that a loud chord could not sit on the limiter's rail, where the tool could not tell it from a blow-up. The lead's ruling: that narrows what the fuzz plays, and a fuzz that does it cannot see the loudest thing a player does. Undo it, read above the rail, and separate loud from growing.
- **What changed:**
  - `tools/h2_fuzz_check.mjs`:
    - Chords play the drawn velocities again, 0.1 to 1 on every note.
    - **Reading above the rail.** A case that touches the rail, or peaks above 8 (where `atanh` is already losing digits), is rendered again with every gain value it sets divided by 2^40, and the reading is multiplied back. Gain changes inside a case are divided too, so no case is left out. An engine with no declared gain key gets "level unread", printed and red.
    - **Loud versus growing.** A steady or periodic level is judged against three bands (pass, grey, too loud). A case above the pass line is rendered twice more with the stretch that held its peak made 0.5 s and 1 s longer; growth is the smallest of three ratios between the last quarters of those stretches. Above `growFail`, or off scale even through the divided gain, it is a blow-up.
    - **The linearity control**, on every run: the drawn preamble cases under the rail are rendered both ways and compared sample by sample; the same comparison on `control/tone`, whose bus ignores gain, must fail. And the source is read on every run: one more read of the gain member anywhere under `h2/engine/` stops the tool.
    - **A named case in the preamble**, `loud-chord`, read on every run.
    - The planted faults are now predicted in kind and in level (`planted()`), not just as "enters the corner".
  - `h2/harness/control_fuzz.h`, `h2/harness/adapters.h`: the blow-up plant is now a level that starts at 4096 x gain and doubles every 40 ms; a fourth plant, `control/fuzz-loud`, is a steady level of 4096 x gain. Both follow `gain` over their whole bus, as the engine does.
  - Unchanged: the key and extreme counting, the NaN and hang plants, the nightly job, the `verify` line, the kept-cases file.
- **What makes the divided-gain reading valid (read, then measured):**
  - Read: `h2/engine/engine.h` lines 1961-1962, `L[i] = std::tanh(yl * s.gain * 1.6);` and its twin for the right channel, are the only reads of the gain member under `h2/engine/`; `s.gain` is written only by `snap()` and by the linear 16-sample glide at line 1802. The DC blocker (lines 1957-1960) is before the multiplication.
  - Measured: over 268 cases with direct peaks from 0.05 to 6, the bus read through the divided gain differed from the direct bus by at most 3.973e-15 of the peak. By hand, before the rework: one case at 1/16 of the gain read 0.62 against about 10 at full gain.
  - What would break it: any stage before that line that reads gain. The tool stops if the source gains such a read. The voice cap's fade passes samples through a float; the harness does not expose the cap.
- **Thresholds: PROPOSED, not ratified (ADR-209 item 5).** Measured 2026-10-11 with `--measure --cases 6000` at seeds 46152 and 20261011: 12000 cases, 7,986,889,844 samples, full-velocity chords in scope, nothing non-finite, nothing off scale.

  | name | measured worst | margin | value | band |
  |---|---|---|---|---|
  | `prePass` | true bus level 21.368: the named case `loud-chord`, held 6 s | x2 (6 dB) | 42.8 | at or under: pass |
  | `preFail` | the same | x4 (12 dB) | 85.5 | to here: grey; above: fail, too loud |
  | `growPass` | growth 1.6904 (seed 46152, case 2307; the other seed 1.6236), every case probed | x1.5 | 2.54 | to here: steady |
  | `growFail` | the same | x3 | 5.08 | to here: grey; above: fail, blow-up |
  | `offScale` | not this tool's: the rail (19) times 2^40 | none | about 2e13 | still on the rail through the divided gain: fail, blow-up |
  | `linTol` | 3.973e-15 of the direct peak (268 cases; the other seed 3.947e-15) | x1000 | 4e-12 | the linearity control's one line |
  | `settlePass` | 0.2022 s from the release law's end to under the floor (seed 46152, case 5425; the other seed 0.1911) | x1.5 | 0.304 s | silent by then: pass |
  | `settleFail` | the same | x3 | 0.607 s | silent only by then: grey; still above the floor: fail |
  | `silentFloor` | not measured here: the line `tools/patchspace/metrics.mjs` `silence()` already draws | none | -90 dBFS | the settle rows are measured against it |

  The margins are argued, not measured, and are the human's to change.
- **The measured worst level, and the case that gives it.** In both 6000-case runs the loudest case was the named chord as first written (eleven items), 19.2663. Of the 11998 drawn cases, three peaked above 8 and none reached that. Minimised while it stays within 10 % of its level the chord has eight items and reads **21.368** with 32 samples on the rail in the direct render; that form is the named case now. It is steady: held 0.5 s and 1 s longer its last two quarters read 20.31 then 21.37 both times, growth 1.000. Its replay line:
  ```
  node tools/h2_fuzz_check.mjs --replay '{"v":1,"engine":"composed","table":"composed","sr":44100,"block":128,"seed":1802188165,"notes":[[93,0.8262921872548759],[52,0.48174431112129246],[66,0.907497893483378],[76,0.7283318708883598]],"hold":264600,"tail":"short","sets":{"base":3,"b2on":1,"mode2":3,"c2":1,"mirror2":3,"N":9,"detune":0},"events":[[8878,[["gain",1]]]]}'
  ```
  The level is periodic (a burst about every 5.3 s, seen by hand over 40 s before the rework), so 21.368 is the worst over a 6 s hold, not a proven ceiling.
- **Evidence (measured 2026-10-11, darwin-arm64, Apple clang, Node 24, the Release `h2_harness_render`; `<s>` is this session's scratch directory, nothing from it is committed):**
  - The PR-size run, `node tools/h2_fuzz_check.mjs --bin build-release/h2_harness_render`: GREEN, exit 0. 1021 cases of `composed` (348 static, 336 sweeps, 336 stacked changes, 1 named), 196,091,048 samples, 0 non-finite. Worst true level 21.368 (the named case, read through the divided gain); 1 case on the rail in the direct render, 0 off scale, 0 unread; no case above the pass line, so none probed for growth. Silence judged in 566 cases including the longest release (11.51 s). Keys: 113 rows all set; 74 of 74 continuous keys sent their minimum and maximum; 141 of 141 options; 112 of 112 swept and stacked.
  - Linearity control: 6 preamble cases (direct peaks 1.26, 0.162, 0.301, 0.946, 0.315, 0.239), worst difference 2.32e-16 of the peak, allowed 4e-12. Must-fail: `control/tone` differs by 1.10e+12 of the peak, reported.
  - `control/fuzz-blowup`: read as blow-up in 59 cases, exactly the 59 it was planted in (14 decided by the growth windows, 45 by going off scale when held longer); first at case 41; that case read 39337 with growth 15.36; minimised to `fb = 1, K = -1`.
  - `control/fuzz-loud`: read as too loud in 60 cases, exactly the 60 planted, each at the planted level (the first: 4096.0 at gain 1, growth 1.000; replayed alone at the default gain 0.35: 1433.6); not once as a blow-up; minimised to `xm = 1, kRuleAmt = 0`.
  - `control/fuzz-nan`: 48 of 48. `control/fuzz-hang`: 71 of 71. `control/tone`: nothing reported. Unknown key: reported. Coverage with one key's counts removed: red five ways.
  - The red and grey paths on the REAL engine, with a bound changed in memory only by a scratch script: with the level lines at 1.5 and 3.0, 44 cases were probed for growth, the worst read 1.418 and none read as growing, and the named chord read too loud at 21.368 with growth 1.000; with the growth lines at 1.05 and 1.3, case 114 (growth 1.418) read as a blow-up and minimised to `K = 1, A = 2000, gain = 1`, a two-second linear attack; with `linTol` at 1e-17 the linearity control went red; with the settle lines at 0.02 and 0.06 s, 22 cases failed as not silent.
  - Determinism: one process against four over 349 cases, outputs identical but for the first line.
  - Sizes: `--cases 100` is refused (exit 2). Through `tools/nightly/run.py` with a scratch job list, `--deep --cases 1021` at a floor of 1021 read as ran, and 349 cases under the same floor as failed. The 20000-case deep run was NOT run here.
  - Run time: the PR-size run took 34 s and 43 s on two runs with the load average between 3 and 5 (112 and 147 s of CPU). At 33 to 42 ms a case, 20000 cases are 11 to 14 minutes here; with the assumed four to six times for a hosted runner, 44 to 85 minutes.
  - `node h2/harness/selftest.mjs`: GREEN, 30 rows.
- **Observed, not judged:**
  - With sustain at 0 a held voice's envelope decays without a floor: a growth probe read 2.1e-83, then 3.7e-125, then 1.9e-190 across successive quarters of a held note (case 159 of a one-round run, read through the divided gain; the four cases of that run that decayed this way all have S = 0 and D = 10). `h2/engine/engine.h` line 1835 has no free-below-a-level in the decay stage, where the release stage (line 1836) frees under 1e-4. Held long enough that reaches subnormal numbers (argued, not measured). ADR-211 rules flush-to-zero on in the shell.
  - The first 6000 drawn cases of the deep run were read by the measurement; cases 6000 to 19999 are unread.
- **Evidence consulted:** the lead's rework message on PR #1059; `h2/engine/engine.h` lines 159-176, 1796-1838 and 1936-1962; `h2/harness/{README.md,adapter_composed.h,harness.h}`; the first trace of this package.
- **Alternatives rejected:**
  - Keeping "on the rail" as the blow-up rule with full-velocity chords: the named chord puts 32 samples on the rail and is steady.
  - One slow plant for the growth windows and one fast one for off scale: the single plant, doubling every 40 ms, is decided both ways in one run (14 and 45), and the run is red if the windows decide none.
  - Excluding cases that change gain from the reading: dividing every gain value is exact, because the glide is linear.
  - A smaller dividing factor: 2^40 costs nothing (a bus of 1e-11 is read more exactly than one of 10) and leaves the growth windows room before a doubling level goes off scale.
- **Verify:** `./verify fast` on the staged tree, then again on the commit before the push; exit code and git hash are in `.harness/last-verify.json` and in the hand-back. `./verify full` was not run by this package.
- **Open questions:**
  1. The thresholds and their margins need the human's ratification. The level bands sit at 2 and 4 times the loudest chord found; whether a chord at 21 on a bus whose limiter saturates near 19 is acceptable is a loudness question this tool does not answer.
  2. The 21 keys with no declared range: declare ranges, or accept the hole as a limit of row R6?
  3. The deep run's 20000 cases may take over an hour on a hosted runner (unmeasured); the count and floor are one edit in `tools/nightly/jobs.json`.
  4. The nightly's seed is fixed; `--seed` exists.
  5. `h2/harness/README.md`'s file table does not list `control_fuzz.h`.
  6. `tools/test_table_check.py` does not read `tools/*.mjs` (the lead has taken this up).
