# 2026-11-09 — The wavetable page lab, round 7: the selected frame against the modulated frame in every view, and a tighter vertical drag (GB37, GUI-69)

**What changed.**
- **`docs/design/wavetable-page-lab.html`** (still exploring):
  - **The selected frame in each view.** Each of the six views draws the selected frame (the oscillator's frame row) dashed in meter ink, at its own place in that view: Stack, Ridges, Turntable, Sonogram (a bracket), Rings and Swarm (a cycle). The modulated frame stays the value-ink beam. The Sonogram's modulated bracket moves to value ink. The dashed frame shows only while the two differ.
  - **The frame rail** on the viewer's right edge: a hollow ring at the selected frame and a dot at the modulated one, a line between, and both numbers.
  - **The readout** reads "FRAME 5.0 → 8.2" while the two differ.
  - **The bench's Frame mark** offers Both, Picture and Rail, for the human's pick.
  - **The vertical drag** crosses its row's range in 150 px (`DRAG_PX.v`), down from 420. Sideways keeps 420.
- **Records:**
  - GUI-69;
  - GN-60, each oscillator's modulated frame position;
  - the ledger rows `gui69-frame-marks` and `gui69-vertical-drag`, GUI-69's engine impact and the lab note;
  - GB37's status.

**Why.** The human, 2026-11-09: "there should be an indicator on the waveform in each view that shows the selected frame position versus the modulated frame position, and reads out both. Also up/down dragging doesn't scrub through frames quickly enough; it takes almost the whole length of the page to get through all 16 and ought to be tighter".

**Readings recorded in GUI-69 for the human to correct:**
- the tighter range belongs to the vertical direction, whatever row it is mapped to;
- the marks are drawn on the viewer only, not on the corner tiles.

**Evidence.**
- **The lab's self-check in headless Chrome:** every row OK. The new and changed rows:
  - **The frame marks, in all six views:** at Motion's tick 75 (frame 5.0 modulated to 8.2), Both differs from Rail, so the dashed frame draws, and from Picture, so the rail draws. The readout reads "FRAME 5.0 → 8.2".
    - **Control:** with Still, the frames meet. Both equals Rail, and the readout reads "FRAME 5.0".
  - **The vertical drag:** up 150 px crosses frame 1 to 16.
    - **Control:** 150 px of Shift sideways moves sync 36 % of its range, so sideways keeps 420 px.
  - **The earlier drag rows, re-measured at 150 px:**
    - up and right 84 px takes the frame from 5.0 to 13.4;
    - Command + up 84 px moves the frame +2.10;
    - the Asym and two-on-Bend rows now drag 30 px up.
- **A real-mouse probe** (CDP mouse events, a fresh profile), each drag from frame 1:

  | Drag | Result |
  |---|---|
  | up 150 px | frame 0 → 15 (16 frames) |
  | up 75 px | frame 0 → 7.5 |
  | Command + up 150 px | frame 0 → 3.75 |
  | Shift sideways 150 px | sync 1.00 → 2.69, frame untouched |

  No uncaught errors.
- **Screenshots** of all six views with the frame modulated from 5.0 to 8.1 were read.
- `engine_needs_check`, `design_chain_check` and `lab_load_check` are green.

**Verify.** `./verify fast` on the commit this trace ships in.
