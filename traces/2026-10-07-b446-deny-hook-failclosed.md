# 2026-10-07 — B446 D-S6: the PreToolUse deny hook parses commands and fails closed

**What changed.**
- `.claude/hooks/pretool-deny.sh` is now a thin wrapper around a new policy file,
  `.claude/hooks/deny_push.py`. The policy:
  - tokenises each Bash command with shell-word rules;
  - judges every git invocation in the chain, after env assignments, wrapper words and git's
    global options;
  - recurses into live `$(...)`, backticks, `sh -c` and `eval`;
  - blocks what it cannot see through.
- It blocks:
  - pushes that could write main or master, push or delete tags, delete branches, or force;
  - config that redirects pushes or hooks;
  - `gh pr merge`;
  - the old destructive list, now independent of flag order.
- It allows named feature-branch pushes. Malformed input, a missing command, a missing policy
  file or a crash all BLOCK.
- `.claude/settings.json` gains belt-and-braces deny rules for force/delete/`HEAD:main` pushes and
  for `git add -A` / `--all`. Nothing was removed from allow or deny.
- `tools/deny_hook_check.py` (WIRED: `./verify fast`) runs a verdict table (49 must-block,
  18 must-allow) against the real hook on two scratch repos, one on a feature branch and one on
  main.

**Why.** ADR-194 D-S6. The human approved the rewrite first-hand ("Yes, go ahead and rewrite the
deny hook"). Implementers are chartered never to edit `.claude/`, and one correctly refused a
relayed approval, so the lead made the change in its own session. The old hook was a regex over the
raw string.

**Evidence.**
- New hook: 0 of 67 verdicts wrong.
- Old hook (`origin/main:.claude/hooks/pretool-deny.sh`): 44 of 67 wrong, and malformed input
  exited 0.
- Controls run every time: an allow-everything stub must get every must-block row wrong, and raw
  malformed JSON must block.

**Stated limits.** The hook cannot see:
- a git alias defined outside the command;
- a script file that pushes;
- a push assembled from shell variables.

The server-side ruleset on `main` (B446 Tier A, applied by the human in the GitHub UI) is the
backstop for those.

**Verify.** See the PR body for the `./verify fast` result at this commit.
