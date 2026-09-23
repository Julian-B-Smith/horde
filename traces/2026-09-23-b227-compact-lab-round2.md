# b227-compact-lab-round2 — the human's three wireframes as working frames, B223's pad modulation view, and a drift check on the lab's table copy

- **Queue item:** B227 (with B223 designed inside it). Rows read with `git show origin/lead-records-82:ROADMAP.md | grep -E '^\| B22[37] \|'`.
  B227's acceptance, verbatim: "the MAIN + OSC schema and the alternative each rendered as working frames at plugin size
  (the alternative at whatever size it needs, stated); B223 prototyped; a list of every ambiguity the lead's reading
  could not resolve, answered from the images where they settle it and asked where they do not."
- **Why:** The human sent their own wireframes, so the lab builds the human's design, not the lab's. The wireframes were
  the primary source. The lead's reading in the row was checked against them and not trusted. Each frame reads and writes
  round 1's one store, so round 1's invariant (decision 6) covers the human's layout with no new machinery. B223 is
  designed where the pad actually lives: on the proposed Main and Osc pages.

## What changed

- `docs/design/compact-lab.html`, extended in place. Round 1's A/B/C frames, six decisions, ledger and self-audit are
  kept and still work. Above them, a ROUND 2 section adds:
  - **H1 MAIN (image 1)** and **H2 OSC (image 2)**, each at 980×720, and **H3 ALTERNATIVE (image 3)** at 1600×1000,
    which is the shell's maximum window (hypersaw_clap.cpp:9333-9338). Every control is a table address, and the tabs are
    live. Pages the wireframes do not redraw (FX, Mod, Morph, Mix, Set) render today's page from the table through
    `renderPageGroups`, which was split out of round 1's `renderFull` rather than copied. The global **Advanced** is round
    1's FULL view (a toggle). The Controls area's own **Advanced** expands in place. The decision-5 strip runs on every
    frame.
  - **B223**: `kdetPad`, a Coupling × Detune pad with x = Detune and y = K reading up (gui2's axes, ADR-156). It is drawn
    three ways: REACH BOX (recommended; ADR-137's arc in 2-D plus a ghost dot), GHOST + TRAIL, and EDGE RAILS. A triptych
    shows all three on the same store, and a pill picks which one the frames use. A morph-owned axis puts the owning
    corner's hue around the magenta ring. The ghost trail is computed from the LFOs' law at earlier lab times, through a
    new optional `at` argument on `lfoVal`/`srcVal`/`modOf`, so it is the same on every load. Sample & hold answers with
    its held value, so the trail never advances SH_RNG.
  - The lab's reading of each image, 20 numbered ambiguities, and an agree/conflict table against round 1's six decisions.
    These are static panels.
  - B224's vocabulary in the new frames only: "Coupling" for K, "Coherence" for R, and "Coherence → Tone". The table is
    unchanged, and so are round 1's frames.
  - A **new preset, SWARM RIDE**, now the landing preset. It has LFO 1 on osc1.detune, LFO 2 and M3 on osc1.K, so the pad
    has something to show. This is lab content, like round 1's presets.
  - **Visualizers are a lab toy.** They are a seeded mean-field Kuramoto sketch driven by the real Voices, Detune and
    Coupling addresses at their modulated values. The page says so, and nothing reads the toy back into the store.
  - `<meta name="lab-review" content="B212 + B227 · 2026-09-23">`. `docs/design/index.html` is untouched.
- **Self-audit, extended.** Three new lines: (1) every hand-listed W_* address is a table address; (2) every named table
  group exists; (3) B223's detector must find 0 modulated axes with no routes and 2 with SWARM RIDE's routes. That third
  line is the must-read-zero control. All three were calibrated by planting faults in a scratch copy (`osc1.volx`,
  group `Dynamicz`, SWARM RIDE's K routes removed), and all three fired: "NO … 1 foreign (osc1.volx)", "NO … 1 unknown
  (Dynamicz)", "NO … SWARM RIDE → 1 (must read 2)". On the real file: `ok: true`, 9/9 lines. Round 1's calibration line
  is now found by a `cal` flag instead of the fixed index `lines[3]`.
- `tools/compact_lab_table_check.py` (new). It compares the lab's embedded PT copy with `src/param_presentation.tsv`:
  the address sets in both directions, plus label, page, group, widget, designed (chunk named), shown_when, scale and
  unit for each row. It calibrates itself on every run with a planted relabel that must be caught exactly once. It
  deliberately does NOT check min/max/default/enum, which come from kParams, and it prints that limit on every green
  run. Current result: 368 rows match.

## Wireframe readings (summary; the lab has the full text)

- **Image 3 labels are read as bottom (foot) labels.** The image settles it. Reading them as tops would leave the first
  box of the right column unlabelled and put "Mod 1" on the box "Scale Quantization" labels at its foot. It would also put
  "XY" on the box "Wheels" labels. Image 1's Osc Controls uses foot labels too (zoomed: no rule above "K x Detune XY" or
  "Mini Visualizers"). Three exceptions are labelled at the top: the top row, Mix, and Visualizers.
- Image 3's second "Swarm 1" is read as Swarm 2.
- In image 2, the blue box and dot on "Waveform" are the spreadsheet's cell cursor. Waveform and Phase Carpet share one
  cell with no rule between them, and are read as stacked.
- Mod 1–4 are read as LFO 1, LFO 2, ENV 3 and ENV 4, the four modulator slots that exist today.

## Evidence consulted

The three wireframe images, including crops of image 1's Osc Controls and image 2's Visualizers. ROADMAP rows B212,
B223–B227 on `origin/lead-records-82`. `traces/2026-09-22-b212-compact-lab.md`. `docs/design/compact-lab.html` (round 1,
all of it). `src/gui/gui2.html`: the XY detune × K pad at :1682-1686; ADR-156's `detK` map, `wirePad` and
`padSpringRelease` at :4474-4560; PAGES at :4416-4419; route notes at :4660-4690. `src/param_presentation.tsv` (header
and columns). `verify` (fast body). `tools/test_table_check.py` (the rule 5 wiring grammar).
`tools/labharness/lab_load_check.mjs`. The design-system README. `git diff fb64f70 origin/main -- src/param_presentation.tsv`
is empty, so round 1's copy is current.

## Alternatives rejected

- **Changing SAW STACK's routes to feed the pad.** Rejected because it would silently change round 1's recorded frames. A
  new preset does the same job and leaves them alone.
- **A per-source colour on the pad.** A palette is an ADR-116 amendment that B207 left open, so sources are named by
  glyph instead. The box could take a source colour once a palette exists.
- **A remembered ghost trail.** Headless screenshots caught only a few frames of it, and it would differ on every load.
  It is computed from the law instead. The panel notes that the shell could not always do this, because envelopes, S&H
  and MIDI have no recomputable past.
- **Checking kParams ranges in the drift tool.** That would be a second parser of a C++ array the registry tools already
  own. The limit is stated on every run instead.

## Checks

- `node tools/labharness/lab_load_check.mjs docs/design/compact-lab.html` → `OK compact-lab.html`.
- `python3 tools/compact_lab_table_check.py` → 368 rows match, calibration plant caught.
- Screenshots come from headless Chrome against a server on port 8227 that serves this worktree. They sit in the session
  scratchpad and are not committed: light and dark (SWARM RIDE, reach box and ghost), GLASS CHOIR dark with the Sub tab
  (the morph-owned ring), INIT light with the Controls Advanced open and rails (the must-read-zero pads), and full-length
  light and dark pages including the panels and round 1.
- **Verify:** see the PR body and `.harness/last-verify.json` for the committed hash. This trace is committed with the
  change, so it cannot contain its own hash.

## Open questions

- **The drift check is UNWIRED.** The implementer charter forbids editing `./verify`. Its header states this, and the lead
  wires it with one line in `fast()`: `python3 tools/compact_lab_table_check.py || ok=1`.
- The 20 ambiguities in the lab. The ones that most affect the design are: (3) the axis order of "K x Detune";
  (4) Onset/Dissolve as one control, which would be a new parameter; (7)/(8) what each Advanced reveals; (9) what
  Filter 1 and Filter 2 are; (15) a polyphony count, which does not exist; (20) window size.
- Conflict with round 1's decision 4: the human's schema needs 980×720, and the alternative needs 1600×1000. It is not
  a smaller window.
- The ghost-trail recompute only works for sources whose past is a function of time. Porting B223 to gui2 needs the box
  or rails for envelopes, S&H and MIDI sources.
- The strip chips in the new frames still use the table's labels ("OSC1 Pull K"), because round 1's `nameOf` is shared.
  B224's rename would fix both at once.
