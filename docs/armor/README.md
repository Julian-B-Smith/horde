# docs/armor — the armor catalogue

The law is [`docs/strategy/blind-spot-armor.md`](../strategy/blind-spot-armor.md) (ADR-197, B448):
every risk-register row is a named gate in `./verify`, or a visible hole on the dashboard.

## What is here

- **`catalogue.json`**: one row per armor category. That is the twelve risk-register rows R1–R12,
  the seven agent-signature rules S1–S7, and the security method's four categories (`SEC-input`,
  `SEC-webview`, `SEC-supply`, `SEC-hygiene`; B446). It is the single source; nothing else lists
  coverage.
- **`dashboard.html`**: the human's one-screen report, generated from the catalogue by
  `tools/armor_dashboard.py`. Never edit it by hand. Regenerate it and commit the result.

`./verify fast` runs `tools/armor_coverage_check.py`, which prints the armor score
(`armor: G/12 green, P partial, H hole`), and `tools/armor_dashboard.py --check`. The rules
are in the check's docstring.

## A row

| Field | Meaning |
| --- | --- |
| `status` | `guarded` (covered, no gaps), `partial` (gates exist, coverage incomplete) or `hole` (no gate) |
| `gates` | Names `./verify` or a workflow actually runs: a `tools/X.py` stem, a `"$build_dir/X"` oracle, a lab harness, a verify function, or a workflow job id or step name |
| `gaps` | Neutral prose: what is missing. It must be empty for `guarded` |
| `tracked_by` | The ROADMAP id that owns the row. Required for `partial` and `hole` |
| `expires` | ISO date. Required for `hole`. On the day after it, verify goes red |
| `tripwire` | The brief's plain-language line for the human |

**Public repo.** A security row names its category and the gates that exist, nothing more.
Specifics of any unfixed finding stay in the gitignored `local/security/`.

## Add a gate

1. Wire the check into `./verify` (ADR-180 §1) or a workflow, in the same PR.
2. Add its name to the row's `gates`. If the check is still being built in another PR, add
   `{"name": "x_check", "pending": true}` instead. A pending gate is reported, not failed, and it
   shows as missing on the dashboard. It is never allowed on a `guarded` row. Drop the flag once the
   check runs.
3. Update `gaps` and `status`, then run `python3 tools/armor_dashboard.py` and commit both files.

## Retire a hole

Land a gate for it, list the gate, set the status to `partial` (or `guarded` if nothing is left
in `gaps`), and remove `expires`.

## Extend a hole

Moving a hole's `expires` later makes it an accepted hole: a known risk carried on purpose.
**Only the human approves that.** The approval is recorded in ROADMAP.md or DECISIONS.md before the
date moves, and the PR cites it. An agent never re-dates a hole to turn verify green.
