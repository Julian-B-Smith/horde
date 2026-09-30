# b373-maw-workstation-skin — the MAW lab as 80s workstation software, driven by the live state

- **Queue item:** B373. I read the row verbatim from `origin/lead-records-148:ROADMAP.md`, with B318, B322, B326, B338, B343 and B361. The horde lead dispatched it on 2026-09-29.
- **Why:** The human asked: "Could I see a version of the functional MAW page rendered in the 80s workstation style from the design lab?" The answer is a switchable skin of the real lab, driven by its state, not a mock.
- **Branch:** `lab-maw-workstation`, from `origin/main` (bc729a8).
- **Files:** `docs/design/maw-horde-lab.html` and this trace. No other file was touched and none was deleted. `docs/design/index.html` was not committed. `lab-review` now reads `B318 + B373 · 2026-09-29`.
- **Evidence consulted:**
  - the MAW lab as it is on main;
  - `docs/design/fx-screens-workshop.html`: the `ws` treatment (:532-535), its renderer (:1184-1328), the 3×5 font (:655-672), B326's MAW proposal for the glass (:1845-1890), `propDrawWS`/`propRule` (:2115, :2170) and `glyphAudit` (:2300-2400);
  - `docs/design/assets/fx-logos/README.md`;
  - the Sluice lab's section N4 and C44-C48 (`sluice-horde-lab.html`) and the B361 trace;
  - `tools/labharness/lab_wheel_scroll_check.mjs` (its static layer and LABS inventory) and `tools/labharness/lab_load_check.mjs`.

## What was built

**The skin.** `?skin=ws`, or the SKIN tabs in the header (DOM · WORKSTATION · BOTH). BOTH shows the two windows side by side.
- The workstation window has the same header and rack strip as the DOM window.
- The glass sits in a bezel: 320×288 logical px at ×2 (640×576), with four numbered soft keys under it.
- Beside the glass is a CHASSIS with the four role knobs. These are DOM knobs that call the same writer as everything else.
- The side panel shows the screen's map, read off the pages themselves.

**The glass.** The workshop's `ws` renderer is copied with line citations:
- `wsWin`, `wsBtn`, `wsSlider`, the menu bar and soft strip layout, and the Bayer-4 dither to paper and ink. Paper is the scheme's `--scr-tube` and ink is its `--scr-physics`, read from this page's tokens for both themes (ORCHID light, TUBE dark).
- Everything is drawn in greys with whole-pixel `fillRect`, then dithered. So every frame is one bit by construction.
- B326's MAW proposal (the MacPaint plate and the jaw icon) is composited bit for bit after the dither, on the FACE page.
- A dropped `assets/fx-logos/maw.png` (96×24) or `maw-icon.png` (16×16) of exactly that size wins. It is shown as drawn, as in the workshop.

**One model, two views (section M).** New writers:
- `setSlot`, `resetSlot`, `setTier`;
- `setBypass`, `setPlay`;
- `setSrc`, `setNote`, `setChord`, `setPatch`;
- `setTheme`, `stepPreset`, `stepPatch`, `resetParam`.

These join the existing `write`, `loadPreset` and `togglePlay`. The DOM handlers that were inline closures now call these writers, with no change in behaviour.

`ACTS` is the registry of 13 actions. The DOM controls carry `data-ctl`: `P/<key>` for MAW's 70 parameters, `S/<role>`, `T/<role>`, and the named ones.

**The app (section N).** Screen controls are data plus handlers, rebuilt from the live state every frame. Hit-testing, keyboard traversal and the checks all read the same VM.

Period idiom:
- a **menu bar** with pull-downs;
- one **document window** per page;
- a **data-entry strip**. It has a data slider, − / + and HOME, all acting on the *cursor*: the last field, cycle or role focused. Four cursor keys move the cursor.
- the **soft strip**: the four role sliders over four page-labelled soft keys. This is the workshop's chassis contract, with MAW's S1 S2 S3 ROUTE on the face (B322).

**Page and soft-key map** (read off the pages; `wsMap()`):

| page | soft keys 1 · 2 · 3 · 4 |
|---|---|
| FACE (logo, preset, route, three live stage curves, tiers and readouts) | S1 · S2 · S3 · ROUTE |
| TRANSFER (one stage's curve, large, at the playhead) | S1 · S2 · S3 · FACE |
| SPECTRUM + SCOPE | TRANSFER · LEVELS · PLAY/STOP · FACE |
| LEVELS (the drive meter) | TRANSFER · SPECTRUM · PLAY/STOP · FACE |
| ROUTE + FEEDBACK (topology, crossover, feedback) | < ROUTE · ROUTE > · TRANSFER · FACE |
| IN - OUT - BEHAVIOUR | TRANSFER · LEVELS · BYPASS · FACE |
| STAGE 1 / 2 / 3 (every stage parameter and a live curve) | S1 · S2 · S3 · FACE |
| MODULATION | TRANSFER · LEVELS · PLAY/STOP · FACE |
| RACK INPUT (source, note, chord, SCALPEL patch, play, bypass) | PLAY/STOP · BYPASS · PATCHES · FACE |
| MAW PRESETS (a list) | PREV · NEXT · REVERT · FACE |
| SCALPEL PATCHES (83 rows, scrolling) | PREV · NEXT · INPUT · FACE |
| INFO + THE DAW (the DAW list, fidelity, the checks) | SPECTRUM · LEVELS · PRESETS · FACE |

Menus:
- `*`: ABOUT MAW, SELF-CHECKS.
- FILE: OPEN PRESET, NEXT, PREVIOUS, REVERT.
- PAGE: all fourteen pages.
- INPUT: the four sources, CHORD, SCALPEL PATCH..., RACK INPUT...
- AUDIO: PLAY/STOP, BYPASS THE SLOT.
- VIEW: LIGHT (ORCHID), DARK (TUBE).

**Input.**
- Pointer: the CSS position ÷ the integer scale gives a glass pixel, and the control whose rect holds it is hit. A field drags like the DOM knob (160 px per turn, Shift for fine). Sliders take the position. A cycle's left or right half steps it. Double-click resets a field or a role.
- Keyboard: Tab / Shift+Tab walk every control, and Tab past the last lets the browser move on. ↑ ↓ move by row. ← → turn a field, role or cycle, or move along a row; in a menu they switch menus. Enter / Space press. Esc closes a menu or goes back. 1–4 are the soft keys.
- Focus is drawn as a solid ring plus inverted value, and a double ring on buttons. Hover is a dotted ring. The cursor shows as an inverted value.

**Relabelled, per the lead's B318 note** ("relabel as 'unallocated' in the next touch"): the GLOBAL roles' host ids now read *unallocated (ADR-186)* everywhere. That covers the tier tooltip, the legend, the DAW list, the notes and the screen's INFO page. Before, they read 300-303.

## Checks

**Count:** 11 self-checks, 19 must-fail controls. The 5 existing checks are kept unchanged; checks 6-11 are new. Result: **11/11, 19/19 caught**. I read them over CDP in headless Chrome for `?skin=both`, `?skin=ws` and the default load. The screen checks take about 6.7 s on top of the lab's boot.

- **6. Every lab action has a screen path.**
  - 13 actions over 1713 controls (14 pages × cursor variants, 6 menus): all reachable.
  - All 86 DOM `data-ctl` values map to an action.
  - All 70 MAW parameters, the 4 roles and tiers, 10 presets and 83 patches are on the glass.
  - Controls: a planted action, a planted DOM control, and `stages.2.res` dropped from the screen.
- **7. Hit-testing.** 8925 points: each control's centre and four corner pixels, 21 page and menu states including a scrolled list, at ×1, ×2 and ×3 on an offset glass. A real pointer event on the live canvas hits the right control at ×2 and resized to ×1. Control: a mapper fixed at ×2 is off at ×1 (2912 points).
- **8. Keyboard traversal.**
  - 614 controls over 20 states: Tab from the first reaches all, and so do the arrows alone.
  - Pressing soft keys, buttons and menu items for real reaches 14/14 pages from the FACE.
  - A real Tab keydown moves the focus.
  - Controls: a Tab that skips the soft strip; the PAGE menu without MODULATION.
- **9. One model, two views.**
  - A real ArrowRight on S1's drive moves the state, and the DOM knob reads it ("0.0 dB" → "+0.6 dB").
  - A real Enter on a route radio moves `BASE.route` and the DOM pill.
  - A real pointer press at 80% of the Amount slider moves `SLOTS` and the DOM face knob.
  - The reverse direction also works: a DOM tier click changes the screen's tier.
  - Control: a field wired to a copy of the state.
- **10. The glass's rules.**
  - 614 frames: every control focused in turn, the next one hovered, the cursor on it where it takes one, 205 of the frames dark. Each frame has exactly 2 colours, the scheme's paper and ink, with 0 px off.
  - B326's logo is bit for bit on 108 face frames, and an open menu is whole over it.
  - The live canvas is 640×576 = ×2.00, pixelated, 320×288 backing, 2 colours.
  - None of the 20 drawing functions uses a vector or text operation.
  - Controls: a ring written straight into the frame in `--scr-value`; a ×1.5 scale; a planted `fillText`; the logo composited over an open menu. That last one was a real bug in the first build: I found it in the screenshots, then added the assertion and its control.
- **11. The visualiser follows the render (L0064).** This is a scratch render of "inertia sweep" (stage 1 swings by up to 1.44 over the loop) at 0.18, 0.86 and 1.55 s.
  - The screen's curve is the render's own snapshot array, 3/3.
  - Its pixels are that curve's in 600/600 columns.
  - The three playheads give three distinct pictures.
  - The LEVELS cursor stands on the playhead's block, 3/3.
  - Controls: the UI-param preview (uiTwin) misses 98% of columns; a playhead frozen at the analysis point misses 96%.
  - My first pick, "polynomial screech · env→morph", moves by only 0.02 (under a pixel), and the frozen control MISSED on it. The comment records this. The pick is by measurement, not by name.

**Gates:**
- `lab_load_check`: GREEN. Under it MAW is absent and the glass says so.
- `lab_wheel_scroll_check`: GREEN, 7 labs. There is no wheel listener in this lab.
- A grep for machine paths finds 0.

## Screenshots (scratch, not committed)

All are in `scratchpad/b373/shots/`, at 1× DPR.
- **Every page in two schemes:** `L-01-face` … `L-16-menu-input` (ORCHID) and `D-01` … `D-16` (TUBE). This includes a PAGE and an INPUT menu open. Montages: `mont-L-1..4.png` and `mont-D-1..4.png`.
- **Focus ring:** `focus-ring-L.png` and `focus-ring-D.png` (×3, the ECOLOGY field), and `focus-hover-face-L.png` (the cursor on S1's drive plus hover on Tone's tier).
- **Visualiser while audio plays:** `play-1-transfer`, `play-2-transfer` (0.76 s and 1.31 s, the fold curve moving), `play-3-levels` and `play-4-levels` (the cursor moving). Montage: `mont-play.png`.
- **DOM and skin side by side after on-screen edits:** `both-after.png` and `both-after-plugs.png`. The edits were made with real CDP mouse and key input: route → FEEDBACK, Amount dragged to 0.76, Regen to 0.80, soft key 1 to STAGE 1, five ArrowRights on the drive, and curve B cycled to RECTIFY. The DOM shows every one.

## Alternatives rejected

- **Alt + wheel on the glass.** The brief asks for it, but the repo's idiom (`onAltWheel`) is enforced by `lab_wheel_scroll_check`. Its static layer fails any lab that defines `onAltWheel` without an entry in the check's `LABS` inventory. That file is outside this round's scope ("touch only the MAW lab and a trace"), and dodging the check's regex would be circumventing a gate. So the glass takes no wheel, and a plain wheel still turns nothing. This is the same gap B361 left for Sluice.
- **The workshop's DOM-overlay logo.** The glass composites B326's 1-bit proposal into the frame instead, so the one-bit rule can be checked on the logo too. A human file is still shown as drawn.
- **Knobs drawn on the face page.** They would duplicate the role sliders over the soft keys. The face carries the tiers and readouts instead.

## Verify

`./verify fast` was run on the committed hash. The result and hash are in the PR and the report, read from `.harness/last-verify.json`.

## Open questions

- **For the human:**
  - Is the menu bar plus data-entry strip the right idiom for horde's FX screens, or should every screen keep to four soft keys and four knobs (B322's contract)?
  - The B326 logo is a knock-out plate. `glyphAudit` (B343) measures GBC titles, whose letters are separable fills. I did not run it on this 1-bit plate, because its letters touch the plate's pattern. Should the workstation logo get its own lettering check?
  - A dimmed (disabled) 3×5 label is illegible. Disabled buttons therefore get a dotted border instead, and an idle radio stays legible under an "IDLE" heading. Is that the right reading?
- **For the lead:** alt + wheel needs this lab added to `lab_wheel_scroll_check`'s LABS inventory, plus an `onAltWheel` copy wired to the glass. That is a small follow-up that touches the check file. Sluice's glass has the same gap.
- **Not verified:** only still frames, scripted CDP input and headless audio clock reads were checked. Nobody has used the glass by hand or listened.
