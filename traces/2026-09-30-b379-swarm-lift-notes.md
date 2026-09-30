# b379-swarm-lift-notes — the critic's two notes on PR #876: the frozen lift set and rule 4 through headers

- **Queue item:** ROADMAP B379 step 1, fixup after the critic's ACCEPT-WITH-NOTES. It follows `traces/2026-09-30-b379-swarm-lift-rework.md`.
- **Why:**
  - **N1 (MEDIUM, confirmed by the critic).** A behaviour change could bypass `divergences.json` by being labelled `kind: "edit"`. The critic's `{"id": "E2", "kind": "edit"}` `dissolve` clamp left `h2_lift_check` GREEN.
  - **N2 (LOW).** Rule 4 scanned `tools/**/*.cpp` only, so a legacy core reached through a `tools/` header went unseen.
- **What changed:**
  - **N1:** `tools/h2_lift_check.py` holds a FROZEN set of non-divergence ids per lifted file: `NS1` and `E1` for swarm_core.h, `NS3` for force_core.h, `NS2` for glide_core.h, and `T1` and `T2` for the parity tool. Any other entry must be `kind: "divergence"` with a `divergences.json` id. There are 4 new self-cases, so the total is 17 (an unknown `edit` id is red).
  - **N1 docs:** README rule 8, the ledger's `about` and `docs/port/phase-1b.md` state that a new non-divergence edit is a deliberate re-lift: the frozen set changes together with a `src_blob`/`lifted_at` re-pin, recorded by the lead in ROADMAP.
  - **N2:** rule 4 scans `tools/**/*.h` too, and follows quoted includes between scanned files. There are 2 new self-cases: a header including a legacy core, included by a `.cpp` that also includes an h2 core, is red on the `.cpp`; a TU reaching only the legacy core is not flagged.
- **By hand, each restored:**
  - The critic's exact attack (an `E2` `edit` patch changing `max(0.01, p.dissolve)` to `max(0.02, …)` in the lifted swarm_core.h, with the matching copy edit) now turns `h2_lift_check` red: "E2 changes the copy but is neither a frozen lift entry (E1, NS1) nor kind 'divergence' …".
  - `tools/zz_leg.h` (legacy include) plus `tools/zz_via.cpp` (it, and an h2 core) turned `h2_rules_check` red with "… via tools/zz_leg.h …".
- **Evidence consulted:** the critic's notes as relayed by the lead; `tools/h2_lift_check.py`, `tools/h2_rules_check.py` and `h2/README.md` at `8ef38f4`.
- **Alternatives rejected:**
  - **Keeping the frozen list in the ledger:** the ledger is exactly what a bypassing edit would change in the same commit, so the list lives in the check, beside the rule it enforces.
  - **Requiring non-edit entries to change only `namespace`/`#include` lines by pattern:** a pattern is easier to game than a named list, and E1 is neither.
- **Verify:** `./verify fast` is run on this commit. The result is reported verbatim to the lead and in the PR #876 comment. `h2_lift_check` and `h2_rules_check` are GREEN locally before the commit.
- **Open questions:** none new. The four phase-1b decisions and the 48 kHz golden question are unchanged.
