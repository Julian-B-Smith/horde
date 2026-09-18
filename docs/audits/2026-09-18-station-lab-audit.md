# B153 layer 1 — STATION lab audit (read-only)

Critic, 2026-09-18. Repo at `d749430` (`main`). **Nothing in the repo was edited.**
Every experiment ran from a headless slice of `reference/station.html` evaluated at
run time under Node v24.10.0 (probes in `…/scratchpad/b153/`), so the protected
reference is never forked (the `extract_core.mjs` doctrine, LIBRARY L0001).

**Method.** `reference/station.html` lines 141–294 (`/* ===== STATION` banner →
`let audioErr=''`) are `new Function`-evaluated and the module scope returned.
Spectra: 65 536-point FFT, Blackman-Harris window, 48 kHz unless stated.
Aliasing floor = worst non-harmonic bin, dB below the fundamental, measured with
`master = 0.02` so the lab's output `tanh` is linear to −102 dB THD (§1.3) — i.e.
the numbers are the *operator's*, not the monitor chain's.

**Calibrated detectors.** Every probe carries a control that must read the null:
the parameter-step probe reads a Wave-RAM edit on a SIN operator (0.034 = the
natural inter-sample step, §2.6); the release-tail probe reads a never-released
voice (3.000 s, still 1 voice, §2.9); the branch-reduction probe proves
bit-identity (max|diff| = 0) before quoting a speedup (§3.3).

---

## 0. Headline

| # | finding | severity | measured |
|---|---|---|---|
| S1 | The lab has **no extractable core class and neither `extract_core.mjs` banner** — it is the only prototype whose DSP cannot be loaded by the house idiom | HIGH (process) | `grep -c '/\* =\+ DSP:'` = 0, `'Audio graph'` = 0, `class ` = 0; `state`/`render` are module globals (`station.html:147,229`) |
| S2 | **Noise output is ±0.7, spec §6 says ±1** — an undeclared divergence that shifts every noise-as-PM index by ×1.43 | HIGH | `station.html:252`; **−3.098 dB**; spec §6:100 vs §11 lists no such divergence |
| S3 | The PM constant is a **truncated `0.1591549`**; a C++ port writing `1/(2π)` misses the ε=1e-6 parity gate | HIGH | rel err 2.708e-7 → RMS diff **2.156e-6 at index 8** (2.2× over ε), **7.635e-7 at index 2.6** (the default patch's own cell — 76 % of the whole budget from one literal) |
| S4 | **All voices seed the LFSR to the same `0x7FFF`** → noise sums coherently across a chord | HIGH | 4 voices / 1 voice RMS ratio = **3.999** (coherent = 4.00, independent = 2.00) with KEYTRK off |
| S5 | `dt` is clamped at **0.45·SR** — the *default patch's* OP3 (ratio 14) plays the **wrong pitch above MIDI 89 at 44.1/48 k and the right pitch at 96 k** | HIGH | MIDI 96 × 14 = 29 302 Hz → 19 845 Hz @44.1 k, 21 600 Hz @48 k, 29 302 Hz @96 k (`station.html:262`) |
| S6 | **Zero parameter smoothing anywhere**, though spec §4 requires 5 ms on matrix cells | HIGH | matrix step |Δ| = **0.4756** vs a 0.034 control floor (**14×**); wave switch 0.3795; LVL 0.2548 |
| S7 | The "band-limited" PURE branch is **not band-limited for TRI/QTR/DRW** and is only ~9 dB better than raw for SAW/PLS at the top of the keyboard | MEDIUM | TRI/QTR pure **−44.8 dB** @MIDI 96; SAW pure −30.0 vs raw −21.3; DRW pure −43.7 (table §2.1) |
| S8 | **No DC blocker** and §1 forbids an internal filter; PLS at pw = 0.1 sits at **−1.9 dB DC**, SHORT noise at −29.9 dB, self-feedback at −26 dB | MEDIUM | §2.7 table |
| S9 | Turning an op OFF mid-note **freezes its envelope**; re-enabling clicks | MEDIUM | env stuck at 0.32392/stage 1 for 500 ms (`station.html:261` skips `envStep`) |
| S10 | Pan is **linear, not constant-power** — hard pan is −3.01 dB of total power; the spec states no law | MEDIUM | §2.8 |
| S11 | Both render branches are always computed; op1's frequency is computed **twice per sample** | MEDIUM (CPU) | 6 `sin` + 4 `pow` per sample per voice at the default patch; the two exact fixes are **−5.2 %** in Node and **bit-identical** |
| S12 | §12's "≤ 2 % of one core, 16 voices" budget requires a **≥ 9.4× JS→C++ ratio** at the max patch | MEDIUM | Node: **18.71 %** of realtime at 16 voices max patch, 11.1 % at the default patch |
| S13 | Release runs to **163 % of the stated time** (absolute 0.0005 cutoff) | LOW | REL = 260 ms → tail **0.4235 s**; SR drift only 0.066 % |
| S14 | RATIO range in the lab is **0.5–14**, spec §10 says **0.25–16**; §11.2 declares only the *stepping* divergent | LOW | `station.html:364` vs spec §3.1/§10 |

**Struck by measurement — NOT findings.** Determinism (§1, exact), block-size
independence (exact at chunks 1/7/64/256/333), silence → silence (exact 0),
denormals (0 subnormals in 6 s post-release), envelope sample-rate portability
(attack 1.59 %, decay-end 0.15 %), LFSR periods (32767 / 93, exactly spec §6),
PM matrix semantics (Bessel-exact to **0.50 dB** over index 0.5–8), self-feedback
stability (bounded at |op| ≤ 1.000 000 at index 8 over 10 s, 0 non-finite).

---

## 1. Determinism — CLEAN

### 1.1 Draw sites
The lab has **exactly one** RNG site and it is seeded (`station.html:165-176`,
ADR-122, seeded 2026-09-10 — **verified today**):

- `mulberry32` at `:165`, `rndStream` at `:169`, reseeded from `state.seed` on
  every RND click (`:508`). Seed 1024 and seed 99 produce different 32-value
  tables; the same seed reproduces its table exactly.
- `grep 'Math.random' reference/station.html` → **1 hit, inside a comment** (`:168`)
  recording the pre-fix state. Zero live calls.

### 1.2 Clock reads
`performance.now()` appears **twice**, both at `:303,:306`, inside
`spn.onaudioprocess` and used **only** for the `cpuMs` display. It never touches
a sample. SPEC §5.7 is satisfied in the DSP; the two calls are browser-monitoring
scaffolding and are on the do-not-port list (§6.3).

### 1.3 LFSR seed / period / reset
`mkVoice` sets `lfsr: 0x7FFF` (`:188`) — constant, per voice, reset on every note.
Periods measured by direct iteration of `:250-252`:

| mode | tap | period | pre-period | ones | duty | spec §6 |
|---|---|---|---|---|---|---|
| LONG | bit0 ⊕ bit1 | **32767** | 0 | 16384 | 0.5000 | 32767 ✓ |
| SHORT | bit0 ⊕ bit6 | **93** | 0 | 48 | 0.5161 | 93 ✓ |

Both are pure cycles (pre-period 0), so the sequence is exactly reproducible from
the seed. **But see S4 (§2.4): a constant seed is deterministic and musically wrong.**

### 1.4 Two fresh instances, everything at max
3 s at 48 kHz, two voices (MIDI 60 + 67), all 12 matrix cells at 8, three DRW ops
at PURE = 0.5 / QNT = 16, looping envelopes, SHORT noise with KEYTRK at rate 0.8,
pitch env ±24 st:

```
max|diff| = 0            FNV-1a(L,R) run A = bc9eea68afc8bfd9
nonzero samples = 144000  FNV-1a(L,R) run B = bc9eea68afc8bfd9
```

**Verdict: DETERMINISM CLEAN.** Nothing blocks ingestion on SPEC §5.7 grounds.
The ADR-122 sanction is confirmed spent.

---

## 2. Lab expedients that would become C++ artefacts

### 2.1 Aliasing across the keyboard — S7

`master = 0.02`, one operator, no PM, steady state. dB below fundamental, worst
non-harmonic bin (bin shown).

**PURE = 1 (the "band-limited" branch)**

| wave | MIDI 36 | MIDI 60 | MIDI 84 | MIDI 96 |
|---|---|---|---|---|
| SIN | −94.3 | −96.9 | −98.2 | −93.2 |
| TRI | −94.2 | −78.7 | −54.4 | **−44.8** |
| SAW | −58.9 | −47.0 | −35.0 | **−30.0** |
| PLS | −59.2 | −47.4 | −35.0 | **−32.9** |
| QTR | −94.2 | −78.7 | −54.4 | **−44.8** |
| DRW | −89.5 | −65.2 | −44.9 | **−43.7** |

**PURE = 0 (raw branch, QNT = OFF)**

| wave | MIDI 36 | MIDI 60 | MIDI 84 | MIDI 96 |
|---|---|---|---|---|
| SIN | −94.3 | −96.9 | −98.2 | −93.2 |
| TRI | −94.2 | −78.7 | −54.4 | −44.8 |
| SAW | −51.0 | −39.1 | −27.1 | −21.3 |
| PLS | −51.1 | −39.3 | −27.1 | −22.5 |
| QTR | −51.6 | −41.5 | −29.2 | −28.9 |
| DRW | −47.2 | −35.2 | −25.1 | −24.9 |

Three readings, each a C++ consequence:

1. **TRI and QTR pure are byte-identical** (same numbers, same bins) because
   `waveOut` case 4 sets `pure = tr(p)` — the same expression as case 1
   (`station.html:217,222`). Spec §3.3 sanctions this ("QTR renders as smooth
   triangle"), so it is *correct*, but it means **the pure branch of TRI is a naive
   triangle**. Spec §3.3's own condition — "BLAMP acceptable for TRI **if aliasing
   is measurable**" — is **triggered**: −44.8 dB at MIDI 96 is measurable by 50 dB.
   The build must add BLAMP, and that is a deliberate divergence needing an ADR
   because it breaks parity on two waveforms.
2. **polyBLEP buys only 8.7 dB at the top.** SAW pure −30.0 vs raw −21.3 at MIDI
   96. `blep()` (`:202-206`) is the 2-sample first-order polyBLEP; it corrects the
   step but not the slope. This is the same class as B109 on the SAW engine. If
   STATION is expected to sit at the top of the keyboard (its chiptune brief says
   yes), 2× oversampling on the pure branch is the honest answer — and an ADR.
3. **DRW pure is linear interpolation** (`:224-225`) and reads −43.7 dB at MIDI 96.
   §5 already declares additive/mipmap band-limiting as a required divergence; this
   is the number that divergence has to beat.

**QNT phase quantization** (raw branch, SIN), the chip-degradation control:

| QNT | MIDI 36 | MIDI 60 | MIDI 84 | MIDI 96 |
|---|---|---|---|---|
| OFF | −94.3 | −96.9 | −98.2 | −93.2 |
| 4 | −51.0 | −39.3 | −27.1 | −22.5 |
| 8 | −51.1 | −39.6 | −27.1 | −23.4 |
| 16 | −51.3 | −39.6 | −30.4 | −23.4 |
| 32 | −51.4 | −39.6 | −30.4 | −29.5 |
| 64 | −51.4 | −42.0 | −35.9 | −35.6 |

QNT is *supposed* to alias — it is the effect. But note the **inversion**: coarser
quantization aliases *less* at the top (QNT 4 = −22.5, QNT 64 = −35.6 at MIDI 96),
because coarse steps push the fold-back products down in level while fine steps
put energy near Nyquist. Whatever the C++ does, this ordering is the parity
signature to preserve; a "cleaner" implementation that monotonically worsens with
QNT has changed the instrument.

### 2.2 Per-tick / sample-rate-bound constants — ADR-009 status: **CLEAN**

This is the finding the brief expected and it is **not present**. Every time
constant in the lab is written in seconds and multiplied by `SR` at use:

| quantity | site | form |
|---|---|---|
| attack | `:193` | `Math.max(0.0005, p.a/1000)*SR`, `lvl += 1/t` |
| decay | `:194` | `t = max(0.001, p.d/1000)*SR`, `lvl += (s−lvl)*(4.6/t)` |
| release | `:197` | same shape with `p.r` |
| pitch env | `:237-238` | `dec = max(0.005, s.pitchEnv.dec/1000)*SR` |
| noise clock | `:245-247` | `nf/SR` per sample |

Measured drift over 44.1 / 48 / 96 kHz (A = 50 ms, D = 400 ms, S = 0.3, R = 200 ms):

| landmark | 44.1 k | 48 k | 96 k | drift |
|---|---|---|---|---|
| attack complete (s) | 0.05079 | 0.05067 | 0.05000 | **1.587 %** |
| decay complete (s) | 0.49923 | 0.50000 | 0.49933 | **0.154 %** |
| release tail (s) | 0.330884 | 0.330667 | 0.330667 | **0.066 %** |

Compare the SAW engine's `0.08` per-tick smoother at **52.9 %** drift (B147 A1).
**STATION inherits none of that class.** The residual 1.59 % attack drift is the
one-sample quantization of `1/t` and is below `samplerate_check`'s 0.3 %... no —
it is **5.3× over** that tolerance. Worth a threshold decision, not a rewrite:
the fix is to start the attack from the fractional residue rather than 0.

The **decay/release one-pole uses `4.6/t` directly, not `1 − exp(−4.6/t)`** — a
forward-Euler approximation. It is stable for every UI-reachable value (worst case
D = 5 ms at 44.1 kHz gives 4.6/220.5 = 0.0209), but a C++ port that "corrects" it
to the exact coefficient changes every envelope shape and breaks parity. **Keep the
Euler form and comment why**, or divergence-ADR it.

### 2.3 The PM depth constant is truncated — S3 (**the single most expensive port trap**)

`station.html:267`:
```
let p=v.ph[i]+pm*0.1591549;
```
`1/(2π) = 0.15915494309189535`. The literal is truncated at the 7th digit —
**relative error 2.708e-7**. Spec §4 writes the law as `p += cell · out_src / 2π`,
so a C++ author reading the spec writes `1.0/(2.0*M_PI)` and is *correct by the
spec and wrong by the oracle*. Measured, default `master = 0.75`, SAW carrier /
sine modulator ratio 7:1, 2 s:

| PM index | RMS diff | max abs diff | vs ε = 1e-6 |
|---|---|---|---|
| 1 | 1.964e-7 | 4.620e-6 | passes |
| **2.6** (the default patch's own cell) | **7.635e-7** | 1.461e-5 | passes — at **76 % of the entire budget** |
| 4 | 7.224e-7 | 1.965e-5 | passes |
| **8** (cell max) | **2.156e-6** | 4.522e-5 | **FAILS (2.2×)** |

This is the CLAUDE.md §Domain invariant "*parity with the JS reference (L0-1,
ε=1e-6 RMS)*" failing on a one-character difference. The port brief must state the
literal verbatim.

### 2.4 Every voice seeds the LFSR identically — S4

`station.html:188`: `lfsr: 0x7FFF` for every voice, every note. Spec §6:103 says
"LFSR seeds nonzero **per voice**; seed value is implementer's choice but must be
deterministic per note" — the lab is deterministic (legal) but not per-voice
distinct. Consequence, 4 voices vs 1 voice noise RMS:

| KEYTRK | 1 voice | 4 voices | ratio | interpretation |
|---|---|---|---|---|
| off | 6.860e-3 | 2.743e-2 | **3.999** | perfectly **coherent** (+12.0 dB, not +6.0 dB) |
| on (different notes) | 6.860e-3 | 1.366e-2 | 1.992 | independent — decorrelated only by clock rate |

With KEYTRK off the noise channel is not noise: it is the *same waveform* summed
N times, +6.02 dB hotter than a real noise bed at 4 voices and **+12.0 dB at 16**,
and it images to the centre instead of spreading. KEYTRK on masks it *only* while
the notes differ; two voices on the same pitch (a retrigger before release, or an
engine-layering unison) are coherent again.

The fix preserves replay determinism exactly: seed from a per-voice deterministic
hash, e.g. `lfsr = (mulberry32(note*2654435761u ^ voiceIndex)() * 32767) | 1`.

### 2.5 The `dt` clamp is a sample-rate-dependent pitch error — S5

`station.html:262`: `const dt=Math.min(opFreq(o,base)/SR, 0.45);`

| SR | clamp | default-patch OP3 (ratio 14) at MIDI 96 | clamped? | actual |
|---|---|---|---|---|
| 44 100 | 19 845 Hz | 29 302 Hz | **YES** | 19 845 Hz |
| 48 000 | 21 600 Hz | 29 302 Hz | **YES** | 21 600 Hz |
| 96 000 | 43 200 Hz | 29 302 Hz | no | 29 302 Hz |

The clamp engages above MIDI **89.4** (F♯6) with the shipped boot patch. Above it
the operator is not merely mistuned — as a modulator its whole sideband structure
changes, so the *timbre* of the top 1.5 octaves depends on the host's sample rate.
Two patches saved at 48 k and reopened at 96 k are different instruments.

Related, and **spec-level**: `nf = s.noise.rate*SR*0.5` (`:245`). Spec §6 defines
RATE as "normalized 0–1 → 0–SR/2", so the spec *mandates* this. Measured, rate = 0.35:
7 717 Hz @44.1 k, 8 400 Hz @48 k, **16 800 Hz @96 k**. A saved patch's noise
character is not sample-rate portable, by design. Recommend the parameter be Hz
with an SR-independent taper; that is a **spec** change, not a lab change.

### 2.6 No smoothing anywhere — S6

Discontinuity across one parameter write, measured as |first sample after − last
sample before|, against the **natural inter-sample step at that instant (0.0342)**
as the control floor. 261.6 Hz, default EP patch, `master = 0.75`.

| change | step \|Δ\| | × the natural step |
|---|---|---|
| matrix[O2→O1] 2.6 → 8.0 | **0.4756** | **13.9×** |
| OP1 wave SIN → SAW | **0.3795** | **11.1×** |
| OP1 LVL 0.9 → 0.1 | **0.2548** | **7.4×** |
| MASTER 0.75 → 0.2 | **0.2154** | **6.3×** |
| ALG recall (matrix + levels together) | 0.0039 | 0.1× |
| OP1 PURE 1 → 0 | 0.0345 | 1.0× (floor) |
| OP1 QNT OFF → 4 | 0.0345 | 1.0× (floor) |
| OP1 PAN 0 → −1 | 0.0345 | 1.0× (floor) |
| **Wave RAM edit while op is SIN (control)** | **0.0345** | **1.0× — reads the null** |

Read this carefully — the calibrated control is what makes it trustworthy. Four
parameters produce a real step of 6–14× the signal's own slope; the other four
read exactly the control floor, i.e. the detector is not simply confirming
what it expected.

Spec §4 requires 5 ms smoothing **on matrix cells only**. The measurement says
**LVL and MASTER need it too**, and **wave select cannot be smoothed at all** — it
needs an equal-power crossfade or a zero-crossing switch. The single 0.0039 reading
for the ALG recall is luck, not safety: that recall happened to land near a zero
crossing; the individual cell write in the row above is the same class of edit at
122× the step.

### 2.7 DC — S8

Spec §1 says STATION has **no internal filter** and §2's diagram shows no DC
blocker; the shared chain is described as "filter bank, FX, master", none of which
guarantees a highpass. Measured DC as dB below peak, one voice, 1 s:

| configuration | DC | dB below peak |
|---|---|---|
| SIN, no feedback (control) | −8.5e-6 | −61.2 |
| QTR raw (control) | 5.9e-6 | −64.5 |
| self-feedback index 1 | −5.0e-4 | −25.8 |
| self-feedback index 4 | −3.4e-4 | −29.3 |
| self-feedback index 8 | −4.5e-4 | −26.7 |
| DRW raw, BELL table | 3.5e-5 | −47.7 |
| **NS SHORT** | 2.2e-4 | **−29.9** |
| **PLS raw, pw = 0.1** | −7.8e-3 | **−1.9** |

Two are structural, not bugs: a pulse at 10 % duty *is* 80 % DC, and a 93-step
LFSR has 48 ones to 45 zeros so SHORT noise cannot be DC-balanced. But both are
**envelope-multiplied at the source** (§2), so the DC is amplitude-modulated —
every note-on and note-off on a narrow pulse or SHORT noise is a thump, and 16 of
them sum. The engine needs either a DC blocker on its output or a documented
contract that the shared chain provides one. Today neither exists.

### 2.8 Pan law — S10

`station.html:275`: `ml += g*(1−max(0,pan)); mr += g*(1−max(0,−pan))`.

| pan | L RMS | R RMS | total power rel. centre |
|---|---|---|---|
| 0.00 | 6.926e-3 | 6.926e-3 | 0.00 dB |
| 0.25 | 5.194e-3 | 6.926e-3 | −1.07 dB |
| 0.50 | 3.463e-3 | 6.926e-3 | −2.04 dB |
| 0.75 | 1.731e-3 | 6.926e-3 | −2.75 dB |
| 1.00 | 0 | 6.926e-3 | **−3.01 dB** |

Linear, with no centre attenuation: panning is also a 3 dB loudness control, and
a stereo-width macro across the four slots is simultaneously a gain macro. The
spec states no pan law at all (§10 gives range ±1 and nothing else). This is a
**ruling owed**, not a bug: sine/constant-power is the usual answer, and it is a
parity-breaking divergence.

### 2.9 Voice lifecycle, denormals, allocation

- **Denormals: clean.** 6 s post-release at the max patch, **0 subnormal float32
  output samples**. Structural, not luck: the decay exits on `|lvl−s| < 0.004`
  (`:195`) and the release on `lvl < 0.0005` (`:198`) — absolute thresholds that
  snap to zero long before the subnormal range. §12's "no branches on denormals"
  is already satisfied by construction.
- **Envelope freeze on op OFF — S9.** `:261` `if(!o.on){outs[i]=0;continue;}` skips
  `envStep` entirely. Measured: after 50 ms the env reads 0.32392 / stage 1; after
  500 ms with the op off it still reads **0.32392 / stage 1**; re-enabling emits
  1.645e-3 immediately — a click of the stale level. In a plugin the ON switch is
  automatable, so this is reachable.
- **Voice steal — §11.4 already flags it; here is the number.** `:541`
  `if(voices.length>=8) voices.shift()`, no fade: measured |Δ| across the steal
  with 8 sustained voices = **0.006016** on a signal of peak 0.0089 — a **68 %-of-peak**
  instantaneous step.
- **Release length — S13.** REL = 260 ms produces a **0.4235 s** tail (**163 %**),
  because the one-pole runs to an absolute 0.0005 rather than for a fixed duration.
  Spec §7 says "τ such that segment completes in ~the stated time". 163 % is not
  "~". Decide deliberately: match the lab (parity) or match the spec (divergence + ADR).
  The SR drift of this tail is only 0.066 %, so it is a *labelling* problem, not a
  portability one.
- **Allocation:** `voices.splice` inside the per-sample loop (`:288`) and
  `voices.shift()` (`:541`). Browser-only expedients; C++ needs a fixed voice array
  with an active flag — never a compacting vector on the audio thread (the RT
  invariant: "Real-time thread allocates nothing").
- **`v.pT` never resets** (`:186,:240`) — an unbounded per-voice sample counter. In
  C++ as `int` it overflows after 2^31 samples ≈ 12.4 h of one held note. Use
  `double` seconds or saturate once the pitch env has completed.

---

## 3. Cost profile against §12

### 3.1 Math calls per sample per voice (shimmed `Math`, 1 s render, 1 voice, 48 kHz)

| configuration | sin | pow | exp | floor | tanh (per sample, not per voice) |
|---|---|---|---|---|---|
| **default** — 3 ops SIN, PURE = 1, noise off, 2 cells | **6** | **4** | 0 | 3 | 2 |
| **max** — 3 ops DRW, PURE = 0.5, QNT 64, 12 cells, SHORT noise + KEYTRK, pitch env | 0 | 5 | 1 | **15** | 2 |
| max, all ops PLS (polyBLEP on both branches) | 0 | 5 | 1 | 6 | 2 |

Two of those numbers are waste, and both are **exactly** removable:

- **6 sines at the default patch, where 3 are discarded.** `waveOut` (`:212-228`)
  always computes both branches and then crossfades. Spec §3.3 asserts "at 3 ops
  this is cheap" — that assertion is what the number tests. At PURE = 1 (the
  default and the parameter's default) the raw branch's `Math.sin(q*TAU)` is
  computed and multiplied by zero. 16 voices × 48 kHz = **2.30 M wasted sines/s.**
  Skipping a branch whose crossfade weight is exactly 0 is algebraically exact,
  not an approximation.
- **`opFreq(s.ops[0], …)` twice per sample.** `:256` computes `dt0` for the hard-sync
  wrap test, `:262` computes the same value again for `i = 0`. `dt0`/`wrapped` are
  used only if OP2 or OP3 has SYNC on (`:263`) — false in the default patch and in
  five of the six algorithm presets. That is the 4th `pow` in the table.

### 3.2 Per-voice memory and structure

Per voice (`mkVoice`, `:185-189`): 3 phases + 4 prev + 4 × {stage, lvl} + pT +
lfsr + nphase + nout + note + freq + gate + dead. In C++ with `double` phases and
`float` envelope state that is **≈ 128 B/voice, ≈ 2 KB for 16 voices** — three
orders of magnitude under `sizeof(SwarmCore)` = 667 032 B (B147 A9). Shared state
adds the 32-entry Wave RAM (32 B as `uint8_t`) plus, if §5's mipmap divergence is
built, one band-limited table per octave — 10 octaves × 32 partials is still under
4 KB. **Nothing here needs lookahead or a block transform.** The Wave RAM rebuild
is the only control-rate work and it is O(32 × 16) additive; it must be
double-buffered so a live edit during a sustained DRW note never publishes a
half-written table (§12's acceptance line requires exactly this).

### 3.3 Reduction, measured and proven bit-identical

Node, best-of-5, 3 s of audio, 16 voices, default EP patch:

| variant | time | Δ | bit-identical to as-is? |
|---|---|---|---|
| as-is | 0.3344 s | — | — |
| skip the branch whose crossfade weight is 0 | 0.3342 s | −0.1 % | **yes, max\|diff\| = 0** |
| compute OP1's `dt` once when no op has SYNC | 0.3250 s | −2.8 % | **yes, max\|diff\| = 0** |
| **both** | **0.3171 s** | **−5.2 %** | yes |

(JS understates the first: V8's `Math.sin` is an intrinsic and the JIT may already
hoist the dead branch. In C++ the saving is a real `sin` call, so expect more.)

### 3.4 Against the §12 budget — the honest framing

| patch | 16 voices, Node | §12 budget | required JS→C++ speedup |
|---|---|---|---|
| default EP | 11.1 % of realtime | ≤ 2 % | **≥ 5.6×** |
| max (DRW/QNT/12 cells/noise) | **18.71 %** | ≤ 2 % | **≥ 9.4×** |

I will not predict a JS→C++ ratio — that is exactly the kind of comfortable
arithmetic the doctrine warns about. The falsifiable statement is the right-hand
column: **the budget is met iff the C++ core is ≥ 9.4× the Node core at the max
patch**, and the two exact reductions above buy ~1.05× of that. Scaling is clean
and linear (1 / 8 / 16 voices = 1.35 / 9.42 / 18.71 %), so `measure_cpu`-style
extrapolation will be trustworthy. A `measure_cpu_station` on the ported core,
run before the goldens, settles it.

---

## 4. Parity strategy

### 4.1 What admits bit-parity

Bit-parity is available and worth taking for the whole DSP, **conditional on three
literals being ported verbatim**:

| must be copied exactly | site | why |
|---|---|---|
| `0.1591549` | `:267` | §2.3 — `1/(2π)` fails ε=1e-6 at index 8 |
| `4.6` and the Euler form `(target−lvl)*(4.6/t)` | `:194,197` | not `1−exp(−4.6/t)`; changes every envelope |
| `TAU = 6.283185307179586` | `:145` | correct `2π` to full double — keep the symbol, not a retyped literal |
| `0.35` slot gain, `1.4` master drive | `:274,290` | only if the monitor chain is ported (it should not be — §4.3) |
| `0.7` noise amplitude | `:252` | **or** fix it to spec's ±1 and declare the divergence — S2 |
| `0.004` / `0.0005` envelope exits, `0.45` dt clamp | `:195,198,262` | shape and range boundaries |

Settings that admit bit-parity directly: all six waveforms on both branches, all
QNT steps, the full matrix including self-feedback and mutual modulation, both
LFSR taps, KEYTRK, the pitch envelope, all four ADSRs including LOOP, the six
algorithm presets, the five Wave-RAM generators (seeded), hard sync, the default
patch. That is essentially the whole of §11's parity list.

### 4.2 What needs a behavioural oracle instead

| item | why parity cannot see it | oracle |
|---|---|---|
| 5 ms cell smoothing (§4) | **absent from the lab** — there is nothing to match | step-discontinuity probe vs the §2.6 control floor |
| RATIO continuous (§11.2) | lab is 0.5-stepped **and** 0.5–14 vs spec's 0.25–16 | monotonic-frequency sweep + inertia glide probe |
| DRW pure band-limiting (§11.1) | lab's lerp is the thing being *replaced* | aliasing floor must beat the lab's −43.7 dB @MIDI 96 |
| FREE phase, RING, STEPPED env (§11.3) | absent from the lab | unit probes, no reference |
| 16-voice release-fade stealing (§11.4) | lab is 8 + hard shift (measured 68 %-of-peak step) | steal-discontinuity probe |
| DC, pan law, voice-correlated noise | the lab *has* these behaviours; parity would certify them | the §2.4/§2.7/§2.8 probes, as gates |

This is LIBRARY L0031 exactly — **parity is agreement, not correctness.** Every
row in the lower table is a place where passing parity would lock a defect in.

### 4.3 The master `tanh` — §11.5 is right, and here is the cost of ignoring it

`:290` `Math.tanh(ml*g*1.4)`. THD of a single full-level sine operator:

| master | peak | THD (h2..h40) |
|---|---|---|
| 0.02 | 0.0098 | −102.3 dB |
| 0.25 | 0.1219 | −58.4 dB |
| 0.50 | 0.2402 | −46.5 dB |
| **0.75 (default)** | 0.3518 | **−39.6 dB** |
| 1.00 | 0.4542 | −34.8 dB |

§11.5 already declares this monitoring-only. The consequence for the oracle is the
part that is easy to miss: **the golden generator must render at a master setting
where the tanh is linear (≤ 0.02, i.e. ≤ −102 dB THD) or the C++ must reproduce a
`tanh` the spec says it does not have.** Whichever is chosen must be written into
the parity harness, not left to whoever runs it.

### 4.4 The Bessel property test — the check the lab cannot lie about

Carrier MIDI 36 (65.41 Hz), modulator ratio 7:1 (no sideband overlap), sine/sine,
pure branch, normalised so Σ Jₙ² = 1. This validates spec §2's one-sample-delay
semantics and §4's index law *without reference to the lab at all*:

| index | n = 0 | n = +1 | n = −1 | n = +2 | n = −2 | n = +3 |
|---|---|---|---|---|---|---|
| 0.5 meas | 0.93893 | 0.23521 | 0.24745 | 0.02907 | 0.03162 | 0.00252 |
| 0.5 theory | 0.93847 | 0.24227 | 0.24227 | 0.03060 | 0.03060 | 0.00256 |
| 2 meas | 0.22487 | 0.56211 | 0.59135 | 0.33641 | 0.36592 | 0.12739 |
| 2 theory | 0.22389 | 0.57672 | 0.57672 | 0.35283 | 0.35283 | 0.12894 |
| 8 meas | 0.17472 | 0.23176 | 0.24382 | 0.10918 | 0.11876 | 0.29149 |
| 8 theory | 0.17165 | 0.23464 | 0.23464 | 0.11299 | 0.11299 | 0.29113 |

**Worst error over indices {0.5, 1, 2, 4, 8} × n ∈ [−2, +3]: 0.50 dB.** The
consistent sign pattern — lower sidebands **+0.14 … +0.43 dB**, upper sidebands
**−0.11 … −0.50 dB** — is the phase lag of the one-sample delay, and it is
therefore a *positive* detector: a C++ port that reads the current sample instead
of `prev` produces a symmetric spectrum and **fails the asymmetry check**, even
though a naive |Jₙ| tolerance of ±0.6 dB would pass it. Gate on both.

### 4.5 Spec ↔ lab disagreements (findings, per the brief)

| # | spec | lab | declared in §11? |
|---|---|---|---|
| S2 | §6:100 noise output **±1** | `:252` **±0.7** (−3.098 dB) | **no** |
| S14 | §3.1/§10 RATIO **0.25–16** | `:364` slider **0.5–14** | §11.2 declares only the *stepping* |
| — | §7 decay/release "completes in ~the stated time" | 163 % of the stated time | no |
| — | §4 "preset recall click-free, ~5 ms smoothing" | no smoothing at all | no (it is a build requirement, but nothing tests it) |
| — | §10 `op{n}.phase` 0–360° | no phase-offset parameter exists | §11.3 mentions FREE mode, not the offset |
| — | §3.3 "BLAMP acceptable for TRI **if aliasing is measurable**" | measurable at −44.8 dB @MIDI 96 | condition triggered, undecided |
| — | §2 `p += cell · out_src / 2π` | `0.1591549` (truncated) | no — and the difference fails ε=1e-6 |

S2 is the one to settle first: ±0.7 vs ±1 changes the noise channel's level **and**
every noise-as-PM index by ×1.43, so the choice propagates into every preset.

---

## 5. The suite — thresholds measured on the lab

Proposed `tools/station_check` (Node for the lab, C++ for the core, same numbers).
Every threshold below is a measurement from this audit, not a guess. Each carries
its control, per the house detector rule.

| # | probe | assertion | measured today | control that must read null |
|---|---|---|---|---|
| 1 | determinism | two fresh instances, 3 s, all mod at max → identical | **max\|diff\| = 0**, FNV `bc9eea68afc8bfd9` | a seed change must differ |
| 2 | Wave RAM seed | seed 1024 → the recorded 32-value table | table pinned in §1.1 | seed 99 differs |
| 3 | LFSR period | LONG = 32767, SHORT = 93, pre-period 0 | exact | a wrong tap gives ≠ |
| 4 | LFSR decorrelation **(new)** | N-voice / 1-voice noise RMS ratio ≤ **2.2** for N = 4 | **3.999 — FAILS** | independent streams give 2.00 |
| 5 | aliasing floor | per waveform × MIDI {36,60,84,96} × PURE {0,1}, within 1.0 dB of §2.1 | table §2.1 | SIN pure must read ≤ −93 dB |
| 6 | QNT aliasing | table §2.1, and the **ordering** QNT 64 quieter than QNT 4 at MIDI 96 | −35.6 vs −22.5 | QNT OFF ≤ −93 dB |
| 7 | Bessel | \|Jₙ\| within **0.6 dB** for n ∈ [−3,3], I ∈ {0.5,1,2,4,8} | worst 0.50 dB | — |
| 8 | one-sample delay **(new)** | lower sidebands **above** upper by 0.2–1.0 dB | +0.14 … +0.43 vs −0.11 … −0.50 | a no-delay build must fail |
| 9 | feedback stability | \|op out\| ≤ 1.0 + 1e-9, 0 non-finite, 10 s at index 8, all 3 diagonals | 1.000000, 0 | index 0 must be periodic (flatness 0.0000) |
| 10 | noise spectrum | LONG flatness ≥ 0.60 at rate 0.35; SHORT ≤ 0.01 (it is pitched) | 0.6050 / 0.0017 | LONG rate 1.0 = 0.6779 |
| 11 | sample-rate portability | envelope landmarks within **0.3 %** at 44.1/48/96 k | decay 0.154 % ✓, release 0.066 % ✓, **attack 1.587 % ✗** | — |
| 12 | dt clamp **(new)** | no op is clamped at any MIDI ≤ 108 with any legal ratio, at 44.1 k | **clamped above MIDI 89.4 — FAILS** | — |
| 13 | block-size independence | chunks 1/7/64/256/333 vs 512, max\|diff\| = 0 | **0 at every chunk** | a deliberate per-block integrator must fail |
| 14 | silence → silence | no voices, 1 s → exactly 0 | **0** | one gated voice must be ≠ 0 (verified: 3.000 s nonzero, 1 voice) |
| 15 | denormals | 0 subnormal float32 samples, 6 s post-release, max patch | **0** | — |
| 16 | parameter steps **(new)** | after smoothing, every write's step ≤ **2×** the natural inter-sample step | matrix **13.9×**, wave **11.1×**, LVL **7.4×**, MASTER **6.3×** — FAIL | PURE/QNT/PAN/table-on-SIN must read 1.0× |
| 17 | DC **(new)** | \|DC\| ≤ −40 dB below peak for every non-pulse configuration | SHORT noise **−29.9**, self-fb **−25.8** — FAIL | SIN no-fb −61.2, QTR −64.5 |
| 18 | env freeze **(new)** | op ON→OFF→ON mid-note leaves no stale level | frozen 500 ms at 0.32392 — FAIL | op left ON must decay |
| 19 | voice steal **(new)** | steal step ≤ 2× the natural inter-sample step | **0.006016 on a 0.0089 peak** — FAIL by design (§11.4) | — |
| 20 | CPU | 16 voices, max patch, ≤ 2 % of one core at 48 kHz | Node 18.71 % → needs **≥ 9.4×** | 1/8/16 voices must scale linearly (1.35/9.42/18.71) |

Rows 4, 8, 12, 16, 17, 18, 19 are **new detectors** — none of them exist anywhere
in the repo, and five of them **fail on the lab today**. That is the point: they
are what turns "the port matched the prototype" into "the engine is correct".

---

## 6. The port plan

### 6.1 The shell seam — and the chimera risk

`src/hypersaw_clap.cpp:1235`:
```
hypersaw::SwarmCore cores[kMaxOsc] = {hypersaw::SwarmCore{44100.0}, …};
hypersaw::SwarmCore &core = cores[0];                              // :1237
constexpr uint32_t kMaxOsc = 2;  // :743   kNumOsc = 2;  // :744
```
`cores[k]` is a **concrete array of `SwarmCore`**, and ~40 call sites index it
directly (`noteOn`, `setParam`, `getParam`, `render`, `voiceAt`, the viz feed at
`:3218-3223`, state save at `:5564`). A second engine **type** cannot be dropped
into that array. Three options, in reduction order:

1. **Parallel array + a source-select.** `StationCore stations[kNumStation];`
   beside `cores[]`, summed into the same mix. Adds no abstraction; every existing
   `cores[k]` line is untouched; the cost is that per-engine code is duplicated at
   the ~40 sites that must now consider both. Smallest diff, worst duplication.
2. **An `EngineSlot` variant/interface** with `noteOn/noteOff/setParam/render`.
   Displaces the duplication of option 1 — but only if it is *narrow*: a base class
   that grows a `SwarmCore`-shaped surface (`voiceAt`, `p.n`, `p.topo`, `focus()`,
   `f0cur`) is the chimera. **Test to apply before accepting it:** if the interface
   needs any member that only one engine has, it has failed the reduction test and
   option 1 is correct.
3. Engine as a **separate plugin**. Rejected by ADR-114 (horde is the device).

The viz feed is the concrete chimera hazard: `:3221-3223` reads `p.n`, `p.topo`,
`s->f0cur` — swarm-only concepts with no STATION counterpart. That feed must fork
by engine type, not gain null accessors.

### 6.2 Parameters and ADR-173 classes

`kParams` has **243** rows today (`:702`), classified 137 morphable / 73 structural
/ 33 device. Counting spec §10 by hand: 3 ops × 14 + 2 sync + 2 ring + 4 slots × 6
env + 12 noise + 12 matrix + 2 pitch-env = **90** lanes, against the ROADMAP row's
"~84" — **worth reconciling before the id layout is frozen**, since ids are frozen
forever once a host has seen them.

ADR-173 classes, by the rule "a stepped waveform is structural":

- **structural** (resolve atomically, never interpolate): `op{n}.wave`,
  `op{n}.mode`, `op{n}.retrig`, `op{n}.sync`, `op{n}.ring`, `op{n}.on`,
  `op{n}.qnt` *(stepped by definition — §3.3's `QNT ∈ {OFF,4,…,64}`; the §10 table
  calls it a "stepped mod target", which is exactly ADR-173's structural case)*,
  `ns.on`, `ns.mode`, `ns.ktrk`, `env.loop`, `env.step`. **≈ 22 lanes.**
- **morphable**: all 12 matrix cells (§9.2 names them the primary morph surface),
  ratio/semis/fine/fixed, lvl, pan, pw, pure, phase, every ADSR time and sustain,
  `ns.rate`, `ns.lvl`, `ns.pan`, `penv.amt`, `penv.dec`. **≈ 68 lanes.**
- **device**: none obviously — STATION has no monitoring parameter once the master
  `tanh` is dropped (§11.5). That is a *result*, and a good one.

The **per-engine page question** the ROADMAP raises is answered by the count: 90
new lanes on top of 243 is a 37 % increase in the host's automation list, and
STATION's parameters are meaningless while the slot runs SWARM. A per-engine page
with per-engine id blocks (like `kMaxOsc`'s "2000–2999 stays free for a third",
`:743`) is the shape that already has precedent here.

### 6.3 What must NOT be ported

- The master `tanh` at `:290` (§11.5, and §4.3 measures its −39.6 dB THD).
- `performance.now()` at `:303,306` — the CPU meter (SPEC §5.7: no wall-clock in
  the core).
- `ScriptProcessorNode` and the whole `power()` block `:295-318` (§11.6).
- `voices.splice` at `:288` and `voices.shift()` at `:541` — audio-thread
  container mutation. Fixed array + active flag.
- Everything from `/* ---------------- UI ----------------` (`:320`) to EOF:
  sliders, the matrix drag handlers, the Wave-RAM canvas, the scope ring buffer
  (`scopeBuf`/`scopeDec`, `:182,292`), the keyboard. The scope buffer in particular
  is a *render-loop side effect*; a C++ core must publish its visualiser data
  through the shell's existing snapshot path, not write into itself.
- The 8-voice cap and `voices.shift()` stealing (§11.4 → 16 + release fade).

### 6.4 Ranked: fix in the LAB before porting (cheap now, expensive after goldens)

Each is a **proposal** — the lab is protected and an edit there is a spec change.

1. **Seed the LFSR per voice** (`:188`). One line; preserves replay determinism;
   removes a +12 dB-at-16-voices coherent-noise defect (S4, ratio 3.999 → 2.00).
   Do it first, because every noise preset written against the current behaviour
   would have to be re-voiced afterwards.
2. **Settle the noise amplitude, ±0.7 vs spec's ±1** (`:252` vs §6:100). Not a
   code question — a ruling. It shifts every noise-as-PM index by ×1.43, so it must
   land before any preset or golden exists. (S2)
3. **Raise or remove the `dt` clamp** (`:262`). `min(f/SR, 0.45)` makes the top of
   the keyboard sample-rate dependent with the *shipped* patch (S5). The honest fix
   is to clamp in Hz below Nyquist and let the operator mute rather than detune.

Three more that are cheaper before goldens but are C++-side, not lab-side:
smoothing (S6 — the lab has none to match), the DC blocker ruling (S8), and the
pan law (S10).

**Deliberately NOT on this list:** the `0.1591549` literal (S3). Do **not** "fix"
it — it is the parity oracle's ground truth. Write it verbatim into the port brief
and comment *why* the pretty constant is wrong.

---

## 7. Oracle coverage — what `./verify full` cannot see

Forty-four distinct `*_check` / `*_probe` / `*_gate` names appear in `./verify`. **Zero mention STATION**; `grep -rl station verify tools/`
returns one file (`tools/routing_check.cpp`) and that is an unrelated string. The
only automated contact with `reference/station.html` is `lab_load_check`
(`tools/labharness/lab_load_check.mjs:33`, which does sweep `reference/` since
B138) — and that gate asserts only **"the file's JS evaluates without throwing."**

So today, every single measurement in this audit is uncovered. Concretely, all of
the following could silently break and `./verify full` would stay green: the
mulberry32 seeding, both LFSR periods, every aliasing floor, the Bessel law, the
one-sample delay, feedback stability, block-size independence, silence, denormals,
all six waveform shapes, the default patch, and the six algorithm presets.

That is the case for building §5 as a real gate rather than a one-off report.

---

## 8. Reduction note

This audit adds no code and proposes no abstraction. Of the twenty suite rows in
§5, thirteen are direct C++ analogues of probes that already exist for other
engines (`parity_check`, `samplerate_check`, `subdiv_check`, `waveshape_check`,
`rtsafety_probe`, `steal_check`) — they should be **rows in those gates**, not a
new gate binary, wherever the existing harness takes a core by template parameter.
Seven are genuinely new detectors (§5 rows 4, 8, 12, 16, 17, 18, 19); each earns
its place by failing on the lab today, which is the only justification a new
oracle ever has.

The two performance changes (§3.3) are **deletions**, and both are proven
bit-identical. Nothing in this audit asks for a new flag, a new dependency, or a
new abstraction.
