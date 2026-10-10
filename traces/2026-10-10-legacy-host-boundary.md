# legacy-host-boundary — event size, sample rate, output buffers and MIDI bytes are checked where they enter

- **Queue item:** B455 (the sweep of host-value entry points that followed PR #1032). ADR-186 §1 admits
  boundary validation of host input to the legacy shell. This branch sits on `origin/host-value-guards`.
- **Why:** four host values entered the shell without the check the others have. Each is now checked at the
  one place it enters, reusing what the shell already had (`minEventSize`, `deferEvents`, `finiteClamp`, the
  refusal `process()` already gave an oversized block).
- **What changed (`src/hypersaw_clap.cpp`, `src/input_guards.h`):**
  - **Event size.** `handleEvent` refuses an absent event, or one smaller than its type's struct
    (`minEventSize`, the rule `deferEvents` already had), and counts it in `deferDropped`. It is the one
    point a live block, a flush and the deferred replay share. An absent event list is taken as empty.
  - **Sample rate.** `plug_activate` returns false unless `hostSampleRateUsable(sr)`: finite and from 8 000
    to 768 000 Hz. The two constants are structural bounds, not the certified range, which stays 44.1 to
    192 kHz (`docs/ROBUSTNESS.md`). A refused activation, for its rate or its block size, is remembered
    (`activationRefused`): `process()` writes silence and touches nothing until an activation is taken.
  - **Output buffers.** `process()` returns `CLAP_PROCESS_ERROR` when there is no output bus, fewer than
    two channels, or a channel with no 32-bit buffer. A block refused this way, or for its size, hands its
    events to `deferEvents`; they are handled at the head of the next block that renders. The silent block
    of an owned sequence asks for the output list before it reads it (`silenceOutputs`).
  - **Out-events.** `emitNoteEnds` treats a missing list as a refused push: the note end stays pending and
    is sent when a list is there.
  - **MIDI data bytes.** A message read here (CC, channel pressure, pitch bend) whose data byte has the top
    bit set is dropped and counted, the rule a NOTE_ON's velocity follows. Channel pressure's third byte is
    not one of its data bytes and is not asked.
- **Rows (`tools/hostile_events_check.cpp`):** 89 before, 187 now, all PASS: SIZE (10), RATE, OUT and MIDI
  sections, each through the real CLAP entry points. The RATE rows derive every rate from the header's two
  constants. The LATCH row sums the output guard's counter over every run, the new ones included: 0.
- **The upper bound, measured:** by reading, the one buffer sized from the rate is the rack's comb bank
  (`fx_rack.h` `setSampleRate`: 8 lines, 2 channels, rate/20 samples); every delay line is a fixed buffer
  that clamps. By a run (a one-off probe counting `operator new` during `activate`): 15.52 MB at 44.1 kHz,
  16.11 MB at 192 kHz, 16.69 MB at 384 kHz, 17.86 MB at 768 kHz; the largest single request is 2 097 288
  bytes at every rate. The RATE rows render a note, finite, at both bounds.
- **Evidence consulted:** `src/hypersaw_clap.cpp` (`handleEvent`, `minEventSize`, `deferEvents`,
  `replayDeferred`, `emitNoteEnds`, `process`, `plug_activate`); `src/input_guards.h`; `specs/ACCEPTANCE.md`,
  `docs/ROBUSTNESS.md`, and the cores read only (`fx_rack.h`, `delay_core.h`, `time_core.h`, `svf_core.h`,
  `subosc_core.h`); `libs/clap-wrapper/src/wrapasauv2.cpp` (what it does with `activate`'s result) and
  `libs/clap/include/clap/events.h`, read only; `tools/weakening_check.py`,
  `tools/tolerance_registry_check.py`.
- **Alternatives rejected:**
  - *The certified range as the activation range.* A wrapper that does not read `activate`'s result would
    then process an instance that was never rebuilt for the host's rate. The bounds admit every rate that
    activated before and is a rate at all; the provisional numbers are the lead's, pending the human.
  - *Render after a refused activation.* The cores were built for another rate. Silence.
  - *Mask a MIDI data byte to 7 bits.* That turns it into a different, well-formed message.
  - *Drop a refused block's events.* A host sends a note-off once.
  - *A new `std::isfinite` for the rate.* `hostSampleRateUsable` is built on `finiteClamp`;
    `weakening_check` reads 0 increases.
- **Must-fail proof (mutations, each reverted; "red" is a nonzero exit of `hostile_events_check`):**
  - event size check off: 6 SIZE rows red. Event null check off: red.
  - rate check off: red from the first refused-rate row. `process()` ignoring a refused activation: the 9
    refused-then-processed rows red.
  - output check off: red at the OUT rows. A refused block not keeping its events: the NOTE_OFF row red.
  - either `emitNoteEnds` site pushing without asking for a list: red. The silent block not asking for the
    output list: red.
  - MIDI drop off: 9 MIDI rows red.
- **Unchanged output for valid input:** `render_neutral_digest` before and after: 48 of 48 digests identical
  (3 state fixtures, each equal to its golden; 45 factory presets; idle load, offline render). The stdout of
  `state_check`, `undo_check`, `statefix_check`, `bank_check`, `morphlayout_check`, `routing_check`,
  `tseed_check`, `parity_check` and `trajectory_check` is byte-identical.
- **Verify:** `./verify fast`, exit 0, at `b86efdb` (this trace is the only later change), per
  `.harness/last-verify.json`. `./verify full` on the branch head, with `rtsan_check` and `tsan_stress_check`
  in it, is reported in the hand-back. One by one at that commit, Release build: `hostile_events_check: GREEN`
  (187 rows); `load_handoff_check: GREEN (0 failure(s))`; `state_check`, `statefix_check`, `routing_check`,
  `tseed_check`, `morphlayout_check`, `bank_check` and `undo_check` green; `parity_check` 156/156.
  `weakening_check`: 0 increases. `tolerance_registry_check`: 0 changed.
- **Open questions:**
  1. The two rate bounds are provisional (the lead's), pending the human's ruling.
  2. `CLAP_PROCESS_ERROR` is returned for a refused block and for a block after a refused activation; both
     pinned wrappers discard the status.
  3. A block refused for its SIZE now keeps its events too (it shares the return); before, they were not
     handled.
