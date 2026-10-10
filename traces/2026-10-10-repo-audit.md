# repo-audit-2026-10-10 — the second repo-wide audit sweep

- **Queue item:** the auditor cadence (CLAUDE.md §Domain; B159, ADR-179 §3), dispatched by
  the horde lead on 2026-10-10 inside the overnight batch recorded under ROADMAP B448. A
  read-only auditor run (`.claude/agents/auditor.md`).
- **Why:** the last repo-wide audit was 2026-09-19, 21 days back against a 7-day cadence,
  and the tree had since gained `h2/`, the B446 security work, the ADR-200 thread handoff
  and the B448 armor gates.
- **What changed:** one new file, `docs/audits/2026-10-10-repo-audit.md`, and this trace.
  No code, spec, reference, check, hook or ROADMAP file was touched.
  - 6 HIGH, 11 MEDIUM and 8 LOW findings, each with file:line evidence and a minimal delta.
  - 12 proposed ROADMAP rows, each with an acceptance test, for the lead to word and number.
  - Appendix A: 31 false statements in eight documents, with the contradicting fact.
  - One finding is withheld from the report and was given to the lead directly.
- **Evidence consulted:** `verify` (all 1,168 lines), `CMakeLists.txt`,
  `.github/workflows/*.yml`, `.claude/settings.json` and `.claude/hooks/*`,
  `.kit/kit-gates.sh`, `tools/sanitize_oracles.sh`, `tools/test_table_check.py`,
  `tools/build_flags_check.py`, `tools/param_id_lock_check.py`,
  `tools/load_handoff_check.cpp`, the golden-parity checks, `src/hypersaw_clap.cpp` (the
  queue, the load doors, the event handler), `src/input_guards.h`, `src/output_latch.h`,
  `src/fx_rack.h`, `h2/README.md`, `h2/engine/*.h`, `docs/armor/catalogue.json`,
  `docs/strategy/blind-spot-armor.md`, README.md, CLAUDE.md, `docs/ENGINEERING.md`,
  `docs/ROBUSTNESS.md`, `docs/PARKED.md`, `docs/manual/`, ROADMAP rows B147, B155, B256,
  B403, B448 and B454, `docs/audits/2026-09-19-repo-audit.md`, and the log of CI run
  38026008521 (both sanitize legs).
- **Method notes.**
  - MEASURED: each of the 43 commands in `fast()` timed alone, twice, by a scratch script
    (not committed); `build_flags_check`, `license_audit_check` and `deny_hook_check` under
    cProfile; 26 never-run targets compiled as single translation units into a scratch
    directory; ROADMAP statistics by a scratch script.
  - PROVEN: eleven golden-parity binaries and three directory-driven ones run against an
    empty manifest or an empty directory. The binaries were the main checkout's existing
    Release build of 2026-10-04; their sources were last changed 2026-09-21.
  - BY READING: H4, M5 and M11. None was executed; the report names the rows that would.
  - The machine was under a load average of 6 to 10 on 8 cores throughout, so wall times
    are upper bounds. `verify full` was not run: no build tree in the worktree.
- **Verify:** `./verify fast` exit 0 at `4a3dbaf` before the report was written, and again
  on the committed tree; the second result is in the PR body from
  `.harness/last-verify.json`. In this worktree `license_audit_check` and
  `choc_patch_check` print WARNING for the absent submodules, and `sluice_hold` and
  `mailbox_delivery` print SKIPPED; the report's M3 is about exactly that.
