# Source strip

The live source strip: one chip per modulation source with its kind's colour, its name and number and a live meter; a click makes it the current source. It is the ONE strip, for the mod matrix and for the MAP control.

**The consumer provides** a `Horde.Session` whose `sources` are `{ id, name, kind, f(t) → 0..1 }` in their key order (1–9 select them in MAP mode with the keyboard on): `Horde.SourceStrip(el, S, { counts?, label?, onPick? })`. `counts: true` adds how many routes read each source (the matrix shows it; the MAP control leaves it off). It registers its own `S.on` sync and `S.add` render, so the strip follows `S.map.src` wherever it is set (the MAP control's selector, a key, the grid) and returns `{ el, sync, render }`.

**Anatomy** the core's `.hd-srcstrip` / `.src-chip` (identical to the MAP control's): a 28 px capsule, `div` edge, `panel` ground, mono 9.5 px `t2`; a 36 × 6 px meter in the source's kind colour (`src-env`, `src-lfo`, `src-macro`, `src-engine`) on `well-alt`; the name (12 characters, then an ellipsis); the count "◦2" in a fixed 2.4-character slot; the number in `hint`. The current source's chip takes a 1.5 px edge in its own colour and `ink` text, `aria-pressed="true"`. Chips wrap; two sources of one kind are told apart by name and number (ADR-261 A1).

**Gestures** Click a chip: `S.setSrc(id)` (it emits; `onPick(id)` follows). Keys 1–9 are the core's accelerators in MAP mode with the keyboard on; the chip is their mouse route.

**For the MAP control** (the integrator wires it): in `MapControl(el, S, opts)`, replace the block that builds `opts.strip`'s chips and the `meters` it renders with `if (opts.strip) window.Horde.SourceStrip(opts.strip, S);`, and drop `meters` from `sync()` and the view's `render()`. The strip keeps its own pressed state and meters.

**Fits** 320–1000 px; chips wrap, never overflow. Meter widths change, the chip never does.

**Sources** docs/design/mod-matrix-lab.html: the strip's styles :222-245, `buildStrip` :1464, `paintStrip` :1482 (the "◦n" count). The lab's 24 numbered slots with retired and free tiles are the engine's slot table, not shown here: the strip shows the session's sources. The MAP control's strip (HORDE Map Mode Spec).

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
