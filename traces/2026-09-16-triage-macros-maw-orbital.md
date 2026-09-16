# Trace — 2026-09-16 triage: module macro tiers, MAW, ORBITAL initial conditions

**What changed.** Three root drops triaged. (1) `SPEC-module-macro-tiers.md` →
`specs/SPEC-MODULE-MACROS.md`, `horde-module-macros.html` →
`reference/horde-module-macros.html` (ADR-169 PROPOSED, row B136). (2)
`maw-files.zip` unpacked: `specs/SPEC-MAW.md` + `reference/maw/` (README,
prototype, core.js, fidelity.js + report, tail.js, presets.js, preset-test.js)
(ADR-170 CANDIDATE, row B137); the zip moved to the lead's scratch, not
deleted. (3) Row B135 (ORBITAL per-body initial conditions + readout distance
scale) written and dispatched (`briefs/2026-09-16-b135-orbital-initial-conditions.md`,
branch `orbital-initial-conditions`). CLAUDE.md §Domain candidate list and
protected paths extended. A notice filed in Sluice's mailbox
(`integrations/hypersaw/notice-module-macros.md` in their tree, ball Sluice,
delivered as a PR on their repo — https://github.com/Julian-B-Smith/Sluice/pull/3 — because a filing is FILED only when it is on the correspondent's origin/main (`tools/mailbox_delivery_check.py`); the human merges; the draft copy lives in `docs/proposals/`).

**Evidence consulted.**
- Macro spec read in full; oracle: 27,827 bytes, one `Math.random` (line ~497,
  `quantumPick`) → the single sanctioned edit. Its §14 acceptance is adopted
  verbatim; open decisions 1–5 answered with recommendations in ADR-169.
- MAW packet: `grep -c Math.random` = 0 in every file; one xorshift32 seeded
  0x51ED; shard table LCG 1337. The shipped `fidelity-report.txt` carries
  31 PASS / 1 FAIL lines — the packet's own
  evidence, to be re-run under our harness before ratification, not trusted
  as ours. The single FAIL is the packet's deliberate demonstration of its §13
  finding — section 7 runs the control-rate staircase with block-held
  coefficients (FAIL, 28.5× boundary energy) beside the shipped per-sample
  interpolation (PASS, 1.0×); it is the bug shown, not a bug present.
- SPEC-ORBITAL §3.2 already stores home + initial velocity; §7 defines reset
  from them; the lab (`mk(...)` at `reference/gravity-modulator.html` ~line
  188) has `x,y,vx,vy` only — no stored initial condition, hence B135(a).
- Distance scale: a readout transform keeps the sim bit-identical by
  construction; the physical similarity (×s, ×s, G×s³) preserves shape and
  period but not wall contact — recorded as the bake button, not the slider.

**Leak/alias.** Product names (Roar, Ableton) are not private siblings; no
sibling real names in any ingested file (`./verify fast` leak gate, below).

**Verify.** `./verify fast` — see the PR body for the verbatim tail.
