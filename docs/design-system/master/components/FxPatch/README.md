# FX patch

Routing on the FX page (GUI-41, a MOCK-UP the human directed): each corner is its own patch of wires between the sources and the hosted modules, and the pad blends the corners. It supersedes GUI-36's chains.

**Every source is its own input.** Osc 1, Osc 2, the sub, the noise and both filter slots each feed modules on their own (the human: "all the oscs and filters need to be individually exposed on the interface because they can each independently feed into modules").

**Gestures.** On the Rack, drag from one box to another to add a wire, and drag again to take it away. A wire from left to right goes over the top, and one from right to left goes underneath (the rack's own drawing). On the Matrix, click a cell. "Show" picks which corner's wires are drawn and edited, or "Heard", what the pad makes of the four, which isn't edited. "Series" resets a corner to the house default and "To all" copies it.

**The law: `Horde.FxGraph`, pure.** A corner's own wires never form a loop: a wire that would close one is refused, with the reason. The pad blends the corners' wires (each corner weighted by the square of its pad weight, as `FxSample`). Wires are then admitted heaviest first, and one that would close a loop with those admitted is held at zero, drawn dotted as waiting. So at rest, at any pad position, the heard graph has no loop. While the pad moves, a leaving wire glides out over `tau` as its replacement glides in, so a loop lives only for that crossfade, on the wire's backward part, one sample late (ADR-201). This is the human's rule for 1.0: "block feedback in 1.0 at all the morph positions except for brief crossfade overlaps".

**The consumer provides** `sources` and `modules` (`{ id, name, short, style }`), `weights()`, `session`, `keys()`, `start` (the corner shown first), `onChange(label)` and `say(text)`, and calls `step(dt)` from its frame loop. `Horde.FxPatch(el, o)` returns `{ graph, view(i), show(i), edit(u, v), redraw(), step(dt) }`. `graph.draw(corner, rng, chaos)` draws a seeded loop-free patch for Randomize.

**MOCK.** Its state lives in the view: not in a patch, a snapshot or undo, and gone on reload. The engine's cable state replaces it (GN-3), and the per-source lanes are GN-50. The filters are Weir's and have no manifest yet.

**FX device (ADR-254).** The same patching, with its own inputs in place of the sources.
