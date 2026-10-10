# host-value-guards-velocity-drop — a NOTE_ON whose velocity is not finite is dropped

- **Queue item:** B455; ADR-206 item 2. Follows `traces/2026-10-10-host-value-guards.md` on the same branch
  (PR #1032) and corrects one choice recorded there.
- **Why:** the earlier trace sent a NOTE_ON with a non-finite velocity down the path the site gives velocity 0
  and below (the release), and listed "drop it" under alternatives rejected. The lead ruled the other way on
  review: the event is ignored, the same as an out-of-range key or a non-finite expression value, because a
  malformed note-on must not end a note that is sounding on its key. The earlier trace's velocity paragraph and
  that rejected alternative are superseded by this entry; the rest of it stands.
- **What changed:**
  - `src/hypersaw_clap.cpp`, NOTE_ON in `handleEvent`: the velocity check now sits directly after the key check
    and `break`s when `finiteClamp` reports a non-finite value, before the note counters, as the key check does.
    Velocity 0 and below still take the release (ADR-038); a finite velocity is still clamped to CLAP's 0..1;
    a velocity in (0, 1] still passes through exactly.
  - `tools/hostile_events_check.cpp`, VEL: for NaN, +inf and -inf, the held-key row now asserts key 60 is still
    held, one voice, source 14 still 1, and the render equal bit for bit to the render where nothing was sent;
    the fresh-key row still asserts no voice and silence. 0 and -0.5 keep their release rows. Still 89 rows.
- **The lead's other rulings, recorded:** the 1 BPM tempo floor is accepted as it is ("a tempo at or below 1 BPM
  is treated as no tempo"); a velocity above 1 reading 1 is accepted (CLAP's range).
- **Evidence consulted:** the lead's rulings on PR #1032's open questions (2026-10-10); the key and expression
  checks in `handleEvent` (the drop this now matches); the earlier trace.
- **Alternatives rejected:** leaving the drop where the old check stood, after the note counters. The key check
  drops before them, so a dropped event is not counted as a note the host played; this matches it.
- **Must-fail proof (mutations, each reverted):** velocity check removed: 7 rows red, 82 green. Drop replaced by
  the release (the earlier form): 3 rows red, 86 green, the three held-key rows. Restored: 89 green. The row
  "velocity 0 on a held key releases it" asserts the release render differs from the held render, so the three
  held-key equalities are not blind.
- **Verify:** `./verify fast`, exit 0, at 9e27201 (the code commit; this trace is the only later change), per
  `.harness/last-verify.json`; `weakening_check` 0 increases. On the Release build of that commit:
  `hostile_events_check: GREEN` (89 rows); `parity_check` (156/156, worst 4.262e-09), `state_check`,
  `statefix_check` and `load_handoff_check` print output byte-identical to the pre-branch baseline;
  `rtsan_check: GREEN` (plant 1, control 0, full 0 violation stacks); `tsan_stress_check: GREEN` (three seeds; main-thread WRITE 0 on
  each; main-thread READ 38, 47 and 53, the accepted class). `./verify full` as a whole was not run.
- **Open questions:** none from this change. -inf was a release before this branch (it satisfied `<= 0`) and is
  now dropped with the other non-finite values, as ruled.
