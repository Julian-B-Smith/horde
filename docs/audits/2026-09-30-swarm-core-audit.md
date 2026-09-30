# B378: Swarm Core 1 audit (efficiency, clarity, fidelity)

- **Origin:** horde lead session, 2026-09-30. ROADMAP B378 (records branch `lead-records-150`, local; the branch is not on origin).
- **Reviewer:** critic (Opus 5.5). This is the same lineage as the core's authors (Fable 5 / Opus 4.8–5), so every finding is tagged MEASURED or JUDGEMENT.
- **Subject:** `src/swarm_core.h` (2,162 lines), `src/force_core.h`, `src/glide_core.h` at `bae777f` (origin/main).
- **Worktree:** detached, in scratch. No tracked file was edited.
- **Machine and build:** Apple M3 (8 cores), Apple clang, `-O3` Release. Load average 3.5–4.9 during the runs. The calibration loop (1e8 dependent multiply-adds) took 99–122 ms. Use ratios, not absolutes.
- **Scratch harnesses:** everything is under `scratchpad/b378/h/`: `bench`, `tcount`+`libcount.dylib`, `proto`, `proto2`, `dec`, `probes`, `nyq`, `zip2`, `arender`, `blepsyn`, `alias.mjs`, `alias2.mjs`, and the patched copy `fastp/`.

## 0. Headline

| # | Finding | Axis | Impact | Kind | h2 copy |
|---|---|---|---|---|---|
| F1 | One fat member loop with 12 feature branches. A specialised plain kernel is **bit-identical on 10/10 hashed patches and 2.0–2.3x faster**. | efficiency | HIGH | MEASURED | yes (output-neutral, ADR-187 §5) |
| F2 | **Members above Nyquist are rendered, not muted.** Shell-reachable patches put up to **+0.29 DC** on the output. | fidelity | HIGH | MEASURED | yes (divergence, mirror to lab) |
| F3 | 2-point polyBLEP at 1x aliases **−48.6 dB (C2) to −26.8 dB (C8)**. A 16-sample BLEP table measures **≤ −120 dB** at 1x. | fidelity | HIGH | MEASURED | yes (divergence) |
| F4 | The saw-shape anchors (`sawBase`) are **unband-limited**: they read like a naive saw (−28 to −9 dB). | fidelity | HIGH (if kept) | MEASURED | cull (B376) or band-limit |
| F5 | **B341 confirmed:** 2x oversampling halves every onset wait, sample-exact. | fidelity | MEDIUM | MEASURED | yes (ledgered) |
| F6 | **Voice-reuse history leak:** the decimator and ITD rings are not cleared at note-on. A reused slot's onset carries the previous note (max 9.1e-3). | fidelity | MEDIUM | MEASURED | yes |
| F7 | The per-voice 2x decimator is scalar and unvectorised: 37 ns per channel-sample. A mirrored, symmetric version runs at 14 ns. | efficiency | MEDIUM | MEASURED | yes, unless F3 retires OS |
| F8 | No smoothing on vol, normExp, width, panCurve, shape, …: **−43 to −51 dB zipper sidebands** under block-rate automation. A 5 ms smoother measures 22 dB lower. | fidelity | MEDIUM | MEASURED | yes |
| F9 | The Kuramoto machinery (sincos + sin per member per tick, atan2 per voice) runs at K = 0, plus three per-tick exps that are constant. | efficiency | MEDIUM | MEASURED (counts) | yes (bit-identical guard) |
| F10 | The saw-shape tables are 1.31 MB of doubles. With `sawBase` + `round` engaged, the per-member cost doubles. | efficiency | MEDIUM (if kept) | MEASURED | cull or shrink |
| F11 | A float32 kernel with a double phase is 1.4–1.56x faster. RMS 3–7e-8 passes the RMS bar; max-abs 2.1e-6 fails a 1e-6 max-abs bound. | efficiency | LOW–MED | MEASURED | after a ruling |
| F12 | String-keyed `setParam` is a 70-branch linear chain: up to 208 ns per event. | efficiency | LOW | MEASURED | yes (manifest ids) |
| F13 | ITD rings are 32 KB per voice (512 KB per core) and allocated unconditionally. The anchor tables are another 1.31 MB static. | memory | LOW | MEASURED | yes |
| F14 | No FTZ/DAZ anywhere in the core or shell. Internal subnormals are reachable only after holds of about 7 min. | robustness | LOW | code fact + MEASURED | yes (shell) |
| F15 | The output pole is rate-dependent: −0.62 dB at 10 kHz at 44.1 k vs −1.13 dB at 192 k (ADR-177 ruled that it stays). | fidelity | LOW | MEASURED | ruling |
| F16 | With 2x OS on, the 1x output `tanh` sets the alias floor at shipped vol (+2.4 to +4.5 dB). | fidelity | LOW | MEASURED | with F3 |
| C1–C5 | Clarity: structure, dead code, 6 stale or wrong comments, duplication, the ADR trail (§3). | clarity | LOW–MED | code fact | yes |

**What is healthy (MEASURED):**
- JS parity: 156/156 within 1e-6, of which 113 are bit-exact; the worst is 4.26e-9 (`dyn-ring.seed42`).
- Bit-determinism holds, and so does block-size independence (blocks 1 to 4096, at 44.1 k and 96 k, with every feature on plus 2x OS).
- The seed reaches the output.
- 160 extreme combinations (4 rates × 4 notes × 10 patches) gave 0 non-finite samples and a peak ≤ 0.76.
- The prior audit's criticals are closed: A1 by B150, A2, A3 by B149, A4, and A8. A6's inertia spread is now 0.76% by `sr_check`; my own probe reads 0.3%. The shipped shell costs the same as the core-direct measurement: 6.20% vs 6.33% at 8 notes × n 32.

**Not measurable as briefed (stated, not guessed):**
- The core has **no 4x oversampling**. `oversample` is 0/1, i.e. 2x only (`swarm_core.h:268-270`, `:1003`).
- The **force system is not a swarm-core dynamics mode**. `swarm_core.h` uses only `forcecore::rngNext` and `onePoleCoef` (`:1372`, `:946-948`); the rest of `force_core.h` is the Track E effects' spring system (ADR-034).
- The **swarmalator is a separate core** (`swarmalator_core.h`), so every run here is "swarmalator-free" by construction.
- **Heavy-hitter per-voice CPU:** no web access in this session, and I will not quote numbers from memory (§4).

---

## 1. Efficiency

### 1.1 CPU per voice across N × OS × dynamics mode (MEASURED)

Core-direct, 44.1 kHz, 128-sample blocks, 1 s of audio after a 0.3 s warm-up, minimum of 3 runs. "Per voice" is 8 held notes (keys 48+3k) ÷ 8. "ns per member-sample" is total ÷ (8·N) per output sample.

| mode | os | N=1 | N=5 | N=9 | N=16 | N=32 | ns/member-sample at N=32 |
|---|---|---|---|---|---|---|---|
| plain (defaults) | 1x | 0.059% | 0.156% | 0.246% | 0.424% | 0.792% | 5.61 |
| plain | 2x | 0.353% | 0.639% | 0.808% | 1.123% | 1.793% | 12.71 |
| K 0.6 (coupling) | 1x | 0.061% | 0.157% | 0.247% | 0.422% | 0.789% | 5.59 |
| drift 20 ct | 1x | 0.062% | 0.165% | 0.264% | 0.448% | 0.834% | 5.91 |
| gravity 0.5 (8-note chord) | 1x | 0.067% | 0.163% | 0.257% | 0.430% | 0.799% | 5.66 |
| ring topology, K 0.6 | 1x | 0.044% | 0.165% | 0.304% | 0.563% | 1.037% | 7.35 |
| two-cluster, K 0.6 | 1x | 0.051% | 0.175% | 0.272% | 0.462% | 0.851% | 6.03 |
| inertia 0.7 + K 0.6 | 1x | 0.062% | 0.159% | 0.251% | 0.430% | 0.795% | 5.63 |
| sawBase .3 + round .5 | 1x | 0.104% | 0.287% | 0.471% | 0.796% | 1.544% | 10.94 |
| shape 0.5 | 1x | 0.064% | 0.170% | 0.278% | 0.475% | 0.893% | 6.33 |
| tilt 0.4 | 1x | 0.063% | 0.162% | 0.261% | 0.443% | 0.823% | 5.83 |
| width 1.3 (ITD) | 1x | 0.060% | 0.168% | 0.259% | 0.433% | 0.813% | 5.76 |
| voiceEnv + onset 5 ms | 1x | 0.058% | 0.154% | 0.233% | 0.393% | 0.730% | 5.17 |
| everything | 1x | 0.124% | 0.327% | 0.560% | 0.938% | 1.791% | 12.69 |
| everything | 2x | 0.567% | 0.976% | 1.385% | 2.109% | 3.755% | 26.61 |

The full 13-mode × 2-OS grid is in `h/cpu_grid.tsv`.

- **Polyphony is linear** at N 7: 0.216 / 0.205 / 0.203 / 0.209% per voice at 1 / 4 / 8 / 16 notes.
- **Sample rate is linear** as well: 8 notes at N 7 cost 1.62% at 44.1 k, 1.78% at 48 k, 3.52% at 96 k and 6.95% at 192 k.
- **Shell cross-check:** the shipped shell (`measure_cpu`, CLAP factory) gives 0.53 / 1.82 / 3.37 / 6.20% at n 1 / 8 / 16 / 32, which agrees with core-direct.

**Where horde stands, in player terms:**
- 16 notes × 7 members at 1x costs 3.3% of one M3 performance core.
- The same at 2x costs about 11%.
- 8 notes × 32 members with every feature on at 2x costs 30%.
- The `kPoly` comment calls 16 voices × 32 members "trivial CPU" (`:51-53`). Measured, it is **12.7% at 1x and 28.7% at 2x**.

### 1.2 F1: the member loop is structurally slow, and the fix is bit-identical (MEASURED)

**The gap.**
- The isolated kernel (saw + polyBLEP + equal-power pan + stereo sum, the core's own expressions, `h/proto.cpp`) costs **1.5–1.7 ns per member-sample** at N 32 in scalar double.
- The full core costs 5.6 ns.
- A `sample(1)` profile puts about 75% of time in `renderSeg`'s member loop (`swarm_core.h:1048-1155`) and about 16% in `controlTick`.
- Hoisting the `p.*` reads out of the loop gains nothing (measured), so this is not aliasing.
- The loop body carries 12 feature branches: onset, glide, digital, sawBase, rnd, shape, tilt, hiTame, per-voice ADSR, ensemble, mono and ITD. That body defeats scheduling.

**The experiment.** `h/patch_fast.py` → `h/fastp/swarm_core.h` adds one specialised branch: when none of those features is live, run the lean kernel with the same expressions in the same order. Result, 8 notes, FNV-1a of 1.5 s of audio with a mid-render note-off:

| patch | hash (both) | original | fast path | speed-up |
|---|---|---|---|---|
| plain n 7 | 71fc5aeab3e48871 | 1.616% | 0.831% | 1.94x |
| plain n 32 | 0a2316a0db7bd7b3 | 6.169% | 2.734% | 2.26x |
| K 0.6 n 16 | 0c7487d5b8e3ac2a | 3.341% | 1.508% | 2.22x |
| os2 n 9 | 6be72d06e2a1bae8 | 6.512% | 3.380% | 1.93x |
| digital 0.5 | 1211fc4aa0ae2e34 | 1.633% | 0.838% | 1.95x |
| shape, tilt, hiTame, width, mono, voiceEnv, everything | unchanged | — | — | 1.0x (not on the fast path) |

The ns per member-sample at N 32 falls from 5.61 to **2.36** at 1x, and from 12.71 to 5.66 at 2x.

- **Fix (h2):** specialise the member kernel on the active feature set. Use `template<bool…>` kernels, or a small fixed set selected once per segment, and never branch per member.
- **Expected gain:** 2x on every patch, once each feature combination has its own kernel. JUDGEMENT: the feature-on patches should gain similarly, because the waste is the same branch soup.
- **Fidelity risk:** none. It is bit-identical by digest, and it is output-neutral work under ADR-187 §5.

### 1.3 Transcendentals per member-sample (MEASURED by interposed libm counters, `h/count.c`)

Plain patch, N 7, one note, 1x:

| call | per member-sample | where |
|---|---|---|
| `__sincos_stret` | 0.070 | order parameter R, every tick (`:1912-1917`); RN on the segment's last tick (`:1927-1934`) |
| `sin` | 0.0625 | the coupling term, every tick (`:1989`), **even at K = 0** |
| `atan2` | 0.009 | ψ, per voice per tick (`:1921`) |
| `exp` | 0.030 | pressure smoother, Kenv decay and output-pole coefficient, per voice per tick (`:1701`, `:1740`, `:2090`); all constant at defaults |
| `pow` | 0.001 | law-0 cache misses only (B148/O3 works) |
| `tanh` | 2 per output sample per core | output stage (`:1331-1332`); not per member |

- Per-sample member work calls **no** transcendental: the saw, BLEP and pan are arithmetic.
- By feature: drift adds `exp2` 1/16; tilt adds `exp` 1/16 plus `pow` 1/100; hiTame adds `pow` 1/16; two-cluster doubles sincos and adds `hypot`.
- The architecture is already control-rate-heavy by design, which is good.

**F9 (h2, bit-identical):**
- When `KsmS == 0 && KsmP <= 0.001`, `couple[i]` is exactly ±0, and `vf + (−0) == vf`. So skip the sin per member.
- Compute R/ψ only when something reads them: rtone ≠ 0, the viz on the last tick, or K ≠ 0.
- Hoist the three constant exps to parameter-change time.
- **Expected gain:** 15–25% of what remains after F1 on K = 0 patches. That is an ESTIMATE, from `controlTick` ≈ 16% of the pre-F1 profile, of which sincos + sin is the majority.
- **Risk:** none (exact).

### 1.4 Branches, float/double, denormals, layout, vectorisation (code facts + MEASURED)

**Branches in inner loops.**
- The member loop (`:1048`) has 12 data-independent feature branches, plus the 2-way BLEP branch, which is data-dependent (F1).
- The sample loop has `if (tick == 0) controlTick(...)` inside it (`:1036`). A 400-line call inside the hot loop costs register pressure.

**Float/double mix.**
- All state and arithmetic are double. That is parity-driven (`:8-11`).
- The exceptions are the output buffers, which get a float store per voice per sample (`:1239-1240`, the Float32Array `+=` rule), and `itdRing`, which is **float** (`:363`, `:1150`). The header's "All state is double" (`:8`) is therefore not true.
- The ITD far channel is float-rounded while the near channel is double. JUDGEMENT: harmless, but an asymmetry.

**Denormal policy (F14).**
- There is no FTZ/DAZ in the core or the shell: grep finds 0 hits in `src/hypersaw_clap.cpp`.
- The explicit mitigations are the B156 `apZ` snap and the voice cull zeroing `env/lpL/lpR` (`:1017-1024`). `denormal_check` is GREEN.
- Tick-rate state decays geometrically without a floor. After K 0.6 → 0 with onset 0.5, KsmS/Kenv go 7e-5 → 7e-9 → 7e-13 over 5.8 s steps, reaching subnormal only after about 440 s held. The per-tick cost is negligible.
- M3 cannot show the penalty (the prior audit's control reads ×1.01). On x86 VST3 via clap-wrapper, the flush mode is whatever the host set.
- **Fix:** the h2 shell sets FTZ/DAZ, scoped to `process()`. Zero cost. Output-neutral except for subnormal values, which is within ε.

**Layout.**
- Array-of-structs by voice with struct-of-arrays inside: each `Voice` holds `phase[32]`, `eff[32]`, …
- `sizeof(Voice)` = 39,360 B, of which 32,768 B is `itdRing` (F13). `sizeof(SwarmCore)` = 667,456 B. The anchor tables are 1,310,800 B static.

**Auto-vectorisation** (`-Rpass=loop-vectorize`, `h/vec.txt`):
- 28 loops in `swarm_core.h` vectorise, all of them cold: rebuild, noteOn, zero-fills and the glide pre-pass.
- The **member loop does not** (`:1048`: "could not determine number of loop iterations"; "value that could not be identified as reduction"). The halfband does not ("cost-model indicates … not beneficial", because of the `& 63` gather). The `tanh` loop does not (it is a call).

**SIMD opportunities, each MEASURED in isolation:**

| opportunity | gain | fidelity risk | h2 |
|---|---|---|---|
| SoA double kernel, reduction kept sequential | ×0.82–1.06: **none**. Division, floor and 2-lane NEON give nothing. Bit-identical (0 differing samples over 44,100). | none | no value |
| float32 kernel, double phase (`h/proto2.cpp`) | **×1.40–1.56** on the kernel | RMS 3.1–7.0e-8 (passes 1e-6); max-abs 5.4e-7 to **2.1e-6** (fails a 1e-6 max-abs bound, B332 Q3). Phase stays double, so there is no chaotic feedback of the error. | after JS demotion or a ruling (F11) |
| decimator: mirrored buffer + symmetric folding (`h/dec.cpp`) | **×2.67** (37.3 → 14.0 ns per channel-sample) | 5.1e-10 max. The residue is not reassociation: `kPiRef` makes the "symmetric" Blackman window asymmetric by about 1e-9. | yes (F7) |
| decimate once on the bus, with env/pole run at 2x per voice | removes about 15/16 of decimator cost at 16 voices (ESTIMATE) | a divergence: env/pole at 2x | if OS survives F3 |
| tables instead of sincos in `controlTick` | ESTIMATE 10–20% after F1 | a divergence in chaotic regimes; ledger it | JUDGEMENT: later |

**F12.** `setParam` walks `paramSlot`'s 70-branch `std::string` compare chain on the audio thread (`:1393-1470`). Per event it costs 6 ns ("n"), 23 ("K"), 58 ("vol"), 129 ("toneTilt") and 203–208 ns (late keys or unknown). A block of 128 events costs about 26 µs, about 0.9% of a 128-sample budget. In h2, generated manifest ids index an array.

---

## 2. Fidelity

### 2.1 Aliasing across the keyboard: B346's `aliasConvergence` on the C++ core (MEASURED)

**Method (`h/arender.cpp`, `h/alias.mjs`):**
- The core is sample-rate-independent in seconds (ADR-009, `sr_check` GREEN). So "os M" is emulated by rendering the same patch at 44.1 k × M (M = 1, 2, 4, 8, 16) and decimating to 44.1 k. The decimator is a Kaiser FIR, pass 20 kHz, stop 24.1 kHz, 120 dB.
- The 8x pre-decimation stream feeds the source test. Each render's output pole is divided out through `gainOf`.
- The core's own 2x path (`oversample 1`) is scored with `excessJoint` against {8x, 16x}, with the core's halfband response recomputed from its constructor.
- **Controls:** an additive band-limited saw reads **−120 (clean)** at every note. A synthetic copy of the core's polyBLEP formula (`h/blepsyn.cpp`, mode 2) reproduces the core's figures within 1.2 dB.
- **Scope:** K 0, one note. Coupled patches are not the same realisation across rates (the tick is 16 samples), so the estimator's "dynamics" class would muddy them.

Excess at 1x (dB; f = folding, c = clean) / the core's 2x path:

| patch | C2 (36) | C3 | C4 | C5 | C6 | C7 | C8 (108) |
|---|---|---|---|---|---|---|---|
| control: additive saw | −120c | −120c | −120c | −120c | −120c | −120c | −120c |
| n1 naive (digital 0), vol .05 | −29.3f / −36.5 | −26.3f / −32.5 | −23.2f / −29.7 | −20.3f / −26.5 | −17.4f / −23.4 | −13.7f / −20.2 | −10.3f / −16.8 |
| **n1 polyBLEP, vol .05** | −48.6f / −68.9 | −45.6f / −66.3 | −41.6f / −63.5 | −38.6f / −60.3 | −35.8f / −57.3 | −31.8f / −53.6 | −26.8f / −51.1 |
| n1 polyBLEP, vol .4 (shipped) | −48.2f / −66.5 | −45.2f / −63.8 | −41.2f / −59.0 | −38.2f / −56.1 | −35.4f / −53.4 | −31.5f / −50.3 | −26.6f / −48.2 |
| n7 default supersaw, vol .4 | −60.6c / −84.8 | −48.5f / −73.3 | −43.2f / −66.2 | −39.3f / −61.6 | −35.6f / −57.5 | −32.3f / −53.9 | −26.7f / −50.4 |
| **n1 sawBase .25 (anchor "curved")** | −28.3f / −35.5 | −25.3f / −31.5 | −22.2f / −28.6 | −19.2f / −25.5 | −16.3f / −22.4 | −12.6f / −19.0 | −9.2f / −15.6 |
| n1 round .6, profile .5 | −53.8f / −74.0 | −50.8f / −71.4 | −46.7f / −68.5 | −43.8f / −65.0 | −40.8f / −60.8 | −36.2f / −54.4 | −30.1f / −46.4 |
| n1 shape .5 (square morph) | −50.0f / −70.5 | −46.6f / −67.8 | −42.9f / −64.9 | −39.9f / −61.6 | −38.0f / −59.2 | −34.7f / −54.6 | −31.4f / −51.2 |

**The measured target for a fix** (same estimator, synthetic saw, `h/alias2.mjs`):

| edge treatment at 1x | C2 | C4 | C6 | C8 |
|---|---|---|---|---|
| 2-point polyBLEP (the core's formula) | −47.4f | −40.4f | −34.7f | −25.8f |
| BLEP table, half-width 4, cutoff 92% | −51.8f | −44.8f | −39.7f | −30.0f |
| BLEP table, half-width 8, cutoff 92% | −65.1c | −57.6f | −54.5f | −42.7f |
| **BLEP table, half-width 16, cutoff 80%** | **−120c** | **−120c** | **−120c** | **−120c** |

**F3.**
- **Evidence:** the swarm's saw is **folding at every note from C3 up**, from −45 to −27 dB.
- **Fix:** replace the 2-point polyBLEP (`:1060-1066`, `:1106-1113`) with a BLEP-table edge: a windowed-sinc residual of 16 samples per side, or minBLEP. Then decide whether the 2x path is still needed.
- **Expected gain:** about 80 dB at C4 at 1x, cleaner than today's 2x.
- **Cost (ESTIMATE):** about 32 MACs per edge per channel, and edges per second equal f per member, so the cost **scales with pitch**. Against the ~2.4 ns/member-sample F1 kernel (~105 µs per member-second), it adds roughly +2% at C2, +8% at C4, +30% at C6 and +130% at C8 in scalar code. Accumulating the residual into a per-voice L/R correction ring (pan gains are constant per tick) and vectorising the 32-tap add cuts that by about 4x. Even at C8 it is cheaper than today's 2x path (+120–500% per voice, §1.1).
- **Fidelity risk:**
  - it rolls off partials above 80% of Nyquist (17.6 kHz at 44.1 k, 19.2 kHz at 48 k);
  - the linear-phase version adds 16 samples of latency and pre-ringing, and minBLEP changes the edge phase instead;
  - it is a timbre divergence from the JS, but mirrorable (the table is cheap in JS). One divergence PR.

The earlier caveat about my BLEP-table run is resolved. The first run read −38 dB "dynamics" at C6+ because my residual table's interpolation noise (OS 512) floored it at −86 dB per bin. At OS 16384 it reads clean. That is my harness, not the method.

**F4.**
- **Evidence:** `sawBase` anchors 1–4 are tables of functions with a jump at the wrap (`sawBaseAnchor`, `:79-87`). `renderSeg` blends them in after the BLEP (`:1082-1087`), so at `sawBase` 0.25 (anchor 1, weight 1) the BLEP'd saw is fully replaced by an unband-limited one. The measurement agrees: within 1 dB of the naive saw at every note.
- The profile bank (`round`) is continuous at the wrap and reads slightly *better* than the plain saw.
- **Fix:** B376 already recommends CULL for the saw-shape family. If it survives, BLEP the anchor's wrap jump (known per anchor), or use mipmapped band-limited tables.

**F16.** The output `tanh` runs at 1x after the decimator.
- At 1x it adds ≤ 0.4 dB (vol .05 vs .4 rows), which confirms the prior audit §1.4.
- With 2x on, it becomes the floor: n1 at C4 reads −59.0 at vol .4 against −63.5 at vol .05.
- If F3 lands and OS is retired, this is moot. Otherwise run the `tanh` at 2x, or use ADAA on it (B346 language).

### 2.2 F2: members above Nyquist (MEASURED, `h/nyq.cpp`, `h/probes ext`)

- There is no per-member Nyquist guard: `dph = max(0, f) / (sr·os)` has no upper bound (`:1054`).
- With `dph > 0.5` the polyBLEP window overlaps itself and the member emits aliased garbage plus DC.
- Shell-legal patches (`kParams`: law 0–5, detune 0–1, harmReach 0.25–4, n 1–32):

| patch (shell-legal) | sr | note | members > Nyquist | output DC |
|---|---|---|---|---|
| law 4 harmonic, detune 1, harmReach 1 (natural series), n 32 | 44.1 k | 93 (A6) | 20/32 | **+0.068** |
| law 4, detune 1, harmReach 4, n 32 | 44.1 k | 69 (A4) | 19/32 | +0.062 |
| same | 44.1 k | 81 | 25/32 | +0.180 |
| same | 44.1 k | 93 | 29/32 | **+0.289** |
| same | 96 k | 93 | 25/32 | +0.167 |
| law 0, detune 1, spread 24, n 32 | 44.1 k | 127 | 8/32 | +0.015 |

- **Fix (h2):** a per-member gain that fades to 0 between about 0.40 and 0.48 × sr (os-aware), set in `controlTick` from `vf`. Optionally skip the member entirely above the fade.
- **Gain:** removes the garbage and DC, and saves CPU on those members.
- **Risk:** timbre divergence at the top register. JUDGEMENT: the JS lab almost certainly shares the defect; mirror it.

### 2.3 Smoothing and zipper under fast automation (MEASURED, `h/zip2.cpp`)

**Method:**
- Sine members (`round 1`, `sawProfile .75`, i.e. the pure anchor) at 55 Hz, vol low so the `tanh` stays linear.
- Each parameter is swept over 100 ms, delivered as a host-style 128-sample staircase, and compared to a per-sample ramp.
- The difference is high-passed at 250 Hz (8th-order), so only block-rate sidebands count, not lag.
- The "smoothed" column feeds the same staircase through a 5 ms one-pole first.

| param | sweep | staircase: sideband energy re signal | through a 5 ms smoother |
|---|---|---|---|
| vol | .05→.15 | **−45.1 dB** | −67.3 |
| normExp | .75→.30 | **−43.1** | −65.5 |
| width | .2→1.0 (rebuild per event) | **−46.7** | −69.2 |
| panCurve | .2→.9 | −50.6 | −72.8 |
| toneTilt | 0→.8 (per tick) | −56.7 | −62.0 |
| hiTame | 0→1 (per tick) | −63.6 | −83.4 |
| K (core-smoothed, `ksmC`) | 0→.8 | −126.0 | −141.1 |
| detune (tick, phase-continuous) | .1→.5 | −99.1 | −117.5 |

Per-sample stepped parameters with no smoothing in the core: vol and normExp (`gain`, `:943`), shape (`:1114`), digital (`:1066`), sawBase and round (`:1082-1095`), and width and pan via `rebuild` (`:1578-1660`). The shell forwards raw values (`src/hypersaw_clap.cpp:7923-7926`).

- **F8 fix (h2):** smoothing is a property of the parameter's manifest rate class (B275). Use a one-pole for gain-type parameters, and smooth the per-member pan gains for width/pan.
- **Gain:** about 22 dB less zipper.
- **Risk:** small lag. A divergence; mirror it.

### 2.4 Numerical stability at extremes (MEASURED, `h/probes ext`)

- **Grid:** 160 combinations, {44.1, 48, 96, 192} kHz × MIDI {0, 21, 108, 127} × 10 patches. The patches were K ±1 at n 32, detune 1 × spread 24, law 1 linear at the extremes, inertia 1 + K 1, onset −1 with dissolve 0, ring topology at reach 99, two-cluster balance 1 with K −1, and law 4 with harmReach 8.
- **Result:** 0 non-finite samples; peak ≤ 0.76 (tanh-bounded); no frozen members (eff ≤ 0).
- The only anomalies are the Nyquist rows (F2).
- `sr_check` GREEN: onset-lock drift 0.54%, inertia spread 0.76%, pole drift 0.40 dB.
- My inertia probe (K 0.6 + inertia 0.7, R over 10–12 s): 0.3186 / 0.3195 / 0.3190 / 0.3193 / 0.3192 at 44.1 / 48 / 88.2 / 96 / 192 k. The prior audit's 15% spread (A6) is gone.

### 2.5 Denormals in release tails (MEASURED)

- `denormal_check` GREEN: `apZ` snaps to exactly 0 within its derived bound.
- The voice cull zeroes `env/lpL/lpR` and the onset state.
- Tick-state subnormals are reachable only on holds of more than ~7 min (§1.4).
- The x86 residue is F14.

### 2.6 Determinism (MEASURED, `h/probes det`)

- Patch: 16 members with K, drift, gravity, tilt, shape, sawBase, round, width 1.3, voiceEnv, onset scatter, hiTame, pan motion, inertia and 2x OS; 4 notes.
- Two fresh cores produce identical FNV-1a at block sizes 1, 64, 128, 1000 and 4096, at 44.1 k and at 96 k.
- Seed 1234 vs 999: different hashes.

### 2.7 F5: B341, the os2 onset wait (MEASURED, confirmed)

onsetScatter 15 ms, n 7. Output samples until each member enters:

| mode | per-member wait |
|---|---|
| given (`onsD0`) | 1543 1438 1372 0 1390 2748 827 |
| 1x | 1544 1438 1372 0 1390 2749 827 |
| **2x** | **772 719 686 0 695 1375 414** |

- **Cause:** `s.onsD[i] -= 1` inside the `u < osSub` sub-sample loop (`:1052`).
- **Fix (h2 only, ledgered):** decrement by `1.0/osSub` per sub-sample, or once per output sample. The legacy core keeps the defect (ADR-186; B341 awaits the human).

### 2.8 F6: voice-reuse history leak (MEASURED, `h/probes leak`)

- **Setup:** 16 notes at pitch 40 (core A) or pitch 90 (core C) fill every slot and fully die. Then note 64 reuses slot 0, and its onset is compared between A and C.

| config | max \|A−C\| | diff energy re the note's first 100 ms | samples affected |
|---|---|---|---|
| 1x, width 0.8 | 0 | — | none |
| **2x** | **9.06e-3** | −52.9 dB | 0..30 |
| 1x, width 1.3 (ITD) | 5.65e-4 | −78.1 dB | 0..10 |
| 2x, width 1.3 | 8.32e-3 | −51.2 dB | 0..36 |

- **Cause:** `initVoice` clears `vlp` (`:643`) but not `osZL/osZR` (`:365`) or `itdRing` (`:363`). Both hold **pre-envelope** full-amplitude samples of the previous note, and the cull (`:1017-1024`) doesn't clear them either.
- **Fix:** clear both at note-on. That is 1 KB + 32 KB of memset per note-on; better, track a "dirty" flag.
- **Risk:** none for parity (the JS has neither path). The os2 and width > 1 goldens only change for reused slots.

### 2.9 Parity drift against the JS prototypes (MEASURED, existing tools)

`gen_goldens.mjs` + `parity_check`:
- 156/156 within 1e-6; 113 exactly 0.
- The non-zero rows are saw-shape (≤ 2.5e-9) and `dyn-ring.seed42` (4.26e-9).
- No drift.

**Scope, stated:**
- One note (A3), 44.1 kHz, 1024-sample blocks, OS off.
- None of the C++-only supersets has a JS target: oversampling, shape, voiceEnv/onset scatter, width > 1 modes, voice stealing, glide laws, velocity/pressure, the note tap. B376 already lists what the composed engine does not voice.

---

## 3. Clarity (code facts; judgement marked)

**C1. Structure.**
- `swarm_core.h` is 2,162 lines: 1,365 code, 746 comment, 52 blank, a comment/code ratio of 0.55. It references 49 ADRs and 9 B-rows.
- `renderSeg` is about 420 lines; `controlTick` about 395.
- `Params` holds 70 doubles, including booleans and enums stored as doubles.
- Enum reads are inconsistent: `p.law == 4` (`:1818`) against `(int)p.law != 4` (`:1605`, `:1879`). At a non-integer law, rebuild and controlTick disagree; law 2 (ERB) is reached only as the `else` fallthrough (`:1827`). This is latent, because the shell steps the value.
- JUDGEMENT: much of the comment mass is history ("until 2026-09-18…", measured-then narratives) that belongs in ADRs and traces. The h2 copy should keep the *why* and move the chronicle out.

**C2. Dead or reachability-dead code:**
- `polyGlide` (known, `:275`, `:1461`).
- `AnchorTables::base[0]`: 131 KB built, never read, because `sawBi0 == 0` takes `v` (`:1084`).
- `GlideCore::y`: written in every law, never read (`glide_core.h:84-154`, `:274`).
- `lpOut`: a key with no `kParams` row (B376).
- `force_core.h`: 335 lines included for two functions (`rngNext`, `onePoleCoef`), so the lift would drag the Track E spring system along. Extract a 20-line `rng.h`.

**C3. Stale or wrong comments** (each checked against the code):

| where | claim | fact |
|---|---|---|
| `glide_core.h:25` | "NOT YET WIRED INTO THE AUDIO PATH" | wired at `swarm_core.h:1727` since ADR-096 |
| `swarm_core.h:51-53` | "16 voices x 32 osc = trivial CPU" | 12.7% of an M3 core at 1x, 28.7% at 2x (§1.1) |
| `swarm_core.h:333` | "gravity grid at any real rate (1114 @192k)" | `lround(192000·256/44100)` = **1115** |
| `swarm_core.h:8` | "All state is double" | `itdRing` is float (`:363`, `:1150`) |
| `swarm_core.h:55-64` | the ADR-086 grid rationale | orphaned 72 lines above its constant `kGravGridSeconds` (`:136`), with the saw-shape block in between |
| `swarm_core.h:193`, `:2131` | "five-law GlideCore"; "halfband" | `glide_core.h` has four laws + off (law 5 cut); cutoff 0.235 is not a half-band, so it has no zero taps and pays for all 63 |

**C4. Duplication:**
- The BLEP residual appears twice (`:1062-1066`, `:1106-1113`).
- The per-voice ADSR duplicates the shared ADSR (`:1123-1133` against `:1206-1225`).
- The order-parameter sum appears four times (`:1912`, `:1927`, `:1949`, `:2021`).
- `pow(2, xv·dep·100/1200)` appears in laws 0, 3 and 5.

**C5. ADR trail:**
- All 49 `ADR-nnn` references resolve to DECISIONS headings whose topics match the code site (script check).
- One inaccuracy: ADR-164's heading still reads **PROPOSED** ("the human ratifies by merge"), while shipped code depends on it (`:1898-1910`, `:2052`).

---

## 4. Competitiveness against the heavy-hitters (JUDGEMENT, tied to evidence)

**Published per-voice CPU.**
- I had no web access in this session, and I will not quote per-voice CPU figures for Serum, Vital or Diva from memory. Any number would be unsourced.
- **Reference point I can offer (architectural, verify before quoting):** the open-source Vital and Surge XT process their audio paths in 32-bit float SIMD. Vital packs voices into `poly_float` vectors; Surge runs its filter chain four voices per SSE vector. Their oscillators treat edges with band-limited tables or windowed-sinc/BLIT methods rather than 2-point polyBLEP.
- **Proposed protocol for a real comparison:** the same Mac, the same host (Live), 16 notes × 7-voice unison saw, each synth at its default quality, reading Live's CPU meter. Record the host version and settings.

Where horde stands on what a heavy-hitter user would notice first, in order:
1. **Aliasing on high leads and pitch sweeps** (F3). −41.6 dB at C4 and −26.8 dB at C8 at 1x. A band-limited-table or wide-BLEP oscillator is effectively clean there; my W16 BLEP measures ≤ −120 dB.
2. **Top-register garbage and DC** in harmonic or wide patches (F2). A heavy-hitter never emits a +0.29 DC offset from a patch its own UI allows.
3. **Automation zipper** on volume, width and pan (F8): −43 to −51 dB sidebands against about −65 to −73 dB with smoothing.
4. **CPU headroom** (F1, F7, F11). Competitive-feeling today (16 × 7 at 1x = 3.3% of an M3 P-core) but leaving 2x on the table bit-identically, and about 1.5x more with float kernels.
5. **Unison start phase.** The default `retrig = 1` zeroes every member's phase at note-on (`:701`), so every note's attack is the same phase-aligned spike, and the ensemble decorrelates only through detune. JUDGEMENT: heavy-hitters commonly expose per-note random or free-running start phase as a first-class control; B376 already proposes merging horde's start-phase controls into one row.

What horde has that the heavy-hitters don't (evidence: the ADRs and parity scenarios): coupled-oscillator dynamics (sync, splay, two-cluster, ring, gravity, inertia), up to 32 members, ensemble onset correction, and a pitch-ranked fan with ITD. None of the findings touch that identity. Every fix is at the waveform edge, the gain stage or the loop structure.

---

## 5. Verdict: lift or fresh port?

**Lift.**
- The core is correct where it is defined: parity 156/156, with 113 bit-exact.
- It is deterministic and block-size independent, even at 2x OS on the everything patch.
- It is stable at every extreme tried.
- The prior audit's two criticals and its HIGH are closed.
- The largest CPU win (F1, 2x) is a **bit-identical restructuring** that ADR-187 §5 calls output-neutral work, so it can land in the h2 copy with the parity chain intact.
- Every fidelity defect found (F2, F3, F5, F6, F8) is local and can be ledgered as one divergence per PR.
- A fresh port would have to re-derive 49 ADRs' worth of behaviour and would lose the 156-scenario parity chain, to fix problems none of which is architectural.
- **Do first, in the lift:** B376's culls (the saw-shape family removes F4 and F10, polyGlide, and `base[0]`), and extract `rng.h` so `force_core.h` stays behind.

**Top 5 for the h2 copy, in order:**
1. **Specialised member kernels (F1).** Bit-identical, 2.0–2.3x. Enables everything else.
2. **Per-member Nyquist fade (F2).** Removes garbage and up to 0.29 DC. Ledgered divergence, mirrored.
3. **BLEP-table edges (F3).** About −120 dB at 1x in place of −27 to −49 dB; then decide whether 2x OS stays (F7 and F16 depend on it).
4. **Parameter smoothing by manifest rate class (F8).** About 22 dB less zipper.
5. **B341 + voice-reuse state clear (F5, F6).** Two small ledgered divergences that make the os2 and ITD paths honest.

**The one question for the human:** will you trade the top 20% of the audio band (partials above ~17.6 kHz at 44.1 kHz) and either 16 samples of latency (linear-phase BLEP) or a changed edge phase (minBLEP) for a swarm that is alias-clean at 1x, and so drop 2x oversampling?
