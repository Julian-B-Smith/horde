# agent-operating-reference: the agent-facing operating reference, its patch tool and its check

- **Queue item:** B629 (ROADMAP, on the lead's records branch at the time of writing).
- **Why:** The human wants to dictate patches to an agent, so the operational detail must be organised and short. The parameter page is generated, because BD-5 rules that nothing hand-copies the manifest. A tool validates patches, because a refused key or an inert row is silent at the engine. A wired check holds the pages to the table, because a stale page misleads an agent and no audio oracle sees it.
- **What:** `docs/manual/agent/` holds four pages: README, concepts, the generated parameters page and recipes. `tools/gen_agent_reference.py` writes the parameters page from `docs/design/gui3/params.json` and PA-8. `tools/patch_check.py` validates horde-patch/1, translates a bank base by B.1, warns on PA-8-inert rows and prints the GUI 3 transport's `set` message; its `--selftest` is wired. `tools/agent_manual_check.py` (wired) checks:
  - `parameters.md` is current;
  - every recipe and README patch validates with no inert row;
  - every recipe line's value matches its resolved patch;
  - every named key exists;
  - every cited SPEC-ENGINE row exists.

  Each tool has must-fail plants.
- **Evidence consulted:**
  - `specs/SPEC-ENGINE.md`: 1 to 5.8, 6.1 to 6.6 (PA-8), 6.9, 6.10, RC-3 to RC-6, B.1 and C.0 to C.14;
  - `docs/design/gui3/params.json` and `tools/gen_gui3_controls.py` (the `inverse` law is reused);
  - the TRANSPORT in `docs/design/gui3/engine-stub.js`;
  - the not-built list in `tools/engine-preview/README.md`;
  - `engine2/data/bank/presets.json`;
  - the wiring rules in `tools/test_table_check.py`, `tools/weakening_check.py` and `tools/tolerance_registry_check.py`.
- **Alternatives rejected:**
  - Reading live conditions from `docs/design/gui3/layout.json`'s `live_when`: it is a GUI design table that departs from PA-8 on purpose (GUI-29, GUI-30). PA-8 is parsed instead, and the predicates are pinned row by row to its text.
  - Declaring `patch_check.py` UNWIRED: `weakening_check` counts any new `UNWIRED:` as an increase that needs the human. The ROADMAP names the file `*_check.py`, so `test_table_check` needs a declaration. Wiring its `--selftest` satisfies both without approving anything.
  - Listing every non-default setting of a bank-based recipe: that is 15 to 40 lines each. Instead every written value gets a line, plus checked "from the base" lines.
- **Verify:** `./verify fast` exit 0 at 060a390f, the first commit (`.harness/last-verify.json`). The follow-up commit, which adds this trace and makes reserved rows read "never in 1.0", gets its own run; see the report.
- **Open questions:**
  - horde-patch/1 cannot carry `ratios` (custom cut rules), blade-envelope routes or glide.
  - PA-8's "every blade row: that blade is on" conflicts with blade 2 following blade 1's FM, spreads or rotation while blade 1 is off.
  - PA-8's `spreadDrift` row omits U > 1.
  - Bank-based levels take B.1's formula without OU-2's loudness move.
  - The Import patch box does not exist yet.
