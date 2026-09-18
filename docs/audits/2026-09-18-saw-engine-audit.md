# B147 layer 1 — SWARM SAW engine golden audit (read-only)

Critic, 2026-09-18. Repo at `6ae9b47` (`main`), working tree clean of tracked
changes. Nothing was edited in the repo; every experiment ran from a scratch
copy of the headers under `…/scratchpad/b147/`.

**Machine / build.** Apple M3, macOS 26.6.2, Apple clang 16.0.0. Cost numbers
are `clang++ -std=c++20 -O2` core-direct against `src/swarm_core.h`; the
aliasing rows quoted from `docs/MEASUREMENTS.md` are the shipped-plugin
(CLAP-factory) figures and were re-run today, **bit-identical to the recorded
build 319a758 table** — the recorded table is current.

**Instrumentation is proven neutral.** The timing copy of `swarm_core.h` renders
an FNV-1a-identical buffer to the pristine header
(`b2e84461958e80c2`, 38 400 samples, patch with two mid-render param changes), so
the attribution percentages below are not an artefact of the probe.

**What `./verify full` runs, for reference.** Fifteen gates; the engine-relevant
ones are `parity_check`, `trajectory_check`, `samplerate_check`, `subdiv_check`,
`waveshape_check`, `notefuzz_check`, `rtsafety_probe`, `state_check`.
`measure_alias`, `measure_cpu` and `robustness_matrix` are **built but not
gated** (`verify` contains zero references to them — grep count 0).

---

## 0. Headline

| # | finding | severity | measured |
|---|---|---|---|
| A1 | `KsmS/KsmP/KsmD` slew is a **hand-tuned per-tick `0.08`**, banned by ADR-009 | CRITICAL | K-step response **10.0 ms @44.1k → 4.5 ms @96k = 55 % drift**; `samplerate_check` tol is 0.3 % |
| A2 | `SwarmCore::setParam("n", v)` has **no cap**; `n≥33` corrupts memory, `n≥40` segfaults | CRITICAL | exit 139 in `rebuild()`; `n=33` renders wrong audio silently |
| A3 | ADR-077/078 ensemble-timing RNG is **not seeded by `seed`** and is **session-history dependent**, not in saved state | HIGH | seed 1234 vs 999999 → RMS diff **exactly 0.0**; same seed + 5 earlier notes → RMS diff **0.137** (control: 0.119) |
| A4 | `s.RN` is computed every tick for every voice and **never read by the DSP** | HIGH (CPU) | removing it: **−6.6 % to −7.2 %** total CPU, **bit-identical** |
| A5 | `controlTick()` is **23–34 % of all CPU** despite firing 1/16 samples | HIGH (CPU) | table §2 |
| A6 | inertia (spring) path is **not reproducible across sample rates** | MEDIUM | steady-state R spread **15.03 %** across 44.1–96 k |
| A7 | default output pole is a naive one-pole at fc = 18 kHz — **response warps with sr** | MEDIUM | −3 dB at 22 050 Hz @44.1k vs 20 522 Hz @96k; **−1.34 → −5.53 dB at Nyquist** |
| A8 | `rebuild()` runs on the audio thread per param **event**, no coalescing | MEDIUM | 244–503 ns/event; 128 events/block at n=32 = **+31 % of the block's own cost** |
| A9 | 512 KB (78.6 % of the core object) is ITD ring buffers allocated unconditionally | LOW/MEDIUM | `sizeof(SwarmCore)` = **667 032 B**; `sizeof(Voice)` = 39 360 B |
| A10 | pan motion is a per-render-call integrator (known, `subdiv_check` excludes it) | KNOWN | 0.1915 max sample diff at chunk 333 |

Struck by measurement (i.e. **not** findings): denormals (§1.5), the output
`tanh` as an aliasing source (§1.4), `exp2`-for-`pow` (§2.4), long-run
instability (§3.4).

---

## 1. Lab inheritance — what the JS lab put in the C++, ranked by audible effect

### 1.1 CRITICAL — the `0.08` coupling smoother is a per-tick constant (A1)

`src/swarm_core.h:1631-1632,1645`

```
s.KsmS += (syncT - s.KsmS) * 0.08;
s.KsmP += (splayT - s.KsmP) * 0.08;
…
s.KsmD += (signedT - s.KsmD) * 0.08;
```

Ported verbatim from `reference/swarmsaw.html:486-487`. `0.08` is applied once
per 16-sample control tick, so the time constant is `−(16/sr)/ln(0.92)`:
**4.35 ms at 44.1 kHz, 1.99 ms at 96 kHz.** This is exactly the class ADR-009
bans ("hand-tuned per-tick constants are banned") and the class `samplerate_check`
was written to catch — and it does not test this quantity.

Measured (`scratchpad/b147/ksm_rate.cpp`, `lock2.cpp`, 0.5 ms resolution at every
rate, crossings interpolated per L0032):

| quantity | 44.1 k | 48 k | 88.2 k | 96 k | worst drift |
|---|---|---|---|---|---|
| `KsmS` rise to 90 % (ms) | 9.790 | 9.163 | 4.931 | 4.613 | **52.9 %** |
| `KsmS` rise to 63 % (ms) | 4.248 | 3.995 | 2.174 | 1.998 | **53.0 %** |
| K-knob step 0→1, time to 90 % of `4K²σ` (s) | 0.0100 | 0.0090 | 0.0050 | 0.0045 | **54.9 %** |
| onset-lock `t(R peak)`, dissolve 0.02 s (s) | 0.0444 | 0.0440 | 0.0424 | 0.0425 | **4.49 %** |
| onset-lock `t(R peak)`, dissolve 0.30 s (s) | 0.0449 | 0.0445 | 0.0424 | 0.0425 | **5.56 %** |

Audible consequence: the *lock snap* — the single most characteristic gesture of
this engine — is 4.5–5.6 % early at 96 kHz, and any **K automation** slews
**2.2× faster** at 96 kHz. `samplerate_check`'s own tolerance is 0.3 %; the two
things it does test sit at 0.125 % and 0.163 %. This is 183× over that bar.

*Note the tension, honestly:* fixing it changes the 44.1 kHz output unless the
seconds-expressed constant is chosen so `1−exp(−(16/44100)/τ) == 0.08` exactly in
IEEE754 — which is not guaranteed. So this is a **re-baseline** change under the
B81 pre-authorisation, or a special-cased-at-44.1k construction like the
`cullEnv` trick already used at `swarm_core.h:854`.

### 1.2 CRITICAL — the core has no cap on `n` (A2)

`src/swarm_core.h:410-427` (`setParam`), `:1250` / `:1310` / `:1461` / `:815`
(`const int n = (int)p.n;` — four sites, none clamped) against
`double x[kMaxV]` (`:1833`, `kMaxV = 32` at `:40`).

The only cap in the system is the shell's param-table row
`src/hypersaw_clap.cpp:146`: `{1, "n", "Voices", 1, 32, 7, true, nullptr}`.

Measured (`scratchpad/b147/nclamp2.cpp`):

| `setParam("n", v)` | result |
|---|---|
| 33 | **survives, renders audio** — `rebuild()` wrote one `double` past `x[32]`, silently corrupting `panL[0]` |
| 40, 64, 200 | **SIGSEGV (exit 139) inside `rebuild()`** |

Every oracle in `tools/` drives `SwarmCore` directly and therefore bypasses that
row. This violates the doctrine invariant *"Safety by construction (caps,
preallocation, frozen IDs), not by vigilance."* The minimal delta is one
expression, bit-identical for all legal values:
`const int n = std::min(kMaxV, std::max(1, (int)p.n));` at the four sites (or a
clamp in `setParam`).

### 1.3 HIGH — the ensemble-timing RNG is not the seeded stream (A3)

`src/swarm_core.h:1841-1848`

```
double tOff[kMaxV] = {0};
uint32_t tRng = 12345;
double gaussT() { … rngNext(tRng) … }
```

`tRng` is a **literal**, never derived from `p.seed`, never reset. `tOff`
persists across notes *by design* (`:1839-1840`, ADR-077) but is never reset by
`initVoice`, `allOff`, `killAll`, or any state restore (`tOff`/`tRng` are private
with no accessor — `state_check` cannot see them).

Measured (`scratchpad/b147/det.cpp`, patch: `voiceEnv 1`, `attackScatter 1`,
`relScatter 1`, `onsetScatter 8`), with the must-read-nonzero control the
`detector-shares-assumption` lesson requires:

```
A. seed 1234 vs seed 999999                        RMS diff = 0.000e+00
B. same seed + same note, 5 earlier notes vs none  RMS diff = 1.365e-01
C. CONTROL: phase stream, retrig off, seed differs RMS diff = 1.185e-01
```

B is the same magnitude as C — i.e. a *completely different render*. Against
`specs/ACCEPTANCE.md` L0-13 ("Same seed + note sequence → bit-identical control-
path state and ε-identical audio") this is at minimum a spec/code disagreement,
which the brief says is itself a finding: `seed` is advertised as the control and
does not reach this stream, and a bounce is not reproducible across a session
restart for any patch using per-voice envelopes.

Minimal delta: seed `tRng` from `p.seed` in `rebuild()` (already the place `grng`
is seeded, `:1251`) and expose `tOff`/`tRng` to the state chunk — **or**, if the
history-dependence is wanted, say so in ACCEPTANCE and add the falsifier. This is
a **re-baseline** change only for patches with `onsetScatter/voiceEnv > 0`;
defaults are 0 and the stream is never drawn.

### 1.4 STRUCK — the output `tanh` is not an aliasing source

`src/swarm_core.h:1148-1152` applies `tanh` to the summed bus every block, with
no oversampling, *after* the ADR-075 halfband decimator. It matches the reference
(`reference/swarmsaw.html:663-664`), so parity certifies it rather than catching it.

I built a sub-fundamental detector for the **swarm** case that `tools/measure_alias.cpp`
explicitly declines (it is n=1 only): energy in [20 Hz, 0.7·f0] over total energy,
n=7, detune 0.28, Kaiser β=19, 2^17 FFT, with an alias-free additive-swarm
must-read-floor control and a naive-saw must-read-large control
(`scratchpad/b147/swarmalias.cpp`):

| MIDI | sr | additive (control) | naive (control) | polyBLEP | BLEP + 2× OS |
|---|---|---|---|---|---|
| 36 | 44.1 k | −159.7 | −58.7 | **−117.8** | −117.8 |
| 60 | 44.1 k | −166.7 | −45.1 | **−107.7** | −107.6 |
| 84 | 44.1 k | −170.7 | −31.3 | **−96.6** | −96.6 |
| 36 | 96 k | −156.3 | −62.6 | −126.8 | −126.7 |
| 60 | 96 k | −163.4 | −51.2 | −106.7 | −106.7 |
| 84 | 96 k | −170.0 | −38.7 | −97.3 | −97.3 |

At the **shipped** `vol 0.4` the same figure is **−82.1 / −71.7 / −60.7 dB** at
C2/C4/C6 — i.e. the `tanh` adds ~36 dB of sub-fundamental content, and **2×
oversampling buys 0.0–0.1 dB** because it does not touch the `tanh`.

*But it is not aliasing.* Sweeping the rate 44.1 → 352.8 k
(`scratchpad/b147/tanhalias.cpp`) leaves the figure **rate-invariant within ~3 dB
and non-monotonic**, which is the signature of legitimate difference tones, not
fold-back:

```
note  vol  |  44.1k   88.2k  176.4k  352.8k
 60  0.40  |  -71.7   -70.9   -73.8   -73.1
 84  0.40  |  -60.7   -61.3   -59.5   -60.9
```

**Conclusion: do not oversample the `tanh`.** Its sub-fundamental content is
intermodulation — the character. Declared blind spot: this metric cannot see
fold-back landing *above* 0.7·f0; `measure_alias` is the instrument for that and
already reports it at n=1.

### 1.5 STRUCK — denormals

Already investigated and struck by its own falsifier on 2026-08-25
(`docs/research/2026-08-24-cpu-audit.md`); `tools/robustness_matrix.cpp:30-47`
carries the mechanism and a **must-fire control**. Re-run today:

```
FTZ/DAZ active in this process: no
control: subnormal-chain penalty on this CPU: x1.01 — this CPU cannot show a stall
worst tail/held cost ratio 1.15 (threshold 2.0): OK; subnormal output samples: 0
```

The honest residue: the control says an M3 *cannot* show the stall, so this is
**evidence about the metric, not about x86**. ACCEPTANCE L0-6's min-spec names a
"4-core 2018-class Intel ultrabook" and "Windows x64 AVX2", where the penalty is
real. **Gap, not a defect.** Note also that state which decays toward zero and
is never flushed exists (`s.vlp[]`, `s.mom[]`, `apZ` at `:1837`) — the voice cull
(`:862-879`) zeroes `env/lpL/lpR` but not `vlp`, and `apZ` runs unconditionally
whenever `width>1 && superMode!=0` with a silent input.

### 1.6 LOW — truncated π, and the table-vs-anchor asymmetry

`kPiRef = 3.14159265` (`:137`) is π to 1.14e-9 relative, used for the pan seat
angles (`:1417`), pan motion (`:793`) and — less obviously — the **halfband
filter design** (`:379-381`). Parity-load-bearing and documented; the design-time
error is far below the −0.83 dB droop ADR-075 claims. No action.

`AnchorTables::base[0]` (`:112`) is built but unreachable: the render uses `v`
(the BLEP'd saw) when `sawBi0 == 0` (`:938`), never `base[0]`. 128 KB of the
static table is dead. Cosmetic.

### 1.7 KNOWN — pan motion is a per-render-call integrator (A10)

`src/swarm_core.h:772-798`, declared loudly at `tools/subdiv_check.cpp:70-79`.
Re-measured today: **0.1914508641** max sample difference at chunk 333; gravity
and the inert case are **exactly 0**. The exclusion is honest and the ruling is
still open. Ruling it is a precondition for calling block-size independence
"gated".

---

## 2. C++ cost profile

`scratchpad/b147/cost2.cpp`, core-direct, 44.1 kHz, 128-frame blocks, 2 s of
audio, min of 3, cycle counter (`cntvct_el0`, 1 ns tick) with the clock-pair
overhead (1.26 ns) calibrated and subtracted from both the phase and the total.

### 2.1 Cost vs voice count

| notes | n | ns/sample | ns/sample/osc | ctick % | grav % | tail % | loop % | % of a core |
|---|---|---|---|---|---|---|---|---|
| 1 | 1 | 22.65 | 22.65 | 25.8 | 0.0 | 28.4 | 45.8 | 0.10 |
| 1 | 7 | 70.15 | 10.02 | 24.0 | 0.0 | 8.1 | 67.9 | 0.31 |
| 1 | 32 | 198.80 | 6.21 | 21.5 | 0.0 | 2.0 | 76.6 | 0.88 |
| 8 | 1 | 110.06 | 13.76 | **33.8** | 0.0 | 5.7 | 60.5 | 0.49 |
| 8 | 7 | 410.40 | 7.33 | 26.9 | 0.0 | 1.8 | 71.3 | 1.81 |
| 8 | 16 | 879.00 | 6.87 | 24.2 | 0.0 | 0.8 | 75.0 | 3.88 |
| 8 | 32 | 1651.40 | 6.45 | 23.3 | 0.0 | 0.4 | 76.3 | 7.28 |
| 16 | 1 | 219.24 | 13.70 | **34.2** | 0.0 | 3.3 | 62.6 | 0.97 |
| 16 | 7 | 845.92 | 7.55 | 27.6 | 0.0 | 1.0 | 71.4 | 3.73 |
| 16 | 32 | 3486.97 | 6.81 | 23.6 | 0.0 | 0.3 | 76.2 | 15.38 |

Consistent with `docs/MEASUREMENTS.md` §2 (shell, 8 notes, n=8: 2.01 % of a core;
here 8×7 core-direct = 1.81 %), so the core-direct harness is not flattering
itself. Run-to-run machine noise is ±12 % between passes; use ratios, not
absolutes.

### 2.2 The top three cost drivers

**1. `controlTick()` — 23–34 % of total CPU** (`:1456-1821`). It fires once per
16 samples yet costs a quarter to a third of everything, because it runs
~5n transcendentals + n `pow` per voice per tick, unguarded. The dominant
unguarded terms per tick per voice: `pow(2,·)` × n (law 0, `:1536`), `cos+sin` × n
for R (`:1650-1651`), `cos+sin` × n for **RN** (`:1661-1662`), `sin` × n for the
coupling (`:1718`).

**2. `s.RN` is dead weight in the DSP (A4).** `:1664`. Grep of the whole tree:
`RN` is read only by `src/hypersaw_clap.cpp:3255` (viz snapshot of the **focus**
voice), `:6124` (`hypersaw_debug_viz`, also focus-only), the GUI, and
`tools/trajectory_check.cpp` (via `focus()`). It is computed for **all 16 voice
slots on every tick**. Removing it entirely (scratch variant `isrc_noRN`):

| case | base ns/sample | no-RN | delta |
|---|---|---|---|
| 8 notes × n=7 | 412.5 / 422.1 | 388.2 / 389.0 | **−6.6 %** |
| 8 notes × n=32 | 1652.3 / 1653.3 | 1529.8 / 1530.3 | **−7.4 %** |
| 16 notes × n=32 | 3469.0 / 3462.4 | 3216.1 / 3210.6 | **−7.3 %** |

and `ctick%` falls 23.3 → 17.3 at 8×32, i.e. RN is ~26 % of `controlTick`.
**Bit-identical to the audio** (verified: FNV-1a `b2e84461958e80c2` both).
The minimal delta preserves `trajectory_check`: compute RN only for the voice
`focus()` would return, or behind a `vizWanted` flag the shell sets.

**3. The per-sample voice loop, 60–77 %** — irreducible by construction, but two
sub-items are visible: the polyBLEP branch pair (`:916-921`, `:960-967`) costs
**35.6 ns/sample at 8×7** (`digital 0` = −35.56), and the ADR-074 ITD ring costs
**+19.3 ns/sample** when `width > 1`.

Notable non-drivers, measured at 8 notes × n=7 (deltas vs base 414.81 ns/sample):

| toggle | delta ns/sample |
|---|---|
| K = 1 (coupling on) | **−29.64** (locked phases make the BLEP branches predictable) |
| K = −1 (splay) | −17.79 |
| gravity 0.7 | +11.24 (matches the "+2 %" claim at `:813`) |
| toneTilt 0.5 | +15.46 |
| hiTame 0.5 | +0.35 |
| inertia 0.5 | +0.92 |

The O(N²) worry in the brief does **not** apply to the default: mean-field
coupling is O(n) via the order parameter (`:1646-1656`). Only `topo == 1` (ring,
`:1727-1741`) is O(n·r) ≤ O(n²/2), and gravity is O(gated²)·13 `log2` per grid
step at 172 steps/s — measured at 2.7 % with 8 gated notes.

### 2.3 Memory (A9)

```
sizeof(SwarmCore) = 667 032 B (651.4 KB);  sizeof(Voice) = 39 360 B (38.4 KB)
  itdRing = 32.0 KB/voice, 512.0 KB total = 78.6 % of the object
```

`float itdRing[kMaxV][256]` (`:346`) is allocated and zeroed for all 16 slots
regardless of `width`, and the shell holds `cores[kNumOsc]`. A `Voice` is 300×
larger than L1d's line budget for it; the hot fields happen to sit before the ring
in declaration order, so the penalty is footprint and construction, not the inner
loop. Reduction: one shared ring pool, or allocate at `activate()` only when
`width > 1` is reachable.

### 2.4 Rejected optimisations (measured, negative)

| candidate | result |
|---|---|
| `std::exp2` for `std::pow(2,·)` in law 0 | **no win** on Apple libm (425.3 vs 422.1, 468.2 vs 471.2 — inside noise). An early run showed a 2× "regression"; that was thermal drift, re-measured away. |
| double bus accumulation instead of the `Float32Array +=` float-store (`:1093-1094`) | −3.3 % at 8×7, −3.7 % at 16×32, but **changes the output** (FNV-1a `687e17565aa41ebb` vs `b2e84461958e80c2`). It buys 3.3 % and destroys the thing that makes parity mean anything (`:8-11`). **Reject** unless the human retires JS parity as the correctness definition — that is an ADR, not an optimisation. |
| hoisting the law-0 `pow(2, xv·dep·100/1200)` into a per-`(detune,spread,anchor,x[])` cache | **works, bit-identical** through mid-render `detune`/`anchor` changes (FNV-1a match), but only **−1.4 % to −2.0 %**. Take it only as part of the same PR as RN. |

### 2.5 `rebuild()` on the audio thread (A8)

`setParam` on any of `n/dist/seed/width/topo/panScatter/law/panLayout/panCurve/
panInvert/superMode/oversample` calls `rebuild()` (`:422-425`), and
`applyParam` drains the CLAP queue per **event** on the audio thread
(`src/hypersaw_clap.cpp:903`, `:4251`) — no coalescing.

| n | one `setParam("width")` | one `setParam("K")` | ratio |
|---|---|---|---|
| 7 | 243.6 ns | 28.0 ns | 9× |
| 16 | 272.0 ns | 23.6 ns | 12× |
| 32 | 503.4 ns | 22.7 ns | 22× |

Worst realistic case, 8 held notes, 128-frame block (budget 2902 μs):

| n | 0 events | 32 events | 128 events |
|---|---|---|---|
| 7 | 53.4 μs (1.8 %) | 57.8 μs (2.0 %) | 70.5 μs (2.4 %) |
| 32 | 213.0 μs (7.3 %) | 228.8 μs (7.9 %) | **278.6 μs (9.6 %)** |

So: **+31 % of the block's own cost** at n=32 under sample-accurate automation,
but only +2.3 points of the real-time budget. Real, bounded, worth a
"rebuild only if the value actually changed" one-liner; not urgent. Nothing in
`./verify` measures cost under automation (`docs/MEASUREMENTS.md` §2 declares
"no parameter automation" as a blind spot).

---

## 3. The suite the engine lacks

For each: does a gate exist, what it measures, what it misses, the check that
closes the gap, and the threshold **measured on this build**.

### 3.1 Aliasing floor across the keyboard

- **Gate:** none. `tools/measure_alias.cpp` exists (B103) and is calibrated to
  closed form, but `./verify` never runs it — it "prints, never judges"
  (`docs/MEASUREMENTS.md` header).
- **Covers:** n=1, MIDI 36/60/84/96, 44.1/96 k, naive/polyBLEP/2× OS, shape on/off.
- **Misses:** every n>1 patch; coupling, drift, gravity, FX; and the second
  oscillator.
- **Shape of the check:** promote `measure_alias` to a gate over its existing
  rows with per-row floors, and add the n=7 swarm rows from
  `scratchpad/b147/swarmalias.cpp` (same two controls).
- **Measured thresholds to record.** Shipped polyBLEP, n=1, 44.1 k, integral /
  worst: MIDI 36 **−44.5 / −88.0 dB**; 60 **−38.5 / −149.2**; 84 **−32.8 / −185.9**;
  96 **−28.8 / −187.1**. Swarm n=7, sub-fundamental, 44.1 k, `vol 0.05`:
  C2 **−117.8**, C4 **−107.7**, C6 **−96.6 dB** (controls: additive ≤ −159.7,
  naive ≥ −58.7). A gate at **−90 dB** for the swarm rows has ~7 dB margin at the
  worst note and 38 dB over the must-read-large control.

### 3.2 Sample-rate independence (behaviour in SECONDS)

- **Gate:** `samplerate_check`. Two quantities only, 44.1/48/88.2/96 k, tol 0.3 %.
- **Passes today at:** attack **0.125 %**, gravity settle **0.163 %**.
- **Misses (this is the big one):** the K smoother (**54.9 %**, §1.1), the
  onset-lock snap (**5.56 %**), the inertia/spring steady state (**15.03 %**,
  §3.5), the output-pole magnitude response (§3.6), the drift walk, the tone
  tilt, `GlideCore`'s laws end-to-end.
- **Shape of the check:** extend `samplerate_check` with the three probes already
  written in scratch (`ksm_rate.cpp`, `lock2.cpp`, `tickgrid.cpp`) — the
  1-ms-resolution / interpolated-crossing discipline of the existing file is
  already right and should be reused verbatim.
- **Thresholds to record after the fix:** K-step to 90 % of `4K²σ` = **10.0 ms**
  at 44.1 k, target drift **< 0.3 %** (same bar as the two existing rows).
  Onset-lock `t(R peak)` at dissolve 0.02/0.05/0.30 = **44.4 / 45.9 / 44.9 ms**,
  same bar.

### 3.3 Block-size independence

- **Gate:** `subdiv_check`, chunks {N, 2048, 1024, 512, 256, 333, 127, 64}.
- **Measured today:** inert **0**, gravity **0**, pan motion **0.1914508641**
  (KNOWN/excluded), both **0.1918540299** (KNOWN/excluded).
- **Misses:** pan motion (declared); and the rate×chunk cross product — every
  chunk case runs at 44.1 kHz only.
- **Shape:** rule pan motion (fixed grid, as gravity was) and fold the two KNOWN
  rows into the gate; add a 96 kHz leg. Threshold: **0.0 exactly** for gravity and
  inert, which is what it reads today — no tolerance needed.

### 3.4 Long-run stability

- **Gate:** none for duration. `notefuzz_check` and `robustness_matrix` cover
  finiteness and tails at seconds-scale; ACCEPTANCE L0-13 asserts stability
  without a duration.
- **Measured** (`scratchpad/b147/longrun.cpp`, **10 min of simulated time**,
  44.1 kHz, 128-frame blocks, 3 held notes, 9 patches):

| patch | RMS 0–10 s | RMS last 10 s | peak | f0 drift (cents) | max \|Ksm\| | eff range (Hz) | non-finite |
|---|---|---|---|---|---|---|---|
| default (K 0) | −16.20 | −16.18 | 0.7347 | 0.000 | 0.000 | 257.4–398.2 | 0 |
| K 1 locked | −10.10 | −10.09 | 0.7718 | 0.000 | 15.641 | 257.8–397.5 | 0 |
| K −1 splay | −22.85 | −22.87 | 0.3769 | 0.000 | 46.924 | 244.0–418.4 | 0 |
| drift 1 walk | −16.25 | −16.11 | 0.7356 | 0.000 | 0.000 | 251.5–407.5 | 0 |
| gravity 1 basin 60 | −16.12 | −16.11 | 0.7349 | 0.001 | 0.000 | 257.4–399.6 | 0 |
| inertia 0.95 + K1 | −16.26 | −16.38 | 0.7347 | 0.000 | 15.641 | 255.6–400.1 | 0 |
| ring topo r5 K1 | −9.91 | −9.90 | 0.7821 | 0.000 | 15.641 | 257.8–397.4 | 0 |
| two-cluster bal 1 K−1 | −14.20 | −14.22 | 0.7516 | 0.000 | 47.534 | 255.3–399.4 | 0 |
| everything (n=16, OS, drift, grav, motion, glide) | −19.41 | −19.82 | 0.7252 | 0.001 | 50.848 | 247.1–414.2 | 0 |

- **Verdict: clean.** Thresholds to record: over 10 min, **RMS drift ≤ 0.41 dB**,
  **f0 drift ≤ 0.001 cents**, **peak ≤ 0.79** (tanh-bounded), **max \|Ksm\| ≤ 50.9**,
  **zero non-finite values**. A gate at drift ≤ 1 dB / f0 ≤ 0.01 ct / peak ≤ 1.0 /
  \|Ksm\| ≤ 200 / non-finite == 0 has ~2× margin on every column. Cost: ~80 s of
  wall time for all nine patches — a `full` gate, not `fast`.

### 3.5 Long-run / cross-rate reproducibility of the inertia path (A6)

`scratchpad/b147/tickgrid.cpp` — steady-state R averaged over t = 10–12 s (long
after the `0.08` smoother has settled, so this is *not* §1.1):

| patch | 44.1 k | 48 k | 88.2 k | 96 k | spread |
|---|---|---|---|---|---|
| K 0.3 (below threshold) | 0.33181 | 0.33204 | 0.33200 | 0.33223 | 0.13 % |
| K 0.4 (marginal) | 0.35230 | 0.35181 | 0.35244 | 0.35193 | 0.18 % |
| K 0.6 | 0.38391 | 0.38398 | 0.38823 | 0.38667 | 1.13 % |
| K 1.0 (full lock) | 0.96517 | 0.96517 | 0.96517 | 0.96517 | 0.00 % |
| K −1 (splay) | 0.04091 | 0.04091 | 0.04091 | 0.04091 | 0.00 % |
| **K 0.6 + inertia 0.7** | 0.27914 | 0.28200 | **0.25237** | 0.29432 | **15.03 %** |

Root cause candidate: `kTick = 16` (`:53`) is a fixed **sample count**, not a
fixed time — the same defect class ADR-086 Amendment 1 fixed one level down for
the gravity grid, still present one level up. The coupling term is held for
0.363 ms at 44.1 k and 0.167 ms at 96 k, so the 2nd-order momentum integrator at
`:1805-1813` (explicit Euler, `dt = kTick/sr`) has rate-tracking truncation error.
The spread is **non-monotonic** in rate, so I will not claim systematic bias over
chaotic sensitivity — but the player-facing fact is identical: *the spring patch
does not sound the same at 96 kHz, and nothing measures it.* Record the number;
do not "fix" the spring law without a ruling (see §4).

### 3.6 Sample-rate independence of the default output pole (A7)

`:1815-1819`. With `rtone` at its default 0, `fc` is clamped to 18 000 Hz and the
naive one-pole `1 − exp(−2π·fc/sr)` is far outside its accurate region
(`scratchpad/b147/misc.cpp`):

| sr | coef a | −3 dB (Hz) | \|H\| @10 kHz | \|H\| @ Nyquist |
|---|---|---|---|---|
| 44 100 | 0.92305 | 22 050.0 | −0.623 dB | −1.339 dB |
| 48 000 | 0.90522 | 24 000.0 | −0.687 dB | −1.651 dB |
| 88 200 | 0.72260 | 21 160.4 | −0.998 dB | −4.949 dB |
| 96 000 | 0.69214 | 20 522.2 | −1.023 dB | −5.527 dB |
| 192 000 | 0.44515 | 18 499.3 | −1.130 dB | −10.864 dB |

At 44.1 k the "18 kHz filter" is effectively transparent (its −3 dB point is
Nyquist); at 192 k it is a real −10.9 dB shelf. **0.40 dB of audible-band
difference at 10 kHz between 44.1 k and 96 k.** Modest, but it is timbre, it is
rate-dependent, and `samplerate_check` (which tests two *times*) is structurally
blind to it. Shape of the check: assert the magnitude response at 1/4/10 kHz
matches within 0.25 dB across rates. Threshold measured: worst pair today is
**0.51 dB** (44.1 k vs 192 k at 10 kHz); within 44.1–96 k it is **0.40 dB**.

### 3.7 Denormal silence

- **Gate:** none; `robustness_matrix` is diagnostic only (its own header says so).
- **Measured today:** 0 subnormal output samples, worst tail/held cost ratio
  **1.15** (threshold 2.0), and the control reports **×1.01** — this CPU cannot
  show a stall, so this is not evidence either way.
- **Gap:** never measured on the x86 min-spec ACCEPTANCE L0-6 names. Also
  unmeasured: internal state (`s.vlp[]`, `s.mom[]`, `apZ`) rather than output.
- **Shape:** none locally. The honest move is to record the ×1.01 control in
  ACCEPTANCE beside L0-6 so the "we measured it" claim carries its own scope.

### 3.8 CPU per voice

- **Gate:** none. `measure_cpu` prints; `cpu_bench.cpp` states the 50 %-of-a-core
  E-6 budget but is not run by `./verify`.
- **Measured (shell, `docs/MEASUREMENTS.md`, re-confirmed core-direct today):**
  8 notes, one osc — n=1 **0.54 %**, n=8 **2.01 %**, n=16 **3.73 %**, n=32
  **7.00 %** of an M3 core; two osc doubles it (**14.15 %** worst).
- **Shape:** gate `measure_cpu` at a **ceiling of 2× today's worst cell**
  (i.e. 28 % of a core for 2 osc × n=32 × 8 notes) — loose enough to survive
  machine noise (±12 % run to run here), tight enough to catch a 2× regression.
  Note the honest limit: this is one machine, so it is a *regression* gate, not
  the min-spec claim; ADR-082's question is answered only by running it there.

---

## 4. What NOT to touch — the engine's identity, and the oracle that pins each

| thing | file:line | why it is the instrument | pinned by |
|---|---|---|---|
| the `0.08` smoother's **shape** (a one-pole on K, with the sigma-normalised target) | `:1620-1645` | the "herd" phase of the K sweep is that lag; only its **rate** is the defect, not its existence | L0-2, Layer-E E-1, parity |
| `sigma = max(0.08, sd)` floor and `km = 4K·\|K\|` | `:1588`, `:1620` | sigma-normalised coupling is *the* claim ("the supersaw taken seriously as physics") | L0-2, L0-3 |
| the splay term `KsmP·sin(τ(φ_c0 + (i−c0)/n − φ_i))` | `:1719-1720` | splay is an even lattice, not noise — deleting it makes K<0 a mute | L0-3, `twocluster_check`, E-4 |
| the **two-cluster branch** and `kB = 1−2·balance` | `:1745-1782` | ADR-051/164; the K-sign × balance corners are four distinct states | L0-10, L0-23, `twocluster_check` |
| `pivotMode` root-pinned pacemaker dropping the R scaler | `:1694-1718` | lab-measured trait deliberately kept (`:1699-1701`) | trajectory L0-2 variants |
| the seeded distributions `dist 0..4` incl. the JP table and the golden-ratio placement | `:1270-1302` | the placement *is* the ensemble; `JP[7]` is a measured artefact | parity (156 scenarios) |
| the ADR-070 **alternating pitch-ranked fan** + insertion sort (stable, by construction) | `:1346-1392` | ties are reachable; an "obvious" `std::sort` changes the image AND allocates | parity, `rtsafety_probe` |
| the output `tanh` | `:1148-1152` | §1.4: its sub-fundamental content is rate-invariant IMD — the character | parity, `waveshape_check` |
| the ADR-025/074 mode-A negative cross-term "up-cliff" | `:1113-1127` | documented character, pinned as an **expected exception** | `waveshape_check` |
| `Float32Array +=` per-store rounding | `:1093-1094` | it is what makes parity a definition rather than a coincidence | parity (§2.4: −3.3 % is not worth it) |
| the inertia/spring law `w0 = τ(8(1−w)+0.6)`, `ζ = 0.45` | `:1805-1806` | §3.5 found a rate spread here; **record it, do not retune it** — this is the "true inertia" gesture | L0-4 |
| `HZ_CULL_ENV` / the `voiceCull <= −79.9999` special case | `:854-856` | one ULP moves which sample a voice stops on | parity |
| `kGravGridSeconds = 256/44100` exactly | `:135` | chosen so `gravGridSamples()` returns exactly 256 at 44.1 k | `samplerate_check`, `subdiv_check` |

---

## 5. Ranked plan for layers 2 and 3

**Layer 2 (suite) first — nothing in layer 3 is measurable without it.**

| # | work | why first | tag |
|---|---|---|---|
| S1 | extend `samplerate_check` with the K-step, onset-lock and steady-state-R probes (scratch code exists) | it is the oracle that makes A1 and A6 visible; without it, "fixing" the smoother is unverifiable | gate, `full` |
| S2 | promote `measure_alias` to a gate at its existing rows + the n=7 swarm rows | the alias table already exists and is calibrated; the gap is that it never judges | gate, `full` |
| S3 | `longrun_check` — the nine patches of §3.4 at 10 min, thresholds as measured | nothing covers duration; ~80 s, `full`-only | gate, `full` |
| S4 | promote `measure_cpu` to a regression ceiling at 2× today | any layer-3 optimisation needs a before/after that a human did not eyeball | gate, `full` |
| S5 | assert the default output pole's magnitude response across rates (§3.6) | one closed-form assertion, no render needed | gate, `fast` |
| S6 | record the measured numbers in `specs/ACCEPTANCE.md` (L0-25+): alias floors, 10-min stability bounds, CPU ceilings, the ×1.01 denormal control | ACCEPTANCE numbers are "measured, not aspirational" (§Domain) — these are measured | doc |

Layer-2 **reduction note:** S2 and S4 add **zero new tools** — they wire two
existing, already-calibrated binaries into `verify`. S1 extends one file. Only S3
is new code. Resist writing a new FFT: `measure_alias` and
`blep_alias_incommensurate_probe` already carry the calibrated one.

**Layer 3 (optimise), ordered by benefit-per-risk.**

| # | change | benefit | risk | tag | gated by |
|---|---|---|---|---|---|
| O1 | **cap `n` at `kMaxV`** in the core (4 sites) | removes a segfault / silent corruption | none | **bit-identical** | parity + a new `n=33/64/200` row in `notefuzz_check` |
| O2 | **compute `RN` for the focus voice only** | **−6.6 to −7.4 %** total CPU | low | **bit-identical** (FNV-1a verified) | parity + `trajectory_check` (it reads RN via `focus()`) |
| O3 | cache the law-0 `pow(2,·)` per `(x,detune,spread,anchor)` | −1.4 to −2.0 % | low (invalidation) | **bit-identical** (FNV-1a verified through mid-render param changes) | parity |
| O4 | `rebuild()` only when the value actually changed | −65 μs/block worst case under automation | none | **bit-identical** | parity + S4 |
| O5 | **express the `0.08` smoother in seconds** (A1) | closes a 54.9 % rate drift; the largest *audible* defect found | medium — changes the 96 k sound deliberately; may change 44.1 k by ULPs | **re-baseline** (B81) unless special-cased at 44.1 k like `cullEnv` | S1 + re-baselined goldens + a human A/B at 96 k |
| O6 | **seed `tRng` from `p.seed`; put `tOff/tRng` in the state chunk** (A3) | makes `seed` mean what ACCEPTANCE L0-13 says it means | medium — changes any patch with `onsetScatter/voiceEnv > 0` | **re-baseline** for those patches only; defaults untouched | parity + `state_check` + `statefix_check` |
| O7 | allocate ITD rings only when reachable (A9) | −512 KB/core | low | **bit-identical** | parity + `rtsafety_probe` |
| O8 | rule pan motion onto a fixed grid and fold the two KNOWN rows into `subdiv_check` | closes the last declared subdivision hole | medium — it is a *ruling*, not a fix | **re-baseline** | human ruling first, then S-suite |
| — | ~~double bus accumulation~~ | −3.3 % | **rejected** — retires JS parity as the correctness definition for 3.3 % | — | — |
| — | ~~`exp2` for `pow(2,·)`~~ | **rejected** — measured no win on Apple libm | — | — |

**Do not** put O5, O6 and O8 in one PR: each re-baselines a different subset of
goldens, and a combined re-baseline makes the before/after unattributable.

---

## 6. Oracle-coverage gaps this audit found that `./verify full` cannot see

1. The coupling smoother's rate dependence (54.9 %) — `samplerate_check` tests two
   quantities and neither is this one.
2. Any `n > 32` caller — every tool bypasses the shell's only cap.
3. `seed`'s reach: no gate asserts that changing `seed` changes the output, so a
   stream that ignores it reads as correct.
4. Session-history dependence of the ensemble stream: `state_check` cannot see
   private members with no accessor.
5. CPU: no gate. A 2× regression in `controlTick` would ship green.
6. Aliasing: no gate, and nothing at all for n > 1.
7. Duration: no gate runs longer than seconds.
8. Parameter automation cost: `docs/MEASUREMENTS.md` declares it as a blind spot
   and nothing else covers it.
9. Filter magnitude response vs sample rate (only *times* are checked).
10. Denormals on x86 — measured only where the control says the measurement
    cannot fire.

Scratch artefacts, all reproducible:
`…/scratchpad/b147/{ksm_rate,lock2,tickgrid,longrun,det,nclamp2,misc,cost2,rebuild_cost,autostorm,swarmalias,tanhalias,bitid}.cpp`
plus the instrumented header copies `isrc/`, `isrc_noRN/`, `isrc_ratio/`, `isrc_dblmix/`.
