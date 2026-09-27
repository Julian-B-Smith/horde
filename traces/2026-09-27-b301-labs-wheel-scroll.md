# b301-labs-wheel-scroll — a plain wheel never turns a lab control; a pill click keeps its panel's scroll

- **Queue item:** B301. I read the row verbatim from `origin/lead-records-109:ROADMAP.md` (records PR #789). The human, 2026-09-27: "Can we make it so scrolling in the lab no longer turns knobs? It makes navigation a real headache. Also clicking button parameters resets the position of the control window."
- **Why:** Two faults, both measured before they were fixed.
  - A plain wheel over a control changed its value and called `preventDefault`, so the page stopped scrolling under the pointer and the patch changed on the way past.
  - The SCALPEL lab rebuilt its whole frame on a pill click. That built a new Controls scroller, and a new element starts at scrollTop 0.
  - Isolation: six labs, `./verify`, one new harness, and this trace. `docs/design/index.html` is not regenerated. Nothing in `src/`, `specs/` or `reference/` changed. No DSP, visuals or layout changed; help text changed where it describes gestures.

## 1. Inventory (headless Chrome, measured on `origin/main` before any edit)

**Method.** A scratch probe (session scratchpad, never tracked) was injected ahead of each lab's scripts by a scratch copy of `serve_labs.py` on port 8301. It records every `wheel` listener, and runs in real headless Chrome (`--headless=new`, no DevTools):
- **Wheel:** a synthetic `WheelEvent` (plain, then alt) at a 5×5 grid over each listening element. It counts `preventDefault` and changes in the lab's state.
- **Scroll:** every element with `overflow: auto|scroll` that overflows is scrolled to mid. Then each button, pill, tab, checkbox, or pointer-cursor element inside it is clicked, and its `scrollTop` is compared.
- **Page:** the same for the page itself.

This covered 37 labs: every `docs/design/*.html` except `index.html`, plus the SCALPEL lab's `?page=main`, `?skin=c&t2=1&adv=1` and `?t2=1&adv=1` views.

**Wheel: six labs changed a value on a plain wheel.** Nothing else under `docs/design/` listens to `wheel`. `bend-lab`'s "wheel" is a pitch-wheel canvas driven by the pointer.

| lab | handler (origin/main line) | what a plain wheel did | measured before |
|---|---|---|---|
| scalpel-interface-lab | `knob()` :2518 | every knob, ±0.02 of travel | plain: 500/500 preventDefault, 465 value changes |
| scalpel-interface-lab | `cycleEdits()` :3210 (skin C / CYCLE EDITS) | cut rate of the blade under the pointer | plain: 10/25 points preventDefault and changed |
| compact-lab | `knobCell()` :1571 | every knob | plain: 4600/4600 preventDefault |
| filter-lab | `wirePath()` :2663 (a filter box) | resonance | plain: 5/25 points preventDefault and changed |
| fx-design-lab | `wireEq()` :2308 (an EQ node) | Q | plain: preventDefault on a node hit |
| fx-design-lab | `wireKnob()` :2403 | every enabled knob | plain: 825/825 preventDefault |
| morph-editor-lab | `wirePad()` :4089 (a pin) | the pin's value | plain: preventDefault and changed on a pin |
| station-page-lab | `buildMatrix()` :1144 (a PM cell) | the PM index | plain: 300/300 preventDefault and changed |

**Scroll: two labs reset a panel on a click.**

| lab | panel | handler | cause | measured before |
|---|---|---|---|---|
| scalpel-interface-lab | Controls column (`.scroll`) | `rebuild()` :2689, reached from every pill/toggle/select (`afterWrite` for kinds e/t/s, `mkPills`, the tier bars of ONE LEVEL DOWN and ADVANCED, view pills, XY pill, …) | `FR.innerHTML = ''` and a fresh build: the scroller is a NEW element, so scrollTop is 0 | 18/30 clicks → 0 (OSC), 20/30 (skin C); every tier pill in the open tiers |
| compact-lab | a frame's Controls area (`.ctlscroll`) | `renderFrame()` :1991 → `renderWire()` | `F.el.body.innerHTML = ''` and a fresh build | Advanced: 382 → 0 |

**Seen, not a reset, not changed:**
- **Collapse clamps.** `fx-chain-morph-lab` E/G/H, `modulator-lab` "×" / "+ add route", and the SCALPEL ADVANCED tier bar. Scrolled to the bottom, a click that shortens the content clamps scrollTop to the new maximum. The page height changes by the same amount; that is the browser working.
- **`morph-editor-lab` EXEMPT: 1281 → 1167.** The inspector above loses 342 px of rows. This is scroll anchoring, not a jump to the top.
- **`compact-lab`: ±21 px page shifts** on the strip's "moving out of sight" buttons (deliberate navigation, `openFull` → `scrollToFocus`) and on Advanced. Page height is unchanged. Hypothesis: scroll anchoring over a rebuilt frame.
- **`formant-lab` key "C": once 762 → 543.** It did not reproduce on a rerun, and the key has no click handler (pointer only).
- **Native number fields** (seeds in `detune-lab`, `glitch-lab`, `modulator-lab`, `morph-editor-lab`, `station-page-lab`, `shape-lab-mod`, `composed-engine-check`). The browser's own behaviour. A synthetic event has no default action, so the probe cannot see it. Open question 3.

## 2. The fix

- **`onAltWheel(node, f)`**, one copy in each of the six labs, byte-identical:
  - A plain wheel returns before anything: no change, no `preventDefault`, so the page or panel scrolls.
  - With alt, `f` runs and the event is claimed, unless `f` returns `false`. The hit-tested canvases return `false` on a miss, as they skipped `preventDefault` before.
  - Step sizes are unchanged: 0.02 of knob travel, 0.1 of a PM index, ×1.1 Q, and so on.
- **`keepScroll(root, sel, view, build)`**, in the SCALPEL and compact labs:
  - It saves each `sel` scroller's offset by ordinal, builds, and puts the offsets back. The browser clamps them to the new height.
  - `view` is the navigation key: SCALPEL `LAB.page`, compact page/source/full/Advanced page. A different view starts at the top.
  - SCALPEL: `rebuild()` wraps its clear-and-build in it. Compact: `renderFrame()` → `renderFrameNow()`.
- **Help text:**
  - SCALPEL: skin A's side text, skin C's text, and the cycle caption.
  - Compact, filter, FX design (knobs and the EQ note), morph editor (pad hint and interaction list), and the STATION PM matrix note.
- **Measured after** (the same probe, same views):
  - Plain wheel: 0 `preventDefault` and 0 value changes, in every lab.
  - Alt wheel adjusts: SCALPEL 415/450 knob events, and the cycle canvas; STATION 300/300.
  - Scroll moves: 0 of 30 (SCALPEL OSC), 0 of 30 (skin C), and 1 of 100 on the open tiers. The 1 is the ADVANCED bar collapsing its own tier, 378 → 104: a clamp. Compact: 0.
  - Page moves: unchanged from before; none of them was a reset.

## 3. The check — `tools/labharness/lab_wheel_scroll_check.mjs` (WIRED: `./verify fast`, ~3 s)

- **Static totality** over every `docs/design/**/*.html`:
  - No wheel registration (`addEventListener('wheel'|'mousewheel')`, `onwheel =`) outside `onAltWheel`.
  - Every lab that defines `onAltWheel` is in the inventory (`LABS`).
  - Every copy of `onAltWheel` and `keepScroll` is identical to the SCALPEL lab's.
- **Executed**, one worker thread per load (morph_editor_check's pattern):
  - Each inventoried lab runs under a fake DOM, parsed from its own markup, with a real listener registry and the lab_load stub for everything else.
  - **Plain wheel** at every wheel-listening element (a 16×10 grid on canvases): 0 `preventDefault`, and 0 changes to the lab's state plus every form value.
  - **Control:** alt wheel must change state at every `onAltWheel` call site in the source. The site comes from the registration's stack frame, over all variants; SCALPEL loads `?skin=c` too. A call site that never ran fails.
  - **Scroll:** SCALPEL and compact set each rebuilt scroller to scrollTop 137, click every element with a click listener inside it, and require 137 after. That is 101 clicks in SCALPEL.
- **Calibration, every run:**
  - Two planted faults in the SCALPEL source, in memory: the alt guard deleted, and `keepScroll`'s restore deleted.
  - Each must come back red. A needle that is not found fails the run.
- **Must-fail proof, in a scratch copy:** the current SCALPEL lab with only `rebuild()`'s `keepScroll` reverted:
  ```
  OK   scalpel-interface-lab.html: plain wheel over 20 element(s): 0 value change(s), 0 preventDefault(s); alt wheel moved 19/20
  FAIL scalpel-interface-lab.html: 101 click(s) inside 1 ".scroll" scroller(s), 101 moved — .scroll[0] "Sine": scrollTop 137 -> 0 (rebuilt); …
  OK   scalpel-interface-lab.html ?skin=c: plain wheel over 3 element(s): 0 value change(s), 0 preventDefault(s); alt wheel moved 2/3
  OK   scalpel-interface-lab.html: control — alt wheel adjusts at 2/2 onAltWheel call site(s) (knob:2547, cycleEdits:3239)
  lab_wheel_scroll_check: RED — 1 failure(s)
  ```
  On `origin/main`'s SCALPEL and STATION labs, the plain wheel fails with 34 and 24 value changes, and the scroll fails 101/101.
- **What it cannot see:** it has no layout, so it does not model clamping or page scroll. Those were checked once in Chrome, above.
  - `<script src>` oracles load only for SCALPEL, whose cycle view needs `razor-core.js` defaults.
  - SCALPEL's `runChecks` (its own load-time gate, ~50 s in the vm) is replaced by a no-op. Timers and animation frames never run, as in lab_load_check.

## Alternatives rejected

- **A document-level wheel guard, one per lab.** It would need `stopImmediatePropagation` ahead of the controls' listeners, and it breaks the canvases' own miss logic. One idiom at each registration is simpler, and a static check can hold it.
- **Headless Chrome as the gate.** It would be a new `./verify` dependency (gui_history_check's reasoning). It was used only to inventory and to cross-check.
- **A shift or ctrl modifier.** Shift+wheel is horizontal scroll in browsers, and on macOS a mouse's shift+wheel arrives as `deltaX`. Ctrl+wheel is pinch-zoom on a trackpad. Alt is the SCALPEL lab's existing "fine" modifier for the XY drag.

## Verify

`./verify fast` exit 0 at `5354ec0` (`.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"5354ec0"}`); `lab_wheel_scroll_check: GREEN — 0 failure(s); 6 labs, 2 planted faults`. Also green by hand: `station_check` (full leg; 21 checks), `lab_load_check` (54 labs).

## Open questions

1. **Is ⌥/alt the right modifier?** The lead proposed it. Known costs:
   - Firefox on Windows/Linux maps alt+wheel to history back/forward by default. `preventDefault` on a claimed event stops that, but a missed canvas hit falls through.
   - On Windows, releasing Alt can focus the menu bar in some browsers.
2. **"Fine adjust".** The alt+wheel step is the old wheel step, unchanged. Should it be finer? The idiom would carry it; each call site sets its own step.
3. **Native `<input type=number>` fields.** In Chromium, a focused number field steps on the wheel (browser behaviour, not lab code). Seven labs have seed fields. Blurring on wheel is the usual idiom, but it is unverifiable headless (synthetic events have no default action). Not changed; fix it?
4. **gui2 (`src/gui/gui2.html`).** Out of scope, but its PM matrix cell also adjusts on a plain wheel (gui_history_check drives "matrix cell 3001 — wheel"). Same change for the shipping GUI?
5. The compact lab's ±21 px page shifts are not explained (hypothesis: scroll anchoring). They are not resets and were left alone.
