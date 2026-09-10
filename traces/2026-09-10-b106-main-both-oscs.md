# b106-main-both-oscs — MAIN's viz cluster stops naming the active oscillator and shows both

- **Queue item:** B106 — "MAIN visualizers show BOTH oscillators."
- **Why:** MAIN inherited the OSC page's active-osc viz feed when the Editing bar
  left MAIN (ADR-150 era), so it drew one oscillator and *labelled* it "OSC 1"
  whichever one was selected — a label resolved from "active" is a label that
  can lie (L0028). The feed is now addressed by OSCILLATOR INDEX end to end:
  `VizSnapshot.osc*[k]` + `oscScope16[k]` on the wire, `data-viz-osc` on the
  canvas, so the pane's label and the pane's data cannot come apart. The OSC
  pages keep their single active-oscillator view, byte-for-byte.

## What changed

- `src/gui/hypersaw_gui.h` — `VizSnapshot` gains a per-oscillator carpet block
  (`kVizOsc`, `oscActive/oscOn/oscN/oscTopo/oscF0/oscPhase`) beside (not instead
  of) the existing active-oscillator block; `GuiHost::getScopeFor(osc, l, r, n)`
  joins `getScope` as a named-oscillator tap.
- `src/hypersaw_clap.cpp` — the scope ring is now one ring PER OSCILLATOR
  (`scopeL[kMaxOsc][2048]`), fixed-size members, so the audio-thread write in
  `applyOscGainAndMeter` is still a ring store and allocates nothing; the
  `k == vizOsc` gate that made only one oscillator tappable is gone. New
  `fillOscPanes()` runs at BOTH `publishViz` exits — the engine branches above
  wipe the snapshot (`v = VizSnapshot{}`), so a block written earlier would be
  erased silently.
- `src/gui/hypersaw_gui_common.h` — `vizToValue(v, withOscPanes)` carries the
  per-oscillator block; `hzFrame` gains want bits **8** (one packed scope block
  per oscillator, `oscScope16`) and **16** (the carpet block). Bit 8 REPLACES
  bit 4 rather than adding to it, so no page ever carries three scope blocks;
  each bit has its own DOM-derived consumer, per ADR-143.
- `src/gui/gui2.html` — MAIN's Viz cluster: master spectrum (one pane, labelled
  MASTER bus) + `OSC 1 waveform / OSC 1 phase carpet / OSC 2 waveform / OSC 2
  phase carpet`. `#vizWho`, `#vizWho2` and MAIN's `.whoOsc` spans are gone (the
  class survives on the OSC page). The scope painter and `drawCarpet` were
  parameterised by pane — the drawing bodies are unchanged, only their bindings
  became arguments — and `blitScope` now sources whichever MAIN pane the edited
  oscillator owns, which is how the OSC page keeps its single active view.

## Evidence consulted

- ROADMAP B106 acceptance criteria (quoted in the dispatch brief).
- `src/hypersaw_clap.cpp:1222` (the old `k == vizOsc` scope gate),
  `:2453` (`publishViz`'s active-osc intermediary), `:4784` (`getScope`).
- `src/gui/gui2.html` FEED_CONSUMERS / ADR-143 gate comment — the "gate on the
  class every rendering shares, never one rendering's id" rule (2026-08-29
  waveform bug), applied again here for `canvas.wavosc` / `canvas.carpetM`.
- LIBRARY L0028 (address a ROLE, LABEL the resolved instance), L0032 (a
  detector needs a control that must read zero).

## Measurements

Payload per `hzFrame` reply, viz half MEASURED by compiling `vizToValue`
extracted verbatim from the shipping header, scope half from the fixed wire
format (1536 samples x 2 ch x 2 bytes = 6144 -> 8192 base64 chars):

| page | before | after | delta |
|---|---|---|---|
| MAIN (`want` 1\|2\|8\|16) | 9,700 B | 18,274 B | **+8,574 B** = one oscillator's scope block (8,192) + the carpet block (382) |
| OSC / MIX / FX / MOD / SET / MORPH (`want` 1\|2\|4) | 9,700 B | 9,700 B | **0** — `viz` serialises to 1,164 B on both sides |

## Alternatives rejected

- **A second `VizSnapshot` / `getVizFor(osc)`** — doubles a 1 KB struct and a
  publish path to carry ~70 numbers of carpet; the per-osc block on the existing
  snapshot is the smaller change and leaves the active-osc path untouched.
- **One want-bit for both new feeds** — cheaper to write, but it makes a page
  with only carpets pay for two scope blocks. Two DOM-derived consumers, two
  bits, matching the gate rule already in the file.
- **Reusing bit 4 for "osc 0" and adding bit 8 for "osc 1"** — would have made
  the OSC page's block change meaning with the selection. Bit 4 stays "the
  active one"; bit 8 is "all of them, by index".

## Verify

- `./verify fast` — exit 0.
- `./verify full` — exit 0 at `bc6a833`, this commit's tree rebased onto
  `origin/main` 5fe7620 (i.e. after #526 and #527 landed; the rebase was clean,
  and full was RE-RUN on the rebased tree rather than trusted from the
  pre-rebase run). `bc6a833` and the final commit differ only in the wording of
  this trace file — `.harness/last-verify.json` is refreshed after the final
  commit and the run against the pushed HEAD is quoted verbatim in the PR body.
- `node tools/labharness/lab_load_check.mjs` — GREEN, 25 labs, 0 broken.
- Scratch probe (bridgeless vm harness, MAIN structure + per-pane painting):
  GREEN on this tree, **RED (12 failures) on `HEAD:src/gui/gui2.html`** — the
  calibration half, so the probe is known to be able to fail.

## Open questions

- **MAIN still carries three unlabelled active-oscillator readouts**: the phase
  circle (`#phaseC`), the `R / A / B` line (`.vRead`) and the gravity line
  (`.gravline`) are all fed from the active-oscillator block. B106's criteria
  name the labels and the two pane pairs, and duplicating the phase circle needs
  a per-oscillator `R/psi/sigma/Ksm/RA/RB` feed — a wider change than this brief
  authorises. The human's report ("not have anything that references only the
  active one") plausibly covers them; deciding whether MAIN keeps a single-osc
  phase circle at all is a design call I did not make.
- **SPECTRA mode**: `spectra.render` bypasses `renderSpan`, so no oscillator
  scope tap runs and MAIN's two waveform panes read stale. This is unchanged
  from before B106 (the tap always lived in `applyOscGainAndMeter`), not a new
  regression — but it is now visible in two panes instead of one. Osc 1's carpet
  does carry the partial-0 cloud in SPECTRA mode; osc 2's reads inactive.
- **Not verified in a real webview.** The behavioural evidence is a Node/vm
  harness against a recording canvas, not a browser or a DAW: it proves each
  pane is painted from its own oscillator's data, not that the result looks
  right. A human look at MAIN in the plugin is still owed.
