# measure-idle-load — measuring rows for three audit findings that were read and not run

- **Queue item:** ROADMAP B455 (repo audit 2026-10-10: H4, M5, M11; the audit's proposed row 4). DECISIONS ADR-206, "The legacy freeze, as already ruled": H4 is measured first; M11 is measured and brought back. Lead brief of 2026-10-10 (horde lead session).
- **Why:** the auditor read three claims from the code and executed none. The human needs each as a measurement before deciding what the legacy freeze (ADR-186 §1) admits. This change adds rows only. Nothing in `src/` changed.
- **What changed:**
  - `tools/load_handoff_check.cpp`: 31 rows added to the 40 it had. Three groups, each beside its controls: a save taken straight after a load made while not processing (`I-SAVE*`); a second load made over queued entries (`I-SUPERSEDE`, `IS-*`); a direct host load of a chunk saved with morph on (`HOOK-ENGINE-*`, `HE-*`), including two audio comparisons with a sample-identical control.
  - `tools/lfoenv_check.cpp`: section J, four rows. Velocity (slot 14) routed to Detune, two notes at 0.2 and 0.9, in poly (the control) and in the mono branch's three paths. `Rig::note` gained a velocity argument that defaults to the value it had.
  - No new check file and no change to `./verify`: both checks were already wired in `verify full`.
- **Result:** the rows are not all green on `c44a37b`, and none is marked expected-fail. The per-row readings and what follows from them are held privately with the lead, under the B446 disclosure rule, until the human has ruled. This branch is local and is not pushed.
- **Evidence consulted:** `docs/audits/2026-10-10-repo-audit.md` (H4, M5, M11, proposed row 4); DECISIONS ADR-200, ADR-186 §1, ADR-206 (on PR #1028); `tools/load_handoff_check.cpp` as it stood; `src/hypersaw_clap.cpp` (`DirectScope`, `LoadScope`, `initState`, `applyStateJson`, `enqueueParam`, `drainQueue`, `state_save`, `state_load`, `morphRouteEdit`, `applyRoutingChunk`, the note-on handler); `libs/clap-wrapper/src` (how the wrappers service `request_flush`); the private ADR-200 notes the brief named.
- **Alternatives rejected:**
  - A new check file: the rows reuse this check's host, instance and reference helpers, and the audit asked for rows here.
  - `modreadback_check` or `mod_check` for the velocity rows: `lfoenv_check` owns the source-slot probe and already has a rig that plays notes.
  - Marking failing rows expected-fail, or wiring around them: forbidden by the brief and by the charter.
- **Verify:** `./verify fast`, exit 0, on the committed tree (see `.harness/last-verify.json`). The first run, on `0e95fe1`, was exit 1: `tolerance_registry_check` saw two new inline float comparisons in the rows (`< 1e-9`, `> 0.5`). They were removed, not registered: the rows now compare exactly, and the registry in `docs/armor/` is untouched. `verify full` was NOT run to green and cannot be on this branch: both checks exit 1 by measurement. Each check was run directly from a Release build tree; `load_handoff_check` twice, with byte-identical output.
- **Open questions:**
  - The acceptance wording for this item is the audit's proposed row 4 and the brief's; ROADMAP B455 is an "awaiting triage" row with no acceptance clause of its own.
  - No real host was driven. How long a host leaves a flush request unanswered, and whether it restores state before it starts processing, are read from the wrapper source, not measured.
  - One pairing (a host load followed by a preset load) has no row: a preset load leaves the routing matrix alone by design (B193), so the row would read that.
