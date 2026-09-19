# b150-smoother-seconds — the coupling smoother in seconds, 44.1 kHz bit-frozen

- **Queue item:** B150 (human ruling 2026-09-18, option a: express the
  rate-bound constants in seconds with 44.1 kHz special-cased bit-frozen, so
  every other rate is corrected to match 44.1 k; re-baseline at non-44.1 rates
  only; the 44.1 k goldens must not move — that is the gate).
- **Why:** `docs/audits/2026-09-18-saw-engine-audit.md` §1.1 (A1). The
  coupling smoother's `0.08` was a hand-tuned PER-TICK coefficient ported
  verbatim from `reference/swarmsaw.html:486-487` — the class ADR-009 bans. Its
  time constant was 4.35 ms at 44.1 kHz and 1.99 ms at 96 kHz, so a K-knob step
  settled 2.2x faster at 96 k. `samplerate_check`'s own tolerance is 0.3 %; this
  sat at 54.9 %.

## Evidence consulted

- `docs/audits/2026-09-18-saw-engine-audit.md` — headline A1/A6/A7, §1.1, §3.2,
  §3.5, §3.6.
- `tools/sr_check.cpp` (B147 layer 2, PR #635) — its probes, its controls, and
  its own recorded disagreement with the audit on the inertia row.
- `src/swarm_core.h` — the three `Ksm*` smoother lines; the `cullEnv` branch
  and `kGravGridSeconds` / `gravGridSamples()` as the 44.1 k-exact idiom; the
  `s.pressSm` line as the seconds-per-tick idiom already in the file; the
  inertia spring (`w0`/`S`/`D`, explicit Euler on `dt = kTick/sr`); the output
  pole (`s.lpc = 1 - exp(-kTau*fc/sr)`).
- `src/hypersaw_clap.cpp:4329,4335` — the ADR-059 `inertiaCurve` taper, which
  is what answered the dispatch's inertia-curve question.
- `traces/2026-09-18-b148-ncap-bitident.md` — the 164-item corpus, rebuilt here.
- `tools/parity_check.cpp` — the replay protocol corpus A copies.

## What changed

Four commits, one per quantity, each with its own before/after numbers.

### 1. `src/swarm_core.h` — the coupling smoother (commit `025deee`)

`kKsmTauSeconds = 0.004351220802760264` (= `-(16/44100)/ln(0.92)`, 4.3512 ms),
converted to a per-tick coefficient at the running rate and resolved ONCE in
the constructor (`sr` is fixed for the object's lifetime; `controlTick` is
23-34 % of all CPU, so an `exp` per voice per tick would have been pure loss).
The three use sites (`KsmS`, `KsmP`, `KsmD`) read the member.

**44.1 kHz is special-cased to the literal `0.08`, and the branch is
load-bearing:** the seconds round trip is `0.07999999999999996`, three ULP
short. That is the `cullEnv` idiom, for the same reason.

Per-rate coefficient (was `0.08` at every rate):

| sr | coefficient | implied tau (ms) |
|---|---|---|
| 44 100 | 0.08 (literal) | 4.3512 |
| 48 000 | 0.073746064 | 4.3512 |
| 88 200 | 0.040833695 | 4.3512 |
| 96 000 | 0.037579128 | 4.3512 |
| 192 000 | 0.018969484 | 4.3512 |

### 2-4. `tools/sr_check.cpp` — the bars (commits `2b10f9d`, `3eb165d`, `d98e42d`)

The one gate-threshold edit the B150 ruling sanctions, cited as such in the
file. Also moved the inertia wander control down beside the bar it qualifies —
it carried that bar as a duplicated literal, and B150 moving the bar is exactly
the day a threshold written twice disagrees with itself. The runtime banner,
which said "shrinking them is ruling B150, not this file's", was stale the
moment B150 ruled; rewritten.

## The measurements

`tools/sr_check.cpp` built against the pristine header
(`origin/b156-apz-snap`) and against this branch, same binary, same machine
(Apple M3, Apple clang 16.0.0, `-O2`). These renders read no clock and draw
from seeded streams only, so the numbers are bit-reproducible run to run.

**Per rate, per quantity.**

| quantity | rate | before | after |
|---|---|---|---|
| K step to 90 % (s) | 44 100 | 0.00992 | 0.00992 |
| | 48 000 | 0.00909 | 0.01001 |
| | 88 200 | 0.00503 | 0.01005 |
| | 96 000 | 0.00448 | 0.00992 |
| lock peak, dissolve 0.02 (s) | 44 100 | 0.04418 | 0.04418 |
| | 48 000 | 0.04395 | 0.04427 |
| | 88 200 | 0.04241 | 0.04414 |
| | 96 000 | 0.04223 | 0.04415 |
| lock peak, dissolve 0.05 (s) | 44 100 | 0.04567 | 0.04567 |
| | 48 000 | 0.04531 | 0.04560 |
| | 88 200 | 0.04342 | 0.04551 |
| | 96 000 | 0.04320 | 0.04546 |
| lock peak, dissolve 0.30 (s) | 44 100 | 0.04459 | 0.04459 |
| | 48 000 | 0.04427 | 0.04469 |
| | 88 200 | 0.04223 | 0.04481 |
| | 96 000 | 0.04203 | 0.04483 |
| inertia R, 10-120 s | 44 100 | 0.28006 | 0.28006 |
| | 48 000 | 0.28162 | 0.28030 |
| | 88 200 | 0.28555 | 0.27992 |
| | 96 000 | 0.27967 | 0.28205 |
| output pole @10 kHz (dB) | 44 100 | -0.623 | -0.623 |
| | 48 000 | -0.687 | -0.687 |
| | 88 200 | -0.998 | -0.998 |
| | 96 000 | -1.023 | -1.023 |

**Every 44.1 kHz column is unchanged**, which is the gate the ruling set.

**Drifts and bars.**

| quantity | audit | before | after | bar before | bar after |
|---|---|---|---|---|---|
| K step to 90 % | 54.9 % | 54.876 % | **1.252 %** | 0.659 | **0.0150** |
| lock peak, dissolve 0.02 | — | 4.415 % | **0.197 %** | (ungated) | (ungated) |
| lock peak, dissolve 0.05 | — | 5.400 % | **0.454 %** | (ungated) | (ungated) |
| lock peak, dissolve 0.30 | 5.56 % | 5.739 % | **0.536 %** | 0.0667 | **0.0064** |
| inertia R spread, 10-120 s | 15.03 % | 2.103 % | **0.763 %** | 0.1804 | **0.160** |
| output pole @10 kHz | 0.40 dB | 0.400 dB | 0.400 dB | 0.480 | 0.480 (STOPPED) |
| CONTROL attack 90 % | 0.125 % | 0.125 % | 0.125 % | 0.003 | 0.003 |
| CONTROL 1000 samples | — | 54.062 % | 54.062 % | > 0.40 | > 0.40 |
| inertia same-rate wander | — | 3.606 % | 4.915 % | (bar/3) | (bar/3) |

The two controls are unmoved, which is what they are for: the must-read-zero
quantity did not move (0.125 %) and the must-read-large synthetic per-sample
quantity did not move (54.062 %). A fix that had moved either would have been
a fix to the detector.

**Residual on the K-step row.** 1.252 % is not zero, and it is not a rate law:
44.1 k 0.00992, 48 k 0.01001, 88.2 k 0.01005, 96 k 0.00992 is non-monotonic.
It is the 0.2 ms probe grid plus the swarm's own `sigma` (which the smoother
chases and which itself moves), not the smoother's time constant.

## The onset lock: one defect, not two

No engine line was changed for it. `s.Kenv *= exp(-dt / max(0.01, dissolve))`
with `dt = kTick/sr` was already seconds-expressed; the snap's rate dependence
was entirely the smoother it feeds. All three dissolve settings fell together
(4.415 -> 0.197, 5.400 -> 0.454, 5.739 -> 0.536 %).

## The inertia curve — the dispatch's question, answered

The dispatch asked whether "the baked-in inertia CURVE" explains the audit's
15.03 % against the suite's 2 %, and asked for a measurement at the default
curve AND at the setting the audit used.

**The curve is not in the core.** It is the ADR-059 taper in the SHELL
(`src/hypersaw_clap.cpp:4329`): core `inertia = pow(knob, inertiaCurve)`,
default `inertiaCurve = 2.5`. Both the audit's `tickgrid.cpp` and
`tools/sr_check.cpp` call `setParam("inertia", 0.7)` on the CORE, so the taper
is in neither path — but a player's knob at 0.7 reaches the core as
`0.7^2.5 = 0.41`, and core 0.7 is knob 0.867, a much stiffer spring. Both were
measured (scratch probe, K 0.6, n 7, seed 1234, four rates, before and after,
with a same-rate 10-12 s vs 20-30 s control):

| core inertia | build | spread 10-12 s | spread 20-30 s | same-rate control |
|---|---|---|---|---|
| 0.70 (audit + suite) | before | 21.557 % | 10.789 % | 15.354 % |
| 0.70 | after | 20.596 % | 4.256 % | 15.354 % |
| 0.41 (= knob 0.7) | before | 0.032 % | 0.050 % | 1.454 % |
| 0.41 | after | 0.038 % | 0.027 % | 1.470 % |

So the human's hunch was right in substance, by a different route than
"the curve is baked into the core": **A6's 15.03 % is a chaotic-regime artefact
of driving the core to 0.7, not a sample-rate defect.** At core 0.7 the
quantity's same-rate control (15.35 %) is as large as the cross-rate spread the
audit attributed to rate. At core 0.41 — the setting a knob at 0.7 actually
reaches — the spring is rate-reproducible to 0.038 %.

The bar therefore moved only to 0.160: the tightest value that keeps the file's
own "bar >= 3x the same-rate wander" control true. Tightening it to the
measured 0.763 % would gate noise. **Left for a ruling:** add a second inertia
row at core 0.41, where a 0.1 % bar would have real teeth. Not done here — this
brief sanctions moving thresholds, not changing what the gate measures.

## The output pole: STOPPED

The ruling was "express the rate-bound constant in seconds". **This one already
is, exactly.** `a = 1 - exp(-2*pi*fc/sr)` has time constant
`-1/(sr*ln(1-a)) = 1/(2*pi*fc)` = 8.842 us at fc = 18 kHz — independent of `sr`
in closed form, not approximately. The audit's 0.40 dB is impulse-invariant
ALIASING: at 44.1 kHz the pole is so far outside its accurate region that the
response is shaped by its own mirror, and the mirror moves with the rate.

A partial correction was measured (scratch, `pole.py` / `pole2.py`): keep the
topology, and at rates != 44.1 kHz solve the coefficient reproducing
`|H_44.1k|` at a fixed anchor. Worst |dB| error over 20 Hz - 20 kHz restricted
to where `|H_44.1k| > -30 dB`, 44.1-96 k, at the default fc = 18 kHz:

| construction | worst over the band | at 10 kHz |
|---|---|---|
| current law | 1.488 dB | 0.400 dB |
| anchored at 20 kHz | 0.248 dB | — |
| anchored at 18 kHz | 0.177 dB | 0.166 dB |
| anchored at 16 kHz | 0.217 dB | — |
| the BEST single pole at each rate | 0.149 dB | — |

Not taken, and not for effort: it is a new design law (ADR territory — an
intentional divergence from the prototype, the human's call), it is partial by
construction (the 0.149 dB row is the topology's floor: a 44.1 k response is
periodic in f with period 44100 and a 96 k filter's with 96000), and it costs
three transcendentals per voice per control tick in a routine the audit already
measured at 23-34 % of CPU, to buy 0.23 dB at 10 kHz at rates the shipped
default is not. Surfaced, not taken; the bar stays at 0.480.

## The 44.1 kHz bit-identity witness

The B148 164-item corpus, rebuilt from
`traces/2026-09-18-b148-ncap-bitident.md`:

- **A, 156 items** — every scenario in `build-golden/manifest.tsv`, replayed
  under `parity_check`'s exact protocol (midi from the manifest, 4 s at
  44.1 kHz, 1024-frame blocks, note-off before the first block at >= 3 s,
  params in serialized order). 60 of the 156 set `K`, so the smoother is
  genuinely exercised.
- **B, 8 items** — 8 notes x n in {1,7,16,32} x law in {0,4}, 128-frame blocks,
  mid-render changes to detune / anchor / spread / width / n, a no-op
  `setParam("n", same)` and a real n change. B also drives `K` 0 -> 1 -> -1 and
  `onset` mid-render: a corpus that never moves K never touches the line under
  test.

Hashed FNV-1a-64 over (a) the interleaved float32 audio and (b) the focus
voice's `(R, RN, psi)` per block. Built twice from one source, against the
pristine header and against this branch.

**All 164 items byte-identical.**

**Must-differ control** (L0032 — an equality witness that cannot fail is not a
witness): a planted header with the 44.1 kHz branch deleted, so 44.1 k takes
the three-ULP-short seconds coefficient. **14 of the 164 move** — all 8 B items
and 6 A items (`pivot-splay` and `pivot-anchor`, all three seeds each).

*Recorded coverage boundary:* the other 150 A items do NOT move under the
plant. A 3-ULP coefficient difference is below float32 output resolution for
most 4-second renders; only the pacemaker scenarios amplify it far enough to
survive rounding. So corpus A alone would have been a weak witness for THIS
change, and corpus B (which drives K hard, mid-render, over 2 s at 128 frames)
is what carries it.

**The B literals in the B148 trace are not reproduced and cannot be.** That
trace records corpus B's SHAPE, not its script (durations, block indices, note
list), so my B items are a different eight renders. The witness is the
differential — one binary, two headers — not agreement with a recorded literal.

Two more independent 44.1 kHz witnesses, both from gates rather than from a
hash I wrote myself:

- `parity_check`: **156/156 within eps=1e-06, worst 4.262e-09 @ dyn-ring.seed42**
  against goldens regenerated from `reference/swarmsaw.html` in the same run.
- `statefix_check`: GREEN, 3 fixtures — it asserts a bit-identical render
  against frozen `.f32` goldens.
- `waveshape_check` built against the pristine header and against this branch:
  **output byte-identical**, diff empty.

## Alternatives rejected

- **Making the control tick a fixed TIME** (the ADR-086 Amendment 1 treatment,
  one level up) to close the inertia spring's Euler truncation error. It could
  be made 44.1 k-exact (16/44100 s, like `kGravGridSeconds`), but it changes
  the control rate of the ENTIRE engine at every other rate — far past "express
  this constant in seconds" — and the measurement above says the quantity it
  would fix is a chaotic-regime artefact, not a rate defect. Reduce, never
  invent.
- **Anchoring the output pole's coefficient** — see the STOPPED section. New
  design law, ADR territory, partial by construction.
- **A `static double ksmCoefFor(double)` helper on `SwarmCore`.** Written
  first, reverted: the constructor sits in a public section, so the helper
  would have extended the class's public interface — a human gate. The
  expression lives in the member-init list instead, and the only new class
  member is private.
- **A function-local `static const double` for the tau literal**, so
  `std::log` could compute it. Rejected: a thread-safe-init guard in a routine
  on the audio path, to save a literal that a control now pins anyway.
- **Matching the pole at 10 kHz** (which would have driven the gated number to
  0.000 dB). Rejected on sight: the check measures 10 kHz, so a law fitted at
  10 kHz agrees with the detector by construction and sees nothing.

## Verify

- `./verify full` — **exit 0** at `d98e42d`, the complete four-commit change
  set (`.harness/last-verify.json`:
  `{"target":"full","exit":0,"git":"d98e42d","ts":"2026-09-18T21:51:45Z"}`), and
  again on the commit that adds this trace.
  `parity_check` 156/156, `samplerate_check` GREEN (untouched — 0.125 % /
  0.163 %, unchanged), `subdiv_check` GREEN, `waveshape_check` GREEN,
  `state_check` / `statefix_check` / `bank_check` GREEN, `rtsafety_probe` GREEN
  (320 process() calls, allocation-free), `include_check` GREEN (105 files).
- `./verify fast` — exit 0 after each of the four commits.
- `sr_check` (standalone, unwired) — GREEN, 0 failures, all eleven controls and
  bars passing.

## Open questions

1. **The inertia gate has no teeth at core 0.7 and could have them at core
   0.41.** Adding that row changes what the gate measures, which this brief did
   not sanction. Lead's call.
2. **The output pole needs its own ruling.** It is a different defect class
   (aliasing, not ADR-009), a correction exists at a measured cost, and it is
   an intentional divergence from the prototype, so it needs an ADR either way
   — including an ADR that says "keep the aliasing, it is the 44.1 kHz sound".
3. **`specs/ACCEPTANCE.md` is out of scope and its numbers are now stale where
   they quote A1.** What should change, for the lead to write: the K-step /
   onset-lock sample-rate figures, wherever A1's 54.9 % / 5.56 % are quoted.
   The 0.3 % `samplerate_check` bar itself is untouched and still correct.
4. **`sr_check` is still standalone and unwired** into `./verify`. Wiring a
   gate is the human's ruling; nothing here changed that.
5. **Unverified:** the residual 1.252 % on the K-step row is attributed above
   to the probe grid and to `sigma`, from the non-monotonicity of the per-rate
   values. That is an inference, not a measurement — I did not re-run the probe
   at a finer grid to confirm it.
