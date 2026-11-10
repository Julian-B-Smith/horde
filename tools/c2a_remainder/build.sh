#!/bin/bash
# Builds the C2a remainder bench against one committed version of the clean-room engine.
#   tools/c2a_remainder/build.sh [engine commit]        (default 6cd98b5)
#   HORDE_ENGINE2_REPO  the engine's git repo   (default: ~/Documents/Claude/cleanroom-engine)
#   HORDE_C2A_OUT       where to build          (default: $TMPDIR/c2a-remainder)
# The engine comes from `git archive`, never the repo's working tree (its unfinished edits would end up in the numbers).
set -euo pipefail
commit="${1:-6cd98b5}"
here="$(cd "$(dirname "$0")" && pwd)"
repo="${HORDE_ENGINE2_REPO:-$HOME/Documents/Claude/cleanroom-engine}"
out="${HORDE_C2A_OUT:-${TMPDIR:-/tmp}/c2a-remainder}"
full="$(git -C "$repo" rev-parse --short "$commit")"
export_dir="$out/engine-$full"
build_dir="$out/build-$full"
if [ ! -d "$export_dir/engine2" ]; then
  mkdir -p "$export_dir"
  git -C "$repo" archive "$full" engine2 | tar -x -C "$export_dir"
fi
cmake -S "$here" -B "$build_dir" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release "-DHORDE_ENGINE2_DIR=$export_dir/engine2"
# At most 4 threads, niced: the machine is shared (CPU throttle rule).
nice -n 10 cmake --build "$build_dir" -j4 --target c2a_remainder
echo "built: $build_dir/c2a_remainder (engine $full)"
