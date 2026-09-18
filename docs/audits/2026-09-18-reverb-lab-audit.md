# B152 layer 1 — reverb lab audit (read-only), before it becomes the Reverb roster node

Critic, 2026-09-18. Subject: `docs/design/reverb-lab.html` (805 lines, rev at
`main`). Nothing in the repo was edited. Every experiment ran from a scratch
slice of the lab's DSP section under `…/scratchpad/b152/`; mutated variants were
built in memory from that slice, never on disk. Sibling audit and method:
`docs/audits/2026-09-18-saw-engine-audit.md` (B147 layer 1).

**Harness.** The lab's DSP section (lines 38–531) is sliced out at its own
banners and evaluated headlessly. `tools/golden/extract_core.mjs` **cannot** do
this — see R16. Every mutation asserts its anchor text and throws if absent, and
the anchor mechanism was itself verified by a bogus anchor that must throw
(L0032). The C++ Room comparison builds `src/time_core.h` directly with
`c++ -std=c++20 -O2`.

**Detectors were calibrated before use, with must-read-zero controls (L0016/L0032).**

| detector | comb-free control | known-defect control | verdict |
|---|---|---|---|
| `rt60` (Schroeder T30) | — | synthetic decays 0.5/2.2/6.0 s → 0.4955/2.1970/5.9985 | **≤1.0 % error, trusted** |
| spectral flatness | white 0.996 | comb g=0.9 → 0.260 | **REJECTED**: a comb-free but merely *tilted* tail reads 0.301. It measures damping, not combing. Not used for any claim below. |
| `roughnessDb` (1/3-oct) | decaying noise 1.744 dB | comb g=0.9 L=441 → 7.098 dB | **PARTIALLY REJECTED**: the same comb at L=2227 (the lab's own shortest line) reads **1.234 dB — *below* the comb-free floor.** Structurally blind at the lengths it would be used on. See R17. |
| `combPeak` (cepstral) | decaying noise **0.0134**, decaying+tilted **0.0135** | combs at L=441/2227/5737 → **0.706/0.766/0.781** | **50× separation, trusted** (its *lag* estimate aliases for long L; only the peak height is used) |

---

## 0. Headline

| # | finding | severity | measured |
|---|---|---|---|
| R1 | ER tap read can land **exactly on `buf.length`** → out-of-bounds → NaN → watchdog wipes the whole reverb | **CRITICAL** | **0 %** of the erSize slider affected at 44.1 k, **1.5 %** at 48 k (3 wipes/s), **26.5 %** at 88.2 k, **33.8 %** at 96 k (41 664 wipes/s, wet RMS **exactly 0**) |
| R2 | modulation tick is keyed to the index **within the block** — its rate is a function of host buffer size | **CRITICAL** | **16.000×** rate at block=1 (**ADR-175's exact case**), 2.286× at block=7; 8.8 % RMS error vs block=256 |
| R3 | FDN + diffusion lengths are constants **in samples**; the modelled room shrinks with sample rate | HIGH | FDN build-up **50.52 ms @44.1 k → 23.21 ms @96 k** (2.177× = sr ratio, exact) |
| R4 | `modSamp = modDepth * 26` is a depth **in samples** | HIGH | peak wobble **23.73 ¢ @44.1 k → 10.86 ¢ @96 k** at max settings |
| R5 | the RT60 knob does not deliver its number, and drifts across rates | HIGH | at fRef=1 kHz: **+31.0 % / −13.6 % / −6.7 %** at 0.5/2.2/6.0 s; cross-rate **−5.3 % to −17.4 %** vs `samplerate_check`'s 0.3 % bar |
| R6 | **53 transcendentals per sample**, nearly all loop-invariant | HIGH (CPU) | 20 `pow` + 8 `sin` + 1 `exp` + 24 `sqrt`; hoisting them is **bit-identical** and saves **48–74 %** |
| R7 | Householder sign contradicts its own comment, `time_core.h`, and ADR-030 | HIGH (doc) | lab all-ones eigenvalue **−1**; `time_core.h:179` **+1**; comment claims +1. Audible delta measured: **≤1.2 % T30** |
| R8 | no flush-to-zero; the tail decays into the denormal band and never leaves | HIGH | state enters denormals at **t = 52.5 s** (decay 0.5 s) and is **1.98e-323 at 60 s**; never reaches exact zero |
| R9 | 192 KiB of **dead state** allocated and reset every time | MEDIUM | `erBufR` 128 KiB + `apR` 64 KiB + `dcx`/`dcy` = **11.5 %** of the 1664 KiB instance |
| R10 | the mixing-time slider is inert over **43 %** of its travel, and the readout lies | MEDIUM | `baseLen` frozen from **170 ms** to the 300 ms max; UI still prints the requested value |
| R11 | `new Array(NLINE)` allocated **per sample** in the inner loop | MEDIUM | line 446; violates the charter's *"real-time thread allocates nothing"* |
| R12 | the ER tap set has **no provenance** | MEDIUM | a closed-form curve + a hand-picked gain law + a seeded random pan; no measurement, no citation |
| R13 | diffusion allpass lengths are **integer-truncated** and degenerate at small size | MEDIUM | size 0 → 78, 58, 208, 152; **gcd(78,208) = 26** |
| R14 | `mix` is a linear crossfade; the proposed **Amount** macro makes things quieter | MEDIUM | output RMS **−12.0 dB** from mix 0 → mix 1 |
| R15 | the proposed **Motion** macro is **non-monotone** in tail quality | MEDIUM | combPeak 0.175 (off) → **0.253 (depth 0.15, worse)** → 0.147 → 0.100 |
| R16 | `extract_core.mjs` **cannot load this lab** — no golden can be generated | MEDIUM | banners are `DSP` / `UI`; the extractor requires `DSP:` / `Audio graph` |
| R17 | the lab's headline "bigger is smoother" UI claim rests on a metric blind at its own line lengths | MEDIUM | the metric reads a known L=2227 comb at **1.234 dB**, *below* its own 1.744 dB comb-free floor |
| R18 | `mulberry` implemented **twice** (lines 41 and 620) | LOW | second copy is algorithmically identical; pure duplication |
| R19 | pre-delay ring is sized in samples | LOW | 743 ms @44.1 k, **341 ms @96 k**; slider max is 200 ms, so safe today |
| R20 | `preGain`/`postGain` are provably redundant | LOW | the lab says so itself (line 555): measured 4e−17 difference |

**Struck by measurement — these are NOT findings** (each was tested and does not hold):

- **Unseeded RNG. The lab is deterministic.** Every draw is seeded: `mulberry(9173)` for the 12 ER tap pans (line 193), `mulberry(seed + age*7919 + 1)` for voice phases (line 84), and an inline mulberry copy for the impulse burst (line 620). **Zero `Math.random` in the file.** The only clock read is `document.lastModified` for the rev badge (line 797), outside any signal path. Two fresh instances render **bit-identical** 3 s impulse responses, including with every modulation at maximum. *The line 616 claim holds.*
- **Self-oscillation.** Monotone decay at `decay=12, damp=0, lowCut=20, size=1, mixTimeMs=300`: RMS 1.53e-4 @1 s → 8.22e-12 @29 s, all finite.
- **Silence in → silence out.** **Exactly zero**, 0 non-zero samples over 2 s, at three configurations including maximum feedback. (Control: an impulse peaks at 0.650 — the detector is not just reading zero.)
- **DC in the tail.** 1.13e-6 mean, 1.42e-4 worst 100 ms window — **43× under** ADR-031(b)'s 0.006 bar. The per-line HP (line 375) is doing the job.
- **FDN mode collisions from size scaling.** The FDN reads at a *fractional* delay (line 450, no rounding) and a common scale factor preserves every length ratio, so `sizeScale` cannot create collisions. A gcd-of-rounded-lengths analysis measures the rounding, not the lab. (R13 survives only because the *allpass* read is integer-truncated.)
- **Pre-delay accuracy.** Tracks the setting to **±0.08 ms** at all three rates (5→55.488, 20→70.499, 100→150.522 ms against a constant offset). The offset is the FDN's own build-up — which is R3.

---

## 1. Lab expedients that would become C++ artefacts

### 1.1 CRITICAL — the ER read index can land exactly on `buf.length` (R1)

`docs/design/reverb-lab.html:419-421`

```js
const d = Math.max(1, this.tapT[t] * p.erSize * 2 * sr + spin);
let r2 = this.erW - d; if (r2 < 0) r2 += this.erBufL.length;
const j0 = r2 | 0, f2 = r2 - j0, j1 = (j0 + 1) % this.erBufL.length;
```

`j1` is `%`-guarded. **`j0` is not.** Two ways it leaves the buffer:

**(a) the +1-ULP wrap.** `tapT[0]` is `0.006`. At 48 kHz with `erSize = 0.75`,
`d = 0.006 * 0.75 * 2 * 48000` evaluates to `432.00000000000006` — *not* the
integer 432. When the write head `erW` reaches 432, `erW - d = -5.684e-14`,
which is `< 0`, so one wrap is applied: `-5.684e-14 + 16384` rounds to
**exactly 16384** (the double spacing at that magnitude is 3.6e-12). `j0 = 16384`
on a 16384-element array → `undefined` → `NaN`. It fires **once per buffer lap**,
i.e. `sr / 16384` times per second.

**(b) plain overflow.** The longest tap is `0.081 * erSize * 2 * sr`. Above
`sr * erSize > ~101 k` that exceeds 16384 and a *single* `+= length` leaves `r2`
negative. Traced: at 96 k / erSize 1.5, `j0 = -1818`.

Traced first-non-finite value, by instrumenting the scratch slice:

| sr | erSize | first NaN | `j0` | mechanism |
|---|---|---|---|---|
| 44 100 | 1.50 | — | — | clean (`d = 793.8`, never near-integer) |
| 48 000 | 0.75 | tap 0 | **16384** | (a) ULP wrap |
| 48 000 | 1.50 | tap 0 | **16384** | (a) ULP wrap |
| 88 200 | 1.15 | tap 11 | **−47** | (b) overflow |
| 96 000 | 0.75 | tap 0 | **16384** | (a) ULP wrap |
| 96 000 | 1.50 | tap 9 | **−1818** | (b) overflow |

Blast radius, sweeping the erSize slider over its full range at its own 0.01 step,
1 s of audio each:

| sr | settings tested | settings that wipe the reverb | % | worst wipes/s |
|---|---|---|---|---|
| 44 100 | 136 | **0** | 0.0 % | 0 |
| 48 000 | 136 | 2 | 1.5 % | 3 |
| 88 200 | 136 | 36 | 26.5 % | 30 288 |
| 96 000 | 136 | **46** | **33.8 %** | **41 664** |

**The lab is clean at exactly the rate it was developed at and progressively
broken above it.** At 96 kHz a third of the ER-size slider produces wet RMS of
**exactly 0.0**.

**Why this is worse in C++, not better.** In JS an out-of-range index yields
`undefined` → `NaN`, and the NaN watchdog (line 521) catches it. In C++,
`erBufL[16384]` on a `double[16384]` is an out-of-bounds read of adjacent memory —
a *finite* garbage value. **The watchdog will not fire.** The failure mode
converts from "the reverb audibly dies" into "unrelated heap contents are
injected into a feedback loop." The JS watchdog is doing load-bearing work that
does not survive the port.

**MINIMAL DELTA — and this repo already has it.** `src/time_core.h:167` carries
exactly the missing line, with a comment pointing at where the lesson was learned:

```cpp
if (rp < 0) rp += kRBuf;
if (rp >= kRBuf) rp -= kRBuf;   // see the kEBuf note above
```

Add the symmetric guard to the lab's three fractional reads — ER (line 420),
pre-delay (line 402), FDN (line 451) — plus a clamp of `d` to `len - 4` on the
ER read (the pre-delay and FDN already have theirs). Four lines. The shipped
Room's own idiom; no new concept.

*Note the pre-delay read (line 402) is safe only by luck:* `p.preDelay * sr` at
the slider's own 0.001 s step happens to be an exact integer at 44.1/48/96 k, so
`pdW - pdSamp` is exactly 0 rather than −1 ULP. That is a coincidence of the step
size, not a property of the code. Guard it anyway.

### 1.2 CRITICAL — the modulation tick is keyed to the block index (R2)

`docs/design/reverb-lab.html:380`

```js
if ((smp & (TICK - 1)) === 0) this.stepMod(dt * TICK);
```

`smp` is the index *within the current block*, so the 16-sample control tick
re-phases at every block boundary. Measured `stepMod` calls per 44 100 samples
against the correct 2 756:

| host block | stepMod calls | effective mod rate | `modPh[0]` after 1 s |
|---|---|---|---|
| **1** | 44 100 | **16.000×** | 0.92000 |
| **7** | 6 300 | **2.286×** | 0.56000 |
| 16 | 2 757 | 1.000× | 0.24507 |
| 64 | 2 760 | 1.001× | 0.24533 |
| 100 | 3 087 | 1.120× | 0.27440 |
| 256 | 2 768 | 1.004× | 0.24604 |
| 333 | 2 793 | 1.013× | 0.24827 |
| 1024 | 2 816 | 1.022× | 0.25031 |

Audio consequence at chunk 1 and 7 vs chunk 256: **max |diff| 0.149** on a signal
whose peak is 0.65, **RMS ratio 0.912 (8.8 % error)**. Control: with `modDepth 0`
and `spinDepth 0` the same comparison is **bit-exact (0.000e+0)**, which proves
the modulation tick is the whole cause.

**This is precisely ADR-175's case.** ADR-175 rules that a topology carrying a
live cycle edge is processed sample by sample, *modules included* — i.e. this
module is called with **n = 1**. At n = 1 the lab's modulation runs **16× fast**.
A reverb dropped into any feedback cell of the FX matrix would chorus instead of
breathing, and it would do so *only* in cyclic topologies, which is the hardest
class of bug to attribute.

**MINIMAL DELTA.** One line: carry the tick counter in instance state
(`this.tickCtr`) instead of deriving it from `smp` — the idiom `swarm_core.h` and
`time_core.h` already use (`tick = (tick + 1) & (kTick - 1)`).

### 1.3 HIGH — every delay length is a constant in samples (R3)

| line | constant | unit | scaled by size? | scaled by sample rate? |
|---|---|---|---|---|
| 206 | `apLen = [142, 107, 379, 277]` | **samples** | yes (line 437) | **no** |
| 228 | `baseLen0 = [1237 … 3187]` | **samples** | yes (line 450) | **no** |
| 378 | `modSamp = modDepth * 26` | **samples** | no | **no** |
| 195 | `tapT[i] = 0.006 + (i/11)^1.35 * 0.075` | **seconds** | via `erSize` | yes — correct |
| 377 | `pdSamp = preDelay * sr` | **seconds** | — | yes — correct |

The comment at line 205 says the allpass lengths are "scaled at run time"; they
are scaled by *size*, not by *rate*. The ER taps and pre-delay are in seconds and
are correct; the FDN and diffusion are not.

Measured, with ER and diffusion bypassed so the first wet energy is the shortest
FDN line: build-up **50.522 ms @44.1 k · 46.417 ms @48 k · 23.208 ms @96 k**.
Ratio 44.1 k : 96 k = **2.177**, which is `96000/44100` to four figures. Closed
form: `1237 × 1.8 / sr`. **The modelled room is 2.18× smaller at 96 kHz.**

This is the ADR-009 class ("all slew/time-constant math is expressed in seconds
and converted to per-tick coefficients; hand-tuned per-tick constants are
banned") applied to delay lengths rather than coefficients. `time_core.h:106`
already does it right — `tSm[i] = std::pow(2, pop.v[i]) * sr` — lengths from
log2-**seconds**.

**MINIMAL DELTA.** Multiply `baseLen0` and `apLen` by `sr / 44100` at construction
and on rate change. Bit-identical at 44.1 k by construction (`sr/44100 == 1.0`
exactly in IEEE754, and `x * 1.0 == x` — the same guarantee ADR-027/036 lean on).
**This one is free**: no re-baseline needed.

### 1.4 HIGH — modulation depth is 26 samples, so the wobble halves with rate (R4)

`docs/design/reverb-lab.html:378`. The audible quantity is cents, not samples:
peak pitch deviation is `1200 log2(1/(1 − A·2πf/sr))` for depth `A` samples at
rate `f`.

| depth | rate | line | @44.1 kHz | @96 kHz |
|---|---|---|---|---|
| 0.35 (default) | 0.35 Hz | 0 | 0.550 ¢ | 0.253 ¢ |
| 0.35 (default) | 0.35 Hz | 7 | 0.963 ¢ | 0.442 ¢ |
| 1.00 (max) | 0.35 Hz | 7 | 2.752 ¢ | 1.264 ¢ |
| 1.00 (max) | 3 Hz (max) | 7 | **23.73 ¢** | **10.86 ¢** |

So "swarm modulation" now has a number: **0.55–23.7 cents at 44.1 kHz**, and the
whole range is **2.177× shallower at 96 kHz**. Fixed by the same `sr/44100` scale
as R3.

### 1.5 HIGH — the RT60 knob does not deliver its number (R5)

Measured the standard way (Schroeder T30 per octave band), which is also the fair
test of the lab's own claim at lines 322–339 that the loop-gain compensation makes
"the knob mean what it says at fRef" (fRef = 1000 Hz, line 340):

| set | sr | 125 Hz | 250 Hz | 500 Hz | **1 kHz** | 2 kHz | 4 kHz | err @1 kHz |
|---|---|---|---|---|---|---|---|---|
| 0.50 s | 44 100 | 0.712 | 0.791 | 0.631 | **0.655** | 0.601 | 0.644 | **+31.0 %** |
| 0.50 s | 48 000 | 0.657 | 0.794 | 0.628 | **0.619** | 0.572 | 0.608 | +23.8 % |
| 0.50 s | 96 000 | 0.511 | 0.602 | 0.563 | **0.541** | 0.436 | 0.393 | +8.3 % |
| 2.20 s | 44 100 | 1.612 | 1.975 | 2.166 | **1.901** | 1.510 | 1.135 | **−13.6 %** |
| 2.20 s | 96 000 | 1.302 | 1.787 | 2.144 | **1.799** | 1.166 | 0.650 | −18.2 % |
| 6.00 s | 44 100 | 3.251 | 4.911 | 6.654 | **5.596** | 3.302 | 1.555 | **−6.7 %** |
| 6.00 s | 96 000 | 2.572 | 3.770 | 5.162 | **4.623** | 1.998 | 0.842 | −22.9 % |

Cross-rate drift of the 1 kHz band, 44.1 k → 96 k: **−17.3 % / −5.3 % / −17.4 %**
at 0.5 / 2.2 / 6.0 s.

Two separate problems. (a) The **error is non-monotone in the setting** — over by
31 % at the short end, under by 14 % at the middle — so it is not a constant
calibration offset that one scale factor fixes. (b) The **cross-rate drift is
58× `samplerate_check`'s 0.3 % tolerance** (B147 audit §1.1), and is a direct
consequence of R3: shorter lines at 96 k mean more passes per second through the
in-loop filters, which is the very effect `loopComp` exists to cancel, computed
from a `sizeScale` that never saw the sample rate.

The 4.3 : 1 spread across bands at decay 6 s (1.555 s at 4 kHz vs 6.654 s at
500 Hz) is the damping working as designed and is not a defect — but it does mean
a knob labelled "decay (RT60)" is only true in one band, which the port should say
out loud.

### 1.6 HIGH — the Householder sign contradicts its own comment (R7)

`docs/design/reverb-lab.html:460-466`

```js
// Householder mixing (negated: the all-ones eigenchannel must carry +1)
const hh = 2 * sum / NLINE;
…
this.line[i][this.lw] = dif * 0.35 + (outs[i] - hh) * gi;
```

Direct arithmetic: with all `outs[i] = 1`, `sum = 8`, `hh = 2`, so
`outs[i] - hh = -1`. **The lab's all-ones eigenvalue is −1** — the *un*-negated
Householder `I − (2/N)J`. The comment claims the opposite of what the line does.

Meanwhile `src/time_core.h:179` is `h*s - outs[i]` with `h = 2/n` → all-ones
eigenvalue **+1**, and its header comment (lines 9–12) states the ADR-030(a)
requirement explicitly.

ADR-030(a), verbatim: *"Householder I−(2/N)J gives the all-ones vector eigenvalue
−1; injecting input along ones then resonates at odd/(2L) … Negated Householder
puts +1 on the ones channel → comb at k/L, which the room→note and
sympathetic-tuning semantics require. **Any future matrix/topology work must state
which eigenchannel the input excites.**"*

The lab *does* inject along ones — line 466 feeds the identical `dif` to all eight
lines — so it excites exactly the eigenchannel ADR-030 is about, and states its
eigenvalue **wrongly**.

**But the audible consequence is small, and honesty requires saying so.** Built a
sign-flipped variant in scratch (mutation control: max |as-shipped − negated| =
7.058e-3, so the mutation demonstrably took effect):

| config | variant | T30 | combPeak | roughness | tail RMS | \|DC\| |
|---|---|---|---|---|---|---|
| default mod | as-shipped **−1** | 3.179 s | 0.1470 | 2.748 dB | 1.80e-4 | 7.33e-9 |
| default mod | negated **+1** | 3.216 s | 0.1540 | 2.722 dB | 1.81e-4 | 2.40e-8 |
| mod off | as-shipped **−1** | 3.581 s | 0.1751 | 2.868 dB | 2.14e-4 | 3.84e-9 |
| mod off | negated **+1** | 3.547 s | 0.1782 | 2.684 dB | 2.24e-4 | 6.08e-8 |
| decay 6, damp 0 | as-shipped **−1** | 5.058 s | 0.0431 | 1.685 dB | 3.78e-4 | 2.43e-7 |
| decay 6, damp 0 | negated **+1** | 5.080 s | 0.0428 | 1.696 dB | 3.80e-4 | 2.82e-8 |

**≤1.2 % in T30, within noise on comb content, DC negligible in both.** The sign
matters for the *Room* (where the +1 comb at k/L is the tuned-resonance feature)
and does not much matter for a *reverb* (which wants no tuned channel at all).

So this is a **documentation defect with a real blast radius, not an audio
defect**: a porter who trusts the comment will write `+1` and get a different
engine; a porter who "fixes the code to match the comment" changes the shipped
sound. Either way the divergence must be a *decision*, not an accident.

**MINIMAL DELTA.** Correct the comment to state the eigenvalue the code actually
produces (−1) and why a reverb wants it, and record the deliberate divergence from
`time_core.h` in an ADR amendment — ADR-030(a) requires the statement, and this is
the first module to make it.

### 1.7 HIGH — no flush-to-zero; the tail decays into denormals forever (R8)

The feedback path is purely multiplicative (`gi < 1`), so the state approaches zero
asymptotically and **never reaches it**. Measured max |state| across all eight
lines plus the `lp`/`hp` filter memories, after a single impulse, with silence
thereafter:

| decay | t to \|state\| < 1e-30 | t to **denormal** (< 2.225e-308) | t to exact 0 | \|state\| at 60 s |
|---|---|---|---|---|
| 0.5 s | 5.1 s | **52.5 s** | never | **1.98e-323** |
| 2.2 s | 22.3 s | 246.1 s | never | 1.24e-78 |
| 12 s | 112.4 s | > 300 s | never | 2.96e-18 |

`grep` for any flush/epsilon guard in the feedback path: **none**.

In JS this is invisible. In C++ an idle reverb instance at a short decay setting
runs its entire 8-line FDN plus 16 filter states on denormal operands from ~52 s
after the last note, permanently. Honest caveat: the penalty is platform-dependent
— severe on x86 (10–100×), much smaller on Apple Silicon, and zero if the host has
set FTZ/DAZ. But a plugin must not *depend* on the host's FPU mode.

**MINIMAL DELTA.** One line per feedback store: `if (std::abs(v) < 1e-20) v = 0;`
— or set FTZ/DAZ in `process()`. Bit-identical for any audible signal.

### 1.8 HIGH — 53 transcendental calls per sample, nearly all loop-invariant (R6)

Counted by instrumenting `Math` around one 4096-sample `process()` call:

| config | `pow` | `sin` | `exp` | `sqrt` | total/sample |
|---|---|---|---|---|---|
| default (spin 0, input filters bypassed, bassMono 260) | 20.00 | 8.00 | 1.00 | 24.00 | **53** |
| max (spin 3, inHP 200, inLP 8000, modK 1) | 20.00 | 21.00 | 3.00 | 24.07 | **68** |
| bassMono off | 20.00 | 8.00 | 0.00 | 24.00 | 52 |

Where they come from, all inside the per-sample loop:

| line | call | count/sample | depends on |
|---|---|---|---|
| 422 | `Math.pow(p.erDecay, t * 0.4)` | 12 | **params only** |
| 428 | `Math.sqrt(0.5 * (1 ± pan))` | 24 | **params only** |
| 465 | `Math.pow(10, -3*len/(rt*sr)) * loopComp` | 8 | **params only** |
| 511 | `Math.exp(-TAU * p.bassMono / sr)` | 1 | **params only** |
| 391 / 396 | `Math.exp(-TAU * p.inHP or inLP / sr)` | 0–2 | **params only** |
| 449 | `Math.sin(TAU * this.modPh[i])` | 8 | `modPh`, which changes **only in `stepMod`, once per 16 samples** |
| 418 | `Math.sin(TAU * this.spinPh[t])` | 0–12 | `spinPh`, same — **once per 16 samples** |

**Every one is hoistable** — the first five to per-block tables, the last two to
per-tick. Built the hoisted variant in scratch and verified:

| config | max \|as-shipped − hoisted\| | CPU as-shipped | CPU hoisted | saving |
|---|---|---|---|---|
| default | **0.000e+0 BIT-IDENTICAL** | 5.39 % | 1.40 % | **74.0 %** |
| max (spin + filters + modK) | **0.000e+0 BIT-IDENTICAL** | 4.45 % | 2.31 % | **48.2 %** |
| bassMono off | **0.000e+0 BIT-IDENTICAL** | 3.98 % | 1.36 % | **65.8 %** |

*CPU is % of one core at realtime, single instance, Node, median of 5, main
context not `vm` (L0052). Node is not the C++ target — the **ratio** is the
load-bearing number, not the absolute.*

**A porting trap found while doing this, worth carrying into the brief.** The
first hoist was *arithmetically* identical but **not bit-identical** (max diff
6.939e-18), because `(interp * tapG) * pow` was re-associated to
`interp * (tapG * pow)` and float multiplication is not associative. Preserving
the multiply order made it exactly 0.000e+0. **Any "obvious" hoist in the port
must be checked for bit-identity, not assumed.** (6.9e-18 is ~1e-14 relative and
would pass the ε=1e-6 parity bar — so parity would *not* have caught it, and the
goldens would have silently encoded the wrong association.)

### 1.9 MEDIUM — the ER tap set has no provenance (R12)

`docs/design/reverb-lab.html:194-198`

```js
this.tapT[i] = 0.006 + Math.pow(i / (this.NTAP - 1), 1.35) * 0.075;   // 6..81 ms
this.tapG[i] = 1 / (1 + i * 0.55);
this.tapP[i] = rng() * 2 - 1;
```

The comment at line 187 calls this a "prime-ish tap pattern". It is neither prime
nor a pattern: **times** are a closed-form power curve with a hand-chosen exponent
1.35; **gains** are a hand-chosen hyperbolic law with constant 0.55; **pans** are a
seeded uniform draw. There is no measurement, no room, and no citation — and the
lab's own tagline (line 35) makes the ER stage the **central hypothesis** ("the
strings-section quality … comes from HERE, not from tail length").

This is the one place where the audit cannot tell you whether the design is right,
because there is nothing to check it against. The three numbers 1.35, 0.55, 0.075
are exactly the class the port will freeze forever.

**MINIMAL DELTA — before the port, not after.** Either cite a source (a measured
IR, an image-source model, a published tap set) or state in the lab that these are
tuned by ear and are free parameters. One comment either way. A ported constant
with no provenance cannot later be changed without a parity break.

### 1.10 MEDIUM — the diffusion allpasses are integer-truncated (R13)

`docs/design/reverb-lab.html:437`: `const L = Math.max(1, (a.len * sizeScale) | 0) % a.buf.length;`

Unlike the FDN (fractional, interpolated), the allpass read is an **integer**, so
ratios are *not* preserved under scaling:

| size | sizeScale | lengths | worst pairwise gcd |
|---|---|---|---|
| 0.00 | 0.550 | 78, 58, 208, 152 | **26** (78, 208) |
| 0.20 (default UI) | 1.050 | 149, 112, 397, 290 | 2 |
| 0.50 | 1.800 | 255, 192, 682, 498 | 6 |
| 1.00 | 3.050 | 433, 326, 1155, 844 | 2 |

At the smallest size two of four diffusion stages share a period of 26 samples —
a degenerate diffuser feeding the FDN precisely where the lab's own UI says the
sound is most metallic (line 540: "Small settings are the metallic ones"). The `%`
on line 437 is also a modulo where a clamp is meant; it happens never to bind
(max 1155 < 2048) but it is the wrong operator for the intent.

Coefficient and stability, for the record: `g = 0.5 + 0.25 * diffuse` ∈ [0.500,
0.750] (line 435), so `|g| < 1` and each section is unconditionally stable;
**stability margin at the slider maximum is 0.250**. No finding there.

Measured side effect worth flagging: **`diffuse = 1` makes the tail *more* combed,
not less** — combPeak 0.3068 vs 0.1470 at the default 0.7 (2.1×). The diffusion
control's top end works against the stage's stated purpose.

### 1.11 MEDIUM — 192 KiB of dead state (R9)

| buffer | size | % of instance | status |
|---|---|---|---|
| pre-delay ring `1<<15` | 256.0 KiB | 15.4 % | live (743 ms @44.1 k, **341 ms @96 k**) |
| `erBufL` `1<<14` | 128.0 KiB | 7.7 % | live |
| **`erBufR`** `1<<14` | **128.0 KiB** | **7.7 %** | **never read or written in `process()`** |
| diffusion `ap` ×4 | 64.0 KiB | 3.8 % | live |
| **`apR`** ×4 | **64.0 KiB** | **3.8 %** | **allocated (line 208) and cleared (line 282), never used** |
| FDN lines 8 × `1<<14` | 1024.0 KiB | 61.5 % | live |
| **total** | **1664.0 KiB** | | **dead: 192.0 KiB = 11.5 %** |

Plus `this.dcx` / `this.dcy` (line 242), assigned once and never referenced.

The `apR` allocation carries a `L * 1.17` length law (line 208) that reads as a
designed stereo-decorrelated diffusion path that was started and abandoned. The
port should not carry either the buffers or the intent silently.

**MINIMAL DELTA.** Delete `erBufR`, `apR`, `dcx`, `dcy` from the lab — or, if the
stereo diffusion path is wanted, finish it deliberately. Do not port the corpse.

### 1.12 MEDIUM — per-sample heap allocation (R11)

`docs/design/reverb-lab.html:446`: `const outs = new Array(NLINE);` inside the
sample loop. Harmless in JS (V8 scalar-replaces it), and in C++ it would naturally
become a stack array — but it is exactly the shape the charter's *"Real-time
thread allocates nothing"* invariant exists to catch, and `rtsafety_probe` would
be the gate. Hoist it to instance state so the intent is explicit.

### 1.13 MEDIUM — 43 % of the mixing-time slider is inert, and the readout lies (R10)

`scaleToMixTime` (lines 249–270) clamps `k` to
`cap = (LBUF - 64) / 3.05 / max(baseLen0)` = 1.679. With `k = (t/90)^(7/8)`, the
cap binds at `t = 90 · 1.679^(8/7) ≈ 162 ms`. Measured by sweeping the slider at
its own 5 ms step: `baseLen` stops changing at **170 ms** and is identical at 300 ms:

```
@165 ms: 2077,2436,2910,3323,3840,4354,4803,5351
@300 ms: 2077,2436,2910,3323,3840,4354,4803,5351   identical
```

The slider's top **130 of 300 ms (43 % of travel)** does nothing, while the UI
formatter (line 661) keeps printing the *requested* value — so the control reads
"300 ms" for a 162 ms room. The cap comment (lines 264–266) explains *why* the
clamp exists and is correct; nothing tells the user it has bound.

This is the L0023 shape inverted: not a range widened without its control, but a
control whose range exceeds what the mechanism can deliver, with no feedback.

**MINIMAL DELTA.** Either lower the slider maximum to the reachable value, or
have the readout print the achieved mixing time (`mixTimeOf` already computes it)
instead of the request.

### 1.14 The wet/dry law (R14)

`docs/design/reverb-lab.html:522`: `out = (dry*(1-mix) + wet*mix) * outG` — a
**linear** crossfade, not equal-power. Measured output RMS on a fixed impulse:

| mix | 0.00 | 0.25 | 0.50 | 0.75 | 1.00 |
|---|---|---|---|---|---|
| RMS | 2.381e-3 | 1.792e-3 | 1.227e-3 | 7.445e-4 | 5.963e-4 |

Monotone **downward**: **−12.0 dB from mix 0 to mix 1.** This is the measured
mechanism behind the lab's own note at lines 173–179 ("the reverb REDUCES the
peak at every mix setting, because `mix` crossfades and the wet is quieter than
the dry it replaces"). It is not wrong as a mix law, but it directly conflicts
with the four-role face — see §5.

### 1.15 LOW — duplication and redundancy (R18, R19, R20)

- **`mulberry` is implemented twice**: the shared generator at line 41 and an
  inline copy at lines 620–623 for the impulse burst. Verified algorithmically
  identical (`s|0` vs `z>>>0` and the `^` reassociations are all no-ops under
  `Math.imul`'s int32 coercion). The copy exists only because `BURST` is built in
  the second `<script>` block, outside the DSP slice. Pure duplication; the port
  should carry one.
- The **pre-delay ring** is `1<<15` samples = 743 ms @44.1 k but **341 ms @96 k**.
  The slider maximum is 200 ms, so it is safe today; it stops being safe if the
  range is widened or 192 k is supported. Size it from seconds.
- **`preGain` / `postGain` are provably redundant** while the path is linear —
  the lab states this itself at line 555 and measured 4e−17. Keeping both is a
  defensible bet on future saturation, but it is two host-visible parameters that
  currently do the same thing, and every parameter is frozen once shipped (L0027).

---

## 2. Cost profile

**Per sample, at the lab's defaults** (N=8 lines, 12 ER taps, 4 allpasses):

| stage | multiplies/adds | transcendental | memory touched |
|---|---|---|---|
| input filters (bypassed at defaults) | ~8 | 0–2 `exp` | — |
| pre-delay | 4 + 1 interp | 0 | 1 ring read + 1 write |
| early reflections ×12 | ~96 | 12 `pow` + 24 `sqrt` (+12 `sin` if spin > 0) | 12 interpolated reads + 1 write |
| diffusion ×4 | ~20 | 0 | 4 reads + 4 writes |
| FDN ×8 | ~90 | 8 `sin` + 8 `pow` | 8 interpolated reads + 8 writes |
| tail decorr / width / bass-mono | ~20 | 1 `exp` | — |
| **total** | **~240** | **53** | **~26 ring reads, 14 writes** |

**At maximum** (spin 3, both input filters engaged, modK 1): **68 transcendentals**;
the added 12 `sin` are the ER spin, the added 2 `exp` the input filters, and modK
adds one `sqrt`/`atan2` pair per *tick* (measured 0.50 `cos` and 0.06 `atan2` per
sample — negligible).

**Measured CPU** (Node, single instance, % of one core at realtime; ratios are the
load-bearing figures):

| config | as-shipped | after hoisting | saving |
|---|---|---|---|
| default | 5.39 % | 1.40 % | 74.0 % |
| maximum | 4.45 % | 2.31 % | 48.2 % |

**Memory**: 1664.0 KiB per instance, of which 192.0 KiB (11.5 %) is dead (§1.11).
The FDN lines are 61.5 % of it, sized `1<<14` per line — a deliberate and
documented choice (lines 210–216) so the 3.05× size range cannot run past the
buffer. At 96 kHz that 16384-sample line is 171 ms, which is less headroom than
the comment assumes.

**Lookahead / block transform: NONE.** Grep for `fft` / `lookahead` / windowing:
**0 matches.** Every stage reads strictly past samples; the pre-delay is clamped
to ≥ 1 sample (line 377). **Under ADR-175 this module is loop-ELIGIBLE** — it can
sit inside a feedback cycle and be called at n = 1 — **but only after R2 is
fixed**, because n = 1 is exactly the case where the block-keyed modulation tick
runs 16× fast. Loop eligibility and R2 are the same decision.

---

## 3. The fidelity suite the module needs

Thresholds are **measured on the lab**, not invented. Each row names the detector
and its calibration, because three of the obvious detectors are unreliable here
(§0 table).

| # | check | method | measured on the lab | proposed threshold |
|---|---|---|---|---|
| V1 | **silence in → silence out** | 2 s of zeros after reset, three configs incl. max feedback | **exactly 0**, 0 non-zero samples | `== 0.0` exactly; **plus a control that an impulse peaks > 0.1** (0.650 measured) |
| V2 | **no self-oscillation** | 30 s at decay 12, damp 0, lowCut 20, size 1, mixTime 300 | RMS 1.53e-4 @1 s → 8.22e-12 @29 s, finite | RMS at 29 s < 1e-8 · RMS at 1 s; all finite |
| V3 | **RT60 vs the knob** | Schroeder T30, octave-banded, at 1 kHz, 0.5 / 2.2 / 6.0 s | +31.0 % / −13.6 % / −6.7 % | **gate at ±10 % after the fix**; record today's numbers as the pre-fix baseline |
| V4 | **RT60 cross-rate drift** | same, 44.1 / 48 / 96 k | −5.3 % to −17.4 % | **≤ 1 %** after R3/R5 (cf. `samplerate_check`'s 0.3 % bar) |
| V5 | **comb-free tail** | `combPeak` (cepstral), late tail 0.35–4 s | **0.147** default, 0.175 mod off, **0.0117** at depth 1 | **≤ 0.05**; controls: comb-free noise must read ≤ 0.02 (0.0134), a synthetic comb must read ≥ 0.5 (0.71) |
| V6 | **pre-delay accuracy** | first wet arrival, ER-only path, 44.1 / 48 / 96 k | tracks to **±0.08 ms** | ≤ 1 sample or 0.1 ms, whichever is larger |
| V7 | **block-size independence** | chunk 1 / 7 / 64 / 256 / 333 vs 256 | **max diff 0.149, RMS ratio 0.912** at chunk 1 & 7 | **bit-identical** at every chunk size (achievable — the mod-off control already is) |
| V8 | **n = 1 (ADR-175)** | chunk 1, count `stepMod` calls per second | **16.000× the correct rate** | exactly `sr/16` calls/s at every chunk size |
| V9 | **denormal silence** | max \|state\| across all lines + filters, 300 s after one impulse, decay 0.5 s | **denormal at 52.5 s, 1.98e-323 at 60 s, never zero** | state reaches **exact 0** within 60 s of the last input |
| V10 | **modulation depth in cents** | peak `d(delay)/dt` → cents, per line, both rates | **0.55–23.73 ¢ @44.1 k; 2.177× less @96 k** | cents within 1 % across 44.1 / 48 / 96 k at matched settings |
| V11 | **index safety** | sweep erSize ×  sr at the slider's own step, count watchdog fires | **0 / 136 @44.1 k; 46 / 136 @96 k** | **zero** watchdog fires anywhere in the slider space, at every supported rate |
| V12 | **DC in the tail** | mean and worst 100 ms window, 12 s, lowCut at minimum | 1.13e-6 / 1.42e-4 | ≤ 0.006 (ADR-031 b) — **already passes, pin it** |
| V13 | **CPU per instance** | default and maximum settings | 5.39 % / 4.45 % (Node) | re-measure in C++ Release; budget as a fraction of the FX matrix total |
| V14 | **matrix sign** | assert the all-ones eigenvalue numerically | **−1** | assert the intended value explicitly, whichever the ADR rules — this is ADR-030(a)'s "must state which eigenchannel" made executable |

**Two detectors must NOT be used, and the suite should say so**: raw spectral
flatness (reads 0.301 on a comb-free but tilted tail — it measures damping), and
1/3-octave roughness (reads a known L=2227 comb at 1.234 dB, *below* its own
1.744 dB comb-free floor — structurally blind at the lab's own line lengths).

**Consequence for the lab's own UI claim (R17).** Line 540 states "measured tail
roughness falls monotonically from 1.68 dB at minimum size to 1.15 dB at maximum,
against a 0.20 dB floor for ideal decaying noise." Reproduced the *trend*
(roughness 4.288 dB at size 0 → 2.381 dB at size 1) — but `combPeak` over the same
sweep is essentially flat (**0.1547 → 0.1396**). Longer lines put the comb teeth
closer together in Hz, so a fixed-fraction-of-an-octave smoother averages more of
them away. **The measured trend is largely the metric's bandwidth, not the tail's
quality**, and "bigger is smoother" is not established by it. This is the L0016
trap in its purest form: a plausible ranking, in the predicted direction, from an
uncalibrated detector.

---

## 4. Room vs Reverb — with numbers

Both rendered from a unit impulse and measured with the same calibrated detectors.
Room is `src/time_core.h` mode 1 via `processExternal`, `noise=0`, `mix=1`,
`nb=8`, `K=0`, `driftDepth=0`.

| engine | setting | T30 | combPeak | tail RMS @1 s | line budget |
|---|---|---|---|---|---|
| **Room** (mode 1) | regen 0.30 | 0.245 s | **0.5669** | 3.65e-14 | `kRBuf` 8192 = 186 ms |
| **Room** | regen 0.50 | 0.414 s | **0.3587** | 5.82e-11 | " |
| **Room** | regen 0.80 | 1.066 s | **0.2008** | 4.06e-6 | " |
| **Room** | regen 0.95 | 4.289 s | **0.2066** | 5.17e-4 | " |
| **Reverb lab** | decay 0.50 s | 0.659 s | 0.5299 | 1.47e-9 | `LBUF` 16384 = 372 ms |
| **Reverb lab** | decay 2.20 s | 1.807 s | **0.1818** | 1.90e-5 | " |
| **Reverb lab** | decay 6.00 s | 5.023 s | **0.1362** | 1.59e-4 | " |
| **Reverb lab** | decay 12.0 s | 8.798 s | **0.1270** | 2.67e-4 | " |

Room's full RT60 range: `regen 0.05 → 0.11 s · 0.20 → 0.17 s · 0.50 → 0.41 s ·
0.80 → 1.07 s · 0.95 → 4.29 s · 0.99 → 18.82 s` — reachable but wildly nonlinear
in the knob. The lab's range is **0.2–12 s as a calibrated time** (with the V3
error).

**What Room has that the lab does not:**

- delay lengths from `std::pow(2, pop.v[i]) * sr` (`time_core.h:106`) — **seconds,
  sample-rate-correct**, which is the lab's R3 already solved;
- the shared force core (ADR-034): gravity, drift, inertia and coupling K move the
  **line lengths themselves**, including rhythmic and sympathetic attractors — a
  whole modulation dimension the lab has no equivalent of;
- `std::tanh` in the feedback store (`time_core.h:184`) — bounded under any regen;
- the **two-sided index guard** (`time_core.h:167`) the lab is missing (R1);
- a per-line DC blocker at 0.0006 (`time_core.h:174`), the ADR-031(b) law;
- **L0-19 / L0-20 / L0-21 oracle rows** in `tools/time_check.cpp`, including an
  explicit matrix-sign guard.

**What the lab has that Room does not:** pre-delay; 12-tap early reflections with
spin; a 4-stage diffusion chain; decay expressed as a **time** with loop-gain
compensation so size and decay are independent; length-compensated per-line damping
and low cut; tail decorrelation, envelop, width, bass-mono; a mixing-time control.
Room has none of these — it is an FDN and nothing else.

**The honest reading of "Room on trial, may be covered by Reverb"
(`docs/proposals/fx-matrix-rework.md:388`): the measurement refutes the
substitution.** Room's combPeak never drops below **0.2008** across its entire
regen range — 15× the comb-free floor — and that is **by design**: ADR-030(a) says
the +1 eigenchannel produces "comb at k/L, which the room→note and
sympathetic-tuning semantics require." Room is a **tuned resonator** whose combs
are its feature; the Reverb is a **diffuser** whose combs are its defect. They sit
at opposite ends of the same metric. A Reverb that reached Room's combPeak would be
a broken Reverb, and a Room that reached the Reverb's would have lost its reason to
exist. Add that Room's lines cap at 186 ms and it cannot model a large space at all.

**Recommendation for the human: keep both, as two types — and rename Room.**
The word "Room" is what invites the substitution question, and the measurement says
Room is not a room. Calling it what ADR-030 says it is (a sympathetic resonator /
tuned space) resolves the roster row without a deletion, and the human's stated
preference — *"I do like how Room sounds"* — is a preference for a sound the Reverb
structurally cannot make. **One audible difference to A/B before ruling**: Room at
regen 0.95 (T30 4.29 s, combPeak 0.207) against the lab at decay 6 s (T30 5.02 s,
combPeak 0.136) — same ballpark decay, 1.5× the comb content, and Room's is pitched.

---

## 5. The port plan

### 5.1 The four-role face (ADR-169) — the brief's mapping, checked

ADR-169 fixes four slots: **{Amount, Tone, Motion, Regen}**. Measured each
proposed binding for monotonicity and audible range:

| role | brief's proposal | lab param | measured | verdict |
|---|---|---|---|---|
| **Regen** | decay | `decay` (line 541, 0.2–12 s) | T30 1.00 / 1.66 / 3.18 / 6.80 / 8.80 s at knob 1/2/4/8/12 — **monotone** | **CONFIRM.** ADR-169's "Regen = feedback"; for a reverb that is RT60. |
| **Tone** | damping | `damp` (line 542) | tail centroid 2486 → 373 Hz, **monotone down, 6.67× range** | **CONFIRM.** Cleanest of the four. |
| **Motion** | swarm modulation | `modDepth` (line 577) | combPeak 0.175 → **0.253** → 0.147 → 0.122 → 0.100 at depth 0/0.15/0.35/0.6/1.0 — **NOT monotone** | **CORRECT.** See below. |
| **Amount** | wet | `mix` (line 537) | output RMS **falls 12.0 dB** from mix 0 to mix 1 | **CORRECT.** See below. |

**Motion needs correcting.** A small amount of modulation measures *worse* than
none (0.253 vs 0.175). The first 15 % of the macro's travel would make the tail
more combed — a macro that gets worse before it gets better is not shippable.
Two candidate fixes, both cheap and both needing a second detector before choosing:
(a) map Motion to `modDepth` over a **restricted range starting above the dip**;
(b) map Motion to `modDepth` **and** `modRate` together, since the dip is plausibly
a slow shallow sweep the cepstral detector locks onto. **Do not set this curve from
one detector** — the V5 detector is calibrated for presence, not for the shape of
this dip. This is an open question for the lead, not a decision for the port.

**Amount needs correcting.** `mix` as Amount means the macro at 1.0 is **12 dB
quieter** than at 0.0 — an "Amount" macro that reduces level as it increases.
Either bind Amount to `erSend + envelop` (the two wet-level axes, leaving `mix` as
its own control), or make `mix` equal-power. The former is closer to ADR-169's
intent (Amount = how much of the effect) and leaves the dry/wet law alone.

**The gap ADR-169 forces, and the lead must surface.** The roster's own proposed
face (`fx-matrix-rework.md:389`) is **six** controls — `mix · pre-delay · size ·
decay · damping · width`. ADR-169 allows **four**. Whatever the mapping, **`size`
loses its slot** — and `size` is the room-size cue, the second most consequential
control after decay, and the one whose range the lab deliberately widened to
3.05×. Four roles cannot carry this module's surface. That is a genuine collision
between ADR-169 and this module, and it is the lead's to put to the human, not the
implementer's to resolve by picking a favourite.

### 5.2 Parity strategy

Follow the B147 method: **bit-parity where the lab is authoritative, invariant
oracles where it structurally cannot see** (L0031).

**Bit-parity (ε = 0, not 1e-6)** — the lab is a pure deterministic function of its
parameters, it is seeded throughout, and the hoisting experiment proved 0.000e+0
is achievable. Propose a golden set at:

1. defaults, 3 s impulse, 44.1 kHz, block 256;
2. every modulation at maximum (`modDepth 1, modRate 3, modK 1, spinDepth 3,
   spinRate 2`) — this is the path most likely to be re-derived wrongly;
3. `size 0` and `size 1` (the extremes of the scaling law, and where R13 bites);
4. `mixTimeMs 150` (the `scaleToMixTime` path, below the cap);
5. `erSend 0` (tail only) and `envelop 0` (ER only) — the two stages separated;
6. `bassMono 0` vs 260 (the M/S crossover branch).

Anchor each golden with the FNV-1a of the buffer, as `extract_core`-based
generators already do.

**Behavioural / invariant oracles** for everything parity cannot certify — which
is most of §3. In particular V4, V7, V8, V9, V10 and V11 **all pass trivially
against the lab today** (the lab *is* the reference, so it agrees with itself),
which is exactly L0031's blindness (A): *defects the reference shares are not
missed but certified*. V11 is the sharpest case — bit-parity against the lab at
96 kHz would faithfully reproduce **silence**.

**Cross-rate goldens are not possible until R3 is fixed**, because the lab's
44.1 kHz output is not a sample-rate-independent function. Sequence accordingly:
fix in the lab first, re-baseline the lab's own numbers, *then* generate goldens.

### 5.3 What the lab's UI does that must NOT be ported

- **`MiniSwarm`** (lines 51–128) — an audition source, explicitly "Not the
  reference core — just enough" (line 50). It is not the engine and has its own
  simplified voice model; porting it would create a second, divergent swarm.
- **`BURST`** (lines 617–626) and its duplicate mulberry — test excitation.
- **The peak/clip meter** (`this.peak`, `this.clip`, lines 180, 524–527, 713–724).
  The DSP writes UI state on the audio thread. In the plugin this is a host-side
  meter or nothing; the `peak`/`clip` fields must not cross into the core.
- **The `rev`/fingerprint badge** (lines 771–802) — the only `new Date` in the file
  and the only reason a clock appears at all.
- **`scriptProcessor` / `AudioContext` wiring, canvases, key map, PANIC** — all
  adapter concerns.
- **The NaN watchdog (line 521) must be ported but must not be relied on.** It is
  correct defence-in-depth per ADR-032(b), but in C++ it will not catch R1 (an
  out-of-bounds read returns finite garbage, not NaN). Port it *and* fix R1; do not
  treat it as the fix.

### 5.4 Ranked list — fix in the LAB before the port

A lab edit is cheap; a port then re-port is not. In order:

| # | fix | why first | cost |
|---|---|---|---|
| **1** | **R1** — two-sided index guard on all three fractional reads + clamp `d ≤ len-4` on the ER read | the only defect that produces silence, it is invisible at 44.1 k, and in C++ it converts from a caught NaN into an uncaught OOB read. `time_core.h:167` is the pattern | **4 lines** |
| **2** | **R2** — tick counter in instance state, not `smp & 15` | decides ADR-175 loop membership; at n = 1 the module is 16× wrong. Also unblocks V7 (bit-identical block independence) | **1 line** |
| **3** | **R3 + R4** — scale `baseLen0`, `apLen`, `modSamp` by `sr/44100` at construction | bit-identical at 44.1 k (`x * 1.0 == x`), so it costs no re-baseline **if done before goldens**. Done after, it costs every golden | **3 lines** |
| **4** | **R7** — correct the Householder comment to match the code, and record the deliberate divergence from `time_core.h` | ADR-030(a) *requires* the statement; the current comment actively misleads the porter. Audible delta is ≤1.2 %, so this is a comment + ADR, not a code change | **1 comment + 1 ADR amendment** |
| **5** | **R9 + R11** — delete `erBufR`, `apR`, `dcx`, `dcy`; hoist `outs` | do not port 192 KiB of corpse or an allocation into the RT path; deleting is also how the abandoned stereo-diffusion intent gets a decision | **~6 lines deleted** |
| **6** | **R12** — state the ER tap set's provenance, or admit it is by ear | these three constants are the lab's central hypothesis and will be frozen by the first golden. Cheapest possible fix now, unfixable later | **1 comment** |
| **7** | **R10** — clamp the mixing-time slider to its reachable range, or print the achieved value | a control that lies is worse than one that is absent; it will otherwise be ported with the lie | **1 line** |
| **8** | **R8** — flush-to-zero in the feedback store | could be done in the C++ port instead (it is a C++-only symptom), but doing it in the lab keeps the two in parity | **1 line** |
| **9** | **R6** — hoist the 53 transcendentals | **proven bit-identical** and worth 48–74 %. Do it in the lab so the golden encodes the hoisted association order, and the port inherits both the speed and the multiply order | **~20 lines** |
| **10** | **R16** — rename the banners to `DSP: reverb` / `Audio graph` | without this, `extract_core.mjs` cannot load the lab and no golden can be generated by the existing tooling at all. **This is a prerequisite for any parity work** | **2 lines** |
| — | **R5, R13, R14, R15, R17** | each needs a ruling or a second measurement first, not a fix: R5 (what tolerance is acceptable), R13 (fractional allpass reads or accept the truncation), R14/R15 (the macro face, §5.1), R17 (retire the UI claim or re-measure it) | lead/human |

Items 1–3 and 10 are **~10 lines total** and unblock everything else. Items 4, 6, 7
are comments and one clamp. Item 9 is the only substantial edit, and it is
bit-identity-gated.

---

## 6. Oracle-coverage gaps `./verify full` cannot see today

- `verify` contains **zero** references to `reverb`. The lab's only coverage is
  `lab_load_check.mjs`, which reports `OK reverb-lab.html` — it proves the file
  *parses and runs*, nothing about what it computes.
- `tools/golden/extract_core.mjs` **throws** on this lab (banner mismatch, R16), so
  the existing golden machinery cannot reach it even if someone tried.
- Every item in §3 is therefore uncovered. The ones that would have caught a
  finding in this audit: **V11** (R1), **V7/V8** (R2), **V4/V10** (R3/R4),
  **V9** (R8), **V14** (R7).
- Uncovered even after §3 is built, and worth naming so it is a known boundary:
  the **ER tap set** (R12) has no oracle possible — there is nothing to check it
  against until someone states what it is supposed to be.

