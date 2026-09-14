# Dispatch brief — B117: FX presence crossfade toggle (the ruling's instrument)

**Provenance.** HYPERSAW lead organ, 2026-09-13, for a scoped subagent with zero
conversation history. Motivating decision: ADR-163 (human 2026-09-13: "the FX
ruling needs a toggle we can test; once I rule on it, we can bury the toggle
unless both modes seem worth keeping exposed"). Read ADR-163, B49 (one corner
per slot), B95 (the bounded pool — the 1.1 structural answer this toggle
previews), and ROADMAP row B117 first.

## Acceptance criteria (verbatim from ROADMAP B117)

> (1) two DEV params, append-only ids (next free), labelled "(dev)", `data-fixed`, reachable on the SET page: `fxXfade` {0 atomic, 1 crossfade} default 0, `fxXfadeMs` 5–500 default 80. (2) With `fxXfade = 1`, a slot whose type changes keeps the OUTGOING module rendering from its own state in a preallocated per-slot SHADOW for `fxXfadeMs`, equal-power fading it out while the incoming fades in; a type change during a running fade drops the current shadow and starts a new fade from the then-outgoing module. (3) Comb's lines are rack-shared: a slot leaving Comb fades through the existing declick gate `g`, and the singleton guard (`typeAllowed`) counts a fading shadow as HOLDING Comb until the fade ends — no double-write. (4) RT: no allocation; at most two modules per slot, only during a fade; `rtsafety_probe` green. (5) `tools/fxxfade_check.cpp` standalone, unwired: T1 `fxXfade = 0` renders bit-identical to today through a type flip (the default changed nothing); T2 `fxXfade = 1`, a tailed module (delay/room) flipped to Off mid-tail: per-block RMS after the flip decays over ≈ `fxXfadeMs`, no single-block drop beyond 6 dB at the flip (the CONTROL: the same flip at `fxXfade = 0` DOES drop > 20 dB in one block); T3 a blend-mode sweep between corners with different slot types crossfades ONCE at the switch and never re-fades while the weight stays on one side; T4 a second Comb is still refused during a Comb fade-out. (6) The ruling comes after: bury (dev param kept for state compat, dropped from the GUI) or expose in Settings / right-click — not decided here.

## Where things are

- `src/fx_rack.h`: `struct Slot` (`type`, `amount`, `tone`, `mix`, filter
  memory), `Slot slots[kRackSlots]`, `Comb combs[kCombLines]` (rack-shared,
  with the declick gate `g` / `retuning`), the per-slot NotchCore, `setType`,
  `typeAllowed` + `kSlotMaxInstances`, and `processSlot` (map how each type
  keeps its state — some per slot, Comb shared; your shadow must carry
  exactly the outgoing type's state, so the cleanest shape is a per-slot
  `Slot shadow[kRackSlots]` plus a fade counter, and `processSlot` taking a
  `Slot&`).
- The single type-write choke point in the shell: `src/hypersaw_clap.cpp`,
  search `rack.typeAllowed(slot, (int)applied)` — every type change (host,
  preset, morph, GUI) passes here; the crossfade is armed here and nowhere
  else.
- Param table: `kParams` in `hypersaw_clap.cpp` (append-only ids — find the
  highest id in use and take the next two; there is a `(dev)`-labelled
  precedent, search `inertiaCurve`); `tools/gen_gui_controls.py --check` and
  `gui_reach` (in `./verify fast`) will tell you what the GUI must carry —
  put the two controls in the SET page's existing dev/advanced cluster with
  `data-fixed="1"` (patch-scope, see the B113 note in ROADMAP).
- Morph writes slot types through `applyParam` (stepped params flip to the
  picked corner, search `pickCorner`); you change nothing there — the choke
  point sees the flip.
- Rigs: `tools/notefuzz_scaffold.inc` (`#include <algorithm>` for MSVC);
  `tools/combguard_check.cpp` shows how to drive the rack headless and
  assert refusals; `tools/time_check.cpp`/`delay` goldens show which module
  has a tail to measure.

## Files in scope

`src/fx_rack.h`, `src/hypersaw_clap.cpp` (param table rows, the two
handlers/readers, the choke point), `src/gui/gui2.html` (two dev controls on
SET only), `tools/fxxfade_check.cpp` + `CMakeLists.txt` registration,
`traces/2026-09-13-b117-fx-xfade-toggle.md`.

**OUT of scope:** `ROADMAP.md` / `DECISIONS.md` (lead-only — report text);
`./verify` and gates; the morph engine; the mod grid / ENV 2 region of
`hypersaw_clap.cpp` (another stream is there — REBASE onto main before
opening your PR); `reference/**`, `specs/**`; the delay/room/notch DSP
itself; untracked root files.

## Constraints

Branch from `main` (pull first); absolute build paths; `./verify fast` after
each change set, gate every scripted commit on its exit code; `./verify
full` before done (parity chains incl. `time_check` must stay bit-identical
— the default is atomic); paste oracle output verbatim; red halts you; no
allocation on the audio thread; no machine identity in tracked files; MSVC
in CI.

## Deliverable

Branch `b117-fx-xfade-toggle`, pushed, PR via `gh pr create --base main`
whose body leads with the T2 RMS trajectories (atomic vs crossfade, as two
short tables) and the `./verify full` tail pasted from the run. **Never
merge.** Final report: PR URL, `fxxfade_check` output, verify tail verbatim,
the ids you took, the ROADMAP/DECISIONS text you would add.
