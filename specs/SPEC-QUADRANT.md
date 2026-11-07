# Quadrant Wavetable for HORDE — Spec

Nov 7, 2026 · @Julian Beall Smith

## Summary

Add Quadrant to horde 2 as an oscillator engine: four 16-frame wavetables sit at the corners of an XY field, and every voice plays one shared, band-limited blend of them. Two blend laws ship, **Linear** and **Spectral log**. Any blend can be baked into a new named wavetable in the user library and loaded back into a corner.

The browser bench (Quadrant Wavetable, v3) is the behavioural reference for this spec. It is a listening reference and a source of test cases, not a bit-parity oracle. Its naive DFT/resynthesis loops are not to be ported.

Recommendations in brief:

- **One blend per engine instance, not per voice.** XY and frame are global-scope parameters. Cost then stays flat as voice count grows.
- **Spectral representation is canonical.** Tables are stored as harmonic coefficients, and the blend law is a swappable function on them.
- **Rebuild at block rate, read at sample rate.** A control-rate rebuild produces seven mipmapped cycles; voices do one interpolated read per sample and crossfade between rebuilds.
- **No oversampling.** Band-limiting is exact by construction, so this engine opts out of HORDE's 2× path.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| XY scope | Shared per engine instance; X, Y and Frame are global-scope mod destinations | Per-voice positions mean one rebuild per voice, multiplying the expensive path by voice count. Revisit only if per-note XY becomes a must-have. |
| Blend laws | Linear and Spectral log only | They are the two most audibly distinct laws. Equal power and magnitude-only Spectral were dropped after listening. |
| Canonical data | Harmonic coefficients, 64 harmonics × 16 frames per table | Blend laws, band-limiting and capture all operate on spectra. Time-domain cycles are derived output. |
| Rendering | Block-rate rebuild into 7 mip levels; per-sample interpolated reads per voice | Rebuild cost is paid once regardless of voice count. Per-voice cost is the same in both laws. |
| Anti-aliasing | Mip level chosen per voice so the top harmonic stays under 20 kHz | Exact band-limiting without oversampling, which fits HORDE's 2×-maximum constraint. |
| Captures | 16-frame bake of the current blend, saved as a first-class table | Captures behave exactly like factory tables and can be recaptured, so blends of blends work. |

## Data model

A table is 16 frames; a frame is 64 harmonics, each a complex coefficient. Harmonic n contributes re·cos(2πnt) + im·sin(2πnt). Index 0 (DC) is always zero.

| Field | Type | Notes |
| --- | --- | --- |
| Frames | float32 \[16\]\[65\]\[2\] (re, im) | 8,320 bytes per table. Each frame is peak-normalized to 1.0 when the table is built. |
| Log-magnitude cache | float32 \[16\]\[65\] | Derived at load: ln(\|c\| + 1e-4). Lets Spectral log skip hypot and log at blend time. |
| Name | string, ≤ 40 chars | User-visible. |
| Origin | factory \| capture | Factory tables are generated, not stored. |
| Capture metadata | law, x, y, frame or "all", source corner table ids, created time | Recorded for captures only. Feeds provenance and the library card. |

Factory set (8 tables, generated at startup from formulas): Sine → saw, Pulse width sweep, Vowel ah → ee, Hard sync 1 → 6, FM index 0 → 8, Triangle fold, Moving comb, Drawbar registrations. Time-domain generators are analysed to 64 harmonics with an FFT; the bench's formulas are the reference.

The user library stores tables as a file per table in the user's HORDE data folder, keyed by a stable id. A preset that references a user table also embeds its 8,320 bytes, so presets travel between machines. On load, a missing library table falls back to the embedded copy.

## Blend math

Corner A is top-left, B top-right, C bottom-left, D bottom-right, with y = 0 at the top. Weights are bilinear and always sum to 1:

```latex
w_A=(1-x)(1-y),\quad w_B=x(1-y),\quad w_C=(1-x)\,y,\quad w_D=x\,y
```

**Frame interpolation** happens first, per corner: frame position p ∈ \[0, 1\] maps to u = p·15, and the coefficient is a linear lerp of re and im between frames ⌊u⌋ and ⌊u⌋+1. This is linear in both blend laws.

**Linear** sums complex coefficients. It is identical to crossfading the four time-domain cycles, so harmonics with opposing phases cancel and the centre can thin out.

```latex
c_n = \sum_i w_i\, c_{i,n}
```

**Spectral log** blends magnitudes as a weighted geometric mean and takes phase from the linear sum. Only harmonics present in all weighted corners survive near the centre.

```latex
|c_n| = \exp\Big(\sum_i w_i \ln(|c_{i,n}| + \varepsilon)\Big) - \varepsilon,\qquad \varepsilon = 10^{-4}
```

Phase is arg(Σ wᵢ cᵢ,ₙ). When that sum's magnitude is below 1e-6, use the phase of the corner with the largest weight. Negative magnitudes from the ε subtraction clamp to 0.

**Normalization** (user toggle, default on): after rebuilding, scale every mip level by 0.85 / peak of the top level. With it off, apply a fixed 0.85. Gain changes ride the rebuild crossfade, so they cannot step.

## Real-time engine

The engine rebuilds one blended table set at block rate and every voice reads from it at sample rate. Everything runs on the audio thread, so rebuilds are deterministic and replayable.

&#91;embedded content: Quadrant engine · rebuild path and voice read path\]

The top band runs once per engine instance, and only when an input changes. The bottom band runs for every voice on every sample.

**Rebuild (control rate).** At each rebuild boundary, check whether X, Y, Frame, law, corner tables or normalization changed. If nothing changed, skip: a static patch costs nothing here. Otherwise:

1. Interpolate each corner's frame (complex lerp).
2. Apply the blend law to produce 64 coefficients.
3. Inverse-FFT into the mip levels that sounding voices currently use. Levels hold 1, 2, 4, 8, 16, 32 and 64 harmonics.
4. Apply the normalization gain and publish the set to the voices.

Default boundary: every 128 samples, exposed internally so profiling can tune it. Size each level at 32 samples per period of its top harmonic, with a 64-sample floor: 64, 64, 128, 256, 512, 1024, 2048. Seven full 2048-sample levels would cost about 3.5× more for no audible gain.

**Voice read (sample rate).** Each voice picks the highest level whose top harmonic is at or below 20 kHz at its pitch, re-evaluated per block. It reads with a phase accumulator and linear interpolation. When a new set is published, every voice crossfades from the old set to the new one over 256 samples. If another set arrives mid-crossfade, the newest replaces the pending one, so at most two sets are ever live.

Voices start at a random phase so chords do not stack peaks, drawn from a per-voice seeded generator to keep replay deterministic. Envelopes, polyphony limit, stealing policy and output gain staging come from horde 2's shared voice system. The bench's 8 voices, oldest-voice stealing and tanh output stage are bench-only.

| Cost item | Linear | Spectral log | Scales with |
| --- | --- | --- | --- |
| Blend, 64 harmonics | \~512 multiply-adds | \~8k cycles (with log-magnitude cache) | Rebuild rate |
| Inverse FFT, levels in use | \~100–150k flops for 2–3 levels | same | Rebuild rate, pitch spread |
| Voice read | \~15 flops per sample | same | Voices × sample rate |
| Estimated total, 8 voices, XY under constant modulation, 48 kHz | \~1–2% of one core, scalar | \~1–2% of one core, scalar | — |

These are paper estimates and must be confirmed by profiling. The two laws cost almost the same because the inverse FFT dominates.

Optional Linear fast path, only if profiling shows rebuilds dominate: precompute each corner frame's mip cycles at load (about 1 MB for four tables). A Linear rebuild then becomes 8 weighted adds per sample of the level instead of an inverse FFT.

## Capture to library

A capture freezes the current blend into a new table that behaves exactly like a factory table.

- **Scope: all 16 frames** (default). For each frame f, blend every corner at frame position f/15 using the current X, Y and law. The capture still scans like its sources.
- **Scope: this frame only.** Blend once at the current frame position and repeat it across all 16 frames.
- Captures always use the full 64 harmonics, never a pitch-limited level, and run off the audio thread.
- Each captured frame is peak-normalized to 1.0, the same rule factory tables follow. The user's normalization toggle does not affect what is saved.
- **Name**: user-entered, up to 40 characters; blank defaults to "Blend 01", "Blend 02" and so on.
- **Recursion**: captures appear in every corner's table menu and can be captured again without limit. Store source table ids so a capture's lineage can be shown, which fits FOUNDATIONS' tree-based history.
- **Delete** is two-step in the UI. Deleting a table that a corner is using swaps that corner to its default factory table; presets that embed the table keep working.

## Parameters and UI

| Parameter | Range | Default | Mod destination |
| --- | --- | --- | --- |
| X | 0–1 | 0.35 | Yes, global scope |
| Y | 0–1 | 0.30 | Yes, global scope |
| Frame | 0–1 (shown as 0–15) | 0.4 | Yes, global scope |
| Blend law | Linear \| Spectral log | Linear | No |
| Normalize | on \| off | on | No |
| Corner A–D table | factory or user table | Sine → saw, Pulse width sweep, Vowel ah → ee, Hard sync 1 → 6 | No |

UI elements to carry over from the bench:

- XY pad with corner labels and live weights, tinted by a bilinear mix of the four corner colours.
- One card per corner: table menu (factory and user groups), a stacked 16-frame preview with the current frame highlighted, and a weight bar.
- Output scope of the blended cycle and a harmonic spectrum in dB with the spectral centroid.
- Capture row under the pad: name field, scope menu, Save button.
- Library panel: one card per saved table with a frame-stack preview, its capture metadata, load-to-A/B/C/D buttons and two-step delete.

The bench's Orbit control is a stand-in for an LFO. In HORDE, X and Y are driven by the mod matrix instead.

## Integration with HORDE

Quadrant ships as an oscillator engine in horde 2's engine slot, alongside the blade engine, and uses the shared voice, FX and mod systems.

- **Intent bus.** The XY pad writes to intents like every other HORDE pad. Corners bind intents to X and Y, so the pad no longer drives load-bearing corner parameters directly.
- **Inertia.** The XY puck should accept HORDE's inertia model, giving it mass when dragged or modulated.
- **Mod matrix.** X, Y and Frame are global-tier destinations because the blend is shared. Per-voice sources such as velocity or a per-note envelope can't target them in v1, and the UI must say so rather than accept the routing silently.
- **Morph system.** Corner table choices and the blend law can be morph-corner parameters. A table switch at a morph boundary goes through the normal rebuild crossfade.
- **Oversampling.** The engine is band-limited by construction, so exclude it from the 2× path.
- **Presets** embed any user tables they reference (see Data model).

## Acceptance tests and open questions

| Test | Pass condition |
| --- | --- |
| Corner identity | At each corner, both laws reproduce that corner's interpolated frame within 1e-3 relative magnitude per harmonic. |
| Linear equivalence | Linear output equals a bilinear crossfade of the four time-domain corner cycles within 1e-5. |
| Band-limit | For every MIDI note 0–127, no voice plays a harmonic above 20 kHz. A full-range sweep shows no aliasing in an FFT. |
| Click-free motion | X and Y swept corner to corner in 10 ms, and blend law and table switched mid-note: no sample-to-sample step larger than the steady-state maximum. |
| Static patch | With no parameter changing, rebuild count over 10 s is zero. |
| Capture round trip | Load a 16-frame capture into all four corners at X = Y = 0.5 under Linear: output matches the captured blend within 1e-4 at every frame. A this-frame-only capture has 16 identical frames. |
| Recursion | A capture made from corners that are themselves captures loads, plays and saves correctly. |
| Preset portability | Delete a user table from the library, reload a preset that uses it: the embedded copy plays and the corner shows its name. |
| Determinism | Two offline renders of the same MIDI and automation are bit-identical. |
| CPU | 8 voices across 3 octaves, X and Y under a 5 Hz LFO, Spectral log, 48 kHz: within the budget below. |

Open questions:

- [ ] CPU budget for this engine at the CPU test above. Suggested: ≤ 2% of one core, given that 29% for the heaviest patch is already considered too heavy.
- [ ] Spectral log phase source. The bench takes phase from the linear sum; stored per-table phases or fixed phases would be steadier and cheaper. Decide by A/B listening.
- [ ] Per-voice XY as a later increment. It would cost one rebuild per voice, so it needs its own budget.
- [ ] Quantum morph as a third law: each harmonic resolves stochastically to one corner, weighted by XY. It is native to HORDE's morph system but out of scope for v1.
- [ ] Normalization by peak or by RMS. Peak (current) keeps levels safe; RMS keeps loudness steadier across the pad.
