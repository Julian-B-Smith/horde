# horde ENGINE PREVIEW

A throwaway plug-in that lets the human play the clean-room engine in Live (B624, ADR-366). It tests sound and CPU, and since B628 it also carries GUI 3 on the engine. It isn't the horde 2 shell (B398), it plays no part in the cutover decision, and it's deleted when that shell exists. Its bridge is planned as that shell's first slice (`docs/proposals/gui-on-engine.md`).

**What it has:** all 83 bank presets, levelled as the rendered previews are, with their blade-envelope routes played. A Voices control (default 4). There are still at most 64 host parameters (47 engine rows, Preset and Voices), so a host panel stays usable.

**Its window is GUI 3's page** (B628), in a web view, talking to the engine through the bridge the plan's section 3 signs: the page sends `hordeNativeSend(JSON)`, and a 30 fps timer pushes `window.hordeNativeSnapshot(snapshot)`. The page can set **every** engine row, not only the 47, but a row outside the 47 isn't a host parameter, so **it can't be automated in the host** in this slice. Until GUI 3's one-file page is built in, the window shows a stand-in page (`standin_page.html`): the presets, Voices, a few rows by key and one phase picture. `HORDE_PREVIEW_PAGE=<file>` builds another page in, such as the one `tools/gui3_inline.py` writes. `HORDE_PREVIEW_WEBVIEW=OFF` builds the old AppKit window instead (preset menu, Voices, the 47 rows and a CPU readout), the fallback until the page has been tried in Live.

**What it lacks, because the engine lacks it:** filters, effects, the modulation matrix, glide, pitch bend and the morph. Changing preset builds a new engine, which cuts off anything sounding. **No standalone app:** clap-wrapper's standalone format needs RtAudio and RtMidi, which it downloads, and neither is in this repo, so it would add two dependencies (a human gate).

**Every build is labelled** with the engine commit and a build number that only counts up. The label shows in the page (the snapshot's `plugin.build`), in the plug-in's description and in the log's first line, so a test build in Live is never a guess.

**What it saves:** the preset, Voices and every engine row that differs from what that preset loads ("current settings only", the human's ruling). A chunk holding only a preset's own values is about 40 bytes. Older chunks (version 1) still load.

**Silence costs next to nothing.** While the engine reports itself silent (OU-4) and no event or edit arrives, the shell skips it. On 1ad270a a silent `process()` cost 7 to 13 % of a core per instance; `skip()` costs about 0.1 to 0.4 %.

**Its session log.** Each instance writes `~/Library/Logs/horde-engine-preview/session-*.tsv`: for every preset played, the blocks that had sound, their mean and worst share of the block's time, and how many overran. Idle blocks don't count, so the figures say what a note costs. These figures are from a real session, not PF-3, and they don't replace it.

## Build

```bash
HORDE_LIBS_DIR=<a full checkout>/libs HORDE_SDK_CACHE=<a full checkout>/build-release/_deps tools/engine-preview/build.sh <engine commit> --install
```

The engine comes from `git archive` of the named commit, never the builder's working tree. Everything builds outside the repo. `--install` copies the VST3, AU and CLAP into `~/Library/Audio/Plug-Ins` and runs `auval`. Run it outside Claude Code's sandbox, or the install lands in an overlay Live can't see.

## The oracles

None is in `./verify`, because the engine isn't in this repo or in CI (B568). `build.sh` runs them all, and each has a control that must fail.

- `bridge_test.cpp`: the bridge's pure half (`bridge_core.h`), headless. Every message type is acknowledged; a key the manifest lacks is reported and never sent; rack and midi keys and gestures never become Params; the manifest hash matches a value Python computed, and one changed row reads as a mismatch; a frame carries members only when `want` asks; the saved state round-trips and stays small.
- `smoke.cpp`: drives the built CLAP through its own interface and holds the output against the engine's own `horde_offline_render`, bit for bit, on five presets at blocks of 128 and at uneven blocks; that render shares no code with this shell. Through the bridge's extension (`bridge_ext.h`), a `set` on a row the host never sees reaches the engine and its seq comes back as `ack`; a frame during silence claims no sound, and one during a chord does.
- `gui_shot.mm`: opens the plug-in's real window as a host does. For the page, it sends from inside the page as GUI 3 does and checks the host and the page both hear of it; for the AppKit window, it moves real controls. It saves a picture.
- `frames_dump.cpp`: records the frames GUI 3's `ReplayTransport` plays, to `tools/labharness/fixtures/gui3-replay.json`, with `gui3-display.json` beside it (the engine's x, unit and option name for five values of every row). `build.sh` holds a fresh dump against the committed pair, byte for byte. **Re-recording them is a human gate.**

Last verified 2026-11-05 against engine `1ad270a`: bridge test, smoke and window GREEN with the stand-in page and with GUI 3's page from `tools/gui3_inline.py`; the fixtures reproduce. Not yet opened in Live with the page.
