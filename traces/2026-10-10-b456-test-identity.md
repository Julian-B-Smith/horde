# b456-test-identity — the same sources build as a second plugin that installs beside the real one

- **Queue item:** B456 (the human, 2026-10-10: "My current plugin is the legacy version, which I would like to
  keep for a number of existing projects. Could we build a dummy plugin for debugging?").
- **Why:** a host finds a plugin by identity (CLAP id, the VST3 class id derived from it, the AU triple), so a
  build of unmerged code under the real identity replaces the plugin real projects load. `HORDE_TEST_IDENTITY`
  (CMake option, default OFF) gives the same sources a second identity. The identity is spelled once:
  `src/plugin_identity.h` owns the CLAP id, display name and store folder for both, one compile definition
  selects, and the descriptor (`src/hypersaw_clap.cpp`) and `presetRootFor` (`src/gui/preset_store.h`) read it.
  `CMakeLists.txt`'s identity block feeds the output name, bundle identifier and AU subtype.
- **Identities (read from built bundles, not from the sources):**
  real `com.lifted-truck.hypersaw` / `horde` / `aumu Hsaw LfTk` / VST3 `F730E1CE68C657DB87D2452E272BD28F` /
  `horde.*` / store `HYPERSAW`; test `com.lifted-truck.hypersaw.test` / `horde TEST` / `aumu HsTs LfTk` / VST3
  `B97E86E35E255030BEA8313DFA385F26` / `horde-test.*` / store `HYPERSAW-TEST`.
- **Evidence consulted:** `CMakeLists.txt` (`make_clapfirst_plugins` and its frozen-id comments);
  `src/hypersaw_clap.cpp` (descriptor; `writeForensics`; `hypersaw_entry_get_factory`, which returns the plugin
  factory only); `src/gui/preset_store.h`; `src/gui/hypersaw_gui_common.h:227-294` (every store bind derives from
  `presetRoot()`; the factory bank is embedded and installed there at bridge install);
  `libs/clap-wrapper/src/wrapasvst3_entry.cpp:266-276` (class id = `create_sha1_guid_from_name` over the
  descriptor id when the plugin gives no `componentId`); `libs/clap-wrapper/cmake/wrap_auv2.cmake` and
  `src/detail/auv2/build-helper/build-helper.cpp` (this project's AU is built in explicit mode: its browser
  name is the output name, and its Cocoa class names derive from name + codes, so the two AUs' classes differ);
  `libs/choc` (`createDelegateClass` makes unique Objective-C class names, so two loaded copies do not collide);
  `install` (signs the real bundles at install time); `tools/build_flags_check.py`,
  `tools/param_id_lock_check.py`, `tools/weakening_check.py`, `tools/test_table_check.py`,
  `tools/gui_webview_check.mm` (the pattern the store check follows).
- **Default unchanged:** the default bundles were built at origin/main (c44a37b) before any edit and again
  after; a dump of their Info.plists, CLAP descriptor, VST3 class id and file lists is identical before and
  after (empty `diff`). `param_id_lock_check`, `presetstore_check`, `bank_check`, `state_check`, parity:
  untouched. `build_flags_check`: one pin entry for the new `identity_store_check` target, by `--append`; no
  `--approve` was needed or run. `docs/playbooks/integrating-a-source.md`: one citation followed the id to the
  header (`playbook_check` was red on the vanished anchor).
- **Checks added (ADR-180 §1):** `tools/test_identity_check.py` in `fast` (frozen table for the real identity;
  the two share no CLAP id, name, store folder, bundle identifier, output name, AU subtype; option defaults OFF;
  nothing the harness runs turns it on; no hard-coded copy in src/ or h2/; eighteen must-fail controls, each
  asserted by tag) and, in `full` on macOS, its `--built <dir> real` (the built bundles, including the frozen
  VST3 class id) and `tools/identity_store_check.mm real` (a run under a scratch HOME).
- **Store isolation, by a run** (`build-test-identity/identity_store_check test`, 11 rows, GREEN): the test
  build installs its 45 factory presets, saves and loads a preset and writes its dump under
  `LiftedTruck/HYPERSAW-TEST`; a planted `HYPERSAW` folder keeps its 3 files, bytes and modification times, its
  preset never lists, loads or deletes, and its factory stamp does not stop the install. "Not read" is shown by
  those behaviours, not by tracing system calls. Detector control: the same binary judged as `real` reads red
  on 10 of 11 rows (the planted preset loads, is deleted, `"factory":0`).
- **Alternatives rejected:** a fourth parameter on `presetRootFor` for the folder (a public-interface change;
  the constant read inside keeps the one path); reading the expected folder from the identity header inside the
  store check (a detector that learns its answer from the code under test); making the oracles in `tools/`
  read the header's id (they are witnesses that do not, and a drifted header reddens them in `full`); leaving
  the test AU named `horde-test` (see open question 1); a post-build re-sign for the real build too (it changes
  the default build, which this item must not).
- **Not done, by the brief:** nothing was installed; `auval` was not run (it only sees installed components).
  The lead's command after an install: `auval -v aumu HsTs LfTk`.
- **Verify:** `./verify fast`, exit 0, git ecf7b96 (`.harness/last-verify.json`). `./verify full`, default
  configuration: exit 0, git ecf7b96 (`{"target":"full","exit":0,"git":"ecf7b96","ts":"2026-10-10T17:15:26Z"}`);
  inside it, `test_identity_check --built ... real` and `identity_store_check real` both GREEN. This trace was
  written after that commit, so the verified hash is the change set without the trace.
- **Open questions:** (1) clap-wrapper writes the AU's browser name from the output file name, so a post-build
  step (test identity only, macOS) rewrites that plist key to `Mindlathe: horde TEST` and then seals the three
  bundles; the real build has neither step. If one name in all three formats is not worth a build step, drop
  the step and the test AU shows `horde-test`. (2) The real bundles do not pass `codesign --verify --deep
  --strict` as built (`code has no resources but signature indicates they must be present`); `./install` signs
  them. Pre-existing, reported by `--built real`, not changed. (3) B456's `auval` clause is open until the lead
  installs. (4) The Windows build of the test identity is compiled by nothing: CI configures the default only.
