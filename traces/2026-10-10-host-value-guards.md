# host-value-guards — host tempo and note velocity are checked at the event boundary

- **Queue item:** B455 (the item held back from the 2026-10-10 audit report); ruled by the human in ADR-206
  item 2 ("fixed as a small boundary fix with test rows, and published together with its fix").
- **Why:** host tempo and note velocity were the two host values that entered the legacy shell without the
  boundary check the other host values have (note keys, PRESSURE, TUNING: `src/input_guards.h`, B446). They are
  now checked where they enter, like the others. This is input validation at a trust boundary, which the armor's
  "Symptom clamping" rule exempts and requires (`docs/strategy/blind-spot-armor.md`).
- **What changed:**
  - **Tempo.** `Plugin::takeHostTempo` (`src/hypersaw_clap.cpp`) is the one place a host tempo is stored. Both
    doors call it: the block's transport in `process()` and a `CLAP_EVENT_TRANSPORT` in `handleEvent`. A tempo is
    stored only if `hypersaw::hostTempoUsable` (`src/input_guards.h`) says so: finite and above 1 BPM. Any other
    value is treated as the host providing none, so the tempo keeps its last usable value, which is the 120
    default (`swarm_core.h` Params) until a usable one arrives. A usable tempo is stored exactly as sent.
  - **Velocity.** In `handleEvent`'s NOTE_ON, the velocity passes through the existing `finiteClamp` with CLAP's
    documented 0..1. A value that is not finite takes the path that site already gives velocity 0 and below (the
    release, ADR-038). Every later reader in that case (voice gain, Sub Osc, note tag, matrix source 14) takes the
    checked value. A velocity in (0, 1] passes through exactly.
  - **Rows.** `tools/hostile_events_check.cpp` gains VEL (16 rows) and TEMPO (40 rows); 33 rows before, 89 after.
    The existing LATCH row sums the output guard's counter over every run, the new ones included, and reads 0.
    Each family has a row showing the value reaches the render at all (velocity 0.6, tempo 97), so its equality
    rows are not blind. The VEL rows first set id 52 for the sub; that is SPECTRA's sub, not the SUB OSC gate
    (4015). Corrected in 36f8043, with a baseline row showing the sub sounds.
- **Evidence consulted:** `src/input_guards.h` (the B446 helpers and their style); `src/hypersaw_clap.cpp`
  `handleEvent`, `process`, and the tempo's readers (`resolveQTimeMs`, the LFO tick, `rack.setTempo`, the voice
  monitor's grid unit); `src/swarm_core.h` (read only: Params default, `setNoteVelocity`, the tempo-grid law);
  `src/fx_rack.h` and `src/delay_core.h` `setTempo`; `libs/clap/include/clap/events.h` (velocity "0..1", tempo
  "in bpm"); `tools/weakening_check.py` (`dsp_guard`); ADR-206 on PR #1028; ROADMAP B455, B446, B284.
- **Alternatives rejected:**
  - *Reset an unusable tempo to 120.* One unusable transport between two good ones would move a playing patch
    to another tempo and back. The existing comment in `process()` already names the fallback as "the last
    known (or default 120)".
  - *A tempo range of our own (say 20..999).* The code defines no upper bound for a tempo, so none was invented;
    1e300 and the largest double were rendered through every tempo reader (grid law, LFO sync, quantise-time
    sync, Delay sync) and stayed finite. The lower bound is not new: `bpm > 1` is the rule four readers already
    apply.
  - *Drop a NOTE_ON whose velocity is not finite* (what the key and expression checks do). The brief asked for
    the rule this site already has for an invalid velocity, and that rule is the release.
  - *A fix inside `swarm_core.h`.* The core is a byte-identical lift guarded by `h2_lift_check`; the check
    belongs at the shell's entry sites.
  - *A new `std::isfinite` at the two sites.* Both reuse `finiteClamp`, so `weakening_check`'s `dsp_guard` count
    does not rise (0 increases).
- **Must-fail proof (mutations, each reverted):** tempo check removed: 21 rows red, 68 green. Velocity check
  removed: 6 rows red, 83 green. Restored: 89 green. Rows that do not move under a mutation, and why: velocity
  -inf, 0 and -0.5 (the site already released on `<= 0`); "tempo -97 after 97" (the grid law is symmetric in the
  sign of the tempo, so only the row against the 120 default sees it); tempo 1e300 (accepted before and after).
- **Unchanged output for valid input:** `parity_check` (156/156, worst 4.262e-09), `state_check`,
  `statefix_check` and `load_handoff_check` print byte-identical output before and after; the 33 earlier
  `hostile_events_check` rows are byte-identical.
- **Verify:** `./verify fast`, exit 0, at 36f8043 (the three code commits; this trace is the only later
  change), per `.harness/last-verify.json`. A first run at d9b8d01 was red on `test_table_check` alone: the
  longer header had pushed the check's `WIRED:` line past the 40 lines that gate reads; 693781c moves it up. The
  `verify full` rows named in the brief, run one by one on the Release build: `hostile_events_check: GREEN` (89
  rows);
  `load_handoff_check: GREEN (0 failure(s))`; `rtsan_check: GREEN` (plant 1 violation, control 0, full 0
  violation stacks; the schedule delivered 142 tempo events and 1162 note-ons); `tsan_stress_check: GREEN` (three seeds; main-thread WRITE 0 on each; main-thread READ 50, 56
  and 60, the accepted class; both plants reported; the three controls clean).
  `./verify full` as a whole was not run (longer than this dispatch's watchdog).
- **Open questions:**
  1. The 1 BPM floor changes what a tempo in (0, 1] does under the tempo-grid law: it was used as sent, and is
     now ignored, as the other four readers already ignored it. The lead may prefer a different floor.
  2. A NOTE_ON whose velocity is above 1 now reads 1 at the Sub Osc, the note tag and matrix source 14 (the
     voice gain already clamped it).
  3. The sweep of the other host-value entry points went to the lead in the hand-back; nothing else was
     changed here.
