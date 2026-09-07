---
authoring-project: HYPERSAW (github.com/Lifted-Truck/HYPERSAW)
filed: 2026-07-18
motivating-decisions: HYPERSAW ADR-008 (consonance gravity is an audio-engine force), ADR-010(d) (brief obligation before gravity ships), ADR-028 (gravity shipped 2026-07-18 on its default 13-ratio placeholder); specs/SPEC.md Layer 3; traces/2026-07-18-dynamics-surface.md
id: HYPERSAW-001
status: filed
ball: provider
respond-by: 2026-08-08
relay: via Julian (cross-repo push declined in-session; the mailbox README blesses this route)
---

# HYPERSAW → Tonality intake brief

## Disambiguation first (re your README note)

HYPERSAW's feature is **not note quantization**. Consonance gravity operates
on *continuously sounding fundamentals* inside the audio engine: held notes'
frequencies are pulled toward simple ratios by a basin-limited restoring
force (settling is an audible physical event — decelerating beating). It is
not note-in→note-out snapping, so it does not overlap
`conform_to_scale`/`fit_to_key` (Tonality-Live brief-001) — no second
primitive requested, and none of that work is duplicated here.

## The six intake questions

1. **Produces/consumes.** Consumes MIDI note on/off in hard real-time (it is
   a CLAP/VST3 instrument); velocities present, no durations beyond gate.
   Internally: continuous frequencies (12-TET at note-on, then JI-drifting
   under gravity). It can *export* held pitch-class sets or note lists
   offline if that serves an analysis door. Audio itself: out of scope for
   you, understood.
2. **Capabilities wanted.**
   a. *Continuous-pitch ratio priors* — per-ratio weights (attraction) and
      basin scaling for the gravity force. We read your 12-TET boundary
      note: this is your recorded JI/monzo deferral territory. **The ask is
      to register the gap** with this consumer attached, not to build now.
   b. *Later, context-weighting* (per-chord granularity): the held
      pitch-class set / induced key (your shipped 12-TET identity + key
      induction) weighting which basins widen or narrow. This half is the
      genuinely-Tonality part of the idea. No urgency; Phase-5-class for us.
   c. Determinism kinship noted — if/when an export exists, we would vendor
      it with a producer PIN and schema-validate in CI (our L0 oracle
      discipline matches your conformance-golden regime).
3. **Latency budget.** Hard real-time host, therefore **zero live calls by
   design** (our charter forbids provider calls on the audio thread). All
   consumption is precomputed: your versioned-data export is exactly the
   shape we planned for. Edit-time MCP generation of the table is fine.
4. **Direction.** Generation (Tonality proposes the priors table; HYPERSAW
   realizes). Possibly analysis later (offline export of held pc-sets).
5. **Integration door.** Dataset files (versioned export artifact); MCP at
   edit time to produce them.
6. **Spelling/labeling.** Ratio display strings ("3/2", "16/15") welcome in
   the export for our GUI readout; internally we keep raw rationals.

## Expectations

HYPERSAW is not blocked: gravity shipped on its default 13-ratio set (the
visibly-degraded placeholder, per policy); the swap-in is one table load.
Useful verdicts: 🕳 recorded-gap for (2a) with this consumer attached; a
phase pointer for (2b); ✋ boundary rulings where we guessed your scope
wrong. Proposed export sketch (yours to counter):
`{ "schema": "tonality-gravity-priors.1", "provenance": {...}, "ratios":
[{"num":3,"den":2,"name":"3/2","weight":1.0,"basin_scale":1.0}], "context":
null }` — contract tests offered: schema validation, determinism
(byte-identical re-export), fold-safety (all ratios in [1,2), no dups).
