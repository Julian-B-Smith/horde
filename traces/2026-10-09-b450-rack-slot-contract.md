# b450-rack-slot-contract — the hosted-module rack-slot contract, as a PROPOSED document

- **Queue item:** B450 (approved by the human 2026-10-09, "go ahead with B450"; the lead's brief of the
  same day, plus the lead's mid-task additions on state, presets and history).
- **Why:** Bulwark's brief seq 11 asked for the contract its M4 builds against. Its pieces were ratified
  but spread over a dozen records. One document, `docs/proposals/rack-slot-contract.md`, now collects
  them with citations and marks every new clause PROPOSED. It adopts FOUNDATIONS' FX-operator ABI
  (OQ #32) as the code-level interface instead of writing a second one. It adds the hosted-module
  admission test as a specification (rows A1–A20, each with a must-fail control), the parameter-class
  contract, the FX-slot preset mapping onto FOUNDATIONS' cascade, the load/state composition with
  B448's block-boundary rule, and the history clauses.
- **Clause count:** 61 clauses: 26 RATIFIED, 16 PROPOSED, and 19 that restate a ratified rule and add a
  proposed reading (marked "R / P"). There are 13 open questions for the human.
- **Evidence consulted:** horde `docs/proposals/module-1.0-bar.md`, `module-io-gain.md`,
  `fx-slot-contract.md`, `fx-chain-morph-round2.md`; DECISIONS ADR-092 (+amendment), ADR-095, ADR-128,
  ADR-169 (A1, A3, A4), ADR-170 (A2, A3), ADR-172, ADR-173, ADR-175, ADR-188 (+A1), ADR-193, ADR-195,
  ADR-197; ROADMAP B50, B139, B262, B265, B266, B281, B318, B389, B402, B428, B429, B430, B435, B438,
  B439, B448, B450; `specs/SPEC-MODULE-MACROS.md` §4–§8 (read only); `src/input_guards.h`;
  `src/fx_rack.h` `SlotContract`; `docs/H2-PLAN.md` item 26; `integrations/bulwark/` seq 11 and 12.
  FOUNDATIONS ROADMAP rows 10, 24, 32 and 33; DECISIONS #70, #117, #124, #127–#129, #131;
  `fx_operator.h`, `engine_manifest.h`, `registry.h`, `preset_cascade.h`. Read only, in the modules'
  repos: Bulwark `core/include/bulwark/{abi,common,param_table,presets}.h` and ROADMAP; Sluice
  `netcore/include/sluice/node.h` (via `git show HEAD:`, because the checkout has no `netcore/` on
  disk), `docs/horde/SLUICE-IN-HORDE.md` and `sluice-manifest.json`; Shriek
  `mawcore/include/mawcore/{abi,core,param_table}.h`, `presets/shriek-module-presets.json` and ROADMAP;
  Scape README and ROADMAP.
- **Findings for the lead:** (1) ADR-193 records D1–D8 only. The static-node model, cable weights,
  sleep with state kept, one-sample feedback cables and the click metric are from B265 rounds 3 and
  3b, which are the human's direction and listening, not an ADR. Our seq-12 response to Bulwark
  called them "ruled (ADR-193)". The document marks them P and asks the human (Q2). (2) ADR-175 and
  ADR-173 are still recorded as PROPOSED. (3) Bulwark's `presets.h` comment still calls the macro
  curve "pending horde's ruling" and lacks `exp`; ADR-169 A3 ruled it 2026-10-04.
- **Alternatives rejected:** extending ADR-095's rack-owned mix to hosted modules (it doubles their
  dry/wet and breaks ADR-195 for multiband Bulwark); keeping `ModulePreset` as a third preset format
  beside the cascade (recommended conversion at import instead, Q7); requiring `loadState` from every
  module (FOUNDATIONS made it optional, Q8).
- **Verify:** see the PR; `./verify fast` and the leak check were run on the final tree.
- **Open questions:** Q1–Q13 in §10 of the document. For the lead only: §7.2 states B448's privately
  recorded shell rules generically, as the brief listed them. Confirm this is disclosure-safe before
  merging.
