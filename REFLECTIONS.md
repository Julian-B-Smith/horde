# REFLECTIONS — dated, prunable at /wakeup

- [2026-08-28] Stacked-PR merge-order trap: #493 (base: increment-4 branch) was
  merged AFTER #492 had already taken that base into main — GitHub does not
  retarget an open PR when its base's PR merges unless the base BRANCH is
  deleted, so the merge landed on a corpse and increment 5 was absent from main
  until rescue PR #494. Merge stacks bottom-up, or delete merged base branches
  immediately. (Now L0044.)
- [2026-08-28] Route persistence is the loudest open gap: right-click and macro
  routes vanish on session reload (param-161's route survives). B72's
  deterministic link IDs — key (source slot, dest id) — are the natural
  serialization identity; build persistence ON that key, not before it.
- [2026-08-28] RESOLVED same day: the three FOUNDATIONS threads were read —
  all closed, ball none (stage3 took our three routing answers into their
  manifest boundary as their DECISIONS #61; seam-round2 settled by our own
  Aug-26 answer; round1 acked). Nothing owed either direction. Kept one fact:
  cite their DECISIONS #61 (edge gain / node constants / ordering) when the
  B50 visual routing matrix starts.
- [2026-09-10] I recommended exposing SPECTRA for 1.0 because an external audit framed it as "expose or remove" — and never checked the ledger, where the human had parked it on 2026-08-18. A binary framing from outside is not evidence about a standing ruling; check ROADMAP/DECISIONS for the ruling BEFORE recommending on any audit item. Candidate library lesson (evidence: this regression, retracted same day; falsifier: an audit item where the ledger is silent and the framing was the right call).
- [2026-09-10] Parallel subagents share the SESSION scratchpad: B101's PR-body file was overwritten mid-run by a sibling stream (B103 saw the same). Brief every parallel agent to use a uniquely named scratch file, or give the harness per-agent scratch dirs. Candidate library lesson once it bites a third time; evidence: two of five streams today.
- [2026-09-10] From the B105 stream, a library candidate distinct from L0032: concurrent probe runs that share ONE output artifact make every verdict unattributable — two runs over different source states both read GREEN, the planted regression included, because the browser wrote its dump early and lingered. Tag-scope every run's artifacts. Falsifier: a tag-scoped run still reporting another run's verdict.
- [2026-09-10] The leak gate only fires where `.leakcheck-names` exists — the lead's checkout. Agent worktrees skip it (untracked file absent) and CI has no names file, so an agent PR can merge a private name and every gate stays green until the lead runs `./verify` on merged main. That is exactly what happened with #531 (a grep pattern in a trace). Rule: the lead runs `./verify fast` on main after each agent merge before anything else; consider a names file in CI secrets (B-row). Library candidate.
- [2026-09-10] Twice today a scripted `verify && commit && push` chain committed past a red verify because the chain did not gate on the exit code (once past a rebase conflict, once past the leak gate) — and once the PR body then claimed green. Rule for every script the lead writes: `./verify fast || exit 1` BEFORE `git commit`; never write the verify result into a PR body from memory — paste the run. Library candidate (falsifier: a gated chain that still commits red).
- [2026-09-11] The chord-transposition hunt burned two probe iterations on a spectral peak tracker that could not separate three notes once the shift exceeded an interval — it "found" ±250 c edges and neighbouring partials and read as noise. What settled it in one run was a two-line debug export of the lane's EMITTED value and its anchor, read between blocks. When the suspect is a control-rate value the engine already holds, read the value; do not infer it from the audio. (Falsifier: a control-rate suspect with no readable state.) Library candidate — sibling of the detector-shares-assumption lesson.

