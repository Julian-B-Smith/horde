# h2/ — horde 2's cores

This directory holds the DSP cores of **horde 2**, the new plugin shell ratified in
ADR-186. The legacy shell in `src/` is frozen. horde 2 is a separate product
with its own identity, and it shares no code with legacy.

Last verified: 2026-09-30 (B379, B332 phase 1b step 1: the swarm core lifted; the contraction rule ruled).

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
     references resolve unedited; ledger entry NS1). Code outside it, the
     composed layer first, always spells the name in full,
     `horde2::swarm::hypersaw::`, never a bare `hypersaw::`, which names legacy.
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
   change from the JS is a `divergences.json` entry in the core's directory,
   mirrored into the lab JS where the browser can follow it. Where it cannot, the
   entry is marked `js_limit`. There are no divergences yet.
5. **Real-time safe by construction.** All state is preallocated in the core
   object. `render()` allocates nothing, locks nothing and reads no clock.
   Randomness comes only from seeded streams.
6. **CPU is Layer-E (ADR-187 item 8).** CPU is measured by hand, in Release, by
   an unwired `tools/measure_*` bench. It is never a gate.
7. **Every build of an h2 core is compiled with `-ffp-contract=off`.** Ruled by
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
| Swarm (Swarm Core 1) | `cores/swarm/swarm_core.h` + `force_core.h` + `glide_core.h` (`horde2::swarm::hypersaw::SwarmCore`) | LIFTED 2026-09-30 (B379 step 1) from `src/` at `1c421b5`: byte-identical apart from 3 namespace lines and one access-only edit (E1, a public `tickVoice()` forwarder to the private `controlTick`), listed in `cores/swarm/lift-ledger.json`. It carries the legacy chain's proof against SwarmSynth (156/156 within 1e-6, B378 audit §2.9). The legacy L0-1 chain is DUPLICATED onto the copy under h2 flags (`-O2 -ffp-contract=off`): 156/156 within 1e-6 against SwarmSynth's goldens, worst 2.485e-9 @ saw-glass.seed1234 (2026-09-30). NOT yet the composed engine's swarm: at 48 kHz it differs from SwarmSynth (M1, M2 in `docs/port/phase-1b.md`), pending the human. | Legacy: `reference/swarmsaw.html` SwarmSynth (44.1 kHz goldens, `tools/golden/gen_goldens.mjs`). Phase 1b: the composed engine, not yet re-pinned. | `tools/h2_swarm_parity_check.cpp` (full; a ledgered copy of `tools/parity_check.cpp`); `tools/h2_lift_check.py` (fast); `tools/h2_swarm_lift_check.cpp` (full). |
| SCALPEL blade engine | `cores/scalpel/razor_core.h` (`horde2::scalpel::RazorCore`) | JS-normative. Parity-proven against the blade oracle (phase 1a): 383 of 386 scenarios at parity, 3 excluded as chaotic (a pinned count) with evidence re-measured every run. | Phase 1a: `reference/scalpel/prototype/razor-core.js@0ce6a713410d89c65bf55f761f1dc791fae61b16` (git blob; v1.1 as ingested). Phase 1b: the composed engine, `docs/design/scalpel-horde-engine.js` at `c79be56` (B332, ADR-187 item 3). | `tools/h2_scalpel_parity_check.cpp` + `tools/h2_scalpel_render.mjs`, in `./verify full`. |

The full account of what is ported, what is not, and which oracle quirks are
divergence candidates is in `docs/port/scalpel-phase-1a.md`. Phase 1b's first
step, the swarm lift, and the questions that stop the composed layer are in
`docs/port/phase-1b.md`.
