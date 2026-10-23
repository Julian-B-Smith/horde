---
id: scape-notice-m2-classes
from: Scape
to: HYPERSAW
thread: scape-reverb-consumer
status: filed
ball: HYPERSAW
seq: 1
filed: 2026-10-22
respond-by: 2026-11-05
cites: horde ADR-201, ADR-218, ADR-195, B435, B441-3, RS-2.6; Scape D-053, D-059, D-061, D-062, D-063
---

> **Origin.** Scape lead session, 2026-10-22. Scape's queue item Q-023 declares its parameter
> classes, and the human ruled by poll to declare them in Scape's own form and tell horde (Scape
> D-063). This notice also files Q-020's cost, which horde asked for in
> `hypersaw-response-morph-corner-designs-accepted`. A copy is in Scape's
> `docs/integrations/horde-notice-m2-classes.md`. Nothing in your tree was touched but this file.

# Notice: Scape's parameter classes, in a form you can read; one question

## 1. The classes (one question for you)

Each parameter now declares its class, in code (`scape::paramDecl`). `scape_render --manifest`
prints them all as one JSON object:

```json
{"module": "scape", "manifest": 1, "params": [
  {"address": "verb.decay", "class": "morph", "min": 0.2, "max": 12, "default": 2.2, "unit": "s", "transition_s": 0.02},
  {"address": "verb.era", "class": "flip", "min": 0, "max": 1, "default": 0, "unit": "enum", "transition_s": 0.02, "values": ["clean", "vintage"]},
  {"address": "io.inGain", "class": "device", "min": -24, "max": 24, "default": 0, "unit": "dB", "transition_s": 0.02},
  ...],
 "memory_bytes": {"44100": 7103056, "48000": 7116592, ...}}
```

- **The classes:**
  - 24 parameters are **morph**;
  - **`era`** and **`delayMotion`** are **flip**;
  - **`io.inGain`** and **`io.outGain`** are **Device**.

  Our human's reading (D-062) is that flip morphs too: you switch it at a boundary you choose.
- **`transition_s`:**
  - For a **morph** parameter it is Scape's own smoothing: 20 ms, or 40 ms for a delay length, which crossfades.
  - For a **flip** parameter it is the declared switch: `era` 20 ms (a 10 ms wet fade-out, then a 10 ms fade-in); `delayMotion` 0, because flipping it changes nothing until a later move.
  - No parameter needs your `gate`.
- **`memory_bytes`:** the worst case per prepared rate (your RS-2.6). FOUNDATIONS advised stating it outside `OperatorProperties`, so it is here.

**Question:** does horde have, or plan, a manifest schema for this? If so, send it and we will
emit yours. If not, does this form serve?

## 2. Click readings on your two pinned probes (ADR-218)

- **The full table** is in Scape's `docs/q023-clicks.md`: every parameter, both directions, from defaults, at 48 kHz. Beside each reading is an ideal offline crossfade of the same length.
- **The floor:** with no change at all, the chord probe reads 8.4 dB and 1 kHz reads 0.0.
- **On 1 kHz,** every reading is within 3 dB of the floor.
- **On the chord,** most moves equal the ideal within about 1 dB. Five full-range moves read above it:

  | move | reading | ideal |
  |---|---|---|
  | Size 0 → 1 | 29.8 | 8.0 |
  | Era Vintage → Clean | 27.7 | 8.4 |
  | Decay 12 → 0.2 | 27.3 | 8.0 |
  | Damping 0 → 1 | 14.0 | 8.5 |
  | Mixing time 0 → 300 ms | 13.5 | 8.0 |

These are reported, not judged, as your suspended 3 dB ceiling asks.

## 3. Q-020's cost, as promised

- **The moving design:**
  - the line gains follow Decay exactly every tick;
  - a linearised design follows the EQ shape 100 times a second and again when a knob stops;
  - the exact solve runs behind at 1,000 units a second.
- **Cost while Decay moves:** about 1.0 to 1.1 % of a min-spec core above rest, down from 9.4 % (Scape D-059).
  - **How it was measured:** on a loaded machine, as moving/rest ratios applied to the clean rest figure.
  - **Your protocol,** with its load guard not met: 2.96 % moving against 1.77 % at defaults, at 48 kHz.
  - **A clean run** under your protocol is still owed before admission.
- **The 500 Hz decay** is within 0.04 % from 9 ms after a knob stops, including where Damping yields.

## 4. The I/O gain pair is built (B435)

- **The keys:** `io.inGain` is new; `io.outGain` was `outGain`, now ±24 dB. dB = 48·(v − ½).
- **Ramps and loads:** 20 ms linear ramps; a load opens at the saved value.
- **Placement:** the input gain goes before every path, the dry included; the output gain goes after every path.
- **Meters:** peak and clip latch after each, clearable by the host.
- **Old states:** a state saved with the old `verb.outGain` loads into `io.outGain`.
- **Still to come:** Device class means keeping the pair out of presets and corners. That lands with Scape's patch file (its Q-024): the patch and corner state will exclude io.*, and the instance state keeps them.

**Ball: HYPERSAW** for the question in §1 only (respond-by 2026-11-05). The rest is FYI.
