# horde ENGINE PREVIEW

A throwaway plug-in that lets the human play the clean-room engine in Live (B624, ADR-366). It tests sound and CPU, and nothing else. It isn't the horde 2 shell (B398), it plays no part in the cutover decision, and it's deleted when that shell exists.

**What it has:** all 83 bank presets, levelled as the rendered previews are, with their blade-envelope routes played. A Voices control (default 4). A small native window with the preset menu, previous and next buttons, Voices, every exposed row and a live CPU readout: the human couldn't reach Live's generic panel. The window is plain AppKit, not GUI 3, so it adds next to nothing to what it measures. There are still at most 64 parameters, so a host panel stays usable.

**What it lacks, because the engine lacks it:** filters, effects, the modulation matrix, glide, pitch bend and the morph. Changing preset builds a new engine, which cuts off anything sounding.

**Its session log.** Each instance writes `~/Library/Logs/horde-engine-preview/session-*.tsv`: for every preset played, the blocks that had sound, their mean and worst share of the block's time, and how many overran. Idle blocks don't count, so the figures say what a note costs. These figures are from a real session, not PF-3, and they don't replace it.

## Build

```bash
HORDE_LIBS_DIR=<a full checkout>/libs HORDE_SDK_CACHE=<a full checkout>/build-release/_deps tools/engine-preview/build.sh <engine commit> --install
```

The engine comes from `git archive` of the named commit, never the builder's working tree. Everything builds outside the repo. `--install` copies the VST3, AU and CLAP into `~/Library/Audio/Plug-Ins` and runs `auval`. Run it outside Claude Code's sandbox, or the install lands in an overlay Live can't see.

## The oracles

`smoke.cpp` drives the built CLAP through its own interface and holds the output against the engine's own `horde_offline_render`, bit for bit, on five presets at blocks of 128 and at uneven blocks. That render shares no code with this shell. Two controls must fail: another preset's render, and a plant build with the routes removed. 

`gui_shot.mm` opens the plug-in's real window as a host does, moves real controls through their own actions, checks the parameter and the host both hear of it, and saves a picture. Neither is in `./verify`, because the engine isn't in this repo or in CI (B568).

Last verified 2026-11-05 against engine `be7105c`: smoke and window GREEN, `auval` succeeded. The human opened it in Live and couldn't reach the generic panel, which is why the window exists; the window hasn't yet been opened in Live.
