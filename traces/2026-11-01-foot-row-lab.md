# foot-row-lab — six options for the foot row, measured on the running shell (B589)

- **Queue item:** B589. GUI session. The shell hop (#1318) left the foot row as its first open ruling, and the human answered "Draw the options".
- **Why a lab:** the charter starts a lab only for a question the human has asked; this one ends in a ruling. Nothing in the shell, the master or the bundle changed.
- **What it is:** `docs/design/foot-row-lab.html` loads `gui3.html#main,win,bank` unedited in a frame and applies each option as a stylesheet (F3 also moves one node). The shell is one closed function, so the lab drives it through its tabs and the same `pointerover` its own self-check dispatches.
- **Evidence consulted:** ADR-335 r4, ADR-338 r7 and r12, ADR-259 r2; `docs/design/gui3.html`; the composition lab's frame (the source of the 472 figure, an older shell's page area).
- **Readings, from the lab's own table:**
  - As built, 10 of 14 pages scroll. Main, Osc 1, Osc 2, Sub, Noise and Mix fit at 467 px, so a foot row of 121 px lets them fit. Mod, Morph, FX and Ledger scroll under every option.
  - F1 (300 × 120) fits those six and keeps the bank's six rows in view; 76 of 714 descriptions run past the box's edge (142 at the first width tried, 224 px, so its width was tuned after the first reading).
  - F3 narrows pages by 234 px and pushes Mix to a 193 px scroll. F4 hides up to 4 controls while it shows. F5 needs a 980 × 815 window.
- **Found:** moving the pointer onto the info box clears it (`hostOf` and `sectionHostOf` find nothing, so `showInfo(null)` runs). Text past the edge can't be reached by hand, though the shell's check 22 reads "long text scrolls inside it". Confirmed with a real pointer in the browser and printed as a NOTE by the lab. Not fixed here; recorded in `gui3/README.md`.
- **Recommendation given:** F1. It removes the empty band where F5 pays 95 px of screen and keeps it. The call is the human's.
- **Checks:** the lab's self-check, 11 of 11, including three controls: F0's reading equals the number the shell's own fit check prints (95 px); a box made to follow its text moves the layout; leaving every option restores F0's rectangles. The shell's own self-checks read 41 of 41 in the frame. Morph on (F3) and a bank card (F1) were driven and looked at in the browser; F4's float was hovered with a real pointer.
- **Not checked:** other window scales, and the light scheme.
- **Depends on:** #1318 (the lab reads the batch 2 shell). It targets main and carries #1318's commits until that merges.
- **Verify:** `./verify fast` exit 0 on this commit's tree (`lab_load_check`: 75 labs, 0 broken; `design_chain_check` GREEN with the new lab at `exploring`). It first went red on the lab's own `ResizeObserver`, which the load harness doesn't stub; the lab now listens for the window's resize. No gate was edited.
