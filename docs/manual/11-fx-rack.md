# The FX rack

Purpose: explain the FX rack and every module in it. One instance of each module type, chained
per corner, with the chain itself morphing between corners. The chapter first covers what every
module shares (input and output gain, presets, macros, presence, tails), then the per-corner
chains and the FX-chain morph, then one section per 1.0 module. Sluice is a private, optional
part of the build, and the manual says what a build without it offers.

## The rack

One instance per module type, the order of the chain, and how a module is added or removed.

Status: PENDING — B50, B393, ADR-172

[[FIG:fx.rack]]

[[FIG:fx.rack-signal]]

## What every module shares

The controls and behaviours common to all modules.

Status: PENDING — B439, B450

[[TAB:fx.module-common]]

### Input and output gain

The two standard gain controls on every module.

Status: PENDING — B435, B450

### Presence and bypass

Turning a module on and off, and what "off" guarantees.

Status: PENDING — B450, ADR-195

### Module presets

Each module's own presets, and how they sit inside a global preset.

Status: PENDING — B395, ADR-188

### Module macros

Up to eight macros per module, ordered and labelled by its presets.

Status: READY-TO-WRITE — ADR-169

### Tails and latency

Reverb and delay tails ring out after a note, and modules that can morph add no latency in 1.0.

Status: PENDING — B429, ADR-193

[[FIG:fx.tail-ringout]]

## Per-corner FX chains

Each corner can carry its own chain; this section explains how the chain changes as you morph.

Status: PENDING — B394, B265, ADR-193

[[FIG:fx.per-corner-chains]]

### How the chain morphs

Order changes, modules entering and leaving, and the crossfades between them.

Status: PENDING — B394, B266

[[FIG:fx.chain-morph]]

[[AUD:fx.chain-morph-demo]]

### The ordering rules

The house rules that keep a chain musical and affordable.

Status: PENDING — B265, ADR-193

[[TAB:fx.house-order]]

### A module's patch at each corner

A module holds one patch per corner; different corners can hold different patches.

Status: READY-TO-WRITE — ADR-188

[[FIG:fx.module-patch-corners]]

## Shriek

The three-stage saturator (formerly MAW).

Status: PENDING — B318, B393

[[FIG:fx.shriek-screen]]

[[FIG:fx.shriek-stages]]

[[AUD:fx.shriek]]

## Sluice

The morphable FX network: dispersers, shifters, phasers and chorus in one patchable module.

Status: PENDING — B328, B393

[[FIG:fx.sluice-screen]]

[[FIG:fx.sluice-network]]

[[AUD:fx.sluice]]

### When your build has no Sluice

What a public build without the private Sluice part shows and does.

Status: PENDING — B328

## Scape

The reverb.

Status: PENDING — B152, B421

[[FIG:fx.scape-screen]]

[[AUD:fx.scape]]

## ECHO

The standard stereo delay: tempo sync, ping-pong, mid/side, feedback filtering and modulation.

Status: PENDING — B425

[[FIG:fx.echo-screen]]

[[FIG:fx.echo-modes]]

[[AUD:fx.echo-modes-demo]]

## Bulwark compressor

The dynamics module.

Status: PENDING — B421, B439

[[FIG:fx.bulwark-screen]]

[[FIG:fx.bulwark-transfer]]

[[AUD:fx.bulwark]]

### Multiband mode and the ATM preset

CONDITIONAL for 1.0. Band count flips between corners, never blends.

Status: PENDING — ADR-190, ADR-195

## EQ

The equaliser.

Status: PENDING — ADR-190, B234

[[FIG:fx.eq-screen]]

[[FIG:fx.eq-curves]]

## FX filter

A filter as an FX module, after the synth's own filters.

Status: PENDING — ADR-190

[[FIG:fx.fx-filter-screen]]

## Drive

The simplified saturation and drive module, for drive before and after another effect.

Status: PENDING — ADR-190

[[FIG:fx.drive-screen]]

[[AUD:fx.drive-around-reverb]]

## Kuramoto chorus

CONDITIONAL for 1.0: a chorus made of coupled oscillators.

Status: PENDING — B401, ADR-190

[[FIG:fx.kchorus-screen]]

[[AUD:fx.kchorus]]

## Module parameters

A pointer to the generated parameter reference (chapter 19).

Status: PENDING — B450, B275
