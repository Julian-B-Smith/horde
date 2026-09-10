# Dispatch brief — B107 corner-preset dropdowns lose their selection on save

**Provenance.** HYPERSAW lead organ, 2026-09-10, for a scoped subagent with zero
conversation history. Motivating report (human, 2026-09-10, verbatim): *"When
you save a morph corner preset, it removes the selected presets from the morph
dropdowns."* Queue row **B107**.

## Acceptance criteria (verbatim from ROADMAP B107)

> **Saving a morph corner preset clears the corners' dropdown selections.**
> Mechanism to confirm at build: the corner-list refresh after save
> repopulates each corner's select and does not restore the previously
> selected name (the refresh helper's "keep what you are showing on null"
> guard covers the failed-list case, not the success case). Fix: capture each
> select's value before repopulating, restore if still present.

## Files in scope

- **EDIT** `src/gui/gui2.html` — ONLY the morph-corner preset code: the
  `presetStore` object and the corner rows built under `mcorners` (search for
  `hzPresetList`, `CORNER_KEY`, `mcorners`, and the per-corner `load` select).
  Confirm the mechanism first by reading the refresh path; if the mechanism
  differs from the row's hypothesis, fix the real one and say so.
- **CREATE** `traces/2026-09-10-b107-corner-dropdowns.md`.

**OUT of scope:** everything else in `gui2.html` (the XY pad section, the
MOD page and the MAIN visualizers are being rewritten in parallel — do not
touch them); `src/hypersaw_clap.cpp`; `ROADMAP.md` / `DECISIONS.md`
(lead-only); `./verify` and gates; protected paths; the untracked root files.

## Verification you must do

1. `node tools/labharness/lab_load_check.mjs` — must stay GREEN (26 labs).
2. Open `src/gui/gui2.html` in a browser (a `file://` load works; the page
   falls back to browser storage when no plugin bridge exists), go to MORPH,
   save a corner preset with a name, pick it in two corner dropdowns, save
   another preset, and confirm both selections survive. Describe exactly what
   you observed before and after the fix.
3. `./verify fast` — report the last lines verbatim.

## Deliverable

Branch `b107-corner-dropdowns` off `main`, pushed, PR via
`gh pr create --base main`. **Never merge.** Final report: PR URL, the
observed before/after, `./verify fast` tail verbatim.
