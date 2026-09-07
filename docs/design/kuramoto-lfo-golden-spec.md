# Acceptance spec — hardening the Kuramoto LFO *rotor* to a golden reference

**Status:** pre-golden spec (2026-07-20). Design brief: `docs/proposals/2026-07-20-kuramoto-lfo.md`
(ADR-053). This document defines exactly what the `swarmlfo-rotor.html` concept
test must become before it is a portable reference that can be ingested and
ported like every other engine (ADR-003 / ADR-052 prototype-first). The
prototype today is a **non-golden concept test**
(`docs/design/kuramoto-lfo-prototypes/NON-GOLDEN.md`); this is the checklist that
graduates it.

All measured numbers below are **PROVISIONAL** — taken from the current concept
test to ground the anchors in real behavior. Every one is re-measured on the
hardened reference and only *then* written into specs/ACCEPTANCE.md (L0002: an
acceptance number encodes a measurement protocol; it comes from measuring the
reference, never from aspiration).

---

## 1. The one structural decision: the primitive is the modulation stream, not the demo audio

The rotor prototype renders **audio** through a demo synth (exciter → filter →
chorus → saturation) so you can hear it. **That synth is auditioning scaffolding
and is NOT the golden contract.** In the plugin, the four LFO outputs route to
real param ids via the mod matrix; the demo destinations do not ship.

So the golden reference renders, and the C++ port matches, the **control-rate
modulation streams**:

| Logged per control tick | Field | Notes |
|---|---|---|
| 4 LFO phases | `phase[0..3]` | ∈ [0,1) |
| 4 shaped outputs | `lfo[0..3]` | ∈ [−1,1], after the shape selector |
| order-parameter magnitude | `R` | ∈ [0,1] |
| order-parameter angle | `psi` | ∈ (−π,π] |

Parity (§5) is RMS over these streams. **The demo synth audio is explicitly out
of scope** — do not build parity on it, and prefer to strip it from the golden
core entirely (leave only the LFO bank + coupling + shaping), so the reference is
the primitive and nothing else.

---

## 2. Structural contract to freeze

From the concept-test code (verified by reading `swarmlfo-rotor.html`), the facts
the golden reference pins and the port must reproduce:

- **NV = 4** voices, fixed. TICK = 16 samples → control rate 2756 Hz at 44.1 kHz
  (same tick as the audio engines — reuse it, do not invent a rate; ADR-053).
- **Phase-domain Kuramoto** (this is SwarmCore's family, NOT force_core):
  `R = |Σ e^{i·2π·phaseᵢ}| / NV`, `psi = arg(Σ …)`, updated each control tick.
- **Bipolar coupling**, `kHz = (K>0 ? K² : −K²)·3.0`, applied per sample:
  - K>0 (sync): `adv += kHz·R·sin(psi − 2π·phaseᵢ)/sr` — pull toward the mean phase, scaled by R.
  - K<0 (splay): pull toward an assigned even slot — **see §7, this law is defective as written**.
  - K=0: free-run at each voice's own rate.
- **Shape selector** `shape ∈ {0 sine, 1 tri, 2 saw, 3 square}`, one global choice,
  applied identically to every voice via `shapeOf(phase)`.
- **Fully deterministic — NO RNG.** Phases initialise fixed at 0 / 0.25 / 0.5 / 0.75;
  rates are `{rW,rC,rH,rS}·gRate`. Same params → bit-identical streams (measured:
  max|ΔR| = 0 across two runs). This makes the parity contract unusually clean and
  means **no seed axis is needed** for the rotor (unlike every audio engine). When
  the later axes add seeded drift (brief §7), a named mulberry32 stream re-enters
  and the seed axis returns.

---

## 3. Harness deliverables (mirror the existing engine pattern)

1. **`tools/golden/gen_kuramotolfo_goldens.mjs`** — extracts `RotorLFO` headlessly
   (`extract_core.mjs`; the DSP banners at lines 153/306 and `const TICK,TAU,NV`
   already make it extractable — confirmed), renders the §6 scenario matrix,
   writes the modulation-stream logs (`phase[4]`, `lfo[4]`, `R`, `psi` at every
   control tick) + a manifest, and supports `--selfcheck` (render each scenario
   twice, byte-compare — the determinism gate).
2. **`tools/kuramotolfo_check.cpp`** — the C++ parity harness: replays each
   scenario's param script in application order, renders the same streams from the
   ported `kuramoto_lfo.h`, RMS-compares against the golden log. Reports per-scenario
   RMS (like `parity_check`). MSVC-safe (no `M_PI` — local `constexpr double kPi`;
   L0003).
3. **specs/ACCEPTANCE.md rows** — the §7 behavioural anchors as new L0 numbers (next free
   L0-2x), each stating its measurement protocol. *Protected-path edit — human gate
   at that time.*
4. **`./verify` wiring** — a tenth oracle chain (`kuramotolfo`) in `full`; the
   structure gate need not list the prototype (it is non-golden and lives under
   `docs/design/`, deliberately not in the protected reference set).

---

## 4. Determinism & extraction gates (Layer-0, must pass first)

- **Extractable:** `extractCore(rotor.html, 'RotorLFO')` returns a working class
  with `constructor(sr)`, `setParam(k,v)`, `render(L,R)`, and readable
  `phase/lfo/R/psi`. (Confirmed today.)
- **Self-contained DSP section:** no `document`/`window`/`canvas`/`requestAnimationFrame`
  reference above the Audio-graph banner. (Confirmed — all DOM code is below it.)
- **Constructor-initialises every externally-read field** (brief lesson (a)):
  `lfo`, `R`, `psi`, `phase` are all set at construction, so a reader at tick 0
  never sees `undefined`. (Confirmed — `lfo` is explicitly constructor-init.) The
  port's struct must do the same so the GUI/mod-bus reader (render thread) can read
  it before the first control tick.
- **Determinism:** `--selfcheck` byte-identical across two renders (provisional:
  exact, max|ΔR| = 0).

## 5. Parity contract

C++ port correctness = **RMS(stream_diff) ≤ 1e-6** for every scenario × every
logged channel (`phase`, `lfo`, `R`, `psi`), against the JS reference. Expect
bit-exact (0.0) on the polynomial channels and shapes (tri/saw/square measured
0.0); sine and the sin-coupling accrue transcendental ULP only (sine shape
measured 1.3e-10 ≪ 1e-6) — the same class as the audio engines' feedback paths.

---

## 6. Scenario matrix

Per lesson (c), **every scenario integrates ≥ 4 full cycles of the slowest voice**
(slowest default rate 0.4 Hz → 2.5 s period → render ≥ 12 s; measure over a steady
window, e.g. 4–12 s). Per-block metrics are invalid for a sub-audio-rate signal.

- **K sweep** (the spine): K ∈ {−1, −0.5, −0.2, 0, 0.2, 0.35, 0.7, 1.0}, shape = sine.
- **Shape set**: shape ∈ {sine, tri, saw, square} at K = 0 (free-run, isolates shaping).
- **Rate config**: default rates {0.7, 0.4, 0.95, 0.55}·gRate; one alt gRate (e.g. 2×)
  to exercise `updateRates`.
- (Later axes — rate/depth/destination coupling, tempo gravity, seeded drift — add
  their own scenarios when those axes are hardened; not part of the rotor golden.)

Each scenario is `(K, shape, rates, gRate)` → the deterministic stream log.

---

## 7. Behavioural acceptance anchors (PROVISIONAL — re-measure on the golden)

Stated as criteria + protocol; the numbers are today's concept-test measurements
(steady window 4–12 s, SR 44.1 kHz, default rates, shape sine unless noted).

| # | Anchor | Criterion | Provisional | Protocol |
|---|---|---|---|---|
| A1 | **Sync locks** | K ≥ 0.7 → mean R ≥ 0.95, sd → 0 | K=0.7 R=0.990 sd=0.000; K=1.0 R=0.998 | mean/sd of R over steady window |
| A2 | **Free-run floor** | K = 0 → mean R near the N=4 incoherent floor (~0.5) | R=0.475 | same |
| A3 | **Strong-K monotonicity** | R at K=1.0 > R at K=0.35 > R at K=0 | 0.998 > 0.648 > 0.475 ✓ | NB: **not** monotone at weak K (K=0.2 R=0.469 ≈ free-run) — state as "high at strong K, floor at weak," never "monotone in K" |
| A4 | **Splay lowers coherence** | K ≤ −0.5 → mean R < the free-run floor | K=−1 R=0.190; K=−0.5 R=0.255 ✓ | same |
| A5 | **Splay reaches even interleave** | K = −1 → min pairwise phase gap ≈ 1/NV = 0.25, R ≈ 0 | **FAILS: gap=0.038, phases cluster into 2 pairs** | min circular gap of the 4 end phases — see the pre-golden fix below |
| A6 | **Shape fidelity** | `lfo[i]` == analytic shape at `phase[i]` within 1e-6 | sine 1.3e-10; tri/saw/square 0.0 ✓ | worst \|lfo − shapeOf(phase)\| over voices |
| A7 | **Determinism** | two identical renders byte-identical | max\|ΔR\| = 0 ✓ | `--selfcheck` |

### Pre-golden fix REQUIRED (A5) — the splay law

The concept-test splay (`slot = psi + 2π·i/NV; adv += kHz·sin(slot − 2π·phaseᵢ)`)
does **not** converge to even interleave — it settles into a 2-cluster (anti-phase
pairs) state (measured K=−1 → phases `[0.17, 0.23, 0.75, 0.79]`). Because
**splay-past-free is the headline differentiator vs Foxfire's unipolar macro**
(brief §1/§2), this must be resolved before golden:

- **Decide the intended target for N = 4:** true even interleave (phases at
  `c + k/4`, R = 0) or the anti-phase 2-cluster the code currently reaches. The
  brief says "maximally even interleave," so A5 as written assumes even interleave.
- If even interleave is intended, the splay law needs fixing — the psi-relative
  index assignment fights itself as psi drifts. A standard fix is to target a fixed
  even lattice offset per voice with an anti-phase (2nd-harmonic) repulsion term, or
  to assign slots by current rank rather than static index. **Prototype-first:** the
  fix lands in the HTML lab and is ear-checked, then A5 is measured on it.
- Whichever target is chosen, A5's number is measured on the fixed reference and is
  a hard gate — this anchor exists specifically to prevent shipping the broken splay.

---

## 8. Measurement-protocol traps to bake into the harness

- **Multiple cycles, never one block (lesson c):** the #1 false-bug source in the
  brief. Any R/coherence/correlation metric integrates over ≥ 4 LFO cycles. A
  per-block reading of a 0.4 Hz LFO is meaningless.
- **Measure the audible quantity, not just the coordinate (lesson d):** the audible
  effect is *phase* coherence; the port must couple phase (it does — the coupling is
  phase-domain), and any behavioural anchor is on phase R, not on rate agreement.
- **State the protocol with the number (L0002):** window bounds, SR, control rate,
  param application order, initial phases — an anchor without its protocol is not
  reproducible and will drift.
- **Do not build "thin → resonance pitch" (brief §6, self-falsified):** that finding
  was a spectral-centroid/brightness shift misread as pitch by a zero-crossing proxy
  — the same confound class as L0002 and the K-alignment squareness experiment. If a
  future axis graduates resonance pitch, it modulates base tap spacing directly, not
  voice count.

---

## 9. Out of scope for the rotor golden

- The demo synth (exciter/filter/chorus/saturation) — auditioning only; strip it.
- Rate / depth / destination coupling axes — each hardens and ports separately,
  behind the rotor, with its own anchors and scenarios (brief §3, ADR-053).
- Seeded drift, tempo/consonance gravity — re-introduce the RNG/gravity machinery
  (and the seed axis) when those axes are added; the rotor golden is RNG-free.
- The mod-matrix routing surface — standard sources×destinations scaffolding; the
  rotor golden only guarantees the source (the four `lfo[]` values + R).

---

## Amendments from the mod lab (2026-07-24, `docs/design/mod-lab.html`)

Human-directed design evolution auditioned in the lab; the golden re-freezes
these when the rotor graduates:

- **A5 splay law RESOLVED (rank-based lattice + rate entrainment).** Slots are
  assigned by current circular rank (anchor = rank-0 voice); attraction uses
  |kHz| (kHz is negative for K<0 — using it raw repels and CLUSTERS, measured
  meanR 0.842). Each voice's rate blends toward the MEAN rate by |K|, so at
  K=−1 the lattice is rigid at the global rate — the mirror of K=+1's cluster.
  Measured (spec §6 protocol): gap 0.250 exact, meanR 0.000, R sd 0.0000
  (legacy: gap 0.048, meanR 0.189, sd 0.1567). A1–A3 unchanged to the digit
  (0.475/0.469/0.648/0.990/0.998); A7 determinism exact. A5's golden number
  re-measures on the hardened reference as planned. **EAR-CHECKED AND CONFIRMED
  by the human 2026-07-24** ("splay now working properly") — §7's pre-golden fix
  requirement is DISCHARGED and the rotor may proceed to the §3 harness
  deliverables.
- **THE ROTOR BECAME A SWARM — §2's structural contract is SUPERSEDED (human
  direction, 2026-07-24 evening).** Asked for the SAW oscillator's surface on
  the modulation source, the rotor now carries: variable voice count (1..8),
  a rate-DETUNE law with an anchor selector (geometric mean / slowest /
  fastest voice pinned to the global rate), and the topology family
  (mean-field / ring+reach / two-cluster+μ+balance). Three §2 lines no longer
  hold: **NV = 4 fixed** (now 1..8), **one global shape** (already per-voice
  since the morning), and **"no seed axis is needed"** (still true today —
  the swarm remains RNG-free — but only until a seeded rate distribution is
  added, which the detune law invites). CONSEQUENCE: **do not harden the
  rotor to a golden until these axes settle** — a golden measured now would be
  re-measured within a session, and its ACCEPTANCE rows (a protected-path
  edit) would churn with it. The deeper observation, worth its own ADR when
  the port comes: the mod source and the audio engine are now the SAME
  phase-domain Kuramoto swarm at two rates, so the port should reuse
  SwarmCore's machinery rather than grow a parallel implementation.
  Measured on the generalised swarm (12 s, steady window 4–12 s):
  splay gap = **exactly 1/n** at n ∈ {2,4,6,8}; free-run floor falls
  0.631 → 0.425 → 0.389 → 0.360 (tracking the 1/√n incoherent floor);
  ring coherence rises monotonically with reach (r1 0.842 · r2 0.966 ·
  r3 0.983 at n=8, K=0.7); A/B balance splays cluster B (0.988 → 0.634 →
  0.365). A1–A3 shift slightly from the concept-test numbers (K=0 0.425,
  K=0.7 0.974, K=1 0.994) because the hard-coded rate set {0.7,0.4,0.95,0.55}
  is replaced by the detune law — expected, and the reason those anchors are
  re-measured on the hardened reference rather than copied.
- **Degenerate-equilibrium trap (measured, and a defaults decision).** At
  detune 0 every voice shares one rate, and evenly-spaced phases with
  identical rates is an exact fixed point of mean-field Kuramoto: R = 0, so
  the coupling force (∝ R) is exactly zero and the lattice never moves at ANY
  K. The concept test never hit this because its four rates were hard-coded
  heterogeneous. The state is real physics and stays reachable, but the
  DEFAULT detune is non-zero (0.3, ≈1.2 octaves of spread) so the rotor does
  not present as broken. Any future golden must state the rate configuration
  with its anchors — an anchor measured at detune 0 is measuring the fixed
  point, not the dynamics.
- **Satellite swarms + link taper (chorus/phaser own swarms).** Each effect
  now drives from its OWN swarm, with a `link` amount pulling it toward the
  main swarm's mean phase (0 = independent, 1 = entrained). The gain is
  **link² · 2 · R_master**: a linear gain locked the satellite by link 0.15 and
  did nothing above it — the entire slider was one step. Quadratic puts the
  threshold mid-travel (measured ψ-coherence: link 0.20 → 0.019, 0.35 → 0.257,
  0.50 → 1.000), so "partly participating" is reachable. Same taper failure
  ADR-059 recorded for the inertia knob; worth treating as a standing check on
  any new coupling control.
- **Polarity is a matrix-level question, not a rotor one (human note,
  2026-07-24).** Unipolar vs bipolar per source/route (ROADMAP, mod-matrix
  polarity entry) affects how `lfo[]` and `R` are CONSUMED, not how they are
  produced — the rotor golden logs raw `lfo[]∈[−1,1]` and `R∈[0,1]` and stays
  polarity-agnostic. Recorded here so the eventual port does not bake a
  polarity convention into the source.
- **Per-voice shape (§2 delta).** `shape` is now per-voice `shape0..3` (human,
  2026-07-24) — the structural contract's "one global choice" is superseded;
  the golden logs are unchanged in form (lfo[i] already per-voice).
- **Consolidation finding (env→K vs onset lock).** The generic K destination
  cannot reproduce onset lock: km saturates at 4 (K=1) while onset 0.8 injects
  8·0.64 = 5.12 — the dedicated param out-reaches the whole knob range. A
  coupling-domain destination (lab: `Kboost`, in Kenv units) closes it:
  measured rms(A−B) = 0.002 over 3 s against a 5.12 peak. CONSEQUENCE for the
  consolidation review: retiring onset/dissolve via the matrix requires the
  mod bus to reach the COUPLING TARGET, not the clamped K knob.
- **Consolidation finding #2 (per-note instances; 2026-07-24 evening).** Onset
  lock is PER-NOTE (each note's swarm seeds its own Kenv, decaying on its own
  clock); a GLOBAL envelope re-surges every sounding note on each new key —
  heard by the human, then measured on a staggered two-note test: when note 2
  arrives, note 1's coupling under a global env jumps 2.10 → 4.93 (rms vs
  onset lock 1.274), while a PER-NOTE env instance matches onset lock at rms
  0.002 on BOTH notes. CONSEQUENCE: the mod matrix must distinguish per-note
  sources (envelopes) from global sources (LFOs, the rotor) — retiring
  onset/dissolve requires per-note envelope instances routed to per-note
  coupling. With #1 + #2 both met, the lab A/B is indistinguishable mono AND
  poly; the retirement case is closed at lab level, pending the human's ear.
  (The mono-only rms 0.002 previously reported was honest but incomplete —
  global and per-note are structurally identical with one note sounding.)
