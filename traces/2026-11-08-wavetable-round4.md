# 2026-11-08 — The wavetable page lab, round 4: GUI-64, GUI-65 and GUI-66; sync always on (GB37, GB38)

**What changed.**
- **`docs/design/wavetable-page-lab.html`** (still exploring):
  - **The viewer's drag:**
    - sideways is Bend, and Shift + sideways is the sync ratio;
    - sync is always on (the lead's B661 lab), so the ratio sweeps up from ×1, which is no sync;
    - B661's sync softness joins the mappable rows, and the mock warp gains its taper, which fades in from ×1 to ×1.25;
    - the stored mapping's key is new, so round 3's remembered mapping can't keep sync on sideways.
  - **Q1 is recorded as ruled.** Its variants' code is gone; it is in git history at 3c8dfa9c.
  - **Q2 now auditions the looks GUI-65 left open:** card C1 to C4 on an Osc dashboard window, and row R1 to R3 on an Osc 1 page with the viewer, the corner row and the internal modulator.
  - **Two bugs fixed:**
    - Two mapped directions on one row now add up. Before, a still direction overwrote a moving one; the real-mouse probe found it.
    - Every depth fill drew full size since round 1: `pct()`'s "22 %" is not valid CSS. Styles now use `cssPct`.
- **Records:**
  - GUI-64, GUI-65 and GUI-66;
  - GN-58 (PM observation) and GN-57's note;
  - GB37's status and a new GB38 (the global signal flow lab);
  - the ledger's three rows, their engine impact and the lab note.

**Why.** The human:
- "shift+sideways drag would be better" for sync;
- "Bend" for sideways, when asked;
- Q1: "the layout from Q3 best (V1 if it gets condensed)";
- Q2: "both the P1 dashboard card and the signal flow representation… a few visual options for both (global signal flow… its own lab)", and, when asked, "Card and Osc-page row";
- "I've updated the spec and sync is now always on so morphs can be continuous". The source was the lead's B661 trace, read in its worktree: the change is in the lead's lab and not yet in the spec on main.

**Evidence.**
- **The lab's self-check in headless Chrome:** 30 OK and none failed. The new rows:
  - GUI-64's row of four sits under the viewer on both Osc pages.
  - Each of C1 to C4 builds with 9 depths, and each of R1 to R3 with 3. A drag on each writes the shared matrix, and the card reads the row's value.
  - A fill is drawn at its value: 25 % and 75 %.
    - **Plant:** the old spaced unit read 100 % both times, red.
  - **The drag:** up and right moves the frame and Bend, and sync stays at ×1.
    - **Control:** shift sweeps sync from ×1 to ×1.74.
  - Two directions mapped to Bend add up.
  - The warp is the identity at ×1 even at 50 % softness.
    - **Control:** ×3 changes the frames, and full softness tapers its restart.
- **A real-mouse probe:**
  - the viewer drag (frame and Bend), then shift + sideways (sync ×1.49);
  - the pips and the name switch;
  - the mapping, including the two-on-one-row case, and Default;
  - a C3 bubble and an R2 bar, each dragged to 0.60, with the bar's fill at 60 %.

  No uncaught errors.
- **Screenshots** of C1 to C4 and R1 to R3 were read.
- **A stale headless Chrome** left on port 9337 by a crashed screenshot run caused earlier stale error reports. It was stopped, and the reruns were clean.

**Verify.** `./verify fast` on the commit this trace ships in.
