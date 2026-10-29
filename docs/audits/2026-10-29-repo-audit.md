# Repo audit — 2026-10-29

**Commit audited** `1119dc5a56280ff9ac6a8ce285c3340bfe71ac08` (`origin/main`, PR #1248 merged).
**Cause** The auditor cadence (CLAUDE.md §Domain "Auditor cadence", B159, ADR-179 §3), dispatched
by the lead at `/wakeup` step 4b on 2026-10-29. The previous repo-wide audit is
`docs/audits/2026-10-21-repo-audit.md` (`900963c`), 8 days old against a 7-day cadence.
**Scope** The whole tree, read-only, hardest where the lead asked: filter lab round 3 (B566:
`docs/design/filter-lab.html`, `tools/filter_cost_bench.cpp`, `tools/labharness/filter_fidelity_check.mjs`),
the tolerance registry's two new exclusions (ADR-308), the admission test (`tools/admission/`,
`tools/admission_check.py`, B561), ROADMAP rows B555 to B567, ADR-289 to ADR-308, and README's
"Last verified" line.
**Oracle** A full `./verify` was not run (the lead runs it). What ran here, each under `nice -n 10`
and single-threaded unless stated, load average 2.6 to 4.4 on 8 cores:
`filter_fidelity_check.mjs` (GREEN), the cost bench built `clang++ -std=c++20 -O3` and run twice,
`filter_fidelity_check.mjs --parity` (GREEN), `admission_check.py` against the module's working tree
with `HORDE_ADMISSION_JOBS=4` (GREEN), `test_table_check`, `tolerance_registry_check`,
`golden_registry_check`, `playbook_check` (all GREEN). The admission run wrote only its own
`build-admission/` (ignored by the repository).
**CI evidence** GitHub Actions run 40787685515 (`main`, the merge of PR #1243), read with
`gh run view --log`.

Every finding is a **proposal for the lead**. Nothing outside this file was edited. Nothing here
proposes weakening a gate. A delta on a protected path is labelled as a sanction the human would
have to give.

**Rules of evidence.** MEASURED: a command ran today and printed the number. BY READING: the claim
follows from the cited lines and was not executed. Every count below was produced in this session.
The clean-room repositories were not read at all; their state is cited from horde's own records.
One sibling tool (the arpeggiator project's `tools/core_params_check.py`) was read, not run.

## The findings at a glance

| id | rank | class | one line |
|---|---|---|---|
| H1 | HIGH | storage: one copy (recurring) | The off-machine copy of the clean-room engine stopped at stage 7 (2026-10-24); stage 8's acceptance and spec rounds 34 to 47 sit on one disk, 12 and 15 commits ahead, against B533's own rule |
| H2 | HIGH | prose asserting a relationship · a hand list beside a glob | The protected references are guarded by prose only: 7 of the 19 entries in `reference/` are read by no tool, 2 are missing from CLAUDE.md's list, and no gate or armor row covers an unsanctioned edit |
| H3 | HIGH | an exclusion with no expiry · a control that cannot fire | None of the tolerance registry's 13 exclusions pins its value; ADR-308's second exclusion silences every present and future float comparison in the bench file, wider than the two constants ruled; the pinning path has no control |
| H4 | HIGH | green when it never ran (carried, third audit) | CI's sanitize job is still green with 19 oracles not built; B256 has no status at all |
| M1 | MEDIUM | stale record · truth contract | 42 ROADMAP rows have four cells under a three-column header, so GitHub drops their status; B535, B536 and B561 read open though merged; ROADMAP +71.7 KB in 8 days |
| M2 | MEDIUM | doc drift (carried) | README is "Last verified 2026-10-17"; outside the rebuild table four statements are false, one contradicting a paragraph three lines below it |
| M3 | MEDIUM | a number nobody measured · second copy | The lab's cost figures for B303 read 1.5 times what the bench measures today; they are hand-typed twice; the transcription parity they rest on is not gated |
| M4 | MEDIUM | a gate whose purpose moved | ADR-307 moved the filters to Weir; no horde file the move touches says so, and the fidelity gate's fate is unrecorded |
| M5 | MEDIUM | green for the wrong reason (latent) · one rule, two copies | The admission check does not judge a pinned row's controls but prints "every control fired"; A19's 2 KiB is a literal the contract has already amended for Shriek |
| M6 | MEDIUM | reachable but unrecorded · insertion into positional data | ADR-306's slot state has a save rule but no undo or branch rule; the note processor's per-corner record is outside RS-6.12's budget; Arp's append-only value lists are enforced nowhere |
| M7 | MEDIUM | prose asserting a relationship (carried) | Spec provenance (B534) is still open: 31 spec rounds and 13 requirement ids have no entry; SPEC-ENGINE's status line is dated 2026-10-16 |
| M8 | MEDIUM | doc drift and harness (carried) | CLAUDE.md §Domain keeps its 8 false statements (third audit); the nightly has never run; the verify record still holds `"gates":[]` |
| M9 | MEDIUM | stale record | SESSION.md's carried section gives the next ids as B544 and ADR-271; they are B568 and ADR-309 |
| L1-L8 | LOW | stale comments, ordering, consolidation, carried | "twelve filter types"; a cited tool that never existed; ADR order; a design system vendored under one lab's folder; plants caught by an exception; the glob, CMake and playbook carries; B564's pattern; cadence |

---

## CRITICAL

**None.** No finding shows wrong audio shipped, a violated invariant in a shipped product, or an
irreversible loss that has happened. H1 is again the nearest: a loss that has not happened, with
one disk as its only guard.

---

## HIGH

### H1 — The off-machine copy stopped at stage 7; stage 8 and rounds 34 to 47 are on one disk

**Claim.** The 2026-10-21 audit's H1 was closed by B533: two private remotes, and a rule for what
follows. That rule has not been kept since 2026-10-24.

**Evidence (BY READING horde's records; the clean-room repositories were not read).**
- B533, `ROADMAP.md:7546`, DONE 2026-10-21: "After each later stage acceptance, the lead hands the
  human the two push commands and **records the heads here**." The last heads recorded are
  "**2026-10-24: stage 7 accepted and pushed:** engine `bb6b815`, tests `6b9f7ff`".
- Since then: stage 8 accepted (ADR-283, `DECISIONS.md:9410`, 2026-10-25); the bank stage began
  (ADR-284, 2026-10-26); spec rounds 34 to 47 were delivered (ADR-280 to ADR-304), and README's rebuild row
  (`README.md:405`, checked 2026-10-29) says rounds 42 to 47 are "built in the builder's repo". None of these heads is in B533.
- `SESSION.md:26`: "Pushes owed by the human: cleanroom-engine (12 commits ahead) and
  cleanroom-tests (15)."
- `SESSION.md:41-42`, carried from 2026-10-22: "Local-only clean-room commits: engine `72e7fe5`
  (check 12.8), tests `e13b280`; the human pushes both repos." B533 records those two as pushed on
  2026-10-22, so the carried line is itself stale (M9).

**Why it matters.** The same exposure as last time, narrower: the accepted stage 8 (the WebAssembly
stage the labs will run) and fourteen spec rounds of tests exist on one disk. The rule that closed
H1 depends on a step at each acceptance, and nothing checks that the step happened.

**Minimal delta.** For the human: the two pushes. For the lead: the heads recorded in B533, and one
line in the session roundup, printed every session until it reads zero: "clean-room commits not on
origin: engine N, tests M" (a count of commits on each repository's main that its origin lacks).
No horde gate is involved, and none is weakened.

---

### H2 — The protected references are guarded by prose only

**Claim.** CLAUDE.md says of the prototype HTMLs: "they ARE the reference — an edit there is a spec
change" (`CLAUDE.md:166-185`). README says `reference/` holds "the spec-in-code prototypes the
parity oracles extract from — protected" (`README.md:574`). ADR-155 (`DECISIONS.md:4863`) made
`reference/` the folder of protected paths. No gate checks that a protected file is unchanged, and
for seven of them no tool reads the file at all.

**Evidence (MEASURED, a recursive grep for each entry's file name over `tools/` and `verify`).**

| `reference/` entry | tools or gates that read it | on CLAUDE.md's list |
|---|---|---|
| `decoherence-lab.html` | **0** | **no** |
| `kuramoto-views.html` | **0** | **no** |
| `distortion-engine.html` | **0** | yes (WARP, parked) |
| `formant-pulsar-fof.html` | **0** | yes |
| `horde-module-macros.html` | **0** | yes |
| `network-lab-v0.html` | **0** | yes |
| `maw/` | **0** | yes |
| the other 12 (`swarm*`, `station`, `subosc`, `intent-bus`, `gravity-modulator`, `scalpel/`) | 1 to 24 each | yes |

- `.claude/hooks/pretool-deny.sh` and `.claude/settings.json` carry no rule for `reference/` or
  `specs/` (MEASURED: the settings' deny and ask lists hold no such pattern).
- `docs/armor/catalogue.json` has 23 rows (R1 to R12, S1 to S7, four SEC rows). None is about an
  edit to a protected path. Blind-Spot Armor makes every risk "a named gate in `./verify` or a
  visible hole on the dashboard"; this one is neither.
- `README.md:574` says every entry is what a parity oracle extracts from. 7 of 19 are not read by
  any tool. That is prose asserting a relationship.
- CLAUDE.md lists the protected files by hand; ADR-155 and README protect the folder. The two files
  missing from the hand list have been in `reference/` since the folder was made (`63f0c9d`,
  2026-09-07): the "hand list beside a glob" gap.

**Why it matters.** For the twelve files a parity oracle reads, an edit shows up as a parity diff.
For the other seven, and for every protected spec (`specs/SPEC.md`, `SPEC-EFFECTS.md`,
`ACCEPTANCE.md` and the rest), an edit passes every gate. The charter's human gate is then held
only by the agent remembering it.

**Minimal delta (additive, ADR-180 §1).** One declaration, `docs/armor/protected.json`: each
protected glob, the SHA-256 of every file it matches, and per changed file a `sanction: <ADR-id>`
field. A check of about 60 lines in `./verify fast` is red when a hash moves without a new sanction
entry, with two controls (a planted one-byte edit in a scratch copy must go red; the same edit with
a sanction must go green). One armor row for it. Pointing CLAUDE.md's protected-path paragraph at
that file instead of listing the files is **a sanction the human would have to give** (CLAUDE.md is
the human's). The two unlisted files need the human's word either way: protected or not.

---

### H3 — The tolerance exclusions pin nothing, and ADR-308's second one is wider than its ruling

**Claim.** `tolerance_registry_check` can pin an excluded candidate's value: `judge()` compares
`x["value"]` when an exclusion has one (`tools/tolerance_registry_check.py:470-476`). No exclusion
has one, and no control exercises that path. So an excluded candidate's value can change and the
check stays green. ADR-308's file-wide exclusion makes that concrete.

**Evidence (MEASURED, each exclusion re-extracted with the check's own `extract()`).**
- `docs/armor/tolerances.json`: 138 registered entries, **13 exclusions, 0 with a `value`**.
- Today's values of the 13: the six ACCEPTANCE prose items (`±3`, `≥3`, `>0.5`, `≥ 0`, `< 0`,
  `≥5 s`); `filter_cost_bench.cpp::AA_EPS` `1e-5`; `filter_cost_bench.cpp::inline-float-compare`
  `>=0.1 | <0.5`; `gen_gui_controls.py::inline-float-compare` `>=0.5`;
  `h2_engine_parity_check.cpp::floorEps` `0 | 0`; the two stream headers' `eps` `0` and `0`;
  `measure_modsources.cpp::eps` `0.06 | 0.06`.
- `inline-float-compare` is one candidate per FILE: "the NON-ZERO FLOAT literals in comparison
  position" (`tools/tolerance_registry_check.py:26-30`). ADR-308 (`DECISIONS.md:9957`) rules on
  "two of the filter lab's own DSP branch points" (`>= 0.1` and `< 0.5`,
  `tools/filter_cost_bench.cpp:125` and `:265`). The exclusion (`docs/armor/tolerances.json:1291`)
  covers those two **and any float comparison added to that file later**, which would read as
  already excluded.
- The `controls()` list (`tools/tolerance_registry_check.py:553` on) has no case "an excluded value
  changed", and with no exclusion carrying a `value` the pin branch cannot fire.
- Four of the 13 sit in files a gate builds or runs: `h2_engine_parity_check.cpp`; the two
  stream headers, included by the parity checks and the fuzz and RealtimeSanitizer probes; and `gen_gui_controls.py`, which `verify` runs. A change to `floorEps`'s initial
  value or a stream's default fault size is invisible to S3's gate.

**Criterion violated.** Sweep item 1 ("a gate whose exclusion outlived its ruling") and item 6 (a
path with no control proven to trip). The ruling covered two values; the record covers a file.

**Minimal delta (strengthens the gate; not gated, ADR-180 §1).** Add `"value"` to all 13
exclusions, set to today's extraction (the list above), so a moved excluded value is red and needs a
new human ruling like any other. Add one control: an excluded value edited in a scratch copy must go
red. About 15 lines. ADR-308's text needs no change; the record then matches it.

---

### H4 — CI's sanitize job is still green with 19 oracles not built (third audit)

**Evidence (MEASURED, run 40787685515).** Shard 1: "27 oracle(s) passed, 11 not built on this
platform (state_check statefix_check bank_check penv_check morphlayout_check lfoenv_check
modreadback_check paramclass_check hostile_events_check routing_check subosc_check)". Shard 2: "27
oracle(s) passed, 8 not built (undo_check anchor_check preset_pitch_check polarity_check
offcorner_check load_handoff_check bridge_utf8_check intent_check)". Both: **GREEN**. 19, the same
as 2026-10-21 and one more than 2026-10-10.
- B256 (`ROADMAP.md:7211`), the fix row: its status cell is "§ B240 · § B190". It carries no status.
- The 2026-10-21 ratchet (fail above 19 not built) was not taken up.

**Minimal delta (unchanged).** The ratchet, about 5 lines in the sanitize driver, then B256. The set
not built is still the set that parses outside text.

---

## MEDIUM

### M1 — ROADMAP: 42 rows hide their status on GitHub, and closed rows read open

**Evidence (MEASURED).**
- The table header is three columns, `| # | Item | Status |` (`ROADMAP.md:7041`). **42 rows** carry
  four cells, the third a "§ refs" cell: B93, B94, B95, B172, B177, B351, B355, B517, and every row
  from B529 to B562. GitHub's table rule ignores a cell beyond the header's count, so on GitHub the
  Status column of those rows shows the refs and the real status is not shown. Example: B534
  (`ROADMAP.md:7547`) renders with status "§ B518 · § B494"; its "OPEN 2026-10-21" is dropped.
  B563 to B566 went back to three cells; B567 (`:7580`) folds the refs into the status cell.
- Rows that read open after their PR merged:
  - B535 (`:7548`) "OPEN 2026-10-21 **2026-10-24: built** (PR #1208)"; #1208 MERGED 2026-10-24.
  - B536 (`:7549`) the same; PR #1209 MERGED 2026-10-24.
  - B561 (`:7574`) "**BUILT 2026-10-27, PR #1232** (draft) ... Closes on merge plus a `verify full`
    run". #1232 MERGED 2026-10-27 and is not a draft. The admission step of `verify full` ran green
    today (this audit); whether a whole `verify full` has run is not recorded.
  - B516 (`:7581`) "Started: 21 rows"; `docs/port/cleanroom-rulings.md` has **82** numbered rows.
  - B524 (`:7589`) still OPEN; the 2026-10-21 audit (M2) showed its acceptance met.
- Status words in use on rows B520 to B567: OPEN, BUILT, ANSWERED, POST, PUBLISHED, PLANNED, IN
  FORCE, IN PROGRESS, DONE. No closed vocabulary, and no `roadmap_check` (carried).
- Size: **1 966 848 bytes**, +71 716 (+3.8 %) since 2026-10-21. 564 rows, 562 ids: **B147**
  (`:7132`, `:7134`) and **B155** (`:7142`, `:7143`) are still duplicated (fourth audit). The
  largest rows: B448 34 421 bytes, B50 28 855, **B494 22 281** (13 470 on 2026-10-21), B265 17 505.

**Minimal delta.** A records pass: the 42 rows to three cells (refs into the item cell, as B563 to
B566 do); B535, B536 and B561 closed with their merge dates; B516's count; B524's status. Then the
check the last two audits proposed (about 80 lines): every row has exactly three cells, a status
from a closed list and a unique id, each rule with a planted bad row as its control.

### M2 — README: four false statements outside the rebuild table, and a stale date

`README.md:586` "Last verified: **2026-10-17**, against main `5613885`". The rebuild table was
refreshed on 2026-10-29 (its row says so, `:405`); the rest was not.

| README | the tree |
|---|---|
| `:389` "beside horde's own ECHO, EQ, FX filter and drive" | ECHO is a sibling (ADR-249, 2026-10-19), and **`:392-394`, three lines below**, says Weir "provides ... the FX filter module" (ADR-307) |
| `:401` the spec is "amended since by ADR-230 and ADR-231" | Amended by spec rounds 9 to 47; the newest is ADR-304 (round 47, `DECISIONS.md:9882`) |
| `:579` "`docs/design/` — 36 labs" | **46** lab pages plus `index.html` (MEASURED, a listing of `docs/design/*.html`) |
| `:574` `reference/` holds the prototypes "the parity oracles extract from" | 7 of 19 are read by no tool (H2) |

Where tested, the rest holds: `:473` "204 tests (116 ..., 88 ...)" equals `test_table_check` today
(204, 116, 88).

**Minimal delta.** Four lines and a new "Last verified" date with the commit it was checked
against. B455 named this README finding in 2026-10; this is its third audit.

### M3 — The lab's cost figures for B303 read 1.5 times the measurement; the parity behind them is not gated

**Evidence (MEASURED).** `tools/filter_cost_bench.cpp` built with `clang++ -std=c++20 -O3` and run
with the arguments `4 7` at nice 10, twice (4 s of audio, median of 7, thread CPU time), at load
average 3.4 to 4.4. That is above SPEC-ENGINE PF-3's load guard (≤ 3.0), and far below the ~180
under which the lab's figures were taken. The two runs agree within 4 %.

| type (16 voices × 2), % of one core | lab `COST_ROWS` (`filter-lab.html:2164-2168`, load ~180) | today, run 1 | today, run 2 | lab / today |
|---|---|---|---|---|
| BYPASS | 0.04 (stated, `:2157`) | 0.012 | 0.013 | 3.3 |
| svf12.LP | 0.730 | 0.480 | 0.490 | 1.52 |
| svf24.PEAK | 0.972 | 0.608 | 0.607 | 1.60 |
| ladder (2× OS) | 13.656 | 8.573 | 8.864 | 1.59 |
| ms20 (ADAA) | 7.177 | 4.902 | 5.034 | 1.46 |
| formant | 1.381 | 0.833 | 0.846 | 1.66 |
| phaser | 1.259 | 0.913 | 0.928 | 1.38 |
| ladder@1x | 4.403 | 3.376 | 3.371 | 1.30 |

- The order, and the ratio that drives the recommendation, hold: ladder over svf12.LP is 18.7 in
  the lab and 17.9 today. The recommendation (`filter-lab.html:2184`: per note "every linear type
  plus the MS-20", the ladder held) does not change. The figures it quotes do: "the MS-20 is
  7.2 %", "the LADDER 13.7 %", against 4.9 to 5.0 and 8.6 to 8.9 measured.
- **Two hand-typed copies of one output.** `COST_CPP` (`:2160-2163`, per type; drives the pills and
  hints at `:2688` and `:2742`) and `COST_ROWS` (`:2164-2168`, per row; drives the table at
  `:2935`) are both typed from the bench's printout. `COST_CPP` is not derived from `COST_ROWS` by
  any stated rule: `svf24` 0.97 is neither the rows' maximum (0.997) nor their mean (0.935).
- **The parity is real and ungated.** The bench's `--dump` piped into
  `filter_fidelity_check.mjs --parity` today: **18 of 18 rows MATCH**, max |Δ| 4.66e-13 (ms20) and
  ≤ 2.0e-15 for the rest, every control (cutoff +2.4 ¢) caught. The mode is "run by hand, NOT part
  of the ./verify run" (`filter_fidelity_check.mjs:80`). The lab is under active work, so the bench
  can drift from it, and the figures with it, silently.
- B566's status (`ROADMAP.md:7579`) says "the lead re-measures on a quiet machine". The table above
  is nearly that measurement; taken once more at load ≤ 3.0, it can be recorded.

**Minimal delta.** (a) Replace both literal tables with one table the bench prints as a JS object,
the per-type figure derived from it by a stated rule; or, at the least, today's figures with their
date and load. (b) Wire `--parity` into `./verify full`: one Release compile (seconds) and the
comparison, about 6 lines, additive. Under ADR-307 Weir takes the C++ forward, so (b) guards only
horde's copy until the lab is frozen (M4).

### M4 — ADR-307 moved the filters to Weir; no horde file the move touches says so

**Evidence (MEASURED, a count of "Weir" or "ADR-307" per file).** 0 in `docs/design/filter-lab.html`,
`tools/filter_cost_bench.cpp`, `tools/labharness/filter_fidelity_check.mjs`, `docs/design/index.html`
and `CLAUDE.md`. README has it (`:392`).
- ADR-307 (`DECISIONS.md:9941`): "horde's filter lab and `tools/filter_cost_bench.cpp` are its
  starting point, not a parity target". B303, B288 and B289 "now land in Weir".
- `verify:408-418` still opens "THE FILTER LAB'S FIDELITY PROGRAMME IS A GATE (B274)". What the gate
  guards after the move (the lab as listening material, or nothing once Weir's C1 to C7 port is the
  oracle) is written nowhere: a gate whose purpose moved without a recorded fate. This is not a
  proposal to remove it; a removal, if ever, is the human's.
- `docs/H2-PLAN.md:3` reads "Last verified: 2026-10-27 ... through ... ADR-294", the day before
  ADR-307. Its `filters` row (`:134`, depending on `seams` and `engine2`) and `fxfilter` row
  (`:145`, "planned", `rack`) name neither Weir nor B567. Two days' lag is within cadence; it is
  listed so the next refresh catches it.

**Minimal delta.** One sentence in each of the lab's, the bench's and the check's headers: "ADR-307:
Weir leads the filter DSP; this file is its starting point and is frozen at `c0e4c3f` except for
listening work." One sentence in the `verify` comment saying what the gate guards now and that it
retires only by the human's ruling at Weir's Phase 3 hand-off. H2-PLAN's two rows at its next
refresh.

### M5 — The admission check's pinned row is not control-judged; A19's budget is a second copy

**Evidence.**
- MEASURED today: `admission_check` GREEN. 15 rows PASS, A11 "FAIL PINNED" (`method[verb.era]`,
  ADR-296), A21 N/A, every listed control fired, A11's four included; A10 ran under
  RealtimeSanitizer with its planted malloc caught first.
- BY READING, the latent hole. `verdict()` returns "FAIL" as soon as a row has a failure
  (`tools/admission_check.py:301-302`), before it looks at the controls. Red is raised only for NOT
  PROVEN or an unpinned failure (`:379-383`). So on a row whose only failure is pinned, a control
  that stops firing turns nothing red. The docstring says a FAIL row's controls are "still judged
  and shown" (`:33`); they are shown, not judged. The GREEN line prints "every control fired"
  unconditionally (`:415`). It is true today; the code does not make it so.
- One rule, two copies. `tools/admission/harness.cpp:1108` reads
  `const std::size_t kBudget = 2048;   // RS-6.12, provisional (Q9)`, and `:1111` fails a module
  whose own budget exceeds 2048. RS-6.12 (`docs/proposals/rack-slot-contract.md:287`) was amended a
  day after the harness (ADR-300, 2026-10-28): "at most **4 KiB for Shriek**". Correct for Scape
  today; the day Shriek is admitted, A19 fails a module the contract allows.

**Minimal delta.** In `verdict()`, a row whose failures are all pinned and one of whose controls
did not fire is NOT PROVEN (about 3 lines), with a control: a pinned row whose plant is disabled
must go red. The GREEN text then states only what was checked. The budget comes from one place: a
`state_budget` per module in `MODULES` (`admission_check.py:62`), passed to the harness and citing
RS-6.12. Both additive.

### M6 — New stored state without undo or branch rules; a record outside the budget; append-only by prose

**Evidence (BY READING).**
- ADR-306 (`DECISIONS.md:9926-9940`): horde's slot state "keeps `draftFrom` per user patch and
  `pending.base` with an unsaved patch, so revert survives a reload". The save rule is stated. Not
  stated: whether "randomize all four" (four corners from one stream) is one undo step or four;
  whether Sluice's in-slot revert is an undo step; what a → b → a restores for `pending.base`.
  ADR-277 ruled these for horde's own corner revert ("one undo step that branches"). It does not
  cover a hosted module's buttons.
- RS-6.12 (`rack-slot-contract.md:287`): "With four modules and four corners that is 40 KiB of
  B446's 64 KiB cap". ADR-294 adds a per-corner note-processor record, `np["arp"] = {origin_id,
  module_version, preset, blobs}`, including a user-pattern blob.
  `docs/proposals/note-processor-preset-format.md` states no size bound (no "KiB", "bytes" or "size"
  in the file), and RS-6.12's sum does not count the record. ADR-293 plans two arps after 1.0.
  RS-8.1 (`:337`) speaks of "both racks" while RS-6.12 counts four modules; which holds is not
  stated.
- `note-processor-preset-format.md:30`: discrete settings are stored as their "index in the
  manifest's `values` list ... The lists are append-only in Arp's manifest, so an index never
  changes meaning." Arp's `tools/core_params_check.py` (read, not run) compares the C++ with
  `params.json` and the value lists with the spec. It makes no comparison with a previous release,
  so an insertion would pass it. horde holds a snapshot (`docs/design/arp-face/params.snapshot.json`,
  Arp `de4e2af`) and no check reads it for this. That is insertion into positional data, guarded
  by one sentence.

**Minimal delta.** (a) One table in B331 or an amendment to ADR-306: for `draftFrom`, `pending.base`
and the two buttons, the undo step, the branch rule and the a → b → a result. (b) RS-6.12's sum names
the note-processor record and its cap, or says why it is outside. (c) A brief to Arp, through its
mailbox, asking for an append-only gate on `params.json` value lists against its last released
manifest; on horde's side, B550's acceptance names that gate.

### M7 — Spec provenance is still open (B534, carried from H2)

**Evidence (MEASURED).** The highest spec round named in `specs/SPEC-ENGINE.md` is **47**; the
highest named in `docs/port/spec-engine-provenance.md` is 13, and its newest engine ADR is ADR-239
(its one later citation, ADR-292, is a lab list). The file's one change since 2026-10-21 (`2cc5217`)
edited that lab list. Requirement ids introduced after round 16 with no entry: the twelve of the
last audit (EN-5, QS-11, SN-5 to SN-14) plus **ED-7** (round 29, ADR-265, `2c57b9b`). B534
(`ROADMAP.md:7547`) is OPEN, and on GitHub shows no status (M1). `specs/SPEC-ENGINE.md:3` still reads
"The contract for horde's engine, 2026-10-16", 31 rounds ago.

**Minimal delta.** B534 as written. The back-fill is now rounds 17 to 47, not 17 to 28.

### M8 — Carried, unchanged

- **CLAUDE.md §Domain** (last changed `655fa3d`, 2026-10-21): "not yet in the shell" `:78`, "MAW is
  FX-C now" `:81`, "open ruling on" `:95`, "eighth CANDIDATE" `:104`, "seven single-file HTML"
  `:116`, "SECOND CLAP shell" `:126`, "now seven" `:168`, "golden render fixtures once they exist"
  `:185`. `engine2`, `h2/`, Weir and Scape: 0 mentions each. B403 is OPEN. The file is the human's.
- **The nightly has never run.** `.github/workflows/` holds `ci.yml`, `cpu-derate.yml` and
  `docs.yml`; none has a `schedule` or `cron` (MEASURED). The clean room's 17-hour soak is run by
  hand and was stopped by a shutdown (`SESSION.md:14-16`).
- **The verify record.** `.harness/last-verify.json` in the main checkout after the 2026-10-29 run:
  `"git":"1119dc5", ..., "gates":[]`. A green run still records nothing about what it skipped. The
  brief to autonomous (B455, `hypersaw-010`) is due 2026-11-07.

### M9 — SESSION.md gives wrong next ids

`SESSION.md:57`: "**Numbering:** next ROADMAP row B544, next ADR ADR-271, next records branch
lead-records-314." MEASURED: the highest row is B567 and the highest ADR is ADR-308, so the next are
**B568** and **ADR-309**; PR #1247's branch was `lead-records-355`. The line sits under "Carried
from the 2026-10-22 close, not re-checked" (`:36`), with two more false lines: `:48` "Owed rows:
... B535, B536" (both merged 2026-10-24) and `:51` "`traces/` has no entry after 2026-10-17" (17
trace files are dated 2026-10-22 to 2026-10-28). A session that takes its next id from this file
makes a duplicate, the defect B147 and B155 already show.

**Minimal delta.** Delete the carried numbering line, or keep the current numbering at the top of
the file with the live state. Records only.

---

## LOW

- **L1 — "twelve filter types".** `verify:409` and `filter_fidelity_check.mjs:6`; the check pins
  **20** rows (18 per-voice types and modes plus two banks) and says so itself at `:36`.
- **L2 — The bench cites a tool that never existed.** `tools/filter_cost_bench.cpp:17`:
  "tools/labharness/filter_cost_parity.mjs slices the lab's DSP". The repository's whole history has
  no file at that path (a log over all refs, limited to it, is empty). The real command is at `:33`
  (`filter_fidelity_check.mjs --parity`).
- **L3 — Records out of order.** `DECISIONS.md:9592` ADR-289 sits after ADR-294 (`:9574`) and before
  ADR-295. `:8294` ADR-232 still reads "PROPOSED (ratified by merging its PR)"; `:8846` ADR-255 A1
  still sits after ADR-256 (both carried). B516 to B525 sit after B567
  (`ROADMAP.md:7581-7590`).
- **L4 — The design system lives in one lab's folder.** `docs/design/arp-face/{tokens,bundle}.css`
  and `bundle.js` (469 192 bytes) are loaded by three labs: `arp-face-lab.html`,
  `page-chrome-lab.html:54-55,292` and `filter-lab.html:59-60,369`. `tokens.css` is made by rules
  written in prose (`arp-face/README.md`, "How tokens.css was generated"), with no generator in the
  tree. The filter lab takes the GUI 3 path only when the real `window.Horde` loads
  (`filter-lab.html:2119-2123`); the headless gates run it against stubs, so that path is exercised
  by no gate. Delta: move the vendored bundle to a shared folder (three `href`s and one README), and
  a generator of about 20 lines for `tokens.css`.
- **L5 — A plant that throws counts as caught.** `filter_fidelity_check.mjs:292` maps any exception
  in the main plant loop, other than an anchor error, to "MOVED"; `:318`, `:342`, `:397` and `:434`
  do the same for the other groups. A replacement text with a typo would be counted as caught.
  Today's run caught 29 of 29; groups 5 to 8 printed the failure each plant caused, so those 8 are
  real catches. Whether any of the 21 in the main loop is caught by an exception was not measured.
  Delta: print the exception and count it as caught only when the check itself raised it.
- **L6 — Carried counts.** The wiring glob (`tools/test_table_check.py:186-187`) still skips
  `tools/*.mm` and `tools/patchspace/`, where `gui_webview_check.mm`, `identity_store_check.mm` and
  `patchspace/listening_pass_check.mjs` have no declaration (all three are invoked by `verify`).
  `CMakeLists.txt:469`, `:490` and `:511` still say "NOT in ./verify" of three checks `verify` runs.
  `playbook_check`: 145 of 186 citations drifted, unchanged. `tolerance_registry_check`: 74 of 138
  entries are "unattributed". `golden_registry_check`: 1 of 42 goldens has an independent check.
- **L7 — B564's pattern.** `lab_load_check` is green on `docs/design/sluice-horde-lab.html`, which
  opens on "Sluice spec not available" (B564, OPEN). A lab that degrades to a message passes a check
  that looks only for throws. Known and scheduled (S4); listed for the pattern.
- **L8 — Cadence.** 8 days against 7.

---

## Cost (sweep item 4)

- MEASURED, the filter types: M3's table. Bit-identical options were not examined: the bench is a
  measurement of transcriptions, and Weir now owns the DSP.
- MEASURED, the hosted module, as the admission check reports it (A10, never judged, ADR-187 §8):
  the first 128-sample block after a `reset()` with moved parameters took median 0.465 ms and **max
  31.55 ms** over 21, against a block length of 2.90 ms at 44.1 kHz (10.9 times the block). A steady
  block: median 0.0346 ms. That is the module's cost, not horde's. It is the figure `SESSION.md:31`
  says horde owes Scape on a quiet bench (RS-8.2); today's load was 3.4 to 4.4.
- No per-sample code changed in `src/`, `h2/`, `engine2/core` or `engine2/tests` since 2026-10-21
  (a diffstat of those paths from `900963c` to `origin/main` is empty), so nothing new was swept
  there.

## A clean bill, where the sweep found one

- **The filter fidelity gate is green for the right reason.** MEASURED: 20 rows × 7 checks, 124
  pass, 16 FAIL (all pinned findings), **140 of 140 controls fail as they must**, 29 of 29 plants
  caught, the listening batch at 11 items under its −6.02 dBFS cap with both its plants caught.
- **The bench computes what the lab computes**: 18 of 18 rows match to ≤ 4.66e-13, every control
  caught (M3).
- **The admission test fires every control**, A11's four included, and A10's sanitizer is proven by
  its planted malloc before the clean session is believed (M5).
- **The last audit's H4 is closed with controls** (B535, #1208): the tolerance, enrolment, RT-lint,
  weakening and golden checks see `engine2/`; the three render manifests are registered goldens;
  the version-0 states are classed INPUT with a written reason
  (`tools/golden_registry_check.py:138`). That is a deliberate exclusion, written down.
- **Also closed:** H5 (B536, #1209), M6 (B537, ruled by ADR-277), M5 (H2-PLAN refreshed
  2026-10-27), H3 (B525 DONE, B519 IN FORCE, the five relaxations ruled by ADR-288). H1 was closed
  by B533 and has reopened as this report's H1.
- **The lab index is complete**: all 46 lab pages are linked from `docs/design/index.html`.
- **The wiring census holds**: `test_table_check` GREEN, 136 check files WIRED and verified, 1
  UNWIRED and verified.
- **The merges since 2026-10-21 that changed a tool have traces** (from
  `traces/2026-10-24-b535-armor-sees-engine2.md` to `traces/2026-10-28-b566-filter-lab-round3.md`,
  17 files).

## What changed since the last audit (2026-10-21, `900963c`)

- **85 merges, 211 commits.** ADR headers 268 → **314**. ROADMAP rows 529 → **564** (562 ids).
  Traces 490 → **507**. ROADMAP 1 895 132 → 1 966 848 bytes (growth halved, from about 20 KB a day
  to 9).
- **No product code changed.** Everything new is a lab, a tool, a record or a design document. The
  largest additions: `docs/design/arp-face/` (the vendored design system, 7 799 lines), the
  admission test (`tools/admission/`, 2 382 lines, plus 467 in its driver), the differential-report
  tool (709) and the filter cost bench (390).
- **Closed from the last audit:** H3, H4, H5, M5, M6. H1 closed by B533 and reopened (H1 here).
- **Open and unchanged:** H2 (now M7, wider), H6 (now H4), M1 (B538 OPEN), M3, M7 and M10 (now M8),
  M4 (now M2; the rebuild rows were fixed), L2 to L4 (now L6).
- **New ground:** the filter round and the Weir move, the admission test, the tolerance exclusions,
  the hosted modules' state (ADR-294, ADR-300, ADR-306), and the protected-path gap (H2), which
  predates the last audit but was not swept then.

## Reduction budget

| finding | lines removed | lines added | files | checks |
|---|---|---|---|---|
| H1 | 0 | ~4 heads in B533; 1 roundup line | 0 | 0 |
| H2 | ~20 (CLAUDE.md's hand list, the human's) | ~60 check + `protected.json` (generated) + 1 armor row | +2 | +1 |
| H3 | 0 | ~13 `value` fields + ~8 control | 0 | 0 (1 tightened) |
| H4 | 0 | ~5 ratchet | 0 | 0 (1 tightened) |
| M1 | 42 extra cells; 2 duplicate ids | ~6 status cells + ~80 roadmap check | +1 tool | +1 |
| M2 | ~4 | ~5 | 0 | 0 |
| M3 | 2 literal tables (~15) | 1 generated table (~10) + ~6 in `verify full` | 0 | 0 (1 mode wired) |
| M4 | 0 | ~6 sentences + 2 H2-PLAN rows | 0 | 0 |
| M5 | 1 literal | ~3 verdict + ~10 control + 1 field | 0 | 0 (1 tightened) |
| M6 | 0 | 1 table + 1 sentence + 1 brief | +1 brief (Arp's mailbox) | 0 |
| M7 | 0 | B534 as written (~250 back-fill + ~40 check) | +1 tool | +1 |
| M8, M9, L1-L3 | ~20 | ~10 | 0 | 0 |
| L4 | 0 (a move) | ~20 generator | +1 tool | 0 |
| L5 | 0 | ~6 | 0 | 0 (1 tightened) |
| L6 | ~9 CMake + 145 line numbers | ~4 glob + 3 headers | 0 | 0 |
| **total** | **~250 lines** | **~550 lines, mostly records and three checks** | **+5 tracked** | **+3 new, 5 tightened, 1 mode wired** |

No gate is removed or relaxed.

## Not swept, stated rather than guessed

- **The clean-room repositories.** Not read, by ADR-247 and B519. H1 rests on horde's own records.
- **Whether a whole `./verify full` has run since B561 merged.** Only its admission step ran here.
- **The Artifact-hosted design system**, of which `arp-face/bundle.js` says it is a verbatim copy.
- **Weir's tree**, beyond the ADR-307 and B567 text.
- **`integrations/`**, the 46 lab pages beyond the filter, page-chrome and arp-face labs' loaders,
  and the 507 traces.

## Proposed ROADMAP rows

The lead assigns ids and wording.

| # | row | from | acceptance |
|---|---|---|---|
| 1 | **The off-machine copy kept current** | H1 | The human pushes both clean-room repositories; B533 records the heads for stage 8 and every round since; the lead's session roundup prints the not-pushed counts each session. |
| 2 | **Protected paths gated** | H2 | `docs/armor/protected.json` lists every protected file with its hash; a check in `verify fast` is red on a moved hash without a sanction entry, proven by a planted edit; one armor row; the human rules on `decoherence-lab.html` and `kuramoto-views.html`; CLAUDE.md points at the declaration (the human's sanction). |
| 3 | **Tolerance exclusions pin their values** | H3 | All 13 exclusions carry `value`; a control edits an excluded value and must go red. |
| 4 | **B256 with a ratchet** (carried) | H4 | The sanitize job fails above 19 not built; B256 gets a status. |
| 5 | **ROADMAP renders its own status** | M1, M9 | Every row has three cells; B535, B536, B561, B516 and B524 carry true statuses; B147 and B155 are unique; a roadmap check fails on a fourth cell, an unknown status word or a duplicate id, each with a plant; SESSION.md's numbering is current. |
| 6 | **README re-verified** | M2 | `:389`, `:401`, `:574` and `:579` match the tree; "Last verified" carries a new date and commit. |
| 7 | **The filter cost on one table, re-measured, parity wired** | M3 | One cost table, generated or labelled with its date and a load ≤ 3.0; `filter_fidelity_check --parity` runs in `verify full` after a Release compile. |
| 8 | **ADR-307 carried into horde's files** | M4 | The lab, bench, check and `verify` comments say what Weir leads and what the gate guards now; H2-PLAN's `filters` and `fxfilter` rows cite B567. |
| 9 | **The admission check judges a pinned row's controls; one budget source** | M5 | A pinned row with a control that did not fire is NOT PROVEN, proven by a plant; A19 reads each module's budget from one cited table. |
| 10 | **Undo, branch and budget rules for hosted-module state** | M6 | ADR-306's fields and buttons have undo and branch rules; RS-6.12 counts the note-processor record; Arp is asked for an append-only gate, and B550 names it. |

## The five to act on first, and one question

1. **H1.** Stage 8 and fourteen spec rounds have one copy. Two pushes close it.
2. **H2.** Seven protected references and every protected spec can be edited with every gate
   green. The fix is a hash ledger and one check.
3. **H3.** ADR-308 was ruled on two constants and recorded as a whole file, and none of the 13
   exclusions pins a value. About fifteen lines.
4. **M1.** 42 ROADMAP rows show no status on GitHub, where the human reads them. A records pass.
5. **M3.** The lab tells the human the MS-20 costs 7.2 % and the ladder 13.7 %; today's
   measurement is 4.9 to 5.0 % and 8.6 to 8.9 %. The recommendation holds, but the human should not
   rule on those numbers.

**The one question for the human.** `reference/decoherence-lab.html` and
`reference/kuramoto-views.html` sit in the protected folder (ADR-155) but are missing from
CLAUDE.md's protected list, and no tool reads either. Are they protected references, or design labs
that belong in `docs/design/`?
