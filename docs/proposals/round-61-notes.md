# Spec round 61, draft: the morph's ruled gaps (B631)

> **Origin.** Drafted for horde's lead, 2026-11-06, from ADR-370 / B631: the human's answers to the morph questions
> lab (PR #1423), relayed by the GUI session the same day. A draft for the lead's review. Nothing here has reached the
> clean room. The human answered the draft's eight open questions and the puck's default by poll the same day
> (ADR-373, on #1422), and the lead ruled the two interface readings. Every answer is now in the spec text, which has no
> open markers left.

## What changed in `specs/SPEC-ENGINE.md`

| row | change | ruling |
|---|---|---|
| §3 table, §3 tree | The resolver is MO-1 to MO-25 and takes waypoints and region locks. | rulings 1 to 3 |
| IF-10 | The heard setting set names each setting's `started_at`, the sample at which it entered its slot, counted on the engine's clock from reset (PA-3), as a voice's start sample is. That's the lead's reading, for the test author to confirm. | B631 (the lead) |
| IF-12 | New resolver inputs: each Morphable key's waypoints in stored order, the waypoint law and clamp, each key's region lock. A new read that changes nothing: each key's resolved value, and a key's region shape as a lock would take it. | rulings 1 to 3, 8 |
| RC-11.1 | The morph system's own state now lists waypoints, the waypoint law and clamp, region locks, and the patch's own corner presets. The user's corner-preset library is shared by every patch and isn't in any patch's state. | rulings 1 to 3, 7 |
| D intro, MO-1, MO-3 | Waypoints and locks are resolver inputs and morph-system state. | rulings 1 to 3 |
| MO-4 | The waypoint law also reads the position itself, not only the weights. | ruling 1 |
| MO-5 | (b)'s equivalence with gliding the weights holds only while a key's waypoints aren't acting. | ruling 1 |
| MO-7 | A locked key scores and latches by MO-24. | ruling 3 |
| MO-9 | A key whose waypoints act takes MO-22's value. | rulings 1, 2 |
| MO-12 | Waypoints and a lock don't act under Exempt. Exempt concerns the morph only: the held value takes per-voice modulation wherever PA-2 gives its key modulation. | ruling 9 (GN-40) |
| **MO-21** (new) | What a key holds: Morphable keys only, and no circular key; 8 per key, 64 per patch, dormant ones counted, stored in order; 0.04 spacing; the list refused whole past a limit; the waypoint law (TPS·Δ default, TIN·Δ) and Clamp to set values (on by default), page choices saved per patch and never host parameters. | ruling 1; B272; ADR-373 items 2, 4 |
| **MO-22** (new) | The waypoint law as math: the bilinear base, residuals at the nodes, TPS·Δ (thin-plate spline) and TIN·Δ (Bowyer–Watson from the A–D split, in stored order), corners read and never computed, the two clamps. | B235, B272 |
| **MO-23** (new) | When waypoints act: morph on, law Blend, key mode Follow, count Four. Dormant under Quantum or Stepped, under a key's own Quantum, under Exempt and at count Two, and active again when the key blends all four corners under Blend. A corner edit keeps every waypoint's position and value. | ruling 2; ADR-373 items 1, 3 |
| **MO-24** (new) | The region lock: three offsets, the score ln w + δ, which temperature, cohesion, the seed and the draws don't reach; the latch; locking as is, computed by the resolver; a corner edit changes no lock and its new value plays in its region; Exempt keeps the lock dormant. | ruling 3; B269; ADR-373 item 3 |
| **MO-25** (new) | Save current sound: the glided value heard now; only the macro and X/Y offsets baked, no route modulation; a blending group stores its heavier setting (lower id on a tie), and the morph system reports which groups it couldn't keep as a mix; an Exempt key's pushed value goes into all four corners. | ruling 8; ADR-373 items 5 to 8 |

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
parameter**, the position's `morph.x` and `morph.y` included (round 61, ADR-373; GN-51): they come from…". IF-11's
list then reads "`morph.x`, `morph.y`" where it says "the position's x and y". I checked the rows against the shell's
D.2 parser on a scratch copy: they parse, and the only red is the missing placement.

**The default is ruled:** corner A, (0, 0), the legacy shell's default (its ids 152 and 153; ADR-373 item 9). A fresh
patch sits on corner A, the same corner `morph.audible` defaults to. **Still taken from the record:** the keys `morph.x`
and `morph.y`, which no ruling names. **What a patch save does with the position is still GN-10's open question.** A host
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

## The human's answers (ADR-373, 2026-11-06)

The draft's eight open questions, as answered by poll, and where each now stands in the spec:

1. **Waypoints at count Two are dormant:** the plain two-value blend plays (MO-23).
2. **A circular key (law `lin, wraps`) takes no waypoints** (MO-21: a list naming one is refused whole).
3. **Making a key Exempt keeps its pins and lock dormant,** and they return with Follow (MO-23, MO-24).
4. **The waypoint law and Clamp are page choices,** saved per patch and never host parameters (MO-21). So they aren't
   D.2 rows, and the GUI places no host knob for them.
5. **Save during a morph glide stores the glided value heard now** (MO-25).
6. **Save bakes only the macro and X/Y offsets.** LFO and envelope modulation isn't frozen into the corner (MO-25).
7. **Save during a structural blend stores the heavier setting** (the lower id on a tie), and the morph system reports
   which groups it couldn't keep as a mix. A corner never holds a mix, and IF-4 is unchanged (MO-25).
8. **Save on an Exempt key with a macro offset writes the pushed value into all four corners** (MO-25).

Also answered: **the puck's default is corner A, (0, 0)** (item 9), in the staged X/Y rows above.

## Questions for the lead (not sound)

1. **`started_at`'s clock (IF-10), ruled by the lead's reading:** the engine's sample clock from reset (PA-3), as
   `VoiceView::start_sample` counts. The spec marks it "the lead's reading, for the test author to confirm", since every
   current use starts its note at sample 0 and no test tells the two clocks apart yet.
2. **A lock's offsets are the resolver's to compute (MO-24, IF-12), the lead's ruling.** The read stays as drafted, and
   it also gives Save current sound its values (MO-25), which GN-35 already asks of the resolver.
3. **Item 34 needs new sub-items** for MO-21 to MO-25, and the provenance file `docs/port/spec-engine-provenance.md`
   needs rows for them. Both are outside this draft's scope.
4. **No open markers are left,** so no `[Xn]` tag or section 11 entry is needed for this round.
5. **The morph system's report on Save** (ADR-373 item 7: which groups it couldn't keep as a mix) is the shell's to
   show. The spec states only that the report exists.

## Readings taken from the record, not from the rulings

Each rests on the record cited. The lead let them stand as written (2026-11-06).

- **"Per setting" in ruling 1 means per key.** In the lab a "setting" is a parameter. In Appendix D a setting is one
  value of a structural group's keys (MO-14), so the spec says "per key".
- **Only Morphable keys hold waypoints.** The editor lab gave pins to continuous rows only, and the master's BlendMaps
  shows only for a continuous row under Blend. Every Morphable key in 5.x is continuous, and Structural keys blend as
  setting sets, where a pinned value has no meaning. ADR-373 item 2 then takes out the circular ones.
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
- **A lock is kept where its key blends.** The lab kept it ("locked, dormant here") and no ruling clears it. B269's one
  open case, Exempt, is now ADR-373 item 3: kept dormant.
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
voice's sample", and the test compares it with score times, which count on the engine's clock. The lead's reading
settles the clock on PA-3's count, for the test author to confirm.

## FX device (ADR-254)

The FX device has the same morph and pad (ADR-322), so waypoints, locks and Save current sound apply to a hosted
module's continuous rows as written. The 64-per-patch cap counts the FX device's own keys, and an FX slot's structure
(`fx/N`) blends at count 2 (MO-19), so Save's heavier-setting rule (ADR-373 item 7) applies there too.
