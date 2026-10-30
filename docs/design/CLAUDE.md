# GUI session charter — horde design and GUI 3

Chartered by ADR-336 (the human, 2026-10-30). The root `CLAUDE.md` still applies in full; this file adds the GUI session's scope and its one rule: **every design decision travels one chain, and every hop is recorded.**

## The chain of custody

ruling (an ADR in `DECISIONS.md`) → **design master** (`docs/design-system/master/`, published to the Design System artifact) → **vendored bundle** (`docs/design/arp-face/`, stamped with the master's version) → **GUI 3 shell** (`docs/design/gui3.html`, `docs/design/gui3/`) → **engine** (the stub today; the WebAssembly engine at stage 8, IF-10).

- **`docs/design/design-ledger.json` is where the chain is recorded.** One row per ruled design decision: its ADR and clause, the lab it came from, the master component and the version that holds it, its place in the shell, and its state (ruled, in master, vendored, in shell, wired to the engine). Read it before starting anything, and update it in the same PR as the hop it records.
- **The port ledger** (`docs/design/gui3/port-tab.json`, B593) is the view of what has not reached the shell.
- **The master is edited in the repo, never in the artifact.** Change `docs/design-system/master/`, merge, publish the artifact from the merged tree, then re-vendor with the new version stamp. Each master change names the ADR it carries out. A ruling not yet in the master cannot reach the shell (ADR-316): add it to the master first, never draw it locally.
- **Labs are research benches** (ADR-316). Each carries a status in the design index: exploring, ruled (ADR), harvested (master version) or retired. A retired lab is history, never a source. A new lab is started only for a question the human has asked, and it ends in a ruling or a retirement.

## Scope

**Owns:** `docs/design/**`, `docs/design-system/**`, the Design System artifact, and these ROADMAP rows (only this session edits them): B261, B532, B555, B556, B557, B581, B582, B583, B584, B589, B591, B593, B594, B595, B596, and any row the lead hands over by a note in that row.

**Does not touch:** the engine (`engine2/`, `h2/`, `src/`), specs, references, goldens, the clean room, the mailboxes (`integrations/`) or Mangle. When GUI work needs one of these (a parameter row, an engine interface, a sibling's manifest), add a note to the GUI row saying what is needed and from whom; the lead picks it up at its next boundary. Never edit a lead-owned row.

## Shared files

- **`DECISIONS.md`**: one log. Take the next ADR number when you commit. If the lead's PR lands the same number first, `./verify` goes red on the duplicate and the later PR renumbers.
- **`ROADMAP.md`**: edit only your rows. New rows take the next free B number, under the same duplicate rule.
- **`./verify`** is shared. Adding a check is allowed (ADR-180 §1); weakening one needs the human.

## Standing rules

Everything in the root charter, plus:
- **Never merge.** PR per batch; the human merges.
- **Never commit machine identity or a private sibling's name** (the root charter's alias rule; run the leak check on every diff).
- **Present each batch visually**: screenshots or the labs preview, before code.
- **Rulings are the human's.** Variant sheets end in a poll, and the answer becomes an ADR.
- **Models are pinned on every agent**: Opus for judgment (design, critique), Sonnet for scoped building.
