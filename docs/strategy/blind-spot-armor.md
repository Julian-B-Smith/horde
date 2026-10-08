# horde — Blind-Spot Armor

**Status: RATIFIED 2026-10-08 (ADR-197; ROADMAP B448).** This is the human's brief of 2026-10-08,
with edits A–H applied and the open questions answered. The human: "All suggested edits ratified."
The original is kept privately. The edits are summarised at the end.

## Purpose

horde's worst risks are the errors that pass PR review, pass `./verify`, and sound fine in Ableton on
a good day. They live in fields the human can steer but can't audit line by line: realtime systems,
numerical analysis, host integration, platform builds, and licensing. This brief turns each of those
into checks that run whether or not anyone remembers them.

**Readers.** The horde 2 governing agent, and any subagent touching DSP, state, build, or host code.
The human reads the dashboard section and the phasing.

**Core principle.** Expertise the human doesn't hold must live in an enforced check, not in review.
Each category below gets an automated gate in `./verify`, plus one plain-language tripwire the human
can read without domain knowledge. A category with no gate is an open hole, and the dashboard says so.

**Scope.** Security hardening has its own method (B446; the fleet brief hypersaw-005). Both are run
from one armor catalogue and one dashboard: a security finding and a correctness risk are rows of the
same kind. Parity with the browser prototypes stays a gate, but it is not treated as proof of
correctness (see row 10, and ADR-187).

## Risk register

Twelve error categories, ordered by how badly they can hurt a user and how invisible they are to
review. Each one hides from a skim and from casual listening. The armor column is the gate that
catches it instead.

| # | Category | How it hides | What it costs | Armor (enforced gate) | The human's tripwire |
| --- | --- | --- | --- | --- | --- |
| 1 | Realtime safety on the audio thread | Memory allocation, locks, logging, file I/O, or a shared\_ptr release in the callback works fine until the machine is busy | Random dropouts and clicks in users' sessions, impossible to reproduce | Build with Clang's RealtimeSanitizer and fail CI on any violation; a lint of the ban list (new/delete, mutex, std::function capture, vector growth); an allocation counter in the callback with a planted fault | "RT violations: 0" |
| 2 | Thread races between UI, host, and audio threads | Parameter changes, preset loads, and state saves collide only under specific timing | Corrupted state, crashes on preset change, rare garbage audio | ThreadSanitizer over a stress harness that hammers params, presets, and save/load during render | "TSan stress: clean" |
| 3 | NaN, Inf, and denormals | One NaN poisons a filter or coupling state permanently; denormals spike CPU on decaying tails | A silent channel, or a full-scale blast into speakers and ears | A NaN/Inf detector in every engine's output; flush-to-zero set per block; an output guard that LATCHES AND REPORTS (counts events), never silently clamps | "NaN events: 0", plus the guard's trip count |
| 4 | Sample-rate and block-size dependence | Developed at one rate and block size; behaviour changes at 96k, 22.05k, block size 1, odd sizes, or block sizes that vary call to call | Presets sound different in other DAWs or at export; coupling dynamics change speed | Invariance tests: render each reference patch at 44.1/48/96/192k and blocks of 1, 7, 64, 512 and randomly varying; compare perceptual features, not bits | "Rate/block invariance: pass" |
| 5 | Long-run drift | Phase accumulators, Kuramoto integration, OU walks and inertia states drift or blow up only after minutes or hours | Pads that slowly detune, go quiet, or explode mid-set | Soak test: 4 hours of offline render per engine at extreme settings, with level, DC and spectral-centroid bounds | "Soak 4h: pass" |
| 6 | Numerical stability at the edges | Integrators and feedback loops are stable at defaults but not at extreme K, inertia, feedback or modulation depth | Screaming feedback or silence at the corners of the parameter space | Property-based fuzzing of the full parameter space, including automation sweeps and modulation stacking, with output-bound assertions | "Param fuzz: N runs, 0 failures" |
| 7 | State and preset compatibility | Renamed parameter IDs, changed defaults or new curves alter old saved sets with no error | Users' finished songs change sound after an update; trust is lost permanently | A frozen corpus of saved states from every horde 2 release, loaded and rendered in CI; parameter IDs append-only; every default change requires a migration and an ADR | "Old sets render unchanged: yes" |
| 8 | Host integration edge cases | Fine in Ableton; different in Bitwig, Reaper, Logic, FL Studio. Offline render, bypass, transport jumps, tempo changes, multiple instances, reactivation | Crashes or wrong behaviour in DAWs the human doesn't use | pluginval at strictness 10, clap-validator, auval; a scripted host matrix (see the host-matrix answer); a test that two instances share no static state | "Validators: all green, N hosts tested" |
| 9 | Perceptual quality not heard on monitors | Aliasing, DC offset, subsonic energy, inter-sample peaks and poor mono compatibility hide on laptops and nearfields | Mixes that pump, mud on club systems, harsh fold-back on bright patches | Automated measurements per reference patch: alias energy, DC, sub-20 Hz energy, true peak, mono-sum loss; thresholds stored as gates | A spectrum report per engine, red flags only |
| 10 | The parity-golden trap | C++ matches the JS prototype bit for bit, including the prototype's own bugs (WARP's clicks, for example) | Bugs certified as correct by the oracle itself | Independent reference tests that don't derive from the prototype: closed-form cases (K = 0 equals independent saws; K = 1 locks), conservation and symmetry checks, metamorphic tests | "Goldens with independent check: N of M" |
| 11 | Build and platform divergence | -ffast-math silently disables NaN checks; FMA contraction and x86-vs-Apple-Silicon float differences break bit-parity; unsigned binaries get blocked | Gates that pass but test nothing; installs that fail on users' Macs | Compiler flags pinned and audited in CI; parity tests run on both architectures; signing and notarization in the release pipeline | "Both archs green; release signed" |
| 12 | Worst-case CPU and licensing exposure | Average CPU looks fine, but the worst patch spikes; a dependency's license conflicts with a commercial release | Dropouts on modest machines; legal problems if horde goes commercial | The ratified CPU budget (B439: the E-6 envelope, 44.1 kHz at a 128-sample buffer, 8-voice poly, min-spec ×1.5 an M3 core; 1× oversampling by default, 2× as HQ, ADR-191), gated per release on the worst-case patch at the default; an automated dependency license audit (SPDX) with an allow list | "Worst-case CPU: X% of budget"; "Licenses: clear" |

Preset files also count as untrusted input. The preset parser gets fuzzed (malformed, truncated,
huge, hostile files), which is where this brief meets the security work.

## Agent-signature failure modes

Agents rarely write obviously broken code. Their typical failure is making a problem look solved.
Each pattern below is easy to miss in a skim and should be detected automatically, or flagged by a
dedicated reviewer agent that sees only the diff.

- **Symptom clamping.** A NaN, click or overflow is "fixed" with a clamp, a guard or a limiter
  instead of at its root cause. Rule:
  - Any new clamp, isfinite guard or limiter inside DSP code requires a trace explaining the root
    cause.
  - **Input validation at a trust boundary is exempt.** Checking host-supplied values, file contents
    or bridge arguments where they enter is required, not symptom clamping.
  - **An output safety guard is allowed only if it latches and reports** (row 3). It may never zero
    or clamp silently.
- **Snapshot of the bug.** A new test asserts whatever the code currently outputs. Rule: every new
  golden must name its independent justification (a closed form, the prototype plus a listening
  sign-off, or a measurement).
- **Tolerance creep.** Comparison tolerances widen a little per PR until the test means nothing.
  Rule: a tolerance registry check inventories every tolerance (in `specs/ACCEPTANCE.md` and in each
  check) and fails on any change not approved by the human. Any change is a gate-weakening event.
- **Quiet disabling.** Tests skipped, warnings suppressed, sanitizers turned off "temporarily". Rule:
  a CI check counts skips, suppressions and disabled sanitizers, and fails on any increase.
- **Happy-path tests.** Tests exercise defaults only. Rule: every engine test suite includes
  extreme-parameter and automation-sweep cases.
- **Confident wrong explanations.** A PR description gives a plausible reason that doesn't match the
  diff. Rule: the reviewer agent checks that the stated cause is actually what the diff changes.
- **Cross-agent contradiction.** Two agents fix the same seam in incompatible ways. Rule:
  shared-state and threading changes go through the seam registry before merge.

## Parallel verification lanes

Most of the armor produces numbers, so it can run as independent subagent lanes that never need the
human's ears. Each lane owns its gates, its fixtures and one report file in `docs/armor/`; anything
sensitive stays in the gitignored `local/`. Lanes write only to their own files (writes stay home),
and one aggregator merges the reports into the dashboard.

| Lane | Owns (register rows) | Verdict source | Example checks |
| --- | --- | --- | --- |
| Realtime and threading | 1, 2 | Sanitizer exit codes | RealtimeSanitizer, ThreadSanitizer stress harness, allocation counter in the callback |
| Numerics | 3, 5, 6 | Bounds and assertions | Parameter fuzzing, soak runs, NaN/denormal counters, integrator energy bounds |
| Invariance | 4 | Feature distance vs threshold | Same patch across sample rates and block sizes; compare loudness, centroid, envelope |
| Measurement | 9 | Published standards | True peak and loudness (ITU-R BS.1770 / EBU R128), THD+N, alias energy above the fundamental's harmonics, DC, sub-20 Hz, mono-sum loss |
| Analytical reference | 10 | Closed-form theory | Filter magnitude/phase vs the exact transfer function; Kuramoto order parameter r vs K against mean-field predictions; K = 0 equals independent saws; splay at K = −1 |
| Compatibility | 7, 8 | Validator results, bit or feature match | pluginval, clap-validator, auval, saved-state corpus renders, two-instance isolation |
| Performance | 12 | Budget vs measurement | Worst-case patch CPU, per-engine cost, oversampling cost, preset-change spikes |
| Build and supply chain | 11, 12 | Pass/fail | Both architectures, pinned flags, signing, license audit, preset parser fuzzing |
| Adversarial | All | Did the gate catch it? | Planted-fault drills, a diff-only reviewer checking the agent-signature patterns |

**Turn ear tests into numbers wherever possible.** Many things that seem to need listening have good
machine proxies:

| What you'd listen for | Machine proxy |
| --- | --- |
| Clicks and zipper noise | Sample-to-sample discontinuity and high-band transient detectors |
| Harshness | Alias energy, spectral flux |
| Level jumps on preset change | Short-term loudness delta |
| Stereo collapse | Correlation and mono-sum loss |
| "Did it change?" | Feature distance against the last approved render |

Each proxy gets three bands: pass, fail, and a gray zone. Only the gray zone goes to the human.

## Listening batch

The human's ears are the scarcest resource in the system, so they get a scheduled, capped queue
instead of interruptions. Nothing reaches them mid-flow; agents keep working on other lanes while
items wait.

1. **Only three things qualify.** A proxy landed in its gray zone; a new golden needs sign-off; or an
   intentional sound-design change needs approval. Everything else is decided by numbers.
2. **One or two fixed sessions a day,** at times the human picks, capped at about 15 items or 20
   minutes. Overflow rolls to the next session, ranked by how many other items it blocks.
3. **Each item is ready to judge in seconds.** It brings:
   - a level-matched A/B pair (blind where it matters);
   - a one-line question ("click at the loop point?");
   - the proxy reading;
   - three buttons: accept, reject, note.

   One generated listening page per batch holds the whole session.
4. **Verdicts feed back.** Every decision is logged with its proxy values and used to tighten the
   gray-zone bands, so the queue should shrink over time. A lane whose items the human always accepts
   gets its band narrowed; one they keep rejecting gets a new or stricter proxy.
5. **No silent blocking.** Unjudged goldens stay pending, and their lane carries on with other work.
   The aggregator reports anything waiting more than two sessions.

The only things that justify interrupting the human are failures the machine already caught and
can't route around, such as a broken main branch. Those come as one notice, not a stream.

## The human's dashboard

The human reads one generated report per merge window, instead of skimming every PR for things they
can't judge. It has three parts and fits on one screen.

1. **Armor status.** One line per risk-register row: gate present or missing, last result, trend. A
   missing gate shows as a hole, not as green.
2. **Gate-weakening events.** Any tolerance change, skip, suppression, disabled sanitizer, new clamp
   or default change since the last report. These need the human's explicit approval; nothing else
   does.
3. **Listening queue.** The scheduled, capped listening batch, with renders the system flagged for
   human ears: invariance failures near threshold, new goldens awaiting sign-off, and the
   worst-measured patch per engine. This is where the human's sound-design judgment does the work
   that tests can't.

The lead's per-message FOUNDATIONS roundup carries one armor line (for example "armor: 0/12 green, 11
partial, 1 hole"). The dashboard carries the detail.

Rule of thumb: if the dashboard is all green and the listening queue is empty for weeks, that is
itself suspicious. Have an audit agent try to break something on purpose and confirm the gates catch
it (a planted NaN, a planted allocation, a planted ID rename).

## Phasing for horde 2

horde 2's engine already exists, so phase 1 is a retrofit, done now. Retrofitting realtime and state
discipline only gets more expensive the longer it waits.

1. **Now (retrofit).**
   - Realtime sanitizer and thread-sanitizer harness.
   - NaN/Inf detector with a latching, reporting output guard; flush-to-zero.
   - Pinned compiler flags, append-only parameter IDs, and the gate-weakening counter.
   - The license audit, moved up because a commercial release is becoming likelier.
   - Covers rows 1–3, 7, 11 and 12 (licensing).
2. **With the next engine milestone.** Rate/block invariance, parameter fuzzing, independent
   reference tests and the perceptual measurement suite (rows 4, 6, 9, 10). The composed engine sets
   the template every later engine copies.
3. **Before any public build.**
   - Soak tests, the host matrix with validators, and the worst-case CPU budget.
   - The saved-state corpus, signing and notarization.
   - Preset parser fuzzing.
   - Covers rows 5, 8, 11 and 12.
4. **Ongoing.** A dashboard per merge window, and a monthly planted-fault drill to prove the gates
   still bite.

## Standing instructions

For the governing agent:

- Add each risk-register row to `./verify` as a named gate, or list it as missing on the dashboard
  until it exists.
- Never close a correctness issue by weakening a gate; route any such change to the dashboard for
  approval.
- Assign a reviewer agent that sees only diffs and this brief, with the agent-signature patterns as
  its checklist.
- Write an ADR for every default change, parameter ID change or new tolerance. A default change also
  needs a migration, so that state saved before it still sounds the same.

## Answers to the open questions (the human, 2026-10-08)

- **Host matrix.** Bitwig, Reaper, Logic and FL Studio are the targets, but the human owns none of
  them, so the matrix leans on automation first: pluginval, clap-validator, auval, and the
  two-instance and offline-render tests. A hands-on pass in each host before a public build can use
  free evaluation or trial versions.
- **Soak and host matrix.** A 4-hour soak "sounds about right". Public repositories get free Actions
  minutes, and 4 hours fits the 6-hour job limit, so the soak can run nightly on main.
- **Commercial release.** "It's becoming likelier." The license audit moves to phase 1.
- **Legacy sessions.** "I'm not worried about legacy sessions. They can stay on the old system."
  horde 2 is a clean break for saved state. Its compatibility corpus starts with horde 2's first
  release. Legacy presets still import (ADR-193 D7).

## Edits applied at ratification

| | Edit |
| --- | --- |
| A | The human's name → "the human" (the public-repo convention) |
| B | Phase 1 is a retrofit, now |
| C | Row 12 points at the ratified B439 budget, not a 16-voice 2× case |
| D | Symptom clamping exempts trust-boundary input validation; an output guard must latch and report |
| E | A default change also needs a migration (applied first to the B445 os 1 flip) |
| F | Lane reports live in `docs/armor/` (sensitive items in `local/`), not `traces/` |
| G | A tolerance registry check, not a single tolerance file |
| H | The dashboard carries the missing-gate list; the lead's roundup carries one armor line |
