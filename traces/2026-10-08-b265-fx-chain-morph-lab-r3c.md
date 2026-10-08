# b265-fx-chain-morph-lab-r3c — RACK, MATRIX and PANEL together, refined, checked against the engine

- **Queue item:** B265, round 3c. Dispatched by the horde lead on 2026-10-08 after the human's layout verdict on #982 (verbatim): "I like the rack, the matrix, and the panel (I think each could be designed a little more elegantly, but the general concepts are good). I think each should be visible."
- **Why:** Show the three chosen views at once from one cable state, make each calmer and more legible, and give them one visual language.
- **Branch:** `lab-fx-chain-morph-r3c`, cut from `origin/lab-fx-chain-morph-r3b`, because #982 was still OPEN when this was cut (`gh pr view 982`). It rebases cleanly once #982 lands. This trace extends `traces/2026-10-08-b265-fx-chain-morph-lab-r3b.md`.

## What changed

**Structure.** Each view is now a pure SCENE built in the model block, `rackScene`, `matrixScene` and `panelScene`: normalised geometry plus the engine's own numbers, no DOM. A thin painter draws each scene in the page. All three paint every frame from one `viewDataOf` state.

**Removed views.** RING, FLOW and ROLL are removed rather than kept dormant: keeping them would have cost a selector, three painters and a history buffer. The `layout=` query, the localStorage key and the LAYOUT pills are gone with them.

**Without audio.** A persistent view router follows the puck at control rate, so the views glide through transitions, loops and waits included. Before, they showed a state re-settled at the puck. It draws only; nothing listens to it.

**Review links.** `?to=x,y&after=ms&hold=N` moves the puck after load and freezes the view router N control ticks into the transition, for screenshots.

**Shared encoding (all three views).**

| what | how it is drawn |
|---|---|
| colour | one colour per module, the same on both themes' tuned pairs; a cable wears its source's colour (Body magenta, Sub marker, OUT cyan) |
| weight | thickness 0.8–5.6 px and opacity 0.22–1, one scale everywhere |
| backward part | dashed, marked z⁻¹ |
| live loop | an amber halo |
| waiting cable | amber dots |
| Scape → Sluice | ✕ (amber); Scape → Bulwark marked E1 (dim) |
| asleep module | dashed box, faded header or dark LED |

The per-module colours are a DEPARTURE from ADR-116's colour roles, marked in the code; amber stays reserved for findings.

**RACK.**
- Arcs are half-ellipses whose height grows with the square root of the span, so long arcs no longer tower.
- Cables leave right of centre and land left of centre, so in- and outgoing arcs part at every box.
- Long arcs are drawn first, so short ones stay on top.
- One small arrowhead marks each landing.
- Three slow flow dots move per arc only while live, and not at all under reduced motion.
- Node boxes carry the module's colour; short names are used below 560 px.

**MATRIX.**
- Rounded cells with gaps.
- Fill is the source colour at the weight's opacity, and the value is printed only when it fits (`1`, `.75`).
- Unavailable cells are hatched amber with ✕; E1 cells are hatched dim.
- Loops are outlined amber; waits are a dotted amber border; a backward part is a dashed band along the cell's foot.
- Row and column headers take module colours and fade when asleep. The corner caption is `↓→`.

**PANEL.**
- Panels carry a colour stripe, an LED (a glow in the module's colour when awake, dark when asleep) and the CPU cost.
- Cords are cubic sag curves whose sag grows with the span, with a dark sheath under each so crossings read as cords.
- Plugged jacks fill with the cord's colour.
- Jacks, their captions (placed above each jack, while cords leave downward) and the rule marks are drawn LAST, so nothing covers them.

**Layout.**
- The rack spans the top; the matrix and the panel sit side by side below it.
- Under 900 px all three stack (the rack at 2.2:1).
- No horizontal overflow at 500 px, headless Chrome's minimum.

**Self-check 12 (new).** At every other tick of every sweep (4 sets × 2 strategies × 7 paths: 26 600 states), all three scenes draw exactly the engine's live cables:
- the same forward and backward weights, bit for bit;
- the same loop and wait flags;
- no extra cables.

1080 of those states have a live loop and 3027 a waiting cable. The plant, a view that drops the backward part, is caught: on close (a), top edge, the rack would draw ECHO → Scape at 0 against the engine's 0.0429.

## Screenshots (scratch only, not committed)

- **Light, close set, held 30 ticks into the top-edge move.** The ECHO ↔ Scape reversal is a live loop. RACK shows the dashed z⁻¹ arc with an amber halo, MATRIX the Dl → Rv cell outlined amber with a dashed foot, and PANEL a dashed, haloed cord. The live box reads 1 loop at gain 0.24.
- **Dark, four presets, (b), held 20 ticks into a bottom-edge move.** Drive → Filter waits (flip). It shows as an amber dotted arc under the rack, a dotted cell and a dotted cord, beside the parallel hand-over cables; Bulwark reads ‖2.
- **Light at 500 px.** The three views stack and stay legible; the rack uses short names.

## Self-check: 12/12 claims hold, 12/12 planted faults caught (headless node; 256 steps, longest about 2.1 s)

## Evidence consulted

- The r3b lab and trace
- The human's verdict as relayed by the lead
- ADR-116 (colour roles)
- L0064 (views driven by what is rendered)
- `tools/gen_lab_index.py`; the navigator was unchanged

## Alternatives rejected

- **Keeping RING, FLOW and ROLL behind a hidden switch:** not free (a selector, three painters, a buffer).
- **Colouring cables by lane:** it would not tie a cable to its module across views.
- **Animating PANEL cords:** decoration, not state.

## Verify

`./verify fast` and `./verify full` run on the pushed head. Their JSON lines go in the PR body and the report back.

## Open questions

1. The module palette (a departure from ADR-116): does it read, and should gui3 adopt per-module colours?
2. Is PANEL's sag convincing, and does it get busy on dense chains?
3. Should the view router (the views glide without audio) stay, or should the views show only what the audio plays?
4. Still open: p = 1 as a lab default, glide τ, the per-cable-threshold reading, the declared gains, ADR-195's "brief", and wiring the self-check into `./verify`.
