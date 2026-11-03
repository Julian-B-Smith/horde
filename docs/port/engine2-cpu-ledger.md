# The new engine's CPU ledger

Origin: horde lead session, 2026-11-03, ADR-347 and ADR-354. The figures are the builder's commit `5442272`, before any CPU pass, on an Apple M3. This file stays out of the test author's reach (ADR-247).

## The official figure

PF-3's protocol and load guard, the unmodified tool, load 1.5 to 1.6:

| Cell | Share of a core | Target |
|---|---|---|
| Glass horde pad, 8 voices, 44.1 kHz | **217.31 %** | 10.7 % |
| Glass horde pad, 8 voices, 48 kHz | 212.95 % | |

## Against the current engine

Measured, not PF-3: scratch copies of both tools with the load limit at 5 (the human, 2026-11-02). The current engine runs at `--os 1`, 44.1 kHz, 8 voices. Load read 2.35 to 4.6 in its three rounds, whose Glass horde pad figures agree within 0.1 %.

| Preset | Current | New | New ÷ current |
|---|---|---|---|
| Quarter sync | 1.66 % | 9.53 % | 5.7× |
| Harmonic stack | 9.88 % | 52.69 % | 5.3× |
| Fold over sync | 6.92 % | 52.32 % | 7.6× |
| Crushed bells | 15.06 % | 104.21 % | 6.9× |
| Breathing pad | 19.71 % | 147.90 % | 7.5× |
| Glass horde pad | 26.73 % | 218.14 % | 8.2× |
| Glass horde pad, `edges` 0 on both blades (new only) | | 93.65 % | |

The lead's estimate before the run, recorded 2026-11-02: about 250 % (225 to 300) against about 29 %. The result is below that range.

## Where the time goes

A sample of `pf1` on a `-O3 -g` copy, 23,116 samples. Line numbers are `5442272`'s `swarm/blades.cpp`.

| Where | Share |
|---|---|
| `blade_sample` and what it calls | 88 % |
| ED-7's taper integral, lines 941 to 1018 | at least 48 % |
| of which line 1014, the 32-tap write inside the eight nodes | 44 % |
| Math-library calls | about 12 % |
| `effective` | 4 % |
| `window_point` | 3 % |

The `edges` 0 control agrees: tapers are 57 % of this patch. 83 presets set blade 1's edge, with a median of 0.15, and the heavy ones set it high (Glass horde pad 0.7 and 0.6).

## The ambitious patch, scratch

The lead's own reading of PF-10's wording, not the test author's cells, and no morph yet. Two oscillators and the sub on Glass horde pad, 44.1 kHz. Worst block is the smallest of five repeats' worst, as a share of the block's duration.

| Notes | Voices | Average | Worst block |
|---|---|---|---|
| Held chord | 8 | 439 % | 483 % |
| Note stream | limit 8 | 411 % | 480 % |
| Held chord | 16 | 1,110 % | 1,169 % |
| Note stream | limit 16 | 688 % | 936 % |

## Voices

Glass horde pad: 218.58 % at 8 voices, 552.21 % at 16, 19,557.54 % at 64, so per voice 27.3, 34.5 and 305.6 %. PF-5 says cost grows linearly. PF-3's chord adds keys upward, reaching key 237 at 64 voices, so a likely cause is that higher notes make more edges. That's a hypothesis, not tested. The 64-voice figure says little about real playing.

The sanity control read 0.830 between unison 16 and unison 1, within its factor of 2.
