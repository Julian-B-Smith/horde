# Modulator bank (F5, BT1)

The bank in the foot row (GUI-8 r3, r5, r6; ADR-340 r11): **120 px tall** (`size-bank-h`), about 302 px wide in the shell, with a scope over a spectrum beside it. The foot row spans the window, so the morph dock and the inspector can't squeeze it (GUI-8 r4). Rounded corners (GUI-5 r3).

`Horde.ModBank(el, S, { mods, macros, set, pageKeys, onSet, onAdd, onRemove, onMap, onDrop, onSelect, onMacro, macroPath, keys })`
- `mods`: an array or a function returning the modulators to list. The host decides: in the shell, ENV 1 always, every modulator with a route on any page, and the pending ones (GUI-7, amending ADR-323 r4). `macros`: the Macros set's controls (see below).
- Callbacks only; the bank removes and adds nothing itself.

**The side column** (44 px): the **BT1 segments** MAC over MODS (an ink-edged pair stacked to fit the column, `aria-checked`, 28 px hit), the count ("6 active", "10 macros"), and + on the Mods set.

**Mods (R2, GUI-8 r6):** two columns of short rows, 26 px tall, eight in view and the rest scrolling: swatch, name, a 22 px picture, Map. The value and × are on the row's compact card.

**Macros (GUI-8 r5):** `MacroFace`, eight compact knobs in two rows of four, beside the bend and mod wheels as small vertical sliders spanning both rows. All ten fit. The bend springs back and the mod wheel holds (ADR-341 r7); the hardware wins while it holds. No motion trace, and how a knob maps is open: the human hasn't ruled that yet. `MacroRow` stays exported, for a host with room for rows.

**The expanded state**: a click on a row's name opens its compact card above the bank, aligned to the row; a second click, a click elsewhere, or Escape closes it. Escape goes through the layered cancel (`S.escapeLayers`, ADR-338 ruling 11), which acts in keyboard mode or where the host sets `mapKeysAlways`.

**Returned** `{ set(s), get(), refresh(), close(), macro(id), setHardware(id, v | null) }`. `setHardware('bend', v)` is the hardware wheel's value arriving from the engine.

**Sources** GUI-5 r3, GUI-7, GUI-8 r3 to r6, ADR-338 ruling 7, ADR-340 ruling 11, ADR-341 ruling 7, `docs/design/bank-lab.html` (F5, R2).
