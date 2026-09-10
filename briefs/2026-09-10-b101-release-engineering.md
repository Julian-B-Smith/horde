# Dispatch brief — B101 release engineering

**Provenance.** HYPERSAW lead organ, 2026-09-10, for a scoped subagent with zero
conversation history. Motivating decision: the 1.0 definition of done (ROADMAP
§"1.0 — DEFINITION OF DONE", ratified by the human 2026-09-10) and queue row
**B101**, filed from an external audit whose claims the lead verified against
the tree the same day (trace `traces/2026-09-10-one-point-oh-proposal.md`).

## Acceptance criteria (verbatim from ROADMAP B101)

> **Release engineering** (audit 2026-09-10; verified absent: no LICENSE, no
> CHANGELOG, zero tags, no CI artifacts, no notarization, no sanitizer runs).
> LICENSE is a HUMAN decision (open-source vs source-available — either;
> undecided = all-rights-reserved by default and reviewers notice); CHANGELOG
> from the traces; tagged semver releases with CI-built artifacts — notarized
> macOS installer, Windows binary (build-windows exists; artifacts do not);
> robustness matrix PUBLISHED: ASan/UBSan/TSan runs of the oracles, denormal
> handling verified, 44.1/48/96/192 k × buffers 32–2048 (samplerate_check +
> rtsafety_probe already exist — this is turning them into a table), Reaper +
> Bitwig loads. CI's Windows pluginval step still targets a bundle name that
> no longer exists (B99) — fix rides here.

## Files in scope

- **CREATE** `CHANGELOG.md` — Keep-a-Changelog format, one `## [Unreleased]`
  section derived from `traces/` (newest first, one line per merged change,
  cite the ADR/B-row), plus a `## [0.1.0] — planned` stub the human will cut.
- **CREATE** `docs/LICENSE-OPTIONS.md` — two complete candidate texts the
  human can choose between (a permissive OSS licence and a source-available
  one such as PolyForm Noncommercial or BUSL), with a one-paragraph
  recommendation and what each implies for the reference prototypes and the
  ingested specs. **Do NOT create `LICENSE` itself** — the choice is the
  human's; leave a clearly-marked TODO in `CHANGELOG.md` for it.
- **EDIT** `.github/workflows/ci.yml` — (a) fix the Windows pluginval step's
  bundle name (`horde.vst3`, per ADR-114; verify the actual produced name in
  the build tree rather than trusting this brief); (b) add `upload-artifact`
  for the macOS and Windows plugin bundles; (c) add a **release** job
  triggered on `v*` tags that attaches those artifacts; (d) add a
  **sanitizer** job (Linux or macOS, `-fsanitize=address,undefined`) that
  builds and runs the oracle binaries listed in `./verify fast`. Notarization:
  write the job **skeleton** referencing secrets by name (`APPLE_ID`,
  `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD`) and gate it on their presence — you
  cannot notarize; the human supplies credentials later.
- **EDIT** `CMakeLists.txt` — an `HYPERSAW_SANITIZE` option (OFF by default)
  that adds the sanitizer flags to the host-side executables only. Bit-inert
  when OFF: the shipped build must not change (verify full must stay green).
- **CREATE** `docs/ROBUSTNESS.md` — the published matrix: sample rates
  44.1/48/96/192 k × buffers 32/64/128/256/512/1024/2048, each cell the
  measured result from `samplerate_check` / `rtsafety_probe` / a sanitizer
  run (extend those tools' *invocation*, not their assertions — see gates
  below), denormal handling (state what the code does and how you verified
  it: a probe that feeds a decaying tail and asserts no denormal slowdown or
  the presence of FTZ/DAZ), and a host-load table with Reaper and Bitwig
  rows marked **UNVERIFIED — human** (you cannot load hosts).
- **CREATE** `traces/2026-09-10-b101-release-engineering.md` — what changed,
  evidence, verify result + hash (the provenance skill's shape; see any
  recent file in `traces/`).

**OUT of scope:** `ROADMAP.md` and `DECISIONS.md` (the lead is their only
writer — put anything they should record in your final report); `./verify`
and every tool it runs (`tools/*_check.py`, `tools/*_check.cpp`) — you may
ADD new probes/targets, never edit an existing gate's assertions; `src/**`
except nothing; `reference/**`, `specs/**` (protected); git tags (a human
act — prepare, never tag); `LICENSE` (human); the untracked files at the
repo root (the human's drafts — never add, move or delete them).

## Constraints you inherit

- Build with absolute paths and Unix Makefiles on this Mac:
  `cmake -S . -B build-release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release`
  then `cmake --build "$(pwd)/build-release" -j"$(sysctl -n hw.ncpu)"`. Your
  worktree starts without a build dir — configure fresh (several minutes).
- **No machine identity in tracked files** (no `/Users/...`, no usernames) —
  `./verify fast` runs a leak gate that will catch it.
- **Alias discipline:** this repo is public; never write a private sibling's
  real name into a tracked file (the alias map is untracked `PRIVATE-NOTES.md`).
- The audio thread is untouched by anything here; sanitizer flags apply to
  host-side tools only.
- Run `./verify fast` after every change set and `./verify full` before you
  call the item done. **Report oracle output verbatim** — never summarise a
  failure. A red oracle halts you: fix or revert, do not stack on red.

## Deliverable

Branch `b101-release-engineering` off `main`, pushed, with a PR opened via
`gh pr create --base main` whose body leads with what a reviewer can check
without reading the diff (the matrix, the CHANGELOG head, the CI run link).
**Never merge.** Final report to the lead: the PR URL, `./verify full`'s last
lines verbatim, the CI run status, and a bullet list of what ROADMAP /
DECISIONS should record (the lead folds it in).
