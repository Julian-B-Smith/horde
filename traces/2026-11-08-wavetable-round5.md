# 2026-11-08 — The wavetable page lab, round 5: Cells for the PM matrix, an output scope, and a double-click for Bend (GB37, GUI-67)

**What changed.**
- **`docs/design/wavetable-page-lab.html`** (still exploring):
  - **Cells are ruled.** C1 and R1 carry the tag "ruled". The other options stay in the pickers as the record.
  - **The output scope.** The glass under the viewer was the read cycle. It is now the oscillator's output after every stage, in the engine's order:
    - the blend at the puck;
    - the frame;
    - the frame spread;
    - sync, window, softness, asym and bend;
    - PM from the other two oscillators, then the oscillator's own feedback.

    The raw read frame is a dotted trace under it, and the harmonics glass beside it shows the output's harmonics.
  - **The PM in the scope is a MOCK:**
    - the other oscillators sit at unison, and their phase offset runs with Motion;
    - a depth d is an index of 10 d radians;
    - feedback runs two passes of the cycle, sample by sample, and keeps the second.
  - **A double-click on the viewer** sets Bend to 0, whatever the mapping, in one undo step. At 0 it writes nothing.
- **Records:**
  - GUI-67;
  - GN-59, the output cycle per oscillator;
  - GB37's status;
  - the ledger's lab note, the rows `gui67-pm-cells`, `gui67-output-scope` and `gui67-dblclick-bend`, and GUI-67's engine impact.

**Why.** The human, on round 4: "Cells are probably the way to go after all, in both cases. It will be important for there to be an output scope that shows the combined effects of all transformations. And double clicking the waveform should return bend to 0".

**Readings recorded in GUI-67 for the human to correct:**
- "all transformations" means the oscillator's own stages, before the filter;
- "the waveform" means the viewer, not the scope.

**Evidence.**
- **The lab's self-check in headless Chrome: 33 OK, none failed.** The new rows:
  - **The scope at rest:** with every depth and warp at 0, the output scope equals the read frame.
  - **Each transformation alone moves it:**
    - the frame, the puck and the spread;
    - sync, asym and bend;
    - PM from osc 2, PM from osc 3 and feedback;
    - window and softness, tested at sync ×2.
    - **Control:** PM leaves the viewer's table unchanged, so only the scope shows it.
  - **Inked:** the scope draws on both Osc pages, 19.0 % of its pixels.
  - **The double-click:**
    - Bend goes from +0.40 to 0.00 with Bend mapped to no direction;
    - the frame, sync and asym are untouched (control);
    - it makes one undo step;
    - a second double-click at 0 writes nothing.
  - The C1 and R1 rows from round 4 still pass.
- **A real-mouse probe** (CDP mouse events, two presses with clickCount 1 then 2):
  - Bend went from 0.4 to 0;
  - sync stayed at 2.2;
  - it made one undo step.

  No uncaught errors.
- **Screenshots** of Q3's Osc 1 page and of the Osc 1 page with the PM row were read. They had sync ×2.2, PM 0.25 from osc 2 and feedback 0.15.
- **The lab load check** is green.

**Verify.** `./verify fast` on the commit this trace ships in.
