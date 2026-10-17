# readme-refresh — the README re-scoped to the legacy release and horde 2 (last verified 2026-10-17)

- **Queue item:** unqueued. The human, 2026-10-17: "Let's make sure to update the Readme". The
  doctrine's clarity standard treats a README that lies as a bug as severe as a failing test.
  No ROADMAP row holds this work; the lead's brief gave the acceptance criteria.
- **Why:** the README was last verified 2026-09-10. Since then the legacy plugin was frozen and
  released as `v0.9-legacy` (ADR-186 item 2, done 2026-10-17, ROADMAP B255). horde 2 became the
  product in development: a new shell with a new identity (ADR-186, ADR-235), a new engine being
  rebuilt from a ratified spec in a clean room (ADR-223 to ADR-231, B494, B497, B498, B502), FX
  modules from sibling projects, and a module cutoff on 2026-12-11 (ADR-236). The README still said
  "no release exists yet", described the legacy plugin as if it were the whole product, and listed
  a road to a legacy-era 1.0 that the records had superseded.
- **What changed (README.md only):** the intro says which product is downloadable; Download names
  the release and its five assets; "What it does today" says it describes the legacy plugin and is
  corrected (the sub, nine FX types, two LFOs and four envelopes as mod sources, the history page);
  "Where it is going" points at `docs/H2-PLAN.md` and its map and gives the rebuild's stages in a
  table; "How correctness is defined" separates the legacy plugin (parity) from the new engine
  (spec, invariant suite, ears); the Map adds `engine2/`, `h2/`, `specs/SPEC-ENGINE.md`, the plan
  and its map, and `integrations/`, and corrects the lab count and the generated-control count;
  "Status and known gaps" is re-verified; the Licence line says "until 1.0" (ADR-219), not "until
  its first release", because a release now exists.
- **Evidence consulted:** README.md at `5613885`; DECISIONS.md ADR-077, ADR-149, ADR-180, ADR-186,
  ADR-187, ADR-219, ADR-223, ADR-224, ADR-232 to ADR-236; ROADMAP.md B99, B101, B103, B129, B159,
  B171, B255, B458, B494, B497, B498, B502, B505 and the 1.0 section's superseded note;
  docs/H2-PLAN.md; engine2/README.md; h2/README.md; integrations/README.md; docs/img/README.md;
  specs/SPEC-ENGINE.md (status, BD-8); src/hypersaw_clap.cpp (FX, LFO, detune, topology, bend
  labels; voices 1..32; vendor "Mindlathe"); src/fx_rack.h; src/plugin_identity.h;
  src/gui/gui2.html (PAGES, MOD_SRC_NAMES, design tokens); src/gui/preset_store.h;
  .github/workflows/ci.yml; tools/package_macos.sh; tests/feature_tests.tsv;
  docs/research/2026-08-22-two-osc-cpu-measurement.md; traces/2026-08-03-fold-ensemble-timing.md;
  `gh release view v0.9-legacy` and `engine2-reference-renders-v1`; `gh pr view 1096`, `1097`.
  Numbers from fresh runs on `5613885`: `./verify fast` (test_table_check: 204 tests, 116 agentic,
  88 human, 16 awaiting an oracle; gen_gui_controls 238; engine2 suite 0 items) and a scratch
  Release build of `parity_check` against freshly generated goldens (156/156 within 1e-6, worst
  4.262e-09).
- **Removed as unverifiable or false:** the note-lifecycle conformance counts (8 passed, 3 ruled
  divergences): `conformance_check` builds only where the private sibling's headers are vendored,
  and a fresh run was not possible here. "B159 holds the ten whose reasons are still owed": the gate
  now prints 1 UNWIRED. "the labs deliberately show none" (WebAudio readouts). "Validated: ... load +
  play + MPE in Live" as a standing claim (human-run, not re-checkable here). "Build hash sits
  bottom-right of every shot" (docs/img/README.md: none of the four has it in frame). "the
  preset store's path is not yet portable" on Windows (closed by B129). "The instrument has no
  LFOs yet" (B171 built two). The legacy-era road-to-1.0 table.
- **Alternatives rejected:** a separate horde 2 README section per part, which would copy
  H2-PLAN's table and rot beside it; the plan is linked, not duplicated.
- **Verify:** `./verify fast`, exit and git hash in `.harness/last-verify.json` (see the PR).
- **Open questions:** the Licence section says there is no licence to use the code while the
  README offers a binary download; whether the human wants different words there is the human's
  call. Records inconsistencies are reported to the lead, not edited here (ROADMAP, DECISIONS and
  other files are out of this change's scope).
