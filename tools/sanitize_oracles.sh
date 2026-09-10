#!/usr/bin/env bash
# sanitize_oracles.sh — build and run the compiled ./verify oracles under a
# sanitizer (B101 robustness matrix; the CI `sanitize` job runs exactly this).
#
#   tools/sanitize_oracles.sh address,undefined [build-dir]
#   tools/sanitize_oracles.sh thread            [build-dir]
#
# THE ORACLE LIST IS PARSED FROM ./verify, never written here: the same
# "$build_dir/<name>" shape test_table_check reads, with each oracle's golden
# argument taken from the same line, and the golden generators taken from the
# `node tools/golden/gen_*_goldens.mjs` lines. A second list would drift from
# the gates it claims to cover (L0005), and drift here would read as coverage.
# conformance_check is skipped where its vendored headers are absent, exactly
# as ./verify skips it. Anything ./verify runs that this cannot parse is a
# silent gap, so the parsed list is printed before anything runs.
#
# Leak detection is OFF (ASAN_OPTIONS=detect_leaks=0): the oracles create
# plugin instances through the factory and several never destroy them by
# design, so LeakSanitizer would report the harness, not the plugin. UBSan
# halts on the first report so undefined behaviour is red, not a log line.
set -uo pipefail

SAN="${1:?usage: sanitize_oracles.sh <address,undefined|thread> [build-dir]}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${2:-build-sanitize}"
case "$BUILD" in /*) ;; *) BUILD="$ROOT/$BUILD" ;; esac
GOLDEN="$ROOT/build-golden"

# name + optional golden subdir, in ./verify's order
ORACLES=$(python3 - "$ROOT/verify" <<'PY'
import re, sys
v = open(sys.argv[1], encoding="utf-8").read()
pat = r'"\$build_dir/([a-z_0-9]+)"(?: "\$\(dirname "\$build_dir"\)/(build-golden[a-z/]*)")?'
seen = set()  # conformance_check appears twice: its [ -x ] guard and its run line
for m in re.finditer(pat, v):
    if m.group(1) in seen:
        continue
    seen.add(m.group(1))
    print(m.group(1), m.group(2) or "-")
PY
)
GENERATORS=$(grep -oE 'node tools/golden/gen_[a-z_]+\.mjs' "$ROOT/verify" | sort -u | sed 's/^node //')

echo "== sanitize_oracles: -fsanitize=$SAN  build=$BUILD"
echo "== oracles parsed from ./verify:"
echo "$ORACLES" | sed 's/^/   /'

cmake -S "$ROOT" -B "$BUILD" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DHYPERSAW_SANITIZE="$SAN" || exit 1

# Only targets that exist in this tree (conformance_check is conditional).
KNOWN=$(cmake --build "$BUILD" --target help | grep -oE '^\.\.\. [A-Za-z_0-9-]+' | sed 's/^\.\.\. //')
TARGETS=()
while read -r name _; do
  if grep -qx "$name" <<<"$KNOWN"; then TARGETS+=("$name"); else echo "   $name: no target in this tree — SKIPPED"; fi
done <<<"$ORACLES"
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
cmake --build "$BUILD" --target "${TARGETS[@]}" -j"$NPROC" || exit 1

command -v node >/dev/null || { echo "sanitize_oracles: node required for goldens" >&2; exit 1; }
for g in $GENERATORS; do
  ( cd "$ROOT" && node "$g" > /dev/null ) || { echo "sanitize_oracles: $g FAILED" >&2; exit 1; }
done

export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"
export TSAN_OPTIONS="${TSAN_OPTIONS:-halt_on_error=1}"

status=0
pass=0
while read -r name golden; do
  exe="$BUILD/$name"
  [ -x "$exe" ] || continue
  # A plain string, not an array: macOS ships bash 3.2, where an empty array
  # under `set -u` is an "unbound variable" (the first run died on every
  # golden-less oracle that way).
  if [ "$golden" = "-" ]; then arg=""; else arg="$ROOT/$golden"; fi
  echo "== $name $arg"
  if ( cd "$ROOT" && if [ -n "$arg" ]; then "$exe" "$arg"; else "$exe"; fi > "$BUILD/$name.sanitize.log" 2>&1 ); then
    tail -1 "$BUILD/$name.sanitize.log"
    pass=$((pass + 1))
  else
    echo "   FAILED (exit $?) — full log follows"
    cat "$BUILD/$name.sanitize.log"
    status=1
  fi
done <<<"$ORACLES"

echo "== sanitize_oracles [$SAN]: $pass oracle(s) passed, $([ $status -eq 0 ] && echo GREEN || echo RED)"
exit $status
