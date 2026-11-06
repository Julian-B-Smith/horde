# Spec round 61, draft: the morph's ruled gaps (B631)

> **Origin.** Drafted for horde's lead, 2026-11-06, from ADR-370 / B631: the human's answers to the morph questions
> lab (PR #1423), relayed by the GUI session the same day. A draft for the lead's review. Nothing here has reached the
> clean room. The human answered the draft's eight open questions and the puck's default by poll the same day
> (ADR-373, on #1422), and the lead ruled the two interface readings. Every answer is now in the spec text. A further
> poll (ADR-376) ruled how an oscillator's on switch morphs, that a preset saves the puck, and where a chain blend mixes.
> MO-26 has four `[OPEN: …]` markers, listed under ADR-376 below.

## What changed in `specs/SPEC-ENGINE.md`

| row | change | ruling |
|---|---|---|
| §3 table, §3 tree | The resolver is MO-1 to MO-26 and takes waypoints and region locks. | rulings 1 to 3; ADR-376 |
| IF-10 | The heard setting set names each setting's `started_at`, the sample at which it entered its slot, counted on the engine's clock from reset (PA-3), as a voice's start sample is. That's the lead's reading, for the test author to confirm. | B631 (the lead) |
| IF-12 | New resolver inputs: each Morphable key's waypoints in stored order, the waypoint law and clamp, each key's region lock. A new read that changes nothing: each key's resolved value, and a key's region shape as a lock would take it. | rulings 1 to 3, 8 |
| RC-11.1 | The morph system's own state now lists waypoints, the waypoint law and clamp, region locks, and the patch's own corner presets. The user's corner-preset library is shared by every patch and isn't in any patch's state. | rulings 1 to 3, 7 |
| D.2, IF-11 | The pad's `morph.x` and `morph.y`: two D.2 rows (0 to 1, lin, default 0, the legacy ids 152 and 153), named in D.2's host sentence and in IF-11's list. | ruling 4 (GN-51); ADR-373 item 9 |
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
| D.2 opening line, RC-11.1 | A preset saves the pad's position, `morph.x` and `morph.y`, which stay host parameters. | ADR-376 (GN-10) |
| MO-14, MO-16, MO-20, IF-12, SB-1 (1) | `oscN.on` joins the by-gain groups as `oscN/on`, first in an oscillator's set order, and its on setting renders the oscillator. | ADR-376 |
| MO-9, MO-17 (by MO-16) | An oscillator's keys blend only over the corners where it is on (MO-26). | ADR-376 |
| SB-1 (4), (5) | `sub/chain` and `noise/chain`: each setting renders the whole chain, mixed Σ w·y at the chain's output (the sub's before its gate and level, the noise's at its output), so each corner sounds exactly as itself. PA-5's points are where a one-value flip crossfades, not where a blend mixes. | ADR-376 ("After the drive") |
| **MO-26** (new, D.8) | An oscillator's on switch: the level follows the on-weight; its other keys blend only over its on corners, renormalised; plain blend where it's on in no weighted corner; values land unglided when it comes back on; Stepped, Quantum and per-key modes. | ADR-376; legacy ADR-123, B48, B65, ADR-183, B232 |
| **MO-25** (new) | Save current sound: the glided value heard now; only the macro and X/Y offsets baked, no route modulation; a blending group stores its heavier setting (lower id on a tie), and the morph system reports which groups it couldn't keep as a mix; an Exempt key's pushed value goes into all four corners. | ruling 8; ADR-373 items 5 to 8 |

`engine2/structure/engine-structure.json`: `spec.blob` moved to the new spec's blob. The structure check reads green
with no other change. Round 61 adds no block or edge the file declares: the resolver's inputs aren't drawn there.

## Ruling 4 (X and Y as host parameters) is in the spec

**Landed with their place under the pad.** The first draft held the rows back: alone, they turned `./verify fast` red
(`gen_gui3_controls: RED [unreached] 2 host parameter(s) on no page: morph.x, morph.y`). #1428 (GUI-45 r3) then gave
the Morph page a `side` group under its pad, so this round lands three things together:
- **D.2:** two rows after `morph.glide`, `morph.x` and `morph.y`, each 0 to 1, lin, default 0, with the legacy ids
  152 and 153 named in D.2's defaults sentence;
- **D.2's prose and IF-11:** "the position's `morph.x` and `morph.y`" in place of "the position's x and y";
- **`docs/design/gui3/layout.json`:** the lead's Position group (`side`, tag IF-11, abbreviations X and Y), right after
  the Morph group, with `shell-data.js` regenerated by `tools/gen_gui3_controls.py`.

**Left for the GUI session.** The stand-in (`docs/design/gui3/device.js`) treats the two rows as it treats any D.2
setting. A turn writes `st.dev` as one undo step, and it doesn't move the puck. GUI-45 r3 wants a turn to move the puck
with no undo step, and the knobs to follow a drag of the pad, so the wiring is still the GUI session's.

**D.2's opening line** now says so outright: a preset saves the puck's position, and `morph.x` and `morph.y` stay host
parameters (ADR-376, GN-10). RC-11.1 says the same.

**The default is ruled:** corner A, (0, 0), the legacy shell's default (its ids 152 and 153; ADR-373 item 9). A fresh
patch sits on corner A, the same corner `morph.audible` defaults to. **Still taken from the record:** the keys `morph.x`
and `morph.y`, which no ruling names. **A preset saves the position** (ADR-376: "Yes, saved with it").

## ADR-376 (the human, by poll, 2026-11-06)

**1. An oscillator's on/off fades in with the corner it heads to.** The human, verbatim: "We came up with a special
rule for this in legacy: it fades in, but with the values of the corner it's headed to instead of the arbitrary values
of the off-oscillator in the corner it's coming from." The builder had found `osc1.on` and `osc2.on` in neither MO-14's
groups nor MO-15's list. Q-TA-74 had read them as flip-only.

**The legacy rule, three records:**
- **ADR-123 / B48** (2026-08-26). The on-weight, the bilinear weight of the corners holding the oscillator on, becomes a
  gain ramp through the ~8 ms smoother. The stepped switch flips only at the 10⁻³ floor (−60 dB). The ramp uses plain
  weights "under all laws", and an Exempt switch pins the weight to its live value. `src/hypersaw_clap.cpp`:
  `morphApplyOscEnable`, `morphOnWeight`, `kMorphOnFloor`.
- **B65** (2026-08-27). The human ratified ADOPT: "weighted average of only the on corners sounds right".
- **ADR-183 / B232** (2026-09-24, a revision-2 law). In Blend, a continuous per-oscillator key takes the weights
  w·on/Σ(w·on). There are three fallbacks to the plain blend: no weighted off corner, an Exempt switch, and a live
  weight at or below the floor. A source that was off lands without the morph glide (critic S1). Blend only: Quantum is
  untouched, and B250, the Quantum version of the same problem, was never ruled. `src/hypersaw_clap.cpp`:
  `sourceGateOf`, `morphLiveWeights`, `morphBlendTarget`, and the landing in `morphStep`.

**How MO-26 states it.** `oscN.on` is a by-gain group, `oscN/on`. Its on and off settings at pooled weights W_on and
1 − W_on give the level ramp through the existing set machinery (MO-17, SB-1, SB-2, PA-1), so no new engine input is
needed. The oscillator's other keys, Morphable and Structural, take their weights or candidates only from on corners,
renormalised. That covers MO-17's two-setting law too, which renormalisation doesn't change. Where W_on is 0, or no
corner holds the oscillator on, the plain blend applies, which keeps corners exact.

**Where the record left a choice, marked `[OPEN: …]` in MO-26 (not picked):**
1. **A floor.** The legacy shell switched the oscillator off below W_on = 10⁻³ and read its keys' plain blend there.
   MO-26's set lets it sound at any W_on above 0.
   - *Keep the floor:* the oscillator cuts out below −60 dB.
   - *No floor:* the oscillator fades all the way to silence, on its on corners' values throughout.
2. **The engine's PA-1 glide on landing.** The legacy shell committed the values outright. MO-26 skips MO-5 (b)'s
   glide, but PA-1 still glides every `Param` for about 13 ms. During that time the fade-in, at its quietest, carries
   part of the off corner's values. The options are a no-glide path for these `Param`s, or accepting it.
3. **Quantum.** The legacy shell ramped the level under every law and left the other keys' picks alone (B250's open
   problem).
   - *Keep that:* an off corner's values can sound while the oscillator fades.
   - *Ramp the level, and pick only among on corners:* the oscillator fades in already wearing an on corner's values.
   - *Let `oscN.on` pick like any Structural key:* the oscillator snaps on and off at its boundary, with a 5 ms fade.
4. **Waypoints on an oscillator key** whose oscillator is off in some corner. The pins can bend against the plain
   base, against the on corners' base, or go dormant there.

**Drafting choices, from the spec rather than from legacy, for the lead to check:**
- **Under Stepped,** W_on reads the stepped position (MO-10's "every key"). The legacy shell had no Stepped law.
- **`mix.oscN` counts as one of the oscillator's keys,** because the legacy rule covered the oscillator's own volume
  (id 17/1017).
- **A Quantum-mode `oscN.on` flips by its pick** (MO-13), and the rule still restricts its other keys to on corners.
- **`oscN/on` comes first** in MO-20's per-oscillator set order.

**Not touched, but in the same record:** B203 and B232 put the sub's on switch under the same law ("this is also how the
Sub should work when blend is on"). In the spec today, `sub.on` and `noise.on` sit inside their chains' settings. An
off setting renders nothing, so their structural values from off corners already never sound. Their Morphable keys
(level, tone and the like) still blend over all four corners. Whether MO-26 extends to the sub and the noise is the
lead's to raise.

**2. A preset saves the puck's position** (GN-10): "Yes, saved with it". `morph.x` and `morph.y` are morph-system state
saved with the patch (D.2's opening line, RC-11.1), and they stay host parameters.

**3. "After the drive": a chain blend mixes at the chain's output.** SB-1 items (4) and (5) now say it. In `sub/chain`
and `noise/chain`, each setting renders the whole chain, and the renders mix as Σ w·y at the chain's output: for the
sub, before its gate and level; for the noise, at its output. Each corner therefore sounds exactly as itself. PA-5's
points are where a one-value flip crossfades, not where a blend mixes.

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

Also answered: **the puck's default is corner A, (0, 0)** (item 9), now in D.2's X and Y rows.

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
