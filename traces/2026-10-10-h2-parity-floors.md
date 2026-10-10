# h2-parity-floors — the two horde 2 stream-parity gates fail below a pinned scenario count

- **Queue item:** B455 H1 follow-up (ADR-206 item 1, approved by the human 2026-10-10): "in-source floors
  for those two gates, with a control that drops a scenario from the binary stream".
- **Why:** `h2_engine_parity_check` and `h2_scalpel_parity_check` read a binary stream, not a manifest, so
  #1031 left them out. The engine gate checked that header, END and rows agree (a consistency check, not a
  floor) and the scalpel gate pinned only the chaotic-exclusion count: a stream with a scenario dropped and
  its header and END rewritten to agree passed both. Each now pins its total in its own source
  (`kMinScenarios`, via `scenarioFloorHolds`): engine 543, scalpel 386, both read off a real render before
  being written. Fewer is a FAIL naming both numbers and counts as infrastructure (the FMA controls exit 2,
  never "fired"). More prints "raise the floor in the same PR".
- **Evidence consulted:** tools/h2_engine_parity_check.cpp (STREAM block, `infra`), tools/h2_scalpel_parity_check.cpp,
  tools/h2_engine_stream.h and tools/h2_scalpel_stream.h (the record format the Python walker mirrors: LIBM
  n*3 float64, DATA frames*2 float64), tools/h2_engine_render.mjs / h2_scalpel_render.mjs (header and END
  lines), tools/scenario_floor.h, tools/parity_floor_check.py (#1031), `verify` full() (TRUNC control, the
  scalpel and engine blocks), tools/sanitize_oracles.sh (self-spawn path kept working).
- **What changed:** (1) the two floors. (2) `h2_scalpel_parity_check` gains `--full-from FILE` (same meaning as
  the engine's: the caller declares the file the full render; a plain file stays a subset, so no floor, no
  pin); `verify` now renders the scalpel stream once to a file and feeds the gate, its FMA control and the
  controls from it (it rendered twice before). (3) `tools/parity_floor_check.py --stream <build> <gate> <file>`
  derives scratch streams from the real one and runs DROP (last scenario removed, header and END rewritten),
  HEADER-ONLY (END 0), EMPTY (zero bytes), ABSENT (no file): each must read red, the first two BY the floor
  line naming the right counts, and on DROP the floor line must be the only FAIL line (so nothing else turned
  it red). The untouched stream's green run is `verify`'s own run of the gate on the same file (POSITIVE).
  (4) `verify` comments no longer imply the engine count was already pinned; the floor lines are echoed on
  green runs.
- **Alternatives rejected:** controls in `verify` shell beside TRUNC (the stream surgery needs a binary walker;
  shell cannot do it without guessing offsets); re-rendering the stream per control (55 s); a separate new
  tool file (outside the brief's scope list); a header/END cross-check in the scalpel gate (not asked).
- **Item 5 (same shape elsewhere):** `h2_swarm_lift_check` iterates a hard-coded `{44100, 48000}` and has no
  corpus or manifest, so it cannot shrink silently (changing it is a source edit with its own control): not the
  same shape. `h2_swarm_parity_check` / `h2_swarm48_check` were floored by #1031 in `parity_floor_check.py`.
  `h2_engine_selfdigest_check` pins every row but only on a keyed platform (unkeyed CI: SKIPPED); the engine
  corpus it reads is now floored by the parity gate on that same stream in `verify full`, and in the sanitize
  lanes where the parity gate self-spawns. No other `h2_*` gate in `verify` reads a corpus.
- **Cost, measured on this Mac, Release:** the controls add 7.2 s (scalpel) and 8.4 s (engine) to `verify full`;
  rendering the scalpel stream once instead of twice removes one ~15-25 s render, so the net is not slower.
- **Verify:** `./verify fast` exit 0 and `./verify full` exit 0 at aff80a3 (`.harness/last-verify.json`,
  full ts 2026-10-10T22:13:32Z). `fast` is re-run at the pushed commit, which adds this trace only.
- **Open questions:** none. Not run: the DROP stream against the pre-change binaries; the control is
  proven to see the floor alone by its only-FAIL-line rule, and the pre-change engine STREAM row passes on the
  same shape (header, END and rows agree: see the STREAM block).
