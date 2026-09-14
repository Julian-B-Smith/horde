# 2026-09-14 — History round 2 (B118): morph-pad gesture, vertical list, host-independent hotkeys

**Human notes after audition.** (1) morph position needs history; (2) depth
columns confuse two-branches-off-one-base with branches off consecutive nodes
— "vertical lists with dropdowns"; (3) hotkeys still not working.

**(1)** `src/gui/gui2.html`: the MORPH page pad's pointerdown/up now bracket
`bridge.gesture(152/153)` exactly like the MAIN mini pad, so one drag = one
node (snapshot at gesture END; nothing per write).

**(2)** `renderHistory` rewritten: one path from a root through the current
node to a leaf; `<select class="histbranch">` at every node with >1 children
(default = the child on the way to the current node, else the newest);
`histChoice` is viewer-side state; rows after the current node carry
`.ahead` (redo path); `<select class="histroot">` only when the forest has
more than one root. CSS switched `#histTree` to a column.

**(3)** `src/gui/hypersaw_gui.mm`: `Impl::installKeyMonitor()` at attach —
`NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown`; claims
Cmd/Ctrl+Z ± Shift only when the pointer is inside our view's bounds and our
view is NOT first responder (then the page's listener owns it); calls
`host.undoStep(±1)` and returns nil so the host's own undo does not also
fire. Removed in `~Impl`. The GUI catches up on the next frame (params and
the history page poll). Windows counterpart owed (B99).

**Verification.** `lab_load_check` GREEN (26 labs); full Release build clean;
`./verify fast` exit 0. The monitor is host-delivered behaviour: the human's
DAW test. Installed for audition.

**Addendum 2026-09-14 (off-by-one).** Human: "clicking an option selects the one below, and clicking dropdown options doesn't seem to work." Reproduced in the browser pane with a stubbed tree: clicking row 1 restored node 3. Cause: the path loop reassigned its `let` variable at the bottom of the body, so each row's click handler and dropdown captured the NEXT node. Fix: `const n = walk` per row. Re-test in the pane: click restores the clicked row; a dropdown choice redirects the list below it.
