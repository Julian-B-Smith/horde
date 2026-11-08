# 2026-11-08 — B656: the wavetable integration inventory

> Origin: horde lead session, 2026-11-08, ROADMAP B656 and ADR-387 (the human, by poll: "Replace, for 1.0", "In 1.0";
> and: "let's run through everything that will have to be figured out for a C++ version to wire into the plugin
> (morph & mod contracts, randomization, gui needs)"). Written by an Opus agent on the lead's brief.

- **Queue item:** B656.
- **What changed:** one new document, `docs/wavetable/INTEGRATION.md`, and this trace. No spec, reference, ROADMAP,
  DECISIONS, GUI, tool or verify edits.
- **Why:** ADR-387 made the wavetable member the swarm's only waveform source in 1.0 and put a third oscillator in
  the plug-in. The inventory lists every change that follows (SPEC-ENGINE, the morph and modulation contracts,
  randomization, state and migration, the GUI handover, the clean room and the test plug-in's bridge, the FX device),
  with owners, an ordered plan from today to the wavetable in the test plug-in, and 24 numbered decisions with
  recommendations.
- **Evidence consulted:** `specs/SPEC-WAVETABLE.md`, `docs/wavetable/CONTRACT.md` and `specs/SPEC-ENGINE.md` on
  origin/main c85c5e03; `git show lead-records-421:DECISIONS.md` (ADR-387, 5bf3738b); DECISIONS ADR-247, ADR-254,
  ADR-374 to ADR-386, GUI-30, GUI-38, GUI-55; the lab and its traces (B650, B651, B652); the B648 bench traces (first
  run and follow-up); the clean-room engine at e72a092, read only (`core/manifest/manifest.h`, `voice/voice.h`,
  `api/engine.h`, `morph/resolver.h`, `tests/harness/horde_engine.h`, `ANSWERS.md`, `BRIEF.md`); `tools/engine-preview/`
  (README, `bridge_core.h`, `preview_plugin.cpp`, `build.sh`, `demo_bank.json`); `docs/design/gui3/layout.json`,
  `params.json`, `ENGINE-NEEDS.md`, `docs/design/CLAUDE.md`; `docs/design/random-roles-lab.html` and
  `tools/gen_random_roles_data.py`; `reference/scalpel/data/presets.json`.
- **Checks made for the document:** 0x8000–0x8FFF is unused in horde's `tools`, `docs`, `engine2` and `specs` (only
  CONTRACT names it) and in the clean-room `core` and `tests/harness`; `params.json` has 330 rows, none at or above
  0x6000; `mix.osc3` at 0x0013 is free (`kQuality` is 0x12). All 83 factory presets have blade 1 on (`w` > 0), 42 have
  blade 2 on; 14 of the 16 demo presets carry blade rows (Bamboo Flute and Rainglass carry none).
- **Findings worth the lead's attention:**
  1. The B648 follow-up counts the specified table member at 13.3 % of a core per oscillator (uncalibrated, never
     timed), against WT-BUDGET's 4 % and WT-P7's quoted 3.9 %; PF-2's 34 % over three oscillators leaves about 7.1 %
     of an M3 core per whole oscillator.
  2. The lab's per-voice Spectral-log frame is built on a worker with a nearest-frame fallback: not deterministic as
     it stands, and up to 16 frames' inverse FFTs per rebuild under motion.
  3. MO-27's corner exactness fails under Spectral log unless one-hot weights skip the blend arithmetic.
  4. RC-6.1 and RC-6.2 forbid what the PM matrix needs (the voice handing one oscillator source another's output);
     CONTRACT §8 does not list them.
  5. horde 2 has no legacy-style revision gate; IF-4's `version` and RC-9.4's migration are its equivalent.
  6. Every recipe generator, TABLECASTER's ten engines included, needs a C++ port with goldens for recipes to rebuild.
- **Alternatives rejected:** a blade-cycle generator to translate old patches in 1.0 (most of the bank is re-voiced
  anyway; kept as an option for the factory session); osc 3 with reserved blade blocks (inventing rows for a feature
  it never had); building the seam at K = 2 and adding osc 3 in a later round (the same code touched twice).
- **Private-name check:** `python3 tools/private_name_check.py` (it takes no file arguments and scans the working
  tree, untracked files included); result in the report.
- **Verify:** not run here; the lead runs `./verify`.
- **Open questions:** INTEGRATION.md section 12, decisions 1 to 24.
