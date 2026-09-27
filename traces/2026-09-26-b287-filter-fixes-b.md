# b287-filter-fixes-b — filter lab phase B under B290: ADAA first, then oversampling; four checks re-cut; B291 fixed

- **Queue item:** B287 phase B, B290 (RULED 2026-09-26) and B291. The rows are carried in records PR #772, branch
  `lead-records-100`. The human, verbatim: "Is there a way to incorporate ADAA to reduce the need for
  oversampling? Otherwise your recommendation is ratified for all tolerances." Follows
  `traces/2026-09-26-b274-filter-lab-round2.md` and `traces/2026-09-26-b287-filter-fixes-a.md`.
- **Why:** B290 is a recorded human decision to change four gates (C3, C4, C5, C6), and B287 asks for every
  failure to be fixed or refused with numbers. The human asked specifically whether ADAA could stand in for
  oversampling. So each nonlinear type got an ADAA-first evaluation, with every option measured through the checks
  that ADAA's half-sample delay puts at risk.
- **What changed:**
  - `docs/design/filter-lab.html` (DSP section and page):
    - **The gates, as re-cut per B290.** Each tolerance is stated in `FID` / `FID_CHECKS` and in the harness header.
      - **C3:** the self-oscillation level must sit within ±6 dB of the unfiltered source's RMS. The source is
        C6's 110 Hz saw at 0.25 peak, −16.8 dBFS, measured by `fidSrcDb` and never typed in. The ±6 dB is the
        lead's choice (C6's bound). Pitch ≤ 5 ¢ and spread ≤ 3 dB are unchanged. A new control checks that the
        ring +12 dB reads out of bounds.
      - **C4 (the gate):** round 2's two stimuli (random jumps across 20 Hz–20 kHz, and ±4 oct at 1 kHz) are now
        delivered on the plugin's mod tick. `MOD_GRID_S = 256/44100` is `kGravGridSeconds` at
        `src/swarm_core.h:136`, 279 samples at 48 kHz. Bounds are unchanged (+12 / +40 dB), and so is the DF2
        control.
      - **C4b (reports, never gates):** the same stimuli, delivered every sample (the bank per 16-sample tick),
        at rates from 172 Hz upward in half-octave steps to 22 kHz. It reports the maximum viable rate, the last
        rate before the first break. Its control: the DF2 biquad must break at the first rate.
      - **C5:** −60 dBc at the NOMINAL drive `DRIVE_NOM = 0.3` (×2.35), for all types. This is the knob default
        (every `defaultPatch()` filter starts there) and the drive C1, C2 and C3 already measure at. Full drive is
        reported in the ADAA table. **COST** is reported beside it by `fidCost`: the clock is injected (so the
        DSP section reads none), the result is the median of 7 × 65 536 samples after a warm-up, taken relative to
        SVF 12 LP.
      - **C6:** ≤ 6 dB swing at both the FIXED geometry (1 kHz over 110 Hz) and the KEYTRACKED one (cutoff = f0).
        Each cell also reports the **law room**, range(L_fixed − L_key). The reasoning: a scalar resonance law
        adds the same c(res) to both geometries, so a law passing both exists iff range ≤ 12 dB. Proof:
        c = −(L_f + L_k)/2 leaves each at range/2. The cell labels a failing row **NO LAW** (> 12, proof of
        impossibility) or **FITTED ONLY** (≤ 12, where only a law fitted between the two geometries would pass,
        and was declined).
    - **Anti-aliasing (C5):** `AA_NL`, `aaD1`/`aaD2`, `aaR`/`aaR2` (series near 0; Li2 by its Bernoulli series),
      `HalfBand` / `Os` (55-tap Kaiser β = 10 polyphase half-band, 14 multiplies each way), and
      `osResp`/`osLat`, which carry the latency and response into H and into C2's claim (`lat`).
      - `aaType(T, opt)` builds any option.
      - The registry records the chosen one: `TYPES.ladder.aa = 'os2'`, `TYPES.ms20.aa = 'adaa1c'`.
      - `aaEval` re-runs the table.
      - `landmark()` removes a claim's `lat` before reading the phase.
    - **PHASER C4:** `Ap4` is now a normalised lattice (y = c·s − a·x, s ← c·x + a·s). It is the same H, and C1
      reads 0.0000 dB. A reflection cannot create energy when the coefficient jumps.
    - **B291:** `FilterRig.control()` now rebuilds the rack instance BEFORE the PER SOURCE early return.
    - **Page changes:**
      - The fidelity table has two report columns (C4b, COST).
      - A new **Manifest** card: type × {anti-aliasing, C5 dBc, cost, max viable modulation rate}.
      - A new **ADAA first** card: every option, measured live after the table.
      - Types-card and inspector notes explain the anti-aliasing choice.
      - C6 cells carry their refusal (the law room).
      - The comb/bank held notes and Q6 are rewritten.
      - `<meta name="lab-review" content="B274 + B287 + B290 · 2026-09-26">`.
  - `tools/labharness/filter_fidelity_check.mjs`:
    - Pins updated. **F → P** (fixed): ladder C5, ms20 C5, phaser C4. **P → F** (the re-cut gate, not a
      regression): C6 of svf12 LP/BP/HP/PEAK, svf24 LP/BP/HP/PEAK, ladder, ms20, phaser, bankP.
    - A plant is caught when its cell LEAVES its pin (a detector plant must flip a finding).
    - Eight new plants, plus two proofs that each carry their own plant:
      - the ADAA conditioning proof (silence, held DC, the C5 sine; every ADAA option × both types × drive
        0.3 and 1);
      - the B291 rig row (3 placements × 12 rack types).
    - `--report` prints the ADAA-first table and asserts that the recorded options pass in it. The manifest
      prints every run.
    - Runtime is about 6 s (it was about 2 s). `verify`'s comment still says "~1.5 s". `verify` is a protected
      path, so the comment was not touched.
- **ADAA evaluation** (C5 at nominal drive 0.3 gates; drive 1 is reported). Cost is × SVF 12 LP, node, from one
  `--report` run; the page's browser timings differ in scale but not in order.

  | LADDER | C5 @0.3 | C5 @1 | C1 | C2 | C3 | cost | all |
  |---|---|---|---|---|---|---|---|
  | none | −35.9 | −13.3 | P | P | P 0.4 ¢ | 6.1 | — |
  | (a) ADAA1 on whole tanh | −45.2 | −33.1 | F 115 dB | F 912 ¢ | F 308 ¢ | 7.3 | — |
  | (a) ADAA1 on residual | −56.2 | −43.0 | P | P | F 51.4 ¢ | 8.7 | — |
  | (a) ADAA1 residual, centred | −40.6 | −26.1 | P | P | F 6.2 ¢ | 8.7 | — |
  | (b) ADAA2 residual | −88.9 | −68.2 | P | P | F 88.3 ¢ | 21.5 | — |
  | (b) ADAA2 residual, centred | −41.7 | −27.8 | P | P | F 11.4 ¢ | 21.5 | — |
  | **(c) 2×, chosen** | **−64.7** | −21.3 | P | P | P 0.4 ¢ | **15.7** | PASS |
  | (d) 2× + ADAA1c | −77.3 | −41.0 | P | P | P 1.2 ¢ | 21.8 | PASS |
  | (e) 4× | −127.9 | −33.7 | P | P | P 0.4 ¢ | 33.1 | PASS |
  | (e) 8× | −129.0 | −58.2 | P | P | P 0.4 ¢ | 66.6 | PASS |
  | 4× + ADAA1c | −128.7 | −62.3 | P | P | P 0.5 ¢ | 45.7 | PASS |

  | MS-20 | C5 @0.3 | C5 @1 | C1 | C2 | C3 | cost | all |
  |---|---|---|---|---|---|---|---|
  | none | −55.3 | −36.0 | P | P | P 1.0 ¢ | 4.3 | — |
  | (a) ADAA1 on whole tanh | −64.2 | −49.4 | F 7.8 dB | F 361 ¢ | F 327 ¢ | 6.2 | — |
  | (a) ADAA1 on residual | −66.0 | −47.3 | P | P | F 40.5 ¢ | 5.0 | — |
  | **(a) ADAA1 residual, centred, chosen** | **−61.3** | −44.8 | P | P | P 3.4 ¢ | **5.6** | PASS |
  | (b) ADAA2 residual | −86.0 | −69.0 | P | P | F 134.3 ¢ | 9.5 | — |
  | (b) ADAA2 residual, centred | −64.4 | −48.7 | P | P | F 6.1 ¢ | 9.7 | — |
  | (c) 2× | −109.0 | −48.6 | P | P | P 1.0 ¢ | 11.6 | PASS |
  | (d) 2× + ADAA1c | −115.7 | −63.4 | P | P | P 1.4 ¢ | 14.4 | PASS |
  | (e) 4× | −161.1 | −77.3 | P | P | P 1.0 ¢ | 23.6 | PASS |
  | (e) 8× | −154.4 | −133.0 | P | P | P 1.0 ¢ | 49.4 | PASS |
  | 4× + ADAA1c | −161.5 | −96.0 | P | P | P 1.1 ¢ | 30.0 | PASS |

  C7 passes on every row, with the DC gain exact and mean ≤ 2e-14.

  - **The hazards, tested:**
    - **ADAA on the whole tanh** delays the linear part too, so the ZDF solve's prediction no longer matches the
      loop. C1 and C2 collapse.
    - **ADAA on the residual** tanh(a) − a keeps the linear filter exact to the last digit. But its half-sample
      delay (one sample for ADAA2) detunes self-oscillation, which is where the residual is large.
    - **Analytic compensation:** the window is centred on the current sample, [½(a+a₁), a+½(a−a₁)], so its
      small-Δ limit is r(a). C3 comes back, but the extrapolation lifts the top octave and gives back most of the
      suppression.
    - **A second compensation, tried in scratch:** full ADAA with a consistent ZDF solve, and the resonance
      re-placed analytically (per-stage phase π/4 − ω/8, k/(4m)). It landed the cutoff at 0.00 ¢ and C3 at
      3.2 ¢, but only reached −48.6 dBc, so it was not carried into the lab (`add_ladderF.py`, `exp3.mjs`).
    - **Extrapolating the output instead** (`res1x`) reached −49.1 dBc with 19.6 ¢ on the ladder, and
      −60.6 dBc with 11.6 ¢ on the MS-20.
    - **The fallback:** below |Δ| = 1e-5 (1e-3 for second order) the midpoint value is used. The conditioning
      proof shows no NaN and exact silence from silence. Its plant (fallback removed) gives 51 failures, the
      first `ladder adaa1t drive 0.3: silence → NaN`. C7 shows no DC on every option.
  - **What ADAA buys:**
    - **Ladder:** ADAA alone cannot pass at 1×. With 2×, it adds 12.6 dB of margin. At full drive, 4× + ADAA
      (−62.3) passes where 8× alone (−58.2) does not: it halves the oversampling needed.
    - **MS-20:** ADAA alone passes at 1×, at about half the cost of 2×. At full drive, 2× + ADAA (−63.4) passes
      where 2× alone (−48.6) does not; that also halves 4×.
- **C5 choice, flagged (L0024):** MS-20 `adaa1c` passes by 1.3 dB. I re-derived it with a scratch sweep
  (`sweep5.mjs`, bin-exact inputs at 5 and 7 kHz × res 0.2/0.5/0.8 × cutoff 3k/12k, nominal drive).
  - The improvement is real but small: it is better than plain in every valid cell, by 5–7 dB.
  - At res 0.8 it still aliases: −44 dBc at 7 kHz / 12 kHz cutoff.
  - 2× also fails one cell (−54 dBc at 5 kHz, res 0.8).
  - The ladder at 2× passes every 5/7 kHz cell (−62 to −91 dBc).
  - Bins 512 and 1536 were excluded from the sweep: as multiples of N/16, their aliases land on harmonic bins, so
    they are blind.
  - C5's single stimulus is therefore the gate as ruled, and not a guarantee elsewhere.
- **Per-type results** (48 kHz, harness at 76ad4ab):
  - **C3, self-oscillation, relative to the source:**
    - ladder: 0.4 ¢, +3.0…+3.0 dB, onset 0.952;
    - MS-20: 3.4 ¢, −3.9…−1.9 dB, onset 0.952.
  - **C4 gate (on the tick):** every row passes. The worst is bankP at +5.0 dB; the phaser reads −0.9 and
    ALL-PASS ×4 −0.3.
  - **C4 re-judged:**
    - PHASER in its TPT form: +14.1 dB at the tick (FAIL), and C4b < 172 Hz.
    - ALL-PASS ×4 in its TPT form: +7.1 dB (a PASS under C4a, so it needed no fix), and C4b 2.8 kHz.
    - The lattice fixes both.
  - **C4b max viable modulation rate:** ≥ 22.1 kHz for every type; the two SWARM rows are tick-limited.
  - **COST, × SVF 12 LP** (node, manifest run; SVF 12 LP is 8.2 ns/sample):
    - SVF 0.8–1.4;
    - LADDER 19.6 (os2);
    - MS-20 7.8 (adaa1c);
    - COMB 1.6–1.8;
    - FORMANT 2.1;
    - ALL-PASS 1.8;
    - PHASER 1.9;
    - DJ 1.7;
    - SWARM 5.8–5.9.

    Repeat runs move these by about ±20 % (the `--report` run read the ladder's os2 at 15.7).
  - **Fidelity totals:** 124 PASS, 16 FAIL (all pinned), 140/140 controls fail, 23/23 plants caught.
- **Refusals (C6, two geometries), with the law room:**
  - **NO LAW** (range > 12 dB, so no resonance law can pass both):

    | Row | law room | fixed swing | key swing |
    |---|---|---|---|
    | svf12 LP | 17.1 dB | 2.7 | 19.9 |
    | svf12 PEAK | 13.1 dB | 4.5 | 8.6 |
    | svf24 LP | 18.1 dB | 1.9 | 20.0 |
    | LADDER | 19.8 dB | 4.2 | 22.5 |
    | MS-20 | 13.6 dB | 2.6 | 16.2 |
    | COMB + | 15.0 dB | 15.0 | 0.0 |
    | SWARM · PROPOSED | 13.7 dB | 1.0 | 14.7 |

  - **FITTED ONLY** (range ≤ 12 dB; only a law fitted between the geometries would pass, declined per the
    brief):

    | Row | law room | fixed swing | key swing |
    |---|---|---|---|
    | svf12 BP | 9.0 | 0.3 | 9.4 |
    | svf12 HP | 6.1 | 0.5 | 6.6 |
    | svf24 BP | 7.7 | 2.2 | 9.9 |
    | svf24 HP | 7.4 | 0.9 | 6.5 |
    | svf24 PEAK | 11.9 | 2.5 | 9.5 |
    | PHASER | 10.6 | 1.4 | 9.2 |
    | COMB − | 5.9 | 18.1 | 24.1 |

    - COMB −'s fitted midpoint law would take its tuned case (cutoff 2·f0, 1.2 dB) to 19.9 dB.
    - COMB +'s midpoint law would cost its keytracked case 7.5 dB (`combfit.mjs`).
  - **Phase A's laws, re-ruled:**
    - LADDER (1 + k/2) is kept: it passes the fixed geometry, and no law passes both.
    - PROPOSED P1 is kept on the same grounds.
    - COMB ± keeps the peak law: it keeps keytracked COMB + flat (0.0 dB).
  - SWARM · REF C2 and C6 stay PINNED as properties of the reference. It fails C6 at both geometries: 21.4 and
    23.2.
- **Other verdicts that moved:** only the pins listed above. The phase-A plants for the SVF 12, SVF 24 and
  ladder laws now target C1, not C6, because those rows fail C6 at the keytracked geometry, so a removed law
  cannot flip C6. This is a coverage boundary recorded per L0033. The formant plant still targets C6.
- **Must-fail proof (wired gate):** a scratch copy of the lab at 76ad4ab was given two plants: the MS-20's `aa`
  set to `'none'`, and C3's source reference shifted +10 dB. The gate went RED, exit 1, with 8 errors, among
  them `ms20 C5: now FAILS (pinned P) — -55.3 dBc`, `ladder C3: control PASSED (... +12 dB MISSED) — the check
  is blind`, and the anchor errors of the plants that the copy invalidated. Summary line: `RED —
  filter_fidelity_check: 20 types × 7 checks, 121 pass, 19 FAIL (16 of them pinned findings), 140/140 ... `.
  The exact line is `138/140 controls fail as they must, 20/23 planted faults caught, 8 error(s)`. Every
  built-in plant was caught on the real file (23/23).
- **Screenshots:** in the session scratchpad `b287b/shots/`, not committed.
  - `light-fidCard.png` and `dark-fidCard.png`: the fidelity table.
  - `light-manCard.png` and `dark-manCard.png`: the manifest.
  - `light-aaCard.png` and `dark-aaCard.png`: the ADAA table.
  - `light-typesCard.png`.
  - `c5-spectra-pg.png`: LADDER and MS-20 at the nominal drive, before and after.
  - `c6-geometries-pg.png`: level vs res at both geometries, 20 rows, on a 0.05 grid.
  - All were captured by CDP from `tools/serve_labs.py 8288` once every row and option had been measured.
- **Evidence consulted:**
  - ROADMAP B199, B274, B287, B288, B290 and B291 (`origin/lead-records-100`).
  - Both prior traces.
  - `src/swarm_core.h:136`.
  - `tools/golden/extract_core.mjs` and `tools/labharness/lab_load_check.mjs`.
  - INDEX L0016, L0024, L0032, L0033 and L0052.
  - Scratch scripts: `exp1–3.mjs`, `sweep5.mjs`, `c6geo.mjs`, `c4proto.mjs`, `lattice.mjs`, `hbdesign.mjs`,
    `aaeval.mjs`, `combfit.mjs`, `rowvals.txt`.
- **Alternatives rejected:**
  - **ADAA on the whole tanh:** it breaks C1 and C2.
  - **Consistent full ADAA** with analytic re-placement: −48.6 dBc.
  - **Output extrapolation:** it detunes.
  - **A fitted C6 exponent** (for example k^¾ for BP, the midpoint of the peak and energy laws): the brief
    forbids fitting to the test.
  - **A shorter FIR:** 31 taps loses 1.4–1.9 dB at 20 kHz.
  - **A 79-tap FIR:** 39 samples of latency for 0.15 dB.
  - **Picking the option by timing at run time:** it would be machine-dependent. The choice is recorded instead.
  - **Keeping the ADAA table in `fast`:** about 8 s. It sits behind `--report`, and the page runs it live.
- **Verify:** `./verify fast` exit 0 at git 76ad4ab (`.harness/last-verify.json`). The line was: `GREEN —
  filter_fidelity_check: 20 types × 7 checks, 124 pass, 16 FAIL (16 of them pinned findings), 140/140 controls
  fail as they must, 23/23 planted faults caught, 0 error(s)`.
- **Open questions:**
  1. **MS-20 C5 is marginal:** 1.3 dB inside the bound, and −44 dBc at res 0.8. Keep ADAA1c, or take 2×
     (−109 dBc at about twice the cost)?
  2. **C5 samples one stimulus.** Every 1× or 2× option aliases somewhere at res 0.8. Should C5 get more cells?
  3. **C6 at two geometries now fails 12 non-reference rows,** 7 of them provably with no law, SVF LP among
     them: a resonant peak on the fundamental IS louder. Is "both geometries" the bar, or should each type be
     judged at the geometry it is played at (the comb keytracked, the SVF fixed)?
  4. **The LADDER's 27-sample latency** combs against a non-oversampled branch in PARALLEL. Accept it, or look
     for a minimum-phase half-band?
  5. **C4b reads ≥ 22 kHz for every type,** so the manifest value carries no discrimination at this stimulus
     (res 1, ±4 oct). Is a harsher C4b wanted?
  6. **Citations** (Parker/Zavalishin/Le Bivic 2016; Bilbao/Esqueda/Parker/Välimäki 2017; Holters 2019;
     Välimäki & Huovilainen 2006) are from memory and unverified.
  7. **`verify`'s "~1.5 s" comment** is stale (now about 6 s). `verify` is a human gate, so it was left unedited.
