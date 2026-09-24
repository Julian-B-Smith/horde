# b240-morph-append-site — the morph field gets one append site, and its whole layout-9 order is frozen

- **Queue item:** B240 (the row is carried in PR #746, `lead-records-86`; ratified 2026-09-24 as the first step of B252's SCALPEL order). Dispatched by the horde lead. The hazard was found by the B233 playbook (`docs/playbooks/integrating-a-source.md` §3.1–3.2, §13 Q1/Q2).
- **Why:** `morphInit` built the field in table-walking passes. A new per-osc row landed inside the frozen prefix. `kMorphLateIds` appended before the routing block. A second engine block's pass-1 rows would have landed before the SUB's structural rows and gate. None of the gates could see the last case. SCALPEL adds about 77 rows per oscillator, and every one must append without moving a saved corner.

## What changed (`3eaa71d`, `6f5617b`)

- **The passes are frozen at layout 9** (`src/hypersaw_clap.cpp`, at `kMorphTailIds`):
  - The per-osc prefix admits base ids below 182. Every id from 1 to 181 is a row, 182–199 are unused, and 200–288 are all global. That was read from the table and from the dumped order.
  - The engine passes admit only SUB rows 4000–4019.
  - The routing pass skips tail-listed ids. It cannot be bounded, because new source-row cells take mid-table ids.
- **One tail list.** `Plugin::kMorphTailIds` is appended after every pass, in order. A per-osc base brings its +1000 twin. It is empty at layout 9.
  - The comment states the marker rule: one appending change bumps the marker once, at all four writers (9 → 10 first).
  - It also states what old chunks do: `morphSlotMap` reads layout ≥ 2 as a 1:1 prefix, and `resetCorner` defaults the new slots.
  - `kMorphLateIds` is marked closed.
- **The fixture.** `tests/morph_order.txt` holds all 273 layout-9 slots. It was dumped from an `origin/main` `6256301` build before any source edit.
- **`morphlayout_check` T13**, wired in `verify` with the fixture path:
  - T13a: the fixture is readable.
  - T13b: the fixture is an exact prefix of the live order.
  - T13c: the marker equals the fixture's layout with no append, and is exactly one higher with a pending append.
  - T13d: no duplicate ids.
  - T13e is the control. A planted insertion, removal and swap at slot 240 (inside the routing block) must each be caught at that slot. A planted append must be admitted. The marker rule must reject an unbumped append and a double bump.
  - T13f: the field fits `MorphCore::kMaxParams` (512). `pickCorner` indexes that table by slot with no guard.
  - The `WIRED:` line moved to the header top, because the new paragraph pushed it past `test_table_check`'s 40 lines.
- **Comments corrected:**
  - the dispatch comment at `kEngineBlocks` ("touches nothing else" → dispatch only, plus the remaining sites);
  - "STATION appends here too" (the three passes would have inserted);
  - the `kEngineBlocks` row comment;
  - a stale "T10d" reference (now T12);
  - **T10b's label.** It was "no previously stored slot moved". It now says what it checks, a contiguous structural run at the tail, and that insertion is T13's to see. Its assertion is unchanged.
- **Playbook:**
  - §3.1(b) has the how-to-append steps, the marker rule, the freeze step and the quantum-draw caveat. §3.1(c) now covers T13.
  - §3.2 records the bound, §10 the pins, and §11 the B238 pre-read.
  - §12 items 3–4 are resolved. §13 Q1 is half answered, Q2 is answered, and Q2b is new.
  - Checklist line 3 is updated. The morph-section citations are re-anchored.

## Evidence (scratch builds, nothing below is committed)

- **Zero-behaviour.** Main, branch and demo were each loaded with all 48 inputs: 41 factory patches, 4 corner presets and 3 state fixtures. Each input was dumped (4 corners), rendered as loaded, and rendered again with morph ON and the pad at (0.3, 0.7).
  - **Branch vs main:** 96 of 96 renders are bit-identical, and 48 of 48 corner dumps are identical, including order.
  - The live order equals the fixture (T13b green at `6f5617b`).
- **Planted insertion (real source, not only T13e).** Scratch tree `plant`: SUB row 4020 is added, and the engine bound is widened by one to reproduce the pre-B240 passes.
  - T1, T1b, T10 and T10b all stayed **ok**.
  - T13b **FAIL**: "slot 264 MOVED: the fixture says 4000, the live order says 4020".
  - T13c **FAIL**: an unbumped marker.
  - T13e's in-run control plants an insertion, a removal and a swap, and catches each.
- **Demo append.** Scratch tree `demo`: per-osc row 289 (default 0.37) and SUB row 4020 (default 0.61) are listed in `kMorphTailIds`, and the marker is bumped to 10 at all four writers.
  - The live tail is exactly `289, 1289, 4020` after the 273 frozen slots.
  - T13 is green.
  - In all 48 inputs, every stored slot is unchanged in position, id and value, and all three new slots hold their defaults in all four corners.
  - `statefix_check`: GREEN, bit-identical to goldens. `state_check`: GREEN.
  - 95 of 96 renders are bit-identical to main.
  - **Expected reds at the first real append** (pins or regeneration, not defects):
    - T1b (id 289 < 3000);
    - T10b (any append ends the structural run's tail position);
    - `bank_check` re-save identity (41 files + calibration; the bank must be regenerated, as at B195/B203);
    - `paramclass` T1a (266 rows);
    - `subosc` 11d's marker (not run: no goldens in the scratch tree).
  - `undo_check` layer 4 found a second no-op id. This is a **hypothesis: an artifact of the dummy row**, which has no backing store.
- **Found, NOT fixed: an append re-deals the quantum draw.** `MorphCore::reshuffle` draws one row per slot and then `gShared`. The field's length therefore moves the shared vector.
  - The one differing demo render is "MO - Quantum Morph" with morph on at (0.3, 0.7).
  - A Python model (seed 1024, coupling 0.3, temperature 1, ignoring group leads, so an estimate) has 36–48 of 273 slots drawing a different corner mid-pad.
  - Scratch experiment: take `gShared` at the layout-9 count and put the new rows after it. All 96 demo renders were then bit-identical to main.
  - This departs from the morph lab's draw order, so it needs an ADR. It is recorded at `kMorphTailIds` and as playbook §13 Q2b.
  - Entailed from the code, not measured: every earlier append (layouts 3→9) re-dealt the draw the same way.

- **Evidence consulted:** ROADMAP B240/B252 on `origin/lead-records-86`; the playbook's §3, §12 and §13; `src/hypersaw_clap.cpp` (`morphInit`, `buildMorphOrder`, `kEngineBlocks`, `cornerJson`/`morphSlotMap`/`applyMorphChunk`, `subSetParam`); `src/morph_core.h`; `tools/morphlayout_check.cpp`; `tools/playbook_check.py`; `tools/test_table_check.py`; `tools/statefix_common.h`; `tests/state_fixtures/README.md`.
- **Alternatives rejected:**
  - Passes skip tail-listed ids, with no bounds. A new Device per-osc row could then not stay out of the field, and an unlisted row would insert destructively instead of simply being absent (T10 red).
  - The fixture as a string inside the check, like T1's `kFrozen`. A file makes an insertion a one-line review diff.
  - Rescoping T1b or T10b to the fixture's layout-9 slots. That admits what they reject today, which is a pin move and needs a ruling.
  - Fixing the quantum draw here. That needs an ADR, because it departs from the prototype.
- **Verify:** `./verify fast` exit 0. `./verify full` exit 0 at `6f5617b` (`.harness/last-verify.json`: `{"target":"full","exit":0,"git":"6f5617b"}`). This trace's own commit is verified in the PR description.
- **Open questions:**
  1. T1b's band and T10b's tail clause (playbook §13 Q1): both go red at B238's first per-osc append.
  2. The quantum re-deal (§13 Q2b): accept it behind `engine_revision`, or freeze the shared draw by ADR.
  3. `kMaxParams` = 512 against SCALPEL's size: 273 + roughly 154 per-osc slots leaves little headroom.
  4. `playbook_check` still reports about 97 drifted citations outside the morph section. Most predate this change; they were left alone to avoid colliding with #744.
