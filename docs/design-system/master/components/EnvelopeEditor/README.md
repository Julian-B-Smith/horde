# Envelope editor

The envelope's breakpoint editor in real seconds: points and bends, a sustain point where the held note waits, and release from whatever level it has reached; the A/D/S/R sliders write a four-point shape.

**Not the Envelope visualizer.** `Envelope` (Visualizers) draws the engine's fixed attack–decay–hold–release envelope, on a receding phosphor floor. This is the modulator lab's EDITOR, on a different data model: any number of points and a sustain index. Both speak the same curve unit, u (below). They share the look (curve in `scr-value` over an 18% fill, `scr-meter` playheads). Use `Envelope` to show a sounding envelope, and this to edit a modulator's.

**The bend unit** is the engine's curve value u ∈ [−2.5, 2.5] (SPEC-ENGINE C.12, ruled 2026-10-21). A stage of length T follows y = (1 − e^(−c·t/T)) / (1 − e^(−c)) with c = 4·u: 0 is straight, positive is fast-start, +1 is the decay and release default, and **+2.30 is the bank's exponential** (ln 10⁴ / 4). The lab wrote the same law as c ∈ [−1, 1] with k = 2·ln 100·c, so **u = (ln 100 / 2)·c ≈ 2.303·c**, with the same sign: the lab's +1 is the bank's exponential and its +0.5 (the one-pole scaled to arrive) is u ≈ +1.15. Readouts are signed with two decimals, naming the bank's exponential ("+2.30 bank exp"), as `Envelope` shows it. `Horde.ModLaw.segCurve(x, u)`, `uFromLab(c)` and `fmtU(u)` carry it.

**The consumer provides** `adsr {a, d, s, r}` (seconds; s a level) or `pts` plus `sus` (−1 = none, one-shot); `voices()` → `[{t, tOff, v0?}]`, the sounding voices' time since the gate, release instant and restart level, from the voice host; `keys()`; `onChange` and `onCommit` (once per gesture).

**Anatomy**
- Glass 2.4 : 1, as in the LFO editor:
  - the y axis 0..1;
  - time ticks in `fSec` units;
  - the window is the last point × 1.2, frozen while dragging so the axis never chases the pointer;
  - SUS: a dashed `scr-marker` line with its label ("ONE-SHOT" when none);
  - voices as `scr-meter` dots.
- Status line, as in the LFO editor.
- Sustain: a stepper (NONE, P1…Pn−1) with a fixed-width name, and a hint saying what the choice does.
- Attack 0.001–2 s, Decay 0.005–4 s, Sustain 0–1, Release 0.005–8 s (log), in an auto-fit grid. These are the shipped ranges.
  - They write `adsrPts`: each stage ends exactly at its time (C.12), with the engine's default bends: attack u 0, decay and release u +1. The ADSR pill restores those bends; moving a slider keeps the bends you set.
  - Edit a point by hand and they stop describing the shape: their faces go dashed and the status line says so.
- Shapes: ADSR · PLUCK · SWELL · GATE · AD.

**Gestures** as the LFO editor. Bending a stage's ring on the ADSR shape edits that stage's curve row (`attackCurve`, `decayCurve`, `releaseCurve`, −2.5..2.5) and does not make the shape hand-drawn. A double-click on a ring restores that stage's engine default: the first segment is the attack (u 0), any later one a decay or release (u +1). The first point keeps t = 0, and the last point may move later (up to 10 s). Adding or deleting a point keeps the sustain index on the same point. The sustain stepper steps by click, Option + wheel, or arrows in keyboard mode.

**Placeholders**
- "The number on a stage is when it arrives" is now the engine's law (C.12: a stage's time is exactly when it ends). The migration of shipped τ patches (τ × ln 100) is the lab's proposal and is not ruled.
- Retrigger for an envelope is assumed: ON restarts from the level reached, OFF is legato. See Mod card header.

**Sources** SPEC-ENGINE C.12 (the curve law and defaults); docs/design/shape-lab-mod.html :375 (the envelope card), :842–868 (`susOf`, `envAt`), :879 (`adsrPts`), :1695 (`ENV_PRESETS`), :2066 (`buildEnv`), :2081 (`paintSus`).
