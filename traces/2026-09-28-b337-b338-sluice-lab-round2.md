# b337-b338-sluice-lab-round2 — snapshot library, fixed + variable macros, corner chips and a GBC skin, as a labelled horde proposal

- **Queue items:** B337 and B338. I read both rows verbatim from `origin/lead-records-134:ROADMAP.md` (records PR #832), with B322, B326 and B329 beside them. Dispatched by the horde lead on 2026-09-28.
- **Why:** the human asked for four changes to the Sluice lab (B337: snapshots, macros, naming, corner chips; B338: a Game Boy Color version). They change Sluice's own model, so the lead filed a brief to Sluice (seq 10). This round builds the human's version, labelled on the page as "horde proposal — pending Sluice, brief seq 10".
- **Files:** `docs/design/sluice-horde-lab.html` (its `lab-review` meta is now `B319 + B329 + B337 + B338 · 2026-09-28`) and this trace. Nothing else was touched and nothing was deleted. `docs/design/index.html` was not committed.

## What changed, item by item

1. **The snapshot library (section J).** Each patch is a library entry with three parts:
   - `snaps`: named snapshots, each with a stable id. Names are unique within the patch, and there are always at least 4.
   - `bind`: the snapshot each corner A–D is bound to.
   - `work`: the four corners' working copies. `LAB.corners` is the selected entry's `work`.

   A corner is a **DRAFT** when its working copy differs from its bound snapshot. This is computed from content, never stored as a flag, so the marker cannot disagree with what plays.
   - "randomize params" writes into the selected corner's working copy, which makes it a draft. The draft is not in the library.
   - "save snapshot" files the draft under a fresh unique name and binds the corner to it. The snapshot it was bound to stays in the library.
   - "discard" restores the bound snapshot.
   - "capture XY" (the B329 save-the-blend feature, kept) now goes through the same draft path.
   - Each corner has a picker. It is refused while that corner holds a draft, and for a snapshot of another structure (the one-patch rule).
   - Delete is enabled iff 5 or more snapshots exist, with a two-step confirm (✕, then delete or keep). If the deleted snapshot is bound, its corner is rebound to a snapshot that no corner uses and that has the same structure.
   - Drafts and bindings survive switching to another patch and back. Undo covers the whole library.

   **The duplicate-name bug:** `variants()` cloned the patch's name into all four snapshots. `nameSnaps` now gives the patch's name and a number, with numbers only increasing. Imports are renumbered where names collide.

2. **Macros (section G).**
   - **FIXED:** the manifest's four knob-shaped globals (Dry/Wet, Width, Time, Tune; the labels come from §6.1 at runtime). They are always present, stay Sluice-global, and are drawn as their own kind: a square cap, the physics ink, F1–F4. The tooltip gives each one's reach, read from the manifest's §8 roles.
   - **VARIABLE:** four slots, the human's 4 capped by the manifest's slot count, using §7's law and bound by order. They are assigned in the interface: `+ assign` or `edit` opens an inline editor for that slot on the Play face, and the Rack keeps its full editor.
   - **One writer, `forPatch`:** it puts a definition into every working copy and every stored snapshot of the patch. So the definition is saved with the preset and never creates a draft. Moving a variable macro changes params, so it drafts the selected corner.
   - §7's "a macro is a view" is kept: the shown value is derived from params.

3. **The A/B/C/D chips.**
   - One builder (`cornerChip`) and one rule (`chipFilled`): a chip is outlined in its corner's colour, and FILLED only for the selected corner. A library tag fills only when its snapshot is bound to that corner *and* that corner is selected.
   - The corner rows show the bound snapshot's name in the picker.
   - The XY draws the corners the same way, shades the selected quadrant, and dashes a draft's ring.
   - Moving the XY into a quadrant selects that corner. This is the human's "go to a corner's quadrant". The fills are repainted in place during a drag, because a rebuild would replace the canvas under the pointer.

4. **The GBC skin (section N2).** A STANDARD / GBC SKIN switch on the node header (`?skin=gbc`).
   - **Renderer:** B322's GBC renderer, copied from `fx-screens-workshop.html` with line citations:
     - the index buffer and per-tile palette map;
     - the four palettes from the ORCHID (light) and TUBE (dark) schemes, rounded to RGB555;
     - the 3×5 font and the tile audit;
     - the dot grid and the 50% ghosting.
   - **Title slot:** B326's GBC Sluice proposal, copied as pixel data. A dropped `assets/fx-logos/sluice.png` / `sluice-icon.png` of the right size wins.
   - **Screen contents,** all read from live state by `gbcVM()`:
     - the patch and its rendered output level;
     - the XY, with the quadrant, cursor and corner markers filled only when selected;
     - the four bindings, with DRAFT or the weight;
     - the fixed macros as bars from the centre;
     - the variable macros and the soft keys NEW / RAND / SAVE / DISCARD. SAVE and DISCARD are lit only on a draft.
   - **Controls:** under the screen are four real knobs (the variable macros) and four keys. The glass takes the pointer on the XY field and on the binding rows. Beside it are the fixed macros, corners, randomize, library and globals, built by the same builders as the standard face.

5. **Kept.**
   - **C17** reads 0.
   - **B320's canvas sizing.** C15 now holds the GBC pixel canvas to its own rule: 160×144 backing, CSS ×3.
   - **Every earlier check.** C1's expected macro slots are now the proposal's 4 variable slots, and its detail says so (see Open questions). C2 now also holds the GBC skin to "Play ⊆ Rack".

## Evidence

- **VERIFIED, self-checks 26/26, each with its must-fail control firing.** Read over CDP from headless Chrome, served on 8337, 20 s in real time so that C18 completes. The run was with the link present, in both skins and both themes. The eight new checks:

| Check | What it proves | Must-fail controls |
|---|---|---|
| C19 | Six randomised patches get 4 distinct names each; the library is unique; 3 saves give 7 unique names; an import of 4 alike-named snapshots is renumbered | B329's variants path gives "quartz engine" ×4, caught |
| C20 | The list's own delete buttons: 4 → all disabled, 5 → all enabled; delete at 4 is refused; a bound delete rebinds and plays | a ≥4 predicate, and a planted enabled button |
| C21 | A draft is out of the library and equals no snapshot; discard restores; save adds exactly one snapshot, binds it, keeps the old one, and leaves A/C/D untouched | an eager randomiser that files every draw, and a no-op discard |
| C22 | On the live page, selecting B C D A through the chips' own action, plus a visit to another patch and back, leaves bindings and working copies unchanged, and the pickers show the bindings | a re-binding select |
| C23 | Exactly the four roles; 4 fixed macros on the standard, GBC and Rack faces; 4 over each of 7 patches; 4 variable slots, none fixed | a schema missing one knob global gives 3 |
| C24 | An edited definition is in all 4 corners and all snapshots, creates no draft, survives discarding every draft, and survives export → import | B329's corners-only writer loses it |
| C25 | 60 chips on three faces, 6 filled, each filled iff selected (and bound, for tags); every picker equals its binding; every row's bound tags are exactly its corners | the same fills judged against another corner, and a planted off-selection fill |
| C26 | Both themes' frames: 360 tiles, at most 4 colours per tile, 0 px off RGB555; the title slot equals B326's proposal pixel for pixel (1421 px); moving fixed macro Time changes 30 px and variable macro 1 changes 480 px, and restoring gives an identical frame | a planted 5-colour tile, an off-grid colour, and a frozen view model |

- **C17 caught a real hit during the build.** The frame canvas `GBC.fb` followed by `.width` spelled one of the manifest's bus keys. The property was renamed to `fcv` and the comment says why. The final scan found 0 of 530 needles.
- **The degraded state:** with `local/sluice` moved aside, the page shows "Sluice spec not available on this machine" and C14 passes (1/1).
- **`lab_load_check`:** GREEN with the link absent and present (58 labs, 0 broken, 1 skipped).
- **The private-name patterns** (case-sensitive, as the gate runs them), run by hand against the lab: 0 matches.
- **Verify:** `./verify fast` exited 0 on the lab commit. `.harness/last-verify.json` reads `{"target":"fast","exit":0,"git":"4863d9a","ts":"2026-09-28T17:18:27Z"}`. The trace commit is re-verified and its hash is in the PR.
- **Not verified:**
  - I checked only still frames. The ghosting and the drag were checked in code, not watched.
  - I did not test with a real human-drawn logo file, since none exists.
  - No audio was listened to; the engine is unchanged.

## Screenshots (scratch, not committed; `scratchpad/b337/shots/`, 2× DPR)

- `01-library-draft-light.png`: the snapshot library with 5 snapshots, bound tags, and delete confirming on an unbound one; corner B holding a DRAFT.
- `02-macros-fixed-variable-light.png`: FIXED F1–F4 beside VARIABLE M1–M4, with M2's inline editor open.
- `03-abcd-corner-C-dark.png`: the chips with C selected (filled) and holding a draft; the others outlined.
- `04-gbc-light.png` (ORCHID) and `05-gbc-dark.png` (TUBE): the GBC skin.

## Alternatives rejected

- **A stored draft flag.** Rejected: it could disagree with the content.
- **Macro definitions per snapshot, edited in the corners only** (B329). Rejected: the human said "saved to presets".
- **Refusing to delete a bound snapshot.** Rejected: C20's "enabled iff ≥5" would not hold for bound snapshots.
- **Drawing the logo as a DOM overlay** (the workshop's way). Rejected: compositing it into the frame lets C26's tile audit cover it.
- **Rebuilding the DOM on every quadrant crossing.** Rejected: it drops pointer capture mid-drag.

## Open questions

**For the lead:**
- C1's expected macro-slot set changed, from the manifest's 8 to the proposal's 4 variable slots. This follows the human's model, not a weakened gate, but it is a check's expectation changing, so it is flagged here.
- "The XY quadrant selects the corner" is my reading of "go to a corner's quadrant".
- Tail now rewrites the gate in every snapshot of a generated patch, so it never makes a draft.

**For the Sluice dialogue** (also on the page, to add to seq 10):
1. Do the fixed macros store per-patch positions, or stay global? The manifest keeps them unchanged when presets change.
2. Should 4 fixed + 4 variable = 8 be written into the manifest? Does §7's rule still hold, that macro 1 is bound to a time or a frequency parameter?
3. Do macro definitions live per patch or per snapshot?
4. A stored patch needs its corner bindings.
5. Snapshot naming: does Sluice's generator name snapshots? Should names be unique across the library?
6. The rule for deleting a bound snapshot.
7. Is a variable-macro move a draft, or an unsaved performance value?
8. Is Tail per patch or per snapshot?
