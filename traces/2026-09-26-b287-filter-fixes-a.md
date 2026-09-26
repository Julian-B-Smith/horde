# b287-filter-fixes-a — the filter lab's phase-A fidelity findings fixed, two held with numbers

- **Queue item:** B287 phase A (row carried in PR #770, branch `lead-records-99`; human 2026-09-26: "Let's fix
  all failures."). Follows B274 (`traces/2026-09-26-b274-filter-lab-round2.md`). Phase B (C4, C5) is held for
  the B290 tolerance ruling and is untouched. The port of the SVF law to `src/svf_core.h` is B289.
- **Why:** B274's fidelity programme reported 17 FAILs. Phase A covers the findings whose tolerance is not in
  question: C2 (COMB +, the bank's clamps), C3 (LADDER), C6 (level at resonance). Each was either fixed at its
  cause, or held with the numbers that show why a fix would trade something else.
- **What changed:**
  - `docs/design/filter-lab.html` (DSP section and page text):
    - **C6, SVF 12 / SVF 24.** The BP, HP and PEAK taps are × √(k/k₀) (k₀ = √2; for SVF 24, the resonant
      stage's K24B). A BP tap's energy for a broadband input is ∝ 1/k exactly, and √k holds it. HP and PEAK
      take the same law because, with the cutoff above the note, their level is the same 1/k peak. LP and
      NOTCH are untouched.
      - The law lives in the TYPE (`svfComp`, used by `Svf12` and `Svf24`). The `Svf` class stays the literal
        transcription, so DJ (which calls it bare) is unchanged.
      - **This is the lab's one divergence from `src/svf_core.h`**, and the page says so in its header, in
        the `Svf` comment, and in the SVF 12 level law.
    - **C6, LADDER.** The output is × (1 + k/2), the half-compensation of the passband (Gcomp = ½, cited
      from memory as Välimäki & Huovilainen 2006; not re-checked against the paper here).
      - It is applied after the tanh, so drive and C5 are untouched.
      - The passband still sinks, to (1 + k/2)/(1 + k): −4.4 dB at res 0.9, where it was −13.6.
      - The resonance-to-bass ratio is unchanged, so the thinning is kept. That is why this is not the
        refusal case the brief names.
    - **C6, FORMANT.** Every band's gain is × √qs (qs, the Q multiplier), folded into `amp[]` so the DSP and
      `H()` cannot disagree. A unity-peak band passes energy ∝ 1/Q, and res 0.5 is unchanged.
    - **C3, LADDER. Cause:** there is no tuning error. At 3 s and 6 s the steady-state pitch reads −0.03 ¢
      at 110, 220, 440 and 1760 Hz (scratch probe `c3.mjs`).
      - The res → k line ran to k = 4.2 at res 1. That is 5 % past the Barkhausen k = 4, but only 1.2 % per
        stage. The ring's growth rate is the pole's real part, σ = (k^¼/√2 − 1)·ωc = 0.012·ωc.
      - So a 110 Hz ring from C3's 1e-3 kick was still growing in C3's window, 1.0–1.5 s. Its RMS per
        quarter-second read −48.0, −33.9, −30.0, then −29.8 steady. The window caught the tail of the start-up.
      - The limit cycle sat barely into the tanh (describing-function gain 4/4.2), hence −29.8 dBFS.
    - **C3, LADDER. Fix:** k = 4.2·res up to the unchanged onset 0.952. Above it, a self-oscillation segment
      runs k from 4 to 4·1.05⁴ = 4.862. That is 5 % per stage: σ = 0.05·ωc, exactly the MS-20's
      (K − 2)/2 at res 1. Derived, not fitted.
    - **C2, COMB.** The delay is read by 3rd-order Lagrange, not linearly.
      - The linear read is a low-pass: |L| = 0.9756 at 3.52 kHz (fr 0.636), and the peak magnifies the
        shortfall by g/(1 − g).
      - Lagrange gives |L| = 0.99905 there, and max |L| = 1.000000000000 over fr ∈ [0, 1) × ω ∈ [0, π]
        (scratch `lagr.mjs`), so the loop stays passive.
    - **C2, SWARM · PROPOSED: P4 range** (a new PROPOSED switch, default on, with a toggle and a diagnostic
      row).
      - The range is the SVF core's own [10 Hz, 0.49·fs].
      - The placement is FITTED, not clipped: the spread contracts about the centre in the placement's own
        scale.
      - The reference's physics run in a log2 frame that keeps its 5.32…13.42 band clamp out of reach.
      - Coefficients are capped at 0.49·fs, not 0.24·fs.
      - It is installed as an instance override of `placeTargets()`. `reference/swarmfilter.html` is not
        touched.
      - The gravity force reads `f0last` unframed; the lab never switches gravity on (noted in the code).
    - **C2 detector tightening:** the bank's C2 now reads each band's REALISED centre, capped where its
      coefficient is capped (`bankCentreCents`, shared with the P4 diagnostic). Before this, a revert of
      P4's cap to 0.24 was invisible to C2 (L0033: the plant that did not fire measured the coverage
      boundary). No tolerance moved.
    - **Held cells are labelled in the page.** A `held` note appears on the cell, the row tag and the
      findings list: SWARM · REF C2/C6 read "PINNED — a property of reference/swarmfilter.html", and
      COMB ± C6 read "HELD". Each type's `law` is printed in the types card and the inspector. Q6 has been
      rewritten to phase-A status.
    - `<meta name="lab-review" content="B274 + B287 · 2026-09-26">`.
  - `tools/labharness/filter_fidelity_check.mjs`: twelve pins moved F → P (fixed findings, a tightening).
    - Eight plants added, one per new compensation law (SVF 12, SVF 24, LADDER, FORMANT; removing any one
      turns its C6 red) and one per other fix (the ladder segment → C3, the linear comb read → C2, the P4
      fit → C2, the P4 cap → C2).
    - The comb plant's anchor was re-pointed at the Lagrange line.
    - 13/13 plants are caught. Each fails for the reason it exists (scratch `plantvals.mjs`). For example,
      removing the SVF 12 law brings back exactly round 2's 10.3 dB; removing the ladder segment brings
      back exactly its 6.7 ¢.
- **Before → after:** measured at 48 kHz, every cell dumped before (origin/main) and after (scratch
  `before.txt` / `after.txt`). No other row's verdict changed.

  | Finding | Before | After |
  |---|---|---|
  | SVF 12 BP C6 | 10.3 dB | 0.3 dB |
  | SVF 12 HP C6 | 10.5 dB | 0.5 dB |
  | SVF 24 BP C6 | 12.2 dB | 2.2 dB |
  | SVF 24 HP C6 | 9.1 dB | 0.9 dB |
  | SVF 24 PEAK C6 | 7.5 dB | 2.5 dB |
  | LADDER C6 | 8.6 dB | 4.2 dB |
  | FORMANT C6 | 8.5 dB | 3.1 dB |
  | LADDER C3 | 6.7 ¢ · −31.5…−29.8 dBFS · onset 0.952 | 0.4 ¢ · −14.0…−13.8 dBFS · onset 0.952 |
  | COMB + C2 | 2.05 ¢ · 0.738 dB | 0.15 ¢ · 0.030 dB |
  | SWARM · PROPOSED C2 | 1026.71 ¢ | 0.00 ¢ |

  - **LADDER C3, the split:** the segment alone gives 0.4 ¢ at −24.7…−24.5 dBFS. The level law adds
    +10.7 dB at res 1. With the law but without the segment, the reading is still 6.7 ¢ (−21.7…−20.0 dBFS).
  - **COMB − C2** (already a pass) went from 1.81 ¢ · 0.191 dB to 0.03 ¢ · 0.002 dB.
  - **SWARM · PROPOSED C2, cross-checked independently:** the peaks of the measured impulse response, with
    P2 off, have an ERB mean within 2 ¢ of 55 Hz, 220 Hz, 880 Hz and 12 kHz. At 12 kHz, 16 resolved peaks
    span 6.1–23.1 kHz. REF's peaks at 55 Hz land 1640 ¢ off (scratch `bankpeaks2.mjs`).
  - **Other values that moved; verdicts unchanged:**
    - C4 static bounds of the compensated types are lower, and their ratios are unchanged.
    - LADDER C4: +4.2 → +5.0 dB.
    - SWARM · PROPOSED C4: −2.0 → +0.3 dB.
    - C7 DC gains now include the laws: LADDER −3.592 dB, PEAK −3.010 dB. The analytic value matches in
      each case.
- **Held, with evidence (grounded refusals):**
  - **COMB + / COMB − C6.** Scalar compensation trades the comb's design point.
    - The comb's `(1 − |g|)` puts every peak at 0 dB. Played as designed (keytracked, cutoff = f0 for
      COMB +), its level vs res holds within 0.0 dB. For COMB − tuned at cutoff = 2·f0 it holds within
      1.2 dB.
    - The energy law √(1 − g²) would pass COMB + C6 (15.0 → 3.8 dB), but would make the keytracked COMB +
      12.0 dB louder at res 0.9. For COMB − it would still FAIL C6 (6.1 dB) and make the tuned case
      10.8 dB louder.
    - C6's fixed 1 kHz cutoff over a 110 Hz saw puts the comb's peaks BETWEEN the harmonics. That is the
      one case its peak law cannot hold.
    - Pins stay F, labelled HELD.
  - **SWARM · REF C2 and C6.** These are properties of the protected reference. Compensating REF in the
    wrapper would stop it measuring the reference. PROPOSED carries the fixes (P4 for C2; P1, already passing
    C6 at 1.0 dB). Pins stay F, labelled PINNED.
  - **SVF 12 PEAK "comfortable":** not reached. It went from 5.5 dB (rising, −16.3 → −10.8) to 4.5 dB
    (falling, −16.3 → −20.8).
    - PEAK's level is carried by its unity passbands, like LP's. No source-independent scalar law holds it:
      √k overshoots, and none overshoots less.
    - An exponent between them (e.g. (k/√2)^¼ reads 0.8 dB) would be fitted to this test. Declined.
- **The cost of a single geometry (informational, not a check):** the same measurement with the cutoff ON
  the fundamental (keytracked, fc = f0 = 110 Hz). Swing, before → after:
  - The SVF laws improve it: SVF 12 BP 19.4 → 9.4, HP 16.6 → 6.6, PEAK 18.6 → 8.6; SVF 24 BP 19.9 → 9.9,
    HP 16.5 → 6.5, PEAK 19.5 → 9.5 dB.
  - FORMANT improves: 7.5 → 2.1 dB.
  - LADDER gets worse, 13.3 → 22.5 dB. It already failed there, and the law lifts the peak with the
    passband.
  - SWARM · PROPOSED gets worse, 7.1 → 14.7 dB: P4 halves the effective spread at a 110 Hz centre.
  - SVF LP is 19.9 dB there, untouched.

  Every scalar level law assumes where the source's energy sits relative to the cutoff; BP's is the only one
  exact for any source. This bears on B290.
- **Must-fail proof (wired gate).** A scratch copy of the committed lab at e4da7d4 had all four laws removed
  (`svfComp → 1`, `ladderComp → 1`, the formant `* qg` dropped). The gate went RED, exit 1, with 7 C6 cells
  flipping: svf12 BP 10.3, svf12 HP 10.5, svf24 BP 12.2, svf24 HP 9.1, svf24 PEAK 7.5, ladder 8.6 and
  formant 8.5 dB. The formant plant's anchor was, as expected, absent from the copy. Summary line: `RED —
  filter_fidelity_check: 20 types × 7 checks, 126 pass, 14 FAIL (7 of them pinned findings), 140/140
  controls fail as they must, 12/13 planted faults caught, 8 error(s)`.
- **Evidence consulted:**
  - ROADMAP B274, B287, B289 and B290 (on `origin/lead-records-99`).
  - The B274 and B209 traces.
  - `reference/swarmfilter.html` :161-379 (read, not modified).
  - `tools/golden/extract_core.mjs` and `tools/labharness/filter_fidelity_check.mjs`.
  - INDEX L0016, L0024, L0032, L0033, L0052 and L0056.
- **Alternatives rejected:**
  - **Thiran all-pass for the comb.** It is exactly unity, and COMB + C2 read 0.86 ¢ · 0.000 dB. But its
    state turned COMB − C4 from −2.2 to +13.5 dB under the per-sample sweep: a C2 pass bought with a C4
    fail.
  - **Scaling the ladder's feedback past 4.2 linearly.** The onset would move below res 0.9 and break C2
    and C6.
  - **√(1 + k) for the ladder** reads 3.4 dB. It was not chosen because the published ½ law exists.
  - **Compensating LP/NOTCH:** LP would fail C6, at 7.3 dB.
  - **Clamps only for P4, no fit:** the ERB scale ends at 0 Hz, so a 55 Hz centre with ±6 ERB cannot be
    centred.
  - **Changing C3's measurement window.** That is a gate change, a human gate.
- **Found, not fixed (pre-existing, outside the brief):** `filter-lab.html` `FilterRig.control()` returns
  early for PER SOURCE placement BEFORE its `rType !== R.type` rebuild. Choosing a SWARM type for the rack
  in the default placement therefore leaves a stale non-bank `rInst`. `drawPath` then reads `inst.b.p` and
  throws (`Cannot read properties of undefined (reading 'p')`, :1965), which ends the rAF loop: the canvases
  freeze and the proposal diagnostics never fill.
  - It reproduces headless with `?rtype=bankP` on this branch, and on origin/main 340963f (the same error at
    that file's :1802), served from a scratch copy. It is pre-existing, from B274.
  - It is a one-line reorder (check `rType` before the early return), but not a phase-A finding, so it is
    left for a row.
  - The diagnostics screenshot uses the default rack type for that reason.
- **Screenshots** (scratchpad `b287/shots/`, not committed):
  - `fidelity-light.png` and `fidelity-dark.png`, the fidelity table (virtual-time headless, with the crash above stopping the rAF loop, which is why the table completes quickly);
  - `proposals-light.png`, the P1–P4 diagnostics;
  - `c6-before-after.png`, level vs res on a fine grid for SVF 12 BP/HP, LADDER and FORMANT;
  - full pages `full-light.png` (real time; its table was still filling at capture, but its diagnostics are live) and `full-dark.png`.
- **Verify:** `./verify fast` exit 0 at git e4da7d4 (`.harness/last-verify.json`). The line was: `GREEN —
  filter_fidelity_check: 20 types × 7 checks, 133 pass, 7 FAIL (7 of them pinned findings), 140/140 controls
  fail as they must, 13/13 planted faults caught, 0 error(s)`.
- **Open questions:**
  - Rule on the COMB ± C6 holds: keep the peak law (the tuned level), or take the energy law (C6's
    geometry)?
  - Is C6's single geometry the right bar (B290)? A second, keytracked C6 row would show the ladder and
    PROPOSED trades above.
  - PEAK's law (see above).
  - The ladder citation is unverified.
  - P4 contracts the spread at the range ends. Is that what the human wants, or should the bank be allowed
    to sit asymmetric there?
  - The pre-existing rack-type crash.
