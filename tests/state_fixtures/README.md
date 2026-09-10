# State-fixture corpus (B100 item 4)

Real state blobs, one per wire shape the loader accepts, each with a golden
render. `tools/statefix_check.cpp` loads every fixture into a fresh instance,
renders 1 s of A3 through the CLAP process path, and requires the float32
stream to equal the golden **byte for byte**. This is the "load + render
bit-identical forever" promise behind every migrator: without it a migrator
is a claim about the past that nothing checks.

## The rule

**A fixture is never edited or regenerated — only added.** A fixture that
changes is no longer evidence about the blobs that already exist in the
world. `tools/gen_state_fixtures.cpp` refuses to overwrite (it skips and says
so). If a change to the shell turns a fixture red, the change altered how an
existing session sounds; that is the finding, and it needs an ADR and a
revision bump (B100 item 2), not a new golden.

New shapes get new files. Suggested names: `<transport>-<schema>-<what>-<date>`
for real captures, e.g. `chunk-v2-inventory-set07-2026-09-12.txt`. The
transport is the extension: `.txt` is a host chunk (`hypersaw-state N`), `.json`
is a preset (`{"plugin":"HYPERSAW","schema":N,...}`). The golden is the same
stem with `.f32`: interleaved L/R float32, 44 032 frames (172 blocks of 256 at
44.1 kHz), note A3 at block 0, held throughout.

## What is here (generated 2026-09-10 by `gen_state_fixtures`)

| fixture | shape | what it exercises |
|---|---|---|
| `chunk-v2-rev1.txt` | chunk v2 + B100 header (`engine_revision=1`, `build=`) | the current session format |
| `chunk-v2-noheader.txt` | chunk v2, no header | every DAW session saved before 2026-09-10 loads as revision 1 |
| `chunk-v1.txt` | chunk v1, osc-0 keys only, no routes | one-oscillator sessions (pre-ADR-082) |

**Held, not yet in the corpus — the JSON preset fixtures** the generator is
written to emit (`json-s3-rev1`, `json-s3-noheader`, `json-s2`, `json-s1`:
the current preset, the header-less preset, and the ADR-103 / pre-note-lane /
pre-ADR-100 migrators). Generating them on 2026-09-10 fired the generator's
round-trip detector: the preset path applies through the shell's bounded
param queue (`kQCap = 256`) in one synchronous pass, and a full patch is 323
keys plus the migrators' enqueues, so the tail is silently dropped
(`o1.enable` among it — the render came out single-oscillator). A golden
rendered from that would pin the truncation forever. The fixtures land the
moment the preset path stops truncating; the generator refuses (RED) until
then, and skips the chunk fixtures it already wrote.

The older-schema blobs are **synthetic archaeology**: this build's blob with
the keys and header a pre-migration build would not have written, so each
migrator is exercised by a blob that names exactly what it lacks. They are
fixtures of the LOADER's contract, not captures of the wild. Real blobs — the
notice-001 inventory's 28 Ableton sets (ADR-154), a preset a user sends in —
are the corpus's intended growth; add them beside these under the naming
above, never in place of these.

## What the check does and does not prove

- **Proves:** on this toolchain, every listed blob loads and renders the
  bytes it rendered on the day it was added. A green run after a shell change
  is evidence that no existing session's sound moved.
- **Calibrated:** every fixture is also loaded with a planted `masterVol`
  change and that render must DIFFER from the golden, so a comparison that
  cannot fail (or a silent golden) is red, not green (LIBRARY L0032).
- **Does not prove:** cross-compiler bit-identity. The goldens were rendered
  by AppleClang on Apple Silicon; a different compiler's FMA contraction or
  libm may differ in the last bit. If the check ever runs on CI, that is a
  measurement to make first, not an assumption to carry.

Not wired into `./verify` — that is a protected path and a human decision.
Run by hand: `build-release/statefix_check tests/state_fixtures`.
