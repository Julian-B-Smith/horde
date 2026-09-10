# horde — measurements

The direct answer to "parity isn't correctness" (`ROADMAP.md` B103; `docs/ENGINEERING.md`
§1). Parity certifies that the C++ engines agree with the browser prototypes; it says nothing
about whether the oscillator aliases or what it costs. These two tables do, for the
**shipped plugin through the CLAP factory** — not a core-direct harness (`LIBRARY.md` L0031).

- **Build:** `319a758` (each tool prints its own stamp; the tables below are its verbatim output).
- **Machine:** Apple M3, 8 cores, 24 GB; macOS 26.6.2; Apple clang 16.0.0; CMake 4.1.2;
  `-DCMAKE_BUILD_TYPE=Release`. The CPU table is meaningless from a Debug build.
- **Deterministic:** the renders use only the plugin's seeded streams; the aliasing table is
  bit-identical run to run. The CPU table is wall-clock around a deterministic render, min of
  three runs per cell, and moved by ≤ 3 % between two runs on the same machine.

```bash
cmake -S . -B build-release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build "$(pwd)/build-release" --target measure_alias measure_cpu -j"$(sysctl -n hw.ncpu)"
./build-release/measure_alias
./build-release/measure_cpu        # alone — nothing else compiling or rendering
```

Sources: `tools/measure_alias.cpp`, `tools/measure_cpu.cpp` (standalone; registered in
`CMakeLists.txt` beside `svf_check`; not gates — they print, never judge).

## 1. Aliasing — one oscillator, four notes, saw shape on and off

**Setup.** `n=1`, detune 0, width 0, K 0, drift 0 (one plain oscillator; params 1/4/14/6/9), a
held note at MIDI 36 / 60 / 84 / 96, 1 s settle, then 2^17 samples. Saw shape "on" is the
ADR-094 section at `sawProfile`(130)=0.5, `round`(131)=0.6, `roundHi`(132)=0.5. Three modes:
naive (`digital` 16 = 0), polyBLEP (`digital` = 1, the shipped default), polyBLEP with 2×
oversampling (`oversample` 88 = 1). 44.1 k and 96 k.

**Two figures per cell, `integral / worst`.** *Integral* is the energy in every FFT bin farther
than 16 bins from any harmonic over the energy within 16 bins of the harmonics, in dB — the
"non-harmonic to harmonic" ratio. It is only honest because of the window: a Kaiser window at
β = 19 puts sidelobes below −180 dB, where a Hann window's leakage from hundreds of harmonics
once reported a clean polyBLEP saw at −27.7 dB (`LIBRARY.md` L0016 — the recorded trap this
tool was built around). *Worst* is the tree's calibrated protocol: the largest local peak at
any inter-harmonic midpoint (k+½)·f0, relative to the fundamental
(`tools/blep_alias_incommensurate_probe.cpp`).

**Calibration first** (L0016/L0032): two synthetic signals with no plugin involved. The
additive band-limited saw is alias-free by construction and must read the detector's floor;
the naive saw `2·frac−1` must read large.

### Detector calibration (synthetic, no plugin)

| note | f0 Hz | sr | additive band-limited saw: integral / worst (must read the floor) | naive saw: integral / worst (must read large) |
|---|---|---|---|---|
| 36 | 65.41 | 44.1k | -159.3 / -185.5 dB (336 midpoints) | -27.7 / -59.3 dB |
| 60 | 261.63 | 44.1k | -158.9 / -200.2 dB (83 midpoints) | -21.4 / -77.4 dB |
| 84 | 1046.50 | 44.1k | -159.0 / -210.2 dB (20 midpoints) | -15.4 / -78.9 dB |
| 96 | 2093.00 | 44.1k | -159.1 / -216.0 dB (9 midpoints) | -12.1 / -78.8 dB |
| 36 | 65.41 | 96.0k | -159.3 / -179.8 dB (732 midpoints) | -31.0 / -64.6 dB |
| 60 | 261.63 | 96.0k | -159.5 / -190.9 dB (182 midpoints) | -24.8 / -68.6 dB |
| 84 | 1046.50 | 96.0k | -159.2 / -204.1 dB (44 midpoints) | -18.7 / -85.5 dB |
| 96 | 2093.00 | 96.0k | -159.0 / -209.8 dB (21 midpoints) | -15.6 / -70.7 dB |

### The shipped oscillator (CLAP factory, n=1)

| note | f0 Hz | sr | saw shape | naive: integral / worst | polyBLEP: integral / worst | polyBLEP + 2x OS: integral / worst |
|---|---|---|---|---|---|---|
| 36 | 65.41 | 44.1k | off | -28.4 / -60.9 dB | -44.5 / -88.0 dB | -60.7 / -86.2 dB |
| 36 | 65.41 | 44.1k | on | -29.2 / -62.0 dB | -45.4 / -89.1 dB | -62.6 / -87.2 dB |
| 60 | 261.63 | 44.1k | off | -22.2 / -78.0 dB | -38.5 / -149.2 dB | -55.0 / -148.8 dB |
| 60 | 261.63 | 44.1k | on | -22.9 / -79.1 dB | -39.4 / -150.1 dB | -57.0 / -149.7 dB |
| 84 | 1046.50 | 44.1k | off | -16.1 / -80.5 dB | -32.8 / -185.9 dB | -51.8 / -184.8 dB |
| 84 | 1046.50 | 44.1k | on | -16.8 / -81.6 dB | -33.6 / -171.6 dB | -54.1 / -173.7 dB |
| 96 | 2093.00 | 44.1k | off | -12.8 / -79.0 dB | -28.8 / -187.1 dB | -48.1 / -184.0 dB |
| 96 | 2093.00 | 44.1k | on | -13.5 / -80.1 dB | -29.6 / -175.2 dB | -49.9 / -172.3 dB |
| 36 | 65.41 | 96.0k | off | -34.1 / -65.3 dB | -52.0 / -98.1 dB | -68.0 / -96.0 dB |
| 36 | 65.41 | 96.0k | on | -34.8 / -66.5 dB | -52.7 / -99.2 dB | -69.6 / -96.9 dB |
| 60 | 261.63 | 96.0k | off | -27.8 / -69.3 dB | -46.0 / -129.4 dB | -62.4 / -126.5 dB |
| 60 | 261.63 | 96.0k | on | -28.5 / -70.4 dB | -46.7 / -130.4 dB | -64.1 / -127.5 dB |
| 84 | 1046.50 | 96.0k | off | -21.6 / -82.1 dB | -39.5 / -186.1 dB | -55.2 / -185.1 dB |
| 84 | 1046.50 | 96.0k | on | -22.3 / -83.2 dB | -40.3 / -177.2 dB | -56.7 / -179.3 dB |
| 96 | 2093.00 | 96.0k | off | -18.4 / -71.1 dB | -36.0 / -172.0 dB | -50.7 / -184.2 dB |
| 96 | 2093.00 | 96.0k | on | -19.1 / -72.2 dB | -36.7 / -149.2 dB | -52.0 / -180.3 dB |

Build 319a758 · 2026-09-10 13:05Z. FFT 2^17, Kaiser beta 19, harmonic exclusion +-16 bins, 1 s settle.

### What the aliasing table shows

- **The detector is calibrated, and by closed form.** The clean control reads a flat −159 dB
  floor at every note and rate (the window's residue, ~115 dB below anything measured on the
  plugin). The naive control is predicted exactly: a 1/k saw's alias-to-harmonic energy ratio
  is Σ_{k>K} 1/k² over Σ_{k≤K} 1/k² with K harmonics below Nyquist; at MIDI 96 / 44.1 k that
  is 0.095 / 1.55 = **−12.1 dB, and the tool reads −12.1**; at MIDI 36 it predicts ≈ −27.4 and
  reads −27.7. (That last number happens to coincide with L0016's famous wrong reading; the
  same-note clean control at −159 is what says this one is not leakage.)
- **The plugin's naive mode is a naive saw** — it matches the synthetic one within 0.8 dB in
  every row. Nothing non-harmonic comes out of the shell at n=1 besides the saw's own fold-back.
- **polyBLEP buys about 16 dB of alias energy over naive, at every note and rate; 2× oversampling
  buys another ~16 dB.** Worst-case midpoints go from −60…−80 dB (naive) to −88 dB at MIDI 36
  and below −149 dB from MIDI 60 up (polyBLEP). 96 k is worth ~7 dB over 44.1 k in the integral.
- **The saw-shape section adds discrete fold-back at high notes, and the reference shares it.**
  With shape on, the integral barely moves (< 1 dB — the rounding softens the waveform, so it
  is even slightly *lower*), but the worst midpoint at MIDI 84 / 96 rises from −186 / −187 dB
  to −172 / −175 dB at 44.1 k, and from −172 to −149 dB at MIDI 96 / 96 k. Rounding is a
  waveshaper applied after the BLEP, so it creates harmonics the BLEP never band-limited; 2×
  oversampling recovers most of it (−174 / −172 / −180 dB). The JS reference computes the same
  expressions, so **parity certifies this** rather than catching it — the L0031 point, in a
  number. Whether any of it is audible is the listening note's question, not this table's.

### What it does not show

- **Only n = 1.** A swarm of n detuned voices is n such spectra summed; the ratio does not
  improve with n, but detune smears the harmonic comb, so this detector cannot be pointed at a
  real patch without re-calibration for that signal class (L0017). Nothing here covers
  coupling, drift, gravity, the FX rack (all Off), or the second oscillator.
- **The integral's declared blind spot:** an alias that lands within 16 bins of a harmonic
  (11.7 Hz at 96 k) is counted as harmonic. **The midpoint figure samples** the alias comb
  where sr/f0 arithmetic happens to put it, so it ranks modes within a row and is not an
  absolute — the naive control reading "better" at MIDI 60 / 44.1 k than at 96 k is that.
- **Audibility.** These are energies and peaks, not loudness; the listening note is the
  other half.

## 2. CPU per voice count

**Setup.** The E-6 envelope: 44.1 kHz, 128-sample blocks, eight held notes (keys 48 + 3k),
swarm size `n` per note (param 1 "Voices" and its osc-2 twin 1001), one oscillator and two
(osc 2 = `enable` 1150 + `vol` 1017 = 0.4; ADR-100 ships it off). Everything else at the
shipped defaults: polyBLEP on, oversampling off, FX rack Off. The last column is the shell's
own readback of the osc-2 params, printed so a wrong id would show in the table instead of
silently timing the default (L0032).

| voices per note (n) | 1 osc: ms CPU / s audio | 1 osc: % of a core | 2 osc: ms CPU / s audio | 2 osc: % of a core | readback (osc2 n / enable / vol) |
|---|---|---|---|---|---|
| 1 | 5.4 | 0.54% | 10.5 | 1.05% | 1 / 1 / 0.4 |
| 4 | 11.6 | 1.16% | 22.6 | 2.26% | 4 / 1 / 0.4 |
| 8 | 20.1 | 2.01% | 38.4 | 3.84% | 8 / 1 / 0.4 |
| 16 | 37.3 | 3.73% | 72.4 | 7.24% | 16 / 1 / 0.4 |
| 32 | 70.0 | 7.00% | 141.5 | 14.15% | 32 / 1 / 0.4 |

Build 319a758 · 2026-09-10 13:05Z. 8 held notes (keys 48+3k), 44100 Hz, 128-sample blocks, 5 s per cell, min of 3 runs.

### What the CPU table shows

- **Linear in n, with a small fixed cost.** One oscillator: ≈ 2.1 ms of CPU per second of
  audio per unit of n (eight notes), i.e. ≈ 0.26 ms per oscillator-second — about 0.026 % of
  one M3 core per swarm voice — over a ≈ 3.3 ms/s floor. Two oscillators cost 2.0× one.
- **The worst cell is 512 oscillators (32 × 8 notes × 2) at 14 % of one core**, against the
  50 % budget `tools/cpu_bench.cpp` states for the E-6 envelope. The shipped default (n = 7,
  one oscillator) sits near the n = 8 row: ≈ 2 % for an eight-note chord.

### What it does not show

- **This machine only.** ADR-082's min-spec question — how much of this survives a ×4 derate
  on the hardware the instrument is meant to reach — is answered only by running the same
  tool there. No hostname, no user: the CPU model is the only identity recorded.
- **Not the DAW.** No host overhead, no GUI, no oversampling (adds a 2× render), no FX (all
  Off), no parameter automation. `tools/shell_bench.cpp` is the tool for toggling suspects.
- **Idle cost** (voices held after release, disabled oscillator) is `README.md`'s 0.03 %
  figure and `tools/ratchet_probe.cpp`'s question; not re-measured here.

## 3. Listening note

> **TODO(human) — the listening note.** This section needs ears and a DAW and is deliberately
> not written by the tool that wrote the rest. What it should answer, against the rows above:
> (1) at MIDI 84 / 96 with the saw-shape section engaged, is the fold-back the table finds at
> −150…−175 dB audible at all, with and without 2× oversampling — if not, the shape section's
> aliasing is a number, not a defect; (2) the polyBLEP-vs-naive difference at MIDI 36 (worst
> −88 vs −61 dB) — is naive ever the sound you want, since it is a switch the patch carries;
> (3) anything the tables cannot see: a swarm at n = 7 with detune, coupling K swept −1 → +1,
> gravity settling a chord — where the reference and the port agree by construction and only
> an ear can say whether they are right. Record the build stamp, the patch, and the sample
> rate with each observation. This note, with the two tables, is the input to the goldens-v2
> workshop (`ROADMAP.md` B100).
