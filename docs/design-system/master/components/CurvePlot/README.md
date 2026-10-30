# Curve plot

A static glass that plots value over time for the modulators' segment law: one or more curves, stated-time marks and arrival rings, used for the curve picker and for the envelope's arrival plot.

**The bend unit** is the engine's curve value u ∈ [−2.5, 2.5] (SPEC-ENGINE C.12, ruled 2026-10-21). A stage of length T follows y = (1 − e^(−c·t/T)) / (1 − e^(−c)) with c = 4·u: 0 is straight, positive is fast-start, +1 is the decay and release default, and **+2.30 is the bank's exponential** (ln 10⁴ / 4). The lab wrote the same law as c ∈ [−1, 1] with k = 2·ln 100·c, so **u = (ln 100 / 2)·c ≈ 2.303·c**, with the same sign: the lab's +1 is the bank's exponential and its +0.5 (the one-pole scaled to arrive) is u ≈ +1.15. Readouts are signed with two decimals, naming the bank's exponential ("+2.30 bank exp"), as `Envelope` shows it. `Horde.ModLaw.segCurve(x, u)`, `uFromLab(c)` and `fmtU(u)` carry it.

**The consumer provides** `T` (the time span, or a function), `lines()` → `[{ f(t) → 0..1, role, glow?, dash?, a?, lw? }]`, `marks()` → `[{ t, label }]`, `rings()` → `[{ t, v }]`, an optional `caption`, `xFmt` (seconds by default) and `label` (the accessible name). Call `draw()` after a change; it redraws itself on resize.

**Anatomy**
- A `.hd-viz` glass (2.2 : 1 by default), y 0..1, with the 0, 0.5 and 1 lines and labels.
- Time ticks at a nice step, in `scr-grid` (the last one right-aligned).
- A curve with `glow` is the beam: a `scr-value` core over its 22% underlay. Others are thin lines in their role; a `dash` marks a reference.
- Marks: dashed `scr-marker` verticals with their labels above (A, D, OFF, R).
- Rings: `scr-grid` circles, 3.5 px.

**Use** the curve picker plots one stage at a bend u with its mirror (u negated, dashed) over the family u −2.5, −1, 0, +1, +2.5. The arrival plot draws the engine's stages (C.12, default bends) solid against the shipped time-constant law dashed, with the same four numbers; its rings mark where the shipped law really gets there (99%).
- **Not** for pitch travel: Bend law and its Bend view draw the pitch system's travel laws.
- **Not** for a sounding envelope: Envelope.

**Screens** six screen roles only; glass text in `scr-grid` or `scr-marker`; it follows the Screen scheme. **Reduced motion** static by nature. **Fits** fills its column at a fixed aspect.

**Placeholders** the stage law is the engine's (C.12), so stages end exactly at their times. The τ × ln 100 migration of shipped patches is the lab's proposal (B208 panel 3) and is not ruled. The lab's comparison table and live migration check are lab-only, so they are not carried.

**Sources** SPEC-ENGINE C.12; docs/design/shape-lab-mod.html :431 (panel 3), :809 (`segCurve`), :870 (`envTauAt`), :2122 (`paintLaw`).
