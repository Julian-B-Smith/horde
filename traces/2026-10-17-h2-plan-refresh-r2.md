# h2-plan-refresh-r2 — open decision on ADR-189's D1–D3 defaults restored, at the lead's review

- **Queue item:** B387, the lead's review of PR #1118. Corrects `traces/2026-10-17-h2-plan-refresh.md`
  (append-only; that entry is left as written).
- **Why:** the first commit (`63e177f`) removed open decision #5 (ADR-189's D1–D3 defaults in the
  instrument) on this plan's own reading that spec ED-1, BL-12 and C.14 cover them. No record says
  so, and removing a decision on a reading is a guess the charter forbids. The lead: restore it as
  OPEN, with the reading kept as a proposal for the human.
- **What changed:** `docs/H2-PLAN.md`: #5 is back as item 6 ("ADR-189 defaults ... The lead's
  reading: spec ED-1, BL-12 and C.14 may cover them; for the human to confirm"), removed from
  "Answered since 2026-10-09", and named under "Kept"; the list is renumbered (58 → 59), and the
  cross-references to item numbers are corrected (true stereo 30, spec 38, WebAssembly 45,
  commercial model 57, Shriek 33). Four of those references were already wrong in `63e177f`: they
  carried the map's flat decision index, not the doc's list numbers. The `engine2` detail section's
  Open line carries the item. The map: the decision is added to `engine2`'s list, ADR-189 to its
  ADRs, and the superseded `engine`'s note says the defaults went to `engine2`, still open.
- **Evidence consulted:** the lead's review message; DECISIONS ADR-189; SPEC-ENGINE ED-1;
  `docs/port/spec-engine-provenance.md` (ED-1, BL-12, C.14 cite ADR-189 as a source, not as an
  answer).
- **Alternatives rejected:** leaving #5 under the superseded `engine` (that part has no decisions
  now; the question is about the new instrument).
- **Verify:** `./verify fast` on the staged tree; result in `.harness/last-verify.json`; the map's
  `runChecks()` in node:vm.
- **Open questions:** none new. B504 and ADR-232 stay out until #1117 merges (the lead adds them).
