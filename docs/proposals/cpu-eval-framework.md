# The CPU evaluation framework: a proposal for the human's ratification (B638)

> **Origin.** horde lead session, 2026-11-07, ROADMAP B638 and DECISIONS ADR-380 (PROPOSED). The human wrote a concept brief, "HORDE CPU Evaluation Framework — Agent Brief" (2026-11-06, kept outside the repo), the lead reviewed it, and the human answered "yes, write the proposal". Drafted by an agent the lead dispatched, from the brief, the lead's review, the two clean-room repositories (read only) and horde's records.

**Status: PROPOSED, 2026-11-07.** Nothing here is adopted. The decisions in section 5 are the human's, and nothing in section 4 starts until the human rules them. No gate moves, no spec text changes, and neither clean-room repository is touched by this proposal.

**How to read it.**
- "The brief" is the human's concept document. Its parts keep their names: the measurement layer, the oracle, the alternatives bench, the nested loops.
- "The builder" and "the test author" are the two clean-room roles of ADR-247. Their repositories are `cleanroom-engine` (the builder's) and `cleanroom-tests` (the test author's). Paths in them are written `cleanroom-engine:engine2/...`.
- CPU figures come from the record (REPORT.md, ANSWERS.md, DECISIONS, ROADMAP, `docs/port/cpu-ledger.md`). Nothing here was measured for this proposal.
- "Not established" means the repository does not answer the question. Each says what would answer it.

---

## 1. Summary

**What it is.** A way to make the new engine cheaper without changing its sound by accident. Measurements decide what is accepted. Changes that alter the sound come to the human's ears, in a capped batch, as options rather than as losses.

**What already exists.** Most of the brief's machinery is already built, under other names:
- a headless engine with a render entry and a block-by-block API;
- the test author's CPU tool, which runs PF-1, PF-2, PF-4, PF-6, PF-9 and PF-10 with interleaved repeats, a calibration loop and a load guard, and reads the worst block;
- the bank compared bit for bit against a reference commit at every builder commit (tier 0, in practice);
- the builder/test-author split of ADR-247, which is the brief's optimizer/eval split;
- seeded mulberry32 streams named by purpose;
- must-fail controls on every check, which is mutation testing in all but name;
- RealtimeSanitizer, working here with Homebrew clang 23 and `-isysroot`;
- a sealed hold-out suite with a committed hash (B517), and a differential report with a blind, level-matched listening batch (B523).

**What is new.** A per-module cost tree; a two-build A/B timing comparison with a confidence interval; a deterministic instruction-count proxy; the tier-1 classifier; the alternatives bench; the ledger; the tamper check over delivered tests; and the loop that runs it all.

**The short table.**

| Brief part | Existing piece | New work | Owner |
|---|---|---|---|
| Headless bench | `make_engine()`, `prepare`, `process` (harness header); `horde_offline_render`; the CPU tool `horde_engine2_cpu` | A two-build A/B runner (ABAB, median, bootstrap interval); block sizes other than 128; 96 kHz cells | test author (writes), lead (runs) |
| Corpus | The 83-preset bank, 20 seeded generated patches (PF-4), the heaviest patch (PF-1), two oscillators (PF-2, PF-10) | Idle, single-voice, note-on-burst and 30 s cells; High quality cells; morph-in-motion once the morph is built; FX once the rack is | test author |
| Metrics | Best-of-5 share of a core; PF-10's worst block; the builder's per-block worst-block runs | Median and p99 block time; idle cost | test author |
| Cost tree | The builder's `sample` profiles (ANSWERS.md); h2's subtractive stage toggles as precedent (`cpu-ledger.md` §3) | A node map from `engine-structure.json`; a coverage figure; a dashboard page | lead (map, page); builder (a measurement-only build, if ruled) |
| Proxy | Instructions retired by `/usr/bin/time -l`, already used by the builder (REPORT.md §1) | A repeatability check of that count; callgrind only if the human installs a container runtime | lead |
| Tier 0 | The bank, bit for bit, at 1 s per preset, against the previous commit | Extend it across the rates | test author |
| Tier 1 | Item 15's chaotic-list method (a one-ulp nudge, engine-free); the lead's unruled −120 dBFS escalation line | The self-divergence envelope and long-run statistics; a perturbation hook | test author (writes); the human (whether it auto-accepts) |
| Tier 2 and the bench | B523's differential tool; Blind-Spot Armor's capped listening batch | The alternatives manifest and the A/B/X page with CPU hidden until a verdict | lead |
| Safety batteries | Items 1, 2, 3, 5, 7, 8, 9, 12, 13, 17, 21 and the soak (section 2, Q9) | A denormal timing check on long tails; a battery runner that names each item | test author |
| Holdout and tamper | B517's sealed suite in gitignored `local/holdout/`, its hash committed | A perf holdout drawn by a recorded seed; hashes of the delivered tests checked before each run | lead |
| Mutation suite | Plants and must-fail controls throughout the suite and REPORT.md | The brief's must-catch set as one suite with a score | test author |
| Loops | The lead's batch briefs and scratch prototypes; the builder's batches 1 to 3 | The ledger, the middle-loop digest, the outer-loop report | lead |

---

## 2. The nine repo questions, answered

**Q1. Can the clean-room engine render headlessly, without the plug-in wrapper? Where is the entry point?**
**Yes.** Two entry points:
- the engine object: `horde::make_engine()`, then `prepare(sample_rate, max_block)` and `process(events, streams, out_l, out_r, frames)` (`cleanroom-engine:engine2/tests/harness/horde_engine.h:10`, `:445`, `:448`). The CPU tool drives exactly this (`cleanroom-engine:engine2/tests/cpu/main.cpp:154-289`). Any block size works, and DT-2 guarantees the output is bit-identical however the blocks are split (`specs/SPEC-ENGINE.md:624`).
- the offline entry: `extern "C" int horde_offline_render(state, phrase, routes, sample_rate, out, frames)` (`horde_engine.h:726`, built in `core/api/offline.cpp:117`). It accepts only 44.1 and 48 kHz (`horde_engine.h:708`, `:715`). For 96 kHz the bench uses the engine object.

**Q2. Does the engine core build in a Linux arm64 container, so callgrind can run?**
**Very likely, but not established.** No Linux build of the builder's tree has been run.
- For: the core includes only standard C++ headers, and there is no `__APPLE__`, NEON, x86 intrinsic or floating-point-environment code in `engine2/core` (a grep of `cleanroom-engine:engine2/core`, 2026-11-07). The CMake's only Apple-specific block finds the macOS SDK for the sanitizer compiler, behind `if(APPLE)` (`cleanroom-engine:engine2/CMakeLists.txt:237-242`). The same source already builds with a second, non-Apple toolchain: Emscripten, for the WebAssembly module, and item 15a is green on it (builder commit `ada5a96`). horde's CI runs `engine2/build.sh` on `ubuntu-latest` in every `verify fast` (`verify:660-665`, `.github/workflows/ci.yml:67-74`), but that is horde's own early copy of `engine2/`, not the builder's tree.
- Against: none found.
- **This Mac has no container runtime** (no Docker, Colima, Podman or Lima on the PATH, 2026-11-07), and no valgrind. Installing one is a new dependency (decision 1).
- **What would establish it:** one configure and build of `cleanroom-engine:engine2` on Linux arm64, in a container or on a GitHub-hosted arm64 runner.
- **Two cautions** if callgrind is used. DT-1 does not require bit-identity across builds (`SPEC-ENGINE.md:623`), so Linux renders are a proxy only, never compared bit for bit with the Mac's. And the engine spends a large share of its time in the maths library (about 12 % at the CPU pass's start, ANSWERS.md "The CPU baseline"; `sin`, `pow` and `atan2` still lead the profile in REPORT.md's diagnosis 1). Linux's libm is not Apple's, so a libm-heavy change can read differently under callgrind than on the Mac.
- **An option the brief did not have:** the builder already counts instructions retired on the Mac with `/usr/bin/time -l`, "the load-free measure" (`cleanroom-engine:REPORT.md:15-20`). It is a whole-run count, not per function, and its repeatability has not been measured.

**Q3. Where is the RNG seeded, and is its draw order deterministic and documented?**
**Yes, both.** `cleanroom-engine:engine2/core/random/stream.h` is the engine's one random facility: mulberry32, one stream per purpose (`Purpose`, line 10), seeded from the oscillator's seed, the purpose, the note counter and the member by a murmur3 mix (`stream_for`, line 54). Its header states the rule: "Never from a parameter's position in a list, never from how many draws another voice made." The spec makes it normative: DT-4 (`SPEC-ENGINE.md:626`), DT-5, no clock (`:627`), MO-6 for the morph's quantum draw (`:1874`), MS-5 for the modulation system (`:1998`). Because each purpose has its own stream, reordering two purposes' draws does not change either. Reordering draws inside one purpose does, and the brief is right to measure that as tier 2.

**Q4. Which state variable can take the one-ulp perturbation hook?**
**A member's natural frequency, f_i^nat (C.3), by precedent.** Item 15 already calls a state chaotic when its law, rendered engine-free twice, "once as written and once with a one-ulp change to one member's natural frequency or with every update by fused multiply-add", departs from itself by more than −90 dB within 2 s (`SPEC-ENGINE.md:903`, round 35, ADR-281). The test author keeps that list with its engine-free evidence (`cleanroom-tests:engine2/tests/wasm/chaotic.json`). The engine shows the value as `MemberView::f_natural` (`horde_engine.h:294`).
- **There is no hook in the engine yet.** The engine has no way to set internal state from outside.
- **The precedent for adding one:** test-only library targets built from the same sources with a define, never shipped: `HORDE_TEST_FIFTH_SOURCE` and `HORDE_TEST_ADDED_PARAM` (`cleanroom-engine:engine2/CMakeLists.txt:85-101`). The harness header names the target, and the builder builds it. A `HORDE_TEST_PERTURB` target would follow the same path.
- **Not established:** whether a one-ulp nudge to a continuous parameter through the public `Param` path would serve without any hook. Smoothing and the parameter's law may swallow it. One render pair would answer it.

**Q5. Which debug taps already exist?**
- **The Kuramoto order parameter:** yes. `OscView` publishes `R`, `psi` and `R_N` per oscillator, per voice, through the observation view (IF-10; `horde_engine.h:311-320`). Members' phases, frequencies, drift and inertia are in `MemberView` (`:292-309`).
- **Active voices and members:** yes, as IF-9's operation counters: `member_samples`, `voices_sounding`, `edges_corrected`, `edges_left_over`, `envelope_steps`, `voices_culled`, `voices_taken`, `notes_refused` (`horde_engine.h:229-256`; IF-9, `SPEC-ENGINE.md:689`). `member_samples` is the brief's "fewer unison oscillators" probe already: dropping a member lowers it. `OscView::unison` and `on` are per oscillator.
- **Active FX stages:** no. There is no FX in the engine; the rack is outside it (`engine-structure.json`, item `fx_rack`, a slot).
- **Oversampling factor per stage:** no counter. Quality is a saved per-patch setting, Standard (1×) or High (2×) (`SPEC-ENGINE.md:235`), readable as a parameter. The sub's drive always runs at twice the host rate (the structure's `sub.drive`). **Not established:** whether `member_samples` counts internal samples at High, which would make it an oversampling probe too. Item 17's code or one render answers it.
- **Numerical latches:** `nonfinite_voice_resets`, `nonfinite_output_samples`, `nonfinite_latch`, `clip_latch` (RT-3, IF-6).

**Q6. What does the current `./verify` cover, and where should the perf modes attach?**
- **`fast`** (Layer-0, CI-blocking): among many horde gates, five for the new engine: the clean-room recipe check, a build of horde's `engine2/` with its empty suite, the reference renders' static check, the differential tool's engine-free self-test, and the structure check (`verify:648-717`). It does not build or run the builder's engine; that lives in its own repository.
- **`full`:** `fast` plus the legacy and composed-engine gates. Timing is deliberately left out: "cpu_check is NOT here — a timing measurement must not gate CI" (`verify:1280-1281`).
- **`nightly`:** long jobs from `tools/nightly/jobs.json`, one receipt, "NO CLOCK DECIDES" (`tools/nightly/run.py`, header; `verify:1642`).
- **In the clean room:** the CPU tool is "reported, never gating, never run by ctest" (`cleanroom-engine:engine2/CMakeLists.txt:194-206`; PF-3).
- **Where perf attaches:** see section 3.5. In short, the timed runs stay outside every gate, in the clean-room repositories and on the lead's instruction. Only deterministic, engine-free checks (the oracle's self-tests, the tamper hashes, the dashboard generator's self-test) join `./verify fast`, as new checks under ADR-180 §1.

**Q7. Can the original engine build alongside the clean-room one for the differential profile? How do its modules map onto the clean-room ones?**
- **Which engine is "the original".** The brief's "about 29 %" is horde 2's composed engine, `h2/engine/` (header-only, `horde2::engine`): Glass horde pad, 8 voices, os 1, about 29 % of a core, measured on a busy machine and normalised (B488, `ROADMAP.md:7511`; ADR-347's same-session reading, `DECISIONS.md:10760`). It was not PF-3, and at the preset's own oversampling the same engine read 62 to 72 % (`docs/port/cpu-ledger.md` §2, §3). The legacy plug-in, `src/hypersaw_clap.cpp`, is a third engine. It builds in horde's top-level CMake (`CMakeLists.txt:176`) and is not the 29 % figure.
- **Alongside:** yes, as separate programs. The composed engine builds in horde (`tools/measure_h2_engine.cpp`, and its `--stages` build at `CMakeLists.txt:1108-1112`); the new engine builds in its own repository. Both use this Mac's Command Line Tools and Unix Makefiles. One program linking both is neither needed nor wise. A differential profile runs the same PF-3 cells through each, in alternation.
- **Module map (the lead's reading, not a ruling):**

| Composed engine stage (`cpu-ledger.md` §3) | New engine structure item | New engine files (by name only) |
|---|---|---|
| swarm control tick | `osc.coupling`, `osc.detune`, `osc.drift`, `osc.inertia`, `osc.frame` | `core/swarm/swarm.cpp`, `detune.cpp` |
| swarm phase advance | `member.phase` | `core/swarm/swarm.cpp` |
| blade evaluation (`out`) | `member.base_wave`, `member.blade1`, `member.blade2` | `core/blade/*`, `core/swarm/blades.cpp` |
| PolyBLEP edge search | `member.edges` (ED-7's taper, a different method) | `core/edges/*` |
| DC estimate refresh | `member.dc` | `core/blade/dc.cpp` |
| os-rate output biquads | none at Standard; High's decimation | not established |
| DC blocker and `tanh` | `osc.dc_blocker`, `output` | `core/output/output.cpp` |
| couple and spread | `osc.coupling`, `osc.spreads`, `osc.placement` | `core/swarm/*`, `core/blade/spreads.cpp` |
| remainder (overrides, smoothers, envelopes, pan) | `parameters`, `voice.envelopes`, `member.pan` | `core/api/params.cpp`, `smoother.cpp`, `core/voice/envelope.cpp`, `core/swarm/pan.cpp` |

The edge stage is where the two differ in kind: PolyBLEP against ED-7's taper integral. At the pass's start the taper was "at least 48 % of all the time" (ANSWERS.md, "The CPU baseline", 2026-11-03).
- **The clean-room rule:** the builder never sees the composed engine (BD-10), so this profile is run by the lead and goes to the human, never into either clean-room tree (B523's standing).

**Q8. Does the toolchain support RealtimeSanitizer?**
**Yes, with Homebrew LLVM; Apple clang 16 does not.** Homebrew clang 23 runs RealtimeSanitizer and ThreadSanitizer given `-isysroot "$(xcrun --show-sdk-path)"`, proven by planted faults (ADR-199). The engine's suite already has the test, `item08_rtsan`, which reports NOT RUN with a reason when no such compiler is found, never a pass (`cleanroom-engine:engine2/CMakeLists.txt:220-284`). horde runs its own in `./verify full` (`tools/rtsan_check.py`, `verify:1412`).

**Q9. Which existing tests already overlap the safety batteries?**

| Battery | Existing (test author's items, `cleanroom-tests:engine2/tests/items/`) | Gap |
|---|---|---|
| NaN and Inf | item 1, non-finite safety and the output guard's latch (RT-3, QS-8) | none |
| Denormals | item 5, "no subnormals" in the output (QS-7, OU-4) | **cost** on long tails is not timed; the core sets no flush-to-zero (no FTZ code in `engine2/core`) |
| DC and peaks | item 4, `item04_dc_and_peaks` | none |
| Long-run drift | QS-9's soak, four hours per reference patch (item 6) | none |
| Clicks | item 9, QS-4's ratified detector with its must-fire control; morph flips join with stage 7 | voice steals under a stream; preset change |
| Aliasing | item 7 (QS-1, QS-2 floors), item 26 (sub), QS-3's bank report | a ladder at 96 kHz and High, as one report |
| Semantics probes | item 17, cost when off by the counters; IF-9's counters | per-stage oversampling; FX stages (no FX yet) |
| Contract | item 12 (the manifest: every key, range, default, law), item 13 (state round trip and render), item 21 (an added parameter), item 22 (tier) | none for the engine; the plug-in's chunk is the shell's (`tools/engine-preview/smoke.cpp`) |
| Determinism | item 2 (DT-1), item 3 (DT-2 block splits, DT-3 rates), item 13.r1 (DT-6) | none |
| Real-time safety | item 8: allocation guard, RealtimeSanitizer, ThreadSanitizer, inspection rules | none |

---

## 3. The adapted design

### 3.1 Roles

The brief has four roles. horde has them already, under ADR-247, with one split the brief does not make.

- **The eval agent is the test author, extended.** The test author writes the bench, the corpus, the oracle, the batteries and the mutation suite, in `cleanroom-tests`, from the spec and the human's rulings. They are delivered to the builder as tests are today.
- **But the test author never runs them on the engine.** B519 forbids any builder measurement, as-built fact or engine pass/fail reaching the test author. The brief's eval agent reads CPU numbers and engine renders. So the eval role splits:
  - **eval author:** the test author. Writes the tools and proves each one engine-free, with must-fail controls on synthetic or law-rendered signals.
  - **eval runner:** the lead. Runs the tools on a candidate, as it runs the CPU tool and reruns the builder's commits from a clean export today. Results go to the human and, as verdicts only, to the builder. Never to the test author.
- **Optimizers are builders.** The builder has been the optimizer since ADR-347 (batches 1 to 3, from the lead's scratch prototypes). Any additional optimizer is another builder session, in its own worktree of `cleanroom-engine`, under every builder rule: the spec and the tests only (BD-10); no sight of the composed engine, the legacy engine, `engine-structure.json`, the differential profile, or the holdout.
- **The lead** briefs, runs, records and escalates. In the clean room a "merge to main" is the builder's local commit, accepted after the lead's clean-export rerun. horde PRs are merged by the human only.
- **The human** sets targets, thresholds and tier-2 verdicts, and rules every oracle change.

**Holdout.** B517's pattern carries over. A perf holdout of about a quarter of the patch × scenario pairs, drawn by a mulberry32 seed recorded in horde, kept in gitignored `local/holdout/`, with its hash committed before the first run. One caution: the builder can read the whole bank (`engine2/data/bank/`), so a holdout of bank presets only hides which ones count. Generated patches with unpublished seeds hide more.

**Tamper check.** New work. Before each gate run the lead hashes the builder's `engine2/tests/` and the CPU tool and compares them with the test author's delivered commit. The pattern is B517's `SHA256SUMS`. No such check exists today (no horde tool reads either clean-room repository, 2026-11-07).

### 3.2 Measurement: the corpus and the scenarios

The CPU tool's cells become the corpus core. The brief's eight scenarios map onto them like this:

| Brief scenario | Existing cell (`tests/cpu/main.cpp`) | Gap |
|---|---|---|
| Idle | none timed; item 17 shows idle voices cost nothing by the counters; batch 3 §4 measured a silent `process()` | an idle cell, and a tails-to-silence cell |
| Single voice | none (PF-9 starts at 8) | a 1-voice cell |
| Full poly | PF-9 at 8, 16, 64 voices; PF-5's linearity; PF-10's stream steals at a limit of 8 | none |
| Max unison | the sanity cell at unison 16 | none |
| Morph in motion | PF-4's stepped-key flips every 0.25 s | XY travel with quantum flips, once the morph is built (B631, round 61) |
| Automation sweep | PF-2: per-voice offsets at every control interval and PA-4's audio-rate destinations; PF-11 for the modulation system | host `Param` sweeps |
| Note-on burst | PF-10's note stream and worst block; the chord-start block (REPORT.md diagnosis 2) | a dedicated chord-on-one-sample cell |
| Long sustain (30 s) | the soak checks drift, not cost; item 5 checks no subnormals | a timed 30 s cell with p99 and max |

**Rates and blocks.** PF-3 measures at 44.1 kHz (and 48) with 128-sample blocks. The brief's matrix (44.1, 48 and 96 kHz; 32 to 1024 and a random schedule) is new. DT-2 already guarantees the sound is the same at every block size, so only the cost is new to measure.

**Patches.** The 83-preset bank and 20 seeded generated patches are in PF-4; the heaviest patch in PF-1; two oscillators in PF-2 and PF-10. New: High quality cells (the brief's "2× on"). **Not yet possible:** "every FX module engaged" (no FX until the rack is integrated) and morph patches with four corners (the morph is not built).

**Metrics.** Today: the best of 5 interleaved repeats, and the worst block as the smallest of 5 repeats' values (PF-3, PF-10). New: the median and a bootstrap interval of the A − B difference between two builds, and p99 block time. That needs a runner that alternates two separately built programs (ABAB), since both engines use the namespace `horde` and cannot share one process.

**The human's own sessions.** The test plug-in already logs, per preset played, the mean and worst share of each block and how many overran (`tools/engine-preview/README.md:27`). A patch the human flags while playing (ADR-370) can enter the corpus as a cell. That keeps the corpus tied to what the human actually plays.

### 3.3 The cost tree

**Nodes.** The tree follows `engine2/structure/engine-structure.json` (78 items): engine → voice (× sounding) → osc (× 2) → its control work (`osc.coupling`, `osc.detune`, `osc.drift`, `osc.inertia`, `osc.frame`, `osc.spreads`) and member (× U) → `member.phase`, `member.base_wave`, `member.blade1`, `member.blade2`, `member.edges`, `member.dc`, `member.loop`, `member.pan`; then `sub.*`, `noise.*`, `voice.envelopes`, the `allocator`, `parameters` and `output`.

**How to fill it.** Three methods, in order of cost to the clean room:
1. **Sampling, no engine change.** `/usr/bin/sample` runs here without Xcode, and the builder already profiles this way (ANSWERS.md, "The CPU baseline": "a sample of `pf1` on a `-O3 -g` copy"). The lead keeps a map from function and file names to structure items. Coverage is the share of samples the map places. Inlined code lands in its caller, which limits the depth.
2. **Subtractive toggles.** The composed engine's precedent (`cpu-ledger.md` §3): a measurement-only define per stage, folding to `false` elsewhere. Its own caveat holds: shares are not additive (99 to 106 %).
3. **Scoped timers.** The brief's method. **It conflicts with DT-5** ("The engine reads no clock", `SPEC-ENGINE.md:627`). It is possible only as a measurement-only test target, like the `HORDE_TEST_*` targets, and only by a spec round. The builder cannot read the structure file (BD-10), so the spec would have to name the nodes. Per-member timers would also cost more than the code they time; member-level detail belongs to sampling.

The lead's proposal: method 1 in Phase 0, measured for coverage; method 3 only if 1 cannot reach 95 % and the human rules the DT-5 exception (decision 10).

**The dashboard.** One horde page per run: the tree as a flame graph, the corpus's worst cells, the trend against 10.7 %, and "could the human run this in Ableton?" beside each figure. It joins Blind-Spot Armor's dashboard (row 12, worst-case CPU) rather than standing alone.

### 3.4 Tiers

- **Tier 0, bit-exact: exists.** Each builder commit compares the 83 bank presets at 1 s each, bit for bit, against the previous commit (REPORT.md §1: "83 of 83 bit-identical"). New: across rates, and on the holdout.
- **Tier 1, chaos-equivalent: as the brief has it, and the human's decision whether it auto-accepts** (decision 2).
  - **Today's practice, stated plainly.** The lead's batch briefs let the builder continue past a difference whose peak stays under −120 dBFS against the previous build, with the suite green (`cleanroom-engine:ANSWERS.md:500`, `:510`). Past that it comes to the lead first. Batch 2 landed at "within −133.2 dB" of the earlier tree. So a fixed-threshold tier 1 already runs, by the lead's rule, without an ADR. The DC-refresh stagger (−33 dBFS on Collision bite, REPORT.md §3) went to the human: that was tier 2.
  - **The calibration.** The brief's self-divergence envelope is item 15's method applied to the engine instead of the law. Item 15 decides chaos engine-free only (`SPEC-ENGINE.md:903`), and ADR-247 asks for engine-free evidence before any gate moves. A tolerance computed from the reference engine's own perturbed runs is engine-derived. It sets a new gate rather than moving one, but the human should rule it knowingly.
  - **Where it bites.** The swarm, live loops above BL-12's limits, FM free, and sample-and-hold modulators at knife-edged rates. Eight states are already on item 15's lists: one chaotic version-0 state and seven knife-edged ones (`chaotic.json`).
- **Tier 2, sound-altering → the alternatives bench.** Each alternative becomes a listening-batch item of Blind-Spot Armor's third kind, "an intentional sound-design change needs approval" (`docs/strategy/blind-spot-armor.md:111-135`): level-matched, blind, a one-line question, capped at about 15 items or 20 minutes a session. B523's tool already makes level-matched blind A/B pairs and ranks them (`tools/engine2_differential.mjs`). New: CPU hidden until the verdict, and the five verdicts logged to the manifest.

### 3.5 Where it attaches to `./verify`

ADR-180 §1: adding a check is not gated; weakening one is. This proposal only adds.
- **In `./verify fast`, deterministic and engine-free:** the oracle's own self-tests (the tier classifier on synthetic signals, each with a must-fail control), the tamper-hash check against recorded hashes, and the dashboard generator's self-test. Each is a `tools/*_check` wired in the PR that creates it, or marked `UNWIRED: <reason>`.
- **Not in any gate:** every timed run. They run in the clean-room repositories, on the lead's instruction, as PF-3 already requires. The receipts come to horde's dashboard.
- **New `./verify` modes** (the brief's `perf-fast`, `perf-full`) would edit the dispatcher. The charter lists editing `./verify` as a human gate even when ADR-180 §1 would allow the check itself. The lead's proposal: no new mode. The horde-side reports run as `./verify nightly` jobs, which already report "skipped" with a reason when they cannot run, and the load guard's refusal is such a reason.

### 3.6 Living with ADR-370

ADR-370 put the morph first and made CPU "fixed as the human finds it while playing". This framework does not reverse that.
- **Phases 0 to 2 change no engine code.** They build tools and measure, so they run beside the morph and modulation builds without touching them.
- **Phase 3 waits for the morph to land** (B631, round 61). Before then, the engine changes daily and the parameter contract is still moving: round 61 adds the morph's host parameters, round 62 changes five rows' rates (ADR-379, Engine change).
- **The freeze point.** The brief's frozen parameter contract is item 12's manifest. The lead's proposal: freeze at each spec round's delivered manifest, and re-baseline the corpus and the reference renders at each round. The human may prefer to wait until 1.0's manifest (decision 11).
- **The play-and-fix lane stays first.** A problem the human hears in the test plug-in becomes a builder item between feature work, as ADR-370 says. The framework's job there is to measure that item's gain and classify its tier, not to choose it.
- **The machine.** One heavy job at a time, at most 4 threads, under `nice` (the clean-room throttle rule). Timed runs wait for a quiet window (decision 3).

---

## 4. The phased roadmap

Two gates matter most, as in the brief: nothing is optimized until the bench and the oracle exist, and nothing runs unattended until the oracle has proven it catches bad changes.

**Phase 0 · Assemble and instrument.** Mostly assembly.
- Build: the A/B runner (ABAB, median, interval); the missing cells (idle, 1 voice, burst, 30 s, High, 96 kHz, the block sizes); the sampling map to structure items; the dashboard page; the corpus manifest; the instruction-count repeatability check.
- Exit: the same build renders bit-identical audio across runs (item 2 already proves it for the suite; extend it to the bench); the wall-clock repeat spread is within the noise budget in a quiet window (initially 3 %; note the calibration loop's spread was 15 % in the last PF-3 run, REPORT.md §7, so this may need several windows to reach); the cost tree places at least 95 % of samples; ADR-380 ratified.
- Owners: the test author (cells, runner), the lead (map, page, check).

**Phase 1 · Diagnose.** No engine code changes.
- The lead runs the differential profile: the composed engine against the new one, PF-3 cells, in alternation, mapped by section 2's Q7 table. It goes to the human only.
- The "29 %" is re-measured under PF-3 on the composed engine, so it anchors something.
- The human measures a heavy Serum 2 patch in Ableton, at matched polyphony, rate and buffer.
- Exit: a diagnosis naming the top regressions and their share; the target recorded in DECISIONS.
- Owners: the lead; the human (the Serum 2 reading).

**Phase 2 · Oracle.**
- Build: the tier classifier; the perturbation hook (a spec round naming a `HORDE_TEST_PERTURB` target, if ruled); long-run statistics; the battery runner over the existing items; the tamper check; the perf holdout; the must-catch mutation suite.
- Exit: the reference against itself reads tier 0; against a perturbed run, tier 1; the must-catch set scores 100 %.
- Owners: the test author (writes, engine-free); the lead (runs, holdout, hashes); the builder (the hook target, if ruled).

**Phase 3 · Supervised inner loop.** After the morph lands.
- One builder-optimizer, on the hottest node, one change per commit. The human reviews every accept and listens to a sample of tier-1 accepts.
- Exit: at least 10 changes accepted; the human hears none of the tier-1 accepts as different; the ledger and traces complete.
- Owners: the lead and the builder.

**Phase 4 · Alternatives bench and middle loop.**
- Build: the alternatives manifest, renders, the A/B/X page inside the listening batch, rebase and re-measure, bundles, the digest.
- Exit: one verdict round completed by the human; the proxy validity check running.
- Owners: the lead; the human (verdicts).

**Phase 5 · Unattended operation and the outer loop.**
- Preconditions: the human's tier-1 ruling; the tamper check and protected-path check active; the mutation score at 100 %; quiet windows scheduled.
- Stop and report when the target is met across the corpus and the holdout, or three middle-loop cycles gain under 1 % in total.
- Owners: the lead; the human (the stop review).

---

## 5. Decisions for the human

Each is the human's. Where the lead has a recommendation, it says so.

1. **A container runtime for callgrind** (the human's).
   - (a) Install one (Colima or Docker): a new dependency on this Mac, and Linux's libm differs from Apple's.
   - (b) Run callgrind on a GitHub-hosted Linux arm64 runner in the clean-room repositories: nothing installed here, but a new CI workflow there.
   - (c) No callgrind: use the Mac's instructions-retired count, already in use, plus sampling for attribution.
   - The lead recommends (c) first, with its repeatability measured in Phase 0, and (a) or (b) only if (c) fails.
2. **Whether tier 1 may auto-accept** (the human's).
   - (a) Never: tier 1 joins the listening batch.
   - (b) Yes, after Phase 2's 100 % mutation score and Phase 3's listening sample, with the chaos-calibrated tolerance.
   - (c) Rule today's −120 dBFS practice explicitly as the tier-1 line, until (b) is ready.
   - Either way, the lead's current practice should be ruled or stopped: it is unruled.
3. **Quiet windows** (the human's): when timed runs may happen, what counts as quiet (PF-3's guard: one-minute load at most 3.0, no other process over half a core), and whether other sessions pause for them.
4. **The Serum 2 reference** (the human's): which patch, polyphony, rate and buffer; and whether its figure replaces 10.7 % or sits beside it.
5. **The brief's initial thresholds** (the human's): minimum effect size (2 % of a node or 1 % of the engine), α (1.5) and the floor (−120 dB relative to RMS), the holdout share (25 %), repeats (15; PF-3 uses 5), the per-attempt budget, the middle-loop cadence (5 accepts or 4 hours), and the noise budget (3 %).
6. **The listening set** for the A/B bench (the human's): 6 to 10 excerpts. Until then the lead proposes them from B523's ranking.
7. **The aliasing ceiling for alternatives** (the human's). The brief's default: no worse than the original engine. Note which original: the composed engine, not the legacy plug-in.
8. **Model routing for each role** (the human's), within doctrine: every agent pinned, none above the latest Opus. The brief's suggestion: Opus for the lead and the eval author; Sonnet for optimizers, Opus for structural changes; Haiku for ledger summaries.
9. **Whether promoted alternatives may become user-facing modes** (the human's). That needs new parameters, GUI work and an ADR, and it has an FX-device consequence (ADR-254): a quality or character mode is a device setting the FX device would have to state.
10. **Scoped timers against DT-5** (the human's): allow a measurement-only build target that reads a clock, by a spec round, or keep sampling only.
11. **The parameter-contract freeze point** (the human's): at each spec round's manifest (the lead's proposal), or at 1.0's.
12. **The eval role split** (the human's): the test author writes the oracle and the lead runs it, so no engine figure reaches the test author (B519). This extends ADR-247, so it should be ruled, not assumed.
13. **The agent rung** (the human's): start at rung 2 (the lead, one builder-optimizer, the eval author); parallel optimizers only after Phase 4 is stable.

---

## 6. Risks

- **The swarm's chaos and tier-1 calibration.** A strongly coupled swarm departs from itself within a fraction of a second (item 15's state 09: past −90 dB within 0.2 to 0.45 s). In those states a sample-level comparison means nothing, and tier 1 rests on statistics. Four perturbed runs may under-sample the envelope, so a real change could hide inside it. The mutation set's "benign reassociation must pass" and "quantized coefficients must fail" are the guard; the calibration needs both before anything auto-accepts.
- **Engine-derived tolerances.** ADR-247 exists because the clean room once judged the engine by the engine's own numbers. A tolerance taken from the reference build's own divergence is that shape. Keeping the reference a frozen, accepted commit and the procedure engine-free in its writing reduces the risk; it does not remove it.
- **The noisy machine.** Many sessions share this Mac. PF-3 was refused twice by load from macOS's media analysis and OneDrive before a third try passed (REPORT.md §7); batch 3's stagger readings ran at a load of about 15 (§3); the calibration loop spread 15 %. Unattended wall-clock gates without scheduled quiet windows will either refuse or chase noise.
- **Proxy validity.** Instruction counts miss memory stalls and libm differences. A Linux proxy measures Linux's libm, not Apple's. A whole-run Mac count includes start-up. The brief's proxy validity check is the remedy; it needs enough accepted changes to say anything, so early on every win needs wall-clock confirmation.
- **Benchmark detection.** The builder can read the bank and the tests. The holdout and generated patches with unpublished seeds are the guard, and the tamper check stops the tests from moving.
- **Clean-room leakage.** The differential profile, the structure file and every engine figure are the channels ADR-247 closed. The role split in 3.1 is what keeps them closed.
- **Interference with the morph and modulation builds.** Optimizer commits in the same files as round 61 and 62's work would conflict, and the contract will move twice more. Phase 3 waits for the morph for that reason.
- **The human's ear time.** Tier 2 and any tier-1 listening sample draw on the same capped batch as every other sound question. Alternatives must queue behind blocking items.
- **The real stake.** A patch at 100 % of a core stops the human's Ableton session. The framework pays off over weeks, so it must not delay the open question of a simpler engine for 1.0 if the CPU cannot be fixed in time. Every report should say, beside 10.7 %, whether the human could play the patch.

**Two premises in the brief, corrected from the record:**
- **"Over 98 % of one core."** That was the first CPU batch's scratch figure (Glass horde pad, about 95 to 98 %, ANSWERS.md, 2026-11-03). After batch 3, PF-1 reads **61.89 %** of a core (Glass horde pad, 8 voices, 44.1 kHz, build `866bfd3`; REPORT.md §7). The heaviest worst block is Crunch horde's, **166.0 %** of its duration (REPORT.md §7). PF-10's two-oscillator ambitious patch averages **128.88 %** at a limit of 8 voices. The target is **10.7 %** of an M3 core (ADR-354).
- **"About 29 % in the original engine, a 3.4× regression."** The 29 % is the composed engine at os 1, normalised from a busy machine, not PF-3 (B488). Against 61.89 % it is about 2.1×, and that ratio means little until both engines are measured the same way (Phase 1).
