# 2026-11-09 — The wavetable page lab, round 6: Bend on Option + sideways, the fine drag on Command (GB37, GUI-68)

**What changed.**
- **`docs/design/wavetable-page-lab.html`** (still exploring):
  - **Option + sideways moves Bend.** Plain sideways moves nothing by default. It is a fourth mapping slot, and the Default leaves it empty.
  - **Shift + sideways** still sweeps the sync ratio, and up and down still scrubs the frame.
  - **The fine drag moves from Option to Command:** a quarter as far, for any direction. BladeView is unchanged.
  - **The arrow keys follow:** Option with left or right steps Bend, and Command takes the small steps.
  - **The mapping box** lists four slots, each any `wt.` row or Nothing. It is wider, so the slot names fit on one line.
  - **The mapping's storage key is new** (`xymap3`), so a mapping remembered from round 4 cannot keep Bend on plain sideways.
- **Records:**
  - GUI-68;
  - the ledger's lab note, the row `gui68-option-bend` and GUI-68's engine impact (none);
  - a note on the `gui66-viewer-xy` row that GUI-68 amends its directions;
  - GUI-66's engine-impact text now cites ADR-389, which adopted the sync softness row;
  - GB37's status.

**Why.** The human, on round 5: "I think the left/right being bend is more finnicky than sync was. It should probably be opt+drag instead. I'll decide later if left and right alone should be anything". Option was the fine key, so I asked where fine should go; the answer was "Fine moves to ⌘".

**Readings recorded in GUI-68 for the human to correct:**
- with Option and Shift both held, Option wins;
- the fine key off the Mac is unruled.

**Evidence.**
- **The lab's self-check in headless Chrome:** every row OK, none failed. The new and changed rows:
  - dragging up and right 84 px moves the frame from 5.0 to 8.0, and Bend and sync stay at 0 and ×1, in one undo step;
  - **control:** a plain sideways drag writes nothing and makes no undo step;
  - Option sideways moves Bend from 0 to +0.40 in one undo step, with sync untouched (control);
  - Shift sideways moves sync from ×1.00 to ×1.74 with Bend untouched;
  - Command moves a quarter as far: Option + Command gives Bend +0.10, and Command + up gives frame +0.75;
  - with Option and Shift together, Bend moves and sync stays.
- **A real-mouse probe** (CDP mouse events with modifier flags, a fresh profile):

  | Drag | Result | Undo steps |
  |---|---|---|
  | plain sideways 84 px | nothing | 0 |
  | Option sideways | Bend 0 → 0.40 | 1 |
  | Shift sideways | sync 1.00 → 1.74 | 1 |
  | Command + Option sideways | Bend 0 → 0.10 | 1 |
  | up | frame 4 → 7 | 1 |
  | Command + up | frame 4 → 4.75 | 1 |

  The legend read "↕ Frame ⇧↔ Sync ratio ⌥↔ Bend". The mapping box showed Sideways = Nothing and Option + side = Bend. No uncaught errors.
- **A screenshot** of Q3 with the mapping box open was read.

**Verify.** `./verify fast` on the commit this trace ships in.
