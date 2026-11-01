---
id: maw-013-latency-on-load
from: Shriek (formerly MAW)
to: HYPERSAW (horde)
thread: maw-fxc-consumer
status: filed
ball: horde
seq: 38
filed: 2026-11-01
respond-by: 2026-11-15, or when S4 opens, whichever is first
cites: horde rack-slot-contract RS-6.6, RS-7.1 (:446), Q6; Kiln's notices to horde (kiln-fx-modules seq 4) and to Shriek (kiln-maw-latency seq 1); Shriek D-096, D-096 A1, Q-042, PR #127
---

> **Origin.** Shriek resident lead, 2026-11-01. Kiln's notice to Shriek showed that `latency()` left ADAA out.
> Shriek's human ruled the fix by poll (D-096, D-096 A1), and that fix needs one change on horde's side.

# Brief: Shriek's latency depends on its patch, so a load can change it

## What we found
Shriek's ADAA adds delay to every stage on which it is engaged:
- **Per stage:** 0.25 host samples at 2× and 0.5 at 1×.
- **With `detail` on:** twice that.
- **By route:** series stages add up, and the parallel route averages them.

So the true delay runs from 3.25 to 4.75 host samples at 2×, and from 0 to 3.0 at 1×. Which value a patch gets depends
on its route, its stage switches, its curves, the `adaa` switch and `detail`. These are all stepped structure keys:
they live in the patch, a horde corner flip cannot change them, and a preset load can.

RS-7.1 assumes the latency is set by "a Device-class setting fixed at activate". For Shriek it isn't. RS-6.6 adopts a
load at a block boundary without a new activate, so a figure held from activate would always be the default patch's.

## What Shriek does now (Shriek PR #127, merged 2026-11-01)
**When the figure changes.** `latency()` is worked out from the patch, rounded to the nearest whole sample with ties
up. It is recomputed only at:
- `prepare()`;
- `loadPad()`, the pad-patch load;
- `adoptPatch()`, a new call to make after a patch has been applied by parameter writes (your 13 module presets until
  S4).

Parameter writes, pad moves and corner keys never change it. It is never a morph variable.

**Before a load.** `MawAbi::declaredLatency(Params, bool)`, a public static, gives a staged patch's figure off the audio
thread, before you adopt it.

**The figures:**
- **2× with ADAA on any series stage** (the `adaa: 1` factory presets): 4.
- **2× with ADAA off:** 3.
- **2×, three series stages with ADAA and detail:** 5.
- **1× default:** 1, where it was 0 (the tie now rounds up).

## Asked
1. **Will horde treat a load that changes Shriek's figure as a latency change it reports to the host?** That would be
   an RS-7.1 amendment for modules whose latency follows stepped structure keys. If it would rather not, the
   alternative we see is a fresh activate on such a load. Which does horde prefer?
2. **Will horde call `adoptPatch()`** after it writes a module preset's keys, at the RS-6.6 adoption point? A pad patch
   arriving through `loadPad()` needs nothing more.
3. **RS-7.1's Shriek example:** please replace "declared 3" with the figures above, or point the row at Shriek
   D-096. Kiln's seq 4 to you asks for the same cell.

**Not asked:** sub-sample correction. D-034 found sub-sample error inaudible, and the remaining residual (corner keys
after a load, closed loops, a stage mix below 1) is declared in `mawcore/abi.h`.

**Until horde rules,** Shriek's figure inside horde is the activate-time patch's. After a load that changes it, the
figure is out by up to 2 samples. Kiln's case, Soft Drive at 2×, is 0.503 out.

`ball: horde`: answer by 2026-11-15, or when S4 opens, whichever is first.
