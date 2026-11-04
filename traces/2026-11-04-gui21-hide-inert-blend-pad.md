# 2026-11-04 — GUI-21: a control that can't act isn't drawn; the blend map replaces the pad

**What changed.** Master `1793813622-b097` and its vendored copy: `.idle` hides a control, `BlendMaps` takes `padHost`. `layout.json` gains `live_when`, one condition per parameter, with PA-8's oscillator rows added; `gen_gui3_controls` reads it and refuses a condition on a page or one reading its own key. `gui3.html`: the map in the pad, hidden sections, the console obeying the table, self-checks. `sub-noise-lab.html`: the Weather's Density and Rate, and a self-check. Records: GUI-21, GB7, GB9, GB10, ENGINE-NEEDS, ledger.

**Also in this PR, from the lead's brief (B617):** every need in `ENGINE-NEEDS.md` gets a `GN-n`, four needs that were only prose get rows (GN-14, GN-25 to GN-27), and the ledger gains `engine_impact` with an entry for each of GUI-1 to GUI-21.

**Also in this PR, GUI-22:** the top bar's switches on the colours' row, the Morph page keeping its selection, and GB12's plan for Random.

**Also in this PR, GUI-23:** the record of the human's rulings on Sluice's gap, the GUI labs and FX screen marks (those labs have their own PRs), and `random-home-lab.html`, which tries both homes for Random on the real shell.

**Why.** The human's three notes of 2026-11-04, quoted in GUI-21. The brief relays the human's "yes, set it up" on how GUI decisions reach the engine.

**Evidence consulted.** `specs/SPEC-ENGINE.md` PA-8 and SN-8. ADR-319 r6, ADR-110 ("an item that cannot apply here is absent"). `BlendMaps`' README and code. A measurement of each noise picture at low and high Density and Rate.

**Verify.** `./verify fast`, on the commit this trace ships in. The shell's self-checks and the lab's, in a browser.
