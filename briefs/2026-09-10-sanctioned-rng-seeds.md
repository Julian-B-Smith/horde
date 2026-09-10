# Dispatch brief — the three sanctioned prototype edits: seed the unseeded RNGs

**Provenance.** HYPERSAW lead organ, 2026-09-10, for a scoped subagent with zero
conversation history. Motivating decisions: ADR-091 (CANTO), ADR-122 (STATION),
ADR-152 (intent bus) — each ingested a browser prototype as a protected
reference with **exactly one sanctioned edit outstanding**: seed its unseeded
`Math.random`. CLAUDE.md §Domain "Protected paths" names all three. The
standing sanction IS the human gate for these three edits and nothing else in
those files.

## The edits (one per file, nothing else)

1. `reference/intent-bus.html` — `reshuffle()` (around line 176) draws
   `Math.random()` for every atom seed. Replace with a mulberry32 stream
   seeded from a visible seed (a `seed` field on the device state, default
   1024, exposed in the UI next to the Reshuffle button), so a reshuffle is
   reproducible: same seed → same flip topology. SPEC-INTENT-BUS §3.5 says
   seeds persist with the device state — keep that true.
2. `reference/formant-pulsar-fof.html` — `FormantCore`'s masking RNG (search
   `Math.random` inside the core; ADR-091 names it). Same treatment: a
   mulberry32 stream owned by the core, seeded from an existing or new `seed`
   parameter.
3. `reference/station.html` — line ~167, `RND:i=>Math.floor(Math.random()*16)`
   (the Wave RAM randomize, ADR-122). Same treatment.

Use the mulberry32 implementation the repo already uses in its labs (search
`mulberry32` in `reference/swarmsaw.html`) — copy that exact function so every
prototype shares one RNG definition. Any OTHER `Math.random` in those files
that is purely cosmetic (a UI colour, a demo animation) is left alone and
listed in your report; anything in an audio or state path is in scope.

## Files in scope

- **EDIT** the three files above — the RNG swap and the seed control only.
- **CREATE** `traces/2026-09-10-sanctioned-rng-seeds.md` — list every
  `Math.random` site found per file, which were replaced, which were left
  and why.

**OUT of scope:** any other change to those files (they are the spec — an
edit there is a spec change and only the three above are sanctioned);
`specs/**`; `src/**`; `ROADMAP.md` / `DECISIONS.md` (lead-only — report the
ADR amendment text); `./verify`; the untracked root files.

## Verification you must do

- Each file still loads: `node tools/labharness/lab_load_check.mjs reference/intent-bus.html reference/formant-pulsar-fof.html reference/station.html`.
- Determinism: for each file, write a tiny Node/vm harness (model it on
  `lab_load_check.mjs`) that calls the seeded function twice with the same
  seed and asserts identical output, and once with a different seed and
  asserts different output. Include the harness output in the trace.
- `./verify fast` tail verbatim (the leak gate and structure gate cover
  these paths).

## Deliverable

Branch `sanctioned-rng-seeds`, pushed, PR via `gh pr create --base main`
whose body lists the three sites and the determinism evidence. **Never
merge.** Final report: PR URL, the site list, the harness output.
