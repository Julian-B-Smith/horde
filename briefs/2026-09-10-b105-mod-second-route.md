# Dispatch brief — B105 second modulator to a routed param + destination picker

**Provenance.** HYPERSAW lead organ, 2026-09-10, for a scoped subagent with zero
conversation history. Motivating report (human, 2026-09-10, verbatim): *"there's
currently no way to send a second instance of the param to the mod matrix"*, and
*"in addition to 'send to mod matrix,' there should be a way to manually select
parameters from the mod page."* Queue row **B105**. The lead verified the shell
side already permits it: `modAddRoute` (`src/hypersaw_clap.cpp`, search the
name) has no duplicate refusal and `ModCore::evaluate` (`src/mod_core.h`) sums
every route per destination. **This is GUI-only.**

## Acceptance criteria (verbatim from ROADMAP B105)

> **Mod matrix: send a SECOND modulator to a routed param + manual
> destination picker on MOD.** Shell already supports it: `modAddRoute` has no
> duplicate refusal and `evaluate` sums every route per dest — GUI-only.
> (a) The right-click menu offers "Send another to mod matrix" when the param
> is already routed (today it offers only release, ADR-141); (b) the MOD page
> gets an "Add route" control: pick a destination from the presentation table
> (searchable, grouped by page), pick a source, depth 0.25 — the address is
> the presentation table's, so nothing is a second list; (c) each route row
> keeps its own ×.

## Files in scope

- **EDIT** `src/gui/gui2.html` — ONLY: the right-click menu items (search
  `'Send to mod matrix'`, `pmenuItem`, `contextmenu`), the MOD page's Routes
  cluster markup (`id="modList"` and its cluster), and `renderModRoutes` /
  `MOD_SRC_NAMES`. For (b): the destination list must be DERIVED — from the
  controls already in the DOM (`[data-p]` elements and their labels, grouped
  by the page they sit on; `modDestLabel()` already turns an id into its
  label) — never a hand-typed list. Only continuous params are legal
  destinations (the shell refuses stepped ones — surface the refusal, don't
  pre-filter by guessing: call `hzModAdd` and show the result). Per-osc
  params must be offered per oscillator (ids and their +1000 twins; see
  `effId`, `OSC_STRIDE`).
- **CREATE** `traces/2026-09-10-b105-mod-second-route.md`.

**OUT of scope:** the XY pads, MAIN's clusters, the corner-preset code (other
streams); `src/hypersaw_clap.cpp` and `src/mod_core.h` (no shell change is
needed — if you believe one is, STOP and report instead); `ROADMAP.md` /
`DECISIONS.md`; `./verify` and gates; protected paths; the untracked root
files.

## Constraints you inherit

- Branch from `main`; PRs **#527** and **#526** touch other regions of
  `gui2.html` — rebase onto main before opening your PR if they have merged.
- Verification: `node tools/labharness/lab_load_check.mjs` GREEN; then load
  `src/gui/gui2.html` via `file://` in a browser (it runs bridgeless — the
  `hz*` bindings are stubbed; confirm how the stubs behave before relying on
  them, and if `hzModAdd` is stubbed to a no-op, exercise the menu/picker UI
  paths and state that the shell round-trip is untested here). `./verify
  fast` tail verbatim.
- No machine identity in tracked files; alias discipline.

## Deliverable

Branch `b105-mod-second-route`, pushed, PR via `gh pr create --base main`
with a screenshot or DOM dump of the picker. **Never merge.** Final report:
PR URL, what was verified and how, `./verify fast` tail verbatim.
