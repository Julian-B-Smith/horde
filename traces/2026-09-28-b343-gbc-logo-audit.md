# b343-gbc-logo-audit — the GBC titles on one letter-metric set, measured from their pixels

- **Queue item:** B343 (ROADMAP on records PR #833, branch `lead-records-135`; follows B326, B336, B338).
- **Why:** The human: "the GBA sluice logo needs a consistency audit; not all characters are
  properly spaced or sized, and there's a strange stripe across the 'I'" (the Game Boy Color
  glass). **The stripe's cause was the glyph DATA.** B326 drew the Sluice I as a "lifted gate": at
  logo rows 14-17 its stem carried a 1-row outline, a light-cyan (`b`) row, a blue row and another
  1-row outline, so the I was three pieces with a lighter band under a darker one. It was not a
  tile-palette boundary (row 15 sits in tile row 1, whose palette legally holds `b` and `c`; the
  8-px boundary at row 16 only made the band look deliberate), not ghosting (the workshop's logo is
  an overlay canvas outside the ghosted frame buffer; the lab's ghosting is off in stills), not
  scaling (whole-number, pixelated; self-check 2), and not anti-aliasing (every pixel on the
  palette, measured). The same audit found the rest of the human's complaint: L and U stood a row
  short (rounded stem tops), the I's stem was 5 px against 4 and its serifs 3 rows against 4, and
  the optical gaps ran 4.1-6.8 px (the L's open side and the I's straight sides unaccounted).
- **What changed:**
  - `docs/design/fx-screens-workshop.html` (branch `fx-screens-rack`): the Sluice GBC title redrawn
    on one metric set (cap 18 px on rows 3-20, 4 px stems and bars, 3 px counters, the outer corner
    of every stroke turn rounded 2 px, terminals flat, a 1 px outline plus a 1 px drop shadow, the
    waterline gradient and palette kept, the I a plain 4 px stem). Box gaps 4/3/5/5/3, optical
    5.0/5.5/5.2/5.3/5.0 px. SINGE (the rack's Drive) re-spaced only, letters untouched: box gaps
    2/2/2/2 (optical 5.4/4.3/2.5/2.9) → 1/2/4/3 (optical 4.4/4.3/4.5/3.9); sparks ride with their
    letters. MAW measured clean and is unchanged. Section M2 adds `glyphAudit()` and `GA_TOL`;
    self-check 15 runs it on all three GBC titles and checks each title sits on the 8×8 tile grid.
  - `docs/design/assets/fx-logos/README.md`: the lettering rule and its tolerances.
  - `docs/design/sluice-horde-lab.html` (branch `lab-sluice-r2`): the same title (logo data hash
    identical to the workshop's), `glyphAudit`/`GA_TOL` copied verbatim (section N3, code hash
    identical), C27. C17 still finds 0 (530 needles).
- **How "optically even" was decided:** an area method in the manner of the usual letterspacer.
  Per row of the word's cap band: the blank between the two glyph boxes plus how far each glyph's
  profile recedes from its box, each recess clamped at 3 px (1/6 of an 18 px cap, about the
  letterspacer's 15 % depth), averaged into an effective gap in px. Box gaps were then chosen by
  exhaustive search so the effective gaps span ≤ 1 px (a pixel is the grain; any finer is not
  reachable on a pixel grid). This is why the I gets the widest box gaps (straight on both sides)
  and L-U the narrowest. The tolerance was stated before the search, not fitted to it.
- **Tolerances (GA_TOL):** cap top and baseline 0 px; stem and bar ±1 px of the word's mode (a
  diagonal's horizontal section reads a pixel off); optical gap span ≤ 1.0 px; stripes (a lighter
  band under a darker one down a stroke, or a 1-row outline with fill directly above and below; the
  thinnest designed counter in any GBC title is 2 rows), pieces > 1 and off-palette or
  part-transparent pixels: 0.
- **Controls:** workshop self-check 15 and lab C27 each: B326's pre-fix Sluice title (fails: 6
  faults incl. the stripe across the I: L/U cap row 4 vs 3, I bar 2 vs 4, I in 3 pieces, gap span
  2.72, stripe at I rows 14, 15, 17); the E a row short (caught, cap); the E moved 2 px right
  (caught, gaps); a light-cyan row across the U (caught, stripe); one blended pixel (workshop) or
  one half-transparent pixel (lab) (caught, palette). Workshop: 15/15 checks, 42/42 controls; lab:
  26/26 pass (with the Sluice spec linked through the gitignored `local/sluice`).
- **Evidence consulted:** ROADMAP rows B322/B326/B336/B338/B343 (lead-records-135);
  `docs/design/assets/fx-logos/README.md`; the workshop's K2 data, `propCanvas`/`fillSlot`/`drawGBC`;
  the lab's N2 (`GBC_SLOT`, `logoFromGrid`, `paintGBC`, C26); renders at 1×, 3× and 8× with a metric
  overlay; headless-Chrome self-check tables for both pages.
- **Alternatives rejected:** keeping the gate idea as a slab-serif I (its serifs would need 4-row
  bars to match, making the I a block heavier than its neighbours; the gate itself was the fault);
  fixing SINGE by narrowing the I's serifs (spacing alone reached the tolerance, so the letters
  stay the author's); a stripe rule keyed to the tile rows (would have missed a stripe inside a
  tile, and passed the I's band, which the tile palette permits).
- **Verify:** `./verify fast` exit 0 on `lab-sluice-r2` at 299e92d (`.harness/last-verify.json`);
  the `fx-screens-rack` run is on the commit that carries this trace (hash in the PR comment and the
  lead's report).
- **Open questions:** MAW's A measures a 5 px stem against M/W's 6 (inside ±1, but a diagonal of
  horizontal section 5 is optically lighter still); the GBC icon slot sits at (112,4), off the tile
  grid by 4 rows (legal as sprites, not as background tiles); the audit binds proposals, not a
  file the human drops; the SINGE check spells the rack's first name, so re-spelling the art means
  updating `GBC_LETTERING.drive.word`; only still frames were seen.
