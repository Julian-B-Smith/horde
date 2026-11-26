# gui75-fx-routing-ladder — FX routing as a ladder of separated wires beside a send matrix, one wire model

- **Queue item:** GB41 (GUI-75), under GB21. It pairs with the lead's B714 and B715 (the bridge's wire model) and with ADR-410 r3 and ADR-411 r1.
- **Origin:** the human's ask, relayed verbatim in the lead's ADR-410 brief (2026-11-25), then the human's ruling in this session (2026-11-26).
- **Evidence consulted:**
  - GUI-41 and the master's FxRack, FxMatrix, FxPatch and FxCables READMEs: the arc geometry, with one shared port point per box.
  - ADR-410 and ADR-411 in DECISIONS.md.
  - The bundle's corner glyphs and weight law (`Session.weights`).
  - The tokens' cable colours (ADR-273).
- **Verify:** `./verify fast`, with its exit code in the PR.

## Variants shown (the 980 px FX page, MOCK levels)
- **Wire styles:** W1 ladder, W2 rounded tracks, W3 top-down column.
- **Layouts:** L1 side by side, L2 tabs, L3 stacked.
- **Control:** B0, today's rack, drawn with FxRack's geometry.

The human chose W1 and L1, plus drag-to-connect, a double-click that sets the default, and 0 removing a wire. The default is 100 %, by poll.

## Design choices (why, not what)
- **Tracks by span, ports by track.**
  - Shorter wires sit lower, and a wire goes one track above every wire it overlaps, so nested wires stay inside.
  - At a box, a longer wire's port sits nearer the centre. Arrivals take the left half and departures the right, going forward.
  - These two rules together mean nested wires never cross. The only crossings left are spans that genuinely interleave in the fixed house order, and the self-check counts them.
- **The track pitch is one chip plus 2 px.** At the side-by-side width (490 px), 10 px tracks let two level chips cover each other. Chips also slide along their own track to a free spot.
- **0 removes the wire.** The writer turns any level at or below 0 into "absent at this corner". So "a wire exists at a corner" and "its level there is above 0" can never disagree, and Heard's quantum rule (ADR-377) reads one fact.
- **No pointer capture on the level drag.** Each move re-renders both views, which replaces the node under the pointer, so the drag listens on the window instead.
- **Return wires use FX cables' encoding:** dashed, z⁻¹, one sample late.
  - "Prevent feedback" finds back edges by reachability: a return wire whose destination already reaches its source going forward.
  - Prevent feedback holds those back edges at zero.

## Oracle evidence (the lab's own self-check, in headless Chrome)
- **Every style, patch and corner, at both the L1 and the full width:** no shared port, no overlapping segment, every point on its band's side, no chip on another, and the closest two ports at least 3 px apart.
- **Controls:**
  - Today's rack shares ports: 6 on the busy patch, with 21 crossings, against 9 for the new styles.
  - Tracks squeezed to 6 px cover a chip.
  - A drag under Heard changes nothing.
  - A drag from a box dropped on nothing makes no wire.
- **Gestures, with real pointer events:**
  - a 30 px drag up on a cell moves a level 20 %, and the wires view's chip agrees;
  - a drag to 0 removes the wire at that corner alone;
  - a double-click sets 100 % on a removed wire and on a live one;
  - a drag from the Sub box onto the Echo box makes Sub → Echo at 100 % at A alone;
  - Echo → Weir at C reads "held 0 %" with Prevent feedback on and "z⁻¹ 20 %" with it off.

## Engine and FX device
- **Engine impact:**
  - GN-61: the wire model on the bridge, with make, level and remove.
  - GN-62: Prevent feedback.
  - GN-50's loop rule is superseded by ADR-411 r1.
- **FX device:** the same, with its own inputs.

## Open
- **The master component is next:** a routing view replacing FxRack, FxMatrix and FxPatch's drawing. Then re-vendor, then the shell's FX page and the Mix picture (GUI-14 r5).
- **ADR-411's pick switch and the lead's dead-end guard** wait on B714's message shape.
- **The lab's readings are open to the human:** 150 px per 100 %, the keys, and Heard's blend over the corners that hold a wire.
