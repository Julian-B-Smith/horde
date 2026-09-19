# cpu_bench — measuring HYPERSAW's CPU cost on another Mac

A single self-contained command-line program. **No DAW, no Xcode, no install** —
it renders audio with the real synth core and prints how much of one CPU core
that costs. Nothing is written, played, or installed.

Universal binary: runs on Apple Silicon and Intel Macs alike.

## Rebuilding it (and how to tell if it is stale)

**The first line the program prints is the commit it was built from**, so a
stale copy announces itself — no git log, and no access to this repo, required
to tell whether the numbers below describe today's engine. CMake does not build
this artefact (it targets the host arch only); the universal binary is this one
command, run from the repo root:

```bash
clang++ -std=c++20 -O3 -arch arm64 -arch x86_64 \
  -DHYPERSAW_BUILD_STAMP="\"$(git rev-parse --short HEAD)\"" \
  -I src tools/cpu_bench.cpp -o dist/cpu_bench \
  && codesign --force -s - dist/cpu_bench
```

The `-D` is load-bearing: without it the program compiles and runs but prints
`build unstamped`, which is the failure this section exists to prevent. The
`codesign` re-seal is what lets the binary run after being copied to another
Mac.

**Built from commit `6585db9` on 2026-09-19.** The reference numbers at the
bottom of this file were measured on 2026-08-06 with the binary this one
replaces, so they describe an OLDER engine; they have not been re-measured
against this build. Compare the stamp the program prints against the hash
above before trusting either.

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
cpu_bench: build 6585db9
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
