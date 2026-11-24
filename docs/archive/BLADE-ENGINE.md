# The blade engine, archived

> **Origin.** Written by the horde lead session, 2026-11-24, on ADR-406 ruling 5 (the human: "Let's make sure to cleanly
> archive the old engine though, in case I ever want to return to it"; the archive plan approved by poll the same day).
> ROADMAP B706. Last verified: 2026-11-24, by the proof run recorded in `traces/2026-11-24-b706-blade-archive.md`.

**What this is.** horde's 1.0 replaces blade synthesis with the wavetable member (ADR-387). The factory bank and the demos
are baked into tables once, offline, and nothing blade-era loads in the wavetable plug-in (ADR-406). This page is how to
get the blade engine back: the exact commits, what else a build needs, how to build, install and check it, and what was
recorded so a rebuild can be checked bit for bit.

## 1. What was archived

**The engine you played** is the clean-room engine at `6cd98b5`, hosted by **horde ENGINE PREVIEW**, build 11
(`engine 6cd98b5, build 11`, read from the installed binary). Its spec is SPEC-ENGINE at blob `a9640eb`, which is
horde `0285b09e`'s `specs/SPEC-ENGINE.md` (round 62), not main's: rounds 63 onward were written after the engine was
accepted.

| tag `blade-engine-final` in | commit | what it holds |
|---|---|---|
| the clean-room engine repo | `6cd98b5` | the engine (`engine2/core`, its blades in `core/blade/`), its suite as the test author wrote it, the spec it was built to, the bank (`engine2/data/bank/presets.json`, 83 presets) and the phrases |
| the clean-room tests repo | `20f64da` | the test author's suite that `6cd98b5` passed |
| horde | the commit this page lands in | the plug-in shell (`tools/engine-preview/**`: the bridge, the morph resolver's driver, the demo bank `demo_bank.json`, 16 presets), GUI 3 (`docs/design/gui3/**`, inlined by `tools/gui3_inline.py`), the blade specs and references (`specs/SPEC-ENGINE.md`'s history, `specs/SPEC-SCALPEL.md`, `reference/scalpel/**`), the blade labs (`docs/design/scalpel-horde-engine.js` and the labs that load it), the older composed engine (`h2/`), and this page's renderer (`tools/blade-archive/`) |

**The older engine,** horde's composed swarm and SCALPEL engine in `h2/engine/`, the one the clean-room engine was
measured against, is already pinned by the tag `engine2-reference-renders-v1` and its pre-release of reference renders.
No new tag is added for it.

**The plug-in's identity:** bundle id `com.mindlathe.horde-engine-preview` (`.clap`, `.vst3`, `.auv2`), AU `aumu HsEp
LfTk`, version 0.0.1. The wavetable test plug-in takes a different id (ADR-406 ruling 4), so this one installs beside it,
and Live sets saved with it open in it.

## 2. What a build needs besides the tags

| input | pinned at | where it is kept |
|---|---|---|
| clap, clap-wrapper, choc | horde's submodules at the tag: clap `195b42a0` (1.2.10), clap-wrapper `35f524b7`, choc `a08bfd84`, with the three choc patches in `libs/patches/` | horde, and a git bundle of each at its pin in the local archive folder (`git-bundles/`) |
| VST3 SDK | `9fad977` (v3.8.0_build_66), fetched at configure | the local archive folder (`sdk-deps/`, as fetched); its licence keeps it out of the release |
| AudioUnit SDK | `53a9a20` (AudioUnitSDK-1.1.0), fetched at configure | as above |
| Weir (the test-only post-engine filter, B660) | `078f1436` (`tools/engine-preview/weir.pin`) | the local archive folder: `git-bundles/weir-078f1436.bundle` (the configure checks `git rev-parse HEAD` against the pin, so clone the bundle; an unpacked tree with a fresh `git init` is a different commit and stops the configure) and `weir-at-pin/` (a `git archive`, for reading) |
| the toolchain the hashes hold on | Apple clang 16.0.0 (clang-1600.0.26.6), macOS SDK 15.2, Darwin 25.6.0, arm64 | `render.sh` prints the running one first |

**The local archive folder** is `horde-legacy-archive/2026-11-24-blade-engine/` on the lead's machine (untracked,
beside the 2026-09-23 legacy copy that ADR-186 made). It holds `installed-build11/` (the three bundles as installed,
with `MANIFEST.txt`), `git-bundles/`, `sdk-deps/`, `weir-at-pin/`, `gui3-frames/` and `renders/` (`bank/`, `demos/`).
**It is one copy on one machine:** the SDKs and the bundles of Weir and the libraries exist nowhere else in the archive,
so a backup of the folder is the human's to keep.

## 3. The binary of record

The installed build-11 bundles are the binary of record: a rebuild from the tags says another build number (the counter
lives beside the build output) and may differ in the last bits on another toolchain. The SHA-256 of each bundle's binary
(`Contents/MacOS/horde-engine-preview`):

| bundle | binary SHA-256 |
|---|---|
| `horde-engine-preview.clap` | `0352e883fe826c20d99949560b5d7f4de5e6ed71efcc4142ea81b14a078986df` |
| `horde-engine-preview.vst3` | `72528f610ee407227cdc3de19357d19bbaeca4099d18a21c484b663ca7e97f0f` |
| `horde-engine-preview.component` | `05ac4b53879cd7e1bf6e62f02b9ff434e61d4ee4b05b53c629d6cdb7457a410a` |

They are in the GitHub release `blade-engine-final` and in the local archive folder.

**What ties a rebuild to it** (2026-11-24, the trace): the smoke test, run against build 11's own CLAP, is green (every
preset it plays bit-identical to the engine's offline render at `6cd98b5`, its plant controls firing), so build 11
plays the archived engine; the GUI 3 page embedded in build 11 is byte-identical to the page `tools/gui3_inline.py`
writes from the tag's `docs/design/` (and the page from `8b580042`, before its last change, differs: the comparison
can see a change); and build 11's GUI frames equal the rebuild's. The frames are weak evidence: the plant builds give
the same frames, so they pin the bridge's frame path for the dump's script, not the sound.

## 4. Going back

**To play it:** copy the bundles from the release or the archive folder with `ditto` (never `cp -R` into an existing
bundle, which nests a second copy), then sign each, check its binary against §3, and reset the AU cache:

```bash
P=~/Library/Audio/Plug-Ins
ditto horde-engine-preview.clap "$P/CLAP/horde-engine-preview.clap"
ditto horde-engine-preview.vst3 "$P/VST3/horde-engine-preview.vst3"
ditto horde-engine-preview.component "$P/Components/horde-engine-preview.component"
shasum -a 256 "$P"/*/horde-engine-preview.*/Contents/MacOS/horde-engine-preview
killall -9 AudioComponentRegistrar; auval -v aumu HsEp LfTk | tail -3
```

The bundles are ad-hoc signed as they were installed; check `codesign --verify --deep` on each, and re-sign with
`codesign --force -s -` only if it fails (re-signing changes the binary's bytes, so check the hash first). Run all of
this outside Claude Code's sandbox (installing, signing and `auval` hit a sandbox overlay otherwise; see the global
build notes).

**To rebuild it:** a worktree at the tag with its submodules, Weir from its bundle, a fresh output folder, then
`build.sh` without `--install` (installing would replace the binary of record under the same name):

```bash
git -C <horde> worktree add <wt> blade-engine-final
git -C <wt> submodule update --init --recursive
git clone -b pin <archive>/git-bundles/weir-078f1436.bundle <weir>
python3 -I <wt>/tools/gui3_inline.py <out>/gui3-page.html
HORDE_ENGINE2_REPO=<cleanroom-engine> \
HORDE_PREVIEW_OUT=<out>/build \
HORDE_PREVIEW_PAGE=<out>/gui3-page.html \
HORDE_LIBS_DIR=<wt>/libs \
HORDE_WEIR_DIR=<weir> \
HORDE_SDK_CACHE=<archive>/sdk-deps \
<wt>/tools/engine-preview/build.sh 6cd98b5
```

`<out>/build` must be new: `build.sh` reuses an engine export and a CMake cache it finds there. Without
`HORDE_PREVIEW_PAGE` the build shows the stand-in page, not GUI 3. If the submodules cannot be fetched, clone them from
`git-bundles/` (`git clone -b pin <bundle>`) into `<wt>/libs/<name>`. `build.sh` runs the bridge test, the smoke test
(bit-identical to the engine's offline render, with plant builds as controls) and the window shot, and dumps GUI frames;
at this tag the committed fixtures were recorded on engine `1ad270a`, so it only writes a fresh pair (checked in §5).
To install a rebuild, rename the archived build first: the two share one id.

**To work on it:** the engine's suite: `cmake -S <engine>/engine2 -B <dir> -G "Unix Makefiles"`, `cmake --build <dir>
-j4`, `ctest --test-dir <dir> -LE "long|soak"`, then `<dir>/horde_engine2_suite` (the engine's own `engine2/build.sh`
does the same on every core). The spec is horde `0285b09e`'s SPEC-ENGINE. Returning the blade engine to the product
would be a new ruling: ADR-387 and ADR-406 retired it for 1.0.

## 5. What was recorded, and the check

**The bank renders.** `tools/blade-archive/render.sh <new folder>` exports the engine at `6cd98b5`, builds its two
libraries and renders all 83 factory presets, levelled as the plug-in embeds them, on the engine's four phrases (held,
chord, fast line, trill) through its own offline entry at 44.1 kHz: 332 float WAVs, hashed into `SHA256SUMS`. A
gliding preset plays without its glide there (the phrases carry no pitch runs), and the renders run without the
plug-in's flush-to-zero, so they pin the engine's output, not the plug-in's; the smoke test is what ties the two. The
list is tracked as `tools/blade-archive/bank.sha256`:

```bash
tools/blade-archive/render.sh <new folder> --check tools/blade-archive/bank.sha256
```

It reproduced the list on 2026-11-24 from two fresh exports of the engine, and a list with one planted character reads
red.

**The demo renders.** From the rebuild's build folder:

```bash
cmake --build <out>/build/build-6cd98b5 -j4 --target preview-demo-render
<out>/build/build-6cd98b5/preview-demo-render <out>/build/engine-6cd98b5/engine2/data/bank/presets.json \
  <wt>/tools/engine-preview/demo_bank.json <new folder> --runs 1
(cd <new folder> && shasum -a 256 *.wav) | cmp - <wt>/tools/blade-archive/demos.sha256
```

120 16-bit WAVs: the 16 demos' corners, pad sweeps and single notes. Two runs on 2026-11-24 gave identical lists. Its
`metrics.json` holds CPU timings, which are measurements and are not hashed.

**The GUI frames.** From the same build folder:

```bash
<out>/build/build-6cd98b5/preview-frames-dump <the .clap> 6cd98b5 <new folder>/gui3-replay.json <new folder>/gui3-display.json
(cd <new folder> && shasum -a 256 gui3-display.json gui3-replay.json) | cmp - <wt>/tools/blade-archive/gui3-frames.sha256
```

The committed fixtures (`tools/labharness/fixtures/gui3-*.json`) were recorded on engine `1ad270a`; this pair is
`6cd98b5`'s, the same from build 11's CLAP and from the rebuild (§3).

**On another toolchain** the hashes may differ in the last bits. Then the check is the suite's result as recorded (§5's proof: one known red, 3.b) and every render
within −120 dBFS of the recorded one, the engine's tier 1 (ADR-380 A1):
`render.sh <new folder> --compare <the archive's renders/bank> --limit -120` (or `python3 -I
tools/blade-archive/compare.py <recorded> <fresh> --limit -120`). The demos are 16-bit, where one rounding step is −90.3
dBFS: compare them with `--limit -84`, two steps (the lead's reading, not a ruling). The tool takes no default: a
tolerance in this repo is registered by the human.

**The proof that ran** (2026-11-24, the trace): the bank renders from two fresh exports, identical; the preview built
from a `git archive` of this PR's branch (its preview, GUI 3, `h2` and `libs` byte-identical to main's) with GUI 3's
page: bridge, smoke and GUI green; the demos and the frames twice each, identical; the smoke test green on build 11's
own CLAP; the engine's suite at `6cd98b5`: one item red, 3.b's Crushed burst (harmonics spread 3.274 dB), the known red the engine's own report records at its acceptance, at the same value; ctest's other eight tests pass, the two WebAssembly ones skipped (no Emscripten here). **Still owed:** the same run on clean checkouts of
the tags once they are cut (ADR-406 ruling 5), appended to the trace.

## 6. What must not be deleted before this archive is cut

The tags make the commits permanent; until they exist, none of these may be deleted or rewritten: the clean-room
engine's `6cd98b5` and the tests' `20f64da` (no force-push); horde's `tools/engine-preview/**`, `docs/design/gui3/**`,
`tools/gui3_inline.py`, `tools/labharness/fixtures/gui3-*.json`, `h2/**`, `engine2/data/**`, `libs/patches/**`,
`reference/scalpel/**`, `specs/SPEC-SCALPEL.md` and the blade labs; the releases `engine2-reference-renders-v1` and
`v0.9-legacy`; Weir's `078f1436`. The blade code leaves the new engine at the retirement round after the factory patch
session (ADR-390, I22), after this archive.
