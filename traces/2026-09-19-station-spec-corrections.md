# station-spec-corrections — the eight rulings on port phase 1's findings, written into the spec and the lab

- **Queue item:** ROADMAP B162. The human, 2026-09-19, on the seven findings
  STATION port phase 1 (PR #678, `traces/2026-09-19-station-core-phase1.md`)
  raised against the spec and the lab: *"I'll go with whatever you recommend on
  the Station specs."* The lead's recommendations are therefore RULINGS, and
  they sanction edits to two protected files (`reference/station.html`,
  `specs/SPEC-STATION.md`) limited to the eight items below.

- **Why:** Phase 1 finished green but left the spec asserting three things the
  port had measured to be false (a CPU budget nobody had measured, whole-DSP
  bit-parity, "~the stated time" for a segment that runs to 163 % of it) and
  three lab defects carried for parity with no ruling behind them. A spec that
  disagrees with its own oracle is the defect; the port was correct and the
  document was not.

## The eight items, each with what it turned out to be

1. **§12 budget — MEASURED, estimate retired.** §12 said "≤ ~2 % of one core at
   16 voices" and that figure was an estimate, never a measurement. It now
   records **5.2 %** (phase 1, standalone core, min of three 5 s renders,
   16 voices / max patch / 48 kHz, the reference Mac — Apple Silicon), states
   that the ≤ 2 % figure is retired, and re-sets the budget from the **shell**
   measurement (`measure_cpu`, a STATION column) when phase 2 lands, because a
   standalone-core figure and an in-shell figure are not the same quantity.
   Optimisation is B162's queue item, not a phase-2 blocker.
   **Re-measured here:** 5.12 / 5.18 / 5.21 % on an idle machine, **9.04 % on a
   busy one** — the same binary, same tree. The load-sensitivity is written into
   §12 so nobody quotes a single reading as the figure.

2. **§11 item 9 — parity on the self-feedback diagonal is gated BY INDEX.**
   Above index ≈ 2 the recurrence `out = wave(ph + cell·out_prev·0.1591549)` has
   `d(out)/d(out_prev) ≈ cell·cos(·) > 1`, so it is chaotic, and V8's `Math.sin`
   parts from libm by ~1 ulp: bit-parity is impossible **by construction**, not
   by a port bug. The measured ladder (0.9 → 0; 1.2 → 1.6e-7; 2.0 → 1.3e-7;
   **4.0 → 2.1e-1; 8.0 → 3.1e-1**) is now in the spec, the oracle pins the
   diagonal at index ≤ 2, and index 8 is gated behaviourally. It supersedes the
   lab audit's §4.1 claim that bit-parity is available "for the whole DSP".
   The general form is stated: **parity where the map is contractive,
   invariants where it is not.** No lab change.

3. **Op OFF must not freeze its envelope (S9) — LAB FIXED, not pinned.** OFF now
   silences the op's output and its matrix contribution and nothing else; the
   envelope keeps running, so re-enabling mid-note resumes at the live stage.
   `reference/station.html` and `src/station_core.h` both changed; §3.2 and §7
   carry the sentence; labharness **S18 was INVERTED** (it pinned the freeze)
   and `station_check` gained a row. **The fix is bit-inert on every existing
   golden** — proved, not assumed (below).

4. **§11 item 10 — the linear pan law is INTENDED, declared not fixed.**
   `g·(1 − max(0, pan))`: full level at centre, −3.01 dB of total power at hard
   pan. Constant-power is the usual answer and is not taken, because it would
   drop every centred default — every factory op, every algorithm preset — by
   3 dB, and this law is what the human has been listening to. Same argument as
   the ±0.7 noise ruling (ADR-177 §3). The consequence is stated rather than
   hidden: panning is also a gain control. No lab change; the port matches.

5. **Release at 163 % — a DEFINITION, not an arithmetic defect. MEASURED BOTH
   WAYS BEFORE RULING.** The probe (`scratchpad/b162/rel.mjs`, the headless lab
   via `extract_core.mjs`) separates the two hypotheses cleanly:
   - the law is `lvl += (0 − lvl)·4.6/(R·f_s)` with **4.6 = ln 100**, so the
     level reaches 1 % (−40 dB) of its note-off value at **100.10 %** of the
     stated R at 48 kHz. There is no wrong sample-rate or units factor; the
     0.10 % is the forward-Euler discretisation of the exponential.
   - the extra 63 % is the run from −40 dB down to the **absolute 0.0005 exit**,
     and it depends on the START level exactly as the closed form
     `ln(lvl₀/0.0005)/4.6 · R` predicts, to four decimals at every point:
     0.55 → 152.2 %, 0.896 → **162.8 %** (this reproduces audit S13's "163 %",
     i.e. S13 measured a release from ≈0.9, not from the default sustain),
     1.0 → 165.2 %.
   - sample-rate portable to **0.007 %** (44.1 / 48 / 96 kHz).
   - the decay segment behaves identically: 102.7 % of D for 1.0→0.55,
     **120.0 %** for 1.0→0, at two different D values — the same law, not a
     coincidence of one setting.
   Therefore: documented precisely in §7 as a table, **no sound change**, no
   golden moves, patches' release feel is untouched.

6. **§4/§10 — op LVL and noise LVL join the 5 ms smoothed set.** The smoothing is
   the PORT's (ADR-177 §3): the lab stays step-valued and its harness keeps
   pinning those numbers. `src/station_core.h` smooths the four levels on the
   same `cellCoef` one-pole the 12 cells use, primed on target at the first
   render (hence bit-inert for a static patch), with the slot-skip guard moved
   onto the SMOOTHED level so a fade-to-zero is not bypassed by the very write
   it exists for, and a snap-to-target at 1e-30 (the DC blocker's own constant)
   so a level ramped to 0 cannot idle in the subnormal range inside the mix sum.

7. **§3.4 — FREE phase mode ratified as phase 1 built it.** Per-op free-running
   accumulator against middle C, seeded/advanced by the note+sample stream and
   never a wall clock, skipped entirely when every op is RETRIG. Written as a
   port addition, cross-referenced from §11 divergence 3.

8. **§8/§10 — `op{n}.velSens`, three rows, default 0.** Velocity scales the op by
   `1 − velSens·(1 − vel)`. **Applied AT THE SOURCE, beside the envelope**, not
   at the mix — that is an interpretation and it is load-bearing, so it is
   stated: the brief's own rationale is "a modulator op with velSens > 0 is how
   velocity reaches TIMBRE in PM", which is only true if the scale reaches
   `prev[]` and hence the matrix; a mix-only scale can never affect a pure
   modulator, which has `lvl = 0`. §2 already applies exactly this rule to the
   envelope, so velocity sits beside it rather than inventing a second place.
   Lab, core, generator and check all carry it; `noteOn` takes a 0..1 velocity
   **defaulting to 1**, so no existing call site changes behaviour. The lab page
   gained the three per-op VEL sliders **and** a play-velocity slider — without
   a velocity source the sensitivity would be an invisible feature (L0023).

## Evidence consulted

- `traces/2026-09-19-station-core-phase1.md` (the seven findings and their
  numbers), `docs/audits/2026-09-18-station-lab-audit.md` §2.6 (S6), §2.8 (S10),
  §2.9 (S9 and S13), §3.4 (S12), §4.1.
- ADR-177 §3 + Amendment 1, ADR-179 §5 in `DECISIONS.md`.
- `specs/SPEC-STATION.md` §2, §3, §4, §7, §8, §10, §11, §12 before the edit.
- `reference/station.html` `StationCore` (:141-372), `src/station_core.h`,
  `tools/labharness/station_check.mjs` S16/S18/S20, `tools/station_check.cpp`,
  `tools/golden/gen_station_goldens.mjs`.

## Proofs taken rather than assumed

- **velSens 0 / the op-OFF fix are bit-inert on every phase-1 golden.**
  Generated the 30 phase-1 goldens at `origin/main` FIRST, recorded their
  sha256s, made the lab edits, regenerated: **30/30 hashes identical**
  (`diff` of the two hash lists is empty). The 31st and 32nd files are the new
  `velocity` scenario. `station_check` also carries a standing bit-level control
  row (velSens 0, velocity 1.0 vs 0.25 → `max|diff|` exactly 0).
- **The smoothing row's detector was calibrated by PLANTS, and one of them did
  not fire.** Planting an unsmoothed OP level moves the reading 0.43× → **2.34×**
  and the row goes RED — and 2.34× matches phase 1's independently reported
  2.33×, so the anchor is confirmed, not just the direction (L0032).
  Planting an unsmoothed NOISE level **did not fire** (0.43× → 0.56×, still
  PASS). That is a coverage boundary, not a retry cue (L0033): an LFSR's own
  natural inter-sample step is a full-scale swing every few samples, so no level
  write on noise can exceed it. The noise level is therefore **removed from that
  row's gate** — reported only, with the boundary written into the source — and
  gated instead by a new trajectory row (first-millisecond amplitude after the
  write), which the same plant DOES fire: smoothed 0.969 / 0.131, planted
  0.112 / 0.110 → RED. Its must-read-zero control is an unsmoothed `ns.pan`
  write through the identical detector: 0.005.
- **The first smoothing detector was wrong and was replaced, not tuned.** It
  ratioed |step| against the LAST inter-sample difference, which for noise lands
  inside a zero-order hold five times out of six and read a 1.45e-4 "floor"
  (hence a bogus 6.61× failure). Replaced with labharness S16's unit — 16 writes
  at 16 settle phases across one 261.6 Hz cycle, ratioed against the LARGEST
  natural step in the preceding cycle, reported as the median — which is what
  makes 13.9× / 11.1× / 7.4× comparable numbers rather than coincidences.
- **The op-OFF row's must-fail control is the frozen value itself.** The
  toggled-off-then-on envelope must equal the never-toggled one EXACTLY
  (measured `0.057049 == 0.057049`, delta 0.000e+00) *and* differ from the level
  held at toggle-off (0.569175). A build that still freezes returns the latter
  and fails both clauses. Same construction in labharness S18, where the OFF
  window was shortened to 200 ms so the equality compares two moving values
  rather than 0 === 0.
- **The velocity-proportionality gate's threshold comes from the arithmetic, not
  from what passed.** 1e-6, because the render stores float32 (6e-8 relative per
  sample) and the factor is applied inside the chain; a wire-to-nothing reads
  `1/v − 1` = 0.33 at v = 0.75, six orders the other side. Measured 5.67e-10.

## Alternatives rejected

- *Fixing the pan law to constant-power.* Rejected — it is a 3 dB cut on every
  centred default, i.e. on everything the human has approved by ear. Declared
  intended instead (item 4).
- *Changing the release arithmetic.* Rejected — measurement says there is
  nothing wrong with it (item 5). Rewriting §7 to promise a tail it does not
  produce, or shortening the tail to match a loose phrase, would both have been
  changes made to satisfy a document rather than a listener.
- *Smoothing op LVL in the LAB as well.* Rejected — ADR-177 §3 put the smoothing
  on the port, and a smoothed lab would move every golden and destroy the
  harness's S16 pin for no gain.
- *Scaling velocity at the mix instead of at the source.* Rejected — it cannot
  reach a pure modulator (lvl 0), which is the one case the parameter exists for.
- *Keeping the noise level inside the step-ratio gate.* Rejected — the plant
  proved the detector blind there. A gate that cannot fail is not a gate.
- *Renumbering §11's existing items to slot the two new ones in at 8-9.*
  Rejected — four cross-references in three files point at "§11 item 7/8". The
  new rulings are appended as items 9 and 10 instead and every existing pointer
  still resolves.
- *Adding the per-op VEL sliders to the lab page without a velocity source.*
  Rejected — that is L0023's invisible feature exactly. A play-velocity slider
  went in beside Master so the feature can be heard.
- *Snapping the 12 matrix cells to target as well (the same subnormal
  exposure).* Not taken — outside item 6's scope. Flagged as an open question.

## Verify

- `./verify full` — **exit 0**, run on the committed hash; output quoted
  verbatim in the PR. Log grepped for RED before any git command (the 65
  matches are all substrings of "shared"/"declared"/"Building ... shared").
- `tools/station_check` (built `-O2`, run by hand — still UNWIRED per ADR-179 §4):
  **GREEN, 0 failures, 32 scenarios, worst parity rms 1.257e-07** (gate 1e-6).
  30 of 32 are bit-exact; the two that are not are the index-2 self-feedback
  scenarios, item 2's subject. The new `velocity` scenario is **0.000e+00** at
  both rates.
- `node tools/labharness/station_check.mjs` — **GREEN, 21 checks, 0 failed**
  (S18 now gating the fixed behaviour).
- `node tools/golden/gen_station_goldens.mjs --selfcheck` — deterministic,
  32 scenarios × 2 rates.
- `node tools/labharness/lab_load_check.mjs` — GREEN, 44 labs, 0 broken.
- **Hashes.** `reference/station.html` sha256 was
  `d465475c28802d775fec8e6e516bc1c86798acbf0287627df9071b2e6fc37681` and is now
  `3732c6b6ed71d4bc…` (the manifest's 16-char form; the generator writes it into
  `station-manifest.tsv` every run). Golden render set: the 30 phase-1 files are
  byte-identical across the change; `velocity-480.f32` and `velocity-441.f32`
  are new. Goldens live under `build-golden/`, which `.gitignore` excludes, so
  nothing is committed and the next generator run reproduces them.

## Open questions

1. **The matrix cells have the same asymptote the levels just had fixed.** Item
   6 added a 1e-30 snap-to-target on the four levels; the 12 cell smoothers
   still approach their target geometrically forever, so a cell ramped to 0 goes
   subnormal after ~3.5 s. It lands in `v.ph[i]`, a normal number, so it cannot
   reach the output the way a level could — which is why it was left alone
   rather than swept in. One line if the lead wants symmetry.
2. **Velocity's placement is an interpretation of the brief.** "Scales the op's
   level" was read as the FM sense — the operator's output level, at the source
   — on the strength of the brief's own "how velocity reaches timbre in PM".
   If the lead meant the mix slider only, item 8 needs re-doing and the
   `velocity` golden regenerating.
3. **`station_check` is still UNWIRED** and so are its five new rows. That is
   ADR-179 §4's open ruling, not this brief's to take. A sibling agent was
   wiring it on another branch while this ran; if both land, the wiring PR
   should be rebased onto this one so it picks up the 32-scenario manifest and
   the `@note=midi:freq:vel` field.
4. **§12's real budget is still unknown** until `measure_cpu` grows a STATION
   column in phase 2. 5.2 % standalone is what this brief could measure; it is
   not what a host pays.
5. **The lab page's VEL sliders are untested by any oracle.** `lab_load_check`
   proves the page still loads; nothing proves the sliders are wired to the
   right field. The DSP-side law is gated three ways; the UI is not.
