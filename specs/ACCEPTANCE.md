# horde — acceptance criteria

Two layers, per ATTEST discipline. **Layer-0**: deterministic, CI-blocking, runs in `./verify fast`. **Layer-E**: behavioral/listening checks, `./verify full` + human sign-off. Every number below was measured on the JS reference implementations in this design cycle; tolerances are stated where drift is acceptable.

Standard test note: A3 = 220 Hz, 44.1 kHz, defaults unless stated. "R" = first-harmonic order parameter of the focus swarm. Time bases in seconds of rendered audio.

## L0-1 · Parity oracle (the master test)

C++ core vs JS reference (`SwarmSynth` / `SpectraSynth` / `DynSynth`): identical mulberry32 streams, identical seeding scheme, identical control-tick ordering → sample outputs match within ε = 1e-6 RMS over 4 s renders, for a matrix of {3 seeds} × {per-engine param vectors covering each subsystem}. Any intentional divergence requires an ADR.

**Domain limit (ADR-065, human-approved 2026-07-23).** L0-1 is a valid oracle only in non-chaotic regimes. Where the dynamics have a positive Lyapunov exponent — measured case: the harmonic law (4) at full spread under strong coupling — a 1-ULP perturbation of the JS reference *alone* diverges to rms ≈ 9.4e-2, matching the C++/JS gap, so sample-exact agreement is impossible in principle (`Math.sin` and `std::sin` are not identically rounded). Such regimes are excluded from the golden matrix and covered by behavioural anchors in `trajectory_check` instead: boundedness, NaN-cleanliness, and the qualitative claim the parameter exists to make, each checked with stated margin. Excluding a scenario on these grounds requires the same evidence ADR-065 records — a bracketed divergence curve **and** a reference-only perturbation test — never a bare assertion that a failure is "just floating point."

## L0-2 · Sync phase transition (HYPERSAW core; engine renamed from SAW 2026-08-17, ADR-091)

From retriggered start, K in ×σ units, R sampled ~1/s:

| K setting | Required behavior | Reference measurement |
|---|---|---|
| 4.0×σ (knob +1.0) | full lock, R ≥ 0.95 sustained | R = 0.97 steady |
| 2.9×σ (+0.85) | steady partial lock | R ≈ 0.92 steady |
| ~2.0×σ (+0.7) | oscillating partial lock (shimmer) | R oscillates ≈ 0.24 ↔ 0.78 |
| 1.4×σ (+0.6) | subcritical dissolve from coherent start | R: 0.97 → 0.16 over ~3 s |

Tolerance: regime classification must match; R values ±0.08.

## L0-3 · Splay attractor (saw kernel)

K = −1.0, JP distribution, M = 7, 4 s settle: R₁ ≤ 0.10 (measured 0.04); splay order R_N ≥ 0.75 (measured 0.84); spectral projection at 7·f0 ≥ 5× the uncoupled baseline (measured 9×: 0.0017 → 0.0153, approaching f0's 0.0226). Intermediate check: K = −0.7 → R_N ≈ 0.44 ±0.1 (the continuum exists).

**L0-3 extension — R₁/R_N regime classification** (added 2026-07-17 per the splay-legibility change note, archived at `docs/change-notes/2026-07-17-splay-legibility.md`; measured on the reference at A3, defaults, settled; values ±0.08). R_N is computed per SPEC §5.6 and is parity-relevant (same loop order, same n as the reference):

| Regime | K | R₁ | R_N |
|---|---|---|---|
| Sync | +1.0 | 0.97 | 0.07 |
| Free | 0.0 | see erratum | see erratum |
| Splay | −1.0 | 0.04 | 0.84 |

*Erratum (ratified at the Phase 1 gate, 2026-07-18 — see traces/2026-07-17-phase1-swarmcore-parity.md):* the free-row single values originally recorded here (R₁ 0.39 / R_N 0.16) were snapshots of a fluctuating observable — for n = 7 free oscillators both orders fluctuate around ~1/√n ≈ 0.38 — and do not reproduce even on the reference. Free is classified by time average: dense-sampled ⟨R₁⟩ and ⟨R_N⟩ over a ≥5 s settled window both in (0.1, 0.6) (reference: ≈0.33/0.33). Protocol notes for the other rows: the shimmer pair (L0-2, K=+0.7) is the dense-sampled min/max — 1/s sampling aliases it flat; the K=+0.6 dissolve figures describe the initial transient, with "subcritical" meaning no sustained lock afterwards.

## L0-4 · Inertia

ζ = 0.45. K = +0.85 with inertia 0.5: R hunts in ≈ 0.74–0.85 band without settling to steady lock over 8 s. Inertia 0.8: sustained chaotic hunting (R excursions below 0.4). No NaN, no unbounded momentum, for the full {K × inertia} grid.

## L0-5 · R→tone routing

rtone = +1.0 at full lock: one-pole coefficient falls from bypass (≈0.9) to ≈0.04 (cutoff ~16 kHz → ~280 Hz). Sign flip inverts the mapping against (1−R).

## L0-6 · Cascade zipper (SPECTRA)

K = +0.9, cascade = 1.0, retrigger off, P = 12: lock front (R_k > 0.85) advances monotonically low→high, completing in 7–10 s (reference: 8 s, with turbulence ahead of the front). Cascade = 0.5: completes in ≤ 3 s (reference 2 s). Monotonicity of the front, not exact timing, is the blocking criterion; timing within ±30%.

## L0-7 · Interference gate (SPECTRA)

K = −1.0 vs K = 0, defaults, 4 s settle: RMS reduction ≥ 12 dB (reference −14.8 dB), with per-partial stereo narrowing active (w = K_splay/(K_splay + 2σ)). Regression guard: without narrowing, reference leaked to −1.7 dB — a result near that value means the narrowing path broke.

## L0-8 · Consonance gravity (DYNAMICS)

Notes 220 Hz + 331 Hz (fifth +5.2¢ sharp), gravity 0.7, ~3 s: settled ratio within ±0.5¢ of 3/2 (reference: 1.50001, +0.01¢). Pairs outside basin (default ±35¢) move 0¢. Per-pair readout reports captured ratio name, octave fold, live cents error.

## L0-9 · Sakaguchi lag

Mean-field, K = +0.9, locked: α = 60° shifts locked mean frequency down ≈ 1.8 Hz from 220 (reference 218.19). Direction and monotonicity in α blocking; magnitude ±25%.

## L0-10 · Two-cluster broken symmetry

n = 32, detune 2¢, K = +1.0, μ = 0.6, retrigger off, 25 s, averaged over final 10 s: at α = 78°, sustained cluster asymmetry with ⟨R_A⟩ ≈ 0.82 / ⟨R_B⟩ ≈ 0.67 (±0.08), consistent across seeds {1234, 777, 42}. At α = 86°: seed-dependent regimes (variance across seeds ≥ 2× the α=78° variance). Note: chimera-like states under 2¢ heterogeneity; strict-chimera experiments require absolute-K mode (ADR-004).

## L0-11 · Ring spatial coexistence

Ring, reach 5, α = 80°, K = +0.9, retrigger off, 6 s: local order parameter (±3-neighbor window) spans min ≤ 0.2 and max ≥ 0.8 around the ring (reference 0.08 / 0.87).

## L0-12 · Tempo-grid law

120 BPM, 1 cycle/beat (u = 2 Hz): all pairwise voice-frequency gaps are exact multiples of u (machine precision). Spread scales with detune knob: 10¢ → 4 Hz / 3 rungs; 60¢ → 16 Hz / 9 rungs (M = 16). Envelope periodicity: DFT of squared signal shows ≥ 6× power at u vs an incommensurate probe (reference 10.2× at 2.00 vs 1.37 Hz). Coherence reachable: K = +0.85 over grid law locks to R ≥ 0.88 within 2 s (reference 0.92 in ~1 s).

## L0-13 · Determinism & stability

Same seed + note sequence → bit-identical control-path state and ε-identical audio across runs. Full param-grid fuzz (including stretch × wtilt × poly chords): no NaN, no denormal stall, master peak ≤ 1.0 post-tanh.

## Layer-E (behavioral, non-blocking, human-verified per release)

- E-1: K sweep at ~2×σ is *audibly* a journey (cloud → herd → shimmer → comb → saw), not a switch.
- E-2: ERB-flat law: same patch at C2 vs C6 keeps subjectively comparable roughness; cents law audibly does not.
- E-3: Gravity settling on a held ET major third is audible as decelerating-then-stopped beating within ~2 s.
- E-4: Splay erasure residue (sine kernel) reads as "shimmering silence," not artifact.
- E-5: Mono fold-down of a wide patch is usable, not comb-hollow.
- E-6: 8-voice poly at M = 32 (HYPERSAW) sustains without dropouts on the target hardware envelope (ratified 2026-07-17 at the Phase 0 gate): min-spec = Apple M1 base / 4-core 2018-class Intel ultrabook; Windows x64 AVX2; 44.1 kHz at 128-sample buffer; the patch must hold < 50% of one core on min-spec.


<!-- Track E criteria ingested 2026-07-18 (packet UPDATE-001); numbering L0-14..21 continues the sequence — the Phase 3 Daido formalization planned in ROADMAP will take L0-22+ -->

# ACCEPTANCE delta — APPEND to ACCEPTANCE.md (numbering continues; skip any already present)

## L0-14 · Filter bank dynamics (SPEC-FILTER §3)
K=+1: σ(log2 fc) reduced to 0.20 ±0.05 of free value (spring-ratio law), collapse meter ≥ 0.75. K=−1: sorted-gap CV ≤ 0.02 (reference 0.009). Regression anchor: free-run σ 0.755, CV 0.123 at defaults.

## L0-15 · Harmonic gravity, bank
Gravity +0.8, basin 45¢, f0 = 110: in-basin mean |cents to nearest harmonic| = h/(h+g) of initial ±20% (reference 28.8¢ → 6.9¢ at h=1.5, g=4.8). Anti-gravity −0.8: zero filters remain within 40¢ of any harmonic (from ≥3 initially). The equilibrium-law prediction itself is the criterion — if h or g change, the target changes with them.

## L0-16 · Notch exactness (SPEC-FILTER §4)
True-notch cascade, mix 1: measured null at each swarm frequency ≥ 60 dB below the adjacent ridge (reference ~150 dB; criterion is loose because window/leakage-dependent). Regression guard: values scattering below 15 dB or going negative indicate the allpass-topology defect has returned.

## L0-17 · Tuned harmonic rejection
Gravity +0.9, basin 60¢, f0 = 110: every gravity-captured notch (within 12¢ of a harmonic) rejects ≥ 15 dB at the harmonic vs 90¢ off (reference 19–24 dB). Shared-core check: gather σ and comb CV match L0-14 within tolerance.

## L0-18 · Filter-family stability
Bank: K + inertia 0.7 + drift 150¢ + Q ceiling simultaneously — bounded, NaN-clean, 4 s. Notch swarm: feedback 0.85 + K 0.8 + drift 150¢ + stage Q 3 — peak ≤ 0.7 post-tanh, NaN-clean, 4 s.

## L0-19 · Tap swarm (SPEC-EFFECTS §5)
Gather σ to 0.20 ±0.05 of free; grid CV ≤ 0.08 (reference 0.055). Rhythmic gravity: in-basin error = h/(h+g)·initial ±20% (reference 34.9→8.3¢, law-exact). Impulse honesty: echoes detected at every tap time ±2 ms. **LF stability (regression guard for the √N trap):** 12 s sustained note at regen 0.50 and 0.97 — buffer mean |amplitude| flat and ≤ 0.15 (runaway reference: 0.42 and climbing at 10 s).

## L0-20 · FDN room swarm (SPEC-EFFECTS §6)
Room→note: K=+1, regen 0.85 — IR comb at 1/L̄ ≥ 10 dB over off-comb probe (reference 13.5 dB with 4 Hz blocker). Sympathetic: gravity +0.9, basin 80¢, f0=110 — captured lines settle per equilibrium law ±20% (reference 34.3→7.4¢); IR f0-vs-off ratio improves ≥ 2.5 dB over free (reference +3.7). Matrix regression guard: comb probe at 1/L̄ measuring ≤ 0 dB indicates the Householder sign defect has returned (anti-resonance reference −1.4 dB).

## L0-21 · Time-family stability & DC
Room 12 s at regen 0.95 with note held: bounded output, per-line DC ≤ 0.01, NaN-clean. DC-blocker corner ≤ (lowest intended comb)/6; blocker cost at a 26 Hz comb ≤ 3 dB (reference 2.8).


## L0-22 · Daido q-clusters, bistability, split-as-timbre (ratified at the Phase 3 gate)

Protocol: mean-field, even spread, lpOut 0, n = 24, detune 0.2, retrig off, K = +1.0, A3.
- q ∈ {2, 3}: R_q >= 0.95 after 6 s across seeds {1234, 777, 42} (reference 0.97; ADR-015a).
- Bistability: same knobs with retrig ON (aligned start) sustains full sync R1 >= 0.95 (ADR-015d).
- Split-as-timbre (q = 2, final 2 s of 8 s): the 2f0 projection is seed-invariant at 0.080 +/- 0.015 while the f0 residual varies across seeds by >= 0.01 (ADR-015c; the cluster split is a fundamental-vs-octave mix the seed rolls).
Enforced in trajectory_check; numbers per ADR-015's independently re-verified measurements.


## L0-23 · Two-cluster A/B balance (ADR-051, ratified at ingest of the swarmdynamics balance clone)

Protocol: two-cluster topology, mu=0.3, K=+1, detune 0.3, retrig off, A3, 3 s settle. The `balance` knob scales cluster B's intra-coupling by kB = 1 − 2·balance (A stays gain 1).
- balance 0: kB=1 — BIT-IDENTICAL to the pre-knob two-cluster path (parity golden `dyn-cluster-balance` proves the port matches the reference at balance 0.5 too). R_A, R_B >= 0.9.
- balance 0.5: cluster B's coupling removed; R_A holds >= 0.9 while (R_A − R_B) >= 0.4 (B dissolves).
- balance 1.0: kB=−1, B splayed; full split (R_A − R_B) >= 0.5.
- mu sets the R_B floor (contamination): R_B(mu=0.9) > R_B(mu=0.05) at balance 1.
Anchors are robust (split, monotone floor) rather than exact R_B floor values, which are settle-window-sensitive near ~1/sqrt(n) (the L0002 measurement-protocol caveat). Enforced in trajectory_check; K is UNIPOLAR in this engine (4·K²·σ) so balance<0.5→>0.5 is the only cluster-splay axis.

## L0-24 · Bipolar onset lock (ADR-056, C++-only superset beyond the reference range)

Protocol: n=16, detune 0.3, retrig off, dissolve 0.5, A3, 4 s. Onset lock is a note-start coupling burst (Kenv = 8·onset·|onset|) that decays over `dissolve`. Range widened 0..1 → −1..1.
- onset ≥ 0: BIT-IDENTICAL to the reference (8·onset·|onset| == 8·onset² there; the sync/splay routing reduces to the reference's `max(0,km)+Kenv`). Every existing golden proves it — parity untouched. onset < 0 is a NEW C++-only path (no JS reference; validated behaviorally, like ADR-025 super-width).
- Direction (K=0.9): splay-onset (onset −1) early R < sync-onset (onset +1) early R by a clear margin — negative gives an initial splay burst, positive a sync burst.
- Dissolve neutralizes: at a strongly-locking K the two CONVERGE once Kenv→0 (late |ΔR| < 0.06) — dissolve returns the coupling boost to neutral. NOTE: near-critical K, the splay-vs-sync initial condition can settle into different basins even after Kenv→0 (genuine path-dependence/multistability, not a failure); the anchor therefore checks convergence only in the guaranteed regime.
Enforced in trajectory_check (ADR-056 block). Revertible: the change is a signed `fabs` + a sign-routed splay term.
