# SWARM✱ UPDATE-001 — effects family + accumulated decisions

**For:** the running VST build (two of three original engines complete).
**Nature:** pure addition + append-only record updates. **Nothing in this update modifies the contract of any engine already built.** If a document here conflicts with something already ingested, the later ADR number wins (append-only discipline).

## Contents

| File | Action |
|---|---|
| `UPDATE-BRIEF.md` | this file — read first |
| `specs/SPEC-EFFECTS.md` | ADD as new spec. Defines the effects product line (4 engines, 1 shared force core). Supersedes `SPEC-FILTER.md` if a copy was ever ingested (it was an interim doc; if absent, ignore the supersession note in its header) |
| `DECISIONS-DELTA.md` | APPEND to `DECISIONS.md`: ADR-010 through ADR-016. Idempotent: skip any ADR number already present. ADR-010–012 may already be in the build's copy (they shipped alongside the splay change-note); 013–016 are new |
| `ACCEPTANCE-EFFECTS.md` | APPEND to `specs/ACCEPTANCE.md`: L0-14 through L0-21. Numbering continues the existing sequence; skip any already present |
| `ROADMAP-DELTA.md` | ADD Track E to `ROADMAP.md` as a parallel track. Does not renumber or modify existing phases |
| `reference/swarmfilter.html` | ADD — reference implementation, resonator bank (`FilterLab` core) |
| `reference/swarmphaser.html` | ADD — reference implementation, notch swarm (`PhaserLab` core) |
| `reference/swarmtime.html` | ADD — reference implementation, tap-swarm delay + FDN room (`TimeLab` core) |

## What does NOT change

- specs/SPEC.md, the oscillator-engine contracts, and all acceptance criteria L0-1..13 are untouched. The two built engines require no rework.
- The three original prototypes received a **UI-only** patch (focus-trap fix per ADR-016: sliders/selects release keyboard focus; Escape = panic). No DSP or core change — refresh them only if convenient; they are not parity-relevant.

## Parity notes for the three new reference implementations

1. The JS cores (`FilterLab`, `PhaserLab`, `TimeLab`) are the parity oracles for Track E, same discipline as ADR-003.
2. **The NaN watchdogs in `TimeLab.render` and `PhaserLab.render` are part of the reference semantics**, not debug scaffolding: non-finite feedback state must self-heal within one processing block (ADR-016 law b). Port them.
3. The stability laws are load-bearing and Layer-0-guarded: feedback paths normalize by N (not √N — ADR-015a); every feedback loop contains a DC blocker with corner ≤ lowest-musical-comb/6 (ADR-015b); the FDN uses the **negated** Householder so the input-excited eigenchannel carries +1 (ADR-014a). Each has a regression criterion in ACCEPTANCE-EFFECTS designed to catch its specific silent-reversion mode.
4. The equilibrium law err_final = h/(h+g)·err_initial is confirmed three-for-three exact and is cited *as physics* by the acceptance criteria: if force rates change, the acceptance targets change with them by the formula, not by re-measurement.

## Open items deliberately not in this update

Stuart–Landau state-domain coupling, FDN topology-as-matrix, adaptive basins, stereo populations — all parked with revisit triggers in SPEC-EFFECTS §9. Do not scaffold for them beyond keeping the coupling-function and matrix constructions behind interfaces.
