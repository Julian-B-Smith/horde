# Reference renders

The frozen set of renders of the bank, each with its parameter metadata (spec 10.1). **Supplied by
the lead (ROADMAP B497 part 2); the test author receives them and the builder does not.**

**380 renders** at 44.1 kHz and the same **380 at 48 kHz** (the test author asked, Q-TA-2; ROADMAP B497
part 3): every bank preset (83) and every envelope preset (12), each on the four phrases of
`../phrases/phrases.json` (held, chord, fast line, trill), by the engine as it stood when they were
made. Stereo, 24-bit PCM WAV, **unnormalised**, from sample 0 of the phrase's event list,
for the phrase's length plus a 2 s tail (a release longer than the tail is cut where it falls: the
envelope preset "Long tail" releases over 5 s and is heard for 2). About 373 MB of WAV, 224 MB zipped
at 44.1 kHz; about 425 MB of WAV, 243 MB zipped at 48 kHz.

## Where the audio lives

The audio is **not in git**. It is a GitHub release asset:

| | |
|---|---|
| release tag | `engine2-reference-renders-v1` (created by the lead, never by an agent) |
| asset, 44.1 kHz | `engine2-reference-renders-v1.zip`: the 380 WAV files, flat, and a copy of `manifest.json` |
| asset, 48 kHz | `engine2-reference-renders-v1-48k.zip`: the 380 WAV files, flat (the same file names), and a copy of `manifest-48k.json` |

```
gh release download engine2-reference-renders-v1 --pattern engine2-reference-renders-v1.zip --dir <empty dir>
unzip <empty dir>/engine2-reference-renders-v1.zip -d <another empty dir>
node tools/engine2_reference_renders.mjs --verify <another empty dir>      # needs only Node

gh release download engine2-reference-renders-v1 --pattern engine2-reference-renders-v1-48k.zip --dir <empty dir>
unzip <empty dir>/engine2-reference-renders-v1-48k.zip -d <another empty dir>
node tools/engine2_reference_renders.mjs --verify <another empty dir> --sr 48000
```

The two sets have the same file names, so unzip each into its own directory. `--verify` hashes each WAV
against the manifest of that rate and says which files are missing or differ. This
directory's `.gitignore` keeps audio files out of git; the metadata (JSON) is tracked.

## What is tracked here

- **`manifest.json`**: one entry per render, in preset order (the bank, then the envelope presets) and
  phrase order. `file` (`<preset id>__<phrase>.wav`), `preset_id`, `preset`, `source` (`bank` or
  `envelope`), `phrase`, `rate`, `frames`, `sha256` of the WAV, `peak` and `rms` per channel (left,
  right; linear, of the 24-bit samples the WAV holds) with `peak_dbfs` and `rms_dbfs`, `glides`, and
  `engine_digest` (the engine's own digest of the float samples before quantising). The top of the file
  holds the format, the settings, the SHA-256 of each input file, and the engine the renders came from:
  `engine.git_tree` is the git tree of `h2/engine/` at `engine.commit`, the sources that make the sound.
- **`manifest-48k.json`**: the same, for the 48 kHz set (`rate` 48000, its own `asset`, and
  `format.phrase_timing`, the rule below). Same presets, phrases, order, settings and level; the
  `engine` block is the same engine as `manifest.json` (a static check refuses a manifest made by another
  tree), and `frames`, hashes, levels and `engine_digest` are the 48 kHz render's own. `manifest.json`
  is not touched by it.
- **`parameters.json`**: each preset's full parameter metadata, shared by its four renders (a render's
  `preset_id` names it). `params` is every key the engine received: the preset's own keys in file
  order, then the keys it omits at B.1's base values (listed in `fromBase`). `glide` says whether the
  preset's stored glide acts in the engine that made the renders (`actsInEngine`) and why. `notExpressible`
  lists keys the engine does not have; **it is empty for every preset**: the engine took every key.
  `settings` (the top of the file) are the same for every render. The 48 kHz set uses these
  unchanged: it has no `parameters-48k.json`, because the keys, the settings and the level are identical
  (a rate is not a parameter) and the glide flags agree render for render.
- **`glides`** (per render, in the manifest): true when the engine's glide rule can act in that render. That
  engine glides only in mono and legato presets (28 renders: 14 presets on the chord and the trill),
  between overlapping notes (a chord's simultaneous notes count) or, with `glideAlways`, after every
  note. In a poly preset a stored `glide` has no effect: 22 bank presets store one, and none glided.
  The phrases carry no glide of their own (`../phrases/README.md`).

## How the 48 kHz phrases are timed

`../phrases/phrases.json` gives every event as a sample at 44.1 kHz and as `time_s`, the same instant.
At 48 kHz each event's sample is **`round(time_s * 48000)`** (to nearest), and a phrase's length is its
last note-off's derived sample, plus the 2 s tail (`2 * 48000` frames). Rounding keeps the order of
events and two events a 44.1 kHz sample apart stay at least a sample apart, so key-ups still come before
key-downs at a shared sample. The result: held and chord 192000 frames, fast line 172320, trill 188640.
`phrasesAt` in the tool does this; the static check re-checks the derived phrases.

## How they were made

By `tools/engine2_reference_renders.mjs`, from `../bank/presets.json`, `../envelopes/presets.json` and
`../phrases/phrases.json`. The level is the bank patches' default, `gain` 0.0875; the other settings
are `os` 1 (Standard), `frameHold` 1, `stealFade` 1, `poly` 8, no voice cap, host seed 1, host blocks of
128 (`parameters.json`, `settings`). All are the settings of the A/B tool (`tools/h2_ab.mjs`), which
plays a bank preset as the installed preview does. The first render of the set took 58 s (the 48 kHz
set, 66 to 70 s).

**The voice pool is 8.** The engine that made these renders has a pool of 8 voices (`h2/engine/engine.h`,
`kVoices`), and a `poly` above 8 is clamped to it: `poly` 16 and 64 render bit-identically to 8 (checked on
the trill, the phrase that stacks the most voices). So under long releases (the trill on a preset with a
long release) the renders steal voices where a 16-voice pool would not. The
renders cannot show a larger pool.

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
renders, at each rate. Add `--meta engine2/data/reference-renders` to write `manifest.json` and
`parameters.json` here (only for the full set).

The 48 kHz set is the same command with `--sr 48000` (the harness takes the rate per job; nothing else
changes); with `--meta` it writes `manifest-48k.json` and no parameters:

```
node tools/engine2_reference_renders.mjs --out <dir48> --sr 48000 --bin <repo>/build-release/h2_harness_render
node tools/engine2_reference_renders.mjs --verify <dir48> --sr 48000
```

`--sr` takes 44100 (the default) or 48000; another rate is refused. The zip is the 380 WAVs plus a copy
of `manifest-48k.json`, flat (`zip -X`).

`node tools/engine2_reference_renders.mjs --static-check` (in `./verify fast`) checks these three JSON files
against the bank, the envelope presets, the phrases and B.1's base values, without the engine or the
audio: a changed input with the renders left as they were reads red. It covers both manifests, with its
own must-fail controls for the 48 kHz one (a changed frame count, a missing hash, a dropped render, a
44.1 kHz render among them, another asset name, another engine tree).
