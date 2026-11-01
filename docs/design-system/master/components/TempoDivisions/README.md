# Tempo divisions

The tempo-division list: the 22 straight, dotted and triplet note values inside 0.0625..8 beats, as a picker panel and as the SYNC side of RateSync, the house's one rate-plus-sync control.

**TempoDivisions — the consumer provides** the stored `beats`, the host tempo `bpm()` (a number or function), an accessible `path`, and `onChange(beats)` and `onCommit(beats)`.

**Anatomy**
- A `well` panel: 1.5 px `ink` edge, `radius`, 7 px padding, 420 px at most.
- Three columns headed STRAIGHT, DOTTED, TRIPLET (mono 600 9 px, `t2`). One row per note value, slow to fast.
- Each value is a 28 px pill named in its own words: "3/8 · 1/4D".
- Two gaps say why they are empty: "12 beats: out of range" and "below range" (`hint`).
- A pill lights ONLY on an exact match.
- A readout line (`hint`, two lines reserved), for example: "1/4 = 1 beat = 500 ms = 2.000 Hz at 120 BPM · 10 of 22, slow to fast".

**RateSync — one control for a rate (ADR-266 r10).** `Horde.RateSync(el, opts)` is the modulators' rate and the STEPPED morph's step rate. The morph builder's SteppedControl already uses it.
- The consumer provides:
  - `name`, `path`;
  - `sync` (0 or 1), `hz`, `beats`, `bpm()`;
  - `min` and `max` for the free rate (log), and `unit` ("Hz", or "/s" for steps);
  - `defHz` and `defBeats` (double-click resets);
  - `keys()`;
  - `onChange(state)` (live), `onCommit(state)` (once per gesture).
- State is `{ sync, hz, beats, rate, name }`; `rate()` is the effective Hz.
- Anatomy:
  - row 1 (`hd-srow`, 72 px label, 64 px readout): FREE <unit> · SYNC pills and the period ("500 ms");
  - row 2, one fixed 31 px slot, so FREE/SYNC never resizes the control: the free-rate slider, or the division stepper ◀ name ▶ (RateSync's own, not yet `Horde.Stepper`: see the Stepper README) (a 12 ch name, ellipsis) with its Hz equivalent.
- Gestures:
  - The free slider takes the house drag: 160 px is the full log range at any width, shift ×0.25, relative, double-click resets, Option + wheel sets, arrows only in keyboard mode.
  - The stepper walks the list slow to fast by click or Option + wheel; double-clicking the name resets.
  - A value off the list (a loaded patch) is KEPT and reads "≈ 1/4 −5.0%"; the first step lands it on its nearest name.

**Placeholders**
- The STORED FORM. The list's name is UI and the stored truth is beats per cycle (`lfoNBeats`, its id and its 0.0625..8 range unchanged), so no patch migrates. This is the lab's B208 proposal and is not yet ruled; the plugin's manifest decides it.
- Phase in SYNC is measured from the bar (the lab's proposal), and is not built here.

**Sources** docs/design/shape-lab-mod.html :390 (panel 1), :782 (`DIVS`), :794 (`nearestDiv`), :2008 (`divName`), :2092 (`buildDivs`).
