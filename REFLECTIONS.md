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
