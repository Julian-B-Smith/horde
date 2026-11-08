# 2026-11-08 — The splash probe's control waits for the page, not a fixed 300 ms (GUI-54 A1 follow-up)

**What changed.** In `tools/labharness/gui3_browser_check.mjs`, the splash's press probe (GUI-54) changes how its control waits:
- **The splash:** it now waits up to 6 s for the splash to go (it was 2 s).
- **The tab:** the Mix tab is measured again before the control click.
- **The page:** the switch is polled for up to 3 s instead of read once after 300 ms.

**Why.** The lead reported the check red on main since #1471, reproduced on a clean worktree at 54e6fa5e: "CONTROL: splash (GUI-54): the same click on Mix once it had gone did not open Mix".
- The splash had gone, since no "still up" line came with it, so the second click landed after the splash.
- Mix builds its console on its first visit, which can take longer than 300 ms on a loaded machine.
- The tab's position was measured as stable: it sat at 615, 335 across 3 s of loading. So the re-measure is a guard, not the fix.
- **Nothing is weakened:** the control still needs the click to open Mix, and the swallow, fade, timer and `first_open` lines are unchanged.

**Evidence.** `gui3_browser_check`: GREEN after the change.

**Verify.** `./verify fast` on the commit this trace ships in.
