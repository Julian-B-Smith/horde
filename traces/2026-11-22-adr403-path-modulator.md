# 2026-11-22 — B704: the path modulator replaces the morph rail (ADR-403)

**What changed.**
- DECISIONS: ADR-403, the human's four rulings by poll, each the recommended option:
  - after 1.0, specified now (spec round 65);
  - global or per note, like the LFOs; only a global path may move the morph pad;
  - one shape per patch, not morphed with the corners;
  - two path modulators per patch.
  - The lead's readings are listed in the ADR, open to overrule: GUI-56's drawing modes and puck scrub carried to the path (the GUI session's to rule), ADR-137's amendment superseded for horde 2, the 32-anchor cap and 64 samples per segment, p as a corner key on a lin law, scope per patch with the per-note switch muting morph routes, a per-note path run in the modulation system, a shape edit timed like a `Param` event.
  - The poll's other options are recorded as rejected.
- SPEC-ENGINE: a new section E.10, PM-1 to PM-15, after 1.0. Appendix E's opening names the rows, and E.9's after-1.0 list points at E.10. PM-14 reads that nothing is reserved in 1.0, pending the clean room's confirmation.
  - No 1.0 row changes its meaning. Two 1.0 questions the review found join 11.1, with tags only on the 1.0 rows: X93 (a Control route on the morph position: MS-20 against MS-29, IF-12 and IF-13; MS-20, MS-29, MR-1) and X94 (a later patch's unknown modulator, source or route in `mod`; MS-36). The count goes 73 → 75.
  - The lead's E.10 figures and readings join 11.2 as X95 to X100. The count goes 9 → 15.
- `engine2/structure/engine-structure.json`: `spec.blob` re-pinned to the new spec. E.10 is after 1.0, and the 1.0 rows gain tags only, so no engine item or edge changes.
- `docs/proposals/round-65-notes.md`: the round's notes, as for rounds 61 to 64.
- ROADMAP:
  - new row B704, the path modulator, PARKED after 1.0, with its GUI hand-over;
  - B133 is SUPERSEDED by B704, with a dated note; its text is kept.
- Design ledger: a `not_design` entry for ADR-403.

**Review.** An independent reviewer found 14 defects in the first draft. All were checked against the files and applied: among them PM-10's claim that a host lane is not heard under a Control route (IF-12 cannot do that; now X93), PM-15's FX-device morph position (against MS-38; now X100), the intent bus's X and Y pads (no destination kind; now X99), a closed one-anchor path (now no segment), and shape-edit timing (now like a `Param` event).

**Why.** The human's idea of 2026-11-22: a modulator that draws a path on an XY, whose X and Y map to parameters, moving the Morph pad when global and any pad otherwise, driven by an envelope, a knob or an LFO. It replaces B133's rail, which GUI-56 had put after 1.0.

**Evidence consulted.**
- SPEC-ENGINE Appendix E, E.0 to E.9 (MS-5, MS-6, MS-8, MS-12, MS-13, MS-20, MS-27, MS-29, MS-31 to MS-36, MS-40, MS-41, MS-45, MS-46, LF-2, LF-6, MR-1), IF-4, IF-11, IF-13 and RC-9.3.
- DECISIONS: ADR-137's amendment, GUI-56, ADR-371 (MQ8, MQ12), ADR-379, ADR-394, ADR-336, ADR-254.
- ROADMAP B133, B601, B703; GN-31 in `docs/design/gui3/ENGINE-NEEDS.md` (read, not edited).
- Round 64's delivery to both clean-room repos (`e72a092`, `2df071d`), for round 65's form; both repos' history, which has no modulation-system build or item-36 test yet.
- ADR-137 (the XY pads as macro controllers), IF-12, MS-10, MS-38, MS-43, MS-47, RC-9.3; the poll's options in the lead session.

**Verify.** `./verify fast` on this worktree (the result is recorded in the PR).
