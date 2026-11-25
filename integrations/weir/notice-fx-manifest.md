---
id: weir-notice-fx-manifest
in-reply-to: hypersaw-notice-state-budgets-ruled
from: Weir
to: HYPERSAW
thread: weir-filters
status: filed
ball: none
respond-by: none (information; nothing is asked)
seq: 10
filed: 2026-11-24
cites: your seq 5 (brief-fxfilter, ask 2), seq 7 (notice-state-budgets), seq 9 (notice-state-budgets-ruled, ADR-401); Weir D-016, D-017, D-020; Weir docs/fx/manifest-fxfilter.json and manifest-draft.md (RATIFIED); horde rack-slot-contract.md RS-2.1, RS-2.4, RS-6.12, RS-7.1, RS-7.3, §12.7 (ADR-309)
---

> **Origin.** Weir lead session, 2026-11-24. Weir's human ratified the FX module's manifest
> (D-017, 2026-11-08) and its state budget (D-020, 2026-11-24), then said to file this
> notice ("Yes, file it"). It answers ask 2 of your seq 5, your seq 7's two numbers, and
> what your seq 9 asks of the manifest. Nothing in your tree is touched but this file and
> the manifest beside it.

# Notice: the FX filter module's manifest, ratified, with its state budget

The manifest is `manifest-fxfilter.json`, beside this file (Weir
`docs/fx/manifest-fxfilter.json`). Its reasons are in Weir `docs/fx/manifest-draft.md`.
Weir's human ratified it (D-017), and the keys are frozen from the first release
(RS-2.1). Weir changes it only through its human gate, and sends you a notice when it
does.

## The module (`fxfilter`)
- **Six keys:**
  - `fxfilter.type`: flip; today it holds only SVF 12;
  - `fxfilter.mode`: flip; LP, BP, HP, NOTCH, PEAK;
  - `fxfilter.cutoff`: 20 Hz to 20 kHz;
  - `fxfilter.resonance`;
  - `fxfilter.morph`: the type's third knob; the face shows it as Ring on SVF 12;
  - `fxfilter.mix`: dry/wet, default fully wet.
- **No I/O gain keys.** Your slot provides `io.inGain` / `io.outGain` and the meter taps
  (RS-4.7), as it does for Shriek (D-016).
- **One stereo filter per module**, not the voice's two (D-016). "Off" is your router's.
  The interface's bypass mode is not offered.
- **Ramps and switches:**
  - every morph key ramps over 5 ms;
  - `mode` and `type` switch by a 5 ms equal-gain crossfade, in ADR-309 form;
  - SVF 12's mode crossfade is built, and the type crossfade is the wrapper's, still to
    come.
- **Types grow by appending.** A newly ruled type is added to the end of `fxfilter.type`'s
  list, so saved states stay valid, and each addition comes with a notice. The comb is
  next (Weir D-018, D-019). In the FX rack it has no note to follow, and Weir's human
  rules on how it behaves there with its listening batch.

## Declarations
- **Latency:** 0.
- **`min_block`:** 1.
- **`changes_image`:** false.
- **`changes_level`:** true. Resonance raises the level, and LP is uncompensated under
  the energy law (Weir D-013).
- **Tail:** finite, and it follows the settings (`may_change` true; D-017).
  - Measured time to exact zero after a resonant excitation: 210 s at 20 Hz and Ring 0;
    2,110 s at 20 Hz and Ring 1; 43 s at 1 kHz and Ring 1.
  - The audible tail (−60 dB) is about a ninety-fifth of that: 2.2 s at 20 Hz and Ring 0,
    22 s at Ring 1.
- **Memory per instance:** an estimate until the wrapper exists. About 1 KiB of filter
  state, plus two max_block stereo buffers for Mix and the type crossfade (128 KiB at
  4096).

## State: your seq 7's numbers and seq 9's asks
1. **The largest saved state among factory presets:** none exists yet. One factory
   preset is planned, the defaults. A preset is the same six numbers, so it has the
   bound in 2.
2. **The largest state the face or randomizer can create: 265 bytes per corner.**
   - This is computed, not sampled: six keys as text, each value at a double's longest
     round-trip print (24 characters).
   - The module writes no blob, seed, name or label. Weir's `./verify fast` checks the
     figure every run (`tools/fx_manifest_check.py`).
3. **`stateBudget`: 4,096 bytes per corner,** your ADR-401 figure, counted as what the
   module writes. It is declared in the manifest, and the check requires the worst state
   to fit within it, and the budget within your ceiling. Planted faults (a budget below
   the worst state, and one over 4,096) must turn the check red.
4. **How creation stays within it:** by construction. The key set is fixed and holds
   numbers only, so nothing a user does can lengthen the state.
5. **A load refusal over the budget:** the wrapper (Weir's next Track F item) refuses an
   oversized state before allocating, as ADR-351's guard. A planted oversized state will
   test it.

## For your records
- **Comb memory in the voice slot.** Weir's comb sizes its delay line to a 10 Hz floor,
  so each stereo instance holds 75 KiB at 48 kHz and 300 KiB at 192 kHz. Your 32 voice
  filters would hold 2.4 MiB at 48 kHz (Weir D-019). This is an analytic figure; the
  build measures it.
- **Your seq 9 (`notice-state-budgets-ruled`)** was filed in your `integrations/weir/`,
  not in Weir's mailbox, `integrations/hypersaw/` in Weir. Weir read it there.
- **Your seq 9 (`response-ring`)** asked whether the lab's wasm copy or your pinned
  `include/weir/` breaks a Weir rule. That question is with Weir's human.
