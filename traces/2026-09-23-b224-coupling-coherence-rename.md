# b224-coupling-coherence-rename — rename K and R to Coupling/Coherence in player-facing labels

- **Queue item:** ROADMAP B224 (`grep '^| B224 |' ROADMAP.md`), dispatched as a
  standalone brief from the horde lead session, 2026-09-23.
- **Why:** Human, verbatim: "We should start labeling K 'Coupling Factor' or
  something (and maybe R can be 'Coherence') — K and R are meaningless to 99%
  of users." Renamed the swarm's coupling knob (K) and its coherence readout
  (R, the Kuramoto order parameter) to plain-English wording everywhere a
  PLAYER sees them, while leaving addresses, ids, JSON keys, code identifiers,
  specs and `reference/**` untouched.

- **Files touched:**
  - `src/param_presentation.tsv` — `osc.K` "Pull K" -> "Coupling", `osc.absK`
    "Absolute K" -> "Absolute Coupling", `osc.rtone` "R->Tone" -> "Coherence →
    Tone".
  - `src/gui/gui2.html` — regenerated via `python3 tools/gen_gui_controls.py`
    (never by hand) for the three rows above, plus three hand-placed edits:
    the `.vRead` coherence readout ("R —" / "R <b>0.523</b> · A … · B …" ->
    "Coherence —" / "Coherence <b>0.523</b> · A … · B …"), the OSC pad's
    y-axis header/tooltip ("XY — detune × K" -> "XY — detune × Coupling",
    "y → Pull K" -> "y → Coupling"), and the SET page's retired-pad note
    ("detune and K" -> "detune and coupling").
  - `src/gui/gui.html` (legacy shell, still buildable via
    `-DHYPERSAW_GUI2=OFF`, confirmed still player-facing) — ten matching
    renames: the CSS `::after` "K in absolute Hz" note, the two rmeter
    labels ("R₁ sync (clump)" / "Rₙ splay (formation)" -> "Coherence sync
    (clump)" / "Coherence splay (formation)"), the XY pad title (static and
    the live `xl.textContent` branch, both SAW and SPECTRA wording), the
    "pull K" row label, the "absolute K" toggle label, the "R→tone" row
    label, the Daido-moment dynamics readout, and the grid-lock warning.
  - `src/hypersaw_clap.cpp` — the `ParamDef.name` (3rd field, host-visible)
    for ids 6/12/31: "Pull K" -> "Coupling", "R->Tone" -> "Coherence -> Tone"
    (ASCII arrow — file has no other Unicode in display names, `id` and
    `coreKey` fields for all three rows confirmed byte-identical by diff).
  - `docs/design/compact-lab.html` — the embedded `PT` array only (6 rows:
    osc1/osc2 × K/rtone/absK label fields), matching the TSV exactly; nothing
    else in that file touched (out of the brief's scope).

- **Judgment calls (flagged per the brief's "propose exact wording where the
  obvious choice reads badly" instruction):**
  1. `gui.html`'s Daido-moment readout (`dyn = 'R' + s.poles + ' ' +
     s.RQ.toFixed(2)`, e.g. "R2 0.85") became `'Coh' + s.poles + …` ("Coh2
     0.85") rather than "Coherence2 0.85" — the concatenated full word reads
     worse, and the abbreviated form matches the compact "A 0.12 / B 0.34"
     sibling readout beside it.
  2. `gui.html`'s grid-lock warning ("lower K to hear the grid") became
     "lower coupling to hear the grid" even though the identical string is
     ported verbatim from the PROTECTED `reference/swarmdynamics.html:718` —
     left unchanged there, this instructional text would tell a player to
     adjust a knob that no longer has that name in the same file (line 270's
     "coupling" row), which is a live inconsistency the reference's own
     protection does not create (the reference does not rename its own
     "pull K" label).
  3. Left unchanged, with reasoning: the "R·e^iψ" phase-circle title
     (`gui.html`/`gui2.html` do not carry it, only `reference/*` and
     `gui.html`'s ported header do) is mathematical order-parameter notation,
     not a label — substituting a multi-syllable word inside a formula does
     not read as a formula anymore, and every reference HTML uses the same
     bare "R" (protected). `d1offR..d4offR` "R Offset" (the delay's
     right-channel offset — the brief's own named trap) and every "L over R"
     / "━ R" audio-channel reference were confirmed unrelated and left alone.

- **Out-of-scope findings surfaced, not acted on** (touching them would have
  expanded the brief's file list):
  - `tools/gen_factory_bank.cpp` — five factory-preset description strings
    use "Pull K" as prose ("Pull K is the knob…", "At K 0 sixteen voices…").
    These generate `docs/presets/factory/BANK.md` (already committed, also
    contains the five "Pull K" strings) — real player/sound-designer-facing
    documentation, but neither file is in the brief's FILES IN SCOPE list and
    neither is "a harness that keys on a renamed label" (BANK.md is
    generated prose, not a test assertion).
  - `src/swarmfx_clap.cpp:63` — the parked SWARM-FX shell's own `ParamDef`
    table has a host-visible name `"Pull K"` (id 2). Same shape as the
    `hypersaw_clap.cpp` fix but a different shell entirely, not named in
    scope.
  - Recommend both become their own ROADMAP row(s) if the human wants full
    consistency.

- **Evidence consulted:** ROADMAP.md B224 row; `src/param_presentation.tsv`
  header comments (presentation-table contract); `tools/gen_gui_controls.py`,
  `tools/presentation_check.py`, `tools/depends_check.py`,
  `tools/compact_lab_table_check.py` source (to confirm trailing-tab-trimmed
  TSV rows parse identically — all three guard with `len(r) > N`);
  `src/hypersaw_clap.cpp:77-85` (`ParamDef` struct — `coreKey` vs `name`);
  `reference/swarmsaw.html:123,125,196`, `reference/swarmdynamics.html:116,
  674,718` (protected reference — confirmed several gui.html strings were
  ported verbatim, informing the judgment calls above); exhaustive
  `\bK\b` / `\bR\b` word-boundary sweeps of `src/param_presentation.tsv`,
  `src/gui/gui2.html` and `src/gui/gui.html` (quoted-string, tag-text and
  full-file greps, cross-checked against a Python DOM-text extraction for
  `<label>`/`<h2>`/`.note`/`.vlabel` content).

- **Verify:** `./verify full`, exit 0, git hash `036da90` (from
  `.harness/last-verify.json`: `{"target":"full","exit":0,"git":"036da90",
  "ts":"2026-09-23T06:04:50Z"}`). `./verify fast` was also green pre-commit.
  Local CMake build (`-G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release`) of
  `HYPERSAW_clap`/`_vst3`/`_auv2` succeeded with the renamed `ParamDef`
  table. Visual confirmation: headless-Chrome screenshot of gui2's OSC page
  in LIGHT theme, served from this worktree on port 8224 (symlinked, not
  copied), saved to
  `scratchpad/b224/osc-page-light.png` — shows "Coupling", "Coherence →
  Tone", "Absolute Coupling", the XY pad's "y → Coupling", and the phase
  circle's "Coherence 0.803 · A — · B —".

- **Alternatives rejected:** using Unicode "→" in `hypersaw_clap.cpp`'s
  display name to match `gui2.html`/`tsv` — rejected because the file has no
  other non-ASCII characters in any `ParamDef.name` (checked all rows), so
  the ASCII "->" (matching the original "R->Tone") is the file's own
  established convention, not a divergence.

- **Open questions for the lead:** (1) the two out-of-scope findings above
  (`gen_factory_bank.cpp` / `BANK.md`, `swarmfx_clap.cpp`) — new ROADMAP rows
  or explicit non-action? (2) the two judgment-call wordings above
  ("Coh2 0.85", "lower coupling to hear the grid" diverging from the
  protected reference's "lower K") — ratify or adjust?
