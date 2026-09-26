# b268-morph-waypoints-ux — the waypoints lab opens editable, edits on the pad, and shows the laws apart

- **Queue item:** B268. The row is carried in PR #759 (branch `lead-records-93`), and I read it verbatim from `origin/lead-records-93:ROADMAP.md`. The human, 2026-09-26: "In the morph editor lab, I'm a little confused about the waypoints. They aren't editable and it isn't entirely apparent what exactly they're showing (they all look the same everywhere I've seen)".
- **Why:** The lead diagnosed five causes in the browser:
  1. The page opened in QUANTUM, where pins are dormant.
  2. The law panels were blank until a row was selected.
  3. The table sat a screen below the panels.
  4. The heatmaps differ only subtly.
  5. The edit path (arm, then turn) was buried.

  This round fixes the interaction and layout only. B235's substance is unchanged: the laws, the limits, the state proposal, and both earlier self-checks.
- **What changed** (`docs/design/morph-editor-lab.html` only; `lab-review` meta is now `B211 + B235 + B268 · 2026-09-26`):
  - **Opening state.** The page opens in BLEND with osc 1 Volume (id 17, two pins) selected. `OPEN_STATE` freezes this before any deep link applies.
    - The factory patch is itself QUANTUM. The parity audit reads `FACTORY_MORPH`, never `S.mode`, so the 1092/1092 cannot move.
    - In QUANTUM, a caution banner reads "Waypoints only act in BLEND; they're dormant here" and has a SWITCH TO BLEND button. The inspector's QUANTUM note has the same button.
  - **Layout.** One workbench band holds the pad, the cross-section with the four maps, and the inspector. The row table and Find sit directly under it. The kind legend and the fixture table come next. The long prose (the old tagline and the six answers) is a closed `<details>` at the bottom. The first `.tagline` is now a short one, which is the text the lab index reads.
  - **At the pad:**
    - a heading naming the painted row;
    - the one-sentence definition of a pin;
    - a colour-scale strip in the parameter's own values, with a ◆ tick per pin and an ink bar at the heard value;
    - a gesture line, or the single reason no gesture applies to this row;
    - a value tag beside every pin.
  - **Direct editing:**
    - drag a ◆ to move it;
    - drag its value tag vertically, alt-drag the ◆, or scroll on it, to change its value (shift gives fine control);
    - double-click empty pad to add a pin at the heard value;
    - right-click or Delete to remove a pin.

    Refusals print inline at the pointer: a corner (on add and on move), 8 per row, 64 per patch, stepped, gate, exempt, QUANTUM, and B232-silent. All of these go through ONE write path: `pinAdd` / `pinMove` / `pinSetValue` / `pinDelete`, each taking a state object. The inspector's ARM+HERE (`setPinAtPuck`), the pin list's ✕, the Delete key and the interaction audit call the same four functions.
  - **Cross-section.** A dashed line on the pad has draggable ‹ › ends. By default it runs through the row's two most distant pins, clipped to the pad; buttons offer ↺ THROUGH THE PINS and A → D. The plot overlays all four laws on one set of axes:
    - each law has its own screen-phosphor ink plus a dash pattern, and the heard law is drawn thickest;
    - today's blend with no pins is shown dim and dotted;
    - the range limits are drawn as alarm lines with an alarm band beyond them;
    - the set-value limits are dashed caution lines;
    - overshoot that a clamp cut is shaded, with the raw curve drawn thin;
    - pins on the line, the puck, and the corners the line crosses are all marked.

    A note measures each law's excursion past the set values along the line. TRY pills jump to the three demo rows.
  - **Four maps.** Each map carries a trait line and a measured "differs from TPS·Δ by up to x% of range". A new view, THE VALUE | DIFFERENCE FROM TPS·Δ (REC), shows each map as a diverging picture on one shared scale.
  - **New deep links:** `?mode=quantum`, `?sel=none`, `?maps=diff`, `?xs=x0,y0,x1,y1`, `?answers=1`, and `?drag=n&to=x,y`. The last one moves pin n through `pinMove` and leaves it drawn mid-drag, with a ghost and a trail.
  - **Pointer capture** is wrapped in try/catch, so a capture refusal cannot swallow a gesture.
- **Evidence consulted:**
  - ROADMAP B211, B235 and B268 (`origin/lead-records-93`).
  - `traces/2026-09-23-b235-morph-waypoints-lab.md`.
  - `tools/labharness/lab_load_check.mjs`.
  - `tools/gen_lab_index.py` (it reads the first `.tagline` and the `lab-review` meta).
  - `tools/serve_labs.py`.
  - `docs/design/station-page-lab.html` idioms (tokens, pills, badge).
- **Verified vs entailed:**
  - **VERIFIED (in-page self-checks, also run headless in node under lab_load_check's stub DOM on a311b54):**
    - field audit 1092/1092;
    - waypoint audit 6/6 properties and 4/4 controls, unchanged;
    - NEW interaction audit, all passing:
      - **Open:** BLEND on Volume with 2 pins; 4/4 law maps and the cross-section non-empty (spread > 1% of range, on the painters' own samples, through the painters' own `lawPanelsLive` predicate).
      - **Gestures 5/5**, through the write path on a scratch demo patch:
        - G1: a double-click at the heard value moves the pad by 5e-16, and the corners stay bit-identical.
        - G2: after a drag, every law passes through the moved pin.
        - G3: a value drag or scroll sets a value every law passes through, clamped to the range.
        - G4: delete works, and the row's key goes with its last pin.
        - G5: 9 refusals each name their reason and write nothing.
      - **Controls 4/4:**
        - MO1: B235's opening (QUANTUM, no row) fails the open check.
        - MO2: a flat, unpinned row (osc 2 Pull K) paints 0/4 maps.
        - MG1: a pin 5% off the heard value moves the pad by 4.9%.
        - MG2: a legal spot is refused by nothing.
  - **VERIFIED (scratch real-DOM harness, headless Chrome):** the lab ran in a same-origin iframe while real PointerEvent, MouseEvent, WheelEvent and KeyboardEvent events were dispatched at the pad. 15 assertions pass:
    - the page opens in BLEND on Volume, and the banner is hidden;
    - drag moves a pin;
    - a tag drag raises the value by 0.10;
    - a wheel notch adds 1% of range;
    - a double-click adds a pin at exactly the value heard there without it;
    - a right-click removes a pin, and Delete removes the selected pin;
    - the corner refusal shows inline;
    - QUANTUM shows the banner, and a drag there is refused inline with the pin unchanged;
    - the banner button returns to BLEND;
    - a section end drags;
    - the stepped-row refusal shows inline;
    - the inspector's ARM+HERE still pins.

    A 16th line, "no page error", reads a flag nothing sets. It is **not** evidence.
  - `lab_load_check` reports OK.
  - **Observed, not fixed:** between a pointerdown and the dblclick that follows it, the canvas rect moved by about 0.6 px (y 0.75 → 0.7484 in pad units). The pin lands 0.6 px from where the puck went. The cause was not chased.
- **Alternatives rejected:**
  - Plain vertical drag on the ◆ to change its value. It conflicts with drag-to-move, so the value handle is the printed tag (plus alt-drag and wheel).
  - A new named colour per law. Every named token is a status, so the laws use gui2's screen phosphors plus dash patterns.
  - A verify-wired node harness for the new audit. It would sit outside the brief's file scope; the existing self-checks are in-page, so the new one is too (see open questions).
  - Replacing the heatmaps. The brief says keep them; they gained a difference view instead.
- **Verify:** `./verify fast`, exit 0, git a311b54 (`.harness/last-verify.json`, 2026-09-26T03:18:46Z). This trace and the index regeneration are committed on top; the final hash is in the PR report.
- **Screenshots** (headless Chrome, 1600 px wide; scratch, not committed; `scratchpad/b268/`):
  - 01-open-light, 02-open-dark: the page as it opens.
  - 03-drag-light, 04-drag-dark: pin 1 mid-drag, with the cross-section.
  - 05-quantum-banner-light.
  - 06-refusal-diffmaps-light: an inline corner refusal, with the difference maps.
  - 07-sublevel-overshoot-dark: the planted overshoot, unclamped.
- **Open questions:**
  1. The interaction audit, like B211's and B235's, runs in the page and is not gated by `./verify`. Should a `tools/labharness/morph_editor_check.mjs` read `__morphEditorAudit` and be wired into `verify fast`? That would be a new check, and it is outside this brief's files.
  2. The 0.6 px layout shift between a click and a double-click (above).
  3. QUANTUM refuses DELETE of a dormant pin as well as edits, for consistency with "dormant = not editable". Should deleting stay allowed there?
