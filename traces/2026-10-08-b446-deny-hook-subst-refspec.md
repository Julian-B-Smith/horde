# 2026-10-08 — B446 D-S6 follow-up: push targets built from substitutions block

**What.** `.claude/hooks/deny_push.py` blocks a `git push` whose remote or refspec contains a
command substitution, such as `$(…)` or backticks. Two must-block rows are added to
`tools/deny_hook_check.py`: `git push origin $(echo main)` and a backtick form.

**Why.** After #966 replaced live substitutions with a placeholder before tokenising, a refspec
built from one reached the policy as the placeholder word, which is not a protected branch, and
passed. The lead's own push of `$(git branch --show-current)` went through on 2026-10-08. It went
to the right branch, but `$(echo main)` would have passed the hook too. The server-side `main`
ruleset still blocks that case. The hook now fails closed on its own: a target that cannot be
judged before bash expands it is refused, so name the branch literally.

**Evidence.** Before the fix, the new rows read wrong on main's hook. After it, all 71 rows are
right (52 must-block, 19 must-allow) and both controls fire.

**Approval.** A defect fix inside the rewrite the human approved first-hand (ADR-194 D-S6).
