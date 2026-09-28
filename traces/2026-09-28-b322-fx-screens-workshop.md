# b322-fx-screens-workshop — MAW and Sluice as software on horde's screens: a comparison lab of six retro-future treatments in one chassis

- **Queue item:** B322. I read the row verbatim from `origin/lead-records-125:ROADMAP.md` (records PR #817), with B302, B317, B318, B319 and B321 beside it. The human, 2026-09-28: "not exactly a decision until we test it out, but I kind of like the idea of MAW and Sluice having the appearance of software running on screens within Horde … I want them to have a sort of retro-futuristic appearance. Let's workshop some options … I might start working on a sort of Gameboy Color style logo for each of Sluice and Maw."
- **Why:** the human wants to compare options before deciding. So this is a comparison lab and says so at the top. The CHASSIS stays constant (that is the coherence) and the SCREEN changes per treatment (that is the individuality). Each treatment is shown for both modules. The logo slots are ready for the human's pixel art.
  - New: `docs/design/fx-screens-workshop.html` and `docs/design/assets/fx-logos/README.md`. No image is committed.
  - Not touched: `src/`, `specs/`, `reference/`, ROADMAP, DECISIONS, LIBRARY, INDEX, `docs/design/index.html` and the other labs. Nothing was deleted.

## What the lab is

- **The chassis** is horde's card (the SCALPEL lab's tokens, knob skin and window language), with the well-alt bezel and a 160U × 144U screen (U = 2 in the overview, 4 full-size). Under the screen are **four soft keys and four knobs**. The chassis prints only their numbers. The software labels them in a band at the foot of its screen, directly above each key and knob.
  - MAW's knobs are its ADR-169 roles (Amount, Tone, Motion, Regen). Its keys are S1, S2, S3 (select a stage) and ROUTE (cycle the five routes; Regen reads IDLE outside the feedback route).
  - Sluice's knobs are macros 1–4, bound by order (Sluice notice seq 5, addendum 2). Its keys are NEW, RAND, SYNC and LOCK: a seeded new patch, a seeded redraw on fz.h's threshold law, cycling the delay's division by name, and the leveler's tri-state lock.
  - The knobs and keys work. They drive the mock, and every chassis of that module follows.
- **The treatments.** All six from the brief were kept. The rationale for each is on the page.
  1. **GAME BOY COLOR LCD:** 160×144, a 3×5 pixel font, the dot grid and ghosting. It draws into a 2-bit index buffer with a per-tile palette map, so the hardware rule (one 4-colour palette per 8×8 tile) holds by construction. Its four palettes are built from the screen scheme's roles and rounded to RGB555.
  2. **PHOSPHOR CRT TERMINAL:** a 64×36 character grid with box drawing, inverse-video keys, dumb-terminal plots, an ASCII network, bloom, scanlines and persistence. It overrides the scheme: MAW is P3 amber and Sluice is P1 green.
  3. **VECTOR MONITOR:** strokes on black. MAW's curve is an XY scope (input on X, output on Y). Sluice is a wireframe network with pulses. Text is stroked in the 14-segment font. It takes the dark schemes' inks, and on a pale scheme it draws with TUBE's inks.
  4. **80s WORKSTATION:** 320×288 at 1 bit with Bayer dithering, plus a menu bar, overlapping windows, radios, sliders and dialog buttons. Paper is the scheme's tube and ink is its physics ink.
  5. **SEGMENT GLASS** (refines "segmented LCD"): 7- and 14-segment glyphs, bar graphs and icon segments, with the ghosts of unlit segments visible. It is a reflective LCD on a pale scheme and a VFD on a dark one, so it takes the scheme rather than overriding it.
  6. **TELETEXT PAGE:** 40×24 cells with 2×3 sextant mosaics, double height, flashing, one foreground per cell, and Fastext keys mapped onto the chassis's four soft keys. It overrides the scheme. Spacing attributes are not simulated.
  - Considered and not built: a plasma/gas-discharge panel (too close to the amber CRT) and e-paper (no motion).
- **Logo slots.** TITLE is **96×24** (12×3 tiles) and ICON is **16×16** (2×2 tiles), in logical pixels.
  - Files: `docs/design/assets/fx-logos/{maw,sluice}.png` and `{maw,sluice}-icon.png`.
  - Each file is shown at the largest whole-number scale that fits its slot, with `image-rendering: pixelated`. On the GBC, one logo pixel is one screen pixel (×4 full size, ×2 in the overview); elsewhere the scale is ×2 or ×1.
  - The palette rule: at most 4 colours per 8×8 tile (transparent counts as one), at most 8 palettes, and RGB555. The page lints a dropped file against it.
  - A labelled placeholder (caution inks, hatch, name and size) is drawn in code when a file is absent.
  - `?logotest=1` shows generated TEST patterns, labelled as tests.
- **Interaction:** a treatment picker plus ALL SIDE BY SIDE; a module toggle (BOTH, MAW, SLUICE); the theme chip; the screen chip (gui2's five schemes, with the theme picking the default pair); motion on or still; and `?still=N`, `?view=`, `?mod=`, `?scheme=`, `?theme=`.
  - **Cost:** canvas and CSS only, no WebGL. The overview repaints a third of its screens per frame, and offscreen chassis are skipped (IntersectionObserver).
  - **No wheel listeners**, so `lab_wheel_scroll_check`'s inventory is untouched.

## Evidence

- **VERIFIED: in-page self-checks 7/7, with 11/11 must-fail controls caught.** Read from headless Chrome's dumped DOM (served on 8322, `?still=120`, overview, light, ORCHID).
  1. **Every treatment renders both modules with all their content:** 12 renders. Each has its module's content marks and ink over 1% of the screen.
     - CONTROLS: a blank screen that declares every mark, and a CRT Sluice missing its network. Both caught.
     - The marks are self-declared by each renderer; only the ink test reads pixels.
  2. **Logo slots and pixel screens integer-scale:** 28 surfaces, every one ×1 or ×2 in the overview (×4 full-size), pixelated, and on the logo-pixel grid.
     - CONTROLS: ×2.5, and ×2 with image-rendering auto. Both caught.
  3. **Placeholders show exactly when files are absent:** 24 of 24 slots.
     - CONTROLS: a missing file through the same loader, and a generated PNG. Both caught.
  4. **The GBC tile rule:** 4 frame buffers read back (1440 tiles), max 4 colours per tile, 0 pixels off RGB555.
     - CONTROLS: a 5-colour tile and an off-grid colour. Both caught.
  5. **Scheme policy:** gbc, ws and seg respect the scheme; crt and ttx override; vec is dark-only. Each was rendered under TUBE, ORCHID and DUSK.
     - CONTROL: a renderer that claims "respects" but ignores the scheme. Caught.
  6. **Deterministic:** 12/12 renders repeat bit for bit at frame 97.
     - CONTROL: a Math.random renderer. Caught.
  7. **Every chassis says MOCK, and Sluice says PROVISIONAL:** 12 chassis.
     - CONTROL: an untagged chassis. Caught.
- **The loaded-file path, exercised once:**
  - A scratch 96×24 PNG was placed temporarily as `maw.png`.
  - Results: check 3 read "3 absent, 1 present", check 2 read ×4, and the lint printed "96×24 ✓ · max 4 colours/tile ✓ · 2 palette(s) of 8 ✓ · 8 px off RGB555".
  - The file was deleted before the commit.
- **Found while building:**
  - `while (sl.firstChild) sl.removeChild(…)` never ends under `lab_load_check`'s stub DOM (every property is a truthy stub). It is replaced by `innerHTML = ''`, and the comment says why.
  - Teletext's single-height text was drawn at the page origin (missing the cell offset). Screenshots caught it, and it is fixed.
  - Under the `(k<<3)|(k>>2)` expansion, 0xF8 is not on the RGB555 grid, so the test art uses on-grid values.
- **Verify:** `./verify fast` exit 0 on the lab commit. `.harness/last-verify.json` reads `{"target":"fast","exit":0,"git":"09324a5","ts":"2026-09-28T05:01:56Z"}`.
  - `lab_load_check` is GREEN standalone: 57 labs loaded, 0 broken, 1 skipped (the new lab included).
  - `lab_wheel_scroll_check` is GREEN: 6 labs, 2 planted faults.
  - `private_name_gate` was SKIPPED by verify (`.leakcheck-names` is absent in this worktree). I ran the same pattern file by hand against both new files and found 0 matches.
  - This trace's own commit was re-verified; the hash is in the PR.
- **Not verified:**
  - Frame cost in a real (non-headless) browser was not measured.
  - The animation (ghosting, persistence, pulses) was checked in code and in still frames, not watched.

## Screenshots (scratch, not committed; `scratchpad/b322/shots/`)

- 01 overview light (ORCHID) and 02 overview dark (TUBE), 1600 wide.
- 03–08: each treatment full-size with both modules, light: gbc, crt, vec, ws, seg, ttx.
- 09: GBC dark with `?logotest=1` (the loaded-logo path, TEST art).

## Alternatives rejected

- **A different chassis per treatment:** that would lose the coherence the human asked for. Only the glass changes.
- **Drawing MAW from MawCore:** the brief allows a mock, and the MAW lab already renders the real thing. Real renders belong in whichever treatment is chosen.
- **WebGL for CRT bloom:** the brief says no heavy WebGL. A canvas blur plus CSS scanlines is enough to judge the look.
- **Committing sample logos:** the brief says no image the human hasn't made. Placeholders are drawn in code.

## Open questions

1. Which one or two treatments go forward, to be built against real renders?
2. One glass for all FX (coherence), or one per module (individuality)?
3. The chassis contract: every FX face is four knobs and four keys, and everything else is on the screen. Is that right?
4. Mono logo variants for CRT, vector and the 1-bit workstation: a second file, or a tint in code?
5. Should the screen schemes reach FX screens at all?
6. Where does Sluice's RANDOMNESS live? It is not on the four knobs.
7. How big is the screen? At 640×576 full size, it is most of a 980×720 window.
8. Flashing (teletext, segment PROV): keep it, slow it, or drop it (photosensitivity)?
9. The CRT bloom's cost as a plugin shader has not been measured.
