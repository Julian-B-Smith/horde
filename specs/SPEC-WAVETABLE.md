# The swarm's wavetable member: the spec

> **Origin.** Drafted for the horde lead session, 2026-11-07, on ROADMAP B649 and ADR-385 (the human's rulings for
> an isolated wavetable build). It turns the QUADRANT candidate (`specs/SPEC-QUADRANT.md` and its bench, ADR-384)
> into requirements the build is tested against. ADR-384's eleven amendments are accepted by the human and bind
> this spec. How it wires into the engine, and how the build runs, are in `docs/wavetable/CONTRACT.md`.

**Status.** DRAFT, for the lead's review and the human's. Nothing here is built. ADR-384 is CANDIDATE: whether
the wavetable member replaces the blades or sits beside them is the human's later ruling (section 1).

**How to read it.** The conventions are SPEC-ENGINE's. Requirements are `WT-n` and each is testable. "Must" is a
requirement, "should" a recommendation the builder may depart from with a stated reason. **PROPOSED** marks a
number the human will confirm, built as written until then. **OPEN** marks a choice that waits for the human's
ruling; the spec states what is built until then. A claim about the existing engine cites its SPEC-ENGINE id or an
ADR. Where no evidence was found, it says "unverified".

**What this spec does not change.** The swarm (SW), the voice (VO), the envelopes (EN), the mixer, the morph's
laws (Appendix D) and the modulation system (Appendix E) keep their requirements. Section 8 of the contract lists
the few SPEC-ENGINE rows a port would amend.

---

## 1. What it is, and what it is not

The wavetable member is **a source of the swarm member's waveform**. Each member of a swarm (SW-1) keeps its
phase, its frequency, its coupling, its drift and its pan. What changes is how the member turns its phase into a
sample: it reads a band-limited cycle from a table set its oscillator shares, instead of evaluating the base wave.

- **It is not an engine.** There is one engine (GUI-1). Nothing in any GUI, preset, lab or document offers
  "wavetable" and "blades" (or "swarm" and "SCALPEL") as engines to choose between. The member reads a table;
  the swarm is unchanged around it.
- **The direction is OPEN** (ADR-384, status). Two readings, and this spec serves both:
  - **Replace:** a wavetable member takes the place of the base wave and both blades. The blade rows stay in the
    manifest, inert while the table is on (ids are frozen, RC-9.1).
  - **Beside:** the blades cut a wavetable member's wave the way they cut a base wave today. That needs the blades
    to read the table's value, slope and cell mean where they now use closed forms (CONTRACT §1.7). This build
    does not build it unless the human rules it.
- **Why it exists** (ADR-384 context). After pass 2 of the demo bank (B645), the human concluded that blades reach
  a narrower sound space than "a mixture of FM/PM and sync on a wavetable". The demo bank used blades mostly for
  three things: brightness, resonant peaks, and formants. The transformations in section 6 are chosen to cover
  those three directly.
- **Position** (ADR-337 A1). horde is discovery-oriented: rapid ideation, recombination, fluid coordinated
  morphing. It is not a precision synth, but its output is held to professional fidelity. So the table set
  morphs with the pad, captures any blend as a new table, and meets measured fidelity floors.

**In scope.** The table data model; the per-oscillator table set and its two blend laws; the member's read; the
transformations (PM, sync, warps); factory tables, recipes, capture and the user library; how the table keys meet
the morph; the fidelity and CPU targets.

**Out of scope.** The GUI (the pad, corner cards, library panel); the human's random wavetable generator itself
(only its recipe slot is defined, WT-46); import of wavetable files made elsewhere (OPEN, CONTRACT §7 G3); the
filters and the FX rack.

---

## 2. Glossary

| term | meaning |
|---|---|
| **table** | 16 frames of one cycle each, stored as spectra (WT-1). |
| **frame** | One cycle of a table: 1,024 complex harmonic coefficients. Frames are numbered 0 to 15; the frame position p ∈ [0, 1] picks a point between them. |
| **corner table** | The table a morph corner holds for an oscillator (`oscN.wt.table`, WT-51). Four per oscillator: A, B, C, D. |
| **weights** | The four corner weights the table set blends at, summing to 1 (WT-10). They come from the morph (section 8). |
| **law** | How the corner spectra combine: **Linear** or **Spectral log** (WT-11, WT-12). |
| **level** | One band-limited rendering of a cycle, holding harmonics 1 to H only, for notes high enough that H is the most the band allows (WT-4). A **level set** is every level of one cycle. |
| **published set** | What the members read: the level sets the table set last built, read-only to the voices (WT-14). |
| **rebuild** | Building a new published set from the corner tables, the weights, the law and the frame, at a rebuild boundary (WT-14). |
| **read rate** | How fast a member moves through the table, in cycles per second: its frequency fᵢ times every transformation's speed-up (WT-36). The level is chosen from it. |
| **transformation** | Anything that changes how a member reads the table: PM, the loop, sync, window, asym, bend (section 6). A **warp** is a transformation that bends the read phase within one cycle (asym, bend). |
| **recipe** | What a saved patch stores for a table: how to make it again, deterministically (WT-42). |
| **capture** | A new table made by freezing a blend (WT-44). |
| **c**, **A** | The brightness ceiling c = min(20 kHz, 0.45·f_int) (ADR-384 #8) and the alias-free limit A = f_int − 20 kHz (WT-4), f_int the internal rate (host rate × quality). |

---

## 3. The data model

| id | requirement |
|---|---|
| WT-1 | **A table is 16 frames × 1,024 harmonics.** Harmonic n of a frame is a complex coefficient cₙ = (re, im) contributing re·cos(2πnφ) + im·sin(2πnφ) at phase φ ∈ [0, 1). Harmonic 0 (DC) is always 0. Stored as float32: 16 × 1,024 × 2 × 4 B = **128 KiB per table**. The spectra are canonical: every rendering, blend, capture and golden is derived from them (ADR-384 #1; QUADRANT's "spectral representation is canonical"). |
| WT-2 | **Stored frames are peak-normalised.** When a table is made (a generator, a capture, the random generator), each frame is scaled so the peak of its full 1,024-harmonic cycle, rendered at 8,192 points or finer, is 1.0. A frame whose cycle peak is below 10⁻⁶ is refused as near-silent (WT-47). This is the stored form only; what the member plays is normalised by loudness (WT-16). |
| WT-3 | **Frame interpolation.** At frame position p, u = 15·p, i = min(14, ⌊u⌋), f = u − i: each coefficient is the complex lerp cₙ = cₙ[i] + (cₙ[i + 1] − cₙ[i])·f. Under both laws this happens first, per corner, before the law (QUADRANT's blend math; unchanged). |
| WT-4 | **Levels.** A level holds harmonics 1 to H only. For a level that serves read rates from r_lo to r_hi: **H·r_hi ≤ c** (ADR-384 #8: no harmonic above min(20 kHz, 0.45·f_int)). Levels are spaced by a ratio ρ in read rate, with H halving per octave. **ρ = 2, one level per octave (ADR-384 #1), is built**, with ρ a single named constant so S3 can measure and the human can hear ρ = 2^½ and 2^¼ beside it. **OPEN: the spacing.** Engine-free arithmetic: with one level per octave and every harmonic under c, the top harmonic at the bottom of each octave falls to c/2, about 9.9 kHz at 44.1 kHz; a glide's brightness then rises and falls by an octave of harmonics across every octave. With ρ = 2^¼ the top harmonic stays between c/2^¼ and c (16.7 to 19.8 kHz at 44.1 kHz) at about 3.3 times the memory. A level's harmonics may exceed c only by ruling: everything up to A = f_int − 20 kHz aliases above 20 kHz, where QS-1 does not judge, which is the other lever (CONTRACT §7 C1). |
| WT-5 | **Pitch crossfade between levels** (ADR-384 #8). A member whose read rate lies between two levels' ranges plays both, mixed by C.0's raised cosine of log₂ of its read rate across the overlap, so a glide never steps in brightness. The overlap is the upper third of each level's range in log read rate (PROPOSED). The mix weight is computed per sample from the member's read rate, so it follows a glide and a pitch envelope sample by sample. |
| WT-6 | **Level sizes and the read.** A level of H harmonics is stored as N samples of one cycle, N ≥ 2H, with guard samples so a read never wraps an index mid-kernel. **The read and N are the builder's**, judged by WT-F1. Two reference designs, measured engine-free on a 1/n-saw table by QS-1's method at 44.1 kHz (the drafter's numpy probe, double precision, 2026-11-07; the lead's S1 gate re-derives them): linear interpolation at N = 2048 reads 54, 64, 83, 100 and 116 dB at C1, C2, C4, C6 and C8 and fails QS-1's floors below C6; 4-point cubic at N = max(2048, 32·H) reads 118, 115, 109, 138 and 140 dB (296 KiB per one-per-octave level set); **6-point Lagrange at N = max(2048, 16·H) reads the 140 dB cap at all five (176 KiB per level set)**. The 6-point design is the recommendation. A plain 2048-sample cycle with linear interpolation, the bench's read, is the must-fail plant of WT-F1. |
| WT-7 | **The 2048-sample cycle** is the exact time-domain image of one frame (1,024 harmonics need 2,048 real samples). It is the form of export, capture previews and the goldens' time-domain check. It is not required to be the form the member reads (WT-6). |
| WT-8 | **Memory is stated** (PF-7). The table set's worst-case memory per instance, at the build's ρ, N and blend strategy (WT-23), is stated by the manifest and reported at every stage. CONTRACT §1.5 gives the figure for the reference design. **PROPOSED cap: 48 MiB for both oscillators' table sets together**; a design above it goes to the human with the alternative. |

---

## 4. The table set (per oscillator, per instance)

One table set per oscillator per plug-in instance, shared by every voice and member of that oscillator. Its cost
is flat in the voice count (QUADRANT's central decision, kept).

| id | requirement |
|---|---|
| WT-10 | **Inputs.** Four corner tables (slots A to D) and four weights w = [w_A, w_B, w_C, w_D], each in [0, 1], summing to 1; the law (`wt.law`); the frame (section 5). Under morph outcome (A) the weights are one-hot (WT-52); under (B) they are the morph's (WT-53). Whatever the outcome, the table set is built for four tables at any weights, since that blend *is* the oscillator (ADR-384 #5). |
| WT-11 | **Linear.** cₙ = Σ_c w_c·cₙ[c], on the frame-interpolated coefficients (WT-3). It equals a bilinear crossfade of the four corners' time-domain cycles, so harmonics in opposed phase cancel and the centre can thin (QUADRANT; WT-16 restores its level). |
| WT-12 | **Spectral log.** \|cₙ\| = max(0, exp(Σ_c w_c·ln(\|cₙ[c]\| + ε)) − ε), ε = 10⁻⁴. Its phase is arg(Σ_c w_c·cₙ[c]); where that sum's magnitude is below 10⁻⁶, the phase of the corner with the largest weight, **the lowest corner (A, B, C, D order) on a tie** (a fix: the bench's `indexOf(max)` took the first, which is the same rule but unstated). A corner with weight 0 contributes nothing to either sum (a fix: ln(ε) × 0 is 0, but the bench's form computed it; the rule is now explicit so a NaN cannot enter from a zero weight). The log-magnitude ln(\|cₙ\| + ε) may be cached per table frame (QUADRANT); a cache is derived data, never state. Whether a fixed or per-table phase sounds steadier is a listening question at S2 (QUADRANT's open question, kept OPEN). |
| WT-13 | **Order.** Per corner: frame interpolation (WT-3). Then the law across corners. Then levels (WT-4) and loudness normalisation (WT-16). Then publication (WT-14). |
| WT-14 | **Rebuild only on change, at control rate** (ADR-384 #7). A rebuild boundary falls every **R = 4 control intervals** (PA-3: 116 samples at 44.1 kHz, 128 at 48 kHz; ADR-384 #7's "128 samples", stated in time as SW-14 requires, PROPOSED). At a boundary the table set rebuilds only if a corner table, a weight, the law, or (under Spectral log) the instance frame changed since the last one. A static patch rebuilds **zero** times (QUADRANT's static-patch test), counted (IF-9). Boundaries are counted on the engine's own sample clock, never the host's blocks (PA-3), so the result is independent of block size (DT-2). The rebuild runs on the audio thread, deterministically, with bounded work (RT-2) and no allocation (RT-1). |
| WT-15 | **The crossfade between published sets lasts R** (ADR-384 #7), by C.0's equal-gain raised cosine. Every member reads both the outgoing and the incoming set while it runs. A set that arrives during a crossfade replaces the pending one and starts when the running crossfade ends; at most two sets are ever heard, three exist (heard, heard, building). Gain changes from WT-16 ride the same crossfade, so they cannot step. |
| WT-16 | **Loudness normalisation, with a peak limit** (ADR-384 #10). No user toggle (the bench's Normalise row is dropped, reduce-never-invent). (1) **Loudness:** the blended spectrum is scaled so its RMS, by Parseval √(½·Σₙ\|cₙ\|²) over the harmonics the level holds, equals the RMS of a full-scale sine × 0.85 (the bench's 0.85 headroom, kept; PROPOSED). (2) **Peak limit:** if the scaled top level's cycle peak exceeds 1.0, the gain is reduced so it equals 1.0, and the engagement is counted (IF-9). (3) **Gain cap:** the loudness gain never exceeds +24 dB (PROPOSED), so a blend that cancels (Linear between a saw and a reverse saw, at the midpoint, is silence) fades as it cancels instead of amplifying rounding noise; each engagement is counted. Under a strategy that never renders the blend whole (Linear blended at read time, WT-23), the gain comes from the corners' Gram matrix (Σ_c Σ_d w_c·w_d·⟨c, d⟩, exact for Linear) and the peak limit from the bound Σ_c w_c·peak_c, which can only over-limit, never under. Level steadiness is judged by WT-F7. |
| WT-17 | **Determinism.** Same state, same events → the same output sample for sample on one build (DT-1), at any block split (DT-2). The table set reads no clock (DT-5) and draws no random numbers. After `reset()` it is as a fresh prepare with the same state (DT-6): corner tables kept, published set rebuilt at the first boundary, no crossfade in flight. |

---

## 5. The member

| id | requirement |
|---|---|
| WT-20 | **Phase from the swarm, unchanged.** Member i's phase θᵢ, its frequency fᵢ, its start phase (SW-10), its onset wait (SW-12), its drift, coupling and inertia are the swarm's (SW-1 to SW-17). The table is read at **φ = θᵢ**: table index 0 is the member's cycle start, the instant the saw base resets (in the clean-room engine the base wave is evaluated at θᵢ + ½ with its own origin, so its reset falls at θᵢ = 0; CONTRACT §1.1). |
| WT-21 | **One read per sample per cycle it plays, from the published set.** With no crossfade running and the read rate inside one level's range, a member makes one interpolated read per internal sample of each cycle it plays: one under Spectral log; under Linear, two (the adjacent frames) with a pre-blend, eight (four corners × two frames) with a read-time blend (WT-23). WT-15's crossfade (both sets) and WT-5's pitch crossfade (both levels) each double that while they run. Reads are counted (IF-9). |
| WT-22 | **The frame.** **Under Linear the frame is per voice** (ADR-384 #6): each voice reads at its own resolved `wt.frame` plus its per-voice offsets (PA-2), so a note can scan its table. **Frame spread:** member i reads at pᵢ = clamp(p + `wt.frameSpread`·sᵢ, 0, 1), sᵢ its position from the oscillator's spread law (C.10, BL-13: the same law, draws and motion grain as the blades' spreads; a single member has sᵢ = 0). **Under Spectral log the frame is global** (ADR-384 #6): the table set rebuilds at one frame, the frame of **the newest held voice** (MS-46's rule: with none held, the last such value), including that voice's offsets; frame spread is inert. |
| WT-23 | **The cost consequence of per-voice frame** (stated, measured at S4). A Spectral-log set holds one blended cycle (one level set); a member reads it once. A Linear set must serve every frame a member may read. Two strategies meet WT-11, and the builder picks by measurement against WT-BUDGET: **(a) read-time blend:** pre-built level sets for every corner frame (4 × 16 per oscillator, built when a corner table arrives, off the audio thread), each member reading 4 corners × 2 frames and summing with w and the frame fraction: no rebuild ever, 8 reads per member-sample; **(b) pre-blend:** at each rebuild, 16 blended frames B_f = Σ_c w_c·T_{c,f} over the levels in use, then 2 reads per member-sample; the rebuild costs 16 × (levels in use × N) × 4 multiply-adds while the weights move. (a) costs reads and memory; (b) costs rebuilds and memory. Neither may change the sound beyond WT-F10's tolerance. |
| WT-24 | **What the member hands on.** The member's output is its read (and transformations), times SW-13's band-edge fade, before its PA-6 fade, entry gain, density scale and pan, exactly where the clean-room engine's uncorrected output y⁰ sits today (CONTRACT §1.1). A table read is band-limited by construction, so it bypasses ED-1's filter E and the pending corrections, **except while sync is on** (`wt.syncIn` not off, WT-32): its restart is an edge, corrected by ED-1 and ED-6 exactly as a base wave's edges are, and the member then passes through E as a base-wave member does. Because `wt.syncIn` is stepped, E engages and releases only inside PA-5's 5 ms crossfade, so E's 2.4-sample delay (ED-2) never shifts a sounding member without one. |
| WT-25 | **DC.** Tables have no DC (WT-1). A transformation that makes a cycle mean (sync at a ratio that is not whole, the window, asym and bend, which spend unequal time in the two half-cycles) has it removed so that QS-6's bound holds (WT-F8). How is the builder's: the exact cycle mean from the table's integral is recommended (the antiderivative of a spectrum is exact: harmonic n divided by 2πn·i), C.16's 8 Hz blocker the fallback. |
| WT-26 | **Real-time safety.** RT-1 to RT-4 hold. A table change reaches the audio thread through a handoff that never waits or locks (CONTRACT §1.6); everything is allocated at prepare for the maxima (64 voices, 9 members, ADR-381); no denormal is produced (RT-4). |

---

## 6. Transformations

**The read chain**, per member per sample, in this order (each stage off at its neutral value):

θᵢ → **sync** (ratio, WT-32) → **the warp** (asym and bend, WT-34) → **phase push** (PM, WT-30; the loop, WT-31) →
the read → **window** (WT-33) → SW-13's fade → the member's output.

The order is chosen so that the phase maps nest the way a musician hears them: sync sets how many table cycles fit
the member's cycle, the warp reshapes time inside each of those, and PM pushes the result.

| id | requirement |
|---|---|
| WT-30 | **PM from osc 2** (osc 1 only in 1.0). `osc1.wt.pm` at index I pushes osc 1 member i's read phase by (I/2π)·m, BL-5's phase-FM form, where m is **osc 2's member (i mod U₂) output, times osc 2's governing envelope** (RC-6.5), before osc 2's level, pan and gate. So osc 2 can be heard or not (its level) independently of how hard it modulates, and its envelope shapes the index as an FM operator's does. osc 2 must be on (MO-26, RC-6.3) and renders first. **osc 1 → osc 2** needs per-sample interleaving of the two swarms, which the clean-room voice does not do (it renders each oscillator over a whole segment, CONTRACT §1.8); its row `osc2.wt.pm` is reserved (5.1) until that is built (OPEN, CONTRACT §7 E2). |
| WT-31 | **The loop: cross-mod and feedback, reused.** While a member is a table member, the existing rows `crossMod`, `feedback` and `loopDrive` act on the table read by C.14's law unchanged: xinᵢ (C.14) is added to the read phase. Feedback gives self-PM (an operator's feedback); cross-mod rings the swarm's members through one another, a swarm-native FM network. **No new row** is added for self-PM. BL-12's stability map was measured on blade carriers and does not transfer: S5 maps the table member's limits engine-free for the factory set, and they are reported, not judged. |
| WT-32 | **Hard sync.** `wt.syncIn` is off, × pitch or Hz. While it is not off, the read phase is frac(r·θᵢ), restarting at 0 at each wrap of θᵢ, with r = `wt.sync` (× pitch) or max(1, `wt.syncHz`/fᵢ) (Hz: a fixed formant whatever the note). The restart is a step of the read's value and a corner of its slope at a known instant; both are corrected by ED-1's step table and ED-6's corner table on the member's pending ring, scaled by `bandLimit` as a base wave's edges are. The table's value and slope at the restart instant are exact from the spectra (the slope is harmonic n times 2πn·i). Switching `wt.syncIn` crossfades by PA-5. |
| WT-33 | **Window** (sync's formant form). The read is multiplied by g(θᵢ) = (1 − a) + a·½(1 − cos 2πθᵢ), a = `wt.window`. At a = 0 it is plain hard sync; at a = 1 the restart falls where g is 0 with zero slope, so no step remains, and the sound is a train of windowed table bursts: a fixed resonant peak under `wt.syncIn` Hz, the blades' most used formant sound in the demo bank (ADR-384 context). Between, the step WT-32 corrects is (1 − a) of the full one. Live only while sync is on. |
| WT-34 | **Asym and bend: one smooth warp, two axes.** φ' = φ + (0.9/2π)·[b·sin(2πφ) + a·(1 − cos 2πφ)], with b = `wt.bend` and a = `wt.asym`, each in [−1, 1], and the pair scaled down together wherever √(a² + b²) > 1. Its slope 1 + 0.9·(b·cos 2πφ + a·sin 2πφ) stays in [0.1, 1.9], so the map is smooth, periodic and monotonic: no corners, no edges, nothing to correct, and E is not needed. At (0, 0) it is off. **Bend** compresses the cycle around its start and stretches its middle (brighter attack of each cycle, or darker with b < 0). **Asym** moves the half-cycle point, to 0.307 or 0.693 of the cycle at +1 or −1: one half of the table plays faster than the other, a pulse-width-like brightness on any table. They are the two quadrature components of one sinusoidal phase warp, so together they reach any angle of it. Narrower widths than about 0.3 come from the factory Pulse width sweep table (to 0.06, WT-40), not from a cornered map whose corners would alias. |
| WT-35 | **Why these warps and not others.** The demo bank used blades for brightness, resonant peaks and formants (ADR-384 context). Bend and asym cover brightness smoothly and morph without aliasing; sync with its window covers resonant peaks and formants (WT-33); PM covers the FM growls. **Mirror** (play the table forward then back) is left out: it makes corners at each turn, and what it does to the harmonics (even/odd balance) the table set already reaches by blending. It can return by ruling. |
| WT-36 | **The level follows the read rate.** The level is chosen (WT-4) for the member's **peak read rate**: fᵢ × r (sync) × the warp's peak slope 1 + 0.9·√(a² + b²), evaluated at control rate with the member's ramped values. So sync and the warp never push the table's harmonics past c; they cost brightness, not aliasing. PM and the loop push the phase by an amount the level cannot predict; their sidebands are the one source of aliasing that remains at Standard (WT-37). |
| WT-37 | **Aliasing, transformation by transformation,** is as the table below states. Each row's Standard-quality story is a requirement; the High column says when the 2× quality setting (`quality`, SPEC-ENGINE 5.2) is needed. |

| transformation | what aliases | at Standard (1×) | when High (2×) is needed |
|---|---|---|---|
| none, a table alone | interpolation only (WT-6) | meets WT-F1 by construction | never |
| sync, window | the restart, if uncorrected | step and corner corrected (ED-1, ED-6); the level is chosen for r·fᵢ | never for the restart; reported per QS-3 |
| asym, bend | nothing new | a smooth map; the level chosen for its peak slope | never |
| PM (osc 2 → osc 1) | sidebands beyond the level's band, growing with I and the modulator's frequency | **aliases by design** above roughly I·f_mod + c, as blade FM does (QS-3) | recommended for bright, high-index PM; reported per QS-3 |
| the loop (cross-mod, feedback) | the loop's own spectrum, unbounded above its stability limit | aliases by design, as the blades' loop does (QS-3, BL-12) | recommended; reported per QS-3 |

QS-3's method (renders against 4× and 8× renders) reports them; no floor is judged until the human ratifies one
(QS-3's rule, kept). At High the member runs at the internal rate like every member (`quality`, D class; IF-9):
there is no opt-out, because one behaviour per thing (BD-3) and an unwarped table already meets WT-F1 at both.

| id | requirement |
|---|---|
| WT-38 | **Smooth motion.** Every transformation amount is a continuous parameter, glided by PA-1 and ramped per control interval (PA-3); the rate-A rows (`wt.sync`, `wt.syncHz`, `wt.pm`) also take per-sample offsets (PA-4). `wt.on`, `wt.law` and `wt.syncIn` are stepped and change by PA-5's 5 ms crossfade. Sync's ratio is continuous: a sweep never steps the phase (the restart is where the change shows, corrected by WT-32). |
| WT-39 | **Cost when off** (PF-6). Each transformation at its neutral value costs nothing measurable (under 1 % of the patch each), and with all neutral the member is one read per sample (WT-21). |

---

## 7. Tables: factory, recipes, capture, library

| id | requirement |
|---|---|
| WT-40 | **The factory set is generated from formulas, and only from formulas.** No table is taken, sampled, traced or derived from another product's tables, presets or factory content, and none is imported. The 1.0 set is the bench's eight, regenerated at 1,024 harmonics: Sine → saw, Pulse width sweep, Vowel ah → ee, Hard sync 1 → 6, FM index 0 → 8, Triangle fold, Moving comb, Drawbar registrations (QUADRANT, data model). Each is a generator with an id and a version (WT-42). More come only by ruling. |
| WT-41 | **Generator accuracy.** A spectral generator writes its coefficients directly. A time-domain generator (sync, FM, fold) is analysed from its formula **without aliasing**: by its analytic Fourier series where one exists, else by an FFT of the formula sampled at ≥ 64 × 2,048 points; either way each frame's coefficients agree with a 4× finer analysis within 10⁻⁶ of the frame's peak (−120 dB). The bench's 512-point DFT of a discontinuous wave aliased and is not ported. |
| WT-42 | **Recipes.** A table referenced by a saved patch is stored as a recipe, never as its data in the host's state chunk (ADR-384 #2; large chunks fail to save silently). A recipe is one of: **generator** (generator id, generator version, settings, seed); **capture** (its source tables' recipes, x, y, frame or "all", law, and the recipe format version). Rebuilding a recipe gives the same table on one build (DT-1). The format is versioned (CONTRACT §3). |
| WT-43 | **Goldens, frozen per version.** Each generator version, at its default settings and at a fixed set of settings and seeds, has a golden: its spectra's hash on the reference build, and its coefficients within 10⁻⁶ of peak on any build. A generator whose output changes takes a new version; old versions stay buildable, so old patches keep their sound. A golden never moves to make a build pass (CONTRACT §6). |
| WT-44 | **Capture.** A capture freezes the current blend of an oscillator into a new table. **All 16 frames** (the default): frame f is the blend of every corner at frame position f/15 under the current weights and law. **This frame only:** one blend at the current frame, repeated in all 16. Captures always use all 1,024 harmonics, run off the audio thread, are peak-normalised per frame (WT-2) and pass WT-47. A capture's sources may be captures (recursion), to a depth of 8 (PROPOSED); deeper is refused, named. |
| WT-45 | **The user library** is device-wide storage (ADR-384, FX device note) in the user's application-support folder, outside every patch. A table's id is a **hash of its recipe**, never a random draw (the bench's `Math.random()` library ids are not ported). A table is a file holding its recipe and its rendered spectra. Deleting a table a corner uses swaps that corner to its default factory table on the page; patches and exported presets that reference it keep working from their recipes. Delete is two-step on the page. |
| WT-46 | **The random generator's slot.** The human's random wavetable generator (ADR-384, related, "already in the prototype stage") plugs in as a generator: id `random`, a version, its settings and a 32-bit seed, every draw from a mulberry32 stream keyed by purpose (DT-4, MO-6's FNV-1a scheme, purposes beginning `wt/random/`). Nothing else about it is specified here. |
| WT-47 | **Every table passes the table gates** before it is stored or played (ADR-384, related): no DC (harmonic 0 is 0); its level in the audible band (energy of harmonics under 20 kHz at C3 within 12 dB of its full energy, PROPOSED); no near-silent frame (WT-2); frames that differ (the 16 frames are not all identical within 10⁻⁶ unless the generator says it is a single-frame table); and WT-F1 on a static read. A table that fails is refused and named, never silently fixed. |

---

## 8. The morph

One pad (ADR-384 #5). The table set's X and Y are the morph position, `morph.x` and `morph.y` (D.2, MO-4); the
corners are the morph's corners; there is no second pad.

| id | requirement |
|---|---|
| WT-50 | **The weights are the morph's.** The table set's four weights are MO-4's bilinear weights at the resolved position, as the outcome below sends them. Under `morph.on` off the audible corner (MO-20) is one-hot. X and Y move by the host, the pad, and the global LFOs and the 2-D random only (MS-29). |
| WT-51 | **The corner table key.** `oscN.wt.table` is a corner value of the morph, held in the morph system's corner state as `oscN.ratios` is (MO-13, RC-11.1): a recipe reference (WT-42), not a host parameter, at most 16 KiB of recipe text per corner (CONTRACT §3). It is one of oscillator N's keys for MO-26 (its key begins `oscN.`), so it blends or picks only among the corners where the oscillator is on. |
| WT-52 | **Outcome (A): tables are Structural.** OPEN: the human's ruling (ADR-377, ADR-384 #5). The table key follows its quantum boundaries under every law (MO-13): its pick and latch (MO-7, MO-8), one table at a time. The resolver sends one-hot weights at the pick. A flip changes on sounding voices by PA-5's 5 ms crossfade, both tables read from one member state. Under (A) the pad does not blend tables; the Morphable table rows (frame, warps, sync, PM) blend as every Morphable key does (MO-9). |
| WT-53 | **Outcome (B): a spectral-blend key class.** OPEN: the human's ruling. ADR-377 put "parametric morphing of structure (wave shapes interpolating, and the like)" after 1.0; ADR-384 #5 says a 1.0 QUADRANT needs that line revisited. Under (B) `oscN.wt.table` is a new key class whose weights follow `morph.law`: **Blend and Stepped**, MO-4's unglided weights (at the stepped position under Stepped), corners holding the same recipe pooled into one weight, at count Two by MO-9's two-candidate rule, glided by MO-5 (b) applied to the weights, restricted to the oscillator's on corners (MO-26 (2)); **Quantum**, one-hot at the key's pick and latch (MO-7, MO-8), a flip by PA-5's 5 ms crossfade; **Exempt**, corner A (MO-12). Waypoints and region locks do not act on it in 1.0 (PROPOSED). It renders one blended cycle per member, never two sounds mixed, so ADR-377's "no two settings of one group sound at once" holds in its letter; the ruling is about its spirit. |
| WT-54 | **The build implements the general case either way.** The table set (section 4) blends four tables at any weights under both laws. Only which outcome 1.0 exposes is open. The resolver's side (one-hot under (A), blended weights under (B)) is a small branch in the resolver (CONTRACT §4). |
| WT-55 | **The other rows follow their class** (CONTRACT §2): Morphable rows blend (MO-9) and flip under Quantum (MO-7); Structural rows (`wt.on`, `wt.law`, `wt.syncIn`) follow their quantum boundaries (MO-13), flipping by PA-5. |
| WT-56 | **Save current sound** (MO-25). Under (A) the table key's resolved value is its pick, saved as it stands. Under (B), at a position where the corners' tables differ, Save writes into the corner **a capture recipe** of the heard blend (all 16 frames, the current law, x and y), so the corner alone sounds as the position did. The capture lands in the user library too (WT-45). |
| WT-57 | **Modulation.** Every continuous `wt.` row is a destination (MS-12 (a)) of scope `per-voice`, except as stated: `wt.frame` acts per voice under Linear and, under Spectral log, as WT-22's newest-held-voice frame; X and Y are the morph position, scope `shared`, reachable only from a global LFO or the 2-D random (MS-29). So ADR-384 #9 (per-note sources to X and Y by MS-46 and MS-47) does not arise while MS-29 stands; see CONTRACT §7 B3. |

---

## 9. Fidelity targets (measured)

Each target names its method, a **must-fail plant** (a deliberately broken build or input the gate must catch)
and a **must-read-zero control** (a case the gate must pass, or a measurement that must read nothing). A gate
whose plant does not fire, or whose control does not read zero, is itself red. Numbers marked RATIFIED are
SPEC-ENGINE's; PROPOSED ones wait for the human.

| id | target | number | status | method | plant (must fail) | control (must read zero) |
|---|---|---|---|---|---|---|
| WT-F1 | **Aliasing floor, a table alone** | alias power under harmonic power by ≥ 110, 104, 98, 93 dB at C2, C4, C6, C8; C1 reported | QS-1's numbers, RATIFIED for edges; PROPOSED for the table member | QS-1's method exactly: one member, Standard, 44.1 kHz, an odd whole number of cycles in 65,536 samples, rectangular window, double precision, every non-harmonic bin 20 Hz to 20 kHz, capped at 140 dB; the factory Sine → saw table at frame 1 (a saw), and every factory table at frames 0, 7, 15 (reported) | the bench's read: 2,048 samples, linear (engine-free: 64 dB at C2) | direct additive synthesis of the same harmonics through the meter reads the 140 dB cap (engine-free, uncapped: ≥ 240 dB); and QS-1's naive saw reads 9 to 27 dB, as QS-1 states (engine-free: 16 to 28 dB at C2, C4, C6) |
| WT-F2 | **Top of the band** | the 16 to 20 kHz energy of a single-member saw table within 3 dB of an ideal band-limited saw at C6 (QS-2's numbers); PROPOSED to be reported at every semitone C2 to C8 across a glide | QS-2 RATIFIED for edges; PROPOSED here | QS-2's method; plus a glide C2 → C8 over 10 s, the 16–20 kHz energy per 50 ms window | a level set with no pitch crossfade (the brightness steps at each level) | a static read at a pitch exactly at a level's top (no crossfade, full band) |
| WT-F3 | **Transformations' aliasing** | reported, not judged | QS-3's rule | QS-3's method on every transformation's sweep, at Standard and High | — | a table alone reads clean at both |
| WT-F4 | **Clicks** | no event | QS-4 RATIFIED | the ratified detector, with QS-4's probe rule: judged on a sine-table probe (all four corners the factory Sine → saw at frame 0); a case whose correct sound changes the waveform within a cycle is judged by its law test (WT-F10) | a 0.5 dB gain step planted at a rebuild boundary with the crossfade disabled, on the sine probe | X and Y swept corner to corner in 10 ms and the law switched, all corners sine: no event |
| WT-F5 | **Determinism** | bit-identical | DT-1, DT-2 RATIFIED | two offline renders of the same state and events; the same split into blocks of 1, 7, 128, 4,096 and uneven | an unseeded draw (or a wall-clock read) planted in the rebuild | two renders of one build: zero difference |
| WT-F6 | **Static patch** | zero rebuilds over 10 s | QUADRANT, PROPOSED | the rebuild counter (IF-9) with nothing moving | a rebuild forced at every boundary | the counter reads 0 |
| WT-F7 | **Level steady across the pad** | BS.1770 integrated loudness at a 5 × 5 grid of positions within ±1.5 dB of its median, for every factory corner set the S2 batch uses, under both laws; positions where the peak limit or the gain cap engages are reported, not judged | PROPOSED (ADR-384 #10 sets the rule, not the number) | OU-2's loudness measure, 3 s held C3, one member | normalisation off, corners a sine (Sine → saw, frame 0) and a narrow pulse (Pulse width sweep, frame 15): peak-normalised, their loudness differs by several dB | four identical corners: 0.0 dB spread |
| WT-F8 | **DC** | the output's mean over each second within 0.003·10^(L/20) | QS-6, BL-19 RATIFIED | QS-6's method, with every transformation swept | WT-25's removal off under asym 0.8 on a saw table | a sine table, no transformation: mean 0 within the bound |
| WT-F9 | **Silence and denormals** | exact zero after the last voice; no subnormal output | QS-7, RT-4 RATIFIED | QS-7's method | a subnormal planted in a member's state with flush-to-zero off | the tail of a released note reads exact zero |
| WT-F10 | **The blend's law** | corner identity: at each corner both laws reproduce that corner's frame within 10⁻³ relative magnitude per harmonic; Linear equals the bilinear crossfade of the four time-domain cycles within 10⁻⁵ of peak; a sweep's output equals the per-sample ideal of WT-14 and WT-15 within 10⁻⁵ | QUADRANT's tests, PROPOSED | engine-free reference blends against the table set's published set and its output | corners swapped (A ↔ B weights) | four identical corners: the blend equals the corner exactly |
| WT-F11 | **Capture** | a 16-frame capture loaded into all four corners at X = Y = 0.5 under Linear matches the captured blend within 10⁻⁴ at every frame; a this-frame capture has 16 identical frames; a capture of captures loads, plays and saves | QUADRANT, PROPOSED | round trip through the library and through the state | the capture made at the wrong frame position | capture of four identical corners equals the corner |
| WT-F12 | **Bounded and finite** | every output finite; peaks under QS-8's bound for the class | QS-8, RT-3 RATIFIED | fuzzed parameters, extreme PM and loop | a NaN planted in a published set (it must be contained and counted, RT-3) | the bank-like S7 patches: no non-finite count |
| WT-F13 | **Real-time safety** | no allocation, lock or system call on the audio path | RT-1 RATIFIED | RealtimeSanitizer (Homebrew LLVM) over a render that loads, changes and captures tables | a `malloc` planted in the rebuild | the same render on the clean build: no report |

---

## 10. Performance

| id | requirement |
|---|---|
| WT-P1 | **The budget.** `WT-BUDGET: <from B648, the lead proposes; the human approves before stage 1>`. It is a share of one Apple M3 performance core for the wavetable member's whole cost on one oscillator: the table set's rebuilds plus every member's reads and transformations, as PF-1 states its target for the swarm oscillator (10.7 %, ADR-354). |
| WT-P2 | **The workload is PF-3's chord**: 8 voices, the k-th on key 48 + 3·(k mod 8), velocity 0.85, all held before the first block, 44.1 kHz (48 kHz secondary), 128-sample blocks, 0.25 s untimed then 2 s timed, best of 5, PF-3's load guard and calibration loop. **Cells:** (1) a static patch, Linear, U = 7; (2) X and Y under a 5 Hz global LFO, Spectral log (QUADRANT's CPU test, on PF-3's chord per ADR-384 #11); (3) per-voice frame scanned by an envelope, Linear, frame spread 0.5, U = 7; (4) sync, asym and PM on, Standard; (5) as (4) at High, reported; (6) U = 9, cell (3), reported. |
| WT-P3 | **Two measures, both every stage** (ADR-385; ADR-380 A1). The **instruction count** (the Mac's retired instructions for a fixed offline render of the stage's cells), deterministic, at once, beside the test count. **Timed runs** by PF-3's method only in the 2 to 6 am quiet window (ADR-380 A1). A stage whose instruction count exceeds its share of WT-BUDGET (CONTRACT §5.4) does not close; a timed run over budget reopens it. |
| WT-P4 | **Cost when off.** With `wt.on` off the existing engine renders bit-identically to the engine without this code, and the table set costs nothing (PF-6, RC-6.4's method). |
| WT-P5 | **Scaling.** The table set's cost is flat in voices and members; the reads grow linearly in sounding member-samples (PF-5). The reference figure for comparison: a blade member costs about 194 ns per member-sample in the clean-room engine at 866bfd3 (109.70 % of a core for 8 voices of 16 members, REPORT.md §7, divided by 8 × 16 × 44,100). |
| WT-P6 | **Memory** is reported per PF-7 and held to WT-8's cap. |

---

## 11. Open items

The questions, with recommendations, are in CONTRACT §7. Those that change what this spec builds:

- **Direction:** replace or beside (section 1). Built: replace's seam; beside's interface is specified, not built.
- **Morph outcome:** (A) or (B) (WT-52, WT-53). Built: the general four-table core, the resolver's branch for both.
- **Level spacing** (WT-4): built at one per octave with ρ a constant; S3 measures and the human hears the others.
- **Rebuild interval** (WT-14): built at 4 control intervals.
- **osc 1 → osc 2 PM** (WT-30): reserved.
- **Every PROPOSED number** in section 9.
