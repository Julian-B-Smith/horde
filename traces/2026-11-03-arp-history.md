# 2026-11-03 — Arp's button and the History page (GUI-10)

**What changed.** Arp has its own top-level button. The master gains `HistoryRail`, at 1793730698-1175. The device's flat undo stack is a tree with `restore` and a cap of 200; `undo`, `redo` and `history` read as before. The shell gains the History page, and the preset bar narrows to 218 px for the two buttons. Self-check 4f, seam test `historytree`; the `history` test's plant moved to the new code, testing the same thing.

**Why.** The human: "Arp should get a top-level button", "we haven't added the History page yet", then "Yes, Arp and History".

**Evidence consulted.** `src/gui/gui2.html` (`renderHistory`, the HIST page); ADR-160; B118; B119; ADR-357; a real mouse in the pane: knob drags on Main, Undo twice on the page, a new edit, a real fork in the graph, a click on the left branch.

**Not checked.** A plugin host, the light scheme, more than a handful of states by hand (the cap is held by the seam test). Once, with no wait between actions, a click straight after a knob drag was lost in the pane; with a pause it wasn't. Whether that's the tool or the shell isn't known.

**Open.** Arp's page: the human hasn't said what isn't right.

**Verify.** See the PR.
