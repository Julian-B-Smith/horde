# 2026-11-16: the module rows re-run on Scape's Vintage change (Scape seq 8)

**What changed.**
- horde seq 10 (`response-vintage-jitter.md`) was filed into Scape's `integrations/hypersaw/`, uncommitted there; Scape's residents commit it. It answers Scape's seq 8 (`integrations/scape/notice-vintage-jitter.md`, filed 2026-11-05).
- B561 gets a dated note.

**Why.** Scape's seq 8 asked horde to re-run the module rows on Scape main `8350fb8`, A11 in particular, and to say if anything moved past its bound. It had been waiting since 2026-11-05. The human: "go, start with 1 then 2".

**Evidence consulted.**
- `tools/admission_check.py --module scape` from horde main `4f47edf3`, with `nice -n 10`, on clean `git archive` exports:
  - Scape `11e1698`, the commit our seq 5 measured, as the control. A11's worst reading came out at 2.54544 dB, which reproduces seq 5's 2.55 dB.
  - Scape `8350fb8`, run twice. A11's line was identical in both runs.
- All three runs exited 0 (GREEN), with every control fired.
- A11's per-flip readings were compared from the JSON reports:
  - 76 of 80 readings changed in the reading, its ideal or both;
  - the reading itself went up in 42 cases and down in 25;
  - the worst rose from 2.55 to 5.80 dB, at vintage-wash, 1 kHz, Vintage to Clean, where the ideal reads 0.00.
- A11's reading is reported only (ADR-218), so no bound was crossed.
- `verb.era` runs min 0 = clean, max 1 = vintage (Scape `core/manifest.lock.json` id 24).
- The response was drafted by an Opus agent, checked by an independent Opus critic who recomputed every figure from the reports, and the critic's eight fixes were applied. The critic's main catch was a moves table that skipped the ninth-largest move.
- The metric hypothesis in the response (less Vintage noise lowers the click metric's reference) is labelled untested. It does not explain the 25 readings that fell.

**Verify.** `./verify fast` on this worktree (the result is recorded in the PR).
