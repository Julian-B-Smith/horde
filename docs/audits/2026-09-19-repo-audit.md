# Repo audit — 2026-09-19

**Commit audited** `5a496c1f18ae37f6226d2990f233f2811467d36b` (`origin/main`, PR #668 merged).
**Cause** The lead's call — the auditor's first dispatch (B159), at the human's request
the same day ("set up an auditor for this repo that routinely scans through the files
searching for optimizations and things to clean up or consolidate").
**Scope** The REPO, not the engines. The three 2026-09-18 engine/lab audits
(`docs/audits/2026-09-18-{saw-engine,reverb-lab,station-lab}-audit.md`) are on the record;
their open items are ROADMAP rows B147–B156 and are not re-derived here.
**Oracle** `./verify fast` GREEN at `5a496c1` on this branch (target fast, exit 0).
**No "since last audit" section** — this is the first repo-wide sweep.

Every finding is a **proposal for the lead**. Nothing outside this file was edited.
Nothing here proposes weakening a gate. Where a minimal delta touches a protected
path (`./verify`, `specs/*`, `reference/*`), it is labelled as a sanction the human
would have to give.

---

## CRITICAL

**None.** No finding rises to a defect shipping wrong audio, a violated invariant in
the product, or an irreversible loss. The hazards below are latent (H2), uncovered
(H1, H3), or about artefacts and docs rather than the audio path.

---

## HIGH

### H1 — Two regression gates for two defects that actually shipped are built and never run

**Claim.** `ncap_check` and `tseed_check` were written on 2026-09-18 against two
measured defects. Both are registered in CMake, both are green, neither is in `./verify`.

**Evidence.**
- `tools/ncap_check.cpp:4-14` — before B148, `setParam("n",33)` wrote one double past
  `x[32]` "straight into `panL[0]`" and rendered the resulting corruption SILENTLY,
  and `n >= 40` took SIGSEGV inside `finishRebuild()`. Fix landed at
  `src/swarm_core.h:1345-1355` (`voiceCount()`).
- `tools/tseed_check.cpp:5-20` — until 2026-09-18 the ensemble-timing stream was
  `uint32_t tRng = 12345`; measured `seed 1234 vs seed 999999 -> RMS diff 0.000e+00`
  (the seed knob was inert) and `same seed, 5 earlier notes vs none -> 1.365e-01`
  against a control of `1.185e-01` (a restored session rendered a different phrase).
  Fix landed at `src/swarm_core.h:507,514,526`.
- `CMakeLists.txt:352, 362` — both registered. `verify:205-227` — the ADR-171
  ratified block; neither appears. `verify` names 41 binaries; these are not among them.
- Both files declare their own status honestly (`tools/ncap_check.cpp:39`,
  `tools/tseed_check.cpp:62`: "wiring a gate is the human's decision").

**Criterion violated.** CLAUDE.md §Domain invariant 1: *"Same seed + note order ->
identical output. mulberry32 streams only."* `tseed_check` is the only thing in the
tree that measures that invariant on the ensemble stream, and it does not run.
Charter sweep item 1 (correctness the oracles cannot see).

**Why this is structural, not a one-off.** ADR-171 ("Gates ratified", 2026-09-16)
wired 15 checks by one human act. These two were written two days later. The
default for a new check is *unwired*, so the ungated set grows monotonically
between ratifications: **13 `_check` binaries are ungated today** (`combguard`,
`cpu`, `delay`, `fxxfade_plugin`, `intent`, `mixer`, `mod`, `ncap`, `paramclass`,
`strata`, `svf`, `tseed`, `voicetap`) against the README's stated five
(`README.md:460`).

**Minimal delta.** Two lines in `./verify` beside the ADR-171 block —
`"$build_dir/ncap_check" || return 1` and `"$build_dir/tseed_check" || return 1`.
Both are core-direct and sub-second. **`./verify` is a protected path: this is a
sanction the human would have to give**, in the same shape as ADR-171.

---

### H2 — 44 `extern "C"` debug exports, 70 hand re-declarations, no header

**Claim.** The shell exports 44 `hypersaw_debug_*` symbols. There is no declaration
header. Thirteen tools re-declare them by hand — 70 declaration lines — and
`extern "C"` suppresses name mangling, so a signature that drifts **links cleanly
and reads garbage at runtime**.

**Evidence.**
- `src/hypersaw_clap.cpp:7048-7716` — 44 `extern "C" ... hypersaw_debug_*`
  definitions (`grep -c '^extern "C" .*hypersaw_debug_'` -> 44).
- `src/hypersaw_clap_entry.h` (55 lines) contains **zero** `hypersaw_debug_`
  declarations.
- 13 tools re-declare, 70 lines total: `tools/intent_check.cpp:560-578` (19 symbols),
  `tools/routing_check.cpp` (7), `tools/state_check.cpp` (6), `tools/bank_check.cpp` (6),
  `tools/morphlayout_check.cpp` (6), `tools/preset_probe.cpp` (6),
  `tools/gen_factory_bank.cpp` (5), `tools/corner_probe.cpp` (5), `tools/penv_check.cpp` (4),
  `tools/undo_check.cpp` (2), `tools/paramclass_check.cpp` (2), `tools/anchor_check.cpp` (2),
  `tools/polarity_check.cpp` (1), `tools/ncap_check.cpp` (1).

**Criterion violated.** Charter sweep item 2 (second copies) and item 1: this is a
failure no oracle can see, because the oracle is the thing holding the wrong
prototype. The repo already names the shape at `verify:352`: *"a per-voice getter
would test the accessor, which is how state_check once agreed with itself through a
broken one."*

**Minimal delta.** `src/hypersaw_debug.h` — 44 declarations — included by
`src/hypersaw_clap.cpp` (so the compiler checks the definitions against it) and by
the 13 tools. Removes 70 lines, adds ~46 + 13 includes = 59. **Net -11 lines**, and
the class converts from silent UB to a compile error.

**On the brief's question — would a single table-driven query displace more than it
adds?** No. One `hypersaw_debug_query(const char *verb, ...)` replaces 44 typed
signatures with one stringly-typed one: it deletes the compile-time checking that is
the entire value of the fix, forces per-call parsing and marshalling at 14 call
sites, and turns a rename into a runtime miss instead of a build failure. It fails
the reduction test. The header is the right delta.

**Separately (cost, not correctness):** those 668 lines and 44 symbols carry **no
preprocessor guard** (`src/hypersaw_clap.cpp:7048` onward; no `HYPERSAW_DEBUG` in
`CMakeLists.txt`), so all of it compiles into the shipping
`horde.vst3` / `.clap` / `.component`. That is a separate decision from the header
and should not be bundled with it.

---

### H3 — The external-audio path and the second CLAP shell have zero oracle coverage, and a comment says otherwise

**Claim.** `src/swarmfx_clap.cpp` is a complete second CLAP plugin (439 lines, its own
factory, its own 16-param surface, its own bundle `com.lifted-truck.swarmfx`). It and
the three `processExternal*` implementations it depends on are compiled by
`./verify full` and validated by nothing.

**Evidence.**
- `grep -rln processExternal tools/` -> **empty**. No oracle references it.
- Implementations: `src/filter_core.h:212`, `src/time_core.h:240`
  (`processExternalStereo`), `src/time_core.h:330`. `filter_check` / `notch_check` /
  `time_check` drive `render()` / `processSample`, never these.
- `src/swarmfx_clap.cpp:250,252,254` — the plugin's entire audio path is these calls.
- **Two sources disagree**, which is itself a finding: `src/time_core.h:237` says
  *"A deliberate divergence from the mono reference (which the parity oracle still
  guards via processExternal/render)"*. The parity oracle does not touch
  `processExternal`.
- The param surface has **already diverged and is known to have diverged**:
  `INTEGRATION-STANDBY.md:84-89` recorded on 2026-08-10 that swarmfx's `ParamDef` has
  7 fields vs 8, no `coreKey`, and positional dispatch. Still true 40 days on —
  `src/swarmfx_clap.cpp:51-58` and `:107,121,178` (`indexOf`).
- Every repo-wide gate reads **only** `src/hypersaw_clap.cpp`:
  `tools/gui_reach.py:26`, `tools/presentation_check.py`, `tools/test_table_check.py`,
  `tools/paramclass_check.cpp`. swarmfx's 16 params are not absent-and-flagged; they
  are invisible.
- CI builds it (`cmake --build` builds all targets) but `pluginval` validates only
  `horde.vst3` (`.github/workflows/ci.yml`, both platform legs).

**Criterion violated.** Charter sweep item 1. Also CLAUDE.md §Domain: *"C++
correctness is defined as parity with the JS reference ... never as plausible-sounding
audio."* The swarmfx path has neither parity nor an invariant oracle.

**Minimal delta, two tiers.**
1. **One word, today:** strike `processExternal/` from `src/time_core.h:237` so the
   comment stops claiming coverage that does not exist. Cost: 1 word. This is the
   whole of the "a comment that restates code, wrongly" class here.
2. **The real fix, as a ROADMAP row:** either declare swarmfx PARKED in
   CLAUDE.md §Domain and `docs/PARKED.md` (it is in neither today), or give it a
   `swarmfx_check` in the `notchslot_check` idiom — silence in / silence out, a
   non-zero engine measurably changing the spectrum, with a must-read-nothing
   control. Estimated ~150 lines. **Do not do both.** The lead should ask which,
   because "a second shell nobody parked and nobody tests" is the worst of the two.

---

### H4 — `dist/cpu_bench` is a committed binary, 44 days stale, and not reproducible from this repo

**Claim.** The repo ships a 186 KB universal Mach-O that a human is instructed to hand
to another Mac to measure this synth's CPU cost. It was built from a core that is
31 commits out of date, and no command in the repo can rebuild it.

**Evidence.**
- `dist/cpu_bench` — `Mach-O universal binary with 2 architectures [x86_64][arm64]`,
  186 048 bytes. Committed `74df8dd`, **2026-08-06** (44 days ago).
- 31 commits have touched `src/swarm_core.h` since that commit — the exact core it
  measures.
- `grep -n "OSX_ARCHITECTURES\|lipo" CMakeLists.txt` -> **no hits**. CMake builds
  host-arch only; the committed artefact is a manual two-build `lipo` with no recorded
  command. `dist/README-cpu-bench.md` gives run instructions and reference numbers but
  **no build instructions**.
- `dist/README-cpu-bench.md` presents `1.60%` / `2.98%` as "Reference numbers from the
  development machine" with no build stamp, so a tester cannot tell which code produced
  them.
- No gate touches `dist/`. `tools/depends_check.py`, `test_table_check.py` and
  `presentation_check.py` do not know it exists.

**Criterion violated.** Charter sweep item 3 (stale) and item 1. Also the doctrine's
Clarity standard: staleness must be *visible*, never silent — this artefact's staleness
is invisible by construction, because it prints a number and not a hash.

**Minimal delta (~8 lines, no deletion, no human gate).** Add to
`dist/README-cpu-bench.md`: (a) the exact two-arch build + `lipo` command; (b) the
commit hash the committed binary was built from; (c) a line saying the numbers are
that commit's. Better, if the lead wants the class closed rather than labelled: have
`tools/cpu_bench.cpp` print the build stamp the GUI already carries
(`hzGetBuild`), so a stale binary announces itself on every run. Deleting the binary
is a **human gate** and is not proposed — it has a real purpose.

---

### H5 — CI runs nothing at all on a docs-only PR, and two `verify fast` gates read exactly those files

**Claim.** `paths-ignore: ['**.md', 'docs/**', 'traces/**']` applies to the whole
workflow, so a PR touching only markdown runs **zero** CI jobs — including
`verify-fast`, whose `leak_gate` and INDEX/LIBRARY gate consume markdown and nothing
else.

**Evidence.**
- `.github/workflows/ci.yml:32-36` — `paths-ignore` on both `push` and `pull_request`.
  GitHub applies path filters per *workflow*, not per job, and `**.md` matches any
  depth.
- The gates that read markdown and would be skipped:
  - `verify:78` `leak_gate` -> `.kit/kit-gates.sh:90`, an ERE over all tracked files,
    i.e. `ROADMAP.md` (8 783 lines), `DECISIONS.md` (6 088), 249 traces. A
    machine-absolute path committed in a doc is exactly this gate's purpose and would
    reach `main` unseen.
  - `verify:146-152` — the INDEX/LIBRARY pointer-integrity heredoc, whose only two
    inputs are `INDEX.md` and `LIBRARY.md`. A pointer added to INDEX without its
    LIBRARY entry is a markdown-only change by definition.
  - `verify:122-128` — the structure check, whose entire input list is `.md` files.
- `private_name_gate` (`verify:52`) already skips on CI by design (`.leakcheck-names`
  is untracked), so the *only* CI-side privacy enforcement is `leak_gate` — the one
  the filter switches off.

**Criterion violated.** The global doctrine's "Never commit machine identity":
*"Enforced by: the `leak_gate` in every project's `./verify` (so it blocks the Stop
hook **and CI both**)"*. On this repo CI enforces it only for non-docs PRs.
Charter sweep item 1.

**Mitigating fact, stated honestly:** the local `Stop`/`SubagentStop` hook
(`.claude/settings.json`) runs `./verify fast` on every session end, so an
agent-authored docs change is gated locally. A human editing a `.md` in the GitHub web
UI, or any push that bypasses a session, is not.

**Minimal delta, cheapest first.** A second workflow, `docs.yml`, ~12 lines:
`on: pull_request: paths: ['**.md','docs/**','traces/**']`, one ubuntu step running
`./verify fast`. Ubuntu is the 1x runner and `verify fast` is seconds, so the cost
model in `ci.yml:1-18` is preserved (the reason `paths-ignore` exists is the **10x
macOS** job and the Windows build, neither of which this leg runs). Adding 12 lines to
close a privacy-gate hole passes the reduction test. Narrowing `paths-ignore` instead is
one line but re-enables the 10x job on every ROADMAP edit — do not do that.

---

## MEDIUM

### M1 — gui2's matrix stub is pinned to the layout PR #646 replaced yesterday

`src/gui/gui2.html:1754-1761` builds the dev-server / `lab_load_check` cell set with
`const NSRC = 1, NSLOT = 4` and slot rows at `f - NSRC`. The shell shipped
`kRoutingNSrc = 2` and slot rows at `kRoutingMaxSrc + (mi - kRoutingNSrc)` = row 8+
on 2026-09-18 (`src/hypersaw_clap.cpp:897-941`, ADR-088 amendment, PR #646). The stub
also emits no `22000+s` (`srcOut`) ids at all — `:1761` writes only `20000+t` and
`21000+t`.

Consequence: the only automated GUI gate (`lab_load_check`, `verify:162`) exercises a
one-source, contiguous-row pane. The two code paths the amendment exists for —
`mxShape()`'s `srcRows` discovery (`:6355-6364`) and `mxRowOf` (`:6371`), plus the
whole source-row terminal column (`:6436`) — are never reached by it. The stub's own
comment says *"If the real table and this disagree, the real table is right by
definition"* (`:1750`), which is correct and is precisely why the stub is now
decoration rather than a test fixture.

**Minimal delta, ~4 lines in `src/gui/gui2.html`:** `NSRC = 2`; slot rows at
`8 + (f - NSRC)`; emit `22000 + s` for each source. Removes 0, adds 0 net, and the
srcRows branch starts being exercised by a gate that already runs.

### M2 — The morph layout marker `5` is a bare literal in seven places

`src/hypersaw_clap.cpp:4538`, `:4644`, `:4807` (three independent writers — the
corner-preset export, the second corner writer, and `morphJson`),
`tools/intent_check.cpp:723`, `:1223`, `tools/gen_factory_bank.cpp:434`,
`tools/bank_check.cpp:382`. No named constant exists; `grep -n kMorphLayout src/` ->
no hits. The reader is `parseMorphLayout` (`:4705`) and the remap is `morphSlotMap`
(`:4682`).

`bank_check:382` pins the value, so a **factory blob** that forgets the bump goes red —
but nothing compares the three shell writers to each other. A bump applied to `:4538`
and missed at `:4807` gives a chunk and a preset that disagree about their own layout,
with `morphlayout_check` green (it is parametric in `layout`,
`tools/morphlayout_check.cpp:46,81`).

The value has moved three times in three weeks (3 -> 4 -> 5, per the comment at
`:4538`), so this is a live edit site, not a frozen one.

**Minimal delta:** `constexpr int kMorphLayout = 5;` beside `kRoutingIdBase`
(`src/hypersaw_clap.cpp:892`), referenced by the three writers. Removes 3 literals,
adds 1 line. **Net -2**, and the three writers can no longer disagree. Tools keep their
literals on purpose — an oracle that reads the value it is checking checks nothing.

### M3 — 109 parameter labels disagree between the shell and the presentation table; 33 are a different word

The shell's `kParams` carries a user-facing name (`{1, "n", "Voices", ...}`,
`src/hypersaw_clap.cpp:158+`, the string CLAP hands the host) and
`src/param_presentation.tsv` carries a `label` column (328 rows by `presentation_check`, the string the
generated GUI shows, `tools/gen_gui_controls.py`). Nothing compares them.

Measured at this commit: **109 rows disagree**. 77 are benign — the tsv strips a
trailing unit the `unit` column supplies (`"Bend Time (ms)"` -> `"Bend Time"` + `ms`)
or a page prefix. **33 are a different word**, including:

| address | host shows | GUI shows |
|---|---|---|
| `fx1tone`...`fx4tone` | `FX1 Tone` | `Resonance` |
| `d1sync`...`d4sync` | `D1 Sync` | `Time Mode` |
| `xyAsn0X` | `XY1 X > Macro` | `OSC1 x` |
| `mainAsnX` | `Main X > Macro` | `Main x` |
| `osc1.oscPitch` | `Pitch (cont.)` | `Pitch (st)` |

A user who finds "Resonance" in the GUI and searches the host's automation list for it
finds nothing. `presentation_check`'s docstring (`tools/presentation_check.py:5-22`)
gates totality, the absence of an `id` column, and scope-vs-address — deliberately not
this.

**Minimal delta, ~6 lines, and it should NOT be a gate.** Extend
`tools/presentation_check.py` with the "gaps are counted, not hidden" idiom it already
uses at `:147` for `page=TODO`: print the hard-disagreement count every run. A hard gate
would be false-positive-heavy (prefix-stripping in a grouped GUI is legitimate) and
would be weakened the first time it fired, which is worse than not having it. A number
on screen is enough to stop it reaching 33 again unnoticed.

### M4 — The leak gate cannot see any tracked binary file

`.kit/kit-gates.sh:90` uses `git grep --untracked -nIE`. The `-I` flag means *do not
match binary files*. Eight tracked files are therefore outside every privacy gate:
`dist/cpu_bench` (186 KB), `docs/img/gui-{hero,osc,light,morph}.png` (1.5 MB),
`tests/state_fixtures/chunk-*.f32` (1 MB) — 2.3 MB total, on a **public** repo
(CLAUDE.md §Domain alias note).

**Checked by hand at this commit and all clean:** `strings -a dist/cpu_bench` matching
the identity ERE -> 0 hits; `nm -pa` shows no `OSO`/`SO` debug stabs (no `__FILE__`
paths); the four PNGs -> 0 hits each. The finding is the **gap**, not a present leak —
and the doctrine names this gap explicitly (`governor/REPO-HYGIENE.md`: "binary/EXIF
vectors the text gate can't see").

**Minimal delta, ~4 lines:** a `binary_leak_gate` in `./verify`'s project-owned section
(beside `private_name_gate`, `verify:52`) running the same ERE over the tracked files
`-I` skipped. **`./verify` is a protected path — this is a sanction the human would
have to give.** Alternative with no sanction: fold it into `governor/leak_scan.py` at
the fleet level, where the doctrine already says it belongs.

### M5 — `docs/research/2026-08-24-cpu-audit.md` contradicts itself about the CPU budget

The document's orienting table (`:12-22`) reads:

| case | measured | x4 derate |
|---|---|---|
| 1 osc parameter ceiling | 16.23% | **64.9% — over budget** |
| 2 osc polyphony ceiling | 16.90% | **67.6% — over budget** |

followed by *"The ceiling is already over budget ... which is what makes the list below
urgent rather than tidy."* Its own §6 (`:214-230`) then records the measurement that
struck it: *"RESOLVED 2026-08-25 — the derate is x2.2-2.5, not x4 ... derates to ~41% —
inside [the budget]"*, measured on a real EPYC 7763 via
`.github/workflows/cpu-derate.yml`. ROADMAP B41 (`ROADMAP.md:7178`) carries the same
correction.

The document strikes finding 1 inline and marks finding 6 RESOLVED — good practice — but
leaves the table a reader hits 200 lines earlier saying the opposite. Charter rules of
evidence: where two sources disagree, that IS a finding.

**Minimal delta: 2 lines.** Strike the two "over budget" cells and add
"— superseded 2026-08-25, see §6" to the table caption.

### M6 — The lab index links a lab that does not exist

`docs/design/index.html:106-108` renders a card for `quantum-morph-lab.html`.
`ls docs/design/quantum-morph-lab.html` -> no such file; the delete-filtered history
shows it never existed. The other 24 cards all resolve.

**Minimal delta, two options.** (a) Delete the 4-line card. (b) Better, because it
closes the class rather than the instance: `tools/labharness/lab_load_check.mjs`
already parses every lab and already runs in `verify fast` (`verify:162`) — ~5 lines
there asserting every `href="*.html"` in `docs/design/index.html` resolves on disk.
Adds 5 lines, removes 4, and the next dead card fails a gate instead of waiting for an
audit.

### M7 — The landing page is missing the newest reference prototype

`index.html` lists 17 `reference/` files. `reference/subosc.html` (SUB OSC, ingested
2026-09-19, ADR-178, listed in CLAUDE.md §Domain as the seventh CANDIDATE and as a
protected path) is not among them. Everything else in `reference/` is listed, including
`reference/maw/maw-horde-distortion-prototype.html`.
**Minimal delta: 1 line.**

### M8 — Four transcribed counts in README/CLAUDE.md are wrong, and one contradicts itself

- `README.md:406` — *"`./verify fast | full` — 31 gates."* Actual at this commit:
  **41 compiled binaries** invoked by `verify` alone, plus 7 python gates, 8 node golden
  generators (each run twice) and 4 shell gates.
- `README.md:460` — *"Five standalone oracles are green but not wired into `./verify`"*.
  Actual: **13** (listed in H1).
- `CLAUDE.md` §Domain, "Stack & entrypoints" — *"fifteen gates: nine parity/trajectory
  chains ... plus ten behavioural/invariant probes"*. Nine plus ten is nineteen, so the
  sentence is wrong against itself before it is wrong against the tree. The same
  paragraph says *"seven single-file HTML prototypes plus one CANDIDATE"*;
  `reference/` holds 17 files plus `maw/`.
- `README.md:457` — *"Fourteen test-table rows have no oracle yet"*. The gate prints
  **16** at this commit (`test_table_check: GREEN (191 tests ... 16 awaiting an oracle)`).
  The line itself says it is "counted by the gate rather than quietly carried", which is
  exactly why the transcribed number should not be in prose at all.
- `README.md:448` — *"Last verified: 2026-09-10"*. Nine days and ~25 PRs ago, including
  the ADR-088 routing renumbering, the second source, `bassMonoPos`, ADR-171's gate
  ratification, and the MAW / SUB OSC ingestions.

The dated freshness line is doing its job — the staleness is visible, per doctrine —
but two of the numbers it certifies are wrong, and one is an arithmetic error that
predates any drift.

**Minimal delta: 4 lines** (`README.md:406`, `:457`, `:460`, and the §Domain sentence),
plus moving the `Last verified` date when the lead next re-checks.
**`CLAUDE.md` §Domain is the lead's to write, not a subagent's.**

---

## LOW

### L1 — A 1 911-line file committed twice, byte-identical

`docs/design-system/support.js` and `docs/design/logo/support.js` have the same MD5
(`951ae391b8ae72ef12e671c2fad23353`), 1 911 lines each. Four pages reference the first
via `./support.js`; one references the second
(`docs/design/logo/text-distortion-reference.dc.html:6`).

**Minimal delta:** point that one page at `../../design-system/support.js` and delete
the copy. **Removes 1 911 lines and 1 file, adds 0** — the largest single reduction in
this report. **Deleting a file is a human gate.**

### L2 — Three lab harnesses, three FFTs, 241 lines of same-named helpers

`tools/labharness/{reverb,station,subosc}_check.mjs` (25 KB / 54 KB / 40 KB) each carry
their own `fft`, `rms`, `peak`, `check`/`want`/`judge`, `run`, `planted`/`PLANT_*`, and
two of them their own alias-floor measurement (`station:aliasFloor` 16 lines,
`subosc:aliasFloorDb` 33 lines). Measured: reverb 78 lines, station 101, subosc 62 —
**241 lines** in same-named helpers. All three already share
`../golden/extract_core.mjs`, so the import seam exists.

**Minimal delta:** `tools/labharness/rig.mjs` — one FFT, rms/peak/dB, one
`check`/`want`/`run`/`planted` harness. Estimated ~90 lines; **net ~ -150 lines, +1
file**. Passes the reduction test on lines, and the real gain is that two
independently-written alias-floor numbers stop being asserted comparable and start
being comparable.

**Caveat the lead should weigh first:** reverb uses a Welch-averaged magnitude
(`welchMag`) where station uses a plain one — some of this divergence may be
deliberate. Confirm before consolidating; a shared rig that silently changes one lab's
measured pins is worse than three copies.

### L3 — Known stale comment, already on the record

`docs/design/station-page-lab.html:404-410` — the "NOTE FOR THE PORT" block describing
why `reference/station.html` cannot be read by `extract_core.mjs`. Listed as known;
nothing further spent.

### L4 — 249 traces, 17 862 lines, 1.5 MB, no index — and I do **not** propose adding one

`traces/README.md` is 5 lines and declares the directory append-only, per doctrine. No
index exists. **No deletion is proposed and none should be.**

An index is the obvious suggestion and it fails the reduction test: it would either be
hand-maintained (drifts, and there is no gate that could catch it without reading every
trace) or generated (a new tool plus a new gate, to replace an `ls traces/` plus grep
that already works because the naming convention `YYYY-MM-DD-<slug>.md` is 100%
observed across all 249 files). **Recommendation: change nothing.** Revisit if the
count passes ~500 or the naming convention breaks.

**Related negative result, worth recording:** the brief hypothesised duplicated corpus
descriptions across traces. Measured — every paragraph over 180 characters, across all
249 files — **two** are repeated verbatim: a DOM-stub harness in three ORBITAL traces,
and one pasted `verify` output block in three others. The traces are essentially
duplication-free. The hypothesis does not hold.

### L5 — A stale comment inside a stale scratch tool

`tools/scratch_b18_two_osc_matrix.cpp:9-12` says *"under the CURRENT default,
user_patch_bench's 'both oscillators audible' comment is stale — osc2 is silent AND
skipped"*. `tools/user_patch_bench.cpp:50` now sets `{1150,1}` explicitly, so the
criticism no longer applies. The file is 28 days old, deliberately not in CMake
(`:2`), and its measurement is published at
`docs/research/2026-08-22-two-osc-cpu-measurement.md`.
**Minimal delta:** 1 line, or leave it — it is honestly labelled SCRATCH.

### L6 — Two audit directories

`audits/2026-08-13/seams.yaml` (650 lines) at the root, and `docs/audits/` (this file
plus the three 2026-09-18 audits). Nothing references the root one outside itself.
**Minimal delta:** move it to `docs/audits/2026-08-13-seams.yaml` and delete the root
directory. Removes 1 directory. **Moving/deleting is a human gate.**

### L7 — Viz work runs unconditionally, and closing it fails the reduction test today

`src/hypersaw_clap.cpp:6465` calls `publishViz()` (170 lines, `:4075-4244`) on every
`process()` block, plus a 2-pass spectrum-ring fill and peak scan at `:6466-6480`.
There is no editor-attached guard anywhere — `grep -n "guiOpen\|editorOpen\|vizWanted"`
-> no hits — so with no GUI open the whole snapshot is computed and discarded.

**Estimated, method stated** (I could not measure: `libs/` submodules are not checked
out in this worktree, so `cpu_bench`/`measure_cpu` cannot be built here). The work is
per-BLOCK for `publishViz` (bounded by `kPoly` and `kMaxV = 32`) and two per-sample
passes for the ring — ~4 ops/sample, order 1e5 ops/s against a 44.1 kHz render. Against
the measured default-patch cost of 1.60-1.81% of a core
(`docs/research/2026-08-22-two-osc-cpu-measurement.md:67`), this is well under the
measurement noise floor.

**I am listing it and recommending against acting on it.** A 4-line `vizWanted` atomic
set by `gui_create`/`gui_destroy` would buy a fraction of a percent, and B41's explicit
rule is *"do not optimise anything before it is measured"* (`ROADMAP.md:7178`).
Revisit only if `cpu_check` puts viz in the top three sites.

### L8 — Branch volume (one line, the human's act)

328 remote branches besides `main`; **316 of them fully merged into `origin/main`**.
Pruning is the human's act.

---

## Corrections to this run's "already known" list

Recording these because a stale entry in the lead's own known-items list is the same
failure class the sweep is looking for.

1. **`tools/gui_reach.py`'s `intentBus` exemption is already gone.** The brief listed
   its wording as a live stale comment. At `5a496c1`, `grep -n "intent" tools/gui_reach.py`
   -> **no hits**; `EXEMPT` (`:33-37`) holds three entries, none of them `intentBus`.
   PR #666 removed it along with adding the SET control. Nothing to do.
2. **"Nine pre-session branches" is 328 / 316 merged** (L8).
3. **The duplicated-trace-corpus hypothesis does not hold** (L4).

## Refusal, stated rather than guessed

**I could not time `./verify full`.** `libs/clap`, `libs/clap-wrapper` and `libs/choc`
are empty in this worktree (worktrees do not inherit submodule checkouts), so a run
requires a recursive submodule init plus a cold Release build of both plugin targets
and 76 executables. That submodule operation is outside a worktree-isolated agent's
grant, and a cold-build number would not be the number the lead wants anyway (the
useful figure is warm-build gate time). **This measurement should be taken by the lead
in the main checkout, once, and recorded in `specs/ACCEPTANCE.md` beside E-6** —
`verify full` is now 41 binaries, 8 double-run node generators and a link of the whole
plugin, and nobody in the repo knows what it costs. The only figure on record is
`traces/2026-07-19-track-e2-time-engines.md:8` ("verify full = 9 chains"), from when it
was a quarter of its current size.

---

## Reduction budget

Lines and files each finding would remove against what it adds. Negative net is a
reduction.

| # | Finding | Removes | Adds | Net lines | Human gate? |
|---|---|---|---|---|---|
| H1 | Wire `ncap_check` + `tseed_check` | 0 | 2 | +2 | **yes — `./verify`** |
| H2 | `src/hypersaw_debug.h` | 70 | 59 | **-11** | no |
| H3 | Strike the false claim at `time_core.h:237` | 0 | 0 | 0 | no |
| H3' | *or* a `swarmfx_check` / a PARKED entry | 0 | ~150 / ~3 | +150 / +3 | ruling first |
| H4 | Build command + commit stamp in `dist/README` | 0 | ~8 | +8 | no |
| H5 | `docs.yml` docs-only CI leg | 0 | ~12 | +12 | no |
| M1 | Refresh gui2's `mxStub` | 3 | 3 | 0 | no |
| M2 | `constexpr int kMorphLayout` | 3 | 1 | **-2** | no |
| M3 | Count label disagreements in `presentation_check` | 0 | ~6 | +6 | no |
| M4 | Binary leak scan | 0 | ~4 | +4 | **yes — `./verify`** |
| M5 | Strike the superseded derate table | 0 | 2 | +2 | no |
| M6 | Gate lab-index hrefs in `lab_load_check` | 4 | ~5 | +1 | no |
| M7 | Landing-page card for `subosc.html` | 0 | 1 | +1 | no |
| M8 | Correct four gate counts | 0 | 4 | +4 | §Domain is the lead's |
| L1 | De-duplicate `support.js` | **1 911** | 1 | **-1 910** | **yes — deletion** |
| L2 | `labharness/rig.mjs` | ~241 | ~90 | **-151** | no |
| L5 | Strike the stale scratch comment | 1 | 0 | -1 | no |
| L6 | One audit directory | 0 | 0 | 0 | **yes — move** |
| L7 | *recommended against* | — | — | — | — |
| **Total** | | **~2 233** | **~197** | **~ -2 036** | 4 gated |

Files: **-2** (one `support.js`, one `audits/` directory), **+2**
(`src/hypersaw_debug.h`, `docs.yml`), **+1 conditional** (`labharness/rig.mjs`).

Two findings dominate the line count (L1 at 1 910 and L2 at 151) and neither is
load-bearing. The findings that matter — H1, H3, H5 — **add** lines, because what they
close is uncovered behaviour, and coverage is not free. That asymmetry is the honest
shape of this audit: the repo is not carrying much fat, it is carrying a handful of
places where the oracle stops and nothing says so.

## A clean bill, where the sweep found one

Reported because a clean result is a real result, and the absence of a finding here
should be citable next time.

- **TODOs: four in the whole of `src/` and `tools/`,** all four inside
  `tools/presentation_check.py` describing that gate's own `page=TODO` counting. No
  `FIXME`, no `HACK`, no `XXX`. Nothing older than a month.
- **No orphaned tool sources.** 76 CMake executables; the two `tools/*.cpp` with no
  target (`blep_alias_incommensurate_probe`, `scratch_b18_two_osc_matrix`) each declare
  themselves SCRATCH in line 1, and the two targets with no same-named source
  (`alias_check`, `cpu_check`) are documented re-compiles of `measure_alias` /
  `measure_cpu` under a judge flag (`CMakeLists.txt:564-579`).
- **The `chunk-v2-rev1.f32` / `chunk-v2-noheader.f32` byte-identity is correct, not a
  bug.** Both are chunk v2 and a header-less v2 loads as revision 1
  (`tests/state_fixtures/README.md:30-31`), so the two renders *must* match; the
  discrimination is on the load side. Checked because two identical golden files is
  normally a broken fixture.
- **Traces are duplication-free** (L4) and 100% conformant to their naming convention.
- **No ROADMAP row is marked running or dispatched with its PR already merged**
  (zero rows match the in-flight status pattern). ROADMAP references PRs through #667;
  #668 is the record of itself.
- **Every committed binary is free of machine identity** at this commit, verified by
  hand (M4). The gap is the gate, not the content.
- **The pan-motion exclusion is retired, not expired.** `subdiv_check`'s known
  exclusion was ruled by ADR-177 §1 and closed by PR #656 on 2026-09-19
  (`tools/subdiv_check.cpp:18,132-141`). It was the obvious candidate for "an exclusion
  whose reason outlived its ruling" and it is clean.
- **`slotcontract_check`'s three pinned violations are debts with names**
  (`verify:311-317`), not expired exclusions: each is a declared identity failure the
  rack owes a fix, and removing a pin is how that fix proves itself. No change proposed.
