# b446-w3c-harness-sandbox — every harness that evaluates lab text runs under the permission model; the Sluice lab gets an off-by-default opt-in

- **Queue item:** ROADMAP B446 / ADR-194, P1 Wave 3, item W3c. Brief from the lead session,
  2026-10-08 ("start Wave 3c. Sluice lab ratified.").
- **Why:** W3b sandboxed the six lab-loading harnesses and named the rest as residue: harnesses that
  evaluate lab or prototype text with `new Function` or `require` run in the host realm with the
  human's full filesystem and process rights. The Sluice lab (B329) also lost its spec link when W3b
  stopped serving `local/`; the human ratified a narrow, explicit opt-in.

## What changed

### 1. The remaining harnesses (found by grep over `tools/`, then by following imports)

Every file that evaluates lab text now has `import '<path>/sandbox_guard.mjs'` as its FIRST import
(30 files, on top of W3b's 6). A bare `node tools/x.mjs` re-runs the ENTRY through
`sandboxed_node.mjs`, so the `./verify` and C++ command lines are unchanged (`test_table_check` and
`playbook_check` read them literally), and an entry with no `PROFILES` row is refused.

| Family | Files | Sandbox profile (all: no child process, no addons) |
|---|---|---|
| Golden generators | `tools/golden/gen_*_goldens.mjs` (12 entries), `extract_core/force/glide/intent.mjs` (the evaluators they import) | read `tools/golden` + the lab trees; write ONLY `build-golden/<own dir>/`, and nothing under `--selfcheck` (`gen_goldens.mjs`: `build-golden/` itself, its files sit at the top) |
| Fidelity/oracle checks | `station_check`, `subosc_check`, `reverb_check`, `filter_fidelity_check`, `composed_engine_check`, `divergence_ledger_check` | read only; `composed_engine_check` also `h2/cores/swarm` + `tools/patchspace`; the ledger check also the evidence trees it `existsSync`es (`traces`, `docs/port`, ...) |
| Legacy-preset porter | `tools/port_legacy_presets.mjs`, `port_legacy_presets_check.mjs` | check: read only. CLI: read `--store` (default the legacy store under `~/`), write `--out` (default `local/legacy-presets/`) |
| h2 parity renderers | `tools/h2_engine_render.mjs`, `h2_scalpel_render.mjs`, `h2_scenarios.mjs` | read only, workers allowed; stream to stdout (C++ checks `popen` them unchanged) |
| Patch-space | checks `metrics_check`, `fidelity_scan_check`, `listening_pass_check`, `dependency_tree_check`; tools `gen_dependency_tree`, `gauntlet`, `gauntlet_report`, `fidelity_audit`, `alias_sources`, `antialias_scope`, `listening_sample`, `calibrate`, `os_quality`; evaluators `space`, `fidelity`, `alias_sources`, `listening_sample`, `gen_dependency_tree` | checks read only (`dependency_tree_check` also workers). Tools write only their own output: `local/patchspace/...`, one named file under a flag (`--write`, not `--stdout`), `--out` |
| Manual lab tools | `glide_roundup`, `modlab_sweep_report` (each writes exactly one report file), `modlab_probe/reach/sweep`, `feedback_scan` | read only, plus the one report file |

48 `PROFILES` rows now (W3b had 7). Rows can name writes per mode (`{ path, unless: '--selfcheck' }`),
path-valued flags (`readArgs`, `writeArgs`, `writeDirArgs`) and directory trees (trailing `/`).

**Two child processes the sandbox cannot grant.** `port_legacy_presets` ran `python3
tools/registry_decl.py` and `git check-ignore`; `os_quality` ran `git check-ignore`. A child-process
grant is a grant of everything, so the launcher runs those two commands itself, outside, and hands
the answers down in the environment (`facts`; new `tools/labharness/sandbox_facts.mjs`, imported by
both). Unsandboxed (no env) the module runs the command as before; a throw reads as "not ignored",
which is the refusing side of the privacy rule.

**W3b regression fixed on the way.** `fxmorph_check.mjs --sweep/--merge/--write` write, and the W3b
row granted no write at all (its comment said "human-run unsandboxed", but the guard sandboxes every
bare run). They now get `--out`, and the gallery file under `--write`. Run once each; the committed
gallery was restored afterwards.

### 2. The Sluice opt-in (off by default)

- `python3 tools/serve_labs.py [--allow-sluice] [port] [root]`. With the flag the server also serves
  `local/sluice/**`, nothing else of `local/`. Rules under the flag: `local` must be a real directory;
  the one link, `local/sluice`, is followed; nothing BELOW it may be a symlink (a link planted inside
  the subtree is refused); Host check, dot-file rule, no listings and every other tree's no-symlink
  rule are unchanged. One handler subclass per server, so the flag cannot leak between servers.
- `tools/labs_preview.sh [--allow-sluice] [branch ...]`. With the flag, after the merges, it removes
  whatever `local/sluice` the merged tree holds (a branch can force-add a symlink anywhere), links the
  preview's `local/sluice` to the MAIN checkout's, and prints the serve command with `--allow-sluice`.
  No main-checkout link: nothing is linked, the command stays plain, and it says why. Without the
  flag nothing differs from W3b.
- Usage: `tools/labs_preview.sh --allow-sluice`, then the printed
  `python3 <main>/tools/serve_labs.py --allow-sluice 8146 <main>/.claude/worktrees/labs-preview`.

## Control results (each red before, green after)

| Control | Without the sandbox / flag | With it |
|---|---|---|
| Planted payload in a lab file inside each family (golden generator via `reference/subosc.html`; fidelity check, same file; patch-space check, renderer and porter check via `razor-core.js`; manual report tool via `bend-lab.html`): WRITE a file, SPAWN a process; generators and the report tool also write BESIDE their granted target | every payload produces its marker with `--permission --allow-fs-read=* --allow-fs-write=* --allow-child-process --allow-worker` (asserted per row) | each row exits non-zero with `ERR_ACCESS_DENIED`, no marker |
| An unguarded evaluator under `tools/`; a guard that is not the first import; a node entry in `./verify` or a C++ check that reaches an evaluator with no `PROFILES` row | scan flags each (asserted on planted text) | none in the tree |
| `local/sluice` | 404 (default server), 200 on the stock handler | 200 only under the flag, and the bytes served are the spec's |
| `local/` elsewhere, `PRIVATE-NOTES.md`, `.git`, the link target by its own name (`local/sluice-real`), a prefix (`local/sluicex`), `..` and encoded dots, wrong Host | 404 / 403 | 404 / 403 under the flag |
| A file symlink and a directory symlink planted INSIDE `local/sluice` pointing out | stock handler answers 200 (asserted) | 404 |
| `local` itself a symlink | not followed | 404 even under the flag |
| A branch that force-adds `local/sluice` -> elsewhere | the link arrives in the merged tree (asserted) | the flag run re-points it at the main checkout's |

Mutation checks (each turned the named check red, then restored): adding `--allow-child-process` to
the launcher (every spawn row, all six families); adding `--allow-fs-write=*` (every write row);
widening a generator's grant to `build-golden/` (the "beside its own target" row); deleting a guard
import (coverage scan); deleting a `PROFILES` row (wiring scan); dropping the in-subtree symlink
test, the `local`-is-real test, or the `allow_sluice` condition in the server (three different rows);
dropping the launcher's pre-link removal in `labs_preview.sh`.

**Output unchanged.** Every harness above was run under the sandbox and against an untouched copy of
`origin/main` (`git archive`): stdout identical after masking timings for all read-only checks;
`build-golden/**` (718 files) byte-identical; the h2 scalpel and engine streams byte-identical
(`shasum` of the full stream, 304ceac7... and 4f550034...); `gen_dependency_tree --stdout` equals the
committed `dependency_tree.json`. The only baseline difference was the porter check's privacy row,
red in the copy only because `git archive` has no `.git`.

## Evidence consulted

`traces/2026-10-07-b446-w3b-devtools.md` (residue list), `tools/labharness/sandbox*.mjs`, every file
named above, `verify` (command lines), `tools/*.cpp` (`popen` of the h2 renderers), `tools/serve_labs*.py`,
`tools/labs_preview*.{sh,py}`, `docs/design/sluice-horde-lab.html` (the `local/sluice` links), Node 24.10
permission-model behaviour (tested, see below).

## Alternatives rejected

- Wrapping each `./verify` line with the launcher: the playbook and wiring checks read those lines
  literally; the guard import is the W3b answer and stays.
- A guard only on entries: the evaluators are the shared libraries (`extract_*`, `space.mjs`), so
  guarding them covers every importer, including a new one, and "every evaluator is guarded" is a
  scan a check can enforce. Entries still need rows, which a second scan enforces.
- Granting child processes for the two `git`/`python3` calls: it grants everything.
- A `--allow local/sluice` generic flag (W3b's open question): the opt-in is one named subtree.

## Node permission-model facts learned (not in the Node docs; reproduced on 24.10)

- `--allow-fs-write=<dir>` is a tree only if `<dir>` EXISTS when the flag is parsed; otherwise it is
  one exact path. `<dir>/*` covers children of a directory that does not exist yet but NOT a `mkdir` of
  `<dir>` itself. The pair covers both (a fresh clone has no `build-golden/`). The launcher does this
  for any path ending `/`.
- A file grant listed BEFORE a grant on its own directory makes Node refuse `mkdir` of that directory
  (reproduced on 24.10). Output-file flags therefore grant the file's directory, not the file.
- `fs.existsSync` on an ungranted path THROWS, so a check that tests for evidence files needs read grants.

## Known residue

- Network, symlinks inside a granted tree and the CI node version: unchanged from W3b.
- Manual tools that write get a grant for the output they were written to produce, not for whatever a
  future edit makes them write; a new write fails with the permission error naming the path and wants a
  row. `calibrate.mjs --remeasure` and `listening_sample.mjs --remeasure` write nothing I could find
  beyond `docs/design/listening-pass.json` (granted); I ran neither to completion (4 to 45 minutes).
- `gauntlet_report --out F` and `os_quality --out D` grant a directory tree, not a single file.
- `.claude/launch.json` (name "labs") is not changed (out of scope): to use the opt-in from the
  preview panel, add `--allow-sluice` before the port in its `runtimeArgs`.

## Verify

Both run on the tree before the commit, base `08eed62`:
- `./verify fast`: `{"target":"fast","exit":0,"git":"08eed62","ts":"2026-10-08T02:09:52Z"}`
- `./verify full`: `{"target":"full","exit":0,"git":"08eed62","ts":"2026-10-08T02:21:52Z"}`

`sandbox_check` costs about 9 s in `verify fast` (was about 2 s): six families times three payloads, each with its control.

## Open questions

- Should `.claude/launch.json` carry `--allow-sluice`, or stay a per-run flag? (The brief made it per-run.)
- CI's node version is still unverified (W3b's question). The new plant rows are skipped, loudly,
  without `process.getBuiltinModule`; the coverage scan runs on any node.
