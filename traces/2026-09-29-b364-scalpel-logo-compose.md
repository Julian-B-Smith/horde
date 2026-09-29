# b364-scalpel-logo-compose — a seventh logo style, DROP + INSET + GRADIENT

- **Queue item:** B364. Row read verbatim from `origin/lead-records-143:ROADMAP.md`
  (`git fetch origin && git show origin/lead-records-143:ROADMAP.md | grep -E '^\| B(334|364) \|'`),
  alongside B334, whose trace (`traces/2026-09-28-b334-scalpel-lab-round11.md`) documents the
  six existing logo styles, their paint code, the per-paint cost table, load shedding, and
  `checkB334`. The human, 2026-09-29: "I really like the logo with the hard colored drop
  shadow; I also like the inset+gradient mode. Could we compose these two?"
- **Why:** A seventh logo style in `docs/design/scalpel-interface-lab.html`'s click cycle,
  composing DROP and INSET + GRADIENT unchanged, right after INSET + GRADIENT in the cycle
  and in `?logo=`, with the existing six kept exactly as they are.
- **Evidence consulted:** `traces/2026-09-28-b334-scalpel-lab-round11.md`; the lab's
  `LOGO_STYLES`, `logoTint`, `logoOutline`, `logoInset`, `logoDrop`, `logoGradient`,
  `logoDropColour`, `logoFrame` (paint order: `logoTint(dt); logoOutline();`, confirmed at
  the source, :3529 pre-change); `checkB334` (the click-cycle test, the storage-cleared
  control, the centring test — all already parametrised on `LOGO_STYLES.length`, so a new
  entry is picked up by the generic tests for free, except one hardcoded index the SOFT DROP
  control used, fixed below).

## What changed (`docs/design/scalpel-interface-lab.html` only)

1. **One `LOGO_STYLES` entry**, placed after `inset-gradient`, before `soft`:
   `{ id: 'drop-inset-gradient', name: 'DROP + INSET + GRADIENT', drop: 'crisp', inset: true, grad: true }`.
   No new paint function. `logoOutline` already runs `if (st.inset) logoInset(g); if (st.drop)
   logoDrop(g, ...)` in that order after the fill (which `logoTint` already renders with
   `logoGradient` when `st.grad`), so a style carrying all three flags composes them in
   exactly the brief's stated paint order (shadow beneath the letters; fill with gradient;
   inner shadow inside the stroke) for free: INSET composites inside the fill's own
   silhouette (`destination-in` with the fill), and DROP composites LAST via
   `destination-over`, which by definition lands behind whatever the canvas already holds.
   Colour: unchanged derivations — DROP's shadow colour and GRADIENT's stops both already
   read `LOGO.tint`, the fill of the moment, so the composite still shifts with morph and
   drift like every other style.
2. **A fixed hardcoded index.** The storage-cleared CONTROL row painted `LOGO.style = 5`
   assuming SOFT DROP was still index 5; the new entry took that slot, so SOFT DROP moved to
   index 6. Replaced with `LOGO_STYLES.findIndex(s => s.id === 'soft')`.
3. **`checkB334` extended** with a dedicated block (after the storage-cleared CONTROL, before
   the centring test): the composite differs from DROP-only and from INSET+GRADIENT-only by a
   measured pixel count (≥200, gate matches the existing per-style DROP-vs-FLAT gate), and two
   BENEATH checks — every opaque INSET+GRADIENT-only interior pixel is unchanged in the
   composite (destination-over cannot touch an opaque destination pixel), and the composite's
   shadow-only band (pixels INSET+GRADIENT-only leaves fully transparent) matches DROP-only's
   own paint there. A planted composite missing its `drop` key is the must-fail control (it
   collapses onto INSET + GRADIENT, 0 px differ, so `diffFromIG ≥ 200` catches it).
4. **Round 12 header block** (top-of-file comment) and the `lab-review` meta tag bumped to
   `B271 + B293–B364 · 2026-09-29`.

## Measured first (headless Chrome, real wall clock — see below)

- **Cost per paint** (warp included, 20 paints/style): FLAT 1.54 ms, DROP 1.97, INSET 1.81,
  GRADIENT 1.61, INSET + GRADIENT 1.84, **DROP + INSET + GRADIENT 2.27** (now the heaviest —
  three passes), SOFT DROP 2.00.
- **Load shedding with the new heaviest style:** 30 → 8 paints in 60 frames (same shed ratio
  B334 measured; `checkB334`'s shedding test already runs on `cost.indexOf(Math.max(...cost))`,
  so it exercised the new style automatically once it became the heaviest).
- **The composite differs from its parents:** 3635 px differ from DROP alone, 1744 px differ
  from INSET + GRADIENT alone (both ≥ 200, the same threshold the other five non-FLAT styles
  clear against FLAT).
- **Shadow beneath the letters:** 60 opaque INSET+GRADIENT-interior pixels sampled, unchanged
  by the drop pass; 200 shadow-only-band pixels sampled, matching DROP-only's own paint there.
  Both checks use `differ()`'s own ≤ 8-per-channel-sum tolerance, not bit-exact equality — the
  first version of this check used strict `===` and failed on real measurement: the canvas
  shadow compositor's worst observed pixel was 1 unit off on one channel (`[170,34,162,211]`
  vs `[171,34,162,210]`) between two separately-painted styles sharing the same alpha
  silhouette, i.e. float noise in the browser's own shadow rasteriser, not a logic bug. Adopting
  the file's existing noise floor (`differ()`'s `>8` per-pixel threshold) is the fix; it is not
  an invented number.

## Legibility (the brief's ask: check both themes and every screen scheme; adjust if the drop
   and inset fight, and explain the choice)

No adjustment to either DROP's or INSET+GRADIENT's own parameters was made or needed. The two
passes never occupy the same pixel: INSET is clipped to the fill's own silhouette
(`destination-in`), DROP paints only where the fill is NOT opaque (the shadow band established
above), so there is no pixel where the two composite together and could muddy each other — the
"fight" the brief warns about structurally cannot occur given how the two existing styles are
already coded, independent of colour. Screenshots (scratch, not committed;
`scratchpad/b364/shots/` in the session's temp dir, paths below) confirm this visually:
- **Light theme:** the composite shows the gradient's lift/lower on the fill plus a clean
  violet drop shadow beneath-right, matching DROP-only's own shadow exactly in hue/position.
- **Dark theme:** DROP's shadow colour is pulled toward the chassis ink, which on the dark
  chassis reads as a light magenta/violet halo (the same behaviour DROP-only already has in
  dark — not something this composite introduces); still legible against the dark background,
  not muddying the cyan edge or the gold gradient fill.
- **Three screen schemes (TUBE, FROST, EMBER):** visually IDENTICAL to each other and to the
  default (ORCHID) — screen schemes recolour "the data wells only" (§Domain, ADR-121), not the
  frame header where the logo sits, so the composite is scheme-independent by construction, and
  the screenshots confirm no scheme accidentally reaches the logo canvas.

## Evidence

- **VERIFIED — in-page self-check** (headless Chrome, `--dump-dom`, real wall clock, `?page=main`):
  self-check reads **89/93** on this branch, same **4 pre-existing failures** confirmed present
  on an unmodified `origin/main` copy served the same way (87/91 there — the count differs only
  because 91→93 is the two new rows this brief adds): three "WebGL unavailable here" rows
  (Specimen underlay/orbit/shedding-and-centring — this headless Chrome build refuses a WebGL
  context) and one "the pointer reaches the mark false" clause inside the logo-styles row (an
  `elementFromPoint` quirk in this same headless environment). None of the four touch the
  DROP + INSET + GRADIENT rows, which read GREEN on every run once the tolerance fix (above)
  landed. Both the new "DROP + INSET + GRADIENT: differs..." row and its planted CONTROL passed.
  The environment quirks are pre-existing (reproduced identically against `origin/main`), not
  introduced by this change, and out of scope to fix (`reference/**`, engine, other labs are
  out of scope per the brief; this is a headless-probe limitation, not a lab bug).
- **Not verified:** nobody has listened to or looked at this on a real display; the cost numbers
  are one machine, one run, headless.

## Screenshots (scratch, not committed — session temp dir)

`/private/tmp/claude-501/-Users-machinepriest-Documents-Claude-synthetic-worlds-HYPERSAW/8f39079a-d2c3-45ac-95ed-20c1077b7b03/scratchpad/b364/shots/`:
- `drop-light-zoom.png`, `ig-light-zoom.png`, `dig-light-zoom.png` — DROP, INSET+GRADIENT, the
  composite, light theme, close-up (6× crop of the mark).
- `drop-dark-zoom.png`, `ig-dark-zoom.png`, `dig-dark-zoom.png` — the same three, dark theme.
- `*-1x.png` for each of the above six — a wider 1× crop with page context (the PAGE/SKIN row).
- `dig-tube-zoom.png`, `dig-frost-zoom.png`, `dig-ember-zoom.png` — the composite across three
  screen schemes (TUBE, FROST, EMBER), light theme.
- `*-full.png` — the full captured viewport per shot, for provenance.

## Alternatives rejected

- **A new dedicated paint function for the composite:** rejected — the existing architecture
  (independent `grad`/`inset`/`drop` flags, each already reading `LOGO.tint`) composes for free;
  writing a bespoke function would duplicate `logoInset`/`logoDrop`/`logoGradient` for no
  behavioural gain, and directly contradicts "reduce, never invent."
- **Bit-exact (`===`) equality for the BENEATH checks:** rejected after measurement — the
  canvas shadow compositor is not guaranteed bit-identical across two independently-drawn
  styles sharing the same alpha silhouette (1-unit float noise observed). Reused the file's own
  `differ()` tolerance instead of inventing a new threshold.
- **A different composite ordering (drop before inset) driven by the style entry itself:**
  unnecessary — `logoOutline`'s existing order (inset, then drop) already yields the brief's
  required paint order because `destination-over` places content behind regardless of when in
  the frame it runs, so no reordering was needed or made.

## Verify

- `git commit`, then `./verify fast` on the committed hash (not chained): exit 0 at git
  `e9c5c46` (`.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"e9c5c46","ts":"2026-09-29T19:39:40Z"}`).
  `test_table_check: GREEN (204 tests — 116 agentic, 88 human; 16 awaiting an oracle; 76 check
  files declaring WIRED and verified so, 1 declaring UNWIRED and verified so)`;
  `lab_wheel_scroll_check: GREEN — 0 failure(s); 6 labs, 2 planted faults`. The private-name
  leak check was SKIPPED (`.leakcheck-names` absent in this worktree, expected on this Mac).
  No new `tools/*_check` file was added (the extension lives inside the lab's own in-page
  `checkB334`, already wired via `runChecks()`/`boot()`), so ADR-180 §1's wiring header does
  not apply here.
- `./verify full` was not run: no engine, gate file, or other lab was touched, and the lab's
  own checks run in-page (per B334's own precedent).
- **Merge-tree check against `origin/scalpel-antialias-189`** (PR #852, also edits this lab
  file — the anti-aliasing toggle chips, a different region near the PAGE controls row):
  `git merge-tree origin/main HEAD origin/scalpel-antialias-189` on this commit produced **no
  `CONFLICT` marker** (0 occurrences over 1516 lines of output) — the two branches touch
  disjoint regions of the file (AA189 toggles near the header controls row vs. `LOGO_STYLES`
  and `checkB334` here), so a clean three-way merge is expected when both land.
- This trace is committed on top, and `verify fast` is re-run on that hash.

## Open questions

None from this brief. Whether DROP + INSET + GRADIENT becomes the human's kept default (vs. one
of the other six) is the open design question B334 already carried forward and this trace does
not resolve.
