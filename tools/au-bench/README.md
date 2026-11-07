# tools/au-bench — a headless AU CPU bench (B638)

A command-line host that measures an installed Audio Unit instrument under the
conditions horde's CPU tool uses for PF-1, PF-3 and PF-9 (`specs/SPEC-ENGINE.md`
§9). Its result is in horde's unit: **% of one core** = render time ÷ audio
duration × 100. It exists to give the human a reference reading for a heavy
**Xfer Serum 2** patch on this Mac (ADR-380, decision 4), the number that sets
horde's CPU target.

It measures and never judges. It is not a `./verify` gate (UNWIRED): it needs
real installed AUs and a quiet machine, so it runs by hand.

| file | what it is |
|---|---|
| `au_bench.cpp` | the host. Its header is the contract: the conditions, what differs from the CPU tool and why, the controls |
| `build.sh` | builds `build-au-bench/au_bench` with the CPU tool's Release flags |

## Run it

```sh
tools/au-bench/build.sh                      # builds build-au-bench/au_bench (sandbox is fine)
build-au-bench/au_bench --controls           # the must-fail controls, on Apple's DLSMusicDevice
build-au-bench/au_bench --list               # the installed instruments (loads no plug-in)
build-au-bench/au_bench --name Serum2 --allow-third-party \
    --preset ~/horde-gauntlet/refs/heavy.adv --report ~/horde-gauntlet/runs/serum2-heavy.md
```

Inside Claude Code, **run** it with the sandbox disabled: the sandbox's overlay
is not the real AU registry. It installs nothing and writes nothing under
`~/Library`. Keep reports of third-party results outside the repo
(`docs/design/serum-gauntlet-protocol.md`, rule 3).

Options: `--sr 44100,48000`, `--block 128,512`, `--voices 8,16,64` (the
defaults: every combination, 12 cells), `--report <file.md>`. Two diagnostics:
`--voices 0` is the no-notes control (it must read silent, which shows the
chord's sound comes from the notes), and `--first-channel 2` moves notes 1 to 8
to MIDI channel 2 (does the plug-in hear the channels the larger cells use?). Exactly one of
`--preset <file>` and `--default` is required: a run with no preset says so on
the command line, and a missing or bad preset file is refused, never rendered at
the defaults. Anything not Apple's or horde's is refused without
`--allow-third-party` (the same latch as `tools/auhost`, B381).

Exit status: 0 measured; 1 refused (component, preset, arguments); 2 the load
guard refused; 3 a cell failed or rendered silence.

## Preset files it takes

- **`.aupreset`**: the AU's state as a property list (what Logic, Live's AU
  preset menu and AU Lab save). Its `type`, `subtype` and `manufacturer` must be
  this component's.
- **A Live `.adv` device preset** with Serum 2 loaded **as an Audio Unit**: gzipped
  XML whose `<AuPluginDevice>` names the component and whose `<AuPreset><Buffer>`
  holds the same property list, hex-encoded. The layout was read from a Live 12
  set on this Mac; the controls round-trip a synthetic `.adv` through it. An `.adv`
  saved with the VST3 Serum 2 is refused with a message saying so (its state is the
  VST3's, which the AU cannot take). No Live-saved AU `.adv` existed on this Mac to
  test, so the first real one is the open check: if it is refused, export an
  `.aupreset` instead.
- Serum's own `.SerumPreset` files are not accepted: load the patch in Serum 2
  inside a host and save one of the two above.

## What it measures, and how

Copied from the test author's CPU tool (`engine2/tests/cpu/main.cpp` and
`pf3_chord.h` there), so the two numbers mean the same:

- the chord: the k-th note on key 48 + 3·(k mod 8), velocity 108/127 (0.85), all on
  at sample 0 and held to the end; 8 notes for PF-1's cell, 16 and 64 for PF-9's;
- 0.25 s untimed, then 2 s timed; best of 5 repeats, interleaved across the cells,
  a fresh instance per repeat;
- **mean % of one core** (the best repeat), its **spread** (worst/best), and the
  **worst block** as % of its deadline (the smallest of the 5 repeats' values, as
  PF-10 reads it);
- the **calibration loop** (10⁸ dependent multiply-adds) before every run, compiled
  with the same flags, so its seconds compare with the CPU tool's report;
- the **load guard** before every run: load average at most 3.0 and no other
  process above half a core, else wait 20 s and re-check for up to 10 minutes,
  then refuse. Never bypassed.

It also prints the timed span's **peak and RMS** (silence or NaN fails the cell),
the plug-in's **reported latency and tail**, and any parameter whose name says
polyphony, voices, oversampling or quality.

What differs, and why:

- **MIDI has no note ids.** Notes 9 to 64 repeat the 8 keys, so the k-th note goes
  on MIDI channel ⌊k/8⌋+1: a repeated key on one channel would retrigger, not add
  a voice. Whether the plug-in voices them all is its polyphony's business. The
  host cannot count its voices, so it prints each larger cell's cost and RMS
  against the 8-note cell's. A cost ratio near 1 means the extra notes were not
  voiced. First run (2026-11-07): Serum 2's default patch hears channel 2 exactly
  as channel 1, and its 16- and 64-note cells cost and sound the same as its
  8-note cell. That cap is the patch's polyphony, so for PF-9's cells the human
  sets the patch's voice count to at least 64 before exporting it.
- **Worker threads.** Beside PF-3's wall-clock share it prints the process's CPU
  time over the timed span (all threads, `getrusage`). If that is far above the
  wall share, the plug-in spreads its work across cores and "% of one core"
  understates it.
- **In process** (`AudioComponentInstanceNew`, the AUv2 default). Live hosts AUv2
  in process, and an out-of-process instance would add an IPC round trip to every
  render, charged to the plug-in.
- After Initialize and the preset, the main run loop runs 0.2 s, untimed, so a
  plug-in that finishes loading on the main queue can do so.
- PF-3 fixes 128-sample blocks; 512 is measured too because a DAW buffer often is.

## The controls (`--controls`)

On Apple's DLSMusicDevice. All must read `ok`:

| | control | must |
|---|---|---|
| C1 | the guard's rule on planted readings | refuse load 3.5 and 0.6 of a core; pass 2.9 and 0.4 |
| C2 | the stand-in, 8 notes, 44.1 kHz, 128 | read a finite, plausible share, not silent |
| C3 | a planted "skip the render" | read under a tenth of C2 and be flagged silent |
| C4 | a missing preset file | be refused |
| C5 | a garbage `.aupreset`; an unsupported format | be refused |
| C6 | the stand-in's state claiming another component | be refused |
| C7 | the stand-in's state, one parameter moved, through a gzipped `.adv` | load, and read the moved value back; an instance with no preset must not read it |
| C8 | an `.adv` holding a VST3 device | be refused |
| C9 | an `.adv` whose buffer is corrupt | be refused |

The controls test the pipeline and report no reference reading, with margins of
10× or more, so they print the guard's reading rather than wait on it.
