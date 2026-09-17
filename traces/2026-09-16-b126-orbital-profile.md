# b126-orbital-profile — what ORBITAL costs, measured, before anything is trimmed

- **Queue item:** B126 (ORBITAL, `reference/gravity-modulator.html` + `specs/SPEC-ORBITAL.md`,
  ADR-165 CANDIDATE). Measurement half only, per the dispatch brief of 2026-09-16.
- **Why:** the human asked (2026-09-16, verbatim) *"Before we go much further with Orbital,
  I suppose the responsible thing would be to test its computational efficiency and trim
  out any unnecessary controls that are bogging it down."* This trace is the numbers and a
  ranked list; **no lab file was edited** and the trim itself is the human's ruling.

## Headline — the premise does not survive the measurement

**No control in ORBITAL is expensive enough to be worth removing, and the simulation is
not where the cost is.** At the spec's largest body count and its own step rate
(N = 8, `dt_phys = 1/960 s`, SPEC-ORBITAL §6.2) the whole physics step costs
**443.5 ns**, which is **426 µs/s — 0.043 % of one CPU core**. With *every* regulator and
damper switched on at once it is 0.072 %. Trimming controls to recover simulation CPU is
optimising something that is already free three orders of magnitude over.

What is *not* free is the render path, and its cost is concentrated in **implementation
choices, not in controls**. Per frame at N = 8 the lab issues **13,533 canvas calls and 65
`getComputedStyle()` forced style resolutions** — 812,000 canvas calls and 3,900 style
resolutions per second at 60 fps. Of those 13,533 calls, **8,776 (65 %) are the trails**,
and they are that expensive only because the per-segment alpha fade forces one
`beginPath`/`moveTo`/`lineTo`/`stroke` per segment instead of one path per body.

So the ranked list below is mostly "keep it, and make it cheap", and the three genuinely
worthwhile changes (cache `css()`, batch the trail strokes, square the max-speed clamp)
cost the user **nothing** — no control disappears, no dynamics change. The one change that
would save real simulation time (the second `accel()` call) is a C++-port restructure, not
a lab edit.

## Two corrections to the brief, both load-bearing

1. **PR #595 is merged, so there is only one lab to profile.** The brief says to profile
   both `origin/main` and "the one on `origin/orbital-regulators` (PR #595, … unmerged)".
   `git merge-base --is-ancestor origin/orbital-regulators origin/main` is true — the
   merge is commit `67e095e` — and the two files are byte-identical
   (`sha256 8352db2c8c87ac2654b167ade3f70328b95992762fb2e47f8177c7fae011f2bd`). The
   thermostat and Van der Pol band are already in `main`, off by default. This is the
   LIBRARY L0037 shape exactly: the brief's frontmatter claim about protocol state versus
   the tree as the fact.
   *Consequence for the brief's second control:* "the two lab versions must produce
   bit-identical trajectories with the regulators off" is **vacuous** between two copies of
   one file. The control the brief wanted is run instead: `main` versus `2876005`, the
   commit immediately before the regulators landed. It passes, 0 of 3,200 doubles differing
   (control B), and it is paired with a negative that fires (control B-neg, 3,200/3,200).

2. **The lab's physics step is `dt = 1/480`, not `1/960`.** The brief states 1/960 citing
   SPEC §6.2; the spec's own §6.2 says the opposite — *"the prototype uses 1/480; adopt
   1/960 in C++ and regenerate goldens rather than inherit the browser value"*
   (`specs/SPEC-ORBITAL.md:88`), and the lab reads `const DT=1/480` at line 550. All ns/step
   figures below are per step and rate-independent; where a per-second figure is given it is
   stated at both rates.

## Method, and the measurement that lied first

Timing runs are **one variant per child process**: V8 specialises `step()`/`accel()` to the
shapes and branch history it has seen, so timing several feature variants in one process
measures the deopt rather than the feature. Each cell is the **median of 9 runs of 20,000
steps** after **three full discarded runs** — a plain step warm-up was not enough (the first
two timed runs came in at 802 and 467 ns against a settled 160). Every block measurement
carries a **null floor** (an empty loop of the same shape, 1.5–7 ns) so a number close to
the floor is reported as floor-bound rather than as a result.

**The trap worth recording.** The obvious move was to reuse the B135 correctness harness
(`traces/2026-09-16-b135-orbital-initial-conditions.md`), which loads the lab into a
`vm.createContext` sandbox. That is right for bit-identity and **fatal for timing**: every
global lexical read crosses the context interceptor. The first cut reported **1,272 ns for
four arithmetic calls** on one body — four times the cost of an entire physics step — and it
was internally consistent enough to write down. `calib_context.mjs` runs the identical
benchmark body both ways and measures the tax directly:

```
{"ns_per_tick_contextified":4385.96,"ns_per_tick_main_realm":77.67,"ratio":56.47,"node":"v24.10.0"}
```

**56×**, and it scales with the number of global reads, so it does not even cancel between
variants. The lab is therefore run with `vm.runInThisContext` and the DOM stubs installed on
the real `globalThis`. The physics is unaffected by the change — the state fingerprint after
2,000 steps is `4042959567` under both harnesses — so only the clock moved. (LIBRARY L0016:
calibrate the detector before trusting it; the ranking between variants looked perfectly
sensible the whole time.)

**Mechanisms that are not switchable from the UI** (the pair loop, the max-speed clamp, the
wall handling, the second Verlet force evaluation) are ablated in **scratch copies** of the
lab — `reference/gravity-modulator.html` itself is never touched. Every mutation asserts its
anchor matched **exactly once** and that the output differs from the input, because a replace
that silently matches nothing produces a file that times identically, which reads exactly like
"this feature is free" (LIBRARY L0032 corollary).

**Machine and toolchain:** Apple Silicon laptop, macOS, **Node v24.10.0**, Release-equivalent
(V8 JIT, no flags). Figures are JS-in-V8 and are used below as a conservative upper bound on
the C++ port, never as a C++ prediction.

## 1. Simulation cost per physics step vs body count

Baseline = the lab's own defaults at `load()`: damping 0, cushion 0, both regulators off,
COM lock on, reflect walls, scale 1. Bodies are laid out deterministically on a ring by the
lab's own `mulberry32` (presets come in fixed counts of 2/3/4; the spec's range is 2–8, so a
like-for-like sweep needs one generator) with a seeded jitter that keeps every N inside the
box and clear of the 0.1-wide cushion band at t = 0, so the cushion's cost is not already
sitting in the baseline.

| N | pairs | ns/step (median of 9) | min | spread | ns/pair | µs/s @960 | % of one core @960 |
|---|-------|----------------------|-----|--------|---------|-----------|--------------------|
| 2 | 1 | **94.0** | 87.4 | 13.1% | 94.0 | 90 | **0.0090%** |
| 3 | 3 | **122.9** | 115.4 | 16.9% | 41.0 | 118 | **0.0118%** |
| 4 | 6 | **166.5** | 164.9 | 10.4% | 27.8 | 160 | **0.0160%** |
| 6 | 15 | **291.6** | 289.9 | 2.4% | 19.4 | 280 | **0.0280%** |
| 8 | 28 | **443.5** | 442.3 | 1.7% | 15.8 | 426 | **0.0426%** |

**The shape is clean.** A least-squares quadratic over the five points fits to within ±3 %:

```
ns/step  =  53.7  +  9.82·N  +  4.94·N²
  N=2 fit  93.1  measured  96.0   err -3.0%
  N=3 fit 127.6  measured 124.5   err +2.5%
  N=4 fit 172.1  measured 169.3   err +1.6%
  N=6 fit 290.5  measured 295.2   err -1.6%
  N=8 fit 448.5  measured 446.8   err +0.4%
```

so the step decomposes as **53.7 ns fixed + 9.82 ns per body + 4.94 ns per body²**, and the
pairwise term is 21 % of the cost at N = 2 but **70 % at N = 8**. (Coefficients from the run
recorded in `results.json`; the tables above are a later re-run of the same harness, which is
why the totals differ by a percent or two — see the spread column.)

## 2a. Marginal cost of each switchable feature

| feature | what it is | N=4 ns | N=4 Δ | N=4 % | N=8 ns | N=8 Δ | N=8 % |
|---------|-----------|--------|-------|-------|--------|-------|-------|
| `damp` | global damping (`P.damp`) | 175.8 | +9.3 | +5.6% | 463.9 | +20.4 | +4.6% |
| `cushion` | edge cushion (`P.cushK`) | 175.2 | +8.7 | +5.2% | 456.1 | +12.6 | +2.9% |
| `com_off` | COM lock **off** (baseline has it on) | 142.6 | -23.9 | -14.4% | 398.0 | -45.4 | -10.2% |
| `wrap` | wrap bounds instead of reflect | 176.7 | +10.2 | +6.1% | 431.8 | -11.7 | -2.6% |
| `thermo` | thermostat (regulator A) | 252.7 | +86.2 | +51.8% | 582.3 | +138.8 | +31.3% |
| `vdp_wall` | Van der Pol, wall leg only (g<0) | 173.1 | +6.6 | +4.0% | 455.7 | +12.2 | +2.8% |
| `vdp_full` | Van der Pol, both legs (g>0) | 227.3 | +60.8 | +36.5% | 555.2 | +111.8 | +25.2% |
| `all_on` | everything at once | 333.3 | +166.8 | +100.2% | 752.1 | +308.7 | +69.6% |

Notes, because three of these rows are easy to misread:

- **`com_off` is negative because the baseline has COM lock ON.** The centroid recentre
  therefore *costs* +45.4 ns at N = 8 (10.2 %); the row shows what turning it off saves.
- **`wrap` is faster than `reflect` at N = 8** (−11.7 ns). Wrap replaces the reflect branch
  with two modulo operations *and* disables the cushion and the Van der Pol band entirely
  ("distance to the nearest wall" has no meaning on a torus — `step()` lines 395 and 400), so it
  removes more than it adds. It is a bounds *mode*, not a cost.
- **`all_on` (+308.7 ns, +69.6 %) is the worst case the user can construct**, and it is
  still 0.072 % of one core at 960 steps/s.

**The thermostat is the most expensive control in the instrument**, at +138.8 ns (+31.3 %)
at N = 8 and +86.2 ns (+51.8 %) at N = 4. Roughly half of that is one primitive: `meanSpeed()`
(line 314) calls `Math.hypot` once per body per step, and substituting
`Math.sqrt(vx*vx+vy*vy)` recovers **66.2 ns** of it (control E). It is off by default —
`load()` sets `P.thermoOn=false` — so nobody pays it who has not asked for it.

## 2b. Mechanisms that are not switchable (scratch-copy source ablation)

| mechanism | what was removed | N=4 cost ns | N=4 % | N=8 cost ns | N=8 % |
|-----------|------------------|-------------|-------|-------------|-------|
| `nopair` | the O(N²) pair loop in `accel()` — **control** | 20.1 | 12.1% | 193.2 | 43.6% |
| `noclamp` | the max-speed clamp (line 411) | 33.9 | 20.4% | 66.5 | 15.0% |
| `nobounds` | the reflect/wrap wall block | -0.5 | -0.3% | 6.1 | 1.4% |
| `oneaccel` | the leading `accel()` call (prices the 1-eval Verlet) | 35.8 | 21.5% | 131.8 | 29.7% |

- **`nopair`** stubs the pair loop out of `accel()` — this is the brief's must-fail control,
  see §5.
- **`noclamp`** removes the max-speed clamp at line 411.
- **`nobounds`** removes the reflect/wrap block. At 1.4 % of the step it is **essentially
  free**; the walls are not a cost.
- **`oneaccel`** removes the *leading* `accel()` call in `step()` — not a legitimate
  refactor (it changes the physics), present only to price what the standard
  carry-the-acceleration formulation of velocity Verlet is worth.

### The primitive behind three of these rows

`profile_prims.mjs` times each math primitive against a null loop of the same shape:

```
ns per call, net of a 3.65 ns loop floor:
  Math.hypot(x,y)              7.938
  Math.sqrt(x*x + y*y)         0.490      <- 16.2x cheaper than hypot
  x*x + y*y  (no root)         0.182      <- 43.6x cheaper than hypot
  Math.atan2(x,y)              8.378
  Math.exp(x)                  3.424
  Math.sqrt(x)                 0.171
  1/x                          0.093
  Math.min(a,b,c,d)            0.217
```

`Math.hypot` is the single most expensive thing in the file, and the lab calls it in five
places: the max-speed clamp (line 411, every body every step), `meanSpeed()` (314),
`outRadius()` (458), `vdpBand()`'s core leg (372) and `energy()` (430). The clamp's measured
cost of 66.5 ns at N = 8 is 8 × 7.94 = 63.5 ns of `Math.hypot` and nothing else — two
independent measurements agreeing to 5 %.

## 3. Cost of the readout path

Per call of each block, measured on a settled state, with the null floor beside it:

| N | null floor | obs x/y | obs polar (atan2+hypot) | all 4 obs | strip write tick | trail tick | `energy()` | `meanSpeed()` |
|---|-----------|---------|------------------------|-----------|------------------|-----------|----------|-------------|
| 2 | 6.71 | 6.162 | 45.248 | 46.683 | 71.942 | 86.987 | 20.631 | 20.681 |
| 3 | 1.363 | 7.315 | 62.4 | 62.452 | 110.608 | 113.754 | 45.773 | 32.146 |
| 4 | 1.66 | 7.61 | 82.002 | 84.108 | 139.771 | 157.469 | 78.794 | 38.133 |
| 6 | 7.015 | 9.269 | 116.254 | 120.223 | 194.767 | 230.904 | 186.265 | 57.01 |
| 8 | 1.631 | 11.912 | 158.796 | 163.281 | 272.404 | 334.11 | 330.229 | 72.973 |

- **The polar observables are 13.3× the cartesian ones** — 19.85 ns/body against 1.49 ns/body
  at N = 8 — entirely because `outAngle` (457) is an `atan2` and `outRadius` (458) is a
  `hypot`. They are computed unconditionally in the strip write whether or not the polar
  toggle is on.
- **In absolute terms none of this matters.** The strip tick fires every 8 physics steps
  (`HIST_EVERY`, line 551) = 60/s of sim; the trail push, `energy()` and `meanSpeed()` fire
  once per animation frame. At N = 8 that is **16.3 µs/s for the strips, 20.0 µs/s for the
  trails and 19.8 µs/s for the energy readout** — about 0.006 % of a core between them.

The readout path's real cost is not the arithmetic; it is the drawing that consumes it (§4).

## 4. The rendering half — canvas operation counts

A headless harness cannot measure frame *time* (no rasteriser, no compositor), so it counts
what it can honestly see: 2D-context calls, context state writes, and `getComputedStyle()`
calls. The counting context **is** the context the lab captured at load (`cx` is a top-level
`const` and cannot be reassigned afterwards), supplied through a minimal DOM. Trails are
filled to their 220-sample cap and strips to their 240 before counting, so no loop is
undercounted.

| N | trails | strips | field canvas | strips (N bodies) | vbar | **total ops/frame** | `getComputedStyle`/frame | ops/s @60fps |
|---|--------|--------|--------------|-------------------|------|--------------------|--------------------------|--------------|
| 2 | on | cartesian | 2310 | 1010 | 256 | **3576** | 14 | 214,560 |
| 3 | on | cartesian | 3447 | 1515 | 256 | **5218** | 20 | 313,080 |
| 4 | on | cartesian | 4591 | 2020 | 256 | **6867** | 27 | 412,020 |
| 6 | on | cartesian | 6900 | 3030 | 256 | **10186** | 44 | 611,160 |
| 8 | on | cartesian | 9237 | 4040 | 256 | **13533** | 65 | 811,980 |
| 8 | **off** | cartesian | 461 | 4040 | 256 | **4757** | 57 | 285,420 |
| 8 | on | **polar** | 9237 | 4040 | 256 | **13533** | 65 | 811,980 |

At N = 8 and 60 fps that is **811,980 canvas calls per second and 3,900 forced style
resolutions per second.**

### Which control drives which cost

**Trails — 8,776 of 9,237 field-canvas ops (95 % of the field, 65 % of the whole frame).**
Turning trails off drops the field canvas from 9,237 to 461 ops. The breakdown is exact:

```
op                        trails on   trails off    delta
field.beginPath                1812           60    +1752
field.moveTo                   1812           60    +1752
field.lineTo                   1812           60    +1752
field.stroke                   1796           44    +1752
field.set:globalAlpha          1816           64    +1752
```

1752 = 8 bodies × 219 segments. The trail loop (line 514) sets `globalAlpha` per segment to
fade the tail, and an alpha change cannot be batched into one path — so each of the 219
segments per body pays a full `beginPath`/`moveTo`/`lineTo`/`stroke`. **The fade is what
costs, not the trail.** Trail length is hard-coded at 220 (line 573) and is not a user control.

**Per-body strip graphs — 505 ops per body per frame, 4,040 at N = 8 (30 % of the frame).**
479 of the 505 are `lineTo`: two 240-sample series redrawn in full, per body, every animation
frame (`updateReadouts`, line 830 → `drawXYGraph`, line 776). This is the cost that scales
with **body count**, and it is the reason the frame total grows from 3,576 at N = 2 to 13,533
at N = 8.

**The polar toggle is free.** N = 8 polar and N = 8 cartesian both count 13,533 ops — the
strip draws two series either way.

**The mean-speed strip (`drawVbar`, line 808) is 256 ops/frame, 1.9 %** — constant in N.

**`css()` (line 211) is called 65 times per frame.** It is
`getComputedStyle(document.documentElement).getPropertyValue(v).trim()`, and it is called
inside the O(N²) force-line loop, once per trail, and three times per body. In a browser each
call is a forced style resolution against the document element. This is the one render cost a
call count settles rather than estimates, and it buys the user nothing: the palette is
static.

## 5. Controls — including the one that did not fire

All five pass. Two were written specifically so that a green result could not be vacuous.

```
PASS  A_nopair_is_visible_and_grows_with_N
      N=4: baseline 168.9 -> nopair 143.3, saved 25.6 ns (15.1%)
      N=8: baseline 445.2 -> nopair 247.7, saved 197.6 ns (44.4%)
PASS  B_regulators_off_bit_identical
      main vs 2876005 (pre-regulators), regulators off, N=8, 4000 steps sampled every 40:
      0 of 3200 doubles differ
PASS  B_negative_comparator_can_fire
      same comparator, main vs lab-ablate-nopair: 3200 of 3200 differ
PASS  D_fastclamp_is_free_and_identical
      squared-compare clamp: 445.2 -> 380.6 ns (saved 62.8) AND 0 of 3200 doubles differ
PASS  E_fastmean_saves_only_under_thermostat_and_is_NOT_identical
      thermostat on: 582.3 -> 516.1 ns (saved 66.2); 2607 of 3200 doubles differ
```

**Control A's bar is deliberately not a flat percentage.** At N = 4 the pair loop is six pairs
against a fixed per-body cost that does not shrink, so gravity is only 15 % there and a flat
15 % bar would have failed for a reason that has nothing to do with whether the timer sees the
loop. What the timer seeing an O(N²) loop actually looks like is a saving that **grows with N**
— 25.6 ns at N = 4, 197.6 ns at N = 8 — so that is what is asserted.

### The control that did not fire, recorded rather than retried

The negative control for B was first written against `lab-ablate-noclamp` and **reported
0/3,200 — it did not fire.** Per LIBRARY L0033 that is a finding, not a failed attempt, and it
is the single most actionable result in this trace:

```
C_maxspeed_clamp_never_engages
  removing the max-speed clamp changes 0 of 3200 doubles
  peak speed reached over 4000 steps at N=8: 1.609   MAXV = 3.0   headroom 1.86x
```

**The max-speed clamp costs 66.5 ns/step at N = 8 (15.0 % of the whole step) and never
engages.** It is an unconditional `Math.hypot` per body per step guarding a threshold nothing
reaches. The negative control was then re-pointed at `lab-ablate-nopair`, where divergence is
structural, and it fires 3,200/3,200.

*Boundary this leaves open, stated rather than assumed:* "never engages" is measured over this
seeded configuration, 4,000 steps, no user interaction. A hand-thrown slingshot could plausibly
exceed MAXV, which is exactly why the recommendation below is to make the clamp free, **not**
to delete it.

## 6. The C++ port's expected cost

From §1, at N = 8 the pair loop is 28 pairs and the fitted pairwise term is 4.94 ns/body² =
316 ns/step, out of 443.5 ns total. SPEC-ORBITAL §6.2 fixes the port at `dt_phys = 1/960 s`,
so:

| | JS measured (upper bound) | per second @ 960 steps/s | % of one core |
|---|---|---|---|
| N = 4, baseline | 166.5 ns/step | 160 µs/s | 0.016 % |
| N = 8, baseline | 443.5 ns/step | 426 µs/s | **0.043 %** |
| N = 8, everything on | 752.1 ns/step | 722 µs/s | 0.072 % |

These are **JS-in-V8 figures used as a conservative ceiling**, not a C++ prediction. The pair
loop is pure `double` arithmetic with one `Math.sqrt` and one divide per pair and monomorphic
object shapes, which is close to the best case for V8; a scalar C++ port should land at or
below these numbers, and the honest claim is therefore "**no worse than 0.043 % of a core at
N = 8**". At 44.1 kHz with a 128-sample block (2.9 ms), 960 steps/s is 2.8 steps per block —
about **1.2 µs of work per audio block against a 2.9 ms budget**.

**Does any of it belong on the audio thread? Yes, and §6.2 wants it there.** Determinism
requires "identical step count per block, which means wall-time accumulation must be replaced
by sample-count accumulation" (`specs/SPEC-ORBITAL.md:88`); running the field on the audio
thread from the sample counter is how that is satisfied, and a timer thread is how it is
violated. At 0.04 % of the block budget there is no realtime objection. Three conditions
attach, all already in the spec or in this repo's invariants:

1. **Allocate nothing.** The lab's `nodes[]`, `trail[]` (a JS array that `push`/`shift`es every
   frame) and the four `Float32Array(240)` rings per body must become fixed-capacity
   preallocated storage — the trail in particular is an allocating ring today.
2. **No `float` in the force loop** (§6.2(c)), which the numbers above assume.
3. **Keep the polar observables at block rate, not step rate.** They are the expensive part of
   the readout (19.85 ns/body, §3) and §6.2 already specifies "observables computed once per
   physics step, one-pole smoothed at control rate"; computing `atan2`/`hypot` per step per
   body buys nothing a block-rate computation does not.

## 7. Ranked trim candidates — for the human to rule on

Ordered by measured cost recovered per unit of user-visible loss. **Nothing is recommended
for deletion on cost grounds**, because no control is expensive enough to justify it.

### A. Free wins — no control removed, nothing the user can see changes

| # | Change | What it costs today | What it does for the user | Recommendation |
|---|---|---|---|---|
| **A1** | **Cache `css()` (line 211)**: resolve the CSS custom properties once at load instead of calling `getComputedStyle` per use | **65 forced style resolutions per frame at N = 8 = 3,900/s at 60 fps**; called inside the O(N²) force-line loop and 3× per body | **Nothing.** The palette is static; this is not a control at all | **FIX.** Highest priority, zero user cost, zero risk |
| **A2** | **Batch the trail strokes**: one path per body with a quantised fade (~8 alpha buckets) instead of one `beginPath`/`stroke` per segment | **8,776 of 13,533 canvas ops per frame (65 %)** — 1,752 each of `beginPath`/`moveTo`/`lineTo`/`stroke`/`globalAlpha` at N = 8 | The trail itself is how you see the *shape* of the motion — keep it. Only the **per-segment** fade is being paid for; 8 buckets is visually near-identical | **FIX.** ~27× fewer calls on the dominant cost |
| **A3** | **Square the max-speed clamp (line 411)**: `vx*vx+vy*vy > MAXV*MAXV`, take the root only when it fires | **66.5 ns/step at N = 8 — 15.0 % of the entire physics step** — and control C shows **it never engages** (peak 1.61 vs MAXV 3.0) | Nothing changes: it is a safety net against a hand-thrown slingshot and stays one | **FIX.** **Verified bit-identical** (control D: 0/3,200 doubles differ, 62.8 ns saved) |

### B. Real savings that require a ruling because behaviour moves

| # | Change | What it costs today | What it does for the user | Recommendation |
|---|---|---|---|---|
| **B1** | **Carry the acceleration** across the step in the **C++ port** (standard velocity Verlet, one force evaluation per step instead of two) | **131.8 ns/step at N = 8 — 29.7 %**, the largest single simulation cost | Nothing visible; it is the same integrator written the standard way | **DO IT IN THE PORT, NOT THE LAB.** The lab is the parity reference (ADR-003) and B135 established bit-identity of its trajectory as a gate; changing it there is a spec change |
| **B2** | **Cheapen `meanSpeed()`** (line 314): `Math.sqrt(vx*vx+vy*vy)` for `Math.hypot` | **66.2 ns/step of the thermostat's 138.8 ns** (control E) | Nothing visible — but it is **not** bit-identical (2,607/3,200 doubles differ), because the thermostat feeds the mean back into the physics | **HUMAN RULING.** A real saving on an off-by-default control, at the price of moving a CANDIDATE engine's trajectory |
| **B3** | **Run the thermostat every 8 steps** with `dt×8` instead of every step | up to ~7/8 of the thermostat's 138.8 ns | τ = 0.5 s and `dt/τ = 1/240`, so λ would sit within parts-per-thousand of the same value | **HUMAN RULING**, and only if B126 rules the thermostat in. Same objection as B2: it moves the trajectory |
| **B4** | **Redraw per-body strips lazily** — only when the row is visible, at the strip's own 60/s sim cadence rather than per animation frame; or drop strip resolution from 240 to the canvas pixel width | **4,040 canvas ops per frame at N = 8 (30 %)**, 479 `lineTo` per body | Seeing the modulation signal is the *point* of a modulation source (SPEC §3, §9) — do not remove | **MAKE LAZY.** Below-pixel-width samples are invisible by construction, so half the `lineTo` calls draw nothing |

### C. Keep as they are — measured, and they earn their cost

| Control | Measured cost at N = 8 | Verdict |
|---|---|---|
| **COM lock / centroid recentre** | 45.4 ns (10.2 %) | **KEEP.** Without it the field drifts out of view and the observables saturate — it earns it |
| **Damping** | +20.4 ns (+4.6 %) | **KEEP.** Cheap |
| **Edge cushion** | +12.6 ns (+2.9 %) | **KEEP.** Cheap |
| **Van der Pol band, wall leg (g ≤ 0)** | +12.2 ns (+2.8 %) | **KEEP.** Cheap |
| **Van der Pol band, full (g > 0)** | +111.8 ns (+25.2 %) | **KEEP, off by default.** The core leg's `Math.hypot` is most of it; the A3 substitution applies here too and is free |
| **Thermostat** | +138.8 ns (+31.3 %) | **KEEP, off by default** as `load()` already sets it. The most expensive control, and still 0.013 % of a core |
| **Wall handling (reflect/wrap)** | 6.1 ns (1.4 %) | **KEEP.** Effectively free |
| **Polar toggle** | 0 canvas ops, 0 ns | **KEEP.** Genuinely free |
| **Energy / HUD readout** | 330 ns per frame = 19.8 µs/s | **KEEP.** Optionally throttle the HUD to ~10 Hz; not worth doing for the number |
| **Mean-speed strip (`drawVbar`)** | 256 canvas ops/frame (1.9 %) | **KEEP** if the thermostat survives B126 — it exists to make the settling visible. If the thermostat is ruled out, this goes with it, for coherence rather than for cost |

### D. Deliberately **not** recommended

- **Reducing the maximum body count.** N = 8 costs 0.043 % of a core. The limit should be set
  by what is musically useful, not by CPU.
- **Removing the max-speed clamp** even though it never fires — see the boundary note in §5.
- **Removing trails or strips.** They are 95 % of the drawing precisely because they are the
  two things that show the user what the modulator is doing.

## Evidence consulted

- `reference/gravity-modulator.html` in full (lines cited inline: 211 `css`, 222 `MAXV`,
  292 `accel`, 314 `meanSpeed`, 341 `thermostat`, 372 `vdpBand`, 382 `step`, 411 clamp,
  430 `energy`, 457–458 polar observables, 483 `draw`, 501/514/528 force lines / trails /
  nodes, 550–551 `DT`/`HIST_EVERY`, 553 `frame`, 776 `drawXYGraph`, 808 `drawVbar`,
  830 `updateReadouts`).
- `specs/SPEC-ORBITAL.md` §6.2 (step rate, determinism, no `float`), §3, §9.
- `traces/2026-09-16-b135-orbital-initial-conditions.md` — the vm harness this one is modelled
  on, and whose approach §"Method" documents as unusable for timing.
- `traces/2026-09-16-b126-orbital-regulators.md`, `traces/2026-09-15-orbital-lab-cushion-graphs.md`.
- `git log`/`merge-base` for the PR #595 correction; `git show 2876005:` for control B.
- LIBRARY L0016 (calibrate the detector), L0032 (a detector sharing an assumption; assert every
  mutation's anchor), L0033 (a plant that does not fire is a finding), L0037 (frontmatter is a
  claim, the tree is the fact), L0048 (scratch namespaced per stream), L0049 (B135's
  scale-is-a-readout gate).

## Alternatives rejected

- **Putting the harness in `tools/labharness/`.** The brief permits it *if reusable*. It is
  not: `profile_sim.mjs` knows ORBITAL's `nodes`/`P`/`step`/`mk` and its variant names by
  name. Same call, same reason, as the B135 harness — embedded below instead, because a
  scratch file is not durable evidence (L0048).
- **Reusing the B135 vm harness as-is.** Measured to be 56× wrong for timing; see §"Method".
- **Timing all variants in one process.** V8 deopt across variants measures the polymorphism,
  not the feature. One process per cell instead.
- **Reporting the max-speed clamp as "not separable".** It is not switchable from the UI, but
  a scratch-copy ablation prices it exactly, and it turned out to be the most actionable
  number in the trace. Giving up a number that exists is not a refusal, it is a gap.
- **Measuring real frame time headlessly.** There is no rasteriser; a "frame time" from a stub
  context would be the stub's. Operation counts are reported instead, and the lead's
  browser measurement is the thing that closes §4.
- **Editing the lab to add instrumentation hooks.** Out of scope and unnecessary — the lab's
  own top-level bindings are reachable from a script sharing its global lexical scope.

## Verify

`./verify fast` — **exit 0**, git `b9ee065` (`.harness/last-verify.json`:
`{"target":"fast","exit":0,"git":"b9ee065","ts":"2026-09-17T03:53:18Z"}`).
This change set is one new trace file; no code, gate or lab is touched by it.

```
verify: .leakcheck-names absent — private-name leak check SKIPPED (expected off this Mac)
mailbox_delivery: no sibling mailbox checked out — SKIPPED
presentation_check: GREEN (325 rows, scopes: global, osc1, osc2; 34 undesigned (no chunk named), 5 ungrouped)
  note  engine guards law==0 but no parameter declares it
  note  engine guards law==1 but no parameter declares it
depends_check: GREEN (121 declared dependencies, header current, 2 advisory)
gen_gui_controls: GREEN (197 generated control(s), gui2 markup current)
test_table_check: GREEN (189 tests — 104 agentic, 85 human; 15 awaiting an oracle)
  gui.html     reaches 102 / 243 params
  gui2.html    reaches 226 / 243 params
  exempt: inertiaCurve — dev-only, labelled (dev) in the param table
  patch-scope params (raw-id dispatch, must be data-fixed): 89
gui_reach: GREEN (every declared param is reachable in some GUI)
```

## Open questions

1. **Real frame time is still unmeasured.** §4 gives operation counts, not milliseconds. The
   brief assigns the browser measurement to the lead; A1 and A2 should be confirmed there
   before either is called a win, because a canvas call count is a proxy and `getComputedStyle`
   in particular may be cheaper than feared if the browser caches it within a frame.
2. **Control C's boundary.** "The max-speed clamp never engages" is measured over one seeded
   configuration, 4,000 steps, no user interaction. A hand-thrown slingshot was not tested and
   plausibly exceeds MAXV. The A3 recommendation is written to be safe either way.
3. **B2/B3 need a ruling before anyone patches them.** Both move a CANDIDATE engine's
   trajectory, and B135 made trajectory bit-identity a gate. They are listed because they are
   real savings, not because they should be taken.
4. **The `dt` discrepancy in the brief** (1/960 vs the lab's 1/480) should be corrected wherever
   the lead drafted it from, since it would silently double any per-second figure computed on
   the lab.
5. **A LIBRARY candidate is proposed but not written.** `LIBRARY.md`/`INDEX.md` are outside this
   brief's file scope, so the entry is offered to the lead in the dispatch report rather than
   landed here: *a correctness sandbox is not a timing harness — `vm.createContext` taxes
   global lexical reads ~56×, so a harness reused from a parity check reports fabricated
   absolute costs while keeping the rankings plausible.*
6. **Nothing in `src/**` was measured.** There is no C++ ORBITAL port yet; §6 is a derivation
   from JS figures and is labelled as an upper bound, not a measurement of a port that does
   not exist.

## Harness (the durable copy — the scratch originals are ephemeral, L0048)

Run order: `make_ablations.mjs lab-main.html` → `drive.mjs .` → `drive_draw.mjs .`;
`calib_context.mjs` and `profile_prims.mjs` stand alone.

<details><summary>calib_context.mjs — the 56× vm-context tax that invalidated the first cut</summary>

```javascript
/* calib_context.mjs — is the vm CONTEXT taxing every global read?
 *
 * Runs the identical benchmark body two ways: inside vm.createContext (what
 * profile_sim.mjs does) and inside vm.runInThisContext (the main realm, whose
 * global is a real global object, not a contextified proxy). If the two agree,
 * the vm numbers are the lab's; if they diverge, every absolute ns figure from
 * the contextified run is the interceptor and must be discarded.
 */
import vm from 'node:vm';

const BODY = `
let SINK = 0;
const PP = { scale: 1 };
const clamp = v => Math.max(0, Math.min(1, v));
function gu2(u){ return 0.5 + (u-0.5)*PP.scale; }
function oX(b){ return clamp(gu2(b.x)); }
function oY(b){ return clamp(1-gu2(b.y)); }
function oA(b){ return (Math.atan2(0.5-gu2(b.y), gu2(b.x)-0.5)/(2*Math.PI)+1)%1; }
function oR(b){ return clamp(Math.hypot(gu2(b.x)-0.5, gu2(b.y)-0.5)/Math.SQRT1_2); }
const BODIES = [];
for (let i=0;i<4;i++) BODIES.push({ x:0.3+i*0.1, y:0.4+i*0.05 });
function tick(){ for(const b of BODIES) SINK += oX(b)+oY(b)+oA(b)+oR(b); }
function med(a){ a=a.slice().sort((x,y)=>x-y); return a[(a.length-1)>>1]; }
function measure(){
  for(let i=0;i<50000;i++) tick();
  const out=[];
  for(let r=0;r<9;r++){ const t0=hrt(); for(let i=0;i<20000;i++) tick(); out.push(Number(hrt()-t0)/20000); }
  return med(out);
}
globalThis.__r = measure();
`;

// (a) contextified sandbox — what the profiler uses
const sandbox = { Math, hrt: process.hrtime.bigint.bind(process.hrtime) };
sandbox.globalThis = sandbox;
const ctx = vm.createContext(sandbox);
new vm.Script(BODY).runInContext(ctx);
const inContext = sandbox.__r;

// (b) main realm
globalThis.hrt = process.hrtime.bigint.bind(process.hrtime);
vm.runInThisContext(BODY);
const inRealm = globalThis.__r;

console.log(JSON.stringify({
  ns_per_tick_contextified: +inContext.toFixed(2),
  ns_per_tick_main_realm: +inRealm.toFixed(2),
  ratio: +(inContext / inRealm).toFixed(2),
  node: process.version,
}));
```

</details>

<details><summary>profile_sim.mjs — one timing cell per process, in the main realm</summary>

```javascript
/*
 * profile_sim.mjs — one timing measurement of ORBITAL's step() in a fresh process.
 *
 * THREE DESIGN RULES, each one a measurement that lied first:
 *
 * 1. ONE VARIANT PER PROCESS. V8 specialises step()/accel() to the shapes and
 *    branch history it has seen, so timing several feature variants in one
 *    process measures the deopt, not the feature. The driver spawns this once
 *    per (lab, N, variant).
 *
 * 2. NEVER TIME INSIDE vm.createContext. The B135 correctness harness loads the
 *    lab into a contextified sandbox, which is right for bit-identity and FATAL
 *    for timing: every global lexical read crosses the context interceptor.
 *    Measured (calib_context.mjs, the identical benchmark body both ways):
 *    4385.96 ns/tick contextified vs 77.67 ns/tick in the main realm — a 56x
 *    tax that scales with global reads, so it does not even cancel between
 *    variants. The lab is therefore run with vm.runInThisContext and the stubs
 *    are installed on the real globalThis (LIBRARY L0016: the first cut reported
 *    1272 ns for four arithmetic calls and I nearly wrote it down).
 *
 * 3. EVERY MEASUREMENT CARRIES ITS FLOOR. __nullTick is an empty loop of the
 *    same shape; anything within a few multiples of it is reported as
 *    floor-bound rather than as a number.
 *
 * Usage: node profile_sim.mjs --lab <file> --n <N> --variant <v> [--steps S] [--runs R]
 * Prints one JSON line.
 */
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import vm from 'node:vm';

const arg = (k, d) => {
  const i = process.argv.indexOf('--' + k);
  return i < 0 ? d : process.argv[i + 1];
};
const LAB = resolve(arg('lab'));
const N = +arg('n', 4);
const VARIANT = arg('variant', 'baseline');
const STEPS = +arg('steps', 20000);
const RUNS = +arg('runs', 9);

// ------------------------------------------------- stubs on the REAL globalThis
function stub() {
  const f = function () {};
  return new Proxy(f, {
    get(t, p) {
      if (p === Symbol.iterator) return function* () {};
      if (p === Symbol.toPrimitive) return () => 0;
      if (p === 'length') return 0;
      if (p === 'then') return undefined;
      return stub();
    },
    set() { return true; }, has() { return true; },
    apply() { return stub(); }, construct() { return stub(); },
  });
}
const g = globalThis;
/* defineProperty, not assignment: several of these (navigator) are getter-only
   accessors on the Node global and a plain `g.navigator = ...` throws. */
const def = (k, v) => Object.defineProperty(g, k, { value: v, writable: true, configurable: true });
def('document', stub()); def('navigator', stub()); def('location', stub());
def('getComputedStyle', () => stub());
def('AudioContext', function () { return stub(); });
def('webkitAudioContext', g.AudioContext);
/* The lab arms requestAnimationFrame(frame) and a setInterval rAF fallback at
   top level. In the main realm those would be REAL timers: left alone they fire
   frames into the middle of a timing run and hold the process open. */
def('requestAnimationFrame', () => 0); def('cancelAnimationFrame', () => {});
def('setInterval', () => 0); def('clearInterval', () => {});
def('setTimeout', () => 0); def('clearTimeout', () => {});
def('addEventListener', () => {}); def('alert', () => {});
def('Image', class {});
def('hrt', process.hrtime.bigint.bind(process.hrtime));
def('window', g); def('self', g);

const html = readFileSync(LAB, 'utf8');
const blocks = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);

// ------------------------------------------------------------------ the bench
/* Bodies are laid out DETERMINISTICALLY by the lab's own mulberry32 rather than
   taken from a preset: presets come in fixed counts (2, 3, 4) and the spec's
   range is 2..8, so a like-for-like N sweep needs one generator. Ring placement
   with a seeded jitter keeps every N inside the box and clear of the 0.1-wide
   cushion band at t=0, so the cushion variant's cost is not already sitting in
   the baseline. */
const BENCH = `
const SEED = 0x51DE;
function __setup(N){
  const r = mulberry32(SEED);
  nodes = [];
  for(let i=0;i<N;i++){
    const th = 2*Math.PI*i/N, rad = 0.22 + 0.06*r(), sp = 0.18 + 0.10*r();
    nodes.push(mk({ name:'B'+i, m: 0.6 + 1.0*r(),
      x: 0.5 + rad*Math.cos(th), y: 0.5 + rad*Math.sin(th),
      vx: -sp*Math.sin(th), vy: sp*Math.cos(th), tx:'none', ty:'none' }));
  }
  P.G=0.025; P.eps=0.06; P.bounds='reflect'; P.com=true; P.scale=1;
  P.damp=0; P.cushK=0; P.cushW=0.1; P.thermoOn=false; P.vdpOn=false; P.vdpG=0;
  simTime=0;
}
const __variants = {
  baseline(){},
  damp(){ P.damp = 0.2; },
  cushion(){ P.cushK = 0.5; },
  com_off(){ P.com = false; },
  wrap(){ P.bounds = 'wrap'; },
  thermo(){ if(typeof thermostat!=='function') throw new Error('no thermostat in this lab'); P.thermoOn=true; P.thermoV=0.4; P.thermoTau=0.5; },
  vdp_wall(){ if(typeof vdpBand!=='function') throw new Error('no vdpBand in this lab'); P.vdpOn=true; P.vdpG=-0.5; },
  vdp_full(){ if(typeof vdpBand!=='function') throw new Error('no vdpBand in this lab'); P.vdpOn=true; P.vdpG=0.5; P.vdpR0=0.2; },
  all_on(){ P.damp=0.2; P.cushK=0.5; P.thermoOn=true; P.vdpOn=true; P.vdpG=0.5; },
};

/* The blocks frame() runs BESIDE the physics, lifted verbatim so what is
   attributed to them is what the lab actually pays. */
/* The mean-speed strip arrived with the regulators (B126); the pre-regulators
   lab used as control B has no vbarHist. Hoisted to a const so the guard is one
   monomorphic boolean load, not a typeof in the timed loop. */
const __hasVbar = typeof vbarHist !== 'undefined';
function __recordTick(){
  for(const b of nodes){ b.hx[b.hp]=outX(b); b.hy[b.hp]=outY(b); b.ha[b.hp]=outAngle(b); b.hr[b.hp]=outRadius(b); b.hp=(b.hp+1)%b.hx.length; }
  if(__hasVbar){ vbarHist[vbarP]=meanSpeed(); vbarP=(vbarP+1)%vbarHist.length; }
}
function __trailTick(){
  for(const b of nodes){ b.trail.push([b.x,b.y]); if(b.trail.length>220)b.trail.shift(); }
}
let __sink = 0;
function __obsTick(){ for(const b of nodes) __sink += outX(b)+outY(b)+outAngle(b)+outRadius(b); }
function __obsXYTick(){ for(const b of nodes) __sink += outX(b)+outY(b); }          // cartesian only
function __obsPolarTick(){ for(const b of nodes) __sink += outAngle(b)+outRadius(b); } // atan2 + hypot
function __energyTick(){ const e = energy(); __sink += e.K + e.U; }
function __meanSpeedTick(){ __sink += meanSpeed(); }
function __nullTick(){ __sink += 1; }          // the FLOOR control

function __median(a){ a=a.slice().sort((x,y)=>x-y); return a[(a.length-1)>>1]; }

function __timeStep(N, variant, steps, runs){
  /* THREE FULL RUNS ARE DISCARDED, not a plain step warm-up. With only a step
     warm-up the first two timed runs came in at 802 and 467 ns against a settled
     160 — V8 was still tiering up inside the measurement, and a mean over that
     is a lie the median only partly survives. The discarded runs are identical
     in shape to the timed ones (setup included), so nothing tiers up later. */
  const out=[];
  for(let r=0;r<runs+3;r++){
    __setup(N); __variants[variant]();
    const t0=hrt();
    for(let i=0;i<steps;i++) step(DT);
    const ns = Number(hrt()-t0)/steps;
    if(r>=3) out.push(ns);
  }
  return out;
}
function __timeBlock(N, variant, fn, iters, runs){
  __setup(N); __variants[variant]();
  for(let i=0;i<3000;i++) step(DT);             // a settled, representative state
  for(let i=0;i<50000;i++) fn();                // warm
  const out=[];
  for(let r=0;r<runs;r++){
    const t0=hrt();
    for(let i=0;i<iters;i++) fn();
    out.push(Number(hrt()-t0)/iters);
  }
  return __median(out);
}
function __fingerprint(N, variant, steps){
  __setup(N); __variants[variant]();
  for(let i=0;i<steps;i++) step(DT);
  let h=0x811c9dc5;
  for(const b of nodes) for(const v of [b.x,b.y,b.vx,b.vy]) h=Math.imul(h^(v*1e9|0),0x01000193)>>>0;
  return h;
}
function __trajectory(N, variant, steps, every){
  __setup(N); __variants[variant]();
  const out=[];
  for(let i=0;i<steps;i++){ step(DT); if(i%every===0) for(const b of nodes) out.push(b.x,b.y,b.vx,b.vy); }
  return out;
}
/* The fastest any body gets, against MAXV. The max-speed clamp costs a
   Math.hypot per body per step unconditionally; whether it ever ENGAGES is a
   separate question from what it costs, and this is the direct answer rather
   than the indirect "removing it changed no bits". */
function __peakSpeed(N, variant, steps){
  __setup(N); __variants[variant]();
  let mx=0;
  for(let i=0;i<steps;i++){ step(DT); for(const b of nodes){ const s=Math.hypot(b.vx,b.vy); if(s>mx)mx=s; } }
  return { peak: mx, maxv: MAXV, headroom_x: MAXV/mx };
}
globalThis.__api = { __timeStep, __timeBlock, __fingerprint, __median, __trajectory, __peakSpeed,
  blocks: { nul:__nullTick, record:__recordTick, trail:__trailTick, obs:__obsTick,
            obsxy:__obsXYTick, obspolar:__obsPolarTick, energy:__energyTick, meanspeed:__meanSpeedTick },
  hasVdp: typeof vdpBand === 'function', hasThermo: typeof thermostat === 'function',
  DT: DT, HIST_EVERY: HIST_EVERY };
`;

/* Lab and bench compiled as ONE script: they must share the global lexical
   scope (that is how the bench reaches `nodes`, `P`, `step`), and one script
   context is also the fastest thing V8 can give a global-heavy program. */
vm.runInThisContext(blocks.join('\n') + '\n' + BENCH, { filename: 'lab+bench' });
const api = g.__api;

let runs;
try { runs = api.__timeStep(N, VARIANT, STEPS, RUNS); }
catch (e) {
  console.log(JSON.stringify({ lab: LAB.split('/').pop(), n: N, variant: VARIANT, error: e.message }));
  process.exit(0);
}
const median = api.__median(runs);

// The per-frame blocks do not depend on which regulator is on, so they are
// measured on the baseline variant only; per-variant would be N x V of noise.
const blk = name => +api.__timeBlock(N, VARIANT, api.blocks[name], 20000, RUNS).toFixed(3);
// --traj runs exist only to compare trajectories; timing the frame blocks there
// would double their cost for nothing.
const extra = (VARIANT === 'baseline' && !process.argv.includes('--traj'))
  ? { ns_null_floor: blk('nul'), ns_record_tick: blk('record'), ns_trail_tick: blk('trail'),
      ns_obs_all: blk('obs'), ns_obs_xy: blk('obsxy'), ns_obs_polar: blk('obspolar'),
      ns_energy: blk('energy'), ns_meanspeed: blk('meanspeed') }
  : {};

const out = {
  lab: LAB.split('/').pop(), n: N, variant: VARIANT, steps: STEPS, nruns: RUNS,
  ns_per_step: +median.toFixed(2),
  ns_per_step_min: +Math.min(...runs).toFixed(2),
  runs_ns: runs.map(v => +v.toFixed(1)),
  spread_pct: +(((Math.max(...runs) - Math.min(...runs)) / median) * 100).toFixed(1),
  ...extra,
  fingerprint: api.__fingerprint(N, VARIANT, 2000),
  dt: api.DT, hist_every: api.HIST_EVERY,
  node: process.version, has_vdp: api.hasVdp, has_thermo: api.hasThermo,
};
if (process.argv.includes('--traj')) out.traj = api.__trajectory(N, VARIANT, 4000, 40);
out.peak_speed = api.__peakSpeed(N, VARIANT, 4000);
console.log(JSON.stringify(out));
```

</details>

<details><summary>make_ablations.mjs — scratch-copy ablations and rewrites, every anchor asserted</summary>

```javascript
/*
 * make_ablations.mjs — scratch copies of the lab with one mechanism removed each.
 *
 * Some of what the brief asks to cost is not switchable from the UI (the pair
 * loop, the max-speed clamp, the wall handling, the second Verlet accel call).
 * Reporting those as "not separable" would be giving up a number that exists,
 * so they are ablated in a SCRATCH COPY — the lab in the repo is never touched.
 *
 * EVERY MUTATION ASSERTS ITS ANCHOR (LIBRARY L0032 corollary): a replace that
 * silently matches nothing produces a file identical to the baseline, which
 * then times identically, which reads exactly like "this feature is free".
 * count must be 1 or this script throws.
 */
import { readFileSync, writeFileSync } from 'node:fs';
import { resolve, dirname, join } from 'node:path';

const SRC = resolve(process.argv[2]);
const OUT = dirname(SRC);
const src = readFileSync(SRC, 'utf8');

const ANCHORS = {
  // CONTROL: gravity itself. Must time measurably FASTER than the baseline, or
  // the timer is not seeing the pair loop at all and the whole N-sweep is void.
  nopair: `    for(let j=i+1;j<n;j++){
      const b=nodes[j];
      let dx=b.x-a.x, dy=b.y-a.y;
      if(P.bounds==='wrap'){ if(dx>0.5)dx-=1; if(dx<-0.5)dx+=1; if(dy>0.5)dy-=1; if(dy<-0.5)dy+=1; }
      const r2=dx*dx+dy*dy+e2;
      const inv=P.G/(r2*Math.sqrt(r2));
      a.ax+=dx*inv*b.m; a.ay+=dy*inv*b.m;
      b.ax-=dx*inv*a.m; b.ay-=dy*inv*a.m;
    }
`,
  // The max-speed clamp: unconditional, one Math.hypot per body per step.
  noclamp: `    const s=Math.hypot(b.vx,b.vy); if(s>MAXV){b.vx*=MAXV/s;b.vy*=MAXV/s;}
`,
  // Wall handling (reflect / wrap), also unconditional per body per step.
  nobounds: `    if(P.bounds==='reflect'){
      if(b.x<0){b.x=-b.x;b.vx=Math.abs(b.vx);} else if(b.x>1){b.x=2-b.x;b.vx=-Math.abs(b.vx);}
      if(b.y<0){b.y=-b.y;b.vy=Math.abs(b.vy);} else if(b.y>1){b.y=2-b.y;b.vy=-Math.abs(b.vy);}
    } else if(P.bounds==='wrap'){
      b.x=((b.x%1)+1)%1; b.y=((b.y%1)+1)%1;
    }
`,
  /* The SECOND O(N^2) force evaluation. Velocity Verlet as written calls accel()
     twice per step; the standard formulation carries the end-of-step
     acceleration into the next step's kick and calls it once. Removing the
     leading call is NOT that refactor and changes the physics — it is here only
     to put a number on what that refactor is worth. */
  oneaccel: `  // velocity Verlet (kick–drift–kick)
  accel();
`,
};

/* REWRITES, not removals: a candidate fix, so the saving it buys is measured
   rather than argued from a microbenchmark. Same anchor discipline. */
const REWRITES = {
  // Math.hypot is 16x Math.sqrt(x*x+y*y) and 44x the squared compare
  // (profile_prims.mjs). The clamp only needs the magnitude when it fires.
  fastclamp: [
    `    const s=Math.hypot(b.vx,b.vy); if(s>MAXV){b.vx*=MAXV/s;b.vy*=MAXV/s;}
`,
    `    const s2=b.vx*b.vx+b.vy*b.vy; if(s2>MAXV*MAXV){const s=Math.sqrt(s2);b.vx*=MAXV/s;b.vy*=MAXV/s;}
`],
  // meanSpeed() runs once per step under the thermostat and once per strip tick.
  fastmean: [
    `  for(const b of nodes){ if(b.pinned||b.drag)continue; n++; s+=Math.hypot(b.vx,b.vy); }
`,
    `  for(const b of nodes){ if(b.pinned||b.drag)continue; n++; s+=Math.sqrt(b.vx*b.vx+b.vy*b.vy); }
`],
};

for (const [name, [anchor, replacement]] of Object.entries(REWRITES)) {
  const n = src.split(anchor).length - 1;
  if (n !== 1) throw new Error(`rewrite anchor '${name}' matched ${n} times, expected exactly 1`);
  const out = src.replace(anchor, replacement);
  if (out === src) throw new Error(`rewrite '${name}' produced an identical file`);
  writeFileSync(join(OUT, `lab-ablate-${name}.html`), out);
  console.log(`${name}: anchor matched 1x, wrote lab-ablate-${name}.html (rewrite)`);
}

for (const [name, anchor] of Object.entries(ANCHORS)) {
  const n = src.split(anchor).length - 1;
  if (n !== 1) throw new Error(`anchor '${name}' matched ${n} times, expected exactly 1 — the ablation would have been a silent no-op`);
  const out = src.replace(anchor, `/* ABLATED: ${name} */\n`);
  if (out === src) throw new Error(`ablation '${name}' produced an identical file`);
  const path = join(OUT, `lab-ablate-${name}.html`);
  writeFileSync(path, out);
  console.log(`${name}: anchor matched 1x, wrote ${path} (${src.length - out.length} bytes removed)`);
}
```

</details>

<details><summary>drive.mjs — the sim matrix and the five controls</summary>

```javascript
/*
 * drive.mjs — runs the whole B126 profile matrix, one child process per cell,
 * and prints the tables plus the two must-fail controls.
 */
import { execFileSync } from 'node:child_process';
import { resolve, join } from 'node:path';

const DIR = resolve(process.argv[2] || '.');
const P = join(DIR, 'profile_sim.mjs');
const run = (lab, n, variant, extra = []) => {
  const out = execFileSync('node', [P, '--lab', join(DIR, lab), '--n', String(n), '--variant', variant, ...extra],
    { encoding: 'utf8', maxBuffer: 1 << 28 });
  return JSON.parse(out.trim().split('\n').pop());
};

const NS = [2, 3, 4, 6, 8];
const FEATURES = ['damp', 'cushion', 'com_off', 'wrap', 'thermo', 'vdp_wall', 'vdp_full', 'all_on'];
const ABLATIONS = ['nopair', 'noclamp', 'nobounds', 'oneaccel', 'fastclamp', 'fastmean'];

const result = { node: null, sweep: {}, features: {}, ablations: {}, blocks: {}, controls: {} };

// ---------------------------------------------------------- 1. N sweep (baseline)
console.error('# N sweep');
for (const n of NS) {
  const r = run('lab-main.html', n, 'baseline');
  result.node = r.node;
  result.sweep[n] = r;
  result.blocks[n] = {
    floor: r.ns_null_floor, record: r.ns_record_tick, trail: r.ns_trail_tick,
    obs_all: r.ns_obs_all, obs_xy: r.ns_obs_xy, obs_polar: r.ns_obs_polar,
    energy: r.ns_energy, meanspeed: r.ns_meanspeed,
  };
  console.error(`  N=${n}  ${r.ns_per_step.toFixed(1)} ns/step  (spread ${r.spread_pct}%)`);
}

// -------------------------------------------- 2. switchable features at N=4 and 8
console.error('# features');
for (const n of [4, 8]) {
  result.features[n] = {};
  for (const v of FEATURES) {
    const r = run('lab-main.html', n, v);
    result.features[n][v] = r;
    const d = r.ns_per_step - result.sweep[n].ns_per_step;
    console.error(`  N=${n} ${v.padEnd(9)} ${r.ns_per_step.toFixed(1)}  delta ${d >= 0 ? '+' : ''}${d.toFixed(1)} ns`);
  }
}

// ------------------------------------------- 3. source ablations at N=4 and N=8
console.error('# ablations');
for (const n of [4, 8]) {
  result.ablations[n] = {};
  for (const a of ABLATIONS) {
    const r = run(`lab-ablate-${a}.html`, n, 'baseline');
    result.ablations[n][a] = r;
    const d = result.sweep[n].ns_per_step - r.ns_per_step;   // cost of the REMOVED thing
    console.error(`  N=${n} ${a.padEnd(9)} ${r.ns_per_step.toFixed(1)}  removed costs ${d.toFixed(1)} ns`);
  }
}

// ---------------------------------------------------------------- 4. controls
console.error('# controls');
/* CONTROL A (the brief's): with the pair loop stubbed the step MUST be
   measurably faster, or the timer is not seeing the O(N^2) loop and every
   number above is about something else. */
/* The bar is NOT a flat percentage. At N=4 the pair loop is six pairs against a
   fixed per-body cost (clamp, walls, recentre, two Verlet passes) that does not
   shrink, so gravity is a small share there and a 15% bar would fail for a
   reason that has nothing to do with whether the timer sees the loop. The
   signature of "the timer sees an O(N^2) loop" is that the saving GROWS with N,
   so that is what is asserted, alongside a positive saving at both N. */
{
  const s = {};
  for (const n of [4, 8]) {
    const base = result.sweep[n].ns_per_step, abl = result.ablations[n].nopair.ns_per_step;
    s[n] = { baseline_ns: base, nopair_ns: abl, saved_ns: +(base - abl).toFixed(1),
             saved_pct: +(((base - abl) / base) * 100).toFixed(1),
             spread_pct: result.sweep[n].spread_pct };
  }
  result.controls.A_nopair_is_visible_and_grows_with_N = {
    ...s,
    pass: s[4].saved_ns > 0 && s[8].saved_ns > 0 && s[8].saved_ns > s[4].saved_ns * 2,
    note: 'stubbing the pair loop must be faster at both N and the saving must grow superlinearly',
  };
}
/* CONTROL B (the brief's second): the two lab versions must produce
   bit-identical trajectories with the regulators off. The brief names
   origin/main and origin/orbital-regulators, but PR #595 MERGED (67e095e), so
   those two files are byte-identical (sha256 8352db2c...) and the comparison is
   VACUOUS as a physics control. The control the brief WANTED is main vs the
   commit before the regulators landed (2876005), which is what runs here. */
{
  const a = run('lab-main.html', 8, 'baseline', ['--traj']);
  const b = run('lab-preregulators.html', 8, 'baseline', ['--traj']);
  const n = a.traj.length;
  let diff = 0;
  for (let i = 0; i < n; i++) if (!Object.is(a.traj[i], b.traj[i])) diff++;
  result.controls.B_regulators_off_bit_identical = {
    doubles: n, differing: diff, pass: diff === 0 && n > 0,
    note: 'main vs 2876005 (pre-regulators), both regulators off, N=8, 4000 steps sampled every 40',
  };
  /* CONTROL B-neg: the same comparator against a lab that IS different must
     report non-zero, or "0 differing" is a check that cannot fail (L0032b).
     FIRST ATTEMPT USED lab-ablate-noclamp AND DID NOT FIRE — 0/3200. That is
     recorded below as a FINDING rather than retried away (L0033): removing the
     max-speed clamp changes no bit of the trajectory because no body ever
     reaches MAXV, so the clamp is pure cost at these knob values. The negative
     control now uses nopair, where divergence is structural. */
  const c = run('lab-ablate-nopair.html', 8, 'baseline', ['--traj']);
  let diff2 = 0;
  for (let i = 0; i < n; i++) if (!Object.is(a.traj[i], c.traj[i])) diff2++;
  result.controls.B_negative_comparator_can_fire = {
    doubles: n, differing: diff2, pass: diff2 > 0,
    note: 'same comparator, main vs lab-ablate-nopair — must be non-zero',
  };
  const d = run('lab-ablate-noclamp.html', 8, 'baseline', ['--traj']);
  let diff3 = 0;
  for (let i = 0; i < n; i++) if (!Object.is(a.traj[i], d.traj[i])) diff3++;
  /* The headline recommendation, verified rather than argued: the squared-compare
     clamp must be FASTER and must leave the trajectory bit-identical. */
  const e = run('lab-ablate-fastclamp.html', 8, 'baseline', ['--traj']);
  let diff4 = 0;
  for (let i = 0; i < n; i++) if (!Object.is(a.traj[i], e.traj[i])) diff4++;
  result.controls.D_fastclamp_is_free_and_identical = {
    doubles: n, differing: diff4,
    baseline_ns: result.sweep[8].ns_per_step,
    fastclamp_ns: result.ablations[8].fastclamp.ns_per_step,
    saved_ns: +(result.sweep[8].ns_per_step - result.ablations[8].fastclamp.ns_per_step).toFixed(1),
    pass: diff4 === 0 && result.ablations[8].fastclamp.ns_per_step < result.sweep[8].ns_per_step,
    note: 'Math.hypot -> squared compare in the max-speed clamp: faster AND bit-identical at N=8',
  };
  const f = run('lab-ablate-fastmean.html', 8, 'thermo', ['--traj']);
  const a2 = run('lab-main.html', 8, 'thermo', ['--traj']);
  let diff5 = 0;
  for (let i = 0; i < n; i++) if (!Object.is(a2.traj[i], f.traj[i])) diff5++;
  /* meanSpeed() is only called from step() when the thermostat is on, so the
     saving has to be measured there, not at baseline. And the assertion is that
     the substitution DOES change the trajectory — sqrt(x*x+y*y) is not
     bit-identical to hypot, and a regulator that reads the mean feeds that
     difference straight back into the physics. Writing the assertion the other
     way round made this print FAIL for the expected result. */
  const thermoBase = result.features[8].thermo.ns_per_step;
  const thermoFast = run('lab-ablate-fastmean.html', 8, 'thermo').ns_per_step;
  result.controls.E_fastmean_saves_only_under_thermostat_and_is_NOT_identical = {
    doubles: n, differing: diff5,
    baseline_off_saving_ns: +(result.sweep[8].ns_per_step - result.ablations[8].fastmean.ns_per_step).toFixed(1),
    thermo_on_ns: thermoBase, thermo_on_fastmean_ns: thermoFast,
    thermo_on_saving_ns: +(thermoBase - thermoFast).toFixed(1),
    pass: diff5 > 0,
    note: 'ASSERTS THE SUBSTITUTION IS NOT FREE. meanSpeed() is unreached from step() with the '
        + 'thermostat off, so the baseline saving is ~0; with it on the saving is real, but the '
        + 'trajectory moves, so this one is a physics change and needs a ruling, not a patch.',
  };
  result.controls.C_maxspeed_clamp_never_engages = {
    doubles: n, differing: diff3,
    peak_speed: a.peak_speed,
    pass: diff3 === 0 && a.peak_speed.peak < a.peak_speed.maxv,
    note: 'RECORDED COVERAGE BOUNDARY, not a gate on the lab: removing the clamp changes nothing '
        + 'and the peak speed reached is far below MAXV, so the clamp costs what it costs and does nothing',
  };
}

console.log(JSON.stringify(result, null, 1));
for (const [k, v] of Object.entries(result.controls))
  console.error(`  ${v.pass ? 'PASS' : 'FAIL'}  ${k}  ${JSON.stringify(v)}`);
```

</details>

<details><summary>profile_draw.mjs — counting canvas context + minimal DOM</summary>

```javascript
/*
 * profile_draw.mjs — how many canvas operations ORBITAL's render path issues per
 * frame, and which control drives each one.
 *
 * A headless harness cannot measure frame TIME (there is no rasteriser and no
 * compositor), so it measures the only thing it can honestly see: the number and
 * kind of 2D-context calls, counted by a stub context, plus the number of
 * getComputedStyle() calls, which is the one render cost a call count DOES
 * settle — css() at line 211 of the lab is
 *   getComputedStyle(document.documentElement).getPropertyValue(v).trim()
 * and in a browser each of those is a forced style resolution.
 *
 * The lab's own minimal DOM is supplied here rather than the generic Proxy stub,
 * because the point is to be `cv.getContext('2d')` — the counter has to BE the
 * context the lab captured at load, not one swapped in afterwards (cx is a
 * top-level const and cannot be reassigned).
 *
 * Usage: node profile_draw.mjs --lab <file> --n <N> [--trails 0|1] [--polar 0|1]
 */
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import vm from 'node:vm';

const arg = (k, d) => { const i = process.argv.indexOf('--' + k); return i < 0 ? d : process.argv[i + 1]; };
const LAB = resolve(arg('lab'));
const N = +arg('n', 8);
const TRAILS = arg('trails', '1') !== '0';
const POLAR = arg('polar', '0') !== '0';

// ------------------------------------------------------------ counting context
const counts = Object.create(null);
const bump = k => { counts[k] = (counts[k] || 0) + 1; };
const CTX_METHODS = ['beginPath', 'moveTo', 'lineTo', 'stroke', 'fill', 'arc', 'rect', 'fillRect',
  'clearRect', 'strokeRect', 'fillText', 'strokeText', 'save', 'restore', 'setLineDash',
  'closePath', 'translate', 'scale', 'rotate', 'createRadialGradient', 'createLinearGradient',
  'quadraticCurveTo', 'bezierCurveTo', 'drawImage', 'measureText', 'ellipse',
  'setTransform', 'resetTransform', 'transform', 'clip', 'createPattern', 'roundRect'];
const CTX_PROPS = ['fillStyle', 'strokeStyle', 'lineWidth', 'globalAlpha', 'font', 'textAlign',
  'textBaseline', 'lineCap', 'lineJoin', 'globalCompositeOperation'];
function makeCtx(tag) {
  const c = {};
  for (const m of CTX_METHODS) c[m] = (...a) => {
    bump(`${tag}.${m}`);
    if (m === 'createRadialGradient' || m === 'createLinearGradient')
      return { addColorStop: () => bump(`${tag}.addColorStop`) };
    if (m === 'measureText') return { width: 10 };
    return undefined;
  };
  // property WRITES are state changes the driver pays for too; count them
  for (const p of CTX_PROPS) {
    let v = '';
    Object.defineProperty(c, p, { get: () => v, set: x => { bump(`${tag}.set:${p}`); v = x; } });
  }
  c.canvas = { width: 600, height: 600 };
  return c;
}

// ------------------------------------------------------------------ mini DOM
function el(tag = 'div', id = '') {
  const e = {
    tagName: tag, id, dataset: {}, children: [], _ctx: null,
    style: { setProperty() {}, removeProperty() {}, getPropertyValue: () => '' },
    className: '',
    width: 600, height: 600, value: '0.5', checked: false, textContent: '', innerHTML: '',
    title: '', selectedIndex: 0, options: [],
    classList: { add() {}, remove() {}, toggle() {}, contains: () => false },
    addEventListener() {}, removeEventListener() {}, appendChild(c) { this.children.push(c); return c; },
    remove() {}, setAttribute() {}, getAttribute: () => null,
    getBoundingClientRect: () => ({ left: 0, top: 0, width: 600, height: 600 }),
    setPointerCapture() {}, releasePointerCapture() {},
    getContext(kind) {
      // which canvas this is decides which counter tag its calls land under
      const tag2 = id === 'field' ? 'field' : (id === 'vstrip' ? 'vstrip' : 'strip');
      return (this._ctx ||= makeCtx(tag2));
    },
    querySelector(sel) { return (this._q ||= {})[sel] ||= el('div', id + sel); },
    querySelectorAll() { return []; },
  };
  return e;
}
const registry = Object.create(null);
const byId = id => (registry[id] ||= el('div', id));

const g = globalThis;
const def = (k, v) => Object.defineProperty(g, k, { value: v, writable: true, configurable: true });
let cssCalls = 0;
def('document', {
  getElementById: byId,
  querySelectorAll: () => [],
  createElement: t => el(t),
  documentElement: el('html', 'html'),
  addEventListener() {},
});
def('getComputedStyle', () => ({ getPropertyValue: () => { cssCalls++; return '#7fd3e6'; } }));
def('navigator', {}); def('location', {}); def('devicePixelRatio', 2);
def('AudioContext', function () { return { destination: {}, createGain: () => ({ connect() {}, gain: { value: 0 } }) }; });
def('webkitAudioContext', g.AudioContext);
def('requestAnimationFrame', () => 0); def('cancelAnimationFrame', () => {});
def('setInterval', () => 0); def('clearInterval', () => {});
def('setTimeout', () => 0); def('clearTimeout', () => {});
def('addEventListener', () => {}); def('alert', () => {});
def('Image', class {});
def('window', g); def('self', g);

const html = readFileSync(LAB, 'utf8');
const blocks = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);

const BENCH = `
const SEED = 0x51DE;
function __setup(N){
  const r = mulberry32(SEED);
  nodes = [];
  for(let i=0;i<N;i++){
    const th = 2*Math.PI*i/N, rad = 0.22 + 0.06*r(), sp = 0.18 + 0.10*r();
    nodes.push(mk({ name:'B'+i, m: 0.6 + 1.0*r(),
      x: 0.5 + rad*Math.cos(th), y: 0.5 + rad*Math.sin(th),
      vx: -sp*Math.sin(th), vy: sp*Math.cos(th), tx:'none', ty:'none' }));
  }
  P.G=0.025; P.eps=0.06; P.bounds='reflect'; P.com=true; P.scale=1;
  P.damp=0; P.cushK=0; P.cushW=0.1; P.thermoOn=false; P.vdpOn=false; P.vdpG=0;
  simTime=0;
}
/* The trails are filled to their 220-sample cap and the strips to their 240 —
   a frame counted on a cold sim would undercount the two loops that matter. */
function __fill(N){
  __setup(N);
  for(let i=0;i<20000;i++){
    step(DT);
    if(i%HIST_EVERY===0){
      for(const b of nodes){ b.hx[b.hp]=outX(b); b.hy[b.hp]=outY(b); b.ha[b.hp]=outAngle(b); b.hr[b.hp]=outRadius(b); b.hp=(b.hp+1)%b.hx.length; }
      if(typeof vbarHist!=='undefined'){ vbarHist[vbarP]=meanSpeed(); vbarP=(vbarP+1)%vbarHist.length; }
    }
    if(i%4===0) for(const b of nodes){ b.trail.push([b.x,b.y]); if(b.trail.length>220)b.trail.shift(); }
  }
  return { trail: nodes[0].trail.length, strip: nodes[0].hx.length, S: S };
}
globalThis.__api = { __setup, __fill, draw, drawXYGraph,
  drawVbar: typeof drawVbar==='function' ? drawVbar : null,
  P, nodes: () => nodes, S: () => S };
`;
vm.runInThisContext(blocks.join('\n') + '\n' + BENCH, { filename: 'lab+bench' });
const api = g.__api;

const filled = api.__fill(N);
api.P.trails = TRAILS;
api.P.polar = POLAR;

// ---------------------------------------------------------------- count a frame
function snapshot() { return { ...counts, __css: cssCalls }; }
function delta(a, b) {
  const out = {};
  for (const k of new Set([...Object.keys(a), ...Object.keys(b)])) {
    const d = (b[k] || 0) - (a[k] || 0);
    if (d) out[k] = d;
  }
  return out;
}
const sum = o => Object.entries(o).reduce((s, [k, v]) => k === '__css' ? s : s + v, 0);

const before = snapshot();
api.draw();
const fieldOps = delta(before, snapshot());

// per-body strip graph (updateReadouts calls this once per body per frame)
const b0 = snapshot();
const fakeStrip = el('canvas', 'strip');
api.drawXYGraph(fakeStrip, api.nodes()[0]);
const stripOps = delta(b0, snapshot());

// the mean-speed strip (once per frame)
let vbarOps = null;
if (api.drawVbar) {
  const b1 = snapshot();
  api.drawVbar();
  vbarOps = delta(b1, snapshot());
}

const perFrame = {
  field: { ops: sum(fieldOps), css: fieldOps.__css || 0, detail: fieldOps },
  strip_one_body: { ops: sum(stripOps), css: stripOps.__css || 0, detail: stripOps },
  vbar: vbarOps ? { ops: sum(vbarOps), css: vbarOps.__css || 0, detail: vbarOps } : null,
};
perFrame.total_ops = perFrame.field.ops + perFrame.strip_one_body.ops * N + (vbarOps ? perFrame.vbar.ops : 0);
perFrame.total_css = perFrame.field.css + perFrame.strip_one_body.css * N + (vbarOps ? perFrame.vbar.css : 0);

console.log(JSON.stringify({
  lab: LAB.split('/').pop(), n: N, trails: TRAILS, polar: POLAR,
  trail_len: filled.trail, strip_len: filled.strip,
  per_frame: perFrame, node: process.version,
}, null, 1));
```

</details>

<details><summary>drive_draw.mjs — the render-path configuration sweep</summary>

```javascript
/* drive_draw.mjs — the render-path op-count sweep, one child per configuration. */
import { execFileSync } from 'node:child_process';
import { resolve, join } from 'node:path';

const DIR = resolve(process.argv[2] || '.');
const run = (n, trails, polar) => JSON.parse(execFileSync('node',
  [join(DIR, 'profile_draw.mjs'), '--lab', join(DIR, 'lab-main.html'),
   '--n', String(n), '--trails', trails ? '1' : '0', '--polar', polar ? '1' : '0'],
  { encoding: 'utf8', maxBuffer: 1 << 28 }));

const CFGS = [
  [2, 1, 0], [3, 1, 0], [4, 1, 0], [6, 1, 0], [8, 1, 0],
  [8, 0, 0],            // trails off
  [8, 1, 1],            // polar strips
];
const rows = [];
for (const [n, t, p] of CFGS) {
  const r = run(n, t, p);
  const f = r.per_frame;
  rows.push({
    n, trails: !!t, polar: !!p,
    field: f.field.ops, strip_one: f.strip_one_body.ops, strips_total: f.strip_one_body.ops * n,
    vbar: f.vbar ? f.vbar.ops : 0, total: f.total_ops,
    css_field: f.field.css, css_total: f.total_css,
    detail_field: f.field.detail,
  });
  console.error(`N=${n} trails=${t} polar=${p}  field=${f.field.ops}  strip/body=${f.strip_one_body.ops}  vbar=${f.vbar ? f.vbar.ops : 0}  TOTAL=${f.total_ops}  css=${f.total_css}`);
}
console.log(JSON.stringify(rows, null, 1));
```

</details>

<details><summary>profile_prims.mjs — math primitive costs, each against a null loop</summary>

```javascript
/*
 * profile_prims.mjs — the cost of the individual math primitives the step uses,
 * so a recommendation like "replace Math.hypot with the squared comparison" is a
 * measured claim and not folklore.
 *
 * Each case is timed against a NULL case of the same loop shape; the reported
 * figure is the difference, and the null figure is printed so a reader can see
 * how much of each number is loop. Values come from a pre-filled array so the
 * optimiser cannot constant-fold them, and every result is accumulated into a
 * sink that is printed, so nothing is dead-code-eliminated.
 */
const hrt = process.hrtime.bigint.bind(process.hrtime);
const M = 1024;
const xs = new Float64Array(M), ys = new Float64Array(M);
for (let i = 0; i < M; i++) { xs[i] = (i % 37) * 0.031 - 0.5; ys[i] = (i % 53) * 0.019 - 0.5; }

let sink = 0;
const CASES = {
  null_loop:   () => { for (let i = 0; i < M; i++) sink += xs[i]; },
  hypot:       () => { for (let i = 0; i < M; i++) sink += Math.hypot(xs[i], ys[i]); },
  sqrt_sumsq:  () => { for (let i = 0; i < M; i++) sink += Math.sqrt(xs[i] * xs[i] + ys[i] * ys[i]); },
  sumsq_only:  () => { for (let i = 0; i < M; i++) sink += xs[i] * xs[i] + ys[i] * ys[i]; },
  exp:         () => { for (let i = 0; i < M; i++) sink += Math.exp(xs[i]); },
  atan2:       () => { for (let i = 0; i < M; i++) sink += Math.atan2(xs[i], ys[i]); },
  sqrt:        () => { for (let i = 0; i < M; i++) sink += Math.sqrt(xs[i] + 1); },
  div:         () => { for (let i = 0; i < M; i++) sink += 1 / (xs[i] + 2); },
  min4:        () => { for (let i = 0; i < M; i++) sink += Math.min(xs[i], 1 - xs[i], ys[i], 1 - ys[i]); },
};

const med = a => { a = a.slice().sort((p, q) => p - q); return a[(a.length - 1) >> 1]; };
function time(fn) {
  for (let r = 0; r < 200; r++) fn();                 // warm
  const out = [];
  for (let r = 0; r < 11; r++) {
    const t0 = hrt();
    for (let i = 0; i < 2000; i++) fn();
    out.push(Number(hrt() - t0) / (2000 * M));
  }
  return med(out);
}

const raw = {};
for (const k of Object.keys(CASES)) raw[k] = time(CASES[k]);
const floor = raw.null_loop;
const net = {};
for (const [k, v] of Object.entries(raw)) if (k !== 'null_loop') net[k] = +(v - floor).toFixed(3);

console.log(JSON.stringify({
  ns_per_call_net: net,
  ns_loop_floor: +floor.toFixed(3),
  hypot_vs_sqrt_sumsq: +(net.hypot / net.sqrt_sumsq).toFixed(2),
  hypot_vs_sumsq_only: +(net.hypot / net.sumsq_only).toFixed(2),
  node: process.version, sink_nonzero: sink !== 0,
}, null, 1));
```

</details>
