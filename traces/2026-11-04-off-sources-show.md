# 2026-11-04 — A source that's off shows its controls; Detune on Main (GUI-29, GB10)

**What changed.** `layout.json`'s `live_when` has no source rules now, so a source's on switch hides nothing. Main's Osc tabs gain Detune. Self-check 4b6 holds both, with a blade's own rule as its control. GUI-21's reading is marked reversed, and GN-24 says what this means for its table.

**Why.** The human: "Suddenly all the parameters disappeared from the oscillators", "why is detune invisible on the oscs in main?", and "off sources should show their controls." The first was a reading of GUI-21 that went past its words, listed as an open question. Detune was never in the deck's rows, and no ruling left it out.

**Evidence consulted.** The latest shell's control counts per page before the change (Osc 1 12, Osc 2 1, Sub 1, Noise 1), GUI-4, GUI-21, and `layout.json`'s deck and `live_when`.

**Verify.** `./verify fast`, on the commit this trace ships in. The shell opened in a real browser.
