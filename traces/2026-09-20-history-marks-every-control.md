# history-marks-every-control — one balanced gesture bracket per control, and a gate that keeps it

- **Queue item:** B191 ("I'm also not certain the sub osc parameters are all wired into history
  properly (I changed the shape value and it didn't make a node). I want to make sure all relevant
  parameters are." — human, 2026-09-21)

- **Why:** A history node exists only where a gesture bracket CLOSES
  (`src/hypersaw_clap.cpp` `guiGesture` calls `undoMarkParam` on the END and on nothing else).
  gui2 opened and closed that bracket from `pointerdown`/`pointerup` **on the control element**,
  and that signal is missing for a whole CLASS of controls — not for the sub's Wave alone.
  The fix moves the bracket onto the signal every control does emit (its own value change) for
  discrete controls, keeps the drag bracket for continuous ones, and puts both triggers through
  ONE idempotent latch per element so the two cannot disagree or double.

- **Evidence consulted:**
  * `src/gui/gui2.html` — the generic `[data-p]` wiring (pre-fix: `pointerdown`→`gesture(true)`,
    `pointerup`→`gesture(false)`); `wireKnob` (the skin's own bracket, plus its comment that an
    unbalanced gesture "is how a host records automation it can never undo"); the `.knob
    input[type=range] { … pointer-events:none }` rule whose own comment reads "the input keeps the
    value and the keyboard; the SKIN takes the pointer" — i.e. the inner input's pointer listeners
    were dead and its keyboard path had no bracket at all; the `.msbtn` branch, which `return`s
    BEFORE any bracket is wired; the two hand-placed power buttons in `buildOscSelectors` /
    `buildSubTab`, which called `bridge.setParam` with no bracket at all.
  * `src/hypersaw_clap.cpp` — `guiGesture`, `undoMarkParam`, `undoService` (the node is made at the
    END, and only once the param queue has drained); `stateJson` (what an undo node actually
    stores); ADR-160 (3) (host events and automation never mark — structural, not filtered).
  * `src/undo_tree.h` — `push` dedups a snapshot identical to the one under foot, which is why the
    new per-parameter check writes a value BEFORE bracketing.
  * `src/param_presentation.tsv` — the totality table (widgets: knob 257, select 69, toggle 36,
    enum 4, none 2); `tools/gen_gui_controls.py` (gui2's controls are GENERATED from it, and
    `./verify` gates their currency — that chain is what makes a new parameter covered
    automatically).
  * `tools/undo_check.cpp` — `editablePool` and the `kAliasGapId` / `kCombType` idiom for a
    coverage boundary that is re-earned every run; `automationControl`'s must-read-zero + a
    calibrating one.
  * `tools/test_table_check.py` — the ADR-180 §1 wired-or-`UNWIRED:` rule and its 40-line header
    window; `tools/gui_reach.py` — the patch-scope derivation that explains id 1043.
  * LIBRARY L0023 (an invisible control no audio oracle can see), L0032/L0033 (a plant must fire;
    a zero needs a one beside it), L0036 (pin your refusals), L0046 (exact-line anchors),
    L0051/L0056 (never chain verify to commit in an isolated worktree; read
    `.harness/last-verify.json`).

- **The matrix, measured not reasoned** (`node tools/labharness/gui_history_check.mjs` run against
  the PRE-FIX page, markers added and nothing else: 399 failures over 874 scenarios):

  | control | interaction | pre-fix | post-fix |
  |---|---|---|---|
  | knob (184) | skin drag | 1 bracket | 1 bracket |
  | knob (184) | keyboard arrows on the focused input | **0 brackets** | 1 bracket |
  | knob (184) | drag stolen (pointercancel) | 1 bracket | 1 bracket |
  | bare range (25) | drag | 1 bracket | 1 bracket |
  | bare range (25) | keyboard arrows | **0 brackets** | 1 bracket |
  | bare range (25) | pointercancel | **left OPEN** | 1 bracket |
  | select (47) | native popup eats the release | **left OPEN — the human's case** | 1 bracket |
  | select (47) | keyboard / type-ahead | **0 brackets** | 1 bracket |
  | select (47) | popup that does return the release | 1 bracket | 1 bracket |
  | checkbox (32) | click | 1 bracket | 1 bracket |
  | checkbox (32) | keyboard (space) | **0 brackets** | 1 bracket |
  | checkbox (32) | release lost | **left OPEN** | 1 bracket |
  | msbtn M/S (4) | click | **no bracket wired at all** | 1 bracket |
  | osc power button (2) | click | **no bracket wired at all** | 1 bracket |
  | SUB power button (1) | click | **no bracket wired at all** | 1 bracket |
  | matrix cell | drag / wheel / double-click | 1 bracket | 1 bracket |

  The sub's Wave is id 4000, `<select data-p="4000" data-fixed="1">` at `src/gui/gui2.html:1761` —
  RED pre-fix on both of its interaction paths, GREEN after. **Not verified and not claimed:**
  WHICH of the two select paths the human personally took. Both were broken; the platform
  behaviour of a native `<select>` popup is not observable from any headless harness on any
  platform, so the fix removes the dependence on pointer events rather than asserting what the
  popup does.

- **What changed**
  1. `src/gui/gui2.html` — `gestureFor(el, idOf)`: one idempotent latch per ELEMENT, cached on it,
     remembering the id it OPENED with so a bracket cannot be doubled, left open, or closed against
     a re-aimed id. Continuous controls (`type === 'range'`) keep `pointerdown`/`pointerup` (plus
     `pointercancel`, new); every other control, and the keyboard path of a range, brackets around
     its own value change. `wireKnob`, `resetToDefault`, the M/S buttons and both power buttons all
     go through the same latch — there is now one bracket implementation, not four.
  2. `src/gui/gui2.html` — `NO_HISTORY_IDS = new Set([178])`: the declared exclusion. ADR-147 rules
     the specimen toggle a GUI preference `applyStateJson` skips, so a node for it would restore
     nothing. The gate READS this set out of the page, so there is one copy.
  3. `tools/labharness/gui_history_check.mjs` (new, **UNWIRED**) — lifts the wiring verbatim out of
     the page (marker pair `GATE:BRACKET`, plus `wireKnob` / `mxWire` / `buildOscSelectors` /
     `buildSubTab` by exact-line anchor) and EXECUTES it over node's real `EventTarget` against fake
     controls built from the page's OWN markup. 874 scenarios over 292 controls. Unknown control
     kind = FAIL, never skip. Two plants (the pre-B191 pointer-only shape; a control with no bracket
     at all) must both be caught by the same verdict function that must pass the real wiring.
  4. `tools/undo_check.cpp` — layer 4. `unclosedBracketControl`: an OPEN bracket makes ZERO nodes
     (the human's bug at the shell), with the close of that same bracket making exactly one as its
     calibration. `controlMarkChecks`: for every one of the 395 declared parameters, a value change
     plus a closed bracket makes exactly one node (365 covered; see the boundary below).

- **Alternatives rejected:**
  * *Headless Chrome for the GUI half.* It cannot drive a NATIVE `<select>` popup either — the case
    under test — so it would buy nothing over executing the wiring, and would cost `./verify` a
    browser dependency.
  * *A static rule over the wiring source.* A static check has to guess which listener fires when,
    which is exactly the reasoning that produced the bug.
  * *Special-casing the sub.* The lead's brief localised it correctly: 47 selects, 32 checkboxes,
    184 knob keyboard paths, 4 M/S buttons and 3 power buttons were all in the class.
  * *Converting `mxWire` to the shared latch.* It is already balanced and correct; the gate covers
    it as-is, so a rewrite would be risk with no coverage gain.
  * *Wiring the new gate into `./verify`.* ADR-180 §1 permits it, but this agent's operating rules
    forbid editing `./verify`. It carries `UNWIRED:` with the one-line patch instead — a debt for
    the lead, stated at the top of the file where `test_table_check` reads it.

- **Verify:** `./verify full`, exit 0 per `.harness/last-verify.json`, on the change-set commit
  `348bf95`. This trace lands as a second commit and the oracle is re-run on the final HEAD; both
  hashes are in the PR body, because a trace cannot cite the hash of the commit that contains it.
  `undo_check: GREEN (0 failures)`; `gui_history_check: GREEN (874 scenarios over 292 controls +
  3 power buttons + 1 matrix cell)`; `test_table_check: GREEN (46 check files wired, 11 UNWIRED)`.

- **Open questions:**
  1. **FOUND BY THE NEW CHECK, OUT OF SCOPE, AND IT MATTERS: the routing matrix is outside
     undo/redo entirely.** All 29 declared routing ids (`>= 10000`) accept a value and read it back
     changed, and leave `stateJson()` byte-identical — `stateJson` emits kParams, the per-oscillator
     copies, the engine blocks, morph, modRoutes and intent, and no routing coefficient
     (`src/hypersaw_clap.cpp:5773`). The binary chunk has a `routing=` section; the JSON path, which
     is what `UndoTree` stores, does not. So a matrix edit brackets correctly and produces a node
     that restores nothing. Fixing it is a preset-schema decision (ADR-sized), so layer 4 PINS the
     boundary and re-earns it every run — the row says in as many words that when it goes red,
     routing has entered the state and the boundary should be deleted.
  2. Id 1043 is declared but reaches nothing: base id 43 is dispatched by RAW id to a shared object
     (gui_reach's patch-scope derivation), so the oscillator-2 twin accepts a write and reads back
     unchanged. Pinned as the single non-routing member of the boundary. Whether the twin should be
     declared at all is a parameter-table question, not a history one.
  3. The new gate is UNWIRED (one line in `./verify fast`). Until the lead wires it, the GUI half of
     the property is checked by nobody.
  4. Behaviour change worth the human's eye: a range moved by the KEYBOARD now makes one node per
     arrow press (it made none before). That is the honest semantics of a discrete keyboard step,
     but a held arrow key will fill the history faster than a drag does.
  5. The specimen toggle (178) now writes its parameter and marks nothing at all — including no
     host-visible gesture. Previously it bracketed. Declared in `NO_HISTORY_IDS` with its reason;
     if the human wants it in the history, that is a one-line deletion plus an ADR-147 amendment.
