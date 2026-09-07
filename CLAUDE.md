# Agent Charter — HYPERSAW

Everything above §Domain is the invariant harness layer. Do not edit it
per-project. Project-specific facts live in §Domain and in ROADMAP.md.

## Truth contract

- **ROADMAP.md is the single source of truth.** Task state, acceptance
  criteria, invariants, and open questions live there and only there. If the
  conversation and ROADMAP.md disagree, ROADMAP.md wins; if ROADMAP.md is
  wrong, fixing it is the first task.
- **Passing ≠ done.** Done = `./verify full` green AND the ROADMAP acceptance
  criteria satisfied AND a trace entry written in `traces/`. Never collapse
  these into each other.
- **Grounded refusal is a success class.** "I cannot do this within the brief
  because X" with evidence is a correct output. Guessing to appear productive
  is a failure.
- **Reduce, never invent.** Prefer deleting code, tightening a contract, or
  reusing an existing mechanism over adding a new one. Every new abstraction
  must displace at least as much complexity as it introduces.
- **Review beats are visual-first.** When presenting completed work at a
  gate (phase close, ratification request, PR), lead with a visual — a
  self-contained HTML report, render set, or live demo — sufficient to
  evaluate the change WITHOUT reading the diff, plus evidence it works and
  open questions. Code diving is the fallback, never the ask.

## Provenance

- Every nontrivial claim about the codebase must cite its evidence: a file
  path and line, a verify run, or a ROADMAP entry. No provenance → phrase it
  as a hypothesis, not a fact.
- Every merged change gets an entry in `traces/` (see the provenance skill):
  what changed, why, evidence consulted, verify result + git hash.

## Delegation policy (lead session)

- The lead plans, delegates, integrates, and is the **only** writer of
  ROADMAP.md. Subagents never touch it.
- Delegation briefs are self-contained: subagents start with zero conversation
  history. Every brief states (1) files in scope, (2) acceptance criteria
  copied verbatim from ROADMAP.md, (3) the verify target, (4) what is
  explicitly out of scope.
- Use built-in Explore for codebase reconnaissance. Use `implementer` for
  scoped changes, `verifier` for oracle runs, `critic` (Opus) for adversarial
  review of anything architectural, irreversible, or touching an invariant.
- One queue item per implementer dispatch. Parallel dispatches only for items
  with disjoint file scopes.
- Do not start work on an item whose acceptance criteria are missing or
  ambiguous. Surface the gap to the human; that is the deliverable.

## Oracle discipline

- Run `./verify fast` after any change set; `./verify full` before declaring
  a queue item done. Report oracle output verbatim — never summarize a failure
  into vagueness.
- A red oracle halts forward work. Fix or revert; do not stack changes on red.
- Never weaken a gate (skip a test, relax a threshold, mark xfail) without an
  explicit human decision recorded in ROADMAP.md.

## Human gates

Stop and ask before: deleting files, changing the public interface of
anything, editing `./verify` or the gates it runs, adding a dependency,
any git operation beyond add/commit on the working branch, and anything §Domain
lists as protected.

---

## §Domain — HYPERSAW

**What this is.** A synthesizer built from a family of experimental, responsive
engines with dynamical characteristics, oriented toward the sounds of hyperpop
(ADR-091). The founding engine — **SWARM SAW** (formerly SAW, then HYPERSAW; ADR-115) — is a
coupled-oscillator system (Kuramoto swarm over a kernel — "the supersaw taken
seriously as physics"); **SPECTRA** is its per-partial sibling; the first new
member is the **formant engine** (working name CANTO — FOF/pulsar grains, formants
as masses on springs, a register state R; `specs/SPEC-FORMANT.md`, prototype
`reference/formant-pulsar-fof.html`, not yet in the shell). A second CANDIDATE arrived
2026-08-18: **WARP** (`reference/distortion-engine.html`, `specs/SPEC-DISTORTION.md`) — NOT a
fourth engine but the prototype for **FX-C**, the morphing waveshaper with
hysteresis; it is a shared post-stage every source hands off to (ADR-092), and
carries the same unseeded-RNG blocker CANTO does. A third CANDIDATE arrived
2026-08-25: **STATION** (`reference/station.html`, `specs/SPEC-STATION.md`, ADR-122) — a
3-operator PM engine with LFSR noise, the traditional-synthesis workhorse; same
unseeded-RNG blocker (Wave RAM randomize, one sanctioned edit: seed it). CLAP-native instrument
plugin, VST3 via clap-wrapper (ADR-002). The DEVICE is **horde** (ADR-114,
settled 2026-08-23); HYPERSAW is the founding ENGINE, the repo name, and the
frozen plugin id. Design docs: specs/SPEC.md (the instrument), specs/ACCEPTANCE.md (the
oracle contract), docs/PRIOR-ART.md, docs/PARKED.md.

**Stack & entrypoints.** C++20 CLAP-first plugin: impl in `src/hypersaw_clap.cpp`
(static lib + exported entry, clap-first idiom), wrapped to VST3/AUv2 by
clap-wrapper (`libs/` submodules pinned: clap 1.2.10, clap-wrapper v0.15.1).
Build: `cmake -S . -B build-release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release`
then `cmake --build <abs>/build-release -j` (absolute path — sandbox resets cwd).
Reference implementation: seven single-file HTML prototypes plus one CANDIDATE
(`reference/formant-pulsar-fof.html`, `FormantCore` — a candidate until its masking
RNG is seeded, ADR-091) — three oscillator
labs `reference/swarmsaw.html` (SAW; v2 lineage, see ADR-011/012), `reference/swarmspectra.html`,
`reference/swarmdynamics.html` (cores `SwarmSynth` / `SpectraSynth` / `DynSynth`), the
Track E effects labs `reference/swarmfilter.html`, `reference/swarmphaser.html`, `reference/swarmtime.html`
(`FilterLab` / `PhaserLab` / `TimeLab`), and the experimental `reference/swarmalator.html`
(`Swarmalator`, ADR-048) — all spec-in-code (ADR-003). Ported C++ cores so far:
`swarm_core.h` (SAW+dynamics), `spectra_core.h` (SPECTRA), `force_core.h`
(shared force system, ADR-034), `filter_core.h` / `notch_core.h` (E1 effects),
`swarmalator_core.h` (swarmalator). Build/test = `./verify fast|full` (fifteen
gates: nine parity/trajectory chains — parity · trajectory · force · spectra ·
filter · notch · swarmalator · glide · time — plus ten behavioural/invariant
probes: state · notefuzz · rtsafety · subdiv · samplerate · routing · paramscope ·
mpe · preset · waveshape). This Mac: Command Line Tools only, CMake
with `-G "Unix Makefiles"`, absolute build paths (sandbox resets cwd) — see
the global CLAUDE.md audio-plugin section before any build/install/validate.

**Domain invariants** (the critic checks against these):
- The DSP core is deterministic code; no LLM judgment in the runtime path.
  Same seed + note order → identical output. mulberry32 streams only; no
  wall-clock reads anywhere in the core (SPEC §5.7).
- C++ correctness is defined as parity with the JS reference (L0-1, ε=1e-6
  RMS) plus the L0 trajectory criteria — never as plausible-sounding audio.
  Any intentional divergence from the prototypes requires an ADR.
- All slew/time-constant math is expressed in seconds and converted to
  per-tick coefficients (16-sample tick = 2756/s at 44.1 kHz). Hand-tuned
  per-tick constants are banned — ADR-009 records the trap.
- Real-time thread allocates nothing, calls no external provider (doctrine:
  hot paths never call the provider).
- Acceptance numbers in specs/ACCEPTANCE.md are measured, not aspirational; they
  change only with a re-measurement on the reference implementation.

**Alias note.** This repo is public. Private sibling projects are referred to
by alias in all tracked files ("terrain sibling", …); the alias→name map
lives in `PRIVATE-NOTES.md` (untracked, local-only — ADR-014). Never write a
private sibling's real name into a tracked file.

**Protected paths** (human gate to modify): `specs/SPEC.md`, `specs/SPEC-EFFECTS.md`,
`specs/SPEC-SWARMALATOR.md`, `specs/SPEC-FORMANT.md`, `specs/SPEC-DISTORTION.md`, `specs/ACCEPTANCE.md`, `docs/PRIOR-ART.md`, the prototype HTMLs —
now seven: the three oscillator labs, swarmfilter/swarmphaser/swarmtime (Track
E, ingested 2026-07-18), and reference/swarmalator.html (experimental swarmalator engine,
ingested 2026-07-19, ADR-048), and `reference/formant-pulsar-fof.html` (formant
engine, ingested 2026-08-17, ADR-091; one sanctioned edit outstanding: seed its
masking RNG), and `reference/station.html` + `specs/SPEC-STATION.md` (STATION engine, ingested
2026-08-25, ADR-122; one sanctioned edit: seed the Wave RAM randomize), and
`specs/SPEC-INTENT-BUS.md` + `reference/intent-bus.html` (intent bus,
ingested 2026-09-01, ADR-152; one sanctioned edit: seed `reshuffle()`) — (they ARE the reference — an edit there is a spec change), `./verify`, golden render fixtures once they exist.

**Verify targets.** `fast`: leak gate + structure/manifest sanity now; grows
the L0 suite (parity + trajectories) from Phase 1 — seconds-to-minutes,
CI-blocking. `full`: fast + Layer-E behavioral checks with human sign-off —
human-paced, macOS-local (pluginval/auval/host loads are not CI-able here).

<!-- KNOWLEDGE-LOOP:START -->
## Self-Improving Knowledge Loop

Each session: read accumulated knowledge before acting, write distilled knowledge
after. This meta-layer sits on top of my primary role and never overrides it.

### Every session
1. **ORIENT** — Read INDEX.md in full (kept small on purpose). Pull ONLY the matching
   entries from LIBRARY.md into context. Never load all of LIBRARY by default.
2. **ACT** — Do the work, applying retrieved lessons. If a lesson proves wrong,
   correcting it outranks adding a new one.
3. **REFLECT** — Ask: "What did I learn that a future session needs and could not
   cheaply re-derive?" A lesson qualifies only if durable, evidenced (tied to a
   concrete trigger), and non-obvious. If nothing qualifies, write nothing.
4. **WRITE (atomic)** — Append the lesson to LIBRARY.md and a one-line pointer to
   INDEX.md in the same change. New lessons enter as `tier: candidate`; promote to
   `canonical` only on a second independent occurrence or human review.

### Write gate (anti-poisoning)
This loop feeds its own output back as input, so a wrong lesson, written once, is
retrieved and reinforced forever. Therefore: prefer not writing over writing
unverified; every lesson states what would falsify it; if a retrieved lesson
contradicts present evidence, trust the evidence and demote the lesson.

### Consolidation (periodic)
When LIBRARY exceeds ~30 entries, merge duplicates, delete superseded entries,
promote recurring candidates, tighten tags. Refactor it like code; don't grow it
like a log.

### LIBRARY entry template
`[Lxxxx] <title> | tier | added: YYYY-MM-DD | tags: … | lesson: … | evidence: … | falsifier: … | supersedes: …`
<!-- KNOWLEDGE-LOOP:END -->

<!-- kit:2.1.0 begin — mailbox scope, appended by /retrofit 2026-08-18 -->
## Mailbox

- **Briefs TO us land in `integrations/<sender>/` in THIS repo.** Visitors write
  there under the INTEGRATIONS mailbox exception and nowhere else in our tree;
  committing them is a resident act, so an uncommitted brief is still filed and
  still ours to land.
- **Responses to OUR briefs land in the PROVIDER's tree, not here — pull and
  read them.** A consumer that only checks its own mailbox cannot tell an
  answered brief from an ignored one. We lost a session to exactly that on
  2026-08-18: a brief's frontmatter still read `ball: provider` while the ruling
  sat committed beside it (LIBRARY L0037 — frontmatter is a claim, the tree is
  the fact).
- **Reading another repo's exchange is never out of bounds; acting on it is.**
  Read freely. If it concerns us — we are cited, or work is implied in our tree
  — **file a brief**: acting THROUGH the protocol is always in bounds. What is
  forbidden is acting on another repo's obligation directly, or raising it to
  the human as though it were ours. If it concerns nobody but the two parties,
  do nothing and say nothing.

  *This third clause deliberately diverges from the kit CHANGELOG's 2.1.0
  retrofit text ("exchanges between other repos are ignored"), which predates
  the INTEGRATIONS §3 correction of 2026-08-18 — a correction made on this
  repo's own brief (hypersaw-001 Q3). Divergence filed with autonomous.*
<!-- kit:2.1.0 end -->
