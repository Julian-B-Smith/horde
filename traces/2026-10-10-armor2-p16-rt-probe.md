# armor2-p16-rt-probe — the flush-to-zero number for ADR-209 item 6, and the engine's RealtimeSanitizer probe

- **Queue item:** ROADMAP B448 (Blind-Spot Armor), phase 2 Wave 1, package P16
  (`docs/strategy/blind-spot-armor-phase2.md` §3; ruled in ADR-209).
- **Why:** ADR-209 item 6 says "measure first": the human rules on flush-to-zero
  in the horde 2 shell after seeing how many of the 543 engine self-digest rows
  change with it on. This change set carries the tool that measures that, the
  number, and the probe source for the engine's RealtimeSanitizer check.

## What is in this change set

| File | What it is |
|---|---|
| `tools/h2_engine_fz_probe.cpp`, `tools/h2_engine_fz_probe.py` | The flush-to-zero measurement. Not a gate; nothing in `./verify` runs it. It has no re-pin path and writes no reference. |
| `tools/h2_engine_hooked_replay.h` | The parity gate's replay with a hook around every engine call, shared by both probes. |
| `tools/h2_engine_rtsan_probe.cpp` | The engine under RealtimeSanitizer: every scenario, every call a host block makes inside the realtime scope. |

## What is NOT in it, and why

Two more files were written and run, and are held out of the commit:
`tools/h2_engine_rtsan_check.py` (the wrapper that builds and judges the
RealtimeSanitizer probe) and `tools/h2_rt_lint_check.py` (the ban-list lint over
`h2/`). Both carry the not-wired header the brief specified. The lead then
amended the brief: that header raises `tools/weakening_check.py`'s ratchet, which
only the human may approve, so both were to be declared wired and given one line
each in `./verify`. The edit that changes the two headers was refused by the
session's permission system, and was not retried by another route. With the
original header the two files cannot be committed on a green `./verify fast`, so
they are held outside the commit for the lead and the human to decide (the
hand-back says where). `./verify` is untouched. Their measured results are below,
so the work is not lost if the files are.

A second thing stands between the RealtimeSanitizer wrapper and a green verify,
found by running `./verify fast` with it on disk: `tools/build_flags_check.py`
pins the flags of every compiler line outside CMake, and a new one is red until
the human approves it in that ledger. Verbatim:

    build_flags_check: RED
      direct_compiles: 'tools/h2_engine_rtsan_check.py' pinned '<absent>', tree ['-std=gnu++20', '-O1', '-g', '-fno-omit-frame-pointer', '-ffp-contract=off', '-fsanitize=realtime']

No ledger was approved or edited here.

## Measured, 2026-10-10, dev Mac (darwin-arm64, Apple clang 16, Node 24, Homebrew LLVM 23.1.3)

**Flush-to-zero (part c).** Command:
`python3 tools/h2_engine_fz_probe.py --rows --full-from <stream>`, the stream from
`node tools/h2_engine_render.mjs`.

| Build (flags read from the CMake target) | Baseline, FZ off | Rows changed, FZ on | Largest sample difference |
|---|---|---|---|
| parity (`h2_engine_selfdigest_check`: `-O2 -ffp-contract=off`) | 543 of 543 equal the pinned reference | **8 of 543** | 4.09e-308 |
| product (`h2_engine_selfdigest_product`: `-O3 -ffp-contract=off`) | 543 of 543 equal the pinned reference | **8 of 543** | 4.09e-308 |

- What was set: bit 24 (FZ) of FPCR on the calling thread, nothing else. It read
  `0x0` before, `0x1000000` inside the scope and `0x0` after.
- What the bit does on this CPU: a subnormal result (`DBL_MIN * 0.5`) became 0 and
  raised UFC; a subnormal input (`DBL_MIN/4 * 1e300`) was read as 0 and raised IDC.
- The count is the same, and the same eight rows, in all three scopes measured:
  FZ on inside `render` only; on inside every engine call a host block makes; on
  for the whole thread from before the engine is constructed.
- The eight rows: `E/Swell · cut at the key-off` (chord, repeat, arp),
  `E/Gated stab · organ` (chord, repeat, arp),
  `T/restrike after full release :: restrike`, `C/VL tier 1, a faded slot`.
- In every one, only samples differ (92 to 200 samples per row); no row changed in
  its blade events or load readouts alone. Every difference is below 4.1e-308:
  subnormal output values that become exactly zero.
- Denormal control: `y = y * 0.5` from 1 over 1200 frames, through the same switch
  and digest: 104 of 2400 samples differ, first at frame 1023; digests differ.
  Must-read-zero control: the same path stopped at 900 frames gives equal digests.
- Context, not a verdict: 8 of 543 rows raised UFC or IDC with FZ on, and 8 of
  543 raised UFC with FZ off.
- No reference file was edited, regenerated or re-pinned.

**RealtimeSanitizer (part a).** `python3 tools/h2_engine_rtsan_check.py --full-from <stream>`:
the planted malloc was reported at `h2_engine_rtsan_probe.cpp:113`; the empty-scope
control read 0; the full run judged 543 of 543 scenarios (59447 render calls and
28693 event calls inside the scope) with 0 violations. By hand, once: a malloc and
free planted in a build-directory copy of `Engine::render` were reported at the
planted line of `engine.h`. A stream cut at 8 MB turned the check red. With no
Homebrew LLVM on the path it printed SKIPPED and exited 0.

**Ban-list lint (part b).** `python3 tools/h2_rt_lint_check.py`: 8 source files
under `h2/`, 0 banned constructs, 4 waivers in use, all four for the lifted swarm
core's `std::string` parameter-by-name methods and their `<string>` include (the
lifted file is byte-frozen and cannot carry a comment). Its control planted a
`push_back` inside `Engine::render` in a temporary copy of `h2/` and read red at
`engine/engine.h:246`.

## Provenance

- **Evidence consulted:** `docs/strategy/blind-spot-armor-phase2.md` §2, §3 (row
  P16), §5; DECISIONS.md ADR-199 and ADR-209 item 6; `h2/README.md`;
  `tools/h2_engine_selfdigest_check.cpp`, `tools/h2_engine_stream.h`,
  `tools/rtsan_check.py`, `tools/rtsan_probe.cpp`, `tools/h2_rules_check.py`,
  `tools/playbook_check.py`, `tools/test_table_check.py`,
  `tools/weakening_check.py`; LIBRARY L0032, L0068.
- **Alternatives rejected:**
  - Editing `tools/h2_engine_stream.h` to add a hook: it is compiled into four
    wired gates. The probes use a copy instead, and each proves the copy against
    the original on every run.
  - Running the stock self-digest binary with the bit set from outside: it could
    not scope the bit to `render`, nor say how far a changed row moved.
  - An engine-built denormal control: no engine fault flag makes one, and engine
    code is read-only here. The control is a decaying one-pole through the same
    switch and digest.
- **Verify:** `./verify fast`, exit 0, on git `27047c2` plus this change set
  (`.harness/last-verify.json`), run on the tree as committed: the two held-out
  files were moved out of `tools/` first. An earlier run, with them on disk, was
  red on three gates:
  - `build_flags_check`, on the held-out wrapper (quoted above; not fixed here,
    it is the human's ledger);
  - `include_check`: `tools/h2_engine_fz_probe.cpp` used std symbols through the
    file it includes. Fixed: it names its own headers.
  - `tolerance_registry_check`: two names in `tools/h2_engine_hooked_replay.h`
    read as tolerances (`limit`, a scenario count, and an assignment of 0 to the
    engine's `faultEps`). Neither is a comparison tolerance. Fixed without
    touching the registry: the count is now `stopAfter`, and the assignment is
    gone because a fresh engine already holds 0 there. Both measurements were
    re-run on the final sources and gave the same numbers.
- **Open questions:**
  - Whether 8 rows moving by less than 4.1e-308 counts as "any change" under
    ADR-209 item 6 is the human's ruling. The item as written sends the count
    back to the human; nothing here pre-empts it.
  - The held-out files need the human's or the lead's decision (above).
  - `tools/playbook_check.py` rule 3 (no clock, no unseeded random source) scans
    `src/` only; nothing scans `h2/` for those. Not in P16's list; noted.
  - The engine's `setString` path calls `std::strtod` (4 calls in the stream, 0
    violations). What libc does inside it on other inputs is not proven.
  - The x86 pair (FTZ and DAZ) was not measured; this probe is arm64 only.
