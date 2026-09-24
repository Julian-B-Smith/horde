# B240 — critic rework (2026-09-24)

**What changed.** The critic's notes on PR #749, applied:
- S1: the tail-list comment and the playbook now say BRAND-NEW IDS ONLY. An existing parameter joining the field needs a load migration (B255). The measured case: bassMonoHz saved at 300 loads as 120.
- T13g: tail ids must be host-visible, in a real band, and a per-osc base with its twin. T10 is widened, so every non-Device per-osc and engine row must be a member.
- A pinned layout→slot-count table, so the fixture cannot change without a marker bump.
- T13e now plants a duplicate and an oversized field as well.
- The quantum draw is frozen at the layout-9 prefix (273 rows, then `gShared`, then the tail), and `IntentCore::drawSeeds` likewise.

**Evidence.**
- T14b: re-dealing for a 437-slot field (layout 9 + 164) flips 0 owners across 27 sweep points. The control, in the pre-B240 order, flips 1915.
- T14c: the quantum patch renders bit-identically after the frozen re-deal (`ba2638773111222c` both). The pre-B240 re-deal renders `f44745338aaea8c7`.
- T15: the intent seeds keep their prefix and shared seed when grown from 200 to 364 atoms.

**Process.** The implementer stalled twice on the stream watchdog, the second time with the work complete but uncommitted. The lead preserved it as a local commit, built it, ran `morphlayout_check` (PASS), and ran `./verify full` on `3ac8649`: exit 0.

**Not done here:** B255 (the load migration), B256 (the check does not build on Linux CI), the T1b/T10b pin rulings.
