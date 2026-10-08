# libs/patches — changes we carry on vendored code

The submodules under `libs/` stay at their upstream commits. Where we need a
change upstream does not have yet, it lives here as a patch file. The patch is
applied at configure time to a copy in the build tree, never to the submodule's
working tree. A patch that no longer applies stops the configure.

Patching vendored code is a human gate. Each patch here was approved by the
human for that change alone.

## How it works

- `apply_patch.cmake` copies a vendored tree into the build tree, normalises the
  patched files to LF (Windows checkouts may be CRLF), and runs `git apply`. It
  stops with a `FATAL_ERROR` if the patch does not apply. It also stops if `git
  apply` exits 0 but leaves a target unchanged.
- `CMakeLists.txt` runs it at configure time and puts the copy on the include
  path. It also re-runs the configure when the patch, the script or the patched
  upstream file changes.
- Our code includes the patched headers as `<choc/...>`, never by a relative
  path into `libs/`. A relative include would compile the unpatched header.
- `tools/choc_patch_check.py` (`./verify fast`) checks the patch against the
  submodule pin. It runs the same script, checks the wiring, and runs its own
  must-fail controls.

## Patches

| Patch | Upstream | Purpose | Upstream status | Remove when |
|---|---|---|---|---|
| `choc-webview2-navigation.patch` | choc `a08bfd8` | Adds `WebView::Options::allowNavigation (type, uri)`, consulted by the WebView2 backend for page navigations, frame navigations, new-window requests and page messages. The Windows GUI uses it to admit only its embedded page (B446, ADR-194 D-S5). The patch only adds lines; with the option unset, behaviour is upstream's. | Not yet proposed. choc takes feature requests as issues, not pull requests (its `CONTRIBUTING.md`). A request is drafted for the human's review. | The pinned choc provides an equivalent hook. Port `src/gui/hypersaw_gui_win.cpp` to it, then delete the patch, its `CMakeLists.txt` block, and this row. |

## Bumping a patched submodule

`choc_patch_check` turns red on any change to the `libs/choc` pin until the
patch's `Base:` line names the new commit. That is deliberate: every bump
re-asks whether the patch still applies and whether upstream now makes it
unnecessary.

1. Configure. If the patch no longer applies, re-make it against the new pin, or
   remove it if upstream has the hook.
2. Update the `Base:` line and this table, then run `./verify fast`.
