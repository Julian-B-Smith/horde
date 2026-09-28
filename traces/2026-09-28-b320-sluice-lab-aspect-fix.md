# b320-sluice-lab-aspect-fix — canvas backing-store height was never resized

- **Queue item:** B320 (human, 2026-09-28: "there's a strange aspect ratio issue" in
  `docs/design/sluice-horde-lab.html` — text in the network view stretched tall and
  narrow, modules clipped).
- **Why:** `ctx2()`'s sizing helper set a canvas's backing-store WIDTH from
  `clientWidth·dpr` but never touched its HEIGHT, which stayed at the HTML/JS attribute
  value (26/118/236/150). At `dpr=1` this is invisible (the scale factor is 1 on both
  axes anyway); at `dpr>1` the draw transform scales both axes uniformly by
  `cv.width/w` while only the width's backing store actually grew, so drawing past the
  unscaled height fell outside the bitmap (clipped) and what remained was stretched to
  fill the CSS box (tall and narrow). Confirmed with a headless Chrome screenshot at
  `--force-device-scale-factor=2`: pre-fix, the network view's entire LOOP feedback bus
  (drawn near the bottom of the canvas) was missing; post-fix, present.
- **Evidence consulted:** `docs/design/sluice-horde-lab.html:1336-1345` (old `ctx2`),
  its six call sites (`net`, `ir`, `out`, `xy`, per-corner `cir0..3`), the CSS
  (`.viz{width:100%}` — width only, no height rule; `.fxbody`/`.plug`/`.col` layout),
  `tools/labharness/lab_load_check.mjs` (the load-survival gate), ROADMAP.md B320.
- **What actually shipped:** `wantBacking(cv)` is the one formula for a canvas's
  backing-store size, shared by `ctx2` (applies it) and the new **K10** self-check
  (verifies it) — so the check can never drift from the helper's own behavior. Height is
  captured ONCE per canvas into `cv.dataset.b320H`, not re-read from `clientHeight` on
  every call. Reason: these canvases have no CSS height rule, only an auto height that
  falls back to the intrinsic backing-store ratio when reflowed before the inline
  `style.height` lands — re-measuring `clientHeight` after writing `cv.height` closes a
  loop through that ratio. A first implementation that DID re-measure `clientHeight`
  every call was caught live via a Chrome DevTools Protocol trace showing `#net`'s
  `clientHeight` shrinking ~3px every animation frame, never settling — invisible in any
  single screenshot, only visible over time. Switched to the one-time capture, which
  stabilized it. `g.setTransform` now uses the actual per-axis ratio
  (`cv.width/w, cv.height/h`) rather than assuming both equal `dpr`.
- **K10:** audits every canvas on screen (8: `net`, `ir`, `out`, `xy`, `cir0..3`) against
  a must-fail control — a canvas built with the exact shape of the bug (a mismatched
  backing store, never run through `ctx2`). Wired into `runChecks()` beside K1–K8; K9
  (real AudioWorklet) stays separate/async as before. All 10 checks PASS.
- **Alternatives rejected:** deriving the intended height from `clientHeight` on every
  `ctx2` call (mirrors how width already works) — rejected after the CDP trace showed it
  creates a live, non-converging feedback loop through the canvas's auto-height/intrinsic-
  ratio fallback. Making K10 force a repaint before asserting — rejected as
  self-defeating (would always pass, catching nothing).
- **Verify:** `./verify fast`, exit 0, git `a77d946` (`.harness/last-verify.json`).
  `lab_load_check` GREEN (silent — verify only prints on failure). Manual: headless
  Chrome screenshots at `--force-device-scale-factor=2`, both themes, network view
  un-stretched with the LOOP bus fully rendered; live CDP trace of `LAB.checks` showing
  10/10 PASS with each control firing.
- **Open questions:** none for this fix. The lab's underlying model/layout is unchanged
  per scope (B321 covers the architecture spec / rebuild).
