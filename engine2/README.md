# engine2: horde's new engine, built from the spec alone

This directory is the new engine's tree: an empty static library (`horde_engine2`), an empty
acceptance suite and its runner, the input data, the click detector, and a stub for the
WebAssembly build. Nothing is built yet; the build is staged (spec BD-8).

**The clean-room rule.** The builder reads the spec and the acceptance tests (with the input data
they carry for the builder: the bank and the phrase set) and nothing else (BD-10). The tree it
builds in holds no earlier engine, no earlier engine's tests, no prototype and no legacy plugin.
**The spec is the only reference:** `../specs/SPEC-ENGINE.md` from here. A question the spec does
not answer is a gap in the spec, to be reported, not looked up elsewhere. The recipe that makes
that tree is in `CLEANROOM.md`.

## Build and run the suite

```
engine2/build.sh /absolute/path/to/a/build/dir
```

Absolute, because the shell sandbox resets the working directory between calls. It runs, in order,
`cmake -G "Unix Makefiles"` (C++20, Release unless told otherwise), the build, and `ctest`. Today
it builds an empty library and prints `suite: 0 items, 0 failed`; a second ctest test proves the
runner turns red when an item fails.

## Map

| path | what |
|---|---|
| `core/` | The engine, one directory per concept as the spec proposes (section 3). Headers are empty, one purpose line each; adapt the layout (BD-1: one concept per file, BD-2: each concept exists once). |
| `tests/` | `runner.cpp` and `suite.h` (items register themselves), `items/` (the suite, empty until the test author writes it), `harness/` (the harness header, when it lands), `controls/` (the runner's own must-fail control). |
| `data/` | The input data: the bank, the 12 envelope presets, the phrase set. See `data/README.md`. |
| `tools/click_detector.mjs` | The click detector of QS-4: `node engine2/tools/click_detector.mjs <file.wav>`. It reads WAV files and needs nothing else. |
| `wasm/` | The WebAssembly build: a stub that refuses to configure until the human approves an exact toolchain version. |
| `build.sh` | Configure, build, test. |
