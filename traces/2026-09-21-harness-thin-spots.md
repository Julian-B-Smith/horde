# harness-thin-spots — the ten unwired checks ruled, the lying marker killed, the LFO shape computed once

- **Queue item:** B159 (part 1), B190 layer 1 (part 2), B177 note (part 3)
- **Why:** The human, reading the published account of where the harness is
  thin, said "let's make sure to resolve these if we can". Three of the four
  thin spots were unwired oracles, a wiring marker that could lie, and a
  modulation shape computed twice. Each is the same shape of defect: a claim
  about the tree that nothing in the tree checks.

## Part 1 — ten checks, ruled one at a time on evidence (B159)

Every check was RUN before it was ruled on. Measured on this Mac, warm:

| check | time | result | ruling |
|---|---|---|---|
| `delay_check` | 0.27 s | green | WIRED |
| `mixer_check` | 0.22 s | green | WIRED |
| `mod_check` | 0.18 s | green | WIRED |
| `strata_check` | 0.18 s | green | WIRED |
| `voicetap_check` | 0.20 s | green | WIRED |
| `combguard_check` | 0.30 s | **RED on arrival** | fixed in the oracle, then WIRED |
| `fxxfade_plugin_check` | 0.23 s | green | WIRED |
| `paramclass_check` | 0.22 s | green | WIRED (captured — 289 lines a run) |
| `svf_check` | 0.20 s | green | WIRED |
| `labharness/reverb_check.mjs` | ~45 s | not run here | stays UNWIRED, reason sharpened |

Total added: **2.0 s** against the lead's measured 75.5 s warm `verify full`
baseline (+2.6 %). Two warm runs on the committed hash measured 81 s (with the
incremental rebuild of this change set) and **78 s** (fully warm) — consistent
with the baseline plus 2 s plus run-to-run noise, not a regression to
investigate.

The five that stated "reason not stated — see B159" had none. The four that
cited a "standing human ruling on gate scope … pre-dating the ADR-179 §4
inversion" were citing an exemption the 2026-09-19 inversion overturned, so
each got a fresh decision rather than an inherited one. Duplication was checked
per check: `fxxfade_plugin_check` is the PLUGIN-PATH half of `fxxfade_check`'s
core-level claim (L0031 — an oracle that builds the core directly gives the
shell path zero coverage), and the other eight guard cores nothing wired
touches.

`reverb_check.mjs` keeps its exemption on COST, which is a legitimate ground:
45 s on a 75.5 s `verify full` is a 60 % increase for a lab with no port, no
shell route and no caller, so a regression there cannot reach a user. The
reason now names the day it expires — the port's PR.

### combguard_check was red, and the fault was in the ORACLE

T2 ("the audio after the refused write is bit-identical to a single-Comb
render") failed. The cause: `Probe::set()` PROCESSES a block, and the refusal
arm makes two `set()` calls to the reference's one, so the reference entered
`render()` one block YOUNGER — and one block is enough for FX1's amount
smoother to move. A bit-identity claim was comparing two points on a ramp. Age-
matched with an empty second block; the failure detail now prints the first
differing sample and the worst |delta| so the next reader can tell a leak from
an artifact.

Calibrated against the real defect: with `kSlotMaxInstances[Comb]` raised to
`kRackSlots`, T1–T4 fail and T2 reads `worst |delta| 8.078e+30` — the
"blew up the audio" this gate exists for.

## Part 2 — B190 layer 1: a marker that cannot lie

Every `tools/*_check.{cpp,mjs}` (56 files) now carries **exactly one**
line-anchored `WIRED: <where>` or `UNWIRED: <reason>` in its first 40 lines,
and `test_table_check` CROSS-CHECKS the declaration against `./verify`'s own
text in both directions. The old rule tested that a marker EXISTED, never that
it was true, which is why `undo_check.cpp` carried "not wired" for two days
while `./verify` ran it, and `labharness/subosc_check.mjs` declared `UNWIRED:`
in a header whose next sentence explained where in `full()` it runs.

The grammar is line-anchored deliberately: a marker quoted mid-sentence is
prose, which is what lets a header discuss the rule or its own history without
re-declaring itself. Zero and two declarations are both failures.

`subosc_check.mjs` is the first thing the new rule caught (it had zero anchored
declarations while `./verify` named it); its header says so.

**Controls, both directions, run:**

```
--- CONTROL A: a file CLAIMING WIRED that ./verify does not name ---
  tools/planted_check.cpp: declares `WIRED: ./verify full.` but ./verify invokes no such check — the declaration is false, and a false one is worse than none (it is budgeted as coverage).
--- CONTROL B: a file CLAIMING UNWIRED that ./verify does name ---
  tools/svf_check.cpp: declares `UNWIRED: pretending nobody runs this.` but ./verify does invoke it — the declaration is false. Change it to `WIRED: <where>`.
```

**And the detector calibrates itself on every run** (`selftest()`), because a
gate for lying markers that silently stopped detecting them would be the same
bug one level up. Planting `return None` at the top of `judge()`:

```
test_table_check: FAILED
  selftest: 'lies: claims wired' was accepted and must not be
```

Stale wiring prose in 14 headers corrected to agree with the declaration;
honest historical notes (`polarity`, `undo`, `morphlayout`, `gui_history`) left
standing. Churn is one line per file plus those 14 sentences — no prose
rewritten for taste.

**A defect this change set produced and `fast` could not see:** the prose edit
to `tools/fxxfade_plugin_check.cpp` deleted its closing `*/`, and the file did
not compile. `./verify fast` does not build, so it was green on a broken tree;
`./verify full` caught it on the next build. Fixed in the Part 3 commit.

## Part 3 — the LFO shape is computed once (B177 note)

`drawLfo` in `src/gui/gui2.html` computed the LFO's shape from a JS
transcription of the shell's `lfoShapeAt`, with nothing holding them together.
The better fix was available and was taken — the fallback (a gate sampling both
laws) was NOT needed, because a cycle here IS a function of parameters alone.

`lfoCycleJson()` publishes N+1 = 129 points per LFO out of the SAME
`lfoShapeAt` the mod tick calls (`src/hypersaw_clap.cpp`, the switch at ~2788,
called at ~3744), bound as `hzGetLfoCycle` beside `hzGetSubWave`; `drawLfo`
paints what it is handed. There is no LFO shape law left anywhere in `src/gui/`
(`git grep lfoShapeAt -- src/gui/` returns three PROSE references and no code).
The grid is the deleted JS's grid, so the picture is unchanged for every shape
but S&H.

S&H got more honest: the GUI drew a fixed eight-value display list captioned
"illustrative" because it could not reach the engine's stream. The shell now
sends the first eight draws of that LFO's own seeded stream — advanced on a
LOCAL copy of the state, so publishing a picture can never perturb the sound —
and the caption states the sequence instead of disclaiming it.

**Gated anyway**, because "one law" is a property of today's source and the next
author can re-fork it in one line. `lfoenv_check` §E2 (new, through a new
`hypersaw_debug_lfocycle` door, the `hypersaw_debug_subwave` idiom) walks the
LIVE source slot phase by phase on a grid where one mod tick is exactly one
published point:

```
-- E2. the MOD page's LFO picture IS the live source (B177 note) --
  ok    shape 0: every published point is the live source value  —  4.68055e-08 worst over 127 phases (0 skipped at a jump)
  ok    shape 1: every published point is the live source value  —  0 worst over 127 phases (0 skipped at a jump)
  ok    shape 2: every published point is the live source value  —  0 worst over 127 phases (0 skipped at a jump)
  ok    shape 3: every published point is the live source value  —  0 worst over 127 phases (0 skipped at a jump)
  ok    shape 4: every published point is the live source value  —  0 worst over 125 phases (2 skipped at a jump)
  ok    control: walked against the WRONG shape's cycle, the same comparison fails  —  1.55092
  ok    S&H: the published first step is the value the first wrap holds  —  -0.620645 vs live -0.620645
  ok    control: a different patch seed publishes a different first step  —  -0.620645 vs 0.0427934
```

The tolerance is 1e-7 because the publisher prints `%.7f`. At its first `%.5f`
the row read 5e-6 and FAILED — the printf, not the law — and a tolerance
written to absorb that would have absorbed a real divergence of the same size,
so the transport's precision was raised instead of the gate's.

**Calibration, two plants, and the first is reported because it measured a
BOUNDARY rather than the law (L0033):**

- `lfoShapeAt(shape, k/N + 0.01, …)` (a phase shift) fails shapes 0–3 at
  0.0628 / 0.04 / 0.02 / 0.02 and **does not fail the square**. The square is
  two-valued, so the only index a small phase shift can move is the one
  straddling its edge — and that index is excluded by the derived jump rule.
  The square's row is blind to phase, by construction; the header records it.
- `0.9 * lfoShapeAt(…)` (an amplitude scale) fails all five at ~0.1.

## Evidence consulted

- `ROADMAP.md` B159 / B177 / B190 rows (read only; not edited)
- `verify` in full, `tools/test_table_check.py`, all 57 check files' headers
- `src/gui/gui2.html` `drawLfo`/`drawSubWave`/`drawShapeWave`, `bridge` table
- `src/hypersaw_clap.cpp` `lfoShapeAt`, the mod tick, `subWaveJson`,
  `shapeWaveJson`, `bendCurveJson`, the `hostIf` bind block
- `src/hypersaw_debug.h` (`hypersaw_debug_subwave`'s docstring as the model)
- `traces/2026-09-20-osc-sub-panel-mod-visuals.md` (B181 note 3's precedent)
- LIBRARY L0031 (a reference oracle certifies agreement, not correctness),
  L0032/L0033 (calibration; a plant that does not fire measured a boundary),
  L0051 (read `.harness/last-verify.json`, never the wrapper), L0055 (run a new
  check before calling its PR green — which is how combguard's red was found)

## Alternatives rejected

- **Leaving the four "overturned ruling" checks unwired on their inherited
  exemption.** The exemption no longer exists; an inherited reason is not a
  decision.
- **Wiring `reverb_check.mjs` for symmetry.** 45 s is 60 % of the whole gate
  for a lab with no consumer. Cost is a legitimate ground and the reason now
  states its expiry.
- **Weakening combguard's T2 to a tolerance.** The failure was an unfair
  comparison, not a strict threshold; fixing the comparison keeps the
  bit-identity claim that makes the row worth having.
- **A `--selftest` FLAG on test_table_check instead of running the selftest
  every time.** A calibration you have to remember to run is the failure mode
  this whole change set is about.
- **Relaxing the 40-line declaration window to "the leading comment block".**
  Thirteen headers are longer than 40 lines; their declarations were MOVED to
  the end of the header's first paragraph instead, where a reader meets the
  answer before the rationale.
- **For B177: the gate-both-laws fallback.** The brief allowed it if a bind
  were the wrong shape here. It is not — a cycle is a function of parameters
  alone — so the second copy was deleted instead, and the gate added on top.

## Verify

`./verify full` — exit 0 on `858500a`, per `.harness/last-verify.json`:

```
{"target":"full","exit":0,"git":"858500a","ts":"2026-09-21T21:15:52Z"}
```

Two runs on that hash: 81 s (with the incremental rebuild) and 78 s (warm).

## Open questions

1. **`paramclass_check`'s output is captured, not printed.** Its 289 lines are
   a per-id classification table the human is meant to REVIEW, and capturing it
   means `verify full` no longer shows it. The gate is now enforced every run
   (which it was not before) but the review surface moved to
   `./build-release/paramclass_check` by hand. If the human wants the table in
   the log, the capture is one line to remove.
2. **The declaration rule is keyed on FILE NAME**, inheriting the old rule's
   known boundary: `cpu_check` (built from `tools/measure_cpu.cpp`) and
   `alias_check` (`tools/measure_alias.cpp`) are outside it. Both carry a
   header line anyway. Widening to CMake target names is a decision, not a fix
   to smuggle in.
3. **The declaration cross-check verifies the NAME, not the TARGET.** A file
   declaring `WIRED: ./verify fast` that actually runs in `full()` passes.
   `morphlayout_check.cpp` was exactly that and was corrected by hand here.
   Parsing `fast()`/`full()` bounds is doable (the one-shot script that placed
   these lines did it); whether the gate should is a lead call.
4. **B190 layers 2 and 3 are untouched** (core STATUS vocabulary, audit
   cadence), per the brief.
5. **The LFO pictures were not visually confirmed in a live plugin.** §E2 proves
   the published numbers are the sound's; that the canvas draws them correctly
   is the same class of claim `drawSubWave` carries on precedent, unscreenshot.
