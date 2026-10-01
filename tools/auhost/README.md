# tools/auhost — offline AudioUnit host (B381)

This folder is a command-line host that renders macOS Audio Units offline, with
no Xcode needed. It drives an instrument from a JSON note script, writes a
float32 WAV, and times every render call. It also has a `--horde` mode that runs
the same scripts through horde's h2 SCALPEL core, so the two sets of numbers come
from the same host. It is a measurement tool, not a `./verify` gate.

It is stage 1 of the Serum 2 gauntlet (ROADMAP B381). The plan for stages 2–4 is
[docs/design/serum-gauntlet-protocol.md](../../docs/design/serum-gauntlet-protocol.md).

| file | what it is |
|---|---|
| `auhost.cpp` | the host. Its header is the contract: commands, script format, timing method, determinism, and the third-party latch |
| `analyse.mjs` | runs `tools/patchspace/metrics.mjs` on a WAV the host wrote, with aliasing against a 4x-rate render |
| `scripts/*.json` | the dry-run scripts: a C-major chord, a keyboard sweep with pitch bend, and 1, 8 and 16 held voices |

```sh
cmake --build <abs>/build-release --target auhost     # or the clang++ line in the header
build-release/auhost --list --type aumu --mfr appl
build-release/auhost --params aumu 'dls ' appl
build-release/auhost --render aumu 'dls ' appl --script tools/auhost/scripts/chord.json --out /tmp/x.wav
build-release/auhost --horde --script tools/auhost/scripts/voices-8.json --block 512
node tools/auhost/analyse.mjs /tmp/x.wav --note 60 [--ref /tmp/x-192k.wav]
```

Inside the Claude Code sandbox, run AU commands with `dangerouslyDisableSandbox`
(see the global audio-plugin notes); the sandbox's overlay is not the real AU
registry. The host
**refuses** any component that is not Apple's or horde's unless it is given
`--allow-third-party`. That flag is a human gate (B381).
