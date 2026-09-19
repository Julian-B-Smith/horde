# subosc-bump-peak — BUMP is normalised by its measured peak, so every (a, φ) reaches 1

- **Queue item:** ADR-178 ruling R7 (the one sanctioned edit on the SUB OSC
  reference, ingested 2026-09-19). Dispatched as a scoped implementer brief.
- **Why:** the BUMP shape was divided by the analytic bound `1 + a`, which is
  provable in one operation but left the waveform at **0.750** of full scale at
  the defaults and **0.707** at worst — up to 3 dB quieter than the other six
  shapes, a level the human has to correct by hand on every patch. The sanction
  is to normalise by the MEASURED peak per (a, φ) instead. The peak is bought
  once per parameter set, never per sample, so the sample loop is unchanged.
- **What changed:**
  - `reference/subosc.html` — new `subBumpPeak(a, φ)`: a bounded search for
    `max|sin θ + a·sin(3θ + φ)|` over **[0, π)** (half-wave antisymmetry
    `y(θ+π) = −y(θ)` holds for two odd partials, so half a period is the whole
    answer), bracketing the sign changes of `f' = cos θ + 3a·cos(3θ + φ)` on a
    32-interval grid and closing each bracket by 24 bisections. Bisection, not
    Newton: no division, no escape from the bracket, always converges. The grid
    also samples `|f|` and seeds the running peak with it, so a hypothetical
    missed bracket degrades the answer by O(h²) instead of returning something
    wrong — and the return can never be 0, which is what makes `1 / peak` safe
    by construction rather than by a guard. `_recalc` now sets
    `_bumpNorm = 1 / subBumpPeak(...)`. The lab's on-screen formula note was
    corrected in the same beat (it printed `/(1+a)`).
  - `specs/SPEC-SUBOSC.md` — §3 formula line and its sentence, the `bumpAmt`
    row's note, and the status banner (the sanction is spent). Scoped to those
    three by the brief.
  - `tools/labharness/subosc_check.mjs` — `mutantCore` generalised to
    `dspExports(from, to)`, which hands back the free functions as well as the
    class (the peak property needs `subBump`); the `(a, φ)` grid hoisted to one
    place and used by both normalisation rows; new property + must-fail control.
- **No closed form, and that is a finding not an assumption:** the stationary
  points solve `cos θ + 3a·cos(3θ + φ) = 0`, which in `t = tan(θ/2)` is a
  sextic. Solvable in principle, not in a form anyone should maintain. The
  bounded search is the answer, and the brief asked for the derivation either
  way.
- **Cost, measured on this build:** 282 transcendental calls and ~2.4 µs per
  `subBumpPeak`, incurred once per `setParam` (not per sample, not per block
  unless a parameter moves). Both bump rows are morphable, so the mod matrix can
  drive this per block; at the 2756 tick/s rate that is ~6.6 ms/s of one core.
  That is the price of the 3 dB, stated rather than hidden.
- **Accuracy:** verified against a 400k-point brute-force scan of the raw
  formula over a 121 × 101 (a, φ) grid — worst relative disagreement **1.4e-10**,
  which is the brute scan's own sampling error, not this search's.
- **Evidence consulted:** `reference/subosc.html:452-540` (the BUMP comment
  block, `subBump`, `_recalc`), `specs/SPEC-SUBOSC.md` §3/§9 L7/§10.6/§11 R7,
  `tools/labharness/subosc_check.mjs` §7, `traces/2026-09-19-b155-subosc-bump.md`
  (the defaults' derivation), LIBRARY L0032 (a plant must assert its anchor),
  L0036 (pin the refusal), L0056 (a worktree agent cannot chain verify and
  commit).
- **Oracle:**
  - `node tools/labharness/subosc_check.mjs` → **GREEN — 39 properties, 0
    failed** (was 37; the two new rows are the property and its control). The
    new row reads `peak in [0.999999996, 1.000000000]`, worst deviation
    **3.90e-9** over the 8 × 9 grid, tolerance 1e-6 — measured off the SHAPE on
    a 65536-point scan, because the render path cannot resolve 1e-6 (674
    samples/period at MIDI 36 plus a 20 kHz tone filter put its own floor at
    ~8e-6; that is why the rendered-peak row keeps its ≤ 1 form and was not
    "tightened").
  - Control (LIBRARY L0032): planting the old `1 / (1 + p.bumpAmt)` back reads
    **0.750021** at the defaults and **0.707263** at its quietest over the grid
    — the two numbers the spec recorded for the bound. The anchor is asserted by
    `dspExports`, and it fired for real during this change: the pre-existing
    no-normaliser control threw `plant anchor not unique — … occurs 0x` the
    moment `_recalc` changed, which is the lesson working as designed.
  - `node tools/labharness/lab_load_check.mjs reference/subosc.html` → GREEN.
  - `./verify fast` → **exit 0**. It exercises this file only through the lab
    load gate (`verify:162`); no golden chain reads `reference/subosc.html`
    (`grep -rn subosc verify tools/golden/ tests/` → no hits) and
    `subosc_check.mjs` is not wired into `./verify` by design (its own header).
- **Alternatives rejected:**
  - *Keep the bound and let the human turn `level` up* — the R7 ruling already
    rejected it; the level knob is a per-patch correction for a per-waveform
    defect, and it silently changes what a morph between waveforms does.
  - *Memoise the peak on (a, φ) so unrelated `setParam` calls skip the search* —
    two comparisons would make the common case free, but it adds cache state
    that must stay in sync with `p`, which is exactly the staleness seam the
    house prefers not to have (safety by construction, not by vigilance). The
    unconditional recompute costs 2.4 µs on a parameter write. Reconsider at the
    port if profiling asks for it.
  - *Newton on `f'` instead of bisection* — fewer evaluations, but it can leave
    the bracket and needs a safeguard that costs back what it saves.
  - *Measure the peak from the rendered audio in the harness* — cannot resolve
    1e-6 (above); it would have been a threshold fitted to the detector.
- **Open questions:**
  - **`specs/SPEC-SUBOSC.md` §9 L7, §10.6 and §11 R7 still describe the bound
    and carry its measurements** (0.75002 lobes, "up to 3.01 dB", the R7
    question in the open-rulings table). The brief scoped the spec to three
    edits, so they are untouched and now stale; §3 carries a one-clause pointer
    saying so. The fresh numbers for whoever re-measures: lobes
    **1.00000 / 0.88231** (MIDI 24), ratio unchanged at **0.88231**, trough
    fraction unchanged at **0.63694**, rendered-grid worst peak **1.000000** and
    quietest **0.999992**, shape-grid peak **[0.999999996, 1.000000000]**.
  - The §10.1 bump alias row moved by ≤0.7 dB in BOTH directions
    (−166.8/−163.6/−166.5/−164.4 → −167.1/−164.3/−166.6/−164.0; MIDI 100
    −164.1 → −163.9). The floor there is the detector's own Kaiser skirt at
    −165 dB, ten decibels under the −155 gate, and a non-monotone ±0.7 dB is
    jitter, not a trend — the rows still pass and the "indistinguishable from
    the sine row" claim still holds (sine reads −167.2/−164.1/−166.7/−164.4).
  - Nothing is ported: `src/` has no SUB OSC core yet, so no C++ carries this.
