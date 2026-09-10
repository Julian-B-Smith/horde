# 2026-09-09 — First native Windows run: build, oracles, GUI, focus

**What changed.** First build and host load on a Windows machine (Windows 11,
VS 2026 / MSVC 14.51, CMake 4.3, Live 12.3 Beta). Four defects, each only
visible off the Mac:
1. `./verify fast` was RED: three Python tools (`gui_reach`, `gen_gui_controls`,
   `presentation_check`) read UTF-8 files through the Windows default cp1252
   codec. Every text read/write in `tools/*.py` now names `encoding="utf-8"`;
   writes also pin LF so a Windows run of a generator can never CRLF a tracked
   file (`depends_graph.h` was CRLF'd by the first run, restored). Two
   `write_text` sites use `Path.open` because the `newline=` kwarg on
   `write_text` is 3.10+ and the Mac CLT Python is 3.9.
2. `trajectory_check` and `waveshape_check` died with STATUS_STACK_OVERFLOW
   (0xC00000FD). `sizeof(SwarmCore)` = 666,776 bytes under MSVC; MSVC's main
   thread has 1 MB where POSIX has 8, and MSVC does not overlap the stack slots
   of non-trivial locals, so trajectory_check's ~25 cores in nested scopes are a
   ~16 MB frame. CMakeLists now links every host-side EXECUTABLE with
   `/STACK:67108864` under MSVC (virtual reserve; costs nothing). The plugin is
   unaffected — its cores live in the heap-allocated Plugin.
3. GUI was a black rectangle in Live. Mechanism, from `choc_WebView.h`: on
   Windows the WebView2 controller is created asynchronously on the message
   loop and `bind()`/`setHTML()`/`addInitScript()` return false — registering
   nothing — until it exists; `makeWebView` bound 36 functions and loaded the
   page synchronously after construction, so all of it was dropped. The bridge
   now installs from `Options::webviewIsReady` (synchronous on macOS, async on
   Windows); each backend passes its platform binding (`hzGrabKeys`) through
   the same hook. The .mm edit is by reasoning only (no ObjC compiler here).
4. Computer-keyboard MIDI died after a click in the GUI. The Mac backend has
   assigned `GuiHost::releaseKeyFocus` since the 2026-08-12 lingering-note fix;
   the Windows backend never did, so the JS release request was a no-op. Now
   records the parent HWND at attach and `SetFocus`es it on request.

**Evidence.** parity_check 156/156 (worst 2.485e-09); state · notefuzz · trace ·
steal · endprobe · kstuck · rtsafety · paramscope · samplerate · routing ·
notchslot · slotcontract · subdiv · mpe · preset all GREEN; force · spectra ·
filter · notch · swarmalator · glide (3.5e-08) · time all GREEN after their
goldens were generated; trajectory_check and waveshape_check GREEN after the
stack fix. Human: horde.vst3 loads in Live 12.3 Beta, plays, GUI renders with
the build stamp, knobs work, keys return after a click.

**Verify.** `./verify fast` exit 0 on Windows with no env override (was exit 1
before the encoding fix). `./verify full` could not be run as written — it is
hard-coded to `build-release` + Unix Makefiles and is a protected path; the
full chain was replayed by hand against `build-win/Release/*.exe` with every
gate GREEN (list above). Not run: pluginval (a download this session was not
authorised to make), Mac build of the .mm edit.

**Residuals (filed as B99).** Keys still die WHILE a control is being dragged
(release fires on pointerup only). Windows preset store: paths are built from
`$HOME/Library/Application Support`, which does not exist on Windows, so
presets silently no-op there; same for the `~/Library/Logs` trace dump. CI's
Windows pluginval step searches for `HYPERSAW.vst3`, which no longer exists
(the bundle is `horde.vst3` since 2026-08-27), so that step validates nothing.
