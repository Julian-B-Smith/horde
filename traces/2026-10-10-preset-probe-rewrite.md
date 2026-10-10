# preset-probe-rewrite — preset_probe becomes preset_pitch_check: a new voice's pitch asserted from the voice table, over the factory bank

- **Queue item:** B454 item 10 (ADR-206 item 5, approved by the human 2026-10-10).
- **Why:** `tools/preset_probe.cpp` read a false positive (+36 c against a 30 c limit) from a
  one-lag autocorrelation estimator while the plugin's own voice row showed 329.628 Hz exactly,
  and had no must-read-clean control. The one thing nothing guarded, that after a preset load a
  newly struck voice sounds at its own pitch, is now a deterministic assertion on
  `hypersaw_debug_voices`: every gated voice's `f0` within 1 cent of the equal-tempered pitch of
  its own note, glides waited out on the plugin's own `glideActive` flag (30 s cap, a capped wait
  is red). `f0` is the pre-tune, pre-pitch-envelope, pre-vibrato note pitch (swarm_core.h:1769),
  so intentional pitch modulation never enters it and no voice is skipped; `f0cur` is judged too
  when Gravity is off. Part B (wheel lane stays 0 on a chord) is dropped: `anchor_check` owns it.
  The ungated chord pitch print is dropped. The preset load/remap path stays `morphlayout_check`'s.
- **Evidence consulted:** tools/preset_probe.cpp (old); src/hypersaw_clap.cpp:10876
  (`hypersaw_debug_voices`), :9006 (freq = 12-TET), :216-466 (param ids); src/swarm_core.h:540-630,
  1700-1770 (glide, retarget, f0/f0cur); DECISIONS ADR-159, ADR-206; ROADMAP B454; tools/bank_check.cpp
  (inventory idiom); tools/test_table_check.py, tools/build_flags_check.py, tools/tolerance_registry_check.py,
  tools/sanitize_oracles.sh (runs every parsed oracle with NO arguments, so no argument defaults to
  the factory bank).
- **Measured, 2026-10-10, Release:** 41 factory presets (not 45; bank_check's own floor is 30..50),
  x 5 voice-mode overlays x 5 scenarios, 4264 voice readings, 0 red, worst |f0| error 0.0104 c (that is the
  voice row's 3-decimal print), slowest glide settle 0.54 s, ~15 s wall. Every factory file stores
  voiceMono 0, glide 0, glideMode 0, noteLawLink 1 (follow an off bend law): none glides, none is
  mono. "As authored" therefore cannot exercise a glide or a retarget, so the tool imposes
  mono legato, mono restrike, poly glide (400 ms, from-always) and mono legato glide after each load.
  That overlay set is beyond the brief's literal "if the preset uses them"; it is the only way
  the bank runs those paths at all, and it is one array to drop if the lead disagrees.
- **Controls (bank mode):** C1 clean (defaults: 0 red over 6 readings). C2 a READING-LAYER plant
  (newest voice read as the previous note's pitch): 2 red from 8 injected readings; proves the
  comparator and selection, nothing about the plugin. C3 a REAL plant through parameters only: a
  1500 ms constant-time note law, 60 held, 64 struck, read at once: the plugin's own voice table
  has the new voice's f0 at 261.860 Hz, 1.6 c from the old note and 398.4 c from its own, red; the same
  plant read after the glide landed (1.90 s) is clean. NOT built: the pre-ADR-159 layout fault itself
  (it needs the pre-fix shell; no `src/` edit was allowed), so no control proves this gate would have
  caught the 2026-09-11 mechanism, only its symptom's shape.
- **Alternatives rejected:** keeping the name `preset_probe` (fewer approvals: `rtsafety_probe` is a
  wired probe) rejected because `test_table_check` cross-checks the WIRED/UNWIRED claim only on
  `tools/*_check.*`, so a probe-named gate could be un-wired silently. Asserting on `f0cur` alone
  (gravity offsets it by design). Skipping presets with pitch modulation (f0 already excludes it).
- **Sanitizers:** UBSan build (HYPERSAW_SANITIZE=undefined, RelWithDebInfo): 0 runtime errors, exit 0.
  `leaks --atExit`: 0 leaks. ASan is NOT run: on this Mac every ASan binary dies at init
  (`sanitizer_malloc_mac.inc:189 CHECK failed`, `anchor_check` too), an environment fault, not this tool's.
- **Verify:** `./verify fast` exit 1 at c44a37b (tree dirty), `.harness/last-verify.json`
  `{"target":"fast","exit":1,...}`, red on exactly two gates, both human-gated edits outside this brief's
  scope: (1) `tolerance_registry_check`: `tools/preset_probe.cpp::inline-float-compare` is gone, and
  `tools/preset_pitch_check.cpp::kPitchTolCents` (1.0) is unregistered; both need `docs/armor/tolerances.json`.
  (2) `build_flags_check`: `--approve preset_probe` is recorded (the one use the brief allowed) but
  `dynamic_loops`' executables_covered still lists `preset_probe`, so `--append` for the new target is
  refused until `--approve dynamic_loops <ref>`. NOT pushed, no PR, per the push gate.
- **Open questions:** (a) the three human-gated approvals above; (b) `src/hypersaw_debug.h` (lines 71, 98,
  231, 234, 237, 263-264) and `src/hypersaw_clap.cpp:10886` still name `preset_probe` as an owner; they
  are comments in `src/`, out of scope here; (c) the overlay set, as above.
