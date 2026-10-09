# b448-b2-stress-coverage — wider stress schedule and a deterministic load-ordering oracle

- **Queue item:** B448 Wave B item B2, continuing `traces/2026-10-09-b448-b2-tsan-stress-toolchain.md`; the rework the C1 critic asked for before the ADR.
- **Why:** the stress schedule did not drive several main-thread paths, and two load properties are about ordering, which ThreadSanitizer cannot see.
  - `tools/tsan_stress.cpp` adds three seeded ops: the editor's PANIC, the host's `params.get_value`/`value_to_text` reads, and a burst past the editor queue's capacity.
  - `tools/tsan_stress_check.py` runs three seeds with `history_size=7`, and redirects HOME into the build dir so PANIC's forensic dump stays out of the user's store.
  - New `tools/load_handoff_check.cpp` (CMake target `load_handoff_check`, links the impl) is single-threaded and deterministic. It has two rows, each with controls that must read green, plus must-read-nonzero guards. UNWIRED per ADR-180 §1.
- **Evidence consulted:** the critic report (local), `src/hypersaw_clap.cpp` (initState, state_load's key handlers, morphRouteEdit, applyParam's morph hook, panicWithDump, dumpForensics, enqueueParam), and `src/gui/preset_store.h` (presetRoot reads HOME).
- **Alternatives rejected:** making the ordering rows modes of the TSan harness (they must not need a sanitizer toolchain to run); reaching into shell internals (the CLAP surface, the host's request_flush callback and the corner-values probe are enough, so the oracle links the impl like the other probes).
- **Verify:** recorded in the handoff together with `.harness/last-verify.json`. The expected red lines are on `weakening_check`: the stress check's two increases, plus `unwired` +1 for the new check. All are unapproved.
- **Open questions:** OUT-OF-SCOPE RED: the weakening-counter increases await the human. The oracle and the stress check are red on main by design until the handoff fix lands.
