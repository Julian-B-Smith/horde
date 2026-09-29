# tools/patchspace — the composed engine's patch space (B316)

What the SCALPEL × horde composed engine (`docs/design/scalpel-horde-engine.js`) can do,
mapped: which parameters matter when, how a rendered patch measures, and where random
patches break. ROADMAP B316; phases P1–P3 are here, plus the blind listening pass that
calibrates P3's thresholds (B324 v1, B340 v2), B345's metric fixes and re-fit, and B346's
os-convergence aliasing estimator and the aliasing-source harness. P4 (fit bounded
distributions) and P5 (the lab's RANDOM button and FUZZ mode) are not built yet.

Last verified: 2026-09-29 (branch `patchspace-alias-refit`, B351).

| file | what it is | run |
|---|---|---|
| `space.mjs` | Shared: loads the engine (the protected oracle `razor-core.js` and `swarmsaw.html`'s SwarmSynth, both READ, never edited), the lab's parameter table and tapers (sliced from `docs/design/scalpel-interface-lab.html`), seeded rendering, and the condition evaluator. | — |
| `gen_dependency_tree.mjs` | **P1.** Derives which parameters are live under which switches and writes `dependency_tree.json`. Conditions are pinned to the engine's own guard lines, the lab's greying rules are executed, and a seeded perturbation probe confirms both. | `node tools/patchspace/gen_dependency_tree.mjs` (~35 s idle, more under load) |
| `dependency_tree.json` | **DATA. Regenerate it; never hand-edit it.** Each parameter has `active_when` (its role), `side_channels` (other engine paths it is also heard through), `depends_on`, `gates`, evidence (`source` file:line, `probe` counts), plus the predicates, a sampling `order`, and every disagreement found. | — |
| `dependency_tree_check.mjs` | Checks the tree: it must be fresh (byte-compared), have zero structural disagreements, and catch three planted wrong conditions and one vanished anchor. **Wired: `./verify full`.** | `node tools/patchspace/dependency_tree_check.mjs` |
| `metrics.mjs` | **P2.** Pure metrics of a rendered buffer: aliasing, spectral flatness, **noiseDb** (B345), root presence, Sethares roughness, DC, level/crest/LUFS-like, silence, clicks, non-finite, and CPU from timings; and **aliasConvergence** (B346), the os-convergence aliasing estimator, kept beside B345's aliasing until it is proven. They are **measurements, not gates.** Its header states every method, B345's fixes and B346's estimator with its limits. | — |
| `metrics_check.mjs` | Validates every metric on constructed signals, each with a must-read-zero and a must-read-high control, plus B345's rows (aliasing reads the same at any window length; two renders of the same noise are not folding; silence is not measured; noiseDb reads a known noise share), B346's X rows (the estimator: a band-limited saw reads clean, a naive saw folds and converges, independent noise renders read zero, a planted tone reads, a rate-dependent feedback loop reads as dynamics, dense partials over a noise floor read where B345's floor hid them, and a naive saw under vibrato or a glide reads aliased within 3 dB of steady while B345's metric under-reads it) and three engine controls. **Wired: `./verify fast`.** | `node tools/patchspace/metrics_check.mjs` (~8 s) |
| `gauntlet.mjs` | **P3.** Draws seeded patches from the tree in taper space (`broad` = uniform; `edge` = the fuzzer, which favours extremes), renders them, and measures each one. The run is resumable and chunked, and it writes to `local/patchspace/<run>/`, which git ignores. `THRESH` holds the provisional thresholds: they change only by a human ruling. Since B346 each patch also carries the os-convergence fields (`aliasConvDb`, `aliasConvClass`, …) and the output stage (`decimLeakDb`, `tanhFoldDb`); since B351 THRESH's `alias` gate reads `aliasConvDb` (`aliasDb` is a measured column), and `convLeg` is the estimator leg on its own. Since B360, `failures()` (aliasing) and `incoherence()` (root absent) are the only gates; `labels()` tags `noisy`/`noiseDb` and `rough`/`roughness` on the record without reducing healthy or coherent yield. | `node tools/patchspace/gauntlet.mjs estimate`, then `… run --run NAME --n 2000 --edge 1000` |
| `gauntlet_report.mjs` | Writes the committed markdown summary of a run: yields, distributions, hotspots, key pairs, and the Crushed-bells class. | `node tools/patchspace/gauntlet_report.mjs --run NAME --out docs/patchspace/FILE.md` |
| `listening_sample.mjs` | **B324.** Chooses the blind listening pass's patches from a gauntlet run (stratified around each threshold in THRESH by `strataFor`, so a new draw follows the ruled gates, with clean and broken controls and two repeats, seeded) and writes `docs/design/listening-pass.json`: seeds, measured metrics, a patch hash and a render fingerprint only. `--remeasure` (B345) rewrites the SAME items' numbers in place after a metrics change and records the replaced id in `remeasured`. Also the one loader of the page's PURE block (`loadPage`). | `node tools/patchspace/listening_sample.mjs --run lp324 --exclude …`; `… --remeasure --note "why"` |
| `calibrate.mjs` | **B324 / B340 / B345.** Fits each metric's threshold to the human's ratings (ROC best cut, AUC, logistic 50% point) and prints a PROPOSAL; never applies it. v1 exports fit the committed numbers; v2 exports fit the worst heard segment. `--remeasure` re-renders every v2-rated program in Node from its seed and fits the fixed metrics (noiseDb beside flatness, leave-one-out accuracy per metric, Chrome-vs-Node drift named); `--conv` (B351) adds B346's estimator per segment (aliasConvDb, aliasTotalDb) and fits the aliasing answers on all three; the chaotic broad#383 is left out (`CHAOTIC`). Browser-importable: the listening page imports its validator and fit. | `node tools/patchspace/calibrate.mjs [--remeasure]` (reads `local/patchspace/ratings/*.json`; `--remeasure` takes ~4 min, `--remeasure --conv` ~45-60 min) |
| `listening_pass_check.mjs` | Proves the page's sampler and render route are the gauntlet's, every seed re-measures to the committed numbers, the views before the reveal are blind, the human's v1 pass still exports byte for byte, the exports pass the validator, and calibrate's re-measure replaces segments and names drift (20 must-fail controls; B351: every committed stratum label is reproduced by `strataFor`, and T10 reads the saws with the aliasing gate's estimator). **Wired: `./verify full`.** | `node tools/patchspace/listening_pass_check.mjs` (~60 s) |
| `fidelity.mjs`, `fidelity_audit.mjs`, `fidelity_scan_check.mjs` | **B325.** The composed engine against the oracle: the audit report (`docs/patchspace/2026-09-28-fidelity-audit.md`) and the wired scan. Its aliasing section was regenerated by B346 (`fidelity_audit.mjs alias`) with the fixed metric and the estimator. | see each header |
| `alias_sources.mjs` | **B346.** Where the aliasing comes from and what cures it: renders a patch at 1×..16× its oversampling with the internal stream captured (a subclass reads `bqf`; no engine file is edited, and where a subclass cannot reach, a text patch of the oracle is evaluated in memory, B325's method), runs the estimator, splits off the output stage (decimator leak, output-rate tanh) against an ideal decimator and an oversampled tanh, and compares the whole against an oversampled truth. Passes: `matrix` (every rated patch × A1/E5/A5 × one mechanism toggled at a time), `fixes` (candidate cures, each judged against the engine-as-is truth), `cpu`, `b828`, `ulp`. UNWIRED: an investigation run (its distillate is metrics_check's X rows). | `node tools/patchspace/alias_sources.mjs matrix` (~3 min on 5 workers), then `fixes`, `cpu` |

The first gauntlet report is `docs/patchspace/2026-09-27-gauntlet-p3.md`, regenerated by B346 on the same
seeds with B345's fixed aliasing and B346's estimator beside it.

**The listening pass (v2, B340).** Serve the repo (`python3 tools/serve_labs.py`), open
`docs/design/listening-pass.html`, rate the 36 patches (about 19 minutes: six notes from A1 to A5
plus a sweep through the engine's bend, with sine and clean-saw references), export, save the
file into `local/patchspace/ratings/`, then run `calibrate.mjs`. The page measures every segment
it plays with `metrics.mjs` and the v2 export carries those numbers; the fit uses the worst
segment heard. The browser's sin/exp/tanh differ from Node's in the last bits, so a chaotic
patch can measure differently in the browser (`heard: drift`, informational in v2). `?xverify=1`
runs the cross-runtime comparison over the whole sample. The human's v1 pass stays exportable
from the landing view.

**After a metrics change (B345).** `listening_sample.mjs --remeasure --note "…"` re-measures the
committed sample in place (same patches, seeds and order; a new id, the old one listed in
`remeasured`, so the page and calibrate know an export on it rated the same sounds). Then
`calibrate.mjs --remeasure` re-fits the human's answers on the fixed metrics without anyone
listening again. Nothing is applied: `THRESH` changes only by a human ruling.

## How the tree feeds later work

- **B275 manifests.** `active_when` is the manifest's "active when" field in embryo.
- **Morph.** A parameter that is inactive at both corners needs no interpolation, and an
  inactive lane is drawn grey.
- **The mod matrix.** A destination is eligible if its `active_when` can be satisfied
  (`cScale` never can). A modulated *gate* parameter can switch other parameters'
  audibility on and off.

The JSON's `consumers` field states these rules in full.
