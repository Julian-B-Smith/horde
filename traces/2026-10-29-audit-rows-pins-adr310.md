# 2026-10-29 — The audit's rows; exclusions pinned; ADR-310

**What changed.**
- `docs/armor/tolerances.json`: every one of the 13 exclusions now carries its current `value`.
- `tools/tolerance_registry_check.py`: a new control pair. A pinned exclusion left untouched reads
  green, and a pinned exclusion whose value changed reads red.
- `reference/kuramoto-views.html` moved to `docs/design/kuramoto-views.html`, with links in
  `index.html` and `docs/manual/figures.json`.
- `CLAUDE.md`: `reference/decoherence-lab.html` joins the protected list.
- `DECISIONS.md` ADR-310. ROADMAP B568 to B577, the repo audit's ten proposals, B570 done.

**Why.** The repo audit of 2026-10-29 (horde #1250) found H3: no exclusion pinned its value, so
ADR-308's `inline-float-compare` exclusion covered every future comparison in the bench. Pinning
narrows each exclusion back to the value that was ruled, which tightens the gate. The human ruled
"File all 10, fix H3 now", then on H2's two labs "Move kuramoto; protect decoherence". That was
asked a second time after the lead found `specs/SPEC-GLITCH-MODULES.md` citing decoherence-lab as
its behavioral reference.

**Evidence.**
- `tolerance_registry_check`: exit 0; "control: FIRED  a pinned exclusion untouched reads green
  (positive)"; "control: FIRED  a pinned exclusion's value changed reads red".
- The 13 pinned values are printed in the session and recorded in the JSON.

**Verify.** `./verify fast` on this tree before push (see the commit).
