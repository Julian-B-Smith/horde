# h2-plan-refresh — H2-PLAN and its map re-verified against the rebuild, through B503 and ADR-231

- **Queue item:** B387 (the plan of record, OPEN), its "Keeping this true" duty, dispatched by the
  lead 2026-10-17 after the human asked for the quickest wins while the engine is rebuilt. The
  brief cited B387 but did not quote its acceptance text; B387's own words ("every part of horde
  2, its governing rows and ADRs, its status, its dependencies and the order of work") were taken
  as the criterion, as in the 2026-10-09 refresh. Flagged to the lead.
- **Why:** the plan was last verified 2026-10-09 (partly 2026-10-11). Since then the human ruled
  the engine rebuilt from a user-level spec (ADR-223 to ADR-231), so three parts, a dozen
  prerequisites, the critical path, the order of work and about ten open decisions were stale.
- **What changed:** `docs/H2-PLAN.md` and the map's `DATA` together (map self-check 4). One part
  added, `engine2` (in progress). A new status, `superseded`, for `engine`, `port` and `edges`;
  their ids are kept because `docs/manual/figures.json` cites `engine` 33 times and `port` 19, so
  the manual is untouched. Prerequisites re-pointed to `engine2` (`modulators`, `bend`, `mpe`,
  `stereo`, `shell`, `testing`, `perf`); `sub` and `filters` now need `engine2` instead of `bend`
  (the published pitch is the engine's, ADR-225 item 7; the filter slot is RC-1); `noise` gains
  `engine2`; `audit` loses its edge from the superseded `engine`; `engine2` needs `audit` (BD-8
  keeps the parameter table open until Q31, the audit's deferred rows). The critical path is 10
  parts (was 12), one of twelve tied chains, with `engine2` tied with `seams` at its second link.
  Open decisions 47 → 58, with the answered and the narrowed ones named. Inconsistencies: 22
  re-checked (9 resolved since 2026-10-09, one in part, one now tracked under B452), 7 new. The map gains a colour and a
  bottom-row placement for the new status, and its cycle control now plants on the critical
  path's first part: on `engine` (now with no dependents) it would have stopped firing.
- **Corrections to the brief's beliefs (each from the records):** `audit` is not done: it is the
  control audit (B376/B386), not the rebuild audit (B493, done), and its 19 deferred rows are now
  open as spec Q31, so it stays ruled. `seams` is not answered: no record re-rules B275 (a), (c),
  (d) against RC-1 to RC-11. Decision #2 is re-homed to spec Q31, not removed; #6 and #7 (B406)
  stay, because the composed engine's gates run until cutover (ADR-223 item 6); #35 is narrowed
  to the rest of horde 2, since ADR-224 item 8 names a spec for the engine only. B504 and ADR-232
  are not on main: they sit on open records PR #1117 and are cited in prose only.
- **Evidence consulted:** ROADMAP rows B458–B503 in full and every older row the table cites
  (diffed against `e8441e8` and `6dd5884`); DECISIONS ADR-213 to ADR-231 with amendments (ADR-216
  A1–A4, ADR-217 A1–A3, ADR-219 A1–A2, ADR-221 A1–A2, ADR-224 A1, ADR-229 A1, ADR-230 A1);
  B504 and ADR-232 on `origin/lead-records-272` (PR #1117, OPEN); `specs/SPEC-ENGINE.md` §6.3,
  §6.5, §7.2, §10.5 BD-8 and BD-10, §11; `docs/port/spec-engine-provenance.md` (Q31 row);
  `docs/audits/2026-10-15-engine-rebuild-audit.md`; `specs/ACCEPTANCE.md` L0-1 and L0-13;
  CLAUDE.md §Domain; `docs/manual/figures.json` `depends_on` counts; `git ls-remote --tags`.
- **Self-check, outside a browser:** no tool in the repo runs the map's `?check=1` headless
  (`lab_load_check` only loads the page). As in the 2026-10-09 refresh, a scratch harness ran the
  page's script in `node:vm` under stub DOM and called its own `runChecks()` against the files on
  disk: SELF-CHECK GREEN, 5/5 checks pass, 5/5 controls fire; critical path 10. A second scratch
  script enumerated the tied chains (12). `manual_scaffold_check` GREEN.
- **Alternatives rejected:** deleting `engine`, `port`, `edges` (breaks the manual's citations);
  re-pointing the manual's 52 entries to `engine2` (out of scope; noted for B451); marking
  `audit` done or `seams` answered (the records do not support it); citing B504 and ADR-232 in the
  table (check 1 would fail on main until #1117 merges); a separate glide part (B504 is under
  `bend`, as the record has it).
- **Verify:** see the PR; `./verify fast` run on the staged tree, result and hash in
  `.harness/last-verify.json`.
- **Open questions:** (1) the brief's acceptance criteria were not quoted from ROADMAP; (2) the
  reading that ADR-189's D1–D3 defaults are absorbed by spec ED-1, BL-12 and C.14 is this plan's,
  not a record's; (3) the local clock read 2026-10-16 23:55 EDT when this was written, while the
  records and the brief are dated 2026-10-17; the plan uses 2026-10-17; (4) the browser
  self-check was not run; the node harness runs the same functions.
