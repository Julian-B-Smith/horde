# b23-osc-sources — B146 bass-mono placement built; B23 increment 3 STOPPED on a measured id collision

- **Queue item:** B146 (built) · B23 increment 3 (NOT built — blocked, evidence below)
- **Why:** The dispatch carried two rows. B146's acceptance (d)/(f) is satisfiable
  against the tree as it stands and is built here. B23 increment 3's acceptance (b)
  rests on a statement about the id layout that the shell contradicts: raising the
  matrix to two sources does not "light up ids the layout already reserves" — it
  MOVES every slot-source crosspoint id by 64 and REUSES the vacated ids for the new
  source. Choosing the replacement layout freezes a public id space forever, which is
  the lead's call and an ADR, not an implementer's. Stopped and reported instead.

## What was built (B146)

`bassMonoPos`, id 267 (266 left free for the intent flag, stated at the row),
stepped {pre, post, both}, default pre, global, device class by override.
`pre` is the call the summed bus has always made, so the default chain is
unchanged code on unchanged data — bit-identity by construction, not by
measurement agreeing afterwards. The ADR-035 SVF is now `bassMonoStage()`, called
at two points with two state pairs; the second pair exists because `both` runs the
stages in series inside one block.

## The B23 increment 3 blocker, measured

Acceptance (b) says Src 2 takes `10000 + 1*64 + to` and `22001`, and "NO existing
id moves". `22001` is free and correct. The coeff half is not: `routingCoeffId`
takes the MATRIX index, and `from` indexes sources first then slots
(`src/hypersaw_clap.cpp:814` — "`from` indexes SOURCES first ([0, NSRC)) then SLOTS
(NSRC + slot)"), so NSRC 1 -> 2 shifts every slot's row.

Measured, not read: built the tree, dumped `hypersaw_debug_routing_ids()`, flipped
`kRoutingNSrc`/`RoutingMatrixT` to 2, rebuilt, dumped again, reverted. Diff of the
two dumps (`id,kind,from,to`):

    before: 10065,0,1,1;  = Slot 1 -> Slot 2
    after:  10065,0,1,1;  = Src  2 -> Slot 2      (same id, different cell)
            10129,0,2,1;  = Slot 1 -> Slot 2      (+64)

Six live ids change meaning or move (10065-10067, 10130, 10131, 10195), and the
`routing` state chunk is keyed on the numeric id precisely because "the id IS the
cell's coordinates" (`src/hypersaw_clap.cpp:2509`) — so a saved patch's
`10065:0.4` would silently load onto a different crosspoint. The same shift moves
every feedback id ADR-128's layout note says is already reserved. The naive flip
also INSERTS 5 cells into the middle of the routing block in `morphIds`
(19 -> 24 entries, insertions at positions 4, 8, 11, 13), and corner arrays are
positional for layout >= 2 (`morphSlotMap`), so every stored corner would be
re-read against the wrong parameters and the layout marker could not stay 4.

The only design that satisfies (b) as written is to decouple the id's `from`
COORDINATE from the matrix's `from` INDEX — freeze source 0 at coordinate 0 and
slots at 1+slot (today's numbers), and give sources >= 1 a reserved region of the
64-wide coordinate space (e.g. 32 + (src-1), safe forever under
`NSRC + NSLOT <= 32`) — plus building the new cells at the END of the routing
table so `morphIds` appends rather than inserts. That is a permanent change to
ADR-088's id scheme, it contradicts the brief's "NO new routing ids", and the GUI
pane decodes the layout independently (`mxDecode`/`buildMatrixPane` map row f to
`MX_BASE + f*MX_STRIDE + t`, which assumes contiguous rows), so it needs the
mapping too. Ruling requested before any of it is written.

## Evidence consulted

- ROADMAP B146 row on `origin/lead-records-40` (the "Increment 3 acceptance (a)-(g)"
  paragraph) — the acceptance text, read verbatim.
- `src/hypersaw_clap.cpp`: id-layout comment (805-833), `decodeRoutingId`,
  `makeRoutingTable`, `morphInit`'s routing append + shared lead index,
  `morphSlotMap`, `routingChunk`, the render loop and the rack call.
- `src/routing_core.h` — already generic in NSRC; needed no change and got none.
- `traces/2026-09-17-b50-dry-path.md`'s aliasing decision (the dry term initialises
  the output because source and output alias). Untouched: with one source the
  aliasing stands exactly as recorded.
- `tools/routing_check.cpp`, `tools/paramclass_check.cpp`, `tools/gen_gui_controls.py`.

## Alternatives rejected

- **Flip `kRoutingNSrc` to 2 and accept the shift.** Rejected: it silently
  re-points saved crosspoints and stored morph corners, which is the failure the
  append-only rule exists to prevent. Measured above rather than assumed.
- **Implement the coordinate remap unilaterally.** Rejected: it freezes a public
  id space, contradicts an explicit constraint of the brief, and is exactly the
  "architectural, irreversible" class the charter routes to the critic/human.
- **Build B146 with a per-source state array now.** Rejected: with one source the
  array is a one-element pre-emption of a design that has not been ruled on. The
  pre state becomes an array the day sources multiply; nothing else moves.
- **Declare `depends: bassMono=1` for the new row.** Rejected as out of scope: it
  would require regenerating `src/depends_graph.h`. `bassMonoHz` has the same
  property and the same empty column, so the row is consistent with its sibling.
  Named here as a follow-up rather than left as an unexplained gap.

## Calibration (the green is earned)

- PLANT: post stage never runs (`bassMonoPos == 99`) -> assertion 22 RED on two
  clauses (bypassed pre vs post 0.0527, must be 0; both vs pre 0, must be > 0).
  The compile line was read before the result (L0032's stale-object case).
- PLANT: post stage shares the pre stage's filter state -> assertion 22 stays
  GREEN. Recorded as a coverage boundary, not retried until something fired
  (L0033); the boundary is named in the assertion's own comment.

## Verify

- `./verify full` — exit 0, git `f564940`, `.harness/last-verify.json`
  `{"target":"full","exit":0,"git":"f564940","ts":"2026-09-18T20:17:43Z"}`.
- `parity_check: 156/156 scenarios within eps=1e-06 (worst 4.262e-09 @
  dyn-ring.seed42)` — unchanged; the default render path is the same code on the
  same data, so no re-baseline was needed and none was taken.
- `routing_check: GREEN (0 failures)`, including the new assertion 22:
  `energy 31.33, peak L-R 0.1272; bypassed pre vs post 0 (must be 0); control both
  vs pre 0.031 (must be > 0); Drive pre vs post 0.0506 (must be > 0); control
  stage-off pre vs post 0 (must be 0)`.
- `rtsafety_probe: GREEN (audio thread is allocation-free)`.
- The 40 factory files were regenerated (`gen_factory_bank docs/presets/factory`)
  and differ from HEAD ONLY by the new `"bassMonoPos":0` key and the build stamp —
  compared field by field, not eyeballed. `morphCorners` and `morphLayout` are
  byte-identical, which is the device-class choice showing up as evidence.

## Open questions

1. **B23 increment 3 needs the id-layout ruling above before it can be written.**
2. `paramclass_check` is NOT green, and neither failure is fixable in scope:
   - `T1a the table is the 243 frozen kParams rows` — now 244. The pin is doing its
     job; moving it to 244 is a one-number edit to `tools/paramclass_check.cpp`,
     which this brief does not scope.
   - `T4 every per-osc twin shares its base id's class` — **already red before this
     change**. T4 loops every host-exposed id >= 1000, which since B50 includes the
     19 routing ids; 14 of them compare against an `id - 1000` that is not a
     parameter (`10000` class 0 vs `9000` class -1). Measured with a scratch probe,
     not inferred. The twin rule needs an `id < kRoutingIdBase` bound.
   - `paramclass_check` is also not wired into `./verify` (deliberately — its own
     header says wiring is the human's call), so neither failure blocks a gate.
3. `bassMonoPos` is device class per acceptance (d). The B146 rationale also says
   it "flips by argmax under morph", which device class forbids (device parameters
   are not in the field at all). Built to (d); the rationale's clause is the one
   that should be corrected.
