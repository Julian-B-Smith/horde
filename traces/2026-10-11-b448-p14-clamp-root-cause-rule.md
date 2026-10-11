# b448-p14-clamp-root-cause-rule — a `dsp_guard` approval must cite a root cause; a tag picks the approval, it never replaces one

- **Queue item:** B448 phase 2 Wave 1, package P14 (ADR-209; `docs/strategy/blind-spot-armor-phase2.md`
  §1 row S1 and §3 row P14). Sits on PR #1054.
- **Why:** the law says a new clamp or isfinite guard in DSP code needs a trace explaining the root
  cause (`docs/strategy/blind-spot-armor.md`, "Symptom clamping", and edit D). `weakening_check` already
  counted such guards, but `--approve dsp_guard FILE REF` took any non-empty REF, so an approval could
  be recorded with no cause named.
- **What changed** (`tools/weakening_check.py` only; no ledger, no `verify`, no catalogue row):
  - `--approve dsp_guard FILE REF` now refuses unless REF contains `traces/<name>.md`, that file is
    readable, it names FILE, and it has a root-cause section: a heading of any level whose text starts
    with "Root cause", followed by at least one non-blank line before the next heading. The tool prints
    which rule let the approval through. Entries already in the baseline are not re-judged.
  - A new counted category, `dsp_guard_tagged`: the same markers on a line that carries
    `dsp_guard: boundary` or `dsp_guard: latch` in a comment on that same line. A tagged marker moves
    from `dsp_guard` to `dsp_guard_tagged`; it is still an increase, still red, still the human's to
    approve. Its approval needs a non-empty REF and no trace, and `--approve` prints each tagged line.
  - Self-test rows for every rule above, each on a scratch repo and a scratch ledger.
- **The design question inside the package.** The proposal's control reads "a tagged boundary guard is
  green". Read literally, a tag would make a guard uncounted, and an agent could add a clamp, write
  "boundary" beside it and need nobody. Three designs were weighed against edit D ("boundary input
  validation is exempt from the symptom-clamping rule; output guards latch and report") and the control:
  1. *The tag exempts the line from counting.* Meets the control's words. Rejected: it is the
     self-approval path.
  2. *Tagged guards are their own counted category* (built). Edit D's exemption is from the root-cause
     rule, and that is exactly what the tag buys: no trace. It does not buy silence. This also matches
     row S1's own exit wording, "a `dsp_guard` approval must cite a trace with a root-cause section, or
     a boundary or latching tag": the tag is an alternative citation for an approval, not an
     alternative to one. Exact with no ledger schema change: an increase shows up in the category whose
     rule applies to it.
  3. *A registry of host-entry files in which a tag is honoured without approval.* Rejected here: inside
     a registered file it is design 1 again, and the registry would be a new ledger.
  The control is therefore met as "a tagged boundary guard goes green on the human's approval with no
  trace", not as "green with no approval". That second reading is not built, on purpose.
- **Measured before designing** (this tree, `src/` and `h2/`, C/C++ matched on code only). Counted today:
  30 markers in 10 files. Candidates: a lone `min(` 227 and a lone `max(` 363 (useless: ordinary
  arithmetic); a min whose argument is a max call, or the reverse, 79 in C/C++ (45 `src/`, 34 `h2/`);
  any min containing a max anywhere in its arguments 93 in C/C++ (the 14 extra are arithmetic) and 47
  in GUI script; ternary clamps
  45 (38, 6, and 1 in GUI script); `if (x > hi) x = hi;` 47 (18, 25, 4 GUI) once envelope stage changes
  are excluded; `tanh(`/`atan(` 35 (24, 8, 3 prose) with `atan2` excluded (20 uses, all phase angles);
  home-made `*clamp*(` helpers 31 (26 `src/`, 5 GUI shader); `isinf(` 2; `copysign(` 0; an `fabs`
  comparison followed by an assignment 6, none of them a clamp. The earlier trace
  `traces/2026-10-10-host-value-guards.md` records a change that reused a clamp helper and so did not
  raise the count, which is the hole the helper pattern closes.
- **Held back, not in this PR:** the widened pattern set. Recommended: the wide spellings counted in
  C/C++ code under `h2/` only (legacy gets no new gates, ADR-209 item 1). On this tree that is 75 newly
  counted lines in 9 files (22 in the product engine `h2/engine/`, 49 in the two test-reference cores
  under `h2/cores/`, which are never edited, and 4 in the offline harness), so the check goes red until
  the human approves a one-time re-baseline. It was handed to the lead as a patch with the 75-line table. Its control was run on a scratch copy of the
  tool against a scratch ledger: the real tree read red against the real ledger (9 increases, all
  `dsp_guard`), green against the scratch re-baseline, and red again with one nested min/max planted in
  an `h2/` file, which the five original spellings alone did not see.
- **Evidence consulted:** the proposal §1 (S1), §3 (P14), §5; `docs/strategy/blind-spot-armor.md`
  (the failure mode and edit D); ADR-209 in `DECISIONS.md`; `tools/weakening_check.py` in full;
  `tools/banned_api_check.py` (`strip_noncode` keeps offsets and newlines); `docs/armor/README.md`;
  `docs/armor/weakening-baseline.json` (read only); every newly counted `h2/` line in context.
- **Alternatives rejected:** storing a tagged count inside each baseline entry (a ledger schema change,
  and more plumbing than a category); finding "the new lines" with `git diff` (depends on which base
  is fetched, so not reproducible); validating the refs of existing ledger entries in verify mode
  (the brief says existing entries are not re-judged, and the one-time re-baseline would trip it);
  a single widened regex over `src/` too (151 more lines of frozen code for the human to read).
- **Verify:** `./verify fast` exit 0, recorded in `.harness/last-verify.json`, on c7dc9ab plus this
  change staged; `weakening: 80 markers across 6 categories, 0 increases`, the ledger unchanged. Each
  new self-test row was shown red by breaking its rule in a scratch copy of the tool (14 mutations, 14
  caught); the tool's `main()` was run end to end in a throwaway repo (plant red, bare ref refused,
  trace accepted, tagged guard red, then approved with no trace).
- **Open questions:**
  - Does the human accept the control as built (a tag changes which approval is needed, not whether)?
  - The widening: `h2/` only as recommended, or `src/` too? And which ref will the one-time
    re-baseline cite? Under the new rule it must be a trace with a root-cause section that names each
    file; the lead's re-baseline change should carry that trace.
  - The rule lives on `--approve`. A ledger edited by hand skips it; code-owner review is the guard.
  - The tool cannot tell a true root cause from a written one, or whether a `latch` guard latches.
  - `docs/armor/README.md` and catalogue row S1 still need the lead's edit (out of this brief's scope).
