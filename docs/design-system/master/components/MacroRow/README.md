# Macro row (BR-b)

A macro or a wheel as a thin row with its slider (ADR-340 ruling 11's BR1, drawn as BR-b by ADR-341 ruling 6).

`Horde.MacroRow(el, S, m, { onChange, onMap, onDrop, path, sliderWidth, keys })`; `m`: `{ id, name, wheel: 'bend' | 'mod' | null, value, def, fmt, range, touches }`. Returned `{ get(), set(v), setHardware(v | null), held() }`.

**Anatomy** (a 188 x 26 px row, `size-row-macro-w`): the source swatch (`src-macro`; `src-engine` for the wheels), the name over "touches n" or "unbound" (or "springs" / "holds"), a TS1a slider (58 px, 1:1, drag re-anchored at its stops: it is the existing tiny `Slider` on a global value), the value, Map. A macro is a source, so it keeps its Map button and has no x.

**BR-b, the rows follow the wheels**: on the two wheel rows a **half-second trace** (a 2.2 px `value` stroke at 35 %, behind the handle) shows where the handle has been, so the spring's return is visible and not only its end; the **bend row has a centre notch** (an ink triangle above the track at 0.5) where it springs to. No trace under reduced motion.

**ADR-341 ruling 7** (the physics runs every frame, painted or not):
- A mouse drag on the **pitch-bend row springs back to centre on release**, with a 50 ms time constant in seconds (`1 - exp(-dt / 0.05)`, dt from the frame clock `S.now`, never a per-tick constant; reduced motion snaps).
- **While the hardware holds the bend, the hardware wins**: `setHardware(v)` pins the row to v (a mouse drag cannot move it); `setHardware(null)` lets go and the bend springs.
- The **mod wheel's row holds** where it is left; `setHardware(v)` follows the hardware and `null` leaves it where it is.
- **The engine side is not here.** `onChange(id, v, by)` is called for every change the row makes itself (`by` = `mouse` or `spring`); the engine's echo comes back through `set()` or `setHardware()` and is never re-emitted.

**Rules** the bend's value reads in semitones (`range` default 2, a true minus: "+1.8st", "−0.4st"); the other rows in percent. A macro slider under MAP behaves as every slider does (a click would map the current source onto it): whether a macro value is a mapping destination is not ruled.

**Readings taken** the row follows composition-lab-r2's BR1 drawing (swatch, name, slider, value, Map) with r3's trace and notch; r3's own rows dropped the Map button and the swatch. The order of the ten rows is the host's (the ruled order is Tone, Space, Time, Motion, 5 to 8, Bend, Mod wheel).

**Placeholders** names of slots 5 to 8 (ADR-192 leaves the phrase list open); the macros' values are SAMPLES.

**Sources** ADR-340 ruling 11, ADR-341 rulings 6 and 7, composition-lab-r3.html BR, composition-lab-r2.html BK.
