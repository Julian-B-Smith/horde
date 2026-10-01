# b386-legacy-roundup — everything horde Legacy has that horde 2's plan does not yet carry, with an APPROVE / DENY / LATER column

- **Queue item:** ROADMAP B386 (records PR #888, branch `lead-records-158`), dispatched by the horde lead 2026-10-01.
- **Why:** The human: "There are also a few additional parameters I want to make sure survive from horde legacy. Let's do a roundup of everything that hasn't been ported over yet so I can deny and approve." The page lists every legacy parameter family, feature and behaviour, says where each stands in horde 2, and recommends. Only the human decides.
- **What changed:** one new file, `docs/design/legacy-roundup.html` (self-contained, data inline, the engine-audit page's conventions), and this trace. Nothing under `src/`, `reference/`, `specs/` or the engine was touched. No check was added or edited. The page has no wheel listener, so it is outside `lab_wheel_scroll_check`'s LABS inventory but inside its static sweep. `docs/design/index.html` was not regenerated.

## How the data was read (the builder is scratch, not in the tree; the B376 precedent)

At `c64cfdb` (origin/main):
- `src/hypersaw_clap.cpp`: `kParams` (266 rows, with their declaration lines), `kGlobalIds` (184 global, so 82 per-osc ids with a +1000 twin), the SUB OSC block `kSubOscParams` (20 ids, 4000–4019; 4003's row spans two lines and is picked up explicitly), the routing block, the mod matrix, LFO/ENV 3–4, state, presets, history, MPE note expressions, the revision gate.
- `src/gui/gui2.html`: the interface features, by their section headers.
- The B376 audit's DATA (`docs/design/engine-audit.html`, snapshot `bae777f`): its 82 rows that carry a legacy id become 82 items linked by `engine-audit.html?open=all&row=<id>`. Their evidence is not copied.
- "Carried" = the composed engine has the control: the audit's composed keys plus the CURRENT `HORDE_D` keys in `docs/design/scalpel-horde-engine.js`. That catches `beatMult`, which joined with B382 M3 after the audit's snapshot.
- `docs/scalpel/ACCOUNTING.md` §1, §4, DECISIONS ADR-186, and the ROADMAP at `origin/lead-records-158` (b556ded, 391 rows). That row list is embedded so the page can check every cited row.
- Preset use: the human's 39 legacy presets and corners (10 + 29), from the main checkout's git-ignored `local/legacy-presets/legacy-presets.json`, read-only. **Counts only are committed.** "Off default" = |value − kParams default| > 1e-6. This threshold matches the audit's counts: at 1e-9, float noise added 1–2 presets on alpha and mu. Feature counts (FX types per slot, routes, morph corners, routing cells, osc 2 enabled) were counted the same way. A scan of the page for all 39 preset and corner names found 0. The page contains no machine path.

The builder refuses to emit unless:
- every kParams id and SUB id is in exactly one item;
- every cited ROADMAP row exists;
- every audit link resolves;
- every planned item cites a row;
- every `file:line` cite is inside its file.

Row-declaration cites are computed from the parsed lines, never typed.

## Summary

- **156 items: 112 to decide, 44 already in horde 2** (collapsed per area).
- **Status:** planned 57, audit recommendation only 28, not planned 11, archived or retired by a ruling 16, carried 44.
- **Recommendations** (the 112): PORT 45, PORT-MODIFIED 27, FOLD INTO 17, DROP 22, LATER 1.
- **Coverage:** 266/266 kParams ids and 20/20 SUB ids. Osc 2's twins are one item.
- **Self-check** (headless Chrome, served page): GREEN, 5/5 pass and 5/5 must-fail controls fire.

**Most likely to matter: used in the legacy presets, and no row plans them** (audit or not-planned, ≥ 3/39):
- the bend travel law 106–115 (14/39);
- density comp `normExp` (12);
- ring reach (12, only live under ring topology);
- phase lag α (10);
- topology (9);
- the routing matrix (9 store a nonzero cell);
- phase scatter (8);
- poles (7);
- cluster link (6);
- pan motion (6);
- A/B balance (4);
- pan scatter (3).

The topology family is the part of the swarm core the composed engine (the B332 parity target) never composed.

**Heavily used but planned or ruled:**
- R → tone, 18/39 (B304);
- Comb, 12/39 rack slots (B288);
- Drive, 10 (B393 / MAW);
- osc 2, 24/39 (B327);
- quantum morph, 7 (B396);
- the saw-shape family, up to 8/39 (RETIRED, ACCOUNTING H2).

## Alternatives rejected

- **Copying the audit's evidence into each row.** The brief says link. Audit items carry only the status, the audit's recommendation kind and its count.
- **A decision for carried items.** Their keep / lock / merge belongs to the engine audit, so they show "context only". Two decision columns for one control would disagree.
- **Pre-filling from the recommendations.** Forbidden. `?demo=recs` shows the recommendations as decisions, labelled DEMO and never saved, for screenshots.
- **A live fetch of ROADMAP.md for check 4.** main does not yet carry B386–B396 (they are on #888), so a live check would be red until #888 merges. The snapshot list is embedded and named instead.

## Verify

Recorded in the PR: `./verify fast` on the committed hash, read from `.harness/last-verify.json`. This trace is written before that run.

## Open questions

1. **What APPROVE means.** The page defines it as "this survives into horde 2, the way the recommendation says unless your note says otherwise". It does not mean "I approve the recommendation". With that reading, APPROVE on a DROP row is a contradiction the note must resolve. The lead should confirm this reading with the human.
2. **Status is a reading of the plan, not of the code.** "Planned" means a row names the work. Some citations are thin:
   - B388 only names ids 137–149 as legacy context;
   - B370 covers ENV 2..n only by its spread-knob remark.
3. **Not planned anywhere, and worth a ruling:**
   - the inertial bend law, which no horde 2 row plans although B57 reuses its spring;
   - the routing crosspoint matrix (signal flow is open between B263, B258 and B274);
   - a plain stereo Delay (0/39 use, but a standard expectation);
   - copy/paste patch;
   - the off-corner morph rule.
4. **The builder lives in scratch**, like B376's. If the roundup must track later changes, the lead could commit it with a freshness check.
