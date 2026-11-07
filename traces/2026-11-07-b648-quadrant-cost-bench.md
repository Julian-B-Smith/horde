# b648-quadrant-cost-bench — C++ cost bench for the proposed Quadrant wavetable oscillator

- **Queue item:** ROADMAP B648 (the human, by poll: "File it plus a cost bench"; spec ADR-384 CANDIDATE)
- **Why:** the spec's cost is a paper estimate ("~1-2% of one core, 8 voices, 48 kHz"). The human decides replace-vs-supplement for the blade oscillator, so a real figure for the design with the lead's review amendments (per-octave levels, 128-sample crossfade, two-level pitch crossfade, 1024 harmonics, swarm members + coupling) was needed. `tools/quadrant_cost_bench.cpp` is one file, no dependencies, not run by `./verify`.
- **Evidence consulted:** the Quadrant spec and its JS bench (`blendSpec`, `frameAt`, `recompute`, worklet read), `tools/filter_cost_bench.cpp` (house style, mulberry32), `docs/proposals/cpu-eval-framework.md`, the clean-room `REPORT.md` (instructions-retired method, PF-3 chord).
- **Method:** each cell is two child processes under `nice -n 10 /usr/bin/time -l`: one with a 2 s window, one with none (`--seconds 0`: start-up, tables, 0.25 s warm-up). Window instructions = difference. Window = 188 blocks (2.0053 s) at 48 kHz, 173 (2.0085 s) at 44.1. CPU % is an ESTIMATE at an assumed 10e9 instr/s (3.2 GHz, IPC ~3.1), second column at half that. No timed run was made (ADR-380 A1); `--timed` exists, unrun. Machine load ~15 throughout; instruction counts are load-free.
- **Self-checks (all PASS, each with a negative control):** IFFT vs direct sum (3.6e-6), corner identity both laws H=64/1024, Linear equivalence (1.9e-7 vs limit 1e-5), band-limit MIDI 0-127 at 44.1/48 kHz, static patch (0 rebuilds after warm-up; moving control 376/s), determinism (hash equal; other seed differs).

## Results (48 kHz unless stated; PF-3 chord, 8 voices; instructions in 10^6 for the window)

| H | law | M | static instr / CPU% | moving instr / CPU% | rebuild/s moving |
|---|---|---|---|---|---|
| 64 | Linear | 1 | 28.0 / 0.14 | 235.9 / 1.18 | 375 |
| 64 | Linear | 3 | 80.2 / 0.40 | 312.6 / 1.56 | 375 |
| 64 | Linear | 9 | 232.3 / 1.16 | 542.6 / 2.71 | 375 |
| 64 | Spectral log | 1 | 28.8 / 0.14 | 239.2 / 1.19 | 375 |
| 64 | Spectral log | 3 | 78.7 / 0.39 | 316.8 / 1.58 | 375 |
| 64 | Spectral log | 9 | 233.2 / 1.16 | 547.8 / 2.73 | 375 |
| 1024 | Linear | 1 | 29.5 / 0.15 | 398.4 / 1.99 | 375 |
| 1024 | Linear | 3 | 84.2 / 0.42 | 478.2 / 2.38 | 375 |
| 1024 | Linear | 9 | 247.0 / 1.23 | 719.2 / 3.59 | 375 |
| 1024 | Spectral log | 1 | 30.6 / 0.15 | 466.1 / 2.32 | 375 |
| 1024 | Spectral log | 3 | 84.3 / 0.42 | 546.2 / 2.72 | 375 |
| 1024 | Spectral log | 9 | 247.9 / 1.24 | 786.4 / 3.92 | 375 |
| 1024 | Spectral log | 9 @44.1 | 226.4 / 1.13 | 722.6 / 3.60 | 344.5 |

CPU % at the half-speed assumption is exactly double. Static cells rebuilt 0 times in the window (asserted).

Breakdown of the worst cell (H=1024, Spectral log, M=9, moving), instructions removed by switching a stage off: inverse FFTs 303 M (38.5 %), member reads 299 M (38.1 %), blend law 118 M (15.0 %), coupling 30 M (3.8 %); the parts sum to 750 M of 787 M (stages interact).

## Comparison with the current engine
The closest figure in the clean-room `REPORT.md` is §1, "Crunch horde, 8 voices held, by block": 13.01 x 10^9 instructions (after that commit; 14.10 x 10^9 before). It is the builder's whole-process count, window length not stated there, a different harness and workload (a full preset with filters/FX), so compare with care. On the face of it, the worst Quadrant cell (0.79 x 10^9 for 2 s) is under 1/16 of that figure. REPORT §7 gives PF-1 (Glass horde pad, 8 voices, 44.1 kHz) at 61.17 % of a core, timed; the Quadrant estimates are instruction-derived and cover the oscillator only (no envelope, filter, FX, voice management).

## Findings
- The spec's own CPU test is met by a wide margin in the moving case at H=64 (1.2-2.7 % at the 10e9 assumption, 2.4-5.5 % at half), but only M=1 is near the spec's "1-2 %"; nine swarm members per voice cost 2-3x that.
- Rebuilds are only ~half the cost under motion; the member reads are the other half, and under a set crossfade each read is four table reads. Static cost is reads only (0.14-1.24 %).
- Moving costs 2-8x static because of IFFTs plus the doubled reads during crossfades; with constant modulation the crossfade is always on.
- The spec's cached-log blend and its corner-identity acceptance test disagree at FRACTIONAL frames: lerping the cached ln|c| is a geometric lerp, not |lerp c|. The bench passes the test at integer frames (0, 6, 15) and reports fractional frames for Spectral log as a NOTE (worst error 4.7e3 x tol at H=64, 1.6e4 at H=1024). Needs a ruling: exact (hypot+log per corner, more cost) or reword the test.
- H=1024 at the 2048-sample cap leaves 2-4 samples per period at the top harmonics; the spec's 32-per-period rule would want 32768. Not run (see open questions).

## Alternatives rejected
Timed runs (ADR-380 A1). In-process instruction counters (not portable; `/usr/bin/time -l` is the REPORT's method). Per-level fade toward the brighter neighbour (it would read a level over the limit; band-limit check forbids).

## Verify
`nice -n 10 ./verify fast`: exit 1, solely `tolerance_registry_check` (2 new candidates in this tool: `EPS_LOG` 1e-4f and the inline-float-compare group); every other gate green. Not approved here: the human approves tolerances.

## Open questions
- `--maxn 32768` sensitivity (the spec's real level sizes at H=1024) not run; it is one command.
- The assumed 10e9 instr/s is not calibrated; a quiet-window `--timed` run would calibrate it.
- Fixed 0.85 gain stands in for normalisation; normalisation adds a peak scan per rebuild (not costed).
