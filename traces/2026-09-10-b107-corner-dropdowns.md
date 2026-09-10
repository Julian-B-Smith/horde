# b107-corner-dropdowns — corner preset selects keep their name across a refresh

- **Queue item:** B107
- **Why:** Saving a morph corner preset pushes `fill()` to all four corner
  selects (the ADR-era "push, not pull" fix), and `fill()` replaces
  `ld.innerHTML` wholesale. A `<select>` whose options are replaced selects its
  first option — here the blank `load…` — so every save (and every focus, via
  the backstop refresh) blanked whatever corner preset each row was showing.
  The `names === null` guard only kept the list on a FAILED read; the success
  path had no restore. Fix: capture `ld.value` before repopulating and set it
  back if the store still lists that name. The mechanism matches the ROADMAP
  row's hypothesis exactly; no other path was involved.
- **Evidence consulted:** `src/gui/gui2.html` presetStore (~L1964–2006),
  corner rows under `mcorners` (~L2193–2244), `cornerSaveLive` handler
  (~L2047–2060); ROADMAP B107 acceptance text (from the brief). Before/after
  proof: a Node `vm` harness (scratchpad, not tracked) that extracts the real
  `fill` source from the file and runs it against a minimal select model whose
  innerHTML setter selects option 0 as a browser does. Scenario: save p1, pick
  p1 in A and B, save p2. HEAD: after the second save A,B read `"" ""` (FAIL,
  and the focus refresh blanked them too). Fixed file: A,B read `"p1" "p1"`
  after the save and after a focus refresh; C,D stay blank (PASS). No browser
  tool was available to this implementer, so the harness stands in for the
  brief's manual browser step; a human click-through in the plugin webview is
  still owed (see open questions).
- **Alternatives rejected:** Diffing options in place instead of rewriting
  innerHTML — more code for the same effect, and the rewrite is what the
  sibling `presetList` refresh does. Dropping the focus refresh — it is the
  backstop for presets saved outside this GUI and is harmless now that it
  restores the selection.
- **Verify:** `./verify fast`, exit 0, git f607281 (pre-commit hash from
  `.harness/last-verify.json`); `node tools/labharness/lab_load_check.mjs`
  GREEN — 25 labs loaded, 0 broken, 1 skipped.
- **Open questions:** Not exercised in the plugin's WKWebView host store path
  (`hzPresetList`), only the localStorage branch's contract via a stubbed
  store; the restore does not depend on which branch produced `names`, so the
  risk is low but it is entailed, not verified. If a corner's shown preset is
  deleted out from under it, the select now correctly falls back to `load…`.
