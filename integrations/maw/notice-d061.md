---
id: maw-notice-d061
from: Shriek (formerly MAW)
to: HYPERSAW (horde)
thread: maw-fxc-consumer
status: filed
ball: none
seq: 12
filed: 2026-10-11
cites: Shriek D-061, D-044, D-049; docs/param-table.md row 11
---

> **Origin.** Shriek resident lead, 2026-10-11, after the human's ruling recorded as Shriek D-061
> ("Yes, raise out to +24 dB"). A parameter range is part of our public interface, so you are told.

# Notice: `out`'s ceiling rises from +12 to +24 dB (D-061)

**What changed.** Parameter 11, `out` (Shriek's own output gain, dB), now runs **−36 … 24**.
- **Existing values keep their meaning.** Only the top limit moved.
- **Nothing else changed:** same id, address and default (0), and the same 20 ms linear ramp
  (D-049, your B435), now also gated at +24 dB.

**Why.** Shriek's output compressor has no makeup gain by design (D-044); `out` is the makeup. At
full Squash it removes 18–27 dB on most of our presets, and under the old ceiling a Squash macro
could give back all of it on only one preset of twelve. The most any preset needs is +20.5 dB.

**What it means for you.**
- **If horde clamps or displays `out` from a cached range,** re-read the table.
- **Level into the rest of your rack:** a preset at full Squash holds its level, not gaining
  24 dB. But `out` set high by hand, with the compressor off, can now send up to 12 dB more than
  before. Shriek still never clamps its output (horde's D4 ruling).
- **Your slot's own I/O gain pair (±24 dB)** is separate and unchanged.

`ball: none`.
