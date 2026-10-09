# Pitch: bend laws, glide, the quantizer and microtuning

Purpose: explain how horde decides each voice's pitch. Bend and glide travel by the same set of
laws, including the inertial mass-spring, on one published pitch per voice that every part reads.
The scale quantizer snaps pitch to a root and scale and is shared with FX and other devices.
MTS-ESP brings in external microtuning.

## Pitch in horde

The two lanes, bend and note, and the one pitch per voice that the engine, the Sub and the
filters all follow.

Status: PENDING — B397, B278

[[FIG:pitch.lanes]]

## Pitch bend

The bend wheel, its range and its laws.

Status: PENDING — B397, ADR-096

### Bend range

Setting the range per patch.

Status: PENDING — B431, B397

### Bend laws

The laws a bend can follow, and how each feels under the wheel.

Status: PENDING — B397, ADR-096

[[TAB:pitch.bend-laws]]

[[FIG:pitch.bend-law-curves]]

[[AUD:pitch.bend-laws-demo]]

### The inertial mass-spring

A bend with weight: the pitch overshoots and settles like a mass on a spring.

Status: PENDING — B397, ADR-096

[[FIG:pitch.mass-spring]]

[[AUD:pitch.mass-spring-demo]]

## Glide

Gliding between notes, by the same laws as the bend.

Status: PENDING — B397, ADR-096

### Glide time and units

How a glide time is set and measured.

Status: PENDING — B397

### Glide from

The three places a glide can start from.

Status: PENDING — B397

[[TAB:pitch.glide-from]]

[[AUD:pitch.glide-laws]]

## The scale quantizer

Snapping pitch to a root and a scale.

Status: PENDING — B391, B88

[[FIG:pitch.scale-editor]]

### Where the quantizer acts

The quantizer is anchored on the bend lane, and what that means when bending into a scale.

Status: PENDING — B391, ADR-106

[[FIG:pitch.quantizer-placement]]

[[AUD:pitch.quantized-bend]]

### Sharing the scale

The same scale used by FX and other parts, and setting the scale from MIDI.

Status: PENDING — B391, B260

## Microtuning with MTS-ESP

Following an external tuning source, and which wins when both MTS-ESP and the quantizer are on.

Status: PENDING — B431, B390

[[TAB:pitch.mts-precedence]]
