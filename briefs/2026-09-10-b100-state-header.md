# Dispatch brief — B100 state header, engine revision, fixture corpus

**Provenance.** HYPERSAW lead organ, 2026-09-10, for a scoped subagent with zero
conversation history. Motivating decision: the ratified 1.0 definition of done
(ROADMAP §"1.0 — DEFINITION OF DONE") — "state header FIRST — before anyone but
the human saves a set" — and queue row **B100**; background in
`traces/2026-09-10-one-point-oh-proposal.md` and DECISIONS ADR-154 (the 28
Ableton sets that bind by class ID).

## Acceptance criteria (verbatim from ROADMAP B100)

> **State header + per-patch engine revision — the compatibility mechanism,
> BEFORE strangers save sets.** Today: `"schema":3` + append-only ids +
> tolerant load + one exercised migrator (ADR-103). Build: (1) header
> `{schema, engine_revision, build}` on every blob (build hash already exists
> for the GUI corner — stamp it); (2) **`engine_revision` pins DSP behaviour
> per patch** — sets saved under rev 1 load pinned to rev-1 laws, new patches
> default to latest, a patch-level control opts an old patch forward (the
> u-he/Surge pattern); the repo's existing parity-safe-superset discipline
> (new law behind a default-old switch) becomes the rev-1 path by
> construction; (3) **the oracle carries TWO golden sets, both gated**: v1
> goldens become a LEGACY-CONFORMANCE test (rev-1 must not drift — shipped
> sessions depend on it, which makes it a RULING), v2 goldens — workshopped
> against measurement + listening, not the JS — become correctness going
> forward (the goldens-v2 workshop the human has asked for, as 1.1); (4) **a
> state-fixture corpus**: real blobs per schema/revision, asserted
> load+render bit-identical forever — the notice-001 inventory is where
> fixtures come from; without it a migrator is a promise. ADR before the
> workshop; cited in the engineering writeup.

**Your slice is (1), (2) and (4).** Item (3) — the second golden set — is the
1.1 workshop; do NOT create v2 goldens. Design (2) so that v2 can be added
without a wire break: `engine_revision` is an integer in the header, 1 today,
read at load, stored per patch, and there is exactly one place in the shell
that consults it. Since no rev-2 law exists yet, the only observable
behaviour is that the value round-trips and that a blob WITHOUT the header
(every existing session) loads as rev 1.

## Files in scope

- **EDIT** `src/hypersaw_clap.cpp` — ONLY the state save/load region (search
  `"schema"`, `state_save`, `state_load`, `hypersaw-state`) and the JSON
  preset path beside it. Emit the header on save (both the host chunk and
  the JSON path — find both), parse it on load with the existing tolerance
  (missing header → schema 1 semantics as today, engine_revision 1), keep the
  ADR-103 migrator intact. The build hash is `HYPERSAW_BUILD_ID` (see
  `CMakeLists.txt` and the existing getBuildId hook). Do not touch the
  parameter table, the mod matrix, the pads, or process().
- **EDIT** `tools/state_check.cpp` — ADD assertions (never weaken existing
  ones): header present on save; header-less blob loads as revision 1; a
  blob stamped revision 1 round-trips its revision; unknown header keys are
  ignored.
- **CREATE** `tests/state_fixtures/` — the corpus: at least one blob per
  existing schema (1, 2, 3) captured from THIS build's loader semantics
  (generate them with a small tool, `tools/gen_state_fixtures.cpp`, so they
  are reproducible; commit the blobs), plus a `README.md` explaining that a
  fixture is never edited, only added. And **CREATE** `tools/statefix_check.cpp`
  — loads every fixture, asserts it loads, then renders 1 s of a fixed note
  and asserts the render is bit-identical to a stored golden per fixture
  (store the goldens beside the fixtures as raw float32; this is the "load +
  render bit-identical forever" promise). Register both tools in
  `CMakeLists.txt` as standalone executables; do NOT wire them into
  `./verify` (that is a human decision — say so in your report).
- **CREATE** `traces/2026-09-10-b100-state-header.md`.

**OUT of scope:** `ROADMAP.md` / `DECISIONS.md` (lead-only — your report
must include the ADR text you would write, the lead files it); `./verify`
itself; existing gate assertions; goldens v2; `src/gui/**` (the "opt an old
patch forward" control's GUI is a follow-up — expose the shell hook only);
`reference/**`, `specs/**`; the untracked root files.

## Constraints you inherit

- Branch from `main`; PR **#527** (B104) is open and touches other regions of
  `hypersaw_clap.cpp` — **rebase onto main before opening your PR** if it has
  merged, and resolve nothing by dropping their hunks.
- Build: `cmake -S . -B build-release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release`
  then `cmake --build "<absolute worktree path>/build-release" -j"$(sysctl -n hw.ncpu)"`.
  Worktree starts without a build dir.
- The audio thread is untouched; state save/load runs on the main thread.
- No machine identity in tracked files (leak gate); alias discipline.
- `./verify fast` after each change set, `./verify full` before done; oracle
  output verbatim; red halts you.

## Deliverable

Branch `b100-state-header`, pushed, PR via `gh pr create --base main` whose
body leads with the header format, a fixture listing, and the new checks'
output. **Never merge.** Final report: PR URL, `./verify full` tail verbatim,
the ADR draft text, and the wiring recommendation for the two new checks.
