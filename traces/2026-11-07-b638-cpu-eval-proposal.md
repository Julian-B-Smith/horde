# 2026-11-07 — The CPU evaluation framework, proposed (B638, ADR-380 PROPOSED)

**What changed.**
- **`docs/proposals/cpu-eval-framework.md`** (new). The human's brief adapted to horde: a summary table (brief part → existing piece / new work / owner), the nine repo questions answered with evidence, the adapted design (roles, measurement, cost tree, tiers, `./verify`, ADR-370), a phased roadmap, thirteen decisions for the human, and the risks.
- **`DECISIONS.md`: ADR-380, PROPOSED.** States the proposal and that it awaits the human's ratification.
- **`docs/design/design-ledger.json`:** a `not_design` entry for ADR-380.
- **`ROADMAP.md`: B638**, OPEN — proposed.

**Why.** The human wrote the brief (2026-11-06) and answered "yes, write the proposal" to the lead's review (2026-11-07).

**Evidence consulted** (read only).
- The builder's repository: `REPORT.md` (batch 3: PF-1 61.89 % on `866bfd3`, Crunch horde's worst block 166.0 %, instructions retired by `/usr/bin/time -l`), `ANSWERS.md` (the CPU baseline, the gauntlet, batches 1 to 3, and the −120 dBFS escalation line), `engine2/CMakeLists.txt`, `engine2/tests/cpu/main.cpp`, `engine2/tests/harness/horde_engine.h`, `engine2/core/random/stream.h`, a grep of `engine2/core` for platform-specific code (none found), the git log (item 15a green at `ada5a96`).
- The test author's repository: `engine2/tests/items/` headers, `engine2/tests/wasm/chaotic.json`, `ANSWERS.md` Q-TA-50, the CPU tool's history.
- horde: `specs/SPEC-ENGINE.md` (DT-1 to DT-6, RT-1, IF-9, IF-10, PF-1 to PF-11, item 15, BD-7, BD-8, MO-6, MS-5, `quality`), `DECISIONS.md` (ADR-247, ADR-347, ADR-354, ADR-355, ADR-358, ADR-370), `ROADMAP.md` (B488, B517, B523), `docs/strategy/blind-spot-armor.md`, `docs/port/cpu-ledger.md`, `engine2/structure/`, `./verify`'s header, `fast`, `full` and dispatcher, `tools/nightly/run.py`, `.github/workflows/ci.yml`, `tools/engine-preview/README.md`, `h2/README.md`, `CMakeLists.txt`.
- This Mac: no Docker, Colima, Podman, Lima or valgrind on the PATH; Homebrew clang 23.1.3 present.

**Findings worth the lead's attention.**
- A fixed-threshold tier 1 already runs, by the lead's batch briefs (under −120 dBFS against the previous build, with the suite green), with no ADR. The lead's review said anything not bit-identical comes to the human; that is true only past −120 dBFS. Decision 2 asks the human to rule it.
- Scoped timers inside the engine conflict with DT-5 ("The engine reads no clock"). They are possible only in a measurement-only test target, by a spec round (decision 10).
- B519 bars engine figures from the test author, so the brief's eval agent splits: the test author writes the oracle, the lead runs it (decision 12).
- No horde tool checks that the builder's `engine2/tests/` match the test author's delivered commit. The tamper check is new work.

**Not measured.** No CPU figure in the proposal was measured for it; all come from the record. No build of the builder's tree was run on Linux, so Q2 stays "very likely, not established".

**Verify.** `nice -n 10 ./verify fast` on the commit this trace ships in.
