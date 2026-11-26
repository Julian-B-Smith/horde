# b682-dearest-night — the dearest soak pair ran green in 107 of 180 minutes (read 2026-11-14, landed 2026-11-25)

- **Queue item:** B682 (opened by B681). Closes it; no escalation to the human. Also opens B692, a question for the human.
- **Why:** the 11-11 night of the same pair was cancelled at the old 120-minute limit, so the 180 limit (ADR-397) had not yet been tested against the dearest soak pair.
- **Evidence consulted:** nightly run 43584958729 (2026-11-14, commit e4b0cebc), its jobs (`gh run view`), both receipts (`gh run download`) and its log, read by a scratch checker that applies B682's rule deterministically (verdicts NOT-DONE, ESCALATE when Linux is not green or over 160 min, OK); the checker was first run on two control nights: 43412915840 read OK at 146.5, 43007445035 read ESCALATE (cancelled).

## The night
Checker output: `VERDICT OK: Linux 107.3 of 180 min, 72.7 min of margin (projection 123 to 148)`. Linux receipt green, ran 4, skipped 0: stability-legacy-saw 9/9 in 368 s, fuzz-h2-engines 20000/20000 in 2899 s, soak-h2-long 899/899 in 1412 s, soak-h2-extreme 149/149 in 1745 s. macOS receipt green, ran 2: rtsan 23.5 s, tsan 162 s; the job took 3.6 min. Soak cells named in the log: composed/long-fold, composed/extreme-max (night 20771, mod 3 = 2).

## Reading
The night came in under the projection's low end. Extreme-max took 1745 s against about 2070 projected, and the fuzz took 2899 s, faster than the three earlier nights (3251 s from the cancelled 11-11 run's log, which uploaded no Linux receipt; 3343 and 3914 s), so the runner was quick. ADR-397's next step (the fuzz on its own runner) is not needed.

## Landing, 2026-11-25
The row and this trace sat uncommitted in the `b676-rulings` worktree from 2026-11-14 until the human asked for them to land. Before landing, an independent reader (Opus, read-only) checked every figure against the run, its receipts and its log. Seven held. One was wrong: the draft said all three pairs had finished under 180 with a worst margin of 33.5 min, counting only each pair's first finished night. The same dearest pair ran again on 2026-11-17 (run 44077340445, night 20774) and took 154.4 min, 25.6 min of margin, the slowest finished night through 2026-11-25. Every night through 2026-11-25 was green under 180 and none came within 20 minutes, so the verdict (no escalation) holds. The row now cites 11-17 as the dearest night seen. The 2026-11-12 night ran under the old 120-minute limit (24.2 min of margin), so its margin is not counted against 180.

The same worktree held a second uncommitted row, B692: two lab gates went red on fixed time limits under load on 2026-11-14. A second independent reader found the question still open on main, found that ADR-214 (the human's 2026-10-12 ruling raising `lab_load_check`'s limit to 120 s) must be cited, and found that only the `sandbox_check` red is recorded (`traces/2026-11-14-b676-audit-rulings.md:19`). The row says the `lab_load_check` red is the lead's observation with no trace, and puts verify's own parallel gates (since `f21a67fe`) as an unmeasured hypothesis.

## What changed
`ROADMAP.md`: B682 DONE with the figures above; B692 OPEN, for the human, between B691 and B693 (the number was reserved for it, `traces/2026-11-15-overdue-mail.md:21`). Nothing else.

- **Verify:** `./verify fast` on this branch (in the PR body).
