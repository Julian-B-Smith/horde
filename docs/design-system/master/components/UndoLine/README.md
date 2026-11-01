# Undo line (M-R2, modulator removal)

ADR-338 ruling 9 (closing ADR-334's open question): **any modulator is removable**; its routes go with it **as one undo step**; and a line names them with Undo. The engine's fixed sources (ENV 1 to 4, MIDI, engine values) leave the list but stay in the engine; + brings them back.

**The host owns the undo stack.** The tiers' x calls `onRemove(m, routes)`; the host removes the modulator and every route with `src === m.id` as one step on its own history, then:
- `Horde.UndoLine(el, opts)` returns `{ show(text, onUndo), hide(), el }`: a status line (`role="status"`, polite) with an Undo button. `onUndo` is the host's inverse of that one step.
- `Horde.ModRemoval.describe(S, m, routes)` composes the line: "Removed LFO 1 and its 2 routes (Detune, Fine): deleted. One undo step." and, for a fixed source, "...: off the list; the engine keeps the source, + brings it back."

**Rules** the line keeps its place when empty (`visibility: hidden`), so nothing moves when it speaks (H1's rule). Only the last removal's Undo is offered; the host's history does the rest. A pending modulator (unmapped) removed is simply dropped, which ADR-322 already does on close.

**Not built** M-R1 (only an unmapped modulator removable) was not ruled.

**Sources** ADR-338 ruling 9, composition-lab.html M-R2.
