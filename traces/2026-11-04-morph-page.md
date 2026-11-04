# 2026-11-04 — The Morph page's own larger pad and the morph field (GB7, GUI-19, GUI-20)

**What changed.** `docs/design/gui3.html`: `morphBench`, the page's own 320 px pad with `QuantumPad`, the pad's bars, `MorphInspector`, `CrossSection`, `MorphGrid` and `FindPane`; self-check 4o. `gui3/device.js`: one `offsets` function for the owner stand-in, region locks in the undo state, `morphSettings` and `setMorphLaw`. Master `1793773329-83d6`: `state(S).history`, `.solo`, `MorphGrid` `opts.rev`, and painting only the rows in view. Published and re-vendored. Ledger, port tab, GUI-20, GB7. `BlendMaps` on the same bench, with `pins` and `blend` in the device's state.

**Why.** The human, 2026-11-04: "Morph should have its own larger version though (plus all the proposed morph page functionality)". The master's morph layer has its own stand-ins for undo and ownership, and using them would have let the field and the knobs disagree (ADR-110), so the shell points them at the device. Then the human: "We also need to include the blend map (BlendMaps section of the design doc)".

**The handoff.** The human also asked that the lead knows what the design decisions need under the hood. `docs/design/gui3/ENGINE-NEEDS.md` and GB8: extracted by a read-only agent from GUI-1 to GUI-20, then spot-checked against main's spec and roadmap (stage solo, waypoints, region locks, IF-10's summed output, RS-4.8).

**Evidence consulted.** ADR-266 r9, ADR-262, ADR-319, ADR-321, ADR-322, B269, B272; the master's READMEs; `docs/design/gui3/port-tab.json`.

**Measured.** One frame's script in the browser pane: Morph 12 ms showing where corners differ and 17 ms showing all 544 rows (41 ms before it painted only what's in view); Main 6 ms, with the field mounted. Not a plugin webview, and the pane was throttling, so these are rough.

**A red oracle, fixed by reverting.** The first verify failed `gen_gui3_controls` (the shell named morph keys) and `gui3_seam_check`. The keys moved into `device.js`. The device's Quantum change was reverted, because the seam check holds that a Morphable row blends by the weights. Hiding the dock on Morph was reverted too, because the `dock` gate allows Main alone; no gate was edited, and that one is put to the human.

**Not checked.** A plugin host, and the light theme by eye. A real mouse on a boundary: the one real drag I tried landed off the boundary, so that gesture is proved by pointer events on the real layer.

**Verify.** See the PR body.
