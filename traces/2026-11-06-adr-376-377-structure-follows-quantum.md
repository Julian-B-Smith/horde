# No crossfades in the morph; structure follows its quantum boundaries in 1.0 (ADR-376, ADR-377)

**What changed.** DECISIONS: ADR-376 and ADR-377. ROADMAP: B635 new. Design-ledger entries. No code.

**Why.** The human's rulings of 2026-11-06: an oscillator fades in with the corner it heads to; a preset saves the puck; and no sound crossfades except as brief timed transitions, so in 1.0 structure follows its quantum boundaries. That reverses ADR-317's Blend half.

**Evidence consulted.** ADR-317; SPEC-ENGINE Appendix D (MO-13 to MO-20, SB-1 to SB-13); the round-61 notes (the legacy on/off rule: ADR-123, B65, ADR-183); the test author's B-74 study (engine-free).

**Verify.** `./verify fast` on this tree; see the PR.
