# b448-b2-thread-handoff — the legacy shell's main-thread/audio-thread handoff: one writer per state, loads decided once and adopted whole, PANIC on the audio thread

- **Queue item:** B446 Tier C1 / B448 B2 (the handoff plan ratified 2026-10-09, and the rulings of the
  same day on the gate rule, the remaining load writes and the routing_check rig). Local branch
  `thread-handoff`, from `armor-tsan-stress` merged with main at bc7dead.
- **Why:** state the audio thread owns now has one writer at a time. Each change reuses an existing
  mechanism; none allocates or locks on the audio thread.
  1. **Ownership, decided once** (`Plugin::beginMainDirect`, `DirectScope`): a main-thread write sequence
     (a load, PANIC, an editor verb) raises `mainDirect` and then reads `processing`, both seq_cst; the
     audio-thread entry points read `mainDirect` after raising their own flag (`processing` for
     process(), `audioFlushing` for params_flush and reset, `beginAudioEntry`). Either the sequence
     queues, or the audio side leaves untouched: process() renders a silent block, flush skips its
     drain, reset is deferred. Events that arrive in such a block or flush are copied into a fixed
     buffer and replayed after the next drain (`deferEvents` / `replayDeferred`); what does not fit is
     counted. `DirectScope` and `LoadScope` release ownership and end a load on every path.
  2. **PANIC**: while processing, a request the audio thread performs at block start
     (`panicPerformRequested`: fixed-size forensic capture, then the clear); the file is written on the
     main thread from the snapshot (`panicService`). Not processing, it runs directly and writes the
     file after releasing ownership.
  3. **Editor verbs** (mod routes, mod wheel, morph exempt, capture, corner preset): direct when not
     processing, queue kinds 4-14 while processing, applied by `applyCommand` in `drainQueue`. The
     route add's answer comes from the audio thread's published count plus the adds still queued; the
     exempt toggle's target state travels with its entry; a corner preset stages its corner.
  4. **A queued load is one batch** published with one `qHead` store, its routing cells LOAD values
     (kind 3), its mod routes a clear plus one add per entry. Everything else it writes — the morph
     field, intent tables, LFO streams, ensemble timing, engine revision — is written into a
     preallocated stage and adopted whole by the audio thread at the batch's last entry
     (`morphAdopt`, `StageGate`: generation + phase, compare-exchange supersession). The marker's queue
     slot is reserved while a batch is open. A direct load claims both stages first.
  5. The spectrum ring's samples are relaxed atomics.
- **Render neutrality, MEASURED** (`tools/render_neutral_digest.cpp`, FNV-1a per render): 3 state
  fixtures (each equal to its golden .f32) and 45 factory presets loaded idle and rendered offline,
  plus the verbatim stdout of state_check, undo_check, presetstore_check, anchor_check, penv_check,
  twocluster_check, trajectory_check, statefix_check, bank_check, morphlayout_check, parity_check
  and the 543 digest rows of h2_engine_selfdigest_check. Baseline at e5ab96d (before any change),
  after at 3f68421: identical.
- **Gates:**
  - `tools/load_handoff_check.cpp` (in `verify full`; it links the shell, and `fast` has no build tree):
    a stop inside a load does not split it; a processing load writes no corner; a queued load equals
    an idle load (mod routes, whole saved state, and with intent/lfo/ens/engine-revision lines); two
    loads in one block leave the second whole; an overflowing load still lands its morph field and
    counts what it refused; a block owned by a direct load is silent and its note and parameter
    events are replayed, none lost; a flush during a direct load does not drain, and the next one
    does. Each row has a control; mutation proofs (reverted): no marker reservation, no replay, a flush
    that ignores ownership, and no staged extras each turn the matching rows red.
  - `tools/tsan_stress_check.py` (in `verify full`): fails on any report whose main-thread access is
    a write, by ThreadSanitizer's own access type; main-thread reads are counted as accepted. A planted
    main-thread write must be reported and filed as a write, a planted read as a read, and the
    audio-only control must be clean. The harness loads a chunk with intent, lfo and ens lines with the
    intent bus on. Main-thread writes: 0 on seeds 1, 2 and 3.
  - `tools/rtsan_check.py`: GREEN; its probe presses PANIC once per scene with HOME redirected.
  - `tools/routing_check.cpp`: a flush after each load in its processing rig (ruling of 2026-10-09);
    with the routing enqueue removed its two round-trip rows read red (mutation proof, reverted).
  - `tools/tseed_check.cpp` E3: the same flush after its queued load (same ruling); with the staged
    ensemble adoption removed, E3 reads red (mutation proof, reverted).
- **Rig doors stand in for the next block start** (test surface only; the editor never calls them):
  the debug and test exports that load, edit routes or the morph field, or press PANIC drain the
  whole queue (or perform the request) while processing.
- **Evidence consulted:** the private plan, two critic reports and the TSan findings (local only);
  `src/hypersaw_clap.cpp`; `src/mod_core.h`; `src/swarm_core.h`; the tools named above;
  `tools/state_check.cpp`, `tools/morphlayout_check.cpp`, `tools/lfoenv_check.cpp`,
  `tools/trace_check.cpp`, `tools/statefix_common.h`; `verify`; `tools/test_table_check.py`.
- **Alternatives rejected:** corners through the queue (about 4x300 entries beside a measured peak of
  1471/2048); a `processing` read per write; a load returning false on overflow (state_check's B100
  rows load three times while processing without draining); swapping the stage's vectors instead of
  copying (a copy keeps buffer identity fixed for every reader); classifying races by function name.
- **Verify:** `./verify fast` red only on `weakening_check` (`tools/tsan_stress_check.py`
  sanitizer_off 0 -> 2, the `halt_on_error=0` the harness needs, for the human's approval at
  publication). `full`'s body, run past that one line from an untracked copy of `verify`: exit 0
  (load_handoff_check, tseed_check, rtsan_check, tsan_stress_check, routing_check all GREEN).
- **Open questions:** held with the lead.
