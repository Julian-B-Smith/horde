# b376-engine-audit — every swarm-engine control across four sources, with a decision column for the human

- **Queue item:** ROADMAP B376 (records branch `lead-records-149`), dispatched by the horde lead 2026-09-29. B332 phase 1b waits on it.
- **Why:** The human will not approve lifting `src/swarm_core.h` until every control has been audited and ruled keep / lock / cull / merge. ADR-186 §4–§5 generates horde 2's parameters from manifests, so this audit is the swarm core's B275 readiness gate. The page recommends. Only the human decides.
- **What changed:** one new file, `docs/design/engine-audit.html` (a self-contained lab with its data inline), and this trace. Nothing under `src/`, `reference/`, `specs/`, `h2/` or the engine was touched. No check was added or edited. The page defines no wheel listener, so it stays out of `lab_wheel_scroll_check`'s LABS inventory. That inventory is for labs that adjust a value on the wheel: an entry without `onAltWheel` is RED by that check's own static rule. The page is still covered by the check's static sweep over every lab.

## How the data was read (the builder is scratch, not in the tree)

A one-off Node + Python builder read, at `bae777f` (origin/main):
- `src/swarm_core.h` `struct Params`: **70 fields**. It parses statements, not lines. The first draft read lines only and got 59, because `double n = 7, …, onset = 0,` continues onto the next line. The page's self-check 1 would not have caught that draft, since the count it checks against came from the same parser. Every field's `p.<field>` read sites and its setParam keys were collected.
- `src/hypersaw_clap.cpp` `kParams` (266 rows), `kGlobalIds` and `kParamClassOverrides`. **126 ids are in scope**. The other 140 are listed on the page by family (SPECTRA-only, FX rack, morph, macros/XY, intent dev, LFO/ENV 3–4). The build refuses an id that is neither audited nor excluded.
- The composed engine: RazorCore `t` (64) + `d` (38) + HORDE_D (28) + `os` = **131 keys**.
- SCALPEL: `reference/scalpel/data/parameters.json` (**111 keys**, including 10 bench-only), plus the lab's SP table (121 rows).
- `tools/patchspace/dependency_tree.json`: roles for the 113 keys in the composed-engine lab.
- Preset use:
  - SCALPEL: the 83 bench presets plus the lab's 12 ENV_PRESETS, each laid over the oracle's defaults as `applyPreset` does.
  - Legacy: the human's 39 ported presets and corners (10 presets, 29 corners), read from the git-ignored `local/legacy-presets/` via the main checkout. **Only per-key counts are committed**: no names and no values.
- Row mapping and B275 fields follow `docs/scalpel/ACCOUNTING.md` §1.1–§1.5, the seam audit's SUB manifest (§4.2–§4.3), QM-2's classes (SAFE / VOICE_BOUND / STRUCTURAL / FORBIDDEN), and FOUNDATIONS' failure-mode vocabulary (`response-engine-manifest`, their origin/main). A bound appears only where one was already measured: the rebuild cost (503 ns/event, 2026-09-18 audit §2.5), the K smoother τ, the gravity grid, and the oversample CPU.

## Summary

- **185 rows (control concepts).** By section:
  - Swarm core 27, Dynamics & gravity 8, Onset & timing 5, Voice/envelope/glide 10, Image & output 16, Saw shape 5, Shell pitch & voicing 14, Composed glue 4.
  - SCALPEL blade 1: 25, blade 2: 27, spreads 23, interplay & swarm 10.
  - Bench/lab-only 11.
- **Rows carrying each source:** core field 70, legacy id(s) 82, composed key 131, SCALPEL key 116.
- **Recommendations:** KEEP 139 · LOCK 16 · CULL 20 · MERGE 10. 59 are marked thin evidence, and 51 carry a meaning conflict between sources.
- **Self-check** (headless Chrome at the committed page): GREEN, 5/5 pass and 5/5 must-fail controls fire.

**The 10 most consequential recommendations:**
1. `polyGlide` (89) → **CULL**. It is dead: nothing reads it since ADR-102 (`src/swarm_core.h:558-565`, class override "state compat only").
2. `inertiaCurve` (70) → **LOCK 2.5**. The human settled it by ear (ADR-024 A1), and it is a hidden dev id kept only for the state compat horde 2 does not owe.
3. Saw shape family (`shape`, `sawBase`, `sawProfile`, `round`, `roundHi`) → **CULL** (ACCOUNTING H2, ratified). They were used in up to 8/39 legacy presets, so the importer loses that timbre.
4. `retrig` + SCALPEL `phaseMode` + `keepPhase` + `scatter` → **MERGE** into one Start-phase row. They share one precedence at note-on (`src/swarm_core.h:696-701`). phaseMode's default "settled" is not voiced by the composed engine.
5. `glide` (33) + SCALPEL `glide` → **MERGE** into the note-lane law, in one unit. Today they are seconds vs time-to-95 % ms vs bendTau ms.
6. `digital` (16) + SCALPEL `aa` → **MERGE** (continuous vs switch). 3/39 legacy presets hold a fractional value.
7. `voiceMono` + `voiceLegato` + SCALPEL `polyMode` → **MERGE** into one three-way voicing row.
8. `absK` / `cScale` → **LOCK 0**. It is 0/39 legacy, `cScale` is never live in the composed engine, and the mapping is inverted (see below).
9. `tune` and `bpm` → **CULL as parameters**. They are the engine's declared INPUTS: tune is FOUNDATIONS Q3's one per-voice pitch input, and bpm is the host tempo.
10. ADR-189 `aaCarrier`/`aaXin`/`aaLoop` → **LOCK on**. They are correctness choices, not performance controls. The instrument default is still the human's ruling.

Next in weight: `engine` (43) CULL (SPECTRA parked, 0/39); `lpOut` LOCK 1; octave/semi/fine/oscPitch MERGE into one continuous per-osc pitch; `spread` MERGE into detune; `voiceCull` LOCK −80 dB.

**Controls that look dead:**
- `polyGlide`: never read.
- `lpOut`: has a setParam key but no kParams row, so no host can reach it; only tools set it.
- `cScale`: dependency tree `neverLive`.
- The lab's `subLvl`: mapped to SPECTRA's id 53, drawn and not voiced.
- `engine` (43).
- The 10 SCALPEL bench-only keys.
- In the composed engine (the B332 parity target), these are absent or unvoiced: law 3 (plays as ERB), `beatMult`, `topo`/`reach`/`mu`/`alpha`/`poles`/`balance`, `scatter`, `panScatter`, the pan-image family, `normExp`, `rtone`, `digital`, and phaseMode "settled". Lifting `swarm_core.h` carries every one of them with no parity target.

**Where the legacy core and the composed engine disagree on meaning:**
- `detune`: knob 0..1 vs cents 0..100.
- `dist` default: 1 vs 0.
- `K`: horde law vs SCALPEL law, 1–27×.
- `law` and `driftRate`: key collisions (`h.law`, `h.driftRate`).
- A/D/R: one-pole seconds vs linear ms. Horde's R is 4× longer for the same number (F3).
- `sustain` default: 1 vs 0.85.
- `width`: 0..1.5 super-width vs RazorCore's 0..1 pan, which the composed engine keeps.
- `vol`/`gain`: the output stage is not composed. `normExp` 0.75 vs 1/√N.
- `glideMode`: 3 values vs 2.
- `absK` 0 ≡ `cScale` 1: the sense is inverted.
- `oversample`: default off (0/1) vs 2× (1/2).
- `onsetScatter` time base under 2× oversampling: sub-samples in C++ vs output samples in the composed engine.
- `attackScatter`: scales a one-pole vs a linear attack.
- `inertia`: global in legacy vs per-engine.
- `n`: 1..32 vs 1..9.
- Voice pool: 16 vs 6 of 8.
- `voiceCull` (a dB threshold) vs `voiceCap` (a count).
- `panLayout` pitch fan vs `panOrder` balanced/fan.
- `bend` is used for both the wheel and the lab's octave/semi/fine.

## Evidence consulted

ROADMAP rows B275, B276, B308, B332, B339, B376 (`origin/lead-records-149`). DECISIONS ADR-186, ADR-187, ADR-189. `docs/audits/2026-09-26-seam-audit.md` (§0, S6–S9, §4). FOUNDATIONS `integrations/hypersaw/response-engine-manifest.md`. `docs/scalpel/ACCOUNTING.md` §1–§2. `docs/design/scalpel-envelope-conflicts.md` (F3). `specs/SPEC-SCALPEL.md` §4–§7 and §13. The four sources above. `docs/design/listening-pass.html` (design language, store and export pattern).

## Alternatives rejected

- **A separate `engine-audit.json` beside the page.** A second copy of the same data drifts. The data is inline, and EXPORT produces the decision file.
- **Committing the builder.** Out of the brief's file scope. It is an open question.
- **Pre-filling decisions from the recommendations.** The brief forbids showing a decision the human did not make. `?demo=recs` shows the manifest AS IF the recommendations were accepted, labelled DEMO and never saved.
- **Assigning ids in the manifest preview.** B308 H4: ids are declared explicitly in the lockfile, never by order.

## Verify

Recorded in the PR (`./verify fast` on the committed hash, read from `.harness/last-verify.json`). This trace is written before that run, so it cannot quote the result.

## Open questions

1. **The builder lives in scratch.** The page is a snapshot at `bae777f`. If the audit must be regenerated when `swarm_core.h` or the composed engine changes, the builder should be committed (e.g. `tools/engine_audit_build.*`) and could become a wired freshness check. That is the lead's call.
2. **Thin evidence.** Every LOCK below is thin evidence (0/39 legacy use, not a listening test): motionCenter, basin, panCurve, panMode, superMode, tilt, hiTame, voiceCull, the aa flags and the pool size. The human should hear them before locking.
3. **mono → width is an unmeasured hypothesis.** It needs a render to show that width 0 equals mono fold.
4. **Undecided before any recommendation can settle:** `n`'s range (1..32 vs 1..9, ACCOUNTING §3) and the envelope law (envelope-conflicts decision 1). Several KEEP rows depend on them.
5. `docs/design/index.html` was not regenerated (outside scope). `tools/labs_preview.sh` regenerates it.
