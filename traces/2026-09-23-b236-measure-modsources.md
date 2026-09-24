# b236-measure-modsources — computational cost of the Kuro LFO and ORBITAL

- **Queue item:** B236 (row carried in PR #736, `lead-records-84`; dispatched 2026-09-23 by the horde lead). Human, verbatim: "let's test the computational costs of orbital and kuramoto LFO, just to make sure they wouldn't break anything."
- **Why:** Both laws exist only as JS (docs/design/shape-lab-mod.html, B226 round 2). Before either is a serious port candidate, their worst-case CPU cost needed a number against the engine's own recorded budget (tools/measure_cpu.cpp).

## What was done

New file `tools/measure_modsources.cpp` (self-contained; no CLAP link), wired into `CMakeLists.txt` beside `measure_alias`/`measure_cpu` as a standalone `add_executable`. Both laws are ported faithfully in doubles from `docs/design/shape-lab-mod.html:754-843` (KuroSwarm) and `:910-957` (OrbitalField's accel()/step()), which the cited trace below already measured bit-identical to `mod-lab.html`'s own `KuroSwarm` (1,056 configs) and to `reference/gravity-modulator.html`'s own `step()` (four presets). Cushion/regulators/kick/reset/trail/smoothing are left out — not part of the named laws, and irrelevant to the O(N²) cost question.

**Control-tick rate correction.** The brief's parenthetical asked me to check "16 samples" against `src/`. `kTick=16` (swarm_core.h etc.) is the DSP-internal smoothing tick, not the rate a modulation source is read at — the actual modulation/intent grid is `kGravGridSeconds = 256/44100 s ≈ 172 Hz` (`src/swarm_core.h:136`), named "the mod tick" explicitly in ROADMAP B199. The bench reports both, with per-sample-rate tick sizes recomputed via `lround(sr * kGravGridSeconds)` (ADR-009: seconds, never a per-tick constant), matching how `hypersaw_clap.cpp` actually computes its grid.

**Calibration control:** a fixed FLOP loop (`calibLoopMs`, same shape as `measure_cpu.cpp`'s `refLoopMs`), `volatile`-sunk, min-of-3 = 61.98 ms on this run — proves the timer is seeing real work and nothing was optimised away. Every timed loop below also sinks its result through a `volatile` read for the same reason.

## Evidence consulted

ROADMAP B236 (`origin/lead-records-84`); trace `2026-09-23-b226-modulator-lab-2.md`; `specs/SPEC-ORBITAL.md` in full (§3.1 body count 2-8, §5 numerics, §6.2 replay determinism, §11 divergences table); `docs/design/shape-lab-mod.html:601-1022` (DSP section, KuroSwarm + OrbitalField); `docs/design/mod-lab.html:75-248` (KuroSwarm, for the branch's provenance); `reference/gravity-modulator.html:307-460` (accel/step, for provenance); `tools/measure_cpu.cpp` (pattern, calibration idiom, recorded audit numbers); `tools/golden/extract_core.mjs` (used read-only from a scratch Node script to pull the lab's own classes for the parity diff); `src/swarm_core.h:136` and ROADMAP B199 (the real modulation-tick rate); `tools/test_table_check.py` (confirmed `measure_modsources.cpp` outside its `*_check` glob — ran it before and after, same 62 wired / 1 unwired count).

## Parity (by-hand diff, not a gate)

Scratch harness under `scratchpad/b236/` (this session's scratch dir): `tools/measure_modsources --parity` printed deterministic KURO (n=8, K=±0.6, 4800 samples) and ORBITAL (synthetic 8-body ring, dt=1/480, 4800 steps) trajectories; a Node script imported `KuroSwarm`/`OrbitalField` straight out of `docs/design/shape-lab-mod.html` via `tools/golden/extract_core.mjs` ('design' banner set) and ran the identical setup.

- **KURO, both K signs: max|Δ| = 0** (bit-identical over all 16 printed phase/lfo values per config).
- **ORBITAL, as normally built (`-O3`, default FP contraction): max|Δ| = 6.14e-6** after 4800 chaotic steps (100 sampled timesteps × 8 values). Root-caused, not hand-waved: rebuilding the SAME source with `-ffp-contract=off` (disabling fused-multiply-add) gives **max|Δ| = 0** against the same JS run — confirmed the initial conditions were already bit-identical between `std::cos/sin` and `Math.cos/sin` on this machine, so the divergence is FMA rounding in the accel() pair loop, amplified exponentially by this system's chaos over 4800 steps, not a logic error in the port. This is exactly SPEC-ORBITAL §6.2's own warning ("two runs differing in the last bit diverge") and consistent with §11 declaring ORBITAL a *behavioural*, not bit-exact, oracle. The benchmark itself is built with default flags (FMA on) because that is how the shipped engine would compile — the `-ffp-contract=off` rebuild was a one-time diagnostic, not part of the shipped tool.

## Results

Release build (`-O3`), 20 s of simulated audio per cell, min of 3 reps, `build-bench` (separate from `build-release`). Full table is the binary's own stdout (reproduced below is the summary row set; the PR/report has the complete 36-row table).

**KURO LFO, n = 8 (MAXK, ADR-053):**

| branch | rate | 44.1 kHz | 48 kHz | 96 kHz |
|---|---|---|---|---|
| splay (K<0) | per-sample | 0.2167% | 0.2337% | 0.4658% |
| splay (K<0) | per-172Hz grid | 0.2079% | 0.2256% | 0.4452% |
| mean-field (K>0) | per-sample | 0.1652% | 0.1821% | 0.3623% |
| mean-field (K>0) | per-172Hz grid | 0.1644% | 0.1793% | 0.3544% |

**ORBITAL, 8 bodies (max, SPEC-ORBITAL §3.1):**

| dt | rate | 44.1 kHz | 48 kHz | 96 kHz |
|---|---|---|---|---|
| 1/960 s (spec, shipped) | per-sample | 0.0142% | 0.0150% | 0.0175% |
| 1/960 s (spec, shipped) | per-172Hz grid | 0.0122% | 0.0115% | 0.0120% |
| 1/480 s (lab/reference) | per-sample | 0.0084% | 0.0087% | 0.0115% |
| 1/480 s (lab/reference) | per-172Hz grid | 0.0059% | 0.0057% | 0.0058% |

No `droppedTicks` warning fired in any cell (the accumulator's step cap never bound — confirms the "negligible" physics-step rate SPEC-ORBITAL §5 already claims).

**Against the engine's recorded budget** (`tools/measure_cpu.cpp`, 2026-09-18 audit, Apple M3, 8 held notes): 1 osc n=32 = 7.00% of a core, 2 osc n=32 = 14.15%. KURO's worst single instance (splay, 96 kHz, per-sample) is 0.4658%; ×8 (per-voice polyphony, same 8-note convention) is 3.73%. ORBITAL's worst single instance (shipped dt, 96 kHz, per-sample) is 0.0175%; ×8 is 0.14%. Both are a linear, conservative upper bound — SPEC-ORBITAL §2's own default is one *global* field, i.e. the single-instance row, not ×8.

## Verdict

- **ORBITAL is safe at every rate and sample rate tested, including the ×8 per-voice worst case** (0.14% vs. the engine's 7-14% existing budget) — confirms SPEC-ORBITAL §5's own "negligible" estimate with a real number rather than an assertion.
- **Kuro LFO is safe at every rate and sample rate tested**, including ×8 (3.73%, still well under the 2-osc n=32 budget of 14.15%). Its per-sample cost (~0.47% worst case) is roughly 25× ORBITAL's, dominated by the transcendental calls (sin/cos) inherent to n=8 phase voices — expected, not a surprise.
- **Neither law needs to drop rate.** Calling either at the per-sample rate vs. the real ~172 Hz modulation grid makes at most a ~5% relative difference in %CPU (function-call overhead, not algorithmic cost) — nowhere near the difference that would force a decimated read.

## Alternatives rejected

- **Porting from `mod-lab.html` directly** (its own KuroSwarm, with the per-sample allocate-and-sort splay). Rejected because `docs/design/shape-lab-mod.html`'s transcription is the one the brief names as "THE LAWS", and B226's own trace already proved it bit-identical to mod-lab's — porting the slower, allocating version would benchmark a law nobody plans to ship.
- **Bit-parity ORBITAL claim without root-causing the 6e-6.** Reporting the number without the `-ffp-contract=off` control would have looked like a silent port defect; the diagnostic rebuild is what turns "differs" into "differs for a named, expected reason".
- **Wiring this into `./verify`.** It is a timing measurement with no threshold to defend (like `measure_cpu`/`measure_alias`); a bar here would be exactly the coin-flip `measure_cpu.cpp`'s own header warns against on a machine whose load varies run to run.

## Verify

`./verify fast` on the committed hash — see the PR for the exact hash and `.harness/last-verify.json` output.

## Open questions

1. Whether Kuro LFO and ORBITAL are per-voice or global-scope sources is still open (SPEC-ORBITAL §12.1 for ORBITAL; Kuro's scope was never ruled). This bench reports both a single-instance and a ×8 linear bound so either ruling has a number to check against.
2. `tools/measure_modsources.cpp` is a benchmark, never a gate — if a future ROADMAP item wants a regression bar on either law (the `cpu_check` pattern: same source built twice, judging switched on by a compile define), that is a human decision, not something this dispatch should pre-empt.
