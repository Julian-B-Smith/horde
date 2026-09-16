# Dispatch brief — B134: a per-route polarity setting for every modulation route

**Provenance.** HYPERSAW lead organ, 2026-09-16, for a scoped subagent with zero
conversation history. Motivating record: ROADMAP B134 (human 2026-09-16:
"Important roadmap issue: all mods need a bipolar toggle"). Read B134,
ADR-136, ADR-137, ADR-141 and CLAUDE.md first.

## Acceptance criteria (verbatim from ROADMAP B134)

> (1) `ModCore::Route` gains `int polarity` {0 as-is (default), 1 unipolar 0..1, 2 bipolar ±1, 3 inverted}; `evaluate` maps the source value BEFORE depth: as-is → v; unipolar → sources declared bipolar map (v+1)/2, others pass; bipolar → sources declared unipolar map 2v−1, others pass; inverted → −v after the polarity map; the source polarity table lives in the shell beside `MOD_SRC_NAMES` (ENV 1/2, macros, velocity, wheel, pressure = unipolar; pitch wheel = bipolar; retired aliases bipolar) and is handed to ModCore as a per-source flag array, so the core stays framework-free; (2) the `modroutes` chunk grows a fourth field `src:dest:depth:pol;` — absent field = 0, so every existing patch is bit-inert (`routing_check` + `state_check` + `statefix_check` green); `modRoutesJson` carries `"pol"`; (3) binds `hzModPolarity(routeIndex, pol)`; the MOD page route row gets a four-state cell (as-is · uni · bi · inv) beside the source select, and the B105 "Send another" flow leaves it as-is; halos (`modHaloFrame`) draw from the MAPPED value; (4) `polarity_check` standalone, unwired: a bipolar source through a unipolar route reads 0..1, a unipolar source through a bipolar route reads ±1, inverted negates, as-is is bit-identical to today (control: the same route with pol 0 renders identically to a build without the field), and the chunk round-trips the field; (5) no new param ids; no RT allocation

## Where things are

- `src/mod_core.h`: `struct Route {src, dest, depth, scope, active}`, `kMaxRoutes 64`,
  `addRoute`, `evaluate` (`const double d = q.depth * src[q.src];` is the line
  the polarity map goes in front of). `kMaxSources 24`.
- `src/hypersaw_clap.cpp`: `modRoutesChunk()` writes `"%u:%u:%.6g;"` per
  (src,dest) — note it SUMS duplicate routes' depths into one entry, which
  cannot survive per-route polarity: write one entry per ROUTE instead (the
  loader already creates one route per entry); `applyModRoutesChunk` parses
  with `sscanf("%u:%u:%lf")` — extend to an optional fourth field;
  `modRoutesJson()` (the GUI's view); `modAddRoute`/`modSetDepth`/
  `modSetSource`/`modRemoveRoute` and their `hostIf.*` wiring (search
  `hostIf.modSetDepth`); `MOD_SRC_NAMES` lives in `src/gui/gui2.html` — the
  source polarity TABLE goes in the shell (one array beside the source-slot
  comments, ADR-137/149) and is exposed in `modRoutesJson` as `"srcPol"` so
  the GUI never guesses.
- `src/gui/hypersaw_gui_common.h`: the `hzMod*` binds (search `hzModDepth`);
  `src/gui/hypersaw_gui.h` `GuiHost` has the matching `std::function`s.
- `src/gui/gui2.html`: `renderModRoutes()` builds each row (source select,
  depth slider, ×); `modHaloFrame` paints halos from `hzModLive`; the
  "Send another to mod matrix" item in `PARAM_MENU`.
- Checks: `tools/routing_check.cpp` (a wired gate — do NOT edit its
  assertions; it must stay green), `tools/state_check.cpp` (wired),
  `tools/statefix_check.cpp` (run by hand with `tests/state_fixtures`);
  `tools/notefuzz_scaffold.inc` is the headless rig (include `<algorithm>`
  first for MSVC). `hypersaw_debug_apply`/`_state` exist for round-trips.

## Files in scope

`src/mod_core.h`, `src/hypersaw_clap.cpp` (routes + chunk + JSON + wiring +
the source polarity table), `src/gui/hypersaw_gui.h`, `src/gui/hypersaw_gui_common.h`,
`src/gui/gui2.html` (route row cell + halos), NEW `tools/polarity_check.cpp`
+ `CMakeLists.txt` registration, `traces/2026-09-16-b134-mod-polarity.md`.

**OUT of scope:** `ROADMAP.md` / `DECISIONS.md`; `./verify` and gate
assertions; new parameter ids; the modulation lab (B16) and any LFO; ORBITAL;
protected paths; untracked root files.

## Constraints

Branch from `main` (pull first); absolute build paths; `./verify fast` after
each change set, gate every scripted commit on its exit code; `./verify full`
before done (routing/state/undo gates must stay green); paste oracle output
verbatim; no allocation on the audio thread; no machine identity or private
sibling names in tracked files; MSVC in CI. Use a per-stream scratch
subdirectory for any harness you write (LIBRARY L0048).

## Deliverable

Branch `b134-mod-polarity`, pushed, PR via `gh pr create --base main` whose
body leads with a DOM dump of a route row showing the four-state cell, then
`polarity_check` output and the `./verify full` tail pasted from the run.
**Never merge.** Final report: PR URL, check output, verify tail verbatim, the
ROADMAP/DECISIONS text you would add.
