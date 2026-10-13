# The edge-correction lab's table design, read against the resampler literature

**Date** 2026-10-13 · **Roadmap** B466 (serves B383, B380, B378 F3) · **Read at** `origin/main` `293cc39`
· **Author** a research agent dispatched by the horde lead on B466. No code and no lab change.

**To read in ten minutes:** sections 1, 4 and 5. The rest is reference.

**Whose numbers.** "The lab reports" is a number from ROADMAP B383 or
`docs/design/edge-correction-lab.html`. "Measured here" is a scratch program, kept outside the
repo, that re-implements a few of the lab's formulas (lines named in the last section). Neither the
lab page nor the engine was run; statements about the engine are read from `h2/engine/engine.h`.

**Words.** An *edge* is a jump in the wave's level (a *step*); a *corner* is a jump in its slope
only. Sampling either as it stands folds its high frequencies back into the audible band:
*aliasing*. A *kernel* is the short smooth shape pasted over an edge to remove what would fold,
counted in *taps* (output samples touched). A *BLEP* is a band-limited step; its *residual* is the
part actually added. *Nyquist* is half the sample rate; the *cutoff* is where the kernel stops
passing, as a fraction of Nyquist. A *window* is the taper that makes an ideal kernel finite; it
sets the depth of the *stop band*. A *linear-phase* kernel is symmetric round the edge and so needs
look-ahead (latency); a *minimum-phase* one passes the same frequencies entirely after the edge.
*DC* is a constant offset.

## 1. The answer

The lab's table is sound as an anti-aliasing device. Measured here by a second, independent
method, options (c) and (d) put aliasing 112 to 139 dB under the wanted harmonics from C2 to C8,
where the present 2-point correction reads 44 to 26 dB under. Its 32 taps are the literature's
usual size, and its way of reading between stored points is better than the literature asks for.
Four things should change before a table goes into the engine. (1) The cutoff sits lower than it
needs to: the same 32 taps can keep the top of the audible band within 1 to 3 dB where the lab's
table loses 6.6 dB. (2) The minimum-phase table, the one the lab's recommendation favours, adds a
DC offset that grows with pitch (as large as the sound itself at C8). Because the lab matches
loudness on a figure that includes DC, it plays options (d) and (f) quieter than the others at high
notes: about 2 dB at C6 and 10 dB at C8 on its default sound. The ear test above C6 is not
level-matched until that is fixed. (3) Option (e) mixes two kernels that arrive at different times,
which cuts a notch of up to 18 dB near 6 kHz inside its blend band. (4) The table is stored eight
times finer than its read rule needs. And a table does not reach three things: corners, soft blade
edges (the engine has both, and corrects neither), and the output `tanh`, which re-creates aliasing
far above the table's floor when driven.

## 2. The lab's design, stated

From `docs/design/edge-correction-lab.html`: options at lines 304-317, defaults at 321 (`W` 16,
`cut` 0.80), the table build at 349-386, the read at 413-423, option (f) at 428-444, latency at 520.

| option | taps per edge | stored per sample | window | cutoff | read between points | corrected: before + after | phase | latency |
|---|---|---|---|---|---|---|---|---|
| (a) 2-point polyBLEP | 2 | nothing (a formula) | none | none | exact | 1 + 1 | symmetric | 0 in the lab; 1 sample in the engine (`engine.h:1636`) |
| (b) (a) at 2×, 63-tap halfband | 2 at 2× | the halfband | Blackman | 0.235 of the 2× rate | exact | 1 + 1 at 2× | linear | 15.5 samples |
| (c) linear-phase table | 32 | 128 points, value and slope | 4-term Blackman-Harris, ±16 samples | 0.80 (17.64 kHz) | cubic Hermite on the stored slope | 16 + 16 | linear | 16 samples |
| (d) minimum-phase table | 32 | as (c) | as (c), turned minimum-phase by the real cepstrum (8× zero padding, floor 200 dB down), cut at 32 samples | as (c) | as (c) | 0 + 32 | minimum | 0 |
| (e) blend of (a) and (d) | 2 to 34 | as (d) | as (d) | as (d) | as (d) | 1 + 32 | mixed | 0 |
| (f) eight minimum-phase tables | 64 at C1 to 20 at C8 | 128 points each | as (d) | 0.92 at C1 to 0.72 at C8 | as (d) | 0 + 2·W | minimum | 0 |

(e) weights its two kernels with a raised cosine over 12 semitones centred on C3. (f) has one table
per octave (`W` 32, 27, 23, 19, 16, 14, 12, 10), uses the nearer, and crossfades two over
±2 semitones round each mid-octave point; pitch scaling changes the tap count and the cutoff and
nothing else. "W 32 with cutoff 0.90" (64 taps) is the B383 row's suggestion, not a lab default.

## 3. What the literature recommends

**Length.** Brandt's minBLEP kernel is 32 samples. De Soras uses 12 taps only because his
interpolator runs at twice the output rate, so its roll-off sits in a band that is thrown away; he
notes that doubling the taps only halves the roll-off width. Smith's example is 13 zero crossings
a side for about 80 dB.

**Storage and reading between points.** Smith's estimates for a table of L points per zero
crossing: an error of about 0.79/L with no interpolation, under 0.412/L² with linear, hence
L = 2^(bits/2) (256 points for 16 bits). De Soras shows the same as spectra: uninterpolated, each
doubling of the phase count buys 6 dB (128 phases, 51 dB); linear interpolation at 128 phases
reaches 100 dB. He stores each phase's taps side by side with the difference to the next phase, so
one edge reads one contiguous run of memory. Brandt uses 64 points per sample, linear. None uses
the lab's cubic read on a stored slope.

**Window.** De Soras: "prefer parametric windows if possible, like Dolph-Chebyshev or Kaiser".
Smith uses Kaiser for the same reason: one number trades stop-band depth against roll-off width.
The lab's Blackman-Harris window has no such dial: it always buys about 110 dB and pays with the
widest roll-off.

**Cutoff against roll-off.** Both resampler sources place the roll-off so that what folds lands
where nobody hears it. At 44.1 kHz the stop band need only begin at 24.1 kHz, since everything
below that folds to above 20 kHz (de Soras gives the 4.1 kHz figure). The lab's table does not use
that room: measured here it is 65 dB down at Nyquist and 132 dB down at 24.1 kHz. So the loss the
lab reports at C6 (5.1 to 8.5 dB kept in 16 to 20 kHz) is the expected price of that placement, not
of 32 taps.

**Linear against minimum phase.** Brandt chooses minimum phase to avoid look-ahead: no latency, no
guessing where an edge will fall while pitch moves. A linear-phase kernel rings for half its length
before each edge, at its cutoff (here 17.6 kHz, for 0.36 ms); no source read gives a listening
threshold for that, so it stays the lab's question. De Soras insists on linear phase wherever two
filtered versions are crossfaded, because a phase difference shows during the fade (see F7). No
source read discusses what minimum phase costs a wave that is not only steps (see F5).

**Pitch-scaled kernels.** De Soras's "MIP-map" (one pre-filtered copy of a recording per octave)
exists because a recording's content moves with playback pitch. What an edge kernel must remove is
always what lies above Nyquist, whatever the note, so scaling it with pitch buys CPU, not
correctness. The abstract of Lehtonen, Pekonen and Välimäki (paper not obtained) says the
attenuation a sawtooth's aliases need grows with pitch, to about 40 dB near 4 kHz: by audibility it
is the low notes that could take a short table. (f) gives them its longest, for a flat top band
where edges are few, and is laid out for flat CPU.

## 4. Findings

KEEP: the literature supports it. CHANGE: do it differently before the engine. MEASURE: a named
measurement decides.

**F1. KEEP: the table removes the aliasing.** Measured here on the lab's single saw, alias power
over harmonic power in dB, every bin counted:

| | C2 | C4 | C6 | C8 |
|---|---|---|---|---|
| (a) 2-point polyBLEP | −43.5 | −37.5 | −31.8 | −25.8 |
| (c) and (d), W 16, cutoff 0.80 | −114.5 | −112.3 | −134.6 | −139.3 |
| (c) and (d), W 32, cutoff 0.90 | −116.9 | −120.5 | −139.4 | −137.2 |

The lab reports (a) at −67 to −26 and the tables at −120. The methods agree at C8 and differ lower
for a stated reason: the lab's estimator counts a bin only when it stands 10 dB over its
references, counts nothing outside 20 Hz to 20 kHz, and floors at −120
(`tools/patchspace/metrics.mjs:353-355`). Its "−120" means "nothing counted".

**F2. KEEP: 32 taps, and the minimum-phase derivation.** Measured here, the minimum-phase table has
the linear one's magnitude to 0.0000 dB wherever that is above −60 dB, and the same stop band
(−111.2 against −111.3 dB).

**F3. CHANGE: raise the cutoff; consider a window with a dial.** Measured here on the linear-phase
build (the minimum-phase one has the same magnitude). Aliases are over harmonics, worst of C2 to C8.

| table (32 taps unless marked) | −1 dB at | at 18 kHz | at 20 kHz | kept 16–20 kHz at C6 | worst alias under 20 kHz | worst alias anywhere |
|---|---|---|---|---|---|---|
| the lab's: Blackman-Harris, 0.80 | 15.8 kHz | −7.8 | −24.9 | −6.6 dB | −134 | −112 |
| Blackman-Harris, 0.865 | 17.2 kHz | −2.4 | −11.3 | −2.8 dB | −128 | −75 |
| Kaiser set for 80 dB, 0.92 | 18.8 kHz | −0.2 | −4.5 | −0.9 dB | −104 | −64 |
| 64 taps, Blackman-Harris, 0.90 (B383's suggestion) | 18.9 kHz | 0.0 | −7.5 | −1.0 dB | −137 | −117 |
| 64 taps, Blackman-Harris, 0.98 | 20.7 kHz | 0.0 | −0.1 | 0.0 dB | −137 | −53 |

The "kept" figure for the lab's own table reproduces the lab's (−6.55 here, −6.6 reported), which
checks the transcription. The last column is the price of a higher cutoff: aliases parked between
20 and 22 kHz, which the lab's estimator cannot see. A 32-tap table at 0.865 keeps 3.8 dB more of
the top band than the lab's with nothing added below 20 kHz; a 32-tap Kaiser table equals what the
B383 row hoped to buy with 64 taps. **To measure:** whether parked aliases matter once something
nonlinear follows. On a model (one saw into the output `tanh`) the choice of table moved the result
by under 2 dB; in the engine, read B346's estimator with its band widened to Nyquist, per table, on
one bright patch at os 1.

**F4. CHANGE: store the table eight times coarser, and by phase.** Measured here: the largest
error, in dB re a unit step, of reading the lab's step table between its points.

| points per sample | 8 | 16 | 32 | 64 | 128 (the lab) | 512 |
|---|---|---|---|---|---|---|
| size, value and slope | 4 KB | 8 KB | 16 KB | 32 KB | 64 KB | 256 KB |
| cubic on the stored slope (the lab's) | −97 | −121 | −146 | −170 | −194 | −242 |
| linear | −55 | −67 | −79 | −92 | −104 | −128 |
| nearest | −26 | −32 | −38 | −44 | −50 | −62 |

Linear and nearest follow Smith's estimates (12 and 6 dB per doubling). On the saw at C6 the cubic
read gives the same alias figure at 16 points as at 128 (−134.4 against −134.6 dB). At 128, option
(f)'s eight tables total about 610 KB, more than any current first-level cache; at 16, about 77 KB.
De Soras's layout (each phase's taps contiguous) gives bit-identical output, measured here. It was
no faster on the one machine used (about 61 against 62 ns per edge), whose cache holds the lab's
single table; it matters on smaller caches and for (f).

**F5. CHANGE: a minimum-phase table shifts the wave's level, more as pitch rises.** The residual of
(d) is not balanced round zero: for a unit step its area is 3.31 samples (measured here; it is the
kernel's delay at low frequency). The steps arrive 3.3 samples late (7 at 15 kHz) and the ramp
between them does not, so a saw gains a DC offset of 2 × amplitude × 3.31 × frequency ÷ sample
rate: predicted 0.2263 at C8 on the lab's saw, measured 0.2264.

| the lab's single saw (amplitude 0.36), option (d) | C4 | C6 | C7 | C8 |
|---|---|---|---|---|
| DC offset | 0.014 | 0.057 | 0.113 | 0.226 |
| DC against the level of the audible part | −23 dB | −11 dB | −5 dB | +1.5 dB |

(a) and (c) read zero (under 1e-14). Read from the code, not measured: in the engine this offset
would ride each member's envelope into the output `tanh`; the per-member DC estimate (`dcEst`,
`engine.h:1367`) integrates the uncorrected wave and would not see it, and the output blocker runs
only in some modes (`engine.h:1957`). A linear-phase table has no such offset and costs 16 samples
of latency. A minimum-phase one needs a law of its own (the known offset subtracted per member, or
the smooth part delayed to match), and that law is a design to rule on with the table. **To measure
in the engine:** a bright patch, a held C7, (d) against (c): the output's mean over 1 s before the
`tanh`; above −60 dB re the peak, the law is needed.

**F6. CHANGE, in the lab, before the ear test is trusted above C6: the level match counts DC.** The
lab trims each render to −20 dBFS RMS, DC included (`edge-correction-lab.html:773-775`, `:1079`).
Measured here on a transcription of its default source (seven members, ±30 cents, seed 1), the
audible part of each option after that trim, in dBFS:

| | C4 | C5 | C6 | C7 | C8 |
|---|---|---|---|---|---|
| (a), (c) | −20.0 | −20.0 | −20.0 | −20.0 | −20.0 |
| (d); (f) within 0.2 dB | −20.1 | −20.5 | −21.9 | −25.1 | −30.2 |

The swarm is worse than one saw (−3.8 dB at C8) because the members' offsets add in step and their
sound does not. In a blind A/B the minimum-phase options, and (e) above its crossover, are
therefore played quieter, and a quieter option tends to lose. This is a property of the lab's
level match, not of the table's sound.

**F7. CHANGE: option (e) has a notch.** Its two kernels arrive 3.3 samples apart (F5), so their mix
cancels where that is half a period. Measured here at equal weights, against the mean of the two
responses: −0.2 dB at 1 kHz, −1.0 at 2, −4.8 at 4, −18.3 at 6, −7.9 at 8, −1.6 at 10 kHz. The
control (the 2-point kernel mixed with the linear-phase table, which arrive together) reads
0.00 dB throughout. The lab's continuity check looks for a step in level along a glide and cannot
see a smooth notch. This is de Soras's warning. As built, (e) colours the notes inside its 12
semitones, most at the middle.

**F8. KEEP, with a note: option (f)'s crossfades.** Its eight tables arrive within about 0.2
samples of each other (3.21 to 3.43), so neighbours mix cleanly: the worst are −0.41 dB (C7 with
C8) and −0.21 dB (C1 with C2), both at 16 kHz. The note is roll-off: the C8 table is −6.4 dB at
16 kHz and −14.4 at 18 kHz. (f) carries F5 and F6 in full.

**F9. MEASURE: the output `tanh` sets a floor far above the table's.** A model, not the engine: one
table-clean saw through `tanh(0.56·y)`, the output law at the default gain (`engine.h:1961`).
Aliasing under 20 kHz over harmonics: −48 dB at C4, −42 at C6, −37 at C8 for a saw peaking at 1,
and 12 dB lower per halving of level (−65 at C6 for a peak of 0.25). Without the `tanh` the same
renders read −138 to −144. B346 recommended no change for the `tanh` (its R6), a fair reading
against edges at −40 dB. **To measure in the engine:** a loud unison patch at os 1, B346's
estimator, the table on, the `tanh` on and off. If the `tanh` holds the reading above about −60 dB,
the table's last 60 dB cannot be heard through it, and the ruling should weigh that.

## 5. Corners

**The lab does not correct corners.** Its sources are a saw and a hard-synced saw, whose pieces all
have one slope; its edge model carries a jump in level only (`edge-correction-lab.html:285-290`).

**The engine has corners and corrects none** (read from `h2/engine/engine.h`). The correction,
`addE` (`:1482`), takes one number: the level jump that `hAt` measures by evaluating the blade just
either side of the edge (`:1344-1365`). Slope jumps with little or no level jump come from the
triangle as a base or carrier wave (`:492`), a mirrored blade's centre, where its inner phase turns
round (`:584`), crush with slew, whose ramps start and stop (`:526-528`, `:606-612`), and every
hard blade boundary, where the level jump is corrected and the slope change beside it is not.

**A soft blade edge is neither a step nor a corner.** With `hard` above zero the blade fades in on
a raised cosine that starts flat (`:627-634`; the same law at `blade.h:161-167`). Level and slope
stay continuous and the curvature jumps: one order gentler than a corner, provided the fade is
several samples wide. When it is not, see section 6.

**The literature.** Brandt names the remedy in a line: band-limit the next derivative too, and
"aliasing will fall off by 6dB/oct per derivative whose discontinuity is made bandlimited".
Esqueda, Välimäki and Bilbao work it out. The band-limited ramp (BLAMP) is the integral of the
band-limited step; its residual is added at each corner, scaled by the jump in slope; the integral
of that again serves a jump in curvature. Two-point and four-point polynomial forms exist beside
the table form.

**How much it matters.** Measured here on a triangle (amplitude 0.36), alias over harmonic power:

| | C4 | C5 | C6 | C7 | C8 |
|---|---|---|---|---|---|
| corner uncorrected | −65.6 | −56.5 | −48.1 | −37.9 | −31.4 |
| 2-point polynomial BLAMP | −77.8 | −68.8 | −61.6 | −49.4 | −48.7 |
| table BLAMP: the integral of the lab's (c) residual, same grid and read | −149.2 | −146.7 | −167.3 | −149.5 | −159.7 |

With steps at −120, an uncorrected corner is the next floor from about C5 up. At C7 to C8 it
stands about where the 2-point step correction stands today at C4 to C6.

**What a corner table would need.** One more stored value per point (the integrated residual; its
slope is the step residual already held, so storage grows by half). The slope jump at each edge:
`hAt` makes two blade evaluations per edge for the level jump (`docs/port/cpu-ledger.md`
section 3), and a slope jump needs two more. And the same choice of phase, with F5's consequences.
**To measure in the engine:** B346's estimator on held C5, C6 and C7, one patch with a triangle
carrier and one with a mirrored blade, steps on the table. A reading above −60 dB with steps clean
is the corners, and the 2-point polynomial form is the cheap first step.

## 6. What a table does not fix

**Soft edges narrower than a sample or two.** Read from the code: `hAt` probes 1e-7 of a cycle
either side of the boundary, where a soft edge has not yet moved, so it reads no step and nothing
is corrected however fast the fade. Measured here on a model of that law (a gate of width 0.2 with
raised-cosine fades; not the engine), alias over harmonics at C6: hard 0 with the table −133.6 dB;
hard 0 uncorrected −15.0; hard 0.01 (a 0.04-sample fade) −15.2; hard 0.1 (0.4 samples) −17.4;
hard 0.3 (1.3 samples) −24.8; hard 1 (4.2 samples) −48.6. A table would make the hardest edge the
cleanest and leave the slightly softened one as it is today: over 100 dB apart as `hard` leaves
zero. **To measure in the engine:** a sync-blade patch at C6, os 1, B346's estimator at `hard` 0,
0.01, 0.1 and 0.3.

**Edges closer together than the kernel.** Nothing breaks: residuals add, and the sum is exact
because each is the same linear correction of its own step. The price is work and memory. At C8 a
member has an edge every 10.5 samples, so three 32-tap residuals overlap at all times. The engine
holds one residual sample per member today (`blepHeld`, `engine.h:1484`, `:1637`). A table needs
32, per member and not per voice, because the member's loop filter, cross-modulation taps, DC
subtraction and pan gains all follow the correction (`:1537`, `:1876`, `:1922`, `:1940-1942`):
8 voices × 9 members × 32 values, cleared or played out when a voice is reused (B378 F6 found
that kind of leak in the older core).

**Feedback and cross-modulation** read a member's output one and two samples back. A linear-phase
table delays the corrected output by 16 samples, so a loop either waits or reads the uncorrected
wave. B346 tied a limit cycle to the loop reading the corrected output one sample late, and
ADR-189 A1 records that moving the tap moved the cycle without curing it. A minimum-phase table
keeps the loop causal and changes what it sees for 32 samples after each edge. Either changes the
sound of those patches.

**Frequency modulation and sync.** A table is as exact as the edge's time and height. `scan` places
a carrier's edge on a straight line between the step's two phase ends (`:1478`), and under
audio-rate modulation the phase is not straight within a sample. Behind a 2-point kernel at −40 dB
that error is invisible; at −120 it is the floor. `scan` also stops after six edges in a step
(`:1475`). ADR-189 D1 (the carrier averaged over the step) remains the tool there. **To measure:**
an FM blade patch at C5, the table on, the estimator's reading against modulation index.

**Parameter steps (B460, B461).** Spread values and the frame angle reach the blade as steps every
32 samples: jumps in a control, on the sample grid. A table would band-limit the click and leave
the click; the remedy is the ramp those rows propose.

**The edge search.** A table replaces the two lines of `addE`. `tryE`, `scan` and `hAt` stay, and
they are the 11 to 15 % of the render the ledger reports.

## 7. Cost

**Literature.** Cost is edges per second times kernel length (Brandt, who calls anything better an
open problem): two multiply-adds per tap with a stored difference (de Soras), four with the lab's
cubic read. **The lab and the audit.** (d) against (a) on a bare saw in JavaScript: ×1.07 at C1,
×1.32 at C4, ×4.7 at C8; the B378 audit's C++ estimate is +8 % at C4 and +130 % at C8.

**Measured here** (C++, `-O2`, contraction off, an Apple M3 under other load, so ratios are firmer
than figures): one 32-tap cubic read and add costs 54 to 64 ns as the lab writes the loop; the
2-point correction is below what the timer resolves.

**Small where the engine is heavy.** The ledger counts 0.0055 edges per member step on both heavy
presets, and its figures give roughly 60 to 100 ns per member step (8.8 % of a core for 864,000
member steps a second, on a run it says read about 1.6 times high). A 60 ns read per edge is then
0.3 ns per member step, well under 1 %. The table costs most on bright, plain, high unisons, where
edges are dense and the member is cheap: the case the lab timed.

**Vectorising.** No tap depends on another, so two-lane double arithmetic keeps each tap's
operations and their order, and the output stays bit-for-bit. Measured here, with the loop written
so a compiler can vectorise it (two loops in place of the per-tap select, no aliasing between table
and output): 35 to 41 ns against 54 to 62, bit-identical. Fused multiply-add must stay off, because
JavaScript has none. Vectorising buys nothing on the search and the probes, which are branches and
whole blade evaluations, and that is where the edge time goes.

## 8. Sources

- Laurent de Soras, [The Quest For The Perfect Resampler](https://ldesoras.fr/doc/articles/resampler-en.pdf)
  (2003, revised 2005). Read in full. Roll-off placed in unused band; phase count and coefficient
  interpolation, with spectra; storage by phase; parametric windows; linear phase under crossfades;
  what MIP-mapping is for.
- Julius O. Smith, [Digital Audio Resampling Home Page](https://ccrma.stanford.edu/~jos/resample/):
  [Implementation](https://ccrma.stanford.edu/~jos/resample/Implementation.html),
  [Application to the Ideal Lowpass Filter](https://ccrma.stanford.edu/~jos/resample/Application_Ideal_Lowpass_Filter.html),
  [Error in the Absence of Interpolation](https://ccrma.stanford.edu/~jos/resample/Error_Absence_Interpolation.html),
  [Relation of Interpolation Error to Quantization Error](https://ccrma.stanford.edu/~jos/resample/Relation_Interpolation_Error_Quantization.html).
  Read through a fetch tool's summary of each page, not the pages' own text. The error estimates
  for nearest and linear reads, the table-size rule, the Kaiser window's role.
- Eli Brandt, [Hard Sync Without Aliasing](http://www.cs.cmu.edu/~eli/papers/icmc01-hardsync.pdf)
  (ICMC 2001). Read in full. Table size, the cepstral route to minimum phase and the reason for
  it, cost per edge, the remark on higher derivatives.
- Fabián Esqueda, Vesa Välimäki, Stefan Bilbao,
  [Antialiased Soft Clipping using an Integrated Bandlimited Ramp](https://www.ness.music.ed.ac.uk/wp-content/uploads/2016/12/esqueda_eusipco.pdf)
  (EUSIPCO 2016). Read in full. Step, ramp and integrated ramp as successive integrals; scaling by
  the jump; table and polynomial forms.
- The same authors, [Rounding Corners with BLAMP](https://www.dafx.de/paper-archive/details/xRM0GiuF-JmeN0_aaP9VNg)
  (DAFx-16). **Abstract only**; the paper's file could not be fetched.
- Vesa Välimäki, Antti Huovilainen,
  [Antialiasing Oscillators in Subtractive Synthesis](https://research.aalto.fi/en/publications/antialiasing-oscillators-in-subtractive-synthesis/)
  (IEEE Signal Processing Magazine, 2007). **Not obtained** (publisher's wall). Nothing here rests
  on it.
- H.-M. Lehtonen, J. Pekonen, V. Välimäki,
  [Audibility of aliasing distortion in sawtooth signals](https://pubmed.ncbi.nlm.nih.gov/23039464/)
  (JASA, 2012). **Abstract only, through a search result.** That the attenuation needed grows with
  pitch.

## Method, and what was not checked

**The scratch programs** (Node and C++, outside the repo) transcribe these lines of
`docs/design/edge-correction-lab.html`: the FFT 324-339, the window 350, `linImpulse` 351-355,
`minPhase` 358-372, `stepTable` 376-386, `addPoly` 408-412, `addTable` 413-423, `bankF` 428-432,
the swarm's members 465-470, the saw 500-507, the listening envelope 561-564. Points per sample
and the window are parameters there; the rest is as the lab has it.

**The alias measure is not the lab's.** The note is set to an odd number of cycles in 65,536
samples, so every harmonic falls on its own bin of one un-windowed transform and every alias on
another; alias power is the sum of the other bins. Its control: on the same code the naive saw
reads −9 to −27 dB and the tables −112 to −139.

**Not checked.** The lab page and the engine were not run: F5's engine reading, F9 and section 6
are models or code reading, each with its engine measurement named. Nothing was listened to.
Oversampling 2 was not studied (where the table's cutoff belongs at a 2× internal rate is left
open). Timings are one machine, under load. Three sources, marked above, were not read in full.
