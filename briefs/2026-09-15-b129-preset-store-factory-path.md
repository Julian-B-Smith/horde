# Dispatch brief — B129: one preset-store path helper + the factory install path

**Provenance.** HYPERSAW lead organ, 2026-09-15, for a scoped subagent with zero
conversation history. Motivating rows: B99(b) (Windows preset store silently
no-ops — paths built from `$HOME/Library/...`), B102 (the factory bank must
ship in app-support), split into B129 (this) and B130 (content, another agent,
disjoint files). Read ROADMAP rows B99, B129, B130 and CLAUDE.md first.

## Acceptance criteria (verbatim from ROADMAP B129)

> (1) ONE function `presetRoot()` in the GUI common header returns the per-user store root — macOS `$HOME/Library/Application Support/LiftedTruck/HYPERSAW`, Windows `%APPDATA%\LiftedTruck\HYPERSAW`, else `$HOME/.local/share/LiftedTruck/HYPERSAW` — and every `presets`/`corners`/`prefs` path (list, save, load, delete) and the note-trace dump root derive from it; no other site spells the path. (2) FACTORY tier: `docs/presets/factory/**/*.json` is embedded at build time by the existing `tools/embed_file.py` mechanism (one generated header listing name, category, bytes); on the FIRST GUI open per machine the shell copies each factory file into `<root>/presets/factory/<category>/` ONLY if that file is absent — a user's edit or deletion of a factory file is never overwritten; a `factory.version` stamp records which bank was installed so a newer bank adds new files without touching existing ones. (3) The preset list shows factory presets under a "factory" heading, read-only from the plugin's point of view (SAVE into the factory folder is refused with a message; users save copies elsewhere). (4) `preset_check` (existing gate) untouched; a NEW standalone `presetstore_check` proves: the helper resolves per platform (env-driven), first-open install writes every embedded file, second open writes nothing, a modified factory file survives a re-install, a newer `factory.version` adds only new files. (5) No RT-path change; no new ids. B99(b) closes with this; B99(a) (Windows keyboard focus during drag) and the `./verify` build-dir note stay open

## Where things are

- Store paths: `src/gui/hypersaw_gui_common.h` — `hzPresetList` / `hzPresetSave`
  / `hzPresetLoad` / `hzPresetDelete` binds (search `Application Support`);
  each builds the path itself today. The note-trace dump root:
  `src/hypersaw_clap.cpp` (search `Library" / "Logs"`).
- Embedding: `CMakeLists.txt` embeds `gui2.html` through
  `tools/embed_file.py` into `generated/gui_html.h` — extend the same
  mechanism (a glob over `docs/presets/factory/**/*.json` → one generated
  header with an array of `{category, name, bytes, size}`); the folder may be
  EMPTY or absent while B130 is in flight — the build must succeed with zero
  factory files, and the install step must be a no-op then.
- The GUI's preset dropdown: `src/gui/gui2.html`, search `presetList`,
  `presetStore` (list/save/load/delete go through `hzPreset*`; `PRESET_KEY`
  is the browser-storage fallback). A "factory" group in the `<select>` is a
  `<optgroup>`; SAVE with a factory name selected refuses with a note.
- First-open hook: the GUI bind that runs once the page is ready (search
  `hzGetBuild` or the ready callback in `hypersaw_gui.mm` / `_win.cpp`) —
  keep the install on the MAIN thread and off the audio thread; it is file
  I/O.
- Headless check rig: `tools/notefuzz_scaffold.inc` (include `<algorithm>`
  for MSVC); the check can call the helper and the install function directly
  if you expose them in a small header the check includes (`src/gui/preset_store.h`
  is a fine new home for the helper + install, included by the GUI common
  header) — set `HOME`/`APPDATA` via `setenv`/`_putenv` in the check to
  drive platforms, and use a temp dir.

## Files in scope

`src/gui/hypersaw_gui_common.h`, NEW `src/gui/preset_store.h`, `src/gui/gui2.html`
(the optgroup + refusal only), `src/hypersaw_clap.cpp` (ONLY the note-trace
dump root line), `CMakeLists.txt` (embedding + the check target),
`tools/embed_file.py` if it needs a multi-file mode, NEW `tools/presetstore_check.cpp`,
`traces/2026-09-15-b129-preset-store.md`.

**OUT of scope:** `docs/presets/factory/**` content (B130's agent writes it —
you only embed whatever is there, possibly nothing); `ROADMAP.md` /
`DECISIONS.md`; `./verify` and gates; protected paths; any RT path; the
Windows focus issue B99(a); untracked root files. B130's agent touches
`CMakeLists.txt` too (its check target) — REBASE onto main before opening your
PR, keep both hunks.

## Constraints

Branch from `main` (pull first); absolute build paths; `./verify fast` after
each change set, gate every scripted commit on its exit code; `./verify full`
before done; paste oracle output verbatim; no machine identity in tracked
files (the helper's OUTPUT is machine-local and never written to a tracked
file); MSVC in CI (no `__attribute__`, `<filesystem>` is fine).

## Deliverable

Branch `b129-preset-store`, pushed, PR via `gh pr create --base main` with
`presetstore_check` output and the `./verify full` tail pasted from the run.
**Never merge.** Final report: PR URL, check output, verify tail, the
ROADMAP/DECISIONS text you would add.
