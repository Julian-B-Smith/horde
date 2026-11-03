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

Glass horde pad: 218.58 % at 8 voices, 552.21 % at 16, 19,557.54 % at 64, so per voice 27.3, 34.5 and 305.6 %. PF-5 says cost grows linearly. PF-3's chord adds keys upward, reaching key 237 at 64 voices. The gauntlet shows higher notes do cost more (1.4× an octave up, 1.7× at three), which covers the 16-voice step at most. It doesn't explain 64 voices, whose cause isn't known. That figure says little about real playing.

The sanity control read 0.830 between unison 16 and unison 1, within its factor of 2.

## The cost gauntlet

The human's request, 2026-11-03. One change at a time, scratch tool, not PF-3, load about 1.7, 8 voices, 44.1 kHz, best of 5, spreads under 1.4 %. Two runs of the unchanged patch read 218.2 and 221.1 %, so differences under about 3 points aren't real.

**Taking away from Glass horde pad (221 %):**

| Change | Reads |
|---|---|
| All edge correction off (`bandLimit` 0) | 77 % |
| Edges 0 on both blades | 96 % |
| ED-7's correction off (code) | 96 % |
| Blade 1 off | 106 % |
| ED-7's integral at 1 point (code) | 116 % |
| ED-7's integral at 2 points (code) | 131 % |
| Blade 2 off | 150 % |
| Both blades off | 14 % |
| FM reset to Sync on blade 1 | 201 % |
| Sine→saw carrier to saw on blade 2 | 205 % |
| The per-member filter E off (code) | 212 % |
| Spread law, spread drift, cut rule, quantize, coupling, detune, width, FM depth 0 | 218 to 221 % |
| Cross-mod 0 | 255 %: more, since a live loop skips the corners' correction |
| FM reset to FM free on blade 1 | 310 % |
| High quality | 335 % |

**Adding to a plain swarm of 9 members:** plain 13.9 %; coupling 13.9 %. One blade at Edges 0: Noise 49.5, Sync 51.0, Fold 51.1, Ring 52.8, Crush 54.5, FM reset 63.9, FM free 150.1 %. Sync with: a sine→saw carrier 70.9, twins 57.1, Edges 0.1 86.0, Edges 0.3 137.6, Edges 0.7 147.9, cross-mod 51.8, feedback 51.7 %. Two Sync blades 81.1 %, with serial 82.0, with collision 82.1 %.

**Members on Glass horde pad:** 1, 29.4 %; 3, 77.4 %; 5, 125.5 %; 9, 221 %; 16, 389.4 %. About 24 points a member at 8 voices.

**Height on Glass horde pad:** up 1 octave 313.7 %; 2, 338.3 %; 3, 385.0 %.

**Profiles of single blades:** Sync at Edges 0 has no one hot spot: 46 % in `blade_sample` spread over many lines, `effective` 9.5 %, `render` 12 %. FM free: about 33 % in `sin` and the math library, `hot` 10 %, `dc_estimate` 5 %.

**The soft-edge A/B, against the exact version, on Glass horde pad and Breathing pad.** A control of exact against exact reads below −550 dB.

| Phrases | 2 points | Off |
|---|---|---|
| Held, chord, fast line (keys up to about 69) | −77 to −93 dB | −59 to −75 dB |
| Fast line up 2 octaves | −55 to −62 dB | −37 to −38 dB |
| Held note up 3 octaves | −48 to −57 dB | −23 to −32 dB |

The lower phrases alone would have made “off” look safe. The suite hasn't judged any of these versions, and nobody has heard them yet.
