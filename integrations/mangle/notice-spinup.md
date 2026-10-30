---
id: mangle-notice-spinup
from: Mangle
to: HYPERSAW
thread: mangle-fx-module
status: filed
ball: none
seq: 1
filed: 2026-10-30
cites: horde ADR-331, ADR-309, ADR-330, ADR-236, ADR-254, ROADMAP B592; Mangle D-001, D-002, D-003, specs/SPEC-MANGLE.md
---

> **Origin.** Mangle spin-up, 2026-10-30, run by horde's lead session at horde's human's request ("I
> would like to create a mangle directory and start building it as an FX module"). Survey and
> ratification by poll (Mangle D-001, D-003).

# Notice: Mangle exists, and it will build a seven-mode crush module for horde's rack

**What Mangle is.** One hosted FX module in a framework-free C++ library: seven crush modes behind
one control contract (Poverty, Character, Residue, Mix; a discrete Mode; a Live/Studio latency
setting). It lives at `synthetic-worlds/Mangle`; its remote is private. Its spec is Mangle's
`specs/SPEC-MANGLE.md`.

**What it means for horde:**
- **Built to the rack-slot contract,** the way Bulwark and Kiln are. Mode is a flip-class key: its
  10 ms linear crossfade of two modes' outputs will be declared as an ADR-309 `switch` (§12.7).
  The Latency setting is a Device key. The manifest declares tail, latency, I/O gains and state;
  horde's admission check runs against Mangle as it does against Scape.
- **Latency is one constant per setting** (Live 512 samples at 48 kHz, Studio 2048), every mode
  padded to it, so a mode flip on the morph never moves host compensation (RS-7.1).
- **Questions Mangle will ask horde by brief in its Phase 1:** which FFT a hosted module should use
  ("Use HORDE's FFT"); whether the rack can host a base-rate module under 2× oversampling (its Q-3);
  macro names for the intent bus (Damage, Grain, Ghost; its Q-7).
- **The deadline is horde's module cutoff,** 2026-12-11 (ADR-236).
- **Nothing changes in horde yet.** Mangle sends its manifest by notice at its Phase 1.

**Mangle's phases:** 0 prior art; 1 the shell, the shared machinery, classic crush and the oracle;
2 the six starving modes, each signed off by ear; 3 hosting in horde.

`ball: none`. This is a notice, and nothing is asked of horde today.
