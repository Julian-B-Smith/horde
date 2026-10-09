# Morph

Purpose: explain horde's morph: a global preset holds four corners, and a pad moves between
them. Blend morph crossfades values; quantum morph flips parameters one by one at seeded points
across the pad; stepped morph glide travels in steps. The chapter covers corners and global
attributes, the modes, each parameter's morph class, the pad and its waypoints, reshuffling, and
mixing corners taken from different presets.

## What morph is

Four corners and a position between them, and why horde treats a preset as a space to explore.

Status: READY-TO-WRITE — ADR-104, ADR-192

[[FIG:morph.concept]]

## Corners

Capturing, editing and loading a corner.

Status: PENDING — B269, ADR-109

[[FIG:morph.corner-editor]]

### Global attributes and corner material

What belongs to the whole preset and what belongs to each corner.

Status: READY-TO-WRITE — ADR-192

[[TAB:morph.global-vs-corner]]

## Morph modes

The three ways a move between corners can sound.

Status: PENDING — B269, B424

[[TAB:morph.modes-compare]]

### Blend

Every continuous parameter crossfades.

Status: PENDING — B269, ADR-104

[[FIG:morph.blend]]

[[AUD:morph.blend-demo]]

### Quantum

Parameters flip from one corner's value to another's, each at its own seeded point across the
pad.

Status: READY-TO-WRITE — ADR-104, ADR-185

[[FIG:morph.quantum-flip-map]]

[[AUD:morph.quantum]]

### Stepped morph glide

A move that travels in discrete steps, at a rate in Hz or synced, with glide softening each
step.

Status: PENDING — B424

[[FIG:morph.stepped]]

[[AUD:morph.stepped-demo]]

## Morph classes

Each parameter's morph class (blend, quantum or stepped, or exempt) and what it means for a move.

Status: PENDING — B275, B269

[[TAB:morph.classes]]

## The morph pad

Moving, gliding and automating the morph position.

Status: PENDING — B269, B302

[[FIG:morph.pad]]

### Waypoints and custom curves

Shaping the path between corners.

Status: PENDING — B235, B268

[[FIG:morph.waypoints]]

### Morph glide

How long the morph position takes to reach a new target.

Status: PENDING — B424, B386

## Reshuffle and the quantum draw

Redrawing where each parameter flips, and why an old patch never changes how it sounds when
horde grows.

Status: PENDING — B308, ADR-185

[[FIG:morph.reshuffle]]

## Mixing corners from different presets

Swapping in a corner from another preset, and what happens to the global attributes.

Status: READY-TO-WRITE — ADR-192

[[FIG:morph.mixed-corners]]

## The advanced morph page

Fine-tuning controls: corner sharpening, per-group cohesion, exempt parameters and locks, and
importing a corner's global parameters.

Status: PENDING — B269, ADR-193

[[FIG:morph.advanced-page]]
