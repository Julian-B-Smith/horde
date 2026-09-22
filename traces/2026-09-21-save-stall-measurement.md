# save-stall-measurement — B204 measured and diagnosed, nothing fixed

- **Queue item:** B204 (the human force-quit Ableton because saving a set
  containing horde stalled). MEASURE-AND-DIAGNOSE dispatch: the brief forbids a
  fix, because two hypotheses had already been spent guessing at one.
- **Why:** A stall with unsaved work at risk is the most serious class of
  report this project has had, and the lead had ruled out chunk size, disk I/O,
  an unbounded write loop, locks, and a stale wrapper. What remained was a
  hypothesis (B174's dirty test contending for the host's main thread) and a
  path nobody had timed (VST3, which is how Live had horde loaded). Both are
  now numbers.

## What was measured, and how

Two diagnostics, both min-of-n with `steady_clock` and one timed call per
repetition (the minimum, not the mean: it estimates the work with scheduler
noise excluded rather than averaged in; a median prints beside it so a bimodal
cost cannot hide).

- `tools/save_bench.cpp` — the CLAP factory path, default and adversarial patch
  (every parameter off its default through the host's own list, the same
  `disturbAll` `tools/state_check.cpp` uses; four morph corners captured; four
  mod routes).
- `tools/vst3_save_bench.cpp` — the BUILT `horde.vst3`, loaded as a host loads
  it (CFBundle → `bundleEntry` → `GetPluginFactory` → `createInstance`),
  `setupProcessing` + `setActive(true)`, then `IComponent::getState` timed.

| figure | default patch | adversarial patch |
| --- | --- | --- |
| host chunk | 5 251 B | 15 937 B |
| `state_save` (CLAP) | 105.50 µs | 240.92 µs |
| `IComponent::getState` (VST3) | 172.33 µs | 213.17 µs |
| `presetMatches` = `hzPresetDirty` (matching) | 1 089.25 µs | 1 007.08 µs |
| `presetMatches` (first key differs) | 0.38 µs | 0.67 µs |
| `cornerMatches` ×4 = `hzMorphCornerDirty` | 14.83 µs | 14.25 µs |
| poll tick, dirty tests ON | 1 303.08 µs | 1 274.75 µs |
| poll tick, dirty tests OFF (**control**) | 197.83 µs | 251.29 µs |
| **marginal cost of the dirty tests** | **1 105 µs/tick = 0.22 % of a core at 2 Hz** | 1 023 µs/tick = 0.21 % |

`presetMatches` complexity, measured rather than asserted: 368 `find()`s over
the document, so O(keys × document) by construction — but the constant is in
the per-key needle build and search (1 026 µs of the 1 089), not `atof`
(6.6 µs for all 368) and not the scan (27× the document length only doubles the
time, because the pad has no `"` for `find`'s first-character search to stop
on). Session length reaches nothing: the undo ring is a fixed 200 slots
(`src/undo_tree.h:46`), the chunk never contains it, and `state_save` is
135.54 µs empty and 135.17 µs full.

## Alternatives rejected

- **Timing the VST3 path by substituting the CLAP number.** Refused: the
  difference between the paths is the thing Live's log points at. A host-side
  loader was written instead, and it says the wrapper adds nothing
  (`libs/clap-wrapper/src/wrapasvst3.cpp:276` forwards to the same
  `clap_plugin_state.save` through a stream adapter).
- **Reporting a state-restore defect.** The first round-trip arm lost the SUB
  engine block (`sub.wave=4` → `3`, 19 of 20 keys) on BOTH paths, which read
  like a serious bug. An activation control killed it: activated — the only rig
  a host presents — the round trip is byte-identical. The claim was withdrawn
  before it was made. (A never-activated instance is not a lighter host, it is
  a different one; the same lesson that made `IComponent::initialize(nullptr)`
  segfault inside the wrapper.)
- **Attaching a debugger/sampler.** Out of bounds by the brief, and unnecessary:
  every figure above came from timing code in the repo, reproducible by anyone.

## Evidence consulted

`src/hypersaw_clap.cpp` (`state_save` 8352, `state_load` 8456, `presetMatches`
5962, `stateJson` 5996, `enqueueParam` 4830 — drop-on-overflow, no spin-wait,
`undoMark` 6362 / `undoService` 6405), `src/undo_tree.h`,
`src/gui/gui2.html` (the 500 ms poll 5941, `syncPresetName` 3043,
`syncCornerNames` 3010, `frameFetch`'s `frameBusy` guard 4785),
`src/gui/hypersaw_gui_common.h:376,577`,
`libs/clap-wrapper/src/wrapasvst3.cpp:271-278`,
`libs/clap-wrapper/src/detail/vst3/state.h`,
`libs/choc/choc/gui/choc_WebView.h:1945` (the bind round trip is asynchronous
end to end — no synchronous main-thread wait anywhere in it).

## Verify

`./verify full` — exit 0, git `2e41773` (`.harness/last-verify.json`).
`state_check` GREEN inside it, which is the standing proof that the round-trip
finding above was the bench's rig and not the shell.

## Open questions

- **The measurement does not support the lead's hypothesis.** 1.1 ms twice a
  second cannot stall a save, and the bridge has no synchronous wait to
  deadlock on. It is an honest non-result, and it was the outcome hoped for,
  which is why it is stated as a refutation rather than a confirmation.
- **The narrowest next test is the human's**, and it is cheap: save the same
  set twice — once with horde's window OPEN, once CLOSED — and once more with
  the CLAP build instead of VST3. Closing the window should make no difference
  if this measurement is right; if it does make the stall disappear, the cause
  is main-thread contention that is NOT this 1.1 ms and the next place to look
  is the host's own webview servicing, not our poll.
- **One unguarded re-entry remains**, reported without a claim attached:
  `frameFetch` has a `frameBusy` in-flight guard (`src/gui/gui2.html:4785`) and
  `syncPresetName` / `syncCornerNames` have none, so a slow bridge can overlap
  them. At 1.1 ms a tick that is nowhere near 500 ms, so it is a shape, not a
  cause.
- Both benches are diagnostics, not gates — same standing as `shell_bench` and
  `user_patch_bench`, and named `*_bench` so `test_table_check`'s
  wired-or-explained rule (keyed on `*_check.cpp`) does not claim them.
