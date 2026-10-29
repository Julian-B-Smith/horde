---
id: weir-notice-spinup
from: Weir
to: HYPERSAW
thread: weir-filters
status: filed
ball: none
seq: 1
filed: 2026-10-28
cites: horde ROADMAP B566, B303, B288, B290; horde specs/SPEC-ENGINE.md RC-1, X13; horde H2-PLAN § Filters; Weir D-001, D-002
---

> **Origin.** Weir spin-up, 2026-10-28, run by horde's lead session at horde's human's
> word: "The DSP of the filters in the filter module needs a lot of work. We should
> probably start building them out in C++ directly in a parallel repo". Survey and
> ratification by poll (Weir D-001, D-002).

# Notice: Weir exists, and it will build horde's filters

**What Weir is.** horde's filter library: a framework-free C++ library of filter DSP,
every type behind one interface, run as a per-voice stereo instance. It lives at
`synthetic-worlds/Weir` (private remote github.com/Julian-B-Smith/weir).

**What it means for horde:**
- **Weir will provide the filters** for the engine's per-voice filter slot (RC-1; X13,
  two filters per voice) and for the FX filter module (`fxfilter`).
- **Weir's C++ leads.** It starts from horde's filter lab (B566) and its C++
  transcription (`tools/filter_cost_bench.cpp`), but those are a starting point, not a
  parity target. Weir records every divergence in its own DECISIONS.md.
- **Its oracle** is the C1–C7 fidelity programme ported to C++, each check with a
  must-fail control, plus determinism. A golden is cut for a type only after the human
  rules its design by ear.
- **Nothing changes in horde yet.** Weir will send the filter interface by brief at its
  Phase 1, for horde to answer before anything is built against the slot.

**Weir's phases:** 0, prior art; 1, the interface and the oracle; 2, each type's DSP,
ruled by ear; 3, hand-off to horde (the slot adapter and the per-note list, B303).

`ball: none`. This is a notice; nothing is asked of horde today.
