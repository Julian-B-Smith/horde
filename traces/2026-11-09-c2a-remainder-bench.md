# c2a-remainder-bench — a bench for the swarm oscillator's non-blade remainder

- **Queue item:** B667 / CONTRACT 7 C2a (ADR-390 left PF-1's re-anchored figure open)
- **Why:** PF-1's re-anchored figure needs the swarm's cost apart from the members' blade/wave reads; `tools/c2a_remainder/` measures it by cells (as is, blades off, unison 1/5/9, silent) in an instruction-count mode that may run now and a PF-3 timed mode for the quiet window.
- **Evidence consulted:** specs/SPEC-ENGINE.md PF-1..PF-3; clean-room engine6cd98b5 `engine2/tests/cpu/main.cpp`, `pf3_chord.h`, `tests/harness/bank.h`; tools/engine-preview/build.sh; count-mode run (see commit message / report).
- **Alternatives rejected:** linking the clean-room bench itself (its cells are fixed, and it would need edits to the archive); kpc counters (need root), so `proc_pid_rusage` instead.
- **Verify:** `nice -n 10 ./verify fast`; result recorded in the report to the lead.
- **Open questions:** the timed mode has only been smoke-run (one repeat, load guard skipped); its numbers wait for the 2-6 am window.
