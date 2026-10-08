# b448-a1-armor-catalogue — one armor catalogue, a coverage gate, a generated dashboard

- **Queue item:** B448 Phase 1 Wave A item A1 (ADR-197; shared with B446).
- **Why:** The ratified brief (`docs/strategy/blind-spot-armor.md`) requires every risk-register
  row to be a named gate or a visible hole. Before this, the armor score lived only in ROADMAP
  prose, which nothing checks. Now `docs/armor/catalogue.json` holds 23 rows (R1–R12, S1–S7,
  SEC-input/-webview/-supply/-hygiene). `tools/armor_coverage_check.py` gates them in
  `./verify fast`: gates are parsed from `./verify`'s code lines and the workflows, and the check
  runs ten must-fail controls. `tools/armor_dashboard.py` generates the one-screen
  `docs/armor/dashboard.html` and stays current through `--check`.
- **Evidence consulted:** `docs/strategy/blind-spot-armor.md` (all of it); ROADMAP B448 row;
  `verify` lines 60–400 and 530–1031; `.github/workflows/{ci,cpu-derate,docs}.yml`;
  `tools/test_table_check.py` (invocation parse); `tools/private_name_check.py` (names-file
  lookup, imported); `tools/sanitize_oracles.sh` header; `tools/rtsafety_probe.cpp:133`
  (param ids `1 + (... % 99)`); `tools/stability_check.cpp` header; `src/input_guards.h:139`
  and `src/hypersaw_clap.cpp:8943-8944` (the `zeroNonFinite` return value is discarded);
  `tests/state_fixtures/` (3 fixtures).
- **Corrections to the lead's seeded assessment:**
  - R5 went from hole to partial. `stability_check --seconds=60` runs in `verify full`
    (verify:764) and bounds RMS drift, f0 drift, peak and non-finite count on the legacy SAW
    core. That is a long-run-drift gate, though not a 4 h soak. The score is now 0/12 green,
    12 partial, 0 hole, against ROADMAP's "11 partial, 1 hole".
  - R8: auval is not a gate. Neither verify nor CI runs it (verify:531 and ci.yml:13/188 call
    it human-run). The gates are the CI jobs `validate-windows` (every PR) and `validate-macos`
    (main pushes only).
  - R11: "x86 is never tested" is too strong. CI's `sanitize` job runs the compiled oracles on
    ubuntu-latest (x86_64), and `build-windows`/`validate-windows` build and validate there.
    What is missing is a parity run on x86_64 at the shipped Release flags.
  - R3: oracles do count non-finite samples in test renders (`stability_check`,
    `hostile_events_check`). The lead's point holds at run time: the shipped guard zeroes and
    its count is discarded.
  - Every other seeded claim matched the tree.
- **Alternatives rejected:**
  - A dashboard that reads `./verify` to mark gates present. Every PR that wires a gate would
    make the committed page stale, so `--check` would go red on unrelated PRs. Presence is the
    coverage check's job; the page renders from the catalogue alone.
  - A YAML parser for the workflows. PyYAML is not a dependency, and job ids and `- name:` steps
    are the only shapes the catalogue cites.
  - An `accepted` status value. The brief fixes three statuses. An accepted hole is a hole whose
    `expires` the human moved, and the README says so.
- **Verify:** `./verify fast`, exit 0, `.harness/last-verify.json` git `13e9035` (the base; the
  change set was uncommitted when it ran).
- **Open questions:**
  - The parallel Wave A PRs that add `docs/armor/tolerances.json` or
    `docs/armor/weakening-baseline.json` must regenerate `dashboard.html` in the same PR, or
    `armor_dashboard.py --check` goes red. Section 2 reports only each file's presence and
    entry count, because their schemas are not on main.
  - The S-row and SEC-row tripwires are my wording, since the brief gives tripwires only for
    R1–R12.
  - S3/S4 list pending gates. Once `tolerance_registry_check` and `weakening_check` run, their
    PRs should drop `"pending"` (the check reports a pending gate, never fails it).
