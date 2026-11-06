# 2026-11-06 — The Map Mode spec per corner; GUI-53 (corner mappings' five answers); GUI-54 (the Mindlathe splash, waiting on artwork)

**What changed.**
- **`docs/design-system/HORDE-Map-Mode-Spec.md`, a protected path.** The scope rule and its decisions-table row now read per corner, citing ADR-371 MQ17 and GUI-51. The human approved the edit directly in this session ("Yes, edit that rule"), after the lead relayed the same approval.
- **GUI-53:** the corner-mapping lab's five answers, relayed by the Labs facilitator. Two need GUI work: the per-patch loop-mode setting, and a warning naming what a load dropped.
- **GUI-54:** the Mindlathe splash. Once per Live session, it fades on the first click or after 2.5 s. It waits on the artwork as a file.
- **GN-53 and GN-54**, with GN-27 citing GUI-53.
- **GB30 and GB31.**
- The corner-mapping lab is ruled; the ledger and the index are updated.

**Why.** The human's words in this session ("I want my Mindlathe logo to overlay the gui...", the poll answers), the facilitator's corner-mapping relay, and the lead's request for the spec edit.

**Not done.**
- The splash's component and shell: the logo was sent as an image in chat, but no file reached the repo, and an overlay needs it with a transparent background.
- The per-corner route rows and the loop control (GB31).

**Verify.** `./verify fast` on the commit this trace ships in.
