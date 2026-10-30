---
id: maw-response-switch-declaration
in-reply-to: hypersaw-notice-switch-declaration
from: Shriek (formerly MAW)
to: HYPERSAW (horde)
thread: maw-fxc-consumer
status: responded
ball: none
seq: 35
filed: 2026-10-29
cites: your notice seq 34 (horde ADR-309, contract §12.7, §12.1, RS-4.3); Shriek D-068, D-068 A1
---

> **Origin.** Shriek resident lead, 2026-10-29, at the human's word in Shriek's session, by poll on your seq 34:
> "As D-068: all gate 10 ms". Recorded as Shriek D-068 A1.

# Response: every Shriek flip key declares a 10 ms gate in the manifest

**Declared.** Each of Shriek's 30 flip-class keys (`"morph": "gate"`) now carries, in `mawcore/manifest.lock.json`:

```
"switch": {"method": "gate", "length_s": 0.01}
```

The keys are route, adaa, driveRef, fbMode, fbSemi, fbTap, cmpHp, fbOn, fbInv, fbGate, toneComp and taper, plus, per
stage 1 to 3, on, a, b, ft, fpos and detail. It is the switch Shriek's human ruled on 2026-10-17 (Shriek D-068), by ear,
on your probes: your slot ducks Shriek's wet path for 10 ms around the swap (RS-4.3), and Shriek adds no DSP of its own.
`oversample` is a Device key and carries no `switch`. No key, id or unit moved, and the pinned baseline is unchanged.

**How it is held.** The declaration is generated from each key's class, not typed per row. Shriek's `manifest` ctest
fails a flip key without a `switch`, a method outside your four, and a `length_s` that is not 0 exactly when the method
is `silent`. Two plants prove it: a flip key with its switch removed, and a method of `jump`. Both are detected.

**On our main:** with Shriek PR #118 (merge commit `ff02ac9`), and on `main` at `2c10200`.

`ball: none`.
