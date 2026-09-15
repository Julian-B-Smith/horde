# 2026-09-14 — ORBITAL intake (ADR-165, B126)

Two files at the root (`gravity-modulator.html`, `orbital-modulator-spec.md`)
moved to `reference/gravity-modulator.html` and `specs/SPEC-ORBITAL.md` (the
ADR-155 layout). Protected paths updated in CLAUDE.md §Domain; the three spent
RNG-seed sanctions retired from that list at the same touch (the seeds landed
2026-09-10). One new sanction: the add-body `Math.random` (line 430).

Triage on ROADMAP B126: placement (modulation lab), the source-slot collision
(24 fixed slots vs ~50 observables → a source bank is the recommendation), the
behavioural-oracle rule, the seams it needs, the gate gap (`lab_load_check`
does not glob `reference/`), and the lead's answers to the spec's §12.

`./verify fast` exit 0 (structure gate covers the new paths).
