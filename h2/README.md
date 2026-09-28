# h2/ — horde 2's cores

This directory holds the DSP cores of **horde 2**, the new plugin shell ratified in
ADR-186. The legacy shell in `src/` is frozen. horde 2 is a separate product
with its own identity, and it shares no code with legacy.

Last verified: 2026-09-28 (B332 phase 1a).

## Rules

1. **Copied forward, never shared (ADR-186 item 4).** A core enters horde 2 by
   being copied into `h2/cores/<core>/` under the C++ namespace
   `horde2::<core>`. Nothing here includes a file from `src/`, and nothing in
   `src/` includes a file from here. The namespace is required: legacy's cores
   are header-only with the same class names, so a link that saw both copies
   would be a silent ODR violation.
2. **No target links legacy.** A CMake target that builds an `h2/` core never
   links `${PROJECT_NAME}-impl` (see the `horde 2 cores` block in
   `CMakeLists.txt`).
3. **Correctness is parity first (ADR-187).** Until a core's JS is demoted by a
   human ruling, correctness means parity with the core's named JS target on
   every scenario that no ratified divergence claims. Parity has three parts:
   RMS < 1e-6, a stated max-abs bound, and identical blade-event counts and
   times. It is measured in a parity build: doubles, `-ffp-contract=off`, and
   JS number semantics ported literally (`Math.round` → `floor(x + 0.5)`,
   NaN-propagating `min`/`max`, JS truthiness). The parity build is a separate
   target from any Release build.
4. **Divergences are ledgered, never silent (ADR-187 item 5).** A deliberate
   change from the JS is a `divergences.json` entry in the core's directory,
   mirrored into the lab JS where the browser can follow it. Where it cannot, the
   entry is marked `js_limit`. There are no divergences yet.
5. **Real-time safe by construction.** All state is preallocated in the core
   object. `render()` allocates nothing, locks nothing and reads no clock.
   Randomness comes only from seeded streams.
6. **CPU is Layer-E (ADR-187 item 8).** CPU is measured by hand, in Release, by
   an unwired `tools/measure_*` bench. It is never a gate.

## Status

| Core | Directory | Status | Parity target | Oracle |
|---|---|---|---|---|
| SCALPEL blade engine | `cores/scalpel/razor_core.h` (`horde2::scalpel::RazorCore`) | JS-normative. Parity-proven against the blade oracle (phase 1a): 369 of 372 scenarios at parity, 3 excluded as chaotic with evidence re-measured every run. | Phase 1a: `reference/scalpel/prototype/razor-core.js` (v1.1 as ingested). Phase 1b: the composed engine, `docs/design/scalpel-horde-engine.js` at `c79be56` (B332, ADR-187 item 3). | `tools/h2_scalpel_parity_check.cpp` + `tools/h2_scalpel_render.mjs`, in `./verify full`. |

The full account of what is ported, what is not, and which oracle quirks are
divergence candidates is in `docs/port/scalpel-phase-1a.md`.
