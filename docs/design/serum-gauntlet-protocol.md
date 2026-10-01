# Serum 2 gauntlet: the protocol for stages 2 to 4

> Origin: horde lead session brief, 2026-10-01, ROADMAP **B381** (stage 1). This is
> the PLAN for stages 2–4, written for the human to review before any third-party
> plug-in is loaded. Status: **PROPOSED**. Nothing here has run against Serum.

## What B381 asks, and the ruling it rests on

B381 is a reference test against Serum 2, the human's installed wavetable synth. On
2026-10-01 the human ruled that all three parts matter: "CPU will matter for any
consumer, quality will matter to the heavy users, and imitation is how we ensure
it's a tool capable of making industry-standard sounds". They also ruled that the
offline host may load the installed Serum 2 ("Yes, though it may end up
benefitting from my oversight"), and that "I can render the reference sounds
myself".

Stage 1 (this PR) built the host and dry-ran it on Apple's built-in Audio Units
and on horde's own code only. Stages 2–4 below start only after the human has
reviewed stage 1.

## Ground rules (they hold in every stage)

1. **Nothing of Serum is ever committed.** That means no binary, no preset, no
   render, no screenshot, no parameter listing, and no parameter or preset name.
   The repo is public. A parameter list is the product's own text, so it is read
   locally and never written to a tracked file. This document uses no Serum text
   beyond the product name.
2. **Results are numbers only.** A committed result is a table of measurements:
   percentages of real time, dB, distances and counts. Rows are keyed by OUR
   labels ("ref-03", "unison 7"), never by a preset or parameter name.
3. **Third-party audio stays out of the tree.** Serum renders and the human's
   reference WAVs live in a local folder outside the repo, `~/horde-gauntlet/`
   (proposed): `refs/` holds the human's renders, `runs/` holds the host's
   renders and logs. The repo's `.gitignore` is not the safeguard; the location
   is.
4. **Loading Serum is a human gate, enforced in code.** `tools/auhost` refuses to
   instantiate any component that is not Apple's or horde's (`'Hsaw' 'LfTk'`)
   unless `--allow-third-party` is passed. That flag is used only in a stage the
   human has approved. The first Serum load happens with the human present (see
   §Oversight points).
5. **Measurements, not gates** (Layer-E). Nothing here passes or fails horde. A
   number that looks bad becomes a ROADMAP row, never a threshold someone tunes
   toward.
6. **Same host, same session, interleaved.** Every horde-versus-Serum
   comparison runs in the same `auhost` binary, in the same sitting, with A and B
   runs interleaved (ABAB). This stops thermal drift from favouring whichever ran
   first. Each run prints a calibration loop so a slow machine can be told from a
   slow engine. The machine runs on mains power, otherwise idle.

## The host (stage 1, built)

`tools/auhost/auhost.cpp` is the host, and its header is the full contract.
`tools/auhost/analyse.mjs` runs `tools/patchspace/metrics.mjs` on its WAVs. In
brief:

- `--list` reads the component registry. `--params` prints id, name, range and
  default for each parameter.
- `--render` drives an `aumu` unit offline from a JSON script: note on/off, bend,
  CC, program, and parameter changes by id or name at sample times. It writes a
  float32 WAV and times every `AudioUnitRender` call. It reports the median, p90,
  p99 and max % of real time per block, and the median divided by the voice count.
- `--horde` runs the same scripts through the h2 SCALPEL core
  (`h2/cores/scalpel/razor_core.h`) with the same loop and clock.
- Determinism is measured per run. Each repeat is a fresh instance, compared with
  repeat 0 (max |diff| and first differing frame). `--prime 1` (the default)
  discards one whole render first. On Apple's DLS, the first instance in a
  process differed from all later ones (the header has the numbers).

**What the host cannot make reproducible** is anything inside the plug-in. That
includes random unison or oscillator start phases, noise seeded from a clock or
from a per-process counter, worker threads, and assets that finish loading after
Initialize. Stage 2 therefore records Serum's measured determinism (bit-identical
or not, and where the repeats diverge) before it uses any of its numbers.
Imitation (stage 4) compares spectra, not samples, so phase randomness does not
spoil it. It only needs the magnitude to be stable, and stage 2 checks that by
repeat-to-repeat spectral distance.

## Stage 2: CPU

**Matrix**: voices × unison × oscillator type × buffer, with **FX off**. Every
cell is run at 48 kHz, with one 44.1 kHz column as a cross-check.

| axis | values |
|---|---|
| voices (held notes, spread over C2..C6) | 1, 2, 4, 8, 16 |
| unison per voice | 1, 3, 7, 16 (the horde core caps at 9 members: horde runs 1, 3, 7, 9) |
| oscillator | (a) a static saw-like wavetable; (b) the same with the wavetable position swept by automation; (c) two oscillators on |
| buffer | 64, 128, 512 |
| filter | off for the matrix, plus one column with a low-pass on |

- **Per cell**: 5 repeats, 8 warm-up blocks, 1 primed render, 3 s held notes.
  Reported: median and p99 % of real time per block.
- **Per voice is a slope, not a ratio.** Median-divided-by-voices folds the
  fixed cost (an idle reverb, a voice manager) into every voice. The stage 1 dry
  run shows the trap. DLS reads 0.277 % per voice at 1 voice and 0.030 % at 16,
  but its marginal cost is about 0.013 % per voice. So stage 2 fits median against
  voices by least squares. It reports the slope (marginal cost per voice) and the
  intercept (fixed cost) per unison count and buffer, and keeps the ratio column
  for the brief's format.
- **horde's side**: the same matrix, run through `--horde` (the SCALPEL core,
  unison = `N`) and through horde's installed AU (the legacy SWARM SAW engine).
  Both are labelled. Neither one stands in for the other.
- **Setting Serum's unison and oscillator type**: by parameter id, which
  `--params` prints locally, or by a state file the human saves. Stage 2 adds one
  host feature for the second option: `--preset <file.aupreset>`, which loads
  `kAudioUnitProperty_ClassInfo`. The human then sets up each cell's patch in
  their DAW, and we never need to touch Serum's parameter names. Those state
  files live in `~/horde-gauntlet/`, never in the tree.

## Stage 3: quality

Every quality measure comes from `metrics.mjs`, which has its own must-read-zero
and must-read-high controls (`metrics_check.mjs`). Where it lacks a measure,
stage 3 adds one there with controls, not in a side script.

- **Aliasing across the keyboard.** Each patch is rendered twice: at 48 kHz, and
  at 192 kHz as the 4x reference (`analyse.mjs --ref`). The script is
  `sweep.json`'s pattern: one note per 0.25 s window, C1..C8 in steps of 6
  semitones, plus a held note under a pitch-bend ramp. Reported: `aliasDb` per
  note window, as a curve over the keyboard. The stated caveat is `metrics.mjs`'s
  own: the 4x render is the plug-in's best, not alias-free. If a plug-in cannot
  render at 192 kHz, its aliasing is NOT measured. It is never compared against a
  different reference instead. *The dry run proved the pipeline*: DLS's
  saw-lead chord read −30.8 dB, and the must-read-low control (AUSampler's
  near-sine default) read −78.3 dB.
- **Zipper under parameter automation.** A held note while one parameter ramps
  (cutoff, wavetable position), sent as one event per block, at block sizes 64
  and 512. Reported: `clicks()` count and worst excess during the ramp, against
  the same note static. If the 512-sample stepped ramp reads like the 64-sample
  one, the plug-in smooths. If it reads worse, it zippers. horde gets the same
  test.
- **Noise floor.** `noiseDb` (aperiodic power) on held notes, measured in two
  places. One is the output with no notes at all after a note's release tail,
  which shows idle hiss, dither or denormal noise. The other is the RMS of that
  idle output.
- **Width.** `metrics.mjs` has no stereo measure today. Stage 3 adds one: side
  RMS / mid RMS and the L/R correlation per 0.1 s frame. It gets a must-read-zero
  control (identical channels read 0) and a must-read-high one (independent noise
  reads about 1). Reported: the median and the maximum over the note, which is
  the measure the human's third reference needed (side/mid up to 0.98).

## References: the human renders them

The human renders each reference in their DAW. Nothing about a reference is
committed except the numbers measured on it.

- **Where**: `~/horde-gauntlet/refs/`.
- **Name**: `ref-NN__<note-or-chord>__<sr>.wav`, for example
  `ref-04__C3__48k.wav`, with float32 or 24-bit, stereo, FX off unless the
  reference is about FX. Beside it goes `ref-NN.txt`, in the human's words: what
  was played (notes, held length, velocity), tempo if it is synced, FX on or off,
  and what it is meant to show. The `.txt` is never committed.
- **Analysed** with `node tools/auhost/analyse.mjs <wav> --note <midi> --from s
  --to s`, which gives level, LUFS-like loudness, DC, clicks, noiseDb, root and
  roughness. Width comes after stage 3 adds it.
- **The list we ask for** (one note, C3, 2 s held, then a 1 s release, unless
  stated otherwise):
  1. a plain saw, unison 1, FX off: the baseline;
  2. the same with unison 7, detuned;
  3. the same as a 16-note chord (CPU and density);
  4. a wavetable-position sweep over 2 s;
  5. a low-pass cutoff sweep over 2 s (zipper);
  6. a supersaw chord of the kind hyperpop uses;
  7. a pluck with a pitch envelope;
  8. a reese or similar bass;
  9. one noise-heavy patch;
  10. the three hits already sent on 2026-09-30 count as ref-11..13, already
      analysed;
  11. any patch the human wants horde to be able to reach (stage 4's targets).

## Stage 4: imitation

The question is what SCALPEL can reach, and what modulation it lacks. All three
references so far point at the same gaps: a pitch envelope, ENV 2–4 routed to
blade parameters, and mod routing (B381's row).

- **Search space**: the SCALPEL core's keys (`razor_core.h`'s `set()` table),
  with the lab's envelopes and modulation as the composed engine provides them.
  Which engine the search drives is stage 4's first decision, and the lead will
  ask the human rather than guess. One option is the C++ core, which `--horde`
  already drives: fast, but with no lab envelopes. The other is the lab's JS
  composed engine: it has the envelopes, but it is slower.
- **Distance**: a multi-resolution log-magnitude STFT distance (FFT 512, 2048
  and 8192; spectral convergence plus log-magnitude L1). It is computed after
  loudness matching (LUFS-like, `metrics.level`) and at the reference's detected
  root (`metrics.root`). Two terms are added: an envelope term (frame RMS curve)
  and, from stage 3, a width term. The distance is validated before any search
  uses it:
  - **must read zero**: a render compared with itself, and with the same patch
    under another seed;
  - **must read high**: a sine against a saw, and the right patch a fifth off.
  - **monotone**: one parameter moved away from a known target must give a
    distance that rises.
- **Search**: deterministic. The budget is counted in renders, never in wall
  time. The search uses seeded mulberry32 restarts, then coordinate descent (or
  CMA-ES with a fixed seed), with the same budget for every reference.
- **Output (numbers only)**: the best distance and its per-term breakdown, plus
  the term that would not go down. For example, "the envelope term stays high"
  names a missing pitch envelope. The best horde patch found may be committed,
  because it is ours. The reference may not. The human listens to each best match
  side by side with its reference (Layer-E), because a low distance is not the
  same as sounding like the reference.

## Oversight points (where the human is asked)

1. **Before stage 2**: review this PR: the host, the latch, the dry-run numbers
   and this protocol.
2. **The first Serum load**: with the human present. `auhost --list` finds
   Serum's component triple, and `--params` runs once, locally, to confirm the
   host can instantiate it offline. Nothing is printed into a tracked file.
3. **Patches for the CPU matrix**: the human saves the per-cell state files, or
   approves setting unison and oscillator type by parameter id.
4. **References**: the human renders the list above, in their own time.
5. **Before stage 4**: the human rules on which engine the search drives, and
   reviews the distance's controls.

## Stage 1 dry run (2026-10-01, this Mac, Apple Silicon)

The calibration loop measured 180–184 ms. Settings: 48 kHz, 5 repeats, 8 warm-up
blocks, 1 primed render, 3 s held notes (`tools/auhost/scripts/voices-*.json`,
GM program 81). Figures are the median % of real time per block; per voice is
the median divided by voices.

| unit | buffer | 1 voice | 8 voices | 16 voices |
|---|---|---|---|---|
| Apple DLSMusicDevice | 128 | 0.277 | 0.370 | 0.478 |
| Apple DLSMusicDevice | 512 | 0.269 | 0.352 | 0.449 |
| Apple AUSampler (default patch) | 128 | 0.030 | 0.180 | 0.345 |
| Apple AUSampler (default patch) | 512 | 0.022 | 0.127 | 0.235 |
| horde AU, installed (legacy SWARM SAW, default patch) | 128 | 0.281 | 1.766 | 3.492 |
| horde AU, installed (legacy SWARM SAW, default patch) | 512 | 0.273 | 1.754 | 3.524 |
| horde h2 SCALPEL core (default keys, N 5, os 2) | 128 | 1.800 | 13.095 | 26.819 |
| horde h2 SCALPEL core (default keys, N 5, os 2) | 512 | 1.800 | 13.089 | 26.802 |

Every run above was bit-identical across its repeats once primed. The clock is
`steady_clock`, which ticks at 41.67 ns on this machine, or 0.0016 % of a
128-sample block. Figures closer than that are equal.
