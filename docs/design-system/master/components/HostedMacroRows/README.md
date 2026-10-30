# Hosted macro rows

A hosted module's macros as horde's matrix shows them: live and visible, read-only, never cross-mapped; each module keeps its own macro mapping (ADR-266 ruling 3).

**The consumer provides** a `Horde.Session` (for its frame loop and its message line) and the module: `Horde.HostedMacroRows(el, S, { module: { name, macros: [{ id, name, f(t) → 0..1, targets: [names inside the module] }] } })`. The macros are NOT added to `S.sources`, so no strip, MAP selector or grid row can map them. The grid takes the same list (`hosted`) for its read-only rows.

**Anatomy** visibly not horde's:
- a 1.5 px DASHED ink edge round the group, a faint ink hatch over `panel`, a lock;
- a header: the module's tag (Archivo 700, 8.5 px, ink edge) and "macros · read-only · mapped inside MAW" (mono 9 px `t2`);
- one 28 px row per macro: "M1", its name, a 44 × 6 px meter in `t2` (never a source colour: these are not horde's sources), its value (tabular, fixed width) and "→" its targets inside the module (`t2`, ellipsis); a not-allowed cursor.

**Gestures** A click on a row only explains: "MAW Drive is read-only here: MAW maps its own macros (to Drive); they cannot be cross-mapped into horde". Tab reaches each row; its accessible name carries the value and the targets.

**Fits** 320–1000 px; below 360 px of container the targets fold away. Values change; no box does.

**Placeholders** The data a hosted module publishes for its macros (names, values, what each moves inside it) is assumed as `{ name, f, targets }`. The rack-slot contract (B450) and the module macro spec (SPEC-MODULE-MACROS) decide what the module actually reports.

**Sources** ADR-266 ruling 3 (the human's B392 suggestion); the module macro tiers (ADR-169). The mod matrix lab has no hosted module; the read-only look follows its retired-slot treatment (dashed edge, no fill; mod-matrix-lab.html :244-246, :381).

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
