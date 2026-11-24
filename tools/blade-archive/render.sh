#!/usr/bin/env bash
# tools/blade-archive/render.sh <out dir> [--work <dir>] [--check <SHA256SUMS>] [--compare <recorded dir> --limit <dBFS>]
# The blade engine's archive renders (B706, ADR-406 ruling 5; docs/archive/BLADE-ENGINE.md). Built from nothing but
# the clean-room engine's history and this file's directory:
#   1. git archive of the clean-room engine at the archived commit into <work dir>/engine: read-only, nothing is
#      written to that repository. Its location: $HORDE_ENGINE2_REPO, default ~/Documents/Claude/cleanroom-engine.
#   2. its two libraries built Release, -G "Unix Makefiles", nice -n 10, at most 4 jobs (the machine is shared).
#   3. archive_render.cpp compiled against them, -O3 -DNDEBUG, and run on the engine's own bank and phrases: 83 presets
#      x 4 phrases = 332 float WAVs in <out dir>, and SHA256SUMS beside them.
#   4. With --check, SHA256SUMS is compared with the given list (tools/blade-archive/bank.sha256): any difference is red.
#      With --compare <dir> --limit <dBFS>, every file is held against a recorded set by compare.py (largest difference
#      in dBFS; the doc names the limit): the check that remains when a newer toolchain moves the last bits.
# <out dir> must be empty or new: a stale bank-*.wav from an earlier run would otherwise be hashed with this one's.
# The toolchain is printed first, to set beside the one the hashes were recorded on (docs/archive/BLADE-ENGINE.md §2).
# Heavy (an engine build and 332 renders): one heavy job at a time.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# The commit the installed preview runs (build 11) and the tag `blade-engine-final` names in the engine's repo.
COMMIT=6cd98b5c6016aced3bef61c55d25c8f237523989
ENGINE_REPO="${HORDE_ENGINE2_REPO:-$HOME/Documents/Claude/cleanroom-engine}"
usage="usage: render.sh <out dir> [--work <dir>] [--check <SHA256SUMS>] [--compare <recorded dir> --limit <dBFS>]"
OUT="${1:?$usage}"
shift
WORK="${TMPDIR:-/tmp}/horde-blade-archive"
CHECK=""
COMPARE=""
LIMIT=""
while (( $# )); do
  case "$1" in
    --work) WORK="${2:?$usage}"; shift 2 ;;
    --check) CHECK="${2:?$usage}"; [[ -f "$CHECK" ]] || { echo "render.sh: no list at $CHECK" >&2; exit 2; }; CHECK="$(cd "$(dirname "$CHECK")" && pwd)/$(basename "$CHECK")"; shift 2 ;;
    --compare) COMPARE="${2:?$usage}"; [[ -d "$COMPARE" ]] || { echo "render.sh: no folder at $COMPARE" >&2; exit 2; }; shift 2 ;;
    --limit) LIMIT="${2:?$usage}"; shift 2 ;;
    *) echo "$usage" >&2; exit 2 ;;
  esac
done
[[ -z "$COMPARE" || -n "$LIMIT" ]] || { echo "render.sh: --compare needs --limit (the doc names it)" >&2; exit 2; }
JOBS="${HORDE_JOBS:-4}"
(( JOBS >= 1 && JOBS <= 4 )) || { echo "render.sh: HORDE_JOBS must be 1 to 4" >&2; exit 2; }
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
[[ -z "$(ls -A "$OUT")" ]] || { echo "render.sh: $OUT is not empty; give a new or empty folder" >&2; exit 2; }

echo "== load: $(sysctl -n vm.loadavg)"
echo "== toolchain: $(c++ --version | head -1); macOS SDK $(xcrun --show-sdk-version 2>/dev/null); Darwin $(uname -r); $(uname -m)"
echo "== the clean-room engine at ${COMMIT:0:7}, exported from $ENGINE_REPO"
# A fresh work dir each run: an earlier export or build could otherwise stand in for this one.
WORK="$(mkdir -p "$WORK" && mktemp -d "$WORK/run.XXXX")"
mkdir -p "$WORK/engine" "$WORK/phrases"
git -C "$ENGINE_REPO" archive "$COMMIT" engine2 | tar -x -C "$WORK/engine"
cmake -S "$WORK/engine/engine2" -B "$WORK/build" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release >/dev/null
nice -n 10 cmake --build "$WORK/build" -j"$JOBS" --target horde_engine2 horde_engine2_bank >/dev/null
echo "== the renderer"
nice -n 10 c++ -std=c++20 -O3 -DNDEBUG -I"$WORK/engine/engine2/core" -I"$WORK/engine/engine2/tests/harness" \
  "$HERE/archive_render.cpp" "$WORK/build/libhorde_engine2_bank.a" "$WORK/build/libhorde_engine2.a" -o "$WORK/archive_render"
# One file per phrase object, so the renderer passes each to the offline entry as it stands (python3 -I: no site or
# cwd imports).
python3 -I - "$WORK/engine/engine2/data/phrases/phrases.json" "$WORK/phrases" <<'PY'
import json, sys
doc = json.load(open(sys.argv[1]))
for p in doc["phrases"]:
    json.dump(p, open(f"{sys.argv[2]}/{p['id']}.json", "w"))
PY
echo "== render into $OUT"
nice -n 10 "$WORK/archive_render" "$WORK/engine/engine2/data/bank/presets.json" "$OUT" \
  "$WORK/phrases/held.json" "$WORK/phrases/chord.json" "$WORK/phrases/fast_line.json" "$WORK/phrases/trill.json"
(cd "$OUT" && shasum -a 256 bank-*.wav > SHA256SUMS)
echo "== $(wc -l < "$OUT/SHA256SUMS" | tr -d ' ') files hashed into $OUT/SHA256SUMS"
if [[ -n "$COMPARE" ]]; then
  python3 -I "$HERE/compare.py" "$COMPARE" "$OUT" --limit "$LIMIT" || { echo "RED: the renders differ from $COMPARE beyond the limit"; exit 1; }
fi
if [[ -n "$CHECK" ]]; then
  if cmp -s "$CHECK" "$OUT/SHA256SUMS"; then echo "archive renders: every file reproduces $CHECK"
  else echo "RED: the renders differ from $CHECK"; diff "$CHECK" "$OUT/SHA256SUMS" | head -20; exit 1; fi
fi
