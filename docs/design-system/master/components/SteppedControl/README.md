# Stepped control

STEPPED morph as rate plus sync (ADR-266 r10, B424): the morph follows the puck in steps, at a rate in steps per second or a tempo division, with the morph glide softening each step's edge.

**The consumer provides** the session and the tempo (`S.morph.bpm`). The rate control is the house's one rate-plus-sync control, `H.RateSync`, from the modulator family, so the morph and the modulators share it. Its FREE side is a rate in steps per second; its SYNC side a tempo division in beats (one step per division, rate = BPM / 60 / beats). `opts.glide === false` leaves out the Glide row. `H.MorphMode(el, S)` is the QUANTUM · BLEND · STEPPED pill group it sits beside.

**Size** `opts.size === 'compact'` puts the control in `.hd-dense` (20 px pill and stepper faces, 28 px hits, slider rows with a 52 px label and a 52 px value column) and lowers the scope glass to 7 : 1, for the morph cell of a dense page. Without it nothing changes.

**Anatomy**
- RateSync: FREE /s and SYNC pills, the period in `value` style; the free rate (log, 0.25-32 steps/s) or the division stepper with its rate at the tempo.
- Glide: a house slider row, 0-500 ms (the morph glide, B424: it softens each step's edge).
- A scope glass (aspect 4.2:1): the last three seconds; the puck's travel dashed `scr-grid`, the heard position as the beam in `scr-value`, each step a short `scr-marker` tick on the floor. Outside STEPPED the glass says so.
- On the pad (with the quantum pad layer) a hollow `scr-marker` square marks where the morph is heard.

**Gestures** the RateSync's own; Glide: relative drag at 160 px, shift fine, double-click resets, Option + wheel sets, arrows in keyboard mode. Every committed change is one undo entry.

**Reduced motion** the heard position is the puck (one still frame); the scope draws once.

**Placeholders**
- What a step does. Assumed: the heard position samples the puck once per step and the glide eases each step (a one-pole at the glide time). B424 also describes a move "in discrete steps between the start and target"; whether a step is a sample of the puck or a fixed fraction of the move is not ruled. Decided by: B424's design in the morph editor lab and the human's listening.
- The tempo. A fixed 120 BPM sample tempo stands in for the host's.

**Conflict resolved** the lab's STEPPED is a step SIZE in two readings (the position snaps to a grid, or each value snaps to steps of its range). ADR-266 r10 rules a RATE plus sync; the ruling wins, and neither step-size reading is built.

**Sources** `docs/design/morph-editor-lab.html` :574-576 and :3856-3882 (superseded by the ruling); ROADMAP B424; ADR-266 r10.

The preview draws SAMPLE data generated in the preview, never the engine.
