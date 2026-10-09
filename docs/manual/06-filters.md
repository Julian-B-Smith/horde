# Filters

Purpose: explain the filter section: the types, how cutoff, resonance and keytracking behave,
where filters sit in the signal (serial, parallel, per source), per-note filters, and comb as a
keytracked filter type. The filter lab measures each type against a fidelity programme, and the
manual reports what it guarantees.

## Filters in horde

What the filter section does and where it sits between the sources and the FX.

Status: PENDING — B274, B209

[[FIG:filters.overview]]

## Filter types

Every type, its character and its typical use.

Status: PENDING — B274, B287, B290

[[TAB:filters.types]]

[[FIG:filters.responses]]

[[AUD:filters.types-tour]]

## Cutoff and resonance

How the cutoff knob maps to pitch, how modulation sweeps it evenly in pitch, and how far
resonance goes.

Status: PENDING — B274, B218

[[AUD:filters.resonance-sweep]]

## Keytracking

Cutoff that follows the played note, read from the voice's one published pitch.

Status: PENDING — B278, B274

[[FIG:filters.keytracking]]

## Comb as a filter type

Comb moves out of the FX rack into the filters, with keytracking on, so it plays in tune.

Status: PENDING — B288

[[FIG:filters.comb-response]]

[[AUD:filters.comb-keytracked]]

## Serial and parallel placement

Chaining filters or running them side by side, and the `+` inlet.

Status: PENDING — B274, B209

[[FIG:filters.serial-parallel]]

## Per-note filters

A filter per voice for keytracking, and which types are available per note.

Status: PENDING — B303

## Stereo filter offsets

Left and right cutoff and resonance skew as part of true stereo.

Status: PENDING — B408

## What the filters guarantee

The measured fidelity of each type: response accuracy, behaviour under fast modulation,
aliasing and DC.

Status: PENDING — B290, B287

[[TAB:filters.fidelity]]

## Self-oscillation

Why self-oscillation is not offered as an intentional mode in 1.0.

Status: PENDING — B292
