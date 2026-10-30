# spec-round-48-morph-appendix — Appendix D (the morph) applied to SPEC-ENGINE, and the structure declaration

- **Queue item:** B585 step 5.
  - **Ratification:** the lead reports that the human ratified Appendix D revision 4 (`063aeca`, horde `b585-appendix`) by poll ("Ratify") and ruled X44 ("Yes, per X39") and X45 ("256 bytes"). This becomes spec round 48.
  - **Authority:** the lead authorized this edit of the protected `specs/SPEC-ENGINE.md` for this round only.
  - **Not yet on record:** the ratification was not on any records branch when this was written (`origin/lead-records-372` ends at ADR-327). The lead records it, and delivers the round to the clean-room repos.
- **Why:** to apply the ratified text exactly. The spec is the clean room's only input (BD-10).
- **What changed:**
  - `specs/SPEC-ENGINE.md`: 41 scripted edits. Each asserted its before-text verbatim exactly once, and its after-text present afterwards.
  - `engine2/structure/engine-structure.json`:
    - §2.28's 2 items, 6 edges, the label and multiplicity changes, and the counts;
    - `spec.blob` set to `dfec522a586cd7be835776f75fc7c5566c1abcc5`.
  - This trace.
  - Not edited: DECISIONS, ROADMAP and `docs/port/cleanroom-rulings.md`.
- **Self-check:**
  - All 38 before → after replacements matched once and are present after the full sequence.
  - The inserts were asserted: IF-12, §10.1 items 34 and 35, Appendix D.
  - The spec's PROPOSED count is 12 before and 12 after. None was added; the 12 are earlier rounds' own marks.
  - No `[X44]` or `[X45]` tag remains, and the §11.1 heading stays (24).
  - `engine2_structure_check`: ok (67 blocks, 12 slots, 117 edges, 47 §4.2 stages, 9 must-fail controls).
- **Judgment calls:**
  1. **`morph_resolver` is `external`, not §2.28's `engine`.** The check requires `engine` scope exactly when the engine item is an ancestor, and the resolver sits beside the engine, under `instrument` (MO-1, ADR-325). So the counts read engine 67 / external 12, not the text's 68 / 11.
  2. **BD-8's amendment is flattened into BD-8's single sentence**, in stage 5's inline style.
  3. **RC-7.4 is written as `  - **RC-7.4** **The standard tempo divisions.**`**, with its table indented under the bullet.
  4. **§10.1 items 34 and 35** are numbered items with indented 34.x and 35.x sub-bullets.
  5. **Ruling references:**
     - `[X44]` and `[X45]` become "(round 48)";
     - the D.2 note gains "the Division menu's triplet and dotted toggles are host parameters too (round 48)";
     - "the human's ruling on X41/X43" becomes "ADR-327";
     - X42's references (IF-4, RC-11.1, MO-14) cite ADR-327, which re-ruled it.
  6. **PA-7:** the inner bold on "merged" and "live" is dropped, because it sat inside a bold span and would break the markup.
  7. **Placement:**
     - the glossary rows go after the class row, and the layout line after `api/`;
     - the start-table row goes last, IF-12 after IF-11, and Appendix D at the end of the file.
  8. **Structure-file wording:**
     - `modulation`'s label is "the modulation system and the morph's editor and storage: …";
     - the chain multiplicities sit on the `sub` and `noise` items;
     - edge labels are the text's parentheses, with the `modulation->morph_resolver` edge citing IF-12, because the text gave no cite.
- **Evidence consulted:**
  - `063aeca:docs/proposals/morph-appendix-spec-text.md`;
  - `tools/engine2_structure_check.py` (its scope and cite rules);
  - `engine2/structure/README.md`;
  - `origin/lead-records-372:DECISIONS.md` (ADR-326, ADR-327).
- **Verify:** `./verify fast` was run after this trace was written; its result is in the commit message.
- **Open questions:** none from the text. Judgment call 1 changes the structure counts that the text gave; the lead may prefer `morph_resolver` under `engine` instead, which would change the text's "beside the engine".
