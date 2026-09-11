# 2026-09-11 — Morph field layout: the append-only promise broke on 2026-08-31 (ADR-159, B115)

**Report.** Human: held notes step to other notes at random intervals; a
second note "plays the first note again and needs a second press"; "this bug
has currently ruined all my presets". The first round found and fixed a real
but secondary mechanism (B114/ADR-158, PR #547). This round loaded the
human's own preset files into the headless rig.

**Measured.** `tools/preset_probe.cpp` + `hypersaw_debug_notelaw` on
`power hour.json` (params: bendLaw spring, springF 3.85, damp 0.49, distOver
1.24, quant off, step timing continuous):

```
before: notelaw: model=4 tau=1 gtime=60 rate=3.85478 springF=1.24 damp=1 distOver=0 quant=1 qhyst=3 qTime=2000
        +64@50ms voices: 0,60,1,261.626,261.626,0;1,64,1,261.626,261.626,1;   <- the new voice parked at C4
        A  heard 1.05 s after striking 64: MIDI 65.04
after:  notelaw: model=4 tau=60 gtime=120 rate=24 springF=3.85478 damp=0.49 distOver=1.24 quant=0 qhyst=8 qTime=0
        +64@50ms voices: 0,60,1,261.626,261.626,0;1,64,1,293.876,293.876,1;   <- gliding
        A  heard 1.05 s after striking 64: MIDI 64.10
```

Every field is the value of the param two slots later: the corner arrays
(222 entries, saved 2026-08-25) were being read through a 224-entry order.

**Cause.** `morphInit` builds the per-osc prefix from the param table;
`oscPitch` (181, ADR-150, 2026-08-31) was inserted there, shifting the tail.
Dates: tail complete 2026-08-22 (`b4f36a3`), 181 added 2026-08-31
(`8f648b1`, no morphIds change). Hidden until B110 (2026-09-11) let
`morphOn` land on preset load.

**Fix.** `kMorphLateIds = {181}` appended last; `"morphLayout":2` stamped;
layout-1 arrays of exactly 224 entries remapped by id through the legacy
order (`morphSlotMap`). `tools/morphlayout_check.cpp`: PASS (T1 frozen
order · T2 222-entry remap · T3 224-entry remap · T4 layout-2 identity · T5
must-read-wrong control). Standalone, unwired.

**Evidence consulted.** `src/hypersaw_clap.cpp` morphInit / applyMorphChunk
/ morphJson; the human's `presets/*.json` and `corners/*.json` (24 files,
Aug 21–30); `git log -S` for rows 129–132, 150, 181 and the tail blocks.

**Oracle.** `./verify full` — pasted in the PR body.
