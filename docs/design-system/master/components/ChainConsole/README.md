# Chain console

The Mix page as one console for the whole chain (GUI-14, GUI-16): every stage's level and gain, in signal order, with the signal flow's picture in the same box. It replaces the Channel strip plus Gain staging and Chain line on Mix: the sources' strips and Gain staging's first four stages were the same tap and the same gain, shown twice.

**The consumer provides** `stages` (`H.GainStaging.chain`'s list), `strips` for the stages that have a parameter (`{ id, name, path, fader, range: { lo, hi }, on?, mute?, solo? }`), `gain(id)` and `onGain(id, dB)` for the rest, `read(id)` (`{ l, r }` dBFS peaks, or `{ gr }`), `latch(id)` (`{ on, at, max }`), `clear(id)`, `onToggle(id, what, val)` and `flow: { F, onChange }`. It computes no level. `step(dt)` advances the ballistics by the dt it's given (no clock) and `render()` paints.

**Anatomy** (a minimum of 906 px wide; narrower, it scrolls sideways inside its own box)
1. A top line: the first over, or who is soloed, and Clear all lamps.
2. The picture, 110 px on a screen: each node directly over its strip's bars, at its level on the console's ruler, with the flow's wires. A wire that skips stages arcs clear of the line, a split above and a merge below, so serial and parallel don't look alike. A bypassed module's nodes are hollow and the wire jumps over them.
3. The strips, grouped by section. Each: name, peak, CLIP lamp, the level bars with the gain's arrow on them, the gain's value, and its buttons.

**One ruler.** Every strip reads −66 to +26 dB, the meter's floor to `out.level`'s top. Each arrow's rail covers only its own range. A stage's gain is ±24 dB, so a meter on its own fader's scale couldn't show a level under −24 dBFS. The cost is a red zone across the top 28 %.

**A strip with a parameter** is the house morph `Slider`, so its corner pips, tether, mod lanes, MAP, undo and double-click are the fader's. **A stage with none yet** (B402) has a drawn arrow: relative, 1:1 with the ruler, Shift fine, double-click home, arrows in keyboard mode. It can't be mapped, and says so under MAP.

**Solo.** A source's S is its own solo key: additive, and it silences the other sources. Any other stage's S taps that stage's output and plays it directly: one at a time, because two points on one chain would double the signal. Both can apply together. A tap still goes through the limiter. GR has no S, because it's no signal, and the master's OUT has none, because it's what you already hear.

**Bypass.** BYP on each FX module's header. The wire jumps over the module and its own gains are out of the signal; its strips dim and read the level going past. Bypassing a soloed module ends the solo.

**The words.** The engine's output stage reads SUM · VOICES: the sum of every voice, the sources through the filters (OU-1). The key stays `out.level`.

**Strips stay in slot order** when the routing changes, so a fader never moves under the hand. Only the wires move.

**`H.ChainConsole.flow`** is the instrument chain's flow, pure: `make()`, `edges(F)`, `feeds(F)`, `heard(F)` and `levels(F, src, gainOf, order)`. `levels` is a model for a preview or a stub until the engine has a tap at every stage (IF-6, RC-8); it is not a measurement. The FX device's chain (ADR-254) starts at its audio input, with no source strips; its flow isn't built.

**Colour** no new colour: `celebrate` (fill), `value-ink` (hold), `alarm` (over zone, latched lamp, mute, the 0 dB line), `caution` (a pressed BYP), ink (solo, arrow).

**Accessibility** each strip is a `role="group"`; a drawn gain is a `role="slider"` with its value; BYP and S are `aria-pressed` buttons named for their stage.

The preview draws SAMPLE levels from `flow.levels` and a seeded signal, never the engine.
