# 2026-11-24 — B706: the blade bake, once and offline (ADR-406), and the blade engine's archive

**What changed.**
- DECISIONS.md: ADR-406, the human's rulings of 2026-11-24 (option 2; once, offline, nothing blade-era loads; the baked
  presets are raw material; a new plug-in id for the wavetable test build; the archive), with the lead's readings.
- The specs amended to it: BLADE-BAKE (option 2's frames indexed by member position, every frame rendered under the
  gradient law; the frozen engine is the generator, offline, no port; BB-F9 and BB-F12 deleted with their ids kept,
  BB-F4 re-aimed per frame, BB-F13 the tap's gate added, PROPOSED), CONTRACT (S6c the lead's stage; the S7 and S8 rows;
  §1.9's version-1 migration as RC-9.4 stands; §3's recipe and library copy; §7's answered rows appended), SPEC-WAVETABLE
  (WT-19, WT-43, WT-45, WT-48, WT-F30), TEST-PLAN (TP-16, E2E-5, Q-10, RG-5) and INTEGRATION (a superseded note). No
  SPEC-ENGINE change, so nothing goes to the clean-room repos.
- The archive: `docs/archive/BLADE-ENGINE.md`; `tools/blade-archive/` (`archive_render.cpp`, `render.sh`, and
  `bank.sha256`, the 332 bank renders' SHA-256 list).
- ROADMAP: B706, IN PROGRESS; B658's status notes ADR-406. Design ledger: `not_design` for ADR-406.

**Review.** Two independent Opus reviews of the spec edits, each fresh context, neither the editors.
- **The first** found 11 defects. Three were important, and all three stretched the ruling: S6c deleted outright, gate
  stages that could not be met, and BB-F4 narrowed to U = 1, a judged gate weakened without a ruling. The lead decided
  each: S6c stays as the lead's stage; each gate closes where its inputs exist (the bake's own at S6c, BB-F11 at S7,
  BB-F3 (b), F4 and F10 at S8); BB-F4 is re-aimed, so every frame is judged against the frozen engine's member at that
  frame's position. A fix pass applied all 11.
- **The second** confirmed all 11 resolved and the frame arithmetic exact (members at sᵢ land on frame
  k0 + (sᵢ + ½)(U − 1), with alternate's ±½ on the end frames and no clamp at U ≤ 9). It found one real defect: under
  alternate at odd U, rendering under the preset's own law would put the −½ cycle in the top frame. Every frame is now
  rendered under the gradient law. It also found the drift law, used by one demo, uncovered; that is now approximate and
  reported, a lead reading in ADR-406. Eight smaller items were applied by the lead.

**The archive's review.** An independent Opus critic (fresh context) judged the first draft REWORK: the Weir fallback
could never configure (the pin is checked with `git rev-parse HEAD`, which an unpacked tree with `git init` fails); the
build command could silently use unpinned libraries or a stale build folder; nothing computed the off-toolchain check;
the doc claimed proof steps that had not run; and nothing tied a rebuild to the binary of record. All applied: git
bundles of Weir and the three libraries at their pins (each verified, and a clone of each lands on its pin);
`tools/blade-archive/compare.py` (largest difference in dBFS; identical sets read -inf, a planted 1e-5 change reads
-100.0 dBFS and red, the demos compare at -84); `render.sh` takes named options, refuses a non-empty output folder and a
missing list, and prints the toolchain; the doc's commands rewritten (a worktree at the tag with its submodules, a new
output folder, no `--install`, `ditto` and a hash check to reinstall) and its proof paragraph limited to what ran. Its
question for the human (was build 11's horde commit recorded?) was answered by evidence instead, below.

**Why.** The human, 2026-11-24: "go with option 2", then, once the plug-in bake was explained, "No old sessions need to
load, bake once now" and "Let's make sure to cleanly archive the old engine though, in case I ever want to return to it";
three polls answered "Raw material", "New id" and "Approve".

**Evidence consulted.** BLADE-BAKE.md, CONTRACT §1 to §8, SPEC-WAVETABLE WT-19 to WT-48, TEST-PLAN, INTEGRATION §6.4;
SPEC-ENGINE C.10 (the spread laws and cut rules), WT-22 (the frame read), RC-9.1 and RC-9.4; ADR-186 item 2 and B255
(the `v0.9-legacy` precedent); the installed preview's binary (`engine 6cd98b5, build 11`); `tools/engine-preview/`
(build.sh, CMakeLists.txt, weir.pin); the clean-room engine at `6cd98b5` (the bank, the phrases, the offline entry);
`engine2/data/bank/presets.json`'s spread laws (74 gradient, 5 swarm, 3 alternate, 1 random) and the demo bank's
(String Machine on drift).

**The archive's proof** (the lead's runs, 2026-11-24; outside the sandbox where installing or the SDK fetch needs it).
- **The bank renders:** `tools/blade-archive/render.sh` from two fresh exports of the engine at `6cd98b5`: 332 files
  each, `archive_render: 332 files, 83 presets, median -18.10 LUFS`, the two lists byte-identical (`archive renders:
  every file reproduces …`); a list with one planted character reads different (control). No file is silent or
  non-finite (quietest peak 0.023, Breathing pad's trill; loudest 0.989).
- **The plug-in:** `tools/engine-preview/build.sh 6cd98b5` on a `git archive` of this branch (its `tools/engine-preview`,
  `docs/design/gui3`, `h2`, `tools/gui3_inline.py` and `libs` are byte-identical to main's), GUI 3's page inlined, Weir
  cloned at `078f1436`, not installed: `bridge_test: GREEN (0 failed)`, `engine-preview smoke: GREEN (0 failed)`,
  `engine-preview gui: GREEN (0 failed)`, `built: engine 6cd98b5, build 1`. The committed GUI frame fixtures were
  recorded on engine `1ad270a`, so build.sh only wrote fresh frames ("the committed fixtures aren't from 6cd98b5"):
  build 11's frames were never pinned. The fresh pair is archived with its hashes (below).
- **Build 11 itself:** the smoke test run against the archived build-11 CLAP: `engine-preview smoke: GREEN (0 failed)`,
  165 checks, among them every preset it plays "bit-identical to the engine's offline render, blocks of 128" (Trance
  jitter, Glass horde pad, Crunch horde, Wobble jaw, Blade pluck) and the morph's corner checks, with the plant builds
  as its must-fail controls. So build 11 plays the archived engine. **Its GUI page:** the page embedded in build 11 and
  in the rebuild are byte-identical (2,855,660 bytes, the same seven script hashes in their policy); as a control, pages
  inlined from older commits are identical back to `e9e20c6f` and differ from `8b580042` (2026-11-08), so the
  comparison sees a change and build 11's page is the tag's. **Its GUI frames** equal the rebuild's; but the control
  failed: the plant builds (wrapped as bundles so the dumper can load them) give the same frames too, so the frames pin
  the bridge's frame path for the dump's script, not the sound. The doc says so.
- **The engine's suite** at `6cd98b5`, built from the same export (4 jobs, niced): `ctest` `89% tests passed, 1 tests
  failed out of 9`; the one is the suite itself (5,395 s), whose only red item is `item 03 bank: DT-3's sample-rate
  independence`, 3.b's Crushed burst at `harmonics spread 3.274 dB`, the known red the engine's REPORT records at its
  acceptance with the same value (its other known red, 9.b1, is green here). bank_report, the inspect and sanitizer
  tests pass; 15a and 15b (WebAssembly) are skipped, no Emscripten on this machine.
- **The demos:** `preview-demo-render` from that build, twice: 120 files each, the two lists identical
  (`tools/blade-archive/demos.sha256`). **The GUI frames:** `preview-frames-dump` twice on `6cd98b5`, identical
  (`tools/blade-archive/gui3-frames.sha256`: display `56397bad…`, replay `24b7e5c9…`).
- **The local archive folder** `horde-legacy-archive/2026-11-24-blade-engine/`: `installed-build11/` (the three bundles
  as installed, `codesign` valid, binary SHA-256 CLAP `0352e883…`, VST3 `72528f61…`, AU `05ac4b53…`); `sdk-deps/` (the
  VST3 SDK at `9fad977` and the AU SDK at `53a9a20`, as fetched); `weir-at-pin/` (`git archive` of `078f1436`); `git-bundles/` (Weir `078f1436`, clap `195b42a0`, clap-wrapper `35f524b7`, choc `a08bfd84`, each `git bundle verify`-ed and cloned onto its pin);
  `renders/bank/` and `renders/demos/` (re-checked against their lists); `gui3-frames/`.

**Verify.** `./verify fast` in this worktree, exit 0 (the last run's output in the PR).

**Open.** The tags and the release, after merge (the lead cuts the tags; the clean-room repos' tags go up with the human's
push of their pending commits). The bake itself (S6c): its tap (BB-F13) first. GB37 and ADR-388's ledger reason, the GUI
session's to reword (B706's hand-over note).

## Rows filed: B707 and B708

**What changed.** ROADMAP gains B707 (an odd-harmonic mode for the noise Resonator, for stopped-pipe breath) and B708
(a delay and fade-in on the LFOs), both QUEUED with their 1.0 scope the human's to rule. **Why.** The human,
2026-11-24: "file the odd comb and LFO fade-in as rows", after the breath-and-plink wavetable kit (artifact
QfmCfooaUrM9rfh1sSP8eG). **Evidence.** SN-9 (the Resonator: one period, positive feedback, every harmonic); LF-1 to
LF-14 (no delay or fade row); the kit's siku A/B on a held A5: breath about 20 dB stronger at odd than even harmonic
slots with the odd comb, about 2 dB with the current one (an offline render, not horde's engine).
