# b403-h2-readme-replan — h2/README.md brought to the re-scoped plan (B385 checkpoint 1)

- **Queue item:** ROADMAP B403 (recorded by the horde lead on `lead-records-158`), an addition to B385 sent to
  the implementer mid-task on 2026-10-01: update `h2/README.md` in the design PR.
- **Why:** the README still described the superseded plan. It said the swarm core "waits on the razor_core
  ruling", pinned the composed engine at the stale `c79be56`, and its rule 4 said "There are no divergences
  yet" while `docs/port/divergences.json` holds 7. A README that lies about its tree is a bug (doctrine,
  living README).
- **What changed:** `h2/README.md` only.
  - The plan: ONE engine at `h2/engine/` (`horde2::engine`); `razor_core.h` and the lifted swarm core are
    test references.
  - Rule 1: the engine copies from the cores and includes none of them; the stale "composed layer" wording
    is gone.
  - Rule 4: the ledger as it stands. D1–D3 built, default off; M1–M3 built, default on; D4 planned (the
    sampler).
  - Rule 7: names `h2/engine/`. Its targets join the rules check when they land.
  - Status table: a composed-engine row with the five golden pins at main `c64cfdb`; the two cores' rows
    marked test references; the `c79be56` pin removed.
  - Every other rule is unchanged. The `razor-core.js@0ce6a71` pin that `h2_rules_check` rule 3 reads is
    kept.
- **Evidence consulted:** `h2/README.md` (in full); `docs/port/divergences.json` (7 entries: D1 M2 D2 M3 D3 M1
  D4; status and default per entry); `docs/port/h2-engine.md` (this PR); git blobs at `c64cfdb`.
- **Alternatives rejected:** editing this PR's first trace (traces are append-only; this entry is the record).
- **Verify:** `./verify fast` on the committed hash; verbatim in the PR body.
- **Open questions:** none beyond the design's three.
