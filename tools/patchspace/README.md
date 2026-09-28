# tools/patchspace — the composed engine's patch space (B316)

What the SCALPEL × horde composed engine (`docs/design/scalpel-horde-engine.js`) can do,
mapped: which parameters matter when, how a rendered patch measures, and where random
patches break. ROADMAP B316; phases P1–P3 are here, P4 (fit bounded distributions) and P5
(the lab's RANDOM button and FUZZ mode) are not built yet.

Last verified: 2026-09-27 (branch `patchspace-p1-p3`).

| file | what it is | run |
|---|---|---|
| `space.mjs` | Shared: loads the engine (the protected oracle `razor-core.js` and `swarmsaw.html`'s SwarmSynth, both READ, never edited), the lab's parameter table and tapers (sliced from `docs/design/scalpel-interface-lab.html`), seeded rendering, and the condition evaluator. | — |
| `gen_dependency_tree.mjs` | **P1.** Derives which parameters are live under which switches and writes `dependency_tree.json`. Conditions are pinned to the engine's own guard lines, the lab's greying rules are executed, and a seeded perturbation probe confirms both. | `node tools/patchspace/gen_dependency_tree.mjs` (~35 s idle, more under load) |
| `dependency_tree.json` | **DATA. Regenerate it; never hand-edit it.** Each parameter has `active_when` (its role), `side_channels` (other engine paths it is also heard through), `depends_on`, `gates`, evidence (`source` file:line, `probe` counts), plus the predicates, a sampling `order`, and every disagreement found. | — |
| `dependency_tree_check.mjs` | Checks the tree: it must be fresh (byte-compared), have zero structural disagreements, and catch three planted wrong conditions and one vanished anchor. **Wired: `./verify full`.** | `node tools/patchspace/dependency_tree_check.mjs` |
| `metrics.mjs` | **P2.** Pure metrics of a rendered buffer: aliasing, spectral flatness, root presence, Sethares roughness, DC, level/crest/LUFS-like, silence, clicks, non-finite, and CPU from timings. They are **measurements, not gates.** | — |
| `metrics_check.mjs` | Validates every metric on constructed signals, each with a must-read-zero and a must-read-high control, and adds three engine controls. **Wired: `./verify fast`.** | `node tools/patchspace/metrics_check.mjs` (~4 s) |
| `gauntlet.mjs` | **P3.** Draws seeded patches from the tree in taper space (`broad` = uniform; `edge` = the fuzzer, which favours extremes), renders them, and measures each one. The run is resumable and chunked, and it writes to `local/patchspace/<run>/`, which git ignores. | `node tools/patchspace/gauntlet.mjs estimate`, then `… run --run NAME --n 2000 --edge 1000` |
| `gauntlet_report.mjs` | Writes the committed markdown summary of a run: yields, distributions, hotspots, key pairs, and the Crushed-bells class. | `node tools/patchspace/gauntlet_report.mjs --run NAME --out docs/patchspace/FILE.md` |

The first report is `docs/patchspace/2026-09-27-gauntlet-p3.md`.

## How the tree feeds later work

- **B275 manifests.** `active_when` is the manifest's "active when" field in embryo.
- **Morph.** A parameter that is inactive at both corners needs no interpolation, and an
  inactive lane is drawn grey.
- **The mod matrix.** A destination is eligible if its `active_when` can be satisfied
  (`cScale` never can). A modulated *gate* parameter can switch other parameters'
  audibility on and off.

The JSON's `consumers` field states these rules in full.
