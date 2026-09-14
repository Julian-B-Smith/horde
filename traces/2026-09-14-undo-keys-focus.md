# 2026-09-14 — Undo shortcuts: the keyboard follows the mouse

**Ask (human).** "make sure cmd/ctrl+z and cmd/ctrl+shift+Z trigger undo and redo."

**Finding.** The listener (B84) was correct but unreachable in practice: the
2026-08-12 key-focus passthrough handed focus back to the host on EVERY click
and pointer-up, so after the knob tweak you would want to undo, the next
Cmd+Z went to the DAW. A webview cannot receive a key it does not have focus
for; no shell change can fix that.

**Change.** `src/gui/gui2.html`: focus is released when the pointer LEAVES the
editor (`mouseleave` on the document element / `pointerleave`), not on click.
While the mouse is over the editor, shortcuts are ours; over the DAW, the
DAW's. The passthrough toggle (A/B) is unchanged. The Live computer-keyboard
note-off finding still holds whenever the pointer sits over the editor — the
same condition under which the player is not looking at Live's keyboard.

**Verification.** `lab_load_check` green; `./verify fast` exit 0. The
shortcut itself is host-delivered and is the human's DAW test. Also: the FX
crossfade toggle IS on the SET page (cluster "FX crossfade (dev)"); the
human's installed build predated the merge.
