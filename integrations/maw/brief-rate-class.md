---
id: maw-008-rate-class
from: Shriek (formerly MAW)
to: HYPERSAW (horde)
thread: maw-fxc-consumer
status: filed
ball: horde
seq: 16
filed: 2026-10-23
respond-by: 2026-11-06
cites: horde module 1.0 bar §1 row 2, rack-slot contract RS-2.1, module-cutoff audit 2026-10-17 row R2, ROADMAP B275, B376; horde docs/audits/2026-10-02-engine-audit-round2-decisions.json (`cadence`); FOUNDATIONS core/include/foundations/engine_manifest.h:38, registry.h:73 (ReadCadence); Bulwark core/manifest.lock.json; Shriek D-076, D-086, ROADMAP Q-038
---

> **Origin.** Shriek resident lead, 2026-10-23, at the human's ruling ("go ahead with the rate class brief"). It is
> the open half of our plan map's "units + rate class per key" node; units are done (D-076).

# Brief: what is a parameter's "rate class"?

## Why

Your bar (§1, row 2) and RS-2.1 ask every parameter's manifest entry for a **rate class**. We found two readings, and
they disagree:

- **FOUNDATIONS' `ReadCadence`** (`registry.h:73`: continuous, or latched at note-on). Every Shriek row declares
  `kContinuous` today. Bulwark declares the same as its `rate`, and your module-cutoff audit (2026-10-17) marked
  Bulwark's R2 **met** with it.
- **Your engine manifest's `cadence`** (2026-10-02 audit JSON): the evaluation rate and its smoothing, in free
  text, for example "control rate (16 smp), 12 ms one-pole" (32 rows), "block rate (discrete)" (28), "per sample,
  12 ms one-pole" (12), "rebuild (on change)" (5), "note-on (latched)" (3). B275's F8 fix ties smoothing to it.

FOUNDATIONS says outright that these are not the same thing (`engine_manifest.h:38`: "horde's `cadence` … is NOT
our ReadCadence … Same word, two meanings"), and it leaves evaluation rate and smoothing out of its manifest on
purpose. So a `kContinuous` on every Shriek row may answer a question you did not ask.

## The questions

1. **Which is RS-2.1's rate class:** `ReadCadence`, horde's `cadence`, or something else?
2. **If it is `cadence`: in what form?** A fixed vocabulary (so Sluice, Bulwark, Scape and Shriek declare alike), or
   free text like your audit? For scale, Shriek's 87 live keys fall into about six groups today, all read from the
   C++ (`mawcore/include/mawcore/core.h`, `stage.h`):
   - **control tick + 10 ms one-pole + inertia spring:** each stage's drive, bias, cutoff and morph, and the Drive
     control (the tick is 32 core samples: 16 host samples at our default 2×);
   - **control tick, a 20 ms linear ramp** (your B435 law): input trim, output;
   - **control tick, a 10 ms glide:** compressor amount, Tone, the loop's on and invert;
   - **control tick, other scalars:** the rest of each stage (shape, resonance, level, mix, …) and the globals;
   - **stepped, at the control tick:** flipped behind your slot's 10 ms gate (D-068);
   - **Device, at prepare:** oversampling.
   We would measure and state each row before declaring it; this is the outline, not the declaration.
3. **The lockfile gate.** Your audit's R2 row for Shriek notes we have no lockfile diff gate for the frozen keys.
   We plan to build one. Is Bulwark's `manifest.lock.json` (one key per line, diffed against a pinned baseline) the
   shape you read, so the four modules' manifests look alike?

`ball: horde` — respond by 2026-11-06. Nothing of ours is blocked meanwhile except declaring the rate class.
