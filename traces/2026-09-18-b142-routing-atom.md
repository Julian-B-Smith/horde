# b142-routing-atom — the routing block is ONE atom under quantum morph

- **Queue item:** B142 ("The routing block picks each crosspoint cell
  independently under quantum morph — a half-owned table"). Ruling: ADR-176 §3
  (2026-09-18) — under quantum morph all routing cells (ids ≥ 10000) share one
  lead index; cell-wise blend under BLEND mode.
- **Why:** `morphInit` appended the ADR-088 crosspoints with a comment citing
  ADR-125 for the OPPOSITE of what ADR-125 rules, and left `morphLead` at
  identity for the block. In the shipped default (`morphMode` 157 = 0, quantum)
  every cell therefore drew its own corner: a live table assembled from up to
  four corners — a topology none of them authored (ADR-124's chimera) and, under
  ADR-175, a mixture of two acyclic corner tables can carry a CYCLE, flipping the
  whole FX pass to sample-by-sample at 1.44× the cost with no corner declaring
  it. The fix is the mechanism the scale (ADR-109 A1) and the FX slots (ADR-124)
  already use, not a new one.
- **Evidence consulted:** `docs/proposals/b89-phase2-intent-resolver.md` R4 (the
  finding) and §5 (field length N = 243 = 224 prefix + 18 routing + 1 dry, so the
  whole block sits inside `MorphCore::kMaxParams` = 512 and no cell's Gumbel
  draws are the zero-filled tail); DECISIONS ADR-125 ("ARGMAX over topology means
  every route coefficient draws the same corner … all route ids point at one lead
  index") and ADR-125 A1; ADR-175 (cyclic topologies process sample by sample);
  `src/hypersaw_clap.cpp` — `morphInit`, `morphLead`/`morphGroupLead`/
  `morphGroupRange`, `morphStep`'s two branches, `decodeRoutingId`,
  `makeRoutingTable` (every routing row `stepped = false`), `morphOwnersJson`
  (ADR-110); `tools/routing_check.cpp` assertions 8–19 and their calibration
  notes; ROADMAP B142 and B89; LIBRARY L0032 / L0033 / L0051 / L0053.

## What changed

1. **`morphInit` (one block, `src/hypersaw_clap.cpp`).** After the scale and FX
   slot groups, every index whose id `decodeRoutingId` names is pointed at the
   FIRST routing index. Membership is asked of `decodeRoutingId`, not of an
   `id >= 10000` test restated here, so "which cells exist" stays one function.
2. **The comment above the crosspoint append** now says what the code does and
   cites ADR-125 for what ADR-125 says, with ADR-176 §3 as the ruling and the old
   text named so the correction is legible rather than silent.
3. **`tools/routing_check.cpp` assertions 20–21** (the brief numbers them 22–23,
   continuing the file's recorded offset), sharing ONE 200-position sweep with
   four corners holding four different tables.

## What 20–21 prove, and their controls

- **20 — one owner.** 200 pad positions (a 20×10 grid that includes all four
  exact corners), quantum default, temperature and coupling left at their shipped
  values: routing cells split at **0** positions (worst 1 distinct owner), and the
  block takes **4** different corners across the pad.
  *Control (a), must-read-nonzero:* the identity-lead parameters — same picker,
  same report, same positions — split at **196 / 200**. This is the pre-fix map
  read off the parameters that still carry it, so "the block agrees" cannot be a
  statement about a picker that returns one corner for everything.
  *Control (b):* the block must take ≥ 2 corners across the pad, or the cells
  agree only because nothing ever flips.
- **21 — no chimera.** At every one of the 200 positions the live matrix (read
  off the doubles `processBlock` multiplies by, via `hypersaw_debug_routing*`,
  never a readback) matched one authored corner table: **0 / 200** unmatched.
  *Detector calibration, both directions:* `matchCorner` hits a real corner table
  (yes) and misses a hand-built one-cell chimera (yes) — the chimera is exactly
  the shape the identity map produced. Corner values are 0.2 apart and a whole
  table spans 0.09, so no two corners can hold the same value for a cell.
  Tolerance is 1e-9, not bit equality: the morph glide computes `a + (b−a)·coef`
  and at coef 1 that is not exactly `b`; 1e-9 is eight orders below the corner
  separation.
- **The source plant (L0032/L0033).** `morphLead[i] = (uint32_t)i` for the block
  (the exact pre-fix state), object deleted and rebuilt, binary re-run:
  `FAIL … cells split at 184 (worst 4 owners)` and `FAIL … 184 of 200 live tables
  matched no corner`. **Assertion 11 stayed GREEN under the plant** — which is the
  direct evidence for acceptance (b): `morphStep`'s blend branch never consults
  the lead map, so BLEND is untouched by construction, not by measurement luck.
  Source restored from a pre-plant copy and verified plant-free before the build
  that `./verify full` ran on.

## By-design behaviour changes, stated rather than buried

- Under QUANTUM at INTERMEDIATE pad positions the owner map for routing cells
  changes: the block now reports one corner where it previously reported up to
  four, and the GUI's ADR-110 tint changes with it. That is the ruling, not a
  regression.
- At the four exact corners nothing changes (lw pins the winner), and under BLEND
  nothing changes at all.
- `morphGroupRange` is consumed by `morphToggleExempt`, so the routing block now
  **exempts as a unit** — exempting one crosspoint exempts the table. Entailed by
  the shared lead and consistent with how the scale and FX slots already behave;
  no oracle asserts it, and it is named here rather than left to be discovered.

## No fixture samples an intermediate quantum position with a non-default topology

Checked, because acceptance (d) asks: every factory preset holds
`morphX = morphY = 0` (40/40) and none carries a `rt.` key or a `routing=` chunk;
the three `tests/state_fixtures/*.txt` chunks hold `morphX=0 morphY=0 morphMode=0`
and no `routing=`. So every golden and fixture sits at an exact corner on the
default series chain, where all four corners hold the same table — the reason
`parity_check` 156/156, `state_check`, `statefix_check` and `bank_check` are
untouched, and nothing was regenerated.

- **Alternatives rejected:** (1) a second debug export for the owner map — the
  existing `hypersaw_debug_ownersjson` is the report the GUI reads, and a report
  only the oracle can see is not the report the player is shown; (2) computing
  the expected owner in the check from a second copy of `pickCorner` — a probe
  that restates the implementation cannot disagree with it (L0032); (3) also
  correcting `makeRoutingTable`'s "field BLENDS the coefficients as values"
  comment — out of scope for this brief, and it is not wrong under BLEND, only
  incomplete (see open questions).
- **Verify:** `./verify full` — exit 0, chained to the commit in one shell line
  (L0051); the hash is in `.harness/last-verify.json`.
  `routing_check: GREEN (0 failures)`;
  `parity_check: 156/156 scenarios within eps=1e-06`; `include_check: GREEN`
  (ADR-174); every other gate GREEN; no golden regenerated.
- **Open questions:**
  1. **ADR-176 is not in `DECISIONS.md`.** At `origin/main` (c82afd8) the file
     ends at ADR-175 + an ADR-166 A5. The ruling text used here is the brief's
     and ROADMAP B142's; the ADR record is the lead's to write, and this trace
     and two code comments cite a number that must exist for them to be true
     (L0009: wrong ADR cross-references are a recurring defect here).
  2. `makeRoutingTable`'s ParamDef comment still reads "continuous → morphable, so
     a corner holds a topology and the field BLENDS the coefficients as values
     (ADR-125: argmax is for structure)". True of BLEND, silent about the quantum
     default, and out of this brief's scope — worth a one-line follow-up.
  3. Assertions 20–21 sample 200 positions on a 20×10 grid with the default seed
     (1024), temperature (1) and coupling (0.3). They say nothing about other
     seeds; the claim they support is structural (one lead index) rather than
     statistical, so a seed sweep was judged redundant — recorded as the coverage
     boundary it is.
