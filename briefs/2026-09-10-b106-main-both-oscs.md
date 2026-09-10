# Dispatch brief — B106 MAIN shows both oscillators

**Provenance.** HYPERSAW lead organ, 2026-09-10, for a scoped subagent with zero
conversation history. Motivating report (human, 2026-09-10, verbatim): *"Main
still has visualizers that show only the active Osc but no longer have a way of
selecting Oscs. I think main should show the waveforms and phase carpets for both
Oscs and not have anything that references only the active one."* Queue row
**B106**.

## Acceptance criteria (verbatim from ROADMAP B106)

> **MAIN visualizers show BOTH oscillators.** Today the viz snapshot is
> single-osc (`setVizOsc` picks which); MAIN inherited the OSC page's
> active-osc feed when the Editing bar left MAIN (ADR-150 era). Build: the
> feed carries both oscs (a second VizSnapshot or a per-osc field set in
> hzFrame — B76's batched transport already packs scope/spec; add the second
> osc's carpet + wave), MAIN renders two waveform + two phase-carpet panes
> labelled OSC 1 / OSC 2, and every "active osc" label leaves MAIN. OSC pages
> keep the single active-osc view.

## Files in scope

- **EDIT** `src/gui/hypersaw_gui.h` — extend the seam: either a second
  snapshot getter (`getVizFor(osc)`) or fields on `VizSnapshot` for the
  second oscillator's phase carpet + scope. Prefer the smaller change that
  keeps the OSC pages' existing single-osc path bit-for-bit (they must not
  change).
- **EDIT** `src/gui/hypersaw_gui_common.h` — `vizToValue` / the `hzFrame`
  batched bind (search `hzFrame`, `b64`): carry the second oscillator's data
  in the SAME frame (one bridge round-trip per frame is the ADR-143 rule —
  do not add a second poll). The scope payload is int16 base64, the spectrum
  uint8 — follow the packing that is there.
- **EDIT** `src/hypersaw_clap.cpp` — ONLY the viz snapshot producer
  (`getViz`, `setVizOsc`, the scope/carpet capture — search those names).
  Nothing in process() may allocate; if capturing a second osc's scope needs
  a buffer, it is preallocated like the first.
- **EDIT** `src/gui/gui2.html` — ONLY MAIN's Viz cluster (`id="vizWho"`,
  `#specC`, `#wavC2`, the phase-carpet canvas on MAIN) and `paintVizSnapshot`
  / the scope painter where they draw MAIN. Two waveform panes + two carpet
  panes, labelled "OSC 1" / "OSC 2"; remove every "active osc" reference from
  MAIN (the `whoOsc` / `vizWho` spans and the note text). The master spectrum
  stays one pane. OSC pages untouched.
- **CREATE** `traces/2026-09-10-b106-main-both-oscs.md`.

**OUT of scope:** the XY pads and MOD page (other streams); the specimen
(CHROME) code; `ROADMAP.md` / `DECISIONS.md`; `./verify` and gates;
protected paths; the untracked root files.

## Constraints you inherit

- Branch from `main`; **#527** touches other regions of `gui2.html` and
  `hypersaw_clap.cpp` — rebase onto main before opening your PR if merged.
- Build: `cmake -S . -B build-release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release`
  then `cmake --build "<absolute worktree path>/build-release" -j"$(sysctl -n hw.ncpu)"`.
- The plugin's bridge cost is real (the 2026-08-30 lag saga, ADR-143/B76):
  measure that the frame payload grows by roughly one osc's worth and no
  more; report the byte count before/after.
- `node tools/labharness/lab_load_check.mjs` GREEN; in a `file://` load of
  gui2 (bridgeless, stubbed feeds) confirm MAIN shows two labelled waveform
  panes and two carpet panes and no "OSC 1" active-osc heading; `./verify
  full` before done (the C++ changed); oracle output verbatim; red halts you.
- No machine identity in tracked files; alias discipline; no allocation in
  the audio thread.

## Deliverable

Branch `b106-main-both-oscs`, pushed, PR via `gh pr create --base main` with
a screenshot of MAIN. **Never merge.** Final report: PR URL, payload bytes
before/after, `./verify full` tail verbatim.
