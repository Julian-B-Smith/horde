# Blind-Spot Armor — phase 2 proposal

**Status: PROPOSED, for the human to rule on.** Written 2026-10-10 by a read-only planning agent for the lead (B448, ADR-197).

**Evidence base.** `origin/main` at `4a3dbaf`, which matched the remote head (no fetch was run). ADR-205 and the Wave C catalogue rows for R7, R11 and R12 are only on the lead's unmerged local branch `lead-records-230` (`45df963`), and are cited as "(unmerged)". Run times are estimates unless a source is named.

## 1. Where phase 1 left each row

"Engine" means horde 2's composed engine in `h2/engine/`. "Enrolled" is defined in section 2.

| Row | Guards it now | Biggest remaining gap | GREEN when (so `gaps` can be empty) |
|---|---|---|---|
| R1 realtime safety | `rtsafety_probe`; `rtsan_check` on the legacy `process()` over 397 ids (ADR-199, PR #999) | Runs only on the dev Mac and SKIPs without Homebrew LLVM; no ban-list lint; horde 2 has no shell (catalogue R1) | The probe passes on the horde 2 shell's entry points and on each enrolled engine's render, in CI or a receipted nightly; a ban-list lint covers `h2/`; "paths outside the seeded schedule" is an accepted limit (decision 2) |
| R2 thread races | `tsan_stress_check`, `load_handoff_check` (ADR-200); CI `sanitize` | Legacy only and Mac-only; read-only races accepted until the park trigger (ADR-186 §6) | ADR-200's harness and oracles pass on the horde 2 shell with no accepted races (its shell rules forbid main-thread reads of audio memory), in CI or nightly |
| R3 NaN, Inf, denormals | `nan_latch_check`, `hostile_events_check`, `denormal_check`, `stability_check` | The engine has no output guard and ends in tanh; the legacy count reaches no user; flush-to-zero is set nowhere (catalogue R3) | The horde 2 shell has a latch-and-report guard whose count the user can see; flush-to-zero is ruled (decision 6); engine fuzz and soak report zero non-finite samples |
| R4 rate and block size | Per-mechanism checks on the legacy core: `sr_check`, `blocksize_check`, `samplerate_check`, `subdiv_check`, `swarm48_check` | No reference-patch matrix; the engine still owes determinism across block sizes (B404) | Every enrolled engine passes 44.1/48/96/192 kHz × blocks 1, 7, 64, 512 and varying: bit-identical across block sizes, inside ratified feature bands across rates, gray zone sent to listening |
| R5 long-run drift | `stability_check`, 60 simulated seconds of the legacy SAW core (`verify` full) | No 4-hour soak; nothing on the engine | A nightly 4-hour soak per enrolled engine holds level, DC, spectral-centroid and pitch bounds, its drift control fires, and the receipt is fresh |
| R6 edge stability | `notefuzz_check`, `hostile_events_check`, `kstuck_probe` (legacy). The JS gauntlet's `edge` fuzzer measures but does not gate C++ (`tools/patchspace/README.md`) | No C++ property fuzz of the parameter space | A seeded fuzz reaches every parameter key of each enrolled engine (counted), with sweeps and stacked changes, asserting finite output, a bounded pre-limiter level and silence after release; failures are kept as seeds |
| R7 state compatibility | `statefix_check`, `state_check`, `modreadback_check`, `param_id_lock_check` (397 ids; unmerged) | Legacy only; defaults not locked; no horde 2 manifest (B308 H4); nothing released, so no corpus | horde 2's lockfile gates ids, ranges and defaults; state has a schema version with migrations as tested data (ADR-186 §5); a corpus frozen at each release tag renders unchanged in CI |
| R8 host edge cases | pluginval strictness 10 in CI (`validate-windows`, `validate-macos`) | auval is manual; no clap-validator, CLAP's only validator (B427); no two-instance test; Reaper and Bitwig UNVERIFIED (`docs/ROBUSTNESS.md` §4) | On signed horde 2 builds pluginval, clap-validator and auval are gates; two-instance isolation passes; one hands-on trial pass per target host is recorded (B427, ADR-197 answers) |
| R9 perceptual quality | `alias_check` (legacy). `tools/patchspace/metrics.mjs` measures the JS engine, not C++ | No per-patch true peak, DC, sub-20 Hz or mono-sum gate. At os 1, five of six heavy presets cross the fitted alias line, yet the human passed them by ear (B445, B351) | Each reference patch of each enrolled engine has the five measurements against ratified three-band thresholds, with a red-flags report and the gray zone in the listening queue |
| R10 parity-golden trap | Parity checks, `divergence_ledger_check`; only `twocluster_check` and `svf_check` are prototype-independent | Few closed-form tests; no N-of-M count; parity is provisional (ADR-187) | A closed-form suite is wired per engine; every horde 2 golden is registered with a justification; each core has its ADR-187 §4 status |
| R11 build and platform | `h2_rules_check`, `h2_engine_fma_control`, `build_flags_check` (108 targets; unmerged), `sanitize`, `build-windows`, `sign-macos` | No x86_64 parity at Release flags; macOS builds the native architecture only; MSVC gets no floating-point flags (B454 items 4, 6) | Tolerance parity is green at shipped flags for every shipped compiler and architecture; shell targets set flags on purpose (B454 → B398); the release is signed and notarized |
| R12 CPU and licensing | `license_audit_check` (allow list ratified, ADR-205 unmerged), `release_path_check`, SBOM | CPU is measured by hand; min-spec ×1.5 is an assumption (`docs/port/cpu-ledger.md`); the worst known patch is over budget at the default (B445: 40.3 % against 34 %); one license item and ten release questions pending | The heavy-patch policy is ruled (B441 phase 3); each release carries a CPU receipt for the worst patch found by search; a work-counter ratchet runs in `verify`; the license items are closed |
| S1 symptom clamping | Listed as none. In fact `weakening_check`'s `dsp_guard` category already fails on any unapproved new `isfinite`, `isnan` or `std::clamp` in `src/` and `h2/` (its docstring) | Not credited; min/max and tanh-style clamps unseen; an approval need not name a root cause | It is listed; patterns are widened; a `dsp_guard` approval must cite a trace with a root-cause section, or a boundary or latching tag |
| S2 snapshot of the bug | None (`golden_pin_check` pins, but asks for no justification) | No registry | A registry enumerates every golden and pin; a new one without a named justification is red |
| S3 tolerance creep | `tolerance_registry_check` (138 pinned) | `.mjs` oracles outside; 75 entries without an ADR; `approved` is unauthenticated (catalogue S3) | Wired `.mjs` checks are inside; the 75 get refs; approvals are authenticated (P1) |
| S4 quiet disabling | `test_table_check`, `weakening_check` | Macro-built markers; `approved` is unauthenticated (catalogue S4) | Approvals are authenticated; the macro limit is formally accepted |
| S5 happy-path tests | None | — | Every engine or module under `h2/` is enrolled in the fuzz and sweep harness; an un-enrolled one is red |
| S6 wrong explanations | None | — | A diff-only reviewer runs on every PR as a required check, its prompt pinned to a passed planted-PR drill |
| S7 cross-agent contradiction | None. B275 planned a seam registry (`docs/contracts/SEAMS.md`); it is not written | — | The registry exists; any file with threading or shared state must belong to a seam; two open PRs on one seam are flagged |
| SEC-input, SEC-webview, SEC-supply, SEC-hygiene | The gates listed in `docs/armor/catalogue.json` (4, 4, 2 and 8) | Held privately under B446's disclosure rule | B446's private exit list for the category is empty; its gates run in CI or a receipted nightly; the independent review of ADR-203 item 1, stage 2, is done before any outside binary |

## 2. The key strategic question

**Recommended rule: engine now, shell at birth, legacy frozen.**

1. **Exit conditions are written against horde 2 only.** Legacy stays gated by `./verify` until the park trigger (ADR-186 §6) but gets no new gates. Two exceptions: class-α fixes (ADR-186 §1), and running an existing legacy check for longer.
2. **If the risk lives in DSP maths, build the gate now on `h2/engine/`.** The engine is standalone (`Engine(sampleRate)`, `set`, `render`; `h2/engine/engine.h`). ADR-187 §4(b) already demands the same invariant suite before any core's JS can be demoted, and the module 1.0 bar's "correct" group demands it per module (B439). One harness serves all three.
3. **If the risk lives in the shell, do not build on legacy.** Write the gate now as a B398 acceptance row and build it with the shell skeleton (`docs/H2-PLAN.md` step A2).
4. **"Enrolled" keeps green honest.** A small registry lists every engine and module under `h2/`. A new one turns R4, R5, R6, R9, R10 and S5 red until it joins the harness.
5. **Process rows (S1–S7, approvals, nightly, listening) do not depend on the shell.** Do them now.

| Invest | Rows |
|---|---|
| Engine now | R4, R5, R6, R9, R10; the parity half of R11; the counter half of R12; small engine-level slices of R1, R3 and R8 |
| Parked until the shell (B398) | R1, R2, R3's guard, R7, most of R8, R11's flags and signing. They stay `partial`, with gaps reworded "waits on B398" |
| Parked until the first release | R7's corpus (ADR-197 answer: the corpus starts with horde 2's first release) |
| Never on legacy | A user-visible NaN count, flush-to-zero, clap-validator fixes, a real-blob corpus (B308 H5; decision 11) |

## 3. Work packages

Sizes: S is about a day of agent work, M a few days, L a week or more. Tool names are suggestions.

| # | Size | Builds | Moves | Must-fail control | Who | Needs | Cost to run |
|---|---|---|---|---|---|---|---|
| P1 Authenticated approvals | S | A CODEOWNERS file naming the human for the approval ledgers: `docs/armor/weakening-baseline.json`, `docs/armor/tolerances.json`, `tools/param_id_lock.json`, `tools/build_flags_pin.json`, `tools/license_allowlist.json`, the self-digest references, and a new `docs/armor/acceptances.json` for hole expiries and accepted limits. The main ruleset requires code-owner review. Each ratchet tool prints in plain words what is being approved | S3, S4 → green (with decisions 2 and 12) | A ledger file with no owner is red; one live drill shows an agent-authored PR touching a ledger cannot merge without the human | Agent builds; the human flips the ruleset toggle and runs the drill (about 10 min) | — | fast +1 s; one click per approval |
| P2 Engine harness and enrolment | M | An adapter per engine, the reference patches (the seven ledger presets of `docs/port/cpu-ledger.md` plus extreme corners), shared features, `docs/armor/engines.json` and an enrolment check | S5 → green; base for P3–P9 | An un-enrolled engine directory is red; an adapter that returns silence fails a must-read-loud control | Agent | — | fast +1 s |
| P3 Parameter fuzz | M | Seeded patches from the existing sampler (`tools/patchspace/space.mjs`, `dependency_tree.json`) rendered by the C++ engine, with sweeps and stacked changes; bounds on the pre-limiter bus (tanh would hide a blow-up); failing seeds minimised and kept | R6 → green; R3 gains an engine NaN count | A corner-only instability and a NaN, planted behind fault flags, must be found within the PR-size run; every key must be hit | Agent; one thresholds ADR | P2 | full +1–2 min; nightly about 1 h on 4 CI cores |
| P4 Invariance matrix | M | The R4 matrix. Block axis bit-exact (the legacy bar is 0.0 exactly, `tools/blocksize_check.cpp`); rate axis by loudness, centroid, envelope, onset and pitch distance, in three bands | R4 → green | A planted per-call integrator breaks bit-identity; a planted fixed-sample-count time constant (the class `samplerate_check` once found) fails the rate axis | Agent; thresholds ADR; gray items to P10 | P2 | full about 1 min; full matrix nightly, about 20 min |
| P5 Nightly runner | M | A `./verify nightly` target and one thin scheduled workflow that only calls it, so later additions need no workflow edit. Receipts are CI artifacts, never commits. The lead's roundup prints the last green night; a monthly drill | Enabler; R1 and R2 lose "CI does not run it" if the macOS job works | Drill mode plants a fault and requires red; a receipt older than 48 h shows as a hole | The human pushes the workflow once (workflow edits are theirs, B448) | — | See "What runs where" |
| P6 Soak | M | Four simulated hours per enrolled engine at extreme settings and with note churn, one cell at 192 kHz (sample counters wrap soonest there). Legacy: `stability_check --seconds=14400`, no new code | R5 → green | A planted slow drift, exaggerated so the control takes seconds, must go red; a pre-aged-counter row | Agent; thresholds ADR | P2, P5; the overnight batch's first soak report (not on main when this was written) | About 6 core-hours a night, derived from the ledger's 8-voice figures: about 3 h on 4 CI cores (unmeasured) |
| P7 Analytical reference suite | M | Closed-form cases, listed below | R10 → green with P8 and each core's ADR-187 §4 status | Existing engine fault flags (`H2_ENGINE_FAULTS`) must turn the matching row red; each row has its own plant | Agent; two independent derivations must agree before a threshold is set (critic pass); thresholds ADR | P2 | full +20 s |
| P8 Golden-justification registry | S | Every golden and pin listed with its kind (closed form, prototype plus listening sign-off, or measurement) and evidence; prints "N of M" | S2 → green; R10's tripwire | A new golden with no entry is red; an entry citing an unwired check is red | Agent | — | fast +1 s |
| P9 Measurement suite | M | True peak, DC, sub-20 Hz, mono-sum loss and alias energy per reference patch. C++ renders are measured by the existing `metrics.mjs` (precedent: `tools/auhost/analyse.mjs`), which gains the missing metrics | R9 → green once bands are ratified | Constructed signals: an inter-sample over reads above 0 dBTP; planted DC; a 10 Hz tone; L = −R reads total mono loss; a naive saw reads aliased | Agent; the human rules bands and listens to the gray zone | P2, P10 | full about 1 min |
| P10 Listening queue page | M | One generated page per batch in gitignored `local/listening/`: level-matched blind A/B, a one-line question, the proxy reading, accept / reject / note. Capped at 15 items; verdict log as numbers only. Reuses `docs/design/listening-pass.html` and `listening_pass_check`'s blindness proofs | Dashboard part 3 stops being a hole; unblocks ADR-201 Q3 | A batch with a level mismatch, a visible label, or more than the cap is red | Agent builds; the human listens | — | Local seconds; 20 min of the human per session |
| P11 x86_64 parity at Release flags | S | (a) a Linux x86_64 Release leg reusing `tools/sanitize_oracles.sh`'s parsed oracle list; (b) the macOS x86_64 slice under Rosetta on the dev Mac (present, checked 2026-10-10); (c) an MSVC leg in `build-windows`. Tolerance parity, never keyed digests | R11 narrower, still partial | The FMA control, built for x86_64, must still fire | Agent; the workflow edit rides with P5; decision 8 | P5 | (a) about 10 min of CI; (b) about 3 min local |
| P12 Worst-case CPU | M | Work counters per voice-second with a ratchet in `verify` (the proxy ADR-187 §8 allows); a search for the worst patch (fuzz top-K by counter, ledger presets, factory bank); a per-release receipt measured under the frozen protocol (`tools/measure_h2_engine.cpp`), which the release job checks for existence and verdict | R12's CPU half → green after the policy ruling | Planted extra oversampling turns the ratchet red; a missing or over-budget receipt turns the release check red | Agent; the human rules the heavy-patch policy and measures once on a real min-spec machine | P3 | Negligible in verify; about 10 min per release on a quiet dev Mac |
| P13 Diff-only reviewer | M | A reviewer agent that sees only the diff, the PR text and the brief's checklist, and returns a verdict per pattern. A drill corpus of planted PRs, one per pattern plus clean controls; a `fast` check pins the reviewer's prompt to its last passed drill | S6 → partial; green if run as a required CI check (decision 9) | The drill | The lead writes the agent file (implementers never edit `.claude/`, B446) | — | One agent run per PR |
| P14 Root-cause rule for clamps | S | Lists `weakening_check` on S1, widens its patterns, and makes a `dsp_guard` approval cite a trace | S1 → green | A planted clamp with no trace is red; a tagged boundary guard is green | Agent; the human approves the one-time re-baseline | P1 | fast, negligible |
| P15 Two-instance isolation | S | One engine instance must render bit-identically whether or not a second runs beside it, on the same thread and on another (under CI's ThreadSanitizer leg) | R8 narrower, still partial | A planted shared mutable static is red | Agent | P2 | full +5 s |
| P16 Engine RT probe, lint, flush-to-zero probe | S | RealtimeSanitizer over the engine's render on the 543 scenarios; a ban-list lint over `h2/`; both self-digests re-run with flush-to-zero on, reporting how many of 543 rows change | R1, R3 narrower, still partial | A planted allocation is reported; a planted container growth in `h2/` is red | Agent; the human rules decision 6 on the number | — | full +1 min, dev Mac |
| P17 Shell acceptance list | S | One page for B398 turning every parked gap into a named gate with its control: ADR-200's rules and harness, the static-warm rule (B448 B3), the guard and flush-to-zero, the lockfile with defaults, state schema and migrations, flags per target (B454 items 2–7), validators (B427), the corpus mechanism, parser fuzzing | Nothing now; unblocks the shell | — | Agent drafts; the human ratifies with B398 | Decisions 6–8 | — |
| P18 Shell gates | L | Builds P17 on the shell skeleton. Includes clap-validator (a pinned download, so a human gate), auval as a gate, and host trials on free trial versions | R1, R2, R3, R8, R11 → green; R7 after the first release | Per P17 | Agent; the human runs the trials and the Windows test (B447) | B398 | Set in P17 |
| P19 Seam registry gate | M | Built on B275's planned registry, not a second one: a static membership check plus an open-PR overlap check | S7 → green | A new atomic in an unregistered file is red | Agent; the human rules the seam list (B275 (a)) | B275 P1 | fast +1 s |

**P7's closed-form cases.** K = 0 equals the sum of independently rendered voices. Two oscillators lock exactly when their detune is no larger than the coupling, at an offset of asin(detune ÷ coupling). With zero detune the phase gap decays as tan(ψ/2)·e^(−Kt). Mean frequency is conserved under symmetric coupling. Order goes to 1 at strong positive K and to 0 (splay) at K = −1 with zero detune. Output is unchanged by relabelling members or rotating every phase. Pitch matches equal temperament. For filters (the decimation biquads and DC blocker now, the filter module later): magnitude and phase against the exact transfer function, and envelope times in seconds.

**Which standards apply to P9.** ITU-R BS.1770 supplies the true-peak method (at least 4× oversampled) and K-weighted loudness. EBU R 128 with Tech 3341 supplies the gating and the short-term window, used for level jumps on preset change. Only the methods apply: the broadcast targets (−23 LUFS, −1 dBTP) are delivery rules, not synth thresholds. AES17 defines THD+N, which is meaningless for an oscillator that makes harmonics on purpose, so it is held for linear modules (EQ, FX filter). DC, sub-20 Hz energy, mono-sum loss and alias energy have no standard: they are in-house definitions, with alias energy on B346's estimator. Current revision numbers were not checked in this session (unverified).

**What runs where (P5).**

| Tier | Where and when | Runs | Proof |
|---|---|---|---|
| PR | GitHub, every PR (`.github/workflows/ci.yml`) | `verify fast`; the Linux sanitizer legs over every compiled oracle; Windows build and pluginval | Required status check |
| Full | The dev Mac, before an item is called done | `verify full`, including the Mac-only sanitizer checks (ADR-199) | `.harness/last-verify.json`, local only |
| Nightly, Linux | GitHub, scheduled on main | Soak, deep fuzz, full invariance matrix, x86_64 Release parity | Run history and artifacts |
| Nightly, macOS | GitHub macOS runner with Homebrew LLVM (ADR-199: "a CI job is still possible later") | `verify full` with `rtsan_check` and `tsan_stress_check`. Their planted-fault pairs make a floating toolchain safe. Keyed self-digests SKIP there by design | Run history |
| Dev Mac only | Idle, behind the load guard | CPU ledger rows (a named machine, ADR-187 §8), keyed self-digests, Rosetta parity, auval | A receipt in gitignored `local/armor/`, read by the lead's roundup |

CI cost is zero if the brief's recorded answer holds ("public repositories get free Actions minutes"; a 6-hour job limit). Whether that covers macOS runners is unverified.

## 4. Recommended order

**Wave 1: trust, the template, and the worst outcomes.** P1, P2, P3, P4, P5, P6, P8, P14, P16, P17.
- P1 first. Every later package adds thresholds, and a ratchet is only worth its approval.
- P2 next: one harness makes P3–P9 cheap and gives every later module (B452) its template before it is ported.
- P3 buys the most risk per effort. Row 6's cost is "screaming feedback or silence", it reuses the existing sampler, and its output feeds P6's extreme patches and P12's worst-case search.
- P17 costs a day and is what the shell is waiting on: the brief says retrofitting realtime and state discipline only gets more expensive.
- Conditional result: R4, R5, R6 and S1–S5 green.

**Wave 2: what needs the human's ears or a ruling.** P9 with P10 (a gray zone needs a queue), P7, P11, P12, P13, P15.
- P10 can start early with the click-ceiling batch ADR-201 already owes.
- P12 waits on the heavy-patch policy.
- Conditional result: R9 and R10 green; R11 and R12 narrower; S6 partial.

**Wave 3: with the shell skeleton, before any public build.** P18 and P19. This follows the brief's own "before any public build" list. `seams` is already on the critical path (`docs/H2-PLAN.md`), so P19 rides on work the path needs anyway.

Within a wave, packages have disjoint new files. The lead does the shared wiring (`verify`, `CMakeLists.txt`, the catalogue), as in Waves A to C.

## 5. What not to do in phase 2

- **No new gates on the legacy shell.** It is frozen and will be deleted at the park trigger (ADR-186 §1, §6).
- **No stopwatch in `./verify`.** ADR-187 §8 forbids it; P12 gates on counters and receipts.
- **No audio, nightly receipts or generated reports committed by feature PRs.** The repo is public (ADR-187 §7), and committed generated reports break parallel PRs (B446's method gaps).
- **No mean-field Kuramoto prediction as a sharp threshold.** Its error at 8–64 members is about 12–35 % (B441 research). Use exact small-N forms.
- **No broadcast loudness targets as thresholds, and no thresholds set by aspiration.** Use today's measured worst plus a stated margin, the posture of `tools/stability_check.cpp`, ratified in one ADR per wave.
- **No bit-exact digests on other platforms.** They are keyed to one compiler and Node (`h2/README.md`). Use tolerance parity elsewhere.
- **No scripted DAW matrix yet.** The human owns none of the hosts, and the shell does not exist (ADR-197 answers).
- **No saved-state corpus before the first release.** Until the stability line, saved state may change sound on purpose (ADR-186 §5(f)).
- **No language model inside `./verify`.** It is not deterministic. P13 runs beside it, pinned by its drill.
- **No second metrics library or second seam registry.** Reuse `tools/patchspace/metrics.mjs` and B275's.
- **No row turned green by rewording its gaps.** Section 1's exit conditions are the test.

## 6. Decisions for the human

1. **Adopt the rule in section 2.** Recommend yes. Cost: R1, R2, R7 and R8 stay amber until the shell exists.
2. **Let a green row carry "accepted limits".** Some gaps can never close, such as R1's "paths outside the seeded schedule". Without this R1, R2, S3 and S4 can never be green, and a board that is always amber gets ignored. Recommend a human-approved, expiring `accepted_limits` field kept in the code-owned file. Cost: one ADR.
3. **Authenticate approvals with code-owner review (P1).** Recommend yes. Cost: one settings change, then one click per approval. Alternative: signed approvals with a hardware key (a purchase and more friction), not recommended now.
4. **Approve the nightly workflow, and let a row be green on a nightly gate.** Recommend yes, with a 48-hour freshness rule; otherwise the soak can never count. Cost: one workflow push. Also decide whether the dev Mac may run about 15 minutes of idle jobs each night.
5. **Accept "measured worst plus margin, three bands", one thresholds ADR per wave.** Recommend yes. Cost: about 10 minutes reading a table per wave.
6. **Flush-to-zero in the horde 2 shell.** Recommend ruling after P16's number. If all 543 digests are unchanged, turn it on per block from day one; no migration is owed, since no horde 2 state has been saved. If not, amend row 3 to accept the software snapping the cores already do.
7. **The horde 2 output guard.** Recommend copying the latch-and-report guard forward (ADR-186 §4) and showing its count in the new GUI, as B398 acceptance rows. No cost now.
8. **Is Intel macOS a 1.0 platform?** The records disagree (see section 7). If yes, add a universal build and the Rosetta leg. If no, read row 11's "both archs" as macOS arm64 plus Windows x64.
9. **Where the reviewer runs.** Recommend lead-run now (free; S6 stays partial), then as a required CI check before any outside binary, together with ADR-203's different-model-family rule. Cost then: an API key the human creates, and spend per PR.
10. **The CPU gate's form.** The brief says "gated per release"; ADR-187 §8 says never a verify gate. Recommend P12's receipt plus counters. It needs the heavy-patch policy first (B441 phase 3), because the worst known patch is over budget today, and one measurement on a real min-spec machine to replace the assumed ×1.5 (about 10 minutes on the human's Windows PC).
11. **Skip the legacy real-blob corpus before the freeze tag** (B308 H5; `docs/H2-PLAN.md` open decision 37). Recommend skipping, in line with "I'm not worried about legacy sessions."
12. **Tolerance registry scope** (open since Wave A). Recommend only the `.mjs` checks that `verify` runs, not lab UI code.
13. **Listening sessions.** Pick one or two fixed times and confirm the cap of 15 items or 20 minutes (the brief). Recommend the first batch be ADR-201's click ceiling.

## 7. Open questions

- **ADR-205** is not on `origin/main`; it was read from the lead's unmerged branch.
- **The overnight soak's result** (batch item 2 in the unmerged B448 row) was not available.
- **Who authors and who merges PRs.** Code-owner review only works if the human is not the PR author. The records show an agent token account (B446) but not who clicks merge. Whether code-owner review binds at "0 required approvals" is unverified.
- **CI runner speed** against the dev Mac is unmeasured, so every nightly duration is an estimate. Whether Homebrew LLVM's sanitizers start cleanly on GitHub's macOS runners is unverified; ADR-199's crash was specific to one toolchain.
- **Intel macOS:** ADR-186 §2 says "universal", B432 says "macOS and Windows x64", and B454 item 4 finds only the native architecture built.
- **clap-validator:** whether a pinned prebuilt binary exists for both platforms is unverified.
- **Closed-form tests** read member phases through the engine's existing `voice(i)` accessor. Whether every case in P7 can avoid a new accessor was not traced. A new one would be an access-only edit; the B441 ruling on measurement-only defines suggests no JS twin is needed, but that reading is the lead's to confirm.
- **Do bit-identity pins count as goldens under S2?** Recommend yes, registered as "prototype parity at pin; sign-off pending", so "N of M" stays honest.
- **Edit F against the parallel-PR problem.** Edit F puts lane reports in `docs/armor/`. This proposal assumes only the lead's records PRs write there. If the human wants per-lane committed reports, P5's receipts design changes.
- **The module cutoff and first public build dates** are not set (`docs/H2-PLAN.md`), so Wave 3 has no deadline.
