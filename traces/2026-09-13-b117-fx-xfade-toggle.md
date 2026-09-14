# b117-fx-xfade-toggle — an FX presence crossfade the human can switch on, hear, and rule on

- **Queue item:** B117 (dispatched under ADR-163, brief
  `briefs/2026-09-13-b117-fx-xfade-toggle.md` on `lead-records-7`).
- **Why:** Module TYPE is stepped, so under both morph modes a slot's module
  flips atomically (B49) — a tail cut, an entrance with no lead-in. The bounded
  pool (B95) dissolves that structurally at 1.1; ADR-163 wants the 1.0
  instrument so the choice can be HEARD before it is ruled on. Two dev
  parameters, default = today, and a standalone probe that proves the default
  is inert.

## What changed

- `src/fx_rack.h` — a preallocated per-slot `Slot shadow[kRackSlots]` plus a
  fade counter. `setType` arms BEFORE it stores, so the shadow captures the
  outgoing slot; a type change during a fade overwrites the shadow, which is
  how "at most two modules per slot" is true by construction rather than by
  counting. `processSlot` gained ONE branch (`fadeLeft > 0`), never taken at
  the default. The DSP switch was moved from `slots[idx]` to a `Slot &s`
  parameter — `idx` still selects the per-slot cores, which is exactly what
  lets a shadow keep rendering its own tail without a second copy of anything.
  `typeAllowed` counts a fading shadow as holding its type (`else if`, so a
  slot is worth at most one instance and no non-singleton is spuriously
  refused).
- `src/hypersaw_clap.cpp` — ids **264 `fxXfade`** {0 atomic, 1 crossfade,
  default 0} and **265 `fxXfadeMs`** {5–500, default 80}, both global,
  write-through to the rack at the existing choke point neighbourhood, read
  back from the rack. Not in the morph field (globals are excluded by
  construction).
- `src/gui/gui2.html` — a hand-written **FX crossfade (dev)** cluster on SET,
  both controls `data-fixed` (the rack is one object, dispatched by raw id).
- `tools/fxxfade_check.cpp` + CMake — standalone, unwired, 10 assertions.

## The one thing that does NOT crossfade, and why

Echo and Room are ONE `TimeCore` per slot in two modes, and writing `mode`
CLEARS both of that core's buffers (`time_core.h` `setParam`) — so an
Echo↔Room handover has no outgoing state left to fade, and running both modes
on one core would re-clear it every block. `sharesCore()` refuses to arm for
that pair and the flip stays atomic, exactly as today. Closing it needs a
second time engine per slot (~1.8 MB × 4 slots), which is a memory-budget
decision for the lead/human, not an implementer's. Every other type pair owns
disjoint state and crossfades.

## Evidence consulted

- ROADMAP B117 (acceptance, verbatim in the brief), B95, B49, B113; ADR-163.
- `src/fx_rack.h` state map per type: Filter → `Slot.zL/zR`; Notch →
  `notch[idx]`; Echo/Room → `timeFx[idx]` + `timeApplied[idx]`; Delay →
  `delayFx[idx]` + `delaySet[idx]`; Comb → the rack-shared `combs[]` bank;
  Comp → the rack-shared `compEnv`; Drive/Gain/Off stateless. Only the
  Echo/Room pair collides.
- `src/hypersaw_clap.cpp:2216` — the morph writes a param only when its value
  actually moves (`morphCur`), and stepped params always take `pickCorner`'s
  discrete corner value. So a blend sweep delivers ONE type write at the
  crossing; the rack's own `type != prev` guard is the second, independent
  reason a sweep cannot re-fade.
- `tools/combguard_check.cpp` (how to drive the rack through the factory and
  assert a refusal), `tools/gui_reach.py` / `tools/gen_gui_controls.py` /
  `tools/presentation_check.py` / `tools/test_table_check.py` (what a new
  param obliges).

## Alternatives rejected

- **A second bank of per-slot cores, ping-ponged on every type change** — would
  make Echo↔Room work and remove the special case, at ~15 MB per instance for a
  toggle that ADR-163 says will probably be buried. Rejected as an invention
  beyond the brief; the memory budget is a human call.
- **Copying the outgoing core's state into a dedicated shadow core** — 1.8 MB
  of memcpy on the audio thread. Rejected outright.
- **Driving T3 through the real morph engine** — the morph is explicitly out of
  scope, and the property under test (a re-written type never re-arms) lives in
  the rack, where fade epochs are observable. The limit is declared in the
  probe's docstring rather than papered over.

## Scope note (flagged to the lead)

Two files outside the brief's list were touched because a gate in `./verify
fast` forces them and refusing would have finished on red:
`src/param_presentation.tsv` (presentation_check's TOTALITY rule — every
declared param needs exactly one row) and `tests/feature_tests.tsv`
(test_table_check — every (page, feature) the GUI shows needs a test row; the
new SET group is a new feature pair). Two rows each, no logic. `depends` was
left blank deliberately: declaring it would regenerate `src/depends_graph.h`,
which feeds the morph hierarchy — out of scope — and the row is hand-placed, so
nothing is generated from the column anyway. The GUI row carries `data-when`
directly.

## Verify

- `./verify fast` — green (exit 0); `patch-scope params` 87 → 89, which is the
  gate confirming both new ids are raw-id dispatch and pinned.
- `./verify full` — green; output pasted verbatim in the PR body. The parity
  chains (parity · trajectory · force · spectra · filter · notch · swarmalator ·
  glide · time) stay bit-identical, which is the repo-level proof that "the
  default changed nothing".
- `build-release/fxxfade_check` — GREEN, 0 failures. Calibrated against two
  plants: `setXfade` forced false (5 assertions red — and the two that stayed
  green did so on SILENCE, which is why both now carry a vacuity control), and
  the same-type re-arm guard removed (the fade never ends: 60 fading blocks
  instead of 13; T3 red on both halves).

## Open questions

- **Echo↔Room stays atomic.** Stated above; needs a memory ruling to close.
- **Param ids 264/265 were the next free at `origin/main`.** A concurrent
  stream (`b116-penv-per-note`) is unpushed; if it also appends params, one of
  us renumbers. Append-only ids make that a rebase conflict, not a silent
  collision.
- **`fxxfade_check` is not in `./verify`** — the standing rack-probe ruling
  (combguard_check, delay_check). Wiring it in is a human decision; the
  feature-test row says `oracle=none` rather than claiming a gate that does not
  run.
- **Equal-power, not constant-gain.** Two correlated signals sum +3 dB mid-fade.
  ADR-163 says equal-power, so that is what shipped; if the human hears a bump
  at the crossing, that is the knob to revisit.
