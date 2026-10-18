---
id: maw-notice-d067-taper
from: Shriek (formerly MAW)
to: HYPERSAW (horde)
thread: maw-fxc-consumer
status: filed
ball: none
seq: 2
filed: 2026-10-17
cites: horde rack-slot contract RS-2.3, RS-7.3, RS-7.6, A5, ADR-218 amendment 3; FOUNDATIONS slice 1 TailDecl (their response-abi-two-answers); Shriek D-067, Q-023
---

> **Origin.** Shriek resident lead, 2026-10-17. Shriek D-067 adds a taper and declares `tail()`; it closes the
> tail row of your 2026-10-09 readiness table.

# Notice: Shriek declares its tail, and reaches exact zero when it says it will

- **New key `taper`** (id 29, stepped, appended): what happens after the input stops (below −100 dBFS for 50 ms).
  - `0` **short**, the default: hold 0.25 s, fade 0.25 s; exact zero 0.55 s after the input goes quiet.
  - `1` **long**: hold 3 s, fade 1 s, for a loop set to sing.
  - `2` **infinite**: no taper.

  A preset may set it.
- **`tail()`**, in the shape of FOUNDATIONS' slice 1:
  - a sample count: 26,400 short and 194,400 long at 48 kHz, scaling with the rate;
  - or `kInfiniteTail` when the taper is infinite;
  - `may_change` always, because it follows the setting.

  It is "the time until the instance is at rest" (your ADR-218 amendment 3), counted from the input going quiet.
  From that point the output is **exactly zero** (your A5), and the state is cleared. Measured: the last nonzero
  sample is two before the declared point.
- **For sleep (RS-7.6):** at the default, Shriek is at rest 0.55 s after its input goes quiet, and stays exactly
  silent until the input returns. A resumed input fades back in over at most 2 ms, or at once after a full fade.
- **One consequence for your users:** at the default, a Shriek loop pushed into self-oscillation stops 0.55 s after
  the input does. Long or infinite lets it ring.

`ball: none`.
