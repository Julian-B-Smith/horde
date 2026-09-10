# b100-state-header — state header, per-patch engine revision, fixture corpus

- **Queue item:** B100, items (1), (2) and (4); item (3) — goldens v2 — is
  the 1.1 workshop and untouched.
- **Why:** the ratified 1.0 definition puts the compatibility mechanism
  BEFORE anyone but the human saves a set (ROADMAP §"1.0 — DEFINITION OF
  DONE"; 28 Ableton sets already bind by class id, ADR-154). Without a
  header there is nothing to pin a patch's DSP laws to, and without a fixture
  corpus every migrator is a promise.
- **What changed.**
  - Header on every blob. Host chunk: the version line stays the chunk's
    schema, followed by `engine_revision=<n>` and `build=<HYPERSAW_BUILD_ID>`
    as plain key=value lines (a pre-B100 build reads them as unknown keys and
    ignores them — the chunk version does NOT move). JSON preset:
    `"schema":3,"engine_revision":<n>,"build":"<id>"` as the first keys;
    schema stays 3 because the header adds keys, it changes no key's meaning.
  - `Plugin::kEngineRevision = 1`, `patchEngineRevision` (atomic int),
    `engineRevision()` — the ONE read site a future gated law consults — and
    `setEngineRevision()` (the opt-forward hook the GUI control will bind;
    exported headless as `hypersaw_debug_engine_revision` /
    `hypersaw_debug_set_engine_revision`). Load semantics on both transports:
    missing header → 1; present → stored; a value above the latest clamps to
    the latest (a revision with no laws behind it is never stored; a re-save
    records what actually rendered). ADR-103's migrator is intact.
  - `tools/state_check.cpp`: +12 assertions (header pinned in TEXT on both
    transports; header-less → 1; rev-1 round-trip; re-save keeps the pin;
    unknown header keys ignored with params still applying; future revision
    loads and clamps). No existing assertion weakened.
  - `tests/state_fixtures/`: three host-chunk fixtures + goldens
    (`chunk-v2-rev1`, `chunk-v2-noheader`, `chunk-v1`; 352 256-byte
    interleaved float32 renders of 1 s of A3), a README with the append-only
    rule. `tools/gen_state_fixtures.cpp` (refuses to overwrite; asserts
    round-trip AND intent after every load; refuses a silent golden),
    `tools/statefix_check.cpp` (load + bit-identical render per fixture, with
    a planted-masterVol control that must DIFFER — L0032), shared scaffold
    `tools/statefix_common.h` (one render, single-sourced — L0005). Registered
    in CMake, NOT wired into ./verify (protected path; human decision).
- **Finding — JSON fixtures held (out of scope to fix).** Generating the
  preset-path fixtures fired the generator's detector: `applyStateJson`
  enqueues every key into the shell's bounded param queue in one synchronous
  pass (`kQCap = 256`, src/hypersaw_clap.cpp:1130) and a full patch is 323
  keys (241 base + 82 twins; `params->count` = 323) plus the migrators'
  enqueues, so the tail is dropped — `o1.enable` among it, and the render came
  out single-oscillator (rms 0.0966 = chunk-v1's, vs 0.1430 for two). A lone
  `{"o1.enable":1}` applies fine, isolating the cause to overflow. The GUI's
  preset LOAD takes this exact path (src/gui/gui2.html:2030 →
  hostIf.applyStateJson, src/hypersaw_clap.cpp:4893), so this is a shipped
  defect in the preset path, deterministic headless and racy (partial drain
  by the audio thread) in a DAW. A golden rendered from it would pin the
  truncation forever; the JSON fixtures land the moment the path stops
  truncating. The queue is outside this brief's file scope — reported to the
  lead with the fix options (raise kQCap past the key count with headroom, or
  apply directly when idle as state_load already does).
- **Second finding — the first detector shared the fixture's assumption.**
  Key-echo alone passed `json-s1` because the keys the migrator supplies are
  exactly the ones the schema-1 blob omits; the dropped `enqueueParam(1150,
  1)` echoed nothing. The generator now asserts the patch's INTENT after the
  load (both oscillators on; glideMode where the schema says) and json-s1
  correctly fails on BOTH: `o1.enable` 0, and `glideMode` 1 where ADR-103
  says 2 — the migrator's own `enqueueParam(90, 2)` is issued after the 323
  keys and is dropped too, so on a full preset through the GUI the ADR-103
  migrator does not run. The tainted pair was removed before commit. Also
  re-learned the hard way: the first rerun used a STALE generator binary
  (the rebuild was inside a hook-blocked command) and the "fixed" detector
  passed json-s1 again — verify the object, not the source (L0032, L0042).
- **Evidence consulted:** ROADMAP B100 row + §1.0 definition;
  traces/2026-09-10-one-point-oh-proposal.md; DECISIONS ADR-082/103/138/154;
  src/hypersaw_clap.cpp state_save/state_load (4570–4720 pre-edit),
  stateJson/applyStateJson (3021–3150 pre-edit), enqueueParam/drainQueue
  (2396–2420), kQCap (1130); tools/state_check.cpp; CMakeLists.txt build-id
  plumbing (60–86); verify gates (git grep -I skips binaries, so .f32 goldens
  are safe under leak_gate/portability_gate); LIBRARY L0003/L0004/L0005/
  L0032/L0044.
- **Alternatives rejected:** bumping the chunk version or JSON schema for the
  header (would make every existing session unloadable by the previous build
  for no semantic gain); storing an unknown future revision verbatim (would
  round-trip a value this build cannot render — chose clamp, flagged for the
  ADR); a stub host whose request_flush drains synchronously to make the JSON
  fixtures pass (certifies a host behaviour no real host has — L0031); a
  Fixture-hash golden instead of raw float32 (the brief asks for raw, and raw
  gives a first-differing-frame diagnostic).
- **Verify:** `./verify fast` exit 0 (git f472c96, ts 2026-09-10T13:17:03Z);
  `./verify full` exit 0 (.harness/last-verify.json: target full, git
  f472c96, ts 2026-09-10T13:21:13Z; zero FAIL lines; tail `time_check:
  GREEN (0 failures; worst parity rms 5.5853e-12)`). statefix_check
  tests/state_fixtures: GREEN (3 fixtures, 0 failures), run by hand — not a
  verify gate.
- **Open questions:** (1) the kQCap truncation — whose fix, and should the
  JSON fixtures be a follow-up on the same branch once it lands; (2) clamp vs
  preserve for a future revision (ADR text proposed in the report);
  (3) whether statefix_check joins ./verify full (recommended: yes, beside
  state_check; it is ~1 s per fixture) and gen_state_fixtures stays manual;
  (4) cross-compiler bit-identity of the goldens is unmeasured — the README
  says so; (5) the corpus should grow from the notice-001 inventory (real
  chunks extracted from the 28 .als sets) — a human-held asset.
