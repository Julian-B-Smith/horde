# The swarm's wavetable member: the spec

> **Origin.** Drafted for the horde lead session, 2026-11-07, on ROADMAP B649 and ADR-385 (the human's rulings for
> an isolated wavetable build). It turns the QUADRANT candidate (`specs/SPEC-QUADRANT.md` and its bench, ADR-384)
> into requirements the build is tested against. ADR-384's eleven amendments are accepted by the human and bind
> this spec. Reworked the same day on an independent critic's review, the human's rulings by poll (ADR-385) and
> the cost bench's findings (B648). How it wires into the engine, and how the build runs, are in
> `docs/wavetable/CONTRACT.md`; its §7 is the one list of decisions still open.

**Status.** DRAFT, for the lead's review and the human's. Nothing here is built. ADR-384 is CANDIDATE: whether
the wavetable member replaces the blades or sits beside them is the human's later ruling (section 1).

**Ruled for this spec (ADR-385, by poll, 2026-11-07).**
- **Blending different tables across the morph pad is in 1.0** ("Yes, in 1.0"). This amends ADR-377's post-1.0
  bullet for wavetables (cited as **ADR-377 A3 / ADR-385**). Section 8 states the one design.
- **MS-29 stands:** only the global LFOs and the 2-D random move the morph position. ADR-384 #9 is moot.
- **WT-BUDGET = 4 % of one core per oscillator, PROVISIONAL** (WT-P1).

**How to read it.** The conventions are SPEC-ENGINE's. Requirements are `WT-n` and each is testable. "Must" is a
requirement, "should" a recommendation the builder may depart from with a stated reason. **PROPOSED** marks a
number the human will confirm, built as written until then. **OPEN** marks a choice that waits for the human's
ruling; the spec states what is built until then, and the choice is listed in CONTRACT §7. A claim about the
existing engine cites its SPEC-ENGINE id, an ADR, or a file in the clean-room engine at commit `e72a092`. Numbers
marked **engine-free** come from the drafter's numpy probes (double precision, 2026-11-07); the lead's gates
re-derive each before it is used.

**What this spec does not change.** The swarm's laws (SW), the voice (VO), the envelopes (EN), the mixer and the
modulation system (Appendix E) keep their requirements. CONTRACT §8 lists the SPEC-ENGINE rows a port amends,
among them C.14, C.16, PA-4, PA-5, PA-8, IF-12 and the morph's key classes.

---

## 1. What it is, and what it is not

The wavetable member is **a source of the swarm member's waveform**. Each member of a swarm (SW-1) keeps its
phase, its frequency, its coupling, its drift and its pan. What changes is how the member turns its phase into a
sample: it reads a band-limited cycle from a table set its oscillator shares, instead of evaluating the base wave.

- **It is not an engine.** There is one engine (GUI-1). Nothing in any GUI, preset, lab or document offers
  "wavetable" and "blades" (or "swarm" and "SCALPEL") as engines to choose between. The member reads a table;
  the swarm is unchanged around it. Whether a patch's members read the base wave or the table is **patch state
  the GUI never shows as a choice** (WT-19).
- **The direction is OPEN** (ADR-384, status; CONTRACT §7 A1). This spec serves both readings:
  - **Replace:** a table member takes the place of the base wave and both blades. The blade rows stay in the
    manifest, inert for table members (ids are frozen, RC-9.1).
  - **Beside:** the blades cut a table member's wave as they cut a base wave today. That needs the blades to read
    the table's value, slope and span mean where they now use closed forms (CONTRACT §1.7). This build does not
    build it unless the human rules it.
- **Why it exists** (ADR-384 context). After pass 2 of the demo bank (B645), the human concluded that blades reach
  a narrower sound space than "a mixture of FM/PM and sync on a wavetable". The demo bank used blades mostly for
  brightness, resonant peaks and formants; section 6's transformations cover those three directly.
- **Position** (ADR-337 A1). horde is discovery-oriented: rapid ideation, recombination, fluid coordinated
  morphing. It is not a precision synth, but its output is held to professional fidelity.

**In scope.** The table data model; the per-oscillator table set and its two blend laws; the member's read; the
transformations (PM, sync, warps); factory tables, recipes, capture and the user library; the tables and the
morph; the fidelity and CPU targets.

**Out of scope.** The GUI; the human's random wavetable generator itself (only its recipe slot, WT-46); import of
wavetable files made elsewhere; the filters and the FX rack.

---

## 2. Glossary

| term | meaning |
|---|---|
| **table** | 16 frames of one cycle each, stored as spectra (WT-1). |
| **frame** | One cycle of a table: 1,024 complex harmonic coefficients. Frames 0 to 15; the frame position p ∈ [0, 1] picks a point between them. |
| **corner table** | The table a morph corner holds for an oscillator (`oscN.wt.table`, WT-51). Four per oscillator. |
| **weights** | The four corner weights the table set blends at, summing to 1 (WT-10), from the morph (section 8). |
| **law** | How the corner spectra combine: **Linear** or **Spectral log** (WT-11, WT-12). |
| **level** | One band-limited rendering of a cycle holding harmonics 1 to H only (WT-4). A **level set** is every level of one cycle. |
| **published set** | What the members read: the level sets the table set last built, read-only to the voices (WT-14). |
| **rebuild** | Building a new published set at a rebuild boundary (WT-14). |
| **read rate** | How fast a member moves through the table, in cycles per second: fᵢ times every transformation's speed-up (WT-36). |
| **transformation** | Anything that changes how a member reads the table: PM, the loop, sync, window, asym, bend (section 6). |
| **recipe** | What a saved patch stores for a table: how to make it again (WT-42). |
| **capture** | A new table made by freezing a blend (WT-44). |
| **c** | The brightness ceiling c = min(20 kHz, 0.45·f_int) (ADR-384 #8), f_int the internal rate (host rate × quality). |
| **detector plant**, **mutation plant** | A gate's two kinds of must-fail case: an input the meter must catch, proven engine-free at S0; and a build with one engine mechanism disabled, run at every stage close (section 9). |

---

## 3. The data model

| id | requirement |
|---|---|
| WT-1 | **A table is 16 frames × 1,024 harmonics.** Harmonic n of a frame is a complex coefficient cₙ = (re, im) contributing re·cos(2πnφ) + im·sin(2πnφ) at phase φ ∈ [0, 1). Harmonic 0 (DC) is always 0. Stored as float32: **128 KiB per table**. The spectra are canonical: every rendering, blend, capture and golden is derived from them (ADR-384 #1). |
| WT-2 | **Stored frames are peak-normalised.** When a table is made (a generator, a capture, the random generator), each frame is scaled so the peak of its full 1,024-harmonic cycle, rendered at 8,192 points or finer, is 1.0. A frame whose peak is below 10⁻⁶ is refused as near-silent (WT-47). This is the stored form; what the member plays is normalised by loudness (WT-16). |
| WT-3 | **Frame interpolation**, first, per corner, before the law. At p: u = 15·p, i = min(14, ⌊u⌋), f = u − i. **Linear:** the complex lerp cₙ = cₙ[i] + (cₙ[i + 1] − cₙ[i])·f. **Spectral log, OPEN** (CONTRACT §7 C6): lerping the cached log-magnitude is a geometric lerp, not \|lerp c\|, and misses QUADRANT's corner-identity tolerance by 4.7 × 10³ (H = 64) to 1.6 × 10⁴ (H = 1,024) at fractional frames (B648). **Built: the log domain,** ln\|cₙ\| = (1 − f)·ln(\|cₙ[i]\| + ε) + f·ln(\|cₙ[i + 1]\| + ε), the phase from the complex lerp. It is the same law on both axes, uses the cache, and moves a spectral peak between frames rather than crossfading two (a weighted geometric mean of two Gaussian peaks is a Gaussian at the weighted mean position). The alternative, exact, costs a log and a hypot per harmonic per corner per rebuild. So the two laws hear different in-between frames of one table, by design; WT-F10 tests each law against its own rule. |
| WT-4 | **Levels.** A level holds harmonics 1 to H. For a level serving read rates up to r_hi: **H·r_hi ≤ c** (ADR-384 #8). Levels are spaced by a ratio ρ in read rate, H = ⌊2^(k·log₂ρ)⌋ for k = 0, 1, …, up to H = 1,024. **Built: ρ = 2, one level per octave** (ADR-384 #1), ρ one named constant so S3 can measure and the human hear ρ = 2^½ and 2^¼ beside it (OPEN, CONTRACT §7 C1). Engine-free: at one level per octave the top harmonic falls to c/2 (about 9.9 kHz at 44.1 kHz) at the bottom of each level's range. |
| WT-5 | **Pitch crossfade** (ADR-384 #8). In the upper third of each level's range in log read rate (PROPOSED), a member plays the level and its darker neighbour, mixed per sample by C.0's raised cosine of the position, so a glide never steps. The band limit stays exact, but the top harmonics fade early (B648). Engine-free, the loss in the 16 to 20 kHz band is in WT-F2's row; half-octave levels halve it for about twice the memory (CONTRACT §1.5). |
| WT-6 | **Level sizes and the read.** A level of H harmonics is N samples of one cycle, N ≥ 2H, with guard samples so no read wraps mid-kernel. The read and N are the builder's, judged by WT-F1. Engine-free, on a 1/n-saw table by QS-1's method at 44.1 kHz (C1, C2, C4, C6, C8): linear at N = 2048 reads 54, 64, 83, 100, 116 dB (fails QS-1 below C6); 4-point cubic at N = max(2048, 32·H) reads 118, 115, 109, 138, 140 dB; **6-point Lagrange at N = min(8192, max(2048, 16·H)) reads the 140 dB cap at C1, C2 and C4 (144 KiB per one-per-octave level set, the largest level 32 KiB): built**; 8-point capped at 4,096 reads 128 dB at C1 and the cap at C2 and C4 (112 KiB, largest 16 KiB), the fallback if the timed runs show cache misses. The H = 1,024 level (below about 19.5 Hz) has 8 samples per period: reported, not judged. |
| WT-7 | **Memory and cache.** Large levels cost memory and cache misses, and instruction counts do not see cache misses (B648): the timed runs judge them (WT-P3). The 2048-sample cycle is the exact time-domain image of one frame, the form of export, previews and the goldens' time-domain check; it is not required to be the form the member reads. |
| WT-8 | **Memory is stated** (PF-7) by the manifest at the build's ρ, N and Linear strategy (WT-23), reported every stage. **PROPOSED cap: 48 MiB** for both oscillators' table sets. CONTRACT §1.5 gives the figures. |
| WT-9 | **Every level exists before it is read.** A table set builds **every** level of a published set at each rebuild, not only those a sounding pitch uses, so a new note never meets a missing level. (A builder who builds only the levels in use must show the result is bit-identical at every pitch, including a note started mid-interval; otherwise this rule holds.) |

---

## 4. The table set (per oscillator, per instance)

One table set per oscillator per plug-in instance, shared by every voice and member of that oscillator. Its cost
is flat in the voice count (QUADRANT's central decision, kept).

| id | requirement |
|---|---|
| WT-10 | **Inputs.** Four corner tables (slots A to D) and four weights w = [w_A, w_B, w_C, w_D], each in [0, 1], summing to 1, from the morph (WT-53); the law (`wt.law`); the frame (section 5). Blending four different tables at any weights is the 1.0 design (ADR-377 A3 / ADR-385). |
| WT-11 | **Linear.** cₙ = Σ_c w_c·cₙ[c], on the frame-interpolated coefficients (WT-3). It equals a bilinear crossfade of the four corners' time-domain cycles; harmonics in opposed phase cancel and the centre can thin (WT-16 restores its loudness). |
| WT-12 | **Spectral log.** \|cₙ\| = max(0, exp(Σ_c w_c·Lₙ[c]) − ε), Lₙ[c] = ln(\|cₙ[c]\| + ε) interpolated by WT-3's log rule, ε = 10⁻⁴. A corner with weight 0 contributes nothing. The phase is arg(Σ_c w_c·cₙ[c]); where that sum is below 10⁻⁶ in magnitude, the phase of the heaviest corner, the lowest (A, B, C, D order) on a tie. The cache is derived data, never state. A fixed phase is the alternative to hear at S2 (QUADRANT's open question). |
| WT-13 | **Order.** Per corner, frame interpolation; then the law; then levels and loudness (WT-16); then publication. |
| WT-14 | **Rebuild on change, at control rate** (ADR-384 #7). A boundary falls every **R = 4 control intervals** (PA-3: 116 samples at 44.1 kHz, 128 at 48 kHz; ADR-384 #7's 128 samples stated in time, SW-14; PROPOSED). At a boundary the table set rebuilds only if a corner table, a weight, the law or (under Spectral log) the instance frame changed. A static patch rebuilds **zero** times, counted (IF-9). Boundaries are counted on the engine's sample clock (PA-3), so the result is independent of block size (DT-2). The rebuild runs on the audio thread with bounded work (RT-2) and no allocation (RT-1). |
| WT-15 | **The crossfade between published sets lasts R** (ADR-384 #7), by C.0's equal-gain raised cosine. A set that arrives during a crossfade replaces the pending one and starts when the running one ends; at most two sets are heard, three exist. Loudness changes (WT-16) ride it. **This crossfade is also the transition for a change of the law, of a corner's table, and of a Quantum flip of the tables** (WT-38): the set is per instance and shared by the voice's PA-5 copy and its live swarm, so PA-5 cannot crossfade it. **OPEN: the scheme** (CONTRACT §7 C7; B648: under motion a crossfade always runs, so every member read touches both sets, 4 table reads with the pitch crossfade). Three schemes: **(i)** each member reads both sets (exact); **(ii)** a fused read of both sets at one index, sharing the interpolation coefficients; **(iii)** the table set writes (1 − g)·old + g·new once per control interval and members read one set, a staircase of 4 steps across R. (iii) is kept only if it passes WT-F4 on its moving-phase probe (WT-F4 can fail it). **Built: (ii)**; S2 measures (iii). |
| WT-16 | **Loudness, level-matched to the base saw, with a safety peak limit** (ADR-384 #10). No user toggle. (1) **Loudness:** the blended spectrum is scaled so its **K-weighted** RMS (ITU-R BS.1770's K-weighting magnitude at each harmonic's frequency n × 130.81 Hz, C3, harmonics under 20 kHz there) equals that of the band-limited ±1 saw at C3, which is the base saw a table member replaces (`waves.h`'s saw). The weights are fixed, so the gain does not depend on the played pitch. Engine-free, on the factory set's frames 0, 5, 10 and 15 (32 frames, one member): this leaves a loudness spread of **0.00 dB at C3, 1.58 dB at C2 and 2.46 dB at C5**; plain Parseval RMS would leave 2.42 dB at C3 (−0.66 to +1.76, Vowel the loudest). (2) **Peak limit 2.6** (PROPOSED): if the scaled top level's peak exceeds it, the gain is reduced to meet it, counted. Engine-free, at this loudness the factory frames peak at up to 2.27 (crest factors 1.18 to 4.36, median 1.93), so the limiter is a safety, not the common path; a peak above 1.0 is allowed, as the engine guarantees none (OU-1, QS-8). (3) **Gain cap +24 dB** (PROPOSED), so a cancelling blend fades instead of amplifying rounding noise, counted. Under Linear's read-time strategy (WT-23) the gain comes exactly from the corners' K-weighted Gram matrix and the peak limit from the bound Σ_c w_c·peak_c, which can only over-limit. |
| WT-17 | **Determinism.** Same state and events give the same output sample for sample on one build (DT-1), at any block split (DT-2). The table set reads no clock (DT-5) and draws no random numbers. After `reset()` it is as a fresh prepare with the same state (DT-6). |
| WT-18 | **Tables arrive as events, built in full first.** A state load and a `set_table` call build every table they name, its spectra and its level sets, **on the calling (main) thread, completely, before they return**; never on the audio thread and never on a worker the audio thread waits for. A table assigned while the engine plays reaches it as a **sample-timed event** (the first sample of the shell's next processing call, recorded in the event stream), so a replay of the same events applies it at the same sample. After a load, the first published set exists before the first sample renders: no silent first R samples. So no table becomes audible at a time that depends on a thread's speed (WT-F5's load case). |
| WT-19 | **Base or table: patch state, never a choice on the page.** Per oscillator the engine's state holds `member`: `base` or `table` (not a manifest row and not a host parameter). Every patch made before the table member reads `base` and sounds as it did (RC-9.2). The GUI shows one oscillator with one wave; how the base wave appears beside tables is the direction ruling's (CONTRACT §7 A1, A2). A change while notes sound (a load, a migration) crossfades by PA-5. |

---

## 5. The member

| id | requirement |
|---|---|
| WT-20 | **Phase from the swarm, unchanged.** θᵢ, fᵢ, the start phase (SW-10), the onset wait (SW-12), drift, coupling and inertia are the swarm's (SW-1 to SW-17). The table is read at **φ = θᵢ**. In the clean-room engine the base wave is `wave(base, θᵢ + ½)` (`blade/waves.h`), so its saw is 2θᵢ − 1, rising from −1 at θᵢ = 0, and its sine is −sin 2πθᵢ. **The factory Sine → saw table has the same polarity** (its coefficients are the negated bench ones, WT-40), so a patch moved from the base wave to that table keeps its polarity at frames 0 and 15. |
| WT-21 | **Reads.** With no crossfade running and the read rate inside one level, a member makes one interpolated read per internal sample of each cycle it plays: one under Spectral log; under Linear two (adjacent frames) with a pre-blend or eight (four corners × two frames) with a read-time blend (WT-23). WT-15's and WT-5's crossfades each double that while they run. Counted (IF-9). |
| WT-22 | **The frame.** **Under Linear it is per voice** (ADR-384 #6): each voice reads at its resolved `wt.frame` plus its per-voice offsets (PA-2). **Frame spread:** member i reads at pᵢ = clamp(p + `wt.frameSpread`·sᵢ, 0, 1), sᵢ its position from the oscillator's spread law (C.10, BL-13; a single member has sᵢ = 0). **Under Spectral log it is global** (ADR-384 #6): the table set rebuilds at the frame of **the newest held voice** (MS-46's rule; with none held, the last such value), offsets included; frame spread is inert. |
| WT-23 | **Linear's strategy, and its cost.** Two strategies meet WT-11; the builder picks by measurement against WT-BUDGET and WT-8 (CONTRACT §7 C9). **(a) read-time blend:** level sets for every corner frame (4 × 16, plus staging, WT-18), built when a table arrives; each member reads 4 corners × 2 frames and sums; no rebuild. **(b) pre-blend:** at each rebuild, 16 blended frames B_f = Σ_c w_c·T_{c,f}, then 2 reads per member-sample; built from the blended spectra by inverse FFT (16 per rebuild while the weights move), or from (a)'s corner level sets by weighted sums (then (a)'s memory too). Neither may change the sound beyond WT-F10's tolerance. |
| WT-24 | **What the member hands on.** Its read, times SW-13's band-edge fade, before its PA-6 fade, entry gain, density scale and pan: where the clean-room engine's y⁰ sits (CONTRACT §1.1). A table read bypasses ED-1's filter E and the pending corrections, **except while sync is on** (`wt.syncIn` not off): its restart is an edge, corrected by ED-1 and ED-6 as a base wave's edges are, and the member then passes through E. `wt.syncIn` is stepped, so E engages and releases only inside PA-5's crossfade, and E's 2.4-sample delay (ED-2) never shifts a sounding member without one. |
| WT-25 | **DC.** Tables have no DC. The cycle means the transformations make are removed so QS-6's bound holds (WT-F8): **sync** exactly, from the table's antiderivative Y (harmonic n ÷ 2πn·i), mean = (Y(frac r) − Y(0))/r; **the warp, and the window**, numerically, by the midpoint rule over one member cycle with J ≥ 4·H points, recomputed at C.16's cadence and followed through C.16's 7.56 ms one-pole. C.16's 8 Hz blocker is the fallback for the loop (WT-31). |
| WT-26 | **Real-time safety.** RT-1 to RT-4 hold. Everything is allocated at prepare for the maxima (64 voices, 9 members, ADR-381). The table handoff never waits or locks on the audio thread (CONTRACT §1.6). |

---

## 6. Transformations

**The read chain**, per member per sample (each stage off at its neutral value): θᵢ → **sync** (WT-32) → **the
warp** (WT-34) → **phase push** (PM, WT-30; the loop, WT-31) → the read → **window** (WT-33) → the member's output.
Sync sets how many table cycles fit the member's cycle, the warp reshapes time inside each, PM pushes the result.

| id | requirement |
|---|---|
| WT-30 | **PM from osc 2** (osc 1 only in 1.0). `osc1.wt.pm` at index I pushes osc 1 member i's read phase by (I/2π)·m, BL-5's phase-FM form. **The modulator** m is osc 2's member j = (i mod U₂) output after its own PA-6 fade and entry gain, times osc 2's governing envelope (RC-6.5) **and osc 2's on-fade** (the voice's per-source fade, so osc 1's PM fades out with osc 2 and never drops in one sample); not its density scale, level, pan or gate. So osc 2 can be silent (level −∞) and still modulate, its envelope shaping the index. **During osc 2's PA-5 crossfade** m is the crossfade's mix, by the same raised cosine, of the copy's and the live swarm's member outputs; **during a change of osc 2's unison** the old and new j mappings are mixed over PA-6's 5 ms. osc 2 must be on (MO-26, RC-6.3) and renders first (CONTRACT §1.8). **osc 1 → osc 2** needs the two swarms interleaved per sample; `osc2.wt.pm` is reserved (5.1) until that is built (CONTRACT §7 E2). |
| WT-31 | **The loop: cross-mod and feedback, reused.** For a table member, `crossMod`, `feedback` and `loopDrive` act by C.14's law with xinᵢ added to the read phase: feedback is self-PM, cross-mod rings the members through one another. No new row. **This amends three things** (CONTRACT §8): C.14 (xin today offsets only Sync, FM and Ring carriers), C.16 (its blocker engages while the loop is live, which must include a table member), and PA-8's live conditions for the three rows (today each needs a blade on in Sync, FM or Ring). In the clean-room engine xin and the tap are computed only inside `n_blades_ > 0` blocks of `Swarm::render` (`swarm/swarm.cpp`, around lines 419–434, 456 and 483), so the table member's path must compute them. BL-12's stability map does not transfer: **before CONTRACT §7 D2 goes to the human, the lead maps the loop's contraction limits engine-free on the table member** for Sine → saw at frames 0 and 15 and Vowel at frame 7, by B521's method. |
| WT-32 | **Hard sync.** `wt.syncIn`: off, × pitch, or Hz. While not off, the read phase is frac(r·θᵢ), restarting at each wrap of θᵢ, r = `wt.sync` (× pitch) or max(1, `wt.syncHz`/fᵢ) (Hz: a fixed formant). The restart's value step and slope corner are corrected by ED-1's step table and ED-6's corner table on the member's pending ring, scaled by `bandLimit`; the table's value and slope there are exact from the spectra. |
| WT-33 | **Window.** The read is multiplied by g(θᵢ) = (1 − a) + a·½(1 − cos 2πθᵢ), a = `wt.window`. At 0, hard sync; at 1, the restart falls where g and its slope are 0 and no step remains: windowed table bursts, a fixed resonant peak under `wt.syncIn` Hz, the blades' most used formant sound (ADR-384 context). Between, WT-32 corrects (1 − a) of the step. Live only while sync is on. |
| WT-34 | **Asym and bend: one smooth warp, two axes.** φ' = φ + (0.9/2π)·[b·sin 2πφ + a·(1 − cos 2πφ)], b = `wt.bend`, a = `wt.asym`, each in [−1, 1], the pair scaled down together where √(a² + b²) > 1. The slope stays in [0.1, 1.9]: smooth, periodic, monotonic; no corners, no edges, no E. **Bend** compresses the cycle around its start; **asym** moves the half-cycle point to 0.307 or 0.693 at ±1, a pulse-width-like brightness on any table. Narrower widths come from the factory Pulse width sweep (to 0.06), not from a cornered map. |
| WT-35 | **Why these.** Brightness: the warp, smooth and morphable without aliasing. Resonant peaks and formants: sync and its window. FM growls: PM. **Mirror** is left out: it makes corners, and its even/odd balance the table blend already reaches. It can return by ruling. |
| WT-36 | **The level follows the read rate.** A member's level is chosen (WT-4) at each control tick for its **peak read rate over the coming interval**: fᵢ × the larger of the interval's two sync ratios (its ramp's ends) × the warp's peak slope 1 + 0.9·√(a² + b²) at the larger of its ends. So sync and the warp cost brightness, not aliasing. PM and the loop push the phase by an amount no level can predict: they are the aliasing that remains at Standard (WT-37). |
| WT-37 | **Aliasing, per transformation**, as below: the Standard column is a requirement, the High column says when the 2× `quality` (SPEC-ENGINE 5.2) is needed. |

| transformation | what aliases | at Standard (1×) | High (2×) |
|---|---|---|---|
| none, a table alone | interpolation only (WT-6) | meets WT-F1 | never needed |
| sync, window | the restart, if uncorrected | step and corner corrected; level for r·fᵢ | never needed for the restart |
| asym, bend | nothing new | smooth; level for the peak slope | never needed |
| PM | sidebands past the level's band | aliases by design, as blade FM does (QS-3) | recommended for bright, high-index PM |
| the loop | its own spectrum above its stability limit | aliases by design, as the blades' loop does (QS-3) | recommended |

Table members run at the internal rate like every member; there is no opt-out (BD-3).

| id | requirement |
|---|---|
| WT-38 | **Transitions.** Every amount is continuous, glided by PA-1 and ramped per control interval (PA-3); `wt.sync`, `wt.syncHz` and `wt.pm` also take per-sample offsets (PA-4). `wt.syncIn` and WT-19's `member` change by PA-5's 5 ms crossfade (per member). **The law, a corner's table and a Quantum flip of the tables** change by WT-15's rebuild crossfade (R, 2.6 ms at 44.1 kHz), a brief timed transition (ADR-377). |
| WT-39 | **Cost when off** (PF-6). Each transformation at its neutral value costs under 1 % of the patch; all neutral, the member is one read per sample (WT-21). |

---

## 7. Tables: factory, recipes, capture, library

| id | requirement |
|---|---|
| WT-40 | **The factory set is generated from formulas, and only from formulas.** No table is taken, sampled, traced or derived from another product's content, and none is imported. The 1.0 set is the bench's eight at 1,024 harmonics: Sine → saw, Pulse width sweep, Vowel ah → ee, Hard sync 1 → 6, FM index 0 → 8, Triangle fold, Moving comb, Drawbar registrations. Sine → saw takes the bench's coefficients negated, to match the base wave's polarity (WT-20). More only by ruling. |
| WT-41 | **Generator accuracy.** A spectral generator writes its coefficients. A time-domain one is analysed without aliasing (its analytic Fourier series, or an FFT of the formula at ≥ 64 × 2,048 points), each frame within 10⁻⁶ of peak of a 4× finer analysis. The bench's 512-point DFT is not ported. |
| WT-42 | **Recipes.** A table a patch references is stored as a recipe, never as data in the host's state chunk (ADR-384 #2). A recipe is a **generator** (id, version, settings, seed) or a **capture** (its sources' ids, x, y, frame or "all", the law, **the law's version `lawV`**). Rebuilding gives the same table on one build (DT-1). Format: CONTRACT §3. |
| WT-43 | **Goldens, frozen per version.** Each generator version and each blend-law version has goldens: spectra hashes on the reference build, coefficients within 10⁻⁶ of peak on any. A change of output is a new version; old versions stay buildable, so old patches and captures keep their sound. A golden never moves to make a build pass. |
| WT-44 | **Capture.** Freezes an oscillator's blend into a new table, off the audio thread: all 16 frames (frame f at f/15, the current weights and law) or this frame only (repeated). All 1,024 harmonics, peak-normalised per frame (WT-2), passing WT-47. Sources may be captures. **Bounds are checked when the capture or the assignment is made**, never at save (a plug-in cannot refuse a host's save): a capture or assignment that would leave the patch with more than **64 distinct recipes** or a capture chain deeper than **8** is refused at that moment, named. The 64-recipe bound is the binding one (a depth of 8 with four distinct sources at every step would need 4⁸). |
| WT-45 | **The user library** is device-wide storage (ADR-384, FX device note) in the user's application-support folder. A table's id is the hash of its recipe (CONTRACT §3.1), never a random draw. A library file holds a table's recipe and spectra. **Deleting a library table deletes only the file**: corners and patches that use it keep playing from their recipes; the page marks it "not in the library" and offers to restore it. Delete is two-step. |
| WT-46 | **The random generator's slot:** generator id `random`, a version, settings and a 32-bit seed; every draw from a mulberry32 stream keyed by purpose (DT-4, MO-6's scheme, purposes beginning `wt/random/`). |
| WT-47 | **Every table passes the table gates** before it is stored or played (ADR-384, related): no DC; its energy under 20 kHz at C3 within 12 dB of its full energy (PROPOSED); no near-silent frame; frames that differ (unless the generator declares one frame); WT-F1 on a static read. A failing table is refused and named, never silently fixed. |

---

## 8. The morph

One pad (ADR-384 #5): the table set's X and Y are the morph position (`morph.x`, `morph.y`, D.2, MO-4); its
corners are the morph's corners.

| id | requirement |
|---|---|
| WT-50 | **The weights are the morph's** MO-4 weights as WT-53 sends them. With `morph.on` off, `morph.audible` is one-hot (MO-20). **X and Y move by the host, the pad, and the global LFOs and the 2-D random only** (MS-29, ruled by ADR-385; ADR-384 #9's per-note routes to X and Y are moot). |
| WT-51 | **The corner table key** `oscN.wt.table` is a corner value held in the morph system's corner state, as `oscN.ratios` is (MO-13, RC-11.1): a recipe id (WT-42), not a host parameter; the recipe text lives in the patch's table dictionary (CONTRACT §3.2). It is one of oscillator N's keys for MO-26, so it blends or picks only among the corners where the oscillator is on. |
| WT-52 | **Everything else follows ADR-377.** Every other Structural key still follows its quantum boundaries (MO-13). The corner tables are the one exception, by ADR-377 A3 / ADR-385. |
| WT-53 | **The spectral-blend key class** (ADR-377 A3 / ADR-385): `oscN.wt.table` is a key of a new class whose weights follow `morph.law`: **Blend and Stepped**, MO-4's unglided weights (at the stepped position under Stepped), corners holding the same recipe pooled into one weight, Count Two by MO-9's two-candidate rule, glided by MO-5 (b) applied to the weights, restricted to the oscillator's on corners (MO-26 (2)); **Quantum**, one-hot at the key's pick and latch (MO-7, MO-8), a flip by WT-15's rebuild crossfade; **Exempt**, corner A (MO-12). Waypoints and region locks do not act on it in 1.0 (PROPOSED). |
| WT-54 | **The other `wt.` rows follow their class** (CONTRACT §2): Morphable rows blend (MO-9) and flip under Quantum (MO-7); Structural rows (`wt.law`, `wt.syncIn`) follow their quantum boundaries (MO-13), `wt.law` changing by WT-15, `wt.syncIn` by PA-5. |
| WT-55 | **Save current sound** (MO-25). Where the corners' tables differ at the position, Save writes into the corner a capture recipe of the heard blend (all 16 frames, the law and its version, x and y), subject to WT-44's bounds, so the corner alone sounds as the position did. |
| WT-56 | **Modulation.** Every continuous `wt.` row is a destination (MS-12 (a)) of scope `per-voice`. Under Spectral log, `wt.frame` acts through WT-22's newest-held-voice frame, and the page warns as MS-46 has it. Under `device.perNoteToShared` = Refuse (MS-47), per-note routes to `wt.frame` are muted and named while the law is Spectral log (recommended, CONTRACT §7 B2). |

---

## 9. Fidelity targets (measured)

Each target has a method, plants and a control. **Detector plants (D)** are inputs the meter must catch, proven
engine-free at S0. **Mutation plants (M)** are builds with one engine mechanism disabled, run at every stage
close. **The control** must read zero, or pass. A gate whose plant does not fire or whose control does not read
zero is red. RATIFIED numbers are SPEC-ENGINE's; PROPOSED ones wait for the human.

| id | target | number, status | method | plants | control |
|---|---|---|---|---|---|
| WT-F1 | **Aliasing, a table alone** | ≥ 110, 104, 98, 93 dB at C2, C4, C6, C8 (QS-1's, RATIFIED for edges; PROPOSED here); C1 reported | QS-1's method exactly (one member, Standard, 44.1 kHz, odd whole cycles in 65,536 samples, rectangular window, double precision, non-harmonic bins 20 Hz to 20 kHz, capped at 140 dB) on Sine → saw at frame 15; every factory table at frames 0, 7, 15 reported | (D) QS-1's naive saw reads 9 to 27 dB (engine-free 16 to 28); (M) the 2048-sample linear read (engine-free 64 dB at C2) | direct additive synthesis reads the 140 dB cap (engine-free ≥ 240 uncapped) |
| WT-F2 | **The top of the band** | **OPEN** (CONTRACT §7 C1): QS-2's 3 dB at C6 (RATIFIED for edges) **cannot be met under ADR-384 #8's ceiling and WT-5's crossfade**. Engine-free, the 16 to 20 kHz energy of a saw table against an ideal band-limited saw at 44.1 kHz: **C6 −6.84 dB at one level per octave, −5.30 at half- and quarter-octave, −2.54 at eighth-octave**; the band is empty at some notes from C2 to B5 at one and half per octave, −7.6 dB worst (semitones) at quarter, −2.5 dB at eighth. **Judged until ruled:** the measured figure equals the engine-free one for the built spacing within 0.5 dB at C6 and at every semitone C2 to B5 | QS-2's method per note; the engine-free figure from the built spacing, crossfade and ceiling | (D) a saw with its top four harmonics removed; (M) the pitch crossfade disabled: at one level per octave C6 reads −5.30 against the design's −6.84, outside 0.5 dB | a read at a pitch exactly at a level's top, no crossfade: the design's figure |
| WT-F3 | **Transformations' aliasing** | reported (QS-3's rule) | QS-3's method on each transformation's sweep, Standard and High | (M) sync with its level chosen for fᵢ instead of r·fᵢ: at r = 4, C5, its alias report worsens by ≥ 20 dB (PROPOSED) | a table alone reads clean at both |
| WT-F4 | **Clicks** | no event (QS-4, RATIFIED) | the ratified detector and its probe rule. **The probe moves phase:** four sine tables at 0°, 45°, 90° and 135° in corners A to D (every blend of them is a sine, never cancelling, whose phase moves with the position). Cases: X and Y corner to corner in 10 ms; the law switched; a Quantum flip of the tables; a corner's table changed; each with one and seven members. A case on rich, differing corners is judged by WT-F10's sweep law and reported here | (D) QS-4's own (0.05 steps planted, 10 of 10); (M) the rebuild crossfade disabled: sets swapped at once make a phase step of about 34° per rebuild in the 10 ms sweep, which must fire; WT-15 (iii) must pass this case to be kept | the same render with the position still: no event |
| WT-F5 | **Determinism** | bit-identical (DT-1, DT-2, RATIFIED) | two offline renders of the same state and events; blocks of 1, 7, 128, 4,096 and uneven; **and the load case: load a state with captures and user tables, render at once, twice: bit-identical** (WT-18); and a `set_table` mid-render at a recorded sample, twice | (M) an unseeded draw in the rebuild; (M) a table applied when a worker finishes instead of at its event | two renders of one build: zero difference |
| WT-F6 | **Static patch** | zero rebuilds in 10 s (PROPOSED) | the rebuild counter | (M) a rebuild forced every boundary | the counter reads 0 |
| WT-F7 | **Loudness steady across the pad** | BS.1770 integrated loudness (OU-2's ungated measure, 3 s held C3, one member) at a 5 × 5 grid within **±1.0 dB** of its median for every corner set S2 uses, both laws (PROPOSED; WT-16's engine-free spread at C3 is 0.00 dB on single tables); C2 and C5 reported; positions where the peak limit or gain cap engages reported, not judged | OU-2's measure | (M) WT-16's K-weighting replaced by plain RMS on a sine and Vowel corner pair (engine-free 2.42 dB apart) | four identical corners: 0.0 dB |
| WT-F8 | **DC** | mean over each second within 0.003·10^(L/20) (QS-6, BL-19, RATIFIED) | QS-6's method with every transformation swept | (M) WT-25's removal off under asym 0.8 on a saw table | a sine table, no transformation |
| WT-F9 | **Silence and denormals** | exact zero after the last voice; no subnormal (QS-7, RT-4, RATIFIED) | QS-7's method | (D) a subnormal planted in the output; (M) one planted in a member's state with flush-to-zero off | a released note's tail is exact zero |
| WT-F10 | **The blend's law** | corner identity: at each corner each law reproduces that corner's frame by **its own** WT-3 rule within 10⁻³ relative magnitude per harmonic, at whole and fractional frames; Linear equals the bilinear time-domain crossfade within 10⁻⁵ of peak; a sweep's output equals the ideal of WT-14 and WT-15 within 10⁻⁵ (10⁻³ for WT-15 (iii), PROPOSED) | engine-free reference blends against the published set and the output | (M) corners swapped (A ↔ B); (M) Spectral log checked against the other WT-3 rule (misses by ≥ 4.7 × 10³) | four identical corners: the blend equals the corner within 10⁻⁶ before normalisation |
| WT-F11 | **Capture** | a 16-frame capture in all four corners at X = Y = 0.5 under Linear matches the blend within 10⁻⁴ at every frame; a this-frame capture has 16 identical frames; captures of captures load, play, save; bounds refused at the limit (WT-44) | round trip through the library and the state | (M) the capture made at the wrong frame position | a capture of four identical corners equals the corner |
| WT-F12 | **Bounded and finite** | every sample finite, under QS-8's bound (QS-8, RT-3, RATIFIED) | fuzzed parameters, extreme PM and loop | (M) a NaN planted in a published set: contained and counted | the S7 patches: no non-finite count |
| WT-F13 | **Real-time safety** | no allocation, lock or system call on the audio path (RT-1) | RealtimeSanitizer and ThreadSanitizer (Homebrew LLVM) over a render that loads, assigns and captures tables | (M) a `malloc` planted in the rebuild (rtsan); (M) an unsynchronised write to a staged slot after it is posted (TSan) | the clean build: no report |

---

## 10. Performance

| id | requirement |
|---|---|
| WT-P1 | **WT-BUDGET = 4 % of one Apple M3 performance core per oscillator**, at PF-3's chord with 9 members and the morph moving: **PROVISIONAL** (ADR-385), confirmed by a timed run in the 2 to 6 am window at S0 and again at every stage's close (CONTRACT §5.4). It covers the oscillator's table-member cost: the table set's rebuilds plus every member's reads and transformations. **Under the replace reading the whole oscillator**, swarm laws included, is held to PF-1's 10.7 % per swarm oscillator (ADR-354); today's reference is 61.9 % for the Glass horde pad (ADR-384 #11; REPORT.md §7 at 866bfd3). |
| WT-P2 | **The cells:** PF-3's chord (8 voices, the k-th on key 48 + 3·(k mod 8), velocity 0.85), 44.1 kHz (48 kHz secondary), 128-sample blocks, PF-3's timing method and load guard. **Judged against WT-BUDGET, all at U = 9 with X and Y under a 5 Hz global LFO:** (1) Spectral log; (2) **Linear with per-voice frame scanned by an envelope** and frame spread 0.5; (3) as (2) with sync, asym and PM on. **Reported:** each static; U = 7; High; 16 voices. |
| WT-P3 | **Two measures, every stage** (ADR-385; ADR-380 A1): the instruction count of the cells, at once, beside the test count; and a timed run of the judged cells in that stage's night window. Only the timed run sees cache misses and memory stalls (B648). A stage closes only after its own night's timed run is within its share (CONTRACT §5.4). |
| WT-P4 | **Cost when off.** With every oscillator's `member` at `base` the engine renders bit-identically to the engine without this code, and the table sets cost nothing (PF-6, RC-6.4's method). |
| WT-P5 | **Scaling.** The table set's cost is flat in voices and members; reads grow linearly in sounding member-samples (PF-5). |
| WT-P6 | **Memory** per PF-7, held to WT-8. |
| WT-P7 | **The evidence so far** (B648, the lead, 2026-11-07; one oscillator, PF-3's chord, 48 kHz, 2 s, retired instructions at an uncalibrated 10 × 10⁹ per second). It measured a 2,048-sample linear read with a fixed gain, no per-voice frame, no DC removal and no transformations, so it is a floor for this design, not its cost: worst cell (H = 1,024, Spectral log, 9 members, morph moving) 786 M instructions, about **3.9 %**; static 1.2 %; H = 64, Linear, one member, static 0.14 %; moving costs 2 to 8 times static; the worst cell splits as inverse FFTs 38 %, member reads 38 %, the blend law 15 %, coupling 4 %. **The critic's rough estimate puts the specified design near 7 %**, so the provisional 4 % will likely need WT-15's cheaper schemes and the cheaper Linear strategy; CONTRACT §5.4 says what happens when a timed run exceeds it. The bench is being extended (6-point read capped at 8,192, normalisation, per-voice frame, the crossfade schemes). |

---

## 11. Open items

CONTRACT §7 is the one list of open decisions, each with a recommendation.
