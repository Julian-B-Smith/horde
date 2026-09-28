# b326-fx-logo-proposals — lead logo proposals for MAW and Sluice, one per screen treatment in the fx screens workshop

- **Queue item:** B326. I read the row verbatim from `origin/lead-records-128:ROADMAP.md` (records PR #821), with B322 beside it. The human, 2026-09-28: "the screens are amazing; I'm having a hard time choosing. In the meantime, could you take a swing at the logos? The aesthetic in each of these is impeccable. You could try a different one for each style".
- **Why:** the human is still choosing between the six treatments from B322 and asked for logos in the meantime. So each treatment gets its own MAW mark and its own Sluice mark, TITLE 96×24 and ICON 16×16. Each is drawn in that medium's own terms and shown in the workshop's logo slots as a "lead proposal". Nothing here is a ruling.
  - Changed: `docs/design/fx-screens-workshop.html` (new section K2, the slot precedence, the LOGOS toggle, the gallery, and self-checks 3, 8 and 9) and `docs/design/assets/fx-logos/README.md` (a "Lead proposals" section).
  - Not touched: `src/`, `specs/`, `reference/`, ROADMAP, DECISIONS, LIBRARY, INDEX, `docs/design/index.html` and the other labs. No PNG is committed, none was written to `docs/design/assets/fx-logos/`, and nothing was deleted.

## The marks

The through-line: **MAW is a jaw** (teeth, heat, a clipped signal) and **Sluice is water** (a waterline, a gate, a loop). Each medium says that in its own vocabulary.

| treatment | MAW | Sluice |
|---|---|---|
| GBC LCD | M and W mirror each other as upper and lower jaws. The fire gradient is banded one palette per tile row, with orange fangs biting down and white-hot fangs biting up. Icon: a front-on jaw. | Rounded letters in a foam-to-navy gradient with a sine waterline. The I is a lifted sluice gate with water running under it. Icon: a whirlpool. |
| Phosphor CRT | Double-line box-drawing capitals ("Calvin S") on the terminal's own 5×8 cell, in P3 amber, trailing a ▓▒░ heat haze. Icon: ▼▼▼ over ▲▲▲. | The same capitals in P1 green, trailing ≈ water. Icon: a double-line loop with a ◄ return arrow. |
| Vector | The beam draws M, A and W as a waveform clipped at two rails. The flats burn white where the beam dwells. Icon: two rows of teeth in a mouth. | A stroked wordmark riding a sine waterline that exits through an arrow. Icon: a loop arrow around a ripple. |
| 80s workstation | MacPaint: solid capitals knocked out of a plate filled with the zigzag pattern (teeth), with a 50% shadow. Icon: a dithered jaw with white teeth. | The same plate filled with the wave pattern. Icon: a sluice gate over a channel. |
| Segment glass | Three 14-segment glyphs and a VFD drive meter whose last two bars are the clip segments. Icon: a jaw of triangular segments. | SLUICE in 14-segment, a gate segment lifted above its ghosted closed position, and a channel of wave segments with one pulse lit. Icon: a ring of arc segments with its arrow. |
| Teletext | A red banner with MAW in yellow contiguous mosaics at the two-sextant stroke of a Ceefax headline. Icon: a four-sextant zigzag bite. | A blue banner with SLUICE in white separated mosaics. Icon: a loop. |

## Mechanics

- **Authored as data:**
  - GBC: pixel-grid strings with an RGB555 palette.
  - Workstation: 1-bit grids, using the scheme's paper and ink.
  - CRT: cell maps plus per-cell attributes. Box drawing is built as a thick path whose boundary is lit, which gives CP437's double lines with correct joins.
  - Vector: stroke paths, arcs and waves.
  - Segment glass: segment glyphs and shapes, with unlit segments ghosted.
  - Teletext: sextant grids plus one foreground and one background per cell.
  - A scratch Python generator drew the GBC and workstation masks and the resulting grids were pasted in. The generator is not committed; the grids are the design.
- **Slot precedence:** test art, then a present file (except in PLACEHOLDERS), then the proposal (in PROPOSALS), then the placeholder.
  - **LOGOS toggle:** PROPOSALS (the default), FILES (B322's behaviour) or PLACEHOLDERS (the canvas guides only).
  - `?logos=` selects a mode.
  - The TEST chip is kept and renamed "test art".
- **Gallery** (the LEAD LOGO PROPOSALS box): every proposal at ×4 and ×1 on its medium's ground, with its concept, its lint or medium rule, and export links.
  - Exports: 48 PNGs (title and icon, at ×1 and ×4). GBC and workstation exports are clear where the art is clear; the glass media's exports carry their ground.
  - The gallery is cached per screen scheme, because it draws about 100 canvases.
- **CRT slots moved** by half a unit, from y 4.5 to 4, so a cell-map logo lands on the terminal's text rows.

## Evidence

- **VERIFIED: in-page self-checks 9/9, with 20/20 must-fail controls caught.** B322 had 7 checks and 11 controls. Read from headless Chrome's dumped DOM (served on 8326, `?still=120`, overview):
  - **Check 3** now checks each slot against its mode.
    - PROPOSALS shows 24 proposals, FILES 24 placeholders and PLACEHOLDERS 24 placeholders; all three pass.
    - Controls: a missing file and a generated PNG, each in FILES and in PROPOSALS mode. In PROPOSALS mode the file beats the proposal. All four caught.
  - **Check 8** runs the page's own `lintLogo` on the GBC proposals:
    - The titles are 96×24 with at most 4 colours per tile, 3 palettes and 0 px off RGB555.
    - The icons are 16×16 with at most 4 colours per tile, 2 palettes and 0 px off RGB555.
    - Controls: a 5th colour in a full tile (caught, 1 tile over), an off-grid palette colour 0x95 (caught, 98 px) and art needing 9 palettes (caught, it counted 10).
  - **Check 9:** 24 proposals, all at the README's sizes, with ink covering 13 to 100% of the canvas, and every medium rule held.
    - Controls: a grey pixel in the 1-bit art, a third colour in a teletext cell, a glyph the terminal lacks, and a stroke leaving the canvas. All caught.
- **Independent read-back of the exports:**
  - I decoded the 48 export links from the dumped DOM: all 48 are the right size.
  - A separate Python lint of the four GBC 1× PNGs gave a maximum of 4 colours per tile, 3/2/3/2 palettes and 0 px off RGB555.
- **Found while designing (from the renders):**
  - The GBC Sluice "I" as a gate with a wheel read as a T ("SLUTCE"). I redrew it as a beam, a lifted slab and a sill.
  - The MAW icon's first teeth read as glyphs, not teeth.
  - The teletext MAW with fangs read as "MAWI". I dropped the fangs and made the strokes two sextants wide.
  - A colour change at mid-height cut the teletext letters in half, so each teletext title now uses one colour.
  - The vector MAW icon's teeth crossed into an X mesh. I opened the jaw.
- **Verify:** `./verify fast` exit 0 on the lab commit. `.harness/last-verify.json` reads `{"target":"fast","exit":0,"git":"3adc32e","ts":"2026-09-28T07:02:23Z"}`.
  - `lab_load_check` is GREEN standalone: 57 labs loaded, 0 broken, 1 skipped.
  - `lab_wheel_scroll_check` is GREEN.
  - `private_name_gate` was SKIPPED by verify (`.leakcheck-names` is absent in the worktree). I ran its patterns by hand against both changed files: 0 matches.
  - This trace's own commit was re-verified; the hash is in the PR.
- **Not verified:**
  - I have seen only still frames. I checked in code that the proposals do not animate.
  - No real host or plugin render was tested.
  - Legibility at ×1 was judged by eye from screenshots on a 2× display.

## Screenshots (scratch, not committed; `scratchpad/b326/shots/`)

- `01`–`12`: each treatment full-size with both proposals in place, light (ORCHID) and dark (TUBE). The order is gbc, crt, vec, ws, seg, ttx, with light and dark alternating.
- `13-gallery-light*` and `14-gallery-dark*`: the whole proposals gallery; `-a`/`-b` are crops.
- `13-gallery-light-gbc.png` and `14-gallery-dark-gbc.png`: GBC close-ups at ×4. `01`/`02` also show the GBC logos in place at ×4.

## Alternatives rejected

- **One logo drawn once and tinted per medium:** the human asked for "a different one for each style". The shared concept keeps the family instead.
- **Rendering letters from a system font:** that raises a licensing question for a logo. Every glyph here is hand-built from rectangles, polygons, strokes or the page's own segment and sextant tables.
- **Writing sample PNGs into `assets/fx-logos/`:** the brief forbids it. The dropped-file path is exercised by the data-URL controls instead.

## Open questions

1. Which marks survive, per glass? Does one concept (jaw / water) carry across every glass, or should the chosen glass get its own?
2. The PLACEHOLDERS mode hides a present file (it shows guides only). Is that the right third mode, or should files always win in every mode?
3. The glass media's exports carry their ground (CRT, vector, segment, teletext). Transparent versions would be a small change if wanted.
4. The teletext and CRT titles are coarse by nature at ×1. Should the logo slots on those glasses be larger than 96×24?
