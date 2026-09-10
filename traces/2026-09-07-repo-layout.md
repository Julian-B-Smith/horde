# 2026-09-07 — repo layout: reference/ and specs/ (ADR-155)

**What changed.** 24 root files moved with `git mv`: 13 prototype HTMLs to
`reference/` (basenames standardised, `horde_` prefixes dropped, `swarm*`
kept), 9 specs + ACCEPTANCE to `specs/` as `SPEC-<NAME>.md`, PRIOR-ART and
PARKED to `docs/`. Every live reference rewritten root-relative in the same
change (47 files: tools/golden extractors, port_gap, trajectory_check,
`./verify` structure list, CLAUDE.md protected paths, README map, ROADMAP,
LIBRARY, specs' cross-refs, core comments, design labs, index.html); history
(traces, DECISIONS, audits, briefs, integrations) left as written.

**Verify.** `./verify full` exit 0 post-move; lab_load 26/0; no stale old
names in live files; no double prefixes; 24 renames detected by git.
