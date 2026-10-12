# h2-switch-rounding — the flush-to-zero switch forces round-to-nearest inside its scope

- **Queue item:** ROADMAP B448 (Blind-Spot Armor), the flush-to-zero work (ADR-209 item 6,
  ADR-211 item 1; merged as PR #1058). The lead's brief of 2026-10-12 carried the human's
  ruling on the rounding mode: option A of `docs/proposals/h2-flush-to-zero.md` section 8,
  with the count from option B. The brief says the lead records it as ADR-213.
- **Why:** the pinned bits need round-to-nearest, and the switch left the mode to the host.
  Measured on 2026-10-11: a host that calls in rounding toward zero moved 484 of 543 scenarios
  out of the parity bound and changed every pinned digest. So the sound depended on the host.
  The switch now owns the rounding field as well as the flush bits, gives the host its own word
  back, and reports the mode the host arrived with so the shell can count such hosts.
- **Dated 2026-10-12** after the brief and the ruling. The dev Mac's clock read the evening of
  2026-10-11 while the work was done.

**The ruling is cited from the brief, not from the tree.** `DECISIONS.md` on the commit this
branch was cut from (`73ec3c5`) has no ADR-213, and ROADMAP's B448 row records the finding and
not the choice. Every tracked file here says "the human's ruling of 2026-10-12, which the lead
records as ADR-213". If the number changes, those citations change with it.

## What is in this change set

| File | What changed |
|---|---|
| `h2/flush_to_zero.h` | The word written on the way in is the caller's with the flush bits set AND the rounding field cleared (round-to-nearest, ties to even, on both architectures). `underFlushToZero` returns the rounding mode the caller arrived with (`horde2::Rounding`). `flushToZeroState()` also reads rounding back: the field, and four additions. The header says which fields the switch owns and which it leaves. Tools-only: read-only copies of the two owned masks, and a call that plays a host whose other fields are flipped. |
| `tools/h2_engine_stream.h` | `control ROUND` now proves the switch SETS the mode; `control WORD` is new. The tally also counts the replays whose caller arrived in another mode. The first clean replay is kept so the controls can make it again from other hosts. The tool's own `roundsToNearest` is gone; the header's read-back and a three-addition classifier replace it. |
| `tools/h2_engine_hooked_replay.h` | Offers its first clean replay to the same control (one line), so a probe that only replays through the hook still has a row for it. |
| `tools/h2_engine_selfdigest_check.cpp`, `tools/h2_engine_parity_check.cpp`, `tools/h2_engine_rtsan_probe.cpp`, `tools/h2_engine_fz_probe.cpp` | Comments, and the count in one message string of the realtime probe: three controls where there were two, and what ROUND now means. Nothing they compute or judge changed. |
| `h2/README.md`, `docs/proposals/h2-flush-to-zero.md` | The choice, what was built, what was measured, the fields a later ruling could add, and new words for the shell acceptance row OG-4. |

Not touched: the two self-digest reference files (`git diff origin/main` is empty for both),
`docs/armor/goldens.json` (no cited line moved: its citations into `h2/README.md` are lines
118 to 136, and the edit is below them), `tools/h2_engine_fz_probe.py`, `verify`,
`CMakeLists.txt`, every ledger.

## Evidence consulted

- `docs/proposals/h2-flush-to-zero.md` in full, `h2/flush_to_zero.h`, `tools/h2_engine_stream.h`
  and `traces/2026-10-11-h2-flush-to-zero.md`, as the brief asked. The other tools in the
  table, `tools/h2_engine_rtsan_check.py`, `tools/h2_engine_fz_probe.py`,
  `tools/h2_rt_lint_check.py`, `tools/h2_rules_check.py`, `tools/golden_pin_check.py`,
  `tools/h2_libm_count.py`, `verify`'s `full()` lines for the engine,
  `docs/proposals/h2-shell-acceptance.md` row OG-4, `docs/armor/goldens.json`.
- ROADMAP B448 and `DECISIONS.md` (ADR-211, ADR-212; no ADR-213).

## Measured (dev Mac, darwin-arm64, Apple clang 16, Node 24)

On one render of the 543-scenario stream. Scratch copies were built outside the tree with the
gate targets' own flags and were not committed.

**Before any edit** (`73ec3c5`): both self-digests GREEN, 543 of 543; parity GREEN, 540 of 543.

**With the process entered in the default mode, after the change:** both self-digests GREEN,
543 of 543, TOTAL `894c3bc4b62bcab0` as before. The parity check's output differs from the
run before the change in its control lines and nothing else.

**The whole set from a host in another rounding mode.** A scratch copy of
`tools/h2_engine_stream.h` whose `replay` calls `fesetround` immediately before the switch and
aborts afterwards unless the thread is still in that mode (by `fegetround` and by arithmetic):

| Every replay entered rounding | Parity build | Product build |
|---|---|---|
| toward zero | 543 of 543 bit-identical, GREEN | 543 of 543, GREEN |
| upward | 543 of 543, GREEN | 543 of 543, GREEN |
| downward | 543 of 543, GREEN | 543 of 543, GREEN |

The tools' own count read "543 of them called from another rounding mode" in each.

**The whole process rounding toward zero** (`fesetround` at the first line of `main`): 507 of
543 rows differ in both builds while the three controls pass. The cause is the tool's reading
of each script, which happens before the switch: with only that reading put back to nearest
and the rest of the process left rounding toward zero, 543 of 543 in both builds, GREEN.

**Scratch mutations of the switch,** product-build self-digest, default entry unless said:

| Mutation | Digests | `control FZ` | `control ROUND` | `control WORD` |
|---|---|---|---|---|
| (a) the rounding write does nothing (both builds) | 543 match | PASS | FAIL | FAIL |
| (a), every replay entered toward zero (both builds) | 0 match | PASS | FAIL, 0 of 543 | FAIL |
| (b) the restore gives back all but the rounding mode | 543 match | PASS | FAIL | FAIL |
| (b) the restore removed | 543 match | FAIL | FAIL | FAIL |
| (c) the write also clears default-NaN | 543 match | PASS | PASS | FAIL |
| (c) the write also sets default-NaN | 543 match | PASS | PASS | FAIL |
| (c) the write also clears a trap enable | 543 match | PASS | PASS | FAIL |
| (c) a constant word is written | 543 match | PASS | PASS | FAIL |

Each ends RED, exit 1. The failing lines are quoted in the proposal, section 2. The three
toward-zero, upward and downward runs, mutation (a), both forms of (b) and the first (c) were
run again on the final sources. The last three (c) rows were cut from the tree two
comment-only edits earlier.

Over mutation (a) the parity check ends RED, exit 1, with 540 of 543 still at parity (the two
controls and its three determinism rows fail), and the FMA control exits 2, "the run broke".

**Found on the way.** With the restore removed, the first version of the tools-only host left
the trap enables on, and the tool died on an illegal instruction during its controls with its
output lost. The host now puts its word back by a plain write, so the same mutant reports.

**The other gates,** final sources, same stream: see "Verify".

**x86-64 under Rosetta 2:** the parity-build self-digest tool as an x86-64 binary: unkeyed
SKIP, "controls fired"; `control ROUND` and `control WORD` PASS; the field `fesetround` moves
reads 0x6000, as declared. A translator, not an x86 CPU.

**An unknown target** still stops at the `#error`: checked by defining by hand the macros of
arm64 under Microsoft's compiler, 32-bit x86 and ARM64EC.

## Alternatives rejected

- **Keep `control ROUND` on the header's own read-back alone.** A read-back written beside the
  write shares its mistakes. The host's mode is set and read with `<cfenv>`, and arithmetic in
  the tool is a third source.
- **Compare the four hosts' replays only with each other.** A script that did not care how it
  is rounded, or a `fesetround` that did nothing, would read "identical". Each host's replay
  with NO switch must give other bits.
- **A third switch variant that keeps the host's rounding, for that must-differ half.** The
  replay's body run with no switch shows the same thing and adds nothing to product code.
- **Give the tools a raw writer of the control word.** The tools-only host flips a fixed set
  of fields the switch does not own and nothing else.
- **Flip the exception masks on x86-64.** Clearing one turns the next inexact result into a
  fault. Only the six sticky flags are flipped there.
- **A flag on the self-digest tools to enter replays in another mode.** It would make the
  whole-set proof repeatable without a scratch copy. Left as a question: it is a new way to
  run a gate binary, and the brief asked for the proof by hand.
- **Mark `underFlushToZero`'s return must-use.** Left as a question for the lead.
- **Write one whole known control word.** It would own fields nobody has measured.

## Verify

- `./verify fast`: exit 0, on git `73ec3c5` (the base) plus this change set, staged
  (`.harness/last-verify.json`).
- **One red run on the way, outside this change.** An earlier `./verify fast` on the same
  staged files exited 1 at `lab load check FAILED`: `fx-chain-morph-lab-r3.html  script block
  2: Error: Script execution timed out after 20000ms`. That check gives each lab script 20
  seconds of wall-clock time; the machine's load average was above 100 (other sessions' builds
  and this dispatch's own scratch runs). This change touches no lab. With the scratch runs
  stopped, the next run was green, and so was the one before it. A wall-clock limit inside
  `verify fast` can be turned red by load alone; that is the lead's to weigh.
- The engine's `full()` lines, run by hand after a rebuild of the final sources, on one render
  of the stream:
  - TRUNC: the parity check on a stream cut at 8 MB exits 1 with a `FAIL  STREAM` row; the FMA
    control on it exits 2.
  - `h2_engine_parity_check`: exit 0, "GREEN — 540/543 scenarios at parity (rms < 1e-06, max <
    1e-06, events and readouts identical), 3 excluded as chaotic (onset window 384 frames,
    events, readouts, bounded and finite), 0 miss parity, 0 red"; mean bit-exact share 37.00%;
    `control FZ`, `control ROUND` and `control WORD` PASS over 753 replays.
  - `parity_floor_check.py --stream`: exit 0, "GREEN (h2_engine_parity_check: 543 scenarios; 4
    controls red; the untouched stream is the gate's own green run)".
  - `h2_engine_fma_control`: exit 0, "FIRED ... 24 of 543 scenarios miss parity"; the three
    controls PASS over 543 replays.
  - `h2_engine_selfdigest_check`: exit 0, "GREEN — parity build, 543 scenarios against
    darwin-arm64.appleclang-16.node24; 0 differ; 0 red"; COVERAGE, FZ, ROUND, WORD, ULP,
    KEYSELF and FAULT all PASS; TOTAL `894c3bc4b62bcab0`.
  - `h2_engine_selfdigest_product`: exit 0, "GREEN — product build, 543 scenarios ...; 0
    differ; 0 red"; FZ, ROUND, WORD, ULP and KEYSELF PASS; 543 of 543 rows share the parity
    digest.
  - `tools/h2_engine_rtsan_check.py`: exit 0, GREEN. The planted allocation is reported, the
    empty scope reads zero, 543 of 543 judged, 0 violations, 59,447 render and 28,693 event
    calls inside the realtime scope; the three controls PASS over 1086 replays.
  - `tools/h2_engine_fz_probe.py` (no gate), both builds: MEASURED. The pinned scope equals all
    543 pins; forced off, 8 rows differ, 8 in their samples (largest 4.09e-308), 0 in events,
    0 in readouts; the render scope and the calls scope differ on 0 rows.
  - `tools/h2_libm_count.py` (context): still matches the committed count.
  - `measure_h2_engine` builds. It was not run.
- `tools/h2_rt_lint_check.py`, `tools/h2_rules_check.py` and `tools/weakening_check.py` are in
  `./verify fast` and green: 0 banned constructs in the header, 0 increases.
- `./verify full` as a whole was NOT run by this dispatch (the brief leaves it to the lead).

## Open questions

1. **The ruling's record.** ADR-213 is not in `DECISIONS.md` yet; see the note at the top.
2. **x86 hardware and Windows.** The x86-64 rounding half is written from the documentation
   and has run only under Rosetta 2. CI's Linux lanes are its first run on an x86 CPU.
   Microsoft's compiler has not built the new control code; nothing has run on Windows.
3. **The shell's counter is not built,** and nothing holds "every number the engine is handed
   is computed inside the switch". Both are in the proposed words for OG-4.
4. **The other fields of the control word** (trap enables, default-NaN, FEAT_AFP's AH and FIZ,
   the x86 masks) stay the host's. The proposal, section 8, lists what each would change.
5. `./verify full` as a whole was not run by this dispatch (the brief leaves it to the lead).
