# b385-h2-engine-design — the design of horde 2's one composed C++ engine (checkpoint 1)

- **Queue item:** ROADMAP B385 (records PR #888, branch `lead-records-158`), which re-scopes B379 (B332
  phase 1b). Dispatched by the horde lead session, 2026-10-01, on the human's "Yes, re-scope phase 1b that
  way … Confirming clean-port path."
- **Why:** one C++ engine, `h2/engine/` (`horde2::engine`), ported fresh against the composed JS golden,
  replaces the plan to compose over a byte-stable `razor_core.h` (B379's blocker: it has no extension
  points). This checkpoint is the plan only: structure, what is copied from where, what is dropped and
  why, the parity harness and its controls, the scenario families, and the CPU plan.
- **What changed:** `docs/port/h2-engine.md` (new). No code.
- **Evidence consulted:** ROADMAP B310, B323, B325, B332, B335, B366, B375, B378, B379, B382, B384, B385 (on
  `origin/lead-records-158`); DECISIONS ADR-184 (A2), ADR-186, ADR-187 (A1), ADR-189 (A1);
  `docs/port/phase-1b.md`; `docs/port/scalpel-phase-1a.md`; `docs/audits/2026-09-30-swarm-core-audit.md`;
  `docs/port/divergences.json`; `h2/README.md`; LIBRARY L0068, L0070, L0071; the golden itself
  (`docs/design/scalpel-horde-engine.js` in full, `reference/scalpel/prototype/razor-core.js` in full,
  `reference/swarmsaw.html` :203–716); `h2/cores/scalpel/razor_core.h` in full;
  `h2/cores/swarm/swarm_core.h` :152, :304, :380–395, :1385–1400, :1699–1990; `tools/h2_scalpel_render.mjs`,
  `tools/h2_scalpel_parity_check.cpp`, `tools/h2_scalpel_stream.h`, `tools/h2_rules_check.py`,
  `tools/sanitize_oracles.sh`, `tools/labharness/composed_engine_check.mjs` (header and seeding);
  `docs/design/scalpel-interface-lab.html` :1774–1806 (the B366 presets); git blobs at main `c64cfdb`.
- **Alternatives rejected:**
  - Calling the lifted swarm core for the swarm half: its `controlTick` is private, it carries extras the
    golden does not have, and its parity with SwarmSynth is within 1e-6, not bit-exact, while a sync
    blade amplifies a 1e-10 relative pitch error into 6.5e-6 max-abs (phase 1a's floor).
  - Subclassing or including `razor_core.h`: it is now a test reference, and the engine owns the render.
  - Extending phase 1a's renderer in place: it is `razor_core.h`'s live gate, so the engine gets its own.
- **Verify:** `./verify fast` on the committed hash; result verbatim in the PR body.
- **Open questions:** (1) dropping M1–M3's OFF positions from the C++; (2) where the engine's five target pins
  are enforced (full, proposed, or fast); (3) the floor's form, settled by the checkpoint 2 measurement.
