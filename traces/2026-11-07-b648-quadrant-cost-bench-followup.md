# b648-quadrant-cost-bench-followup — the bench now measures the design SPEC-WAVETABLE specifies

- **Queue item:** ROADMAP B648 (follow-up from the lead, 2026-11-07; extends `2026-11-07-b648-quadrant-cost-bench.md`)
- **Why:** the first run measured a cheaper design than the draft spec (b649-wavetable-spec: WT-5, 6, 6a, 15, 16, 23, 25) and an independent critic said so. The bench gains run-time options for each missing piece, so tonight's timed run (`--timed --followup`) measures the specified design.
- **Evidence consulted:** `git show b649-wavetable-spec:specs/SPEC-WAVETABLE.md` rows WT-5, 6, 6a, 15, 16, 21, 23, 25; the first trace; instruction counts below (`/usr/bin/time -l`, difference against a no-render child, as before; machine load ~13).
- **What was added** (all options, defaults = first run's design): `--read 2|6|8` (6-point Lagrange at N = clamp(16h, 2048, 8192); 8-point at 4096), `--spacing 1|2`, `--norm 1` (WT-16: Parseval RMS gain, +24 dB cap, top-level peak limit), `--dc 1` (one-pole per voice), `--pv 1` (WT-23 (a), 8 reads per member-sample, no rebuild), `--xfade 0|1|2` (WT-15 (i), (ii) fused, (iii) interpolated set every `--xstep` = 32). `--timed` takes every option; `--timed --followup` runs the follow-up cells.
- **New self-checks (all PASS, each with a control):** interpolating reads (6/8-point 5e-7 / 3e-7 vs 2-point 4.8e-3 on a 32-per-period sine), Linear equivalence at N0 = 8192 half-octave 6-point (2.4e-7), per-voice blend (a) equals pre-blend (b) (2.0e-7), WT-16 (RMS target or peak-limited, peak <= 1; fixed-0.85 control misses by 76 %), scheme (iii) lands exactly on the new set, band-limit over spacing x read x H x fs, static patch and determinism for five option sets.

## Results (H = 1024, M = 9, 48 kHz, 2.0053 s window; CPU % ESTIMATED at 10e9 instr/s, double at half)

| design | instructions | rebuild/s | CPU % |
|---|---|---|---|
| Spectral log, 2-point, 2048, fixed 0.85 (the first run's worst cell, re-measured by this build) | 805.2 M | 375 | 4.02 |
| Spectral log, 6-point/8192, octave, WT-16, DC | 2,663.6 M | 375 | 13.28 |
| Linear, same | 2,583.2 M | 375 | 12.88 |
| Linear + per-voice frame (WT-23 a), same | 2,048.9 M | 0 | 10.22 |
| Spectral log, half-octave | 2,880.5 M | 375 | 14.36 |
| Linear, half-octave | 2,806.3 M | 375 | 13.99 |
| Linear + per-voice frame, half-octave | 2,048.5 M | 0 | 10.22 |
| Spectral log, octave, WT-15 (ii) fused | 2,212.6 M | 375 | 11.03 |
| Spectral log, octave, WT-15 (iii) interpolated set | 2,006.0 M | 375 | 10.00 |
| Spectral log, octave, 8-point/4096 (fallback) | 2,892.5 M | 375 | 14.42 |
| Spectral log, 6-point, octave, static | 811.7 M | 0 | 4.05 |
| Spectral log, 6-point, octave, 44.1 kHz | 2,439.3 M | 344.5 | 12.14 |

Breakdown, Spectral log 6-point octave moving (2,652 M): member reads 1,433 M (54.0 %), inverse FFTs 1,023 M (38.6 %), blend law 117 M (4.4 %), coupling 26 M (1.0 %). Linear + per-voice frame: reads 1,971 M (96.1 %), coupling 1.5 %; the rest is within noise (nothing rebuilds).

## What changed against the first run
- The specified design costs about 3.3 times the first run's worst cell (13.3 % against 4.0 %), at the assumed rate; the 6-point read alone triples the static cost (1.2 % to 4.05 %): its coefficients (a 6-term Lagrange kernel) are computed per member-sample per level, scalar and unoptimised, and the pitch crossfade reads two levels.
- The IFFTs also grew (top-level 8192 and WT-16's always-built top level and peak scan): 303 M to 1,023 M.
- Half-octave levels add about 8 % (more levels in use, not more reads). The 8-point fallback is no cheaper than 6-point here (it halves the largest level, which instruction counts do not reward; WT-P3's timed run judges that).
- Per-voice frame under Linear (a) costs 77 % of the shared-frame Linear cell, because it never rebuilds, but 8 reads per level make it 96 % reads.
- WT-15 scheme (iii) saves 25 % and (ii) 17 % against scheme (i) at the 6-point read.
- **The default (first-run) design in this build reads 2 % (moving) and 15 % (static) above the first trace's table** (e.g. 802 M against 785 M moving, 283 M against 245 M static, same cell, measured side by side against the first commit's binary): the loop was restructured (read struct, member accumulation, options). Compare cells within one table; the first trace's numbers stay valid for the first design.

## Caveats
- WT-5's raised-cosine fade over the top third of each range and its per-sample mix weight are not modelled (pitch is fixed in the workload; the weight is linear in log pitch). The Gram-matrix gain for WT-16 under strategy (a) is not costed. Cache effects are invisible to instruction counts (WT-P3): tonight's timed run is where the 8192 levels are judged.
- No timed run was made. `--timed` was compiled and arguments parsed, not run.

## Alternatives rejected
Reading two sets with shared coefficients as scheme (i) (that is (ii)); per-sample XY weights for strategy (a) (per 16-sample tick here); an FFT-size-dependent twiddle (unchanged).

## Verify
`nice -n 10 ./verify fast`: exit 1, solely `tolerance_registry_check` (the tool's `inline-float-compare` group changed with the new literals; needs the human's `approved:` ref). Every other gate green. Not approved or reworded here.

## Open questions
- None blocking. If `tolerance_registry_check` reds on the new literals, the human approves them; I did not.
