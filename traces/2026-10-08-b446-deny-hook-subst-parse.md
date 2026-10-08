# 2026-10-08 — B446 D-S6 follow-up: the deny hook parses quotes nested in command substitutions

**What.** `.claude/hooks/deny_push.py` replaces each live `$(...)` / backtick substitution with a
placeholder before tokenising the outer command. It still judges the substitution's own text
recursively, and the `$(...)` scanner now tracks quotes. Two rows were added to
`tools/deny_hook_check.py`:
- an ALLOW row: a `for` loop whose `"…$(gh … -q '[… "\(.x)" …]')…"` nests double quotes inside
  single quotes inside a substitution inside double quotes;
- a BLOCK row: a protected push hidden in the same shape.

**Why.** Valid bash with quotes nested inside a `$(...)` that sits inside double quotes defeated
the plain `shlex` pass. The hook then failed closed ("unbalanced quotes") on the lead's own
read-only command (2026-10-08). Failing closed was correct, but blocking ordinary agent commands
would push people to work around the guard. Bash opens a fresh quoting context inside `$(...)`,
and the placeholder pass reproduces that.

**Evidence.** On today's main hook, the new ALLOW row reads wrong (blocked). On the fixed hook,
all 69 rows are right (50 must-block, 19 must-allow), and both controls fire.

**Approval.** This is a defect fix inside the rewrite the human approved first-hand (ADR-194 D-S6).
