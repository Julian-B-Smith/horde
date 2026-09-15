# b129-preset-store — one store-path helper, and the factory bank's install path

- **Queue item:** B129 (ROADMAP) — B99(b) plus the factory-install half of B102.
  Dispatched by the HYPERSAW lead organ 2026-09-15; brief
  `briefs/2026-09-15-b129-preset-store-factory-path.md` on `lead-records-11`.

- **Why:** The disk preset store built its path from `$HOME/Library/...` at four
  separate call sites (`src/gui/hypersaw_gui_common.h`, the list/save/load/delete
  binds), and the forensic dump built a fifth at `src/hypersaw_clap.cpp:1067`.
  On Windows that directory does not exist, `create_directories` fails, and the
  save bind returns `false` into a UI with nowhere to show it — presets silently
  no-op (B99(b), trace `2026-09-09-windows-first-run.md`). Five copies of one
  fact is why a single-platform assumption could hide in four of them. New
  `src/gui/preset_store.h` is now the only file that spells the path, and it
  also owns the factory-bank install, because WHERE the bank lands is the same
  question as where a preset lands.

  The install is "copy if absent" guarded by a `factory.version` stamp whose
  body is a LEDGER of every relative path the plugin has ever installed. The
  ledger is what makes acceptance (2)'s "a user's ... deletion of a factory file
  is never overwritten" actually true rather than nominally true: without it,
  "install if absent" cannot tell *you threw this away* from *this never
  shipped to you*, and a newer bank resurrects deleted presets. The version
  itself is a sha256 digest of the bank's own contents emitted by
  `tools/embed_file.py --bank`, so adding or editing a preset bumps it with
  nobody remembering to.

- **Evidence consulted:** ROADMAP rows B129 / B130 / B102 / B99 (on
  `origin/lead-records-11`); CLAUDE.md §Domain (protected paths, human gates);
  the four bind bodies and the `dumpForensics` root named above; `CMakeLists.txt`
  lines 46–53 (the existing `embed_file.py` mechanism) and the `preset_check`
  target at 284–288 (the tool-target idiom, `if(NOT MSVC)` around `-O2` — L0003);
  `src/gui/gui2.html` `presetStore` / `refresh()` (2138–2218); LIBRARY L0003
  (MSVC tool-target traps), L0032 (must-read-zero controls, and the stale-object
  corollary), L0036 (pin your refusals), L0046 (exact-line anchors for HTML
  edits), L0047 (`installBridge` runs from `webviewIsReady` — the install hook
  goes there, not after the constructor).

- **Alternatives rejected:**
  - *An `#ifdef`'d `presetRoot()` only.* Rejected: it is testable only by owning
    three machines. `presetRootFor(platform, home, appdata)` takes the platform
    as a VALUE, so one run asserts all three mappings; `presetRoot()` is the
    thin real-environment wrapper over it.
  - *Generated header emits its own POD struct.* Rejected as a second source of
    truth for `FactoryFile`; instead `generated/factory_bank.h` includes
    `gui/preset_store.h`, which is why `${CMAKE_CURRENT_SOURCE_DIR}/src` had to
    join the impl target's include path (a quoted include from a generated file
    has no `src/` to be relative to).
  - *A separate `tools/embed_bank.py`.* Rejected — the brief sanctions a
    multi-file mode on the existing script, and one script beats two.
  - *Stamp = version string only.* Rejected; see the ledger reasoning above.
  - *Refusing DELETE of a factory file as well as SAVE.* Rejected: acceptance
    (3) names SAVE only, and acceptance (2) treats a user's deletion as a
    legitimate act the installer must respect. DELETE is allowed and the ledger
    remembers it.

- **Calibration (L0032/L0033) — the plants, and what each one broke:** the check
  cannot be trusted until something can make it fail, so six regressions were
  planted in `preset_store.h` one at a time, each with the test object deleted
  first (a stale `.o` makes a plant "fire" exactly as predicted). All six fired,
  each on the assertion that should own it, and the restored tree is green:

  | plant | result |
  |---|---|
  | `installFactoryBank` ignores an existing file | 9 failures — only-new-file, edit-survival, deletion-survival |
  | Windows root falls back to `HOME` | 1 failure — "HOME must not stand in" |
  | `saveAllowed` accepts any non-empty kind | 2 failures — the factory-SAVE refusal |
  | same-version shortcut removed | 1 failure — "second open does not even rewrite the stamp" |
  | deletion ledger ignored | 7 failures — deletion survival, `listFactory` |
  | `sanitiseName` passes every character | 8 failures — every path-escape refusal |

  The embedded-bank leg (case 8) reads 0 while B130 is in flight, which alone
  cannot fail, so it was calibrated against a non-empty corpus: two temporary
  `.json` files under `docs/presets/factory/` made `kFactoryBank_count` read 2
  and the digest move `e3b0c442…` → `ee767232…`; the files were then removed and
  the header regenerated to 0. Recorded rather than left implied, because the
  green run you see has an inert case in it.

- **Coverage boundary (recorded, not closed):** the gui2 factory `<optgroup>`
  and the SAVE refusal have no automated oracle — they need a GUI open against a
  non-empty bank, which does not exist until B130 lands. What IS checked: the
  single `<script>` block parses (`node --check`), and the tier-prefix logic
  (`selTier`, the optgroup HTML, the `factorylike`-is-not-factory case) was
  calibrated out-of-tree. The C++ half of the same refusal — `saveAllowed`
  rejecting `"factory"` — IS pinned, in `presetstore_check`.

- **Verify:** `./verify fast` exit 0 (`.harness/last-verify.json`); `./verify
  full` exit 0. `presetstore_check`: PASS, 55 checks, 0 failures. Two leak-gate
  hits on the way, both mine and both in `tools/presetstore_check.cpp` — the
  fixture paths, and then the COMMENT explaining the fixture paths. The gate
  greps tracked files for machine-absolute paths and cannot tell a fixture from
  an identity leak; it is right not to try, so the fixtures are now obviously
  synthetic (`/stand-in-home`, `Q:/stand-in-roaming`).

- **Open questions:**
  1. **The dump root MOVED on macOS**, from `~/Library/Logs/HYPERSAW` to
     `<root>/logs`, because acceptance (1) says the note-trace dump root derives
     from `presetRoot()`. Any instruction that tells a human where panic dumps
     live is now stale. Not searched for — out of scope.
  2. **B130 ships `docs/presets/factory/corners/*.json`** (its acceptance (3)).
     This installer treats `corners` as just another category and lands them at
     `<root>/presets/factory/corners/`, NOT at `<root>/corners/` where the
     corner dropdown reads. Whether shipped corner presets should appear in that
     dropdown is unresolved and belongs to whoever closes B130.
  3. **`CONFIGURE_DEPENDS` did not re-glob on a targeted build.** Deleting the
     two calibration files and building only `presetstore_check` kept the stale
     2-file header; an explicit re-configure fixed it. Building `all` is the
     normal path, so this is a note, not a defect — but a bank edit followed by
     a single-target build can test yesterday's bank.
  4. `src/hypersaw_clap.cpp` now includes `gui/preset_store.h`. That header is
     choc-free and webview-free by construction, so the ADR-019 seam ("the shell
     sees nothing but `hypersaw_gui.h`") is intact in substance; a lead may still
     want the file moved out of `src/gui/` — the brief put it there.
