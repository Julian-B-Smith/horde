# 2026-11-08 — The wavetable page lab, round 2: looks on horde's screen, and morph off shows one table (GB37, GUI-63)

**What changed.**
- **`docs/design/wavetable-page-lab.html`** (still exploring):
  - **Every table picture is a horde screen now.** It is drawn only through the house kit (`H.Viz.kit`) with the six `scr-` roles, so it follows the Screen scheme and the chassis. It keeps the phosphor rules: glow is a wider underlay stroke, and scanlines run at the scheme's strength. Corner colours are on the chassis (the tile's frame and chip). Inside the glass a corner is its letter in the glass's ink (screens.md).
  - **Q3, new:** six looks for the lead picture and the corner tables:
    - Phosphor stack;
    - Ridgeline;
    - Turntable;
    - Sonogram;
    - Polar rings;
    - Swarm beams.

    Each has its own motion. With Motion on, a mock LFO moves the read frame.
  - **A bench of lab controls above the windows:**
    - Morph on or off;
    - which corner plays with morph off;
    - the puck, which stands in for the Morph page's pad;
    - Motion live or still;
    - Cream or Dark;
    - the Screen scheme pills.
  - **GUI-63 built into Q1:**
    - V1 and V6 lose their pads;
    - V4 is retired;
    - morph off shows one table, and Q1 says there is nothing to place.
  - Q2's internal-modulator scopes moved onto glass too.
- **Records:**
  - GUI-63;
  - GN-57's corner half reads "only while morph is on";
  - GB37's status;
  - the ledger's lab note, the row `gui63-morph-off-one-table` and its engine impact.

**Why.** The human, on round 1: "the design you're inheriting from the wavetable bench gets a lot of Horde design details wrong, like the colors of screens. I want something that fits the aesthetic more, and is a little more lively." Also: "there's no reason to show the four wavetables when morph is off, and when morph is on there's no reason to add a morph grid to the center of the four."

**Evidence.**
- **The lab's self-check in headless Chrome, 24 OK:**
  - Each look inks its lead glass: 9.6 to 57.6 % of the pixels differ from an empty screen of the same size and scheme. All four of its corner glasses are inked too.
    - **Control:** the empty screen reads 0 %.
  - **Morph off:** 1 picker and 0 corner tables in Q3 and Q1, and the audible corner's table plays.
    - **Control:** morph on shows 4.
  - **No pad on any Osc page variant.**
    - **Control:** V5's Morph page keeps its pad.
  - **Every variant is inside its window.**
  - **The glass follows the scheme:** TUBE's ground reads 27,7,51 and ORCHID's 243,235,255.
    - **Control:** TUBE read twice is the same.
  - **Motion:** Live changes the picture over 40 frames.
    - **Control:** Still holds it.
  - **The blend and the seed checks** from round 1 still pass.
- **A real-mouse probe** (CDP mouse events):
  - a puck drag moves the weights;
  - Morph Off removes the corner glasses and the puck;
  - the plays-corner pill changes the lead's table;
  - a look option, and a corner menu pick, both work;
  - Still stops the frame count and Live restarts it;
  - a scheme pill sets the scheme.

  No uncaught errors.
- **Screenshots** of the six looks, on both chassis, and of morph off and the Q1 variants, were read. Four things were fixed from them:
  - the bench's selected pills didn't show;
  - weights wrapped;
  - L2's ridges were flat;
  - L5's back rings were faint.

**Verify.** `./verify fast` on the commit this trace ships in.
