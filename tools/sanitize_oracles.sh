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

# SANITIZE_SHARD="i/n" (B505 round 2, CI's two address,undefined legs) builds
# and runs only shard i of n. ALLOWED under CI, unlike SANITIZE_ONLY, because
# the shards are a partition by construction: every parsed oracle lands in
# exactly one shard, so n legs together run every oracle once. The heavy
# oracles (HEAVY) are dealt round-robin in the order below; every other oracle goes to
# shard (its index among the non-heavy, in ./verify's order) mod n. Each shard
# prints its list and the parsed total, so the union is checkable from the
# logs. HEAVY is also the launch order inside a shard. The ORDER is the
# balance, from the sharded run 39008555080: shard 1 got selfdigest_check 586 s,
# selfdigest_product 510, stability_check 668 (the longest single oracle) and
# scalpel_fma 214, and took 21.4 min; shard 2 got parity 351, fma_control 334
# and scalpel_parity 157, and took 12.5. A shard's batch is bounded by its
# longest oracle, so stability_check moves to shard 2 (position 4 below):
# shard 1 is then bounded by selfdigest_check, shard 2 by stability_check.
HEAVY="h2_engine_selfdigest_check h2_engine_parity_check h2_engine_selfdigest_product stability_check h2_engine_fma_control h2_scalpel_parity_check h2_scalpel_fma_control"
ALLNAMES=$(echo "$ORACLES" | cut -d' ' -f1 | tr '\n' ' ')
NALL=$(echo "$ORACLES" | grep -c .)
SHARD_I=1 SHARD_N=1 SHARDTAG=""
if [ -n "${SANITIZE_SHARD:-}" ]; then
  # Strict: positive integers, no leading zero (bash arithmetic reads 08 as bad octal).
  if ! printf '%s' "$SANITIZE_SHARD" | grep -qE '^[1-9][0-9]*/[1-9][0-9]*$'; then
    echo "sanitize_oracles: SANITIZE_SHARD='$SANITIZE_SHARD' is malformed (want i/n, 1 <= i <= n)" >&2
    exit 1
  fi
  SHARD_I=${SANITIZE_SHARD%/*} SHARD_N=${SANITIZE_SHARD#*/}
  if [ "$SHARD_I" -gt "$SHARD_N" ]; then
    echo "sanitize_oracles: SANITIZE_SHARD='$SANITIZE_SHARD' is malformed (want i/n, 1 <= i <= n)" >&2
    exit 1
  fi
  SHARDTAG=" shard $SHARD_I/$SHARD_N of $NALL parsed;"
fi
MINE=""
k=0
for h in $HEAVY; do
  in_list "$h" "$ALLNAMES" || continue
  [ $((k % SHARD_N + 1)) -eq "$SHARD_I" ] && MINE="$MINE $h"
  k=$((k + 1))
done
k=0
for name in $ALLNAMES; do
  in_list "$name" "$HEAVY" && continue
  [ $((k % SHARD_N + 1)) -eq "$SHARD_I" ] && MINE="$MINE $name"
  k=$((k + 1))
done
if [ -n "$SUBSET" ]; then
  KEEP=""
  for name in $MINE; do in_list "$name" "$SANITIZE_ONLY" && KEEP="$KEEP $name"; done
  MINE="$KEEP"
fi
selected() { in_list "$1" "$MINE"; }
if [ -n "$SHARDTAG" ]; then
  echo "== shard $SHARD_I/$SHARD_N: $(echo $MINE | wc -w | tr -d ' ') of $NALL parsed oracle(s):${MINE}"
fi

cmake -S "$ROOT" -B "$BUILD" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DHYPERSAW_SANITIZE="$SAN" || exit 1

# Only targets that exist in this tree (conformance_check is conditional).
KNOWN=$(cmake --build "$BUILD" --target help | grep -oE '^\.\.\. [A-Za-z_0-9-]+' | sed 's/^\.\.\. //')
TARGETS=()
while read -r name _; do
  selected "$name" || continue
  if grep -qx "$name" <<<"$KNOWN"; then TARGETS+=("$name"); else echo "   $name: no target in this tree — SKIPPED"; fi
done <<<"$ORACLES"
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
# The shared impl library first, then ALL the oracle targets in ONE parallel
# keep-going build (`make -k`; B505 round 2: one target at a time cost ~18 min
# of a CI run), so that ONE that cannot link on this platform is reported as a
# loud SKIP rather than taking every other verdict with it. The first Linux run
# died exactly that way: state_check references hypersaw_debug_* hooks that
# src/hypersaw_clap.cpp compiles for Apple/Windows only, so the whole job was
# red and 24 oracles went unmeasured. Built means "its executable exists after
# the build"; each one is deleted first, so a stale binary from an earlier
# build can never stand in for a target that no longer builds. A skip is
# printed per oracle and counted in the summary; it is a coverage gap, never a
# pass.
cmake --build "$BUILD" --target HYPERSAW-impl -j"$NPROC" || exit 1
UNBUILT=""   # a string, not an array: bash 3.2 + set -u (see the run loop)
NUNBUILT=0
BUILDLOG="$BUILD/oracles.build.log"
if [ ${#TARGETS[@]} -gt 0 ]; then
  for t in "${TARGETS[@]}"; do rm -f "$BUILD/$t"; done
  # Exit status ignored on purpose: per-target existence below is the verdict.
  cmake --build "$BUILD" -j"$NPROC" --target "${TARGETS[@]}" -- -k > "$BUILDLOG" 2>&1
fi
for t in ${TARGETS[@]+"${TARGETS[@]}"}; do   # empty-safe under bash 3.2 + set -u
  if [ ! -x "$BUILD/$t" ]; then
    echo "   $t: DOES NOT BUILD on this platform — SKIPPED (see $BUILDLOG)"
    # The log is interleaved (-j); this target's lines are the ones naming its
    # object dir or its source file, plus make's own "*** [...]" line.
    grep -E "(CMakeFiles/$t\.dir/|/$t\.(cpp|mm)[:(]).*(undefined reference|error:)|\*\*\*.*CMakeFiles/$t\.dir/" "$BUILDLOG" \
      | head -3 | sed 's/^/      /'
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
# Launch order is SLOWEST FIRST (HEAVY's order, then the rest in ./verify's
# order), so the longest oracle is not the last one started; this changes
# when an oracle starts, never whether it runs or how its verdict is reported.
RUNLIST=""   # "name golden" lines
for h in $HEAVY; do
  RUNLIST="$RUNLIST$(echo "$ORACLES" | grep -E "^$h " )
"
done
RUNLIST="$RUNLIST$(echo "$ORACLES" | while read -r name golden; do in_list "$name" "$HEAVY" || echo "$name $golden"; done)
"
KEPT=""
while read -r name golden; do
  [ -n "$name" ] || continue
  [ -x "$BUILD/$name" ] || continue
  [ "$name" = "$NOT_UNDER_SANITIZER" ] && continue
  selected "$name" || continue
  KEPT="$KEPT$name $golden
"
done <<<"$RUNLIST"
RUNLIST="$KEPT"

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
SCOPE="$SHARDTAG$SUBSET"
echo "== running $NRUN oracle(s), $NPROC at a time${SCOPE:+ —$SCOPE}"
t_batch=$SECONDS
printf '%s' "$RUNLIST" | xargs -n 2 -P "$NPROC" bash -c "$RUN_ONE" _
t_batch=$((SECONDS - t_batch))

status=0
pass=0
TIMES=""
while read -r name golden; do
  exe="$BUILD/$name"
  selected "$name" || continue
  [ -x "$exe" ] || continue
  if [ "$name" = "$NOT_UNDER_SANITIZER" ]; then
    echo "== $name: NOT RUN under -fsanitize (replaces the allocation operators the sanitizer interposes; verdict is ./verify full's)"
    continue
  fi
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
echo "== sanitize_oracles [$SAN]: $pass oracle(s) passed, $NUNBUILT not built on this platform (${UNBUILT# }) —$SHARDTAG$SUBSET $([ $status -eq 0 ] && echo GREEN || echo RED)"
exit $status
