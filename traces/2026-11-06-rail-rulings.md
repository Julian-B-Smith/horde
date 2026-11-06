# 2026-11-06 — The morph rail lab's four answers (GUI-56, GB17)

**What changed.**
- **`DECISIONS.md`: GUI-56.**
  - Rails are drawn three ways.
  - The puck scrubs the rail.
  - Per-parameter distribution is D2's share bars, in 1.0.
  - The rail is after 1.0, on every XY pad.
- **The ledger.**
  - `morph-rail-lab.html` is ruled (GUI-56).
  - Two rows: the share bars (MorphInspector) and the rail after 1.0.
  - `engine_impact` GUI-56 → GN-31.
- **`ENGINE-NEEDS.md`.** GN-31 narrows: distribution needs nothing beyond GN-6's offsets, and the rail has no 1.0 need. It cites GUI-56.
- **GB17** is RULED.
- **The design index** is regenerated. It also drops the stale review card for `h2-plan-map.html`, which the generator no longer lists on main.

**Why.** The human answered all four questions, reviewing the lab live with the Labs facilitator session, which relayed the answers with the human's words.

**Not mine to change.**
- B28 (closed by D2, per the lab) and B133 (the rail moves after 1.0) are the lead's rows. The lead has the same text.
- Whether the lab closes or stays pinned as the rail's bench is the human's call.

**Verify.** `./verify fast` on the commit this trace ships in.
