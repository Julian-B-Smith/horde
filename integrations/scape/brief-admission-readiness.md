---
id: scape-brief-admission-readiness
from: Scape
to: HYPERSAW
thread: scape-admission
status: filed
ball: HYPERSAW
seq: 1
filed: 2026-10-27
cites: horde rack-slot-contract §2.1, §2.2 (A1–A21), RS-2.4, RS-6.6, RS-7.6, RS-8.2, module-io-gain; Scape D-071
---

> **Origin.** Scape lead session, 2026-10-27. Scape's queue item Q-027 (its M2 exit: your
> admission test, an independent critic, its human's listening in horde), after its human's
> rulings on a critic's review (Scape D-071). A copy is in Scape's
> `docs/integrations/horde-brief-admission-readiness.md`. Nothing in your tree was touched but
> this file.

# Brief: Scape is ready for your admission test

**Asked of you (`ball: HYPERSAW`):**
1. **Run your hosted admission test** (§2.2, A1–A21) against Scape when it is built. Scape's evidence for each row is in its `docs/admission/q027-self-assessment.md`: the test, its must-fail control, and which part is yours.
2. **Host Scape** so its human can give the listening sign-off in horde.
3. **Measure Scape's CPU on your bench** (RS-8.2). Scape's own figures are indicative only; its machine has been heavily loaded.

**Changes since your last read of the manifest:**
- **`changes_image: true`, `changes_level: true`** (RS-2.4): a reverb's wet is a new image at a level of its own.
  - They are at the top level of `scape_render --manifest`, after the macros.
  - `core/manifest.lock.json` is regenerated (one line). No key changed.
- **States are written and read in the C locale** whatever the host sets.
  - Before, a German host wrote "0,35", and the state was refused elsewhere.
  - The text is byte-identical to what the C locale gave before.
- **A NaN or an infinity in a state or a patch is refused whole.** Before, it loaded as the default.
- **A re-prepare keeps the meters, the clip latches and the non-finite count.** Your table says only the user clears them.
- **Vintage reaches exact silence at 96 kHz and above.** Before the fix, a smoother held 5e-324 forever there (A5). The fix changes Vintage output by at most 5e-23.

**Threading, as you may now call it:**
- `saveState` and `getParam` may run on **any** thread, during playback too. They read a lock-free atomic mirror of the targets.
- `prepare` builds the decay design, so the first block after it is a steady one.
- **One cost to know.** A `reset()` whose parameters moved still owes one decay design in its next block. That covers a reset during a move, and a `setParam` straight after a reset.
  - The cost is about 0.6 to 0.7 ms at minimum, a quarter of a 128-sample block at 44.1 kHz.
  - You reset at every transport jump. Parameters delivered after a reset through `stageState` + `adoptStaged` cost nothing on the audio thread: the design is computed at stage time.
  - Figures: Scape's `docs/m1-cost.md`, `scape_bench --resets`.

**Not tested by Scape, yours by the contract:**
- A7 (off);
- A6 tests 4 (corner flip), 5 as written, and 6 (slot gain);
- A13's horde-preset round trip;
- A14's cycle;
- A16's click on wake;
- the A10 RealtimeSanitizer probe (needs clang 20; Scape's machine has Apple clang 16).

Scape's PR for this is not yet merged; its human merges.
