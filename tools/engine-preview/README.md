# horde ENGINE PREVIEW

A throwaway plug-in that lets the human play the clean-room engine in Live (B624, ADR-366). It tests sound and CPU, and since B628 it also carries GUI 3 on the engine. It isn't the horde 2 shell (B398), it plays no part in the cutover decision, and it's deleted when that shell exists. Its bridge is planned as that shell's first slice (`docs/proposals/gui-on-engine.md`).

**What it has:** all 83 bank presets, levelled as the rendered previews are, with their blade-envelope routes played. 48 host parameters (47 engine rows and Preset), at most 64 so a host panel stays usable. **Polyphony is the one voice control** (the human, 2026-11-05: "One control: Polyphony"): the engine's own `voice.polyphony`, set from the page or the fallback window, with no separate Voices limit. **The plug-in's own safety rule:** a preset that loads asking for more than 8 voices, by the page, the host's Preset, a saved state or as the default, plays 8, set through the engine's Param path before the load completes, so the page shows 8. On engine `1ad270a` every bank preset asks for 16. A polyphony set by hand, at any value, stands, and a saved state keeps it.

**Its window is GUI 3's page** (B628), in a fixed 980 × 720 web view, talking to the engine through the bridge the plan's section 3 signs: the page sends `hordeNativeSend(JSON)`, and a 30 fps timer pushes `window.hordeNativeSnapshot(snapshot)`. The page's `hello` and `preset` are LOADs: the bridge answers each with one frame that acknowledges it and carries every engine row, and for a preset only once the new engine is playing, so the page never adopts the old preset's values. Host automation reaches the page as ordinary changed rows. The page can set **every** engine row, not only the 47, but a row outside the 47 isn't a host parameter, so **it can't be automated in the host** in this slice. Until GUI 3's one-file page is built in, the window shows a stand-in page (`standin_page.html`): the presets, Voices, a few rows by key and one phase picture. `HORDE_PREVIEW_PAGE=<file>` builds another page in, such as the one `tools/gui3_inline.py` writes. `HORDE_PREVIEW_WEBVIEW=OFF` builds the old AppKit window instead (preset menu, Polyphony, the 47 rows and a CPU readout), the fallback until the page has been tried in Live.

**What it lacks, because the engine lacks it:** filters, effects, the modulation matrix, glide, pitch bend and the morph. Changing preset builds a new engine, which cuts off anything sounding. **No standalone app:** clap-wrapper's standalone format needs RtAudio and RtMidi, which it downloads, and neither is in this repo, so it would add two dependencies (a human gate).

**Every build is labelled** with the engine commit and a build number that only counts up. The label shows in the page (the snapshot's `plugin.build`), in the plug-in's description and in the log's first line, so a test build in Live is never a guess.

**What it saves:** the preset and every engine row that differs from what that preset loads ("current settings only", the human's ruling). A chunk holding only a preset's own values is about 30 bytes. Older chunks still load, and a `voices` line in them is ignored.

**Silence costs next to nothing.** While the engine reports itself silent (OU-4) and no event or edit arrives, the shell skips it. On 1ad270a a silent `process()` cost 7 to 13 % of a core per instance; `skip()` costs about 0.1 to 0.4 %.

**Its session log.** Each instance writes `~/Library/Logs/horde-engine-preview/session-*.tsv`: for every preset played, the blocks that had sound, their mean and worst share of the block's time, and how many overran. Idle blocks don't count, so the figures say what a note costs. These figures are from a real session, not PF-3, and they don't replace it.

## Build

```bash
HORDE_LIBS_DIR=<a full checkout>/libs HORDE_SDK_CACHE=<a full checkout>/build-release/_deps tools/engine-preview/build.sh <engine commit> --install
```

The engine comes from `git archive` of the named commit, never the builder's working tree. Everything builds outside the repo. `--install` copies the VST3, AU and CLAP into `~/Library/Audio/Plug-Ins` and runs `auval`. Run it outside Claude Code's sandbox, or the install lands in an overlay Live can't see.

## The oracles

None is in `./verify`, because the engine isn't in this repo or in CI (B568). `build.sh` runs them all, and each has a control that must fail.

- `bridge_test.cpp`: the bridge's pure half (`bridge_core.h`), headless. Every message type is acknowledged; a key the manifest lacks is reported and never sent; rack and midi keys and gestures never become Params; the manifest hash matches a value Python computed, and one changed row reads as a mismatch; a frame carries members only when `want` asks; the saved state round-trips and stays small; the polyphony cap takes 16 to 8 and leaves 8 or fewer alone.
- `smoke.cpp`: drives the built CLAP through its own interface and holds the output against the engine's own `horde_offline_render`, bit for bit, on five presets at blocks of 128 and at uneven blocks; that render shares no code with this shell. Through the bridge's extension (`bridge_ext.h`), a `set` on a row the host never sees reaches the engine and its seq comes back as `ack`; `hello` and `preset` are answered only in the frame that completes them, with every row (a preset only after the engine swap); a frame during silence claims no sound, and one during a chord does. A preset above 8 voices loads at 8; a held 12-note chord at polyphony 12 sounds 12 voices (the plant with the old Voices limit of 4 sounds 4). The mapping audit sets every engine row through the bridge and reads its exact v back from the engine and the next frame (a plant with the key map one row off must fail it).
- `gui_shot.mm`: opens the plug-in's real window as a host does. For the page, it sends from inside the page as GUI 3 does and checks the host and the page both hear of it; for the AppKit window, it moves real controls. It saves a picture.
- `frames_dump.cpp`: records the frames GUI 3's `ReplayTransport` plays, to `tools/labharness/fixtures/gui3-replay.json`, with `gui3-display.json` beside it (the engine's x, unit and option name for five values of every row). `build.sh` holds a fresh dump against the committed pair, byte for byte. **Re-recording them is a human gate.**

Last verified 2026-11-05 against engine `1ad270a`: bridge test, smoke and window GREEN with the stand-in page and with GUI 3's plug-in mode page from `tools/gui3_inline.py`; the fixtures reproduce. Not yet opened in Live with the page.
