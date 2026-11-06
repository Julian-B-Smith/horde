#!/bin/bash
# Builds and smoke-tests horde ENGINE PREVIEW against one committed version of the clean-room engine (B624).
#   tools/engine-preview/build.sh <engine commit> [--install]
# The engine comes from `git archive` of that commit, never the builder's working tree, whose unfinished edits would
# otherwise end up in what the human plays. The output goes outside the repo.
#   HORDE_ENGINE2_REPO   the engine's git repo   (default: ~/Documents/Claude/cleanroom-engine)
#   HORDE_PREVIEW_OUT    where to build          (default: $TMPDIR/horde-engine-preview)
#   HORDE_LIBS_DIR       libs/ with clap and clap-wrapper (worktrees have none: point this at a full checkout's libs/)
#   HORDE_SDK_CACHE      an earlier build's _deps/, to reuse the pinned SDKs instead of fetching them
#   HORDE_PREVIEW_PAGE   the page the window shows (default: the stand-in, standin_page.html); GUI 3's comes from
#                        tools/gui3_inline.py, one self-contained file
#   HORDE_PREVIEW_WEBVIEW=OFF   the AppKit window instead of the page
set -euo pipefail
commit="${1:?usage: build.sh <engine commit> [--install]}"
install="${2:-}"
here="$(cd "$(dirname "$0")" && pwd)"
repo="${HORDE_ENGINE2_REPO:-$HOME/Documents/Claude/cleanroom-engine}"
out="${HORDE_PREVIEW_OUT:-${TMPDIR:-/tmp}/horde-engine-preview}"
full="$(git -C "$repo" rev-parse --short "$commit")"
export_dir="$out/engine-$full"
build_dir="$out/build-$full"

if [ ! -d "$export_dir/engine2" ]; then
  mkdir -p "$export_dir"
  git -C "$repo" archive "$full" engine2 | tar -x -C "$export_dir"
fi

# A build number that only counts up, kept beside the builds, so every test build the human loads is told apart.
count_file="$out/build-count"
build_no=$(( $(cat "$count_file" 2>/dev/null || echo 0) + 1 ))
echo "$build_no" > "$count_file"
label="engine $full, build $build_no"
args=(-S "$here" -B "$build_dir" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release "-DHORDE_ENGINE2_DIR=$export_dir/engine2" "-DHORDE_PREVIEW_BUILD_LABEL=$label")
[ -n "${HORDE_LIBS_DIR:-}" ] && args+=("-DHORDE_LIBS_DIR=$HORDE_LIBS_DIR")
[ -n "${HORDE_PREVIEW_PAGE:-}" ] && args+=("-DHORDE_PREVIEW_PAGE=$HORDE_PREVIEW_PAGE")
[ -n "${HORDE_PREVIEW_WEBVIEW:-}" ] && args+=("-DHORDE_PREVIEW_WEBVIEW=$HORDE_PREVIEW_WEBVIEW")
if [ -n "${HORDE_SDK_CACHE:-}" ]; then
  args+=("-DFETCHCONTENT_SOURCE_DIR_HORDE_VST3SDK=$HORDE_SDK_CACHE/horde_vst3sdk-src" "-DFETCHCONTENT_SOURCE_DIR_HORDE_AUDIOUNITSDK=$HORDE_SDK_CACHE/horde_audiounitsdk-src")
fi
cmake "${args[@]}"
# At most 4 threads, niced: the clean-room sessions share this machine (see the CPU throttle rule).
nice -n 10 cmake --build "$build_dir" -j4 --target horde-engine-preview_all preview-plant preview-plant-map preview-smoke preview-gui-shot preview-bridge-test preview-frames-dump

# Every configure rewrites the AU bundle's Info.plist from CMake's template, and only a relink copies the wrapper's real
# one back. So a second run with nothing to relink leaves an AU with no identity, and auval "didn't find the component".
au_built="$(find "$build_dir" -name 'horde-engine-preview.component' -maxdepth 3 | head -1)"
cp "$build_dir/horde-engine-preview_auv2-build-helper-output/auv2_Info.plist" "$au_built/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :AudioComponents:0:name Mindlathe: horde ENGINE PREVIEW" "$au_built/Contents/Info.plist"
codesign --force -s - "$au_built"

clap="$(find "$build_dir" -name 'horde-engine-preview.clap' -maxdepth 3 | head -1)"
plant="$(find "$build_dir" -name 'preview-plant.clap' -maxdepth 3 | head -1)"
plant_map="$(find "$build_dir" -name 'preview-plant-map.clap' -maxdepth 3 | head -1)"
smoke="$(find "$build_dir" -name 'preview-smoke' -type f -maxdepth 3 | head -1)"
"$(dirname "$smoke")/preview-bridge-test"
"$smoke" "$clap" "$plant" "$plant_map"
"$(dirname "$smoke")/preview-gui-shot" "$clap" "$out/window.png"

# The recorded frames GUI 3's ReplayTransport plays (B628), where gui3_browser_check reads them. A fresh dump is held
# against the committed pair when they were recorded on this engine commit, byte for byte: a difference means the
# engine, the bridge or the dumper changed what the page would see, and re-recording is a human gate. On another
# commit it only writes the fresh pair.
fixtures="$here/../labharness/fixtures"
"$(dirname "$smoke")/preview-frames-dump" "$clap" "$full" "$out/gui3-replay.json" "$out/gui3-display.json"
if grep -q "\"engine\": \"$full\"" "$fixtures/gui3-display.json" 2>/dev/null; then
  if cmp -s "$fixtures/gui3-replay.json" "$out/gui3-replay.json" && cmp -s "$fixtures/gui3-display.json" "$out/gui3-display.json"; then
    echo "frames: the committed fixtures reproduce"
  else
    echo "frames: RED, a fresh dump differs from tools/labharness/fixtures/gui3-replay.json or gui3-display.json"
    exit 1
  fi
else
  echo "frames: the committed fixtures aren't from $full; fresh ones are at $out/gui3-replay.json and gui3-display.json"
fi

if [ "$install" = "--install" ]; then
  vst3="$(find "$build_dir" -name 'horde-engine-preview.vst3' -maxdepth 3 | head -1)"
  au="$(find "$build_dir" -name 'horde-engine-preview.component' -maxdepth 3 | head -1)"
  plugins="$HOME/Library/Audio/Plug-Ins"
  mkdir -p "$plugins/VST3" "$plugins/Components" "$plugins/CLAP"
  for pair in "$vst3:VST3" "$au:Components" "$clap:CLAP"; do
    src="${pair%%:*}"; dir="$plugins/${pair##*:}"
    dst="$dir/$(basename "$src")"
    # ditto over the same name replaces files in place; cp -R into an existing bundle would nest a second copy.
    ditto "$src" "$dst"
    codesign --force -s - "$dst"
    codesign --verify --deep "$dst"
  done
  killall -9 AudioComponentRegistrar 2>/dev/null || true
  auval -v aumu HsEp LfTk | tail -3
fi
echo "built: $label"
