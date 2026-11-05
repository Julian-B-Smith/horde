# FX chains

Routing on the FX page (GUI-36, a MOCK-UP the human asked to land). ADR-201's static-node rack: modules stay resident, and the morph moves only cable weights. So routing is edited per corner: a corner is a chain of the modules in its order, with where Sub joins (FX sample's model, from `fx-chain-morph-lab-r3.html`).

**Anatomy.** A Rack, Matrix or Case view (`FxRack`, `FxMatrix`, `FxPanel`, unchanged) over four rows, A to D. The row nearest the pad is ringed in `value`. In a row, modules in the chain come first as numbered cream chips in their order, and modules out of it follow dashed. A select says where Sub joins, and "To all" copies the row to the other three.

**Gestures.** Click a chip to add or remove it (it returns at its house position). Drag a chip in the chain to move it. In keyboard mode, left and right arrows move it. "To all" and the select are each one change.

**The consumer provides** `modules` (`{ id, name, short, style }`), `weights()` (the pad's four weights), `session`, `keys()`, optional `rules` and `onChange(label)`. Call `Horde.FxChains(el, o)`, and `step(dt)` from the frame loop. It returns `{ corners, sample, view(i), redraw(), step(dt), draw(i, rng, chaos) }`.

**MOCK.** Its state lives in the view: it isn't in a patch, a snapshot or undo, and it's gone on reload. The engine's published cable state replaces `FxSample` (GN-3). Sluice and Weir have no manifest yet, so the shell doesn't list them.

**FX device (ADR-254).** The same component with its own modules, and with no Sub lane unless a sidechain input is ruled.
