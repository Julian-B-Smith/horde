# b89-intent-core — IntentCore, the intent-bus golden generator, and the parity check

- **Queue item:** B89 phase 2a (`docs/proposals/b89-phase2-intent-resolver.md` §4,
  "2a — core, golden generator, parity check; no flag, no shell change"), under
  ADR-176 (ratified 2026-09-18).

- **Why:** The resolver's law needs a home and an oracle before it can touch the
  shell. `src/intent_core.h` is SPEC-INTENT-BUS §4.1–§4.5 and §7 as pure
  functions over caller-owned spans (R14's `ModCore` pattern), and
  `tools/intent_check.cpp` holds it to the protected prototype at 1e-6 over 32
  generated fixtures. Nothing in the audio path moved: no flag, no shell edit,
  no golden regenerated, one new unwired CMake target.

- **Evidence consulted:**
  - `specs/SPEC-INTENT-BUS.md` §3.5, §4.1–§4.5, §5, §7, §12 (the acceptance list).
  - `reference/intent-bus.html` lines 136–249 (the model block) and the `commit`
    click handler at line 469 — read, never edited.
  - `DECISIONS.md` ADR-176 (owner law, atoms, intents, pads, flag) and ADR-003
    (spec-in-code: the prototype wins a disagreement).
  - `docs/proposals/b89-phase2-intent-resolver.md` §0–§2 and the 2a increment.
  - `src/morph_core.h` (the mulberry32 stream and the params-then-shared draw
    order, reused rather than re-derived), `src/mod_core.h` (the core pattern),
    `tools/golden/extract_glide.mjs` + `gen_glide_goldens.mjs` (the extractor and
    generator idiom), `.gitignore:9` (`build-*/` covers `build-golden/`).
  - LIBRARY L0031 (a reference oracle certifies agreement only over the surface
    the reference spans), L0032/L0033 (calibration: both halves, and record a
    plant that does not fire), L0053 (submodules in a fresh worktree).

- **The (d) finding — prototype vs spec on the owner law.** CONFIRMED: the
  prototype's `pick(seed, w)` IS §4.3's cumulative walk — `let a=0; for (const c
  of CN) { a += w[c]; if (seed < a) return c; } return 'D'` — A→B→C→D, first
  corner whose cumulative sharpened weight exceeds the seed, strict `<`, with a
  D fallback. Its `reshuffle()` draw order is the eight parameters, then `home`,
  then `unison`, which is §3.5's order. So parity on the owner half is exact and
  no divergence had to be logged for it. Three smaller readings where the
  prototype is narrower than the spec text, resolved the prototype's way under
  ADR-003 and commented at the point of use in `intent_core.h`:
  1. `clamped[p]` is `|cv − v| > 1e-4`, not §4.5's exact `pre ≠ post`.
     (`IntentCore::kClampEps`.)
  2. `respectRange` re-clamps only inside the device tier's `if (gt === p)`, so
     a parameter with no device routing is never re-clamped however the armor
     toggle reads. (`evalParam`'s contract.)
  3. A device routing on an intent is added AFTER the pad's ±1 clamp, so an
     intent may legitimately exceed ±1. (`padIntentX` / `morphmod-intent`.)
  One thing ADR-176 leaves open and this core had to choose: WHERE the
  `morphCoup` shared seed comes from. Chosen: one extra draw appended AFTER the
  per-atom draws, so adding coupling leaves the atom seeds bit-identical —
  the same argument `MorphCore::reshuffle` already makes for its shared Gumbel
  vector (`src/morph_core.h:50-53`). Flagged below as an ADR candidate.

- **Alternatives rejected:**
  - *A general JSON parser in `intent_check.cpp`* — rejected; the generator
    writes a FLAT object instead, so an exact-key scan is complete, and a few
    hundred lines of untested parser do not stand between the oracle and the
    thing it checks.
  - *Option (b) of the plan's §2 (reimplement §7 commit in the generator)* —
    rejected for option (a): the commit handler is extracted as a second slice
    with `buildEditor()` stubbed, so T6 is oracle-parity and not spec-parity.
  - *Putting the LFO sources in `IntentCore`* — rejected; the prototype's two
    LFOs live in the check harness and in the generator's own model. The core
    owns tiering and ownership; the shell already has its own modulators.
  - *Coupling fixtures* — impossible: `morphCoup` under the walk has no
    prototype analogue, so it is pinned by invariants (coup 0 ≡ no coupling,
    coup 1 collapses the field) rather than faked as parity (L0031).

- **Calibration (the oracle checked against itself).** Seven defects planted in
  `intent_core.h` one at a time, each with its anchor asserted present (so a
  no-op plant is impossible) and each forced to a full recompile + relink with
  the binary's md5 printed. Six fired immediately; the seventh did not, and
  closing it is the one behavioural change the calibration bought:

  | plant | fixtures failing |
  |---|---|
  | A. walk boundary `<=` instead of `<` | **0 → 1 after the fix below** |
  | B. corner-tier sum offset by 1e-5 | 32 |
  | C. pad Y sign flipped | 24 |
  | D. spring integrates position before velocity | 21 |
  | E. promoted mods moved inside the clamp | 2 |
  | F. `clamped[]` uses exact inequality | 1 |
  | G. atom map ignored | 2 |

  Plant A is a genuine COVERAGE BOUNDARY of the reference fixtures, not a bug in
  the plant: mulberry32 seeds never land exactly on a normalised partial sum, so
  the strict/non-strict distinction is unreachable from any fixture. Recorded
  rather than retried until something fired (L0033), and closed by an invariant
  in `intent_check.cpp` that constructs `w = {.5,.5,0,0}` by hand and asserts a
  seed of exactly 0.5 belongs to the NEXT corner.

  Two stale-artifact traps hit and recorded on the way: the first calibration
  batch ran `-j8` and deleted only the object file, so a surviving binary made
  two different plants report the same md5 and the RESTORED tree report 2
  failures. Deleting the binary and requiring both `Building CXX` and `Linking`
  in the build output fixed it; the fingerprint column is what made it visible
  (L0032's "a stale object makes a plant fire exactly as predicted").

- **Verify:** `./verify full` — exit 0, git `c82afd8` (`.harness/last-verify.json`).
  The only `FAIL` string in the 900-line log is a control line that reads
  `OK   CONTROL: a planted masterVol change makes the re-save comparison FAIL`.
  `./verify full` is unchanged in content: the new target is compiled by the
  unfiltered `cmake --build` and then never run; no gate was added, no golden
  regenerated, and the 32 fixtures live in gitignored `build-golden/intent/`.
  `intent_check` itself: 32 fixtures, 0 failures, every must-fail control fires,
  5 invariants green.

- **Open questions:**
  1. **Where the shared seed is drawn** (see above). ADR-176 fixes the blend but
     not the draw; appended-last is this core's choice and wants ratifying —
     ADR candidate.
  2. **Wiring.** `intent_check` is standalone and unwired by charter; adding it
     to `./verify` is the human's decision (ADR-171 is the route). Until then a
     regression in `intent_core.h` is caught only by running it by hand.
  3. **The corner-mod tier is exercised only through the prototype's single
     LFO.** `IntentCore::stepParams` accepts a `cornerModSum` span that phase 2
     will pass as null (R5); the general corner-scope routing list is phase 3
     and has no oracle here.
  4. **`atomOf` grouping is proven against a prototype that has no groups.**
     Lead groups are expressed in the fixtures by giving the members equal
     seeds, which is behaviourally the same thing but is not the same code path
     the shell will use; the shell-side map lands in 2b.
  5. Nothing here has been run against the shell. Phases 2b–2d stand untouched.
