# 2026-11-15 — the overdue mail: Echo, Shriek and Sluice (ADR-399, ADR-400)

**What changed.**
- `integrations/echo/response-fixed-row-modes.md` (seq 3) answers echo-003 and echo-004 on the human's poll (ADR-399):
  - Time: one knob on the face, two lanes in the DAW;
  - RC-7.4 binds a hosted module;
  - enums stored by name;
  - A12 requires a zero-macro module to refuse slots.
  - It also carries B590's division notice and the missed §12.7 notice.
- `integrations/maw/ack-d076-modify-law.md` (seq 45) and `integrations/maw/ack-lab-prototype.md` (seq 46) close the two Shriek notices that still held `ball: horde`. Both had been acted on, but no file named them, so the mailbox scan kept them overdue.
- horde seq 88 was filed into Sluice's tree (`integrations/hypersaw/response-gui-design.md`, uncommitted there: Sluice's residents commit it). It answers Sluice's seq 87 on the human's poll (ADR-400):
  - Sluice designs its FX module's glass and pages, and horde's GUI session reviews them;
  - the source stays in Sluice's tree until a copy rule;
  - Dotted and Triplet are handled per host;
  - horde's GUI session finishes the pixel title.
- DECISIONS: ADR-399, ADR-400. Design ledger: `not_design` entries for both.
- ROADMAP:
  - new rows: B693 to B696 (Echo), B697 and B698 (Sluice), and B699 (handed to the GUI session);
  - B590 amended (ruling 2);
  - dated notes on B565, B626, B527 and B331.
  - B692 is left free: an uncommitted row in the `b676-rulings` worktree holds it.

**Why.** The human asked to "clear the overdue mail first, then B603". The session hook listed these as overdue:
- echo-003 and echo-004;
- maw-005 and maw-010, which were in fact Shriek's notices d076 and lab-prototype;
- Sluice seq 87.

**Evidence consulted.**
- One Opus drafter per thread, each checked by an Opus critic. Every critic returned "fix", and the fixes were applied.
- The final Echo and Sluice texts were checked again, and the checkers' findings were applied.
- `governor/ball_scan.py`, run as the session hook runs it, on this tree: no maw thread is overdue after the acks.
- Polls: the human chose the recommended option on all eight questions.

**Verify.** `./verify fast` on this worktree (the result is recorded in the PR).
