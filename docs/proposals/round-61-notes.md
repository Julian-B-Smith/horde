# Spec round 61, draft: the morph's ruled gaps (B631)

> **Origin.** Drafted for horde's lead, 2026-11-06, from ADR-370 / B631: the human's answers to the morph questions
> lab (PR #1423), relayed by the GUI session the same day. A draft for the lead's review. Nothing here has reached the
> clean room. The human answered the draft's eight open questions and the puck's default by poll the same day
> (ADR-373, on #1422), and the lead ruled the two interface readings. Every answer is now in the spec text. A further
> poll (ADR-376) ruled how an oscillator's on switch morphs and that a preset saves the puck. **ADR-377 then reshaped
> the round:** for 1.0 every Structural key follows its quantum boundaries, and setting sets are after 1.0. ADR-376's
> third ruling (where a chain blend mixes) is void. **ADR-377 A1** then answered the open items and took the critic's
> fixes: no `[OPEN]` marker remains, and one item is tagged X46 in §11.

## What changed in `specs/SPEC-ENGINE.md`

| row | change | ruling |
|---|---|---|
| §3 table, §3 tree | The resolver is MO-1 to MO-26 and takes waypoints and region locks. | rulings 1 to 3; ADR-376 |
| IF-10 | The heard setting set names each setting's `started_at` (the engine's clock from reset, PA-3: the lead's reading, for the test author to confirm). Since ADR-377 the whole heard-set clause is after 1.0; 1.0 reports each oscillator's heard morph level instead. | B631 (the lead); ADR-377 |
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
| MO-9 | An oscillator's keys blend only over the corners where it is on (MO-26). Count Two's two-value law is now stated in MO-9 itself, since MO-17 is after 1.0. | ADR-376; ADR-377 |
| **MO-26** (new, D.8) | A source's on switch (osc 1, osc 2, the sub, the noise): under Blend and Stepped a morph level g = W_on, glided by PA-1, switching off at the 10⁻³ floor once the heard level reaches it; its keys blend, pick and bend waypoints over its on corners only; under Quantum the switch flips with g = 1; landing without MO-5 (b)'s glide under any law. | ADR-376, ADR-377, ADR-377 A1; legacy ADR-123, B48, B65, ADR-183, B232 |
| **MO-27** (new, D.1) | Corner exactness in 1.0: every key at a corner sends that corner's stored value, levels 1, so the corner equals morph off there. | ADR-377 A1 (critic B3) |
| ADR-377's rows | See the ADR-377 section below for the full list. | ADR-377 |
| **MO-25** (new) | Save current sound: the glided value heard now; only the macro and X/Y offsets baked, no route modulation; a blending group stores its heavier setting (lower id on a tie), and the morph system reports which groups it couldn't keep as a mix; an Exempt key's pushed value goes into all four corners. In 1.0 every Structural key has one value, so item 7 (a blending group) waits for setting sets; a fading oscillator's level is `[OPEN]`. | ruling 8; ADR-373 items 5 to 8; ADR-377 |

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

**How MO-26 states it, after ADR-377.** The first draft carried the level as a by-gain group, `oscN/on`, through the
set machinery. ADR-377 puts sets after 1.0, so MO-26 now has the resolver compute **a morph level g = W_on** per
oscillator and send it as an engine input (IF-12), not a manifest row.
- **Why an input, not a row.** ADR-123 rejected writing the ramp into the oscillator's volume: it would fight the
  volume's own morph target. A new manifest row would be a host parameter and saved state, and a morph output is
  neither (IF-4).
- **How the engine uses it.** g is one more factor on the `mix.oscN` gain, glided by MO-5 (b) on the resolver and by
  PA-1 in the engine. Morph off pins it to 1 (ADR-123).
- **The oscillator's other keys.** Its Morphable keys blend only over its on corners, renormalised. Where W_on is 0 the
  plain blend applies, which keeps corners exact.

**Drafting choices, from the spec rather than from legacy, for the lead to check:**
- **Under Stepped,** W_on reads the stepped position (MO-10's "every key"), so the level moves in steps. The legacy
  shell had no Stepped law.
- **`mix.oscN` counts as one of the oscillator's keys,** because the legacy rule covered the oscillator's own volume
  (id 17/1017).
- **A Quantum-mode `oscN.on` flips by its pick** (MO-13) with g = 1, and the rule still restricts its Morphable keys'
  blends to on corners.
- **The engine reports each oscillator's heard morph level** (IF-4, IF-10), so the GUI and the tests can read it.

**2. A preset saves the puck's position** (GN-10): "Yes, saved with it". `morph.x` and `morph.y` are morph-system state
saved with the patch (D.2's opening line, RC-11.1), and they stay host parameters.

**3. "After the drive": void.** The lead's poll on where a chain blend mixes presumed render mixing, which ADR-377
rules out for 1.0. SB-1 (4) and (5) are back to their earlier text, inside the after-1.0 block. The question returns
with setting sets.

## ADR-377 (the human, 2026-11-06): structure follows its quantum boundaries in 1.0

The human: "I don't want any sounds to crossfade except as brief transitions, temporally-bound. The whole concept is
parametric morph that allows every point to become its own corner preset." And by poll: "For 1.0, anything that can't
blend naturally follows its quantum boundaries. We can work on the waveform morphing after 1.0 and when we start adding
new sound engines, etc." For 1.0 this reverses ADR-317's Blend half.

**How the spec says it.**
- **MO-13, rewritten.** Every Structural key resolves to one value under every law and every mode but Exempt: its pick
  and latch, at the stepped position under Stepped. It changes by PA-5's brief timed crossfade, by PA-6 for `unison`
  and by VO-14 for `voice.mode`. Blend and Stepped blend Morphable keys only. The envelope selectors, the pan pair,
  every chain's keys and the on switches of the blades, the sub and the noise are named, and so is `oscN.ratios`, whose
  parsing and 256-byte rule move here from MO-14. A reserved row is never emitted (Q-TA-74's reading, ADR-364).
- **The one exception** is `oscN.on`, which fades as MO-26's level.
- **MO-20, moved and restated.** It now lives in D.4, after MO-13. The 1.0 resolver emits `Param` events and morph
  levels only, and morph off sends corner `morph.audible`'s values with a level of 1. The old set-ordering text is kept
  word for word as "MO-20's set half, after 1.0" inside D.5.
- **MO-15 and the merge rule** have no 1.0 meaning of their own. Every Structural key now flips, so "flip-only" marks
  nothing apart, and merging only decides which settings render together. Both stay in the after-1.0 block.

**Kept as a marked block, not removed.** D.5 (MO-14 to MO-19) and D.6 (SB-1 to SB-13) stay in place, retitled "after 1.0
(ADR-377)" under a banner that quotes the human and says nothing in them is built or judged for 1.0. Three reasons:
- **The structure check stays green.** `engine2/structure/engine-structure.json` cites MO-14 to MO-20 and SB-2 to
  SB-13, and its gate fails on a cite that doesn't exist.
- **The design keeps its record.** It is the design ADR-317 and ADR-325 ruled, which the human said returns after 1.0.
- **Section numbers don't move.**

The scattered mentions elsewhere were restated or removed instead, so the 1.0 text never leans on the block.

**Rows restated for 1.0:**
- **Early sections.** §1 scope, the glossary's class row (with structural group and setting set marked after 1.0), the
  §3 table and tree, §4's note under the diagram, and 5.1's Class line.
- **Voice and parameter rows.** SW-1, VO-6's table (the structural-settings row removed), PA-1 (the morph level glides),
  PA-5, PA-7 (the level's check; set refusals and counters after 1.0) and RT-1 (no per-setting instances).
- **Interfaces.** IF-3, IF-4 (the level is an input, never state), IF-9 (setting-samples after 1.0), IF-10 (the heard
  set after 1.0; the heard morph level added) and IF-12 (the level input added; the `SettingSet` event after 1.0).
- **Reserved contracts and history.** RC-1's filter, RC-5's FX slot, 7.2's hand-over, RC-10, and RC-11.1, 11.2 and 11.4.
- **Standards and staging.** QS-4 (the set-sweep case removed), PF-4, PF-7, PF-10 (its Blend cells after 1.0) and
  BD-8's stage 7 amendment, which no longer reopens a stage.
- **Acceptance.** Items 34.7, 34.8, 34.9 and 34.15 restated; 34.10, 34.11 and 35 marked after 1.0.
- **Appendices C and D.** C's fade list and its loop-tap note. In D: the intro, MO-1, MO-5, MO-9 (the two-value law
  now inline), MO-10, MO-11 (a Structural key's Count acts on nothing in 1.0), MO-13, MO-20, MO-21, MO-22, MO-24, MO-25
  and MO-26, plus D.2's `morph.count` text.

**Memory (D.6, SB-9).** 1.0 needs no per-setting instances, so SB-9's sizing (four instances per group plus SB-8's
crossfade copies, and up to eight swarm instances per oscillator per voice) no longer applies, and RT-1 and PF-7 drop
it. PA-5's crossfade copies stay: one second setting per oscillator, and a crossfade copy of the sub's and the noise's
chains.

**The structure file's declaration (resolved by A1, S6 below).** `engine2/structure/engine-structure.json` still declares
`voice.structure_slots` (citing SB-2 to SB-13), an edge `morph_resolver->voice.structure_slots` for `SettingSet`
events, and labels the resolver "…setting sets". Those describe after-1.0 structure. Only `spec.blob` moved, as asked.
Whether to drop or mark them is the lead's call: dropping changes the counts and loupe's drawing.

**Generated file:** `morph.count`'s "what it does" text changed, so `docs/design/gui3/shell-data.js` was regenerated.
That's a one-line diff.

**For the GUI session:** a Structural key's Count now acts on nothing in 1.0, so the menu might not offer it there, by
GUI-21's rule. In 1.0 ADR-373 item 7 (Save during a structural blend) can't occur.

**MO-26's open items, re-asked under ADR-377 (all answered by A1 below, except X46):**
1. **A floor (still open).** The legacy shell switched the oscillator off below W_on = 10⁻³. With no floor it sounds,
   and costs CPU, at any W_on above 0.
   - *Keep the floor:* the oscillator cuts out below −60 dB.
   - *No floor:* it fades all the way to silence.
2. **PA-1 on landing (still open).** MO-26 skips MO-5 (b)'s glide, but the engine's PA-1 still glides each `Param` for
   about 13 ms, so the fade-in's quietest moment carries part of the off corner's values. The options are a no-glide
   path for these `Param`s, or accepting it.
3. **Quantum (answered by the lead's reading).** `oscN.on` flips by its pick like any Structural key, with g = 1.
4. **The oscillator's picks (new, replacing the Structural half of old (2)).** Its Structural keys now pick under every
   law, and its Morphable keys pick under Quantum.
   - *Among all four corners,* as the legacy shell's picks did: while it fades in under Blend, an off corner's
     structure (say, its base wave) can be the one you hear.
   - *Only among on corners:* it fades in wearing an on corner's structure.
5. **Waypoints on an oscillator key that is off in some corner (still open).** The pins can bend against the plain
   base, against the on corners' base, or go dormant there.

**New, in MO-25:**
6. **Save current sound while an oscillator fades.** It is heard at level g, which no key holds.
   - *Bake g into the corner's `mix.oscN`:* the corner sounds as the position did.
   - *Store the switch alone:* the oscillator comes back at full level in that corner.

**Superseded by A1 ruling 5 ("Sub and noise too"):** B203 and B232 put the sub's on switch under the same fade law ("this is also how
the Sub should work when blend is on"). In 1.0, `sub.on` and `noise.on` follow their quantum boundaries like their
chains (MO-13). Whether the sub and the noise fade as MO-26 does is a question for after 1.0, not now.

## ADR-377 A1 (the human, by poll, and the critic's review at 94b8d793; 2026-11-06)

**The human's rulings.**
1. **The floor.** A source fading by weight switches fully off below W_on = 10⁻³ (−60 dB), as legacy, with ADR-183's
   plain-blend fallback at the floor (MO-26 (1), (2)).
2. **Picks while fading.** Its Structural keys pick only among the corners where it is on (MO-26 (2); MO-7, MO-24).
3. **Save during a fade.** g is baked into the corner's level key, so the corner sounds as the position did (MO-25).
4. **Waypoints.** Where the source is off in some corners, the pins bend the on corners' blend (MO-22 (1)).
5. **"Sub and noise too."** MO-26 now covers `sub.on` and `noise.on`, each with its own level and its own on-corner
   restriction. The level keys are the manifest's gain rows, `mix.osc1`, `mix.osc2`, `mix.sub` and `mix.noise` (5.2).
   The sub has no `sub.level` row; its gain row is `mix.sub`.
6. **Under Quantum the sources switch.** Under Quantum, and under a switch's own Quantum mode, each switch flips at its
   quantum boundary by PA-5 with g = 1 (MO-26 (3)). The landing applies to a Quantum flip back on, too (MO-26 (4)),
   which fixes legacy B250's defect.

**The critic's blocking findings.**
- **B1.** g is out of MO-5 (b): it glides by PA-1 only (IF-12, MO-26 (1)). The switch's on/off is read from the same
  unglided W_on as g's target, and the engine turns it off only once PA-1's heard g has fallen to the floor.
- **B2.** A load or restore leaves each level unchanged, like the transport. IF-4's and RC-11.4's resend sentences are
  gone. IF-12: prepare and `reset()` set g to 1. MO-3 and MO-20: the first resolution after a resolver reset sends
  every level.
- **B3.** MO-27 (D.1), corner exactness for 1.0. At an exact corner every key sends that corner's stored v (a circular
  key with no atan2 round trip; MO-9 says so too), every level is 1 or the switch is off, and the output is
  bit-identical to morph off with `morph.audible` there. Item 34.18 judges it.
- **B4.** "Except the on switches (MO-26)" is added to 34.9, MO-24 and MO-7's last sentence.

**Should-fix.**
- **S3, S4.** Rulings 1 to 4 close four of the five markers. The fifth, whether the engine's PA-1 also skips a landing
  source's glide, wasn't ruled, so it is **X46** (§11.1, now 25 items). Until it is ruled, PA-1 glides them as any
  `Param`, and the resolver skips MO-5 (b). No `[OPEN]` marker remains.
- **S5.** MO-5 (a) is after 1.0: no 1.0 row reads the glided weights (MO-7, MO-9, MO-22 and MO-26 read the unglided
  ones). Item 34.12's weight clauses are struck.
- **S6.** `engine2/structure/engine-structure.json`:
  - dropped `voice.structure_slots` and its three edges (the `SettingSet` edge, setting-samples to the counters, heard
    sets to the observation view);
  - cleared the "× up to four … settings" multiplicities on `member.base_wave`, `member.blade1`, `member.blade2`,
    `sub` and `noise`, and trimmed `osc`'s to "2: osc 1 and osc 2";
  - relabelled the resolver and cited it to MO-1 to MO-13 and MO-20 to MO-27;
  - added three control edges, `morph_resolver->voice.osc_level`, `->voice.sub_level` and `->voice.noise_level`.

  **Counts:** blocks 67 → 66; edges 117 → 117 (control 47 → 49, observation 11 → 9). The structure check is green.
- **N9.** MO-22 (4) now states the triangulation's tests: counter-clockwise storage by orient, the in-circle
  determinant with its 10⁻¹³ threshold, and the 10⁻¹⁴ edge test, in double precision as the editor lab computes them.
- **N1 to N8: not addressed.** The critic's report never reached this session; only N9 was named in the brief. Please
  send N1 to N8 and they'll be fixed in a follow-up commit.

**For the lead to record (S7): the drafter's unruled choices, plainly.**
1. **A morph level is an engine input, not a manifest row** (IF-12, MO-26 (1)). It is a factor on the level key's
   gain and is never state.
2. **For the noise, g applies after Erode's tap** (SN-14), so the sub's Erode never hears it.
3. **A source's keys** are its prefix's keys plus its level key. `mix.oscN` is in, because legacy's rule covered the
   per-oscillator volume (ids 17/1017).
4. **Under Quantum, while a source is on and some weighted corner has it off, all its keys pick only among its on
   corners** (MO-26 (3)). The lead's "arrive at the picked on-corner's values" was read this way.
5. **Save's baking below −60 dB** (MO-25): where g times the heard gain falls below the dB law's −60 dB, the key stores
   the nearest value the law holds (−60 dB), not −∞.
6. **The waypoint clamp's hull** (MO-22 (6)) stays over all four corners' values and the pins, as the editor lab's
   block W computes it, even when the base is the on corners' blend.
7. **Under Stepped,** W_on reads the stepped position, so the level moves in steps.
8. **A Quantum-mode switch under Blend or Stepped** still restricts its source's Morphable keys to its on corners while
   it is on.
9. **Each source reports its heard morph level** (IF-4, IF-10).
10. **MO-27's number and place:** a new row at the end of D.1, since SB-10 is after 1.0.
11. **The structure file:** the after-1.0 slot item was dropped rather than marked, because the format has no marker
    for it.
12. **The X46 wording,** and what is built until it is ruled.
13. **A reserved row is never emitted** (MO-13). This is Q-TA-74's reading, ruled by ADR-364, placed here.

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
   which groups it couldn't keep as a mix. A corner never holds a mix, and IF-4 is unchanged (MO-25). Since ADR-377 this
   applies only after 1.0, when setting sets return: in 1.0 no structural setting blends.
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
settles the clock on PA-3's count, for the test author to confirm. Since ADR-377 the heard set, `started_at` included,
is after 1.0, so 1.0's tests don't read it.

## FX device (ADR-254)

The FX device has the same morph and pad (ADR-322), so waypoints, locks and Save current sound apply to a hosted
module's continuous rows as written. The 64-per-patch cap counts the FX device's own keys. In 1.0 an FX slot's structure
follows its quantum boundaries like any Structural key (MO-13, RC-5). After 1.0 an `fx/N` blend at count 2 (MO-19)
brings back Save's heavier-setting rule (ADR-373 item 7).
