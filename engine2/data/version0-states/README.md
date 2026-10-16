# Version-0 example states

A set of version-0 example states (the key-value format of B.2: the bank's keys plus `gain`,
`master`, `os`, `poly` and the rest of B.2's table, no `version` member) is part of the input data
for the migration tests (IF-4; acceptance items 13 and 21). **Supplied with the reference renders by
the lead (ROADMAP B497 part 2); the test author receives them and the builder does not.**

Each file is one JSON object of keys to numbers (`kCustom` to text), the bank's keys of one bank
preset followed by the keys B.2 adds. They are examples for the migration tests to translate, not
expected outputs: no file says what it must become. The table gives what B.2's rows imply, so a test
can be written against the spec and checked against this list.

The level column is B.2's formula, 20·log₁₀(gain / 0.0875) + 5·log₁₀(N) + 20·log₁₀(master) dB,
before `out.level`'s ceiling of +26 dB. A state with no `gain` has no stored level: it takes B.1's,
0 dB + 5·log₁₀(N).

| file | bank keys of | B.2 keys added | what it exercises | level (dB) |
|---|---|---|---|---|
| `01-plain-bank-preset.json` | Locked horde (N 7) | none | a bank preset on its own; `os` absent, so High; no `gain`, so B.1's level | 5·log₁₀(7) = 4.23 |
| `02-gain-0-35.json` | Locked horde (N 7) | `gain` 0.35, `master` 1, `os` 2, `poly` 6, `enable` 1, `b1on` 1, `octave` 0, `semi` 0, `fine` 0 | a stored 0.35 (+12.04 dB plus the density offset); `os` 2 is High | 16.27 |
| `03-gain-at-default.json` | Quarter sync (N 1) | `gain` 0.0875, `os` 1 | a gain at B.2's reference value lands at 0 dB; `os` 1 is Standard | 0.00 |
| `04-level-under-ceiling.json` | Glass horde pad (N 9) | `gain` 1 | the largest density the bank has, at the largest gain B.2 names: just under the ceiling | 25.93 |
| `05-level-at-ceiling.json` | Glass horde pad (N 9) | `gain` 1, `master` 1.5 | the same with `master` above 1: 29.45 dB before the ceiling, so the level is held at +26 dB | 29.45, held at +26 |
| `06-master-os-poly.json` | Cross-mod shimmer (N 8) | `gain` 0.35, `master` 0.5, `os` 1, `poly` 16 | `master` folded into the level (−6.02 dB); `os` 1; `poly` as is | 10.54 |
| `07-switches-and-pitch.json` | Chord of formants (N 5) | `enable` 1, `b1on` 0, `octave` −1, `semi` 7, `fine` −20, `poly` 1 | `b1on` 0 turns blade 1 off (overriding B.1's "on"); transposition inside its ranges; the smallest `poly` | no `gain`: 3.49 |
| `08-pitch-out-of-range.json` | Chord of formants (N 5) | `octave` 5, `semi` −15, `fine` 130 | each beyond its new range (−4..+4, −12..+12, −100..+100): clamped, and the clamps are reported | no `gain`: 3.49 |
| `09-spread-and-detune-clamp.json` | Locked horde (N 7) | `spread` 30 | `spread` 30 is nearest to ×24 by log distance, so `detuneRange` ×24; the stored `detune` is 90 cents (0.9), scaled by 30/24 to 1.125, clamped to 1, and the clamp is reported | no `gain`: 4.23 |
| `10-distribution-and-onset.json` | Locked horde (N 7) | `dist` 2, `h.law` 3, `seed` 4321, `harmReach` 2, `anchor` 1, `pivotMode` 1, `driftDepth` 0.3, `inertia` 0.4, `onsetScatter` 0.5, `onset` 0.2, `dissolve` 0.5, `h.driftRate` 0.8, `onsetAlpha` 0.5, `grav` 0.3, `spreadRamp` 0 | the "as is" renames; `h.law` 3 has no equivalent and becomes cents, reported; `spreadRamp` 0 becomes `motionGrain` 1 | no `gain`: 4.23 |
| `11-keepphase-and-spreadramp.json` | Locked horde (N 7) | `keepPhase` 1, `spreadRamp` 1, `poly` 64 | `keepPhase` 1 turns `osc1.retrigger` off whatever `phaseMode` says; `spreadRamp` 1 becomes `motionGrain` 0; the largest `poly` | no `gain`: 4.23 |
| `12-dropped-keys.json` | Quarter sync (N 1) | `driftMode`, `freqGlide`, `basin`, `attackScatter`, `relScatter`, `voiceEnv`, `motionCenter`, `inertiaCurve`, `bpm`, `beatMult`, `stretchB`, `cScale` 1, `dcMode` 1, `rtone`, `tempoGrid`, `onsetBipolar`, `ksmPerRate`, `bend`, `aaCarrier`, `aaXin`, `aaLoop`, `frameHold`, `stealFade`, `cutLatch`, `edgeTable` | every key B.2 drops, at a value that is not zero (the values are arbitrary examples). Each is dropped; B.2 reports one by name when its value would have sounded different from this engine's fixed behaviour (5.7) or default, and this file does not say which those are | no `gain`: 0.00 |
| `13-custom-ratios.json` | Clockwork (N 7) | `gain` 0.35 | `kCustom` as text (`"1, 7/6, 7/4, 21/8"`) with `kRule` 9, the one text value | 16.27 |

What is not covered, because B.2 does not say: a `gain` or `master` of 0, a `gain` below the
−60 dB floor, a `poly` outside 1 to 64, and a state with a key B.2 does not list.
