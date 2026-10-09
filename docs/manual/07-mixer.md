# The mixer, routing and gain staging

Purpose: show how sound travels from the sources through the filters to the FX and the master,
how to route it, and how to keep every stage out of clipping using the meters and latching clip
warnings at each tap and the standard input and output gain on every module. True stereo and the
width control are covered here because they act across the whole path.

## The mixer page at a glance

Strips, the routing matrix, meters and the master strip on one page.

Status: PENDING — B402, B225

[[FIG:mixer.page]]

## Signal flow

Sources to filters to FX to the master, and the default routing a new patch starts with.

Status: PENDING — B402, ADR-175

[[FIG:mixer.signal-flow]]

## The routing matrix

Sending any source to any filter or FX input with the crosspoint matrix, including feedback
paths.

Status: PENDING — B402, ADR-175

[[FIG:mixer.routing-matrix]]

## Source strips

Level, pan and mute for OSC 1, OSC 2, the Sub and the noise oscillator.

Status: PENDING — B402

## Meters and clip latches at every stage

Where the taps are, which are full meters and which are lamps, and why a clip warning stays lit
until clicked.

Status: PENDING — B225, B402

[[FIG:mixer.tap-map]]

[[TAB:mixer.taps]]

## Gain staging

Keeping each stage in its sweet spot.

Status: PENDING — B435, B225

### Module input and output gain

Every FX module has the same two gain controls, reachable from the mixer.

Status: PENDING — B435, B450

[[FIG:mixer.io-gain]]

### A gain-staging walkthrough

Finding and fixing an overloaded stage, step by step, by ear and by meter.

Status: PENDING — B435, B225

[[AUD:mixer.gain-staging-before-after]]

### The engine's output stage

The soft saturation at each oscillator's output and how hard to drive it.

Status: PENDING — B225, B376

## Corner FX buses

How per-corner FX chains feed the shared buses.

Status: PENDING — B258, ADR-193

[[FIG:mixer.corner-buses]]

## True stereo and width

What true stereo means in horde: width is more than pan, with filter, delay and modulator
offsets as part of one width control. Includes how a patch folds to mono.

Status: PENDING — B408, B414

[[FIG:mixer.width-equation]]

[[AUD:mixer.width]]

[[TAB:mixer.mono-fold]]
