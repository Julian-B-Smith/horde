# 2026-11-09 — The wavetable page lab, round 8: all six views stay, and every direction under every button is mappable (GB37, GUI-72)

**What changed.**
- **`docs/design/wavetable-page-lab.html`** (still exploring):
  - **Six mapping slots.** Up and down, and sideways, each plain, with Shift and with Option. Before, only sideways had Shift and Option slots.
  - **Defaults leave the drag as it was:**
    - up and down is Frame, with or without a button;
    - plain sideways is Nothing;
    - Shift + sideways is the sync ratio;
    - Option + sideways is Bend.
  - **The legend** lists a button's slot only where it differs from the plain slot of its direction, so by default it reads as before.
  - **The readout** names each moving row once.
  - **The arrow keys follow the buttons.**
  - **In the Sonogram** only the plain pair swaps (GUI-70).
  - **Storage:** the mapping is stored under a new key, and a round-6 mapping carries over with the new slots on their defaults.
- **Records:**
  - GUI-72;
  - the ledger row `gui72-six-views-six-slots` with its engine impact (none);
  - the lab note;
  - GB37's status: still open are pips or name, and per-machine storage.

**Why.** The human, on round 7: "I like all of the views for now. If I want any removed we can do it down the line. I don't think plain sideways should move anything at the moment, but let's make both directions (and all button+direction combos) mappable for now".

**Readings recorded in GUI-72 for the human to correct:**
- the buttons are Shift and Option, and Command stays fine;
- Option outranks Shift;
- the new up-and-down slots default to Frame, not Nothing;
- only the plain pair swaps in the Sonogram.

**Evidence.**
- **The lab's self-check in headless Chrome: 43 OK, none failed.** The new rows:
  - **By default**, Shift + up still moves the frame (5.0 → 8.0) and the legend reads "↕ Frame ⇧↔ Sync ratio ⌥↔ Bend".
  - **Mapped:**
    - Shift + up moves Asym and Option + up moves PM A, not the frame or Bend;
    - plain up still moves the frame (control);
    - Option + Shift is Option;
    - Shift + the up key steps Asym.
  - **The Sonogram:** Shift + up feeds its own slot, and plain up moves nothing (control).
  - **A round-6 stored mapping carries over:** sideways on inverted Asym is kept, and the new slots take Frame.
- **Plant:** with the old rule (up and down always the plain slot), the two new drag rows fail.
- **A real-mouse probe** (CDP mouse events with modifier flags):
  - **Default mapping:**
    - plain, Shift and Option up 30 px each moved the frame 5 → 8;
    - Shift right 84 px moved sync ×1 → ×1.74;
    - plain right moved nothing and made no undo step.
  - **The mapping box,** through its real selects: it lists six rows, and Shift + up was set to Asym and Option + up to inverted Asym.
  - **After mapping:**
    - Shift up gave asym +0.40 and Option up gave −0.40, with the frame untouched;
    - plain up still moved the frame;
    - in the Sonogram, Shift up gave asym +0.40.
  - The six-slot mapping was stored.

  No uncaught errors.
- **A screenshot** of the Osc 1 page with the mapping box open was read: six rows, and the box fits.

**Verify.** `./verify fast` on the commit this trace ships in.
