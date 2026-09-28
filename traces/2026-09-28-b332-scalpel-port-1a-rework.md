# b332-scalpel-port-1a-rework — the phase-1a port reworked after an independent critic's review

- **Queue item:** B332, phase 1a, under rework. The lead's message of 2026-09-28 relayed an independent Opus critic's review of PR #837: verdict REWORK (narrow). The critic reproduced 369/372 and found the port itself sound. This entry corrects the record of `traces/2026-09-28-b332-scalpel-port-1a.md` (which stays as written; traces are append-only) and records the fixes.
- **Why:** The first version made three claims the evidence did not support.
  - "FMA contraction is narrow" came from a control filtered to the 111 targeted rows.
  - "Release is within 8e-15 of the oracle" held for only two bench presets.
  - Exclusions exempted every criterion.

  Several paths also had no row.
- **Evidence consulted:**
  - the critic's findings H1, M1–M5 and the LOW items, as relayed by the lead;
  - `razor-core.js:752` (`this.cnt = 32`) and `:857` (`dcS … * 0.003`);
  - `nm -u build-release/h2_scalpel_parity_check`, which lists `_exp2`, `_ldexp`, `_pow` and `___sincos_stret`;
  - scratch builds of this same source at `-O2` and `-O3` with clang's default contraction;
  - interleaved CPU runs at load average 11–17.

## What changed (commit `47de45e`)

- **H1(a) The FMA control runs the full stream.** `h2_scalpel_fma_control` no longer filters to the targeted rows.
  - **Before:** 2 of 111 rows red.
  - **After:** **19 of 386** rows miss parity. Worst rms 2.178e-6, max 1.474e-4, both on Crunch (audio-rate PM) :: repeat. The mean bit-exact share falls to 6.02%, from 29.22% clean.
  - The critic's 18 of 372 are the same rows, plus Cross-mod ring :: arp, which fails the tightened RMS rule for excluded rows under contraction. Clang's default contraction (no flag) at -O2 and at -O3 gives the identical set, row for row.
- **H1(b) Docs corrected.** The "F4 is narrow" paragraph and the CPU table's "Release vs oracle" column in `docs/port/scalpel-phase-1a.md` are corrected, with the full-stream numbers.
- **H1(c) Every h2 build is contraction-off, PENDING HUMAN RULING.**
  - `measure_h2_scalpel` gained `-ffp-contract=off`.
  - `h2/README.md` has a new rule 7.
  - New `tools/h2_rules_check.py` (WIRED in `verify fast`) enforces it. The one declared exception is `h2_scalpel_fma_control`, which must be `-ffp-contract=fast`.
  - The CPU cost of the rule at -O3 is: Crushed bells 5.21% vs 4.91% RT per voice (+6.2%); Quarter sync 0.54% vs 0.58–0.59%, so none (contraction-off was faster).
- **H1 The FMA control's verdict separates outcomes.**
  - Exit 0 means FIRED: a genuine miss on a complete, well-formed stream.
  - Exit 1 means it did not fire.
  - Exit 2 means INFRASTRUCTURE FAILURE: a renderer crash, a missing END, a truncated stream or a NONINV mismatch.

  A stream cut mid-scenario, fed by hand, returned 2. `verify full` reports exits 1 and 2 with different messages.
- **M1 Exclusions.** An excluded row is exempt from max-abs ONLY. It must still hold rms < 1e-6 and identical events, with JS selfMax ≥ C++ max. The count is pinned: `kExpectedExclusions = 3`, printed as `PASS  PIN`.
- **M2 Coverage.** 14 new targeted rows:
  - a re-strike after a full release (short R);
  - w 0, and w2 0;
  - collision swept off;
  - blade 2's own clock at rest, and homing;
  - mono glide 1 ms;
  - settle skipped (K 0);
  - `gate()` mode 6;
  - blade 2 lock 1 with FM and collision;
  - `retune` and `panic`, now in the script grammar as `re` and `panic`;
  - a mid-note `polyMode` switch;
  - the modX 65536 wrap. Checked by hand against the oracle before the row went in: `m.modX` and `bx.modX` each wrap once.

  All 14 pass.
- **M3 Detection floor.** The swarm's pitch in `couple()` is scaled by (1+eps), for eps from 1e-12 to 1e-3, on T/mode 0 :: chord.
  - **The floor is 1e-10 relative** (about 1.7e-7 cents). Max-abs catches it first. The events disagree from 1e-8.
  - 1e-12 gives rms 4.8e-9, max 6.5e-8: green, i.e. invisible.
  - The check requires 1e-3 to be red and prints the ladder.
- **M4** is divergence candidate 12, doc only: the `dcS … * 0.003` per-sample constant (ADR-009 class) and the 32-sample `couple()` cadence (PLAUSIBLE).
- **M5 is recorded in the doc.** Phase 1b composes or subclasses and keeps `razor_core.h` byte-stable, so the 1a harness stays a live regression check.
- **LOW items:**
  - The PRISTINE oracle's samples are now streamed. The instrumented copy is rendered alongside, for events only, and must match bit for bit on every scenario: NONINV 386/386, where it was 8 probe scenarios.
  - New must-fail F5: every event reported one tick late. The samples are bit-identical to clean and the events disagree, so it is caught by events alone.
  - F3 must now break both samples and events.
  - `include_check.py` is widened to `tools/*.h` and `h2/**` (123 files, green).
  - `h2_rules_check.py` enforces three things, and calibrates itself every run with 11 synthetic must-fail cases:
    - h2 ↔ src includes;
    - no h2 target links `HYPERSAW-impl`;
    - the oracle's blob pin, `razor-core.js@0ce6a713410d89c65bf55f761f1dc791fae61b16`, which is also streamed and printed by the check.

    On the real tree, by hand: dropping the flag, linking legacy and a stale pin each fail.
  - The libm table's `pow` row is corrected. The new `pow2` row is V8 `Math.pow(2,x)` vs `std::exp2`, as clang compiles the core, at 99.81%. The general-base `pow` row stays at 100%.
  - `verify full` echoes the EXCL evidence, PIN and FLOOR lines on green runs.

## Before → after

| | before (`9b2e054`) | after (`47de45e`) |
|---|---|---|
| scenarios at parity | 369 / 372 | 383 / 386 |
| worst rms / max (non-excluded) | 2.220e-12 / 1.897e-10 | 2.220e-12 / 1.897e-10 (same rows) |
| exclusions | 3, all criteria exempt | 3, max-abs exempt only, count pinned |
| FMA control | 2 of 111 (targeted only) | 19 of 386 (full stream); infra = exit 2 |
| clang default contraction, -O2/-O3 | not measured | 19 of 386 (same set) |
| NONINV | 8 scenarios | 386 of 386 |
| must-fail controls | F1, F2, F3 (+ F4) | F1, F2, F3 (both criteria), F5 (events only), + F4 full stream |
| detection floor | not stated | 1e-10 relative pitch error |

## Alternatives rejected

- **Tightening max-abs below 1e-6 so the contraction rows would pass as "noise".** That would weaken nothing, but it inverts the question. The rows are real parity misses, and whether shipped builds may contract is the human's call.
- **An env-var or runtime FMA emulation.** It is not the real fault.
- **Making the floor ladder a hard gate at 1e-10.** The floor is a measured property of a scenario, not a ratified threshold, so it is reported, with only the 1e-3 sanity row required.

## Verify

- `./verify fast` exit 0 on the rework working tree.
- `./verify full` exit 0 on `47de45e`. `.harness/last-verify.json` read `{"target":"full","exit":0,"git":"47de45e","ts":"2026-09-28T18:13:00Z"}`.
- Its h2 lines, verbatim:
  - `PASS  PIN  the exclusion list holds its pinned 3 scenarios`
  - `PASS  FLOOR  a relative pitch error eps on the swarm (couple()'s f, one non-hash constant) on 'T/mode 0 :: chord'; detection floor = 1e-10`
  - `h2_scalpel_parity_check: GREEN — 383/386 scenarios at parity (rms < 1e-06, max < 1e-06, events identical), 3 excluded as chaotic with evidence, 0 red`
  - `h2_scalpel_fma_control: FIRED — the -ffp-contract=fast build of the same core fails the parity gate (19 of 386 scenarios miss parity; worst rms 2.178e-06, worst max 1.474e-04; mean bit-exact 6.02%)`
- The final hash (this trace's commit) is re-verified; see the PR.
- `private_name_gate` is SKIPPED in the worktree. Its patterns were run by hand on every touched file: 0 matches.

## Open questions

- **The shipped-flags ruling (the human).** Must the build that ships be the build that passed parity? Until the ruling, rule 7 holds every h2 build contraction-off.
- **Linux CI for the rework.** The full-stream FMA control and the doubled JS renders lengthen the sanitizer jobs. The result will be in the PR's checks.

## Parity table (commit `47de45e`; `build-release/h2_scalpel_parity_check`, full stream)

| scenario | verdict | rms | max-abs | bit-exact | events | edge/carrier BLEPs, entries b1/b2 | note |
|---|---|---|---|---|---|---|---|
| P/Starting points / Quarter sync :: chord | PASS | 3.595e-17 | 3.331e-16 | 53.5% | identical | 289/289/145/0 |  |
| P/Starting points / Quarter sync :: repeat | PASS | 2.168e-17 | 2.220e-16 | 60.7% | identical | 64/64/32/0 |  |
| P/Starting points / Quarter sync :: arp | PASS | 3.338e-17 | 2.776e-16 | 53.6% | identical | 577/576/289/0 |  |
| P/Starting points / Trance jitter :: chord | PASS | 1.694e-13 | 1.477e-11 | 7.7% | identical | 289/145/144/0 |  |
| P/Starting points / Trance jitter :: repeat | PASS | 1.765e-13 | 1.431e-11 | 17.7% | identical | 64/32/32/0 |  |
| P/Starting points / Trance jitter :: arp | PASS | 1.218e-13 | 8.553e-12 | 8.0% | identical | 577/289/288/0 |  |
| P/Starting points / Two blades :: chord | PASS | 4.244e-17 | 3.331e-16 | 45.9% | identical | 1741/864/432/441 |  |
| P/Starting points / Two blades :: repeat | PASS | 2.928e-17 | 2.776e-16 | 55.7% | identical | 387/192/96/99 |  |
| P/Starting points / Two blades :: arp | PASS | 5.709e-15 | 7.212e-14 | 18.4% | identical | 3462/1721/864/869 |  |
| P/Starting points / Crush vs FM :: chord | PASS | 3.816e-17 | 3.331e-16 | 51.8% | identical | 2174/0/437/433 |  |
| P/Starting points / Crush vs FM :: repeat | PASS | 2.897e-17 | 3.331e-16 | 58.0% | identical | 483/0/99/96 |  |
| P/Starting points / Crush vs FM :: arp | PASS | 3.824e-17 | 3.331e-16 | 51.6% | identical | 4329/0/862/869 |  |
| P/Starting points / Chord of formants :: chord | PASS | 4.953e-17 | 6.731e-16 | 36.7% | identical | 2180/0/735/0 |  |
| P/Starting points / Chord of formants :: repeat | PASS | 4.800e-17 | 1.428e-15 | 47.3% | identical | 485/0/165/0 |  |
| P/Starting points / Chord of formants :: arp | PASS | 7.352e-16 | 5.163e-15 | 13.5% | identical | 4340/0/1450/0 |  |
| P/Starting points / Harmonic stack :: chord | PASS | 1.779e-15 | 1.824e-14 | 4.4% | identical | 2018/2305/1010/0 |  |
| P/Starting points / Harmonic stack :: repeat | PASS | 4.083e-17 | 3.331e-16 | 53.8% | identical | 448/512/224/0 |  |
| P/Starting points / Harmonic stack :: arp | PASS | 1.082e-15 | 1.486e-14 | 21.8% | identical | 4025/4605/2020/0 |  |
| P/Starting points / Golden bells :: chord | PASS | 4.842e-17 | 3.331e-16 | 46.0% | identical | 1739/0/873/0 |  |
| P/Starting points / Golden bells :: repeat | PASS | 4.304e-17 | 3.331e-16 | 50.1% | identical | 384/0/192/0 |  |
| P/Starting points / Golden bells :: arp | PASS | 2.314e-15 | 1.676e-14 | 16.8% | identical | 3463/0/1735/0 |  |
| P/Starting points / Undertone drift :: chord | PASS | 2.140e-16 | 8.382e-15 | 36.7% | identical | 2604/1445/869/0 |  |
| P/Starting points / Undertone drift :: repeat | PASS | 3.851e-16 | 1.885e-14 | 39.1% | identical | 576/328/192/0 |  |
| P/Starting points / Undertone drift :: arp | PASS | 5.071e-15 | 5.655e-14 | 18.4% | identical | 5201/3108/1738/0 |  |
| P/Starting points / Cross-mod ring (watch) :: chord | EXCL | 1.422e-07 | 1.235e-05 | 0.5% | identical |  | chaotic: JS alone, inputs 1 ULP apart, rms 8.796e-06 max 9.887e-04 |
| P/Starting points / Cross-mod ring (watch) :: repeat | EXCL | 3.041e-07 | 2.625e-05 | 2.7% | identical |  | chaotic: JS alone, inputs 1 ULP apart, rms 1.871e-05 max 2.103e-03 |
| P/Starting points / Cross-mod ring (watch) :: arp | EXCL | 1.803e-08 | 1.490e-06 | 1.0% | identical |  | chaotic: JS alone, inputs 1 ULP apart, rms 1.317e-04 max 1.431e-02 |
| P/Starting points / Cross-mod horde :: chord | PASS | 1.763e-16 | 7.216e-16 | 17.1% | identical | 1735/1732/869/0 |  |
| P/Starting points / Cross-mod horde :: repeat | PASS | 1.167e-16 | 3.886e-16 | 21.4% | identical | 384/384/192/0 |  |
| P/Starting points / Cross-mod horde :: arp | PASS | 3.521e-16 | 4.058e-15 | 10.0% | identical | 3457/3457/1735/0 |  |
| P/Starting points / Feedback screech :: chord | PASS | 1.290e-13 | 6.126e-12 | 4.4% | identical | 290/0/145/0 |  |
| P/Starting points / Feedback screech :: repeat | PASS | 1.422e-13 | 7.538e-12 | 7.4% | identical | 64/0/32/0 |  |
| P/Starting points / Feedback screech :: arp | PASS | 8.918e-14 | 4.449e-12 | 4.7% | identical | 575/0/287/0 |  |
| P/Starting points / Blade pluck :: chord | PASS | 9.566e-16 | 3.086e-14 | 10.7% | identical | 1439/848/722/0 |  |
| P/Starting points / Blade pluck :: repeat | PASS | 1.270e-15 | 3.590e-14 | 8.1% | identical | 319/239/161/0 |  |
| P/Starting points / Blade pluck :: arp | PASS | 3.499e-14 | 9.484e-13 | 5.9% | identical | 2875/1806/1448/0 |  |
| P/Starting points / Zap bass :: chord | PASS | 2.146e-15 | 1.152e-13 | 11.6% | identical | 564/212/189/0 |  |
| P/Starting points / Zap bass :: repeat | PASS | 2.259e-17 | 2.220e-16 | 61.9% | identical | 288/122/96/0 |  |
| P/Starting points / Zap bass :: arp | PASS | 3.495e-15 | 1.058e-13 | 18.2% | identical | 680/237/228/0 |  |
| P/Starting points / Zap bass :: legato | PASS | 3.426e-15 | 1.043e-13 | 29.1% | identical | 402/146/135/0 |  |
| P/Starting points / Crunch (pitch S&H) :: chord | PASS | 4.340e-15 | 1.455e-13 | 4.5% | identical | 123/62/61/0 |  |
| P/Starting points / Crunch (pitch S&H) :: repeat | PASS | 3.187e-15 | 9.770e-15 | 3.9% | identical | 64/32/32/0 |  |
| P/Starting points / Crunch (pitch S&H) :: arp | PASS | 9.187e-15 | 7.905e-13 | 3.7% | identical | 144/72/72/0 |  |
| P/Starting points / Crunch (pitch S&H) :: legato | PASS | 3.699e-15 | 8.094e-15 | 3.3% | identical | 86/43/43/0 |  |
| P/Starting points / Crunch (audio-rate PM) :: chord | PASS | 5.846e-14 | 6.104e-13 | 55.5% | identical | 123/228/61/0 |  |
| P/Starting points / Crunch (audio-rate PM) :: repeat | PASS | 6.500e-14 | 6.805e-13 | 55.4% | identical | 64/200/32/0 |  |
| P/Starting points / Crunch (audio-rate PM) :: arp | PASS | 3.266e-13 | 3.440e-11 | 55.6% | identical | 144/198/72/0 |  |
| P/Starting points / Crunch (audio-rate PM) :: legato | PASS | 6.304e-14 | 6.105e-13 | 19.0% | identical | 86/205/43/0 |  |
| P/Starting points / Crunch horde :: chord | PASS | 2.625e-14 | 1.965e-12 | 0.1% | identical | 1449/724/725/0 |  |
| P/Starting points / Crunch horde :: repeat | PASS | 1.661e-14 | 1.123e-12 | 0.3% | identical | 321/161/160/0 |  |
| P/Starting points / Crunch horde :: arp | PASS | 2.472e-14 | 1.455e-12 | 0.6% | identical | 2883/1447/1436/0 |  |
| P/Starting points / Jitter swarm :: chord | PASS | 1.315e-13 | 7.900e-12 | 1.0% | identical | 1453/728/725/0 |  |
| P/Starting points / Jitter swarm :: repeat | PASS | 9.268e-14 | 7.559e-12 | 2.0% | identical | 322/161/161/0 |  |
| P/Starting points / Jitter swarm :: arp | PASS | 8.342e-14 | 5.062e-12 | 1.4% | identical | 2886/1444/1443/0 |  |
| P/Starting points / Hollow twin :: chord | PASS | 4.155e-17 | 3.331e-16 | 48.7% | identical | 1741/871/441/0 |  |
| P/Starting points / Hollow twin :: repeat | PASS | 2.787e-17 | 2.776e-16 | 54.8% | identical | 387/193/99/0 |  |
| P/Starting points / Hollow twin :: arp | PASS | 5.753e-15 | 7.161e-14 | 19.0% | identical | 3462/1732/870/0 |  |
| P/Starting points / Reflected sync :: chord | PASS | 2.948e-17 | 2.220e-16 | 54.6% | identical | 290/580/145/0 |  |
| P/Starting points / Reflected sync :: repeat | PASS | 1.865e-17 | 1.665e-16 | 60.2% | identical | 64/128/32/0 |  |
| P/Starting points / Reflected sync :: arp | PASS | 3.005e-17 | 3.331e-16 | 54.4% | identical | 577/1156/289/0 |  |
| P/Starting points / Reverse cut :: chord | PASS | 1.562e-15 | 3.182e-14 | 14.3% | identical | 2168/1440/722/0 |  |
| P/Starting points / Reverse cut :: repeat | PASS | 1.740e-15 | 3.461e-14 | 12.1% | identical | 480/320/160/0 |  |
| P/Starting points / Reverse cut :: arp | PASS | 3.720e-14 | 8.571e-13 | 7.9% | identical | 4324/2869/1445/0 |  |
| P/Starting points / Sine-to-saw fan :: chord | PASS | 5.746e-17 | 3.886e-16 | 33.1% | identical | 2022/0/1011/0 |  |
| P/Starting points / Sine-to-saw fan :: repeat | PASS | 4.491e-17 | 3.331e-16 | 40.8% | identical | 448/0/224/0 |  |
| P/Starting points / Sine-to-saw fan :: arp | PASS | 1.210e-14 | 1.541e-13 | 11.6% | identical | 4026/0/2022/0 |  |
| P/Starting points / Swarm-linked spread :: chord | PASS | 2.552e-16 | 6.772e-15 | 25.1% | identical | 1738/0/870/0 |  |
| P/Starting points / Swarm-linked spread :: repeat | PASS | 1.320e-16 | 8.911e-15 | 28.5% | identical | 385/0/192/0 |  |
| P/Starting points / Swarm-linked spread :: arp | PASS | 1.939e-16 | 5.121e-15 | 26.7% | identical | 3457/0/1734/0 |  |
| P/Starting points / Wandering blades :: chord | PASS | 8.772e-16 | 1.712e-14 | 8.1% | identical | 1731/0/863/0 |  |
| P/Starting points / Wandering blades :: repeat | PASS | 8.226e-16 | 1.593e-14 | 9.4% | identical | 389/0/192/0 |  |
| P/Starting points / Wandering blades :: arp | PASS | 4.020e-16 | 9.121e-15 | 17.6% | identical | 3446/0/1733/0 |  |
| P/Starting points / Humanized :: chord | PASS | 1.793e-15 | 3.009e-14 | 3.9% | identical | 1441/1168/721/0 |  |
| P/Starting points / Humanized :: repeat | PASS | 1.142e-16 | 1.196e-14 | 50.2% | identical | 318/266/159/0 |  |
| P/Starting points / Humanized :: arp | PASS | 1.289e-15 | 4.788e-14 | 23.6% | identical | 2870/2138/1440/0 |  |
| P/Starting points / Anchored horde :: chord | PASS | 2.924e-15 | 2.104e-13 | 35.5% | identical | 1728/864/864/0 |  |
| P/Starting points / Anchored horde :: repeat | PASS | 7.683e-16 | 5.872e-14 | 48.2% | identical | 384/192/192/0 |  |
| P/Starting points / Anchored horde :: arp | PASS | 4.493e-15 | 1.416e-13 | 16.1% | identical | 3444/1728/1728/0 |  |
| P/Starting points / Locked horde :: chord | PASS | 1.463e-16 | 1.559e-14 | 51.8% | identical | 2021/2020/1009/0 |  |
| P/Starting points / Locked horde :: repeat | PASS | 1.422e-16 | 1.521e-14 | 52.9% | identical | 449/449/224/0 |  |
| P/Starting points / Locked horde :: arp | PASS | 1.001e-15 | 1.518e-14 | 23.6% | identical | 4021/4017/2013/0 |  |
| P/Starting points / Splayed blades :: chord | PASS | 2.493e-15 | 3.955e-14 | 2.4% | identical | 1161/1161/581/0 |  |
| P/Starting points / Splayed blades :: repeat | PASS | 2.883e-15 | 4.677e-14 | 1.2% | identical | 258/258/129/0 |  |
| P/Starting points / Splayed blades :: arp | PASS | 1.117e-15 | 2.396e-14 | 23.4% | identical | 2310/2307/1156/0 |  |
| P/Starting points / Drifting cuts :: chord | PASS | 5.443e-17 | 4.441e-16 | 49.2% | identical | 1739/2320/872/0 |  |
| P/Starting points / Drifting cuts :: repeat | PASS | 5.404e-17 | 3.331e-16 | 49.5% | identical | 384/513/191/0 |  |
| P/Starting points / Drifting cuts :: arp | PASS | 4.318e-15 | 6.509e-14 | 28.6% | identical | 3475/4634/1739/0 |  |
| P/Starting points / Vowel choir :: chord | PASS | 1.204e-15 | 5.621e-15 | 4.5% | identical | 1455/0/733/0 |  |
| P/Starting points / Vowel choir :: repeat | PASS | 1.426e-15 | 5.662e-15 | 4.0% | identical | 323/0/163/0 |  |
| P/Starting points / Vowel choir :: arp | PASS | 5.568e-16 | 3.345e-15 | 21.4% | identical | 2892/0/1449/0 |  |
| P/Starting points / Frozen noise FM :: chord | PASS | 1.072e-15 | 1.823e-14 | 3.3% | identical | 1442/1440/722/0 |  |
| P/Starting points / Frozen noise FM :: repeat | PASS | 3.755e-17 | 3.331e-16 | 50.2% | identical | 320/320/160/0 |  |
| P/Starting points / Frozen noise FM :: arp | PASS | 8.912e-16 | 3.497e-14 | 22.2% | identical | 2874/2872/1445/0 |  |
| P/Starting points / Ring saw :: chord | PASS | 3.129e-15 | 6.117e-14 | 2.0% | identical | 867/867/435/0 |  |
| P/Starting points / Ring saw :: repeat | PASS | 2.572e-17 | 2.220e-16 | 55.9% | identical | 192/192/96/0 |  |
| P/Starting points / Ring saw :: arp | PASS | 1.691e-15 | 4.974e-14 | 11.0% | identical | 1726/1734/870/0 |  |
| P/Starting points / Folded slice :: chord | PASS | 1.935e-15 | 8.965e-15 | 1.2% | identical | 867/0/435/0 |  |
| P/Starting points / Folded slice :: repeat | PASS | 2.960e-17 | 3.053e-16 | 51.2% | identical | 192/0/96/0 |  |
| P/Starting points / Folded slice :: arp | PASS | 4.397e-17 | 3.331e-16 | 36.9% | identical | 1724/0/870/0 |  |
| P/Starting points / Fixed formant :: chord | PASS | 5.380e-16 | 2.609e-15 | 11.5% | identical | 2181/0/735/0 |  |
| P/Starting points / Fixed formant :: repeat | PASS | 6.047e-16 | 2.810e-15 | 7.1% | identical | 485/0/165/0 |  |
| P/Starting points / Fixed formant :: arp | PASS | 5.998e-15 | 7.078e-14 | 4.8% | identical | 4341/0/1451/0 |  |
| P/Starting points / Operator jitter :: chord | PASS | 8.839e-14 | 5.947e-12 | 4.1% | identical | 289/145/144/0 |  |
| P/Starting points / Operator jitter :: repeat | PASS | 1.339e-14 | 1.496e-12 | 20.2% | identical | 64/32/32/0 |  |
| P/Starting points / Operator jitter :: arp | PASS | 4.484e-14 | 2.936e-12 | 5.5% | identical | 577/289/288/0 |  |
| P/Starting points / Fixed crush :: chord | PASS | 2.581e-17 | 1.110e-16 | 61.7% | identical | 289/2001/144/0 |  |
| P/Starting points / Fixed crush :: repeat | PASS | 1.638e-17 | 1.110e-16 | 64.7% | identical | 64/678/32/0 |  |
| P/Starting points / Fixed crush :: arp | PASS | 2.320e-17 | 1.110e-16 | 62.1% | identical | 577/3148/288/0 |  |
| P/Starting points / Slewed crush :: chord | PASS | 2.962e-17 | 2.220e-16 | 63.1% | identical | 1308/0/440/0 |  |
| P/Starting points / Slewed crush :: repeat | PASS | 2.307e-17 | 1.110e-16 | 62.9% | identical | 290/0/98/0 |  |
| P/Starting points / Slewed crush :: arp | PASS | 5.833e-16 | 4.927e-15 | 33.9% | identical | 2597/0/869/0 |  |
| P/Two-blade / Two-formant vowel :: chord | PASS | 6.910e-16 | 1.366e-14 | 13.1% | identical | 3635/0/735/735 |  |
| P/Two-blade / Two-formant vowel :: repeat | PASS | 3.749e-17 | 3.886e-16 | 48.2% | identical | 810/0/165/165 |  |
| P/Two-blade / Two-formant vowel :: arp | PASS | 1.397e-15 | 2.384e-14 | 18.3% | identical | 7230/0/1450/1450 |  |
| P/Two-blade / Sync into crush :: chord | PASS | 4.532e-15 | 6.860e-14 | 2.2% | identical | 1744/440/438/434 |  |
| P/Two-blade / Sync into crush :: repeat | PASS | 2.710e-17 | 2.776e-16 | 59.9% | identical | 388/99/98/96 |  |
| P/Two-blade / Sync into crush :: arp | PASS | 3.481e-17 | 4.441e-16 | 55.1% | identical | 3462/869/865/870 |  |
| P/Two-blade / Metal pair :: chord | PASS | 4.917e-17 | 3.331e-16 | 36.1% | identical | 2323/1744/582/580 |  |
| P/Two-blade / Metal pair :: repeat | PASS | 3.173e-17 | 2.776e-16 | 45.1% | identical | 517/390/131/128 |  |
| P/Two-blade / Metal pair :: arp | PASS | 5.066e-17 | 3.331e-16 | 35.3% | identical | 4617/3464/1154/1155 |  |
| P/Two-blade / Hollow double :: chord | PASS | 8.984e-15 | 1.469e-13 | 0.2% | identical | 3484/2611/439/439 |  |
| P/Two-blade / Hollow double :: repeat | PASS | 2.390e-17 | 2.220e-16 | 55.5% | identical | 775/579/99/97 |  |
| P/Two-blade / Hollow double :: arp | PASS | 2.851e-15 | 3.626e-14 | 22.6% | identical | 6928/5194/868/870 |  |
| P/Two-blade / Breath and bite :: chord | PASS | 1.774e-15 | 2.807e-14 | 1.7% | identical | 3623/1452/720/733 |  |
| P/Two-blade / Breath and bite :: repeat | PASS | 2.538e-17 | 2.776e-16 | 56.8% | identical | 803/320/160/163 |  |
| P/Two-blade / Breath and bite :: arp | PASS | 7.199e-15 | 1.601e-13 | 20.1% | identical | 7215/2897/1435/1449 |  |
| P/Two-blade / Chasing blades :: chord | PASS | 4.615e-17 | 3.331e-16 | 40.7% | identical | 2905/1456/730/723 |  |
| P/Two-blade / Chasing blades :: repeat | PASS | 3.344e-17 | 2.776e-16 | 49.4% | identical | 647/325/164/160 |  |
| P/Two-blade / Chasing blades :: arp | PASS | 3.569e-15 | 5.018e-14 | 9.0% | identical | 5777/2889/1442/1442 |  |
| P/Two-blade / Formant pluck :: chord | PASS | 4.125e-17 | 1.610e-15 | 43.3% | identical | 2186/0/441/441 |  |
| P/Two-blade / Formant pluck :: repeat | PASS | 3.784e-17 | 1.610e-15 | 53.0% | identical | 480/0/95/98 |  |
| P/Two-blade / Formant pluck :: arp | PASS | 4.870e-15 | 8.793e-14 | 17.5% | identical | 4346/0/872/870 |  |
| P/Two-blade / Stacked sync :: chord | PASS | 3.085e-15 | 3.669e-14 | 3.0% | identical | 3495/879/881/873 |  |
| P/Two-blade / Stacked sync :: repeat | PASS | 4.610e-17 | 3.331e-16 | 46.3% | identical | 775/196/198/192 |  |
| P/Two-blade / Stacked sync :: arp | PASS | 1.069e-15 | 1.609e-14 | 21.8% | identical | 6948/1740/1737/1740 |  |
| P/Two-blade / Crunch and fold :: chord | PASS | 7.012e-15 | 5.388e-13 | 1.6% | identical | 246/62/61/62 |  |
| P/Two-blade / Crunch and fold :: repeat | PASS | 5.813e-15 | 5.058e-13 | 7.2% | identical | 128/32/32/32 |  |
| P/Two-blade / Crunch and fold :: arp | PASS | 1.464e-14 | 1.614e-12 | 2.4% | identical | 292/73/73/73 |  |
| P/Two-blade / Crunch and fold :: legato | PASS | 1.478e-14 | 1.481e-12 | 9.4% | identical | 174/44/43/44 |  |
| P/Two-blade / Formant over sync :: chord | PASS | 1.256e-14 | 5.901e-13 | 3.3% | identical | 4066/1161/588/576 |  |
| P/Two-blade / Formant over sync :: repeat | PASS | 1.315e-14 | 5.110e-13 | 2.3% | identical | 903/257/132/128 |  |
| P/Two-blade / Formant over sync :: arp | PASS | 3.345e-15 | 1.925e-13 | 21.4% | identical | 8087/2309/1160/1151 |  |
| P/Two-blade / Two FM voices :: chord | PASS | 1.718e-13 | 1.140e-12 | 0.2% | identical | 1734/0/432/435 |  |
| P/Two-blade / Two FM voices :: repeat | PASS | 7.465e-14 | 4.892e-13 | 2.7% | identical | 384/0/96/96 |  |
| P/Two-blade / Two FM voices :: arp | PASS | 1.846e-13 | 1.372e-12 | 2.9% | identical | 3453/0/855/870 |  |
| P/Two-blade / Counter-rotation :: chord | PASS | 3.494e-15 | 4.402e-14 | 1.6% | identical | 2325/1741/587/579 |  |
| P/Two-blade / Counter-rotation :: repeat | PASS | 2.930e-17 | 3.331e-16 | 55.7% | identical | 517/389/132/129 |  |
| P/Two-blade / Counter-rotation :: arp | PASS | 1.417e-14 | 2.687e-13 | 5.8% | identical | 4618/3462/1152/1159 |  |
| P/Two-blade / Smear and strike :: chord | PASS | 5.350e-14 | 4.602e-12 | 3.4% | identical | 4071/2030/1017/1024 |  |
| P/Two-blade / Smear and strike :: repeat | PASS | 4.429e-14 | 3.556e-12 | 1.6% | identical | 902/448/226/228 |  |
| P/Two-blade / Smear and strike :: arp | PASS | 8.291e-13 | 4.466e-11 | 0.9% | identical | 8115/4015/2017/2033 |  |
| P/Two-blade / Split envelopes :: chord | PASS | 5.248e-17 | 9.159e-16 | 39.6% | identical | 2327/697/586/581 |  |
| P/Two-blade / Split envelopes :: repeat | PASS | 2.924e-17 | 4.441e-16 | 46.5% | identical | 520/200/133/128 |  |
| P/Two-blade / Split envelopes :: arp | PASS | 5.604e-17 | 1.610e-15 | 36.0% | identical | 4611/1513/1149/1158 |  |
| P/Two-blade / Chord against scatter :: chord | PASS | 3.976e-17 | 3.886e-16 | 44.4% | identical | 4348/1877/879/864 |  |
| P/Two-blade / Chord against scatter :: repeat | PASS | 2.653e-17 | 2.776e-16 | 54.3% | identical | 968/611/198/192 |  |
| P/Two-blade / Chord against scatter :: arp | PASS | 6.395e-16 | 1.540e-14 | 25.6% | identical | 8662/2908/1736/1728 |  |
| P/Two-blade / Mirror-image spreads :: chord | PASS | 1.190e-14 | 4.127e-13 | 2.6% | identical | 4064/2930/1029/1008 |  |
| P/Two-blade / Mirror-image spreads :: repeat | PASS | 2.078e-14 | 7.561e-13 | 1.4% | identical | 903/653/231/224 |  |
| P/Two-blade / Mirror-image spreads :: arp | PASS | 2.580e-14 | 3.172e-13 | 2.2% | identical | 8080/5795/2030/2014 |  |
| P/Two-blade / Cross-mod pair :: chord | PASS | 4.614e-16 | 1.765e-14 | 8.1% | identical | 3482/1731/875/867 |  |
| P/Two-blade / Cross-mod pair :: repeat | PASS | 3.423e-16 | 1.361e-14 | 11.3% | identical | 773/384/196/192 |  |
| P/Two-blade / Cross-mod pair :: arp | PASS | 5.469e-16 | 2.138e-14 | 5.0% | identical | 6922/3457/1728/1735 |  |
| P/Showcase / Growls / Formant growl :: chord | PASS | 2.501e-16 | 3.303e-15 | 7.9% | identical | 466/0/94/93 |  |
| P/Showcase / Growls / Formant growl :: repeat | PASS | 1.117e-16 | 2.137e-15 | 17.9% | identical | 240/0/46/48 |  |
| P/Showcase / Growls / Formant growl :: arp | PASS | 8.906e-17 | 1.187e-15 | 19.8% | identical | 560/0/113/114 |  |
| P/Showcase / Growls / Formant growl :: legato | PASS | 1.961e-15 | 1.681e-14 | 7.3% | identical | 332/0/67/66 |  |
| P/Showcase / Growls / Feedback snarl :: chord | PASS | 6.757e-17 | 3.331e-16 | 38.3% | identical | 157/36/31/32 |  |
| P/Showcase / Growls / Feedback snarl :: repeat | PASS | 2.378e-17 | 2.220e-16 | 58.3% | identical | 81/22/16/17 |  |
| P/Showcase / Growls / Feedback snarl :: arp | PASS | 7.096e-16 | 8.021e-15 | 31.4% | identical | 189/38/38/38 |  |
| P/Showcase / Growls / Feedback snarl :: legato | PASS | 2.912e-16 | 5.267e-15 | 45.5% | identical | 111/26/22/23 |  |
| P/Showcase / Growls / Cross-mod roar :: chord | PASS | 1.310e-14 | 8.898e-13 | 3.0% | identical | 784/470/157/157 |  |
| P/Showcase / Growls / Cross-mod roar :: repeat | PASS | 6.413e-17 | 3.608e-16 | 40.3% | identical | 403/243/81/80 |  |
| P/Showcase / Growls / Cross-mod roar :: arp | PASS | 6.545e-17 | 3.608e-16 | 37.3% | identical | 942/566/188/189 |  |
| P/Showcase / Growls / Cross-mod roar :: legato | PASS | 4.433e-15 | 2.063e-13 | 22.6% | identical | 557/335/112/112 |  |
| P/Showcase / Growls / Wobble jaw :: chord | PASS | 4.109e-13 | 1.613e-11 | 0.0% | identical | 378/1324/95/93 |  |
| P/Showcase / Growls / Wobble jaw :: repeat | PASS | 6.918e-13 | 2.327e-11 | 0.0% | identical | 186/1310/49/42 |  |
| P/Showcase / Growls / Wobble jaw :: arp | PASS | 4.486e-13 | 1.250e-11 | 0.0% | identical | 455/1308/115/115 |  |
| P/Showcase / Growls / Wobble jaw :: legato | PASS | 5.386e-13 | 1.646e-11 | 0.0% | identical | 269/1311/70/64 |  |
| P/Showcase / FM sines / Glass FM :: chord | PASS | 4.931e-17 | 6.384e-16 | 42.4% | identical | 287/0/287/0 |  |
| P/Showcase / FM sines / Glass FM :: repeat | PASS | 2.947e-17 | 4.233e-16 | 53.5% | identical | 76/0/76/0 |  |
| P/Showcase / FM sines / Glass FM :: arp | PASS | 5.341e-17 | 4.441e-16 | 38.5% | identical | 685/0/685/0 |  |
| P/Showcase / FM sines / FM bell :: chord | PASS | 5.251e-17 | 3.331e-16 | 40.4% | identical | 430/0/289/0 |  |
| P/Showcase / FM sines / FM bell :: repeat | PASS | 3.259e-17 | 3.331e-16 | 53.6% | identical | 76/0/76/0 |  |
| P/Showcase / FM sines / FM bell :: arp | PASS | 5.709e-17 | 3.331e-16 | 37.6% | identical | 904/0/685/0 |  |
| P/Showcase / FM sines / FM pluck pair :: chord | PASS | 4.339e-17 | 4.580e-16 | 42.6% | identical | 2304/0/574/578 |  |
| P/Showcase / FM sines / FM pluck pair :: repeat | PASS | 2.856e-17 | 7.216e-16 | 52.4% | identical | 612/0/152/154 |  |
| P/Showcase / FM sines / FM pluck pair :: arp | PASS | 5.022e-17 | 1.193e-15 | 39.1% | identical | 5474/0/1368/1369 |  |
| P/Showcase / FM sines / Ring of sines :: chord | PASS | 4.428e-15 | 1.270e-13 | 3.0% | identical | 3463/0/1734/0 |  |
| P/Showcase / FM sines / Ring of sines :: repeat | PASS | 5.061e-15 | 1.237e-13 | 3.8% | identical | 922/0/462/0 |  |
| P/Showcase / FM sines / Ring of sines :: arp | PASS | 2.434e-15 | 7.755e-14 | 3.7% | identical | 8239/0/4124/0 |  |
| P/Showcase / Movement / Two clocks :: chord | PASS | 1.519e-15 | 2.674e-14 | 6.4% | identical | 1928/486/486/474 |  |
| P/Showcase / Movement / Two clocks :: repeat | PASS | 1.485e-15 | 3.274e-14 | 3.4% | identical | 759/195/194/186 |  |
| P/Showcase / Movement / Two clocks :: arp | PASS | 6.162e-16 | 1.621e-14 | 15.1% | identical | 6885/1723/1724/1728 |  |
| P/Showcase / Movement / Drift and lock :: chord | PASS | 2.076e-16 | 4.122e-15 | 25.1% | identical | 2300/0/575/575 |  |
| P/Showcase / Movement / Drift and lock :: repeat | PASS | 1.938e-16 | 4.621e-15 | 31.9% | identical | 920/0/230/230 |  |
| P/Showcase / Movement / Drift and lock :: arp | PASS | 1.645e-14 | 3.589e-13 | 7.1% | identical | 8237/0/2062/2057 |  |
| P/Showcase / Movement / Orbiting cuts :: chord | PASS | 2.296e-16 | 8.327e-16 | 21.0% | identical | 2298/1147/579/0 |  |
| P/Showcase / Movement / Orbiting cuts :: repeat | PASS | 1.165e-16 | 4.441e-16 | 31.4% | identical | 922/454/234/0 |  |
| P/Showcase / Movement / Orbiting cuts :: arp | PASS | 1.396e-14 | 2.551e-13 | 10.8% | identical | 8227/4101/2066/0 |  |
| P/Showcase / Movement / Tidal envelopes :: chord | PASS | 3.685e-17 | 3.109e-15 | 58.3% | identical | 1916/627/385/381 |  |
| P/Showcase / Movement / Tidal envelopes :: repeat | PASS | 1.851e-17 | 2.359e-16 | 62.1% | identical | 766/427/155/152 |  |
| P/Showcase / Movement / Tidal envelopes :: arp | PASS | 2.421e-14 | 2.523e-13 | 35.6% | identical | 6864/2804/1366/1374 |  |
| P/Showcase / Leads / Screamer :: chord | PASS | 5.062e-17 | 2.498e-16 | 45.2% | identical | 2428/1256/486/485 |  |
| P/Showcase / Leads / Screamer :: repeat | PASS | 8.024e-17 | 4.441e-16 | 40.2% | identical | 1292/817/258/259 |  |
| P/Showcase / Leads / Screamer :: arp | PASS | 1.165e-14 | 1.244e-13 | 10.9% | identical | 2815/1263/564/562 |  |
| P/Showcase / Leads / Screamer :: legato | PASS | 3.528e-14 | 5.098e-13 | 21.9% | identical | 1705/868/342/340 |  |
| P/Showcase / Leads / Vocal lead :: chord | PASS | 1.543e-16 | 1.311e-15 | 9.6% | identical | 1585/0/318/318 |  |
| P/Showcase / Leads / Vocal lead :: repeat | PASS | 1.125e-16 | 4.441e-16 | 18.1% | identical | 860/0/172/172 |  |
| P/Showcase / Leads / Vocal lead :: arp | PASS | 1.530e-15 | 2.915e-14 | 10.0% | identical | 1798/0/360/360 |  |
| P/Showcase / Leads / Vocal lead :: legato | PASS | 8.662e-15 | 2.104e-13 | 9.6% | identical | 1110/0/222/222 |  |
| P/Showcase / Leads / Hollow square lead :: chord | PASS | 2.781e-17 | 2.776e-16 | 58.8% | identical | 4959/1985/498/495 |  |
| P/Showcase / Leads / Hollow square lead :: repeat | PASS | 3.345e-17 | 2.776e-16 | 58.1% | identical | 2584/1032/259/258 |  |
| P/Showcase / Leads / Hollow square lead :: arp | PASS | 2.569e-17 | 2.220e-16 | 62.1% | identical | 5881/2352/588/588 |  |
| P/Showcase / Leads / Hollow square lead :: legato | PASS | 1.932e-14 | 1.821e-13 | 30.2% | identical | 3507/1404/351/351 |  |
| P/Showcase / Leads / Crunch pluck lead :: chord | PASS | 2.719e-14 | 1.911e-12 | 2.7% | identical | 656/340/164/164 |  |
| P/Showcase / Leads / Crunch pluck lead :: repeat | PASS | 3.333e-14 | 2.869e-12 | 10.8% | identical | 344/186/86/86 |  |
| P/Showcase / Leads / Crunch pluck lead :: arp | PASS | 1.916e-14 | 2.429e-13 | 3.4% | identical | 772/390/193/193 |  |
| P/Showcase / Leads / Crunch pluck lead :: legato | PASS | 3.117e-14 | 2.737e-12 | 4.4% | identical | 462/240/115/116 |  |
| P/Showcase / Pads / Glass horde pad :: chord | PASS | 1.115e-15 | 8.085e-15 | 0.9% | identical | 9663/0/2439/2404 |  |
| P/Showcase / Pads / Glass horde pad :: repeat | PASS | 8.885e-17 | 1.655e-15 | 14.7% | identical | 1382/0/351/344 |  |
| P/Showcase / Pads / Glass horde pad :: arp | PASS | 3.655e-16 | 4.691e-15 | 7.7% | identical | 12363/0/3102/3079 |  |
| P/Showcase / Pads / Undertone cathedral :: chord | PASS | 1.827e-16 | 2.817e-15 | 2.1% | identical | 9435/0/1898/1870 |  |
| P/Showcase / Pads / Undertone cathedral :: repeat | PASS | 2.041e-16 | 3.691e-15 | 0.6% | identical | 1344/0/273/266 |  |
| P/Showcase / Pads / Undertone cathedral :: arp | PASS | 7.256e-17 | 7.026e-16 | 4.5% | identical | 12011/0/2414/2379 |  |
| P/Showcase / Pads / Cross-mod shimmer :: chord | PASS | 4.294e-14 | 4.276e-13 | 0.0% | identical | 4307/0/2152/0 |  |
| P/Showcase / Pads / Cross-mod shimmer :: repeat | PASS | 1.692e-17 | 1.284e-16 | 15.0% | identical | 613/0/306/0 |  |
| P/Showcase / Pads / Cross-mod shimmer :: arp | PASS | 4.924e-14 | 1.051e-12 | 1.8% | identical | 5487/0/2744/0 |  |
| P/Showcase / Pads / Breathing pad :: chord | PASS | 5.145e-17 | 7.112e-16 | 24.4% | identical | 6485/0/1629/1628 |  |
| P/Showcase / Pads / Breathing pad :: repeat | PASS | 1.967e-17 | 5.065e-16 | 49.6% | identical | 921/0/228/232 |  |
| P/Showcase / Pads / Breathing pad :: arp | PASS | 1.315e-16 | 1.579e-15 | 23.3% | identical | 8251/0/2063/2061 |  |
| P/Showcase / Interplay / Fold over sync :: chord | PASS | 4.713e-17 | 3.331e-16 | 47.0% | identical | 1744/876/441/437 |  |
| P/Showcase / Interplay / Fold over sync :: repeat | PASS | 3.150e-17 | 2.220e-16 | 53.4% | identical | 387/194/99/96 |  |
| P/Showcase / Interplay / Fold over sync :: arp | PASS | 4.568e-17 | 3.331e-16 | 45.7% | identical | 3474/1740/870/870 |  |
| P/Showcase / Interplay / Sync over fold :: chord | PASS | 4.671e-17 | 3.331e-16 | 47.7% | identical | 1744/876/441/437 |  |
| P/Showcase / Interplay / Sync over fold :: repeat | PASS | 3.103e-17 | 3.608e-16 | 53.4% | identical | 387/194/99/96 |  |
| P/Showcase / Interplay / Sync over fold :: arp | PASS | 4.414e-17 | 3.331e-16 | 45.9% | identical | 3474/1740/870/870 |  |
| P/Showcase / Interplay / Occlusion sweep :: chord | PASS | 3.730e-17 | 3.886e-16 | 44.2% | identical | 2190/539/441/443 |  |
| P/Showcase / Interplay / Occlusion sweep :: repeat | PASS | 2.606e-17 | 4.788e-16 | 47.6% | identical | 488/197/99/100 |  |
| P/Showcase / Interplay / Occlusion sweep :: arp | PASS | 3.928e-17 | 3.331e-16 | 45.0% | identical | 4346/800/871/870 |  |
| P/Showcase / Interplay / Ring on ring :: chord | PASS | 5.441e-16 | 1.064e-14 | 11.4% | identical | 2324/1160/586/584 |  |
| P/Showcase / Interplay / Ring on ring :: repeat | PASS | 3.610e-17 | 4.441e-16 | 47.3% | identical | 517/256/132/129 |  |
| P/Showcase / Interplay / Ring on ring :: arp | PASS | 2.369e-15 | 4.230e-14 | 6.7% | identical | 4622/2317/1158/1160 |  |
| P/Showcase / Interplay / Crushed burst :: chord | PASS | 4.192e-17 | 3.331e-16 | 52.1% | identical | 1751/878/441/441 |  |
| P/Showcase / Interplay / Crushed burst :: repeat | PASS | 3.230e-17 | 3.331e-16 | 56.7% | identical | 390/195/99/99 |  |
| P/Showcase / Interplay / Crushed burst :: arp | PASS | 2.701e-15 | 2.459e-14 | 20.2% | identical | 3476/1740/869/870 |  |
| P/Showcase / Interplay / Collision chirps :: chord | PASS | 1.526e-16 | 5.551e-16 | 22.6% | identical | 1746/874/439/437 |  |
| P/Showcase / Interplay / Collision chirps :: repeat | PASS | 6.664e-17 | 3.331e-16 | 35.5% | identical | 384/197/99/94 |  |
| P/Showcase / Interplay / Collision chirps :: arp | PASS | 1.379e-16 | 5.551e-16 | 21.2% | identical | 3468/1739/867/868 |  |
| P/Showcase / Interplay / Collision bite :: chord | PASS | 1.875e-16 | 1.041e-15 | 10.3% | identical | 2182/0/441/433 |  |
| P/Showcase / Interplay / Collision bite :: repeat | PASS | 1.158e-16 | 4.996e-16 | 14.2% | identical | 487/0/99/96 |  |
| P/Showcase / Interplay / Collision bite :: arp | PASS | 1.674e-16 | 6.661e-16 | 10.6% | identical | 4330/0/870/855 |  |
| P/Showcase / Oddities / Feedback choir :: chord | PASS | 1.877e-13 | 3.438e-12 | 0.2% | identical | 1452/0/732/0 |  |
| P/Showcase / Oddities / Feedback choir :: repeat | PASS | 2.113e-13 | 3.602e-12 | 0.3% | identical | 582/0/294/0 |  |
| P/Showcase / Oddities / Feedback choir :: arp | PASS | 3.824e-14 | 9.850e-13 | 0.4% | identical | 5194/0/2607/0 |  |
| P/Showcase / Oddities / Crushed bells :: chord | PASS | 5.650e-17 | 3.886e-16 | 34.4% | identical | 2895/0/727/723 |  |
| P/Showcase / Oddities / Crushed bells :: repeat | PASS | 5.027e-17 | 4.996e-16 | 42.2% | identical | 1157/0/292/288 |  |
| P/Showcase / Oddities / Crushed bells :: arp | PASS | 7.474e-17 | 6.661e-16 | 29.2% | identical | 10372/0/2600/2586 |  |
| P/Showcase / Oddities / Clockwork :: chord | PASS | 4.344e-15 | 9.333e-14 | 3.0% | identical | 3390/1691/849/0 |  |
| P/Showcase / Oddities / Clockwork :: repeat | PASS | 2.024e-17 | 2.220e-16 | 50.9% | identical | 1356/679/342/0 |  |
| P/Showcase / Oddities / Clockwork :: arp | PASS | 1.073e-15 | 2.043e-14 | 26.5% | identical | 12112/6057/3029/0 |  |
| T/mode 0 :: chord | PASS | 3.760e-17 | 3.331e-16 | 49.5% | identical | 870/869/436/0 |  |
| T/mode 1 :: chord | PASS | 3.810e-17 | 3.955e-16 | 49.1% | identical | 870/870/436/0 |  |
| T/mode 2 :: chord | PASS | 3.885e-17 | 3.331e-16 | 48.1% | identical | 870/769/436/0 |  |
| T/mode 3 :: chord | PASS | 3.768e-17 | 3.331e-16 | 49.6% | identical | 870/0/436/0 |  |
| T/mode 4 :: chord | PASS | 4.217e-17 | 4.441e-16 | 44.2% | identical | 870/0/436/0 |  |
| T/mode 5 :: chord | PASS | 3.535e-17 | 3.331e-16 | 49.6% | identical | 870/869/436/0 |  |
| T/mode 6 :: chord | PASS | 3.690e-17 | 2.776e-16 | 50.5% | identical | 870/434/436/0 |  |
| T/hot 0 :: chord | PASS | 3.975e-17 | 3.331e-16 | 47.0% | identical | 870/0/436/0 |  |
| T/hot 1 :: chord | PASS | 3.711e-17 | 3.331e-16 | 49.4% | identical | 870/0/436/0 |  |
| T/hot 2 :: chord | PASS | 3.760e-17 | 3.331e-16 | 49.5% | identical | 870/869/436/0 |  |
| T/hot 3 :: chord | PASS | 4.243e-17 | 3.331e-16 | 49.5% | identical | 870/1303/436/0 |  |
| T/hot 4 :: chord | PASS | 3.717e-17 | 3.331e-16 | 50.9% | identical | 870/869/436/0 |  |
| T/hot 6 :: chord | PASS | 5.251e-17 | 3.331e-16 | 34.0% | identical | 870/0/436/0 |  |
| T/base 0 :: chord | PASS | 3.760e-17 | 3.331e-16 | 49.5% | identical | 870/869/436/0 |  |
| T/base 1 :: chord | PASS | 2.718e-17 | 1.110e-16 | 62.7% | identical | 870/869/436/0 |  |
| T/base 2 :: chord | PASS | 2.179e-17 | 1.110e-16 | 63.6% | identical | 1305/869/436/0 |  |
| T/base 3 :: chord | PASS | 3.735e-17 | 2.220e-16 | 60.5% | identical | 1740/869/436/0 |  |
| T/base 4 :: chord | PASS | 2.179e-17 | 2.220e-16 | 63.4% | identical | 1305/869/436/0 |  |
| T/fm mode 1 type 0 :: chord | PASS | 3.696e-17 | 3.331e-16 | 46.4% | identical | 870/436/436/0 |  |
| T/fm mode 1 type 1 :: chord | PASS | 4.288e-17 | 4.441e-16 | 43.9% | identical | 870/869/436/0 |  |
| T/fm mode 2 type 0 :: chord | PASS | 4.039e-17 | 3.886e-16 | 45.9% | identical | 870/745/436/0 |  |
| T/fm mode 2 type 1 :: chord | PASS | 8.090e-17 | 4.441e-16 | 21.4% | identical | 870/730/436/0 |  |
| T/fm mshape 0 :: chord | PASS | 4.243e-17 | 3.331e-16 | 42.8% | identical | 870/855/436/0 |  |
| T/fm mshape 1 :: chord | PASS | 4.056e-17 | 3.331e-16 | 46.3% | identical | 870/730/436/0 |  |
| T/fm mshape 2 :: chord | PASS | 3.961e-17 | 3.331e-16 | 46.8% | identical | 870/1431/436/0 |  |
| T/fm mshape 3 :: chord | PASS | 3.710e-17 | 3.331e-16 | 49.7% | identical | 870/1353/436/0 |  |
| T/fm mshape 4 :: chord | PASS | 3.914e-17 | 3.331e-16 | 46.4% | identical | 870/729/436/0 |  |
| T/fm mshape 5 :: chord | PASS | 3.896e-13 | 2.716e-12 | 3.0% | identical | 870/750/436/0 |  |
| T/fm mshape 6 :: chord | PASS | 5.115e-17 | 3.331e-16 | 35.2% | identical | 870/1152/436/0 |  |
| T/fm mshape 7 :: chord | PASS | 2.220e-12 | 1.897e-10 | 9.6% | identical | 870/851/436/0 |  |
| T/fm Hz units :: chord | PASS | 3.995e-17 | 3.331e-16 | 46.0% | identical | 870/775/436/0 |  |
| T/lock width :: chord | PASS | 3.660e-17 | 3.331e-16 | 49.8% | identical | 870/2608/436/0 |  |
| T/lock Hz :: chord | PASS | 3.723e-17 | 3.331e-16 | 50.3% | identical | 870/676/436/0 |  |
| T/mirror 1 :: chord | PASS | 3.736e-17 | 3.331e-16 | 50.1% | identical | 870/869/436/0 |  |
| T/mirror 2 :: chord | PASS | 3.849e-17 | 3.331e-16 | 48.3% | identical | 1742/1741/436/0 |  |
| T/mirror 3 :: chord | PASS | 4.569e-17 | 3.331e-16 | 49.9% | identical | 1742/1741/436/0 |  |
| T/frame swarm :: chord | PASS | 9.826e-16 | 4.299e-14 | 33.4% | identical | 1440/1446/720/0 |  |
| T/hard edges :: chord | PASS | 3.810e-17 | 3.331e-16 | 48.1% | identical | 870/869/436/0 |  |
| T/crush hard (BLEP steps) :: chord | PASS | 3.690e-17 | 2.776e-16 | 50.5% | identical | 870/434/436/0 |  |
| T/crush slew :: chord | PASS | 3.608e-17 | 3.331e-16 | 50.6% | identical | 870/0/436/0 |  |
| T/noise S&H :: chord | PASS | 5.301e-17 | 3.331e-16 | 43.2% | identical | 1449/0/725/0 |  |
| T/rotate + :: chord | PASS | 3.632e-17 | 3.331e-16 | 49.0% | identical | 870/868/437/0 |  |
| T/rotate - :: chord | PASS | 3.694e-17 | 3.331e-16 | 50.1% | identical | 872/873/436/0 |  |
| T/rotate spread :: chord | PASS | 5.448e-17 | 5.551e-16 | 42.5% | identical | 1449/1449/725/0 |  |
| T/rotate free clock :: chord | PASS | 3.460e-17 | 3.331e-16 | 49.1% | identical | 869/868/435/0 |  |
| T/rotate then home :: chord | PASS | 3.671e-17 | 3.331e-16 | 49.0% | identical | 870/867/436/0 |  |
| T/b2 mode 0 :: chord | PASS | 3.546e-17 | 3.331e-16 | 52.0% | identical | 1743/1305/436/437 |  |
| T/b2 mode 1 :: chord | PASS | 3.823e-17 | 3.331e-16 | 49.5% | identical | 1743/1743/436/437 |  |
| T/b2 mode 2 :: chord | PASS | 3.802e-17 | 3.331e-16 | 46.7% | identical | 1743/1313/436/437 |  |
| T/b2 mode 3 :: chord | PASS | 3.492e-17 | 3.331e-16 | 51.7% | identical | 1743/869/436/437 |  |
| T/b2 mode 5 :: chord | PASS | 3.442e-17 | 3.331e-16 | 49.8% | identical | 1743/1305/436/437 |  |
| T/b2 mode 6 :: chord | PASS | 3.308e-17 | 3.331e-16 | 52.2% | identical | 1743/869/436/437 |  |
| T/b2 own fm :: chord | PASS | 1.726e-12 | 8.073e-11 | 1.6% | identical | 1743/1115/436/437 |  |
| T/b2 sine-to-saw own mod :: chord | PASS | 5.345e-17 | 4.441e-16 | 32.6% | identical | 1743/1044/436/437 |  |
| T/b2 twin mirror :: chord | PASS | 4.204e-17 | 3.331e-16 | 44.1% | identical | 2612/869/436/437 |  |
| T/b2 inverted twin :: chord | PASS | 5.079e-17 | 3.331e-16 | 44.2% | identical | 3484/1741/436/437 |  |
| T/b2 frame :: chord | PASS | 8.053e-17 | 2.942e-15 | 37.1% | identical | 2905/1449/725/736 |  |
| T/b2 own clock :: chord | PASS | 5.336e-17 | 3.331e-16 | 39.9% | identical | 2898/1452/724/727 |  |
| T/b2 own spreads :: chord | PASS | 1.923e-14 | 3.004e-13 | 1.0% | identical | 3486/1736/870/879 |  |
| T/b2 envelope :: chord | PASS | 4.418e-17 | 3.331e-16 | 45.9% | identical | 1740/869/436/435 |  |
| T/b2 Hz lock :: chord | PASS | 4.339e-17 | 3.331e-16 | 39.8% | identical | 1743/869/436/437 |  |
| T/serial l0.5 o0 :: chord | PASS | 4.010e-17 | 3.331e-16 | 47.0% | identical | 1743/869/436/437 |  |
| T/serial l0.5 o1 :: chord | PASS | 4.010e-17 | 3.331e-16 | 47.0% | identical | 1743/869/436/437 |  |
| T/serial l1 o0 :: chord | PASS | 4.010e-17 | 3.331e-16 | 47.0% | identical | 1743/869/436/437 |  |
| T/serial l1 o1 :: chord | PASS | 4.010e-17 | 3.331e-16 | 47.0% | identical | 1743/869/436/437 |  |
| T/serial twins :: chord | PASS | 5.068e-17 | 4.441e-16 | 44.8% | identical | 3484/1741/436/437 |  |
| T/serial crush over sync :: chord | PASS | 3.413e-17 | 3.331e-16 | 52.1% | identical | 1743/869/436/437 |  |
| T/collision pitch :: chord | PASS | 1.983e-16 | 7.772e-16 | 10.2% | identical | 1743/1304/438/437 |  |
| T/collision bite :: chord | PASS | 2.682e-16 | 9.298e-16 | 10.3% | identical | 1743/1281/438/437 |  |
| T/collision upper b1 :: chord | PASS | 3.344e-16 | 1.943e-15 | 10.7% | identical | 1743/3047/438/437 |  |
| T/interplay sweep :: chord | PASS | 4.030e-17 | 3.331e-16 | 47.2% | identical | 1743/869/436/437 |  |
| T/dc 0 :: chord | PASS | 4.526e-17 | 3.886e-16 | 51.0% | identical | 874/874/436/0 |  |
| T/dc 1 :: chord | PASS | 1.890e-16 | 7.216e-16 | 13.9% | identical | 874/874/436/0 |  |
| T/dc 2 :: chord | PASS | 3.176e-17 | 3.331e-16 | 50.2% | identical | 874/874/436/0 |  |
| T/os 1 :: chord | PASS | 3.631e-17 | 3.331e-16 | 54.6% | identical | 1743/869/436/437 |  |
| T/os 2 :: chord | PASS | 4.046e-17 | 3.331e-16 | 46.9% | identical | 1743/869/436/437 |  |
| T/os 4 :: chord | PASS | 5.554e-17 | 5.551e-16 | 41.5% | identical | 1743/869/436/437 |  |
| T/aa off :: chord | PASS | 3.738e-17 | 3.331e-16 | 49.7% | identical | 0/0/436/0 |  |
| T/law 0 :: chord | PASS | 1.401e-14 | 2.745e-13 | 0.8% | identical | 1736/1596/870/0 |  |
| T/law 1 :: chord | PASS | 1.947e-14 | 2.965e-13 | 0.8% | identical | 1738/1416/870/0 |  |
| T/law 2 :: chord | PASS | 2.265e-14 | 3.127e-13 | 0.3% | identical | 1739/1735/871/0 |  |
| T/law 3 :: chord | PASS | 1.559e-14 | 2.856e-13 | 1.0% | identical | 1736/1421/870/0 |  |
| T/law 4 :: chord | PASS | 3.613e-14 | 5.356e-13 | 0.5% | identical | 1767/1448/887/0 |  |
| T/rule 1 :: chord | PASS | 2.424e-14 | 3.007e-13 | 0.5% | identical | 1737/3916/870/0 |  |
| T/rule 2 :: chord | PASS | 1.540e-14 | 3.001e-13 | 1.6% | identical | 1737/720/870/0 |  |
| T/rule 3 :: chord | PASS | 2.716e-14 | 2.997e-13 | 0.4% | identical | 1737/8719/870/0 |  |
| T/rule 4 :: chord | PASS | 2.089e-14 | 3.077e-13 | 0.6% | identical | 1737/2319/870/0 |  |
| T/rule 5 :: chord | PASS | 2.069e-14 | 2.861e-13 | 0.7% | identical | 1737/2319/870/0 |  |
| T/rule 6 :: chord | PASS | 2.328e-14 | 2.921e-13 | 0.5% | identical | 1737/3630/870/0 |  |
| T/rule 7 :: chord | PASS | 2.504e-14 | 3.072e-13 | 0.5% | identical | 1737/4647/870/0 |  |
| T/rule 8 :: chord | PASS | 2.347e-14 | 2.919e-13 | 0.5% | identical | 1737/3484/870/0 |  |
| T/rule 9 custom :: chord | PASS | 5.485e-17 | 2.054e-15 | 47.6% | identical | 2028/3482/1013/0 |  |
| T/rule snapped :: chord | PASS | 2.141e-14 | 2.935e-13 | 0.5% | identical | 1737/2612/870/0 |  |
| T/spread snapped :: chord | PASS | 1.638e-14 | 2.802e-13 | 1.0% | identical | 1737/1594/870/0 |  |
| T/spread snapped -half :: chord | PASS | 3.712e-17 | 3.331e-16 | 51.7% | identical | 581/437/289/0 |  |
| T/blade envelope :: chord | PASS | 5.172e-17 | 2.665e-15 | 48.0% | identical | 866/1168/435/0 |  |
| T/cross-mod :: chord | PASS | 1.914e-16 | 7.772e-16 | 13.2% | identical | 1449/1449/725/0 |  |
| T/feedback :: chord | PASS | 2.335e-16 | 7.772e-16 | 12.3% | identical | 290/290/145/0 |  |
| T/splay K<0 :: chord | PASS | 2.698e-15 | 3.864e-14 | 3.4% | identical | 1743/1745/870/0 |  |
| T/strong lock :: chord | PASS | 6.154e-15 | 3.266e-13 | 9.4% | identical | 2026/2023/1018/0 |  |
| T/cycles coupling :: chord | PASS | 1.403e-15 | 1.932e-14 | 7.6% | identical | 1448/1445/727/0 |  |
| T/phase random :: chord | PASS | 4.462e-17 | 3.331e-16 | 41.3% | identical | 1449/1448/724/0 |  |
| T/phase zero :: chord | PASS | 5.367e-17 | 3.331e-16 | 46.2% | identical | 1444/1443/724/0 |  |
| T/pan order :: chord | PASS | 4.615e-17 | 5.551e-16 | 48.3% | identical | 2028/2030/1013/0 |  |
| T/bend :: chord | PASS | 4.239e-17 | 3.331e-16 | 50.3% | identical | 1035/1033/519/0 |  |
| T/sweep k w c :: chord | PASS | 3.621e-17 | 3.331e-16 | 49.5% | identical | 866/2229/433/0 |  |
| T/sweep swarm :: chord | PASS | 4.534e-17 | 3.331e-16 | 45.7% | identical | 1188/1187/596/0 |  |
| T/sr 44100 :: arp | PASS | 3.862e-17 | 3.331e-16 | 47.2% | identical | 3777/1884/947/944 |  |
| T/poly 2 steals :: arp | PASS | 2.782e-17 | 2.220e-16 | 53.6% | identical | 864/863/433/0 |  |
| T/mono retrig :: legato | PASS | 5.363e-15 | 1.741e-13 | 30.9% | identical | 263/263/132/0 |  |
| T/legato glide always :: legato | PASS | 1.265e-14 | 3.425e-13 | 28.8% | identical | 250/249/126/0 |  |
| T/restrike after full release :: restrike | PASS | 1.739e-17 | 2.220e-16 | 77.6% | identical | 84/84/42/0 |  |
| T/blade 1 off (w 0) :: chord | PASS | 3.817e-17 | 3.331e-16 | 49.2% | identical | 0/0/0/0 |  |
| T/blade 2 off (w2 0) :: chord | PASS | 3.760e-17 | 3.331e-16 | 49.5% | identical | 870/869/436/0 |  |
| T/collision swept off :: chord | PASS | 3.083e-16 | 1.193e-15 | 7.4% | identical | 1743/1304/438/437 |  |
| T/b2 own clock at rest :: chord | PASS | 5.142e-17 | 3.331e-16 | 42.1% | identical | 2906/1449/726/732 |  |
| T/b2 own clock homing :: chord | PASS | 5.369e-17 | 3.331e-16 | 40.3% | identical | 2903/1452/724/731 |  |
| T/mono glide 1 ms :: legato | PASS | 2.412e-17 | 2.220e-16 | 57.8% | identical | 279/279/140/0 |  |
| T/settle skipped (K 0) :: chord | PASS | 4.007e-17 | 3.331e-16 | 39.5% | identical | 1451/1451/725/0 |  |
| T/gate mode 6 collision :: chord | PASS | 1.341e-16 | 6.106e-16 | 26.8% | identical | 1743/2177/438/437 |  |
| T/b2 lock 1 FM collision :: chord | PASS | 3.205e-16 | 1.166e-15 | 6.5% | identical | 1743/4257/438/437 |  |
| T/retune message :: retune | PASS | 5.731e-15 | 1.568e-13 | 17.6% | identical | 485/484/244/0 |  |
| T/panic message :: panic | PASS | 3.261e-17 | 3.331e-16 | 52.0% | identical | 870/869/436/0 |  |
| T/polyMode switch mid-note :: chord | PASS | 3.097e-17 | 3.331e-16 | 52.4% | identical | 976/976/489/0 |  |
| T/modX wrap :: long | PASS | 4.165e-17 | 3.331e-16 | 44.1% | identical | 102493/89803/25623/25625 |  |
