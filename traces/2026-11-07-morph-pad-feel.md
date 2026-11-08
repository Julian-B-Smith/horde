# 2026-11-07 — The Morph page's pad and layout, and no text selected while dragging (GUI-61, GB36)

**What changed.**
- **The master, `1794086465-fbf9`:**
  - QuantumPad's `jumps` option: an empty click puts the puck there, and the drag carries it. A boundary is still dragged instead.
  - BlendMaps:
    - `onPad`, for the host's puck;
    - Option-click drops a waypoint;
    - ADD WAYPOINT arms the next click (renamed from "Add pin");
    - no double-click drop.
  - The Session sets `hd-nosel` while any pointer capture holds, and clears a selection the press began. `bundle.css` gives it `user-select: none`, `-webkit-` included.
  - Three READMEs say so.
- **`docs/design/gui3.html`:**
  - the Blend map card after the Morph field;
  - QuantumPad with `jumps`;
  - BlendMaps with `onPad`;
  - self-check 4b3d;
  - GUI-21's check updated to the ruling.
- **`tools/labharness/gui3_browser_check.mjs`:** real-mouse drags of a knob and the Morph pad onto a paragraph, on a fresh `?fast` load.
- **Records:**
  - GUI-61;
  - GB36;
  - the ledger's three rows, `master_versions` and engine impact "none";
  - the arp-face README's provenance row.

**Why.** The human's three requests, 2026-11-07, and "Morph page pad, always".

**Evidence.**
- `gui3_browser_check`: GREEN, 93 self-checks.
- **Plants, each run and restored:**
  - the old layout and no `jumps` read NOT on both in 4b3d;
  - without the Session's guard, both drags left the page selectable mid-drag.

**Found on the way.** These are headless Chrome's limits, recorded in the check's comment:
- its pointer capture already keeps these drags from selecting, guard or not, so the WebKit bug can't be shown here;
- a press at a half pixel selects nothing;
- while the button is down it reports only a caret;
- after the self-checks have run, a plain text drag leaves a caret.

The probe therefore holds the mechanism, on a fresh load, with whole-pixel coordinates.

**Verify.** `./verify fast` on the commit this trace ships in.
