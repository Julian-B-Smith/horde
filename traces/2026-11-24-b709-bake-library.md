# 2026-11-24 — ADR-406 A1 and B709: the bake bounded, a wavetable library; the archive tags cut

**What changed.** DECISIONS.md: ADR-406 A1, the human's direction after #1516 merged (most presets, not all; a
wavetable library as in Serum, beside the patch library), with the lead's readings (the bake runs the transplant tap
with BB-F13's L1 and L2 and names what it cannot carry, no L3 or pan-separation reference; Smear and strike released as
approximate; the full bake at BB-13's f_ref; WT-45's library extended, its browser the GUI session's). ROADMAP: B709
(the bake over the bank and the demos, into the library), QUEUED; B706 records the merge, the tags and the release
assets. Design ledger: ADR-406's entry notes A1's hand-over.

**The archive tags** (annotated `blade-engine-final`, cut by the lead on ADR-406 ruling 5): horde at `39bc2e23` (the
merge of #1516), the clean-room engine at `6cd98b5`, the tests at `20f64da`. The harness refused the lead's push of
the horde tag ("pushing tags is not allowed. If it is genuinely needed, ask the human to run it"), so the human pushes
all three. **The release assets**, built from the local archive folder and checked: the build-11 bundles
(`horde-engine-preview-build11-macos.zip`, `f466249d…`; unzipped, each bundle's `codesign --verify --deep` passes and
its binary hashes `0352e883…`, `72528f61…`, `05ac4b53…` as recorded), the bank renders (`b71c2f44…`), the demo renders
(`6feee991…`), the GUI frames (`493d32a2…`), and their SHA256SUMS. The draft release follows the tag's push.

**Why.** The human, 2026-11-24: "Merged." and the A1 text quoted in DECISIONS.

**Evidence consulted.** ADR-406 and B706; WT-45 (the user library); the bake-1 report and its two reviews
(the wavetable repo at `2f9b3a0`).

**Verify.** `./verify fast` in this worktree, result in the PR.

**Open.** The tags' push (the human); the draft release (the lead, after the push), published by the human; the proof
run on clean checkouts of the tags; B709's bake.
