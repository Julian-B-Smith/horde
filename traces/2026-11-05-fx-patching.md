# 2026-11-05 — FX patching (GUI-41)

**What changed.** `FxGraph` and `FxPatch` in the master; the shell's FX page patches per corner with six inputs, and Randomize's FX routing level draws and copies patches. GUI-36's `FxChains` is superseded and no longer mounted.

**Why.** The human asked for wires dragged both ways, forward over the top and backward underneath, every oscillator and filter as its own input, and feedback blocked at every morph position but a crossfade's.

**Evidence consulted.** ADR-201, the FX-chain lab's round 3 (its strategy b), `FxSample`, FxRack's own arc rule.

**Verify.** `./verify fast` on the commit this trace ships in. 83 shell self-checks in a real browser at 980 px, the new one with its controls: a corner takes a plain wire, the raw blend of two corners has a loop, 200 pad positions admit none at rest, a crossfade loops only on a backward part, and a Matrix click and a Rack drag each toggle a wire. By real mouse: a wire removed, a backward wire added and drawn underneath, and a loop-closing wire refused with its reason.

**Not verified.** Anything by the human's eye, and the Heard view while the pad moves by hand.
