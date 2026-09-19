# wire-checks-inversion — four checks wired, the wiring default enforced, three dead exports deleted

- **Queue item:** ADR-179 §4 (the wiring inversion the human ratified 2026-09-19)
  and ADR-179 H2 (the three no-caller debug exports, human ruling "whatever you
  recommend"). B159/B160 supply the evidence.
- **Why:** Under the old default a check was standalone until the human wired it,
  so a calibrated oracle could sit green and gate nothing. Two of the four wired
  here (ncap, tseed) guard defects that SHIPPED and were fixed 2026-09-18; one
  (intent) was unrunnable in a fresh checkout because its fixtures were hand-made.
  The inversion is only real if something enforces it, hence the
  `test_table_check` rule; and the same reasoning deletes an export nobody calls.

## What changed

**Commit 1 (b783f13) — `verify`:**
- `ncap_check`, `tseed_check` → `full`, beside `alias_check` (both core-direct
  SAW-fidelity rows, matching the sibling's placement per the brief).
- `station_check` → a golden chain beside swarmalator's: generator `--selfcheck`,
  generator, binary with the golden dir as argv[1].
- `tools/labharness/station_check.mjs` → top of `full()`, lab_load_check's capture
  idiom (quiet on green) plus an echo of its final GREEN line, because a gate whose
  green run prints nothing reads as no gate. Not in `fast()`: fast is the
  seconds-scale CI-blocking leg and this is ~9 s of DSP measurement.
- `intent_check` → a golden chain at the end of `full()`; the two generation lines
  are the fix for B160's "FAIL no manifest at build-golden/intent".
- Headers in `tools/{ncap,tseed,station,intent}_check.*` that claimed UNWIRED /
  "hand-run" corrected in the same commit.

**Commit 2 (640e669) — `tools/test_table_check.py` rule 5:** every
`tools/*_check.cpp` and `tools/labharness/*_check.mjs` is either invoked by
`./verify` or carries `UNWIRED: <reason>` in its first 40 lines. Grammar defined
in the docstring, printed in the failure message.

**Commit 3 (2559c63) — `src/hypersaw_clap.cpp`, `src/hypersaw_debug.h`:**
`hypersaw_debug_panic`, `hypersaw_debug_modpolarity`,
`hypersaw_debug_set_engine_revision` deleted, definition and declaration together.

## Evidence consulted

- `verify` end to end (the ADR-171 block, the golden chains in `full()`,
  `lab_load_check`'s invocation idiom).
- Headers of `tools/ncap_check.cpp`, `tools/tseed_check.cpp`,
  `tools/station_check.cpp` (its own "when wired it belongs beside swarmalator's
  chain"), `tools/labharness/station_check.mjs`, `tools/intent_check.cpp`,
  `tools/golden/gen_intent_goldens.mjs`, `tools/measure_cpu.cpp` (CPU_JUDGE).
- All four measured GREEN standalone BEFORE wiring (a red check must not be
  wired): ncap PASS 0.39 s; tseed 0 failures 0.32 s; station chain 3.32 s
  selfcheck + 2.33 s generate (30 scenarios × 2 rates) + 3.54 s check, worst
  parity rms 1.257e-07; labharness station 21 checks 0 failed 9.10 s; intent
  chain 0.25 + 0.13 + 1.81 s, 32 fixtures 0 failures.

### Must-fail control for the new rule (both directions)

A planted `tools/planted_check.cpp` with neither wiring nor the line:

```
test_table_check: FAILED
  tools/planted_check.cpp is not run by ./verify and states no reason (ADR-179 §4:
  wire it, or give it a line `UNWIRED: <reason>` in its first 40 lines)
exit=1
```

The same file with an `UNWIRED:` line added: `GREEN … 12 declaring UNWIRED`,
exit 0. Plant removed; the clean tree reads `43 check files wired into ./verify,
11 declaring UNWIRED`. A rule that can only say "fine" has not been shown able to
say anything else (L0032).

### The eleven UNWIRED declarations, and which ones are findings

| file | reason as it now stands |
|---|---|
| `tools/combguard_check.cpp` | standing human ruling, stated in-header, pre-dates the inversion |
| `tools/fxxfade_plugin_check.cpp` | same ("human gate") |
| `tools/paramclass_check.cpp` | same (cites the charter's standing rule + ADR-171) |
| `tools/svf_check.cpp` | same (cites delay/strata/voicetap as the same status) |
| `tools/labharness/reverb_check.mjs` | ~45 s of measurement (V6 alone is 38), lab has no port |
| `tools/labharness/subosc_check.mjs` | **FINDING — only said "run by hand", no reason** |
| `tools/delay_check.cpp` | **FINDING — no reason stated anywhere in the header** |
| `tools/mixer_check.cpp` | **FINDING — none** |
| `tools/mod_check.cpp` | **FINDING — none** |
| `tools/strata_check.cpp` | **FINDING — none** |
| `tools/voicetap_check.cpp` | **FINDING — none** |

The six findings carry `UNWIRED: reason not stated — see B159` rather than a
reason invented on their behalf. None was wired blind. The four "standing ruling"
reasons are the very default ADR-179 §4 inverted, so their line records that it
pre-dates the inversion and was not revisited in this PR — they are open
questions, not settled exemptions.

`cpu_check` is a real exemption ("a timing measurement must not gate CI"), and its
line is in `tools/measure_cpu.cpp` — but that file is OUTSIDE the rule's glob,
because `cpu_check` and `alias_check` are both built from a `measure_*.cpp`
source. The boundary is stated in the checker's docstring rather than papered
over; widening the rule to CMake target names is a decision for the lead.

### No-caller proof for the three deletions (run before commit 3)

`grep -rn` each of the three names across `tools/ src/ tests/ docs/design verify`,
then `.github/` and `src/gui/` separately, then repo-wide excluding `libs/`,
`build-*`: **six hits, two per name** — the definition in `src/hypersaw_clap.cpp`
(7087, 7106, 7300) and the declaration in `src/hypersaw_debug.h` (216, 100, 53).
`docs/` and `.github/` returned nothing. The shell methods behind them keep their
readers (`panicWithDump` via `hostIf.panic` + `hypersaw_test_panic`;
`modSetPolarity` via `hostIf.modSetPolarity`; `setEngineRevision` via the
state-load path), so this removes doors, not rooms, and leaves no dead code.

## Alternatives rejected

- **Wiring the other nine unwired C++ checks too.** Out of the brief's sanctioned
  set, and none was run green first. Declaring their status is the honest move;
  wiring unmeasured checks is how a gate lands red.
- **Widening the rule to CMake `add_executable(<n>_check …)` target names** so it
  reaches `cpu_check`. More machinery, and it converts a documented boundary into
  an invented policy. Reported instead.
- **Putting the STATION lab check in `fast()` beside `lab_load_check`.** Nine
  seconds is not the CI leg's budget; `full` is where Layer-E measurement lives.
- **Editing `README.md`'s "31 gates" / "Five standalone oracles" lines.** Outside
  the files the brief permits; raised to the lead instead (both are now stale, and
  the second was already stale before this PR).

## Verify

`./verify full` — exit 0 on `2559c63`
(`.harness/last-verify.json`: `{"target":"full","exit":0,"git":"2559c63","ts":"2026-09-19T23:27:54Z"}`).

The four wirings, visible in that log:

```
15:station_check.mjs: GREEN — 21 checks, 0 failed
1330:ncap_check: PASS
1356:OK   tseed_check: 0 failure(s)
1785:station_check: GREEN (0 failures; worst parity rms 1.257e-07)
2071:32 fixtures, 0 failure(s)          [intent_check]
8:test_table_check: GREEN (191 tests — 106 agentic, 85 human; 16 awaiting an
  oracle; 43 check files wired into ./verify, 11 declaring UNWIRED)
```

**What the wiring costs.** The added steps, timed individually: 21.2 s
(labharness station 9.10 + station chain 9.19 + intent chain 2.19 + ncap 0.39 +
tseed 0.32). Whole-run wall clock is NOT a clean read of that here — the warm
baseline run (1:24.83) re-linked the three plugin bundles and the post-change runs
(1:14.39, 1:09.00, 1:08.21) did not, so incremental-link variance (~25 s) is
larger than the wiring itself. Report the 21.2 s, not the difference of the
totals; on a no-relink warm run `full` now takes 68 s, implying ~47 s before.

## Open questions

- Six checks now declare `UNWIRED: reason not stated` (above). Each needs either a
  wiring decision or a written reason; they are listed as B159 material.
- The rule's file-name glob does not reach `cpu_check` / `alias_check`
  (`measure_*.cpp` sources). Widen to CMake target names, or leave the boundary
  documented?
- `README.md:406` says "31 gates" and `README.md:461` says "Five standalone
  oracles are green but not wired (delay, STRATA, voice-tap, SVF, comb-guard)".
  Both are stale — the second was already wrong before this PR (eleven, not five)
  and the first is now wrong by four. README is outside this brief's file scope.
- CLAUDE.md §Domain's "fifteen gates" enumeration is the lead's to update.
