# b241-critic-notes — correcting `2026-09-23-b241-modulated-readback.md`: the fix CHANGES SOUND, main also corrupted unrouted osc-2 twins, and the oracle now checks what the engine holds

- **Queue item:** B241. This addresses the critic's APPROVE-WITH-NOTES on PR #743, relayed by the lead on 2026-09-24. Traces are append-only, so this entry corrects the earlier one and leaves it unedited.
- **Why:** The earlier trace had three errors:
  - It said "none [of the gates] tripped" but never looked at sound.
  - It left out main's worst symptom.
  - It said the GUI shows a halo on every modulated row. Routing cells have none.

## Correction 1: SOUND CHANGES (the code stays; whether these are a bug fix or a revision-gated law is the lead's question for the human)

Each of these changes what the engine hears for a session or patch that has a mod route on the row:

1. **Inertia (11).**
   - On main, a non-matrix write under a route stored the TAPERED value as the base, so the engine heard `taper(taper(knob)+δ)`. Now it hears `taper(knob+δ)`.
   - The critic's render hashes differ from main for route + host write, and for a swept morph with a route on 11. With only this edit reverted, both match main.
   - The check's "asked" law on main: a write of 0.1 made the matrix ask 0.2532 instead of 0.35.
2. **Inertia Curve (70).** On main, a write under a route was undone at the next mod tick, because the base never moved. Now it lands. On main, a write of 2.97 made the matrix ask 1.325 instead of 1.795.
3. **Step Grid (148).** On main, the base was stored unsnapped. Now it is stored snapped.
   - The critic measured 2.806 → 2.506.
   - The check: a write of 4.775 asked 6.7125 on main, 5.9375 now.
   - Audio was identical in the critic's run only because step-quantise was off.
4. **NEW in this change: a write under a SATURATED route.** modStep re-applies only when its target differs from `lastApplied`. A host write puts the bare value into storage. If base+offset is clamped at a bound, the target equals `lastApplied`, so on main the engine kept the bare write, without its offset, for as long as the modulator stayed saturated.
   - The critic's new "heard" assertion found this. On main it fails 24 (row, write) pairs: 20 saturation cases plus the four 11/70 pairs above. On this branch before the fix it failed 21, the 20 plus Step Grid (148).
   - The saturation cases: Dissolve (8), Attack (19), Release (22), Grid Cycles/Beat (23), S.Attack/S.Release (65/68), Bend Rate/Lag (108/109), Note Time (139), Morph Y (153), D1/D2/D4 Beats, D2/D4 Loop HP, osc-2 Dissolve/Release/S.Release, and SUB Attack/Release (4012/4013).
   - The fix: the base intercept (and 70's branch) forgets `lastApplied`, so the next tick applies base+offset.
   - **Entailed, not measured:** the morph field's writes to a modulated destination pass through the same intercept, so under morph + a saturated route the engine now holds corner+offset where main held the bare corner value.
5. **The readParam fix itself changes sound under morph.**
   - Corner capture and both morph-on adoption branches now take the base. On main they took the modulated value, and the field then wrote it back as the base: 70 vs 20 on sub.fine, measured in §A.
   - Any morph patch with a route on an affected row now morphs between different corner values.
   - Loading an existing chunk is unchanged (the loader reads the same values). What changes is re-saving, capturing and adopting.

## Correction 2: main corrupted the UNROUTED osc-2 twin

The old lookup was keyed on `d->id`, which for an osc-2 id is the osc-1 base def.

- **Probe:** detune (4) = 0.3, osc-2 detune (1004) = 0.8, route on id 4.
  - On main: `get_value(1004)` = 0.3 and the saved `o1.detune` = 0.3.
  - Now: both 0.8.
- **The repo's own fixture `tests/state_fixtures/chunk-v2-rev1.txt`** holds detune 0.35, o1.detune 0.22 and `modroutes=0:4:0.5;`.
  - On main, loading it and re-saving wrote `o1.detune=0.35`.
  - Now it writes 0.22.
  - This fixture is read by `modreadback_check` §D.

**The earlier trace's paragraph on existing sessions was incomplete.** It is replaced by this: since ADR-136 (2026-08-28), any session that had a route on an osc-1 row and was re-saved may hold that row's osc-2 twin at osc-1's BASE. The chunk cannot show which twins were overwritten. The same goes for engine, routing and shell-owned rows saved at a modulated value.

## Correction 3: the GUI note

The earlier trace said modulated rows show the base "with the halo (`modLiveJson`), as swarm rows always did." That is wrong for routing matrix cells. `paintMatrixCell` (`src/gui/gui2.html:7432`) draws no halo, so under modulation a cell now FREEZES at its base with no sign that it is modulated. The lead is queuing a follow-up row.

## Oracle reach (critic item 3)

- **3a:** a new export, `hypersaw_debug_stored` (`src/hypersaw_debug.h`), backed by `Plugin::readStored`. `readParam` is now `base-if-routed` followed by `readStored`, one body with no parallel chain. The sweep asserts three laws per write:
  - **base:** get_value under the route reads the same as the no-route write.
  - **asked:** the matrix target equals that base + 0.25·src·span, clamped.
  - **heard:** the engine's STORAGE holds what a no-route write of the target stores.
- **3b:** each row's value in the SAVED state chunk is asserted, keyed as `state_save` keys it. An unknown engine prefix returns NaN and fails; it is never skipped.
- **3c:** two write values per row (0.1 and 0.3 of the span, toward the middle).
- **§D:** the twin probe and the fixture.

Before and after. "Main" is main's source plus a test-only `rawRead` door for `hypersaw_debug_stored`, used only for this measurement and not committed.

| family | routable | LEAKED get_value | CHUNK | BASELOST | ASKED | HEARD |
|---|---|---|---|---|---|---|
| engine | 11 | 11 → 0 | 11 → 0 | 20 → 0 | 0 → 0 | 2 → 0 |
| osc 1 + global | 145 | 78 → 0 | 78 → 0 | 145 → 0 | 6 → 0 | 19 → 0 |
| osc 2 | 54 | 54 → 0 | 54 → 0 | 105 → 0 | 0 → 0 | 3 → 0 |
| routing | 29 | 29 → 0 | 29 → 0 | 58 → 0 | 0 → 0 | 0 → 0 |

BASELOST, ASKED and HEARD count (row, write) pairs, two writes per row. Main's BASELOST is high because on main get_value under a route never read the base for those rows.

§D on main: `osc 2 detune get_value reads ITS OWN 0.8 — 0.3`, `saved o1.detune is 0.8 — 0.3`, and `re-saved o1.detune is the fixture's, not osc 1's — 0.35`. All three pass now. The whole check: 20 failures on main, 0 now.

## Critic item 4: wiring into `./verify fast`, not done (stopped and reported)

`fast()` in `./verify` runs scripts and markdown only. It builds nothing, and CI's `verify-fast` job checks out without submodules, per `.github/workflows/docs.yml`: "`verify fast`'s gates are scripts + markdown, not a build". A C++ check cannot run there. Two ways around that exist, and neither is mine to take:
- adding a cmake build to `fast` changes its cost and CI contract;
- "run it if already built" is a silent skip.

The check stays in `full()`, and its `WIRED: ./verify full.` header remains true.

- **Evidence consulted:** the critic's notes as relayed; `src/hypersaw_clap.cpp` (modStep's `lastApplied` dedupe, the base intercept, `readParam`); `src/gui/gui2.html:7432`; `./verify` `fast()`; `.github/workflows/docs.yml`; `tests/state_fixtures/chunk-v2-rev1.txt`.
- **Alternatives rejected:**
  - Asserting "heard" through `lastApplied`: that is the matrix's own record of what it asked, and the critic's exact objection.
  - Leaving the saturation defect for a follow-up: the new law is red without the fix, and the fix is two lines inside the intercept this PR already owns.
- **Verify:** `./verify full` on the committed hash is recorded in the PR comment and the report; this file is committed with that change set.
- **Open questions:**
  - Sound changes 1–5 are a human ruling (bug fix vs revision-gated law).
  - The morph-path part of change 4 is entailed, not measured.
  - Wiring into `fast` needs a build-in-fast decision.
