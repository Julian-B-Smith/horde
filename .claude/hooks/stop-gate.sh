#!/usr/bin/env bash
# Stop / SubagentStop: the harness's closing gate.
# Blocks (exit 2) if: files were edited but verify hasn't run since (dirty),
# the last verify run was red, or the TREE changed after the last verify.
# stderr is fed back to Claude as instructions.
set -uo pipefail

INPUT=$(cat)
# Prevent infinite loops: if we already blocked once this stop cycle, allow.
ACTIVE=$(printf '%s' "$INPUT" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("stop_hook_active",False))' 2>/dev/null || echo "False")
[ "$ACTIVE" = "True" ] && exit 0

if [ -f .harness/dirty ]; then
  echo "Harness gate: edits exist that have not been verified. Run ./verify fast (or full, if closing a queue item) and report the output verbatim before finishing." >&2
  exit 2
fi

if [ -f .harness/last-verify.json ]; then
  EXIT=$(python3 -c 'import json; print(json.load(open(".harness/last-verify.json")).get("exit",0))' 2>/dev/null || echo 0)
  if [ "$EXIT" != "0" ]; then
    echo "Harness gate: last oracle run was RED (./verify report). Fix or revert before finishing; do not end on red. If the failure is out of scope, say so explicitly and record it as an open question." >&2
    exit 2
  fi
fi

# B455 H2 (audit 2026-10-10; ADR-206 item 3). The two tests above trust a marker
# only the Edit and Write tools set, and a record a fresh worktree does not have.
# An edit made through the shell (a generator, sed, a patch, a merge), or any
# edit in a worktree where verify never ran, passed them. So judge the TREE:
# every file that differs from the commit the record judged (tracked changes,
# commits since, and new untracked files) must be OLDER than the record. With no
# record, the base is where this branch left origin/main, and any change blocks.
#
# Why mtimes and not the record's commit hash: agents verify, THEN commit. The
# record then names the parent commit while the files are byte-for-byte what was
# verified; comparing hashes would force a second two-minute run for nothing.
# Known limit, held by tools/stop_gate_check.py's LIMIT row: a file DELETED after
# verify has no mtime to read, so a pure deletion is not seen here (CI sees it).
STALE=$(python3 - <<'PY' 2>/dev/null
import json, os, subprocess, sys

def git(*a):
    r = subprocess.run(("git",) + a, capture_output=True, text=True)
    return r.returncode, r.stdout

if git("rev-parse", "--is-inside-work-tree")[0] != 0:
    sys.exit(0)                      # not a repository: nothing to judge

rec = ".harness/last-verify.json"
rec_time, base = 0.0, None
if os.path.isfile(rec):
    rec_time = os.path.getmtime(rec)
    try:
        h = str(json.load(open(rec)).get("git", "")).strip()
    except Exception:
        h = ""
    if h and git("cat-file", "-e", h + "^{commit}")[0] == 0:
        base = h
if base is None:                     # no record, or one naming a commit unknown here
    rc, out = git("merge-base", "HEAD", "origin/main")
    base = out.strip() if rc == 0 and out.strip() else "HEAD"

names = set()
rc, out = git("diff", "--name-only", "-z", base)          # committed since + uncommitted
names.update(n for n in out.split("\0") if n)
rc, out = git("ls-files", "--others", "--exclude-standard", "-z")   # new, not ignored
names.update(n for n in out.split("\0") if n)

stale = sorted(n for n in names if os.path.lexists(n) and os.lstat(n).st_mtime > rec_time)
print("\n".join(stale[:5] + (["… and %d more" % (len(stale) - 5)] if len(stale) > 5 else [])))
PY
)
if [ -n "$STALE" ]; then
  {
    echo "Harness gate: the tree changed after the last ./verify (or verify never ran here):"
    printf '%s\n' "$STALE" | sed 's/^/  /'
    echo "Run ./verify fast (or full, if closing a queue item) and report the output verbatim before finishing."
  } >&2
  exit 2
fi
exit 0
