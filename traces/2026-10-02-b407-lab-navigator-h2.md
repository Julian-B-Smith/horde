# b407-lab-navigator-h2 — lab navigator lists only the labs horde 2's plan uses, grouped by plan part

- **Queue item:** ROADMAP B407 (brief from the horde lead session, 2026-10-02; human ask: "refresh the lab navigator and only link the relevant labs").
- **Why:** The navigator was a flat list of every lab with stale review pills. Relevance is now each lab's own claim (`lab-part`, `lab-archive`, existing `lab-superseded-by`), read by `tools/gen_lab_index.py`, so no list of lab names lives in the generator. Plan-part order is owned by the generator (`PARTS`) because it is the plan's order.
- **Evidence consulted:** `tools/gen_lab_index.py` docstring; `docs/design/*.html` heads (41 tracked labs); lead's brief assignments; lead's mid-task correction (current-lab count 22, not 23: 3+3+1+3+2+2+5+2+1 = 22, 22 + 19 = 41).
- **Result:** 6 pinned for review, 22 current (0 unsorted), 19 archived (3 superseded + 16 tagged). Review labs are shown pinned AND in their own part (parts are a complete map; the pin is a to-do list; only the pinned copy carries review styling). Two consecutive generator runs are byte-identical (cmp); no wall clock, labs sorted by filename.
- **Alternatives rejected:** pinned-only review labs (parts would have holes); a name list inside the generator (rots, contradicts the generator's own design); an unknown `lab-part` silently dropped (instead it falls to "Unsorted").
- **Verify:** `./verify fast`, exit 0, git c12ae6f (`.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"c12ae6f","ts":"2026-10-02T00:53:44Z"}`).
- **Open questions:** none. `lab-review` was removed from 12 labs on the brief's say-so ("built, merged and seen"); I did not re-check that against ROADMAP.
