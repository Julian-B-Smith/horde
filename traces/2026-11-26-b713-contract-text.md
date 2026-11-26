# 2026-11-26 — B713: the contract text and spec round 67 (PROPOSED), for the human's ruling

**What changed** (drafts for the human; nothing here is ruled, built or delivered):
- **`docs/proposals/rack-slot-contract.md`:** §12.10, ADR-410. It also changes:
  - the header's status lines;
  - RS-2.9 (new: `law`, `mod` and the name rule), RS-3.1, RS-3.2 (each corner's whole wire set is one Structural value; levels per wire), RS-3.3 and RS-3.8 (new: loop legality by ADR-088's per-edge predicate);
  - §4.1's class table, RS-4.1 to 4.3 and RS-4.8;
  - RS-6.2 to 6.4, §6.2 (the patch's FX section) and RS-6.15 (new: a load replaces the FX, and a patch with none loads dry).
  - §10 gains Q14 to Q22 for the human.
- **`specs/SPEC-ENGINE.md`:** spec round 67, PROPOSED (round 66 is ADR-405's). It adds:
  - D.9: MO-28 (rack keys in the resolver, via a slot table), MO-29 (a slot's on switch, MO-26's law), MO-30 (the wire set and per-wire levels) and MO-31 (the patch's `fx` member, with a dry load);
  - amendments to IF-12, IF-13, MO-3, MO-7, MO-13, MO-20, MO-26 (3), MS-20, MS-25, RC-5 and RC-11.1;
  - test item 40;
  - BD-8 (10);
  - §11.1's X101 to X124 (the count is 99).
- **`engine2/structure/engine-structure.json`:** `spec.blob` re-pinned to the draft (`2115c76…`). The declaration was reviewed, and its check is green.
- **`ROADMAP.md`:** B713 IN PROGRESS.

**Why.** ADR-410's rulings (the human, 2026-11-25) need contract and spec text before the generic module slot (B714) and the routing data (B715) can be built. The human asked for this text on 2026-11-26: "start on B713's contract text".

**Evidence.**
- **Two Opus drafters,** one per file, after two readers: one on conventions (the amendment style, the round format, clean-room delivery) and one on FOUNDATIONS' schema and each module's manifest.
- **Two critics:**
  - On rulings: 29 defects, all applied except one. Its defect 10 is wrong: round 66 exists, cleanroom-tests `7e82892` and cleanroom-engine `4f4c359`.
  - On blind testability: 14 defects, all applied.
- **The lead's calls on the critics' forks** (in the drafts as readings, open to overrule):
  - each corner's whole wire set gets one pick (round 64's precedent);
  - parity is the module's declared call;
  - loads use RS-6.6's staged adoption;
  - GUI-41 is quoted in full;
  - Device keys never reach the resolver;
  - the slot amendments name the rulings they change;
  - FX routes follow the key's own pick.
- **A fresh recheck on each lens,** then a final fix pass.
- **Checks** after that pass: `state_budget_check` GREEN, `private_name_check` GREEN, `engine2_structure_check` ok after the re-pin.

**Verify.** `./verify fast`, exit 0 (in the PR body).

**Open.**
- The human's rulings on Q14 to Q22 and X114 to X124.
- Then:
  - the round-67 notes;
  - delivery to both clean-room repos the same turn;
  - the FOUNDATIONS brief (`law`, `mod`, `io-cut`, the name rule);
  - the module notices.
- **The lead's notes for the ruling:**
  - `routing_core.h`'s `edgeLive()` passes every edge today (ADR-128), so blocking loops in 1.0 means narrowing it again.
  - A parity class needs `glides(a,b)` to be an equivalence relation, and nobody has checked Sluice's.
