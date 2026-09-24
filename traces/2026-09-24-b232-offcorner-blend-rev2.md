# b232-offcorner-blend-rev2 — in blend, a corner where the source is OFF gives no weight; the first revision-2 law

- **Queue item:** B232, ruled by ADR-183 (both as carried on `lead-records-85`, PR #737). The opt-forward control is B239 and is NOT built here.
- **Why:** The human (2026-09-23): an off corner's irrelevant Osc 2 / Sub settings pulled the blend (Osc 2 detune 0.1 off / 0.8 on read 0.45 at the midpoint). ADR-183 puts the fix behind B100's `engine_revision` as revision 2, so every revision-1 patch keeps its sound.

## What changed

- `src/hypersaw_clap.cpp`
  - `kEngineRevision` 1 → 2. New instances start at 2. `initState` still sets 1, because it is the "header-less blob" default. A loaded header sets N, clamped to [1, 2]. The B100 comment now lists the laws by revision.
  - **Declaration: the source's gate** (`sourceGateOf`).
    - An engine block's id resolves to the block's `gateId` (the sub: 4015 over 4000..4019).
    - A per-osc id resolves to its oscillator's enable, `150 + k*kOscStride` (150 over osc 1, 1150 over the 1000-block).
    - Globals, the FX rack, routing cells and the gates themselves have no source gate.
    - These are the same ids the B48/B203 level ramp already reads. STATION, or a third swarm oscillator, gets the rule by declaring its gate or by the stride; no new list.
    - The gate is resolved to a morph-slot index once, in `morphInit` (`morphGateSlot`), so the audio thread neither searches nor allocates.
  - **The law** (`morphLiveWeights`, `morphBlendTarget`).
    - Revision 2 blends with `e = w·g / Σ w·g`, where g is the corner's stored gate. The denominator is the same `w·g` sum `morphOnWeight` uses for the ramp.
    - It falls back to the plain four-corner sum in these cases:
      - no gate;
      - the gate is exempt;
      - no weighted corner is OFF;
      - every weighted corner holds the same value (forward only; see below);
      - live weight ≤ `kMorphOnFloor` = 1e-3.
    - `kMorphOnFloor` names the ramp's existing literal. The two `onW > 1e-3` sites now use it, which changes no behaviour. The parameter switch-over and the source's own switch-off are therefore one threshold, compared the same way. Below the floor, the source's stepped enable has flipped to 0 and its gain is ≤ −60 dB.
    - The plain path is the original expression in the original order, so revision 1 is selected, not rewritten.
  - **The inverse** (`morphRouteEdit`, blend branch). It now distributes an unarmed edit by the same `e` weights at revision 2. Without this, an edit at the midpoint lands 0.85 instead of 0.5 and is overwritten at the next tick (measured with a plant; see Calibration). Revision 1 keeps the old weights, same code.
- `tools/offcorner_check.cpp` (new, `WIRED: ./verify full`, ~0.5 s) plus its CMake target and `./verify full` line (ADR-180 §1: adding a check). Every row renders (L0063).
- `tools/state_check.cpp`: the B100 expectations are re-pinned to the new latest. A fresh instance is 2 and a future revision clamps to 2. New rows:
  - revision-2 JSON and chunk both load and re-save as 2;
  - a header-less chunk after a revision-2 load drops back to 1.
  
  These rows moved with the ADR; no row was removed or relaxed.
- `tests/feature_tests.tsv`: B232-1 (agentic, offcorner_check) and B232-2 (human listening row).
- `docs/presets/factory/**`: regenerated with `gen_factory_bank` from clean `bbaa4f3` (commit d40866f).

## Declaration choice — the gate, not `depends`

`param_presentation.tsv` `depends` was rejected for four reasons. The lead's mid-task note from the integration playbook (PR #740 §12–13) agrees independently on 1–3.
1. `depends` drives the GUI's shown_when. `enable=1` would hide an off oscillator's controls, but the player must still be able to edit it.
2. It drives ADR-108's hold on the **pick** path, which is not revision-gated. Declaring the enable there would re-voice revision-1 patches under quantum, which is a human gate.
3. `gen_depends_header.py` flattens a clause's AND into OR, and skips engine-block rows entirely. The sub would silently get no rule.
4. The gate is already the declaration the ramp reads. One declaration with two consumers beats a second one that must be kept in step.

## Edge cases (as built)

- **Pure OFF corner** (live weight 0): falls below the floor, so it takes the plain blend and reads the corner's stored value exactly. Measured: 0.1 at X=0.
- **Pure ON corner** / all-ON field: offW = 0, so it takes the plain blend, bit-identical to revision 1.
- **Floor 1e-3**: at live weight 5e-4 the value is the plain blend (rev 1 = rev 2) with osc 2's enable reading 0. At 2e-3 the value is the ON corner's, with the enable reading 1.
- **Stepped / quantum pick / ADR-108**: untouched. Stepped params never reach `morphBlendTarget`.
- **Exempt params**: untouched. They are skipped before the blend.
- **Exempt gate**: plain blend. An exempt gate is live-only, so its corners do not say where it is off.
- **Intent-bus path** (`intentStep`, flag off by default): shown NOT to blend these parameters. `IntentCore::stepParams` (src/intent_core.h:262-277) takes each parameter from ONE owner corner (`base[cornerParamIdx(ownerParam[p], p)]`), and `intentApply` keeps ADR-108's hold. There is no cross-corner weighted sum to change. Row: flag on, rev 1 and rev 2 render bit-identically.
- **The forward-only "weighted corners agree" fallback**. Off the grid, bilinear weights do not sum to exactly 1 in doubles. Without this fallback, at (0.3, 0.4) untouched osc-2 rows (e.g. 1019, 1026) came out an ulp away from revision 1. The fallback keeps "rev 2 differs only where the rule applies" true bitwise. It is kept out of the shared weight function because the inverse must use `e` even for an uncontested slot. Otherwise an edit would not land: 0.027 instead of 0.05 was measured with the plant.

## Evidence consulted

- ROADMAP B232/B239 and DECISIONS ADR-183 (`git show origin/lead-records-85:…`).
- `src/hypersaw_clap.cpp`:
  - morphStep, morphApplyOscEnable/GateEnable, morphOnWeight, depLiveInCorner, morphRouteEdit, morphAdoptUncontested, intentStep/intentApply;
  - the B100 header (`kEngineRevision`, `setEngineRevision`, `initState`, `applyStateJson` header read, `state_load`/`state_save`);
  - kEngineBlocks and sourceGate ids, kGlobalIds.
- `src/intent_core.h` stepParams; `tools/gen_depends_header.py`, `tools/depends_check.py`; `tools/gen_factory_bank.cpp`, `tools/state_check.cpp`, `tools/gen_state_fixtures.cpp` (its source instance is loaded header-less, so the fixtures stay revision 1 even if regenerated).
- LIBRARY L0059, L0063, L0032, L0033, L0036.

## Measurements

**Rows red today, green here.** The final check source was compiled against origin/main's `libHYPERSAW-impl.a` (build of 9532eb1): `offcorner_check: RED (14 failures)`. The key rows:
- `FAIL swarm 2: rev 2 midpoint detune reads the ON corner's 0.8 (read 0.45000000000000001)`
- `FAIL sub: rev 2 midpoint level/tone read the ON corner's 0.9 / 8000 (read 0.55000000000000004 / 4250)`
- `FAIL swarm 1: rev 2 reads 0.8, rev 1 the plain 0.45 (read 0.45000000000000001 / 0.45000000000000001)`
- `FAIL edit: rev 2 leaves the OFF corner's stored detune alone (0.1, read 0.15)`

The CONTROL (`swarm 1 detune … bit-identical across revisions`) was OK on both builds. The same rows are green on d40866f (`offcorner_check: GREEN (0 failures)`).

**Revision 1 is bit-identical to origin/main.**
- The check's printed revision-1 render hashes are identical across the two builds: `osc2-midpoint a166cb18784998de`, `sub-midpoint c3dc3130bab3baa4`.
- Scratch probe: all 41 factory patches, 11 puck positions each, 400 ms note, FNV over the float output. It gave 451/451 revision-1 renders bit-identical between origin/main and this branch.
- Standing half: statefix_check (revision-1 fixture corpus) and bank_check, both green.

**Factory bank diff.** A parse-compare of all 41 patches against the previous bank:

```
45 files parse-compared
  header 1/de7d67f -> 2/bbaa4f3: 41 file(s)
  header None/None -> None/None: 4 file(s)
files differing beyond engine_revision/build: 0
```

(The 4 are the corner presets, which carry no header and did not change; neither did BANK.md.) The reason is that no factory patch has a source gate that differs across its corners: every patch reads 150=1111, 1150=0000, 4015=0000. So the rule re-voices none of them. The probe confirms it: 0 of 451 renders differ between revision 1 and revision 2.

**Calibration** (L0032/L0033; each plant was temporary and then reverted, with the source restored from a backup):

| Plant | Result |
|---|---|
| Inverse left on the old weights | FIRES: the edit reads 0.85 at the midpoint and 0.898 off-grid, and the off corner's stored value moves 0.1 → 0.15 |
| No "weighted corners agree" fallback | FIRES: the off-grid differing set is {…1016,1021,1023,1026…}, want {1004} |
| No `offW == 0` guard | Did **NOT** fire at (0.3, 0.7), a coverage boundary: plain and renormalised sums happened to agree there. The control was moved to (0.3, 0.4), found by searching for a position where they differ in doubles. There it FIRES: osc 1 detune 0.32000000000000001 vs 0.32000000000000006 |
| No live-weight floor | FIRES: rev 2 reads 0.8 at live weight 5e-4 |
| Exempt gate ignored | FIRES: 0.45 vs 0.8 |
| Revision gate ignored (`offCornerRule = true`) | FIRES: 12 rows, all the revision-1 expectations |

**Aside, not ours:** with the "agree" plant, ids 45/48/67 (osc 1) showed up as differing. The cause is the known `o1.tilt` / toneTilt readback alias that undo_check already names (`id 1045 o1.tilt still loses values above 1 to the toneTilt alias`). Osc 1's parameters did not take the rule.

## Alternatives rejected

- `depends` as the declaration: see above.
- A "reference-offset" form of the renormalised sum, so that agreeing ON corners are exact off the grid. Rejected as complexity without a consumer. The as-if identity row is asserted at the midpoint, where both forms are exact.
- Leaving `morphRouteEdit` alone: rejected, because it breaks ADR-109's "an edit sticks" at revision 2.
- A setter debug export for the revision: rejected. The check stamps the revision into a real blob and loads it through the preset door instead (hypersaw_debug.h: exports without an owner are deleted).

## Verify

`./verify full` exit 0 on **d40866f** (`.harness/last-verify.json`: `{"target":"full","exit":0,"git":"d40866f","ts":"2026-09-24T03:57:37Z"}`). This trace's own commit is re-verified in the PR.

## Open questions

1. **B222's morph-on adoption** (`morphAdoptUncontested`) still asks "do all FOUR corners agree". At revision 2, a slot whose ON corners agree but whose OFF corner differs is not adopted: the field then plays the ON corners' value, not the live edit. This is narrow: it needs a source off in some corner, and it is harmless at pure corners. Is it the human's case? Not built; not asked.
2. **Installed factory copies.** Users who installed the bank before this build keep their revision-1 copies of the factory files until the preset store installs a newer bank. That depends on presetstore's version rule, which was not examined here.
3. **The floor is a discontinuity in the parameter value** (a step at live weight 1e-3). It is inaudible by construction only because the source is killed at the same threshold. That holds for the swarms (enable flip) and the sub (gate flip), measured for osc 2. A future source whose gate does NOT kill below the floor would inherit an audible step.
4. **B239** (the opt-forward control) is untouched. Until it exists, a revision-1 patch stays revision 1 forever (ADR-183 §4).
