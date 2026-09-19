# station-core-phase1 — STATION ported to C++ with its golden chain and a standalone check

- **Queue item:** B153 layer 3 (the port), B44 (port order). Phase 1 of 2: core +
  oracle only. The shell seam (id block, source row, GUI page) is phase 2 under a
  separate brief; nothing in `src/hypersaw_clap.cpp` or the GUI was touched and
  the param count is unchanged at 246.

- **Why:** The human's ruling 2026-09-19, verbatim: *"Let's add a DC blocker to
  Station, and then I would actually like to see it build into the VST."* The
  port lands on proven ground first — the same order the swarmalator port used
  (core + parity chain before any shell integration).

- **Evidence consulted:**
  - `specs/SPEC-STATION.md` §2–§12 (the engine, the parameter table, the parity
    list and its six deliberate divergences, the performance budget).
  - `reference/station.html` `StationCore` — the parity oracle, sliced live by
    `tools/golden/extract_core.mjs`; never forked (ADR-003, LIBRARY L0001).
  - `docs/audits/2026-09-18-station-lab-audit.md` — S3 (the truncated PM
    literal), S4/§2.4 (LFSR seeding), S5/§2.5 (the Nyquist clamp), S8/§2.7 (DC),
    S9 (env freeze), §2.2 (the Euler envelope), §3.1/§3.3 (the two provably
    bit-identical reductions), §3.4 (the CPU framing), §4.1–§4.4 (parity
    strategy), §5 (the suite rows and every threshold's origin), §6.3 (what must
    not be ported).
  - `tools/labharness/station_check.mjs` — S3/S4/S8/S12/S13/S17/S19 for
    thresholds and, critically, S8's correction of the audit (below).
  - `tools/golden/gen_swarmalator_goldens.mjs` + `tools/swarmalator_check.cpp`
    (the port idiom), `src/spectra_core.h` + `src/swarmalator_core.h` (the core
    idiom), `src/force_core.h` (`rngNext`, `onePoleCoef` — reused, not rewritten),
    `CMakeLists.txt` ~560-600 (the check-target idiom), `verify` ~370-415 (how a
    golden chain is run — NOT wired; that is a gate edit and a human decision).
  - `origin/station-dc-blocker` — the sibling's lab + spec diff, read before the
    output stage was written, so the blocker is the same form, the same
    `R = exp(-2pi*f_c/f_s)`, the same position, the same per-channel state and
    the same 1e-30 flush. SPEC §11 item 7 on that branch makes it a PARITY item.

- **What landed:**
  - `src/station_core.h` — `hypersaw::StationCore`. Header-only, no allocation
    after construction, no wall-clock, mulberry32 only, all time constants in
    seconds. The STATUS header states parity-vs-divergence line by line.
  - `tools/golden/gen_station_goldens.mjs` — 15 scenarios x 2 rates (48 000 /
    44 100), block 128, `--selfcheck` determinism mode, manifest + f32 stereo
    into `build-golden/station/`.
  - `tools/station_check.cpp` — parity + 15 behavioural/divergence rows + a CPU
    report. UNWIRED, by the charter.
  - `CMakeLists.txt` — one `add_executable`, the existing core-direct idiom.

- **Findings that changed the plan (each is a measurement, not a preference):**
  1. **The audit's §4.1 claim that bit-parity is available "for the whole DSP" is
     FALSE on the self-feedback diagonal above index ~2.** `out = sin(2pi*ph +
     cell*prev)` has `d(out)/d(prev) ~= cell*cos(.)`, so the map is contracting
     below index 1 and chaotic above it, and V8's `Math.sin` and libm's differ by
     ~1 ulp somewhere in range. Measured, this machine, RMS vs the lab at
     48 k / 44.1 k: index 0.9 `0.000e+00 / 0.000e+00`; 1.0 `0.000e+00 /
     0.000e+00`; 1.2 `1.585e-07 / 3.232e-08`; 2.0 `1.257e-07 / 2.408e-08`;
     **4.0 `2.091e-01 / 2.084e-01`; 8.0 `3.085e-01 / 3.031e-01`.** The brief
     asked for "a self-feedback patch at a high index"; index 8 was tried first,
     failed by five orders, and the scenario is pinned at index 2 (8x under eps)
     with index 8 gated BEHAVIOURALLY instead (bounded, finite, |op| <= 1.000000
     over 10 s on all three diagonals). The ladder is recorded in the generator
     so nobody "improves" it back to 8.
  2. **The audit's Bessel-sideband-sign detector for the one-sample delay is not
     used**, because labharness S8 already disproved it (a planted no-delay build
     reproduced the pattern to 3 decimals — one sample of delay on a sinusoidal
     modulator is a pure phase rotation and |Jn| is invariant under it). The
     lag-residual detector is used instead and separates the two hypotheses by
     **2 813 837x** (lag 1 `4.257e-08`, lag 0 `1.198e-01`).
  3. **The truncated PM literal is load-bearing, measured core-vs-core** at the
     audit's own patch (SAW carrier / sine modulator 7:1, 2 s): `1/(2pi)` differs
     from `0.1591549` by rms `3.83e-07` at index 1 (under eps), `1.49e-06` at
     2.6, `1.41e-06` at 4, **`4.19e-06` at 8 — over eps**. The measurement is
     core-vs-core rather than core-vs-golden on purpose: index 8 cannot be a
     parity scenario at all (finding 1).
  4. **SPEC §12's <= ~2 % CPU budget is NOT met as built.** 16 voices, max patch,
     48 kHz, 5 s, min of 3: **5.21 % of one core** — 3.6x the Node lab, against
     the >= 9.4x the audit says the budget needs. That is after applying the
     audit §3.3 reductions (both re-proved bit-identical by the parity rows going
     to 0.000e+00) plus a third: `opFreq` paid a `std::pow` per operator per
     sample to multiply by exactly 1, since `fine` is 0 in every factory patch.
     Remaining hot spots, unaddressed and unmeasured individually: the pitch
     envelope's `pow`+`exp` per voice per sample, and the DRW mipmap read.
  5. **The DC blocker needed a denormal flush that the first draft lacked** —
     measured **25 418 subnormal output samples** in the 6 s after note-off (a
     5 Hz one-pole takes ~2.6 s to decay into the subnormal range and then sits
     there). The sibling's lab independently arrived at the same 1e-30 flush.

- **Alternatives rejected:**
  - *Replicating the lab's master `tanh` to take parity at master 0.75.* Rejected
    — SPEC §11.5 says the engine output is clean. The generator INVERTS the
    monitor stage instead (`atanh(y)/(0.02*1.4)`), which is exact rather than a
    linearisation and keeps parity at full engine amplitude; comparing at master
    0.02 would have handed the eps budget a 36x discount it did not earn.
  - *A string-keyed `setParam` on the core.* Rejected — ~100 lines of strcmp the
    shell's id switch would duplicate. The Patch is a public struct and the §10
    address -> field map is in the header. The check owns the one `applyKey` the
    manifest needs, and an unknown key there is a hard failure.
  - *Mirroring the scenario table in C++.* Rejected — two languages drift. The
    generator dumps the WHOLE resulting lab state as flat key=value tokens.
  - *Grepping the lab for a DC-blocker variable name.* Rejected — an anchor that
    rots silently. The generator DETECTS the blocker behaviourally (a 10 %-duty
    raw pulse is 80 % DC; the two answers are 0.7997 and 1.512e-4, no middle
    ground) and writes `@dcblock` into the manifest.
  - *Fixing the lab defects the spec does not list as divergences* (the op-OFF
    envelope freeze S9, the linear pan law S10, the 163 % release S13).
    Rejected — each is an ADR owed, not this brief's to invent. Preserved and
    named in the core header.
  - *An FFT harness for the DRW alias floor.* Rejected — none exists on the C++
    side. A Goertzel at the folded bins reads the same quantity one bin at a
    time: band-limited **-125.0 dB**, raw ZOH **+1.3 dB**, against the lab lerp's
    -43.7 dB.

- **Verify:** `./verify full` exit 0 on this tree; re-run on the committed hash
  and reported verbatim in the PR. `tools/station_check` run by hand against
  BOTH golden sets, per the brief:
  - goldens from `origin/main`'s lab (blocker ABSENT, `@dcblock=0`, core
    bypassed): **GREEN, 0 failures, worst parity rms 1.257e-07**.
  - goldens from `origin/station-dc-blocker`'s lab (blocker PRESENT,
    `@dcblock=1`, core enabled): **GREEN, 0 failures, worst parity rms
    1.257e-07**. 28 of 30 scenarios are bit-exact (`0.000e+00`).
  Goldens are generated artifacts under `build-golden/` (gitignored), so there is
  no committed set to regenerate when the blocker merges: the next generator run
  flips `@dcblock` on its own. No bookkeeping is owed.

- **Open questions (for the lead):**
  1. **Wiring.** `station_check` is UNWIRED. `./verify` is protected and B159's
     wiring-default question is the human's. When wired it belongs beside the
     swarmalator chain: `--selfcheck`, generate, then the binary with the golden
     dir as argv[1]. Cost: the generator is ~40 s, the check ~90 s.
  2. **The CPU budget.** 5.21 % against §12's ~2 %. Is that a blocker for phase 2,
     a ruling to re-measure the budget, or an optimisation queue item? The two
     named hot spots are the per-voice pitch-env `pow`/`exp` and the mipmap read.
  3. **Three preserved lab defects need ADRs** before 1.0: the op-OFF envelope
     freeze (S9), the linear pan law (S10, also a 3 dB gain control), the 163 %
     release (S13). The port carries all three for parity, deliberately.
  4. **Smoothing beyond the matrix.** §4 asks for 5 ms on cells only and that is
     what landed (measured 1.10x the natural slope, vs the lab's 13.9x). The
     check's own control shows op LVL still steps at 2.33x — audit S6 says LVL
     and MASTER need it too. Spec change, not a build call.
  5. **`noteOn` takes no velocity.** SPEC §10 declares no velocity parameter, so
     none was invented; the shell scales at the mixer as it does for every other
     core. Flag if phase 2 needs it.
  6. **FREE phase mode had to be defined, not ported** (the prototype has none).
     Chosen: a per-op free-running accumulator against middle C, the same
     reference KEYTRK already uses — deterministic, 3 adds/sample, skipped
     entirely when every op is RETRIG. Worth a ruling if a different definition
     is wanted.
