# 2026-11-03 — The wheel and the page (GB6, GUI-15)

**What changed.** `docs/design/gui3.html`: `overscroll-behavior: contain` gone from `#pages` and the info box, an override on Mod's list and pane, and self-check 4i. `docs/design/gui3/README.md` item 25, the ledger row, GUI-15, GB6.

**Why.** The human, 2026-11-03: "it's hard to scroll on a lot of pages because screens and other features seem to block the scroll."

**Evidence consulted.** Every `wheel` listener in `arp-face/bundle.js` and `gui3.html`: all return at once on a plain wheel. A hit-test grid of the window on ten pages. Real wheel input, which showed the trap on Mod and that the rack screen and Mix's console pass the wheel through.

**A gate held.** A first build forwarded the wheel from outside `#pages` with its own listener. `lab_wheel_scroll_check` went red on it (B301 allows a wheel listener only inside `onAltWheel`), so it was removed and put to the human. It's in this branch's first commit.

**Not checked.** A plugin host's webview, and how smooth a scroll feels on the human's own mouse or trackpad.

**Verify.** See the PR body.
