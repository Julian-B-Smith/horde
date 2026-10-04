# The CPU ledger of horde 2's composed engine, `h2/engine/`

ROADMAP B441 phase 1(A), the CPU campaign (human, 2026-10-04: "I feel like we really
need to start doing research and running optimization audits and constructing
experiments to whittle down the CPU weight"). The budget is B439's, in
`docs/proposals/module-1.0-bar.md` (appendix): the sources may use **at most 34 % of
a min-spec core for 8 voices**.

This document holds four things:
- §1, the **frozen measurement protocol** (B441-1), which every phase-2 experiment re-runs;
- §2, the **baseline ledger**, which those experiments are measured against;
- §3, the **cost attribution** of a heavy voice, by stage;
- §4, the **interpretation**: the top cost centres, and the B378 findings that map onto them.

CPU is Layer-E (ADR-187 item 8). It is measured and reported, never a verify gate. The
tool is `tools/measure_h2_engine.cpp`, UNWIRED for that reason (its header). This PR
measures only. Nothing in the engine's output changed (§5).

**Machine:** this Mac, Apple M3 (4 performance + 4 efficiency cores), Apple clang 16,
Node 24. **Measured** 2026-10-04 at `deb4d1a` (this PR's first commit). The load
averages seen are in §2.

## 1. The protocol (frozen: B441-1)

A phase-2 experiment is measured with exactly this. Changing any item below makes a
new protocol version (B441-2, …). The baseline is then re-measured under it in the same
PR. Figures are never compared across versions.

| item | frozen value | why |
|---|---|---|
| build | `build-release` (`-DCMAKE_BUILD_TYPE=Release`, Unix Makefiles), target `measure_h2_engine`: `-O3 -ffp-contract=off` on top of Release's `-O3 -DNDEBUG` | The h2 flags (h2 rule 7): the shipped build is arithmetically the tested one, so it is also the timed one. Never Debug: `-O0` figures are meaningless. |
| engine | `horde2::engine::Engine` exactly as shipped: no `H2_ENGINE_FAULTS`, no `H2_ENGINE_STAGES` | The stages build (§3) is for attribution only. Its figures are never ledger figures. |
| rate, block | 48 kHz, 128-sample blocks | The brief's choice. B439's E-6 anchor says 44.1 kHz. 48 kHz is ~8.8 % more work per second of audio, so the ledger is conservative against the anchor (open question 1). |
| oversampling | the preset's own. No bank preset sets `os`, so every row runs at the engine's 2 | "As the preset sets it." |
| presets | the bank (`reference/scalpel/data/presets.json`), read from the parity stream, so each is played exactly as the parity check plays it: `presetCmds(params)` (with render-goldens' `gain 0.35`), then `snap`. "The oracle defaults" is no preset. | One source for the bank, and no copy to drift. |
| voice pool | `poly 8` after the preset | The engine's pool is `kVoices = 8` (`h2/engine/engine.h:69`). The `poly` default is 6 (`h2/engine/blade.h:45`), so without this an 8-voice row would play 6. |
| note script | V voices, V ∈ {1, 8, 16}: keys 48 + 3k (k = 0 … V−1, B378's chord), velocity 0.85, all on before the first block, held | A held chord is the steady-state cost. B378's keys keep the swarm audit comparable. |
| 16 voices | two engines of 8 (the second plays keys 72 … and is seeded `0xB385 + 1`), both rendered every block | The engine has 8 voices. Two instances is what 16 would cost today (open question 2). |
| seed | `seedRandom(0xB385)` (checkpoint 4's) | |
| window | 0.25 s rendered untimed (the onset and the first DC estimates), then 2.0 s timed (750 blocks) | Steady state, not onset. |
| repeats | best of **5**, **interleaved**: repeat r runs every cell once, in a fixed order, before repeat r + 1 begins | A transient disturbance then hits one repeat of every cell, not every repeat of one cell. The best is the least disturbed. |
| calibration | 1e8 dependent multiply-adds (`x = x * 1.0000001 + 1e-9`, contraction off), timed at the start of every repeat, best kept. **ratio** = (best seconds per second of audio) ÷ (best calibration seconds) | The B236 / B262 pattern, with checkpoint 4's own loop so its figures compare. The loop is latency-bound (a multiply then an add, about 7 cycles), so ~175 ms is a performance core at 4.05 GHz. A figure near 250 ms suggests an efficiency core or a throttled one (hypothesis, from the cycle count). Later runs are compared on the **ratio**. |
| load guard | before each repeat, the 1-minute load average (`getloadavg`) must be ≤ 3.0. Above it the tool waits 20 s and re-reads, for up to 10 min, then refuses to measure (exit 3). The lowest and highest loads seen go into every row. | The brief's guard. The bench itself adds ~1 while it runs, so `load_hi` (read at the end of a repeat) can exceed 3. |
| sink | each timed block's first sample is summed into a `volatile` | The optimiser cannot drop the render. |
| conversion | **min-spec % = M3 % × 1.5. AN ASSUMPTION** (module-1.0-bar.md appendix) until it is measured once on an M1 base or on the Intel min-spec. It is labelled in every row. | |
| verdict | the 8-voice row's min-spec %, against the **34 %** slice | It prints and never judges (Layer-E). |

**How to run it** (from the repo root, with Node 24 for the preset stream):

```
node tools/h2_engine_render.mjs --only '^P/.* :: chord$' > <scratch>/presets.bin
build-release/measure_h2_engine --ledger --presets <scratch>/presets.bin
```

The preset stream is written beforehand because rendering the 83 goldens takes Node
~6 s on 7 workers. That lifts the load average right before the first repeat. Without
`--presets`, the tool renders the stream itself. `--reps R` changes the repeat count
(the protocol is 5). `--force` skips the load guard and must never be used for a ledger row.

**The machine-readable row.** One line per cell, `LEDGER {json}`, with these fields:
`protocol` (`B441-1`), `head` (`git describe --always --dirty`), `preset`, `voices`,
`os`, `sr`, `block`, `timed_s`, `best_s`, `pct_m3` (% of one M3 core, all voices),
`pct_m3_per_voice`, `cal_ms`, `ratio`, `minspec_pct_assumed_x1.5`, `slice_pct`,
`over_slice` (8-voice rows only), `load_lo`, `load_hi`. A phase-2 PR pastes its rows
beside the baseline's.

## 2. The baseline ledger (B441-1 at `deb4d1a`)

**The presets, light to heavy.** Four were named by the brief: Quarter sync, the oracle
defaults, Crushed bells and Glass horde pad. Three more were chosen from a coarse sweep
of the whole bank (`--sweep`: one voice, best of 2, 1 s timed; not a ledger figure).
That sweep ranks the bank from Quarter sync at 0.66 % to Glass horde pad at 8.78 %:
- **Harmonic stack**: the middle of the bank (2.94 %, rank 44 of 84). One blade, N 7, sync.
- **Fold over sync**: serial interplay (`b2mix` 1), so it takes `outSerial` and `dcPair`. 3.20 %.
- **Breathing pad**: the second-heaviest preset in the bank. 6.33 %.

Every row runs at os 2, 48 kHz, 128-sample blocks, with interleaved best of 5. The
calibration's best was 190.2 ms. The load average ran from 2.69 to 3.38 (the guard
waited it below 3.0 before each repeat). The min-spec column is **ASSUMED ×1.5**.

| preset | shape | 1 voice, % of an M3 core | 8 voices, % M3 (per voice) | 16 voices, % M3 | 8 voices, min-spec (×1.5, ASSUMED) | vs the 34 % slice |
|---|---|---|---|---|---|---|
| Quarter sync | N 1, one sync blade | 0.64 | 3.71 (0.46) | 7.77 | 5.6 % | under |
| the oracle defaults | N 5, one sync blade | 2.03 | 16.05 (2.01) | 33.61 | 24.1 % | under |
| Harmonic stack | N 7, one sync blade | 2.93 | 24.22 (3.03) | 50.56 | 36.3 % | **OVER** (×1.07) |
| Fold over sync | N 3, two blades, serial | 3.19 | 25.47 (3.18) | 52.99 | 38.2 % | **OVER** (×1.12) |
| Crushed bells | N 6, ring + crush | 5.83 | 47.71 (5.96) | 98.08 | 71.6 % | **OVER** (×2.1) |
| Breathing pad | N 6, sine→saw sync + noise | 6.66 | 53.10 (6.64) | 107.37 | 79.6 % | **OVER** (×2.3) |
| Glass horde pad | N 9, FM + sine→saw sync, xm 0.15 | 8.72 | 72.32 (9.04) | 147.32 | 108.5 % | **OVER** (×3.2) |

**What it says.**
- **Five of the seven presets miss the slice at 8 voices.** Only the light two fit. The
  middle of the bank is already ~7 % over, and the heavy pads need a 2–3× reduction.
- **Cost is linear in voices.** On every preset, the per-voice cost at 16 voices is 1–5 %
  above the cost at 8, because the second engine adds its own fixed work. The exception
  is a 1-voice row, which carries the engine's per-sample fixed work alone (Quarter sync:
  0.64 % at 1 voice, 0.46 % per voice at 8).
- **It agrees with checkpoint 4.** One voice of Glass horde pad, Crushed bells and Quarter
  sync read 8.72 / 5.83 / 0.64 % here, against 8.69 / 5.90 / 0.68 % there (4 s including
  the onset).
- **It reproduces.** An earlier run of the same protocol, on the same engine code
  (`a2dd06e` plus this PR's then-uncommitted edits, which compile out of this build;
  calibration 189.4 ms, load 2.42–3.30), agreed on every cell to within 1.6 % (2.0 % on
  the ratio).

The 21 `LEDGER` rows of the baseline run, verbatim:

```
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Quarter sync","voices":1,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.012793,"pct_m3":0.6396,"pct_m3_per_voice":0.6396,"cal_ms":190.18,"ratio":0.033633,"minspec_pct_assumed_x1.5":0.959,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Quarter sync","voices":8,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.074283,"pct_m3":3.7141,"pct_m3_per_voice":0.4643,"cal_ms":190.18,"ratio":0.195299,"minspec_pct_assumed_x1.5":5.571,"slice_pct":34,"over_slice":false,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Quarter sync","voices":16,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.155442,"pct_m3":7.7721,"pct_m3_per_voice":0.4858,"cal_ms":190.18,"ratio":0.408676,"minspec_pct_assumed_x1.5":11.658,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"defaults","voices":1,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.040510,"pct_m3":2.0255,"pct_m3_per_voice":2.0255,"cal_ms":190.18,"ratio":0.106506,"minspec_pct_assumed_x1.5":3.038,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"defaults","voices":8,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.320926,"pct_m3":16.0463,"pct_m3_per_voice":2.0058,"cal_ms":190.18,"ratio":0.843758,"minspec_pct_assumed_x1.5":24.069,"slice_pct":34,"over_slice":false,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"defaults","voices":16,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.672257,"pct_m3":33.6129,"pct_m3_per_voice":2.1008,"cal_ms":190.18,"ratio":1.767453,"minspec_pct_assumed_x1.5":50.419,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Harmonic stack","voices":1,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.058638,"pct_m3":2.9319,"pct_m3_per_voice":2.9319,"cal_ms":190.18,"ratio":0.154166,"minspec_pct_assumed_x1.5":4.398,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Harmonic stack","voices":8,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.484361,"pct_m3":24.2180,"pct_m3_per_voice":3.0273,"cal_ms":190.18,"ratio":1.273449,"minspec_pct_assumed_x1.5":36.327,"slice_pct":34,"over_slice":true,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Harmonic stack","voices":16,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":1.011229,"pct_m3":50.5614,"pct_m3_per_voice":3.1601,"cal_ms":190.18,"ratio":2.658654,"minspec_pct_assumed_x1.5":75.842,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Fold over sync","voices":1,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.063870,"pct_m3":3.1935,"pct_m3_per_voice":3.1935,"cal_ms":190.18,"ratio":0.167922,"minspec_pct_assumed_x1.5":4.790,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Fold over sync","voices":8,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.509460,"pct_m3":25.4730,"pct_m3_per_voice":3.1841,"cal_ms":190.18,"ratio":1.339438,"minspec_pct_assumed_x1.5":38.210,"slice_pct":34,"over_slice":true,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Fold over sync","voices":16,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":1.059779,"pct_m3":52.9889,"pct_m3_per_voice":3.3118,"cal_ms":190.18,"ratio":2.786299,"minspec_pct_assumed_x1.5":79.483,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Crushed bells","voices":1,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.116616,"pct_m3":5.8308,"pct_m3_per_voice":5.8308,"cal_ms":190.18,"ratio":0.306598,"minspec_pct_assumed_x1.5":8.746,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Crushed bells","voices":8,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.954127,"pct_m3":47.7064,"pct_m3_per_voice":5.9633,"cal_ms":190.18,"ratio":2.508527,"minspec_pct_assumed_x1.5":71.560,"slice_pct":34,"over_slice":true,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Crushed bells","voices":16,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":1.961671,"pct_m3":98.0836,"pct_m3_per_voice":6.1302,"cal_ms":190.18,"ratio":5.157492,"minspec_pct_assumed_x1.5":147.125,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Breathing pad","voices":1,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.133297,"pct_m3":6.6648,"pct_m3_per_voice":6.6648,"cal_ms":190.18,"ratio":0.350454,"minspec_pct_assumed_x1.5":9.997,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Breathing pad","voices":8,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":1.061914,"pct_m3":53.0957,"pct_m3_per_voice":6.6370,"cal_ms":190.18,"ratio":2.791913,"minspec_pct_assumed_x1.5":79.644,"slice_pct":34,"over_slice":true,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Breathing pad","voices":16,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":2.147485,"pct_m3":107.3743,"pct_m3_per_voice":6.7109,"cal_ms":190.18,"ratio":5.646022,"minspec_pct_assumed_x1.5":161.061,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Glass horde pad","voices":1,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":0.174331,"pct_m3":8.7166,"pct_m3_per_voice":8.7166,"cal_ms":190.18,"ratio":0.458340,"minspec_pct_assumed_x1.5":13.075,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Glass horde pad","voices":8,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":1.446493,"pct_m3":72.3246,"pct_m3_per_voice":9.0406,"cal_ms":190.18,"ratio":3.803021,"minspec_pct_assumed_x1.5":108.487,"slice_pct":34,"over_slice":true,"load_lo":2.69,"load_hi":3.38}
LEDGER {"protocol":"B441-1","head":"deb4d1a","preset":"Glass horde pad","voices":16,"os":"preset","sr":48000,"block":128,"timed_s":2.0000,"best_s":2.946359,"pct_m3":147.3180,"pct_m3_per_voice":9.2074,"cal_ms":190.18,"ratio":7.746368,"minspec_pct_assumed_x1.5":220.977,"slice_pct":34,"over_slice":null,"load_lo":2.69,"load_hi":3.38}
```

## 3. Cost attribution: where the time goes inside a heavy voice

**Method 1: subtractive stage toggles (primary).** `h2/engine/engine.h` gained
`H2_ENGINE_STAGES`, a measurement-only define built into exactly one target,
`measure_h2_engine_stages`. Each bit of `stageOff` skips one stage. A stage's share is
(T_full − T_without) / T_full, on the protocol's cell with interleaved best of 5. In every
other build each site folds to `false` (`#define H2E_SKIP(bit) false`), the same pattern as
`H2_ENGINE_FAULTS`, so the shipped, parity and ledger builds compile the code they compiled
before. With no bit set, the stages build costs what the ledger build does (Glass horde
pad, one voice: 8.80 % against 8.72 %), so the toggles do not perturb the full figure.

The stages are:
- the swarm control tick (`tickSwarm`, per voice every 16 samples);
- the swarm's per-sample phase advance (φ_H);
- the blade evaluation (`out()`: both blades and their twins);
- every PolyBLEP (the `tryE` / `scan` edge search and the `hAt` probes);
- the DC estimate's refresh (`dcEst` / `dcPair`);
- the os-rate output biquads (the decimation filter);
- the DC blocker and the output `tanh`;
- `couple()` + `spread()` (per voice every 32 samples).

The **remainder** is the time with all eight skipped: the per-member-step frame (the
parameter overrides, `kq`, `fillG2`, the FM step, the modulator phase), the smoothers,
the envelopes, rotation and the pan mix. **Oversampling** is not a stage. It multiplies
the member loop, so it is measured as a configuration instead (os 1, nothing skipped).

**Caveats.** Skipping a stage changes what the rest computes. For example, without
the tick the member frequencies stay at the look-ahead's, and without the blade the
cross-mod ring reads zeros. It can also change inlining and register allocation. So
shares are **not** strictly additive: the check line shows how far they miss 100 %.

Run at `deb4d1a`: calibration 205.2 ms, load 2.73–3.38, best of 5, interleaved. Shares
are in % of the full render:

| stage | Glass horde pad, 1 voice | Glass horde pad, 8 voices | Crushed bells, 1 voice | Crushed bells, 8 voices |
|---|---|---|---|---|
| full render, % of an M3 core | 8.80 | 72.80 | 5.94 | 48.22 |
| swarm control tick (/16) | 0.5 | 0.8 | 0.7 | 0.6 |
| swarm phase advance (per sample) | −0.1 | 0.6 | −0.3 | 0.5 |
| **blade evaluation (`out`)** | **52.5** | **53.9** | **58.7** | **62.0** |
| **PolyBLEP (edge search + `hAt`)** | **10.8** | **12.0** | **13.5** | **15.3** |
| DC estimate refresh | 3.6 | 4.2 | 3.6 | 3.7 |
| os-rate output biquads | −0.2 | 0.1 | 0.7 | −0.1 |
| DC blocker + `tanh` | 0.2 | 0.2 | 0.8 | 0.1 |
| couple + spread (/32) | 5.7 | 5.8 | 2.3 | 1.5 |
| **remainder (all eight skipped)** | **25.9** | **25.5** | **25.5** | **22.5** |
| *sum (additivity check)* | *99.0* | *103.2* | *105.6* | *106.3* |
| *os 2 → 1, nothing skipped: saves* | *43.1* | *44.0* | *44.4* | *44.7* |

Shares within about ±1 point of zero are noise. Read them as "under 1 %".

**Method 2: `sample` (cross-check).** This is the macOS statistical profiler (`/usr/bin/sample`,
no Xcode needed). It ran for 12 s on `measure_h2_engine --hold "<preset>" 8 40`, the
shipped-flag build with no toggles. The figures below are inclusive shares of the
samples, counted once per stack. They are attributed to functions, so anything inlined
lands in its caller.

| function | Glass horde pad, 8 voices | Crushed bells, 8 voices | matching stage, by method 1 |
|---|---|---|---|
| `out` (the blade evaluation) | 43.7 % | 43.8 % | blade evaluation, 54 / 62 % |
| `scan` + `hAt` | 6.2 + 0.9 % | 6.0 + 0.8 % | PolyBLEP, 12 / 15 % |
| `dcEst` | 4.0 % | 3.7 % | DC estimate, 4.2 / 3.7 % |
| `spread` + `couple` | 5.6 + 0.2 % | 1.7 + 0.2 % | couple + spread, 5.8 / 1.5 % |
| `controlTick` | 0.8 % | 0.8 % | swarm tick, 0.8 / 0.6 % |
| `fmStep` | 3.3 % | 3.0 % | remainder |
| own bodies (self time) of `renderCall` + `stepMember` + `stepBlade` | 19.8 + 4.4 + 3.1 % | 18.2 + 8.3 + 5.6 % | remainder 25.5 / 22.5 %, plus the inlined `tryE` search |
| libm leaves (`sin`, `cos`, `atan2`, `__sincos_stret`, `log`, `pow`, `asin`, the unnamed libm kernels and their stubs) | ~27 % | ~26 % | inside every stage, mostly the blade |

**The two methods agree** on every stage that is a real function: the DC estimate,
couple + spread, and the tick. They disagree by 8–18 points on the blade evaluation and
by 5–8 points on PolyBLEP. The ablation reads higher. A hypothesis: removing a call from
`stepBlade` also frees registers and inlining budget for the code around it, and that
saving is credited to the removed stage.

**The op count (untimed; the engine's own event counters).** It covers one voice for
2.25 s at os 2:
- **Glass horde pad**: 10,615 BLEP edges in 1,944,000 member-steps.
- **Crushed bells**: 7,068 BLEP edges in 1,296,000 member-steps.

That is **0.0055 edges per member-step** in both, and no `scan` (carrier) edges, because
neither preset has a scannable carrier. Each edge costs two full blade evaluations in
`hAt`, so the edges themselves are ~1 % of the time, which `hAt`'s 0.8–0.9 % matches. The
other 10–14 points of the PolyBLEP share are the **edge search on the 99.5 % of steps that
have no edge**: four `tryE` calls, and two `scan` calls that return at once for these blades.

## 4. Interpretation (hypotheses for the audit, B441 phase 1(B); not fixes)

**The top three cost centres for heavy patches:**

1. **The blade evaluation, `out()`: 44–62 % of the render.** Most of it is double-precision
   libm: the libm leaves alone are ~26–27 % of all samples. Per step, the work is:
   - the FM modulator's `sin` (`mod()`);
   - the carrier's `sin` (`wave(hot 0)`);
   - the sine→saw band-limit's `atan2` + `sin` + `cos` (hot 6);
   - crush's antiderivative `cos` pairs (`crushAvg`, `blade.h:146`).

   **A NEW candidate:** `wave(base, phi)` is evaluated for the same phase up to three
   times per step: blade 1's `voicePlain` (`engine.h:374`), blade 2's `voicePlain`, and
   `out()`'s `- wave(g.base, phi)` (`engine.h:486`). With base 0 that is three `sin` calls
   of one argument (Crushed bells). Hoisting it is output-neutral, because it is the same
   pure function of the same double.
2. **The per-member-step frame (the remainder): 22–26 %.** Every oversampled step
   re-derives per-member parameters that change at most once per sample or once per 32
   samples (`engine.h:1563-1599`: the `s.w` / `s.depth` / `s.I` overrides, `kCap`, `kq`, a
   `fillG2` copy, the `mEff` division). It also calls helpers that return at once for the
   patch's modes, but are not inlined: `fmStep` costs 3.0–3.3 % by sampling, for a
   `fmType != 1` early return (`engine.h:358`).
3. **The PolyBLEP edge search: 11–15 %** (by ablation; `scan` alone is 6 % by sampling). It
   is almost all search, not correction (the op count above): the per-step `tryE` / `scan`
   dispatch on the 99.5 % of steps with no edge.

**Across all three, oversampling is a multiplier, not a stage.** Going from os 2 to os 1
saves 43–45 % on both heavy presets, because the member loop runs once per output sample
instead of twice. The decimation filter itself costs under 1 %. This lever changes the
sound (aliasing; B378 F3's BLEP-table route), so it is a ledgered divergence or a quality
mode (B441 phase 3), never output-neutral.

**What is NOT a cost centre in `h2/engine` (measured):**
- the swarm control tick, 0.5–0.8 %;
- the swarm phase advance, under 1 %;
- the decimation biquads and the output stage, under 1 % each.

Checkpoint 4 attributed the engine's 13–25 % over the blade port to "the swarm's
control tick … and the per-sample swarm phase advance". By this attribution, those two
are under 2 % together. The overhead must be elsewhere: `couple` + `spread` (1.5–5.8 %),
the composed frame, and the second blade. That is a hypothesis for the audit to settle.

**B378's findings mapped onto `h2/engine`:**

| finding | what it said (swarm core) | in `h2/engine` | maps onto |
|---|---|---|---|
| **F1** | a fat member loop with 12 feature branches; a specialised plain kernel is bit-identical and 2.0–2.3× faster | The same shape, larger. `stepBlade` / `out` / `voicePlain` switch per step on `mode`, `hot`, `base`, `b2on`, `mirror`, `aa`, `b2mix`, and call non-inlined helpers that return at once (`fmStep`, `scan`). Kernels specialised on the discrete parameters (`d.*`, which change only between `render()` calls) are the direct analogue. | **cost centres 2 and 3, and the dispatch part of 1.** The largest output-neutral lever, if the expressions and their order are kept. |
| **F7** | the per-voice 2× decimator is scalar: 37 ns per channel-sample; vectorised, 14 | There is no per-voice decimator. Two biquads per channel run once on the bus at the os rate (`engine.h:1628`), which is already B378's "decimate once on the bus". Measured ≤ 0.7 %. | **nothing material.** F7 does not carry over. |
| **F9** | the Kuramoto machinery (sincos and sin per member per tick, atan2 per voice, constant exps) runs at K = 0 | `SwarmField::controlTick` (`swarm.h:136`) carries the same law, but the whole tick measures 0.5–0.8 %. (Both heavy presets have K ≠ 0 anyway: 0.35 and 0.3.) | **nothing material:** at most ~0.8 % to win. |
| **F11** | a float32 kernel with a double phase is 1.4–1.56× faster; max-abs 2.1e-6 fails a 1e-6 bound | Everything is double, and the blade's transcendentals are double libm (~26 % of samples). Float kernels with `sinf` / `atan2f`, or polynomial approximations with stated error bounds, act on cost centre 1. | **cost centre 1.** A divergence: it needs a ruling (ADR-187 §5) and a quality metric. |
| **F12** | string-keyed `setParam`: a 70-branch chain, up to 208 ns per event | `Engine::set` walks two `strcmp` tables (64 target keys, ~70 discrete keys) per event. The held-chord protocol sends no events while timed, so the ledger cannot see it. | **none in steady state.** It needs an automation cell (open question 3). |

**New candidates for phase 1(B)**, from this attribution and not from B378:
- **The redundant `wave(base, phi)` evaluations** (cost centre 1). Output-neutral.
- **The DC estimate's refresh: 3.6–4.2 %.** It is numeric integration over up to 1024
  points (`dcEst`), every 256 samples, with `dcWait` spacing. Moving it off the
  member-step path, or caching it while its inputs stand still, is a hypothesis to test.
- **`spread()`'s per-32-sample work: 5.7 % on Glass horde pad**, whose `law` 4 draws 14
  Gaussians per member every 32 samples (`engine.h:686`: a `log`, a `sqrt` and a `cos`
  each).

## 5. Parity: the instrumentation changes nothing

`H2_ENGINE_STAGES` is defined in exactly one target (`CMakeLists.txt`,
`measure_h2_engine_stages`). Everywhere else, each of the eight sites compiles to the
expression it replaced: `!false &&` and `false ? 0 :` fold away, and a skipped block is
`if (false)`. `tools/h2_engine_parity_check` was built and run on the normal build with
this PR's `engine.h`. It was GREEN: 540 of 543 scenarios at parity, the 3 chaotic rows
held, and 0 red. The mean bit-exact share was 37.00 %, the figure `h2/README.md`
already records, so no sample moved (`traces/2026-10-04-b441-cpu-ledger.md`).

## Open questions

1. **48 kHz or 44.1 kHz.** The brief froze 48 kHz. B439's E-6 anchor is 44.1 kHz at a
   128-sample buffer, which costs ~8.8 % less per second of audio. Should B441-2 move to
   44.1 kHz to match the anchor exactly? The verdicts above do not change (the closest
   over-slice row, Harmonic stack at 36.3 %, would read ~33.4 %, under the slice).
2. **What "16 voices" means.** The engine's pool is 8. The 16-voice rows are two engines.
   If 1.0 offers 16 voices, its shape (a bigger pool, or two instances) is a design decision.
3. **An automation cell** (F12, and the smoothers under motion) needs a protocol row of its
   own, for example a parameter sweep at block rate. It is not in B441-1.
4. **The ×1.5 min-spec factor** stays an assumption until it is measured once on an M1 base
   or on the Intel min-spec. Every min-spec figure above inherits it.
