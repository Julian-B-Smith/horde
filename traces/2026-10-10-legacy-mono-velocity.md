# legacy-mono-velocity — mono note-ons write the velocity source

- **Queue item:** B455 (repo audit 2026-10-10, M11). A feature bug, outside what ADR-186 §1 admits by
  default; held on its own branch for the human's ruling (ADR-206). The measuring rows (section J of
  `tools/lfoenv_check.cpp`) are this branch's first commit. It sits on `origin/host-value-guards`, which
  rewrote the handler this line is in.
- **Why:** matrix source 14 (Velocity) is written by the poly and SPECTRA branches of the NOTE_ON handler
  and was not written by the mono branch.
- **What changed:** one line in `src/hypersaw_clap.cpp`: the mono branch sets `srcVel` to the note's checked
  velocity, beside the note tag, as the two branches beside it do.
- **Rows (`tools/lfoenv_check.cpp`, section J):** 48 before the measuring rows; 54 now, all ok.
  - The source slot and the applied destination, in poly (the control) and in the mono branch's three paths.
  - In the render: mono legato, a held note, a second note over it at 0.2 or at 0.9. A legato retarget keeps
    the voice's gain, so the second velocity reaches the samples through the route alone. With a Velocity to
    Detune route the two renders differ and each differs from its no-route render; with no route the two
    are the same samples.
- **Evidence consulted:** `src/hypersaw_clap.cpp` (the NOTE_ON handler's three branches, `retargetAll`);
  `src/swarm_core.h` read only (`setNoteVelocity`, the voice gain); `docs/audits/2026-10-10-repo-audit.md`
  (M11); `tools/lfoenv_check.cpp` as it stood.
- **Alternatives rejected:**
  - *Compare a routed render at two velocities in poly, or on a fresh strike.* Velocity scales the voice
    gain there, so the renders differ with or without the route. The legato retarget is the path where the
    route is the only carrier, and the no-route control shows it.
  - *A tolerance on a gain-normalised comparison.* Every comparison here is exact.
- **Must-fail proof (the line removed, then restored):** 5 rows red: the three mono source rows, "the render
  with the second note at 0.2 differs from the one at 0.9" (0 samples differ), and "each routed render
  differs from its no-route render". The no-route control stays green.
- **Unchanged output for other input:** `render_neutral_digest` before and after: 48 of 48 digests identical.
  None of the 48 inputs is a mono patch with a route from source 14, which is why the render rows above
  exist.
- **Verify:** `./verify fast`, exit 0, at `56d333e` (this trace is the only later change), per
  `.harness/last-verify.json`. `./verify full` on the branch head, with `rtsan_check` and `tsan_stress_check`
  in it, is reported in the hand-back. One by one at that commit, Release build: `OK   lfoenv_check: 0
  failure(s)` (54 rows); `state_check`, `statefix_check`, `bank_check` and `undo_check` green; `parity_check`
  156/156. `weakening_check`: 0 increases. `tolerance_registry_check`: 0 changed.
- **Open questions:**
  1. This changes the sound of any patch that is mono and routes Velocity somewhere; that is the fix.
  2. The human has not ruled on whether the freeze admits it.
