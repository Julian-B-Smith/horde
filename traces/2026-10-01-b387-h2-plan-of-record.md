# b387-h2-plan-of-record — horde 2's plan of record and its visual map

- **Queue item:** B387, read verbatim from `origin/lead-records-158:ROADMAP.md` (records PR #888), with B275, B302, B327, B330, B331, B376, B379, B381–B396, ADR-186, ADR-187 and `docs/PARKED.md`.
- **The row, verbatim:** "One committed document plus a visual map: every part of horde 2, its governing rows and ADRs, its status, its dependencies and the order of work, reconciled with the human's 2026-09-30 summary and their 2026-10-01 rulings. It becomes the README-level orientation for horde 2."
- **Why:** horde 2's parts were spread over ~90 rows and five ADRs, with no single place that says what 1.0 is, what each part waits on, or which order the labs should be synced in now that the 2026-10-01 rulings reprioritised B331's stages.

## What was built

- `docs/H2-PLAN.md`: 39 parts in a machine-readable parts table (between `parts-table` markers), each part in detail (what, governing rows/ADRs, status, needs-first, open decisions), the human's summary mapped item by item to parts, a PROPOSED order of work (Track A, the C++ spine; Track B, lab sync S0–S7, reconciling B331 with B385/B389/B392/B394), the critical path, "What 1.0 is", post-1.0, parked, 41 open human decisions in one list, 12 inconsistencies, and a dated "Last verified" line.
- `docs/design/h2-plan-map.html`: one node per part, top-to-bottom by dependency layer, colour by status, the critical path (computed, not typed) in pink, inferred edges dashed, 1.0-open parts with a dashed outline. Clicking a node shows its rows READ LIVE from ROADMAP.md (title and latest status phrase), its ADRs, prerequisites, dependents and decisions. Both themes. Self-check behind `?check=1` (5 checks, each with a must-fail control): citations exist in ROADMAP.md/DECISIONS.md; acyclic; references valid; the map's DATA equals the doc's parts table; the critical path is a real chain and the longest.

## Findings (MEASURED)

- Critical path, 12 parts: engine → audit → seams → bend → sub → mix → rack → sluice → fxmorph → history → presets → release. The long pole is the parameter decisions through the seam gate, pitch seam, sources, routing and rack, not the C++ port; Sluice (B328, ball Sluice) is on it via ADR-188. Three of its edges are the plan's inferences (port→bend, sub→mix, mix→rack), drawn dashed.
- No git tag exists in the repository (`git tag -l` and `git ls-remote --tags origin` both empty): the legacy freeze tag is still owed.
- `docs/port/divergences.json` holds 7 entries (D1–D4, M1–M3) while `h2/README.md` rule 4 says "There are no divergences yet".

## Evidence consulted

- ROADMAP rows on `lead-records-158` (b556ded): the brief's list in full. The titles and status phrases of every row from B240 up. Heads and tails (not full text) of B23, B32, B50, B57, B80, B84, B88, B114, B119, B122, B126, B152, B160, B168, B170, B172, B186, B207, B208, B210, B211, B222, B225, B226, B235, B238, B240, B252, B253, B257–B259, B261–B270, B272–B274, B276–B278, B281, B282, B287–B289, B292, B302, B303, B305, B306, B308, B312–B314, B316, B318, B321–B325, B328, B332, B335–B337, B339, B340, B344, B345, B347, B349, B350, B353, B357, B360, B361, B366, B369, B370, B372, B373, B375, B377, B378, B380.
- DECISIONS.md: ADR-186, ADR-187 in full; ADR-096, -106, -188, -189 and the titles of the ADRs cited.
- `docs/PARKED.md`, `h2/README.md`, `docs/port/divergences.json`, `h2/cores/swarm/lift-ledger.json`, CLAUDE.md §Domain, `docs/design/engine-audit.html` (tokens, theme and self-check idioms), `tools/labharness/lab_load_check.mjs`, `tools/gen_lab_index.py`, `git log` for the lab files, `gh pr list`.

## Alternatives rejected

- **Basing the branch on `lead-records-158`** so the cited rows exist: rejected, the brief says `origin/main`, and only the lead writes ROADMAP. The map's check 1 is therefore RED on this branch until #888 merges (8 parts cite only B385–B396), and GREEN against the records branch.
- **Embedding a snapshot of ROADMAP row ids in the map** so check 1 passes anywhere: rejected, a check against its own copy is the detector-shares-assumption trap. It reads the served ROADMAP.md.
- **A left-to-right layout:** 14 layers were ~2600 px wide and half the graph was off-screen; top-to-bottom fits the widest layer (6 parts) beside the panel.
- **Regenerating `docs/design/index.html`:** out of scope; the lead regenerates the navigator.

## Verify

- `node tools/labharness/lab_load_check.mjs docs/design/h2-plan-map.html`: `GREEN — 1 labs loaded, 0 broken, 0 skipped`.
- Map self-check, headless Chrome, `tools/serve_labs.py 8387` on this branch: `SELF-CHECK RED — 4/5 pass, 5/5 controls fire` (check 1: mpe, modmorph, ott, kchorus, tonality, microtuning, granular, arps cite only rows absent from main's ROADMAP.md, which is expected until #888).
- The same page beside `lead-records-158`'s ROADMAP.md and DECISIONS.md (a scratch copy served by a copy of `serve_labs.py` on 8388): `SELF-CHECK GREEN — 5/5 checks pass with their controls firing`.
- Private-name patterns from `.leakcheck-names` run by hand over both files: 0 case-sensitive hits (the gate's mode); 2 case-insensitive hits, both the ordinary word "place". No machine paths.
- `./verify fast`: see the PR (run on the committed hash; `.harness/last-verify.json`).

## Open questions

- The order of work is a PROPOSAL; B331 still awaits the human.
- 1.0 membership of MPE, OTT, the Kuramoto chorus and arps is unruled; the plan leaves them out of the critical path.
- The 12 inconsistencies in `docs/H2-PLAN.md` are the lead's to resolve (several are ROADMAP or charter edits).
