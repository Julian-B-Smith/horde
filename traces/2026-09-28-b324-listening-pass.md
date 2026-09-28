# b324-listening-pass — the gauntlet's blind listening pass: page, stratified seed sample, calibration script, check

- **Queue item:** B324, read verbatim from `origin/lead-records-126:ROADMAP.md` (records PR #819), beside B316. The human: "Help me organize the listening pass, please".
- **Why:** B316's coherence metrics (aliasing, roughness, root presence, noise) use PROVISIONAL thresholds (`gauntlet.mjs` THRESH). P4 fits the random-patch distributions to those thresholds, so a short blind pass by ear has to set them first. This builds the pass and the script that turns its ratings into a proposed threshold per metric.

## What changed

- `docs/design/listening-pass.html` (new lab, `lab-review` B324 · 2026-09-28).
  - Regenerates each patch from its committed seed. It uses the gauntlet's sampler over the SCALPEL lab's own table and tapers (sliced by `space.mjs`'s anchors) and `dependency_tree.json`. The sound comes from the composed engine, loaded by the SCALPEL lab's route (`razor-core.js` and `scalpel-horde-engine.js` by `<script src>`, `swarmsaw.html` by fetch).
  - The phrase is the two measured renders: A3 held 0.5 s in 512-sample blocks, then E5 held 0.3 s. Each is followed by its release. The phrase loops, and each patch is level-matched: held-part RMS to −20 dBFS, peak ceiling −1 dBFS, at most +40 dB.
  - Blind:
    - The rating view is built from the patch key alone.
    - After every view change, a scan checks every non-script element's text and attributes (all except `style`) against the reveal's own vocabulary.
    - The order is a seeded shuffle (`orderSeed`), and each repeat sits half the list from its original.
  - Five binary questions. The keys are 1/2, 3/4, 5/6, 7/8 and 9/0 (odd = fine, even = flag). Space replays, L toggles the loop, Enter/→ goes next, ← goes back, N writes a note, E exports.
  - Resumable through localStorage (all access in try/catch).
  - Reveal: calibrate.mjs's own fit (imported), controls, test-retest, roughness origin, and a per-patch table where a shaded cell disagrees with the provisional threshold. Every row has a replay button.
  - Export: `listening-pass-ratings-<sampleId>.json`. There is no timestamp; the order index is the sequence. The instructions say to save it into `local/patchspace/ratings/`.
  - `?demo=rate|reveal` gives screenshot states with SYNTHETIC answers, labelled on screen and never saved. `?xverify=1` is the cross-runtime pass described below.
- `docs/design/listening-pass.json`: the sample, written by the tool below. It holds only seeds, measured metrics, `ph` (patch hash), `fp` (render fingerprint), strata and the exclusions. Its id is `03c97d3e`.
- `tools/patchspace/listening_sample.mjs` (new): stratified, seeded selection from a gauntlet run. Pool: `lp324`, which is seed 0xB316, 900 broad + 300 edge, i.e. the first indices of B316's `p3` run. It measured in 490 s on 4 workers, under `local/` (not committed).
- `tools/patchspace/calibrate.mjs` (new): the fit and the export validator.
  - The fit is ROC best cut (Youden J, middle of tied cuts), a Mann-Whitney AUC, the bracket between neighbouring samples, and a 1-D logistic 50% point (reported only inside the rated range).
  - It is browser-importable: the pure exports have no Node import, and the CLI imports `node:fs` lazily.
  - It prints a PROPOSAL and never applies anything.
- `tools/patchspace/listening_pass_check.mjs` (new): 31 checks with 8 must-fail controls. **WIRED: `./verify full`**, beside `dependency_tree_check`.
- `tools/patchspace/gauntlet.mjs`: `A_SCRIPT`/`B_SCRIPT` are now exported (one word each, no behaviour change), so the sample tool renders the exact measured scripts.
- `tools/patchspace/README.md`: three rows, a "listening pass" paragraph, and the last-verified line.
- `verify`: one block in `full()`.

## The sample (36 patches)

- **Per metric (aliasDb, rootPresence, flatness):** 2 FINE, 3 BORDER and 2 FLAG. The three border sub-bands sit just below, at and just above the threshold. The bands are:
  - aliasDb: < −45 / [−36, −32) [−32, −28) [−28, −24) / ≥ −15
  - rootPresence: ≥ 0.98 / [0.38, 0.46) [0.46, 0.54) [0.54, 0.62) / < 0.25
  - flatness: < 0.02 / [0.2, 0.27) [0.27, 0.33) [0.33, 0.42) / ≥ 0.5
- **Roughness:** 2 fine (< 0.04), 3 border ([0.07, 0.09) [0.09, 0.11) [0.11, 0.14)), and 4 flag (≥ 0.2) split by ORIGIN.
  - Origin is measured by rendering the same patch with N 1 (`roughnessSolo`): ≤ 0.04 means the swarm's own beating; ≥ 0.1 means the partials are discordant on their own.
  - The pool is lopsided: of 164 rough candidates, most are beating. The sample tags every rough patch; the "beating" group holds 15 and "partials" 3.
- **Controls:**
  - 2 clean: alias ≤ −90, roughness ≤ 0.03, root ≥ 0.99 at interval 0, flatness ≤ 0.01, no clicks, dc < 0.05, rms > −30 dBFS.
  - 2 broken: alias ≥ −15, roughness ≥ 0.15, and flatness ≥ 0.3 or root ≤ 0.3.
  - 2 repeats of border patches (test-retest), 18 positions from their originals.
- **Estimate:** 36 × ~20 s ≈ 12 minutes, shown on the intro.

## Finding: Chrome's libm is not Node's

- Chrome 153 and Node 24 (V8 13.6) give different last bits for `Math.sin, cos, tan, exp, log, tanh, atan, atan2, log2, log10, expm1, log1p, sinh, cosh, cbrt, asin, acos, atanh`. They agree on `pow`, `sqrt` and `hypot`.
  - Method: FNV hashes over 20 000 seeded inputs per function in both runtimes (scratch probe `mathdiag.js`).
- So a browser render is NOT bit-identical to the gauntlet's Node render. The first design's in-page fingerprint check failed in Chrome (fp `73c02c7d` vs Node `05e2654e`) while the patch hash matched (`091a2efc` in both).
- The sampler reads only `Math.pow` (the lab's `fromPos`), so **patches are exact everywhere**. The page checks all 36 patch hashes at boot.
- For **sounds**, the page instead MEASURES what it plays (`measureHeard`, `metrics.mjs` imported) against a tolerance (TOL: aliasDb ±3 dB, roughness/flatness ±max(0.01, 15%), rootPresence ±0.05, rmsDb ±1 dB). It marks an outside patch as `heard: drift`; the export carries the mark and `calibrate.mjs` leaves those ratings out of the fit and names them.
- Cross-runtime pass (`?xverify=1`, headless Chrome 153):
  - Most patches measure identically to 6 digits.
  - Chaotic patches amplify the last-bit difference into a different realisation. On the first draw, `broad#276` read flatness Δ0.20 and roughness Δ0.18.
  - Four patches were outside tolerance across the redraws: broad#276, broad#649, broad#775, broad#71. They were excluded BY KEY (`--exclude`, with the reason in the JSON), and the seeded draw took the next candidate.
  - Final pass: **36/36 within tolerance**. The largest differences left: broad#462 alias Δ2.2 dB, broad#118 Δ1.4 dB, broad#88 Δ0.57 dB.
  - The stated bias: patches whose chaos amplifies last-bit differences are under-represented.
- **Scope of the finding:** any golden computed in Node and compared bit-exact in a browser (or the reverse) will fail on this machine. In-browser self-checks that compare the browser against itself are unaffected. This is a hypothesis for other labs; I only checked this page.

## Evidence

- **VERIFIED: `node tools/patchspace/listening_pass_check.mjs`** GREEN, 31/31 checks, 8 must-fail controls, 24 s.
  - T1 sample integrity (id = content hash `03c97d3e`; thresholds = THRESH; exclusions absent).
  - T2: the page's sampler equals `gauntlet.mjs` samplePatch 36/36, and patch hashes agree 36/36.
  - T3: the page's phrase fingerprint equals the committed `fp` 36/36, and so does `space.mjs` render. The page's `measureHeard` reproduces committed numbers exactly for 6 patches. The tolerance reads zero on itself; CONTROL: it flags broad#612 against edge#194.
  - T4: every seed re-measures to the committed 14 fields exactly (34 primaries).
  - T5 CONTROLS: the perturbed index is caught (broad#612 → #613), and so is the perturbed run seed.
  - T6: 144 tokens × 75 views show no leak; CONTROL: the reveal shows all 36 patches' values.
  - T7: complete and partial exports validate; CONTROLS: timestamped, out-of-range and gap exports are rejected.
  - T8: a planted rater at −20 dB is recovered (AUC 1, bracket −20.69 .. −16.03); CONTROL: coin flip AUC 0.53. A drift-marked rating is left out and named.
  - T9: the order is a fixed permutation, with repeats 18 apart.
- **VERIFIED, in-page self-checks 4/4**, read from headless Chrome's dumped DOM (served on 8324):
  - determinism: 36/36 patch hashes, repeated render bit-identical, first patch within tolerance.
  - perturbed seed: caught as a different patch and a different sound.
  - blind: 0 leaks; the reveal-text control is caught.
  - export schema: passes calibrate's validator; the timestamped control is caught.
- **calibrate CLI, end to end** on a SYNTHETIC export (scratch only: a threshold rater at −22 dB / 0.15 / 0.45 / 0.25 with 10% slips). It suggested −22.9 / 0.153 / 0.439 / 0.258 with AUC 0.93 / 0.95 / 0.80 / 0.86, and all four controls came out as expected.
- `lab_load_check`: GREEN, 57 labs loaded, 0 broken, 1 skipped (the full sweep, including this page).
- **Not verified:** nobody has listened. Playback, loop and replay were exercised in code and the screenshots, not by ear. The page was checked in Chrome only, not in Safari or Firefox, whose libms differ again; there, the drift mark is the safety net.

## Evidence consulted

- ROADMAP B316 and B324 (`origin/lead-records-126`).
- `tools/patchspace/README.md`, `space.mjs`, `gauntlet.mjs`, `metrics.mjs`, `gen_dependency_tree.mjs` (hash32, asEvalTree).
- `docs/patchspace/2026-09-27-gauntlet-p3.md`.
- `traces/2026-09-27-b316-patchspace-p1-p3.md`.
- `docs/design/scalpel-interface-lab.html`: tokens :178-306, the engine load route :6408-6427, the worklet :4630-4700, toPos/fromPos :1378-1403.
- `tools/labharness/lab_load_check.mjs`, `tools/test_table_check.py`, `verify`.
- `traces/2026-09-28-b318-maw-horde-lab.md` (screenshot and dump-dom method).

## Alternatives rejected

- **Committing rendered audio.** The brief forbids it, and the seed route regenerates the sound.
- **Playing through the lab's AudioWorklet (live engine).** An offline render per patch is what was measured, can be level-matched exactly, and loops cleanly. The worklet would also re-seed per note differently from `space.mjs` render.
- **A bit-exact fingerprint check in the browser.** It is impossible here (the libm finding). The fingerprint is kept for Node (T3), where it is exact.
- **Recomputing the committed metrics in the browser.** The thresholds being calibrated are the Node gauntlet's; the Node numbers stay canonical, and the browser only confirms that its sound measures alike.
- **Keeping the four chaotic patches and relying on drift marks alone.** Four of 36 ratings would be lost at fit time, concentrated in the noise strata. Excluding them by key keeps 36 usable ratings, at a stated bias.
- **Metric values in the export.** Blind-safety and brevity; calibrate reads them from the committed sample by key.
- **Wiring the check in `fast`.** It re-renders 36 patches (~25–60 s), and the sample is a golden against the engine of the day, so it runs in `full`, beside `dependency_tree_check` and for its reason.

## Verify

- `./verify fast` exited 0 on the change-set commit. `.harness/last-verify.json` reads `{"target":"fast","exit":0,"git":"1726739","ts":"2026-09-28T06:36:53Z"}`.
- `lab_load_check` is quiet on green inside verify; run standalone it is GREEN (above).
- `test_table_check` reads GREEN: 72 check files declaring WIRED and verified so.
- `private_name_gate` was SKIPPED: `.leakcheck-names` is absent in this worktree.
- This trace's own commit was re-verified; the hash is in the PR.
- The new check was run standalone, as above. `./verify full` was not run: it is human-paced here, and it builds the plugin.

## Open questions

1. **Tolerance.** TOL was set from one Chrome pass. Should a patch that drifts in the human's actual browser count as a lost rating (the current behaviour), or should the page record its own measured numbers so the rating can still be used?
2. **Chaotic patches are under-represented** (4 were excluded). Acceptable for calibration, or should P4 treat cross-runtime-unstable patches as their own class (they are also the ones a C++ port will not match bit for bit)?
3. **The libm finding affects other labs.** Any Node-made golden that is checked bit-exact in a browser will fail. Worth a LIBRARY lesson; the lead owns LIBRARY.
4. **The roughness-origin pool is lopsided:** "partials" is rare (3 in the sample), so the swarm-beating question will be answered with few discordant counterexamples.
5. `test_table_check`'s census still does not glob `tools/patchspace/*_check.mjs` (B316's follow-up). This new check's `WIRED: ./verify full` header is true but uncensused.
6. CODEMAP.md does not list `tools/patchspace/` (out of scope, as B316 noted).
