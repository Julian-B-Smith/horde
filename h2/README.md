# h2/ — horde 2's cores

This directory holds the DSP of **horde 2**, the new plugin shell ratified in
ADR-186. The legacy shell in `src/` is frozen. horde 2 is a separate product
with its own identity, and it shares no code with legacy.

**The plan (ROADMAP B385, the human 2026-10-01: "Confirming clean-port path").**
horde 2 gets ONE engine: the composed engine (horde's swarm driving SCALPEL's
blades), a clean C++ port at `h2/engine/` (namespace `horde2::engine`) written
fresh against the composed JS golden. The two proven ports under `h2/cores/` are
now TEST REFERENCES, not product code. They are never edited, and the engine
copies their proven code instead of including them:

- `cores/scalpel/razor_core.h`, the blade oracle's port (phase 1a);
- `cores/swarm/`, the lifted legacy swarm core (B379 step 1).

So nothing drifts between two SCALPELs. The design, what is copied from where,
and the parity plan are in `docs/port/h2-engine.md`.

Last verified: 2026-10-01 (B385 rework after the critic: `h2/engine/` built; 540 of 543 parity scenarios green, the parity check not yet wired, pending the human's ring ruling).

## Rules

Rules 1, 2, 7 and the status row's pin are enforced by `tools/h2_rules_check.py`
in `./verify fast`, and rule 8 by `tools/h2_lift_check.py`, also in `./verify fast`.
The rest are enforced by each core's parity check.

1. **Copied forward, never shared (ADR-186 item 4).** A core enters horde 2 by
   being copied into `h2/cores/<core>/` under the C++ namespace
   `horde2::<core>`. Nothing here includes a file from `src/`, and nothing in
   `src/` includes a file from here. The namespace is required: legacy's cores
   are header-only with the same class names, so a link that saw both copies
   would be a silent ODR violation. No translation unit includes both a legacy
   core and an h2 core (`h2_rules_check` rule 4, which scans `tools/` too).
   - The lifted swarm core keeps its legacy name NESTED,
     `horde2::swarm::hypersaw` (so its own `hypersaw::` and `forcecore::`
     references resolve unedited; ledger entry NS1). Code outside it (the
     test tools that use it as a reference) always spells the name in full,
     `horde2::swarm::hypersaw::`, never a bare `hypersaw::`, which names legacy.
   - The engine, `h2/engine/`, lives in its own namespace, `horde2::engine`. It
     COPIES from the cores and includes none of them (B385), so the cores stay
     test references that nothing in the product depends on.
   - One name escapes every namespace: the macro `HZ_CULL_ENV` (`swarm_core.h`,
     the B38 sizing instrument). Both copies define it under the same
     `#ifndef`, so a `-DHZ_CULL_ENV=...` build moves legacy and h2 together.
2. **No target links legacy.** A CMake target that builds an `h2/` core never
   links `${PROJECT_NAME}-impl` (see the `horde 2 cores` block in
   `CMakeLists.txt`).
3. **Correctness is parity first (ADR-187).** Until a core's JS is demoted by a
   human ruling, correctness means parity with the core's named JS target on
   every scenario that no ratified divergence claims. Parity has three parts:
   RMS < 1e-6, a stated max-abs bound, and identical blade-event counts and
   times. It is measured in a parity build: doubles, `-ffp-contract=off`, and
   JS number semantics ported literally (`Math.round` → `floor(x + 0.5)`,
   NaN-propagating `min`/`max`, JS truthiness).
4. **Divergences are ledgered, never silent (ADR-187 item 5).** A deliberate
   change from the JS is a `divergences.json` entry, mirrored into the lab JS
   where the browser can follow it. Where it cannot, the entry is marked
   `js_limit`. The composed engine's ledger is `docs/port/divergences.json`
   (checked by `tools/labharness/divergence_ledger_check.mjs` in `verify fast`).
   As of 2026-10-01 it holds 7 entries:
   - **D1, D2 and D3** (ADR-189 with A1): built, default OFF;
   - **M1, M2 and M3** (B382): built, default ON;
   - **D4** (ADR-189): planned. It changes the random-patch sampler, not the
     engine.

   All six built ones are in the composed JS, so they are part of the engine's
   parity target. The B378 fixes enter later, one ledgered divergence each.
5. **Real-time safe by construction.** All state is preallocated in the core
   object. `render()` allocates nothing, locks nothing and reads no clock.
   Randomness comes only from seeded streams.
6. **CPU is Layer-E (ADR-187 item 8).** CPU is measured by hand, in Release, by
   an unwired `tools/measure_*` bench. It is never a gate.
7. **Every build of h2 code is compiled with `-ffp-contract=off`:** the cores and
   `h2/engine/` (the rules check treats both as h2 code). Ruled by
   the human on 2026-09-30 (ROADMAP B332): the build that ships is arithmetically
   the build that passed parity. Clang's default (`-ffp-contract=on`) fuses
   `a*b+c` into one rounding, which V8 never does. At that default, the SCALPEL
   core misses parity on 19 of 386 scenarios (worst rms 2.2e-6, max 1.5e-4;
   measured 2026-09-28 at -O2 and -O3). The one declared exception is
   `h2_scalpel_fma_control`, the must-fail control whose job is to be the
   contracted build. The cost of the
   rule at -O3 is +6% CPU on the heavy class and none on the light one; see
   `docs/port/scalpel-phase-1a.md`.
8. **A lifted core is its legacy source plus a lift ledger, byte for byte.** A
   core lifted from `src/` (ADR-186 item 4) keeps every line of its original
   except the ones its `lift-ledger.json` lists: the namespace lines, and any
   edit the lift could not avoid, each with its reason and its proof. A
   byte-identical lift carries the legacy core's parity proof (ADR-187, L3).
   - **The one path for every later edit** (the lead, 2026-09-30): each change
     to a lifted copy is a hunk in its `lift-ledger.json`. Lift edits are
     `E<n>`. Divergences, such as the B378 fixes, are `kind: "divergence"`
     hunks that each carry their ADR-187 id from the `divergences.json` beside
     the ledger.
   - The gate rebuilds the copy from the current source plus every hunk, as
     bytes, and demands equality. So it stays exact after the first divergence
     instead of being loosened by it.
   - Multi-line changes use the `patch` op (an exact old block becomes a new
     block).
   - A moved source (its git blob is no longer the ledger's `src_blob`) is its
     own verdict: re-lift deliberately.
   - **The non-divergence entries are FROZEN per file** (the lead, 2026-09-30,
     after the critic's N1). `tools/h2_lift_check.py` holds the list: swarm
     `NS1`, `NS2`, `NS3` and `E1`; the parity tool `T1` and `T2`. Every other
     hunk must be `kind: "divergence"` with a `divergences.json` id, whatever it
     calls itself; an `edit` outside the list is red.
   - Adding a new non-divergence edit is a deliberate RE-LIFT. The frozen list
     changes together with a re-pin of `src_blob` and `lifted_at`, recorded by
     the lead in ROADMAP. This closes the path around the divergence ledger that
     the critic demonstrated: a `dissolve` clamp change labelled
     `{"id": "E2", "kind": "edit"}` passed the gate.

## Status

| Core | Directory | Status | Parity target | Oracle |
|---|---|---|---|---|
| Swarm (Swarm Core 1) | `cores/swarm/swarm_core.h` + `force_core.h` + `glide_core.h` (`horde2::swarm::hypersaw::SwarmCore`) | LIFTED 2026-09-30 (B379 step 1) from `src/` at `1c421b5`: byte-identical apart from 3 namespace lines and one access-only edit (E1, a public `tickVoice()` forwarder to the private `controlTick`), listed in `cores/swarm/lift-ledger.json`. It carries the legacy chain's proof against SwarmSynth (156/156 within 1e-6, B378 audit §2.9). The legacy L0-1 chain is DUPLICATED onto the copy under h2 flags (`-O2 -ffp-contract=off`): 156/156 within 1e-6 against SwarmSynth's goldens, worst 2.485e-9 @ saw-glass.seed1234 (2026-09-30). A TEST REFERENCE since B385 (not the composed engine's swarm: `h2/engine/` carries SwarmSynth's law with M1–M3 and copies this core's constants). At 48 kHz (B382's golden set) it is within 1e-6 of SwarmSynth with the divergences the composed engine registers mirrored (`docs/port/divergences.json`; negative onset and law 3 are reported as KNOWN differences until M2 and M3 are registered), and of DynSynth with M1's coefficient in its smoother. | Legacy: `reference/swarmsaw.html` SwarmSynth (44.1 kHz goldens, `tools/golden/gen_goldens.mjs`). | `tools/h2_swarm_parity_check.cpp` (full; a ledgered copy of `tools/parity_check.cpp`); `tools/h2_lift_check.py` (fast); `tools/h2_swarm_lift_check.cpp` (full); `tools/h2_swarm48_check.cpp` (full, 48 kHz, B382). |
| SCALPEL blade engine | `cores/scalpel/razor_core.h` (`horde2::scalpel::RazorCore`) | A TEST REFERENCE since B385 (the engine copies its blade path). JS-normative. Parity-proven against the blade oracle (phase 1a): 383 of 386 scenarios at parity, 3 excluded as chaotic (a pinned count) with evidence re-measured every run. | Phase 1a: `reference/scalpel/prototype/razor-core.js@0ce6a713410d89c65bf55f761f1dc791fae61b16` (git blob; v1.1 as ingested). | `tools/h2_scalpel_parity_check.cpp` + `tools/h2_scalpel_render.mjs`, in `./verify full`. |
| Composed engine (horde 2's one engine) | `engine/` (`horde2::engine::Engine`: `js.h`, `swarm.h`, `blade.h`, `engine.h`) | BUILT 2026-10-01 (B385 checkpoint 2; `docs/port/h2-engine.md`). JS-normative. 540 of 543 scenarios at parity (rms < 1e-6, max-abs < 1e-6, events and load readouts identical; worst rms 2.1e-12, worst max-abs 1.8e-10), and the mean bit-exact share is 37.00% (proposed floor 30%, keyed darwin-arm64 / Apple clang 16 / Node 24). The 3 Cross-mod ring (watch) rows are chaotic (ADR-065): 2 are excluded with evidence under the 1a rule, and the arp misses RMS (1.7e-6). The ring criterion, and so the wiring, is the human's ruling (rule b is built and selectable). M1–M3 are built in; D1–D3 are flags, default off. | The composed JS at main `c64cfdb`: `docs/design/scalpel-horde-engine.js@581d7942684c91245e4a6637dd40d137335b5d69` over the blade oracle (the pin in the row above) and `reference/swarmsaw.html@e47da6c9e0b4a058e18d79f62d71ab31c3d3b1b0` (SwarmSynth), with `reference/scalpel/data/presets.json@44b48d72a9bed4717edd0ac5cf9ef4b8d7b0de93` and the B366 presets in `docs/design/scalpel-interface-lab.html@6abf848e91065bc33275724a6967a7399ba40a07`. The ledger's defaults: M1–M3 on, D1–D3 off. | `tools/h2_engine_parity_check.cpp` + `tools/h2_engine_render.mjs` (UNWIRED until the ruling; its FMA control is `h2_engine_fma_control`). |

The full account of what is ported, what is not, and which oracle quirks are
divergence candidates is in `docs/port/scalpel-phase-1a.md`. The swarm lift and
the questions that stopped a composed layer over `razor_core.h` are in
`docs/port/phase-1b.md`; the human's re-scope (B385) answered them with the one
engine, whose design is `docs/port/h2-engine.md`.
