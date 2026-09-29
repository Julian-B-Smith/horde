# b361-sluice-lab-round5 — one shape system; the GBC screen as the whole instrument; the FX page follows horde's morph

- **Queue item:** B361. I read the row verbatim from `origin/lead-records-142:ROADMAP.md` (records PR #855), with B338, B343, B348, B353 and B358. The horde lead dispatched it on 2026-09-29.
- **Why:** The human asked three things:
  1. "Why are the sluice lab macro knobs rounded squares, and the corners outlines ovals in some places and circles in others?"
  2. "Let's focus on the GBC view and integrate the full functionality into the GBC screen (all the buttons and sliders in the GBC screen should be interactive as well)".
  3. "when the global morph xy crosses into a corner, the FX page should switch to editing that corner (it's confusing to edit otherwise)".
- **Branch:** `lab-sluice-r5`, from `origin/land-sluice-r4`. PR #854 was still OPEN when I started (`gh pr view 854`), so this branch stacks on it.
- **Files:** `docs/design/sluice-horde-lab.html` and this trace. No other file was touched and no file was deleted. `docs/design/index.html` was not committed. The `local/sluice` link was made for the build (gitignored, pointing at the sibling's `docs/horde`) and removed at the end. Nothing was written under the sibling's tree. `lab-review` is `B319 + B329 + B337 + B338 + B343 + B348 + B353 + B361 · 2026-09-29`.
- **Evidence consulted:** the lab as it is on `land-sluice-r4`, with its header; the B353 trace; `docs/design/fx-screens-workshop.html` (the chassis contract at :18-24, `gbcSoft` at :876, the GBC palettes and tile audit); `tools/labharness/lab_wheel_scroll_check.mjs` (its static layer and inventory); `tools/labharness/lab_load_check.mjs`; `tools/sluice_hold_check.py`.

## Item by item

### 1. One shape system (C43)

The causes were measured in the round-4 lab through CDP (getBoundingClientRect):
- The CORNERS list's chips rendered 22×18. `.cchip` had `min-width:18px`, and its 22 px grid column stretched it.
- B337 gave the FIXED macros' cap `border-radius:6px`, on purpose, to tell them from the variable ones.
- Horde's corner chips (`.hchip`) were 4 px rounded squares, which B353 did on purpose.

The fix:
- One chip component (`chipEl`): a circle whose width is fixed equal to its height, with `aspect-ratio`, `max-width` and `justify-self`/`align-self`. Sluice's A–D and horde's 1–4 both use it. They differ only in ink, label and fill. The `.hchip` rules are gone.
- Every knob cap is round. The fixed macros keep their physics ink and recessed cap fill, so they still read as a different kind.
- Horde's corners on the morph canvas are drawn as circles.
- On the glass, chips are pixel circles (7 px in one tile, 13 px in 2×2) and knobs are round caps (a ring carrying the value arc, plus a pointer).

**C43** checks two things:
- Every rendered `.kcap` and `.cchip` on the Play, GBC and Rack faces, horde's MAIN page and the rack row (88 of them) has an aspect ratio of 1.00 ± 0.02 and a border radius of at least half its side.
- Every GBC chip, corner row and knob on every page (74) is drawn alone into a scratch buffer, and its pixel box is square.

C43's controls:
- a chip stretched to 30 px;
- a knob cap with a 6 px radius (B337's cap);
- a GBC chip on a 2×1 tile rect.

### 2. The GBC screen is the whole instrument (C44–C48)

- **The actions.** Section K3 holds the writers both views call:
  - `setSluiceXY`, `bindTo`, `captureXY`, `setSeed`, `undo`/`redo`, the snapshot delete, `libRevert`/`libExport`/`libImport`, `renamePatch`;
  - `setFixed`, `setGlobal`, `globalAction`, the macro mapping writers and `setRange`;
  - the module machinery, `setSync`, `groupPresent`, `normalizeOut`, `setValue`;
  - `hordeMove`, `setFlipBand`, `editHorde`, `setHordePos`, `setHordeMacro`, the map writers, `setLoadTail`, `setGlide`.

  The DOM handlers that were inline closures now call these writers, with no change in behaviour. `ACTS` is the registry: 58 actions, each listing the DOM `data-ctl` values that reach it. Lab chrome is left out on principle (audio, theme, lab input, voice, host tempo, the skin / face / page tabs, the JSON box).
- **The app (section N4).** Fifteen pages:
  - the default face (XY, the four corners, the fixed and variable macros);
  - MENU, SNAPSHOTS (bindings, tags, delete from 5 with a confirm), PATCHES (load, revert, delete, name, export, import);
  - NAME ENTRY (a character grid, plus typing on the keyboard);
  - MACROS (fixed four, variable four, range bars) and MACRO EDIT (label, targets, curves, bindings, each corner's MIN and MAX);
  - RANDOMIZE, GLOBAL, FULL DEVICE (every number and all the machinery, scrolling);
  - HORDE MORPH (the class-tinted XY, the flip line, the four corners, the band), HORDE CORNER (patch, rand, edit, its Sluice XY pad, two position knobs), HORDE MACROS (two knobs, the mappings, + map, demo);
  - SETTINGS (tails, load tail, glide, bypass, the tail meter) and SCOPE.

  Soft key 1 opens the MENU on every page. Each control is data plus handlers. The drawing reads only the data, so C26's frozen-view control still holds.
- **Input.**
  - Pointer: the CSS position ÷ the integer scale gives a screen pixel, and ÷ 8 gives a tile. Knobs drag vertically (the DOM knob's 160 px per turn, Shift for fine). Sliders and pads take the pointer position. A cycle's left half steps back and its right half steps on. Double-click on a knob centres it.
  - Keyboard: Tab / Shift+Tab walk every control, and Tab past the last lets the browser move on, so there is no trap. ↑ ↓ move by row. ← → turn a knob or slider, step a cycle, or move along the row. Enter / Space press, and on a pad they grab it so the arrows move the puck. Esc is back. 1–4 are the soft keys.
  - Focus switches the control's own tiles to palette 3 and draws a ring in its colour 2. Hover draws a dotted ring in the tiles' own colour 2.
  - The bezel and canvas are built once and kept, so a rebuild never drops keyboard focus or pointer capture.
- **The GBC face** is now the bezel, the glass and the chassis. The four soft keys are numbered only, because the screen names them; the four knobs are the variable macros. Beside it sit the screen map (read off the pages' own soft keys) and the JSON box as lab chrome. The DOM control panels that used to sit beside the glass are gone.
- **Checks:**
  - **C44**: all 58 actions have a GBC control (796 controls over 23 page instances, including the full device over every manifest type). All 66 DOM `data-ctl` values map to an action. The full device's pool numbers and param ids are each a GBC row. Controls: a planted action, a planted DOM control, and a row dropped from the screen.
  - **C45**: 2770 points (each control's centre and corner pixels, every page, at ×1 and ×3 on an offset glass) all land on their own control. A real pointer event hits the right control on the live canvas at ×3 and with the canvas resized to ×1. Control: a mapper fixed at ×3 is off at ×1.
  - **C46**: Tab from the first control reaches all 316 controls on the 15 pages, and so do the arrows alone. Pressing soft keys, menu rows and buttons for real reaches every page from the default one. A real Tab keydown moves the focus. Controls: a body-only Tab, and a menu without SCOPE.
  - **C47**: a real ArrowRight on the glass turns the fixed Time macro, and the DOM Play face's knob reads the new value. A real Enter on a corner row moves the DOM's filled chip. Control: a knob wired to a copy of the state.
  - **C48**: 427 frames, with every control focused in turn, the next one hovered and pads grabbed, in light and in dark every third frame. The result is at most 4 colours per tile and 0 pixels off RGB555. Controls: a ring written straight into the frame (off RGB555) and a fifth colour in the tile.
- **Adapted, not retired:**
  - **C23** reads the GBC skin's four fixed macros from the screen's controls, because the DOM panel beside the glass is gone.
  - **C26** renders the default page (the one with the logos and macros).
  - **C2** and C25–C27 are unchanged. `glyphAudit` and C27 are untouched.
  - The play-face "FULL DEVICE →" button lost its `data-ctl` again, because the Rack face cannot carry a way to itself (C2).

### 3. The FX page follows the morph (section K4, C49)

- **The rule.** "Crosses into" means B353's hysteresis-live patch, the live parity class that `morphRig` picks with the ruled 0.1 buffer, not the raw quadrant.
  - If the live class has one corner, the editor follows it.
  - If the class has several corners (they blend), the same hysteresis rule (`pickLive`, `'hyst'`, the same band) runs over the class's corners with its weights renormalised, and where no corner dominates it holds.
  - Only a move of horde's XY follows. Picking a corner by hand still works and holds until the next crossing.
- **Drafts.** Drafts live in the corner's patch (its working copies), so a switch never loses one. The message names any draft the corner left behind still holds.
- **Where it shows:** "EDITING: HORDE CORNER n · follows the morph" (or "picked by hand, not the live patch") appears in the FX rack row, on horde's MAIN page with the rule stated, on the GBC header ("EDIT H3 / LIVE =3") and in the glass's side map. While the puck is dragged, the display updates in place, and the rebuild waits for the gesture's end.
- **C49 results:**
  - Two structures left and right, swept along y = 0.2 with a ±0.06 wobble over 401 steps: exactly 1 switch, to corner 2.
  - One patch on all four corners, the puck wobbling on the middle line: 0 switches.
  - On the live page, horde's XY swept through `hordeMove`: 1 switch, to corner 3. The FX page reads "EDITING: HORDE CORNER 3 · follows the morph".
  - Control: band 0 gives 27 switches (chatter).

## Hold, load and names

- **C17:** manifest 0 of 530 needles; prose 0 of 13 491 runs; all controls caught.
- **`tools/sluice_hold_check.py`** with the link present: `ok … controls 7/7`.
- **`lab_load_check`:** GREEN with the link and GREEN with it moved aside. The page shows the no-spec face, and only C14 runs, 1/1.
- **Private names and paths:** the file has no machine path (a grep for a home-directory path finds 0). The default face's name appears only at runtime (C32).

## Checks

- **Count:** 48 self-checks (41 + C43–C49), **48/48** with every control firing. I read them over CDP in headless Chrome for the default load, `?skin=gbc`, `?theme=dark&skin=gbc` and `?face=rack`.
- **Retired:** none this round. C34 stays retired, from B353.

## Screenshots (scratch, not committed; `scratchpad/b361/shots/`, 2× DPR)

- `shapes-before-after.png`: round 4 against round 5 for the CORNERS list, the MACROS row and the rack row's horde chips. It was served from a scratch tree holding the round-4 file.
- GBC pages, light (`L-*.png`) and dark (`D-*.png`): xy, menu, snaps, patches, name, macros, medit (the range editor), random, global, device, horde, hcorner, hmacros, settings, scope. Montages are in `mont-L1.png`, `mont-L2.png` and `mont-D.png`.
- `focus-ring-zoom.png`: the focus ring, light (a knob) and dark (a range slider).
- `follow-0-main` … `follow-3-gbc` (montage `mont-follow.png`): horde's XY before and after a sweep, the FX page's indicator, and the GBC header.
- `it-after.png`: after a scripted run of real pointer and keyboard events over the glass (menu, new patch, naming, randomize, save, XY drag, knob drag, range edit, grab-and-move on horde's XY, settings, device paging, the mapping demo).

## Alternatives rejected

- **Alt + wheel on the glass.** The repo's idiom (`onAltWheel`) is enforced by `lab_wheel_scroll_check`. A lab that defines it must be on that check's LABS inventory, which is a file outside this round's scope. So the glass takes no wheel, and plain wheel still never turns anything. I did not edit the check.
- **A DOM overlay for the screen's controls.** Ruled out by the brief: a GBC app would not do it, and the tile rule could not be held.
- **Following the raw quadrant inside a blended class.** It chatters on the line, which the human asked to avoid.

## Verify

`./verify fast` was run on the committed lab hash. The result and hash are in the PR and the report, read from `.harness/last-verify.json`.

## Open questions

- **For the human:**
  - Inside a parity class the editor moves only where one corner holds more than 0.6 of the class, so it stays put along the middle. Is that the "crossing" meant?
  - Should a corner picked by hand stay pinned until it is released?
  - Horde's corners are now round like Sluice's. Are the numerals and inks enough to tell them apart?
- **For the lead:** alt + wheel on the glass needs this lab added to `lab_wheel_scroll_check`'s LABS inventory (`state: 'LAB'` or similar) and an `onAltWheel` copy. That is a small follow-up that touches the check file.
- **Not verified:** only still frames and scripted events were checked. No one used the glass by hand, and no audio was listened to.
