# subosc-core-phase1 — the SUB OSC C++ core, its golden chain and its check

- **Queue item:** B155 (SUB OSC), port phase 1: the core + oracle only. No shell
  edits — the routing source row 2 (ADR-178) is phase 2 under a separate brief.
- **Why:** SUB OSC became a protected reference on 2026-09-19 (ADR-178) and the
  human ruled the port. Phase 1 mirrors STATION's (PR #678,
  `traces/2026-09-19-station-core-phase1.md`): land the core and its oracle
  first so the shell integration lands on proven ground. One difference from
  that brief — ADR-180 §1 inverted the wiring default, so the chain is wired
  into `./verify` in this PR rather than left for a ratification.
- **Evidence consulted:** `specs/SPEC-SUBOSC.md` (§2 architecture, §3 shapes,
  §4 pitch, §5 tone/level/envelope, §6 sync, §7 the parameter table, §9 parity
  list and named limits, §10 the MEASURED acceptance rows); `reference/subosc.html`
  `SubOscCore` (the parity oracle, ADR-003); `tools/labharness/subosc_check.mjs`
  (39 rows — the measurement method for every §10 row was ported from it);
  `src/station_core.h`, `tools/golden/gen_station_goldens.mjs`,
  `tools/station_check.cpp` (the idiom); `verify` ~370-430 and `CMakeLists.txt`
  ~616 (the chain and target idioms); `tools/test_table_check.py` (the wiring
  rule); `src/force_core.h` (`rngNext` — mulberry32, reused not re-written).
- **What landed:**
  - `src/subosc_core.h` — `hypersaw::SubOscCore`, header-only, allocation-free,
    one instance PER VOICE (§1/§8.2 make the sub a per-voice source; the class
    holds one note's state and `noteOn` replaces rather than layers — stated in
    the STATUS header so phase 2 instantiates it correctly). No divergence from
    the lab: §9's table of divergences is the LAB's departure from the SAW
    engine's habits, and the port inherits all five. Parameters enter through
    one clamped table with `setParam` throwing on an unknown address (§7 / audit
    A2); `attack`/`release` are carried and flagged PROVISIONAL (§5.3, R4).
  - `tools/golden/gen_subosc_goldens.mjs` — 28 scenarios x 48 000 / 44 100 Hz
    sliced live from the HTML; whole-parameter-table dump per row; the sync
    scenarios STATE their master source (a saw at an absolute Hz, dumped to 17
    digits so no `Math.pow` sits in the shared path).
  - `tools/subosc_check.cpp` — 91 rows: 56 parity + §10.2/.3/.4/.5/.6 re-measured
    with controls + §7's table contract + a CPU report.
  - Wired: `verify` gained the three-line golden chain beside station's AND the
    lab suite at the top of `full()`; `CMakeLists.txt` gained the target;
    `tools/labharness/subosc_check.mjs`'s header rewritten (it had carried
    "UNWIRED: reason not stated — see B159").
- **Measurements:**
  - Parity: **0.000e+00 on all 56 scenarios** — bit-identical at both rates, not
    merely inside eps = 1e-6. Load-bearing prerequisite, measured before a line
    of the core was written: `std::pow` and V8's `Math.pow` agree BIT-FOR-BIT on
    the pitch law over all 9600 (note, octave, semitone, fine) combinations this
    module can reach, which is why the phase increment — the one quantity an
    error in accumulates into a discontinuity — is recomputed rather than dumped
    into the manifest. `std::tan` differs by 1 ulp on 5 of 15 (rate, cutoff)
    pairs; that is a 1e-16 move in one filter coefficient.
  - §10.3 reproduces the spec's numbers exactly: attack drift 0.0000 % (45.0000
    ms at every rate), tone tau drift 0.0105 % (795.67 us), |H(987.8)| drift
    0.1581 %, closed-form error 0.1081 %, ADR-009 control 68.20 %.
  - §10.5: 0 subnormals, last non-zero at 1951; control (flushFloor planted to
    0) 584 — the lab's own numbers.
  - §10.6: lobe ratio 0.88231-0.88238 over four notes, trough 0.637; phi = 0
    control 1.00000, phi = +0.25 control 1.13339; peak grid worst |peak - 1| =
    3.898e-09, un-normalised control 1.600000, old-bound control 0.750021.
  - CPU (REPORTED, not gated): 16 instances of pulse + tone + hard sync, 48 kHz,
    5 s, min of 3 = **0.395 % of one core**. SPEC-SUBOSC §10.7 listed this as
    still to measure; this is the first measurement.
  - Added cost to `verify full`, measured per step: generator selfcheck 0.25 s +
    generate 0.10 s + `subosc_check` 0.21 s + lab suite 0.58 s = **1.14 s**.
- **Alternatives rejected:**
  - Porting §10.1's aliasing floors to C++: they need a 65 536-point
    Kaiser-windowed FFT sweep and no FFT harness exists on the C++ side. Instead
    the lab suite (now wired) keeps them, and `subosc_check` carries a narrow
    Goertzel confirmation that the polyBLEP is in THIS build (port -51.9 dB vs
    naive control -38.7 dB at MIDI 60; the lab's FFT reads -51.9 / -40.9). The
    boundary is written into the check's header, not left implicit.
  - Gating a ceiling of 1.0 on every rendered shape: the first draft did, and it
    FAILED — square/saw/pulse read 1.056 and noise 1.425 through the module.
    That is the TPT one-pole's step overshoot at a near-Nyquist cutoff (state
    pole 1-2G = -0.726), not the oscillator exceeding unity: the same saw
    through a 200 Hz tone reads 0.998, and both numbers are printed. Limit L4
    already pins that the numbers are the MODULE's. The row now gates the claim
    R7 actually bought — no shape is QUIETER than full scale, and the bump
    matches the sine exactly — with the shape-level 65 536-point scan as the
    exact ceiling oracle.
  - A naive-vs-TPT control at the lab's 987.8 Hz / fc 500: measured and
    DISCARDED — both laws are rate-portable there (naive drift 0.065 %), so the
    control did not fire. Audit A7's defect lives at a HIGH cutoff, so the
    control now reads both laws at Nyquist with the tone at its §7 default of
    20 kHz: naive 0.891 / 0.864 / 0.575 (40.69 % drift) against the TPT's
    structural zero at every rate.
- **Verify:** `./verify full` exit 0 — see the PR body for the run on the
  committed hash (`.harness/last-verify.json`). Parameter count unchanged at 246
  (the shell is untouched). `test_table_check` GREEN: 45 check files wired (was
  43), 10 declaring UNWIRED (was 11).
- **Open questions:**
  - §11's rulings are the human's and none were taken here: R1 (keytrack off
    pins to C2), R2 (naive triangle), R3 (sync without a BLEP), R4 (do
    attack/release survive the port), R5 (per-module seed vs the voice's), R7
    (spent). The port implements the lab as it stands for all of them.
  - `setParam` throws. That is §7's stated contract and audit A2's fix, but a
    throw is a control-path affordance: phase 2's id switch must use the `Param`
    overload, which cannot name a key that does not exist. Flagged in the header
    rather than designed around.
  - `recalc()` runs the 282-transcendental bump peak search on every parameter
    write, and `bumpAmt`/`bumpPhase` are morphable (§7), so the mod matrix can
    drive it per block. The lab measured ~6.6 ms/s of one core for that at the
    tick rate; this build's whole 16-voice figure is 0.395 %, so it is not a
    problem today — but it is the one place where a control write is not free.
  - The CPU figure is load-sensitive and a single reading is a sample, not the
    figure.
