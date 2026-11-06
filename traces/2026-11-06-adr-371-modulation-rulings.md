# The modulation system's eighteen questions, ruled (ADR-371)

**What changed.** DECISIONS (ADR-371), ROADMAP row B633, and a design-ledger entry. No code.

**Why.** The human answered B630's MQ1 to MQ18 by poll in five batches, 2026-11-06, adding MPE as a source, an LFO envelope mode, and a limit that only a global LFO moves the morph position.

**Evidence consulted.** `docs/proposals/modulation-spec-draft.md` §9 (PR #1424). ADR-261 r3, ADR-266 r4 (mappings global for 1.0, now reversed). B392, B396 and the corner-mapping lab. ENGINE-NEEDS GN-40.

**Verify.** `./verify fast` on this tree; see the PR.
