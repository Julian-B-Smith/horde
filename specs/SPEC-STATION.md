# STATION — Engine Specification

**Project:** HORDE (engine type)
**Status:** Approved for implementation
**Reference prototype:** `reference/station.html` (browser, Web Audio, validated by Julian 2026-08)
**Version:** 1.0
**Last amended:** 2026-09-19 — eight rulings the lead made on port phase 1's findings under the human's *"I'll go with whatever you recommend on the Station specs"* (ROADMAP B162): §12's budget is now the MEASURED 5.2 % and the ≤ 2 % estimate is retired; §11 item 9 gates diagonal parity by index; §3.2/§7 an OFF operator keeps running its envelope (prototype fixed); §11 item 10 declares the linear pan law intended; §7 states the envelope-time law exactly (the "163 % release" is a definition, not a defect); §4/§10 add the four levels to the 5 ms smoothed set; §3.4 ratifies FREE phase mode as built; §8/§10 add `op{n}.velSens`, default 0 and bit-inert.

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
                 DC BLOCKER (one pole/channel, 5 Hz)
                         ▼
                 engine stereo out → HORDE shared chain
                 (filter bank, FX, master — out of scope)
```

- **DC blocker on the engine stereo out.** One pole per channel on the level/pan
  sum, `y[n] = x[n] − x[n−1] + R·y[n−1]`, `R = exp(−2π·f_c/f_s)`, `f_c = 5 Hz`;
  state zeroed with the core. This is *not* the tone filter §1 rules out — it is
  a −3 dB-at-5-Hz highpass that leaves the audio band alone (measured: −0.008 dB
  at 100 Hz) and exists because several ordinary configurations carry real DC
  that is **envelope-multiplied at the source**, so each note-on is a thump and
  16 voices sum theirs: self-feedback −26 dB, SHORT noise −30 dB, a 10 % pulse
  −1.9 dB, all against a −61…−65 dB DC-free control. Human ruling 2026-09-19.

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

**`op{n}.on` silences the op; it does not suspend it.** An operator switched OFF mid-note contributes nothing to the mix and nothing to the matrix, **and its envelope keeps running**, so re-enabling it resumes at the live stage rather than at the level it held when it went off. `on` is an automatable parameter in the plugin, so a frozen envelope is reachable and audible: the prototype froze it (measured stuck at 0.32392 / stage 1 for 500 ms, then a 1.645e-3 step the instant the op returned — audit S9), and **the prototype was fixed on 2026-09-19 rather than the defect preserved**, so this is a parity item, not a divergence. This is the same rule the Nyquist mute already follows (§3.1: an operator muted for running above Nyquist still steps its envelope, so a pitch envelope sweeping back down finds a current level, not a stale one). *(Ruled by the lead 2026-09-19 under the human's "I'll go with whatever you recommend on the Station specs".)*

### 3.3 Pure↔raw continuum (per op)
Two render branches, crossfaded by `PURE` (0–1):

- **Pure branch (PURE=1):** band-limited. SAW/PLS via polyBLEP (BLAMP acceptable for TRI if aliasing is measurable); SIN/TRI analytic; QTR renders as smooth triangle; DRW renders band-limited (§5).
- **Raw branch (PURE=0):** naive rendering of the **phase-quantized** phase: `q = floor(p·QNT)/QNT` when `QNT > 1`, else the raw phase. `QNT ∈ {OFF, 4, 8, 16, 32, 64}` phase steps per cycle. Phase quantization is the chip-authentic degradation (distinct from any downstream bitcrush) and is a mod target.

Both branches are computed and crossfaded; at 3 ops this is cheap and it keeps the continuum artifact-free under modulation.

### 3.4 Phase, sync, ring
- Per-op phase offset 0–360°, with **RETRIG / FREE** mode (prototype is retrig-only at phase 0 — build adds both; a **port addition absent from the prototype**, cross-referenced at §11 divergence 3).
  - **RETRIG:** a new note starts the op at its phase offset. This is the prototype's behaviour at offset 0 and is the default.
  - **FREE:** the op runs a **free-running phase accumulator against middle C (261.63 Hz)** — the same reference `KEYTRK` already uses — advanced once per sample at the op's own tuning, and a new note starts at that accumulator's current value plus the op's phase offset. It is therefore **one accumulator per operator, not one per voice**: what "free-running" means here is that the oscillator does not restart, so two notes struck at different instants are at different phases and a chord does not stack coherently.
  - **Determinism is a requirement, not a side effect:** the accumulator is seeded and advanced by the note/sample stream alone — never from a wall clock, never from an unseeded draw (SPEC §5.7, CLAUDE.md's determinism invariant). Same seed + same note order ⇒ identical output.
  - The whole step is **skipped when every op is RETRIG**, which is the default patch and every parity scenario, so FREE costs nothing when it is off and is bit-inert by construction.
  - *(Ratified by the lead 2026-09-19 as built in port phase 1, under the human's "I'll go with whatever you recommend on the Station specs".)*
- **SYNC:** ops 2 and 3 may hard-sync to op 1's phase wrap (reset to their phase-offset value, not to 0).
- **RING** *(not in prototype)*: per-op partner select on ops 2/3 — `OFF | ×OP1 | ×OP2` — multiplying the op's post-envelope output by the partner's output before mix. Ring applies to the mix path only, not the matrix tap.

---

## 4. PM matrix

- 4 sources (OP1, OP2, OP3, NS) × 3 destinations (OP1–3). Op diagonal = self-feedback. NS has no destination column (noise receives no PM).
- Cell range 0–8 "index"; applied to phase as `p += cell · out_src / 2π`.
- **Every cell is a global-mod-matrix destination** (this is where quantum-morph and macros grab the routing).
- **Algorithm presets** are stored patches over (matrix values + op levels), nothing more. Ship the six from the prototype: `STACK`, `2→1`, `3→2→1`, `2+3→1`, `3→1+2`, `FB CH`. Preset recall must be click-free (control-rate smoothing, ~5 ms).
- **The ~5 ms smoothed set is the 12 matrix cells AND the four levels** — `op{1,2,3}.lvl` and `ns.lvl` — under one law. The cells alone are not enough: an algorithm preset *is* cells + levels, so smoothing half of a recall still clicks, and the prototype's unsmoothed LVL write measures **7.4× the signal's own inter-sample slope** against a 1.0× calibrated floor (audit S6 / §2.6), second only to a cell write's 13.9×. **The smoothing belongs to the BUILD, not to the prototype** (ADR-177 §3): `reference/station.html` stays step-valued and its harness pins those numbers, so the port has nothing to match here and the parity generator holds levels constant within a scenario. It is bit-inert for a static patch because each smoother primes on its target at the first render. Note what is *not* on the list: **waveform select cannot be smoothed at all** (11.1× measured) — it needs an equal-power crossfade or a zero-crossing switch, which is separate work. *(Ruled by the lead 2026-09-19 under the human's "I'll go with whatever you recommend on the Station specs".)*

---

## 5. Wave RAM

- One shared table per patch: **32 samples × 4-bit** (values 0–15), Game Boy wave-channel semantics — every op set to DRW reads the same RAM.
- **Raw branch:** zero-order hold on the quantized phase.
- **Pure branch — divergence from prototype:** the prototype linearly interpolates. The build must render band-limited via the table's exact spectrum: 32 real samples → ≤16 partials; either direct additive resynthesis or a per-octave mipmap rebuilt from those partials. Rebuild happens at control rate on edit (the table is user-drawable live); rebuild must be allocation-free and click-free.
- Factory presets: SIN, SAW, SQR, BELL, RND (match prototype generators).
- Table is preset-scoped and a candidate quantum-morph surface (corner tables) — expose it to the morph system but do not build table-morph logic into the engine.

## 6. Noise channel (NS)

- 15-bit LFSR, NES semantics: feedback = bit0 XOR bit1 (**LONG**, 32767-step) or bit0 XOR bit6 (**SHORT**, 93-step metallic/pitched). Output **±0.7** zero-order hold between clocks. *(Amended 2026-09-19 from ±1: the prototype has always rendered ±0.7 and ADR-003 makes the lab the reference, so the spec moves to it rather than the other way round — ADR-177 §3. The 3.098 dB gap scales every noise-as-PM index by 1.43, which is why it had to settle before any preset or golden existed.)*
- Clock: `RATE` (normalized 0–1 → 0–SR/2, log-ish taper), `KEYTRK` toggle (clock scales with note frequency relative to middle C — short-period + keytrack is a playable melodic voice).
- Own envelope; no PM input; **is a PM source** (matrix row NS — LFSR-modulated sines are a first-class texture, not an afterthought).
- LFSR seeds nonzero per voice; seed value is implementer's choice but must be deterministic per note for replay determinism.

## 7. Envelopes

- One ADSR per slot (3 ops + noise): A 1–2000 ms, D 5–3000 ms, S 0–1, R 5–4000 ms. Attack linear; decay/release exponential (one-pole toward target — match prototype constant 4.6).

**What the stated times MEAN — stated exactly, because "~the stated time" was not precise enough to settle a 163 % reading (audit S13).** The segment law is a forward-Euler one-pole, `lvl += (target − lvl) · 4.6/(T·f_s)` where `T` is the stated time in seconds, and **4.6 = ln 100**. So:

| segment | what the stated time means | exit | measured |
|---|---|---|---|
| **A** | linear ramp; reaches 1.0 in exactly A | `lvl ≥ 1` | — |
| **D** | closes 99 % (**−40 dB**) of the gap from the current level to S in exactly D | absolute `\|lvl − S\| < 0.004` | 102.7 % of D for 1.0→0.55; 120.0 % for 1.0→0 |
| **R** | falls to 1 % (**−40 dB**) of the level held at note-off in exactly R | absolute `lvl < 0.0005` | **100.10 %** of R to the −40 dB point at 48 kHz |

The **audible tail is longer than R, and by a knowable amount**: the segment continues past −40 dB to the absolute floor `lvl < 0.0005` (≈ −66 dBFS of envelope scale), so the tail runs for `ln(lvl₀/0.0005)/4.6 · R`, where `lvl₀` is the level at note-off. Measured at REL = 260 ms, 48 kHz: **152.2 % of R from the default sustain 0.55**, **162.8 % from 0.896** (the reading audit S13 reported as "163 %"), **165.2 % from 1.0**, matching the closed form to four decimals at every point.

**This is a DEFINITION, not an arithmetic defect, and nothing is changed.** It was checked against the alternative before being written down: there is no wrong sample-rate or units factor — the −40 dB point lands at 100.10 % of the stated time (the 0.10 % is the forward-Euler discretisation of the exponential, not an error in the constant), and the tail length is sample-rate portable to **0.007 %** across 44.1 / 48 / 96 kHz. So the release **feel is unchanged** by this ruling; what changes is that a patch designer can now compute the tail instead of being surprised by it. *(Ruled by the lead 2026-09-19 under the human's "I'll go with whatever you recommend on the Station specs".)*
- **LOOP:** while gated, cycle A→D→A→D (release from current level on note-off).
- **STEPPED** *(not in prototype)*: quantize envelope output to N levels, N ∈ {OFF, 2–16} — the chip "envelope on a timer tick" sound.
- Implement as engine-declared instances of the FOUNDATIONS envelope module; engine-local in UI, standard plumbing underneath.
- **An envelope runs whether or not its slot is audible.** `op{n}.on = 0` and the Nyquist mute both silence the operator and leave its envelope stepping (§3.2), so a slot re-enabled mid-note resumes at the live stage. The only thing that stops an envelope is the voice ending.
- **Pitch envelope** (engine-global): amount ±24 st, decay 5–800 ms, exponential; multiplies all op frequencies and the keytracked noise clock.

## 8. Voice and note behavior

- Target polyphony 16 (prototype caps at 8); oldest-voice stealing with a short release-fade on the stolen voice.
- **Velocity.** A note carries a velocity `vel` ∈ [0, 1]; each operator responds to it by its own `op{n}.velSens` ∈ [0, 1], scaling the operator's output by **`1 − velSens·(1 − vel)`**. So `velSens = 0` is velocity-deaf (and, being a multiply by exactly 1.0, is bit-identical to an engine with no velocity at all — that is the default and it is what keeps every existing patch and golden unchanged), and `velSens = 1` makes the operator's level **proportional to velocity**. The scale is applied **at the source, beside the envelope**, exactly as §2 describes for the envelope itself — so it shapes the slot both as a carrier and as a modulator. That placement is the whole point: **a modulator op with `velSens > 0` is how velocity reaches TIMBRE in PM**, and a mix-only scale could never do it, because a pure modulator has `lvl = 0`. Velocity is per note and fixed at note-on; it is not a mod-matrix destination (the matrix already reaches `lvl` and the cells). *(Ruled by the lead 2026-09-19 under the human's "I'll go with whatever you recommend on the Station specs".)*
- Per-voice state: 3 op phases, 4 previous-sample outputs, 4 envelope states, pitch-env timer, LFSR register + clock phase, note velocity.
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
| `op{n}.lvl` | Level | 0–1 | .85/0/0 | ✓ | – | **5 ms smoothing** (§4) |
| `op{n}.velSens` | Velocity sens | 0–1 | **0** | ✓ | – | ×3, one per op. Scales the op **at the source** by `1 − velSens·(1 − vel)` (§8); 0 is bit-inert |
| `op{n}.pan` | Pan | ±1 | 0 | ✓ | – | **linear law** — also a 3 dB gain control at hard pan (§11 item 10) |
| `op{n}.pw` | Pulse width | .05–.95 | .5 | ✓ | ✓ | snaps .125/.25/.5 |
| `op{n}.pure` | Pure↔raw | 0–1 | 1 | ✓ | – | |
| `op{n}.qnt` | Phase quant | OFF,4,8,16,32,64 | OFF | ✓ | – | stepped mod target |
| `op{n}.phase` | Phase offset | 0–360° | 0 | ✓ | – | |
| `op{n}.retrig` | Phase mode | RETRIG FREE | RETRIG | – | – | |
| `op{2,3}.sync` | Hard sync → OP1 | bool | 0 | – | – | |
| `op{2,3}.ring` | Ring partner | OFF ×OP1 ×OP2 | OFF | – | – | mix path only |
| `op{n}.env.*` | ADSR + LOOP + STEP | see §7 | see proto | ✓ (A/D/R) | – | |
| `ns.on/mode/rate/ktrk/lvl/pan/env.*` | Noise | see §6 | see proto | rate ✓ | – | `ns.lvl` carries **5 ms smoothing** (§4); `ns.pan` is the same linear law as `op{n}.pan` |
| `mtx[src][dst]` | PM index ×12 | 0–8 | EP patch | ✓ | – | 5 ms smoothing |
| `penv.amt` | Pitch env amt | ±24 st | 0 | ✓ | – | |
| `penv.dec` | Pitch env dec | 5–800 ms | 80 | ✓ | – | |

Default patch = prototype boot patch (soft EP: OP2 2:1 idx 2.6, OP3 14:1 idx 1.1 → OP1), with `velSens = 0` on all three ops so the default is velocity-deaf and bit-identical to the pre-velocity engine.

*Rows added / amended 2026-09-19 (ROADMAP B162, ruled by the lead under the human's "I'll go with whatever you recommend on the Station specs"): the three `op{n}.velSens` rows; the 5 ms smoothing note on `op{n}.lvl` and `ns.lvl`; the pan-law note.*

## 11. Prototype parity and deliberate divergences

`reference/station.html` is the parity oracle for: waveform shapes (raw and pure branches), phase-quantization behavior, matrix/feedback semantics including the one-sample delay, envelope segment shapes and loop behavior **including an OFF operator's envelope continuing to run** (§3.2), LFSR sequences (both taps), pitch-env curve, algorithm preset values, default patch, the **velocity law** `1 − velSens·(1 − vel)` (§8), **and the engine-output DC blocker** (item 7). It is the oracle **only where it is contractive** — see item 9.

**Deliberate divergences (do NOT replicate the prototype here):**

1. DRW pure branch: additive/mipmap band-limiting, not linear interpolation (§5).
2. RATIO continuous, not 0.5-stepped (§3.1).
3. Add: FREE phase mode (defined in full at §3.4 — a port addition, ratified 2026-09-19 as phase 1 built it), RING, STEPPED envelope mode (absent in prototype).
4. Polyphony 16 with release-fade stealing (prototype: 8, hard shift).
5. The prototype's master `tanh` drive is monitoring convenience only — engine output is clean; saturation belongs to the downstream chain.
6. ScriptProcessor/main-thread rendering is a browser sandbox workaround; the DSP core class structure (usable standalone) is the pattern to keep.

*Items 7-10 are parity notes, not divergences: one thing the port must copy (7), one it need not (8), and two rulings of 2026-09-19 about what parity can and cannot certify here (9, 10).*

7. The **DC blocker IS a parity item** (added 2026-09-19 by human ruling on ROADMAP B153/suite S17; it differs from the reverb precedent, where the blocker lived only in the port). Port it exactly: same form `y[n] = x[n] − x[n−1] + R·y[n−1]` with `R = exp(−2π·f_c/f_s)` and `f_c = 5 Hz` (named in Hz and derived from the running sample rate — never a hand-tuned per-tick constant, ADR-009), in the same **position** — after the level/pan sum of ops + noise, before the master gain and the prototype's monitoring `tanh` (divergence 5 above), so the port's clean output carries the identical stage; two states (x₁, y₁) per **channel**, not per voice, zeroed wherever the core is reset. The tail is flushed to exact zero below 1e-30 so the filter cannot idle in the subnormal range once the last voice dies (§12's flush-to-zero requirement; suite S15). Gated by `tools/labharness/station_check.mjs` S17 (every configuration at the DC-free detector floor, with the pre-blocker lab as the must-fail control) and S21 (−3.0074 dB measured at 5 Hz, −0.0080 dB at 100 Hz).
8. The LFSR **seed derivation** is not a parity item. The prototype derives it per voice from the patch seed and the voice slot (`mulberry32(seed ^ slot·2654435761)`, forced odd — ADR-177 §3, 2026-09-19; it was a constant `0x7FFF` for every voice, which summed a chord's noise coherently at +12 dB for 16). §6 leaves the value to the implementer, so the port may choose its own derivation; what IS gated is the rule (nonzero, per voice, deterministic per note), the periods (32767 / 93), and an N-voice/1-voice noise RMS ratio of ~√N rather than N.

9. **Parity on the self-feedback diagonal is gated BY INDEX, because above index ≈ 2 the map is chaotic and bit-parity is impossible by construction.** On the diagonal (op → self) the sample recurrence is `out = wave(phase + cell·out_prev·0.1591549)`, whose derivative `d(out)/d(out_prev) ≈ cell·cos(·)` is contracting below index 1 and expanding above it; V8's `Math.sin` and the platform libm's part company by ~1 ulp somewhere in range, and a chaotic map amplifies that last bit without bound. Measured RMS of the port against the lab, 48 kHz / 44.1 kHz (phase 1, 2026-09-19): index 0.9 `0 / 0`; 1.0 `0 / 0`; 1.2 `1.585e-7 / 3.232e-8`; 2.0 `1.257e-7 / 2.408e-8`; **4.0 `2.091e-1 / 2.084e-1`; 8.0 `3.085e-1 / 3.031e-1`** — five orders of magnitude in one step, which is the signature of the regime, not of a port bug. Therefore: **the parity oracle pins the diagonal at index ≤ 2** (8× under ε at the loudest parity-able setting), and **index 8 is gated BEHAVIOURALLY** — bounded, non-silent, stable (`|op| ≤ 1.000001` over 10 s on all three diagonals), and of the right spectrum class. This is the oracle-kinds rule stated for this engine: **parity where the map is contractive, invariants where it is not**. It supersedes the lab audit's §4.1 claim that bit-parity is available "for the whole DSP", which is false here. No lab change; the ladder is recorded in `tools/golden/gen_station_goldens.mjs` so nobody "improves" the scenario back to index 8. *(Ruled by the lead 2026-09-19 under the human's "I'll go with whatever you recommend on the Station specs".)*
10. **The pan law is LINEAR, and that is intended — declared, not fixed.** `L += g·(1 − max(0, pan))`, `R += g·(1 − max(0, −pan))`: full level at centre, hard L/R at the extremes, and therefore **−3.01 dB of total power at hard pan** (measured, audit §2.8). Constant-power (sine) panning is the usual answer and is **not taken**: it attenuates the centre by 3 dB, so adopting it would drop every centred default — which is every factory op and every algorithm preset — by 3 dB, and what the human has been listening to and approving is this law. The same argument settled the ±0.7 noise amplitude (ADR-177 §3): when the lab and the abstraction disagree about a level the human has already heard, the lab wins. The consequence is stated rather than hidden: **panning is also a gain control**, so a stereo-width macro across the four slots is simultaneously a loudness macro, and any macro design must account for it. No lab change; the port already matches. *(Ruled by the lead 2026-09-19 under the human's "I'll go with whatever you recommend on the Station specs".)*

## 12. Performance budget and acceptance

- Per-voice: 3 ops × 2 render branches + noise + 4 envelopes; no allocations, no branches on denormals (flush-to-zero).
- **Budget — MEASURED, and the previous number is retired.** The port costs
  **5.2 % of one core** at 16 voices, max patch, 48 kHz (phase 1, 2026-09-19;
  standalone core, min of three 5-second renders, the reference Mac / Apple
  Silicon; `tools/station_check` reports it every run). The **≤ ~2 % written
  here before was an ESTIMATE, never a measurement, and is retired** — a
  specification does not get to assert a number nobody took (CLAUDE.md: "acceptance
  numbers are measured, not aspirational"). The budget is **re-set from the
  shell measurement** — `measure_cpu` with a STATION column, the same instrument
  every other engine is held to — once phase 2 lands the engine in the shell;
  a standalone-core figure and an in-shell figure are not the same quantity and
  only the second is what a host pays. Reducing the 5.2 % is ROADMAP **B162**'s
  optimisation queue item, not a blocker on phase 2; the two named hot spots are
  the per-voice pitch-envelope `pow`/`exp` and the Wave-RAM mipmap read. The
  reading is load-sensitive (the same binary read 9.04 % on a busy machine and
  5.12–5.21 % idle), so a single sample is a sample, not the figure.
  *(Ruled by the lead 2026-09-19 under the human's "I'll go with whatever you
  recommend on the Station specs".)*
- Acceptance: parity oracle passes on the §11 list; pluginval/auval clean inside HORDE host; matrix cells and all flagged params respond to global mod without zippering; wave RAM editable during sustained DRW notes without clicks; algorithm preset recall click-free; replay-deterministic (fixed seed ⇒ identical output).

---

## Appendix A — Downstream filter-bank recommendations (informative, out of scope)

Build order for HORDE's shared filter bank:
1. **TPT/ZDF state-variable filter** (Zavalishin formulation) — one core yields LP/HP/BP/notch/peak 12 dB, cascade for 24 dB; stable under audio-rate and stochastic modulation, which is non-negotiable given HORDE's mod systems.
2. **Nonlinear ZDF ladder** (Moog-style) with input drive, for character and screaming resonance.
3. Later: comb (cheap Karplus territory), formant/vowel pair (CHOIR synergy), one-pole tilt.
- Plumb **input drive and keytracking** into the bank interface from day one; retrofitting keytracking is painful.
