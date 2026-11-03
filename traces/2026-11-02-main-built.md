# Main through the chain: GUI-2, GUI-4, GUI-5 r2 (B581)

- **Queue item:** B581. GUI session. The second half of "Foot row, top bar, Main".
- **Master, version `1793658873-4f0e`:** `Deck`, `AssignXY`, `Viz.response` (a SAMPLE curve, marked), `SecondKnob`, `LooseKnob`, and `MainStage`: the bench's logo and Specimen, from `main-layout-lab-parts.js` verbatim (ADR-333), with the lab's shims. Two changes to that code, both GUI-2's reading: the logo keeps its locked style, with no click to cycle and no stored style.
- **Settled:** ADR-139 and the Specimen README were thought to disagree on R. The bench's code is `kT = max(0.12, 0.45 - 0.3 * R)`, which is what the README says. ADR-139's 0.1 was the requested material's rest value. The README's "off by default" follows GUI-2 r3.
- **Shell:** `mainPage` and `fillMain`. `layout.json` holds the deck, because `gen_gui3_controls` forbids the shell naming a key, and the generator now checks each tab's keys against its source page, with a plant. The gate that the shell calls `H.Knob` once also held, so Main's extra knobs go through `SecondKnob` and `LooseKnob`.
- **Evidence consulted:** GUI-2, GUI-4, GUI-5, ADR-315, ADR-333, ADR-139, ADR-140, ADR-147, ADR-262 r1, the bench's `buildMain`.
- **Found while building:** my new self-check's write wasn't wrapped in a step, so its undo reached back and left morph on, which failed four later checks. Wrapped, they pass.
- **Exceptions and readings, flagged to the human:** Main has no Grid | Table (PV1), and the ENV 1 and Voice tabs use the pages' tiny-knob sections as they are, so they look sparse in a large panel.
- **MOCK:** the XY's value, Filter, Bend, Scale and the macros. The ring, cycle and curves are SAMPLE drawings.
- **Not checked:** the Specimen's cost in a plugin webview, which ADR-140 found untenable once and GUI-2 still owes; the logo's cost; a host; the light scheme; other window scales. Nobody has seen the Specimen struck in the shell, because it has no voices.
- **Checks:** the shell's self-checks, 52 of 52, including a new line walking all ten tabs; `gui3_seam_check`, 44 tests; `gen_gui3_controls`.
