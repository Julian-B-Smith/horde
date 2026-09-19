# docs-ci-leg-cpubench-stamp — CI gates docs-only changes; cpu_bench prints its commit

- **Queue item:** unqueued: repo audit `docs/audits/2026-09-19-repo-audit.md`
  findings H5 and H4, both ruled by the human on 2026-09-19 ("go ahead with the
  docs-only fix"; on H4 the lead chose to close the class rather than label it).

- **Why:** Two invisible-by-construction failures, one commit each.
  **H5** — `.github/workflows/ci.yml:31-36` carries
  `paths-ignore: ['**.md','docs/**','traces/**']` on both triggers, and GitHub
  applies a path filter per *workflow*, not per job, so a docs-only PR ran zero
  jobs. Two `verify fast` gates read exactly those files and nothing else:
  `leak_gate` (`verify:78`) and the INDEX/LIBRARY pointer heredoc
  (`verify:146-152`); the structure check (`verify:122-128`) takes only `.md`
  inputs. The doctrine's "never commit machine identity" names `leak_gate` as
  its enforcement, so on this repo CI enforced it only for non-docs PRs. New
  `docs.yml` is the inverse filter over the same three globs running ci.yml's
  ubuntu `verify-fast` job verbatim — the 1x runner only, never the 10x macOS
  or Windows jobs, so ci.yml's cost model is untouched and ci.yml itself is not
  edited.
  **H4** — `dist/cpu_bench` is a committed universal Mach-O handed to another
  Mac while the core it measures moves under it (31 commits touched
  `src/swarm_core.h` since the 2026-08-06 build). It printed a number and not a
  hash, so its staleness was undetectable by its own reader. It now prints
  `cpu_bench: build <stamp>` as its first line, and the README carries the
  exact rebuild command.

- **Evidence consulted:**
  - `docs/audits/2026-09-19-repo-audit.md` §H4 (l.160-194), §H5 (l.195-240).
  - `.github/workflows/ci.yml:1-48` — the cost-model header, both `paths-ignore`
    triggers, the `concurrency` idiom, and the `verify-fast` job (plain
    `actions/checkout@v4`, **no** `submodules: recursive` — unlike the
    `sanitize` job at l.62-64).
  - `verify:70-172` — the whole `fast()` body: python gates, shell gates,
    markdown structure list, two heredocs. Nothing compiles, nothing reads
    `libs/`, which is what makes the submodule-less checkout correct to mirror.
  - `CMakeLists.txt:78-103` (`HS_GIT_HASH`, `HYPERSAW_BUILD_ID`),
    `CMakeLists.txt:540-550` (measure_cpu/measure_alias print
    `HYPERSAW_BUILD_STAMP`, "so a table names the code that produced it"),
    `tools/build_stamp.cmake` (the generated header), `tools/measure_cpu.cpp:78`
    and `:200-201` (the include and the print idiom being reused).
  - `dist/README-cpu-bench.md` as it stood (the H4 blockquote this replaces).

- **Alternatives rejected:**
  - *Narrow ci.yml's `paths-ignore` instead of adding a workflow* — one line
    rather than twelve, but it re-enables the 10x macOS job on every ROADMAP
    edit, which is the bill the frugal config exists to avoid
    (`ci.yml:1-13`). The audit says explicitly "do not do that."
  - *Include `build_stamp.h` in `cpu_bench.cpp` the way `measure_cpu.cpp` does*
    — that header is a CMake byproduct and `cpu_bench` is built by a bare
    two-arch `clang++` outside CMake, so the include would not resolve. The
    `-D` + `#ifndef "unstamped"` default keeps the bare build compiling and
    makes an unstamped binary say so rather than lie.
  - *Only document the build command (the audit's cheaper option a-c)* — labels
    the staleness in a file the tester on the other Mac does not have. The lead
    chose to close the class.
  - *Touch the reference-numbers table* — not done. Those figures were measured
    2026-08-06 on the older engine; the README now says so instead of
    implying a re-measurement that did not happen.

- **Verify:** `./verify fast` exit 0 on the working tree before each commit;
  `./verify full` exit 0 on the committed hash — see the run recorded below and
  `.harness/last-verify.json`. Commits: `adcc5f6` (docs.yml), `6585db9`
  (cpu_bench.cpp), and the binary/README commit that stamps `6585db9`.

  Build (run from the repo root, the command the README now documents):

  ```
  clang++ -std=c++20 -O3 -arch arm64 -arch x86_64 \
    -DHYPERSAW_BUILD_STAMP='"6585db9"' -I src tools/cpu_bench.cpp -o dist/cpu_bench
  codesign --force -s - dist/cpu_bench     # -> "replacing existing signature", exit 0
  codesign --verify --verbose dist/cpu_bench
  #   dist/cpu_bench: valid on disk
  #   dist/cpu_bench: satisfies its Designated Requirement
  file dist/cpu_bench
  #   Mach-O universal binary with 2 architectures: [x86_64] [arm64]
  ```

  `./dist/cpu_bench` (defaults, this dev Mac), verbatim:

  ```
  cpu_bench: build 6585db9
  cpu_bench: 7 voices x 8 notes = 56 oscillators
    audio rendered   8.00 s
    cpu consumed     0.140 s
    % of one core    1.75%   (E-6 budget 50%, 57.0x realtime)
  ```

  YAML validation: `python3 -c "import yaml; yaml.safe_load(open(...))"` parsed
  `docs.yml` to the expected mapping — `name: docs`, the two triggers each with
  `paths: ['**.md','docs/**','traces/**']`, `concurrency.group:
  docs-${{ github.ref }}`, one `verify-fast` job. PyYAML renders the `on:` key
  as boolean `True` (YAML 1.1), exactly as it does for `ci.yml` — a parser
  quirk of the local check, not of the file; GitHub reads `on` correctly.

- **Open questions:**
  1. **The binary grew 186 KB -> 2.80 MB and nobody asked it to.** `size -m`
     attributes essentially all of it to a new `__data` section: 0 bytes in the
     2026-08-06 build, 1 310 808 bytes now (`__text` moved only 31.9 KB ->
     42.3 KB). That is a ~1.3 MB non-const global table that `src/swarm_core.h`
     acquired over those 31 commits and that every `dist/` copy, and the
     plugin, now carries. Unexamined here — out of scope (the brief bars
     anything about what the bench measures), and flagged rather than guessed
     at.
  2. **The new numbers differ from the committed reference table** (1.75% vs
     1.60% for `7 8 8`). One run on a machine whose load I did not control is
     not a re-measurement, so the table is left alone and labelled historical.
     Whether to re-measure and on which hardware is the lead's call.
  3. `docs.yml` has never run. Its first execution is this PR (it touches
     `traces/**` and `**.md`), so the PR's own checks are the first evidence
     the workflow is well-formed to GitHub as well as to PyYAML.
  4. This branch carries **three** commits, not the two the brief named: the
     README's "built from commit" line cannot name the commit that contains
     it, so the source change was committed alone, the binary built at that
     hash, and README + binary committed together naming it. No placeholder
     hash was ever committed.
