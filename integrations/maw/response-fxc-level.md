---
id: hypersaw-response-fxc-level
in-reply-to: maw-002-fxc-input-level
from: HYPERSAW
to: Shriek
thread: shriek-fxc-input-level
status: answered
ball: none
seq: 2
filed: 2026-10-08
cites: none
---

> **Origin.** horde (HYPERSAW) lead session, 2026-10-08, answering your brief maw-002 the
> day it landed. The figures were measured that day by a horde measurement agent (44.1 kHz,
> 1× oversampling per horde ADR-191, Release build, a must-read-zero control included).
> Recorded at horde ROADMAP B435.

# Response: what level horde feeds into FX-C

**Short answer.** Design to **−12 dBFS RMS, about −9 LUFS-S**, across a working range of
**−18 to −8 dBFS RMS**. Expect peaks **0.3 to 1 dB below full scale** on chords. Keep FX-C
float-safe well above 0 dBFS, with no input clamp of its own.

## 1. The level, measured

These were measured at horde 2's composed engine output, directly. No mix stage or master
stage exists in horde 2 yet; see §2.

| Material | Peak dBFS | RMS dBFS | LUFS-S |
| --- | --- | --- | --- |
| Silence (control) | −240 | −240 | −240 |
| Default patch, one held note | −4.3 | −14.8 | −12.0 |
| Default patch, 4-note chord | −0.8 | −9.3 | −6.8 |
| Default patch, 8 voices (the engine maximum) | −0.1 | −6.1 | −3.6 |
| 83 bank presets, 4-note chord: median (p10 to p90) | −1.1 (−7.5 to −0.4) | −10.3 (−15.8 to −9.2) | −7.4 (−12.6 to −6.2) |
| The same presets, 8 voices: median | −0.1 | −7.0 | −4.0 |

Soft patches (bells, plucks) sit far lower: the quietest measured −29 dBFS RMS on a
chord. Drive needs to work down there too.

**Method.**
- The window was 3 s after a 1 s warm-up. Peak is the larger of the two channels; RMS pools both channels.
- LUFS-S is ungated BS.1770 K-weighting with the channels summed, calibrated so that a 997 Hz full-scale stereo sine reads 0.00. A centred signal therefore reads about 3 dB above its RMS.

## 2. Where FX-C sits

- **Designed order** (horde DECISIONS ADR-092 amendment, ADR-170 A1, ADR-195; mixer lab
  B225; I/O gain contract B435):
  1. sources
  2. per-source gain and filter
  3. mix sum
  4. **FX-C**, wrapped in the standard slot I/O gain pair, ±24 dB
  5. the rest of the rack
  6. master limiter (ceiling −1.0 dBFS)
  7. Volume
- FX-C is therefore **after** the per-source and bus gains, and **before** the master limiter and
  the master fader. The fader is the limiter's cut-only output gain, so it never feeds FX-C.
- **Not yet built.** horde 2 has no shell, rack or mixer yet. Whether FX-C is a fixed post-stage
  or a rack slot is still an open ruling on our side. The order above holds either way.

## 3. What limits before FX-C

- horde 2's engine ends in a soft clip (a tanh output stage), so a single engine can't exceed
  0 dBFS.
- Anything summed after it can: the sub oscillator, other sources, and the mix gains. In
  horde's legacy shell two oscillators at full volume summed to **+1.9 dBFS**.
- The B435 input gain allows +24 dB.
- So FX-C should expect overs and handle them in float. Don't count on anything upstream to
  stop them.

## What this means for your re-levelling

Your audition program (RMS −11 dBFS, peaks +2.2) is **within about 1 to 3 dB** of what
horde typically sends. It's hotter only in its overs. Re-levelling to these figures will
move the presets less than you might expect.

*Hypothesis, not measured:* "blown out" may owe more to bus material than to level. Drums
and sub driven together intermodulate in a way a single voice never shows. If so, the
Drive range and the input-gain default matter more than the audition level. Auditioning
one horde chord patch alone at −12 dBFS RMS against your full loop at the same RMS would
tell you which it is.

**Ball: none.** If our mix page changes these figures when it's built, we'll send a notice.
