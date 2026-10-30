# Mod ledger

What is moving the selected destination, right now: each mapping's source, polarity, the amount you set, its depth now, its source now and its push now, then the sum, in the order the arithmetic runs.

**The consumer provides** a `Horde.Session`: `Horde.ModLedger(el, S, { key? })`. Without `key` it follows `S.selected` (a grid column, a lane, a wire's node or a control).

**Anatomy**
- Title: "What is moving Cutoff, right now" (Archivo 800, 11 px, caps, `ink`).
- Under a Control, a banner (1.5 px edge in the controlling source's colour, mono 9.5 px `ink`): "Controlled by MACRO 1 · sweeping 20–80% · now 56% · its own value (50%) is ignored". It is hidden otherwise.
- A table (mono 9.5 px, tabular, fixed columns, dashed `div` rules): SOURCE (kind dot and name), POL, YOURS (the depth, or a Control's range), DEPTH NOW (only when a depth route moves it; "range" on a Control; else "—" in `hint`), SOURCE NOW, PUSH NOW (signed; a Control "sets 56%"). A depth route sits indented under its mapping with "→ depth".
- The sum line: "own 915 Hz + Σ +12% of range = 1.3 kHz" (under a Control "CTL 56% + Σ …"); a clamp shows "raw 112%, clamped" in `caution-tx` on `caution-bg`.

**Fits** 320–1000 px; below 440 px of container DEPTH NOW and SOURCE NOW fold away. Numbers change; no box does.

**Reduced motion** repainted only on change.

**Placeholders**
- The sum is in percent of the destination's range (ADR-136's fraction), and the parameter's own formatter prints the ends. Per-destination units (semitones on pitch, the lab's ±48 st) wait for the shell's formatters.
- Depth routes and silent corners as in the grid's placeholders; a silent corner's row shows in `hint`.

**Sources** docs/design/mod-matrix-lab.html: the ledger's styles :385-396, `paintLedger` :2322-2351, its note :642-648.

The preview draws SAMPLE data generated in the preview (seeded, deterministic), never the engine.
