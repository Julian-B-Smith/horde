#!/usr/bin/env bash
# PreToolUse(Bash): second line of defense behind settings.json deny rules.
# Reads hook JSON on stdin; exit 2 blocks the tool call and feeds stderr to Claude.
# The policy lives in deny_push.py (B446, ADR-194 D-S6): it PARSES the command and
# FAILS CLOSED. If python3 or the policy file is missing, this blocks rather than
# allows -- the old version read a parse failure as an empty command and passed it.
set -uo pipefail

HOOK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
POLICY="$HOOK_DIR/deny_push.py"

if ! command -v python3 >/dev/null 2>&1 || [ ! -r "$POLICY" ]; then
  echo "BLOCKED by harness: the deny hook's policy (python3 + deny_push.py) is unavailable." >&2
  exit 2
fi

python3 "$POLICY"
rc=$?
# Only 0 allows. Any other status (a crash included) blocks.
[ "$rc" -eq 0 ] && exit 0
exit 2
