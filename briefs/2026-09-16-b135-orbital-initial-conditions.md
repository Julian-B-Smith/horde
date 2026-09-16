# Dispatch brief — B135 ORBITAL per-body initial conditions + distance scale

**Provenance.** HYPERSAW lead organ, 2026-09-16, for a scoped subagent with zero
conversation history. Motivating request (human, 2026-09-16, verbatim): *"there
needs to be some way to control starting position and trajectory of bodies in
Orbital. And if it makes sense I think a physical distance scale slider for when
you've found a relative equilibrium you like but want it to touch more of the
grid without messing up the gravity dynamics."* Queue row **B135**; the lab is a
CANDIDATE (ADR-165), not golden, and this request is the human gate for the
edit.

## Acceptance criteria (verbatim from ROADMAP B135)

> Build (a) on the body card: editable home x/y and initial velocity entered as
> speed + heading (polar, because a trajectory is designed as "this fast, that
> way"; the cartesian pair shown beside it), "Set home = here" (captures the
> live position AND velocity as the initial condition) and "Return home"
> (per-body reset to it); Reset uses the stored initial conditions, never the
> preset literal. (b) a distance `scale` (0.25–4, log, default 1) that is a
> READOUT transform, not a physics change: observables, strips and the drawing
> map sim space to the grid as 0.5 + (x − 0.5)·scale, clamped to 0..1, so the
> simulation is bit-identical at every scale by construction — that is the gate
> (recorded sim trajectory at scale 2 equals scale 1 exactly). The physical
> alternative — positions ×s, velocities ×s, G ×s³ about the centroid, a
> period-preserving similarity — keeps orbit shape and rhythm too, but the
> walls and cushion live at the grid edge, so any wall contact changes the
> dynamics; offered as a "Bake scale into bodies" button that rewrites the
> initial conditions once, not as the slider.
>
> Acceptance: body card edits home/speed/heading and Return home lands the body
> there with that velocity; Set home = here round-trips (set, run, return → same
> first frames); presets store per-body home + initial velocity + the global
> scale; scale 2 vs scale 1 → identical sim trajectory (harness),
> observables/strips scaled and clamped; bake button rewrites initial conditions
> such that the first frame after bake equals the pre-bake readout;
> lab_load_check green; SPEC-ORBITAL §3 rows updated.

## Files in scope

- **EDIT** `reference/gravity-modulator.html` — the body model (`mk(...)`,
  search `nodes=[]`), the body card (`data-act=pin` is the pattern to follow),
  the observable readout / strips / draw path, the preset save-load, and the
  global controls panel (the `scale` slider goes beside `damp` / `cushK`).
  Keep the sim loop (`step`, the Verlet halves, the wall bounce, the cushion,
  the centroid recentre) untouched except for reading `home`/initial
  velocity on reset. Per-body fields to add: `hx0, hy0` (home) and `vx0, vy0`
  (initial velocity) — the card edits speed + heading and writes vx0/vy0.
  Note `hx/hy/ha/hr` are already the strip ring buffers — do not collide names.
- **EDIT** `specs/SPEC-ORBITAL.md` — §3.2 body table: the home row already
  exists; make initial velocity its own row (speed + heading, preset-stored),
  add "Set home = here" / "Return home" to §7's reset semantics; §3.1 global
  table: a `scale` row stating it is a readout transform with the exact
  formula and clamp, and the bake button's similarity (×s, ×s, G×s³).
- **CREATE** `traces/2026-09-16-b135-orbital-initial-conditions.md`.

**OUT of scope:** `src/**`; `ROADMAP.md` / `DECISIONS.md` / `CLAUDE.md`
(lead-only — report any spec-row text you think the lead should add);
`./verify` and gates; every other protected path; the untracked root files
(Chrome*.dc.html, GoopBox.jsx, HORDE*.dc.html, ROAM-spec.md,
STRATA-integration-spec.md, Text*.html/zip, hp-support.js, support.js,
strata-modulation-bench.html — never touch, never `git add -A`).

## Constraints you inherit

- Branch `orbital-initial-conditions` from `main` (`origin/main` at c914bfa or
  later). Another PR touches CLAUDE.md/ROADMAP/DECISIONS only — no overlap.
- The lab records strips in sim time every 8 physics steps and has a timer
  fallback for hidden tabs (trace `traces/2026-09-15-orbital-lab-cushion-graphs.md`
  explains why) — keep both.
- The add-body draw is seeded (mulberry32); any new randomness uses that
  stream. No wall-clock reads in the sim.
- Verification: `node tools/labharness/lab_load_check.mjs reference/gravity-modulator.html`
  GREEN; a Node/vm harness (model it on `lab_load_check.mjs`, put it in
  `tools/labharness/` only if reusable, otherwise in your scratch dir and paste
  the output in the trace) that runs the sim N steps at scale 1 and at scale 2
  from the same preset and asserts the raw `x,y,vx,vy` arrays are bit-identical
  while the scaled observables differ; and one that sets home = here, runs,
  returns home, and asserts the next frame equals the frame after the set.
  `./verify fast` tail verbatim; red halts you.
- Use your own scratch subdirectory for harness output; no machine identity in
  tracked files; alias discipline (no private sibling names).

## Deliverable

Branch `orbital-initial-conditions`, pushed, PR via `gh pr create --base main`
with a screenshot of the body card and the scale slider. **Never merge.**
Final report: PR URL, harness output, `./verify fast` tail verbatim, and any
spec-row text the lead should carry into ROADMAP.
