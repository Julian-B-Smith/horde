# 2026-11-06 — horde 2's modulation spec, drafted for the human's ruling (B630, ADR-370)

**What changed.**
- **`docs/proposals/modulation-spec-draft.md`**, new: the modulation system as a proposal in SPEC-ENGINE's style. 59 proposed clauses (MD-1 to MD-59) over scope, sources, the matrix, the LFOs, state and the morph, CPU and the FX device; 18 questions for the human, each with its options and, where the record gives evidence, a recommendation; 10 technical gaps for the lead; 7 places where the record contradicts itself.
- **MD-59** states the human's ruling relayed by the lead the same day (the lab review, GN-40, to be recorded as ADR-371): a key set to Exempt still takes modulation; Exempt holds the value heard at the pick in all four corners, and an edit moves all four.
- Nothing else. SPEC-ENGINE, the protected specs and the clean room are untouched (ADR-370: nothing reaches the clean room until the human rules).

**Why.** ADR-370 (the human, 2026-11-06: "yes to all, build the morph lab and start everything"): horde 2's engine accepts per-voice offsets and publishes sources, and nothing specifies the LFOs, the matrix or the routes. B630's acceptance asks for every clause cited, every open choice a question, and no clause without a ruling.

**Evidence consulted.**
- SPEC-ENGINE: §1, 4.2, 4.3, 5.1, 5.2, 5.7, VO-6, VO-8, EN-5, PA-1 to PA-8, PF-1 to PF-10, IF-1 to IF-4, IF-11, IF-12, RC-4, RC-7, RC-9, RC-11, 11.1 (Q2, Q12, X21), B.1.
- DECISIONS: ADR-136, 137, 138, 152, 162, 168, 176, 192, 224, 226, 254, 255, 261, 266, 267, 269, 290, 322, 323, 330, 338, 340, 341, 349, 354, 355; GUI-3, 7, 9, 17, 37, 46; ADR-370 on `lead-records-410`.
- ROADMAP: B16, B26, B57, B70, B72, B82, B171, B180, B198, B199, B218, B226, B229, B237, B253, B264, B370, B396, B528, B540 to B542, B556, B586, B590, B591, B601, B616, B630.
- `specs/SPEC-INTENT-BUS.md` 3.4, 4.5, 6; `docs/design-system/HORDE-Map-Mode-Spec.md` (Model, Host constraints, Open decisions); `docs/design/gui3/device.js`, `ENGINE-NEEDS.md` (GN-5, 10, 14, 20, 23, 27, 30, 39, 49), `port-tab.json`; `docs/design/corner-mapping-lab.html` (its five rulings); `docs/audits/2026-10-21-fx-device-audit.md` (b); `docs/H2-PLAN.md` Modulation; `docs/proposals/module-1.0-bar.md`.
- The legacy matrix, as behaviour only: `src/mod_core.h`, `src/hypersaw_clap.cpp` (the LFO rows, the polarity table, `modRouteAllowed`, the mod tick).
- A search for ADR-330's "drafted rows" (params.json, layout.json, `tools/gen_gui3_controls.py`, `engine2/structure/`, `docs/port/`) found none on `origin/main`; recorded as contradiction 1.

**Verify.** `./verify fast` on the commit this trace ships in; the result is in the PR.
