# Gain staging

The level as it moves through the chain, stage by stage, in signal order: where it goes over, and what to turn down. The mixer lab's model (B225) as ADR-230 (3) and A1 rule it, on the Mix page.

**Looks** One card per place in the chain, in signal order, wrapping onto new rows; each card holds one strip per stage. A strip is the **Meter's own** (the same `hd-tap`, `hd-mpair`, `hd-mbar` and `hd-lamp` drawing, not a second meter): the CLIP lamp on top, the stereo pair of bars on `well-alt` with their permanent over zone, the peak readout, the stage's name, then **its gain control** and the gain's readout. The instrument's chain, with the stages the engine and the rack publish:
1. **Sources**, each after its level, pan and gate (OSC 1, OSC 2, SUB, NOISE).
2. **Filters**, each slot's input and its output.
3. **Engine**, the engine's output (`out.level`), linear and unclipped.
4. **Make-up**, the make-up gain's output, which is the rack's input. Its default is **×4 (+12.04 dB)**, shown as both ("+12.04 dB", "×4.00"); it is a user control (ADR-230 A1).
5. **FX slots**, each slot's input and its output.
6. **Master**, the limiter's input (the pre-limiter tap, where the master lamp reads: ADR-203), the limiter's **gain reduction** (one bar that grows down from the top in `meter`, no over zone, no lamp, no gain), and the output after the limiter and Volume.

The level bars keep the Meter's anatomy: scale −66 to +6 dBFS, so 0 dBFS sits at 91.7% and the top 8.3% is the over zone, tinted `alarm` at `opacity-clip-zone`, solid while latched; fill `celebrate`; peak hold a 2 px line in `value-ink`, held 1.5 s and then falling 20 dB/s; the bar itself falls with a 0.11 s time constant. The gain control is the house slider's drawing (track, handle, hit 28), `value` fill, with the readout under it in the shell's format ("−10.0 dB", "+12.04 dB"; Volume's bottom of travel reads "−∞ dB").

**The clip lamp** latches on one sample over 0 dBFS, and clears on a click (or "Clear all lamps"). The latch lives on the audio side and is cleared by the GUI; the strip shows "CLIP" over the latched maximum ("+2.3"). A lamp can only latch what it was shown. **First over:** the latched stage with the earliest first-over sample wears a 2.5 px `marker` outline (an outline, not a fill: red is the only fill that means over), a tie going to the most upstream stage; a line above the cards names it, with its time and how many more lamps are lit ("FIRST OVER — MAKE-UP · RACK IN at 0.43 s, +2.3 dB · 1 more lamp lit"). Red appears only for a latched lamp and the over zone's tint, so a healthy patch shows none; the make-up's default puts the rack's input over on a hot chord (ADR-230 item 2), and **that is shown truthfully**, so the first-over marker lands on the make-up and the first FX slot's input lights behind it.

**Gain controls** at every stage that has a gain (ADR-230 A1 (2)): each source's level, each filter slot's input and output, `out.level`, the make-up, each FX slot's input and output, and the master's input and Volume. Relative drag, `size-drag` (160 px) the full range whatever the width, shift ×0.25; double-click resets to the default; alt+wheel only; arrows in keyboard mode. The slots' I/O gains are global to the device, never morphed (B435: gain staging should not move with corners). Ranges and defaults: the make-up's default is ADR-230 A1; module I/O gains are ±24 dB, 0 dB (B435); Volume is cut-only (−∞ to 0 dB, B438); **placeholders until the mixer panel's ranges are ruled (B402, RC-2 "deferred"):** the sources' and engine's ranges, the make-up's range (−12 to +24 dB), and the preview's source levels.

**FX device (ADR-254)** The FX device has no sources, filters or make-up, so its view starts at the **audio input** stage (with its input gain), then the FX slots, then the master: `Horde.GainStaging.chain('fx')`. The rack and the master are the same stages as the instrument's.

**The consumer provides** the stage list from the manifest (`GainStaging.chain(mode)` is the reference's own: four sources, two filter slots, four FX slots, all counts the consumer's), and per frame each stage's published peaks since the previous frame (`read(id)`: `{ l, r }` in dBFS, or `{ gr }` in dB), its latch (`latch(id)`: `{ on, at, max }`, `at` a sample index) and the callbacks `clear(id)` and `onGain(id, dB)`. The component draws the ballistics and the peak holds itself, advancing by the `dt` it is given, and reads no clock.

**Fed by** RC-2 and RC-8: each source meter's and the output meter's peak since the previous frame and their clip latches come from the engine's snapshot (IF-6); the filters, the rack and the master stage supply their own (the filters' own meters; the make-up's output is the rack's input; the master's pre-limiter tap and gain reduction). A stage whose reading is not yet published draws as silent, never as a guess. The filter and FX slots' gains are the filters' and the rack's (B435; the rack contract carries them).

**Voice-bound or audio-capable** Audio-capable: it reads levels, not notes, so the FX device uses it as it is, minus the stages it does not have. (The Kuramoto circle is voice-bound, and the FX device does not show it.)

**Screens** a chassis view on `panel` cards, not a glass: it uses the chassis tokens (`celebrate`, `value-ink`, `alarm`, `marker`, `meter`), so it follows the theme, not the Screen scheme.

**Reduced motion** the meters are a transform only, and with reduced motion the view is one still frame, drawn when its data changes: the preview runs the chain to 0.8 s (the first strike's peak held) and redraws on a gain change.

**Fits** every card and strip takes `min-width: 0`; the cards wrap onto new rows and a card never exceeds its container, down to a phone's width (each strip is 60 px, so four sources fit in about 260 px).

**Sources** mixer lab (`docs/design/mixer-lab.html`, B225: the chain's stages, the latching lamp, first over, the ballistics); ADR-230 and A1 (the stages and their gain controls, the make-up's ×4); ADR-203 (the pre-limiter lamp); ADR-254 and B435 (the FX device, the module I/O gain); specs/SPEC-ENGINE.md RC-2, RC-8 and IF-6; the Meter component.

The preview draws SAMPLE levels generated in the preview (a seeded hot chord struck every 2.4 s, run through the stages with the gains the controls hold; nothing is planted), never the engine. The Instrument and FX device buttons switch the chain.

**The Chain line can set these gains (B547, B549).** The Chain line's gain lane shows one vertical offset bar per stage, from 0 dB, in the stage's own dot's column: drag a bar and this view's fader follows (`setGain(id, db)`), and a fader here moves its bar. Both read one gain model, with the ranges above. The Chain line's dots are readings only; the bars are its controls.
