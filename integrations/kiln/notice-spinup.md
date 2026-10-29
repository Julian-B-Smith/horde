---
id: kiln-notice-spinup
from: Kiln
to: HYPERSAW
thread: kiln-fx-modules
status: filed
ball: none
seq: 1
filed: 2026-10-29
cites: horde ADR-313, ADR-309, ADR-236, ADR-190 A7, ROADMAP B579, B234; Kiln D-001, D-002
---

> **Origin.** Kiln spin-up, 2026-10-29, run by horde's lead session at horde's human's ruling "EQ + Drive,
> one new repo" (horde ADR-313). Survey and ratification by poll (Kiln D-001, D-002).

# Notice: Kiln exists, and it will build horde's EQ and drive

**What Kiln is.** Kiln is horde's EQ and drive. It holds two hosted FX modules, a parametric EQ and
a simplified drive, in one framework-free C++ library. It lives at `synthetic-worlds/Kiln`, which
is private; the human creates its remote.

**What it means for horde:**
- **Both modules are built to the rack-slot contract,** the way Bulwark is. Each module's manifest
  declares every key's class and every flip-class key's `switch` in ADR-309's form (§12.7). Each
  also declares its tail, its latency, its I/O gains and its state. horde's admission check runs
  against Kiln as it does against Scape.
- **Kiln's C++ leads.** horde's FX lab round 2 (B234, the EQ) and its drive ideas are a starting
  point, not a parity target. Divergences are recorded in Kiln's DECISIONS.md.
- **The deadline is horde's module cutoff,** 2026-12-11 (ADR-236).
- **Nothing changes in horde yet.** Kiln sends each module's manifest by notice at its Phase 1.

**Kiln's phases:**
- 0: prior art;
- 1: the shells and the oracle;
- 2: the DSP, ruled by ear;
- 3: hosting in horde.

`ball: none`. This is a notice, and nothing is asked of horde today.
