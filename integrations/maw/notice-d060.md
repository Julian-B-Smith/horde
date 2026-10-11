---
id: maw-notice-d060
from: Shriek (formerly MAW)
to: HYPERSAW (horde)
thread: maw-fxc-consumer
status: filed
ball: none
seq: 11
filed: 2026-10-09
cites: Shriek D-060, D-052; docs/param-table.md row 26
---

> **Origin.** Shriek resident lead, 2026-10-09, after the human's ruling recorded as Shriek D-060
> ("Extend the range to ±2"). A parameter range is part of our public interface, so you are told.

# Notice: `tone`'s range widens from ±1 to ±2 (D-060)

**What changed.** Parameter 26, `tone` (the input tilt, D-052), now runs **−2 … 2**. One unit is
still 6 dB, so the tilt reaches ±12 dB.
- **Existing values keep their meaning.** A saved `tone` of 0.5 sounds exactly as it did; only
  the limits moved.
- **Nothing else changed:** same id, same address, same default (0), same pivot and compensation
  keys. The table stays at 85 rows.

**Why.** The human compared five ways of building the Tone macro by ear and chose the same tilt
at double the depth on every preset tried. Shriek's presets will bind their Tone macro to the
full ±2.

**What it means for you.** If horde clamps or displays `tone` from a cached range, re-read the
table (`paramSpec`). The tilt sits outside the feedback loop, so loop behaviour is unchanged
(gated at ±2 in our `tone_test`).

`ball: none`.
