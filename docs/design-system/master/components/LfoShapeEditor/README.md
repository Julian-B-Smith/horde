# LFO shape editor

The LFO's one-cycle breakpoint editor, bipolar ±1, with points, per-segment bends, the start phase φ and the live read position, and with S&H and RANDOM as shapes beside the drawn ones (ADR-266 ruling 6).

**The bend unit** is the engine's curve value u ∈ [−2.5, 2.5] (SPEC-ENGINE C.12, ruled 2026-10-21). A stage of length T follows y = (1 − e^(−c·t/T)) / (1 − e^(−c)) with c = 4·u: 0 is straight, positive is fast-start, +1 is the decay and release default, and **+2.30 is the bank's exponential** (ln 10⁴ / 4). The lab wrote the same law as c ∈ [−1, 1] with k = 2·ln 100·c, so **u = (ln 100 / 2)·c ≈ 2.303·c**, with the same sign: the lab's +1 is the bank's exponential and its +0.5 (the one-pole scaled to arrive) is u ≈ +1.15. Readouts are signed with two decimals, naming the bank's exponential ("+2.30 bank exp"), as `Envelope` shows it. `Horde.ModLaw.segCurve(x, u)`, `uFromLab(c)` and `fmtU(u)` carry it.

**The consumer provides** `pts` (`[{t, v, u}]`: t in cycles 0..1, v in ±1, u the segment's bend −2.5..+2.5, the engine's unit), `kind` (0 DRAWN, 1 S&H, 2 RANDOM), `smooth` (0–4) and `slewMs` for RANDOM, the patch `seed`, `phase`, and `keys()` (keyboard mode). It also provides `position()`, the LFO's position p = acc + φ, unwrapped, in cycles, from the modulator host; `cycleSec()` and `rateLabel()` for the caption; and `onChange(state)` (live, during a gesture) and `onCommit(state)` (once per gesture: one undo step).

**Anatomy** (a column inside a `.hd-modcard`)
- Glass: a `.hd-viz` well at 2.4 : 1, drawn with `Viz.kit`.
  - Grid lines in `scr-grid` at 20% (the zero line 40%). Labels are glass text in `scr-grid`.
  - The curve in `scr-value`: a 2 px core over a 6.8 px underlay at 22%.
  - The wrap step: dashed `scr-value` at 60%. It is a real step, not a handle.
  - φ: a dashed `scr-value` line labelled "φ".
  - Points: 8 px `scr-value` squares with a ground edge; the selected point has a `scr-marker` ring.
  - Bend rings: 3.6 px `scr-value` rings on the law at each segment's midpoint.
  - The read position: a `scr-meter` dot with its bloom.
  - A caption, top right: "1 cycle = … · rate".
- Status line: `hint`, one line, fixed 13.5 px, ellipsis. It reads the selected point ("Point 3 of 9 · t 0.375 cycle · +0.71 · bend +0.46") or the gestures.
- Shape: DRAWN · S&H · RANDOM (a radio group; the kept points return with DRAWN).
- Draw: SINE, TRI, SAW ↑, SAW ↓, SQUARE, STEPS (seeded: (seed, draw count)), MIRROR (exact under the curve law), INVERT. Each writes points and returns to DRAWN.
- Smooth: NONE · LINEAR · COSINE · CUBIC · SLEW. Picking one switches to RANDOM. Slew: 1–2000 ms, log.
- Rows not in force (Smooth outside RANDOM, Slew outside SLEW) keep their place and their contrast; their faces go dashed (`.idle`).

**Gestures** (every point and ring is a 28 px target, `HIT` 14 px)
- Drag a point: relative (it moves by the pointer's travel and never jumps), both axes. The end points keep t at 0 and 1. Shift ×0.25.
- Drag a ring up or down: bends its segment; up raises the midpoint. 160 px (`size-drag`) is the whole −2.5..+2.5. Shift ×0.25.
- Double-click: empty glass adds a point; a point deletes it (the lab's rule); a ring resets its bend to 0. Right-click deletes a point. End points are never deleted.
- Keyboard mode only: arrows move the selected point (shift fine), `[` and `]` select, Delete removes it. Each has the mouse route above.
- Edits are live; each gesture commits once.
- Under S&H and RANDOM there are no points: the glass shows eight cycles of the seeded stream (the dots are the draws, k labelled), and the read position.

**S&H and "Sample on trigger"** S&H is this shape: draw k of mulberry32(seed ^ 0x9E3779B9) at cycle k, held flat. "Sample on trigger" is a trigger action, listed in Event lanes, and it shares the hold and draw rules. ORBITAL's mappings have an S&H mode of their own (ORBITAL card).

**Screens** six screen roles only; glass text is `scr-grid` or `scr-physics`; glow is an underlay stroke; it follows the Screen scheme. **Reduced motion** one still frame, redrawn on an edit. **Fits** the glass fills its column at a fixed aspect; the pill rows wrap with the width only.

**Placeholders**
- The breakpoint shape (points with a bend per segment) and start phase as a live offset are the lab's B208 proposals, not yet ruled. Each segment's curve is the engine's C.12 law, which is ruled.
- SLEW's picture forecasts the whole page from its first draw. The plugin draws the cycles already played from the engine's record.
- A strike takes the next unused draw (lab round 3, question 3: open).
- The look-ahead knots of LINEAR, COSINE and CUBIC (question 4) and CUBIC's clamp at ±1 (question 5) are open.

**Sources** SPEC-ENGINE C.12 (the bend unit); docs/design/shape-lab-mod.html :361 (the LFO card), :1766 (`paintEditor`), :1859 (`paintRandLfo`), :1893–1966 (hit and gestures), :2015 (`buildLfo`), :809 and :823 (`segCurve`, `shapeAt`), :1314–1346 (the random laws), :1681 (`LFO_PRESETS`). The law functions are exported as `Horde.ModLaw`.
