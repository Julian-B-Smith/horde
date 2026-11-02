# The foot row and top bar through the chain: GUI-5, GUI-6, GUI-7, GUI-8 (B589, B581)

- **Queue item:** B589, B581. GUI session. The human chose "Foot row, top bar, Main". This is the first half; Main follows. It goes separately because it changes every page and can be seen and merged on its own.
- **Master, version `1793657919-9064`:** `.hd-bank` gets a radius and R2's rows; `MacroFace` (new) is the knobs and wheels, keeping `MacroRow`'s physics; the count says "n active"; `Lens` gains `form: 'foot'`; `PresetBar` (new); `size-bank-h` 120 and `size-bank-side` 44. `MacroRow` stays exported. Vendored into `arp-face/`.
- **Shell:** `#footrow` moves from the page column to the window. One Lens replaces the per-page ones. The bank sits beside `#sums`. The undo line floats. `PresetBar` and the colour controls are absolutely placed in the room the page tabs leave.
- **Evidence consulted:** GUI-5 to GUI-8, ADR-338 r5 and r7, ADR-341 r6 and r7, ADR-345 r1 and r4, `bank-lab.html`, `lens-preset-lab.html`.
- **Found while building:**
  - The self-checks run before the `win` flag, in whatever width the page has, and read 867 px. The new check sets 980 px while it measures.
  - **The info sidebar with morph on shrank to 93 px** once the foot row left the column, smaller than the normal box. I did not loosen the check. The rail now runs past the body's foot and the row sits beside it, and the box is 219 px. The foot row also stays 120 px with the sidebar on rather than ADR-345 r4's 92 px, because the Lens and macro knobs need it. That's a reading, open to the human.
  - A browser cache served the old shell for two runs, which looked like checks failing on code I had already fixed. Cache-busting the URL showed the real results.
- **Checks that moved with a ruling, not weakened:** the macro self-check (rows to knobs and wheels), the sidebar's (92 to 120 px, full height now 126 px taller), `infolock`'s width, and `pageforms`' Lens plant.
- **MOCK:** the scope and spectrum, the preset bar's loading, and the macros.
- **The labs:** `bank-lab.html` and `lens-preset-lab.html` ran on the old shell, so on today's they apply their forms twice. They are harvested, and the ledger says so.
- **Not checked:** a host; the light scheme; other window scales; the 8 pages' freed left columns, which are unchanged and need a look; the macro knobs' map route, which is open.
- **Checks:** the shell's self-checks, 51 of 51, including morph on with the sidebar; `gui3_seam_check`, 43 tests; `design_chain_check`.
