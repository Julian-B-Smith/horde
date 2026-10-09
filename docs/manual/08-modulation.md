# Modulation

Purpose: teach the reader to make things move. Every relevant parameter is a modulation
destination; this chapter covers the sources (LFOs, the Kuramoto LFO, ORBITAL, envelopes, MIDI
trackers, randoms, followers), the mod matrix that connects them, how polarity and depth work,
modulating a modulator, and modulator morph across corners.

## How modulation works in horde

Sources, routes and destinations, and where modulation shows on the controls.

Status: PENDING — B392, B207

[[FIG:mod.overview]]

## Modulators

The modulation sources other than envelopes.

Status: PENDING — B208, B226

### LFOs

Shapes, rate in Hz or tempo-synced, phase, sample-and-hold and smoothed random.

Status: PENDING — B208, B237

[[TAB:mod.lfo-shape-list]]

[[FIG:mod.lfo-shapes]]

### The Kuramoto LFO

An LFO made of coupled oscillators that drift into and out of step.

Status: PENDING — B208, B226

[[FIG:mod.kuro-lfo]]

[[AUD:mod.kuro-lfo-demo]]

### ORBITAL

A small gravity simulation whose bodies' positions and speeds become modulation sources.

Status: PENDING — B126, B267

[[FIG:mod.orbital]]

[[AUD:mod.orbital-demo]]

### MIDI trackers

Velocity, note number, mod wheel, channel aftertouch and other performance sources.

Status: PENDING — B226, B431

[[TAB:mod.midi-sources]]

### Note-on randoms

A fresh random value per note, alone or as a linked pair.

Status: PENDING — B264

### The dry-signal follower

An envelope that follows the level of horde's own dry signal.

Status: PENDING — B259

### Coherence as a source

How locked the swarm is, as a modulation source.

Status: PENDING — B253

[[FIG:mod.coherence]]

### The spring

The pitch spring's displacement, from the bend laws, as a source.

Status: PENDING — B57, B397

## Envelopes

Amplitude and modulation envelopes, their hierarchy and their curves.

Status: PENDING — B366, B377

### The envelope hierarchy

Which envelope is global and which are per note, and how they relate.

Status: PENDING — B377, B229

[[FIG:mod.envelope-hierarchy]]

### Stage curves and times

Shaping each stage, and what a time knob means.

Status: PENDING — B366, B377

[[FIG:mod.envelope-curves]]

### Retriggering

What happens when a note is struck again while it sounds.

Status: PENDING — ADR-161, ADR-162

## The mod matrix

Connecting sources to destinations.

Status: PENDING — B392, B207

[[FIG:mod.matrix]]

### Making a route

Creating, editing and removing a route, from the matrix or from a control.

Status: PENDING — B392, B207

### Depth and polarity

Polarity belongs to the route, not the source: one LFO can push one destination up and another
both ways.

Status: READY-TO-WRITE — ADR-168

[[FIG:mod.polarity]]

### Modulating a modulator

Using one route's output to change another route's depth or a modulator's settings.

Status: PENDING — B207, B392

[[FIG:mod.mod-on-mod]]

### Per-corner modulation ranges

Modulation depths that differ at each morph corner.

Status: PENDING — B270, B392

### How fast modulation can go

The fastest rate each destination follows cleanly, and what happens beyond it.

Status: PENDING — B282, B275

[[TAB:mod.max-rates]]

## Modulator morph

Modulator shapes and settings morph between corners; their mappings flip, and a guard refuses
any in-between state that would form a modulation loop.

Status: PENDING — B396

[[FIG:mod.modulator-morph]]

[[TAB:mod.morph-per-type]]

[[FIG:mod.cycle-guard]]

## Sources at a glance

Every modulation source in one table.

Status: PENDING — B392, B275

[[TAB:mod.sources]]
