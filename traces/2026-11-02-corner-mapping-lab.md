# The corner mapping lab (B396, B270)

**What changed.** A new lab, `docs/design/corner-mapping-lab.html`; its status in the design ledger; a note on B396. No engine, spec or shell change, and no ruling.

**Why.** The human asked for a lab on how mod mapping works across morph corners, including circular dependencies and depth modulation reversals, and chose per-corner design with the 1.0 question left open.

**Evidence consulted.** B70, B72, B270, B396, ADR-261, ADR-266, GUI-3, SPEC-INTENT-BUS section 2, SPEC-ENGINE MO-4, MO-6 and MO-7. The lab's self-check prints in the page, with must-fail controls. The controls were tried with a mouse in the preview.

**Not checked.** The sound, which is the human's to judge. The draw and pick are copied from the spec's text, not compared with the test author's harness.

**Verify.** `./verify fast` on this tree; see the PR.
