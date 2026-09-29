# b346-alias-sources — an os-convergence aliasing estimator, where the composed engine's aliasing comes from, what cures each source, and the two stale reports regenerated

- **Queue item:** B346, read verbatim from `origin/lead-records-140:ROADMAP.md` (records PR #845) beside B316, B323, B325, B342, B345 and B350. The human: "could we apply something like ADAA to this, or does it only work with overdrive-type effects?" and "I think we should try to build the cleanest system we can muster." Mid-task, the lead added the vibrato and glide controls (from B350's red T10: the listening page's naive-saw-with-vibrato control reads −27.2 dB on the B345 metric, under the ruled cut).
- **Why:** B345's metric under-reads moving partials and hides weak aliases under its noise floor, and nothing said where the aliasing comes from. The estimator had to be proven on controls before any mechanism was attributed with it, and every cure had to be judged against one fixed truth.

## What changed

- `tools/patchspace/metrics.mjs`: `aliasConvergence`, `excessJoint`, `foldPrediction`, BESIDE `aliasing()` (unchanged). Renders at os N, 2N, 4N, 8N, 16N:
  - excess at N over TWO finer references (4N, 8N). Per 33-bin region: where the references are one realisation (80% of their power agrees within 10%), the power in bins that at least doubled; where they are not, 10× over the ±3-bin max of both. No noise floor.
  - convergence (the same excess at 2N against 8N, 16N) and a source test (the 8N render's pre-decimation spectrum folded onto the band as N would).
  - class: clean (≤ −60 dB), folding (converges ≥ 3 dB AND half explained), dynamics.
  - The header states the design history and the limits: output-rate stages are invisible to any os comparison; the source test is fooled by broadband fine content; A1.
- `tools/patchspace/metrics_check.mjs`: rows X1–X8 and X7c (INFO X2i). Existing rows are unchanged. ~8 s, still `WIRED: ./verify fast`.
- `tools/patchspace/alias_sources.mjs` (new, `UNWIRED: an investigation run`):
  - capture: a subclass reads `bqf`, the samples stay unchanged, proven bit-identical to `space.render` on all 34 rated patches;
  - `estimate` (the estimator, the output-stage split at the patch's own os, and the total against an oversampled truth);
  - 12 mechanism toggles and 11 candidate cures. Toggles use params, subclasses, or in-memory text patches of the oracle (B325's `patchedRazor`, anchors asserted once). No file is edited;
  - passes `matrix`, `fixes`, `fixes2`, `fixes3`, `cpu`, `b828`, `ulp`.
- `tools/patchspace/gauntlet.mjs`: `measure(patch, seed, opt)`.
  - `opt.conv` adds `aliasConvDb`, `aliasConvClass`, `aliasFoldDb`, `aliasDynDb`, `aliasConvergeDb`, `decimLeakDb`, `tanhFoldDb` and `aliasTotalDb`.
  - The run and its report ask for them. Existing callers, whose checks re-measure, do not, so their numbers and cost are unchanged. **THRESH untouched.**
- `tools/patchspace/gauntlet_report.mjs`: a `--note` banner, the new fields in the distributions, and a new section "Aliasing re-read (B346)".
- `tools/patchspace/fidelity_audit.mjs`: a new chord pass (the audit's own chord hold) and its summary table.
- `tools/patchspace/README.md`: the rows and the verified line.
- The two reports:
  - first commit (548e2da): STALE banners;
  - last commit: regenerated (see Regeneration).

## Findings

1. **The estimator's controls (metrics_check, wired).**
   - Band-limited saw: −120.
   - Naive saws: −24.1 / −19.5 / −15.0, converging 6.3–6.6 dB, explained 1.00.
   - Independent noise renders: −120, five seeds.
   - A tone 15 dB under noise: −15.3.
   - Sine feedback FM β 0.6 with the engine's loop: dynamics (a limit cycle near a third of the rate at os 1, gone at 2).
   - Dense partials over a noise floor: −22.9 against B345's −41.5.
   - A 1.5 dB rate-dependent level: clean. The first design, a region power sum, read it −5.3 dB, which is why the rule is per-bin doubling.
   - The lead's controls, estimator against B345:

     | control | estimator | B345 | steady saw (estimator) |
     |---|---|---|---|
     | naive E5 vibrato (30 c, 5 Hz) | −22.4 | −29.1 | −19.5 |
     | naive E5 glide (2 st/s) | −20.4 | −22.5 | −19.5 |
     | naive A3 vibrato | −26.6 | −120 | −24.1 |
     | naive A3 glide | −26.9 | −55.9 | −24.1 |

   - Band-limited saws on the same paths read −97.6 to −120.
2. **broad#383's Chrome/Node drift is chaos (L0066 class).**
   - Through the page's own render and measurement, 1-ULP nudges of the played frequency, fb (0.983), detune or w swing its E5 aliasDb over −23.9 … −19.7: **4.15 dB**, the size of the Chrome drift (4.1 against TOL 3). Roughness moves 0.126–0.165 and rootPresence 0.41–0.46.
   - The must-read-zero twin, broad#511, has spread 0.00 on every field: a regular patch's 1-ULP perturbation vanishes in the float32 output.
   - Across all 34 rated patches the E5 spread is ≤ 0.68 dB, except two threshold-edge readings. edge#245 and edge#145 flip between −120 and −57 / −40.
   - Proposed: `heard: drift`, excluded from fits, with this evidence (`alias_sources.mjs ulp`).
3. **Attribution (34 rated patches × A1/E5/A5 = 102 patch-notes, 72 with total > −60 dB).**
   - Classes: clean 34, folding 42, dynamics 26.
   - Over the ruled cut (−26.8): B345 26, the estimator 40, the total vs truth 41.
   - Removing a mechanism drops the total by ≥ 6 dB on:

     | mechanism removed | patch-notes | patches |
     |---|---|---|
     | the base wave: 6 of the 12 patches have Band-limit OFF (aa 0), the naive base folds and they are the loud ones, −12 to −30 dB; the other 6 have it on, at −39 to −57 dB, where the base's share is BLEP residual and decimator leak | 27 | 12 |
     | feedback / cross-mod | 17 | 8 |
     | the carriers | 12 | 6 |
     | the scanner missing xin | 5 | 2 (#689, edge#168) |
     | the noise blade | 1 | edge#29 |
     | fold, FM, collision, crush, the noise hash, the DC estimator | 0 | — |

   - Output stage: decimator leak > −40 dB on 9 of 102, tanh fold > −40 on 1 (#564 E5, −39).
4. **broad#828's component near 0.2 × the internal rate is a rate-locked limit cycle of the feedback loop: a discretisation artefact, not a sound.** Evidence (`alias_sources.mjs b828`, band within 1.5 kHz of 0.2·R):
   - The band is −5.6 / −6.7 / −7.2 dB of the window in attack / mid / late at os 1 (9.6 kHz) and os 2 (19.2 kHz). At os 4 it sits at 38.4 kHz and leaks −24.5 dB.
   - fb 0 removes it (−30 dB, the estimator −120), and so does blade 2 off. xm 0, collision 0, the hash → sine and dcMode 0 leave it.
   - The blade envelope off lowers only the attack (−5.6 → −9.2).
   - Feeding back the RAW sample instead of the PolyBLEP output, which lags one internal sample, moves it to 1/3 of the rate (−21.5 → −5.95 dB at 16 kHz). So the loop's taps at 2 and 3 internal samples set the period, 5 samples.
   - The estimator calls it dynamics: it converges only 1.6 dB per doubling, although the source test "explains" 82% by coincidence.
5. **The cures (each judged against the engine-as-is truth; see the report's §0 table).**
   - ADAA-1 on the carrier phase→wave, replacing the scanner's carrier BLEPs: 17 of 72 helped, up to −99 dB (#754), −34 (#689), −18 (edge#168); none hurt ≥ 3 dB; ≈ 1.0× CPU.
   - Stacked on the carrier BLEPs it is the same within 1 dB on 72 of 72.
   - ADAA on the BASE wave with its BLEP active double-corrects: 11 hurt, +4 to +29 dB, all 11 with Band-limit on (#758, #692, #472, edge#225, edge#42).
   - Switching Band-limit on: 14 helped, up to −50 dB.
   - The scanner tracking xin: 5 helped, up to −22.
   - os 4×: 61 helped, median −15 dB, 2.5–3.2× CPU. os 2×: 52 helped, median −7, 1.5–1.7×.
   - But 6 of the 72 got WORSE at os 4×, by +6.5 to +13.9 dB (#692, edge#42, edge#225, all with Band-limit on, at −46 to −53 dB). The clean #479 E5 rose from −69.9 to −46.7. The blade caps scale with os, and the brighter patch leaks through the decimator. Verified on #479 A5: leak −120 / −40.9 / −38.3 at os 2 / 4 / 8.
   - A steep FIR decimator: 4 helped (edge#42, #692 −9 to −13; #479 E5 −50), 1.0–1.4× in JS, 1 ms latency.
   - tanh inside the oversampled loop: 0 helped (max −0.4 dB). ADAA-1 on the output tanh "helps" by its own low-pass: small-signal cos(πf/sr), −11.7 dB at 20 kHz.
   - CPU was measured under load average 7–8 on an 8-core M3 shared with other sessions, min of 5, ratios ±8%.

## Regeneration (last commit)

- **B350** (PR #846) was checked with `git fetch` + `gh pr view 846` before regenerating: **OPEN, draft, not merged**. Both reports are regenerated against the CURRENT THRESH (aliasDb −30), stated in each.
- **The P3 gauntlet:** the same seeds (0xB316, 2000 broad + 1000 edge) re-run as `p3b346` with `measure(…, { conv: true })`; `gauntlet_report.mjs --run p3b346 --out docs/patchspace/2026-09-27-gauntlet-p3.md --note …`.
- **The fidelity audit:** its B346 chord pass, and §1a and §5 rewritten from `fidelity_audit.mjs summary`; the banner replaced.

## Evidence consulted

- ROADMAP B316/B323/B325/B342/B345/B346/B350 (`origin/lead-records-140`).
- The traces for B345 and B325.
- The headers of `metrics.mjs`, `space.mjs`, `gauntlet.mjs`, `fidelity*.mjs`, `listening_sample.mjs`, `calibrate.mjs`, the page's PURE block.
- `docs/design/scalpel-horde-engine.js`, read only.
- `reference/scalpel/prototype/razor-core.js`, read only: `stepM`'s `m.prev + _cp` latency, `scan` (no xin), `tryE`, `hAt`, `dcEst`, `setOS`/`bqf`, `kCap`, the output tanh, `RazorCore.F`, the hash.
- `local/patchspace/ratings/`: not read. It is not needed; the patches come from `listening-pass.json`.

## Alternatives rejected

- **The estimator:**
  - A single finer reference without the noise floor: chance peaks read −24 to −30 dB on independent noise.
  - References {2N, 4N}: 2N's own aliases masked N's; broad#511 E5 read 7 dB low.
  - The per-bin 10× rule alone: blind under vibrato (A3 −44).
  - A region power sum: read a 1.5 dB level as −5.3 dB of folding.
  - Convergence alone: fooled by the β 0.6 loop.
  - The source test alone: fooled by broadband fine content (#828).
  - A time-domain difference: the decimation filters and the BLEP latency differ per os.
- **The output stage:** measured at 8N first. Rejected: the caps make 8N a brighter patch, which overstated the leak (#479 −42 at 8N vs −120 at its own os).
- **Second-order ADAA:** not built. `RazorCore.F` is periodic only because each shape is zero-mean. Its antiderivative is not, so a second antiderivative needs unwrapped-phase bookkeeping per shape: not cheap.
- **Relaxing any threshold or TOL:** not done.

## Open questions (for the lead / the human)

1. **broad#383:** mark it `heard: drift` and exclude it from fits (the evidence is above)?
2. **ADR-187 divergences proposed, none applied:**
   - R1: ADAA-1 on the carriers, replacing the carrier BLEPs;
   - R2: decouple the blade caps from os;
   - R3: a steep polyphase decimator;
   - R4: the feedback loop (the BLEP-latency tap; 4× for fb/xm patches, or a gain bound or loop filter in seconds).
3. **aa (Band-limit off)** is half of the random sampler's patches and the largest single source in the rated set. Should P4 weight it toward on?
4. **The class labels** are only meaningful above about −40 dB. The source test can be fooled by broadband fine content, so the class rests on convergence there (stated in metrics.mjs).
5. **The aliasing re-fit** (after B350) should use `aliasConvDb` or `aliasTotalDb`, not `aliasDb`.
6. **The chord's Breathing pad** reads −17 dB total and is classed dynamics. Its mechanism was not isolated: the blade-envelope and law toggles move it; fb and dcMode do not.
7. **ADAA's carrier droop at os 1** (cos πf/R on the blade content) was not measured apart from the total.

- **Verify:** see the PR (the committed hash's `.harness/last-verify.json`, fast and full).
