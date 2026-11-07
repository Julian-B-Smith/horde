# The plan map leaves the review queue; B628's state; main's red interval

**What changed.** The plan map's `lab-review` meta is removed. ROADMAP notes are added on B387, B578 and B628. Nothing else.

**Why.** The human dropped the plan map from the lab review ("Yes, drop both") and deferred random-lab. B628's row needed its built state, and a record of main going red after #1414 until #1415.

**Evidence consulted.** The Labs facilitator's batch 4 message. `./verify fast` on the bridge follow-up, which read gui3_browser_check RED on main plus bridge-only changes: "replay: the bank didn't reach the preset bar ("Trance jitter ▾"), or next sent {"type":"preset","seq":7,"index":2}". The GUI session's #1415.

**Verify.** `./verify fast` on this tree; see the PR.
