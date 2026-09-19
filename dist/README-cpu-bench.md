# cpu_bench — measuring HYPERSAW's CPU cost on another Mac

A single self-contained command-line program. **No DAW, no Xcode, no install** —
it renders audio with the real synth core and prints how much of one CPU core
that costs. Nothing is written, played, or installed.

Universal binary: runs on Apple Silicon and Intel Macs alike.

> **Staleness (repo audit 2026-09-19, H4).** The committed `cpu_bench` was
> built 2026-08-06 and the engine (`src/swarm_core.h`) has changed in 31
> commits since; nothing in the repo rebuilds this universal binary
> (`CMAKE_OSX_ARCHITECTURES` is set nowhere), and the program prints a
> number, not the engine hash it was built from. Treat its figures as
> historical until it is rebuilt and stamped — a rebuild recipe and a
> printed build id are the fix, queued on the ROADMAP.

## Running it

Copy `cpu_bench` to the other Mac, open **Terminal**, and drag the file into the
window (that types its path for you), then press Return.

If macOS refuses to open it because it was downloaded, clear the quarantine flag
first — this is the standard block on any unsigned downloaded binary, not a
warning about this program specifically:

```bash
xattr -d com.apple.quarantine /path/to/cpu_bench
```

Copying by USB stick or AirDrop sets that flag; copying inside the same iCloud
Drive usually does not.

## What it prints

```
cpu_bench: 7 voices x 8 notes = 56 oscillators
  audio rendered   8.00 s
  cpu consumed     0.128 s
  % of one core    1.60%   (E-6 budget 50%, 62.5x realtime)
```

**The number that matters is "% of one core."** The project's performance
envelope (E-6) says a heavy patch must stay under 50% of one core on minimum-spec
hardware at 44.1 kHz with a 128-sample buffer.

## Arguments (all optional)

```
cpu_bench [voices] [notes] [seconds]
```

Defaults are `7 8 8` — 7 swarm voices per note, 8 notes held, 8 seconds of audio.

Two runs are useful:

```bash
cpu_bench 7 8 8      # one oscillator's worth  (default patch)
cpu_bench 14 8 8     # two oscillators' worth  (the ADR-082 question)
```

## Reference numbers from the development machine (Apple M3)

| load | % of one core |
|---|---|
| `7 8 8` — 56 oscillators | 1.60% |
| `14 8 8` — 112 oscillators | 2.98% |

Anything under ~25% on the other machine means two oscillators fit the envelope
with room to spare. Close the laptop lid / other heavy apps first; the timer
measures wall-clock through the render loop, so a busy machine reads high.
