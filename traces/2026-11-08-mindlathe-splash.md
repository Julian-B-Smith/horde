# 2026-11-08 — The Mindlathe splash (GUI-54 A1, GB30)

**What changed.**
- **The master (`1794163020-0651`):**
  - **`Splash`:** the logo over the window, fading on the first press or at 2.5 s, at once under reduced motion.
  - **`decodePng`:** 8-bit RGB/RGBA, not interlaced, inflated with `DecompressionStream`.
  - **`MINDLATHE_PNG`:** the art as base64.
  - **The asset:** `assets/Logos/mindlathe-saturated.png`, the human's saturated header graphic scaled to 960 px wide with only IHDR/IDAT/IEND kept.
  - **Docs:** a Splash README and preview, and a Logos README entry.
- **The shell:** `H.Splash($('win'), { reduce: S.reduce })` at load. On the plug-in, hello's answer with `first_open: false` dismisses it at once.
  - The screen-colour self-check skips the splash's canvas, as it skips the logo's: art, not a scheme screen.
- **`tools/splash_art_check.py`, wired into `./verify fast`:**
  - It holds the bundle's base64 to the PNG, byte for byte, in the master and the vendored copy.
  - It checks the PNG is what the decoder reads, with no metadata chunk.
  - Two must-fail controls: one changed byte, and a tEXt chunk.
- **`gui3_browser_check`** gains the splash probes. They use real mouse input, with the splash's life timestamped by a MutationObserver:
  - The art is painted, cyan and magenta, under the plug-in page's policy too.
  - The splash covers the window's inside.
  - A real click on the Mix tab under it changes nothing and starts the fade. **Control:** the same click afterwards opens Mix.
  - It fades on its own between 2 and 4 s after it is seen. **Control:** it is still up at 1.2 s.
  - Under reduced motion a press removes it with no fade.
  - On the plug-in, `first_open: false` removes it before any fade. **Control:** `first_open: true` leaves it to its timer.
  - The text-selection probe now waits for the splash to go before its drags.
- **`docs/proposals/gui-on-engine.md` §3:** `first_open` in hello's answer. GN-54's row names it.
- **Records:**
  - GUI-54 A1;
  - GB30's status;
  - the ledger row `gui54-splash` (in the shell) and `master_versions`;
  - the arp-face provenance row.

**Why.** The human's GUI-54 (2026-11-06) waited on the art. On 2026-11-08: "Realizing we never added the Mindlathe logo".
- **The art:** the human picked the PNG.
- **The page policy:** it blocks every image, and the human chose decoding to a canvas over loosening it.

**Evidence.**
- `splash_art_check`: GREEN, 415953 bytes, in 2 bundles, with 2 controls fired.
- `gui3_browser_check`: GREEN, with the splash's line and its controls.
- **Plants, run together and restored.** Each gave its own red line:
  - **The splash letting clicks through** (`pointer-events: none`): "a click on the Mix tab under it changed the page", and "a click did not start the fade".
  - **A decoder that reads nothing:** "the art is not painted", on the lab page and under the plug-in's policy.
  - **The shell ignoring `first_open`:** "first_open false took it away only after a fade".
- **Two bounds in the new probes were set by measurement:**
  - "Covers" is the window's inside, since its 1 px border stays.
  - The timer window is 2 to 4 s, because the observer stamps "added" after the shell's first script, so a 2.5 s timer read 2.39 s.

**Verify.** `./verify fast` on the commit this trace ships in.
