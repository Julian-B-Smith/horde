# 2026-11-08 — The wavetable page lab, round 3: the viewer switches views and is an XY you drag (GB37)

**What changed.** `docs/design/wavetable-page-lab.html` (still exploring). The lead glass is now the viewer.

- **The switch, on the glass's top left.** Six pips, or the view's name that steps on a click (shift-click steps back). The bench picks which form shows. The Q3 cards and the switch share one choice.
- **The XY drag,** on BladeView's pattern (GUI-30, B293 to B299):
  - **Relative:** a grab moves nothing.
  - **Default directions:** up is a later frame, right is more sync ratio, and shift with sideways moves the window.
  - **Range:** 420 px crosses a row's range, and Option moves a quarter as far.
  - **Undo:** one step per gesture that changed something.
  - **Keys:** the arrow keys work when the glass is focused.
  - **Readout:** what is moving shows at the top right, in the glass's own ink.
- **The legend pill, on the glass's bottom right,** opens the mapping:
  - three slots, each any of the nine continuous `wt.` rows in CONTRACT §2.1, with an invert;
  - Default puts it back;
  - one mapping for every viewer, remembered per machine and never saved with a patch, as BladeView's is.
- **The picture follows the rows:**
  - sync, window, asym and bend warp every frame, a MOCK of WT-32 to WT-34;
  - frame spread fans L6's members across frames;
  - the PM rows move the readout only.
- **A bench toggle for osc 1's sync input.** The lab starts it at × pitch so the default drag is heard. The engine's default is off, which is a question for the human.

**Why.** The human, on round 2: "I like all of these views. I'm thinking we should include some combination of them with a little clickable signal on the viewer to switch between them. Could we also make the viewer an interactive XY like the scalpel was? Maybe up/down scrubs the frame and left/right increases sync by default, with different options mappable or something?"

**Open, for the human** (not ruled yet):
- which views stay in the switch;
- pips or a name;
- what sideways does while sync is off (the engine's default);
- whether the view and mapping stay per machine, like BladeView's.

**Evidence.**
- **The lab's self-check in headless Chrome:** 30 OK and none failed, the round-2 rows plus these:
  - **The warp:** it is the identity with sync off and window, asym and bend at 0.
    - **Control:** sync ×3 changes the frames.
  - **Dragging up and right 84 px:**
    - the frame goes from 5.0 to 8.0;
    - sync goes from ×2.00 to ×3.48;
    - the window is untouched;
    - it makes one undo step.
  - **Shift with sideways** moves the window and leaves sync. Option moves a quarter as far.
  - **Control:** a grab with no motion writes nothing and makes no undo step.
  - **Mapped to Asym:** up moves asym and not the frame. Inverted, the same drag moves it the other way.
  - **The switch:** a pip changes the view and the picture, and the choice is remembered. The name steps to the next view.
- **A real-mouse probe** (CDP mouse events, a fresh profile):
  - the viewer drag moved the frame and sync and made one undo step;
  - pip 5 chose Rings, and the Q3 card followed;
  - the name switch stepped to Swarm;
  - the legend opened the mapping;
  - remapping up and down to Bend made a drag bend;
  - Default restored the mapping.

  No uncaught errors.
- **Screenshots** of the viewer and of V3, whose corner cards moved to clear the switch and the legend, were read.

**Verify.** `./verify fast` on the commit this trace ships in.
