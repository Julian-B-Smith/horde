# b274-filter-lab-round2 — twelve filter types behind one interface, a seven-check fidelity programme, one signal-path picture

- **Queue item:** B274 (row carried in PR #767, branch `lead-records-98`; human 2026-09-26: "I want quite a
  few more filter options. We'll need to do extensive fidelity testing. Let's represent the signal path more
  coherently; only one representation of each filter, oscillators plugging directly in, a distinct
  representation of serial versus parallel filters"). Extends B209 (round 1: `traces/2026-09-22-b209-filter-lab.md`).
- **Why:** The human asked for more filter options, and for fidelity stated as checks rather than taste. Round 1
  drew filter A once per source in its flow chart, then again as a card, and drew the routing twice (flow +
  matrix well). So `docs/design/filter-lab.html` is extended in place (the index link stays valid):
  - **One interface** (`TYPES` in the DSP section). Each type provides make/set/tick, `H(f)` (its analytic
    small-signal response) and `claim(P)` (where its knobs say the landmark is).
  - **A programme of seven checks** that runs every type through that interface, with a must-fail control
    per check.
  - **One canvas** where each filter is a box holding its own curve and sources cable straight into filter
    inputs. SERIAL is drawn as one line through A → B; PARALLEL as a ◆ split into two branches that meet at ½Σ.
- **What changed:**
  - `docs/design/filter-lab.html`:
    - Twelve types (20 fidelity rows counting modes): SVF 12 and SVF 24 × LP/BP/HP/NOTCH/PEAK, LADDER, MS-20,
      COMB +, COMB −, FORMANT, ALL-PASS ×4, PHASER 4, DJ, SWARM · REF and SWARM · PROPOSED (rack only). The
      reasons are in the page's types card and in `TYPES[*].why`.
    - The SVF stays a literal transcription of `src/svf_core.h`. The lab adds taps 4 (NOTCH) and 5 (PEAK) and a
      `setK()`.
    - The swarm bank is still CALLED from `reference/swarmfilter.html`, now wrapped as `SwarmBank` in the one
      interface. P1–P3 are unchanged.
    - Routing reduced to one cable per source: port A / B / A+B (parallel only) / MIX / DRY, plus a global
      topology (SERIAL | PARALLEL). The placement pills (PER SOURCE / RACK / BOTH) are kept.
    - Removed round 1's duplicate representations: per-source filter boxes, separate filter cards, and the
      FILT/DRY matrix well. Removed the round-1 feed/dry gains; DRY is now a cable.
    - `<meta name="lab-review" content="B209 + B274 · 2026-09-26">`.
  - `tools/labharness/filter_fidelity_check.mjs` (new): slices the lab's DSP section by the `design` banners and
    runs the same programme with new Function (not vm, L0052), about 1.5 s. It is red on:
    - a blind control;
    - a verdict that differs from its pin;
    - a missing/extra row, or a check count other than 7;
    - an uncaught planted fault (5 plants, plus a bogus-anchor test of the planter).
  - `verify`: the harness is wired into `fast` beside fxlab_check (ADR-180 §1: adding a check). Its header
    reads `WIRED: ./verify fast (beside fxlab_check).`, and test_table_check verified it.
- **Round 1's self-checks:** round 1 had no in-page self-check beyond `lab_load_check` (still GREEN for this
  file). Its trace measurement "the drawn curves are the audio (0.000 dB)" is **superseded by C1**, which now
  asserts it for every type with a control. The P1–P3 diagnostics (`runDiag`) are kept unchanged.
- **The programme (tolerances as stated in `FID` / `FID_CHECKS`):**
  - **C1 analytic:** |Δ| ≤ 0.05 dB above −60 dB and ≤ 0.5° above −40 dB. Control: the same curve read 1 % sharp.
  - **C2 cutoff·res:** within 5 ¢ and 0.25 dB at the landmark, 55 Hz–12 kHz × res 0.2/0.8. Control: cutoff
    planted 10 ¢ sharp.
  - **C3 self-osc:** pitch ≤ 5 ¢, level ≥ −30 dBFS with ≤ 3 dB spread, onset within 0.01 of the law. Types
    that don't self-oscillate must decay. Controls: res below onset must stop; a 20 ¢ probe must be caught; a
    steady sine must read as sustaining.
  - **C4 fast mod:** random jumps every tick, then ±4 oct at 1 kHz per sample, res 1. Pass: finite, and
    ≤ +12 dB over the worst static setting, which must itself be ≤ +40 dB over the input. Control: a DF2T
    biquad.
  - **C5 aliasing:** a bin-exact 7 kHz sine, drive 1, cutoff 12 kHz; energy on the folded-harmonic bins only
    must be ≤ −60 dBc. Controls: a pure sine must read ≤ −150 dBc (the must-read-zero); a hard clip must read
    above −60 dBc.
  - **C6 level @ res:** swing ≤ 6 dB for a 110 Hz saw, res 0 → 0.9. Control: a planted res-dependent gain.
  - **C7 DC:** DC gain matches H(0) within 0.05 dB (or both ≤ −80 dB), and |mean| ≤ 1e-4. Control: a planted
    2 % leak plus a 1e-3 offset.
- **Measured** (48 kHz; the harness output at 9f7b6f0): 20 rows × 7 = 140 cells. 123 PASS, 17 FAIL (all
  pinned as findings), and 140/140 controls fail as they must. Findings:
  - C6 (level at resonance):
    - SVF 12 BP 10.3 dB, SVF 12 HP 10.5 dB (the B81 core's own taps get louder).
    - SVF 24 BP 12.2, HP 9.1, PEAK 7.5 dB.
    - LADDER 8.6 dB (passband sag).
    - COMB + 15.0, COMB − 18.1, FORMANT 8.5 dB (they get quieter).
    - SWARM · REF 21.4 dB. SWARM · PROPOSED passes at 1.0 dB (P1 works).
  - C5 (aliasing): LADDER −13.3 dBc, MS-20 −36.0 dBc. Neither oversamples.
  - C3 (self-osc): LADDER is 6.7 ¢ flat at 110 Hz, and its level is −31.5…−29.8 dBFS against a −30 floor.
    Its onset is 0.952, which is right. MS-20 passes: 1.0 ¢, −20.7…−18.4 dBFS, onset 0.952.
  - C2 (cutoff·res):
    - COMB + peak sags 0.74 dB at 3.52 kHz, res 0.8 (linear-interpolated delay).
    - SWARM REF and PROPOSED: the bands cannot follow the centre to 55 Hz (lands at 99.5 Hz, +1027 ¢) or to
      12 kHz (9.59 kHz, −388 ¢), because of the reference's 40 Hz / 11 kHz clamps. 880 Hz and 3.52 kHz are
      exact.
  - C4 (fast mod): PHASER 4 reaches +20.5 dB over static at full feedback. It is bounded, not growing: the
    per-second peak holds at about 3.1 over 4 s (scratch probe). At 100 Hz modulation it is about 1.8.
  - **Marginal pass, flagged (L0024):** ALL-PASS ×4 C4 is +11.8 dB against a 12 dB tolerance. Its verdict
    flipped (12.3 → 11.8) when, during development, the static bound was redefined to span the swept range
    instead of fixed cutoffs. SVF 12 PEAK C6 is 5.5 dB against 6 dB.
- **Detector corrections made during development:** each was found by a control or a plant, before
  committing, and none loosened a tolerance.
  - C1's control first used "the same curve designed at fs × 1.01". That is blind for HP/NOTCH/PEAK, because
    scaling fs scales f and fc together. It became "read at f × 1.01".
  - C3's sustain test `late ≥ early` called a saturated MS-20 decaying, which put its onset at 1.000. It is
    now `late ≥ early − 0.5 dB`, and the onset reads 0.952.
  - The AP4 phase bracket is now taken in the warped domain. Linear [fc/1.5, 1.5 fc] hit the wrap at 12 kHz.
  - The design-landmark search (formant, phaser) now shares its bracket with the measured search. Different
    brackets found different local minima on the phaser.
  - C4 got the static ceiling. The ladder-without-tanh plant passed without it, because an unstable static
    bound inflates itself.
- **Must-fail proof of the wired gate:** these are in addition to the 5 built-in plants that every run catches.
  - A scratch copy of the committed lab, with the SVF damping planted (`Math.SQRT2*(1-r)` → `1.3*(1-r)` in
    `Svf.set`), went RED (exit 1) with 11 errors: svf12 LP/BP/HP/NOTCH/PEAK and DJ C1 (0.72 dB · 2.4°), and
    their C2 levels (0.73 dB).
  - Summary line: `RED — filter_fidelity_check: 20 types × 7 checks, 112 pass, 28 FAIL (17 of them pinned
    findings), 140/140 controls fail as they must, 5/5 planted faults caught, 11 error(s)`.
- **Evidence consulted:**
  - ROADMAP B274, B209, B199 (on `origin/lead-records-98`), and the round-1 trace.
  - `src/svf_core.h`, via round 1's transcription.
  - `reference/swarmfilter.html` DSP section (read and called, not modified).
  - `tools/golden/extract_core.mjs` (BANNERS), `tools/labharness/fxlab_check.mjs`, `station_check.mjs`,
    `lab_load_check.mjs`, `tools/test_table_check.py`, and `verify`'s labharness block.
  - `docs/design/station-page-lab.html` (the model), and the gui2 tokens as round 1 copied them.
  - INDEX L0016, L0024, L0032, L0052.
- **Alternatives rejected:**
  - Compensating each new type so that C6 passes. Rejected: the types are canonical, and their level
    behaviour is a finding for the human to rule on.
  - Oversampling the ladder and MS-20. Rejected: that would hide the C5 finding the programme exists to
    report.
  - Running the harness in a vm sandbox (rejected per L0052), or in `full`. It runs in about 1.5 s, so `fast`
    it is.
  - Per-source serial/parallel (round 1's model): it cannot be drawn as "one line through two boxes", so
    SERIAL became global (Q5 asks the human).
  - Keeping the matrix well: it was a second drawing of the routing.
- **Screenshots:** headless Chrome at 1×, served from this worktree by `tools/serve_labs.py 8274`. In the
  session scratchpad `b274/shots/` (not committed):
  - `path-light-par.png`, `path-light-ser.png`, `path-dark-par.png`, `path-dark-ser.png`;
  - `fidelity-light.png`, `fidelity-dark.png`;
  - full pages `light-par.png`, `light-ser.png`, `dark-par.png`, `dark-ser.png`.
- **Verify:** `./verify fast` exit 0 at git 9f7b6f0 (`.harness/last-verify.json`). The line was: `GREEN —
  filter_fidelity_check: 20 types × 7 checks, 123 pass, 17 FAIL (17 of them pinned findings), 140/140
  controls fail as they must, 5/5 planted faults caught, 0 error(s)`.
- **Open questions:**
  - **Q5:** is SERIAL a global line (the lab's drawing, Serum's shape) or per source (round 1)?
  - **Q6:** each finding needs a ruling: fix, compensate, or keep as character. In particular, the B81 core's
    BP/HP get louder with resonance; the ladder and MS-20 alias; the bank can't reach 55 Hz or 12 kHz.
  - **The two marginal passes:** AP4 C4 and SVF 12 PEAK C6.
  - **The C3 level floor** (−30 dBFS) and **the C5 target** (−60 dBc) are stated targets, not re-measurements
    of a reference. The human may set them.
  - **C4 is harsher than the plugin:** it modulates per sample, while the plugin's mod tick is about 172 Hz
    (B199). Whether the plugin's rate should be the bar is open.
  - **Q3's caveat** is now concrete: LADDER and MS-20 have a tanh in the loop, so per-source state is no longer
    equal to filtering the sum when drive is up.
