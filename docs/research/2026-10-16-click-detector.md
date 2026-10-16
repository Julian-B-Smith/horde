# A level-relative click detector for bright tones: method as built, controls, proposed numbers

**Date** 2026-10-16 · **Roadmap** B490 (serves B494 step 2, B486) · **Decision** ADR-225 item 12
· **Read at** `origin/main` `2e4ad6c` · **Author** an implementer dispatched by the horde lead.
No engine change.

**The numbers here are PROPOSED.** ADR-225 item 12 ratified the method ("a predictor from the
preceding samples; a click is a residual far over the local residual level; planted-step
controls") and left the numbers to the test author, to be ratified by the human with their
controls. Nothing below is a ruling until the human makes it.

**Files.** `tools/click_detector.mjs` (the module and a CLI), `tools/click_detector_check.mjs`
(its oracle: a synthetic half in `./verify fast`, an engine half in `./verify full`). Every number
in sections 3 to 6 is printed by the check on every run unless marked "measured once".

**Words.** A *render* is one channel of samples. The *residual* is a sample minus what the samples
before it predict. A *score* is a sample's residual over the residual level around it. An *event*
is a run of samples scoring over *k*. A *plant* is a fault added on purpose: a *step* (every later
sample raised by s), a *spike* (one sample raised by s) or a *corner* (the rise s spread over 1 ms:
a slope that changes, no step). Plant sizes are fractions of the render's own peak. The *smooth
set* is renders known to hold no click.

## 1. The answer, in plain words

The detector catches every 0.05 step (one twentieth of the render's peak) on a sine, on the
engine's band-limited saw at C2 and C6, and on Crushed bells at C4, and every step of 0.05 of full
scale on Crushed bells (B490's own wording). The old detector caught none of the same 0.05 steps on
Crushed bells, and none of them on the saws. On the smooth set (the sine and the band-limited saw
held from C1 to C7, both channels, plus white noise, filtered noise and an FM tone) it reads no
event; the largest score there is 5.2, half the proposed k.

It cannot see a 0.05 step on three of the brief's patches: **Glass horde pad**, **Breathing pad**
and the **Crushed bells chord**. Each of them is made, by itself, of steps as large as the plant:
Glass horde pad steps at the engine's 32-sample tick (a finding, section 5), Breathing pad's noise
blade steps by design, and the chord's 24 members each reset on their own period, which a single
pitch predictor cannot follow. On those three a step is caught every time only from about 0.2 of
the peak, and not even then on all of them. That is the exception the brief asked to be stated; it
is not a tuning problem (section 6 shows no setting that fixes it without false events).

On the 83 bank presets played as a chord, 16 read events at k = 10. Almost all are the tone's own
discontinuities (crush blades, sample-and-hold noise, hard sync, feedback); one family is a real
step the engine makes that nobody has listened for (Glass horde pad, section 5).

## 2. The method, as built

Per channel, in order (the module's header has the measurement behind each choice):

1. **Analysis band.** A causal 8th-order Butterworth low-pass at 0.75 of Nyquist. What the pitch
   predictor cannot cancel lives near Nyquist; with the band open the engine's own band-limited
   saw at C1 read an event at every reset. Causal, because a zero-phase filter rings before a
   step and the predictor, seeing the ringing, half-anticipated it.
2. **The first difference is predicted**, so the predictor of a sample always carries a zero at
   DC: a step leaves one unpredicted sample instead of a lasting offset.
3. **Short-term predictor**, order p, fitted by the autocorrelation method (Hann window,
   Levinson-Durbin, white-noise correction eta) on the window before each block, refitted every
   hop, crossfaded block to block. Burg's method was tried first and rejected: on a pure
   band-limited tone its residual at a saw's reset was 588 times the tone's peak.
4. **Pitch predictor.** The lag is the shortest strong peak of the short-term residual's
   normalised autocorrelation; 33 taps around it are fitted by exact least squares over the 40 ms
   before each block. The short-term predictor then whitens what it leaves. A click inside the
   fitting window would teach it (the tone's own resets then read as a train of events one period
   apart), so a second pass holds the last clean plan for up to 0.1 s after a first-pass hit.
5. **Residual** = per sample, the smaller of the short-term and the pitch-then-short-term
   residual: a sample is a click only if neither explains it.
6. **Score** = |residual| over the RMS of the residual in the 20 ms around it, leaving out the
   sample's own footprint (the p samples its residual runs through the error filter) and that
   footprint's echo one period later; never below -60 dB under the peak of the analysed signal.
7. **Events**: samples scoring over k; hits within 1.5 ms are one event; an event where a stronger
   event's echo falls (one pitch period later) is that echo and is dropped.

**Level-relative**: every stage is linear or a ratio and the floor follows the peak. The check
judges it: the same events at gains 0.01, 1 and 4, scores within 1.5e-9 to 5.8e-7 relative.

**Not judged**: the first 0.12 s (the predictors' history) and the last 8 samples. A note-on at
sample 0 has its onset unjudged.

## 3. The proposed numbers and how each was chosen

| number | proposed | procedure |
|---|---|---|
| **k** | **10** | The smallest k at which the smooth set reads no event is its largest score: 5.2 on the engine set, 4.4 on the synthetic set. k is about twice that, for renders the set has not seen (white noise's largest score grows as the square root of the log of the render's length: an hour of it would read about 1.4 times the 1.5 s figure). At 10, every judged must-fire row is caught 10 of 10 (table A). |
| **p** (order) | 24 | A plateau (table C): 12, 24 and 48 give the same smooth maximum (5.3 / 5.2 / 4.6) and the same catches; 24 holds Crushed bells' weakest step highest of the cheap ones (20.7 against 18.4 at 12). |
| **M** (fitting window) | 10 ms (480 samples at 48 kHz) | Same plateau; 5 ms cuts Breathing pad's median plant from 6.9 to 2.1, 20 ms cuts the saw at C2's weakest from 37.5 to 23.4. |
| refit hop | 1.25 ms | Not swept: the crossfade makes the residual continuous across refits; cost only. |
| eta | 1e-5 | The white-noise correction that keeps the filter's gain bounded (the Burg failure above); not swept further. |
| local window | 20 ms centred (±10 ms) | 10 ms and 40 ms move Crushed bells' weakest step between 23.2 and 19.3 and the smooth maximum not at all (table C). |
| floor | -60 dB under the analysed signal's peak | The most sensitive 10 dB step at which the engine's smooth set reads under k: at -70 dB the band-limited saw at C1 read 29 (table D). The cost: the smallest event on a perfectly predicted tone is k x floor, 1 % of that peak. |
| **merge** | 1.5 ms; plus the echo rule | One step's residual runs through the 24-tap error filter (0.5 ms); 1.5 ms holds that with room. The echo rule (section 2, step 7) removed the one echo a step left one period later on a saw at C2. |
| pitch predictor | fmin 24 Hz, 33 taps, fit 40 ms, refit 10 ms, hold 0.1 s | 17 taps and 33 behave alike on the patches (table C); 33 keeps a full-band saw's reset cancelled to 1e-5. fmin covers C1. |
| analysis band | 0.75 x Nyquist, order 8 | 0.6 lowers every catch (the saw at C6's weakest from 22 to 12); 0.9 lets the smooth maximum rise to 7.9 (table C). |

## 4. The tables

**A. 0.05 x peak steps caught, of 10, against k** (engine, the check's TABLE rows; "alone" is the
largest score the patch reaches with nothing planted):

| patch | k 6 | 8 | **10** | 12 | 15 | 20 | 25 | 30 | alone |
|---|---|---|---|---|---|---|---|---|---|
| sine C4 | 10 | 10 | **10** | 10 | 10 | 10 | 10 | 10 | 0.0 |
| bare saw C2 (band-limited) | 10 | 10 | **10** | 10 | 10 | 10 | 10 | 10 | 2.3 |
| bare saw C6 (band-limited) | 10 | 10 | **10** | 10 | 10 | 10 | 10 | 9 | 0.4 |
| Crushed bells C4 | 10 | 10 | **10** | 10 | 10 | 8 | 7 | 6 | 5.8 |
| Glass horde pad C4 | 3 | 1 | **0** | 0 | 0 | 0 | 0 | 0 | 39.0 |
| Breathing pad C3 | 5 | 5 | **4** | 4 | 4 | 3 | 1 | 0 | 17.8 |
| Crushed bells chord C4 E4 G4 B4 | 1 | 0 | **0** | 0 | 0 | 0 | 0 | 0 | 5.6 |

**B. Events on clean renders against k** (both channels):

| set | k 6 | 8 | **10** | 12 | 15 | 20 | 25 | 30 |
|---|---|---|---|---|---|---|---|---|
| smooth set (sine and band-limited saw, C1 to C7; 28 channels) | 0 | 0 | **0** | 0 | 0 | 0 | 0 | 0 |
| 83 bank presets, chord C4 E4 G4 B4 at 0.85 (166 channels) | 6156 | 2307 | **1534** | 1251 | 666 | 270 | 110 | 50 |
| defaults, Crushed bells, Glass horde pad, Breathing pad at C1 to C7 (56 channels) | 3677 | 2069 | **1361** | 933 | 570 | 280 | 163 | 104 |

(The scores are computed once, at the proposed k's second pass; the counts at other k are read off
them. A run at another k would hold plans after its own hits, so its counts may differ slightly.)

**Catch rate by size at k = 10** (steps of 10; spikes and corners of 5; the check's MUSTFIRE,
EXCEPT and PLANT rows):

| patch | step 0.02 | 0.05 | 0.1 | 0.2 | spike 0.05 / 0.2 | corner 0.05 / 0.2 |
|---|---|---|---|---|---|---|
| sine C4 | 10 | 10 | 10 | 10 | 5 / 5 | 0 / 2 |
| bare saw C2 | 10 | 10 | 10 | 10 | 5 / 5 | 0 / 0 |
| bare saw C6 | 10 | 10 | 10 | 10 | 5 / 5 | 4 / 5 |
| Crushed bells C4 | 6 | 10 | 10 | 10 | 5 / 5 | 0 / 0 |
| Glass horde pad C4 | 0 | 0 | 3 | 9 | 1 / 4 | 0 / 0 |
| Breathing pad C3 | 0 | 4 | 4 | 7 | 2 / 5 | 0 / 0 |
| Crushed bells chord | 0 | 0 | 3 | 8 | 0 / 4 | 0 / 0 |
| Crushed bells C4, 0.05 of FULL SCALE (B490) | | 10 | | | | |

**C. Sweeps, one number at a time** (measured once, 2026-10-16; 8 plants each; "min" is the
weakest 0.05 step's score, "median" the middle one):

| change | smooth max | CB C4 min | saw C2 min | saw C6 min | sine C4 min | GHP median | BP median | CB chord median |
|---|---|---|---|---|---|---|---|---|
| p 12 | 5.3 | 18.4 | 38.5 | 28.4 | 40.4 | 4.9 | 6.4 | 4.9 |
| **as proposed** | 5.2 | 20.7 | 37.5 | 22.1 | 38.1 | 4.8 | 6.9 | 4.5 |
| p 48 | 4.6 | 21.3 | 37.6 | 26.7 | 38.1 | 4.7 | 6.6 | 4.5 |
| window 5 ms | 5.7 | 21.7 | 42.6 | 22.9 | 55.5 | 4.0 | 2.1 | 4.1 |
| window 20 ms | 3.9 | 22.8 | 23.4 | 27.9 | 36.9 | 4.0 | 4.4 | 4.8 |
| local 10 ms | 5.2 | 23.2 | 37.5 | 22.1 | 38.1 | 5.8 | 7.3 | 4.7 |
| local 40 ms | 5.2 | 19.3 | 37.5 | 22.1 | 38.1 | 3.0 | 6.0 | 4.6 |
| 17 pitch taps | 5.2 | 21.0 | 37.5 | 21.9 | 42.3 | 4.8 | 6.9 | 3.6 |
| band 0.6 | 4.5 | 17.8 | 19.7 | 11.9 | 20.4 | 4.2 | 5.1 | 2.4 |
| band 0.9 | 7.9 | 17.8 | 50.4 | 42.3 | 40.6 | 5.7 | 7.2 | 8.6 |

No row lifts the three exceptions over the smooth set's level: their medians stay at 2 to 9 while
the patches alone score 6 to 39.

**D. The floor** (measured once; the engine's smooth set and the weakest of 5 plants at 0.05 /
0.02):

| floor under the analysed peak | smooth max | sine C4 | saw C2 | saw C6 | Crushed bells C4 |
|---|---|---|---|---|---|
| -100 dB | 818 | 752 / 718 | 545 / 221 | 218 / 123 | 18.7 / 8.2 |
| -70 dB | 33.5 | 127 / 51 | 120 / 48 | 97 / 51 | 18.7 / 8.2 |
| **-60 dB** | **5.2** | 40 / 16 | 38 / 15 | 31 / 16 | 18.7 / 8.2 |

At -100 dB the band-limited saw at C1 reads 818: its resets cancel well but not to zero, and
against a residual of almost nothing the remainder reads as a click. The engine's saw
is periodic to the sample (733.87 samples at C2, every interval 733 or 734); what limits the
cancellation was not found. Floors relative to the residual's RMS were tried and needed -40 dB to
quiet that saw, which dropped the saw at C6 below k.

## 5. Events in today's engine (findings, not false positives)

Classes, from the check (k = 10, both channels): **tick** = in a render whose events crowd onto one
run of 6 of the engine's 32 tick phases (at least half of at least 5 events; chance is 6/32);
**period** = another event one note period away (or half or twice that, within 3 %): a
discontinuity that recurs with the tone; **isolated** = neither. Bank chords: tick 10, period 1056,
isolated 468. Held notes: tick 664, period 653, isolated 44.

1. **Glass horde pad steps at the engine's tick** (held C1 to C4: 139, 188, 185 and 149 events,
   all but a handful tick-class; as a chord, 1). A control with no detector agrees: the 89 largest
   second differences of the C4 render all fall on sample 0 or 1 of a tick. This is with
   frameHold, stealFade and spreadRamp all on. One change at a time (measured once, C4): the steps
   go with blade 1's depth at 0 (not with blade 2's), with the spread law off (`law` 0), with
   `kRule` 0 or with one member (`N` 1), and mostly with `lock` 0 or `b2sp` 0; they stay with
   spreadRamp 0 or 1, frameHold 0, `xm` 0, os 2, and blade 2 off. **Hypothesis, not measured:** a
   per-member value that reaches blade 1's k through the drifting spread law, the k rule and the
   lock to kHz is handed over once per tick and is not among the values the spread ramp (D8)
   ramps. Nobody has listened for it. It needs a row of its own.
2. **The plain saw (`base` 2) is computed naively**: its reset lands on a whole sample, so it moves
   by a fraction of a sample from period to period and no predictor follows it. Held, it reads 36
   events at C1 and 40 at C2 (up to 115), none from C3. That is aliasing, by design of that wave;
   the band-limited saw (`base` 6) reads none.
3. **Drift and lock**, as a chord: 12 events, 10 of them tick-class. Weak evidence (few events);
   listed so it is not lost.
4. **The tone's own discontinuities**, by design and recurring with the tone: the crush and crunch
   presets (Crunch (pitch S&H) 368 events, Crunch and fold 362, Crunch pluck lead 350, Zap bass
   70, Crunch (audio-rate PM) 12, Fixed crush 4), Breathing pad's noise blade (held C1 to C3: 117,
   187, 37; ROADMAP B481 found one such step with the old detector), the defaults' hard-sync blade
   at C1 and C2 (176 and 154, almost all period-class: five detuned members, each cut every
   cycle).
5. **Isolated events on feedback patches**: Feedback snarl 226 (164 isolated), Screamer 114 (94
   isolated). Feedback at these settings is irregular by nature; whether any of these is a click
   the ear hears, nobody has judged.
6. **Single isolated events** (score 10 to 14) on Trance jitter (2, the same sample on both
   channels), FM bell (6), Glass horde pad chord (1), Cross-mod shimmer (1): unexplained, so to be
   counted as possible false events until someone listens.

`--events <file>` writes every one of these with its render, channel, sample, tick phase, size,
size over peak, score and class.

## 6. What it can and cannot see

**Can:** a step, a spike or an abrupt corner on a single tone the predictors follow, bright or not
(a saw at C2 and C6, a sine, a ring-and-crush bell at C4), at 0.02 to 0.05 of the peak, at any
level.

**Cannot, and why:**
- **A step no larger than the tone's own steps.** A crush blade, a sample-and-hold noise, a
  control-tick step: these are unpredicted samples, which is exactly what this detector looks
  for. No per-sample test tells a planted step from them.
- **Chords and detuned members that do not lock.** One pitch predictor follows one period. A
  chord of four band-limited saws hides a 0.2 step (0 of 6 caught, synthetic), and its own resets
  score up to 9.5, near k. A cascade of pitch predictors, one per note, was tried and did not help:
  the lag search on a mixture misleads it. A predictor with lags for every note of a chord is the
  likely fix; it is not built.
- **Gentle corners.** A slope change spread over 1 ms is caught 0 of 5 times at 0.05 on most
  patches (4 of 5 on the saw at C6).
- **A naive waveform at low pitch** reads its own resets (finding 2).
- **Onsets in the first 0.12 s** of a render, and anything above 0.75 of Nyquist.
- **A weaker second click exactly one period after a first** is dropped as its echo.

**Using it in the new engine's tests.** On a patch that reads events by itself, an absolute "no
event" cannot be asked; a test can ask that a render reads no event the same render without the
fault does not (a difference of event sets), or judge on a patch that reads none by itself.

## 7. The old detector, on the same plants

The isolated-jump detector of `tools/h2_frame_hold_check.mjs`, unchanged, on the same step plants
(output channel as rendered, and scaled to peak 1, which its absolute 0.02 floor favours):

| render | 0.02 | 0.05 | 0.1 | 0.2 |
|---|---|---|---|---|
| Crushed bells C4 | 0 / 0 | 0 / 0 | 0 / 0 | 7 / 7 |
| Crushed bells chord | 0 / 0 | 0 / 0 | 0 / 0 | 6 / 6 |
| band-limited saw C2, C6 (synthetic, 0.05 only) | | 0 of 6, 0 of 6 | | |

The new detector, on the same Crushed bells C4 plants: 6, 10, 10, 10.

## 8. Open questions for the human

1. **Ratify or change the numbers** (section 3), k first. Table A shows what a higher k costs: at
   20, Crushed bells C4 drops to 8 of 10 and the bank's events fall from 1534 to 270.
2. **The false-event bound is the smooth set, not the bank.** The bank has no ground truth; its
   events are reported by class (section 5) and judged nowhere. Is that the bound you want?
3. **RMS or a quantile for the local level.** The brief said RMS; the ratified method says
   "level". A 99th-percentile level was measured once, on an earlier build of the detector (before the
   causal band and the floor): on the bright patches it moves plants and
   clean renders together (no gain in discrimination), but it brings a chord of saws' own score
   from 10.1 to 2.6 and a detuned stack's from 12.3 to 4.2, so multi-pitch textures would stop
   reading events by themselves. It is about six times slower as measured.
4. **Glass horde pad's tick steps** (finding 1) need a row and a listen.
5. **A predictor for chords** (section 6) would lift the chord exception; worth a row?
6. **B490 acceptance (1)**, the dashboard hole naming the patches the detector cannot see, is not
   in this change; the three exceptions above are the candidates.
