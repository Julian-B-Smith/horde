# station-dc-blocker — a 5 Hz one-pole DC blocker on STATION's engine output, in the LAB

- **Queue item:** B153 (suite row S17 — the DC ruling the row was pinned waiting on)
- **Why:** The human ruled 2026-09-19: "Let's add a DC blocker to Station, and then
  I would actually like to see it build into the VST." Audit §2.7 / suite S17 had
  measured real DC by configuration — self-feedback −26 dB, SHORT noise −30 dB,
  a 10 % pulse −1.9 dB, against a −61…−65 dB DC-free control — all of it
  envelope-multiplied at the source, so every note-on was a thump and 16 voices
  summed theirs. Differing from the reverb precedent (blocker in the port only),
  it goes in the LAB so the coming C++ port copies it and parity INCLUDES it:
  one fewer out-of-parity stage for the port to reason about.

- **Evidence consulted:** `docs/audits/2026-09-18-station-lab-audit.md` §2.7 and
  the S8/S17 rows of §5; `specs/SPEC-STATION.md` §1/§2/§11/§12;
  `reference/station.html` (`StationCore`, the per-sample level/pan sum and the
  monitoring `tanh`); `tools/labharness/station_check.mjs` (the `planted()`
  anchored-substitution mechanism, which is what made the must-fail control
  cheap); `tools/golden/extract_core.mjs` (banner contract).

- **What changed.**
  1. `reference/station.html`: `DC_FC = 5` (Hz) and `DC_FLOOR = 1e-30` named next
     to the other DSP constants; `this.dcX`/`this.dcY` (one pair per CHANNEL, not
     per voice — the offset is summed) on the core; `dcR = exp(−2π·DC_FC/SR)`
     derived once per `render()` call, NOT cached in the constructor because the
     audio graph assigns `core.SR` after construction; `y = x − x₁ + R·y₁` applied
     to `ml`/`mr` after the level/pan sum and before the master gain and the
     monitoring `tanh`, so the port (which has neither) copies the same stage in
     the same place. The tail is flushed to exact zero below `DC_FLOOR` — without
     it the one-pole tail idles in float32's subnormal range for ~20 s after the
     last voice dies and S15 fails.
  2. `specs/SPEC-STATION.md`: the blocker in §2's diagram plus one paragraph
     stating form, `f_c`, position, state, and why it is not the tone filter §1
     rules out; §11 gains numbered item 7 — the blocker IS a parity item, with the
     exact port instructions — and the old item 7 (LFSR seed derivation) becomes 8.
  3. `tools/labharness/station_check.mjs`: S17 turned from a PIN into a gate; new
     S21 for the cutoff; S1's hash pin moved; S8 and S9 taught to see through the
     new stage; header note updated (S17 is no longer one of the four pins).
  4. `docs/design/station-page-lab.html`: the stale "reference/station.html CANNOT
     be read by extract_core.mjs" note replaced — it predates the `StationCore`
     class wrap and the reference's banners; the suite reads it every run.

- **Numbers (measured, 48 kHz, 2 s held note, mean over the LAST 1 s so the
  blocker's 32 ms settle is not read as offset; dB of |mean| below peak / below
  RMS).** Pre-blocker → post-blocker:

  | configuration | pre peak/rms | post peak/rms |
  |---|---|---|
  | SIN, no feedback (control) | −67.8 / −64.8 | −68.3 / −65.3 |
  | QTR raw (control) | −61.4 / −56.7 | −61.4 / −56.6 |
  | self-feedback index 8 | −24.1 / −21.6 | **−95.3 / −91.7** |
  | NS SHORT | −30.0 / −30.0 | **−65.0 / −64.6** |
  | PLS raw, pw 0.1 | −1.9 / −1.9 | **−76.8 / −67.2** |

  The gate is −55 dB (peak-referenced) / −50 dB (RMS-referenced): the DETECTOR'S
  OWN FLOOR plus ~6 dB. A 1 s mean of a 261.63 Hz note is not a whole number of
  cycles, and that truncation residue is what the two control rows read — both
  before and after — so "zero DC" is not a number this probe can produce, and
  claiming one would have been the wrong gate. The audit's original S17 figures
  (−26.7 / −29.9 / −1.9, control −61.2 / −64.5) were taken over the FIRST 1 s
  from note-on; the small differences above are that window change, not drift.

  Blocker response, measured (post vs. planted pre-blocker, FIXED-mode sine so
  every tone is a whole number of cycles in the window): **5 Hz −3.0074 dB**
  (a one-pole's −3.0103 dB), 10 Hz −0.9662, 25 Hz −0.1675, 50 Hz −0.0404,
  **100 Hz −0.0080**, 400 Hz +0.0022, 1000 Hz +0.0027 dB.

  Render hash (S1, the max patch): `75e363cf732c9081` → **`7aa8506c5eee18e1`**.
  Expected and deliberate. The brief quoted `bc9eea68afc8bfd9`, which is two
  moves stale — it is the pre-ADR-177 §3 value recorded in the audit, and S1's
  own comment carries the chain. The lab's other invariants held unchanged: two
  instances bit-identical (S1), silence → exactly 0 (S14), block sizes
  {1,7,64,256,333} bit-identical to one call (S13), no subnormal output samples
  6 s past note-off (S15).

- **The detector is falsifiable (the must-read-zero discipline, LIBRARY L0016/
  L0032).** Three independent controls, not one:
  1. *Inside the row:* S17 measures every configuration on BOTH the shipped lab
     and a `planted()` pre-blocker build, and requires the three DC-prone rows to
     BREACH the gate pre-blocker while the two DC-free rows sit at the floor in
     both — so the row cannot pass by reading "blocker present".
  2. *Whole-suite:* run against a scratch copy of the lab with the blocker
     reverted, S17 and S21 fail on the anchor guard (loud, by design). Run against
     a copy where the blocker still computes but its result is discarded — so the
     anchor still matches — they fail on their NUMBERS: S17 reads
     −24.1 / −30.0 / −1.9 against the −55 gate, S21 reads +0.0000 dB at 5 Hz.
  3. *S21's own zero:* the same difference must read 0.000 ± 0.01 dB at 1 kHz, or
     the probe is reporting a constant offset and its −3 dB means nothing.

- **Alternatives rejected:**
  - *Blocker in the C++ port only* (the reverb precedent) — rejected by the human's
    ruling and for the reason given in the brief: it leaves a stage out of parity.
  - *`R = 1 − 2π·f_c/f_s`* (the cheaper approximation the brief offered) — chose the
    exact `exp(−2π·f_c/f_s)`. It costs one `exp` per render CALL, not per sample,
    and it puts the −3 dB point where the constant says it is: S21 measures
    −3.0074 dB at 5 Hz, which is a gate the approximation would have made fuzzy.
  - *Per-voice blockers* — 16× the cost for the same result; DC is summed.
  - *Caching `dcR` in the constructor* — would have been a 48 k pole on a 44.1 k
    host, because the graph assigns `core.SR` after construction. This is ADR-009's
    trap wearing a different hat, so the coefficient is derived per render call.
  - *Loosening S8/S9 instead of teaching them the new stage* — never. Both rows
    state their gate in terms of an OPERATOR's signal and recover it by inverting
    the monitor path; the blocker is exactly invertible
    (`x[n] = y[n] + x[n−1] − R·y[n−1]`), so `unmonitor()` undoes it and the
    assertions are unchanged. S8's residual IMPROVED, 3.22e-2 → 7.11e-15, because
    the inverse integrates (it amplifies input noise by up to 1/(1−R) ≈ 1500×) and
    so those two rows now render into `Float64Array` rather than the graph's
    float32. S9 returned to exactly 1.000000 (it read 1.1297 through the blocker's
    note-on transient, which is the monitor path's business, not the loop's).
  - *Hard-coding 5 Hz in the harness* — the suite reads `DC_FC` out of the lab, so
    moving the lab's cutoff moves the row instead of silently passing.

- **Verify:** `./verify full` — see the report; run on the committed hash.

- **Open questions:**
  1. **The brief's premise that `station_check.mjs` is run by `./verify full` is
     false.** `grep -n "station_check" verify` returns nothing; the only lab gate
     in `verify` is `tools/labharness/lab_load_check.mjs` (verify:162), which
     asserts a lab's JS evaluates and nothing about what it computes. The suite's
     own header says so explicitly ("WHY IT IS STANDALONE AND HAND-RUN"). Wiring
     it would edit `./verify` — a human gate and explicitly forbidden by the brief
     — so it was not done, and S17/S21 are hand-run gates today. Ruling wanted:
     wire `station_check.mjs` into `./verify full`, or leave it hand-run?
  2. `docs/design/station-page-lab.html` has its OWN `StationCore` (the build-side
     core, with FREE/RING/STEPPED) and did NOT get a blocker — the brief scoped
     the page to "still loads" plus the comment fix, and out-of-scope named "any
     other lab behaviour". If that page is meant to track the reference's signal
     chain, it needs the same stage; that is a decision, not an oversight.
  3. `docs/audits/2026-09-18-station-lab-audit.md` §2.7 / §5 row 17 still describe
     S17 as an open FAIL awaiting a ruling. Audits are historical records, so it
     was left alone, but a reader arriving there first will be a day stale.
  4. The C++ port of the blocker is a separate brief (out of scope here). §11
     item 7 carries the exact instructions for it.
