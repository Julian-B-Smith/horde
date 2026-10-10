# libs/patches — changes we carry on vendored code

The submodules under `libs/` stay at their upstream commits. Where we need a
change upstream does not have yet, it lives here as a patch file, named
`<submodule>-<what>.patch`. The patches are applied in order at configure time
to a copy in the build tree, never to the submodule's working tree. A patch that
no longer applies stops the configure.

Patching vendored code is a human gate. Each patch here was approved by the
human for that change alone.

**Configuring now requires git** on the PATH: the patches are applied with
`git apply`, and the configure stops without it. No repository is needed for
that step, so a source export configures as long as git is installed.

## How it works

- `apply_patch.cmake` copies a vendored tree into the build tree, normalises the
  patched files to LF (Windows checkouts may be CRLF), and runs `git apply` once
  per patch, in list order. It stops with a `FATAL_ERROR` if a patch does not
  apply. It also stops if `git apply` exits 0 but leaves a target unchanged.
- `CMakeLists.txt` holds the list, `HS_CHOC_PATCHES`, runs the script with it at
  configure time, and puts the copy on the include path. It also re-runs the
  configure when a patch, the script, any file in the vendored tree, or the
  submodule's checked-out commit changes. That list is the only one: the SBOM
  (`tools/gen_sbom.py`) reads it too.
- Our code includes the patched headers as `<choc/...>`, never by a relative
  path into `libs/`. A relative include would compile the unpatched header.
- `tools/choc_patch_check.py` (`./verify fast`) checks each patch against the
  submodule pin. It runs the same script on the list and on each patch alone,
  checks the wiring, and runs its own must-fail controls.
- The release SBOM marks the patched component: `tools/gen_sbom.py` lists each
  patch in the component's CycloneDX `pedigree.patches`, with a `horde:patch`
  property carrying its SHA-256. `tools/release_path_check.py` (`./verify fast`)
  is red if a patch file here is missing from it.
- Each patch is made against the pristine pin and changes no line another
  patch's hunks rely on, so each applies alone as well as in any order, and any
  one can be dropped by itself.

## Patches

| Patch | Upstream | Purpose | Upstream status | Remove when |
|---|---|---|---|---|
| `choc-webview2-navigation.patch` | choc `a08bfd8` | Adds `WebView::Options::allowNavigation (type, uri)`, consulted by the WebView2 backend for page navigations, frame navigations, new-window requests and page messages. The Windows GUI uses it to admit only its embedded page (B446, ADR-194 D-S5), through `src/gui/embedded_page_policy.h`. The patch only adds lines; with the option unset, behaviour is upstream's. Runtime-unverified on Windows as of 2026-10-08; see `traces/2026-10-08-b446-choc-win-nav.md`. | Requested upstream as Tracktion/choc#111. | The pinned choc provides an equivalent hook. Port `src/gui/hypersaw_gui_win.cpp` to it, then delete the patch, its entry in `HS_CHOC_PATCHES`, and this row. |
| `choc-webview2-permissions.patch` | choc `a08bfd8` | Adds `WebView::Options::allowPermission (kind)`, consulted by the WebView2 backend's `PermissionRequested` handler: true grants, false refuses. The Windows GUI refuses every kind, so page script cannot read the clipboard; PASTE reads it natively (`hzPasteState`), as on macOS (B446, ADR-196), through `src/gui/embedded_page_policy.h`. The patch only adds lines; with the option unset, behaviour is upstream's (clipboard reads granted). Applied after the navigation patch. Runtime-unverified on Windows as of 2026-10-08; see `traces/2026-10-08-b446-choc-win-clipboard.md`. | Not yet proposed. A follow-up comment on #111 is drafted for the human's review. | The pinned choc provides an equivalent hook. Port `src/gui/hypersaw_gui_win.cpp` to it, then delete the patch, its entry in `HS_CHOC_PATCHES`, and this row. |
| `choc-webview2-external-drop.patch` | choc `a08bfd8` | Adds `WebView::Options::allowExternalDrop` (a bool, default true) and declares `ICoreWebView2Controller3` and `ICoreWebView2Controller4`, which choc lacks. When the option is false the WebView2 backend queries the controller for Controller4 and calls `put_AllowExternalDrop(FALSE)`. The Windows GUI passes false, so the view takes no content dragged in from outside it (B448 C3, ADR-205 item 3). It is a second layer: both pages refuse drops in script (`tools/gui_sink_check.py`) and the navigation rule already stops a drop from navigating the view. The patch only adds lines; with the option at its default nothing new is called. On a WebView2 runtime without Controller4 the switch is not set and the view is created as before. The interface ids and method order follow Microsoft's WebView2 SDK header (the patch's INTERFACES paragraph); `tools/choc_patch_check.py` holds them. Applied after the permission patch. Runtime-unverified on Windows as of 2026-10-10; see `traces/2026-10-10-choc-patch-external-drop.md`. | Not yet proposed. A feature request is drafted for the human's review. | The pinned choc provides an equivalent option. Port `src/gui/hypersaw_gui_win.cpp` to it, then delete the patch, its entry in `HS_CHOC_PATCHES`, and this row. |

## Bumping a patched submodule

`choc_patch_check` turns red on any change to the `libs/choc` pin until each
patch's `Base:` line names the new commit. That is deliberate: every bump
re-asks whether the patch still applies and whether upstream now makes it
unnecessary.

1. Configure. If a patch no longer applies, re-make it against the new pin, or
   remove it if upstream has the hook.
2. Update each `Base:` line and this table, then run `./verify fast`.
