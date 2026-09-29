# b345-metrics-fix-refit — the aliasing metric explained and fixed, a noise measure beside flatness, the sample re-measured in place, and the ratings re-fitted without re-listening

- **Queue item:** B345. Read verbatim from `origin/lead-records-137:ROADMAP.md` (records PR #841), beside B316, B340, B342, B344 and B346. The human: "Let's fix the metrics first, then re-fit".
- **Why:** B344's fit rested on an aliasing metric with two unexplained anomalies (B342) and a noise metric that did not track the ears. The fix had to happen at the cause, with controls that prove each fix, before any threshold is proposed.

## Root causes

1. **Long windows went blind (B342(1)).**
   - **Cause:** `aliasing()` averaged the WHOLE window into one Welch spectrum and only then compared it with the reference. Partials that move more slowly than one window, but far within a long one, smear into an envelope. Weak aliases sitting between the partials then hide under that envelope. Examples: a glide, or the engine's slow drift (broad#511: 88 cents of drift under a 1.6 s attack).
   - **Pitch smearing, correctly stated:** B340's vibrato test (5 Hz) smears inside one 0.25 s window as much as over many, so it could not see this.
   - **Proof on constructed signals (origin/main code):**
     - a naive saw gliding 2 st/s at A3 reads −30.7 / −35.4 / −39.3 / −40.7 dB at 0.25 / 0.5 / 1.0 / 1.45 s;
     - at E5 it reads −19.6 → −29.0;
     - a steady naive saw reads −18.9 at every length, because the bug needs moving partials.
   - **Fix:** compare 2-frame UNITS and sum the counted power. A 0.25 s window is exactly one unit, so a gauntlet window's unit IS its old spectrum. The new readings of the same glides are −30.9 / −30.7 / −30.7 / −30.7 and −19.6 / −20.2 / −20.1 / −20.1.
   - **Invariant:** stated tolerance 1 dB (metrics_check M11). The must-fail control M11c runs the same comparison in the pre-B345 order and gets a 9.9 dB spread.
2. **Noise read as folding (B342(2)).**
   - **The hypothesis holds, for a different reason than guessed.** Two independent realisations of the SAME band-limited seeded noise (no folding by construction) read −23 to −31 dB in a third to a half of their 0.25 s windows under origin/main. The worst window, which is the page's rule, read −25.9 / −27.0 / −28.6 dB on three of four seeds, which is above the fitted cut.
   - **Mechanism:** each test bin was compared with the reference's ±3-bin maximum, and a chance dip in the reference let the test's chance peak count.
   - **Fix:** the reference level is max(its ±3-bin max, its noise floor). The floor comes from `noiseFloor()`, the same estimator as noiseDb. After the fix: −120 on all four seeds. The planted tone 15 dB under the noise still reads −14 to −19 in every window.
   - **broad#828 itself is NOT this case:**
     - a same-os, other-seed reference reads −120 at A1, so this is not decorrelation;
     - its 1x render carries a band near 0.2 × the internal rate (9.6 kHz at os 1, 19.3 kHz at os 2), strongest in the attack;
     - that is real, rate-locked content the 4x render lacks, so the reading stands (committed −5.1 → −5.3 dB).
3. **Silence read as total aliasing.** B340 noted an outlier: edge#145's sweep read 0.0 dB. Its tail decays to subnormal samples (−500 to −930 dB) against a 4x reference that reached exact zero, so every bin "exceeded". Units under −90 dBFS in band are now not measured (M13).

## What changed

- `tools/patchspace/metrics.mjs`:
  - `spectrum()` keeps per-frame powers `F`; `P` is bit-identical.
  - New `noiseFloor()`: the 10th percentile over ±32 bins ÷ −ln 0.9.
  - New `aperiodic()` → `noiseDb`.
  - `aliasing()` as above.
  - `analyse()` adds `noiseDb`.
  - The header states each method, the evidence and the limits.
- `tools/patchspace/metrics_check.mjs`: 9 new rows (M11, M11c, M12, M12h, M13, N1–N4) and 3 INFO rows. Every existing row is kept and unchanged. ~5 s, still `WIRED: ./verify fast`.
- `tools/patchspace/listening_sample.mjs`:
  - `--remeasure --note` rewrites the SAME items' metrics in place. Patches, seeds, order, strata labels and exclusions stay; ph/fp are asserted unchanged. The file records the replaced id in `remeasured`.
  - `KEEP` gains `noiseDb`.
  - `loadPage()` is now the one loader of the page's PURE block; `listening_pass_check`'s inline copy is gone.
- `docs/design/listening-pass.json`: re-measured, id `cec81302 → 379d4349`. See Committed-number diffs.
- `docs/design/listening-pass.html`:
  - `measureWindows` records `noiseDb`.
  - `sameSoundsAs()` treats the file's earlier ids as the same sounds: the human's v1 pass on cec81302 still exports byte for byte, and a v2 store on cec81302 keeps its answers while its segments are re-measured (`load()`).
  - The header note and the long-window comment are updated (the mystery is explained).
  - `lab-review` meta now reads B324 + B340 + B345.
- `tools/patchspace/calibrate.mjs`:
  - `SEG_FIELDS` += `noiseDb`.
  - `PAIRS_B345` (noiseDb ↔ noisy, beside flatness), `looAccuracy()`, `remeasureExport()`, and an optional `analyse(sample, exports, {pairs, loo})`. The default output is unchanged.
  - A remeasured-aware sample-id warning.
  - `--remeasure` CLI: re-renders every v2-rated program in Node through the page's PURE block and prints BEFORE → AFTER, drift and sensitivity. It writes nothing.
- `tools/patchspace/listening_pass_check.mjs` (58/58, 19 must-fail controls; see Expectations changed for the pins):
  - uses `loadPage`;
  - the T7 v1-export pin is now reached via `cec81302`;
  - new rows: T7 same-sounds (with a control), and T8 re-measure (segments replaced, drift named by segment and field as a control, noiseDb row plus LOO).
- `tools/patchspace/gauntlet.mjs`: header note only. `THRESH` is UNTOUCHED.
- `tools/patchspace/README.md`: rewritten for v2 and B345; it no longer describes v1's phrase.

## Committed-number diffs (same seeds, same exclusions; only aliasDb moved, plus noiseDb added)

- 23 of 36 items' aliasDb changed; 13 are unchanged, all of them tonal or clean.
- Moves to −120, noise-like content the floor now discounts:
  - broad#857 −16.5 → −120;
  - broad#692 −70.2 → −120;
  - broad#70 −63.0 → −120.
- Large drops:
  - broad#383 −14.0 → −29.9;
  - broad#292 −34.5 → −40.1;
  - broad#511 −29.9 → −34.7;
  - broad#754 −23.0 → −28.7;
  - broad#730 −22.2 → −27.0;
  - broad#595 −16.1 → −19.7;
  - broad#689 −35.8 → −39.0;
  - edge#145 −36.5 → −39.6.
- The others moved by ≤ 2 dB.
- Roughness, root, flatness, level, clicks, fp and ph are identical.

## The re-fit PROPOSAL (nothing applied)

Full tables are in the report. The v2 basis is the worst heard segment; LOO = leave-one-out accuracy.

| v2, 34 ratings | AUC before → after | cut before → after | agree@cut | LOO | provisional agree |
|---|---|---|---|---|---|
| aliasDb ↔ aliased | 0.91 → 0.91 | −27.0 → −26.8 | 0.85 → 0.85 | 0.79 → 0.79 | 0.82 → 0.79 |
| roughness ↔ rough | 0.86 → 0.86 | 0.120 → 0.120 | 0.88 | 0.85 | 0.85 |
| rootPresence ↔ root unclear | 0.86 → 0.86 | 0.616 | 0.91 | 0.85 | 0.85 |
| flatness ↔ noisy | 0.83 → 0.83 | 0.006 | 0.88 | 0.82 | 0.50 |
| **noiseDb ↔ noisy** (new) | — → **0.85** | — → **−21.5 dB** | **0.91** | **0.88** | (no provisional) |
| "not usable" AUCs | 0.50–0.63 → 0.49–0.62 (noiseDb 0.60) | | | | |

- **Node, old metrics** reproduces "before" exactly. The runtime does not move the fits; the metric change does.
- **Drift:** 8 ratings drift (Chrome vs Node, on fields whose method did not change, at the page's tolerance): broad#383, #466, #706, #140, #511 (and its repeat), #828, #595. They are kept, per the v2 rule.
- **Sensitivity without the drifted ratings (26):** noiseDb 0.99 vs flatness 0.97; aliasDb 0.90 → 0.89, with the cut moving to −35.5.
- **v1 (committed A3/E5 numbers):**
  - aliasDb AUC 0.75 → 0.75, cut −35.1 → −32.3, agreement at the provisional −30: 0.71 → 0.76, "not usable" AUC 0.74 → 0.67;
  - noiseDb 0.84 (LOO 0.74) vs flatness 0.85 (LOO 0.79): **on v1, flatness is marginally better.**
- **noiseDb has a logistic trend** (50% at −22.2 dB, consistent with its ROC cut); flatness has none.
- **Where the worst v2 segment comes from, before → after:**
  - aliasDb: sweep 10 → 8, A5 10 → 11, A1 9 → 9, E5 2 → 3;
  - noiseDb: A1 18, A2 5, A5 4.
- **Plainly:**
  - **Improved:** aliasing no longer depends on window length, no longer reads decorrelated noise or silence, and the committed-basis aliasDb AUC rose to 0.96 on v2. On v2, noiseDb beats flatness slightly (AUC +0.02, LOO +0.06) and has a physical scale (dB of aperiodic power). Its cut near −21.5 dB means "a continuous floor within about 20 dB of the tone".
  - **Did not improve:** the v2 aliasing agreement with the ears; the "not usable" prediction; and the confounding of noisy with rough. noiseDb predicts "rough" with AUC 0.85 too, and 18 of 22 noisy answers are also rough. The noise gain is within the noise of 34 ratings, and on v1 it does not beat flatness.

## Evidence

- **VERIFIED `./verify fast` exit 0 at 35a47f3:** `{"target":"fast","exit":0,"git":"35a47f3","ts":"2026-09-28T23:37:05Z"}`. Its line: `metrics_check: GREEN — 10 metric rows + 9 B345 rows (aliasing window-length invariance, decorrelation, silence; noiseDb) on constructed signals, 3 INFO rows, + 3 engine controls`.
- **VERIFIED `./verify full` exit 0 at 35a47f3:** `{"target":"full","exit":0,"git":"35a47f3","ts":"2026-09-28T23:42:47Z"}`. Its line: `listening_pass_check.mjs: GREEN — listening pass: 58/58 checks, 19 must-fail controls, 36 patches, 46 s`. This trace's own commit was re-verified fast; the hash is in the PR.
- **VERIFIED `node tools/patchspace/listening_pass_check.mjs`:** GREEN, 58/58, 19 must-fail controls, 49 s (working tree = the commit).
- **VERIFIED pins:**
  - 1726739's calibrate on the OLD file gives 60ae017d 791cc92a 921c14fd (the old pins, a control);
  - on the NEW file, both 1726739's and the branch's calibrate give 1726cdb7 8b35c25a 369a01c9.
- **VERIFIED Chrome `?xverify=1`** (headless, served on 8345, on the re-measured sample):
  - self-checks 6/6; every detector row identical to Node; 36/36 full programs pass the v2 schema;
  - **35/36** measured windows within tolerance: broad#383 is OUTSIDE (E5 aliasDb Δ4.1 dB against a 3 dB tolerance; other fields Δ ≤ 0.02).
- **VERIFIED the page's `load()` migration:** extracted and run against a fake localStorage (scratch). A cec81302 store keeps its answers, and its segments, heard marks and revealed flag are reset. The current store is untouched. 03c97d3e and another phrase load nothing.
- **Scratch (not committed):** `scratchpad/b345/`
  - `b345-report.html` and `b345-report.png`: the visual report;
  - `remeasure.out`: calibrate `--remeasure` on the human's two exports, 230 s;
  - `exp*.mjs`: the investigations: estimator variants, the os-convergence tests (4x vs 16x) and the seed controls.
- **Not verified:** nobody listened to anything new, and the page's migrated-v2 path was not exercised in a real browser with the human's store.

## Evidence consulted

- ROADMAP B316 / B340 / B342 / B344 / B345 / B346.
- `traces/2026-09-28-b340-listening-pass-v2.md`.
- The headers of metrics, calibrate, gauntlet, listening_sample, listening_pass_check and the page.
- `reference/scalpel/prototype/razor-core.js` setOS / kCap and `docs/design/scalpel-horde-engine.js` os use (read only).
- The human's two exports: read only, aggregates only.

## Alternatives rejected

- **The reference's 20th-percentile floor ×100 (RF).** It silenced 511's and 169's real aliasing (their 4x-vs-16x readings converge, so the aliasing is folding). AUC 0.85.
- **A line test on the test's own floor.** It read a dense naive-saw alias field as −120.
- **A time-neighbourhood reference max.** Its noise floor was still −40 at 1.45 s, with a worse invariance.
- **An os-convergence (16x) estimator.** It needs a third render and was not needed after the floor.
- **Re-selecting the sample on the new numbers.** That would orphan the human's ratings; the sample was re-measured in place instead.
- **Relaxing the page's TOL for broad#383.** Not done: a gate is never relaxed.
- **Adding noiseDb to PAIRS.** Not done: it would change the pinned v1 output. PAIRS_B345 is used by the re-fit only.
- **Regenerating the P3 gauntlet report and the fidelity audit report.** Not done: they are dated reports of runs, 18 minutes and more. Both are noted as carrying pre-B345 aliasing.

## Expectations changed (said explicitly)

- T8's v1-fit hash pins: the committed data changed, and the code path is proven unchanged (see Evidence).
- T7's v1-export pin: the same hash (667f6b8e), now reached via id cec81302.
- No tolerance or threshold was relaxed.

## For B346 (aliasing sources)

- **broad#828:** a component near 0.2 × the internal sample rate that moves with os. It is rate-locked, not a harmonic folding. Suspects: the feedback loop (fb 0.38), collision (colK) or a noise-FM hash at the internal rate.
- **broad#511 and #169:** their 1x→4x difference vanishes 4x→16x (folding, os-limited).
- **broad#857:** 2x→8x −17 dB and 8x→32x about −26 dB. It is feedback FM (fb 0.6, xm 0.93) whose DYNAMICS depend on the internal rate. Part of what reads as aliasing is discretisation, not folding.
- **Worst segments are A5, A1 and the sweep.**

## Open questions

1. **broad#383 drifts in Chrome under the fixed aliasing** (35/36 xverify). Mark it `heard: drift`, exclude it from future fits, or accept it? The sample is not re-selected.
2. **The floor's cost.** It hides weak aliases between dense, drifting partials: broad#511's E5 windows read −40 to −46 where they read −30 to −35. Its program-worst is still −23 dB, and the ratings did not move. Accept, or pursue an os-convergence estimator for B346?
3. **Should THRESH.flatness be replaced by noiseDb (cut ≈ −21.5 dB, logistic −22)?** This is the human's ruling. The gain is small, and on v1 flatness is marginally better.
4. **Does "noisy" mean noise?** 18 of 22 noisy answers are also rough, and noiseDb reads dense swarms as part-noisy. The question wording may deserve a look.
5. **The page's reveal** still shows flatness, not noiseDb (a UI choice, deferred).
6. **The P3 report and the fidelity audit** carry pre-B345 aliasing numbers.
