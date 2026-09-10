# Dispatch brief — B103 the evidence layer

**Provenance.** HYPERSAW lead organ, 2026-09-10, for a scoped subagent with zero
conversation history. Motivating decision: the 1.0 definition of done (ROADMAP
§"1.0 — DEFINITION OF DONE", ratified 2026-09-10) and queue row **B103**,
filed from an external audit the lead verified (trace
`traces/2026-09-10-one-point-oh-proposal.md`). The audit's own words: "the
part your instincts will tell you to skip."

## Acceptance criteria (verbatim from ROADMAP B103)

> **The evidence layer — measurements, writeup, landing page, video** (audit
> 2026-09-10). Measurements in docs/: aliasing spectra at 3–4 notes with saw
> shape engaged, CPU per voice count, a listening note — the direct answer to
> "parity isn't correctness" ahead of goldens v2; a one-page engineering
> writeup (correctness definition, RULING/ENCODING taxonomy, agentic-process
> governance, the goldens problem + v2 plan — B100 is its centrepiece); README
> as landing page (pitch, three clips, screenshots, download, link to the
> writeup); the video — 90 s sound (coupling sweep, gravity settling a chord,
> morph patchwork), 2 min process (the oracle running, the gaps, an ADR). Repo
> description + topics; the account rename is sequenced by autonomous
> Decision 69 (B98).

Your slice is the **measurements and the writeup draft and the README
structure**. The video, the audio clips, the screenshots and the listening
note are the human's (they need ears and a DAW) — leave marked slots.

## Files in scope

- **CREATE** `tools/measure_alias.cpp` — deterministic, core-direct or via
  the CLAP factory (copy the `Probe` pattern from `tools/combguard_check.cpp`
  and `tools/notefuzz_scaffold.inc`). Render single notes at MIDI 36, 60, 84,
  96 with the saw-shape section engaged (params: `round` id 131 at 0.6,
  `roundHi` 132 at 0.5, `sawProfile` 130 at 0.5 — confirm ids in
  `src/hypersaw_clap.cpp`'s table before use) and `digital` (polyBLEP) on
  and off; FFT the steady state (write a small radix-2 FFT in the tool — no
  dependency); report, per note, the ratio of energy at non-harmonic bins to
  harmonic bins in dB (the aliasing figure) at 44.1 k and 96 k. Register in
  `CMakeLists.txt` beside `svf_check` (standalone, NOT in `./verify`). Print
  a markdown table.
- **CREATE** `tools/measure_cpu.cpp` — CPU per voice count: render 5 s at
  n = 1, 4, 8, 16, 32 voices, one and two oscillators, report ms-of-CPU per
  second-of-audio (`shell_bench.cpp` / `renderer_bench.cpp` show the timing
  pattern already in the tree — reuse their approach, not their code).
  Release build only (note the machine's CPU model in the doc, never its
  hostname or username).
- **CREATE** `docs/MEASUREMENTS.md` — the two tables with the exact commands
  that produced them, the build hash, and a short reading of what they show
  (and don't). A **Listening note** section left as a marked slot for the
  human.
- **CREATE** `docs/ENGINEERING.md` — the one-page writeup, marked **DRAFT —
  the human's voice replaces this**. Sections, in order: how correctness is
  defined here (L0-1 parity vs the JS reference at 1e-6 RMS plus L0
  trajectory criteria — `specs/ACCEPTANCE.md`); the RULING / ENCODING
  taxonomy (find it in `DECISIONS.md` and `tests/feature_tests.tsv`'s
  columns — cite, don't invent); how the agentic process was governed
  (`CLAUDE.md`'s charter: ROADMAP as single truth, oracles gate phases, human
  gates, traces per merge, the knowledge loop); the goldens problem and the
  v2 plan (ROADMAP **B100** verbatim is the centrepiece). Every claim cites a
  file. Five minutes to read.
- **EDIT** `README.md` — restructure the TOP into a landing page: a
  30-second pitch, a **screenshots** slot and **three audio clips** slot and
  **download** slot each marked `TODO(human)` with exactly what is needed
  (never fabricate media or links), then a link to `docs/ENGINEERING.md` and
  `docs/MEASUREMENTS.md`, then the existing content below a divider. Keep
  every existing checkable claim; do not freshen the dated "last verified"
  line unless you re-verified those claims.
- **CREATE** `traces/2026-09-10-b103-evidence-layer.md`.

**OUT of scope:** `ROADMAP.md` and `DECISIONS.md` (lead-only — report what
they should record); `./verify` and existing gate tools; `src/**`;
`reference/**`, `specs/**` (protected); the video; the listening note; any
media files; the GitHub repo description/topics (human, via the web UI); the
untracked files at the repo root.

## Constraints you inherit

- Build: `cmake -S . -B build-release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release`
  then `cmake --build "$(pwd)/build-release" -j"$(sysctl -n hw.ncpu)"`;
  worktree starts without a build dir.
- Measurements are deterministic: seeded RNG only, no wall-clock in the
  rendering (timing the render is fine); same inputs → same table.
- **No machine identity in tracked files** (leak gate in `./verify fast`).
- **Alias discipline:** never write a private sibling's real name into a
  tracked file.
- `./verify fast` after each change set; `./verify full` before done; oracle
  output reported verbatim; red halts you.

## Deliverable

Branch `b103-evidence-layer` off `main`, pushed, PR via
`gh pr create --base main` whose body leads with the two tables. **Never
merge.** Final report: PR URL, `./verify full` last lines verbatim, and what
ROADMAP / DECISIONS should record.
