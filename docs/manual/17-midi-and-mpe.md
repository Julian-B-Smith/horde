# MIDI, MPE, the arpeggiator and automation

Purpose: cover how horde is played and controlled from outside: the MIDI messages it answers,
voicing modes, MPE and channel aftertouch, the sustain pedal, MIDI learn, the simple
arpeggiator, and host automation in each plugin format. MPE is in 1.0 by ruling; its design is
still to come, so its sections are outlines.

## MIDI input

Notes, velocity, pitch bend, mod wheel, channel aftertouch and the sustain pedal.

Status: PENDING — B431, ADR-190

[[TAB:midi.messages]]

## Voicing: poly, mono and legato

How notes are assigned to voices, when an envelope retriggers, and portamento in mono.

Status: PENDING — B431, B310

[[TAB:midi.voicing-modes]]

[[AUD:midi.legato-vs-retrigger]]

## MPE

Per-note pitch, pressure, timbre and slide, reaching the swarm, the blades, the mod matrix and
morph.

Status: PENDING — B388, ADR-190

### The MPE dimensions

Which per-note expression drives what by default.

Status: PENDING — B388

[[TAB:midi.mpe-dimensions]]

### Setting up MPE in your DAW

Enabling MPE per DAW and per format.

Status: PENDING — B388, B429

[[TAB:midi.mpe-hosts]]

## MIDI learn

Assigning a hardware control to a horde parameter.

Status: PENDING — B431

[[FIG:midi.learn]]

## The arpeggiator

The simple arpeggiator in 1.0: its modes, rate and sync, octave range and gate.

Status: PENDING — B391, ADR-190

[[TAB:midi.arp-modes]]

[[AUD:midi.arp]]

## Automation in your DAW

How parameters appear to the host, their names, and how modulation reaches the host in each
format.

Status: PENDING — B429, B432

[[TAB:midi.automation-by-format]]

### Parameter identity after 1.0

Automation and project data keep working across updates because parameter ids are locked at the
stability line.

Status: READY-TO-WRITE — ADR-186, B428
