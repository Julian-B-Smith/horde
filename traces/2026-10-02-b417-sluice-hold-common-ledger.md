# b417-sluice-hold-common-ledger — one ledger and cache for every checkout, resolved through git's common dir

- **Queue item:** ROADMAP B417, a follow-up to `2026-10-02-b417-sluice-hold-proposal-c.md`. That trace's open question 1 was ruled by the lead, within the human's B415 ruling: resolve the ledger and the cache through the git common dir.
- **Why:** The ledger was resolved per checkout (`REPO/local/`), so a failure seen in a worktree run never reached the main checkout's ledger. Since the purpose is "never credit a window once flagged", the stricter reading is one ledger for all checkouts.
  - `state_dir()` resolves the main checkout's `local/`, the parent of `git rev-parse --path-format=absolute --git-common-dir`. That is the way `tools/labs_preview.sh` finds MAIN.
  - It falls back to this checkout's `local/` only when the main checkout has none. The gate line then says "in THIS checkout (main checkout has no local/ — fallback)".
  - The directory is computed at runtime and never written into a tracked file.
  - The ledger append moved into `ledger_append()`, which is append-only.
- **New control (13):** from an injected worktree layout (names only, no directory touched), `state_dir()` must:
  - resolve to the main checkout's `local/`;
  - fall back to the worktree's own `local/` when the main checkout's is absent;
  - land the plant's counted windows in the ledger at the resolved path, once (a second append adds none).
- **Scratch checks (in memory, a stand-in main checkout in the scratchpad, not committed):**
  - A planted 45-character spec piece in a worktree run FAILS. Its 18 windows land in the stand-in main ledger (7 → 25), and the worktree's ledger is untouched.
  - With the stand-in main checkout's `local/` removed, the run falls back, the summary line says so, and the ledger is created.
  - A broken resolver that always returns the checkout's own `local/` fails control 13 (12/13).
- **Housekeeping:** the worktree's own earlier ledger and cache were deleted. They were local, untracked and superseded. The main checkout's seeded ledger (7 entries) is now the one in use, and it gained no entries from these checks.
- **Alternatives rejected:** an env var or a config path. Both would be a new mechanism; the common dir is already the repo's idiom.
- **Verify:** `./verify fast` exit 0 on this change. The PR comment reports the commit hash and the verbatim gate line.
- **Open questions:** none new. Questions 2 and 3 of the prior trace were ruled to stand.
