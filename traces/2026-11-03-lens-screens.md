# 2026-11-03 — The Lens lets go, and every screen follows the colour (GUI-13, ADR-121)

**What changed.** Master at 1793745705-bf68: the Lens's empty state, the Specimen reads its ground from its canvas, and `watch` sees `data-scr` below the root. Shell: `colourKey` in the five pictures' change tests, a press off a parameter clears the selection, the Lens has no fallback. Self-checks 4i and 4j.

**Why.** The human: "I think clicking off of a parameter should remove it from the lens", and "not all screens follow the screen color selection", with a picture: a yellow phase ring beside a lavender envelope, cycle and Specimen.

**Evidence consulted.** `docs/design-system/master/components/ScreenScheme/README.md` (ADR-121: every glass, well, the XY pad and the Specimen follow); the master's `Lens`, `watch` and the Specimen's frame; the shell's change tests.

**What the check caught.** 4j against the old bundle: the Specimen's corner read 28,7,53 against a ground of 255,240,228. With the scheme taken back out of `colourKey`, it listed Main's envelope and cycle and the Dashboard's and Osc 1's envelopes as stuck, the human's picture.

**Also found.** The shell's own self-checks left a parameter selected and the info box locked on every load, which is why it always opened on Detune. They now clear what they pressed.

**A gate changed, the same strength or stronger.** `gui3_seam_check`'s `canvases` looked for the old key's literal text. It now looks for `colourKey()` beside `sizeOf`, with the same count, and adds a test and a plant that the key counts the scheme.

**Not checked.** A plugin host, and every scheme: 4j compares TUBE and EMBER only.

**Verify.** See the PR.
