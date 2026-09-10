# horde — the engineering, on one page

> **DRAFT — the human's voice replaces this.** Drafted 2026-09-10 for ROADMAP B103 by the
> lead's implementer from the files it cites; every claim below names its file so the human
> can check it before rewriting it in the first person. Five minutes to read.

## 1. How correctness is defined here

Not "it sounds plausible." The C++ engines are statement-level ports of browser prototypes
that are the reference implementation by decision (`DECISIONS.md` ADR-003), and correctness
is **parity with them**: same mulberry32 streams, same seeding, same control-tick order,
sample outputs within **ε = 1e-6 RMS over 4-second renders**, across a matrix of seeds and
per-subsystem parameter vectors (`specs/ACCEPTANCE.md` §L0-1). Any intentional divergence
needs an ADR. Beside parity sit the **trajectory criteria** — the sync phase transition, the
splay attractor, inertia, R→tone, the cascade zipper — each a measured number on the reference
with a stated tolerance (`specs/ACCEPTANCE.md` §L0-2…L0-6), and the invariants no reference
can carry: determinism (same seed + note order → identical samples; no wall-clock in the core),
real-time safety (the audio thread allocates nothing), buffer-subdivision invariance,
sample-rate independence with every time constant in seconds (ADR-009 records the per-tick
trap) — `CLAUDE.md` §Domain invariants, `tests/feature_tests.tsv` rows INV-1…INV-7.

The definition has a declared edge. Where the dynamics are chaotic — a positive Lyapunov
exponent, measured for the harmonic law under strong coupling — a one-ULP perturbation of the
JS reference *alone* diverges as far as C++ does from JS, so sample-exact agreement is
impossible in principle. Those regimes are excluded from the golden matrix and pinned by
behavioural anchors instead (boundedness, NaN-cleanliness, the qualitative claim the parameter
exists to make), and excluding one requires the bracketed divergence curve plus the
reference-only perturbation test — never "just floating point" (ADR-065; `specs/ACCEPTANCE.md`
§L0-1 domain limit).

And the definition has a known blindness, which is why `docs/MEASUREMENTS.md` exists: a
reference oracle certifies **agreement**, not correctness — a defect the prototype shares is
not missed, it is certified (`LIBRARY.md` L0031). Parity says the port is faithful; it says
nothing about whether the saw aliases. Measurement and listening are the other two oracles.

## 2. RULING and ENCODING

Every test in `tests/feature_tests.tsv` declares what it pins. **`pins=RULING`**: a decision —
someone chose this, changing it needs a decision, a red is a real divergence. **`pins=ENCODING`**:
how the thing happens to be done today — a red may mean the implementation moved, and the row
is reclassifiable rather than silently wrong. The `owner` column names *whose* decision (an ADR,
a sibling project's ruling, `spec`, or `human`), so a RULING can be traced to what ruled it;
invariants are RULINGs owned by `spec`. The classification lives with the case, not the runner,
so the table survives a harness change (`tests/feature_tests.tsv` header; `ROADMAP.md`
§"FEATURE TEST TABLE", 2026-08-15).

It was adopted from a sibling project that had been wrong twice for want of it — their suite
asserted their own *encoding* as though it were the rule and failed a conforming shell against
it (the header's cautionary tale). Today the table holds 117 RULING and 70 ENCODING rows,
103 agentic and 84 human; `tools/test_table_check.py` parses the oracle list out of `./verify`
itself, so a row cannot claim coverage from a gate that was never written, and the 14 rows
with `oracle=none` are counted every run rather than carried quietly (counts: this file's
date; the gate prints the live number).

## 3. How the agentic process was governed

The repo was built by agent sessions under a fixed charter (`CLAUDE.md`, harness layer above
§Domain), and the charter is the governance:

- **One source of truth.** `ROADMAP.md` holds task state, acceptance criteria, invariants and
  open questions; if the conversation and the roadmap disagree, the roadmap wins, and only the
  lead session writes it. Every decision is an append-only ADR in `DECISIONS.md` (159
  `## ADR-` headers at this writing, amendments included).
- **Passing ≠ done.** Done is `./verify full` green *and* the roadmap's acceptance criteria
  met *and* a trace written in `traces/` (181 entries) recording what changed, why, the
  evidence consulted and the verify result with its git hash.
- **Oracles gate phases and are never weakened.** `./verify fast` after any change set,
  `./verify full` before an item is called done, output reported verbatim, a red halts forward
  work. Skipping a test, relaxing a threshold or marking an xfail needs a human decision
  recorded in the roadmap. The gates include a privacy leak gate (no machine identity in
  tracked files; private siblings by alias only, ADR-014) and a pin on the ratified
  architecture rung in `project.manifest.json` — it caught the lead trying to raise it.
- **Human gates.** Deleting files, changing a public interface, editing `./verify`, adding a
  dependency, any git beyond add/commit on the working branch, and the protected paths —
  the specs and the prototype HTMLs, because an edit there *is* a spec change.
- **Delegation is self-contained.** A sub-agent's brief states the files in scope, the
  acceptance criteria copied verbatim from the roadmap, the verify target and what is out of
  scope; one queue item per dispatch; a grounded refusal ("cannot within the brief because X")
  is a success class, guessing to look productive is not. Adversarial review by a separate
  critic for anything architectural or touching an invariant.
- **A knowledge loop with a write gate.** Sessions read `INDEX.md` first and pull matching
  `LIBRARY.md` entries; new lessons enter as candidates with a stated falsifier, and prefer
  not writing over writing unverified, because the loop feeds its own output back
  (`CLAUDE.md` §Self-Improving Knowledge Loop). Several of the lessons that shaped the
  measurements — calibrate a detector on a known-clean and a known-bad signal before trusting
  it (L0016), a reference certifies agreement not correctness (L0031) — came out of that loop.

## 4. The goldens problem, and the v2 plan

The goldens are renders of the JS reference. That makes them the strongest fidelity check
available and, by construction, a **pin on today's behaviour**: any improvement the ear wants
breaks parity by definition. The repo's answer so far has been the *parity-safe superset* —
every new law sits behind a switch whose default reproduces the reference bit-exactly, so
the goldens stay green while the surface grows (ADR-020; the saw-shape section, ADR-094, is
built that way). That works until the day a shipped session depends on a law the goldens
never described.

The plan is `ROADMAP.md` **B100**, quoted in full because it is the centrepiece:

> **State header + per-patch engine revision — the compatibility mechanism, BEFORE strangers
> save sets** (audit 2026-09-10; converges with the lead's 2026-09-09 answer). Today:
> `"schema":3` + append-only ids + tolerant load + one exercised migrator (ADR-103). Build:
> (1) header `{schema, engine_revision, build}` on every blob (build hash already exists for
> the GUI corner — stamp it); (2) **`engine_revision` pins DSP behaviour per patch** — sets
> saved under rev 1 load pinned to rev-1 laws, new patches default to latest, a patch-level
> control opts an old patch forward (the u-he/Surge pattern); the repo's existing
> parity-safe-superset discipline (new law behind a default-old switch) becomes the rev-1
> path by construction; (3) **the oracle carries TWO golden sets, both gated**: v1 goldens
> become a LEGACY-CONFORMANCE test (rev-1 must not drift — shipped sessions depend on it,
> which makes it a RULING), v2 goldens — workshopped against measurement + listening, not the
> JS — become correctness going forward (the goldens-v2 workshop the human has asked for, as
> 1.1); (4) **a state-fixture corpus**: real blobs per schema/revision, asserted load+render
> bit-identical forever — the notice-001 inventory is where fixtures come from; without it a
> migrator is a promise. ADR before the workshop; cited in the engineering writeup

In one line: v1 goldens stop being "correct" and become "what rev-1 sessions were promised";
v2 goldens are made from measurement and listening rather than from the prototype; and the
header is built *before* anyone outside saves a set, because a migrator with no fixtures is a
promise, not a mechanism. Sequencing: state header first, then the evidence layer, then
outside users, then goldens v2 as 1.1 (`ROADMAP.md` §"1.0 — DEFINITION OF DONE").
