# Meter

A level meter: a stereo pair of bars on `well-alt` with peak hold, a permanent over zone, and a CLIP lamp that latches.

**The consumer provides** the tap's level stream (published by the engine per block, read by the GUI; never computed in the GUI), the tap's name, and its latch state. The latch lives on the audio side and is cleared by the GUI: a lamp can only latch what it was shown (mixer lab, Blind-Spot Armor: output guards latch and report, never clamp silently).

**Anatomy** bars `size-meter-w` (10) × 132, ink edge, `radius-meter`; scale −66 to +6 dBFS, so 0 dBFS sits at 91.7% and the top 8.3% is the over zone, always tinted `alarm` at `opacity-clip-zone` and solid `alarm` while latched. Fill `celebrate` from the bottom. Peak hold a 2 px line in `value-ink`, held `duration-peak-hold` (1.5 s) then falling 20 dB/s. The peak readout below in `value` style; the tap name in `label`.

**The CLIP lamp** 44 × 28: idle `panel` with `hint` text; latched `alarm` with `on-alarm` text. Click clears it. The first stage to latch gets a 2.5 px `marker` outline (first over).

**Motion** transform only (`scaleY`, `translateY`), never width or height. Red appears at most twice on a page: clip and PANIC.
