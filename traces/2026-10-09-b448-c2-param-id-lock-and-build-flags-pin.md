# b448-c2-param-id-lock-and-build-flags-pin — append-only parameter ids and pinned build flags

- **Queue item:** B448 Phase 1 Wave C item C2 (ADR-197, Blind-Spot Armor risk rows 7 "parameter
  ids append-only" and 11 "compiler flags pinned and audited"). Dispatched by the lead on
  2026-10-09, at the human's "start Wave C".
- **Why:** A host stores the parameter id, not the name, in every saved session and automation
  lane, and a build flag such as `-ffast-math` or a changed `-ffp-contract` changes the bits
  the parity goldens were measured under. Neither failure shows up in any audio oracle. Both
  became a committed record plus a check that is red on any difference, so a break needs a
  human's reference and cannot happen by accident.
- **What changed:** `tools/param_id_lock.json` (397 ids, 1..22002, from the real plugin),
  `tools/param_id_lock_check.py`, `tools/param_id_dump.cpp` (the runtime enumeration; a 3-line
  CMake target next to `registry_dump`), `tools/build_flags_pin.json` (108 targets, five
  classes), `tools/build_flags_check.py`; `verify` gets two commented rows in `fast` right
  after `h2_rules_check` and one pair of rows in `full` right after the build.
- **Evidence consulted:** `src/hypersaw_clap.cpp` (kParams from line 178, kGlobalIds, the
  routing block 980-1100, kEngineBlocks, `params_get_info` at 9957); `src/routing_core.h:88`
  (`edgeForward`); `src/fx_rack.h:74` (`kRackSlots`); `tools/registry_dump.cpp` and
  `registry_decl.py` (the only existing enumerators; `registry_dump` aborts today, 371 saved
  keys against 397 params, so it could not be reused); `tools/h2_rules_check.py` rule 2 (the
  only existing flag rule: contraction-off on h2 targets); `tools/paramclass_check.cpp` and
  `statefix_common.h` (the stub host reused by the dump); `CMakeLists.txt` in full;
  `tools/sanitize_oracles.sh`, `rtsan_check.py`, `tsan_stress_check.py`, `.github/workflows/`
  (read only); the emitted `flags.make` of a Release tree; ROADMAP B308 (the h2 lockfile is
  planned, not built) and B448.
- **Alternatives rejected:** (1) Reusing `registry_dump`: it is broken and needs a second
  input on stdin. (2) A configure step in `fast` to read the flags: needs a network fetch of
  the VST3 SDK; a static read of `CMakeLists.txt` costs nothing, and `full` reads the emitted
  `flags.make` to prove the pin is what CMake emitted. (3) A static id extractor alone: the
  routing and engine blocks are built at load time, so `full` runs the real plugin and demands
  field-for-field equality with the extractor. (4) Pinning defaults in the id lock: a default
  change is its own contract (ADR-197: an ADR plus a migration). (5) Strict-on-every-new-target: first built that way, then relaxed on the lead's ruling to the
  id lock's shape (a new target whose flags equal a pinned member of its class is recorded with
  `--append`; anything else needs `--approve <target> <ref>`).
- **Verify:** `./verify fast` exit 0 at `d571efc` (`.harness/last-verify.json`:
  `{"target":"fast","exit":0,"git":"d571efc"}`). The `full` rows were run by hand against a
  fresh Release tree: `param_id_lock_check.py --runtime build-release/param_id_dump` GREEN
  (real plugin equals the static extraction and the lock) and `build_flags_check.py --built
  build-release` GREEN. `./verify full` as a whole was NOT run (human-paced).
- **Open questions:** (a) none on the new-target rule (resolved, see 5 above). (b) `registry_dump`
  is stale (FOUNDATIONS' emitter) and nothing runs it. (c) The h2 section of the id lock is a
  stub until the generated parameter table (B308 H4) exists. (d) Findings that are facts about
  the tree, not changes: no explicit `-O` on the legacy plugin (a bare `cmake` without
  `-DCMAKE_BUILD_TYPE=Release` builds it unoptimised); `CMAKE_OSX_ARCHITECTURES` is unset though
  a comment says "Apple Silicon + Intel", so only the native arch is built; C++ is
  `-std=gnu++20` (extensions on); no `-W`/`-Werror` anywhere in CMake (only the Python-driven
  probes use `-Wall -Wextra -Werror`); MSVC gets no `/fp:` or `/O` flag from this repo (CMake's
  defaults apply), only `/STACK`; the sanitizer loop reaches 94 executables and the 12 declared
  after it (`anchor_check` through `bank_check`) are never instrumented.
