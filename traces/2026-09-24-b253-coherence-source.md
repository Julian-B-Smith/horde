# b253-coherence-source — Coherence 1/2 (swarm R) as mod sources in the mod matrix lab

- **Queue item:** B253 (ROADMAP row carried in PR #746, branch `lead-records-86`).
- **Why:** The human asked for R to be a modulation source ("please make sure it's in the Mod lab if it isn't already"). It was not in the lab. The shipped matrix has 22 sources, and slots 22/23 of `kMaxSources` 24 were free. The lab now carries Coherence 1/2 at 22/23 (unipolar 0..1). A real per-note Kuramoto swarm drives them, and the lab makes the two port questions B253 names concrete: the global projection rule and R → Coupling feedback.
- **What changed:** `docs/design/mod-matrix-lab.html` only.
  - BLOCK A (DOM-free) gains:
    - `class Swarm`, horde's mean-field law with each line cited to `src/swarm_core.h` (JP placement :1507, law 0 pitch :1805, σ floor :1853, km/sync/splay targets :1886-1895, B150 smoother :152/:391, R/ψ :1902-1911, force :1985-1992, kTick :54).
    - A three-slot voice pool that owns one swarm per oscillator per note.
    - Five projection rules (LOUDEST, MEAN, NEWEST, MAX, GAIN-WEIGHTED) and a PER-VOICE switch for the Coupling destinations.
    - OSC 1/2 Coupling as destinations.
    - `coherenceChecks()`.
  - BLOCK B adds three clusters: the swarms (phase circles in the ψ-rotating frame), the rule comparison (five lanes plus a table of the largest one-tick step), and the feedback phase plane (K vs R, with the loop's law line).
  - Also added: a Q3 answer, two scenes (`coh`, `fb`), `lab-review` set to `B207 + B253 · 2026-09-24`, and an audit that counts 24/24 slots plus the self-check.
- **Measured (lab core, sliced headless; scratch probes not committed):**
  - **Self-check 4/4:**
    - C1: K +1, 7 members → R 0.965 (≥ 0.9).
    - C2: K −1, 4 members → R 0.046 (≤ 0.1).
    - X1: planted flipped-sync law → 0.208, refused.
    - X2: planted sequential-update law → 0.145, refused.
  - **Lock and wander:**
    - The lock threshold does not depend on detune: K ≈ 0.74 (5 members) and 0.76 (7), the same at detune 0.1, 0.28 and 0.6, because σ-normalisation cancels it.
    - At K 0, C3, detune sets the wander rate: 1, 3 and 7 R peaks/s at detune 0.1, 0.28 and 0.6, and 8/s at E4.
  - **Rules, chord scene, largest one-tick step:** LOUDEST 0.235 (hand-over), MEAN 0.374, NEWEST 0.897, MAX 0.731, GAIN-WEIGHTED 0.321 (bounded by the new note's gain share, with a 5 ms attack).
  - **Feedback:**
    - +0.5 from base 0.3 latches (R 0.97, K +1). It holds down to base 0 and lets go below about −0.1. A re-strike does not free it.
    - +0.25 bursts (R 0.03 ↔ 0.93).
    - −0.5 from base 0.75 governs (R 0.06–0.61, versus 0.80 locked without the loop).
    - A delay of 0, 1, 4 or 16 ticks gives the same picture (single-swarm scratch probe of the same law).
- **Evidence consulted:**
  - The B253 row.
  - `src/swarm_core.h`: 1-40, 125-160, 1470-1560, 1700-2095.
  - `src/mod_core.h`: 1-80 (Scope kPerNote, OQ-23 note).
  - `src/hypersaw_clap.cpp`: 3525-3545, 3860-4010 (every per-voice source is projected by MAX today).
  - ROADMAP B34, B179, B180, B207 and B224.
  - `src/gui/gui2.html:3174`, `src/param_presentation.tsv:83-95`.
  - `tools/labharness/lab_load_check.mjs`, `station_check.mjs` (the must-fail idiom), `tools/gen_lab_index.py`.
  - `traces/2026-09-22-b207-mod-matrix-lab.md` (screenshot method).
- **Alternatives rejected:**
  - A stand-in R curve: forbidden by the brief, and it could not show hysteresis or bursts.
  - Phases advanced inside the force loop: this was the first draft's bug, which biases the splay. It is kept as the planted law X2.
  - A node harness wired into `./verify`: the brief scoped this to the lab plus a trace. The check lives in BLOCK A, so a harness can slice it later.
  - Per-source hue for the voice trails: B207 already ruled that hue is an ADR-116 matter. Voices are told apart by label.
- **Verify:** `./verify fast` exit 0 on `bf1a83f` (`.harness/last-verify.json`); the trace commit is re-verified in the PR.
- **Open questions:**
  1. **B180 collision.** B180 proposed Gain ENV OSC 1/2 on slots 22/23. B253 takes them and the array is now full, so B180 needs `kMaxSources` to grow.
  2. **Recommendation needs a ruling:** per-voice (kPerNote) on per-note destinations, GAIN-WEIGHTED for global ones. Unconfirmed: whether the shell's voice gain includes velocity (the lab weights by vel × gain env).
  3. **Latch visibility.** Should a latched loop be flagged in the UI, since a re-strike cannot free it?
  4. **Stale index.** `docs/design/index.html` is stale (tagline and lab-review). It is out of scope here; the lead should run `python3 tools/gen_lab_index.py`.
  5. **Unwired self-check.** The coherence self-check is browser- and audit-only. No `./verify` gate asserts it (only `lab_load_check` loads the file).
