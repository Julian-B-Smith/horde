# b385-h2-engine-parity — horde 2's composed engine built, with its parity harness (checkpoint 2)

- **Queue item:** ROADMAP B385, checkpoint 2 (and the scenario families of checkpoint 3, built here as one
  harness). The horde lead dispatched it on 2026-10-01; the human's direction was "Confirming clean-port path".
- **Why:** ONE clean C++ composed engine against the composed JS golden, as designed in
  `docs/port/h2-engine.md` (checkpoint 1, PR #889).
- **What changed:**
  - `h2/engine/` (new):
    - `js.h`: JS number semantics, mulberry32 and the event log;
    - `swarm.h`: SwarmSynth's law with M1–M3 built in;
    - `blade.h`: the blade types and pure functions, copied from `razor_core.h`;
    - `engine.h`: the blade engine copied from `razor_core.h`, with every composed override transcribed at
      its call site (voice law, cap and cull, first tick, A2, gravity, the ensemble, D1–D3).
  - `tools/h2_scenarios.mjs` (new): the scenario language, the instrumentation and phase 1a's 125 blade
    rows, moved out of `tools/h2_scalpel_render.mjs`. Phase 1a's stream is byte-identical before and after:
    sha256 `d3d7897e2000576bf1f9c2b5e1d0c30a063ff30deb4aceb352f224af197ed49e`, both runs.
  - `tools/h2_engine_render.mjs` (new): renders 532 scenarios through the golden (P/ 261, E/ 38, T/ 125,
    C/ 108). It has a `--nudge K` probe.
  - `tools/h2_engine_stream.h` and `tools/h2_engine_parity_check.cpp` (new).
  - `CMakeLists.txt` (additive): `h2_engine_parity_check` and `h2_engine_fma_control`.
  - `tools/h2_rules_check.py` (additive): `h2/engine/` is h2 code; a second declared FMA control; two
    self-cases.
  - `h2/README.md`: the engine row and rule 7.
  - `docs/port/h2-engine.md`: amended with the as-built harness, the floor's form, the controls and the
    results.
- **Result (darwin-arm64, Node 24.10, `-O2 -ffp-contract=off`):**
  - 529 of 532 at parity (worst rms 2.1e-12, worst max-abs 1.8e-10), and events identical on all 532.
  - NONINV 532 of 532; all five golden pins hold.
  - Every one of the 9 must-fail controls fires; determinism holds; detection floor 1e-9.
  - The mean bit-exact share is 37.21% (proposed floor 30%). The FMA control fires on 24 rows (mean share
    17.65%).
  - The 529 non-ring rows also hold parity at 1- and 2-ULP input nudges.
- **The stop (grounded):** P/Starting points / Cross-mod ring (watch) :: arp misses RMS (1.69e-6), so the 1a
  exclusion rule, as briefed, cannot exempt it.
  - The golden against itself, 1 ULP apart, reads rms 2.1e-3, max 9.2e-2.
  - Under nudges, the three ring rows pass or fail RMS by chance.
  - That is ADR-065's chaotic case, and ADR-187 item 6 inherits ADR-065's rule.
  - So the check is NOT wired (it carries `UNWIRED:` with the reason) until the lead rules (open question 4
    of the design doc). Wiring it red would turn `./verify full` red for everyone.
- **Evidence consulted:** everything listed in `traces/2026-10-01-b385-h2-engine-design.md`; DECISIONS ADR-065
  (the chaotic-regime precedent); `tools/test_table_check.py` (UNWIRED declarations);
  `tools/swarm_sr_parity.h` (the KNOWN pattern, considered and not used: it is a gate decision).
- **Alternatives rejected:**
  - A per-scenario bit-exact floor: measured fragile, moving up to 57 points under a 1-ULP nudge.
  - Wiring the check red.
  - Removing the ring rows from the stream, or reseeding them: that is an exclusion without its evidence,
    and the brief's rule forbids it.
  - Keeping `std::round` fault F1: unreachable in the composed engine (A2 pre-rounds), so it would be a
    control that cannot fire. Replaced by A2a and A2b.
  - A V8 libm port to remove the libm seeds: it would tie the product to a defunct system's arithmetic.
- **Verify:** `./verify fast` and `./verify full` on the committed hash, backgrounded; verbatim in the PR
  body. `./verify full` does not yet run the new check (UNWIRED); its own run is in the PR body.
- **Open questions:** (1) M1–M3's OFF positions (built without them); (2) where the five pins are enforced;
  (3) the floor's form (mean share, 30%, darwin-arm64); (4) the ring rows: ADR-065 removal, an evidence tier
  (recommended), or the 1a rule as it stands. Also: Linux CI has no floor pin yet (it reports and SKIPs).
