# b211-morph-editor-lab — the morph field, corner by corner, as a design lab

- **Queue item:** B211 (row carried in PR #720; human 2026-09-22: "Morph expanded parameter editor design lab").
- **Why:** a corner can only be captured whole today. Nothing lets a player read or edit what a corner holds row by row. The lab prototypes that editor against the REAL field, and makes the three membership behaviours and exempt-vs-absent visible. New file only: `docs/design/morph-editor-lab.html`. No change to `src/`, `specs/`, `reference/` or `docs/design/index.html`. The `<meta name="lab-review" content="B211 · 2026-09-22">` line was added at the lead's mid-task request (PR #721's index marker).
- **What the page does:**
  - **Grid.** All 273 field members × corners A..D × a HEARING column. Rows sit under sections (osc 1 / osc 2 / sub / FX rack / routing / play / scale / swarm) with the presentation table's group captions. The default view shows only rows where the corners differ.
  - **Search.** It also finds non-members. The 124 non-members sit in their own NOT IN THE FIELD section, each with the shell's reason.
  - **Staged per-cell editing.** Each edit states its audible consequence at the current pad position before COMMIT: the owning corner, the blend/ramp weight, or "silent until this corner wins it".
  - **Pad.** It paints the selected row's resolution across every position. Continuous-in-blend shows as a gradient, snaps as flat regions with hard edges, a gate as a level gradient with the switch contour.
  - **Kinds and exemptions.** Each kind has an ink glyph and a word (SLIDES / SNAPS / RAMPS). Exempt rows are a ⊘ band with a value and a way back. Absent rows have no cells, a reason and no action.
  - **Deep links** (`?theme=dark`, `?sel=`, `?corner=`, `?stage=`, `?view=`, `?q=`, `?mode=blend`, `?demo=0`) make every screenshot a load, not a click.
- **Evidence consulted:**
  - `src/hypersaw_clap.cpp`: morphInit ~2948-3150, morphToggleExempt, morphOwnersJson, morphRouteEdit ~3363, morphApplyTarget / morphOnWeight / morphApplyGateEnable / morphStep ~3961-4115, paramClassOf ~1591.
  - `src/morph_core.h` (ported line for line); `src/depends_graph.h` (ADR-108 holds); `src/param_presentation.tsv` (labels, groups).
  - `src/gui/gui2.html` tokens (:root 26-96, orchid 142-144, light overrides 167-185, dark 186-212) and its `.owned` / `.held` / `.ghost` / `.exempt` row idioms (~775-871).
  - `docs/design/station-page-lab.html` (the model); DECISIONS ADR-109.
  - ROADMAP B86 (FX 200-263 outside the field), B211 (PR #720's copy).
  - Factory preset `docs/presets/factory/morph/MO - Quantum Morph.json`.
- **Field provenance (scratch, not committed):**
  - A probe linked against `libHYPERSAW-impl.a`, built from this branch's base 3fd3a6b, dumped the field. It used `hypersaw_debug_cornervals` (order), CLAP `get_info` / `value_to_text` (names, ranges, stepped, enum text) and `hypersaw_debug_paramclass` (class + reason).
  - Result: 273 members = 181 continuous + 89 structural + 3 gates (150, 1150, 4015). 124 non-members = 56 Device + 68 class-eligible but never appended.
  - A second probe applied the factory preset at four pad positions and recorded the engine's own `morphOwnersJson`. That is the page's ENGINE_OWNERS fixture.
- **Verified vs entailed:**
  - VERIFIED: ownership parity 1092/1092, run headless on the final file. Must-fail controls: one flipped engine answer reads 1091/1092 and names the row (`1004@0.5,0.5`); the wrong seed (1025) reads 634/1092. So the audit can see a mismatch.
  - VERIFIED: the lab loads under the stub DOM (`lab_load_check`: OK).
  - ENTAILED: blend values, gate ramps and the held rule are ported code read against the shell. Only ownership and holds are measured against the engine; blended values and ramp levels are not.
- **Alternatives rejected:**
  - Placeholders or a hand-typed field. The brief forbids them, and a hand copy is the drift L0-style oracles exist to stop.
  - Colour-coding the three kinds. Every named colour is already a status, so kind is a glyph plus a word (station-page-lab's feedback-glyph ruling).
  - Editing that applies live immediately. That would disturb the sound. Staging with a stated consequence answers the brief without a shell change.
  - Committing the dump probes as tools. Out of scope for a design lab.
- **Verify:** `./verify fast`, exit 0, git 3fd3a6b (`.harness/last-verify.json`, 2026-09-22T17:38:48Z).
- **Screenshots:** headless Chrome at 2× (7 states). Paths are in the PR report; they are scratch files, not committed (station-page-lab precedent).
- **Open questions:**
  1. **68 class-eligible parameters are not in the field.** 60 are covered by B86 (ids 200-263). The other 8 have no ruling this session found: Mono Fold 15, Bass Mono 40, Bass XOver 41, Oversample 88, Master Pitch 101 / Fine 102 / Octave 103, Voice Cull 160. The lab labels them "no ruling found", not "by design". Is that right?
  2. **Returning an exempt row can jump.** ADR-109 says un-exempting never jumps. That holds only if the live value was not moved while exempt: edits to an exempt row are live-only (morphRouteEdit), and the corners keep the value written at exempt time. The lab shows this (osc 2 Volume) and offers "return, keep live in all four". Is that a real shell behaviour the human wants surfaced, or a lab over-reading? Not measured against the engine.
  3. **Staging is a GUI-side proposal.** Shipping it needs only a GUI buffer, no engine change. But the shell's armed-edit path writes the corner at once and is heard at the next grid tick whenever that corner contributes.
  4. **The field table is a snapshot.** It is stale the day morphInit appends (STATION's block, B162). If the editor is ported, the table should come from the shell, not be copied.
  5. **`docs/design/quantum-morph-lab.html` is not on origin/main.** It exists only as an untracked file in the main checkout. This lab ports `src/morph_core.h` instead.
