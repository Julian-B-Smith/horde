# Trace — 2026-09-16 B117 buried: crossfade is the behaviour (ADR-163 A2)

**What changed.** `src/hypersaw_clap.cpp` param 264 default 0 → 1; the SET
cluster removed from `src/gui/gui2.html`; presentation rows for 264/265 moved to
page BURIED (no GEN marker, so the generator emits nothing); `tools/gui_reach.py`
exempts both ids by name; `tools/fxxfade_check.cpp` T1 asserts the buried default;
`tests/feature_tests.tsv` B117-1/2 updated (page `*`).

**Why.** Human ruling on the lead's recommendation. The complaint was atomic
replacement; the human heard the crossfade and called it working; the bounded
pool makes the idea structural at 1.1. The Echo/Room pair stays atomic (declared).

**Evidence.** `./verify full` on this tree (fxxfade_check is a gate since ADR-171) —
tail in the PR body.
