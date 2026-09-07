<!-- Ingested 2026-07-18 from design-session packet UPDATE-001 (archived at docs/change-notes/). ADR references remapped to this repo's numbering: packet 010-012 = local 015-017 (already present); packet 013-016 = local 029-032. SPEC-FILTER.md was never ingested here; supersession note is vacuous locally. -->

# horde — effects specification (consolidated: filter · phaser · delay · reverb)

Supersedes `SPEC-FILTER.md` (ADR-031). Four engines on **one shared force system**, applying swarm dynamics to filter frequencies and delay times at control rate. All four accept external audio in the VST — this is the project's effect product line. State-domain coupling (ring phases/amplitudes; Stuart–Landau) remains parked behind the physical-modeling bridge.

Reference implementations: `reference/swarmfilter.html` (FilterLab), `reference/swarmphaser.html` (PhaserLab), `reference/swarmtime.html` (TimeLab, echo + room modes).

## 1. The shared force system

Population state: v_i = log2(x_i), where x is Hz (filters/notches) or seconds (taps/lines). Integrated per 16-sample control tick (2756 ticks/s @ 44.1k — ADR-009 applies). Forces in log2-units/s:

| Force | Rate | Notes |
|---|---|---|
| Home spring | 1.5/s — **EXPOSED** (ADR-029a) | toward placement target + drift offset |
| Sync (K>0) | 6·K² | toward population mean |
| Splay (K<0) | 18·K² | toward even slots across placement span (3× authority) |
| Gravity | ±6·\|g\| | toward/away from nearest attractor (per-engine attractor sets below), active inside basin only |
| Drift | seeded mean-reverting walks, depth ¢ / rate | in time domain this is literally wow & flutter |
| Inertia | 2nd-order, ζ = 0.45 | overshoot and ring in transit |

**Equilibrium law (three-for-three, exact):** competing springs settle at `err_final = h/(h+g)·err_initial`. Bank gravity 28.8→6.9¢; echo rhythmic gravity 34.9→8.3¢ (predicted 8.3); room sympathetic 34.3→7.4¢ (predicted 7.4). This is now a *design law*: acceptance criteria cite it directly, and it means the home spring sets "how much the dynamics argue with the placement" — full snap requires h→0 or the parked capture-tightening mode.

**Stability laws (both discovered by measurement — ADR-031):**
1. **Feedback normalization must assume worst-case correlation.** N taps/lines reading correlated low-frequency content sum with gain N, not √N. The echo loop at √N feedback norm had LF loop gain regen·√N — unstable above regen ≈ 0.35, producing the measured runaway (buffer mean 0.04→0.42 over 10 s at *defaults*, then tanh-saturation squashing the audio). Feedback paths normalize by N; output paths may use √N for level.
2. **DC-block inside every feedback loop, with the corner far below the lowest musical comb.** The FDN's +1 eigenchannel passes DC at loop gain ≈ regen. A 15 Hz blocker fixed DC but, compounding per pass, gutted a 26 Hz comb (16.3→2.6 dB); at ~4 Hz the comb holds 13.5 dB and line DC stays ≤ 0.006 under 12 s sustained play. Rule: blocker corner ≤ (lowest intended comb)/6.

## 2. Placement × distribution (shared Layer 1)

Orthogonal menus per the oscillator engine's factorization. Placement laws: ERB spread · log spread · harmonic (n·f0, retargets per note) for frequency engines; log-time spread around a size knob for time engines. Distributions: even · Gaussian · Cauchy, seeded, sorted ascending (index order = position order, required by splay slots and viz).

## 3. Engine A — resonator bank (`swarmfilter`)

N≤24 unity-gain bandpass (TPT SVF, band·k), coefficients per tick; collapse→Q routing (Q_eff = Q_base·(1+amt·3·collapse), cap 60). Gravity attractors: harmonics n·f0 of last note (bipolar: resolve ↔ curdle).
Measured: collapse σ 0.755→0.151 (meter 0.80 = spring ratio); comb gap CV 0.123→**0.009**; gravity 28.8→6.9¢ in-basin; anti-gravity expels all filters beyond 40¢ (5→0); stable under full simultaneous modulation.

## 4. Engine B — notch swarm (`swarmphaser`)

N≤12 cascaded **true notch** sections (SVF: y = x − k·v1) — allpass cascade **rejected by measurement** (notches form at cumulative-phase odd-π, not stage centers; pseudo-depths scattered ±10 dB — ADR-029b). Feedback 0–0.85 tanh-bounded; drift-as-sweep default on (seeded walks replace the LFO; never loops).
Measured: ~150 dB nulls exactly at swarm frequencies; tuned harmonic rejection 19–24 dB at gravity-captured harmonics (residual 7–10¢ per equilibrium law); gather/comb identical to bank (shared core verified); stable at max feedback.

## 5. Engine C — tap swarm delay (`swarmtime`, echo mode)

N≤12 read taps on one buffer (~3 s), interpolated reads (moving taps chirp = tape; discrete between-repeat motion parked as a mode). Feedback per stability law 1, DC-blocked, damped. Gravity attractors: **rhythmic ratio set {¼, ⅓, ½, ⅔, ¾, 1, 1½, 2, 3, 4} × beat** (BPM-driven; host-sync in VST) — consonance gravity in the time domain.
Measured: gather σ 0.328→0.066; grid CV 0.720→0.055; rhythmic gravity 34.9→8.3¢ (law-exact); impulse echoes verified at **8/8** tap times; post-fix long-run bounded at regen 0.50 *and* 0.97 (buffer mean ~0.095, flat).

## 6. Engine D — FDN room swarm (`swarmtime`, room mode)

N≤12 delay lines (4–170 ms), **negated Householder** mixing so the input-excited all-ones eigenchannel carries +1 → resonances at k/L (ADR-030a: matrix sign is load-bearing; any topology-as-matrix work must state which eigenchannel the input excites). Per-line damping LP + 4 Hz DC blocker (stability law 2). Gravity attractors: **multiples of the played note's period** — the sympathetic room.
Measured: room→note collapse yields a 13.5 dB comb at 1/L̄ (19.3 dB at its 2nd harmonic pre-blocker; blocker cost −2.8 dB at a 26 Hz fundamental, vanishing higher); sympathetic tuning: captured lines 34.3→7.4¢ (law-exact), IR energy at f0 +3.7 dB relative (3/8 lines captured — capture is geometry-limited: period-multiples are 100–390¢ apart for short lines; adaptive basin and long-line sympathetic presets parked, f0 gain scales with capture count); 12 s sustained play at regen 0.95: bounded, line DC ≤ 0.006, NaN-clean.

## 7. Visualization contract

Per SPEC §5.6 principles. Frequency engines: log-f stage, amber harmonic ticks, cyan population markers (energy bars / needles over live spectrum), magenta mean, gather/comb-regularity/Q/f0 readouts. Time engines: log-time stage with amber beat-division ticks (echo) or note-period-multiple ticks (room), in-basin error readout, beat/period display. Warnings obey ADR-017: cause AND state.

## 8. Mod sources & cross-engine notes

Collapse, comb-regularity, and in-basin gravity error join R/σ as mod sources. The tempo-grid detune law, rhythmic tap gravity, and consonance gravity are one idea in three domains (pitch-ratio, time-ratio, grid) — the unified instrument should present them with shared vocabulary. Exciter→resonator stands validated across all four engines.

## 9. Parked (register entries)

State-domain coupling (Stuart–Landau; amplitude death as spectral gating) · filter-domain ring/cascade topology (traveling formants) · FDN topology-as-matrix (two-cluster = coupled chambers, double-slope decay; chimera tails; freeze as emergent regime) · per-band T60 as third coupled population · stereo populations (independent L/R, weakly cross-coupled) · capture-tightening / adaptive basins · discrete tap motion mode · sympathetic long-line presets.
