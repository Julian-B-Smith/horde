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

## 2026-10-11, later: rework after the critic's review of PR #1057

Nothing above is rewritten. Where this section and the text above disagree, this section is the
later record. Two statements above no longer hold: "Entries already in the baseline are not re-judged"
is now true only of entries that carry the initial-baseline text, and "the rule lives on `--approve`"
is no longer so (see M1).

- **Queue item:** B448 phase 2, package P14, the same PR. The brief's acceptance items were the
  critic's findings (H1, H2, M1, M4 and a list of smaller ones), not text quoted from ROADMAP.md.
- **Why:** the critic showed two ways a tag could let an untagged clamp in with no trace, a rule that
  the oracle itself never ran, and four mutations of the tool that its own controls did not catch.
- **What changed** (`tools/weakening_check.py` only; no ledger, no `verify`, no catalogue row):
  - **H1, a move leaves no headroom.** Tagging an existing guard moves it between the two categories,
    and the category it left kept its old allowance. `--approve` in either `dsp_guard` category now
    also lowers the other category's entry for the same file to the tree's count, removing it at
    zero, in the same write, and prints that it did. The brief asked for the tagged direction; the
    mirror (tags removed, approved under a trace, the tagged allowance left behind) is the same
    hole, so both are closed. The lowering never raises.
  - **H2, a retag hiding a new clamp.** No rule was added, because none can work: see the next
    bullet. Instead the red line and `--approve` state the most new untagged guards the counts
    could be hiding, and `--approve dsp_guard_tagged` prints every guard line in the file, tagged
    and untagged, with both categories' ledger and tree numbers and one plain sentence that the
    tool cannot tell which guard is new.
  - **Why no rule separates the mixed case.** For one file, write the change as (untagged, tagged)
    relative to the ledger. One new tagged guard is (0, +1). One old guard retagged plus one new
    untagged clamp is also (0, +1), from the same ledger. A counter that sees two identical states
    must give them the same answer. Asking that state for a trace would ask every honest new tagged
    guard for one, in every file that already holds an untagged guard (all ten files in today's
    ledger), and edit D exempts those. The exception that can be proved is used: where the ledger
    allows no untagged guard in the file there was nothing to retag, and the "may hide" number is 0.
    The five cases and the formula are in the docstring under RETAG OR NEW, one control per row;
    the controls for the two indistinguishable rows expect the same reading on purpose.
  - **M1, the rule is in the oracle.** Every ordinary run applies the trace rule to each `dsp_guard`
    ledger entry whose `approved` text is not exactly the initial-baseline text. A reference typed
    in by hand, a trace that is not in the tree, and a trace deleted later are red.
  - **M4, four surviving mutations.** Controls added: an untagged guard directly above a tagged one;
    `dsp_guard: fine`; a comment that only says "boundary"; a trace with "root cause" only in prose.
  - **Smaller items.** In C/C++ the tag counts only inside a comment that opens on the marker's own
    line (a string literal, code, and a block comment opened on an earlier line tag nothing). A
    heading inside a code fence is not a root-cause section. The trace must name the whole path,
    not a longer path that contains it. `--approve` says so prominently when one reference raises
    an entry by more than one, and says the dashboard must be regenerated when the ledger's category
    count changes (it does not regenerate it). `approved()` returns two values again.
- **Evidence consulted:** the brief; `tools/weakening_check.py` and `tools/banned_api_check.py`
  (`strip_noncode`) in full; `docs/armor/weakening-baseline.json` (read only: ten `dsp_guard`
  entries, all ten carrying exactly the initial-baseline text, no `dsp_guard_tagged` entry);
  `tools/armor_dashboard.py` (`optional_input` embeds the number of top-level ledger entries).
- **Alternatives rejected:** adding a comment-keeping mode to `strip_noncode` (the right home for it,
  but that file was out of scope; the tool asks the existing lexer whether an offset is in code
  instead); a second small C lexer in this file (a copy to keep in step); storing which lines are
  guards in the ledger, or reading them from `git diff`, to tell a new guard from an old one (a
  ledger schema change, and a result that depends on which base is fetched); requiring a trace
  whenever the counts could be hiding a new untagged guard (it would demand a trace from honest
  tagged guards, which is the reading the lead asked to keep).
- **Verify:** `./verify fast` exit 0 on 6a77eb6 with this change staged, recorded in
  `.harness/last-verify.json`; `weakening: 80 markers across 6 categories, 0 increases`, the ledger
  byte-identical before and after (same SHA-256). On the real tree the trace rule was applied to
  ledger entries zero times, measured by counting the calls. Measured on scratch copies: 36
  mutations of the tool, each breaking one rule above, 36 caught by the controls; and `main()` run
  end to end on a scratch tree and scratch ledger for H1, H2, M1 and the several-guards line.
  `--approve`, `--tighten` and `--init` were never run against the real ledger.
- **Open questions:**
  - H2 is narrowed, not closed. In a file that already holds an untagged guard, a retag plus a new
    untagged clamp is approved with no trace if the human does not read the printed lines.
    Closing it needs something a counter does not have: which lines are guards, kept in the ledger.
  - M1 trusts the initial-baseline text. A count raised by hand under that text is not seen here;
    `ledger_owner_check` and code-owner review are the guard.
  - M1's wiring into `main()` is covered by the end-to-end scratch run, not by a control that runs
    on every `./verify`: the controls call the function, and `main()` calls the same function.
  - A tag in a block comment that was opened on an earlier line is not honoured. That is stricter
    than "in a comment on that line"; the docstring says so.
  - Carried over, still open: the wording of the control against what is built, the widened
    patterns, and the lead's edits to `docs/armor/README.md` and catalogue row S1.
