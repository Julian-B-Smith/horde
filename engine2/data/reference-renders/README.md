# Reference renders

The frozen set of renders of the bank, each with its parameter metadata (spec 10.1). **Supplied by
the lead (ROADMAP B497 part 2); the test author receives them and the builder does not.**

**380 renders**: every bank preset (83) and every envelope preset (12), each on the four phrases of
`../phrases/phrases.json` (held, chord, fast line, trill), by the engine as it stood when they were
made. 44.1 kHz, stereo, 24-bit PCM WAV, **unnormalised**, from sample 0 of the phrase's event list,
for the phrase's length plus a 2 s tail (a release longer than the tail is cut where it falls: the
envelope preset "Long tail" releases over 5 s and is heard for 2). 48 kHz waits until the test
author asks for it. About 373 MB of WAV, 224 MB zipped.

## Where the audio lives

The audio is **not in git**. It is a GitHub release asset:

| | |
|---|---|
| release tag | `engine2-reference-renders-v1` (created by the lead, never by an agent) |
| asset | `engine2-reference-renders-v1.zip`: the 380 WAV files, flat, and a copy of `manifest.json` |

```
gh release download engine2-reference-renders-v1 --pattern engine2-reference-renders-v1.zip --dir <empty dir>
unzip <empty dir>/engine2-reference-renders-v1.zip -d <another empty dir>
node tools/engine2_reference_renders.mjs --verify <another empty dir>      # needs only Node
```

`--verify` hashes each WAV against `manifest.json` and says which files are missing or differ. This
directory's `.gitignore` keeps audio files out of git; the metadata (JSON) is tracked.

## What is tracked here

- **`manifest.json`**: one entry per render, in preset order (the bank, then the envelope presets) and
  phrase order. `file` (`<preset id>__<phrase>.wav`), `preset_id`, `preset`, `source` (`bank` or
  `envelope`), `phrase`, `rate`, `frames`, `sha256` of the WAV, `peak` and `rms` per channel (left,
  right; linear, of the 24-bit samples the WAV holds) with `peak_dbfs` and `rms_dbfs`, `glides`, and
  `engine_digest` (the engine's own digest of the float samples before quantising). The top of the file
  holds the format, the settings, the SHA-256 of each input file, and the engine the renders came from:
  `engine.git_tree` is the git tree of `h2/engine/` at `engine.commit`, the sources that make the sound.
- **`parameters.json`**: each preset's full parameter metadata, shared by its four renders (a render's
  `preset_id` names it). `params` is every key the engine received: the preset's own keys in file
  order, then the keys it omits at B.1's base values (listed in `fromBase`). `glide` says whether the
  preset's stored glide acts in the engine that made the renders (`actsInEngine`) and why. `notExpressible`
  lists keys the engine does not have; **it is empty for every preset**: the engine took every key.
  `settings` (the top of the file) are the same for every render.
- **`glides`** (per render, in the manifest): true when the engine's glide rule can act in that render. That
  engine glides only in mono and legato presets (28 renders: 14 presets on the chord and the trill),
  between overlapping notes (a chord's simultaneous notes count) or, with `glideAlways`, after every
  note. In a poly preset a stored `glide` has no effect: 22 bank presets store one, and none glided.
  The phrases carry no glide of their own (`../phrases/README.md`).

## How they were made

By `tools/engine2_reference_renders.mjs`, from `../bank/presets.json`, `../envelopes/presets.json` and
`../phrases/phrases.json`. The level is the bank patches' default, `gain` 0.0875; the other settings
are `os` 1 (Standard), `frameHold` 1, `stealFade` 1, `poly` 8, no voice cap, host seed 1, host blocks of
128 (`parameters.json`, `settings`). All are the settings of the A/B tool (`tools/h2_ab.mjs`), which
plays a bank preset as the installed preview does. The first render of the set took 58 s.

## Reproducing them

The renders are deterministic: no clock, no unseeded random number. To make them again (the same
toolchain gives the same bytes; another compiler may move a last bit, and `--verify` then names the
files):

```
cmake -S <repo> -B <repo>/build-release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build <repo>/build-release -j4 --target h2_harness_render
node tools/engine2_reference_renders.mjs --out <dir> --bin <repo>/build-release/h2_harness_render
node tools/engine2_reference_renders.mjs --verify <dir>
```

The sources of `h2/` must be committed (the tool refuses a dirty `h2/`, so the engine hash does not lie),
and the engine must be the one in `manifest.json` (`git rev-parse HEAD:h2/engine` equals `engine.git_tree`);
once the engine has been replaced, check out `engine.commit` to reproduce. The set made here was
built with Apple clang 16.0.0 on darwin-arm64 (Node 24); two runs gave identical hashes for all 380
renders. Add `--meta engine2/data/reference-renders` to write `manifest.json` and `parameters.json` here
(only for the full set).

`node tools/engine2_reference_renders.mjs --static-check` (in `./verify fast`) checks these two JSON files
against the bank, the envelope presets, the phrases and B.1's base values, without the engine or the
audio: a changed input with the renders left as they were reads red.
