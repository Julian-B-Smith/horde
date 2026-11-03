# 2026-11-03 — Arp's page from the lab, and double-click home (GUI-11, GUI-12)

**What changed.** Master at 1793733325-2f69: `PicturePicker`, `ArpViz` (the lab's order table, roll and pictures), a double-click home on the wheels, `AssignXY` and a `MacroCard` depth. Layout: Arp's `form`, which the generator holds to placing every key once (with a plant). Generator: `DATA.arp`. Shell: `arpPage`, and one listener for a double-click home. Self-checks 4g and 4h, seam test `arpface`.

**Why.** The human: "Start from the Arp lab", and "double clicking should return to home" on every parameter on every page but toggles.

**Evidence consulted.** `docs/design/arp-face-lab.html`; ADR-291 r4; ADR-351 r6; B449; a real mouse: the Pattern list, a Mix fader, a pill group and a dropdown each double-clicked home.

**A gate changed, the same strength.** `gui3_seam_check`'s `abbr` required nine pages marked grid, and Arp's page left the grid by the human's choice. It now counts grid or form: still nine, and a page in neither fails.

**What the walk found.** The Mix page's five faders, and the pills and dropdowns, had no double-click. The walk's own first version undid a step it hadn't made, because every write it makes is put back and so the bracket can make no step; it reached an earlier check's step and left morph on. It now undoes only if the bracket made a step, and it compares a fingerprint of the shell before and after.

**Not checked.** A plugin host, the light scheme, Arp's page with morph on by hand (self-check 4g measures its fit). `select` double-click was tried in this browser only. 

**Home, ruled.** The human: "I agree that it should return to a loaded preset", then "*of the value of the loaded preset". Recorded in GUI-12, ledger row `gui12-home-preset` and B261, not built: nothing loads in the shell yet, so home stays the default until the preset seam.

**Verify.** See the PR.
