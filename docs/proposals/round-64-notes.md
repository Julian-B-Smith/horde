# Spec round 64: the sources' on switches share one pick (ADR-383, B643)

> **Origin.** Written for horde's lead, 2026-11-07, by an agent the lead dispatched. A debug agent (B641,
> `traces/2026-11-07-b641-morph-dropouts.md` on `b641-morph-dropouts`, reproduction
> `tools/engine-preview/morph_hole.cpp`) found that under Quantum, the morph's default law, each source's on switch
> picks its corner by MO-7 on its own (MO-26 (3), cohesion 0.3), so a held chord goes silent for 350 to 560 ms where
> every switch has picked a corner its source is off in: with A = osc 1 only and B = osc 2 only, about 73 % of the
> A–B edge at D.2's defaults. The human ruled by poll: **"Switches pick together"**. The lead records it as ADR-383;
> this round puts it in the spec. DECISIONS.md is the lead's and is not touched here.

## 1. What changed

| file | row | change |
|---|---|---|
| `specs/SPEC-ENGINE.md` | **MO-26 (3)** | The four switches share one **switch pick**: MO-7's at the same weights, scored with the shared draws g* alone (h = 1 whatever `morph.cohesion` is), held by one MO-8 latch. Every switch that resolves by its pick takes the switch pick's corner's value, so they change at one boundary, each by PA-5. The pick and latch resolve at every resolution, whichever switches read them. Exempt switches and fading Follow switches don't join [X91]. A switch holds no region lock [X92]. The restriction to on corners stays, with why its set is never empty. Title cites round 64, ADR-383. |
| | **D.8 intro** | The human's ruling quoted, with its cause (B641). |
| | **MO-7** | The on-switch exception now reads: fades where MO-26 (1) applies, otherwise takes the shared pick. |
| | **MO-13** | The exception sentence: the switches fade under Blend and Stepped, "and otherwise share one pick". |
| | **MO-24** | A source's on switch is no longer a key that picks for a lock: it "holds no lock, MO-26 (3) [X92]". |
| | **34.19** | A new bullet, "The switches pick together", and a must-fail control (§3). |
| | **§11.1** | X91 and X92 added; the count 71 → 73. |
| `engine2/structure/engine-structure.json` | `spec.blob` | → `f61c2d7d0747405e1ef4d84111d318360e6d8966`. The declaration was reviewed against the new text: the switch pick is internal to the `morph_resolver` block, which already cites MO-7, MO-8, MO-24 and MO-26, so no item or edge changes. |

Unchanged on purpose: **MO-27** (at an exact corner every other corner scores −10¹², so the switch pick is that corner, as the row already says); **MO-26 (5)** ("a Quantum-mode switch follows (3) under every law" now carries the shared pick with it); **IF-12** (its "source's restricted pick (MO-26 (2), (3))" is the source's keys' pick, which stays); **MS-25** (an engine key's pick "is that key's own": for a switch that is now the switch pick, by MO-26 (3)); the glossary, §1 and 5.1 (they say only that a switch fades under Blend and Stepped, which still holds).

## 2. The lead's readings, each tagged `[X]` with a §11 entry

- **[X91] Which switches join.** The ruling names the four switches under Quantum. The reading: every switch that
  *resolves by its pick* joins (a Follow switch under Quantum, a Quantum-mode switch under any law). An Exempt switch
  holds corner A's value (MO-12), which the editor has written into every corner, so it neither joins nor breaks the
  "one real corner's pattern" property. A Follow switch under Blend or Stepped fades by MO-26 (1) and does not join.
  **Consequence to know:** under Blend with one switch set to Quantum mode and the others fading, the Quantum-mode switch
  picks alone and nothing guarantees a source sounds near a corner where it is the only one on; the ruling doesn't
  cover a mixed-mode patch, and the minimal reading adds no rule for it. **The latch** resolves at every resolution
  whatever reads it, so a switch that starts to pick (its mode or the law changes) takes the corner already held. The
  alternative, a latch that clears while no switch picks, needs a clearing rule; this one needs none.
- **[X92] Region locks on a switch: refused.** Two options were on the table: a lock on one switch moves the shared pick
  for all four, or locks on switches are refused. Refusal is the minimal one: four switches can carry four different
  locks for one pick, which would need a precedence rule the ruling doesn't give; and a lock replaces the score the
  ruling fixes (g* alone) with its own offsets. Refusal adds no state, and MO-24's existing "refused whole, as PA-7
  refuses" covers the mechanics. IF-12's read of a region shape gives none for a switch.

## 3. The item (34.19, new bullet)

- On A and C = osc 1 only, B and D = osc 2 only, the sub and the noise off everywhere, under Quantum at cohesions 0,
  0.3 and 1 and seeds 1024 to 1033: a sweep of the A–B edge and back leaves some switch on at every boundary.
- On four corners with four different source sets, one empty, a seeded 10,000-boundary walk at cohesion 0.3: each
  switch's resolved value is the switch pick's corner's; the switch pick is MO-7's at h = 1 with MO-8's latch; switches
  change only where the switch pick moves, and there every differing switch changes (the "same boundary" check).
- An Exempt osc 1 switch holds A's value while the other three follow; a lock sent for a switch is refused.
- **Must fail:** each switch picking on its own at `morph.cohesion` (today's rule), which at seed 1024 and cohesion 0.3
  leaves every switch off on part of the A–B edge. That is B641's finding (`preview-morph-hole`: about 73 % of the
  edge at D.2's defaults; 4 of 10 seeds with a hole at 0.3; none at cohesion 1).

## 4. Consequences outside the spec (the lead's to route)

- **The builder.** The resolver keeps one switch pick and one latch for the four switches (scored λ + g*), and refuses
  a region lock on any of them. The source's other keys keep their own picks restricted to on corners.
- **The test author.** 34.19's new bullet and control (§3). `morph_hole.cpp` already is the control's shape; under the
  new rule it should find no hole at any cohesion or seed, and its Blend control is unchanged.
- **The GUI.** The page paints a row's regions from "its own lock, else its seeded draw mixed toward one shared"
  (`docs/design/gui3/device.js:124`). After ADR-383 a switch's regions are the shared draw's alone (cohesion 1 for
  them), and the page should not offer a region lock on a switch. Per the engine-needs rule this wants a row in
  `docs/design/gui3/ENGINE-NEEDS.md`; not written here (out of this round's scope).
- **The FX device** (ADR-254): the ruling concerns the four sources' switches, which an effects device does not have;
  no consequence found. The lead confirms.
- **Saved patches.** A patch whose morph state holds a region lock on a switch loses it on load (refused). Nothing
  released holds one; the test plug-in's page keeps locks in the page (`device.js:109`), not in the engine.

## 5. For the lead to record

- ADR-383: the ruling, MO-26 (3)'s shared pick, and the two readings X91 and X92.
- Clean-room spec sync: the new spec blob is `f61c2d7d0747405e1ef4d84111d318360e6d8966`.
