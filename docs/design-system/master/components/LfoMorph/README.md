# LFO morph

An LFO's shape, editable per corner and morphing with the pad (GUI-37, a MOCK-UP the human asked to land). It's ADR-349's law on the Mod page, around the bundle's own `LfoShapeEditor`.

**The law: `Horde.ShapeMorph`, pure.** All four corners share one list of up to 32 points, and each corner holds its own time, level and bend for every point. A point added in one corner parks in the others on their own curves, by C.12's exact split (bend u at fraction f becomes u·f and u·(1−f)), so no other corner changes. A point deleted in a corner parks there, and a point parked in all four leaves the list. Blend and Stepped move the points; Quantum picks each point's corner on its own (on trial, ADR-349 r2). A list never runs backward. Rate is per corner and blends on a log taper (B396).

**Anatomy.** Corner pills A to D, with the point count, the parked count, the law and the heard rate. Five small wells: each corner's shape, dimmed by its distance from the pad, with parked points as hollow rings, and what's heard now with its read position. Below them, `LfoShapeEditor` on the selected corner, with parked points as hollow rings (`o.parked(i)`), and a Rate slider.

**The consumer provides** `model` (`ShapeMorph.make(pts)` with `rate: [a, b, c, d]`), `weights()`, `mode()`, `name`, `keys()`, `onCommit(label)`, and calls `frame(dt)` from its frame loop. `Horde.LfoMorph(el, o)`.

**MOCK.** The model lives in the shell: not in a patch, a snapshot or undo, and gone on reload. No sound reads it (RC-4 is deferred; GN-49, B601). Open, from ADR-349: what smooths the output at a point's flip, how a corner of another kind (S&H, random) mixes by output, and whether Cohesion spans Quantum's two behaviours.

**FX device (ADR-254).** It has modulators and corners, so the same editor applies.
