# 2026-09-11 — Chord transposition: the quantiser latch outlived its anchor (ADR-158)

**Report (human, 2026-09-11).** "Playing several notes makes them seemingly glide
to other notes at random intervals, almost as if there's a randomly wandering
S&H LFO applied to pitches … doesn't have anything to do with the scale (drag)
mode … doesn't require pitch bend to be turned on." Repro recipe from the
human: quantise chromatic, bend law off.

**What was measured.** `tools/anchor_check.cpp` (new, standalone) strikes a
chord across three consecutive blocks and reads the wheel lane's emitted value
(`hypersaw_debug_pitchbend`, new export) after every block. Before the fix:

```
  ok    T1 quant=chromatic    gate=continuous  worst |lane| = 0.000 st
  FAIL  T1 quant=chromatic    gate=free 8 Hz   worst |lane| = 12.000 st (anchor 59 @ 0.546s)
  FAIL  T1 quant=chromatic    gate=sync /4     worst |lane| = 12.000 st (anchor 59 @ 0.546s)
  ok    T1 quant=scale        gate=continuous  worst |lane| = 0.000 st
  FAIL  T1 quant=scale        gate=free 8 Hz   worst |lane| = 12.000 st (anchor 59 @ 0.546s)
  FAIL  T1 quant=scale        gate=sync /4     worst |lane| = 12.000 st (anchor 59 @ 0.546s)
```

The earlier chord trace (same rig, lane logged per block, gate free 8 Hz):
`anchorKey=60 pitchBend=+9.000 · anchorKey=64 +5.000 · anchorKey=67 +2.000 ·
@0.1219s +0.000` — the whole chord nine semitones sharp until the window
elapsed, because the lane had committed its idle step at A4 (lastNoteKey's
initial 69) one block earlier.

**Mechanism.** `GlideCore::quantise` (`src/glide_core.h`) latches `qStep` in
absolute pitch; the time gate (`qT`, reset on commit) refuses a new step
inside the window; `base` (= `lastNoteKey` for the wheel lane) changed under
it, so the emitted offset became `qStep − newBase` and `updateTuneAll`
applied it to every voice. Continuous mode is immune (gate always open),
which is why single-note probes and every golden never saw it.

**Fix.** `quantise` re-arms on a base change (`qArmed = false`, `qT = 1e9`),
mirroring `reset()`. After the fix all six T1 rows read 0.000; the T2
control shows the gate still merges a fast wheel ramp into one commit at
0.128 s where continuous commits two by 0.052 s.

**Why now (hypothesis).** Step Timing is saved in the patch; before B110 the
preset tail was silently truncated at 256 queue entries, so a stored
non-continuous setting may only have LANDED since PR #543. The latch itself
predates the wave.

**Evidence consulted.** `src/hypersaw_clap.cpp` bend grid tick (`bendGlide.step(bendTarget, bendLaw, lastNoteKey)`),
`bendActive()`, `stepNoteBends`; `src/glide_core.h` `quantise`/`reset`;
`docs/design/bend-lab.html` lines 302–361 (same latch, single-note bench).

**Oracle.** `./verify fast` exit 0; `./verify full` — see PR body (pasted from
the run). `anchor_check`: PASS after the fix. Standalone, unwired.
