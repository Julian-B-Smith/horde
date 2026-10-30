# Smoothing lanes

RANDOM's five smoothings on one seeded stream, one lane each, the same eight draws in every lane; click a lane to pick it.

**The consumer provides**
- `seed` (the patch seed; the stream is mulberry32(seed ^ 0x9E3779B9), the engine's LFO seed);
- `position()` (the LFO's unwrapped position, in cycles), `cycleSec()` and `slewMs()`;
- `mode`; `onPick(mode)` and `onCommit`.
- Call `draw()` per frame.

**The laws** NONE = draw k (the S&H law). LINEAR, COSINE and CUBIC (Catmull-Rom, clamped to ±1) pass through every draw at its cycle start and look one draw ahead. SLEW glides toward each draw and arrives at its time in ms along the one-pole's shape (u = ln 100 / 4 ≈ +1.15 in the engine's unit, SPEC-ENGINE C.12; the lab's c +0.5), whatever the rate.

**Anatomy**
- A `.hd-viz` glass at 1.9 : 1 in five bands.
- In each band: cycle lines in `scr-grid` at 16%; the curve in `scr-value` (the selected lane a beam, the others 42%); the draws as dots; SLEW's chased step dashed.
- Labels are glass text (`scr-grid`, the selected one `scr-physics`, "◀ selected").
- The read position is a `scr-meter` line across all lanes, with a dot on the selected lane.
- Over each band sits a transparent pick button: `role="radio"`, at least 28 px tall (20% of the glass), ink ring on hover, inset focus ring.

**Gestures** click a lane. The radio group is one tab stop.

**Screens** six screen roles; it follows the Screen scheme. **Reduced motion** one still frame. **Fits** fixed aspect; at 320 px each lane is still 30 px tall.

**Placeholders**
- SLEW's page is a forecast from the page's first draw; the plugin draws the cycles already played from the engine's record.
- The look-ahead knots, CUBIC's clamp against a B-spline, and SLEW's law (arrival, rate limit or one-pole) are the lab's round 3, questions 4 to 6, all open.

**Sources** docs/design/shape-lab-mod.html :658 (the card), :1314–1346 (`m32at`, `randInterp`, `randAt`, `slewAt`), :1609 (`randPage`), :2761 (`buildRand`), :2782 (`paintSm`).
