# Channel strip

The Mix page as a small console: one vertical strip per source, side by side, each fader attached to its own toggles and its own meter, with a live peak readout and a latching clip warning right beside each other (B589: "levels sliders should be side-by-side, and they should be attached to their toggles ... loosely modeled after a physical mixer device").

**The consumer provides** `strips` in signal order, each `{ id, name, path, fader, on?, mute?, solo?, master? }`: `fader` is the level's morph slider props (a manifest `key`, corner `vals` or `locked` with `g`, `def`, the shell's `fmt`); `on` present gives the strip its on toggle; the master has none of on, mute or solo. Per frame, `read(id)`: the tap's peaks since the previous frame in dBFS (`{ l, r }`, published by the engine, never computed in the GUI); `latch(id)`: `{ on, max }`, the clip latch that lives on the audio side; `clear(id)`; `onToggle(id, 'on' | 'mute' | 'solo', value)`; `keys`. `step(dt)` advances the ballistics by the `dt` it is given (no clock); `render()` paints.

**Anatomy** (one console card, `size-strip-w` (72) a strip, 1 px `div` hairlines between strips, the master behind a `stroke-page` ink rule at the right)
1. Head: the dense on toggle and the strip's name (Archivo 800, 9.5 px, UPPERCASE).
2. Peak row: the held peak in `value` style, ink, 600 ("−3.2", "−∞"), and beside it the CLIP lamp, the Meter's lamp at the dense face (30 × 20, 28 px hit): idle `panel` with `hint` text, latched `alarm` with `on-alarm` text. A click clears it.
3. Body: the fader, a vertical morph `Slider` at `size: 'compact'` (`size-fader-h`, 140 px; a 22 × 8 cap in the editing corner's colour, the corner pips, tether and mod lanes as on any morph slider), and beside it the meter pair: two `size-meter-w-dense` (7 px) bars whose height is the fader's track, so 0 dBFS on the meter and the fader's travel line up. The bars keep the Meter's anatomy: −66 to +6 dBFS, 0 dBFS at 91.7 %, the top 8.3 % the over zone tinted `alarm`, solid while latched; fill `celebrate`; peak hold 2 px `value-ink`, held `duration-peak-hold` then falling 20 dB/s; the bar falls with a 0.11 s time constant (Gain staging's).
4. The fader's value under it ("−3.6 dB"), the readout column of a sized slider.
5. Mute and solo, directly under the fader: 28 × 20 faces at 30 px pitch, 28 px hit. Mute on: `alarm` fill, `on-alarm` text (mute is one of alarm's three jobs). Solo on: ink fill, `bg` text (a selection, not a signal).

**Text fits, measured** (headless, both themes, at `size-strip-w` 72): the names OSC 1, OSC 2, SUB, NOISE and OUT beside the on toggle (NOISE, the longest, is 34 px of the 38 px left after the toggle), the peak ("−18.4", 29 px) beside its lamp, and the fader value in a 9ch tabular column that holds the longest value the law can print ("−60.0 dB", 8ch; "−∞ dB" is 5). No ellipsis fires; the ellipsis stays only as a guard for a longer name a consumer might pass. (An earlier draft at 64 px clipped all of these; 72 px is the fix, not a smaller font.)

**Solo** any soloed source silences every other source; the master is never silenced. A strip silenced that way dims its meter and fader (`opacity-muted`) and draws its M dashed, so the reason shows without a colour. A muted or off strip dims too.

**Colour** no new colour: `celebrate` (fill), `value-ink` (hold), `alarm` (over zone, latched lamp, mute; never anything else), ink (solo, text). Red therefore appears only where something is over or muted, as on the Meter.

**Accessibility** each strip is a `role="group"` named "<path> channel"; the toggle `role="switch"`, M and S `aria-pressed` buttons named "<path> · Mute" / "· Solo"; the meter pair `role="meter"` with its peak as the value text ("peak −3.2 dBFS, clipped"); the lamp says when it is latched and at what level. Off keyboard mode nothing takes focus.

**Relation to Gain staging** Gain staging is the chain view (every tap in signal order, the first over); this is the Mix page's console for the sources and the output. Both read the same taps, latches and gains; neither computes a level.

**Placeholders** the fader law and range (the preview: 0–1 → −60 to +6 dB, −∞ at the bottom) wait for the mixer's ranges (B402, RC-2); whether pan and the source's FX send join the strip is open.

The preview draws SAMPLE levels from a seeded signal and the faders' own gains, never the engine.
