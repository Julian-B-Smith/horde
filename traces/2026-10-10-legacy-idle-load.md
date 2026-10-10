# legacy-idle-load — a load made while not processing is whole when it returns

- **Queue item:** B455 (repo audit 2026-10-10: H4, M5). ADR-186 §1 admits state-integrity fixes to the legacy
  shell; ADR-206 asked that H4 be measured first. The measuring rows are this branch's first commit.
- **Why:** the measuring rows pin how a load made while not processing hands its values over and marks its
  writes. Each change below reuses a mechanism the shell already had (ADR-200's load bracket, queue, stage
  gate and deferred events), and every row now reads green.
- **What changed (`src/hypersaw_clap.cpp`):**
  - **A direct load owns the queue at both ends** (`LoadScope`, `drainQueueOwned`). At its start it applies
    what was queued before it, in order, after claiming the stage. At its end it applies the values it queued
    itself, under the load bracket, in the order a flush applied them. Gesture brackets stay queued for the
    next drain that carries out-events. When the audio side deferred events during the sequence
    (`deferWaiting`), the load's values stay queued behind them, in the order `replayDeferred` fixes.
  - **The host is told by rescan.** After a load that applied its own values, `applyStateJson` calls
    `clap_host_params.rescan(CLAP_PARAM_RESCAN_VALUES)` last, on the main thread
    (`libs/clap/include/clap/ext/params.h`, "I. Loading a preset"). A load whose values stayed queued owes
    that call: once the audio side has replayed and drained it asks for a main-thread callback
    (`askForOwedRescan`), and `plug_on_main_thread` makes the call. A host load is the host's own.
  - **Load provenance on the direct lane** (`applyLoadValue`). One helper is `applyParam` under the load
    bracket; the init defaults, the parameter lines, the engine-block lines and the routing cells use it.
  - **A save names the patch whose values it writes** (`savedPresetName`, read by `state_save`). While a
    queued load waits for the next block, the save carries the outgoing name with the outgoing values. Main
    thread only; released when the stage gate shows the load's batch adopted.
  - **A load's zero for the pitch-route depth takes the route out** (`applyParam`, id 161), so the patch's
    value creates it afresh and the mod-route table after a load matches a fresh instance. A host's zero
    keeps the route. Its own commit, with its rows: it changes what the table holds after a load, on both
    lanes.
  - `hypersaw_debug_undo` gains the op `setspec` (the editor's write of specimen), for one row.
- **Rows (`tools/load_handoff_check.cpp`):** 40 before the measuring rows, 71 with them, 84 now, all PASS.
  Added here: I-TELL, I-TELL-HOST, I-TELL-QUEUED, I-TELL-LATER, I-BRACKET (three shapes), I-BRACKET-CTL,
  I-QWIN-NAME, I-QWIN-CLEAR, IS-ROUTE-IDLE, IS-ROUTE-PROC, IS-ROUTE-LOAD.
- **Evidence consulted:** `src/hypersaw_clap.cpp` (`LoadScope`, `DirectScope`, `enqueueParam`, `drainQueue`,
  `deferEvents`, `replayDeferred`, `initState`, `applyStateJson`, `state_load`, `state_save`, `StageGate`,
  `applyParam`); `libs/clap/include/clap/ext/params.h`, `clap/host.h` (`request_callback`) and the wrappers'
  `param_rescan` and `request_callback` (`libs/clap-wrapper/src/wrapasvst3.cpp`, `wrapasauv2.cpp`), read
  only; `src/mod_core.h`; DECISIONS ADR-186, ADR-200; `tools/weakening_check.py`,
  `tools/tolerance_registry_check.py`.
- **Alternatives rejected:**
  - *Apply each value where the preset door reads it.* That applies parameters before the morph chunk and the
    engine revision, a different order from the drain that defines the idle render; and it removes the
    `request_flush` seam rows E-* and F-* stand on. Draining the load's own queue keeps the order exactly.
  - *Discard the queue at a direct load's start.* An editor write to something the load does not write would
    be lost. It is drained.
  - *Have the main thread replay the deferred events.* Only the audio side may touch that buffer.
  - *Leave notify-only entries in the queue for the host.* A second mechanism, and entries that go stale
    under a later load. The contract already names rescan for a plugin-side load.
  - *Carry the name inside the staged batch.* The audio thread would then write a string the main thread
    reads. The main thread holds the outgoing name instead and reads one atomic.
  - *Relax IS-PRE-PRE's mod-route comparison.* It is a gate. The shell change that makes it true as written
    is a separate commit, so it can be judged alone.
- **Must-fail proof (mutations, each reverted):**
  - end drain removed: I-SAVE red. Start drain removed: IS-EDIT red. Both removed: I-SAVE, I-SUPERSEDE,
    IS-EDIT red.
  - deferred-events condition removed: E-ORDER, I-TELL-QUEUED red. Rescan removed: I-TELL red. Owed rescan
    not recorded: I-TELL-LATER red. Bracket re-queue removed: I-BRACKET red.
  - routing cells without the load bracket: 8 rows red (HOOK-ENGINE-IDLE, HOOK-ENGINE-ARM, HE-RUN,
    HE-RUN-ARM, HE-NAT-IDLE, HE-NAT-ARM-I, HE-AUDIO-STILL, HE-AUDIO). Engine-block lines without it:
    HOOK-ENGINE-ARM, HE-NAT-ARM-I red.
  - pitch route: the condition without `loadingState`: IS-ROUTE-IDLE, IS-ROUTE-PROC red. The condition off:
    IS-PRE-PRE, IS-ROUTE-LOAD red.
  - `state_save` reading `presetName`: I-SAVE-QWIN, I-QWIN-NAME red.
- **Unchanged output for valid input:** `render_neutral_digest` before and after: 48 of 48 digests identical
  (3 state fixtures, each equal to its golden; 45 factory presets; idle load, offline render). The stdout of
  `state_check`, `undo_check`, `statefix_check`, `bank_check`, `morphlayout_check`, `routing_check`,
  `tseed_check`, `parity_check` and `trajectory_check` is byte-identical. None of the 48 inputs sets knob
  161 or holds a route from an earlier patch; IS-PRE-PRE and the IS-ROUTE rows are what read that change.
- **Verify:** `./verify fast`, exit 0, at `8da0987` (this trace is the only later change), per
  `.harness/last-verify.json`. `./verify full` on the branch head, with `rtsan_check` and `tsan_stress_check`
  in it, is reported in the hand-back. One by one at that commit, Release build: `load_handoff_check: GREEN (0
  failure(s))` (84 rows); `state_check`, `statefix_check`, `routing_check`, `tseed_check`,
  `morphlayout_check`, `bank_check` and `undo_check` green; `parity_check` 156/156. `weakening_check`: 0
  increases. `tolerance_registry_check`: 0 changed.
- **Open questions:**
  1. The pitch-route commit is a behaviour change made to keep IS-PRE-PRE true as written; it is the last
     commit before this trace so it can be judged, or dropped, alone.
  2. Rescan replaces out-events as the notice for a load made while not processing. Read from the wrappers,
     not run in a host; the hands-on check in a host is the human's.
  3. `request_callback` is called from the audio side (it is `[thread-safe]`); the wrappers implement it as
     one flag. No host was driven.
