# b294-scalpel-lab-round3 — the phase ring becomes the Coupling × Detune XY, the waveform XY's defaults swap, and cross-mod is drawn

- **Queue item:** B294. The row is carried in the records PRs on branches `lead-records-102` (#776) and `lead-records-103` (#777, the added scope). I read it verbatim from `origin/lead-records-103:ROADMAP.md`. The human, 2026-09-27:
  1. "Maybe the phase ring visualizer also can become the surface for the coupling/detune XY to preserve space."
  2. Added mid-task: "Maybe we should swap the width and position dragging after all; it's less inuitive this way."
  3. Added mid-task: "can we visualize the cross-mod?"
- **Why:** This is round 3 of B271's lab, `docs/design/scalpel-interface-lab.html`, and it answers exactly those three asks. The design language, the tokens and round 2's conventions are unchanged.
- **What changed:** one file, the lab (`lab-review` meta `B271 + B293 + B294 · 2026-09-27`), plus `docs/design/index.html` regenerated.
  1. **The ring is the Coupling × Detune XY** (section I3).
     - **Gestures.** The drag is relative: a grab moves nothing, and alt makes it ¼. The ring's plane is the pad's plane: x = Detune, y = Coupling (B227's reading), each across the whole canvas along its own taper. A whole canvas width (or height) is a whole taper, so the crosshair moves exactly with the hand. The pad needed 150 px for the whole Coupling range; the ring needs about 250 px, so it is slightly less sensitive.
     - **Mapping.** ⇧ + horizontal defaults to Cross-mod, which is the lab's proposal. The mapping row is round 2's `xyMapRow` with the same roles.
     - **Blade roles.** A blade role mapped on the ring moves the blade whose arc is under the pointer. Inside the circle, it moves every blade that is on.
     - **One code path.** Both surfaces run one gesture (`xyGrab`/`xyMove`/`xyEnd` with a surface argument; `span` holds each surface's taper length) and write through `writeMany`.
     - **The marker** is drawn UNDER the members, the arcs and the R needle:
       - a faint magenta crosshair (the value ink at 0.2) and a hollow puck;
       - magenta ticks on the left and bottom edge rails;
       - the pad's corner labels: lock, splay, detune →;
       - the K = 0 boundary as two edge ticks;
       - Coherence moved to the top-right corner.
     - **Modulation:**
       - a HOLLOW violet diamond at the carried value, joined to the puck by a dashed line (a filled violet dot would read as a member);
       - violet rail segments from the value to the carried value (the knob halo, once per axis);
       - a dashed violet reach box for how far the bound macros can carry it, which is B227's recommended reach box.
       - There is no trail: nothing in this lab moves Coupling over time, and a trail would need a wall-clock history.
     - **The readout** is a DOM line under the ring. It is live while dragging, so it never covers the members.
     - **OSC:** the pad cell (180 px) is gone. The ring went from 158 to 250 px and the spectrum from 104 to 132 px, and the ring's tools, readout and legend lines take the rest. When the mapping row is open, the spectrum drops to 44 px, so Advanced stays in the window.
     - **MAIN:** MAIN had its own pad in Osc Controls and a duplicate mini ring in its left Visualizers cell. The pad's slot became the ring: it is still the K × Detune XY the human's MAIN wireframe puts there, and now also the mini visualizer the wireframe puts beside it. The left cycle view takes the whole width (132 × 116 → 254 × 150). Both pages share one ring mapping.
  2. **Waveform XY defaults swapped:** ↔ Position (it wraps), ⇧↔ Width, ↕ Cut rate. The mapping stays settable. I updated the pill, the reset tooltip, the page text and round 2's checks. The ring's defaults are independent of this change.
  3. **Cross-mod drawn from the oracle's law.**
     - **The law:** `razor-core.js` render sets xin = ½·xm·y_{q+1}[n−1] (plus a feedback term). voice() adds it to the Sync, FM and Ring carriers only (cases 0/1/2/5), and only inside a window.
     - **The estimate:** `cycleFrom` runs the bench's own three-pass static estimate (`scalpel-bench.html` computeCycle; SPEC §10 calls it a display approximation, and the audio is exact). It uses the neighbour's phase offset from the viz feed and leaves the feedback term out.
     - **The arrows:** meter ink, from member i+1 to member i. Each is weighted by the RMS of what that push adds to member i (`XM_FULL` = 1). A push with no effect is a dotted hairline. Each arrow bows toward the centre, so it stays visible when a locked swarm stacks two members.
     - **The lane:** a dash-dot CONTRIBUTIONS line on the cycle view. The heard trace now includes the estimate. Feedback is still not drawn.
     - **The legend:** a line under the ring reads "Cross-mod off." at xm 0.
     - The arrows were magenta in round 1. They moved to the meter ink because they now show a measurement.
- **Evidence consulted:**
  - ROADMAP B227, B271, B293, B294 (both records branches).
  - The B293 trace.
  - The lab, sections B, C, D, F, H, I, I2, K.
  - `reference/scalpel/prototype/razor-core.js:175-200` (voice, where xin enters) and `:720-735` (render, the push).
  - `reference/scalpel/prototype/scalpel-bench.html:1920-1985` (computeCycle, the three-pass estimate).
  - `specs/SPEC-SCALPEL.md` §6 (line 138) and §10 (line 223).
- **Verified vs entailed:**
  - **VERIFIED, in-page self-check 24/24, 6 of them controls that must fail.** Read from headless Chrome via DevTools on the committed lab. Round 2's 17 still pass, with its XY round trip rewritten for the swapped defaults: c 0.155 (wrapped), w 0.3402, k 7.387, and the oracle agrees. New:
    - **ring: the grab alone moves nothing.**
    - **ring round trip:** Detune 30.9615 = expected, Coupling 0.4000 = expected, and the oracle's `t` agrees. The marker moved 28.50, −25.00 px for the hand's 28.50, −25.00.
    - **ring remap:** Width + inverted Stereo gives w and w2 each +26/260 of their taper and Stereo 0.400. Coupling and Detune hold.
    - **ring modulation:** with the Swarm macro at 0.5, the diamond sits at the carried value, the puck stays on the base, the reach box spans the macro's depth, and the oracle agrees.
    - **CONTROL:** with the macro at 0, no diamond is drawn.
    - **cross-mod magnitude:** the lane RMS goes 0 → 0.2583 → 0.6738 and the strongest arrow 0 → 0.3457 → 0.8326 at xm 0 / 0.3 / 0.8.
    - **CONTROL:** a Fold blade at xm 0.8 draws 0. A decoration keyed to the knob would not.
  - **VERIFIED, real mouse** (a scratch CDP harness using Input.dispatchMouseEvent):
    - A grab-and-release on the ring left K, Detune and xm bit-identical.
    - A drag of +0.16 W and −0.13 H took Detune 14 → 30 c and Coupling 0.35 → 0.61 while dragging. The oracle's `t` agreed, and the readout said "DRAG · ↔ Detune 30 c · ↕ Coupling +0.61".
    - An alt-drag of the same width took Detune 14 → 18 c (¼).
    - On the waveform, a plain drag moved c and c2 (empty space, both blades) and left the widths alone. A shift-drag moved w and w2.
  - `lab_load_check`: GREEN on the lab.
  - Tier audit from the page's DOM: 93 rows at their accounting tier, the same 8 promotions, 0 demoted, 0 not placed.
  - Layout: no clipped text node in any cell, in any of the 21 jobs (OSC and MAIN, both themes, mapping open, xm 0.8).
  - **Not verified:** the three-pass cross-mod estimate against the oracle's audio. It is a display approximation by the spec's own account. Nobody has heard the change.
- **Alternatives rejected:**
  - **Keeping the pad absolute on the ring.** A grab would jump the sound, and the brief asks for round 2's relative convention.
  - **A travel of 420 px on the ring.** The marker would lag the hand on a plane that is the value plane.
  - **The readout drawn in the canvas.** On a square ring there is no corner wide enough, so it would cover members.
  - **A filled modulation dot.** Members are filled violet dots.
  - **A modulation trail.** There is no time-based source, and it would need wall-clock history.
  - **Folding MAIN's pad into its left Visualizers ring.** That takes the swarm's XY out of Osc Controls, where the wireframe puts it.
  - **Cross-mod arrows weighted by the knob or by one instantaneous sample.** The knob is decoration. One stroboscopic sample is arbitrary.
  - **Straight chord arrows.** They vanish when members lock together.
  - **Folding feedback into the lane.** The ask was cross-mod.
- **Verify:** `./verify fast`, exit 0, git 16f1057 (`.harness/last-verify.json`, 2026-09-27T01:55:09Z). This trace and the index regeneration are committed on top.
- **Screenshots** (scratch, not committed; `scratchpad/b294/shots/`):
  - 01–04: before (OSC light/dark, MAIN light/dark).
  - 05–08: after (same set).
  - 09–10: ring at rest, light/dark.
  - 11: mid-drag with the readout.
  - 12–14: modulation marker (OSC light/dark, MAIN).
  - 15–20: cross-mod at xm 0 / 0.3 / 0.8, light then dark.
  - 21: the ring's mapping row open.
- **Open questions:**
  1. The ring's ⇧ slot: Cross-mod (as built) or Stereo?
  2. MAIN folded into Osc Controls (as built) or into the left Visualizers cell?
  3. The cross-mod arrow scale (`XM_FULL` = 1 RMS) is the lab's choice, fitted to the check's voicing.
  4. Should feedback be drawn through the same estimate?
  5. Round 2's open questions (the drag target, MAIN sharing the mapping, the mapping as a global preference) remain the human's. The ring's mapping is lab state, shared by both pages.
  6. The in-page self-check is still not gated by `./verify`, the same state as B271/B293.
