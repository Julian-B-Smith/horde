# The new engine's CPU baseline

**What changed.** A new ledger, `docs/port/engine2-cpu-ledger.md`, and a note on B494. No code, spec or test.

**Why.** PF-3's figure was owed (ADR-347), and the human asked how much more efficient the clean room made the engine.

**Evidence consulted.** The clean-room tool at `5442272`, unmodified for the official figure; scratch copies at load limit 5 for the rest; the current engine's `measure_h2_engine` at `--os 1`; a `sample` of `pf1` with debug lines.

**Verify.** `./verify fast` on this tree; see the PR.
