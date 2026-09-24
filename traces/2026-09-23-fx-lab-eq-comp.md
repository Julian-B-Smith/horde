# fx-lab-eq-comp — FX design lab round 2: a parametric EQ and a per-instance compressor, running real DSP

- **Queue item:** B234 (the row is carried in PR #736, branch `lead-records-84`; acceptance was read from
  that branch's ROADMAP.md, and B230 was read beside it).
- **Why:** The human, 2026-09-23: "Let's also add a more robust EQ and compressor to the FX lab." Two
  cards join B210's seven in `docs/design/fx-design-lab.html`, each with the four-knob FACE, the choice
  of four stated, and the EXPANDED view:
  - **EQ** (kind NEW; the rack has no EQ). Six bands, each one of low cut, low shelf, peak, high shelf
    or high cut, with frequency, gain and Q. Cuts come in 12, 24 and 48 dB/oct as a Butterworth
    cascade whose sections all scale with Q. There is a Depth control (gain scale, 0–200 %).
    - The curve is draggable: a node sets frequency and gain, the wheel sets Q, a double-click turns
      the band on or off.
    - The drawn curve is the digital `|H(e^jw)|` of the same section list the running `EqDSP`
      processes, drawn over a live 4096-point analyser of the lab source after the EQ (dashed = before).
    - Face: Depth · Tilt (two internals: low shelf down, high shelf up) · Sweep (the band-4 bell) ·
      Regen unbound.
  - **COMP** (kind RACK, replacing the rack COMP). Threshold, ratio, knee, attack and release as 1/e
    time constants in ms, makeup, and a sidechain HPF (12 dB/oct).
    - The law is the static curve and smooth decoupled peak detector from Giannoulis/Massberg/Reiss,
      JAES 2012.
    - Pictures: the transfer curve with the live operating point and its trail, IN/GR/OUT meters
      (the alarm colour for clip), the sidechain response, and a 4 s level and GR history.
    - The envelope is a per-instance member, by construction (the contrast with B230).
    - Face: Squash (threshold and ratio together) · SC HPF · Release · Regen unbound.
  - Both cards run the DSP in the page as an offline render: one fixed hop of 735 samples per
    animation frame, and no clock is read.
- **Evidence consulted:** `traces/2026-09-22-fx-design-lab.md` (B210's structure, audit and
  must-fire practice); the lab itself (module records, `buildModule`, `audit`, setup); `src/fx_rack.h`
  :686-712 (today's COMP law: ratio 1+4a over a fixed 0.4 hard knee, 0.06/15 ms, 0.98 brickwall) and
  :886 (`compEnv`, one per rack — B230); `specs/SPEC-MODULE-MACROS.md` :37 and :139 (the role enum and
  what each role is for); `src/gui/gui2.html` :20-100, :139-149, :180-212 (tokens, screen schemes);
  `docs/design/feedback-lab.html` and `formant-lab.html` (the worklet idiom, looked at and not
  used); `tools/labharness/lab_load_check.mjs` (sandbox globals); `tools/test_table_check.py` §5 (the
  WIRED grammar); `verify` :150-200.
- **Self-checks (in the page at load, and under `./verify fast` via the new
  `tools/labharness/fxlab_check.mjs`):**
  - **EQ.** Sines go through a fresh `EqDSP` at 28 log-spaced points on the defaults and on a stress
    set (every type, 12/24/48 dB, Q 8, depth 150 %). Points whose curve is below −60 dB are skipped.
    - Measured against the drawn curve: max Δ **0.0003 dB** over 54 points (tolerance 0.01).
    - **Must-fail control**, the same curve designed at 48 kHz: Δ **3.28 dB**. It fails, as it must.
  - **COMP.** Steady-state GR for a sine at −30, −18 (the knee centre) and −6 dBFS at 1 kHz, and at
    100 Hz through a 200 Hz sidechain HPF. Measured both at the output and on the meter.
    - Max Δ **0.006 dB** against the transfer curve (tolerance 0.05).
    - **Control 1**, a hard-knee curve: Δ **0.56 dB**. It fails.
    - **Control 2 (B230)**: a second instance fed silence reads **0.000 dB** GR. With `b.env = a.env`
      planted, it reads **8.98 dB**. That fails too.
  - **Gate must-fire.** Three faults were planted in scratch copies and each turned `fxlab_check` RED
    (exit 1):
    - DSP depth 2 % off the curve: Δ 0.341 dB.
    - Hard knee in the DSP only: Δ 0.563 dB, and control 1 correctly reports itself blind.
    - A class-level shared envelope: the silent instance reads 8.98 dB.
  - **Interaction probe** (headless Chrome over CDP, served from this worktree on :8234):
    - Dragging EQ node 4 moved band 4 from 1800 Hz / +2.5 dB to 2899 Hz / +13.0 dB. The rows read
      "2.90kHz" and "+13.0dB", and the curve at the node reads 13.01 dB.
    - On a SINE 1k source at −6 dBFS the comp's operating point sat at detector −6.00 dB, with the GR
      meter at 8.990 dB against the curve's 9.000.
- **Alternatives rejected:**
  1. An AudioWorklet LISTEN path. The brief said to use whatever the lab already uses, and the lab had
     no audio. A main-thread render is also what the load-time check and the headless checker can run.
     Left as an open question.
  2. Identical sections for 24/48 dB cuts. That puts the corner at −6/−12 dB; the Butterworth Qs keep
     it at −3 dB when Q is 0.707.
  3. A peak-then-attack envelope on the level, the rack's own shape. On a sine it settles well below
     the peak, depending on the attack time (estimated at more than 2 dB for 10 ms at 1 kHz), so the
     steady state would not be the static curve. The decoupled detector on GR is exact.
  4. An RMS detector. It would move the transfer curve's x-axis off the "sine peak level" the check and
     the meters use.
  5. Dashing all 37 EQ rows as NEW. The card's kind says NEW once instead.
  6. Binding Makeup or Mix to Regen. Neither is a loop; the slot stays inert, as Drive's does.
- **Files:**
  - `docs/design/fx-design-lab.html`: two modules, DSP, painters, drag wiring, a lab source bar,
    self-checks, audit lines, three findings, `lab-review` meta set to `B210 + B234 · 2026-09-23`.
  - `tools/labharness/fxlab_check.mjs` (new; `WIRED: ./verify fast`).
  - `verify`: one invocation, added beside `lab_load_check`; nothing removed or relaxed.
  - `docs/design/index.html` is untouched, per the brief.
- **Verify:**
  - `./verify fast` exit 0 on the committed hash; `.harness/last-verify.json` reads
    `{"target":"fast","exit":0,"git":"3e29385","ts":"2026-09-24T03:36:59Z"}`.
  - The private-name leak gate SKIPPED in this worktree (`.leakcheck-names` is absent here). Its
    pattern was run by hand from the main checkout's list, case-sensitive as the gate is, over the
    added lines and the new file: 0 hits.
  - `lab_load_check` over the lab: OK.
- **Screenshots:** headless Chrome, dpr 1, 1600 px wide, per-card clips. EQ expanded, COMP expanded and
  all faces, each in light and dark, plus COMP on a sine (light). All are in the session scratchpad and
  named in the PR.
- **Open questions:**
  - (a) Should the lab get a LISTEN switch (the same classes in a worklet)? The cards compute but do
    not sound.
  - (b) The role enum leaves Regen empty on three of nine cards (Drive, EQ, COMP), and the EQ's Sweep
    on Motion stretches SPEC-MODULE-MACROS:139. Is the empty slot right, or does the enum want an
    output-shaped role? That is the human's call.
  - (c) The EQ has no output trim and the COMP no dry/wet or lookahead. They were left out as not in
    the brief.
  - (d) The COMP card drops today's 0.98 brickwall. Where the limiter lives is unruled.
  - (e) `fxlab_check`'s planted shared-envelope pair is a ready-made must-fail control for B230's own
    C++ check.
  - (f) Knob and drag feel was verified headless only and wants a human hand.
