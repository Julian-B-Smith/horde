# Spec round 67: FX in the morph, in the patch and on real sends, ruled (ADR-410, ADR-411, B713)

> **Origin.** Written for horde's lead, 2026-11-25, by an agent the lead dispatched (B713). The human ruled ADR-410 on
> 2026-11-25 by poll: "Per corner, like engine", "Dry: FX off", "Real levels per corner" and "Test plug-in first". The
> lead drafted it as round 67 (D.9, MO-28 to MO-31; X101 to X124, all PROPOSED) beside the rack-slot contract's §12.10
> and §10's Q14 to Q22. On 2026-11-25 the human ruled the round's sixteen open points (ADR-411): four in the human's own
> words and twelve as one, "Take all 12 picks". This pass moves round 67 from PROPOSED to ruled and resolves each X item.
> DECISIONS.md and `engine2/structure/engine-structure.json` are the lead's and are not touched here. The contract's
> rows were carried by the lead into §12.11; a later review pass (§6) fixed both texts where they disagreed.

## 1. What changed

| file | row | change |
|---|---|---|
| `specs/SPEC-ENGINE.md` | **D.9's heading, note and opening** | PROPOSED becomes ruled (ADR-410, ADR-411). The note says every row is built as written and its [X] tags are the readings still open. The opening quotes ADR-411's four rulings in the human's words and "Take all 12 picks". |
| | **MO-28** | Not PROPOSED. Tags drop X120. Device-class keys are stored once per patch, outside every corner (MO-31). New: a module key's declared `law` is its one law field, so a `log` key morphs geometrically in its unit (X123 (a)). |
| | **MO-29** | Not PROPOSED; tags [X103, X104]. **(3)** now resolves by the **slot pick**, built both ways: **Alone** (built as the default until X103 is ruled, the human's lean: own pick and latch) and **Wire set** (the value at the wire set's pick, held by its latch). Both picks resolve at every resolution, so changing the setting moves only the switches that differ. A load sets the slot levels and mirrors to 1 (X115 (a)). Save stores the resolved switch, keys and blobs, and nothing for g (X121 (a)). An off slot is true bypass, and g crossfades the module's output against the bypassed input (RC-5.2; X124 (a)). |
| | **MO-30** | Not PROPOSED; tags [X105, X109]. The rack's input is one stereo pair in 1.0 (Q15 (a)). The wire set as one pick is ruled (Q20 (a)), and a pick per wire is not built. An appearing wire's level lands (X116 (a)). What sounds at a set change is RC-5.3's (X122 (a)). What a back edge carries is RC-5.1's. |
| | **MO-31** | Not PROPOSED; tags [X107, X108, X113, X125, X126]. **Device settings (new, Q21):** stored in `fx` once per patch. A load that finds a difference keeps the running values, reports each module, key, stored value and running value, and applies the stored values only on an approval input. A decline keeps the running ones. Tagged [X126], the lead's readings: another load first keeps them too; a document with no Device values differs in nothing; an approved latency-changing value is applied by restart (RS-7.1, ADR-344); a project restore applies them silently. A load sets every morph level and mirror to 1 (X115 (a)). Off slots are true bypass. **Corner-preset loads (new, Q18 (a))** replace that corner's FX, and a corner preset with no FX loads that corner dry [X125]. **The chunk's ceiling** is set after B612, before S6 (Q19 (b)). |
| | **RC-5** | Not PROPOSED. *Deferred:* one input pair in 1.0, with per-source taps once their CPU is measured (Q15 (a)). *Must provide:* Device keys follow MO-31's approval rule (amending RS-4.5); X124 (a), X114 (b), true bypass; the wire-set change and feedback point to RC-5.3 and RC-5.1. |
| | **RC-5.1 (new)** | Feedback behind `device.fxFeedback`, Allow or Prevent (MS-47's form: device-level, outside every patch, never a host parameter; tagged [X127]). A back edge is `routing_core.h`'s non-forward edge. Allow reads it one sample late (RS-3.5, ADR-128). Prevent holds every back edge at zero where it is consumed (B265 round 3 (b)), whether or not it closes a loop, so it also removes GUI-41's crossfade overlap (tagged [X109]). A runaway guard latches, counts and reports (`feedback_runaway`). B714 measures the CPU of a live cycle. The row has checks and a must-fail. Prevent is built as the default until X109 is ruled. |
| | **RC-5.2 (new)** | "Off is true bypass" (Q16 (a), GUI-18): an off slot's output is its input, with no module and no I/O gain, the same for every module. The fade law is X124 (a). Host-parameter status stays RS-5.6 (product) and Q17 (b) (test plug-in). The row has checks and a must-fail. |
| | **RC-5.3 (new)** | A wire-set change (X122 (a)): a brief equal-gain crossfade, with each removed wire scaled by 1 − a and each added wire by a, except that tails ring out (RS-3.7): a removed wire leaving a slot the new set leaves with no input is not scaled and keeps its old level until that slot's output reaches exact zero (the contract's A5 test). The row has checks and a must-fail. |
| | **IF-4, IF-12** | A load now sets each source's morph level to 1, as prepare and reset do; a restore still leaves it (X115 (a)). IF-12 gains the slot-pick call (Alone or Wire set, from the next boundary, never saved; tagged [X103]). Its slot level is reset at a load too. Tags drop X115. X114 and X121 are cited as ruled. |
| | **IF-13, MS-20, MS-25** | X114 is cited as ruled (b), and no longer as a tag. |
| | **MO-3, MO-6, MO-7, MO-9, MO-13, MO-20, MO-25, MO-26, §1, RC-11.1, BD-8, Appendix D's opening** | PROPOSED is dropped. MO-7 and MO-13 say a slot's switch takes its own pick or the wire set's, by the slot pick. MO-20 cites X116 (a), MO-25 X121 (a). RC-11.1 lists the Device settings in `fx`. |
| | **Item 40** | Ruled, so it is tested. Default: the slot pick is Alone. **40.2** gains the Wire set check and its must-fail. **40.3** cites X116 (a). **40.4** gains the load's Blend half (X115 (a)), corner presets (Q18 (a), X125) and Device settings (Q21), with three more must-fails. **40.6** cites X114 (b). |
| | **§11.1** | Removed as ruled: X106, X110, X111, X112, X114, X115, X116, X117, X120, X121, X122, X123, X124 (13). Rewritten: X103, X109 and X113. PROPOSED dropped from X101, X102, X104, X105, X107, X108, X118 and X119; X107 and X119 updated. Added: X125, X126 and X127. The count goes from 99 to 89. |
| `engine2/structure/engine-structure.json` | `spec.blob` | **Not touched (the lead's).** It still pins `2115c762…`. The new text's blob is `1ec7d68ff003b37c48a0b18aaf2167945c97fece`, and `./verify` reports `spec-changed` until the pin moves. The declaration needs reviewing for the slot-pick input on the `morph_resolver` block. |

## 2. The rulings, as the spec now carries them

| item | ruling (ADR-411) | where |
|---|---|---|
| Q14 / X109 | Feedback is built behind a settings toggle that prevents it ("there should at least be a toggle to prevent it in settings"). Its default and 1.0's policy wait on listening in B714. | RC-5.1; X109 open for the default only |
| Q16 / X111 | "Off is true bypass": the module's own I/O gains go too, the same for every module. | RC-5.2, MO-29, MO-31 |
| Q22 / X103 | Deferred to listening. Both behaviours are built: each slot alone (the human's lean, built as the default until ruled), or following the wire set's pick. | MO-29 (3), IF-12, 40.2; X103 open |
| Q21 / X120 | Device settings are stored, and an approval screen asks at a load where they differ. A decline keeps the running ones. | MO-31, RC-5, 40.4 |
| Q15 / X110 | (a) One engine input in 1.0. Per-source taps come once their CPU is measured. | RC-5, MO-30 |
| Q17 | (b) The test plug-in keeps host ids generated from each manifest. This is the test plug-in only; RS-5.6 stands for the product. | RC-5.2 (cited); outside the spec otherwise |
| Q18 / X112 | (a) A corner-preset load replaces that corner's FX. | MO-31, 40.4 |
| Q19 / X113 | (b) The ceiling is set after B612's host save test, before S6. | MO-31; X113 open for the figure |
| Q20 / X106, X117 | (a) One pick for the whole wire set. | MO-30 |
| X114 | (b) Modulation is summed in the rack, and its route follows the key's own pick. | MS-20, MS-25, IF-13, RC-5 |
| X115 | (a) The fade levels reset at a load. | IF-4, IF-12, MO-29, MO-31, 40.4 |
| X116 | (a) An appearing wire's level lands. | MO-30, MO-20, 40.3 |
| X121 | (a) A save stores the resolved switch and settings, and nothing for g. | MO-25, MO-29 |
| X122 | (a) A brief equal-gain crossfade at a wire-set change, and tails ring out. | RC-5.3, MO-30 |
| X123 | (a) One `law` field. | MO-28 |
| X124 | (a) The slot fades by crossfading the module's output against its bypassed input. | RC-5, RC-5.2, MO-29 |

## 3. The readings in this pass, each tagged `[X]` with a §11 entry

- **[X103] Both pick behaviours as one resolver input.** The task says the builder builds both, and the resolver is the
  clean room's (IF-12), so the switch is a resolver call, not a B714-only setting. ADR-411's consequences put "the pick
  switch" in B714's slot library; B714 can expose the resolver's call. The lead's no-dead-end reading is in X103's row,
  open to overrule. Alone as the default, and the call's being never saved or a host parameter, are tagged there too.
  **One edge the lead should see:** under Prevent, a corner set whose only path from `in` to `out` runs through a back
  edge (for example `in` to 2, 2 to 1, 1 to `out`) is cut at play. A write-time "connects `in` to `out`" check that
  counts back edges would pass it. No row states such a check, so X103 and the contract (RS-4.8, Q22, §12.11) now word
  the no-dead-end reading as conditional on one that counts a back edge only under Allow. ADR-411's own reading
  ("refused when the wire set is written") needs the same rewording; DECISIONS.md is the lead's.
- **[X109] What is built until the default is ruled.**
  - The default is Prevent, which matches GUI-41's earlier direction and is the safe side.
  - A back edge is defined statically, as `edgeForward` false. So Prevent also holds a back edge that closes no loop,
    since the rack runs no cycle check (ADR-088). The row says so.
  - Prevent also removes GUI-41's crossfade overlap: a loop that only a wire-set change's crossfade assembles is held
    too. The contract's RS-3.5, RS-3.8 and §12.11 now say the same, in place of their earlier "a loop's back edge" and
    "lives for that crossfade".
  - The runaway ceiling is PROPOSED, a figure B714 states. The latch clears at the rack's next reset (a load counts).
  - None of this is ruled.
- **[X127] Where the feedback setting lives (new).** It is named `device.fxFeedback` and is stored the way MS-47 stores
  its setting (device-level, X85's form): outside every patch, so it does not travel with one, never a host parameter
  and never a morph key, a change applying from the rack's next block boundary. The name is new. The contract's RS-3.5
  carries the same reading.
- **[X113] Still open.** Q19 (b) rules *when* the ceiling is set, not its figure, so the item stays in §11.1 with the
  timing it now has. If the lead counts the timing as closing it, the count becomes 88.
- **[X125] A corner preset with no FX loads that corner dry.** This carries ADR-410 r2's "Dry: FX off" to Q18 (a). Every
  corner preset in a user's library predates round 67, so loading one now switches that corner's FX off.
- **[X126] The Device settings outside the screen's plain case.** A project restore applies them silently (the lead's
  reading in ADR-411); a document with no Device values differs in nothing; another load before an answer keeps the
  running ones; an approved latency-changing value is applied by restart. The row carries all four, as the contract's
  RS-4.5 marks them P.
- **Not tagged, but the lead should know:**
  - RC-5.3 makes the crossfade per wire (removed wires scaled by 1 − a, added ones by a). Crossfading two whole renders
    would need a second instance of every module. Tails follow RS-3.7: a removed wire leaving a slot the new set leaves
    with no input keeps its old level until that slot's output reaches exact zero. The contract's RS-3.2 now marks the
    per-wire form P, the lead's reading of X122 (a).
  - RS-6.5's spill-over setting is not applied to a wire-set change.
  - X116's second half (a wire touching a landing slot is not landed by it) is taken as part of (a), as round 67 wrote
    it.

## 4. The open X list after this pass (§11.1, round 67's items)

Of round 67's, eleven stay open, and none of them blocks the build:
- X101, X102, X104, X105, X107, X108, X118 and X119 are the lead's readings, built as written;
- X103 is the slot pick, both behaviours built;
- X109 is the feedback default, and what Prevent holds;
- X113 is the ceiling's figure.

Three are new: X125, X126 and X127. §11.1's whole count is 89, Q items included.

## 5. Consequences outside the spec (the lead's to route)

- **The clean room** (both repos, the same turn).
  - The resolver gains the slot-pick call and its Wire set behaviour (40.2).
  - The engine now sets each source's morph level to 1 at an IF-4 load (X115 (a)). Before, a load left it unchanged, so
    any test pinned on that must move.
  - 40.4 gains its Blend half, corner presets and Device settings.
  - The new spec blob is `1ec7d68ff003b37c48a0b18aaf2167945c97fece`.
- **B714 (the slot library).**
  - RC-5.1: both feedback states, the runaway guard and its ceiling figure, and the CPU of a live cycle.
  - RC-5.2: true bypass.
  - RC-5.3: the wire-set crossfade and tails.
  - MO-31's Device-settings approval flow on the bridge.
- **The contract:** done. §12.11 carries the rulings into RS-3.1 to RS-3.3, RS-3.5, RS-3.8, RS-2.9, RS-4.2, RS-4.5,
  RS-4.8, RS-5.6, RS-6.12, RS-6.15, §6.2, A7, A11 and §10's cells for Q14 to Q22.
- **ADR-411** (DECISIONS.md, the lead's) wants two corrections this pass could not make: its Engine impact lists "the
  pick switch" under the slot library, where the spec makes it the resolver's slot-pick call (IF-12, MO-29 (3)),
  exposed by B714; and its no-dead-end reading says a corner whose wires do not connect `in` to `out` "is refused when
  the wire set is written", which no row states (§3, X103).
- **The GUI session** (ADR-336): the approval screen; `device.fxFeedback` in settings; feedback wires and the slot-pick
  switch in the routing lab (GB21). Each wants a row in `docs/design/gui3/ENGINE-NEEDS.md`.
- **FX device (ADR-254):** the same. It carries RC-5.1's setting, true bypass, the stored Device settings and the slot
  pick. Per-source inputs stay instrument-only.

## 6. Review pass (after §12.11)

A fresh review found eleven points where the contract, the spec, these notes and the FOUNDATIONS brief disagreed or
left a reading untagged. Fixed in those four files:
- **What Prevent holds:** every back edge, statically, in both texts (RC-5.1; RS-3.5, RS-3.8, §12.11), tagged X109.
- **RC-5.3:** the tail exception no longer contradicts the per-wire scaling; its check is rewritten; "quiet" is A5's
  exact zero. The contract's RS-3.2 describes the same per-wire crossfade, marked P.
- **§12.11's "§12.10's readings" row:** X111 ruled, X113's timing ruled and its figure open, X125 to X127 listed.
  RS-6.15 states X125.
- **RC-5.1's setting** tagged X127; the guard's latch and Prevent as the interim default tagged X109.
- **Alone** marked as the interim default (MO-29 (3), IF-12, X103, RS-4.8); the call's being never saved tagged X103.
- **The no-dead-end reading** reworded as conditional on a write-time check no row states.
- **RS-2.9's law values:** Sluice's `sq` and `bool` and the engine's `pow` and `dB` named, and the closed-set question
  pointed at the brief.
- **The brief's §2.4:** the restore half labelled as the lead's reading.
- **MO-31 and RS-4.5** agree: a patch with no block, another load before an answer and an approved latency setting are
  readings, tagged X126 and marked P.
- **Stale prose:** §10's intro, and §5 above.
