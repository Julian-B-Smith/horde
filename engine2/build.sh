#!/usr/bin/env bash
# Configure, build and test engine2 in one go:  engine2/build.sh <absolute build dir>
# The build dir must be absolute: the shell sandbox resets the working directory between calls,
# and a relative `cmake --build` then builds nothing and exits 0.
set -euo pipefail

build="${1:?usage: engine2/build.sh <absolute build dir>}"
case "$build" in /*) ;; *) echo "engine2/build.sh: build dir must be an absolute path" >&2; exit 2 ;; esac
here="$(cd "$(dirname "$0")" && pwd)"

cmake -S "$here" -B "$build" -G "Unix Makefiles"
cmake --build "$build" -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
ctest --test-dir "$build" --output-on-failure
"$build/horde_engine2_suite"   # ctest hides a passing test's output; the summary line is worth seeing
