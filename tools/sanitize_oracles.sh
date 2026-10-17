#!/usr/bin/env bash
# sanitize_oracles.sh — build and run the compiled ./verify oracles under a
# sanitizer (B101 robustness matrix). CI's `sanitize (address,undefined)` job
# runs exactly this; its `sanitize (thread)` job runs tools/tsan_stress_check.py
# instead, since none of these oracles starts a second thread (ADR-233).
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

# SANITIZE_ONLY="a b c" builds and runs only those named oracles: a LOCAL
# debugging aid (it is how the parallel mechanism was proven on a subset with
# a planted failure, B505). It is refused under CI (GitHub sets CI=true),
# because a subset in the workflow would read as coverage it is not; every
# line that reports on a subset run says SUBSET.
SUBSET=""
if [ -n "${SANITIZE_ONLY:-}" ]; then
  if [ -n "${CI:-}" ]; then
    echo "sanitize_oracles: SANITIZE_ONLY is refused under CI — every oracle runs there" >&2
    exit 1
  fi
  SUBSET=" SUBSET (SANITIZE_ONLY=$SANITIZE_ONLY), not a coverage verdict;"
fi

in_list() { case " $2 " in *" $1 "*) return 0 ;; esac; return 1; }

cmake -S "$ROOT" -B "$BUILD" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DHYPERSAW_SANITIZE="$SAN" || exit 1

# Only targets that exist in this tree (conformance_check is conditional).
KNOWN=$(cmake --build "$BUILD" --target help | grep -oE '^\.\.\. [A-Za-z_0-9-]+' | sed 's/^\.\.\. //')
TARGETS=()
while read -r name _; do
  if [ -n "$SUBSET" ] && ! in_list "$name" "$SANITIZE_ONLY"; then continue; fi
  if grep -qx "$name" <<<"$KNOWN"; then TARGETS+=("$name"); else echo "   $name: no target in this tree — SKIPPED"; fi
done <<<"$ORACLES"
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
# The shared impl library first (one parallel build), then each oracle on its
# own so that ONE that cannot link on this platform is reported as a loud
# SKIP rather than taking every other verdict with it. The first Linux run
# died exactly that way: state_check references hypersaw_debug_* hooks that
# src/hypersaw_clap.cpp compiles for Apple/Windows only, so the whole job was
# red and 24 oracles went unmeasured. A skip is printed per oracle and counted
# in the summary; it is a coverage gap, never a pass.
cmake --build "$BUILD" --target HYPERSAW-impl -j"$NPROC" || exit 1
UNBUILT=""   # a string, not an array: bash 3.2 + set -u (see the run loop)
NUNBUILT=0
for t in ${TARGETS[@]+"${TARGETS[@]}"}; do   # empty-safe under bash 3.2 + set -u
  if ! cmake --build "$BUILD" --target "$t" -j"$NPROC" > "$BUILD/$t.build.log" 2>&1; then
    echo "   $t: DOES NOT BUILD on this platform — SKIPPED (see $BUILD/$t.build.log)"
    grep -E "undefined reference|error:" "$BUILD/$t.build.log" | head -3 | sed 's/^/      /'
    UNBUILT="$UNBUILT $t"
    NUNBUILT=$((NUNBUILT + 1))
  fi
done

command -v node >/dev/null || { echo "sanitize_oracles: node required for goldens" >&2; exit 1; }
for g in $GENERATORS; do
  ( cd "$ROOT" && node "$g" > /dev/null ) || { echo "sanitize_oracles: $g FAILED" >&2; exit 1; }
done

export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"
export TSAN_OPTIONS="${TSAN_OPTIONS:-halt_on_error=1}"

# NOT RUN UNDER A SANITIZER, by mechanism: rtsafety_probe REPLACES the global
# operator new/delete with malloc-backed counting versions — the very
# operators ASan and TSan interpose. Under ASan the pairing breaks
# (alloc-dealloc-mismatch: the runtime's operator new vs the probe's free);
# under TSan the runtime's own interceptor allocations land inside the armed
# window and the probe reports the sanitizer, not the plugin ("RED (the audio
# thread allocates)", first Linux run 2026-09-10). Its verdict is the
# UNSANITIZED gate in ./verify full, unchanged. Printed, never silent.
NOT_UNDER_SANITIZER="rtsafety_probe"

# PARALLEL RUNS (B505, ADR-233). The oracles run NPROC at a time; each writes
# its own $BUILD/<name>.sanitize.log and a $BUILD/<name>.sanitize.rc verdict
# file, and the verdicts are printed below in ./verify's order once all have
# finished, so the output reads exactly as the serial loop's did.
#
# SHARED PATHS, audited before going parallel (2026-10-17, every oracle source
# and the node scripts the h2 parity checks popen):
#   - endprobe and trace_check both press PANIC, which writes
#     $HOME/<store>/logs/panic-<trace counter>.txt; two oracles at the same
#     counter would write one file, and trace_check reads its dump back.
#   - preset_pitch_check and presetstore_check use fixed names under the temp
#     dir (hypersaw_preset_pitch_check, hypersaw_presetstore_check): distinct
#     from each other, but the temp dir is the one place a new oracle would
#     collide unseen.
#   - the golden dirs are written by the generators above, before any run,
#     and only read after; the h2 render scripts write nothing.
# Resolution: every oracle runs with its OWN HOME and TMPDIR under
# $BUILD/sandbox/<name>/ (std::filesystem::temp_directory_path honours
# TMPDIR; the two macOS-only mkdtemp callers make unique names anyway), so no
# two can share a store, a dump or a temp file by construction, not by audit
# alone. stdin is /dev/null: the serial loop's oracles inherited the loop's
# here-string (none reads stdin; only an explicit "-" argument would, and
# none is passed).
#
# Launch order is SLOWEST FIRST (measured under TSan, CI run 38991514882), so
# the longest oracle is not the last one started; this changes when an oracle
# starts, never whether it runs or how its verdict is reported.
SLOW_FIRST="stability_check h2_engine_selfdigest_check h2_engine_parity_check h2_engine_selfdigest_product h2_engine_fma_control"

RUNLIST=""   # "name golden" lines, slowest first, then ./verify's order
for first in 1 0; do
  while read -r name golden; do
    [ -x "$BUILD/$name" ] || continue
    [ "$name" = "$NOT_UNDER_SANITIZER" ] && continue
    if [ -n "$SUBSET" ] && ! in_list "$name" "$SANITIZE_ONLY"; then continue; fi
    if in_list "$name" "$SLOW_FIRST"; then [ $first -eq 1 ] || continue; else [ $first -eq 0 ] || continue; fi
    RUNLIST="$RUNLIST$name $golden
"
  done <<<"$ORACLES"
done

# One oracle: its own sandbox, its log, then "<exit> <seconds>" into its .rc.
# The runner itself always exits 0, because xargs stops launching new commands
# when one exits 255, and an oracle's own exit code must never do that.
RUN_ONE='[ -n "${1:-}" ] || exit 0   # GNU xargs runs once on empty input
  name=$1 golden=$2
  box="$SAN_BUILD/sandbox/$name"; rm -rf "$box"; mkdir -p "$box/home" "$box/tmp"
  if [ "$golden" = "-" ]; then arg=""; else arg="$SAN_ROOT/$golden"; fi
  t0=$SECONDS
  # The outer braces keep bash'"'"'s own "Abort trap" job notice for a crashed
  # oracle out of the interleaved output; the crash is in the log and the .rc.
  { ( cd "$SAN_ROOT" && export HOME="$box/home" TMPDIR="$box/tmp" &&
      if [ -n "$arg" ]; then exec "$SAN_BUILD/$name" "$arg"; else exec "$SAN_BUILD/$name"; fi
    ) > "$SAN_BUILD/$name.sanitize.log" 2>&1 < /dev/null; } 2>/dev/null
  echo "$? $((SECONDS - t0))" > "$SAN_BUILD/$name.sanitize.rc"
  exit 0'
export SAN_ROOT="$ROOT" SAN_BUILD="$BUILD"
while read -r name _; do [ -n "$name" ] && rm -f "$BUILD/$name.sanitize.rc"; done <<<"$RUNLIST"
NRUN=$(printf '%s' "$RUNLIST" | grep -c .)
echo "== running $NRUN oracle(s), $NPROC at a time${SUBSET:+ —$SUBSET}"
t_batch=$SECONDS
printf '%s' "$RUNLIST" | xargs -n 2 -P "$NPROC" bash -c "$RUN_ONE" _
t_batch=$((SECONDS - t_batch))

status=0
pass=0
TIMES=""
while read -r name golden; do
  exe="$BUILD/$name"
  [ -x "$exe" ] || continue
  if [ "$name" = "$NOT_UNDER_SANITIZER" ]; then
    echo "== $name: NOT RUN under -fsanitize (replaces the allocation operators the sanitizer interposes; verdict is ./verify full's)"
    continue
  fi
  if [ -n "$SUBSET" ] && ! in_list "$name" "$SANITIZE_ONLY"; then continue; fi
  # A plain string, not an array: macOS ships bash 3.2, where an empty array
  # under `set -u` is an "unbound variable" (the first run died on every
  # golden-less oracle that way).
  if [ "$golden" = "-" ]; then arg=""; else arg="$ROOT/$golden"; fi
  echo "== $name $arg"
  # No .rc means the runner never finished this oracle: red, never a pass.
  rc=$(cut -d' ' -f1 "$BUILD/$name.sanitize.rc" 2>/dev/null)
  secs=$(cut -d' ' -f2 "$BUILD/$name.sanitize.rc" 2>/dev/null)
  TIMES="$TIMES${secs:-?} $name
"
  if [ "$rc" = "0" ]; then
    tail -1 "$BUILD/$name.sanitize.log"
    pass=$((pass + 1))
  else
    echo "   FAILED (exit ${rc:-none: no verdict recorded}) — full log follows"
    cat "$BUILD/$name.sanitize.log" 2>/dev/null
    status=1
  fi
done <<<"$ORACLES"

echo "== parallel batch: ${t_batch}s wall for $NRUN oracle(s) on $NPROC job(s); slowest:" \
     $(printf '%s' "$TIMES" | sort -rn | head -5 | awk '{printf "%s (%ss) ", $2, $1}')
echo "== sanitize_oracles [$SAN]: $pass oracle(s) passed, $NUNBUILT not built on this platform (${UNBUILT# }) —$SUBSET $([ $status -eq 0 ] && echo GREEN || echo RED)"
exit $status
