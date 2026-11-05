# 2026-11-05 — Each source's on switch on Main (GUI-43)

**What changed.** `layout.json`: Main's Osc 1, Osc 2, Sub and Noise tabs start with that source's on row.

**Why.** The human: "There's currently no way to turn oscillators on and off from main."

**Verify.** `./verify fast` on the commit this trace ships in. 83 shell self-checks in a real browser at 980 px, the Main tabs' fit included; all four switches are mounted on Main.
