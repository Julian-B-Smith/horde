# horde engine: the model, for agents

What each part does to the sound, in the fewest words that let you predict it. Each claim cites its SPEC-ENGINE row; the spec is the authority. Keys use the folded names of `parameters.md` (`oscN.`, `bladeB.`, `envE.`).

## The signal path

A note gets one voice. A voice holds osc 1, osc 2, the sub, the noise oscillator and ENV 1 to ENV 4 (4.1). Each source is scaled by its level, panned, gated, multiplied by its envelope, then the voices are summed and scaled by the output level (4.2, VO-9, OU-1). Not built yet: filters, FX, the modulation matrix, the morph and glide (`tools/engine-preview/README.md`; the spec reserves them in RC-1, RC-3, RC-4, RC-5 and Appendix D).

- A source sounds only when its switch is on: `oscN.on` (osc 1 on by default, osc 2 off), `sub.on` (off), `noise.on` (off). An off source costs nothing (5.2).
- `voice.mode`: poly gives each note a voice; mono restarts the envelopes on every note; legato restarts only when no key is held (VO-6). `voice.polyphony` caps the voices (VO-5).

## The swarm (each oscillator)

An oscillator is a swarm of members, the stacked copies other synths call unison voices (SW-1). Their sum is scaled by U^−0.75 (SW-19).

| what | keys | how it acts |
|---|---|---|
| members | `oscN.unison` | 1 to 16 copies (SW-1). With 1 member, detune, coupling, spreads and the pan image are inert (PA-8). |
| placement | `oscN.distribution`, `oscN.seed` | Where members sit around the pitch, x from −1 to +1: even, the JP-8000 supersaw curve, or seeded draws (C.2). The seed fixes every random choice (SW-2). |
| detune law | `oscN.detuneLaw`, `oscN.detune`, `oscN.detuneRange` | cents: a member at x sits x·detune·range·100 c away, so beating speeds up with pitch; Hz: x·detune·range·20 Hz, the same beat everywhere; ERB: equal roughness; harmonic: member i on f·(1 + detune·range·reach·i) (C.3, 5.3). In the cents law with even placement, detune 0.1 at range ×1 puts the outermost members at ±10 c (SW-3). |
| anchor | `oscN.anchor` | 0 centres the swarm on the pitch; 1 puts its lowest member on it (C.3). |
| base wave | `oscN.baseWave` | What every member plays before the blades act (5.3). At `bandLimit` 1 its edges are band-limited (ED-1a). |
| band-limit | `oscN.bandLimit` | 1 removes aliasing; 0 leaves it as raw digital grit (ED-1). |

### Coupling

`oscN.coupling`, K from −1 to +1, is a pull between members measured in units of their own spread σ, so one knob position gives the same regime at any detune, register and law (SW-4, C.1).

| K | regime (SW-4's anchors: 7 members, JP-8000, cents, detune 0.28) |
|---|---|
| +1.0 to +0.75 | locked to one pitch (R ≥ 0.93) |
| +0.7 | on the edge: R swings between about 0.26 and 0.87 |
| +0.6 | no sustained lock within about 3 s |
| 0 | no pull |
| −0.7 | R_N about 0.57: partway to splay |
| −1.0 | even splay: R ≤ 0.10, R_N ≥ 0.90; the 7th harmonic's power about 7 times its uncoupled level. 5.3's words: "a multiplied, hollower tone" |

- Positive K also draws the members' natural frequencies together by (1 − K/2) before the pull, except under the harmonic law (C.1).
- `oscN.pivot`: mean field locks onto the swarm's average; root member keeps a locked swarm on the played pitch (SW-6).
- `oscN.phaseLag`: positive lowers a locked swarm's mean frequency, negative raises it; the lock holds to at least 60° and is lost at 90° (SW-5).
- `oscN.onsetLock` and `oscN.dissolve`: a burst of lock (or, negative, splay) at each note's start that fades with time constant `dissolve` (SW-9, C.1).
- The bank's presets with K under about 0.72 do not lock (B.1).

## Drift and motion

| what | keys | how it acts |
|---|---|---|
| drift | `oscN.driftDepth`, `oscN.driftRate` | Each member wanders in pitch, up to `driftDepth` cents, by a random walk whose rate is 0.2 + 8·`driftRate` (SW-7, C.4). |
| inertia | `oscN.inertia` | Members swing into new places with weight and overshoot; the note, bends and glide are never swung (SW-8, C.5). |
| gravity | `voice.gravity` | Held notes within 35 c of a just ratio pull onto it: at 0.7 a fifth 5 c sharp settles to 3/2 in about 3 s (SW-11, C.6). |
| ensemble | `oscN.onsetScatter`, `oscN.timingCorrection` | Members enter up to this many ms apart at a note's start, correcting toward the ensemble's average (SW-12, C.7). |
| start phase | `oscN.retrigger`, `oscN.phase`, `oscN.phaseRandom` | On: members restart at Phase, randomised by Random (0 together, 100 anywhere); off: they resume where the oscillator left off (SW-10). |
| stereo | `oscN.width`, `oscN.panImage`, `oscN.pan`, `oscN.panMotion` | Members are seated across the field by an equal-power law; width 0 is mono; the fans order seats by pitch or placement; pan motion wanders each seat slowly (SW-18, C.8). |
| motion grain | `oscN.motionGrain` | 1: spread values reach the blades in control-rate steps (a grain); 0: smooth ramps (SW-16). |

## Blades

A blade is a window over each cycle of a member's wave; inside it the wave is replaced by the blade's sound (BL-1). Each oscillator has blade 1 and blade 2 (BL-9). A blade does nothing until `oscN.bladeB.on` is on (PA-8).

| what | keys | how it acts |
|---|---|---|
| window | `oscN.bladeB.position`, `oscN.bladeB.width` | Centre and length in cycles; a width below 0.004 is off (BL-1, C.9.1). |
| blend | `oscN.bladeB.depth` | 0 the base wave, 1 the blade's sound inside the window (C.9). |
| edges | `oscN.bladeB.edges` | Softens the window's edges; in Crush, the glide between steps (5.4, C.9.4). |
| cut rate | `oscN.bladeB.cutIn`, `oscN.bladeB.cut`, `oscN.bladeB.cutHz` | Per cycle: the carrier runs at `cut` × the member's frequency; per blade: `cut` cycles across the window; Hz: a fixed frequency, a formant that does not follow the note. Capped at 0.45 of the internal rate (BL-4). |
| carrier wave | `oscN.bladeB.wave`, `oscN.bladeB.shape` | The carrier's wave in Sync, FM and Ring; sine→saw morphs by `shape` (BL-3, PA-8). |

**Modes** (`oscN.bladeB.mode`; BL-2, C.9.2):

| mode | inside the window |
|---|---|
| Sync | the carrier, restarted at each window entry: hard sync |
| FM reset | the carrier, phase- or pitch-modulated; the modulator restarts at each entry |
| FM free | the same, with a free-running modulator |
| Noise | held random values, a new one each carrier cycle |
| Fold | the input folded by sin(π/2·(1 + (k − 1)/4)·x): a higher cut rate k folds harder |
| Ring | the input times the carrier |
| Crush | held steps, each the average of the wave over its span (C.9.4) |

- **FM** (BL-5): `fmType` phase or pitch; `fmDepth` is the index, how hard the modulator pushes; `modShape` its wave; `modIn`, `modRate`, `modRateHz` its frequency, as a multiple of the member's pitch or fixed. Blade 2 uses blade 1's FM settings unless `oscN.blade2.fmSource` is its own (5.4).
- **Mirror** (BL-6, C.9.5): reflect makes the window a palindrome; twin adds a second window half a cycle on. On a sine, triangle or square base, twin − removes the even harmonics: a hollow tone.
- **Frame** (BL-10, C.13): swarm frame places every window relative to the swarm's mean phase, so the windows move together as the swarm locks and drifts.
- **Rotation** (BL-8): `rotate` turns the window around the cycle in Hz; `rotation` chooses restart per note or free-running, and whether blade 2 turns with blade 1.
- **Spreads** (BL-13, C.10): `positionSpread`, `cutSpread`, `widthSpread`, `depthSpread`, `shapeSpread`, `fmIndexSpread`, `rotateSpread` fan a setting across the members, by `oscN.spreadLaw`: gradient, random (redrawn each note), drift (wandering at `oscN.spreadDrift` Hz), alternate, or swarm (each member's lead on the swarm's phase). A negative spread mirrors the positive (BL-14). Blade 2 uses blade 1's spreads unless `oscN.blade2.spreadSource` is its own.
- **Cut rules** (BL-15, C.10): `cutRule` gives each member's cut rate a ratio: harmonic, undertone, octaves, major, minor, fifths, golden, primes or custom; `ruleDepth` scales it. `oscN.quantize` whole snaps cut spreads and rules to whole harmonics, changing only at a carrier restart (BL-16).
- **Two blades together** (BL-11, C.11): `upperHears` 0 sums both blades on the base wave; 1 makes the upper blade act on the lower one's result. `stacking` picks the upper blade. Where windows overlap, `collisionPitch` bends the upper carrier up to ±2 octaves and `collisionBite` raises its FM depth up to ×5 and fold drive up to ×3.
- **Cross-mod and feedback** (BL-12, C.14): each member's previous output bends the next member's (`crossMod`) or its own (`feedback`) carrier phase, in Sync, FM and Ring. `oscN.loopDrive` sets the loop's gain: stable below about 0.50 with a saw carrier, 0.38 triangle, 0.42 sine; above, chaotic noise, each setting its own.
- No blade leaves a DC offset (BL-19).

## The sub

One phase, one shape: no members, no coupling, no drift (SN-1).

- `sub.wave`: sine, triangle, square, saw, pulse (`sub.pulseWidth`), bump (the fundamental plus two chosen harmonics, `sub.bumpHarmonic` … `sub.bump2Phase`), smooth square (`sub.softness`) (SN-2). Every shape peaks at 1.
- Pitch: the note × 2^(octave + semitones/12 + pitch/12), down only (`sub.octave` −3 to 0, default −1); `sub.keytrack` off pins it to C2 (SN-1).
- `sub.drive` saturates by `sub.character` (Tube, Diode, Fold-lite); `sub.crunch` adds a parallel band of upper-harmonic grit without darkening; `sub.tone` is a gentle low-pass (SN-3, SN-5).
- `sub.order`: by default the drive follows the envelope, so a decaying or softer note drives less; the other order keeps the drive's colour all note long (SN-3).
- `sub.voiceMode` mono: one sub for the instrument, on the lowest, highest or newest held note (`sub.priority`) (SN-7).
- `sub.erode`: the noise oscillator's signal modulates the sub's timing, a noise-modulated delay. The noise's level, mute, pan and envelope do not reach it, and the noise runs for it even while `noise.on` is off (SN-14, SN-4).

## The noise oscillator

Per voice, seeded per note (SN-4). Chain: source → Resonator → Whisper → Wind → filter → spread and pan (SN-4).

- `noise.type`: Clean (white), Crackle (pops, `noise.density` per second), S&H (`noise.rate` Hz), LFSR (a pitched register: `noise.length` 63 or fewer repeats at the note's pitch, longer becomes hiss) (SN-8).
- `noise.colour`: +1 white, 0 pink, −1 brown (SN-4).
- `noise.resonator`: a comb tuned to the note, ringing by `noise.feedback`; `noise.whisper`: three vowel formants swept by `noise.vowel` (a, e, i, o, u); `noise.wind`: a band wandering by `noise.gust` (SN-9). Each stage keeps its input's level.
- `noise.filter` Off, LP, BP, HP, with `noise.cutoff`, `noise.resonance`, `noise.keytrack` (SN-10). `noise.pitch` moves only the LFSR's clock, the Resonator's tuning and the filter's keytracking (SN-11).
- `noise.spread` 0 is mono; above 0 the channels part (SN-12).

## The envelopes

- ENV 1 to ENV 4 are attack, hold, decay, sustain, release; each stage takes exactly its time (EN-1). Curves: 0 straight, positive fast-start, negative slow-start (C.12). The bank's decay and release use curve +2.303, its exponential (5.2, B.1).
- Each source's level follows one envelope, chosen by its `env` (`oscN.env`, `sub.env`, `noise.env`): ENV 1 by default (RC-6.5). An envelope no on source follows is inert (PA-8).
- Velocity scales an envelope's level by 1 − v + v·velocity, v its `velocity` row: at 1 the level is proportional to velocity (VO-8).
- `envE.oneShot` on: a key-up does not cut the attack or decay short (C.12).
- **ENV 2 and ENV 3 and the blades.** In the bank, ENV 2 and ENV 3 were blade envelopes: routes to a blade's cut rate and width (B.1). Routes belong to the modulation system, which is not built (RC-4, 5.7). So today ENV 2 to ENV 4 reach the sound only as a source's level envelope through its `env`, and a patch cannot route them to a blade.

## The mixer and the output

- `mix.osc1`, `mix.osc2`, `mix.sub`, `mix.noise`: each source's level in dB; the bottom is a true off (5.2, SN-4).
- `oscN.mute`, `oscN.solo` and the sub's and noise's: while any source is soloed, only soloed sources sound (IF-6).
- `out.level`: the output is 0.14 × the sum of the voices × 10^(`out.level`/20), with no limiter and no clamp; a sample over 0 dBFS is caught and reported, not prevented (OU-1).
- `quality` High oversamples the oscillators 2×: less aliasing on FM, ring and fold, at 1.5 to 1.7 times the CPU (5.2).
