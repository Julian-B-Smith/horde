# b105-mod-second-route — a second modulator per param, and a manual destination picker on MOD

- **Queue item:** B105

- **Why:** The human reported two gaps on 2026-09-10: *"there's currently no way
  to send a second instance of the param to the mod matrix"* and *"in addition to
  'send to mod matrix,' there should be a way to manually select parameters from
  the mod page."* Both were GUI-only refusals of things the shell has always
  allowed. `modAddRoute` (`src/hypersaw_clap.cpp:1819`) refuses only stepped
  destinations, its own controls 161-177, and a full table — never a duplicate —
  and `ModCore::evaluate` (`src/mod_core.h:90`) sums every route per destination,
  which is exactly what two modulators on one parameter means. So the change is
  a GUI-side subtraction (the duplicate guard) plus one derived picker.

  Three things made this small rather than architectural:
  - **The duplicate guard was the only thing in the way.** `MODROUTES` has five
    other readers (`gui2.html`: menu release, halo reach, XY route notes) and
    every one of them already handled N-routes-per-dest — `modHaloFrame`
    accumulates `lo`/`hi` across *all* routes with a matching dest, and
    `modRelease` already says "Release all modulators (n)". Nothing downstream
    had to move.
  - **The picker's list is derived, not typed.** Options come from the panel's
    own `[data-p]` range controls and the `.page` each sits in; the panel's
    controls are themselves generated from `src/param_presentation.tsv`
    (`tools/gen_gui_controls.py`, gated in `./verify fast`). A hand-written list
    would be a second presentation table with nothing keeping it honest — L0005.
  - **Legality stays the shell's call.** Stepped params are indistinguishable
    from continuous ones in the DOM (octave and pitch are `<input type=range
    step=1>`), so the picker offers them, calls `hzModAdd`, and *shows* the
    refusal. Re-deriving `pd->stepped` GUI-side would be a second copy of a rule
    free to drift. The only filter applied is the widget's own type, which is a
    DOM fact and already ADR-141's rule.

  Per-osc fan-out is the picker's real reach: a non-fixed control addresses
  whichever oscillator is being edited (`effId`), so the panel can only ever show
  one twin — the invisible one is precisely what a manual picker is for. Each
  twin is offered by id (`base + k*OSC_STRIDE`, `k < numOsc`), skipping bases
  `globalIds` says have no twin.

- **Evidence consulted:** ROADMAP.md B105 (line 7092); `src/hypersaw_clap.cpp`
  `modAddRoute` / `modSetSource` / `modRoutesJson` (1819-1854); `src/mod_core.h`
  `addRoute` / `removeRoute` / `evaluate` (64-95, `kMaxRoutes = 64`);
  `src/gui/gui2.html` ADR-141 menu block, `renderModRoutes`, `modDestLabel`,
  `modHaloFrame`, `effId`/`OSC_STRIDE`/`learnOscLayout`; `verify` fast gate list;
  `tools/labharness/lab_load_check.mjs`; LIBRARY L0005, L0023, L0026, L0032,
  L0033, L0041.

- **Alternatives rejected:**
  - *A second menu entry ("Send another…") alongside "Send to mod matrix".*
    Rejected: ADR-141's whole ruling is that the menu stays two items long
    however many sources exist. Only the label changes.
  - *A hand-written destination list in the MOD markup, or reading
    `param_presentation.tsv` at runtime.* Rejected: the first is a second
    presentation table; the second needs the file at runtime and the GUI ships
    as one HTML file. The DOM already IS the table, generated.
  - *Pre-filtering stepped destinations GUI-side (e.g. by `step` attribute).*
    Rejected: `step` is a presentation choice, not the shell's `pd->stepped`
    flag; guessing would silently hide legal destinations the day the two
    disagree. The refusal is surfaced instead — and the probe confirms a stepped
    destination is offered and refused visibly.
  - *Calling `refreshModAdd()` from the setup-time wiring IIFE.* Rejected: it
    reads `MODROUTES`, whose `let` is declared further down the file — the
    forward-reach TDZ trap of L0026/L0041, five prior occurrences. The wiring
    attaches listeners and calls nothing; the first refresh comes from
    `renderModRoutes` when MOD is revealed.

- **Verify:** `./verify fast`, exit 0, on the branch rebased onto `origin/main`
  `f264ffe` (#526 and #527 had merged; the rebase was clean despite both
  touching `gui2.html`). Plus a headless-Chrome probe of `src/gui/gui2.html`
  (scratchpad only, untracked) that boots the page in a REAL DOM with `hz*` mod
  stubs whose refusal rule is parsed out of `src/hypersaw_clap.cpp`'s ParamDef
  table (84 stepped ids) rather than guessed: 27 assertions, all PASS on the
  rebased tree, including four must-differ controls (a global dest appears
  exactly once; no duplicate ids; an impossible search matches nothing; an
  unrouted param still reads "Send to mod matrix").

  **Calibrated** per L0032, twice, each against an isolated copy of the tree:
  re-planting the removed duplicate guard reports `PROBE-END RED`; reverting
  the base-id test below to a raw-id test reports `PROBE-END RED` naming all
  thirteen phantoms. The shipped tree reports GREEN.

  **The probe found a defect in this very change, which is the point of running
  it.** The first draft excluded the matrix's own controls by testing the RAW
  id (`id >= 161 && id <= 178`), so the per-osc fan-out happily manufactured
  thirteen `· osc 2` twins of global controls — "M1 · osc 2", "Env > Pitch ·
  osc 2", the four P.Env twins. Every one is an address that means nothing;
  161-177 are global by construction. The shell would have refused them
  (`findParam` returns null), so no oracle in `./verify` and no audio test could
  ever have seen it — it was purely a list of thirteen lies in a menu. The test
  is now on the base id (`g`), and the probe asserts it on the base id too.
  Destination count 233 → 220.

  **A harness bug worth recording, because it nearly became the finding.** The
  first runner wrote every artifact to one `dom.html`, and Chrome writes its
  dump early then lingers, so two concurrent runs over *different source states*
  both reported GREEN — the planted regression included. The verdict was read
  from whichever run wrote last, not from the run that was asked. Caught only
  because the rendered screenshot of the planted build visibly lacked "Send
  another". Every artifact is now tag-scoped (`dom-<tag>.html`,
  `shot-<tag>.png`, `cp-<tag>/`) and each state was re-run in isolation. Shared
  output between runs is not weak evidence; it is no evidence.

- **Open questions:**
  - The shell round-trip is **untested here**. `window.hzModAdd` is undefined in
    a bridgeless `file://` load (the dev mocks at `gui2.html:1453` do not stub
    the `hzMod*` family), so the probe supplies its own. That the picker's
    ids are the ids the *shell* accepts rests on `modDestLabel`/`effId` already
    being the addresses every other GUI path uses — entailed, not verified. A
    host run is the only oracle for it.
  - The probe's `globalIds` comes from the 21-entry dev `DEFAULTS` map, so most
    base ids read as global and the fan-out is exercised on the five ids that do
    have twins there (14/17/35/36/37). Under a real shell more params fan out;
    the list will be longer than the 233 options measured.
  - 233 destinations in one `<select>` is usable with the search box but is not
    a *good* browse. If the human wants it grouped further (page → cluster), the
    cluster `<h2>` is already in the DOM and one line away.
  - **The macro knobs 166-173 on MAIN carry no `data-fixed`** (`gui2.html:846`).
    They are patch-scope by every other sign (the shell dispatches 161-177 by
    raw id, and `modAddRoute` refuses that range as global), so `effId` remaps
    them to 1166-1173 whenever oscillator 2 is being edited — the exact
    29-dead-controls shape `gui_reach.py`'s patch-scope pin exists to catch.
    That is how the phantom destinations were reachable at all. NOT touched:
    out of scope, and it is a shell-addressing question, not a picker one.
    Worth its own queue row.
  - The probe reports a THROW as a single line, so the planted run shows only
    `THREW TypeError … reading 'click'` — the assertions that passed before the
    throw are lost. Enough to prove the plant fires, not enough to localise a
    future failure. A per-assertion try/catch would fix it.
  - No `./verify` gate sees any of this. The picker is GUI-only and
    `lab_load_check` proves only that the file loads; the browser probe lives in
    scratchpad. Making it a gate would need Chrome in `./verify`, which is a
    human decision (it edits the gate list).
