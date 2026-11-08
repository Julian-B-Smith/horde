# B646: the dirty marker ignores worktree writes

**What changed.** `.claude/hooks/posttool-dirty.sh` gains one case ahead of the inside-the-repo one: a written path under `"$root"/.claude/worktrees/` exits 0 without writing `.harness/dirty`. ROADMAP gains B646.

**Why.** The human, 2026-11-07: "approve the hook change" (proposed by the lead the same day). Writes in a worktree marked the root checkout dirty, so the Stop gate asked for a root `./verify`, which the lead must not run there (the local Sluice link runs `sluice_hold_check`), and the marker could never clear. Each worktree is verified in place by the agent or lead working in it, unchanged.

**Evidence consulted.** The hook and its B158 history (`0cbe2a3b`); a hand test feeding the hook five paths with `CLAUDE_PROJECT_DIR=/R`: `/R/.claude/worktrees/x/a.cpp` clean, `/R/src/a.cpp` dirty, `/R/.claude/hooks/a.sh` dirty, `/tmp/x` clean, relative `src/a.cpp` dirty.

**Alternatives rejected.** Excluding all of `.claude/` (a hook or agent edit in the root must still dirty it).

**Verify.** `./verify fast` on this tree; see the commit.
