# b383-edge-correction-lab — six edge corrections, by ear and by B346's estimator

- **Queue item:** B383, read verbatim from `origin/lead-records-152:ROADMAP.md` (with B346, B378 and B380 for context).
- **The human, verbatim:** "Is there a way to do a note-dependent edge-correction?" and "I should probably sound-test the note dependent edges against the other options in a lab, yes."
- **Why:** B378 F3 asks the human to trade partials above ~17.6 kHz and either 16 samples of latency or a changed edge phase for an alias-clean swarm at 1×. B380 added two note-dependent schemes. That trade is a listening decision, so the lab puts all six options side by side, level-matched and blind, with the numbers beside them.

## What was built

- `docs/design/edge-correction-lab.html`, a single-file lab.
  - The first inline script is a pure core that the workers run verbatim (the B377 idiom).
  - The sources are a test harness, not the engine: a unison saw, a single saw and a sync blade, all drawn from one edge model (naive waveform + list of edges `(te, J)` + residual kernel).
  - The options:
    - (a) the core's 2-point polyBLEP;
    - (b) (a) at 2× through the core's ADR-075 halfband;
    - (c) a linear-phase windowed-sinc BLEP table;
    - (d) its real-cepstrum minimum-phase companion;
    - (e) (a) below a crossover, (d) above, blended by raised cosine over a band;
    - (f) eight min-phase tables at C1..C8 whose length and cutoff scale with pitch.
  - `aliasConvergence` is imported from `tools/patchspace/metrics.mjs` by each worker, not copied.
  - The page shows:
    - a heat map and table over C1–C8;
    - a CPU table per note, timed in a worker after the estimator runs finish;
    - latency per option;
    - a 16–20 kHz view (what each option keeps, and what lands in that band);
    - an aliasing track along any listening material;
    - A/B and seeded blind rounds;
    - a five-row self-check behind `?check=1`, each row with a must-fail control.
- `tools/labharness/lab_wheel_scroll_check.mjs`: the lab is added to `LABS`. It is a wired addition, and the gate reads GREEN with 8 labs.

## Findings made on the way (MEASURED)

- **Steady state.** With the audit's 3 ms fade inside the window, (d) read −109 dB at C8 at 0.5 s and −113 dB at 1 s. That floor moved with window length and with nothing else: not W, cutoff, FFT padding, the log floor, or the table density. The cause is the onset: a min-phase group delay differs between 1× and 16×. The heat map therefore measures 50 ms after the onset. (a) reads the same either way (C2: −47.5 vs −47.4).
- **Estimator blind spot.** At a held note whose period is a whole number of samples, the estimator reads the polyBLEP as −120 "clean". G2 is 44100/98 = 450 samples, and 47.5 is 347 samples. Stepped half-semitone checks read −45/−120/−45/−72 dB for the same correction. The continuity check therefore uses a glide.
- **The estimator is thresholded.** A smooth (e) blend reads as a fast fall to clean, not a ramp. A hard switch at the crossover shows up as a +37.5 dB spike over pure (a) in the window that straddles it.
- **CPU timing under load.** Timed beside six busy workers, the CPU readout was load, not option: (c) read ×0.76 at C1 and ×2.13 at C2. It is now timed only after the estimator runs finish, over 7 rounds of ≥ 25 ms each, and the readout is monotone.

## Evidence consulted

- `docs/audits/2026-09-30-swarm-core-audit.md`: §0, §2.1 (F3, F4, F16) and the harness in the lead's scratch (`b378/h/alias2.mjs`, `blepsyn.cpp`), read for method only.
- `src/swarm_core.h:1045-1120` (the polyBLEP) and `:397`, `:2131-2137` (the halfband).
- `tools/patchspace/metrics.mjs` (the `spectrum`, `aliasConvergence` and `excessJoint` APIs).
- `docs/design/envelope-hierarchy-lab.html` (tokens, the worker idiom).
- `tools/labharness/lab_load_check.mjs` and `lab_wheel_scroll_check.mjs`.

## Alternatives rejected

- **Stepped-note continuity.** Rejected because of the estimator's pitch-coincidence blindness (above).
- **Linear table interpolation.** Rejected because the audit needed 16384 points per sample; the lab uses cubic Hermite with the exact derivative at 128 points per sample.
- **A per-512-sample block timer.** Rejected because the worker clock is 0.1 ms coarse; a whole 1-second render is repeated until ≥ 25 ms have elapsed.
- **Regenerating `docs/design/index.html`.** Out of scope: the lead regenerates the navigator.

## Verify

- `./verify fast`: exit 0 at `0c670ee` (`.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"0c670ee","ts":"2026-09-30T15:15:33Z"}`).
- Headless Chrome, served by `tools/serve_labs.py 8383`: `?check=1` → `SELF-CHECK GREEN — 5/5 checks pass with their controls firing`, in both themes.

## Open questions

- CPU figures are JavaScript timings of a harness saw that does far less per sample than the engine's member kernel, so they overstate a correction's relative cost. The kernel-tap counts are exact. A C++ figure needs the h2 core.
- The note-dependent schemes do not flatten CPU as hoped. With W₁ 32 → W₈ 10, (f) reads ×3.6 at C8 against (d)'s ×4.7. Keeping it flat would need W ∝ 1/f, and W ≥ 8 is where C8 stops folding at a 0.72 cutoff.
- (c)/(d) at a 0.80 cutoff keep less of 16–20 kHz than (a) at C6: −6.6 dB against −5.1 dB (h19 at −23.5 dB against −6.3 dB). A longer table with a higher cutoff (W 32, 0.90) is one slider move away, but was not measured into this record.
- By-ear preference is the human's.
