# B89 phase 2 — the intent-bus resolver behind a flag, with the prototype as parity oracle

**Provenance.** Planning agent (read-only, Opus) dispatched by the HYPERSAW lead, 2026-09-18, on the lead's brief; the lead edited nothing but the framing. Motivating decisions: ADR-152 (intent bus ingested), ADR-173 (phase 1, parameter classes), the ROADMAP priority track 2 ("classification PR first" — done). Status: a PROPOSAL — its numbered rulings are the human's; nothing here is built.

## 0. The seam, in one paragraph

The control-rate tick is `hypersaw_clap.cpp` ~5283 — `if (morphOn > 0.5) morphStep(...); modStep(...);` — both gated on the same 5.805 ms gravity grid (`swarm_core.h`, `kGravGridSeconds = 256/44100`) but on separate accumulators. The resolver goes **inside `morphStep`, as one early branch**: `if (intentBusOn && morphOn > 0.5) { intentStep(samples); return; }`. Not beside `morphStep`, not into `modStep`. SPEC §4.5's tiers *are* today's two functions read as one: `morphStep` produces the corner value (= `base[p]`), `applyParam`'s ADR-136 intercept records it as the modulation base, and `modStep` adds `base + delta·span` clamped to the parameter's declared range — exactly §4.5's device-tier routings with `respectRange` off. The resolver inserts the two tiers horde lacks (intent bindings, the corner-range clamp) between them and leaves the matrix where it is. With the flag off, `morphStep` executes the identical instruction sequence past one predictable branch and `modStep` is untouched — the bit-identity gate is structural.

**Every function that branches on the flag:** `morphStep` (one early `if`); `applyParam` and `readParam` gain a row for the flag param only, no branch. Nothing else: the resolver's only observable is the value it writes through the existing `applyParam` choke point with `morphFromField` set; every other consumer of the field reads `morphCorner[k][i]`, which the resolver never changes (`morphRouteEdit`, `morphCapture`, `morphOwnersJson`, `applyMorphChunk`/`morphSlotMap`, the ADR-160 undo marks, `modStep`'s macro suspension — all correct with zero edits).

## 1. Data model mapping (SPEC §3 → what horde already stores)

| SPEC §3 | Today | New? | Persists where |
|---|---|---|---|
| `Corner.base[p]` | `morphCorner[k][i]`, raw units, morphIds order | no | `morph=` chunk |
| `Corner.name` | `cornerNamesJson()` | no | `"cornerNames"` |
| `Corner.range[p]` | — | NEW | new `intent=` chunk, normalized, sparse |
| `Corner.bind[i][p]` | — | NEW | new chunk, normalized, sparse |
| `Corner.home` | — | NEW | new chunk |
| `Corner.request[s]` | `morphCorner[k][i]` for a stepped id | no | same chunk |
| `Corner.curve[i][p]` | — | not built (§10: prototype linear only) | — |
| `Corner.macroRest[i]` | — | not built in phase 2 | — |
| `Corner.mods` | — | not built in phase 2 (R5) | — |
| `Device.morph{x,y}` | ids 152/153 | no | params |
| `Device.steepness` | `morphTemp` (154); steepness = 1/temp (R1) | no | param |
| `Device.seeds[a]` | `MorphCore.g[i][k]` from `morphSeed` (156) via `reshuffle` | no | param 156 |
| `Device.intents[i]` | `macroVal[8]` (166–173) + pad axes | no | params |
| `Device.puck/latch` | — | NEW, device-only, not persisted | — |
| `Device.globalMods` | `mod.routes` + `modroutes=` chunk | no | existing |
| exempt / atomic groups | `morphExempt`, `morphLead` | no | existing |

**Units.** `base` stays raw; `bind` and `range` are stored normalized; the resolver converts at the boundary with `modStep`'s `span = maxV − minV` convention. This keeps the existing corner chunk byte-untouched and every ADR-159 remap out of scope.

**The chunk.** New key `intent=` in `state_save` / `"intent"` in `stateJson`, the `modroutes=`/`routing=` shape: emitted only when some corner leaves the defaults (a patch with no bindings writes no key), parsed when present, loadable when absent (absent ⇒ `range=[0,1]`, `bind=0`, `home={0.5,0.5}`). The fragment carries `"intentLayout":1` and `"intentOrder":["X","Y","M1"…"M8"]` — a stored order, not a comment (the ADR-159 lesson applied before the fact).

**Phase 3 needs (§9.2–9.5):** `bind[X]`/`bind[Y]` writable per corner from day one (covered); "absent = default" parsing (covered); §9.4 needs nothing if route storage does not move (R5 guarantees); **§9.5 is already satisfied** — `morphSeed` is a persisted param and `reshuffle` is deterministic from it. Nothing is stored twice.

## 2. The parity oracle

**Reference functions** in `reference/intent-bus.html`: `weights(x,y,k)` (§4.2), `pick(seed,w)` (§4.3), `mulberry32` + `reshuffle()` (§3.5; draw order: the 8 params, then `home`, then `unison`), `resolve(dt)` (§4.1–4.5), the `commit` click handler (§7), `intentOwner`/`intentLiveTargets` (§5 — phase 3).

**Extraction obstacle.** `tools/golden/extract_core.mjs` needs `/* ===== DSP: … */` … `/* ===== Audio graph */` banners; the intent lab uses `/* ---------- model ---------- */` … `/* ---------- audio ---------- */`. A sibling `extract_intent.mjs` with those markers. The model region is DOM-free and self-contained; `commit` is not (it calls `buildEditor()` and touches `S.editing`). Options: (a) extract the commit lines as a second slice with the two UI calls stubbed; (b) reimplement §7 in the generator and declare T6 spec-parity rather than oracle-parity. Recommend (a).

**Fixture shape:** one JSON per case under `build-golden/intent/` plus a manifest: steepness, seed, ticks, dt, four corners (base, range, bind per intent, home, unison, an LFO), morph position, intents, puck, latch, the global LFO, and `expect` (weights, owner map, homeOwner, unisonOwner, `final`, `clamped`). Determinism: every phase set explicitly, `dt = 0.016`, fixed tick count; `gen_intent_goldens.mjs --selfcheck` re-runs and compares. C++: `tools/intent_check.cpp` drives `IntentCore` from the same inputs, compares `final[p]` at 1e-6 absolute; standalone, unwired (ADR-171 is the wiring route).

**§12 tests by phase, with the must-fail control each needs:** T1 lock — phase 2 (widen the range: the same sweep must move the value). T2 — value half in 2, tint in 3 (one non-zero depth: exactly one parameter moves). T3 — phase 3 for picker contents; phase 2 asserts the device route to MorphX moves `morphEff` (depth 0 leaves the owner map identical). T4 — 2 (small temp collapses to one corner; the second binding goes silent). T5 — 2 (break the lead map: members report different owners). T6 — 2 (bake into the wrong corner: before/after differ). T7 — 3 (no corner-scope routings in phase 2). T8 — spring/retarget in 2, pad in 3 (latch on: the puck must not return). T9 — 2 (different seed → different owner map; the four exact corners unchanged). **T9 carries a known spec defect** (the 2026-09-10 amendment): under a seeded `reshuffle()` the check must read *same seed → identical; different seed → different*.

## 3. Rulings (R1, R2, R4, R6, R7, R13, R15, R16 before code)

- **R1 — the resolution law.** The shell's Gumbel-max over `log(w)/T` samples `w^(1/T)` — §4.2's `w^steepness` with steepness = 1/morphTemp: the same DISTRIBUTION, not the same SAMPLER (owner maps and flip curves differ per seed), and the shell's `morphCoup` (155) has no spec analogue. **Recommend: keep the shell's law**; parity at 1e-6 on the evaluation half (§4.5, the clamp tiers, commit) with the owner map as fixture input; the owner half proved distributionally (seeded 10⁵-draw χ² against `w^(1/T)`) and by an equivalence note in the ADR. Alternative's cost: `morphCoup` becomes a dead control and every patch's flip topology moves the instant the flag is on. Range note: morphTemp 0.02–4 ⇒ steepness 0.25–50; the spec's default 8 ⇒ temp 0.125; today's default temp 1 = the spec's softest setting.
- **R2 — the atom is a lead group, not a parameter.** Atoms = the distinct values of `morphLead[]` (the scale is one atom of 13, each FX slot one of 3), plus `home`, plus later each `macroRest`. Alternative's cost: both measured chimeras (ADR-109 A1, ADR-124) return.
- **R3 — `owner[s]` applies the request in full vs ADR-125 ARGMAX.** Identical for a single structural parameter (shipped: the `stepped` branch of `morphStep`); they differ in scope — ADR-125 ruled on topology as a whole, which is R4.
- **R4 — the routing cells (live contradiction, independent of B89).** ADR-125 says "all route ids point at one lead index"; `morphInit` appends the cells citing ADR-125 for the opposite ("the field blends the coefficients as VALUES — no argmax") and leaves `morphLead` at identity for the block, so under the shipped default (quantum) every crosspoint cell picks its own corner; `routing_check` assertion 11 tests blend mode only. **Recommend: the crosspoint block is ONE atomic group under quantum; it blends cell-wise under BLEND.** A half-owned table is a topology neither corner authored, and under ADR-175 a mixture of two acyclic tables can be cyclic — a CPU cliff and a processing mode no corner declared. One line in `morphInit` plus one assertion; its own row.
- **R5 — the mod matrix.** Today's route sum with polarity, clamped to the declared range, IS §4.5's device tier with `respectRange` off. **Phase 2 builds the intent tier and the corner-range clamp only**; route depth (161) stays device (closes ADR-173 R2 for now); §6.2 corner-scope routings and §6.3 promotion are phase 3 — the prototype's corner LFO has no shell counterpart (B16 unbuilt), so that tier is untestable against the oracle in phase 2. `IntentCore` reserves a `cornerModSum[p]` input, zero in phase 2.
- **R6 — intents.** I = 10: `X`, `Y`, and the eight macros (166–173) re-read as intents `M1…M8` (phase 1 already classed them "intent value"); ADR-152's suspension already makes a macro drive intents OR routes. Do not conflate with ADR-169's four module roles (a module tier). **The intent order is append-only**, hence `"intentOrder"` in the chunk.
- **R7 — B133's rail vs the performance pad: two different pads.** §4.4's pad is a PERFORMANCE pad whose `home` flips with the morph; `morphX/Y` (152/153) are the MORPH pad, which B133's rail drives. **Recommend: the MAIN pad (ADR-150) becomes the performance pad**; the morph pad stays; the rail never touches the performance pad. ADR-156's per-osc pads are untouched.
- **R8 — binding inheritance vs copying (§11.1).** Copy (the spec's own), sparse chunk, a per-corner "reset to device default" in phase 3.
- **R9 — armored parameters (§11.2).** Defer to phase 3; the device tier already clamps to the declared range.
- **R10 — `home` as one atom (§11.3).** One atom.
- **R11 — baking corner-mod contributions on commit (§11.5).** No; free in phase 2.
- **R12 — tint weighting (§11.6).** Phase 3; rule now: current contribution `Σ|intent·depth|`.
- **R13 — the flag.** `{266, "intentBus", "Intent Bus (dev)", 0, 1, 0, true, kOffOn}` appended after 265, in `kGlobalIds` and `kParamClassOverrides` as Device; id 266 is free (ADR-169 reserves 300–331). One flag; its meaning changes between 2b (shadow) and 2c (applied), harmless for a dev param defaulting off.
- **R14 — `IntentCore`'s shape.** `src/intent_core.h`, the `ModCore` pattern (pure functions over caller-owned spans), not the `MorphCore` fat struct: the bind table is ~78 KB and scales with the intent count, so the shell owns the storage (sized once in `morphInit`) and the core owns the law.
- **R15 — the flag with morph off: nothing.** Bindings live in corners; with no field there is no owner. Narrows the bit-identity proof to morph-on patches.
- **R16 — the morph glide stays.** The resolver computes §4.5's `final[p]`; the existing one-pole (`morphGlide`, 158, "THE morph rate") carries `morphCur[i]` toward it. Logged as a deliberate divergence from §4.5's instantaneous evaluation.

## 4. Dispatch plan — strictly 2a → 2b → 2c → 2d

- **2a — core, golden generator, parity check; no flag, no shell change.** Files: `src/intent_core.h`, `tools/golden/extract_intent.mjs`, `tools/golden/gen_intent_goldens.mjs`, `tools/intent_check.cpp`, `CMakeLists.txt`, trace. Acceptance: framework-free, allocation-free, seeded-RNG-only core over caller-owned spans; reproduces the prototype's `resolve()` at 1e-6 over ≥ 24 fixtures covering T1, T2 (value half), T4, T5, T6, T8 (spring/retarget), T9 with the named must-fail controls; the generator is deterministic (`--selfcheck`) and extracts from the protected HTML at run time, never copies it; `intent_check` standalone; `./verify full` exactly unchanged. Risk LOW (extraction is the one real risk).
- **2b — the flag, the chunk, the seam wired with output unused: the bit-identity gate.** Files: `src/hypersaw_clap.cpp` (row 266, `kGlobalIds`, class override; the `intent=` chunk; `morphInit` sizing; `intentStep` into a shadow array; debug exports `hypersaw_debug_intent_final/_bind/_range`), `tools/intent_check.cpp`, `docs/presets/factory/**` regenerated, `CMakeLists.txt`. Acceptance: flag off ⇒ bit-identical renders, proven three ways (parity 156/156, `statefix_check`, and a must-fail control that plants the shadow output at flag 0); flag on with zero bindings/full ranges/zero intents ⇒ shadow equals `morphStep`'s output to 1e-12 at 200 morph positions; the chunk round-trips and loads when absent; `paramclass_check` green with the new row; `rtsafety_probe` green. **The trap:** adding one `kParams` row changes every saved preset's bytes, so `bank_check` fails ×40 — the sanctioned fix is regeneration (cleared twice already); the claim is bit-identical RENDER, not bytes. If the human reads the gate as "no file regenerated", the flag cannot be a parameter and loses its automation lane — the human's decision, in the brief. Risk MEDIUM.
- **2c — apply the resolver when the flag is on.** `final[p]` reaches the engine through `applyParam` with `morphFromField` set, no second write path; the glide one-pole still carries it (R16); T1/T4/T5/T6/T9 measured through the plugin with must-fail controls; flag off re-proven; `morphRouteEdit`/`morphCapture`/exempt/undo marks unchanged; no allocation. Risk HIGH — hazards: R16, and the corner clamp vs `depLiveInCorner`'s ADR-108 hold (a held parameter's clamp must be the OWNER's range).
- **2d — the intents: macros and the performance pad.** §4.4 verbatim (home, X/Y from puck displacement, reach ±0.5), the prototype's spring constants at control-rate dt against a golden trajectory at 1e-6, T8 through the plugin, ADR-152's macro suspension pinned (a macro route contributes exactly 0 while the flag and morph are on). Risk MEDIUM; **R7 must be ruled first**.

## 5. Cost and RT

Field length N = 243 slots (224 ADR-159 prefix + 18 routing + 1 dry path); I = 10; 4 corners; control rate 172.3 Hz. New work per tick ≈ 3,200 ops (the N×I binding sum is 2,430 of it) ≈ 550 kop/s, under 0.05 % of a core — about a fifth of the existing `depLiveInCorner` scan. All state sized once in `morphInit` (the same lazy-once site as `morphCorner`), ≈ 95 KB in doubles (`bind[4][10][243]`, `range[4][243][2]`, `home[4]`, the shadow), doubles because the 1e-6 absolute tolerance is uncomfortably close to a ten-term float32 sum.

## 6. The three riskiest assumptions

1. That the Gumbel-max owner law can be declared distributionally equal to the spec's walk and the oracle narrowed to the evaluation half (R1). Ruled the other way, 2a's fixtures change, `morphCoup` dies, and every patch's flip topology moves the moment the flag is on.
2. That the prototype's model block extracts headlessly and `commit` does not — established by reading, not running. 2a's first hour is `node -e` against the extractor.
3. That "bit-identical with the flag off" survives one new `kParams` row — as renders yes, as bytes no; the bank must be regenerated.

## Two findings independent of phase 2

- `morphInit`'s routing-block comment cites ADR-125 for the opposite of what ADR-125 says, and the shipped default picks each crosspoint cell independently under quantum (R4). Its own row.
- SPEC-INTENT-BUS §9.5 is already satisfied by `morphSeed` + `reshuffle`.
