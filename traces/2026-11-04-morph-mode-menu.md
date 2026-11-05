# 2026-11-04 — The Morph page's per-key mode, in-page naming, save all four, delete (GUI-28, GB7, B557)

**What changed.** Master `1793843689-1e6e` adds `MorphMode` and `NameField`, `MorphGrid` can tag a row, and a locked value's readout can carry its own word. The shell's device holds each key's mode and count, plays Exempt and Quantum on its stand-in, and sends both to the resolver. The right-click menu and the Morph page's selected row set them, and the marks show on every face. `window.prompt` is gone. Save all four and a delete button are on the corners. The seam check gains `morphmodes`, with eight plants.

**Why.** The human: "Finish the Morph page." ADR-291 r3, ADR-320 r4, ADR-324 H1 and ADR-325 X30 were ruled, and SPEC-ENGINE MO-11 to MO-13 answers what B557 had left open.

**What it doesn't finish.** The import list, FX fine-tuning, `KindLegend`, the rail and distribution each need something else, as GUI-28 says. The page isn't done, and this says so.

**Evidence consulted.** `docs/design/page-chrome-lab.html` for the marks, D.4 of the spec, and `docs/design/random-lab.html` for the set's naming. Real clicks in the browser: a right-click on Detune, Exempt from its menu, and a typed name saved with Return. They found nothing wrong; a check of the rules found that an edit to an Exempt key could change a locked corner, which is fixed and held.

**Verify.** `./verify fast`, on the commit this trace ships in. The shell opened in a real browser.
