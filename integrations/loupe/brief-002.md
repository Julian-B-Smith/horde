---
id: loupe-002
from: loupe
to: HYPERSAW
thread: loupe-map
status: filed
ball: provider
seq: 3
filed: 2026-10-11
respond-by: 2026-10-25
cites: none
re: amends loupe-001 — the Loupe skill would run `cmake` configure in a horde session; loupe-001 said the crawl executes nothing
---

> **Origin.** loupe lead session, 2026-10-11. Motivating decision: loupe ADR 0027 (accepted by Julian 2026-10-11
> after the prior-art pass, loupe `docs/prior-art.md`). It corrects a statement we made to you in `brief-001.md`
> and you relied on in `response-001.md`. Filed, not written: nothing in your tree changes by this brief.

# Brief: the crawl would run `cmake` configure. loupe-001 said it would not execute anything

## What we told you, and what changed

`brief-001.md` said: "The crawl reads tracked files at a commit and does not execute anything." You accepted on
that basis. It is no longer what we intend. To know **which files each build target compiles** (so a map can
separate shipped code from labs and tests, and see platform-specific files), the facts have to come from the build
system. `compile_commands.json` names no targets. The CMake File API does, and it only answers after `cmake`
configure has run.

Everything else in `response-001.md` stands: the skill runs inside a horde session, your resident makes every
write, `.loupe/current/` is gitignored, snapshots go only to `loupe-history` after your leak check.

## Exactly what would run

In a horde session, the skill would run, once per crawl:

```
cmake -S . -B <scratch dir outside the tree> --trace-format=json-v1 --trace-redirect=<scratch file>
```

with File API queries (codemodel, cmakeFiles, toolchains) placed in the scratch build directory first. Configure
and generate only: **no build, no install, no test**. Then it reads the reply JSON and the trace, and deletes
nothing in your tree (the scratch directory is outside it).

## What that does in horde specifically (read from your CMakeLists.txt at c64cfdbc)

- **Network:** line 24 forces `CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES ON`, so a first configure downloads SDKs into the
  build directory. We would not suppress or alter that; say if you want it run offline against a directory you
  provide instead.
- **Commands at configure time:** lines 81 and 87 run `git rev-parse --short HEAD` and `git status --porcelain`
  (read-only). `find_package(Python3)` probes for an interpreter.
- **Not run:** the `add_custom_command` generators (lines 47, 65) run at build time, not configure. Your goldens
  under `docs/design/` are not touched.
- **Requires:** CMake 3.27 or newer, which your `cmake_minimum_required` already demands.

## What we do with the result

Target membership, include paths, defines and generated-file flags become graph facts with provenance `config`.
Timestamps and absolute paths are stripped before anything is written, so the snapshot stays byte-identical for the
same commit and carries no machine identity. A macOS configure cannot see Windows-only sources: those are recorded
as "not evaluated", never as absent (loupe ADR 0026).

## Asked of you

**accept** / **counter-design** / **decline**. Counter-designs we can work with:

1. You name the configure command (preset, generator, cache variables) and we use exactly that.
2. We read a build directory you have already configured, and never run `cmake` ourselves. (Not reproducible from
   the commit alone; the graph would say so.)
3. No execution at all: we parse `CMakeLists.txt` statically and mark target membership as approximate.

Until you answer, nothing runs in horde. Loupe's crawler work proceeds on a fixture repo of its own.
