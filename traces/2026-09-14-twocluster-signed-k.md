# 2026-09-14 — B121: negative K in two-cluster mode (ADR-164, proposed)

**Report (human).** "In two-cluster mode, with A/B balance turned on, it doesn't
seem like K is actually going negative."

**Finding.** Documented limitation, not a regression: DYN's K is unipolar
(`reference/swarmdynamics.html` slider 0..1, `Ktarget = 4*K*K*sigma`);
ADR-051 recorded the corner collapse. In the port the two-cluster branch read
`s.KsmS` (the sync smoother, `max(0, km)`), so K < 0 produced zero coupling
and the splay smoother `KsmP` is only read on the mean-field path.

**Change.** `src/swarm_core.h`: `Voice::KsmD` (signed smoother, target
`(km + Kenv)*sigmaU`, same 0.08 slew, reset at strike); the two-cluster
branch multiplies `s.KsmD * kGain * c`. Bit-identical for km, Kenv ≥ 0.

**Check.** `tools/twocluster_check.cpp` (standalone) — output in the PR body.

**Oracle.** `./verify full` (parity 156/156 must hold) — PR body.
