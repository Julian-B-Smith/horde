# b334-scalpel-lab-round11 — logo styles on a click; the logo centred; gui2's four-way morph gradient; blade 2's overflow under a Cut rule

- **Queue item:** B334. I read the row verbatim from `origin/lead-records-133:ROADMAP.md` (records PR #831), with B297, B315, B317 and B323 beside it. The human, 2026-09-28:
  1. "Could we test a stylized drop shadow under the horde logo? Or perhaps a small inner shadow, giving the fill some depth within the stroke? Also the logo sits a little high in its frame. I'm also considering adding a mild gradient to the fill. Maybe we can audition a few styles that I can cycle through by clicking the logo."
  2. "can we return the 4-way color gradient to the morph XY from the original device?"
  3. A screenshot: blade 2's SPREAD group under GOLDEN, with the struck "Cut rate" label and "Rule depth" clipping at the card's right edge.
- **Why:** This is round 11 of B271's lab. The only file changed is `docs/design/scalpel-interface-lab.html`, plus this trace. The engine file (B335 in flight), `docs/design/index.html`, `src/`, `specs/` and `reference/` were not touched.
- **Evidence consulted:**
  - `src/gui/gui2.html`: `paintMorphField` :2833-2842 (the one morph-field painter, which the big pad and MAIN's mini both call); `MCOLORS` :1941; the corner tokens :51-55 and :200-203; `drawMorphMini` :4609-4619.
  - The lab's E2 (the logo), `paintMorph`, `bladeRow`, and the CSS for `.kgrp`/`.mgrid3`.
  - The round 8, 9 and 10 traces.

## Measured first

- **Overflow** (headless Chrome, OSC, light theme, oracle defaults with blade 2 on, `kRule` = GOLDEN). Blade 2's face needed 667 px of a 630 px card.
  - The groups: CUT 238, MIRROR 166 (a three-column grid for its extra "=1"), FM 100, SPREAD 148 (three knobs under a rule: Position, the inert Cut rate, and Rule depth, B297).
  - The flex row squeezed the mirror to its minimum (142.7 px, with "twin −" broken onto two lines) and still ran 14 px past the card (the SPREAD group +5.7 px past its border).
  - Blade 1's mirror is 114 px, so blade 1 fits.
- **The logo** (91×60 box, which spans the frame's top padding to the body's top, B323). Over 64 moments of the warp, the mark's alpha-weighted centre of mass averaged **5.2 CSS px above** its box's middle, with the text baked at 0.38 of the box.
  - At 0.42-0.46, the deepest blob pull reaches the box's bottom row at 1 moment in 128. At 0.38 it never does (lowest 56.5 of 60).

## What changed (`lab-review` meta `B271 + B293–B334 · 2026-09-28`)

1. **Logo styles** (E2: `LOGO_STYLES`, `logoCycle`, `logoInset`, `logoDrop`, `logoGradient`). A click cycles six styles:
   - **FLAT:** as before.
   - **DROP:** a crisp offset shadow (1.5, 2 CSS px) in the fill's third triad colour (turned 240°; the edge is 120°), pulled 35% toward the chassis ink.
   - **INSET:** an inner shadow inside the stroke. It is the fill darkened, cast 1.2 px down with a 1 px blur and clipped to the fill.
   - **GRADIENT:** the fill lifted 24% toward white at the top of the letters and lowered 14% toward black at their foot.
   - **INSET + GRADIENT:** both.
   - **SOFT DROP:** a blurred drop. On the light chassis it is the ink at 38%; on the dark one it is the fill at 55%, which reads as a glow.

   How it works:
   - Every colour derives from the fill of the moment, so each style still shifts with the morph and the drift.
   - The shadows use the canvas shadow with an off-canvas source, not `ctx.filter`, because WebKit's support for it is recent.
   - The choice is saved to `localStorage` under the key `scalpel-lab.logo-style`, in try/catch. `?logo=<id>` overrides it.
   - The name shows under the mark in an absolute `.logoname` pill. A CSS fade removes it, and no JS timer is involved.
   - The canvas takes the pointer again (B323 had turned it off).
2. **Centring.** `LOGO_TEXT_Y` 0.38 → **0.46**. The time-averaged centre of mass is now 0.02 px above the middle.
3. **The morph gradient** (`morphField`, `paintMorph`) is gui2's `paintMorphField`, ported in its law:
   - the tube is drawn first;
   - then each corner gets a radial gradient centred on that corner, with radius max(W, H), running from the corner token at alpha 0x55 (1/3) to 0 at 0.7 of the radius.
   - The corner order is gui2's (A top-left, B top-right, C bottom-left, D bottom-right).
   - **Strength:** gui2's, unchanged. The swatches are opaque and the puck keeps its tube ring. The one text ("MORPH OFF — the patch is corner A") now sits on a tube plate, the XY's idiom, so no mark rests on the colour.
   - On this wide pad (254×136), a corner's vertical neighbour still glows faintly there, as on gui2's 340×212 mini pad.
   - OSC has no morph surface.
4. **Blade 2's overflow.** `.mgrid3` is removed. Both blades use `.mgrid2`, and blade 2's "=1" pill takes a row of its own (`.mgrid2 > .pill.link { grid-column: 1 / -1 }`), so its mirror is 114 px like blade 1's. Under any rule the face is now ≤ 615 px of 630. The card does not grow (130.8 and 136.0 px, with and without a rule). MAIN's Osc Controls face shows no spreads.
5. **Page text:** the ROUND 11 header comment, the meta, the badge, the subtitle, and notes 69-72.

## Evidence

- **VERIFIED, in-page self-check 99/99 with 41 controls** (was 91/91 with 37). It ran at 99/99 on each of: OSC, `?page=main&theme=dark`, `?skin=c&direct=1`, `?skin=b` and `?scheme=dusk&theme=light`. The new rows (`checkB334`):
  - **Logo styles.** `elementFromPoint` at the mark's centre is the mark.
    - Six dispatched clicks run DROP → INSET → GRADIENT → INSET + GRADIENT → SOFT DROP → FLAT. Each shows its name and stores its id.
    - Pixels differing from FLAT: 1755, 2169, 2994, 3611 and 2987.
    - With "gradient" stored, a re-read returns GRADIENT.
    - Cost per paint, warp included, over 20 paints each: FLAT 1.85 ms, DROP 1.88, INSET 1.87, GRADIENT 1.87, INSET + GRADIENT 1.91, SOFT DROP 1.89. The first run read 1.50-1.71.
    - Shedding with the heaviest style: 30 → 8 paints in 60 frames.
    - **CONTROL:** with storage cleared, the re-read is FLAT. Two FLAT paints with a SOFT DROP paint between them differ in 0 px.
  - **Centred.** The centre of mass is 0.02 px above the middle over 64 moments, and the ink grazes the edge in 1 of 64. The box runs from 2.0 px below the frame's edge to 0.0 px above the body's top.
    - **CONTROL:** 0.38 reads 5.22 px above.
  - **Morph gradient,** 5 screens × 2 themes:
    - Each corner layer alone at its own corner is its token (RGB within 7, alpha within 0 of 85). At the far corner it is 0.
    - The live pad's four corner pixels match gui2's law, computed by hand (the vertical neighbour included), within Σ|ΔRGB| 3 on all 10 screens (the gate is 10).
    - The plate is the tube within 0, and the puck's ring within 0.
    - **CONTROL:** the field flipped on its diagonal fails the same ≤ 10 test at every corner of every screen (off by 14 or more).
  - **No overflow.** 20 layouts: 10 rules × blade 2 following or own. On both cards, every control is inside the card's padding box and no label is clipped. On skin A the cards grow 0.0 px. On skin B a rule's Rule depth is one more row (+22 px, B297's design), so growth is gated on skin A only. MAIN is inside.
    - **CONTROL:** B323's three-column blade-2 mirror, planted under GOLDEN, puts 6 controls outside (SPREAD +30.3 px). On skins B and C the plant is a 700 px control instead, which is also caught.
- **Planted faults** (served rewrites, nothing committed): **9 of 9 CAUGHT**:
  - the mirror back to three columns;
  - the style not saved;
  - a click that does not advance;
  - the logo with `pointer-events:none`;
  - the inset pass dropped;
  - `LOGO_TEXT_Y` back to 0.38;
  - the morph field removed;
  - the B/C corner tokens swapped;
  - the text without its plate.
- **Not verified:** nobody has looked at these on a real display yet. Costs come from one machine (M3, headless, dpr 2).

## Screenshots (scratch, not committed; `scratchpad/b334/shots/`)

- `11-logo-styles-light.png`, `11-logo-styles-dark.png`: all six styles, labelled (single shots `10-logo-<style>-<theme>.png`).
- `12-logo-click-name-light.png`: the name shown after a click.
- `00-before-header-main-light.png` → `13-after-header-main-{light,dark}.png`: the centring.
- `21-morph-sheet.png`: the morph pad in ORCHID and TUBE, morph on, both themes, plus FROST with morph off, showing the plate (single shots `20-morph-*`).
- `03-blade2-golden-before-after.png`: blade 2 under GOLDEN before, after, and after with its own spreads (full frames `00-before-golden-osc-light.png`, `01-after-golden-osc-{light,dark}.png`, `02-after-golden-own-osc-light.png`).

## Alternatives rejected

- **Letting the knob groups shrink (`min-width:0`)** to absorb the overflow: every knob would narrow a little, and the caption could overflow its group. The mirror's third column was the actual waste.
- **Moving blade 2's "=1" into the MIRROR caption** as FM and SPREAD do: the mirror's "=1" is an enum sentinel, not a switch group, so this would need new rendering code. The grid row reuses what exists.
- **`ctx.filter: blur()`** for the soft shadows: it is newer in WebKit. The canvas shadow works everywhere.
- **Pure corner colours at the corners** (a literal reading of "equal the corner tokens"): that would not be gui2's gradient. The check instead verifies each corner layer is exactly its token, and each composite corner is gui2's law.

## Verify

- `./verify fast`: exit 0 at git 6303514, the code commit (`.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"6303514","ts":"2026-09-28T16:57:31Z"}`).
  - The log includes `lab_wheel_scroll_check: GREEN — 0 failure(s); 6 labs, 2 planted faults` and `test_table_check: GREEN (204 tests — 116 agentic, 88 human; 16 awaiting an oracle; 72 check files declaring WIRED and verified so, 1 declaring UNWIRED and verified so)`.
  - `lab_load_check` is quiet on green inside verify. Run directly, it reads `GREEN — 58 labs loaded, 0 broken, 1 skipped`.
  - The private-name leak check was SKIPPED (`.leakcheck-names` is absent in this worktree).
- `./verify full` was not run: no engine or gate file changed, and the lab's own checks run in-page.
- This trace is committed on top, and `verify fast` is re-run on that hash.

## Open questions

1. Which logo style stays (or none), and whether DROP's triad colour or a darker ink suits better.
2. At 1 moment in 64, the warp's deepest pull touches the box's bottom edge. Accept it, or trade 0.5 px of centring?
3. The morph pad's gradient is always on, as in gui2. Should it dim while morph is off?
