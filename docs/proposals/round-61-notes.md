# Spec round 61, draft: the morph's ruled gaps (B631)

> **Origin.** Drafted for horde's lead, 2026-11-06, from ADR-370 / B631: the human's answers to the morph questions
> lab (PR #1423), relayed by the GUI session the same day. A draft for the lead's review. Nothing here has reached the
> clean room. `ADR-372` in the spec text is a placeholder for the round's ADR number.

## What changed in `specs/SPEC-ENGINE.md`

| row | change | ruling |
|---|---|---|
| §3 table, §3 tree | The resolver is MO-1 to MO-25 and takes waypoints and region locks. | rulings 1 to 3 |
| IF-10 | The heard setting set names each setting's `started_at`, the sample at which it entered its slot. The clock it counts on is marked `[OPEN]`. | B631 (the lead) |
| IF-12 | New resolver inputs: each Morphable key's waypoints in stored order, the waypoint law and clamp, each key's region lock. A new read that changes nothing: each key's resolved value, and a key's region shape as a lock would take it. | rulings 1 to 3, 8 |
| RC-11.1 | The morph system's own state now lists waypoints, the waypoint law and clamp, region locks, and the patch's own corner presets. The user's corner-preset library is shared by every patch and isn't in any patch's state. | rulings 1 to 3, 7 |
| D intro, MO-1, MO-3 | Waypoints and locks are resolver inputs and morph-system state. | rulings 1 to 3 |
| MO-4 | The waypoint law also reads the position itself, not only the weights. | ruling 1 |
| MO-5 | (b)'s equivalence with gliding the weights holds only while a key's waypoints aren't acting. | ruling 1 |
| MO-7 | A locked key scores and latches by MO-24. | ruling 3 |
| MO-9 | A key whose waypoints act takes MO-22's value. | rulings 1, 2 |
| MO-12 | Waypoints and a lock don't act under Exempt. Exempt concerns the morph only: the held value takes per-voice modulation wherever PA-2 gives its key modulation. | ruling 9 (GN-40) |
| **MO-21** (new) | What a key holds: Morphable keys only, 8 per key, 64 per patch, dormant ones counted, stored in order; 0.04 spacing; the list refused whole past a limit; the waypoint law (TPS·Δ default, TIN·Δ) and Clamp to set values (on by default). | ruling 1; B272 |
| **MO-22** (new) | The waypoint law as math: the bilinear base, residuals at the nodes, TPS·Δ (thin-plate spline) and TIN·Δ (Bowyer–Watson from the A–D split, in stored order), corners read and never computed, the two clamps. | B235, B272 |
| **MO-23** (new) | When waypoints act: morph on, law Blend, key mode Follow. Dormant under Quantum or Stepped, and under a key's own Quantum, and active again in Blend. A corner edit keeps every waypoint's position and value. | ruling 2 |
| **MO-24** (new) | The region lock: three offsets, the score ln w + δ, which temperature, cohesion, the seed and the draws don't reach; the latch; locking as is; a corner edit changes no lock and its new value plays in its region. | ruling 3; B269 |
| **MO-25** (new) | Save current sound: what the resolver supplies, and what the morph system writes into the corner. | ruling 8 |

`engine2/structure/engine-structure.json`: `spec.blob` moved to the new spec's blob. The structure check reads green
with no other change. Round 61 adds no block or edge the file declares: the resolver's inputs aren't drawn there.

## Ruling 4 (X and Y as host parameters) is staged, not landed

**The D.2 rows can't land on their own.** With `morph.x` and `morph.y` added to D.2, `./verify fast` reads
`gen_gui3_controls: RED [unreached] 2 host parameter(s) on no page: morph.x, morph.y`. GUI-45 r3 places the two knobs
under the Morph page's pad once the rows exist, and that placement is the GUI session's `layout.json` plus a regenerated
`shell-data.js`. Neither file is in this round's scope, and fixing the text means leaving the rows out, not working
round the gate. **The rows have to land in the same PR as the GUI session's layout change, or after it.**

The text to land, after `morph.glide`'s row in D.2:

```
| `morph.x` | X | 0 to 1, lin | 0 | The pad position's x: 0 is the A–C side, 1 the B–D side (MO-4). |
| `morph.y` | Y | 0 to 1, lin | 0 | The pad position's y: 0 is the A–B side, 1 the C–D side (MO-4). |
```

and in D.2's prose, "**Every setting but `morph.seed` and `morph.audible` is a host parameter**, and so are the
position's x and y: they come from…" becomes "**Every setting but `morph.seed` and `morph.audible` is a host
parameter**, the position's `morph.x` and `morph.y` included (round 61, ADR-372; GN-51): they come from…". IF-11's
list then reads "`morph.x`, `morph.y`" where it says "the position's x and y". I checked the rows against the shell's
D.2 parser on a scratch copy: they parse, and the only red is the missing placement.

**Taken from the record, to confirm:** the keys `morph.x` and `morph.y` (no ruling names them), and **the default 0**.
D.2's other defaults are the legacy shell's, and the legacy shell's X and Y (ids 152 and 153) default to 0, so a fresh
patch sits on corner A, the same corner `morph.audible` defaults to. The GUI stand-in starts the puck at (0.5, 0.5),
which would be the other choice. **What a patch save does with the position is still GN-10's open question.** A host
parameter's value has to come back with the project, but whether a saved preset carries it isn't ruled.

## Rulings with no spec rows

- **Ruling 5, mappings per corner (GN-27).** Modulation, so no rows here. Each route keeps a depth per corner, and the
  puck blends between them. **GN-10's state consequence:** four depths per route in the saved state. It belongs to the
  modulation spec (RC-4, ADR-349) and to the morph system's saved document (GN-37), with a 1.0 conflict to close: the
  Map Mode spec keeps routes global for 1.0.
- **Ruling 6, RAMPS dropped (GN-25).** No spec change. The spec already has Morphable and Structural only (5.1, §2's
  glossary). The design master's third kind and the Find pane's pill are the GUI session's to remove.
- **Ruling 7, corner presets (GN-52).** The spec owns state only at RC-11.1, so that's the one place it's written: the
  patch's own corner presets are morph-system state, and the user library isn't any patch's. The menu order and the
  "which?" on save are the shell's. The library's files belong with GN-9's preset seam.
- **Ruling 9, Exempt takes modulation (GN-40).** One sentence in MO-12. The spec already applies per-voice offsets to
  any continuous key's value (PA-2), so it states a consequence rather than a new mechanism. GN-40's other readings (an
  edit moves all four corners, a locked corner refuses it, revert leaves it alone) are the editor's and stay off the spec.

## Open questions for the human

Each is marked `[OPEN: …]` in the spec text. The options are stated in terms of what you'd hear.

1. **Waypoints on a key whose count is Two (MO-23).** At count 2 a key blends at most its two heaviest distinct corner
   values (MO-9).
   - *Waypoints don't act at count Two.* A pinned key set to Two plays the plain two-value blend, and its pins are
     dormant, as they are under Quantum.
   - *Waypoints act on the two-value blend.* The key passes through its pins, and between them it follows the
     two-value blend rather than all four corners.
   - *Waypoints override the count.* A pinned key plays the four-corner blend bent through its pins, and Two applies
     only while it has none.
2. **Waypoints on a circular key (MO-23)**, such as a phase or a window position, which wrap.
   - *None.* A circular key can't be pinned.
   - *Allowed, residual taken the short way round.* A pin bends the key's path by the shortest arc, so a pin at 0.95
     between corners at 0.0 and 0.1 bends it backwards through the wrap, not the long way through 0.5.
3. **Exempt and the stored pins and lock (MO-23, MO-24).** Under Exempt neither does anything you'd hear, so the only
   difference is what you get back.
   - *Kept dormant.* Return the key to Follow and its bends and its region shape come back, as with Quantum (ruling 2).
     The editor lab kept the lock this way.
   - *Cleared.* Back in Follow, the key has no pins and follows the seed's regions. The editor lab cleared pins this
     way.
4. **Save current sound mid-glide (MO-25).** With a long Morph glide the value you hear lags the puck.
   - *What you hear now:* the glided value.
   - *Where the puck is:* the value the glide is heading for. With the default 8 ms glide the two are the same in
     practice.
5. **Save current sound and route modulation (MO-25).** The ruling bakes "offsets".
   - *Macro and X/Y offsets only.* An LFO or envelope wobbling a key isn't frozen into the corner: the corner holds the
     key's centre value, and the wobble carries on around it.
   - *Modulation too.* The corner takes the value at the instant of saving, wherever the LFO happened to be.
6. **Save current sound when a structural setting blends (MO-25; GN-35).** At the puck, a structural setting (a base
   wave, a blade mode, a ratio set) can be sounding as two versions at once, say 60 % saw and 40 % square. One corner
   holds one value (IF-4).
   - *The heavier one.* The corner takes the saw. Alone, it sounds like the heavier version, not the mix you heard.
     It's IF-4's rule for stored values.
   - *Refuse the save* while any structural setting is blending, and say why, so a saved corner is always exactly what
     you heard.
   - *Let a corner hold a mix.* A corner keeps a two-setting sound. That's a new member of the corner and a
     state-format change: the biggest of the three.
7. **Save current sound on an Exempt key with a macro offset (MO-25).** Exempt holds one value in all four corners.
   - *Not written.* The corner keeps the held value, and the offset isn't baked.
   - *Written into all four corners.* The held value moves everywhere, as GN-40 reads any edit to an Exempt key.
   - *Written into this corner alone.* The corners disagree, but the key still plays corner A's value, so you'd hear
     nothing change until it returns to Follow.
8. **The waypoint law and clamp as host parameters (MO-21).** B272 made them a per-patch setting on the Settings page.
   - *Host parameters,* like the other D.2 settings: a song can switch a pinned key's bend shape. The GUI then has to
     place them, as for X and Y.
   - *Page choices,* like a key's mode: saved with the patch, never automated.

## Questions for the lead (not sound)

1. **`started_at`'s clock (IF-10).** See below. The engine's sample count from reset (PA-3), like `VoiceView::start_sample`,
   or samples since the voice's start? Every current use starts its note at sample 0, so no test tells them apart yet.
2. **Who computes a lock's offsets (MO-24, IF-12).** I gave the resolver a read that returns a key's would-be lock,
   since it holds the draw (MO-6). The shell could compute it instead, by implementing MO-6 itself. The same read gives
   Save current sound its values (MO-25), which GN-35 already asks of the resolver.
3. **Item 34 needs new sub-items** for MO-21 to MO-25, and the provenance file `docs/port/spec-engine-provenance.md`
   needs rows for them. Both are outside this draft's scope.
4. **The `[OPEN: …]` markers** might become `[Xn]` tags with section 11 entries, the spec's own convention for open
   items.

## Readings taken from the record, not from the rulings

Each rests on the record cited. None is a guess, but each is worth a glance.

- **"Per setting" in ruling 1 means per key.** In the lab a "setting" is a parameter. In Appendix D a setting is one
  value of a structural group's keys (MO-14), so the spec says "per key".
- **Only Morphable keys hold waypoints.** The editor lab gave pins to continuous rows only, and the master's BlendMaps
  shows only for a continuous row under Blend. Every Morphable key in 5.x is continuous, and Structural keys blend as
  setting sets, where a pinned value has no meaning.
- **Pins are dormant under Stepped.** That's ruling 2. The editor lab (B269) let pins act under Stepped, and the ruling
  replaces that. Under Stepped a pinned key steps through the plain bilinear blend.
- **Clamp to set values is a per-patch switch, on by default.** That comes from B272 ("TPS·Δ with clamp"), GN-7 and the
  master's BlendMaps. The [0, 1] range clamp always applies.
- **Pins hold absolute values.** A corner edit keeps every pin's value, and its residual moves with the base. That's
  the lab's `makeSurface`, and its property "every law passes through every pin" needs it.
- **0.04 spacing, and PA-7's handling of bad values** (non-finite refused, out of range clamped). The spacing is the
  editor lab's guard (`WP_SNAP`, `WP_GUARD`, Euclidean), and the refusal follows PA-7's pattern for host values. Without
  the spacing, TPS·Δ's system can be singular.
- **A lock is per key.** B269's lab locked a "unit" (scale, FX slot, routing) as one, because the legacy shell drew
  one pick per unit. The new spec draws per key (MO-6), so each key locks alone.
- **A locked key's latch is ln(0.55/0.45), not divided by T,** so Temperature can't reach a locked key (B269: a lock
  survives Temperature), and the band stays 0.1 of the pad, as MO-8 states it.
- **A lock is kept where its key blends.** The lab kept it ("locked, dormant here") and no ruling clears it. Only the
  Exempt case was open on B269, and it stays open as question 3.
- **The editor's boundary drag** (B269's rule: δₖ − δₘ = ln wₘ(p) − ln wₖ(p), keeping δₖ + δₘ, offsets within ±40) stays
  with the editor. The resolver only receives the offsets.
- **The waypoint law's math** (MO-22) is the editor lab's block W (`docs/design/morph-editor-lab.html`: `makeSurface`,
  `surfAt`, `tpsSolve`, `triangulate`), with B272's two laws: TPS·Δ and TIN·Δ.

## Where `HeardSetting::started_at` came from

It is the **test author's** name, from `cleanroom-tests` commit `d525a80` (2026-11-04, "Items 34 and 35 (round 48,
ADR-328)…"). It is a field of `HeardSetting` in `engine2/tests/harness/horde_engine.h`, commented "the voice's sample at
which this setting entered its slot (SB-6)". Item 35.13 reads it twice: a merge's new instances must have entered after
the merge's sample, and a respelled `ratios` text (`1, 5/4` to `1,5/4`) must keep its `started_at`, which shows it
restarted nothing. The header came to the lead under Q-TA-74 ("`observe_setting_set` with `HeardSet`"; Q-TA-76 part 2
lists more additions but not this field). Round 57's answers (ADR-364) say "the names and readings stand as you wrote
them". IF-10 never named it. The builder's tree (`cleanroom-engine`) doesn't have it yet. The header comment says "the
voice's sample", and the test compares it with score times, which count on the engine's clock. That's why the clock
is lead question 1.

## FX device (ADR-254)

The FX device has the same morph and pad (ADR-322), so waypoints, locks and Save current sound apply to a hosted
module's continuous rows as written. The 64-per-patch cap counts the FX device's own keys, and an FX slot's structure
(`fx/N`) blends at count 2 (MO-19), so question 6 applies there too.
