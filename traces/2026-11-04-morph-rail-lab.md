# 2026-11-04 — A lab for the morph rail and per-parameter distribution (GB17)

**What changed.** `docs/design/morph-rail-lab.html`, new. The master's pad and slider, with a rail drawn three ways (points, Bézier handles, by hand) and a strip of what the path passes through, using real rows. Distribution as three options: boundaries only, share bars, or a sharpness per row. It adds a `labs` entry to the ledger and row GB17. Nothing in `gui3*`, `arp-face/` or the master changes.

**Why.** The human: "Finish the Morph page", then "Yes, build the lab". GUI-28 left the rail and distribution with no design ruling.

**Evidence consulted.** B133 and its design notes, ADR-137's amendment ("rail wins"), B28, GN-6 and GN-31, and the shell's owner stand-in in `gui3/device.js`, which the lab copies rather than MO-7's law.

**A check corrected while building it.** The first travel test used a rail with a sharp kink, where a straight-line step can't measure travel round the bend, so it read 1.30 against a limit of 1.12. The limit stayed. The test rail is now smooth with unevenly spaced points, the case the arc-length table exists for: 1.06 against 20.3 for the control.

**Verify.** `./verify fast`, on the commit this trace ships in. The lab opened in a real browser, with real clicks on its pad, switches, a share divider and a boundary.
