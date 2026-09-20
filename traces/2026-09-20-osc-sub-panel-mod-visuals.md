# osc-sub-panel-mod-visuals — the SUB gets its own OSC panel; the MOD page gets visuals and honest ENV 1/2 proxies

- **Queue item:** B176 (SUB as its own OSC panel), B177 (visuals for the LFOs
  and the new envelopes), B178 (ENV 1 / ENV 2 on the MOD page).
- **Why:** The human, after installing the build: "The structure is wrong. Sub
  is currently a section on the OSC pages, but it needs to be its own separate
  OSC panel"; "the LFOs and envelopes need visuals, and envelopes 1 & 2 should
  be represented on the MOD page with proxy controls and a warning". The SUB
  cluster sat between Pitch Env and Saw shape with `sub.on=1` gating every row,
  so the panel a player opened to switch the sub on showed only the switch.
  B171's four modulators shipped as four numbers each, which is not a thing a
  player can evaluate.

  **ONE PREMISE OF THE REQUEST WAS FALSE AND THE CORRECTED VERSION IS WHAT WAS
  BUILT** (the lead's ruling, re-verified here against the shell). ENV 1 (mod
  slot 0) is `envMax` — the maximum amp-envelope level over every voice of
  every ENABLED oscillator (`src/hypersaw_clap.cpp:3400-3411`, `modStep`) — and
  owns no parameters of its own. ENV 2 (slot 1) is a per-note-slot ADSR on
  params 162-165 (`src/hypersaw_clap.cpp:599-602`), the same four the OSC page's
  Pitch Env shows, which also drives pitch at the `Env > Pitch` depth
  (161, `:591`). Neither "controls the gain of OSC 1 and OSC 2 respectively".
  The human's INTENT — surface them and warn that they are not dedicated mod
  envelopes — is what the panel states, in the accurate wording.

- **Evidence consulted:**
  - `src/gui/gui2.html`: `buildOscSelectors`/`paintOscTabs` (the tab-bar idiom,
    raw `150 + k*OSC_STRIDE` never `effId`), `effId` (`base + editOsc*OSC_STRIDE`
    for non-global ids), `applyGates`/`learnGateKeys` (gate grammar and the
    `.row[data-addr]` key registration), `gvizCtx`/`drawEnvelope` (the
    per-stage sqrt-band layout and its rejected-alternative comment), the MIX
    channel strips (the precedent for a data-fixed per-oscillator row with no
    `data-addr`), the knob-grid walk.
  - `tools/presentation_check.py` — rule 1, TOTALITY: every declared parameter
    owns exactly one row, so the `sub.on` row could not simply be deleted.
  - `tools/gui_reach.py` — `params` is filtered to ids < 1000, so the engine
    band (4000-4015) is not gated there and the SUB power button needed no
    exemption.
  - `tools/gen_gui_controls.py` — `already` (hand-placement wins over
    generation) and the one-non-fixed-claimant collision gate.
  - `src/hypersaw_clap.cpp` — `modStep` (ENV 1), the penv block (ENV 2), the
    LFO block at 752-765, `lfoShapeAt` at 2579, and the sync law
    `freq = (bpm/60)/beats` at 3489.

- **Alternatives rejected:**
  - *Overloading `editOsc` with a third value for the sub.* It is `effId`'s
    input; a third value remaps every non-fixed control by 2000 and sends ids
    the shell does not declare. A separate `oscPanel` costs one variable.
  - *Calling `bridge.setVizOsc()` for the SUB tab.* The shell expects
    `0..kNumOsc-1`. The viz stays on the last swarm oscillator, hidden while
    SUB shows and correct on the way back.
  - *Rewriting the `.sc` scope tags to "sub".* The sub's rows carry no tag (an
    engine block gets none from the generator) and every tag that exists
    belongs to a swarm row that is hidden — relabelling them would name
    controls the sub does not own, and they would have to be restored.
  - *Blanking `chunk` on the `sub.on` row to stop it generating.* That files it
    under "not yet designed", and the undesigned count is the queue — the one
    number in that table that must not lie. `widget = none` says what is true.
  - *A second copy of the envelope layout for ENV 3/4.* drawEnvelope takes an id
    quad instead; one law, five call sites.
  - *`data-when` for the ENV 1 OSC-2 sub-group.* The brief expected the depends
    grammar to express "only when OSC 2 is enabled". **It cannot**: `applyGates`
    resolves a base key through the one NON-fixed control, which is whichever
    oscillator is being EDITED, so `enable=1` means "the edited oscillator is
    on" and never "osc 2 is on". Set in `paintOscTabs` instead — the one loop
    that already reads each oscillator's enable by raw id.
  - *A `mulberry32` stream for the S&H picture.* A fixed display list is
    unambiguous and the label says the real sequence comes from the patch seed.

- **Verify:** `./verify full`, exit 0, git `19d9863`
  (`.harness/last-verify.json`: `{"target":"full","exit":0,"git":"19d9863",
  "ts":"2026-09-20T15:42:29Z"}`). `presentation_check`, `depends_check`,
  `gen_gui_controls --check`, `gui_reach` and `lab_load_check` all GREEN.

  Behavioural evidence beyond the oracle: the branch was served with the
  `labs` server and driven by a scratch DOM probe under headless Chrome —
  46/46 PASS, including must-not-fire controls (selecting OSC 1 must leave no
  sub row visible; selecting SUB must leave no swarm cluster visible; `effId`
  must still address the last-selected oscillator after a SUB click) and a
  must-read-zero control on the canvas-ink detector. That control was added
  because the FIRST detector shared the bug's assumption: `gvizCtx` paints a
  tube ground and scanlines before any curve, so "differs from the top-left
  pixel" scored ~7000 on a blank canvas and every ink assertion passed for the
  wrong reason. The corrected detector reads 0 on ground+scanlines alone and
  1170-2343 once a curve is drawn. Three screenshots (OSC/OSC 1, OSC/SUB, MOD)
  are attached to the PR.

- **Open questions:**
  - The SUB tab is added to EVERY `.oscSel` host, so MAIN's bar carries it too
    and selecting SUB there sets the panel the OSC page will show. That matches
    "all kept in sync" but the human has not seen MAIN's bar with three tabs.
  - The LFO shape law is a second copy of the shell's `lfoShapeAt` (there is no
    bind returning an LFO cycle). Nothing gates the two against each other; a
    shape added to the shell would silently keep drawing the old set. The same
    exposure the bend curve has, now doubled.
  - The `oscPanel` selection is session state and is not persisted — out of
    scope here (B174 owns preset persistence).
  - `verify full`'s TE1 finding about `morphRouteEdit` vs the resolver is
    pre-existing and untouched by this change.
