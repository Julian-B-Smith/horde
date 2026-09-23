# b207-mod-matrix-lab — the mod matrix design lab: depth modulation (one level) and four competing route views

- **Queue item:** B207 (carried in PR #720, branch `lead-records-80`); answers B70's open design and gives
  B134 / B179 / B180 / B199 a visual home.
- **Why:** The human asked for a mod matrix lab "and functionality — … modulating modulator amounts, and I
  would like for modulation to be more visually intuitive". The August `mod-lab.html` answered a different
  question (the sources: Kuro rotor, LFO/ENV A/B) in a pre-ADR-116 palette. This PR is a SUCCESSOR,
  `docs/design/mod-matrix-lab.html`, not an in-place rewrite. Four harnesses slice the old file's first
  script block by position (`tools/labharness/modlab_probe|reach|sweep|sweep_report.mjs`) and
  `docs/design/kuramoto-lfo-golden-spec.md:198` cites it, so an overwrite would break all four without any
  error and erase a cited record. The old file gets a header comment, a visible banner and
  `<meta name="lab-superseded-by">` (lead addendum). `modlab_probe.mjs` still runs against it after the edit.

## What the lab decides (proposals for the human, not rulings)

- **Depth modulation (B70).** A depth route is an ordinary route whose destination is another route's depth.
  The law stays SUM, evaluated in two fixed passes within one tick. The depth route speaks its target's units
  (semitones on a pitch route), and the modulated depth is clamped to that route's own ±range (the OQ-30 rule,
  with the depth as destination). It is wired with + DEPTH on a route row, by right-clicking the route's
  depth slider, or by right-clicking a grid cell. It is shown with gui2's own slider mod halo on the depth
  slider, so it adds no new visual vocabulary.
- **Recursion: NO, one level.** A depth route may target only a value route. Depth-of-depth would let routes
  read routes, which admits cycles, and then needs an evaluation order plus OQ-23's unit delay. No case
  anyone has named needs it. The refusal is live in the UI, and a single `refuseWhy()` string serves every
  surface that refuses.
- **Port trap found:** `ModCore::removeRoute` compacts the table (src/mod_core.h), so an index-keyed depth
  route (B70's `kDestRouteDepth | index`) would silently re-aim at its neighbour after a delete. The lab's
  `removeRoute` cascades a route's depth routes and re-points the survivors. The port needs the same fix.
- **Four treatments of one state, side by side:** A arcs on the knob (chrome SVG), B wires (tube), C grid (the
  FX-matrix well idiom), D trace (tube, autoscaled, with a 250 ms per-tick staircase view).
- **Design-system finding:** source-*coloured* arcs need a categorical palette, and the token set has none
  because every hue has one job. Source identity therefore rides tag + ring order. Adding hue would take an
  ADR-116 amendment; the lab does not decide it.
- Sources carry B179's new names on frozen slot indices; retired slots 10–13 and free slots 22/23 are drawn
  dashed. Pitch is per-oscillator ±48 st (B180; follows the lead's per-osc lean). B199 is shown as five rows,
  and only row 5 (the 172.27 Hz tick, 86.1 Hz fold) is stated as arithmetic.

## Evidence consulted

ROADMAP B70, B134, B179, B180, B199, and B207 (from `origin/lead-records-80`); `src/gui/gui2.html` (tokens :26-212,
knob/halo CSS :620-650, MOD_SRC_NAMES :3174, polarity :3195-3210, applyModVis :3461); `src/mod_core.h`;
`src/hypersaw_clap.cpp:752` (LFO rate 0.02–40 Hz, shapes); `src/swarm_core.h:136` (kGravGridSeconds);
`docs/design/station-page-lab.html` (the model: tokens, TOK/TOKA, theme deep link); `docs/design-system/`;
`tools/labharness/lab_load_check.mjs`; `traces/2026-09-18-b154-station-page.md` (screenshot method).

## Verification

- `./verify fast` exit 0, git 3fd3a6b (`.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"3fd3a6b",…}`).
  Hash is the branch base; the change is uncommitted at verify time.
- `node tools/labharness/lab_load_check.mjs` → `OK mod-lab.html`, `OK mod-matrix-lab.html`,
  `GREEN — 45 labs loaded, 0 broken, 1 skipped`. The gate caught two real defects on the way: a literal
  script tag inside the header comment, and a `while (x.firstChild)` loop that never ends under a proxy DOM.
- **Headless core probe** (scratch, not committed; slices the DOM-free first script block): 9/9 PASS.
  - Vibrato bloom: peak |pitch| is 0.0405 st over the first 100 ms and 0.7000 st after 2 s.
  - Control (must read zero): with authored depth 0 and no depth route, |pitch| max is **0 exactly**.
  - Depth-of-depth is refused and the table size is unchanged.
  - A depth pushed past +1 clamps to 1, with the clamp flag set.
  - Removing #0 takes its depth route with it; the surviving depth route is re-pointed to its target's new
    index 0.
  - An S&H route replays bit-identically across two fresh cores.
  - B134 map: macro 0.5 reads bi 0, inv −0.5.
  - A square at 12 Hz folds from harmonic 9 (−19.1 dB); a sine at 40 Hz does not fold.
- **Screenshots:** Chrome headless, 1500×2560, `?at=2.6` (a deterministic 2.6 s preroll, then paused), both
  themes (`&theme=dark`). Served by the `labs` launch config's command pointed at this worktree on port 8187,
  because that config serves the main checkout, where this file does not exist yet. Headless Chrome hangs at
  exit on this machine after writing the PNG. The known-good station lab hangs the same way (control), so the
  hang comes from the environment, not the page.

## Alternatives rejected

- In-place rewrite of `mod-lab.html`: rejected because it would break four harnesses and a spec citation.
- Multiplicative depth modulation (`depth × (1 + e·s)`): rejected because it cannot bloom from an authored
  depth of 0, which is the canonical case (vibrato that blooms). Additive depth modulation covers it and
  keeps one combination law.
- A per-source hue palette: rejected because inventing a palette is forbidden and needs an ADR-116 ruling.
- Unbounded recursion with a unit delay: rejected, for the reasons given above.

## Open questions

- Default source for a sent route: gui2 says "arrives on ENV 1", which today is slot 0 (the gain envelope).
  Under B179 that intent is slot 20, which the lab uses. That is a shell default change for the human or lead.
- Cutoff is modulated linearly in Hz on a log knob (ADR-136 `base + delta·span`). The lab reproduces this
  and flags it but does not endorse it; it may deserve its own row.
- The four treatments are compared by the lab's own reading, not by any player measurement. The human
  chooses. Nothing here has been heard: the audio monitor is untested by ear, and a headless browser has no
  output device.
- Whether per-oscillator pitch (B180) and the migration of param 161 land as the lab draws them is still
  B180's open ruling.
