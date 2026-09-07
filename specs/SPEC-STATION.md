# STATION — Engine Specification

**Project:** HORDE (engine type)
**Status:** Approved for implementation
**Reference prototype:** `reference/station.html` (browser, Web Audio, validated by Julian 2026-08)
**Version:** 1.0

---

## 1. Identity and role

STATION is HORDE's traditional-synthesis workhorse: a lightweight 3-operator phase-modulation engine with a dedicated LFSR noise channel, covering subtractive, FM/PM, and chiptune idioms (design targets: Sylenth1 × Ableton Operator, with a crisp chip bias). It is deliberately the *dependable* engine — minimal exotic process, maximal coverage per CPU cycle.

**Explicit non-goals:**
- **No unison / detune stack.** Unison is the SAW (Kuramoto swarm) engine's entire ontology. Fat stacks of STATION are achieved by engine-layering at the HORDE level.
- **No internal filter.** Filtering happens in HORDE's shared downstream chain (see Appendix A for filter-bank recommendations).
- **No internal FX, no arp/sequencer** (HORDE-level modules).

---

## 2. Signal architecture

```
                 ┌────────────── 4×3 PM MATRIX ──────────────┐
                 │  sources: OP1 OP2 OP3 NS   dests: OP1-3    │
                 │  diagonal (op→self) = feedback              │
                 └───────────────┬─────────────────────────────┘
                                 │ (one-sample delay on all taps)
   ┌───────┐   ┌───────┐   ┌───────┐         ┌───────────┐
   │  OP1  │   │  OP2  │   │  OP3  │         │ NOISE (NS)│
   │ ×env1 │   │ ×env2 │   │ ×env3 │         │  ×envN    │
   └───┬───┘   └───┬───┘   └───┬───┘         └─────┬─────┘
       │ lvl,pan   │ lvl,pan   │ lvl,pan           │ lvl,pan
       └───────────┴─────┬─────┴───────────────────┘
                         ▼
                 engine stereo out → HORDE shared chain
                 (filter bank, FX, master — out of scope)
```

- Each slot's output is **envelope-scaled at the source**, so the envelope shapes it both as a carrier (mix) and as a modulator (matrix). This is the Operator behavior and is load-bearing for FM sound design.
- **Modulation depth lives in the matrix cell; audible level lives in the LVL slider.** They are decoupled (an op with LVL 0 is a pure modulator).
- All matrix taps read the **previous sample's** slot outputs (one-sample delay everywhere). This is the defining semantics, not an approximation: it makes arbitrary routing cycles, including self-feedback and mutual modulation, unconditionally stable. Implement identically in C++.

---

## 3. Operators (×3)

### 3.1 Tuning
Three modes per op:

| Mode | Controls | Range |
|---|---|---|
| RATIO | ratio (continuous), fine | ratio 0.25–16.0 **continuous**, optional snap to 0.5 grid; fine ±50 cents |
| PITCH | semitones, fine | ±24 st; ±50 cents |
| FIXED | frequency | 20–4000 Hz, log taper |

**Divergence from prototype:** the prototype steps RATIO at 0.5. The build must make it continuous — slewing a ratio *through* inharmonic territory under inertia is a first-class gesture for this engine. Snap is a UI affordance, not a DSP constraint.

### 3.2 Waveforms
Per-op selector: `SIN | TRI | SAW | PLS | QTR | DRW`

- **PLS:** continuous pulse width 5–95%, with UI snap buttons at 12.5 / 25 / 50% (NES duty set). PW is a mod target.
- **QTR:** triangle quantized to 16 amplitude levels (Game Boy CH3-style stepped tri).
- **DRW:** reads the shared Wave RAM (§5).

### 3.3 Pure↔raw continuum (per op)
Two render branches, crossfaded by `PURE` (0–1):

- **Pure branch (PURE=1):** band-limited. SAW/PLS via polyBLEP (BLAMP acceptable for TRI if aliasing is measurable); SIN/TRI analytic; QTR renders as smooth triangle; DRW renders band-limited (§5).
- **Raw branch (PURE=0):** naive rendering of the **phase-quantized** phase: `q = floor(p·QNT)/QNT` when `QNT > 1`, else the raw phase. `QNT ∈ {OFF, 4, 8, 16, 32, 64}` phase steps per cycle. Phase quantization is the chip-authentic degradation (distinct from any downstream bitcrush) and is a mod target.

Both branches are computed and crossfaded; at 3 ops this is cheap and it keeps the continuum artifact-free under modulation.

### 3.4 Phase, sync, ring
- Per-op phase offset 0–360°, with **RETRIG / FREE** mode (prototype is retrig-only at phase 0 — build adds both).
- **SYNC:** ops 2 and 3 may hard-sync to op 1's phase wrap (reset to their phase-offset value, not to 0).
- **RING** *(not in prototype)*: per-op partner select on ops 2/3 — `OFF | ×OP1 | ×OP2` — multiplying the op's post-envelope output by the partner's output before mix. Ring applies to the mix path only, not the matrix tap.

---

## 4. PM matrix

- 4 sources (OP1, OP2, OP3, NS) × 3 destinations (OP1–3). Op diagonal = self-feedback. NS has no destination column (noise receives no PM).
- Cell range 0–8 "index"; applied to phase as `p += cell · out_src / 2π`.
- **Every cell is a global-mod-matrix destination** (this is where quantum-morph and macros grab the routing).
- **Algorithm presets** are stored patches over (matrix values + op levels), nothing more. Ship the six from the prototype: `STACK`, `2→1`, `3→2→1`, `2+3→1`, `3→1+2`, `FB CH`. Preset recall must be click-free (control-rate smoothing on cells, ~5 ms).

---

## 5. Wave RAM

- One shared table per patch: **32 samples × 4-bit** (values 0–15), Game Boy wave-channel semantics — every op set to DRW reads the same RAM.
- **Raw branch:** zero-order hold on the quantized phase.
- **Pure branch — divergence from prototype:** the prototype linearly interpolates. The build must render band-limited via the table's exact spectrum: 32 real samples → ≤16 partials; either direct additive resynthesis or a per-octave mipmap rebuilt from those partials. Rebuild happens at control rate on edit (the table is user-drawable live); rebuild must be allocation-free and click-free.
- Factory presets: SIN, SAW, SQR, BELL, RND (match prototype generators).
- Table is preset-scoped and a candidate quantum-morph surface (corner tables) — expose it to the morph system but do not build table-morph logic into the engine.

## 6. Noise channel (NS)

- 15-bit LFSR, NES semantics: feedback = bit0 XOR bit1 (**LONG**, 32767-step) or bit0 XOR bit6 (**SHORT**, 93-step metallic/pitched). Output ±1 zero-order hold between clocks.
- Clock: `RATE` (normalized 0–1 → 0–SR/2, log-ish taper), `KEYTRK` toggle (clock scales with note frequency relative to middle C — short-period + keytrack is a playable melodic voice).
- Own envelope; no PM input; **is a PM source** (matrix row NS — LFSR-modulated sines are a first-class texture, not an afterthought).
- LFSR seeds nonzero per voice; seed value is implementer's choice but must be deterministic per note for replay determinism.

## 7. Envelopes

- One ADSR per slot (3 ops + noise): A 1–2000 ms, D 5–3000 ms, S 0–1, R 5–4000 ms. Attack linear; decay/release exponential (one-pole toward target, τ such that segment completes in ~the stated time — match prototype constant 4.6).
- **LOOP:** while gated, cycle A→D→A→D (release from current level on note-off).
- **STEPPED** *(not in prototype)*: quantize envelope output to N levels, N ∈ {OFF, 2–16} — the chip "envelope on a timer tick" sound.
- Implement as engine-declared instances of the FOUNDATIONS envelope module; engine-local in UI, standard plumbing underneath.
- **Pitch envelope** (engine-global): amount ±24 st, decay 5–800 ms, exponential; multiplies all op frequencies and the keytracked noise clock.

## 8. Voice and note behavior

- Target polyphony 16 (prototype caps at 8); oldest-voice stealing with a short release-fade on the stolen voice.
- Per-voice state: 3 op phases, 4 previous-sample outputs, 4 envelope states, pitch-env timer, LFSR register + clock phase.
- Voice ends when all active slots' envelopes are done.
- Mono/legato/glide: **not engine-internal** — provided by HORDE/FOUNDATIONS glide plumbing (glide with inertia is the point).

## 9. House-tenet integration (FOUNDATIONS)

STATION carries the HORDE tenets lightly — three touchpoints, all via existing plumbing, none engine-internal logic:

1. **Inertia:** RATIO/PITCH/FIXED tuning params and PW are flagged inertia-eligible (mass-slewed). Ratio-glide through inharmonic territory is a signature gesture.
2. **Quantum-morph:** the 12 matrix cells + op levels form the primary morph surface (algorithm presets as corners). Wave RAM tables as corner data are exposed but morph logic stays in the morph system.
3. **One interdependent macro (`TIMBRE`):** an intermediate-model coupling of PM indices × PW × QNT depth, declared as a coupling-layer model per the FOUNDATIONS intermediate-model brief — a suggested default wiring, user-rewirable.

Other integration: all continuous params are mod-matrix destinations; per-op and engine-level scoped presets; Tonality intake for pitch input like every HORDE engine; **no adaptive state in v1** (declare zero-cost absence per the adaptive-state brief).

## 10. Parameter table

| ID | Name | Range / values | Default | Mod | Inertia | Notes |
|---|---|---|---|---|---|---|
| `op{n}.on` | On | bool | 1 | – | – | |
| `op{n}.wave` | Waveform | SIN TRI SAW PLS QTR DRW | SIN | – | – | |
| `op{n}.mode` | Tune mode | RATIO PITCH FIXED | RATIO | – | – | |
| `op{n}.ratio` | Ratio | 0.25–16 cont. | 1 / 2 / 14 | ✓ | ✓ | defaults per op |
| `op{n}.semis` | Pitch | ±24 st | 0 | ✓ | ✓ | |
| `op{n}.fine` | Fine | ±50 c | 0 | ✓ | ✓ | |
| `op{n}.fixed` | Fixed Hz | 20–4000 log | 220·2ⁿ⁻¹ | ✓ | ✓ | |
| `op{n}.lvl` | Level | 0–1 | .85/0/0 | ✓ | – | |
| `op{n}.pan` | Pan | ±1 | 0 | ✓ | – | |
| `op{n}.pw` | Pulse width | .05–.95 | .5 | ✓ | ✓ | snaps .125/.25/.5 |
| `op{n}.pure` | Pure↔raw | 0–1 | 1 | ✓ | – | |
| `op{n}.qnt` | Phase quant | OFF,4,8,16,32,64 | OFF | ✓ | – | stepped mod target |
| `op{n}.phase` | Phase offset | 0–360° | 0 | ✓ | – | |
| `op{n}.retrig` | Phase mode | RETRIG FREE | RETRIG | – | – | |
| `op{2,3}.sync` | Hard sync → OP1 | bool | 0 | – | – | |
| `op{2,3}.ring` | Ring partner | OFF ×OP1 ×OP2 | OFF | – | – | mix path only |
| `op{n}.env.*` | ADSR + LOOP + STEP | see §7 | see proto | ✓ (A/D/R) | – | |
| `ns.on/mode/rate/ktrk/lvl/pan/env.*` | Noise | see §6 | see proto | rate ✓ | – | |
| `mtx[src][dst]` | PM index ×12 | 0–8 | EP patch | ✓ | – | 5 ms smoothing |
| `penv.amt` | Pitch env amt | ±24 st | 0 | ✓ | – | |
| `penv.dec` | Pitch env dec | 5–800 ms | 80 | ✓ | – | |

Default patch = prototype boot patch (soft EP: OP2 2:1 idx 2.6, OP3 14:1 idx 1.1 → OP1).

## 11. Prototype parity and deliberate divergences

`reference/station.html` is the parity oracle for: waveform shapes (raw and pure branches), phase-quantization behavior, matrix/feedback semantics including the one-sample delay, envelope segment shapes and loop behavior, LFSR sequences (both taps), pitch-env curve, algorithm preset values, default patch.

**Deliberate divergences (do NOT replicate the prototype here):**

1. DRW pure branch: additive/mipmap band-limiting, not linear interpolation (§5).
2. RATIO continuous, not 0.5-stepped (§3.1).
3. Add: FREE phase mode, RING, STEPPED envelope mode (absent in prototype).
4. Polyphony 16 with release-fade stealing (prototype: 8, hard shift).
5. The prototype's master `tanh` drive is monitoring convenience only — engine output is clean; saturation belongs to the downstream chain.
6. ScriptProcessor/main-thread rendering is a browser sandbox workaround; the DSP core class structure (usable standalone) is the pattern to keep.

## 12. Performance budget and acceptance

- Per-voice: 3 ops × 2 render branches + noise + 4 envelopes; no allocations, no branches on denormals (flush-to-zero). Budget: full 16-voice poly ≤ ~2% of one core at 48 kHz on the reference machine — this engine must be nearly free next to the swarm.
- Acceptance: parity oracle passes on the §11 list; pluginval/auval clean inside HORDE host; matrix cells and all flagged params respond to global mod without zippering; wave RAM editable during sustained DRW notes without clicks; algorithm preset recall click-free; replay-deterministic (fixed seed ⇒ identical output).

---

## Appendix A — Downstream filter-bank recommendations (informative, out of scope)

Build order for HORDE's shared filter bank:
1. **TPT/ZDF state-variable filter** (Zavalishin formulation) — one core yields LP/HP/BP/notch/peak 12 dB, cascade for 24 dB; stable under audio-rate and stochastic modulation, which is non-negotiable given HORDE's mod systems.
2. **Nonlinear ZDF ladder** (Moog-style) with input drive, for character and screaming resonance.
3. Later: comb (cheap Karplus territory), formant/vowel pair (CHOIR synergy), one-pole tilt.
- Plumb **input drive and keytracking** into the bank interface from day one; retrofitting keytracking is painful.
