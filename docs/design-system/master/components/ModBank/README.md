# Modulator bank (M2, BT1)

The bank at the bottom of a page (ADR-338 ruling 7, ADR-340 ruling 11, ADR-341): **68 px tall** (`size-bank-h`, the strip the empty bank already took), **two lanes of 26 px thin rows** that scroll sideways, so a page keeps its room (468 px in the shell, ADR-345 r1).

`Horde.ModBank(el, S, { mods, macros, set, pageKeys, onSet, onAdd, onRemove, onMap, onDrop, onSelect, onMacro, macroPath, keys })`
- `mods`: an array or a function returning the modulators to list (the host decides what modulates this page, ADR-323 ruling 4: modulators routed to the page's controls, plus the pending ones). `macros`: the Macros lane's rows (see Macro row).
- Callbacks only; the bank removes and adds nothing itself.

**The side column** (52 px): the **BT1 segments** MAC | MODS (an ink-edged pair, `aria-checked`, 28 px hit), the count ("6 here", "10 rows"), and + on the Mods set. **Macros | Mods** (ADR-340 ruling 11): Macros shows the intent-bus macros (Tone, Space, Time, Motion, slots 5 to 8), the pitch bend and the mod wheel, each a thin row with its slider; Mods shows the rest.

**The expanded state**: a click on a row's name opens its compact card above the bank, aligned to the row; a second click, a click elsewhere, or Escape closes it. Escape goes through the layered cancel (`S.escapeLayers`, ADR-338 ruling 11), which acts in keyboard mode or where the host sets `mapKeysAlways`. A hover-expansion is the shell's choice; only the click is built.

**Returned** `{ set(s), get(), refresh(), close(), macro(id), setHardware(id, v | null) }`. `setHardware('pb', v)` is the hardware wheel's value arriving from the engine (see Macro row).

**Readings taken**
- The segment order is Macros then Mods (the ruling's words); the lab's BT1 drawing reads MODS | MAC. The bank opens on Mods (the previous resting form).
- The Mods lane keeps the 226 px rows and the Macros lane the 188 px rows, so the grid column width changes with the set.
- With 6 to 8 mods the lanes hold 3 to 4 columns in 958 px; the rest scrolls (`scrollbar-width: thin`).

**Sources** ADR-338 ruling 7, ADR-340 ruling 11, ADR-323 rulings 2 to 4, composition-lab.html M2, composition-lab-r2.html BK (BT1).
