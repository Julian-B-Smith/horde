# b385-h2-engine-rework — the critic's REWORK (narrow) on B385 at e54b70b

- **Queue item:** ROADMAP B385. An Opus critic reviewed e54b70b with verdict REWORK (narrow); the horde lead relayed
  findings H1, H2, M1–M4 and L1–L6 on 2026-10-01. The work is committed on top of `h2-engine-cp4`, so #893 carries it.
- **Why, finding by finding:**
  - **H1, the B323 cull was never exercised.** The cap rows set the cap before the notes, which made them REFUSE
    rows.
    - Two rows now lower the cap MID-phrase while three tails release; R is 1500 ms, so each 8 ms ramp completes.
      One row puts the HELD note oldest. The other uses per-partial voices (the member fade).
    - New controls: C1 (the cull a no-op), firing on both rows; C2 (the cull takes the oldest voice, held or not).
    - The load readouts (culled, refused, stolen) are now compared with the golden's on every scenario.
  - **H2, quality suites and determinism.**
    - Aliasing and zipper are a named DEFERRED checkpoint in the design doc.
    - Determinism covers three fixed scripts, twice in-process and once in a child process, at host blocks of 1,
      37, 128 and 512.
      - Finding: the mid-phrase cull is block-size dependent within float32 rounding (1.47e-8). That is the
        golden's own law: during a fade the JS renders one sample per call through `Float32Array(1)` to the end of
        the host block. It is bounded at 2^-23 and recorded as a divergence candidate.
    - The CPU numbers are marked "measured with an uncommitted auhost patch; reproducible after #886". The cp4
      trace is not edited (traces are append-only): this entry corrects it, and the design doc carries the mark.
  - **M1, a narrowed gate detector.** `h2_rules_check`'s source detector is a plain substring test again
    (`h2/cores/` or `h2/engine/`), never narrower than the original. Self-cases: `"h2/cores/x.h"`,
    `<h2/engine/engine.h>`, `"../h2/engine/engine.h"`, `"../../h2/cores/scalpel/razor_core.h"`, a
    tab-separated include, a negative case, and rule 4 with `<h2/engine/engine.h>`.
  - **M2, the exclusion path.**
    - The listed names are pinned.
    - A strict 1e-6 bound now applies to each listed row's first 128 frames.
    - Control X1 (xm × (1 + 1e-9) on the ring path) turns all three rows NOT justified.
    - Rule b (RMS exempt; self-divergence ≥ K × the C++'s) is built and selectable (`--chaotic-rule b
      --chaotic-k K`). It is not the default, and the check stays UNWIRED.
    - The ratio was measured over 25 nudges; see the design doc's table.
  - **M3, a floor control.** The FMA build now requires its mean to fall under the pinned floor (17.48% against
    30%). Per-family means are printed. The pin is keyed on platform, compiler and the golden's Node major (a new
    `NODE` stream record).
  - **M4:** the `set()` docs now say render thread, between `render()` calls.
  - **L1:** the CMake comments.
  - **L2:** the doc's `blade.h` row, the floor numbers, and the readout claim (made true).
  - **L3:** the `js.h` rounding note.
  - **L4:** the rows, none skipped.
    - D1: width-locked, mirrored, under collision, on a serial input, on serial twins.
    - D2: under phase FM, width-locked, mirrored, under collision.
    - The ensemble's two switches turned off mid-note.
  - **L5:** an unknown key is red unless whitelisted (the whitelist is empty).
  - **L6, partly:**
    - V1's alternate voice law moved out of product `noteOn` into an `#ifdef H2_ENGINE_FAULTS` member.
    - The evTick/evId writes are gated on `events`. This is bit-neutral: every parity number is unchanged.
    - The stream headers were NOT de-duplicated. Phase 1a's header is `razor_core.h`'s live gate, and merging it
      is a refactor of a gate for no behaviour.
- **Result:**
  - 540 of 543 at parity, events and readouts identical on all 543; 13 controls fire.
  - DET passes for all three scripts. NONINV 543 of 543.
  - The mean bit-exact share is 37.00% (P/ 32.87, E/ 46.33, T/ 43.39, C/ 36.28).
  - The arp ring row is still red under rule 1a, by design until the human rules.
  - Ring ratio over 25 nudges:
    - min 2, medians 564–2430;
    - rule b at K = 10 passes 23 of 25 inputs per row;
    - rule 1a passes 9–14 of 25;
    - the first-128 max is ≤ 8.1e-11 everywhere.
- **Evidence consulted:** the critic's findings as relayed; the golden's `renderPlain` (:1152–1178) for the
  float32 fade; `git show c64cfdb:tools/h2_rules_check.py` (the original detector); this session's measurements.
- **Alternatives rejected:**
  - Asserting block-size independence for the cull script: the golden itself is not independent there.
  - An anchored regex for the rules detector: it was narrower than the substring it replaced.
  - Making rule b the default: it weakens 1a, which is the human's to rule.
- **Verify:** `./verify fast` and `./verify full` on the committed head; verbatim in the report and the PR comment.
- **Open questions:**
  - The ring criterion (1a, b at K, or ADR-065 removal) is the human's.
  - The floor's form needs the human's ratification.
  - M1–M3 have no OFF positions.
  - The float32 fade is a divergence candidate.
