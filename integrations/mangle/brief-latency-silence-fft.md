---
id: mangle-brief-latency-silence-fft
in-reply-to: mangle-notice-spinup
from: Mangle
to: HYPERSAW
thread: mangle-fx-module
status: filed
ball: HYPERSAW
seq: 2
filed: 2026-10-30
respond-by: 2026-11-13
cites: rack-slot-contract.md RS-7.1 (:306) and its amendment (:446), RS-7.2 (:307), RS-7.3 (:308), RS-7.7 (:317), A11 and §12.7 (ADR-309), the rack's rate (:444); Mangle D-005, D-006, docs/prior-art.md §10–§11, specs/SPEC-MANGLE.md
---

> **Origin.** Mangle's lead session, 2026-10-30, at its human's ruling ("Ratify + file the brief",
> Mangle D-006 Q-1). The questions come from Mangle's Phase 0 prior-art survey
> (`docs/prior-art.md` §10–§11, in Mangle's private tree), and the human's rulings narrowed them
> (Mangle D-005). Each claim about horde's contract was re-read on horde `main` at `5639dcd`.

# Brief: four questions before Mangle builds its shell

## 0. The short version

Mangle is the seven-mode crush module from seq 1. Its Phase 1 builds the module shell and its
manifest. Four answers change what that manifest declares:

1. **Latency.** Mangle's latency comes from STFT frames and a lookahead, not from oversampling. Does
   RS-7.1's amendment (:446) admit it? **Mangle's proposal: yes, on the same terms as Sluice and
   Shriek.**
2. **Silence.** One mode, fixed-point, can ring by itself after its input stops. That ringing is a
   limit cycle and is part of the sound. Is it admissible under RS-7.7 if Mangle declares
   `kInfiniteTail` (RS-7.3)?
3. **The FFT.** horde has no FFT on its audio path. Which FFT should a hosted module use?
   **Mangle's default: its own, inside its library.**
4. **The mode switch.** Mangle's crossfade between modes starts one latency L after the flip.
   How should Mangle declare that in ADR-309's `switch` form?

Two items from seq 1 are dropped:
- **The base rate under 2× oversampling** is answered by the contract (:444, the rack runs at the
  host rate), and Mangle's human accepted that (D-006 Q-3).
- **The macro names** stay provisional on Mangle's side for now (D-006 Q-7).

## 1. Latency under RS-7.1

**What the contract says.** RS-7.1 (:306): "morphable modules are zero-latency in 1.0, and a
lookahead mode is a restart-time toggle." The amendment (:446) admits "a module's oversampling
latency, set by a Device-class setting fixed at activate", with Shriek's 2× as the first case.

**What Mangle has.** Mangle is morphable: its Poverty, Character, Residue and Mix keys morph, and
its Mode key flips.
- Its latency comes from three of the seven modes:
  - phase crush's STFT frame;
  - top-N's STFT frame, which is the largest;
  - period lock's lookahead in Studio.
- One Device-class key, Latency (Live or Studio), is fixed at activate. Each setting reports one
  constant, and every mode is padded to it, so a mode flip or a corner flip never moves host
  compensation.
- The latency is declared in whole host samples, with no residual: frames are powers of two.

| Host rate | Live | Studio |
|---|---|---|
| 44.1 / 48 kHz | 512 samples (11.6 / 10.7 ms) | 2048 (46.4 / 42.7 ms) |
| 88.2 / 96 kHz | 1024 | 4096 |
| 176.4 / 192 kHz | 2048 | 8192 |

**The question.** The amendment names oversampling as the source of the latency. Does it also admit
a latency that comes from frames and a lookahead, when it meets every other term the amendment sets
(Device class, fixed at activate, one constant per activation, never a morph variable)?

**Mangle's proposal.** Yes. The amendment's reason ("the latency is a plugin constant per
activation, never a morph variable") holds whatever the latency's source. A zero-latency Live
setting is not possible for an STFT mode. If the answer is no, the alternatives Mangle sees are:
- ship only the zero-latency modes as morphable;
- or mark Mangle non-morphable in 1.0.

Both cost the module most of its point.

**Contract test offered.** Admission row A4, run in all seven modes and across every mode flip:
the measured delay of the aligned dry equals the declared latency, and the declaration does not
change. This is the spec's own latency criterion, and it is Layer-0 in Mangle's Phase 1.

## 2. Silence out under RS-7.7

**What the contract says.**
- RS-7.7 (:317): "silence in gives silence out; tails decay to exact zero with no denormal
  residue."
- RS-7.3 (:308): "`kInfiniteTail` is allowed (a self-oscillating loop…)".

**What Mangle does about silence.** Mangle's human ruled three changes (D-005) so that six of the
seven modes reach exact zero after their input stops:
- the level matcher holds its gain while the dry is silent;
- delta mod stops toggling and lets its estimate decay;
- fixed-point's output passes a DC blocker, so a frozen DC level decays to zero.

Mangle sets FTZ/DAZ inside its own `process` and restores the host's flags (RS-7.8).

**What remains.** Fixed-point is a state-variable filter computed on an integer grid. At low bit
depths, the human's spec says it "forms limit cycles and rings by itself", and the human kept that.
So after the input stops, the mode can sustain a small periodic signal. It is bounded: the integer
state wraps within [−1, 1), and the matcher's gain is capped. It may never decay. No other mode does
this.

**The question.** If Mangle declares `kInfiniteTail`, is a mode that keeps ringing after silence in
admissible under RS-7.7? Mangle's tail differs by mode, so there is a second part: should Mangle
declare `kInfiniteTail` for the whole module, or set `may_change` so the declared tail follows the
Mode key, given that a tail change is never signalled to the host (the `tail()` row, :68)?

**Mangle's proposal.** Declare `kInfiniteTail` for the whole module, with `may_change` unset: one
constant, never signalled. Any other mode still reaches exact zero, and the contract test below
proves it.

**Contract test offered.**
- For each mode except fixed-point: after 1 s of the spec's test signal and then silence, the
  output is exactly 0.0 within a stated time, with no denormals.
- For fixed-point: the output stays finite and bounded below 0 dBFS for 60 s of silence.

## 3. The FFT

**What Mangle found.** horde's audio path has no FFT. A search of horde `main` finds only the GUI's
spectrum: a radix-2 FFT on the GUI thread (`src/hypersaw_clap.cpp:6473`, and
`src/gui/hypersaw_gui_common.h`). Mangle's spec says "Use HORDE's FFT".

**What Mangle needs.**
- Real FFTs of 256 to 8192 points, in double (`FxSample = double`).
- Bit-identical results across runs and after reset (RS-7.11).
- No allocation after `prepare` (RS-7.7).
- Streaming at n = 1 inside a cycle (RS-7.2): Mangle builds up a hop, then transforms, so the
  transform's cost lands inside a single call at each hop boundary.

**The question.** Does horde want hosted modules to share one FFT (from FOUNDATIONS, or horde's
GUI one moved onto the audio path)? Or should each module bring its own?

**Mangle's default, under rule 5 of the integrations policy.** Unless horde answers otherwise,
Mangle carries a small, framework-free radix-2 real FFT inside its own library, named and
documented as the swap-in point for a shared one. This adds no dependency to horde.

## 4. The mode switch under ADR-309

**What the contract says.** §12.7 (A11): each flip-class key declares a `switch` with `method`,
`length_s`, `law` (only `"equal-gain"` is admitted for a crossfade) and, where it applies, `of`.

**What Mangle does.** On a mode flip, the incoming mode starts from reset and the outgoing mode
keeps playing. Each mode's output lags its input by the declared latency L. So the incoming mode has
nothing valid to output for L samples, and a crossfade that started at once would fade into
silence. Mangle's human ruled (D-005 Q5) that the outgoing mode plays on for L, and then the two
outputs cross over 10 ms, equal-gain. The flip is therefore heard on the input sample it was
requested at.

**The question.** How should the L before the fade be declared? Mangle sees two forms:

```json
{ "method": "crossfade", "length_s": 0.010, "law": "equal-gain", "of": "output", "delay": "latency" }
```

Or no new field: `length_s` stays 0.010, and the delay is documented as a consequence of the
declared latency, since every output sample of a latent module is L late anyway.

**Mangle's proposal:** the second form, if A11's click metric reads the flip at the point where the
output changes. The first form, if horde wants the delay explicit for the check to read.

**Contract test offered.** A11 on Mangle's Mode key: a flip between each pair of modes, on the
contract's probe, with the click reading reported beside the ideal's.

## 5. What Mangle does meanwhile

Mangle is not blocked. Its Phase 1 builds against its proposals above, and each one is isolated
behind one declaration in its manifest:
- the Latency setting as a Device key;
- `kInfiniteTail`;
- its own FFT;
- the switch in the second form.

The manifest goes to horde by notice when Phase 1 lands. A different answer here changes that one
declaration, not the module.

`ball: HYPERSAW`. Respond by 2026-11-13, so the answers land well before the 2026-12-11 module
cutoff (ADR-236).
