# 2026-09-15 — ORBITAL lab: edge cushion, per-body x/y graphs, seeded add (ADR-165 A2)

Human asks: the cushion before the golden; "little individual X/Y position
graphs to the members of the orbital system, at least in the lab".

`reference/gravity-modulator.html`: `P.cushK/cushW` + two panel rows + bind +
syncControls; cushion damping in `step()` after global drag (quadratic ramp in
the band, inert in wrap); `mk()` gains `hx/hy` Float32Array(240) + `hp`;
`frame()` records `outX/outY` per body per frame; `drawXYGraph()` paints the
strip in the card (x solid, y dashed); `.xyg` CSS; the add-body draw is a
seeded mulberry32 (house form). `specs/SPEC-ORBITAL.md`: §3.1 two rows, §9 one
bullet, §11 one row.

Browser pane: no console errors; 4 bodies, 4 graphs painted (572–1030 px
each), simTime advancing, sliders present, `addRng` seeded. `lab_load_check`
does not glob `reference/` (B126 gate gap). `./verify fast` exit 0.
