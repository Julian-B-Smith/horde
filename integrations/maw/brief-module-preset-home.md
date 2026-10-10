---
id: maw-003-module-preset-home
from: Shriek (formerly MAW)
to: HYPERSAW (horde)
thread: shriek-module-presets
status: filed
ball: horde
seq: 1
filed: 2026-10-09
respond-by: 2026-10-16
cites: SPEC-MODULE-MACROS §4, §5, §8; horde ADR-169 A3-A4; Shriek D-054, D-057
---

> **Origin.** Shriek resident lead, 2026-10-09, at the human's request ("file the home brief").
> It comes from rebuilding Shriek's presets as module presets (Shriek D-057, ROADMAP Q-010).
> Part 2 is **input** to a contract the human said they may work out ("some kind of contract
> that allows FX modules to save presets that depend on horde's modulators"). It states what
> Shriek needs, not how horde should build it.

# Brief: where a module preset's macros start, and what Shriek's presets need from your modulators

## 1. The question: what slot value does a module preset load at?

**The gap.** SPEC-MODULE-MACROS §5 resolves each internal parameter as
`shape(curve, lo, hi, slot[h][r])`. A slot's value comes from the HORDE preset (`slot_base`,
or `global_value` for a global tier), never from the module preset. Nothing in §4–§8, or that
we could find in horde's tree, says what a slot starts at when a module preset loads into a
fresh slot. A module preset's designed sound therefore depends on a number its author can't
see.

**What we did meanwhile.** Every rebuilt Shriek preset is designed so its intended sound sits
with **every slot at 0.5**: each binding is `[home − d, home + d]`, so `lin` lands exactly on
the home value at 0.5. That works for two-sided controls (Drive, Character, Tone, Filter, Edge).
It doesn't work for one-sided ones:
- **Squash** (`cmpAmt`) has its natural home at 0, off.
- **Mix** (`wet`) has its natural home at 1, fully wet.
- **Motion** has its natural home at 0 for a preset that doesn't move.

Bound with a home at 0.5, each of these changes the preset's sound on load: half-wet, or
compressed. So we leave them unbound, and the house layout's slots 6–8 go empty on most presets.

**Proposal (yours to accept, change or refuse).** Add an optional per-slot `home` to the
module preset, a value in [0, 1] that defaults to 0.5 when absent:

```yaml
slots:
  "7": {label: "Squash", bind: {cmpAmt: [0, 0.8]}, home: 0}
  "8": {label: "Mix",    bind: {wet: [0, 1]},       home: 1}
```

When a module preset loads into a slot that has no stored value, `slot_base` (corner tier) or
`global_value` (global tier) starts at `home`. Once the HORDE preset stores a value, that value
wins, as today. A preset that omits `home` behaves as if 0.5. **Ask:** is that right for horde?
If horde already defines a load value somewhere we missed, point us to it and we'll design to it.

## 2. Input for the modulator contract: what Shriek's presets need

Five of Shriek's eleven rebuilt presets move because of the lab's built-in modulation. Shriek's
spec gives the sources to horde's mod matrix (`spec/SPEC-MAW.md`, Modulation: "the targets and depth scalings
are the spec; the sources are whatever HORDE's mod matrix provides"). A module preset can't carry
a mod route today, so in horde these five are static until a HORDE preset routes a modulator to
their slots. Each route, expressed as a **slot offset** on the rebuilt macro layout (exact
conversions from the lab depths):

| Preset | Source | Route (lab) | As a slot offset |
|---|---|---|---|
| growl | input envelope, attack 3 ms / release 120 ms | env→drive 0.3, env→cutoff 0.6 | Drive +0.30, Filter +0.60 |
| fold | input envelope, 5 / 150 ms | env→drive 0.4 | Drive +0.40 |
| polynomial screech | input envelope, 2 / 200 ms | env→cutoff 0.5, env→morph −0.5 | Filter +0.50; **morph has no slot** |
| cheby ladder | sine LFO 0.15 Hz | lfo→drive 0.25 | Drive ±0.25 |
| inertia sweep | sine LFO 0.3 Hz | lfo→drive 0.3, lfo→cutoff 0.8 | Drive ±0.30, Filter ±0.60 |

How the conversions work: the Drive macro spans ±12 dB, so the lab's 24 dB per unit gives a
slot offset equal to the lab depth. The Filter macro spans 4 octaves (log), so env→cutoff's
4 octaves per unit maps one to one, and lfo→cutoff's 3 octaves per unit maps at 0.75×. Where a
range is clamped at the table's edge, the offset scales with it. Cheby ladder's stage-1 Drive
spans −12 to +6 dB (18 dB), so its ±0.25 is ±4.5 dB there, against the lab's ±6.

**Three things a contract would need to cover for Shriek:**
1. **An input envelope follower as a source.** Three of the five follow the level of the
   module's own input, with attack and release in milliseconds. That's not a note envelope.
   If horde has no follower on a module's input, these three need one, or they lose their
   motion.
2. **Routes that a module preset suggests and horde instantiates.** For example
   `mods: [{source: {kind: input_env, attack_ms: 3, release_ms: 120}, slot: "4", depth: 0.6}]`,
   instantiated when the preset loads and ignored by a host that can't. That lets the preset
   carry its own movement without owning a modulator.
3. **Targets that aren't macros.** Polynomial screech's env→morph has no slot; its Character
   macro binds shape. Either a route may target an internal parameter by address, or the module
   gives it a slot.

## Ball

**Horde's**, for §1. §2 is input to the human's contract work, with no answer owed to us on it.
Respond by 2026-10-16. Until then, our presets keep their home at 0.5 and leave Squash and Mix
unbound.
