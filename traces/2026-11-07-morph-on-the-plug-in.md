# 2026-11-07 — The page drives the test plug-in's morph (GUI-59, GB34)

**What changed.**
- **`docs/design/gui3/device.js`** (the adapter), only when the transport hosts the morph (`morphHost`).
  - While morph is on, no morphing engine row goes as a `set`.
  - Each corner's changes go as `corner` messages, diffed per corner, whatever wrote them.
  - `save` replaces the stand-in's Save current sound.
  - `waypoints` and `waypointLaw` go when they change.
  - The position is sent at most once a frame.
  - The morph's settings go before the sets in one send.
  - Nothing about the corners goes before hello's answer.
  - Each frame's `morph` member is adopted (stale values skipped), and `morphReport()` exposes the levels, refusals and counters.
- **`docs/design/gui3/engine-stub.js`** (the transports).
  - The schema gains `corner`, `save`, `waypoints`, `waypointLaw` and `lock`, and a `corner` is checked as a `set` is.
  - The plug-in's sides now include the resolver, and the native and replay transports say `morphHost`.
  - The merger passes a frame's `morph` member through.
  - The replay can push a test frame with its own ack.
- **`docs/design/gui3.html`.**
  - On the plug-in the morph switch and the morph's rows are live.
  - `want` asks for the morph on every page.
  - The Morph page shows a line reporting the plug-in's morph.
  - The pad follows the plug-in's position when it moves without the page, as host automation does.
- **Tests.**
  - `gui3_seam_check` gains `morphbridge`, with six plants.
  - `gui3_browser_check`'s bridge probe requires a live switch, a morph `want`, `morph.on` sent on a click, and the report.
- **Records.**
  - GUI-59.
  - GB34.
  - The ledger row `gui59-morph-bridge`, with engine impact GN-34, GN-35 and GN-55.
  - GN-34 and GN-35 narrowed, and GN-55 new.
  - `docs/proposals/gui-on-engine.md` section 3 gains the morph's messages and the frame's member.

**Why.** The human reported build 8's morph "isn't working yet". The page sent no corner, so the plug-in held one preset in all four corners. The lead's request, 2026-11-06 and 2026-11-07, set out the bridge's messages (`tools/engine-preview/README.md`, "The morph").

**Evidence.**
- The frame's `morph` member shape is from `tools/engine-preview/bridge_core.cpp` (`morph_json`), and its change rules are from `preview_plugin.cpp` (`morph_member`).
- `gui3_seam_check`: GREEN, 50 tests, 112 planted faults each turning their own test red, six of them new.
- `gui3_browser_check`: GREEN.
- A plant that disables the morph switch on the plug-in again turned two of its lines red, and was restored.
- The shell as it was, with the pad always pushing its position, turned the new position line red.

**Not done here.**
- Region locks: the bridge can't read `lock_as_is` (GN-55).
- Corner names and saved presets have no message yet.
- A drawn fade per source waits on a master component; the levels are printed.
- The lead rebuilds the plug-in with this page, and the human listens.

**Verify.** `./verify fast` on the commit this trace ships in.
