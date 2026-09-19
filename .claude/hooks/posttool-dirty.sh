#!/usr/bin/env bash
# PostToolUse(Edit|Write|MultiEdit): mark the tree dirty. Cleared by ./verify.
# Cheap on purpose — running the full oracle on every edit is the wrong tempo;
# the Stop gate is where dirtiness gets cashed out.
#
# B158 (2026-09-19): only a write INSIDE the repo dirties the tree. The hook
# used to fire on every write — a PR body or a scratch script under the session
# scratchpad after an agent's final verify blocked the agent once and re-ran the
# oracle for nothing; every stream that week paid that round trip. The tool's
# input arrives as JSON on stdin; the file path is `tool_input.file_path`. If
# the path cannot be read, mark dirty anyway — a false positive costs one
# oracle run, a false negative would let an unverified edit finish.
root="${CLAUDE_PROJECT_DIR:-$(pwd)}"
path="$(python3 -c 'import json,sys
try:
    d=json.load(sys.stdin); print(d.get("tool_input",{}).get("file_path","") or "")
except Exception:
    print("")' 2>/dev/null)"
if [ -n "$path" ]; then
  case "$path" in
    "$root"/*|"$root") ;;             # inside the repo: dirty
    /*) exit 0 ;;                     # an absolute path elsewhere (scratchpad, /tmp): not ours
    *) ;;                             # relative: assume the repo
  esac
fi
mkdir -p .harness
date -u +%FT%TZ > .harness/dirty
exit 0
