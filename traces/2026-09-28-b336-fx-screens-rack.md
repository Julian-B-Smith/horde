# b336-fx-screens-rack — every FX module named (proposals) and given one screen style in the fx screens workshop

- **Queue item:** B336. I read the row verbatim from `origin/lead-records-133:ROADMAP.md` (records PR #831), with B50, B288, B322 and B326 beside it. The human, 2026-09-28: "Sluice and Maw FX screens are amazing. I may want to use each of these styles on a different module; it's making me want to audition the reverb and other FX modules in similar styles, maybe give them each their own name".
- **Why:** the human wants each style on a different module, and every module named. So the workshop gains THE RACK:
  - every FX module that survives the rebuild gets 3–5 proposed one-word names, each with a reason;
  - each module gets ONE screen treatment, one-to-one across the rack;
  - each screen shows its lab's real four-knob face and soft keys (mocked);
  - each module gets a logo proposal in its own glass.
  - Everything is a proposal. The names, the styles and the logos are the human's to pick.
- **Files changed:** `docs/design/fx-screens-workshop.html` (section O, the rack view, self-checks 10–14, the notes) and `docs/design/assets/fx-logos/README.md` (the new drop-in file names).
- **Not touched:** `src/`, `specs/`, `reference/`, ROADMAP, DECISIONS, LIBRARY, INDEX, `docs/design/index.html`, the SCALPEL lab, the composed engine and the other labs. No PNG is committed, nothing is written to `assets/fx-logos/`, and nothing was deleted.

## Inventory (from the FX labs and the rack)

- **Screened (10):**
  - Drive, Filter, EQ, Comp, Echo and Reverb: `fx-design-lab.html` (B210/B234).
  - Delay and Room: the rack's own params, since no lab has a card for either (`src/delay_core.h:55-65`, `src/fx_rack.h:714-745, 813-829`, `src/hypersaw_clap.cpp:554-560`).
  - MAW and Sluice: already on the page (B322).
- **Not screened:**
  - Comb: it becomes a filter type (B288).
  - Gain, and the notch, phaser and filter swarms: buried or nixed in roster Revision A (`docs/proposals/fx-matrix-rework.md` Part 3).
  - OTT (B63), Disperser, Shifter, the standard phaser and chorus: on the roster, but no lab defines their controls, so there is nothing true to draw. They are listed on the page.

## The proposals

| module | glass | names (first = lead's pick, spells the logo) | logo |
|---|---|---|---|
| Drive | GBC LCD | SINGE · KINDLE · SCORCH · GRIT | singed paper (cream, a glowing orange line, a charred edge), a lit match |
| MAW | phosphor CRT | (named) | B326's |
| Filter | vector monitor | BALEEN · GILL · WEIR · SIEVE | beam strokes over baleen plates tracing a resonant lowpass; icon: the response |
| EQ | teletext | CHISEL · CONTOUR · TERRACE · ISOBAR | six letters in the six band colours; icon: analyser bars |
| Comp | nixie + neon (NEW) | LUNG · BELLOWS · VISE · CINCH | four nixie tubes over an IN-13 bargraph; icon: the bargraph and a lamp |
| Sluice | 80s workstation | (named) | B326's |
| Echo | sonar scope (NEW) | FATHOM · HOLLER · CANYON · CHIRP | phosphor raster over range rings; icon: the PPI |
| Delay | dot-matrix LCD (NEW) | RELAY · SPOOL · SHUTTLE · TETHER | LCD big font from 8 custom characters; icon: repeat bars |
| Room | CAD blueprint (NEW) | CELLAR · PARLOUR · BOOTH · BUNKER | a plan outline with a door swing and a dimension line |
| Reverb | segment glass | NAVE · VAULT · CISTERN · GROTTO | 14-segment glyphs and a falling bar ladder; icon: a vault |

- **The four new glasses:** ten modules on six glasses cannot be one-to-one, so four were added. Each suits one module better than any glass left over.
  - A sonar screen is the literal picture of an echo.
  - A character LCD is the textbook rack delay's display.
  - A CAD plan draws a room.
  - Nixies and neon bargraphs are a compressor's numbers and its gain-reduction meter.
  - They sit outside `TREATMENTS`, so B322's views and the checks that hold every treatment to MAW and Sluice are unchanged.
- **Naming families:** two surfaced unprompted: anatomy (MAW, GILL/BALEEN, LUNG) and water (SLUICE, WEIR, CISTERN, FATHOM). Both are offered to the human as a possible rule.
- **Delay and Room faces are PROPOSED here**, and their chassis say so:
  - Delay: Mix · Bright · Offset · Feedback. Feedback is the rack's amount × 1.08. Time is on the soft keys.
  - Room: Echo's face.

## Evidence

- **VERIFIED: in-page self-checks 14/14, with 37/37 must-fail controls caught.** B326 had 9 checks and 20 controls. Read from headless Chrome's dumped DOM, served on 8336 at `?still=120`, in three views: THE RACK (light), the default overview, and the teletext view (dark).
  - **10. Every rack module renders in its own glass with all its content marks, and ink over 1%:** 10 renders.
    - Controls: a blank LCD that declares every mark; a nixie Comp without its knob band. Both caught.
  - **11. One-to-one:** 10 modules on 10 treatments, each used once.
    - Controls: EQ planted on the segment glass; Room on a treatment that does not exist. Both caught.
  - **12. Names:** 32 names, 4 per module. Each is one word of 3–8 letters with a reason, none is one of the 31 reserved words, and none repeats.
    - Controls: a two-word name, a module with two names, MAW again, one name on two modules. All caught.
  - **13. Rack logos:** 16 proposals at 96×24 and 16×16, ink 8–100%.
    - Rules held: GBC by the page's own lint; LCD at ≤ 8 custom characters and no dot in a gutter; nixie one lit cathode per tube; sonar phosphor levels only; CAD one pen.
    - Controls: a 5th colour in a SINGE tile, a gutter dot, a 9th custom character, two lit cathodes, an off-phosphor level, a second pen. All caught.
  - **14. The chassis contract (4 knobs, 4 keys) and each knob against a copied table of the lab's face slots** (`fx-design-lab.html:655-663, 999-1002, 1047-1050, 1101-1104, 1432-1437, 1493-1496`). The 3 slots the lab draws unbound are drawn UNBOUND.
    - Controls: a three-knob Comp, a relabelled Decay knob, the Drive's unbound slot drawn live. All caught.
  - **Existing checks extended:**
    - Check 4 now reads the Drive's GBC buffer too.
    - Check 5 holds the four new glasses to their scheme policy (sonar and nix override; lcd and cad respect).
    - Check 6 covers 22 renders.
- **VERIFIED: B322's screens are unchanged.**
  - Method: B322's 12 check renders (6 treatments × MAW/Sluice), under TUBE, ORCHID and DUSK, hashed in scratch copies of `origin/main`'s file and of this branch's file.
  - Result: 36/36 identical. This covers the refactors of `drawTTX` and `ttxSoft` and the `abbr` fallback in `vecSoft`.
- **Found while building (from the renders):**
  - The first rack layout grouped modules in family boxes and left dead space; it is now one packed row with a family tag per cell.
  - The EQ analyser sat below the frame.
  - LUNG's upright bargraph read as a fifth letter ("LUNGI"); it now lies under the tubes.
  - The first RELAY gave R two straight legs and read as A. R now has a diagonal leg, still within 8 custom characters.
  - The sonar's range labels piled up at the top; they now run down the empty lower axis. A phase offset puts the still frame's sweep in the tap sector.
  - The 14-segment stick text read D as "]" on the CAD screen and lost decimal points. The CAD screen text is now a thin monospace; the logo keeps its stroke font, because a logo takes no system font (B326).
  - **The 14-segment helper's decimal point sits against the next digit's foot and vanishes** ("2.0K" read as "20K", "2.2" as "22"). My screens draw the point as its own mark (`dotted()`) or avoid decimals. **B322's MAW drive readouts on the vector and segment glasses share the helper and have the same fault; I left them as they are.**
- **Verify:** `./verify fast` exit 0 on the lab commit. `.harness/last-verify.json` reads `{"target":"fast","exit":0,"git":"04897e9","ts":"2026-09-28T17:06:35Z"}`.
  - `lab_load_check` is GREEN standalone: 58 labs loaded, 0 broken, 1 skipped.
  - `lab_wheel_scroll_check` is GREEN (no wheel listener added).
  - `private_name_gate` RAN this time: the gitignored `.leakcheck-names` was copied into the worktree, untracked. It found 0 hits.
  - This trace's own commit is re-verified; the hash is in the PR.
- **Not verified:**
  - The animation was seen only as still frames.
  - Frame cost with ten screens live in a real browser was not measured.
  - Legibility at ×1 was judged from screenshots.

## Screenshots (scratch, not committed; `scratchpad/b336/shots/`)

- `01-rack-light.png` and `02-rack-dark.png`: THE RACK, 1600 wide.
- `03`–`10`: each new module full size beside its card, light: drive, filter, eq, comp, echo, delay, room, reverb.

## Alternatives rejected

- **Sharing the six glasses (several modules per glass):** the human's idea is one style per module, and the brief allows new treatments where one fits better.
- **Adding the new glasses to `TREATMENTS`:** each would then have to draw MAW and Sluice too. That is four more renderer pairs nobody asked for, and it would change B322's views.
- **Faces for OTT, Disperser and Shifter:** no lab defines their controls, and inventing controls to fill a screen would be a claim about the engine.
- **Logos spelled with the generic type name (REVERB):** the brief asks for named marks, so each spells the lead's pick and the human's pick re-spells it.

## Open questions

1. Which names? One family (anatomy or water) as a rule, or the best word per module?
2. Keep all ten glasses one-to-one, or cut back to the six and let modules share?
3. Ratify the Delay and Room faces, or send both to the FX design lab first?
4. Echo and Room are on trial. If either goes, its glass is free.
5. Which unscreened module (OTT, Disperser, Shifter, phaser, chorus) gets a lab first?
6. Letterform limits: the teletext mosaic fits six letters and the LCD big font five. A longer pick needs a narrower form there.
7. Should the LCD's shared 8 custom characters (screen and splash) be kept as a rule?
8. Should B322's MAW decimal readouts get the same visible-point fix (the vector and segment glasses)?
