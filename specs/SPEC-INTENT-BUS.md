# QM-4 — Intent bus, per-corner bindings, and modulation tiers

**Project:** HORDE
**Status:** Recommendation for implementation. Numbering follows the QM-0…3 series; renumber if it collides.
**Parity oracle:** `reference/intent-bus.html` (browser prototype, 4 corners, 8 parameters, 1 structural parameter, 1 corner-level modulator, 1 global modulator). The prototype defines the *behavior* this spec describes; where the spec and the prototype disagree, the spec wins and the discrepancy should be logged as an ADR.

---

## 1. Problems this solves

These are the concrete failures in the current HORDE build that motivated the design. Every mechanism in this spec should be traceable back to at least one of them.

**P1 — The XY pad silently destroys presets.**
The pad is mapped by default to load-bearing parameters and writes absolute values to them. Its position therefore acts as an invisible fifth author of every corner: a preset authored with the pad at one position sounds wrong at any other, and a preset that depends on a locked value gets overwritten the moment the pad moves. There is no representation of "this parameter must stay here for this corner to work."

**P2 — Macros have no coherent meaning across the morph.**
A macro's meaning is defined once at device level, but the parameters it should act on are corner-level. A parameter that is pivotal in one corner is meaningless or harmful in another. Any single fixed mapping is wrong for at least one corner, and there is no way for a corner to say "this macro does nothing here" or "this macro means something different here."

**P3 — No principled boundary between global and corner-level modulation.**
Some parameters necessarily live above the morph (structural, routing), some routings necessarily live above it (anything that drives the morph itself), and for the rest the user should choose. Without a stated rule, the mod matrix has to special-case each parameter and the UI can't explain why some targets are unavailable.

**P4 — Intermediate (quantum) states have no defined macro behavior.**
At any morph position other than a corner, parameters belong to a mix of corners. A device-level macro mapping has no answer for what it means to turn the knob when half its targets are in corner A and half in corner B.

**P5 — Structural parameters can half-flip.**
Engine selection, unison count, FX routing, and similar parameters cannot be sampled per-parameter without producing nonsense states. They need a resolution rule that differs from ordinary parameters.

---

## 2. Design principles

1. **Macros never write to parameters.** They write to a small fixed set of named *intents*. Corners interpret intents.
2. **A binding belongs to the parameter it targets, not to the macro.** The unit that flips under quantum morph is the parameter *and everything attached to it*: base value, valid range, intent sensitivities, corner-level mod routings targeting it.
3. **Performance controls are offsets from a corner-owned rest position**, never absolute values. At rest, the sound is exactly the authored corner.
4. **Load-bearing is a corner property, not a global one.** A corner declares the valid range of any parameter while that corner owns it. A lock is a range of zero width.
5. **Anything that controls the morph or the intents lives above the morph.** A modulation source cannot live inside the thing it controls.
6. **Structural state resolves atomically.** One atom per structural parameter, sampled like any other atom, never blended.
7. **Saving flattens.** Committing a corner bakes current offsets into its base and re-centers its rest positions, so no saved corner depends on a control being displaced.

---

## 3. Data model

### 3.1 Intents

A device-wide, fixed, ordered set of named intents. Prototype uses four: `X`, `Y`, `INT` (Intensity), `MOT` (Motion). Each intent is a bipolar scalar in `[−1, 1]`.

Recommended production set: `X`, `Y` (pad axes) plus 4–8 named macros. Names are stable across the whole device; only their interpretation morphs. Intent names should be evocative but non-committal ("Intensity," "Motion," "Space," "Color," "Bite," "Air") — they are intents, not parameter names.

### 3.2 Parameter classes

| Class | Lives in | Morphs? | Resolution |
|---|---|---|---|
| **Morphable** | Corner | Yes | Per-parameter atom |
| **Structural** | Device, with per-corner *request* | Yes, atomically | One atom per structural parameter; the resolved corner's request wins in full |
| **Device** | Device only | No | Not part of the morph at all (master volume, MIDI config, global mod sources, intent values) |

Classification is fixed per parameter in the engine definition. Unison/voice count, engine selection, FX topology, oscillator count, and routing switches are structural. Continuous DSP parameters are morphable. Anything that is a source rather than a destination (morph position, intents, global mod sources) is device.

### 3.3 Corner

```
Corner {
  id, name, color
  base[p]          : Float                    // for every morphable parameter p
  range[p]         : [lo, hi]                 // default [0, 1]; lo == hi means locked
  bind[i][p]       : Float in [−1, 1]         // depth of intent i on parameter p, default 0
  curve[i][p]      : Curve                    // optional; default linear (prototype is linear only)
  home             : { x, y }                 // pad rest position in pad coordinates
  macroRest[i]     : Float                    // optional; rest value per macro (prototype: always 0)
  request[s]       : Value                    // for every structural parameter s
  mods             : List<Routing>            // corner-level modulation routings (see §6)
}
```

Depths are in *normalized parameter units per full intent swing*: intent at +1 with depth 0.4 pushes the parameter +0.4 in its 0–1 normalized range, before the corner clamp.

### 3.4 Routing

```
Routing {
  source           : ModSource                // LFO, envelope, random, etc.
  target           : Parameter | Intent | MorphAxis
  depth            : Float
  scope            : corner | global
  respectRange     : Bool                     // global scope only; default false
}
```

Scope rules are in §6.

### 3.5 Device state

```
Device {
  morph            : { x, y }                 // user-set morph position
  steepness        : Float ≥ 1                // corner weight sharpening
  seeds[a]         : Float in [0, 1)          // one per atom a (each morphable parameter, each structural parameter, `home`, and each macroRest)
  intents[i]       : Float                    // live intent values
  puck             : { x, y, vx, vy }         // performance pad state
  latch            : Bool
  globalMods       : List<Routing>            // scope == global
}
```

Seeds are the "flip points." *Reshuffle* redraws them. Seeds persist with the device state, not with corners, so a saved device recalls the same flip topology.

---

## 4. Resolver

Runs once per control-rate tick. Order matters.

### 4.1 Effective morph position

Global routings targeting `MorphX` / `MorphY` are applied here, before anything else:

```
morphEff = clamp01( morph + Σ globalMods[target ∈ {MorphX, MorphY}] )
```

### 4.2 Corner weights

Bilinear from `morphEff`, then sharpened:

```
w_A = (1−x)(1−y), w_B = x(1−y), w_C = (1−x)y, w_D = xy
w_c ← w_c ^ steepness ; normalize Σ w_c = 1
```

`steepness = 1` is a soft blend (many intermediate hybrids); large values approach hard flips. Prototype default 8, range 1–24.

### 4.3 Atom resolution

For every atom `a` with seed `s_a`, walk corners in fixed order A→B→C→D accumulating `w_c`; the owner is the first corner where the cumulative weight exceeds `s_a`.

Atoms: every morphable parameter, every structural parameter, `home`, every `macroRest` (if implemented). This yields:

- `owner[p]` for each morphable parameter
- `owner[s]` for each structural parameter → apply `corners[owner[s]].request[s]` in full
- `homeOwner` → the current pad home is `corners[homeOwner].home`

Flip salience (per-parameter weighting from QM-1) composes with this by biasing the seed distribution; not exercised in the prototype.

### 4.4 Performance pad and intents

```
home   = corners[homeOwner].home
target = dragging ? pointer : (latch ? none : home)
puck  += spring(target)          // mass-spring-damper; prototype k=90, damping=11 at rest; stiffer while dragging
X = clamp(  (puck.x − home.x) · 2 , −1, 1 )
Y = clamp(  (home.y − puck.y) · 2 , −1, 1 )      // up is positive
```

Full intent swing corresponds to ±0.5 of the pad's extent from home. The home marker is allowed to leave the pad-reachable region; the puck is clamped to the pad.

Macro intents come from their knobs, bipolar, rest at `macroRest` (0 in prototype). Global routings targeting an intent add to it here.

### 4.5 Parameter evaluation

For each morphable parameter `p`, with `c = owner[p]`, `cr = corners[c]`:

```
v  = cr.base[p]
v += Σ_i  intents[i] · curve[i][p]( cr.bind[i][p] )          // intents, via this corner's bindings
v += Σ    cr.mods[scope == corner, target == p]               // this corner's own routings
v  = clamp( v, cr.range[p].lo, cr.range[p].hi )               // corner clamp — the load-bearing guard
v += Σ    anyCorner.mods[scope == global, target == p]        // promoted routings, regardless of owner
v += Σ    globalMods[target == p]                              // device-level routings
       ( each of these: if respectRange, re-clamp to cr.range[p] after adding )
final[p] = clamp01( v )
clamped[p] = (pre-clamp v ≠ post-clamp v)                      // for UI
```

Consequences worth stating explicitly:

- A corner that does not bind an intent to `p` (depth 0) makes that intent inert for `p` while the corner owns it. This is how "this macro does nothing here" is expressed — no special case.
- A locked parameter (`lo == hi`) cannot be moved by intents or corner-level mods while locked. Global mods can move it unless `respectRange` is set.
- At intermediate morph positions the same intent acts through different corners' bindings on different parameters simultaneously. This is intended and is the answer to P4.

Corner-level modulators whose own parameters are morphable (e.g., LFO rate) are evaluated after §4.4 and before the parameters they target, using the same evaluation rule. The prototype does this for one LFO; production should resolve the dependency graph generally or restrict modulator parameters to a fixed evaluation tier.

---

## 5. Macros and pad: UI semantics

**Ownership tint.** For intent `i`, compute per-corner live weight `Σ_p [owner[p] == c] · |bind[i][p]|`. The knob (or pad axis) takes the color of the corner with the largest weight. If all weights are zero, it takes the "inert" color and its readout says so ("nothing here"). This extends the QM-0 color scheme from parameters to controls: the color tells you whose interpretation you're turning.

**Readout.** Below each macro, list the parameters it currently touches (non-zero live depth). Updates as ownership flips.

**Pad.** Draw: the current home marker in its owner's color; the other corners' homes faintly; the tether from home to puck; the full-swing reach box (dashed) around home; axis owner tints on the pad edges. The puck is white. Home jumps discretely on flip; the puck follows with inertia — the QM inertia doctrine applies here verbatim.

**Latch.** When off (default), releasing the puck returns it to home, so the sound returns to the authored corner. When on, the puck stays where it was left and displacement is measured from whatever home is current.

**Clamp indicator.** When an intent or corner-level mod is being held back by a corner range, mark the parameter (prototype: red edge on the bar). This is the user's signal that they're pushing against a load-bearing value, replacing the silent overwrite from P1.

---

## 6. Modulation tiers

### 6.1 Scope rules

| Target | Corner scope | Global scope |
|---|---|---|
| Morphable parameter | ✓ (default) | ✓ (by promotion) |
| Structural parameter | ✗ | ✗ (structural params are not modulation targets) |
| Intent | ✗ | ✓ |
| Morph axis | ✗ | ✓ |
| Another modulator's rate/depth, when that modulator is corner-level | ✗ | ✓ |

The UI enforces this by construction: the corner-level target picker does not list intents, morph axes, or structural parameters. Do not implement as a validation error.

### 6.2 Corner-scope routings

Live inside the corner. Apply only while the corner owns the target parameter, inside the corner clamp. They flip with their target; a routing is part of the target's atom bundle. A corner's routing that targets a parameter the corner does not currently own is simply silent — no orphan handling needed.

### 6.3 Promotion

Any corner-scope routing can be promoted to global with a per-routing toggle ("travels with corner" ↔ "persists across morph"). A promoted routing applies regardless of owner, outside the clamp, and is evaluated with the device-level routings. Promotion does not move the routing's storage — it stays in the corner that authored it, so demoting it later restores corner behavior without loss. Storage location and evaluation tier are independent.

### 6.4 Device-level routings

Authored at device level, never in a corner. May target anything global scope permits. Carry the `respectRange` flag; default off ("global means I accept the consequences"). Consider a per-corner "armored" flag on individual parameters that forces `respectRange` for the handful of values whose violation produces silence or a fault rather than merely a bad sound — see Open decisions.

---

## 7. Commit

Command: **Commit to dominant corner** (dominant = argmax `w_c`; expose as "Commit to A/B/C/D" when the user is near a corner, and as a picker otherwise).

For the target corner `cr`:

1. For each parameter `p` with `owner[p] == cr`: `cr.base[p] ← clamp( base + Σ intents · bind , cr.range[p] )`. Corner-scope mod contributions are *not* baked (they are dynamic).
2. `cr.home ← puck`.
3. `cr.macroRest[i] ← intents[i]` if per-corner macro rest is implemented; otherwise zero the macro intents.
4. Parameters owned by other corners are untouched.

After commit, the resolved sound at the current morph position is unchanged and all offsets read zero. This is the invariant that answers P1 at the authoring end: a saved corner never depends on a displaced control.

**Save** (persisting a corner) should require or strongly suggest commit first. Do not allow a save that stores non-zero offsets implicitly.

---

## 8. Structural parameters

Each structural parameter is a single atom with its own seed. The resolved corner's request applies in full. Changes are discrete and should trigger whatever the engine already does for discrete changes (voice re-allocation, routing rebuild) — the resolver's job is only to guarantee the change is never partial. Structural changes are not smoothed; inertia does not apply.

Corners may leave a structural request unset, meaning "inherit the device default." Resolution still picks an owner; an unset request resolves to the device default.

---

## 9. Migration from current HORDE state

1. **Classify every parameter** (morphable / structural / device) in the engine definition. This is the first PR; nothing else can land before it.
2. **Convert the existing XY mapping into default bindings.** The current device-level XY→parameter map becomes the *default* `bind[X]` / `bind[Y]` for a new corner. Existing corners get the same defaults, then the author can zero or change them per corner. The default depth should be modest (≤ 0.3) so the migration doesn't reproduce P1 at reduced scale.
3. **Backfill `home`** for every existing corner as pad center. Backfill `range` as `[0, 1]` everywhere; locks are opt-in and should be added by hand to the corners that actually need them.
4. **Re-home the existing mod matrix.** Routings targeting the morph position or macros become device-level. Everything else becomes corner-scope in every corner that had it, with the promotion toggle available.
5. **Seeds:** generate on first load; persist thereafter.

Open question for migration: whether default bindings should be *inherited* (a corner stores only its deltas from device defaults) or *copied* (every corner stores a full binding table). Inheritance keeps new corners sane and the pad never blank; copying is simpler in the resolver and makes corner export/import self-contained. Prototype copies. Recommend copying for v1 with a "reset bindings to device default" action per corner, and revisiting inheritance only if authoring friction shows up.

---

## 10. Parity with the prototype, and deliberate divergences

**Parity (behavior the implementation must reproduce, testable against the prototype):**
- Bilinear weights, `^steepness` sharpening, cumulative-walk atom resolution in A→B→C→D order, per-atom seeds.
- Evaluation order in §4.5, including where the clamp sits relative to each tier.
- Pad displacement → intent mapping (±0.5 pad extent = full swing), spring return, latch semantics.
- Ownership tint and "nothing here" rule.
- Commit invariant (§7).
- Global-only target restriction expressed as picker contents.

**Deliberate divergences from the prototype:**
- Prototype macros rest at 0 with no `macroRest`; production may implement per-corner macro rest (it is the same mechanism as `home`, one dimension at a time).
- Prototype has one corner-level modulator with one routing; production has a full corner-level routing list per corner, each routing an atom-bundle member of its target.
- Prototype has linear curves only; production supports `curve[i][p]`.
- Prototype resolves the one modulator's rate by hand-ordering; production needs a general evaluation order for modulators whose parameters are morphable.
- Prototype's `respectRange` is a single global toggle; production is per-routing.
- Prototype tint is by `Σ|depth|`; production may weight by current contribution `Σ|intent · depth|` instead. Either is acceptable; choose one and log it.
- Prototype reach box is fixed at ±0.5; production may expose reach as a device parameter.

---

## 11. Open decisions (ADR candidates)

1. **Binding inheritance vs. copying** (§9). Recommended: copy, with reset action.
2. **Armored parameters.** Whether to add a per-corner per-parameter flag forcing `respectRange` for global mods. Recommended: yes, but only for parameters the engine marks as fault-capable (e.g., anything that can produce DC, silence, or unbounded feedback), not as a general authoring tool.
3. **`home` as one atom vs. blended.** Prototype flips home as a single atom, which produces visible jumps for the spring to chase. An alternative is a weight-blended continuous home. Recommended: single atom — it is consistent with the rest of the system and the inertia doctrine makes the jump legible rather than jarring.
4. **Intent count and names** for production.
5. **Whether corner-scope mod contributions are baked on commit.** Spec says no; revisit if authors expect "commit" to mean "freeze what I hear."
6. **Tint weighting** (§10).

---

## 12. Acceptance tests

- **T1 (P1):** Author corner B with `cutoff` locked. Move the pad and macros through their full range while B owns cutoff. Cutoff does not move; the clamp indicator shows. Save B; reload; B sounds identical regardless of pad position.
- **T2 (P2):** Corner B has zero depth for macro MOT on all parameters. While B owns every parameter, MOT reads "nothing here" and turning it changes nothing. Morph toward A; MOT progressively acquires A's targets and tint.
- **T3 (P3):** Corner-level target picker never offers MorphX/MorphY/intents/structural parameters. A device-level routing to MorphX moves the effective morph position and every downstream owner.
- **T4 (P4):** At morph (0.5, 0.5) with steepness 1, a single macro measurably acts through at least two corners' bindings simultaneously.
- **T5 (P5):** With four corners requesting unison 1/3/5/7, sweeping the morph never produces a voice count outside {1,3,5,7} and never produces a partial reallocation.
- **T6 (commit):** Displace pad and macros; commit; resolved output is bit-identical (within control-rate smoothing) before and after; all offsets read zero; the committed corner's home equals the puck position.
- **T7 (promotion):** A corner-scope routing targeting cutoff is silent while another corner owns cutoff. Promote it; it is audible regardless of owner and can push cutoff outside the owner's range.
- **T8 (return):** Latch off: release the puck; it returns to the current home and intents X, Y settle to zero. Flip the home owner mid-return; the puck retargets to the new home without discontinuity in its own position.
- **T9 (seeds):** Save device, reload, sweep morph: flip points are identical. Reshuffle: flip points change; corner sounds at the four exact corners do not.
