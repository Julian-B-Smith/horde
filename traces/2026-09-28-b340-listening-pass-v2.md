# b340-listening-pass-v2 — the listening pass with controls you can check: references, a sweep through the engine's bend, six notes over five octaves, every played segment measured, detector controls, the v1 pass preserved

- **Queue item:** B340, read verbatim from `origin/lead-records-135:ROADMAP.md` (records PR #833), beside B316 and B324. The human: "I completed the listening pass but I don't entirely trust it. Before I can feel certain about it, maybe we could add a simple sine or saw wave playing the same notes as a control so I know the root is coming through. Also adding a pitch sweep would be helpful to confirm there isn't any aliasing. And maybe the notes should play on a few octaves, and for a little bit longer. It's hard for me to make judgments on what's here".
- **Why:** trust. The human must be able to hear that the root is there and whether aliasing is present, and the numbers the fit uses must be shown to measure what the ear hears. That means measuring what is actually played, and running detector controls through the same code.

## What changed

- `docs/design/listening-pass.html` (v2; `lab-review` "B324 + B340 · 2026-09-28"; header comment rewritten for v2).
  - **Program:** seven segments per patch, each rendered on a fresh engine with the patch's render seed.
    - Notes: A1 A2 A3 A4 E5 A5, each held 1.5 s with a 0.5 s release.
    - Then a sweep from A1 to A6.
    - Total: 19 s of sound per patch.
  - **The measured renders stay inside the program.** A3 renders in 512-sample blocks and E5 in 128, so their prefixes ARE gauntlet.mjs A_SCRIPT / B_SCRIPT. `programFp()` equals the committed `fp` for 36/36 patches (check T3). v1's short phrase is no longer played.
  - **Sweep through the engine's own pitch path.**
    - The path is `bend`: a smoothed `t` parameter in semitones (12 ms; razor-core.js render). The lab maps octave/semi/fine onto it, and the swarm reads it as f0 on its tick (scalpel-horde-engine.js `tickSwarm`).
    - A3 is held while bend runs from −24 to +36 semitones over 5 s, with 0.25 s steady at each end. Bend is set once per 128-sample block through `set()`.
    - The engine is not edited.
    - It also has RazorCore's mono glide and the per-note retune message `{t:'re'}`. Bend is the path that moves a held voice smoothly.
  - **Render route:** `renderSteps` is a generator, so the page yields between blocks. It uses the same operations in the same order as space.mjs render. Math.random is the seeded stream for every engine call. `renderScript` runs it to the end. Bit-exactness is proven by T3.
  - **Reference controls:** a pure sine and an additive band-limited saw (harmonics below 20 kHz, faded over 18–20 kHz).
    - Same notes, same timing, same bend target; level-matched by the same LOUD rule.
    - S / W play the reference for the current selection: the whole program, one note, or the sweep.
    - The "reference first" setting (B) plays one before each patch: off / sine / saw.
    - No metric is shown for them.
  - **Player:** space plays the program, P the sweep, [ ] step through single notes, X stops, L loops. Each segment chip plays that segment alone; the sounding segment is highlighted by a playhead.
  - **Measure what is played:** `measureWindows()` is one function for every sound the page plays or tests. It takes the gauntlet's own window lengths (tonal 0.4 s from 0.1 s; aliasing 0.25 s from 0.05 s, against the same patch at 4× its oversampling), slides them across the whole hold in half-window steps, and keeps the worst window per metric. The sweep gets aliasing only, plus the pitch at its worst window.
  - **Background work:** measurement runs in the background, rendering in chunks. Order: this patch's sound, the next patch's sound, this patch's measurement, then any answered patch not yet measured.
  - **Export and reveal wait for measurement.** The v2 export carries per-segment numbers (`segs`) and `phrase: {version: 2, id}`, with PHRASE_ID `3e9b89ac`. `heard: match|drift` is kept, as information only.
  - **Calibrate your ears:** an intro step plays a clean-vs-naive saw pair at E5 and on the sweep, plus a sine sweep. Beside each is the detector's verdict on that known signal (clean / aliased), before any patch is heard.
  - **Detector controls** (`detectorControls()`; self-check 5; T10; the reveal; xverify) go through `measureWindows()`:
    - sine: clean, root clear and not noisy on all six notes, and clean on the sweep;
    - naive saw (2·phase−1): aliased at E5, held and with a 30-cent vibrato, and on the sweep;
    - additive saw: clean in all three;
    - sine a tritone off the note: root unclear on all six;
    - seeded white noise: noisy;
    - INFO rows (not asserted): a semitone-sharp sine per note, to measure the root metric's resolution.
    - For generated signals the aliasing reference is the same generator at 4× the sample rate, with a 4× FFT so the bins match.
  - **The v1 pass is preserved.**
    - `hypersaw.b324.listening-pass` is read only. T11 statically proves that no setItem/removeItem/clear in the page can reach it, and catches planted writes. Self-check 6 confirms at runtime that it is byte-unchanged since load.
    - v2 uses `hypersaw.b340.listening-pass`.
    - "EXPORT THE V1 PASS" is at the top of the landing view whenever the v1 key holds answers.
    - It exports byte-identically to the v1 page's own export, for EITHER sample the pass may have been rated on (next section).
  - **Blindness, resumability, `?demo`, dark/light:** kept. New demo states: `landing` (with a synthetic v1 pass) and `calib`. Developer numbers print with a middle-dot decimal point so they can never equal or hide a revealed value.
- `tools/patchspace/calibrate.mjs`:
  - `SCHEMA_V2`; the validator dispatches on schema, and a complete v2 export must carry `segs` on every rating.
  - `worstOf(segs)`: the flag-side extreme per metric, from PAIRS.
  - `analyse()`:
    - v1 input returns the pre-B340 object exactly (pinned);
    - v2 input fits the worst heard segment and adds `metricsCommitted` (comparison only), `worstFrom`, `unmeasured` and `phrase`;
    - a v1+v2 mix throws.
  - The CLI analyses each version separately.
- `tools/patchspace/listening_pass_check.mjs`: 53 checks, 17 must-fail controls.
  - T3: v2 prefixes match `fp`.
  - T6: every new view is scanned.
  - T7: v1-pass exports are pinned for both samples; v2 export and its controls.
  - T8: v1 fit hashes are pinned against the pre-B340 calibrate; a v2 export is fitted on the worst heard segment; a v1+v2 mix is refused.
  - T10: detector controls.
  - T11: v1 store protection.
  - T12: measureWindows restricted to the first window reproduces the gauntlet's numbers exactly. The bend really sweeps: root at A1 at the start and at A6 at the end, with a control. Whole programs produce valid segments.
  - Still `WIRED: ./verify full`, unchanged.
- `docs/design/listening-pass.json`: NOT changed. Same 36 patches and seeds; id `cec81302`.

## Findings

1. **One long window was wrong, and the first design used it.** Over the whole 1.45 s E5 hold, the aliasing of many patches reads the −120 dB floor, while every 0.25 s sub-window of the same render reads −20 to −33 dB.
   - Examples: broad#511 reads −29.9 (the committed window), −32.0 and −33.0 on sub-windows, and −120 over the whole hold. broad#857, broad#828 and broad#169 behave the same.
   - The static-tone detector controls passed on that design: the detector-shares-assumption trap, again.
   - Hypothesis tested and FALSIFIED: pitch smearing. A naive saw with a 30-cent/5 Hz vibrato, or a 10-cent/0.5 Hz drift, still reads aliased over the long window (−24.6 / −21.1 dB). The mechanism is unknown.
   - The fix is the sliding gauntlet-length window. Its first window IS the gauntlet's measurement (T12, exact).
   - Implication for B316: the gauntlet's short window is consistent with itself over time. Any longer-window use of `aliasing()` elsewhere would be blind.
2. **Root metric resolution.** At A1 and A2, a sine one semitone sharp still reads rootPresence 1.000 (8192-point bins are 5.86 Hz; a semitone at A1 is 3.3 Hz). At A3 it reads 0.808, which is still "clear" by THRESH 0.5. It resolves from A4 up (0.065).
   - A tritone off reads unclear on every note, and a sine at the note reads interval −3 st at A1 while its presence is still 1.0.
   - Reported as INFO rows and as the self-check's LIMIT clause. Not tuned.
3. **Worst heard differs a lot from committed** (all 36 in Node, scratch).
   - Root: broad#564 1.00 → 0.02 at A5; broad#730 0.98 → 0.34 at A5.
   - Roughness: broad#706 0.43 → 1.12.
   - Aliasing: edge#29 −120 → −32 on the sweep; edge#145 −36.5 → 0.0 dB on the sweep (every flagged window's power counted: an outlier to inspect, not tuned).
   - Where the worst came from, on a synthetic rater: aliasing mostly the sweep, A5 and A1; roughness and root mostly A5.
   - A noisy broken patch (broad#828) reads −3 to −7 dB of "aliasing" on every note, A1 included, where real folding is implausible. Hypothesis: for noise-like patches the metric reads render-to-render differences between os and 4×os, not folding.
4. **Two v1 samples.** B324 shipped sample `03c97d3e` (PR #823, merged 02:56). The B325 re-selection replaced it with `cec81302` (PR #829, merged 11:42). The two share no patch.
   - The human's v1 store carries whichever id they rated on; this session cannot know which.
   - The page keeps enough of `03c97d3e` (run seed, order seed, keys) to export either sample exactly. Both are pinned against the historical pages' own export: `667f6b8e` and `5f02623f`.
   - An `03c97d3e` pass rated sounds from the pre-B325 engine. To fit it, calibrate needs `--sample` with `git show 1726739:docs/design/listening-pass.json`; the landing card says so.

## Evidence

- **VERIFIED `node tools/patchspace/listening_pass_check.mjs`: GREEN, 53/53 checks, 17 must-fail controls, 44 s** (on the working tree just before commit; the verify full run on the commit is below).
- **VERIFIED v1 unchanged:** the analyse/report hashes `60ae017d 791cc92a 921c14fd` were produced by origin/main's calibrate.mjs (last changed in 1726739) and by the new one. The v1-pass export pins were reproduced by the historical pages' own `buildExport`.
- **VERIFIED Chrome `?xverify=1`** (headless Chrome, served on 8340, dumped DOM):
  - 36/36 patches' measured windows within tolerance; 36/36 full programs' per-segment numbers pass the v2 schema.
  - Every detector control reads identically to Node.
  - Self-checks 6/6.
  - This run was on the page before the V1_SAMPLES and label edits; the measurement code is unchanged since.
  - Its page-clock timings are virtual time and are not reported as timings.
- **Timings, from Node over all 36 (scratch `all36.mjs`, the page's pure block):** render per patch median 1.3 s, max 4.1 s; measure median 3.7 s, max 11.8 s. Both run in the background while the human rates (~12 s of answers plus 19 s of sound per patch).
- **Session length:** 19 s of sound plus ~12 s of answers ≈ 31 s per patch, ≈ 19 min for 36; ≈ 30 min with a reference before every patch. The intro computes and shows these.
- **calibrate CLI end to end** on SYNTHETIC v1 and v2 files (scratch; a threshold rater on the heard numbers with 10% slips): the two versions are reported separately; the v2 cut is recovered on the heard basis, and the committed-basis comparison is printed beneath.
- `lab_load_check` on the page: GREEN.
- **Screenshots** (scratch, not committed): `scratchpad/b340/shots/`
  - `1-landing-v1-offer.png`
  - `2-calibrate-your-ears.png`
  - `3-rating-view-controls.png`
  - `4-reveal-demo-light.png`
  - `5-reveal-demo-dark.png`
- **Not verified:** nobody has listened. Playback, segment play, references, reference-first and the playhead were exercised in code and screenshots, not by ear. Only Chrome was tested.

## Evidence consulted

- ROADMAP B316, B324, B340 (`origin/lead-records-135`).
- `traces/2026-09-28-b324-listening-pass.md`.
- The v1 page header.
- `docs/design/scalpel-horde-engine.js` (startVoice, couple, tickSwarm); `reference/scalpel/prototype/razor-core.js` (set, noteOn/monoOn, bend in keff/couple, the render smoother and glide) — read only.
- `tools/patchspace/metrics.mjs`, `metrics_check.mjs`, `gauntlet.mjs` (measure, THRESH, A/B scripts), `space.mjs` (render), `listening_sample.mjs`.
- `tools/labharness/lab_load_check.mjs`.
- Git history of `docs/design/listening-pass.json` (1726739, d20082e) and the merge times of PRs #823 and #829.

## Alternatives rejected

- **One long measurement window per segment.** Rejected on evidence (finding 1).
- **Additive vs PolyBLEP for the reference saw:** additive. It is exact below 20 kHz at any rate, so the detector's band-limited control is clean by construction. PolyBLEP leaves residual aliasing that would muddy the "clean" reference.
- **Generating the control reference by a decimator:** rejected. It would be a new abstraction; the same generator at 4× the rate with a 4× FFT gives identical bins using the existing `aliasing()`.
- **Retune `{t:'re'}` or mono glide for the sweep:** bend is the engine's continuous pitch control and moves a held voice without re-triggering anything.
- **Regenerating `listening-pass.json` or changing the order:** rejected. Same 36 patches and seeds, per the brief. The same order seed means the same order as v1 (see open questions).
- **Excluding drift-marked ratings in v2:** no. The v2 fit uses the numbers measured where the rating was made, which resolves B324 open question 1 for v2 only.
- **Committing the old sample file so `03c97d3e` passes can be fitted in-tree:** out of scope. The page carries only what the exact export needs.

## Verify

- See the PR: `./verify fast` and `./verify full` run on the committed hash, read from `.harness/last-verify.json`.

## Open questions

1. **Which sample did the human's v1 pass use?** `03c97d3e` (pre-B325 engine, different patches) or `cec81302`? The landing card says which; the lead should ask. If it is `03c97d3e`, its thresholds calibrate sounds from an engine since fixed.
2. **Worst-of basis vs the gauntlet's basis.** v2 fits the worst heard window over six notes and a sweep. The gauntlet's THRESH applies to its A3/E5 windows. A threshold fitted on the worst heard would need the gauntlet (P4) to measure the same way before it applies. The comparison fit is printed beside it; the choice is the lead's.
3. **The long-window aliasing collapse is unexplained** (finding 1). Worth a B316 follow-up; the pitch-smearing hypothesis is falsified.
4. **Aliasing on noise-like patches** (broad#828 reads −3 to −7 dB on A1, finding 3) may be render-to-render difference rather than folding. This is a hypothesis about metrics.mjs, which is out of scope here.
5. **Root metric blind to a semitone at A1/A2** (finding 2). Should the fit ignore A1/A2 root, or should metrics.mjs raise its resolution for low notes?
6. **Same order as v1.** A human who saw the v1 reveal may remember patches. Keep the v1 order (as built) or reshuffle with a new seed?
7. `tools/patchspace/README.md` still describes v1's phrase. It is out of scope here.
