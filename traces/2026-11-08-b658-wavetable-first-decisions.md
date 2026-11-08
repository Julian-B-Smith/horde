# B658: the wavetable integration's first decisions (ADR-388)

**What changed.** DECISIONS gains ADR-388 (four poll answers: old patches bake their blades into tables; WT-BUDGET ruled after the timed run; a `retired` row class; four technical items accepted). ROADMAP gains B658. The design ledger marks ADR-388 `not_design`.

**Why.** The human, 2026-11-08, by poll, on the seven "now" decisions of B656's inventory (§12 items 1–5, 7, 14). On old patches the human chose "Bake blades into tables" over the recommended flagging.

**Evidence consulted.** `docs/wavetable/INTEGRATION.md` on branch b656-wavetable-integration (178e3c47): §2's manifest check (0x8000 apparently free; osc 2's formula reaches the sub's 0x3000), §6 (every factory preset uses blades), §12. B652's estimate of SCALPEL's presets on wavetables.

**Placement.** The ROADMAP row sits above B654 and the ledger entry above ADR-386, so this PR and #1469 (ADR-387) do not collide.

**Verify.** `./verify fast` on this tree; see the PR.
