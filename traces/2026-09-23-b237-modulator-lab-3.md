# b237-modulator-lab-3 — S&H and smoothed RANDOM join the modulator lab as LFO shapes, under round 1's one-law rule

- **Queue item:** B237 (row carried in PR #736, `lead-records-84`; dispatched 2026-09-23 by the horde lead). Round 1 is B208 (trace `2026-09-22-b208-modulator-lab.md`), round 2 is B226 (trace `2026-09-23-b226-modulator-lab-2.md`).
- **Why:** The human asked for "a standard S&H and a random noise LFO with smoothing options in the lab as LFO options". B237's acceptance asks for four things: both shapes seeded with mulberry32, the one-shape-law rule applied to both, a stated relationship to round 2's TRIGGER → S&H action (so the instrument does not end up with two S&Hs that mean different things), and the batch design standard (both themes, `lab_load_check`, `lab-review`).

## What was done

One file changed, `docs/design/shape-lab-mod.html` (commit **b1a0bc1**). `<meta name="lab-review">` now reads `B208 + B226 + B237 · 2026-09-23`. `docs/design/index.html` and `mod-lab.html` were not touched.

**The law.**
- **The stream is the engine's own.** It is draw k of mulberry32(`lfoSeed`), where `lfoSeed` = patch seed ^ 0x9E3779B9, which is `hypersaw_clap.cpp:2847-2850`'s `lfoSeed(0)`. Each draw becomes v_k = 2u_k − 1, as at `:3960`.
- **Draw k is read by index.** `m32at` works because mulberry32's state after j calls is seed + j·0x6D2B79F5, so output k is one hash. This makes every non-slew mode a pure function of (seed, k, x), so the picture can read any cycle and never advance the sound's stream.
- **The position** is round 1's p = acc + φ, not wrapped: k = ⌊p⌋ and x = p − k. The free accumulator now counts its wraps (`cyc`). The subtraction is unchanged, so round 1's accP is bit-identical.
- **The modes:**
  - S&H = v_k.
  - RANDOM: NONE (identical to S&H, and audited so), LINEAR, COSINE, and CUBIC (Catmull-Rom, clamped to ±1). All four read v_k at each cycle start.
  - SLEW glides from wherever it is to v_k and arrives at the stated ms, counted in samples, so it does not move with the rate. It is round 1's segment law at c = +0.5 (`segCurve`), not a new curve.
- **Strikes and sync.** A strike (retrig, or round 2's LFO RESTART) starts a new cycle on the next unused draw. In sync, k comes from the transport.

**Picture.**
- Under S&H or RANDOM, the LFO canvas shows eight cycles (the page the LFO is on) from `core.randPage`, the B177 idiom. The breakpoint editor is disabled for those shapes, and the points are kept.
- A new card, SMOOTHING, draws the same eight draws under all five laws. You hear the lane you click. A meter-coloured line marks where the sound is reading.
- SLEW's lane is exact for cycles already played, because the core records each glide's start in an 8-slot ring. The rest of the page is a forecast at the current rate. This is stated on the page.

**HEAR = SHIPPED** plays the engine's S&H, transcribed:
- 0 until the first wrap (`:2859`);
- one draw from the in-order stream per wrap (`:3954-3961`);
- a strike rewinds the phase and draws nothing (`:3949`).

The engine has no smoothed random, so SHIPPED plays that same S&H for RANDOM too.

**Naming proposal** (card "TWO S&Hs, ONE NAME EACH"):
- **S&H** is the LFO shape, the shipped name (`lfoShapeAt` case 5). Clock: its own cycle start. Input: its own stream. Law: out(p) = v_⌊p⌋.
- **SAMPLE ON TRIGGER** is round 2's action. Clock: the note-on TRIGGER. Input: its own stream (draw j at trigger j) or any continuous source. Law: held ← input at the trigger's sample.
- The two share one hold rule and one draw rule, and differ only in clock and input.
- The page applies the rename to round 2's labels. The ids `act.sh` and `shIn` are unchanged, so no route or preset moves.

**The engine's S&H compared with this one** (card "THIS S&H vs THE SHIPPED ONE"):
- **Same:** the stream, the seed derivation and the numbers.
- **Different:**
  1. the first cycle (engine 0, proposed draw 0);
  2. which draw is used (engine: the n-th wrap the mod tick saw, one draw per tick however many cycles it crossed; proposed: the cycle index, independent of rate, block and tick size);
  3. a strike (engine keeps the held value; proposed takes a fresh draw);
  4. sync (engine: from activation; proposed: from the transport);
  5. φ (proposed: a live offset).
- With free-running, no retrig and φ = 0, proposed cycle k plays the shipped cycle k + 1. This is measured, below.

**Round 1 and round 2 are unchanged in behaviour.**
- Round 1's audit now pins `St.lfo.kind = 0`, because it audits the breakpoint law. That scopes it; it does not relax it.
- Round 2's text is renamed as described above.

## Verification (beyond ./verify)

Scratch work is under `scratchpad/b237/`, served on 127.0.0.1:8237.

- **In-page audits, headless Chrome and a node vm harness.** Round 1: LFO 0, ENV 0, controls 0.100/0.100, mirror 7.8e-16. Round 2: all 0, every control fires, "9 SAMPLE ON TRIGGER steps".
- **Round 3, two passes:** the card as set (1 Hz reaches draws 0..3), and free 13.7 Hz with φ 0.37 (draws 0..45). Retrig is forced ON with 3 strikes.
  - Destination tap vs a twin drawing from the **sequential** stream (never `m32at`): S&H, NONE, LINEAR, COSINE, CUBIC, SLEW (80 ms) and SLEW (3 cycles) all **0**.
  - S&H vs NONE **0**. `m32at` vs the stream over 4097 draws **0**. HEAR SHIPPED vs the restated engine law **0**.
  - The picture's own `randPage` at each sample's k + x vs the tap, for S&H and the four pure modes: **4.8e-15** (tolerance 1e-9, because k + x re-rounds x).
  - Controls, all firing: seed + 1 (1.92), the shipped draw index (1.79), a strike that keeps the stream (1.37), the neighbouring smoothing (≥ 0.08), slew ×1.25 (≥ 0.065), proposed vs shipped (1.79), the picture one cycle on (1.79).
  - Cost: 0.7-1.0 s per edit in node, against round 2's 1.4 s. It renders at 4 kHz with a lightened clone. Profiling showed voice DSP was 80% of the time.
- **Planted defects** (scratch copies, each anchor asserted to match once). Each of these turns round 3 **RED**, with rounds 1 and 2 green:
  - P1: render × (1 + 1e-12).
  - P2: `m32at` off by one.
  - P3: a strike does not advance.
  - P4: SLEW timed by cycle fraction.
  - P5: a shipped strike draws.
  - P6: the painter pages one draw late.
  - P7: SLEW primes on every cycle.

  Two stay **GREEN**, as expected: **P8**, CUBIC clamp removed, and **P9**, SLEW curve c = 0. Both are inside the shared law functions (`randInterp`, `slewAt`), which the twin shares. This is round 2's M6 coverage boundary, recorded in the audit's header comment. Those properties were checked separately:
  - The Catmull-Rom maximum is exactly **1.25**, on the run (−1, +1, +1, −1). 1.2% of samples on a 2000-draw stream are clamped.
  - All pure modes pass through v_k at x = 0 (**0**). Joins are continuous to 1.9e-12.
  - `slewAt` returns v exactly at the slew time, and c = +0.5 equals the normalised one-pole (**0**).
- **The engine's stream, not a lookalike.** A C++ program built on `src/force_core.h`'s own `rngNext`, with `:2847-2850`'s `lfoSeed` formula, prints lfoSeed(0) = 2654436715 for patch seed 1234. Its first 8 draws match the lab's `m32at` to 12 decimals.
- **Proposed k = shipped k + 1** (real core, 5 Hz free, 13 cycles): max|Δ| **0**. The shipped first cycle reads 0. Control, same-cycle comparison: 1.79. A replay of the same patch: **0**.
- **Load gate:** `lab_load_check` OK. `extract_core` (design banners) returns `ModLabCore` with the new statics (`m32at`, `drawV`, `randAt`, `randInterp`, `slewAt`, `lfoSeedOf`).
- **Screenshots** (not committed), in `scratchpad/b237/`:
  - `b237-full-light.png` (RANDOM COSINE, 2.5 Hz, playhead in page k 8..15);
  - `b237-full-dark.png` (`?theme=dark` as a load; RANDOM SLEW 450 ms, longer than the 333 ms cycle);
  - crops `b237-lfo-light.png`, `b237-lfo-dark.png`, `b237-r3-light.png` and `b237-r3-dark.png`.

  Headless audio is throttled, so each shot's scratch copy drives the page's own `core.render` forward before painting, as round 2's wrapper did.
- **Leak:** no machine paths or private names in the file. The worktree has no `.leakcheck-names`, so verify skips that leg.
- **Verify:** `./verify fast` exit 0 on the committed lab change **b1a0bc1** (`.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"b1a0bc1"}`).

## Evidence consulted

ROADMAP row B237 (`origin/lead-records-84`); traces B208 and B226; `docs/design/shape-lab-mod.html` in full at e07acac; `src/hypersaw_clap.cpp:2825-2885` (Lfo, lfoSeed, lfoReseed, lfoShapeAt), `:3940-3963` (the LFO tick), `:5560-5610` (lfoCycleJson); `src/force_core.h:46-55` (rngNext); `src/gui/gui2.html:6900-6945` (drawLfo's S&H); `tools/labharness/lab_load_check.mjs`; `tools/golden/extract_core.mjs`. LIBRARY lessons applied: L0026 (state above wiring), L0032/L0033 (controls that must fire; a plant that stays green records a coverage boundary), L0048/L0061 (own scratch folder and port), L0051/L0056 (verify on the committed hash, read `.harness/last-verify.json`).

## Alternatives rejected

- **S&H as breakpoints** (STEPS-style). A fixed table repeats; S&H never does. STEPS stays, as a different thing.
- **A sequential-stream core, with the picture on a copied stream** (the engine's `lfoCycleJson` idiom). This breaks under a strike and a φ offset, where cycles are skipped or revisited. Index access makes the draw a function of k, so no copy is needed.
- **Interpolating toward v_k after it is taken** (causal, one cycle late). The knots would no longer sit on the S&H steps. SLEW is kept as the causal form, and this is listed as a question.
- **A B-spline for CUBIC.** It never overshoots, but it misses the draws. Catmull-Rom with a clamp was kept, and this is listed as a question.
- **One module for both S&Hs** (S&H with an input). This would be a feature, not a naming fix. It is listed, not built.
- **A committed `tools/*_check` for the round-3 audit.** ADR-180 §1 would allow it, but the brief did not ask for it. It is listed for the lead.

## Open questions

These are listed on the page, card "ROUND 3 · FINDINGS AND OPEN QUESTIONS".
1. The naming ruling: S&H is the LFO shape and SAMPLE ON TRIGGER is the trigger action. Or should S&H take an input, so the two become one module?
2. The first cycle: should the engine draw at the first cycle start? Today it holds 0 for the first cycle.
3. What a strike does to the stream: the next draw (proposed), keep the held value (shipped), or rewind the stream (a repeatable riff)?
4. Look-ahead in LINEAR, COSINE and CUBIC: keep the knots on the cycle starts?
5. CUBIC's 1.25 overshoot: clamp, or use a B-spline?
6. SLEW's law: arrival in ms (proposed), a constant-rate limiter, or τ? And should SLEW also apply after the other smoothings?
7. **A shipped picture/sound mismatch** (`src/`, out of scope, for the lead to queue). `src/gui/gui2.html:6925-6929` says the S&H start-phase dot sits on draw 0, "the value a strike at any phase holds". The engine's strike does not draw (`:3949`), and draw 0 is first heard in the second cycle (`:2859`, `:3954-3961`).
8. STEPS vs S&H: both stay.
9. There is no committed gate for the round-3 audit.
