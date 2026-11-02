# main-layout-lab — four layouts for Main, on the running shell (B581)

- **Queue item:** B581 step 2. GUI session. The human asked for the Specimen XY and the generative logo on Main and said Main shouldn't have a square envelope, then chose "Main's layout" next.
- **Why a lab:** Main's layout has never been ruled, and a question the human asked starts one. Nothing in the shell, master or bundle changed.
- **What it is:** `docs/design/main-layout-lab.html` loads `gui3.html#main,win` in a frame and moves its real controls, Lens and morph pad into each layout. `main-layout-lab-parts.js` holds the bench's logo (E2) and Specimen (E3) verbatim, with shims in its header, as ADR-333 asks. The file retires with the lab.
- **Evidence consulted:** the bench's `buildMain`, ADR-139, ADR-140, ADR-262 r1, ADR-315, ADR-333, ADR-338 r5 and r7, B227's wireframes, B299's open question on what the XY holds, the master's Logos and XYPad READMEs.
- **MOCK:** the XY's value, the macro knobs (a local session), and the notes.
- **Measured:** see the lab's table. M1, M3 and M4 fit with morph on and off. M2 spills 24 px with morph on, because the docked pad takes the width. None reaches the master's "at least half" for pictures; M2 is closest, at 48%.
- **Found while building:** the page's scroll read every layout as fitting, while a too-small grid cell spills its controls without scrolling, so the table measures the spill instead.
- **Depends on:** #1327, for the size-aware envelope picture. This branch was cut from it and targets main.
- **Not checked:** the Specimen's cost in a plugin webview, which is what ADR-140 measured as untenable; other window scales; the light scheme.
- **Ruled in the same PR:** GUI-2, the human's poll: M1, assignable XY, Specimen on by default, macro knobs. The lab had recommended M4 for its big single surface; the human chose the bench. Not built: this PR only records the ruling.
- **Checks:** the lab's self-check, 11 of 11, with its controls; the shell's own self-checks, 49 of 49, in the frame; `lab_load_check` green. `./verify fast` exit 0 on this commit's tree.
