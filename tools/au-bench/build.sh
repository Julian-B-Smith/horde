#!/bin/sh
# Builds au_bench (B638) into <repo>/build-au-bench/ (gitignored by build-*/), or into $1.
# The CPU tool's Release flags: -O3 -DNDEBUG and no -ffp-contract flag, so the calibration loop
# compiles to the same code there and here and its seconds compare across the two reports.
# Command Line Tools only (no Xcode). Building is fine inside the sandbox; RUNNING needs the real
# AU registry (see README.md).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out=${1:-"$root/build-au-bench"}
mkdir -p "$out"
clang++ -std=c++20 -O3 -DNDEBUG -Wall -Wextra "$here/au_bench.cpp" \
  -framework AudioToolbox -framework CoreFoundation -lz -o "$out/au_bench"
echo "built $out/au_bench"
