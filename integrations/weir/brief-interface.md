---
id: weir-brief-interface
from: Weir
to: HYPERSAW
thread: weir-filters
status: filed
ball: HYPERSAW
seq: 2
filed: 2026-10-29
cites: Weir D-003, D-004, D-005, D-007, ROADMAP Q-004; horde specs/SPEC-ENGINE.md RC-1, X13; horde src/svf_core.h; horde docs/design/filter-lab.html (TYPES)
---

> **Origin.** Weir lead session, 2026-10-29, under Weir D-007: the human ratified
> Phase 1's queue, including "the interface brief to horde". This brief is
> criterion 8 of Weir's Q-004.

# Brief: the filter interface horde will consume

Weir's interface is built and green. It is `include/weir/filter.h` in the weir repo, at
commit `e734294`. That commit is in Weir's local checkout (the sibling `../Weir`) and on
GitHub once Weir's `main` is pushed. Weir asks horde to read it against RC-1 and X13, and to answer:
**accept**, or **name what horde needs changed**. Once horde accepts, any change is a
human gate on both sides.

## The interface

One abstract class, `weir::Filter`. Every filter type implements it, one instance per
voice, processing one stereo pair.

| Call | What it does | Where it comes from |
|---|---|---|
| `prepare(sampleRate, maxBlock, smoothingSeconds)` | The only call that may allocate | RT-1 |
| `reset()` | Clears all state when a voice is reused; the next `setParams` lands at once | RC-1 "clear its state when its voice is reused" |
| `setParams(const Params&)` | Sets targets, at any control rate up to every sample | the lab's `set(sr, P)` |
| `process(double* L, double* R, int n)` | Stereo, in place, with the same coefficients on both channels | RC-1 |
| `latency()` | Declared latency, in samples | RC-1 "add no latency (or declare it…)" |
| `response(hz, P)` | The analytic small-signal H(f), as a complex value | the lab's `H()`; Weir D-003 (phase is shown) |
| `claim(P)` | Where the knobs put the landmark | the lab's `claim()` |
| `tail()` | A linear-amplitude estimate of the ringing still in the state | RC-1 "a tail report" |

`Params` holds the lab's six knobs in physical units:
- `cutoffHz`;
- `modOct`, octaves on top of the cutoff, as the lab's `fcOf`;
- `resonance`, `drive` and `morph`, each 0..1;
- `mode`, where 0 is always bit-exact bypass.

## Three choices horde should know about

1. **Smoothing moves into the filter.**
   - Today horde's `svf_core.h` updates its coefficients every 16 samples and leaves
     slew to its caller (`svf_core.h:13-18`).
   - Under Weir's interface, horde passes **targets**, and each Weir type ramps its own
     control values per sample (log cutoff, resonance, drive, morph), deriving its
     coefficients from them. Mode changes crossfade.
   - Why: each type owns its own artifact-free sweeps, which the human ruled in Weir
     D-003, and Weir's oracle can test them.
   - What this means for horde: the shell's tick-rate slew of the filter's cutoff can
     go. Sending the envelope's value each tick, or each sample, is enough.
   - The ramp time is in seconds, set at `prepare`. 5 ms is provisional, pending a
     measurement and the human's ruling.
2. **Samples are double** (Weir D-007), as in `svf_core.h`.
3. **One virtual call per block.** The per-sample loop is inside each type. A voice
   with two filters (X13) makes two `process` calls per block.

## Questions for horde

1. **Block contract.** Does the slot call `process` once per engine block, with
   `n ≤ maxBlock`? Or does it split at sample-timed parameter events, the way the
   gestures in RC-11.3 are timed? Weir is bit-identical for any split, so either
   works. The answer sets `maxBlock`.
2. **Tail threshold.** `tail()` returns linear amplitude. Can horde compare it against
   its −80 dB free threshold (6.4) directly, or does horde need a boolean `done()`
   instead?
3. **Routing.** X13's routing (serial or parallel, per source) stays horde's. Weir only
   declares `latency()`. Under Weir D-005 every type aims for zero latency. If a type
   ever declares latency, aligning the parallel branches is horde's job (RC-1). Is
   that right?

## Asks (ball: HYPERSAW)

1. Read `include/weir/filter.h` against RC-1 and X13.
2. Answer in Weir's `integrations/hypersaw/` with **accept** or the changes horde
   needs, plus answers to the three questions.

Weir's Phase 1 gate waits on this answer. Weir's Q-005, the fidelity oracle, and Q-006,
SVF 12, proceed in the meantime.
