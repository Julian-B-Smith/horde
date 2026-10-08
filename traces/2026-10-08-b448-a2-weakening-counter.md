# b448-a2-weakening-counter — a ratchet that counts gate-weakening markers and fails on any unapproved increase

- **Queue item:** ROADMAP B448 (ADR-197 Blind-Spot Armor), Phase 1 Wave A item A2. Brief from the lead
  session, 2026-10-08 ("Start phase 1"). Acceptance: "(A2) The gate-weakening counter: skips,
  suppressions, disabled sanitizers and UNWIRED declarations counted against a baseline, failing on any
  increase not approved by the human."
- **Why:** the armor doc's "Quiet disabling" and "Symptom clamping" failure modes are only caught by a
  counter, not by prose. `tools/weakening_check.py` counts six marker categories (skip, suppress,
  sanitizer_off, soft_fail, unwired, dsp_guard) per (category, file) against
  `docs/armor/weakening-baseline.json`; an increase is red until the human's ref is recorded with
  `--approve`. Wired in `fast` right after `banned_api_check`, as its own block (ADR-180 §1).
- **Evidence consulted:** `docs/strategy/blind-spot-armor.md` (Agent-signature failure modes, human's
  dashboard); `verify` lines 60-200 (wiring and comment style); `tools/banned_api_check.py` (house style;
  its `strip_noncode` is reused so dsp_guard counts code, not prose); a grep of every pattern over the
  scope before baselining.
- **Alternatives rejected:** a copy of the C++ lexer (imported the existing one instead); counting
  `-fno-sanitize*` as one prefix (it would count `-fno-sanitize-recover=all`, the STRENGTHENING flag in
  `tools/ncap_check.cpp`, so it is excluded by lookahead); an `--init` that overwrites (it refuses when a
  baseline exists, so it cannot be a second way to raise).
- **Additions beyond the brief's list, same intent:** `detect_leaks=0` and `-fsanitize-recover`
  (sanitizer_off; the former is live in `tools/sanitize_oracles.sh`), MSVC `#pragma warning(disable`
  (suppress), `it|test|describe.skip` for the .mjs oracles (skip), `|| :` and `if: ${{ false }}`
  (soft_fail). All have zero hits except `detect_leaks=0`, which is baselined.
- **Verify:** `./verify fast`, exit 0, git 13e9035 (`.harness/last-verify.json`). Summary line:
  `weakening: 78 markers across 6 categories, 0 increases`.
- **Open questions:** (1) the baseline counts are text matches: `tools/test_table_check.py` carries 7
  `UNWIRED:` because it documents the rule, not because it declares seven unwired checks; the human may
  want those to be a lower number. Tightening them would be a decrease, which this check welcomes.
  (2) a sanitizer option split across lines in a YAML env block is matched by its own spelling, not by
  the variable name; a value assembled by a macro or shell variable is not seen.
