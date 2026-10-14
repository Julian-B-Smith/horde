# h2/harness — the engine harness

One interface for driving **any enrolled horde 2 engine** without knowing which
engine it is. The parameter fuzz, the invariance matrix and the soak (Blind-Spot
Armor phase 2, packages P3, P4 and P6) are each a loop over it.

ROADMAP B448, package P2, ruled in ADR-209. The proposal is
`docs/strategy/blind-spot-armor-phase2.md` (section 2 rule 4; section 3 row P2).

Last verified: 2026-10-10 (darwin-arm64, Apple clang, Node 24). Static check wired in
`./verify fast`; the selftest is green by hand and **not yet run by `./verify`**
(see "Wiring the selftest").

## What is here

| file | what it is |
|---|---|
| `harness.h` | The `Adapter` interface, the job language and its runner, and three control adapters. Includes no engine. |
| `adapter_composed.h` | The adapter for the composed engine (`h2/engine/`). States what the pre-limiter bus can and cannot say. |
| `adapters.h` | The table: one row per enrolled engine, then the controls. |
| `control_fuzz.h` | The parameter fuzz's ten planted faults (below). Control adapters, never enrolled. |
| `control_steal.h` | The steal fade's planted faults (B459, ADR-216 D7), each built from two shipped engines: `control/steal-cut` (no ramp), `control/steal-cut5` (the ramp right for four samples, then a cut), `control/steal-short` (a ramp that stops short of zero), `control/steal-step` (a start at the old level instead of from silence), `control/steal-early` (a start one sample before the fade's end). `tools/h2_steal_fade_check.mjs` must read each as a fault, on the row named for it, on every run. Control adapters, never enrolled. |
| `control_spread.h` | The spread ramp's planted faults (B460, ADR-216 D8), each built from shipped engines through their public interface: `control/ramp-blade1` and `control/ramp-blade2` (the ramp reaches one blade only: three engines, two with one blade made inert and one with both, summed on the pre-limiter bus), `control/ramp-skip` (the ramp dropped at one tick in four), `control/ramp-both` (that composition with nothing planted, which must render what the engine renders), and `control/step-<kind>-b1` / `-b2` (the same composition on a job in which one blade moves one kind of value alone: that kind left stepped, everything else ramped; seven kinds, two blades; a job outside that is refused with NaN). Two more are readings, not sounds: `control/walk-probe` (two engines, one with the ramp: how far apart their spread walks stand) and `control/ramp-probe` (each of the eighteen spread values of every member read after every sample through the engine's public voice view, and scored per tick against equal shares; the engine without the ramp is its plant). `tools/h2_spread_ramp_check.mjs` must read each plant as a fault, and the composition as the engine, on every run. Control adapters, never enrolled. |
| `render_cli.cpp` | `h2_harness_render`: runs job files, writes what was rendered. Measures and judges nothing. |
| `harness.mjs` | The interface tools use: enrolled engines, reference patches, `render`, `features`. |
| `patches.json` | The reference patches. **Generated; never edit it by hand.** |
| `gen_patches.mjs` | Writes `patches.json` from the files that own each value. `--check` reports a stale file. |
| `selftest.mjs` | Proves each adapter drives its engine, with its must-fail controls. |

Beside it: `docs/armor/engines.json` (the enrolment registry) and
`tools/engine_enrolment_check.py` (the gate that finds un-enrolled directories).

### The parameter fuzz's plants (`control_fuzz.h`)

`tools/h2_fuzz_check.mjs` (package P3) runs its own cases through these on every run and is
red unless it reads each exactly as built. Each takes every key and watches one corner of
the declared table (`tools/patchspace/dependency_tree.json`). While a note is held both
channels carry a level of 0.5 x gain through tanh; **the fault is on the right channel
only**. The fuzz's header says what each must read as, and why.

| adapter | corner | the fault |
|---|---|---|
| `control/fuzz-blowup` | `fb` max, `K` min | the level doubles every 40 ms, without end |
| `control/fuzz-slow` | `stretchB` max | the level doubles every 400 ms |
| `control/fuzz-plateau` | `colB` max, `width` min | doubles every 40 ms, then stops at 64 x gain |
| `control/fuzz-burst` | `inertia` max, `harmReach` min | bursts of 12 times the level, 20 ms in every 350 ms; never grows |
| `control/fuzz-loud` | `xm` max, `kRuleAmt` min | a steady level of 4096 x gain |
| `control/fuzz-keyed` | `spread` max, `onset` min | 16 x gain, multiplied by 8 where that is above 10: not linear in gain |
| `control/fuzz-nan` | `detune` max, `S` min, `aa` 0 | a NaN while a note is held |
| `control/fuzz-nan-tail` | `hard` max, `depth` min | a NaN only after the last note is released |
| `control/fuzz-hang` | `R` min, `A` max | a level of 0.001 x gain that never stops after the release |
| `control/fuzz-late` | `R` min, `D` max | the same, stopping 0.65 s after the release |

## Using it (for the P3, P4 and P6 authors)

```js
import { enrolled, patchesFor, render, run, jobFor, patchCmds, features } from '../h2/harness/harness.mjs';

for (const engine of enrolled())                 // ids from docs/armor/engines.json
  for (const patch of patchesFor(engine)) {      // the reference patches of that engine
    const r = render({ patch, sr: 96000, block: 7, seed: 1, seconds: 2, hold: 1.5, pre: true });
    r.L; r.R;                                    // Float64Array: the output
    r.preL; r.preR;                              // Float64Array: the limiter's input
    r.summary.digest;                            // 16 hex digits over the output samples
    r.summary.unknownKeys;                       // keys the engine did not have
    const f = features(r, { from: 0.25, to: 1.25 });   // tools/patchspace/metrics.mjs
    if (f.silent) fail(`${engine}/${patch.name}: silent, ${f.rmsDb} dBFS`);   // the tool's own check goes here
  }
```

A `render` request:

| field | meaning | default |
|---|---|---|
| `patch` | a reference patch, or its id | required unless `cmds` is given |
| `engine` | the adapter id | the patch's |
| `sr` | sample rate | 48000 |
| `block` | host block size in frames, or `{ vary: max }` for sizes drawn 1..max from a seeded stream | 128 |
| `seed` | the host random stream | 1 |
| `seconds` | total length | 1 |
| `hold` | seconds until the notes are released | held to the end |
| `notes`, `vel` | what is played | the cell's: the ledger's 8-voice chord at 0.85 |
| `overrides` | `{ key: value }` set after the patch and before `snap` | none |
| `pre` | also return the pre-limiter bus | false |
| `cmds` | your own script, replacing the default | none |

- **Many renders, one process.** `run([jobFor(a), jobFor(b), ...])` returns the results in
  order. Sixty 8-voice half-second jobs took 16 s on the dev Mac (measured 2026-10-10).
- **Your own script.** A fuzz that sweeps a parameter mid-note, or a soak that churns notes,
  passes `cmds`. The words are `set`, `sets`, `snap`, `on <note> <freq> <vel>`,
  `off <note>`, `panic` and `render <frames>` (`harness.h` has the full language). Start
  from `patchCmds(patch, overrides)` to get a reference patch as commands.
- **Block size never moves an event.** A `render` is cut into host blocks and the last one
  is shortened to fit, so a note or a `set` lands on its exact frame at any block size.
- **A bad request stops the run.** An unknown adapter or an unknown command makes `run`
  throw. Nothing is skipped: a job that did not run must never read as one that passed.
- **Determinism.** No clock, and no randomness but the seeded mulberry32 streams. The same
  job gives the same bytes from the same binary. Digests are keyed to the build that made
  them (`h2/README.md`), so compare them within one binary and pin none.
- **C++ in process.** Include `adapters.h`, call `makeAdapter(id)`, then either the adapter's
  own methods or `runJob(adapter, job, sink, info)`.

### The pre-limiter bus

The engine has no accessor for the value that enters its output limiter. Its last line is
`tanh(y * gain * 1.6)`, and tanh is one-to-one below its rail, so the adapter returns
`atanh(out)`: the limiter's input, after the gain and the DC blocker.

- **It is the real bus.** Checked once against the engine's own stage hook, which skips the
  limiter (a scratch build with `H2_ENGINE_STAGES`, the defaults patch, 8 voices, 1 s,
  2026-10-10): at gain 0.35 the bus peaked at 3.02 and the two agreed within 5.3e-15; at
  gain 1 it peaked at 8.63 and they agreed within 3.9e-10.
- **On the rail it reads infinity.** From about 19 the output is exactly 1 and the input is
  lost. Read `Infinity` as "at least 19", never as a number. In the same check at gain 8 (a
  value outside the declared range, used to force it) 13 % of the samples were on the rail.
- **Precision falls toward the rail:** 1.3e-15 at 3, 6e-13 at 6, 4.5e-9 at 10, 8e-5 at 15,
  0.13 at 18.5 (measured). A bound of a few units is tested exactly enough, and a blow-up
  reads as infinity, which no bound passes.
- A tool that needs more (the bus's true size far above 19, or the bus before the DC
  blocker) needs an engine accessor. This package was told not to add one.

### Features: what `tools/patchspace/metrics.mjs` has, and what it lacks

`features()` returns `metrics.mjs`'s `analyse()` bundle and adds nothing. The proposal
forbids a second metrics library (section 5).

| wanted (proposal, P2 and P4) | in `metrics.mjs` today |
|---|---|
| loudness | `rmsDb`, `peakDb`, `crestDb` at any rate. The K-weighted `lufs` is computed at 48 kHz only and is `null` at every other rate |
| spectral centroid | **missing** (`spectrum()` gives the power spectrum it would be computed from) |
| envelope | **missing** (no attack, release or envelope-shape measure; `rms()` over windows is all there is) |
| onset | **missing** (`clicks` finds one-off discontinuities, not an onset time) |
| pitch | partly: `rootPresence` and `rootInterval` say whether the played note wins, on a semitone grid. A frequency in Hz or cents is **missing** |
| also present | `nonFinite`, `dc`, `dcRatio`, `silent`, `clicks`, `flatness`, `noiseDb`, `roughness`; `aliasing()` and `aliasConvergence()` need a second render |

The missing ones are package P9's to add to `metrics.mjs`. P4's rate axis needs four of them.

## The reference patches

`patches.json` holds ten, all for `composed`. Each records where it came from.

- **Seven ledger presets** (`ledger/...`): the names are read from `kLedger` in
  `tools/measure_h2_engine.cpp`, the values from the bank
  (`reference/scalpel/data/presets.json`), played as the parity scenarios play them (the
  params, then gain 0.35). `ledger/defaults` is the engine with no preset.
- **Three corners** (`corner/...`), from the declared ranges in
  `tools/patchspace/dependency_tree.json`: every continuous parameter at its maximum; the
  same with every toggle on; every continuous parameter at its minimum. They are
  whole-table rules, so no parameter was chosen by judgement. Enumerations keep their
  defaults. The C++ engine declares defaults and no ranges, so that table (the lab's own)
  is the one declaration there is.
- **`expect`** is `loud` or `silent`. `corner/continuous-min` sets gain to 0 and must render
  silence: it is the set's must-read-zero patch.
- **The cell** is the ledger's 8-voice cell: `poly 8`, keys 48 + 3k, velocity 0.85.

Regenerate with `node h2/harness/gen_patches.mjs` after the bank, the ranges or the ledger
list changes.

## Building

`h2_harness_render` is h2 code, so every build is contraction-off (h2 rule 7). There is no
CMake target yet. By hand, from the repo root (3 to 8 s, measured):

```
c++ -std=c++20 -O3 -ffp-contract=off h2/harness/render_cli.cpp -o <dir>/h2_harness_render
```

`harness.mjs` looks for the binary in `run()`'s `bin` option, then `$H2_HARNESS_BIN`, then
`build-release/h2_harness_render`. On the ten reference patches the `-O2` and `-O3` builds
gave the same digests (measured 2026-10-10, this machine only).

## Wiring the selftest

**Done 2026-10-11:** option 1 below. `h2_harness_render` is a CMake target and the selftest
runs in `./verify full`. The rest of this section is kept as the record of why.

`tools/engine_enrolment_check.py` is static and runs in `./verify fast`. `selftest.mjs`
needs the binary, and this package could not build one inside `./verify`:

- `CMakeLists.txt` was outside its brief, and
- a compile line in a `tools/` script is a `direct_compiles` entry of
  `tools/build_flags_check.py`, which only the human's approval can pin.

Two ways to close it, both small:

1. **In `./verify full`, with a CMake target** (no approval needed if the flags equal the
   class's; `tools/build_flags_check.py --append` records a new target):
   ```
   add_executable(h2_harness_render h2/harness/render_cli.cpp)
   target_compile_options(h2_harness_render PRIVATE -O3 -ffp-contract=off)   # inside the existing NOT MSVC guard
   ```
   then in `full()`, after the build: `node h2/harness/selftest.mjs --bin "$build_dir/h2_harness_render" || return 1`.
2. **In `./verify fast`**, by having the enrolment check compile the tool itself (measured
   in a draft of this package: 7.8 s the first time, 1.2 s after with a cached binary). That adds a
   `direct_compiles` entry, so it needs the human's approval.

Until one lands, "an adapter that returns silence is red" holds only when someone runs the
selftest. The enrolment check says so in its green line.

## Enrolling an engine or a module

A new directory under `h2/` that holds C or C++ code turns
`tools/engine_enrolment_check.py` red until it is in `docs/armor/engines.json`.

1. Write `h2/harness/adapter_<id>.h`: a class deriving `Adapter`, going through the
   engine's public interface only. Say in its header what `preLimiter()` can recover.
2. Include it in `adapters.h` and add its row: `{"<id>", false, &make<Name>},`.
3. Add the unit to `docs/armor/engines.json` with `kind`, `id` and `adapter`.
4. Give it reference patches. Teach `gen_patches.mjs` where they come from (every value read
   from a file that owns it), with at least one `expect: "loud"`.
5. Rebuild the tool and run the selftest.

A directory that is not an engine or a module is entered with `why_not_enrolled`. A
`test-reference` must not be included by anything enrolled; the check holds it to that.

## Known limits

- **A module has no input yet.** `Adapter::render` makes sound from notes. An effect needs
  input buffers, and no module exists under `h2/` to design that against, so it is not
  invented here. The first module's enrolment adds it.
- **Not exposed:** retune, the voice cap and its policy, the load readouts, per-voice
  state. No caller needs them yet. While a culled voice fades the engine passes its output
  through a float, which would lower the pre-limiter bus's precision; no harness render
  takes that path because the voice cap is not exposed.
- **Long renders.** The tool streams in chunks of 32768 frames with constant memory, but
  `harness.mjs` reads the whole output file at once. A four-hour soak needs a streaming
  reader on the JS side; the file format already allows one (each job's summary follows
  its samples).
- **The selftest's thresholds** (the loud floor, the tone tolerance, the pre-limiter
  tolerances) are in a `.mjs` file, which `docs/armor/tolerances.json`'s scan does not read
  yet. They are listed at the top of `selftest.mjs`.
