# 2026-09-13 — ENV 2 retriggers on every strike (ADR-161, B116) + undo_check wired

**Report (human).** "When you have pitch envelope activated and hold one note,
consecutive notes bring the pitch peak closer and closer to the destination
until there's no longer a noticeably spike."

**Measured** (`tools/penv_check.cpp` scenario, before the fix — Env>Pitch +12,
hold 60, strike 64 six times):
```
  60 held   peak env2=1.000  peak pitchSm=+11.08 st
  strike 1  peak env2=0.044  peak pitchSm=+0.55 st
  strike 2  peak env2=0.004  peak pitchSm=+0.06 st
  strike 3  peak env2=0.000  peak pitchSm=+0.01 st
```
The stage machine restarted only on `anyGate && !env2Gate`; with 60 held the
edge never came. After the fix every strike peaks at 1.000 (pitchSm +11.1 st).

**Fix.** `env2Retrig` set at the three note-on sites (`lastNoteKey = n->key`),
consumed in the ENV 2 block. Audio-thread flag on the audio thread; no
allocation, one branch.

**Also.** `./verify` now runs `undo_check` after `state_check` in `full`
(human ruling 2026-09-13). New export `hypersaw_debug_penv`.

**Oracle.** `./verify full` — pasted in the PR body.
