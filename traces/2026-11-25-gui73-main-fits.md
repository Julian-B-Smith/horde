# gui73-main-fits — the dimmed-controls note moves into the header; Main fits the plug-in window

- **Queue item:** GB39 (GUI-73).
- **Origin:** the human's message on build 13 in Live, relayed by the lead on 2026-11-25: "Also realizing I want to do something about the main page having to scroll for the XYs". The lead assigned it to the GUI session under ADR-336.
- **Why:** in the plug-in, Main's second XY ran past the window, so the page scrolled.
- **Evidence consulted:**
  - The human's screenshot (via the lead).
  - The plug-in's own page (`tools/gui3_inline.py` under `embed_file`'s policy), loaded in headless Chrome at 980 × 720 with a stand-in bridge on the recorded fixture (`tools/labharness/fixtures/gui3-replay.json`).
  - `tools/engine-preview/preview_webview.mm`: a fixed 980 × 720 window, not resizable.
  - GUI-40 (the 228 px squares) and GUI-46 (dimming on the bridge).
- **Verify:** `./verify fast`, exit code in the PR.

## Measured cause
With the bridge on, `docs/design/gui3.html` put a "Dimmed controls aren't in this engine yet…" line on every page that has dimmed controls. On Main the measurements were:
- page area 463 px;
- the note 14 px plus its margin;
- the stage 434 px;
- the pad 150 to 378 and the XY 384 to 612, against a page bottom at 583.

So the XY overflowed by 29 px and `#pages` scrolled 492 over 463. With no bridge there is no note, which is why the browser lab fitted.

## Variants shown (renders of the plug-in page, sent to the human)
- **A:** the note goes into the header's page row; the squares stay 228 px.
- **B:** the note stays; the squares become 213 px.
- **C:** the note goes into the Info box. Dropped: it overflowed the box, so the box would scroll instead.

The poll offered A, B, and A with squares that size themselves to fit. The human chose A, and "Yes, add the check".

## What changed
- **`docs/design/gui3.html`:**
  - One `#inertNote` span (the bundle's `hd-hint`) is placed absolutely in `#pnav`, just after the last page tab. It is placed there because the bundle's PageNav empties the tab row on every group change.
  - It is shown only on a page with an `.inert` control, and `paintInert()` repositions it on each `selectPage`. Its text is the sentence's first clause; the full sentence is its title.
  - The per-page paragraphs are gone.
- **`tools/labharness/gui3_browser_check.mjs`:** a new FIT probe in the plug-in-mode run, with `fitVerdict`. The control is a run with a 30 px `#page-main::before` planted back. It is wired already, since this check is in `./verify fast`.
- **Records:** GUI-73 in `DECISIONS.md`, the ledger row `gui73-inert-note-in-header` with `engine_impact` "none: …", and GB39.

## Oracle evidence
- **This branch:** `gui3_browser_check: GREEN`, ending "Main fits the window there with no scroll, its pad 228 px and XY 228 px square and inside, the dimmed-controls note in the header meeting nothing (control: a 30 px line planted back on Main reads red)".
- **Main's shell with the new check (the must-fail run):** `RED: Main fit (GUI-73), in plug-in mode: its page area scrolls by 29 px; the XY is 228 x 228, not wholly inside the page; the dimmed-controls note is missing from the header; 3 note(s) still on a page`. The red run used origin/main's `gui3.html` in place, restored afterwards.

## Engine and FX device
- **Engine impact:** none. The window size is unchanged, there is no host resize, and no ENGINE-NEEDS row is needed.
- **FX device:** the same rule, in its header.

## Open
- If a page with many tabs (Osc has six) ever gets dimmed controls, the note could meet the colours; the check would catch that, and its place would then be a new ruling.
