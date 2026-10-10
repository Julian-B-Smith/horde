# Repo audit — 2026-10-10

**Commit audited** `4a3dbaf5e45407590f18ed7cebd2c5ae1e81bc05` (`origin/main`, PR #1021 merged).
**Cause** The auditor cadence (CLAUDE.md §Domain, B159, ADR-179 §3), dispatched by the lead
inside the overnight batch the human approved (ROADMAP B448, "Overnight batch dispatched
2026-10-10"). The previous repo-wide audit is `docs/audits/2026-09-19-repo-audit.md`, 21 days
old against a 7-day cadence.
**Scope** The whole repository, read-only. Hardest where the tree moved since the last
audit: `h2/engine/`, the B446 security files, the ADR-200 handoff in
`src/hypersaw_clap.cpp`, and the B448 armor gates.
**Oracle** `./verify fast` exit 0 at `4a3dbaf` in this worktree (target fast, exit 0, as
recorded in `.harness/last-verify.json`). In a worktree that run is reduced: six gates
print SKIPPED or WARNING (M3).
**Not re-reported** ROADMAP B454 items 1-10, B403's list (CLAUDE.md §Domain, the duplicate
ids B147/B155), and the gaps `docs/armor/catalogue.json` states about itself. Where a
finding builds on one of those, it says which.

Every finding is a **proposal for the lead**. Nothing outside this file and its trace was
edited. Nothing here proposes weakening a gate. A delta that touches a protected path is
labelled as a sanction the human would have to give.

**Rules of evidence used.** MEASURED means a command ran on this machine tonight and
printed the number. PROVEN means a control was run and tripped. BY READING means the
claim follows from the cited lines and was not executed; each such finding names the row
that would measure it. The machine carried a load average of 6 to 10 on 8 cores from the
other overnight agents, so wall times are upper bounds; rankings were stable across two
samples.

## The findings at a glance

| id | rank | class | one line |
|---|---|---|---|
| H1 | HIGH | green for the wrong reason | Eight golden-parity gates, L0-1 among them, exit 0 on an empty corpus; no scenario count is pinned (PROVEN) |
| H2 | HIGH | a control that cannot fire | The Stop gate cannot see an edit made through the shell, and a fresh worktree has no record that could be red |
| H3 | HIGH | green when it never ran | CI's sanitize job is GREEN with 47 of 74 parsed oracles actually sanitized and judged; B256 grew from 1 skipped oracle to 18 |
| H4 | HIGH | one rule, two lanes | A load made while not processing is split between a direct lane and the queue; a direct host load does not supersede queued entries (BY READING) |
| H5 | HIGH | doc drift | README describes the frozen legacy product and never mentions horde 2; "last verified" is 30 days old; 11 false statements |
| H6 | HIGH | truth contract | ROADMAP.md is 1.67 MB; its queue table is 1.0 MB in 451 one-line rows; 20 rows say OPEN and MERGED in the same cell |
| M1 | MEDIUM | a hand list beside a glob | `test_table_check`'s wired-or-explained rule covers three filename patterns; five check files and three probe gates sit outside it |
| M2 | MEDIUM | cost, measured | `verify fast` takes 122-132 s; five gates are about 60% of it; one spends 97% of its time recomputing the same answer |
| M3 | MEDIUM | green when it never ran | Nothing records what a green `verify` skipped; the license rows never run in CI or in a worktree |
| M4 | MEDIUM | prose asserting a relationship | `CMakeLists.txt` says nine checks are "NOT in ./verify"; `verify` runs all nine |
| M5 | MEDIUM | one rule, two implementations | Engine-block values on the direct host-load path lack the "a load is not an edit" bracket the other three sites carry (BY READING) |
| M6 | MEDIUM | a law with no gate | "Every default change carries an ADR and a migration" has one check on one key and no catalogue row |
| M7 | MEDIUM | stale record | The armor catalogue still lists three gaps that PRs #1020 and #1021 closed; its coverage check is one-directional |
| M8 | MEDIUM | stale comment, protected | `verify` and `docs/ROBUSTNESS.md` describe a pan-motion exclusion ADR-177 retired, and two more past shapes |
| M9 | MEDIUM | doc drift | 31 false statements in eight documents, 56 with H5, M4 and M8; each with the contradicting fact (Appendix A) |
| M10 | MEDIUM | carried | Eight findings of the 2026-09-19 audit are still open; one has got worse |
| L1-L8 | LOW | dead, duplicate, cost | Dead functions, literal strides, per-entry host callbacks, never-run targets, overlapping checks, loop hygiene |

One finding reported privately to the lead.

---

## CRITICAL

**None.** No finding shows a defect that ships wrong audio today, a violated invariant in
the product, or an irreversible loss. H1 to H3 are gates that would stay green through a
regression; H4 is a narrow ordering window read from the code and not yet measured.

---

## HIGH

### H1 — Eight golden-parity gates exit 0 on an empty corpus, and no scenario count is pinned

**Claim.** `parity_check` is L0-1, the gate CLAUDE.md §Domain names as the definition of
C++ correctness. It and seven sibling gates loop over a manifest the golden generator
writes, count failures, and return 1 only when a failure was counted. With zero manifest
lines there are zero failures. Nothing in the tree pins how many scenarios a manifest
must hold.

**Evidence (PROVEN).** Each binary was run against a directory holding only a zero-byte
manifest. The binaries are the main checkout's Release build of 2026-10-04; the eight
sources were last changed on 2026-09-21 (`e37fdbc`, `f798f48`), so the binaries are the
audited source.

| gate | exit | last line |
|---|---|---|
| `parity_check` | 0 | `parity_check: 0/0 scenarios within eps=1e-06 (worst 0.000e+00 @ )` |
| `filter_check` | 0 | `filter_check: GREEN (0 failures; worst parity rms 0)` |
| `notch_check` | 0 | `notch_check: GREEN (0 failures; worst parity rms 0)` |
| `spectra_check` | 0 | `spectra_check: GREEN (0 failures; worst parity rms 0)` |
| `swarmalator_check` | 0 | `swarmalator_check: GREEN (0 failures; worst parity rms 0)` |
| `time_check` | 0 | `time_check: GREEN (0 failures; worst parity rms 0)` |
| `station_check` | 0 | `station_check: GREEN (0 failures; worst parity rms 0.000e+00)` |
| `subosc_check` | 0 | `subosc_check: GREEN (0 failures; worst parity rms 0.000e+00)` |
| `force_check` | 1 | `force_check: RED (17 failures)` (control: this one has a floor) |
| `glide_check` | 1 | `glide_check: RED (21 failures; worst parity rms 0)` (control) |
| `intent_check` | 1 | `0 fixtures, 1 failure(s)` (control) |

- `tools/parity_check.cpp:57` declares `failures` and `count`; `:151-153` prints
  `count - failures, count` and returns on `failures` alone. No line compares `count`
  with anything. `tools/filter_check.cpp:137-139` and its four siblings have the same
  shape.
- The corpus is not a committed fixture: `verify:639-641` regenerates it every run from
  `tools/golden/gen_goldens.mjs`, which is not a protected path. `--selfcheck` proves the
  generator is deterministic over whatever scenarios it holds (`gen_goldens.mjs:243`), not
  that it holds 156.
- "156/156" appears in `README.md:406`, in `docs/ROBUSTNESS.md` and in traces. All prose.
  `tools/weakening_check.py` counts skip and suppression markers, not corpus sizes, and
  `docs/armor/tolerances.json` pins tolerances, not counts.
- The directory-driven gates were run the same way as a second control and have floors:
  `statefix_check` ("0 fixtures, 1 failure"), `bank_check` (16 failures), `offcorner_check`.
- The repo already owns the right idiom: `h2_engine_parity_check` pins its 543 scenarios,
  fails on a stream with no END record, and `verify:757-775` plants a truncated stream
  that must go red (TRUNC).

**Criterion violated.** Sweep item 6: "can its corpus distinguish the two behaviours at
all". Blind-Spot Armor's "quiet disabling": a scenario block deleted from a generator
shrinks L0-1 and every counter still reads zero increases.

**Minimal delta.** One floor per gate, pinned at today's count, in the gate's own file:
fewer scenarios than `kMinScenarios` is a failure (156 for `parity_check`; the others
print their own counts). Eight comparisons plus eight constants. Strengthening a check is
not gated (ADR-180 §1). L7 proposes the shared rig that would hold the floor once instead
of eight times. A pinned count that may only rise is the ratchet `param_id_lock_check`
already applies to ids.

---

### H2 — The Stop gate cannot see an edit made through the shell, and a fresh worktree has no record that could be red

**Claim.** The closing gate that makes "an agent cannot finish on unverified edits" true
has two conditions. One depends on a marker only the Edit and Write tools set. The other
depends on a file that does not exist in a new worktree. A session that edits through the
shell in a fresh worktree passes the gate without `./verify` ever having run.

**Evidence.**
- `.claude/settings.json:47` — the PostToolUse matcher is `Edit|Write|MultiEdit`.
  `.claude/hooks/posttool-dirty.sh:2` is the only writer of `.harness/dirty`.
- `.claude/hooks/stop-gate.sh:11-22` — blocks if `.harness/dirty` exists, or if
  `.harness/last-verify.json` exists **and** its `exit` is non-zero. With neither file it
  reaches `exit 0`.
- `.gitignore:2` ignores `.harness/`, so every new worktree starts with neither file.
  Observed tonight: this worktree had no `.harness/last-verify.json` until the audit's own
  verify run wrote one.
- Three of the four agent definitions have no Edit or Write tool at all
  (`.claude/agents/auditor.md:4`, `verifier.md:4`, `critic.md:4`: Read, Grep, Glob, Bash).
  This report was written with shell redirection; the marker was never set.
- Tree changes that never set the marker, all routine here: a generator run
  (`gen_gui_controls.py`, `armor_dashboard.py`, `h2_libm_count.py --write`), an in-place
  stream edit, an applied patch, a merge, a cherry-pick, a path checkout.
- The record carries the commit it judged (`.kit/kit-gates.sh:23-28` writes the short
  hash), but the gate reads only `exit`. Verify at commit A, commit B through the shell,
  stop: allowed.
- No check exercises either hook. `tools/deny_hook_check.py` holds a verdict table for
  `deny_push.py` only; no file under `tools/` names `posttool-dirty` or `stop-gate`.

**Criterion violated.** Sweep item 6: "a gate reports success when it passed, when it
never ran, and when it cannot fail". Charter, Oracle discipline: "Run `./verify fast`
after any change set".

**Mitigating fact.** CI runs `verify fast` on every PR, docs-only ones included since
`docs.yml`. The hole is the local half: `verify full` exists only locally, so "done" for
a queue item rests on a gate that can be passed without running anything.

**Minimal delta (about 12 lines, in two project-owned hook files; neither is listed in
`.kit/MANIFEST`).** In `stop-gate.sh`: treat a missing record as "not verified" when the
working tree is not clean or the branch is ahead of its base, and block when the record's
commit is not HEAD. Add a row set for both hooks to `deny_hook_check.py`, which already
runs hook policy against a planted table. The hooks are harness-layer files, so the same
change is worth a brief to the kit.

---

### H3 — CI's sanitize job is GREEN with 47 of 74 parsed oracles sanitized and judged; B256 has grown from 1 skipped oracle to 18

**Builds on** ROADMAP B256 (2026-09-24, no status cell: `morphlayout_check` does not link
on Linux) and B454 item 1 (the sanitizer loop skips targets declared after it). B256
asked to "audit every other check the sanitize job SKIPS for the same cause". This is
that audit, with the count.

**Evidence (MEASURED, CI run 38026008521 on `4a3dbaf`, both matrix legs).**
`sanitize_oracles [address,undefined]: 53 oracle(s) passed, 18 not built on this platform
... GREEN`, and the same two numbers on the `thread` leg.

| what happened to the 74 names `tools/sanitize_oracles.sh` parses out of `verify` | count |
|---|---|
| not built on Linux, printed SKIPPED, job still GREEN | 18 |
| no target in the tree (`gui_webview_check`, `conformance_check`) | 2 |
| not run under a sanitizer by mechanism (`rtsafety_probe`) | 1 |
| "passed" but compiled without `-fsanitize` (B454 item 1: `presetstore_check`, `twocluster_check`, `fxxfade_plugin_check`) | 3 |
| "passed" by printing SKIP (`h2_engine_selfdigest_check`, `_product`: unkeyed platform) | 2 |
| "passed" but not an oracle (`param_id_dump` prints a table) | 1 |
| sanitized and judged | **47** |

- The 18: `state_check undo_check statefix_check bank_check anchor_check penv_check
  morphlayout_check polarity_check lfoenv_check offcorner_check modreadback_check
  load_handoff_check paramclass_check bridge_utf8_check hostile_events_check routing_check
  subosc_check intent_check`. Every one fails at link on `undefined reference to
  hypersaw_debug_*`.
- Cause, unchanged since B256: 49 debug exports sit at `src/hypersaw_clap.cpp:10469`
  onward, inside the GUI block `#if defined(__APPLE__) || defined(_WIN32)` that opens at
  `:10439` and closes at `:11037`.
- `docs/ROBUSTNESS.md:118-119` still reports this as "23/24 oracles GREEN; `state_check`
  NOT BUILT". It was 1 on 2026-09-10 and is 18 today.
- The set that does not run is the set that parses outside text: every state, preset,
  bank and fixture loader, plus the three checks the B446 and B448 work added for hostile
  input (`bridge_utf8_check`, `hostile_events_check`, `load_handoff_check`).
  `docs/armor/catalogue.json` credits `sanitize` as a gate on rows R2 and R11, and lists
  `bridge_utf8_check` and `hostile_events_check` under SEC-input; on CI those two never
  execute at all, sanitized or not.
- A second defect waits behind the first. The parser at `tools/sanitize_oracles.sh:32`
  captures an argument only when it begins `build-golden` and is made of lowercase
  letters and slashes. Seven invocations in `verify` take another kind of argument
  (`verify:675, 677, 681, 689, 690, 879, 894`), and `swarm48_check` / `h2_swarm48_check`
  lose `build-golden/sr48000` to the digit. Four of those have a default path and pass
  today. Three do not: `statefix_check`, `bank_check` and `bridge_utf8_check` return 2 on
  a missing argument (`tools/statefix_check.cpp:95-98`, `tools/bank_check.cpp:339-343`,
  `tools/bridge_utf8_check.cpp:170`). The day B256 is fixed and they link, the job turns
  red for a reason that is not a sanitizer report.

**Criterion violated.** Sweep item 6 (green when it never ran). ADR-197: a risk-register
row is a named gate or a visible hole; here the dashboard shows a gate.

**Minimal delta.**
1. B256 as written: move the non-GUI debug exports out of the platform guard (they are
   declared in `src/hypersaw_debug.h` already). That is a `src/` edit to the frozen legacy
   shell, so it needs the lead's call under ADR-186.
2. In the same PR, make the driver carry each oracle's whole argument list from `verify`
   (the list is already parsed from there; only the capture is too narrow).
3. Until then, make the hole visible instead of green: the R2, R11 and SEC-input rows
   say "47 of 74 on CI" in their `gaps`. A catalogue edit, the lead's.

---

### H4 — A load made while not processing is split between a direct lane and the queue, and a direct host load does not supersede what is already queued (BY READING)

**Claim.** ADR-200 made a load decide its mode once. For the host-chunk door that holds
end to end. For the preset door it holds for the defaults, the morph field, the mod routes
and the name, and not for the parameter values, which are always queued. While the
instance is not processing that leaves a window in which the live state is none of the
patches involved, and two readers of that window are undefended.

**Evidence, from the code at `4a3dbaf`.**
- The preset door, `applyStateJson` (`src/hypersaw_clap.cpp:7665`): `initState` writes
  the defaults directly when the load is direct (`:7680`, `:7556-7565`); the mod routes
  are applied directly (`:7714`); the morph field is the live field (`:7636`); the name is
  set directly (`:7879`). Every parameter value then goes through `enqueueParam`
  unconditionally (the `load` lambda, `:7778-7781`, used at `:7788`, `:7799`, `:7810`). A
  direct load opens no batch (`:7630-7634`), so those entries are published one by one.
- The host-chunk door, `state_load`, applies its values directly on the direct path
  (`:10409-10424`).
- The queue is drained in three places only: the block start (`:9541`), `params_flush`
  (`:10121`) and the rig door (`:9783`). Taking direct ownership (`:2605-2618`,
  `:7628-7637`) neither drains nor clears it.
- `state_save` reads live storage through `readParam` (`:10158`) and never looks at the
  queue. The editor's writes are always queued (`guiSetParam`, `:7982-7984`).
- The repo's own row confirms the premise: `load_handoff_check` F-NODRAIN asserts a
  non-zero queue depth after a direct `hypersaw_debug_apply`
  (`tools/load_handoff_check.cpp:612-618`).
- One reader is already defended: the undo service refuses to snapshot until the queue
  has drained (`:8029`). The two below have no such test.

**What follows (not executed).**
1. *Save in the window.* Instance active or inactive but not processing, editor open.
   The player loads preset P. Until the host honours `request_flush`, the live parameters
   are the init defaults while the morph field, mod routes, intent chunk and preset name
   are P's. A host save taken then writes that mixture, labelled P.
2. *Host load in the window.* With entries pending (a knob edit, or P's values from 1),
   the host calls `state_load`. The direct load completes; the next flush or block then
   drains the older entries over it. The loaded chunk is not what stands.

Both contradict the storage rule this audit is asked to hold: "a load leaves the
instrument byte-identical to loading that patch into a fresh instance; every load path
gets the same rule". How wide the window is depends on when a host calls flush after
`request_flush`; it is closed by the first block once processing runs.

**A comment that no longer matches.** `:9776-9777` (the rig-door note): "Not processing,
the work was already done directly and nothing is drained." For the preset door the
values were queued, not done. `:7668-7670` says the opposite half: "Queued to the audio
thread — never applied directly from the GUI thread", which is no longer true of the
defaults, the routes or the field. `:7702-7703` likewise. All three predate the ADR-200
change (2026-10-09).

**The rows that would measure it** (additions to `load_handoff_check`, which is not
gated): I-SAVE, an idle preset apply followed at once by a host save, compared with the
chunk saved from a fresh instance that loaded the same preset and was flushed; and
I-SUPERSEDE, an idle queued write followed by a host load and then a flush, compared with
the same load on a fresh instance. Each beside its control (flush first: must match).

**Minimal delta if the rows go red.** A direct sequence owns the state, so it may drain
the queue before it writes, and the preset door may apply values directly when direct, as
the chunk door does. That is a change to the frozen legacy shell (ADR-186) and to a
contract ADR-200 just ruled, so it is the lead's call whether legacy gets it or it becomes
a binding rule for the horde 2 shell (B398).

---

### H5 — README describes the frozen legacy product, never mentions horde 2, and was last verified 30 days ago

**Claim.** The doctrine's clarity standard says a README that lies about the repo "is a
bug of the same severity as a failing test". `README.md` describes what ADR-186 froze on
2026-09-27 as "what it does today", and says nothing of what the repo has been building
since.

**Evidence.**
- `README.md:449` — "Last verified: **2026-09-10**". 521 merge commits have landed since
  that date.
- The words `horde 2`, `h2/`, `SCALPEL`, `H2-PLAN`, `armor` and `manual` do not occur in
  the file. `README.md:427` is the only line containing "legacy", and it means GUI1. The
  Map table (`:431-446`) has no row for `h2/`, `docs/H2-PLAN.md`, `docs/armor/`,
  `docs/manual/` or `docs/strategy/`.
- Ten further statements the tree contradicts:

| README | says | the tree says |
|---|---|---|
| `:39` | "CI builds no artifacts" | `ci.yml:183`, `:273` upload packaged `horde-windows` and `horde-macos`; a `release` job exists (`ci.yml:318`). No tag exists yet: that half is true. |
| `:56` | "HYPERSAW" is "the repository's name" | The repository is `horde` (the origin URL; CLAUDE.md §Domain, B437). |
| `:103-105` | four FX slots of Drive, Filter, Gain, Comp, Comb | `src/fx_rack.h:82-96` has nine types besides Off, with Notch, Echo, Room and Delay among them. |
| `:338` | "The instrument has no LFOs yet" | Two LFOs and two envelopes are mod sources (B171; `lfoenv_check`, `verify:684`; the `lfo=` state key, `src/hypersaw_clap.cpp:10232`). |
| `:406` | "every `tools/*_check` is wired or says why not" | True for three filename patterns only (M1). |
| `:411` | "124 tests" in `tests/feature_tests.tsv` | 204 (`test_table_check`, tonight). |
| `:441` | "197 of its controls are generated" | 238 (`gen_gui_controls.py --check`, tonight). |
| `:443` | "23 labs" in `docs/design/` | 45 HTML pages there; `serve_labs_check` counts 71 lab pages served. |
| `:458` | "Fourteen test-table rows have no oracle" | 16 (`test_table_check`, tonight). |
| `:463` | "ROADMAP B159 holds the ten whose reasons are still owed" | One check declares UNWIRED, with its reason (`tools/labharness/reverb_check.mjs:14`). |

- Not judged: `:407` "worst 4.262e-09". CI's Linux leg prints 2.485e-09 on `4a3dbaf`; the
  macOS number was not re-measured tonight.

**Why HIGH and not a list of typos.** Five of the ten are counts transcribed from a gate
that prints them, the class ADR-180 §1 ruled out of CLAUDE.md ("a number written here
rots"). The 2026-09-19 audit reported four such counts as M8; they were corrected and
have rotted again, which is the evidence that correcting them is not the fix.

**Minimal delta.** (1) Replace each transcribed count with the name of the gate that
prints it: removes five numbers, adds none. (2) One paragraph and two Map rows saying the
shipped plugin is the frozen legacy shell and the 1.0 product is horde 2
(`h2/`, `docs/H2-PLAN.md`). (3) The "last verified" date moves only when someone re-reads
the file. The voice of the README is the human's; the lead drafts, the human edits.

---

### H6 — ROADMAP.md is 1.67 MB, and the truth contract assumes it can be read

**Builds on** B403, which already records the duplicate ids B147 and B155 and four
unscoped legacy rows. Not re-reported; counted below because they are still open.

**Evidence (MEASURED by a scratch script over `ROADMAP.md` at `4a3dbaf`).**
- 1,668,552 bytes, up from 971,122 at the last audit: +72% in 21 days.
- The queue table is 451 rows and 1,007,065 characters, one row per line. Median row
  1,576 characters. The eight longest: B50 28,438; B448 17,453; B265 17,397; B446 14,232;
  B162 13,592; B441 13,576; B89 13,100; B126 10,218. No agent context holds the file, and
  a one-line row means every status edit rewrites the whole row in the diff.
- 449 distinct ids for 451 rows: B147 at lines 7132 and 7134, B155 at 7142 and 7143. The
  copies have diverged (5,530 against 5,370 characters; 3,415 against 1,123) and carry
  different status cells. Known since 2026-10-04 (B403), still open.
- **20 rows carry `**OPEN` and a MERGED, BUILT or DONE marker in the same cell:** B130,
  B340, B343, B345, B346, B348, B351, B361, B364, B365, B366, B368, B370, B373, B375,
  B376, B377, B382, B383, B385. Example: B364 reads "OPEN — dispatched" and then "MERGED
  2026-09-29 (#858)". B359 reads "OPEN — dispatched" while the gate it asked for runs at
  `verify:84`.
- 76 rows carry an explicit `**OPEN`. 69 of them contain none of "accept", "criteri" or
  "done when". The charter says: "Do not start work on an item whose acceptance criteria
  are missing or ambiguous." (A text heuristic: a row can state criteria in other words.
  The count says where to look, not that 69 rows are unworkable.)
- 70 rows have two columns, 369 have three, 12 have more. There is no status column: the
  status is wherever the last bold phrase happens to be. 287 rows have no bold status
  token in their last cell.
- B256 (H3) has no status at all and has grown 18-fold unobserved. The ten rows from the
  2026-09-26 seam audit, B277 to B286, read "PROPOSED, not dispatched" 14 days on.

**Criterion violated.** Truth contract: "ROADMAP.md is the single source of truth ... if
ROADMAP.md is wrong, fixing it is the first task." A source of truth that cannot be read
whole is consulted by search, and a search finds the first copy of B147.

**Minimal delta (the lead's act; subagents never touch ROADMAP).** (1) Merge the two
duplicate pairs. (2) Strike the stale leading status on the 20 contradictory rows. (3)
Decide a shape, and let a check hold it: a row is an id, a title, a status from a closed
vocabulary and a pointer; the narrative lives in the trace the row points at. A
`roadmap_check` of about 40 lines (unique ids, a status token from the list, a row length
ceiling) would have caught every item above. Adding a check is not gated. Moving closed
rows to an archive file is a larger act and the human's to approve.

---

## MEDIUM

### M1 — The wired-or-explained rule covers three filename patterns; five check files and three probe gates sit outside it

`tools/test_table_check.py:181-182` enumerates `tools/*_check.cpp`, `tools/*_check.py` and
`tools/labharness/*_check.mjs`. That is a hand-maintained list of patterns beside the
directory it describes, and the directory has outgrown it:

| outside the rule today | wired? | declaration |
|---|---|---|
| `tools/patchspace/dependency_tree_check.mjs` | `verify full` | `WIRED:` at line 3, unverified |
| `tools/patchspace/fidelity_scan_check.mjs` | `verify full` | line 4, unverified |
| `tools/patchspace/metrics_check.mjs` | `verify fast` | line 7, unverified |
| `tools/patchspace/listening_pass_check.mjs` | `verify full` | line 74, past the 40-line window |
| `tools/gui_webview_check.mm` | `verify full` | line 49, past the 40-line window |
| `tools/endprobe.cpp`, `kstuck_probe.cpp`, `rtsafety_probe.cpp` | `verify full` | none |

All eight happen to be wired. The finding is the next one: a new
`tools/patchspace/x_check.mjs` or `tools/x_check.mm` can be unwired and undeclared with
the gate green. The docstring's KNOWN BOUNDARY (`:69-73`) names `cpu_check` and
`alias_check` only. `README.md:406` and CLAUDE.md §Domain ("`test_table_check` enforces
wired-or-explained") state the rule without the boundary.

A second boundary, the same shape: 26 CMake targets are built by every `verify full` and
every CI build and invoked by nothing (L4). They are named `*_probe`, `*_bench`,
`measure_*` or `*_smoke`, so the rule does not ask them anything. `registry_dump` is one
of them, and it has aborted for an unknown time without anyone being told (B454 item 2).

**Minimal delta, about 3 lines, a strengthening (ADR-180 §1):** glob `tools/**/*_check.*`
and `tools/*probe*.cpp`. Two files will go red on arrival for their line-74 and line-49
declarations, which is the rule working. Whether benches and measures should declare
`UNWIRED:` is a decision for the lead; six of them already do.

---

### M2 — `verify fast` takes about two minutes; five gates are 57 to 61% of it, and one of them spends 97% of its time recomputing one answer

**Evidence (MEASURED twice tonight; each of the 43 commands in `fast()` run alone and
timed; load average 6 to 10 on 8 cores).** Totals 131.9 s and 122.4 s. CI's `verify-fast`
job took 109 s end to end on `4a3dbaf`.

| gate | sample 1 | sample 2 | what its comment in `verify` says |
|---|---|---|---|
| `divergence_ledger_check.mjs` | 20.3 s | 21.0 s | "~3 s" (`verify:454`; the tool's header `:8` too) |
| `build_flags_check.py` | 18.0 s | 14.9 s | "~0.3 s" (`verify:140`) |
| `metrics_check.mjs` | 13.2 s | 13.1 s | "~4 s" (`verify:443`) |
| `lab_load_check.mjs` | 13.2 s | 12.2 s | none stated |
| `filter_fidelity_check.mjs` | 11.0 s | 13.4 s | "~1.5 s" (`verify:360`) |
| `license_audit_check.py` | 8.6 s | 7.3 s | none stated |
| `sandbox_check.mjs` | 7.9 s | 6.8 s | "~2 s" (`verify:341`) |
| `lab_wheel_scroll_check.mjs` | 7.0 s | 6.1 s | "~3 s" (`verify:404`) |
| `deny_hook_check.py` | 5.4 s | 4.1 s | none stated |
| `serve_labs_check.py` | 4.6 s | 4.2 s | "~2 s each" (`verify:263`) |
| the other 33 commands together | 22.7 s | 19.3 s | |

The load does not explain the gap: the single-threaded Python gates with small claims
land on them (`param_id_lock_check` "~0.2 s", measured 0.11 s; `nan_latch_check` "~2 s",
measured 1.0 s). The stated costs of the top gates are 2 to 50 times too low. They are
numbers nobody has re-measured since the gates grew.

**Where the time goes, where a profile was cheap to take.**
- `build_flags_check.py` under cProfile: 33.7 s total, 32.5 s of it in `python_strings`
  (`:102`), called 3,010 times. The self-test re-blanks every Python file in scope once
  per planted control (`code_text`, `:139`, 1,632 calls for about 51 files and 32 scans),
  and each call re-parses the file. The blanked text of an unchanged file is the same
  every time.
- `license_audit_check.py`: 5.0 of 6.1 s in its self-test, through 287 child processes.
- `deny_hook_check.py`: 3.65 of 3.68 s in 148 child processes, one interpreter start per
  verdict row.

**Bit-identical speed-ups (verdicts unchanged, nothing weakened).**
1. Memoise `code_text` on the pair (path, content) in `build_flags_check.py`: about 5
   lines; expected under 1 s from 15 to 18 s. The planted controls edit one file per
   scan, so every other file is a cache hit. Not a gate change: same inputs, same rows.
2. Correct the eight cost comments. Seven sit in `verify`, a protected path: a sanction
   the human would have to give, and worth bundling with M8's comment fixes.

**A speed-up that needs a ruling.** The eleven node gates in `fast()` are independent
processes whose output is already captured to temp files (`verify:330-458`). Run
concurrently they are bounded by the slowest (about 21 s) instead of their sum (about
75 s). It changes no assertion. It edits `verify`, so it is the human's to sanction; it
is also the one change here that makes results depend on machine load, since at least
two of those gates already spawn their own workers.

`verify full` was not timed: this worktree has no build tree, and by `verify`'s own
comments its node and sanitizer stages alone sum past ten minutes
(`tsan_stress_check` "~5 min", `verify:986`). That is longer than the eight-minute
watchdog on a foreground agent command, so an agent can only run `full` in the
background. Worth stating in the brief template.

---

### M3 — Nothing records what a green `verify` skipped

A gate here can end three ways that all exit 0: it passed, it printed SKIPPED, or it
printed WARNING and judged less than it claims. `.harness/last-verify.json` records
target, exit, commit and time (`.kit/kit-gates.sh:23-28`). Which gates ran is in the
scrollback and nowhere else.

**What skipped in tonight's green run of `fast` in this worktree (MEASURED):**
- `license_audit_check`: "libs/choc is not checked out; the rows that read its files DID
  NOT RUN", and the same for `libs/clap` and `libs/clap-wrapper`. Three of three
  submodules. CI's `verify-fast` job checks out without submodules (`ci.yml:67-74`), and
  so does `docs.yml`. So the audit's central row, "its license text still reads as the
  SPDX id on record", is judged only in a checkout that has the submodules: the
  maintainer's. No CI job runs it at all.
- `choc_patch_check`: the APPLY, ORDER, ALONE, CRLF and UPSTREAM rows and five controls
  not run (stated in its WARNING; the build jobs apply the patches).
- `sluice_hold`: SKIPPED. `mailbox_delivery`: SKIPPED. `private_name_check`: runs here
  only because it resolves the names file through the main checkout.

**What can skip without a trace elsewhere.**
- `verify:329` and `:459-461`: with no `node` on the path, eleven gates in `fast()` are
  skipped under one message that names one of them ("lab load check SKIPPED"), and `fast`
  exits 0. `full()` treats the same condition as fatal (`verify:638`).
- The two self-digest gates and the bit-exact floor are keyed to exactly
  `darwin-arm64.appleclang-16.node24` (`h2/engine/selfdigest.*.txt`). This Mac runs Node
  v24.10.0 and Apple clang 16.0.0 today. A Node major bump or a Command Line Tools update
  turns all three into SKIP with exit 0 and a WARNING line among several hundred
  (`tools/h2_engine_selfdigest_check.cpp:46-48`, `:572-573`). Those digests are the gate
  that holds output-neutral work, the CPU campaign's C1 to C3b included, to bit identity.
- `rtsan_check` and `tsan_stress_check` print WARNING + SKIPPED without Homebrew LLVM
  (`verify:972-973`, `:986`); `conformance_check` skips where the vendored headers are
  absent (`verify:1038-1042`).

Each of these is individually deliberate and says so. The finding is the sum: after a
green run, neither the lead nor the dashboard can tell a full run from a reduced one.

**Minimal delta.** Every skip path already prints a line. Have each also append its name
to `.harness/skipped` (cleared at the start of a run), and have `verify` print one closing
line: how many gates ran, which were skipped. The armor dashboard can then show the last
run's skips beside the rows they belong to. About 15 lines across `verify` (a sanction)
and the Python gates (not gated). The eleven-for-one node message is a one-line fix in
the same place.

---

### M4 — `CMakeLists.txt` says nine checks are "NOT in ./verify"; `verify` runs all nine

`test_table_check` cross-checks the wiring claim in each check's own header. The same
claim is written a second time in the build file, where nothing reads it, and it is wrong
in nine places:

| `CMakeLists.txt` | says | `verify` |
|---|---|---|
| `:400-401` | `statefix_check` "Not in ./verify — that is a protected path and a human decision" | `:675` |
| `:403-405` | `undo_check` "NOT in ./verify — standing ruling: wiring a gate is the human's decision" | `:669` |
| `:413-414` | `polarity_check` "STANDALONE and NOT in ./verify" | `:683` |
| `:425-426` | `ncap_check` "STANDALONE and NOT in ./verify" | `:873` |
| `:446-447` | `tseed_check` "STANDALONE and NOT in ./verify" | `:874` |
| `:454-455` | `paramclass_check` "STANDALONE and NOT in ./verify" | `:853` |
| `:1082-1083` | `anchor_check` "Standalone, unwired (human gate)" | `:678` |
| `:1095` | `penv_check` "Standalone, unwired" | `:679` |
| `:1137` | `presetstore_check` "Standalone, unwired (human gate)" | `:676` |

`:316` adds an orphan: "Not yet in ./verify — that is a protected path and needs a human
gate", sitting between the SWARM-FX block and `registry_dump` and attached to neither.

The sentences also teach the rule ADR-179 §4 overturned on 2026-09-19 ("wiring a gate is
the human's decision"). A new author who learns the convention from the build file
learns the old one.

**Minimal delta:** delete the ten wiring sentences. About 14 lines removed, none added.
The header declarations are the single copy, and a gate already holds them.

---

### M5 — Engine-block values on the direct host-load path lack the "a load is not an edit" bracket (BY READING)

B125's rule is that a load's writes are never routed into a morph corner. It is
implemented by raising `loadingState` around `applyParam`, and `morphRouteEdit` tests it
(`src/hypersaw_clap.cpp:4025`). Four sites apply a load's value:

| site | bracket |
|---|---|
| `initState`, direct lane, `:7562-7564` | raised |
| `drainQueue`, kind 3, `:6118-6121` | raised |
| `state_load`, table parameters, direct lane, `:10418-10420` | raised |
| `state_load`, engine-block parameters, direct lane, `:10386-10387` | **not raised** |

`state_save` writes the table first, so the morph switch (id 151) is applied before any
`sub.` line is read (`:10156-10160`, then `:10171-10177`). On a direct load of a chunk
saved with morph on, each engine-block value therefore reaches `morphRouteEdit` with
morph on and the bracket down, and is treated as an edit. Whether that is audible depends
on what the chunk's `morph=` line, read later (`:10181`, `:10296-10299`), overwrites. That
was not traced further and not run.

`load_handoff_check`'s HOOK row covers the queued lane with routing cells
(`tools/load_handoff_check.cpp:22-25`, `:381-398`). No row loads an engine-block value on
the direct lane with morph on.

**Minimal delta.** A row first: HOOK-ENGINE-IDLE, a chunk with morph on and a non-default
`sub.` value loaded into an idle instance, corners compared with the reference. If it is
red, two lines at `:10386-10387` (the bracket the neighbouring site already has). The
second is a legacy `src/` edit, the lead's call under ADR-186.

---

### M6 — "Every default change carries an ADR and a migration" has one check on one key

CLAUDE.md §Domain states it as law (ADR-197; `docs/strategy/blind-spot-armor.md:191`,
edit E at `:215`). What enforces it:
- `os_default_check` (`verify:432`) holds one default, the lab's `os`, for the B445 flip.
  No catalogue row names it.
- `param_id_lock_check` locks id, name, stepping, automatable and range for 397 ids and
  says of the rest: "Defaults are NOT locked (a default is a different contract: ADR-197
  requires its own ADR plus a migration)" (`tools/param_id_lock_check.py:22-23`). It
  already extracts the default from every row (`:79`).
- `docs/armor/catalogue.json` has 23 rows. None is edit E.

A default is not only the fresh-instance sound. Since B192 an absent key loads at the
current default (`src/hypersaw_clap.cpp:7750-7754`), so every patch saved before a
parameter existed reads that parameter's default on every load. A changed default
changes those patches silently, and no gate, fixture or counter sees it unless a fixture
happens to omit the key.

The legacy shell is frozen, which bounds the exposure there. horde 2 has no parameter
manifest yet (the lock's `h2` section is a stub, B308 H4), so today the law has no
subject to bind. That is the moment to decide its gate, not after the first default
moves.

**Minimal delta.** (1) A catalogue row for edit E, as a hole with an owner and an expiry:
the lead's edit, and it only makes visible what is true. (2) When the h2 manifest lands,
the lock includes `default`, changeable only through the approval door the lock already
has. For legacy, adding `default` to the existing lock is about 6 lines and a re-pin.

---

### M7 — The armor catalogue lists three gaps that PRs #1020 and #1021 closed

`docs/armor/catalogue.json`, against `verify` at the same commit:

| row | `gaps` still says | but |
|---|---|---|
| R7 | "no append-only parameter-id check (Wave C)" | `param_id_lock_check` runs at `verify:128` and `:631` |
| R11 | "no audit of the full flag set" | `build_flags_check` runs at `verify:141` and `:632` |
| R12 | "there is no license allow-list audit (Wave C)" | `license_audit_check` runs at `verify:110` |

The merges landed hours before this audit and ROADMAP B448 says the lead updates the
catalogue after each merge, so this is lag, not neglect. It is recorded because of what
it shows about the mechanism: `armor_coverage_check` is red when a row names a gate
nothing runs, and silent when a gate is running that the row's own text says is missing.
The dashboard therefore understates tonight (0 of 12 green is right; three `gaps` are
wrong), and H3 shows it can overstate too.

**Minimal delta.** The three row edits (the lead's). Then one advisory line from
`armor_coverage_check`: gates added to `verify` since the catalogue last changed. Printed,
not failed, in the idiom `presentation_check` uses for its gaps.

---

### M8 — `verify` describes three past shapes of itself

`verify` is a protected path, so each of these is a sanction the human would have to
give. They are comment-only.

- `verify:1054-1055` — "Pan motion (ADR-064) is a KNOWN exclusion, printed loudly,
  pending its own ruling". ADR-177 §1 (B151) ruled it, and `tools/subdiv_check.cpp:182`
  prints "every case gated, no exclusions". The comment's own next clause is "an
  undeclared exclusion is how a gate rots into decoration". A declared exclusion that no
  longer exists misleads the same reader.
- `verify:865` — `blocksize_check` "(pan motion reported, B151)". Its header says pan
  motion "IS NOW GATED TOO" (`tools/blocksize_check.cpp:25-30`).
- `verify:62-65` — "Honestly-scoped day-zero oracle: structure + manifest sanity. The
  real L0 suite ... lands in Phase 1". `fast()` runs 43 commands today. `:614-616` says
  "Phase 0: the plugin builds cleanly ... Phase 1+ adds golden parity renders", above 500
  lines of exactly that.
- The seven cost figures in M2.

`verify` grew from 421 to 1,168 lines since the last audit, and most of the growth is
comment. Those comments are the best documentation the gates have, which is why the
stale ones matter.

---

### M9 — Document drift: the full list is Appendix A

The lead asked for every false statement with its line and the contradicting fact.
Appendix A has 31 across `CLAUDE.md` §Domain, `h2/README.md`, `docs/ENGINEERING.md`,
`docs/ROBUSTNESS.md`, `docs/PARKED.md`, `TESTING.md`, `CHANGELOG.md` and `SESSION.md`.
With H5's eleven, M4's ten and M8's four that is 56. Three patterns account for most:

1. **A transcribed count.** About 15 of the 56. ADR-180 §1 already rules the answer for
   CLAUDE.md: name the gate that prints the number.
2. **A candidate that shipped, or was archived.** CLAUDE.md §Domain still reads as an
   ingest log: eight "CANDIDATE arrived" paragraphs, of which one is now the product
   (the composed engine), one was renamed, one became a hosted sibling module, one was
   ported, and three are parked or archived. B403 records this; the appendix gives the lines.
3. **A dated protocol nobody follows.** `TESTING.md:3-4` says every PR that changes
   human-testable behaviour updates the file. Its last commit is 2026-08-03, 876 merges
   ago, and its build under test is the one after PR 135. `CHANGELOG.md` says each line is
   one merged change set; it was last touched 2026-09-10, 521 merges ago. `SESSION.md` is
   a handoff note "closed 2026-08-28". Each either gets a date line that says it is
   historical, or is retired. Retiring a file is a human gate.

Also measured: `playbook_check` reports 143 of the playbook's 186 line citations drifted
(advisory by design). It was 117 on 2026-09-26.

**Clean bill inside this item.** `docs/manual/` (B451): all 106 distinct ROADMAP ids it
cites exist, every ADR it cites exists, and the only two paths that do not resolve are
the two future directories its README says are future.

---

### M10 — Eight findings of the 2026-09-19 audit are still open; one has got worse

| then | status tonight |
|---|---|
| M1 gui2's matrix stub pinned to an old layout | **Worse.** `src/gui/gui2.html:1981` still builds one source (`NSRC` is 1). The shell is at three (`kRoutingNSrc`, `src/hypersaw_clap.cpp:980`); it was two. The only automated GUI load exercises a one-source pane. |
| M3 host name against GUI label disagreements | Open. `presentation_check` prints no such count. |
| M5 the CPU audit's "over budget" table | Open. `docs/research/2026-08-24-cpu-audit.md:14-15` unchanged; its section 6 still contradicts it. |
| M7 landing page misses `reference/subosc.html` | Open. `index.html` has no link to it. |
| L1 `support.js` committed twice, byte-identical | Open, and now a license matter: `license_audit_check` reports it PENDING THE HUMAN, "tracked twice under docs/". |
| L2 three lab harnesses, three FFTs | Open. No shared rig. |
| L5 a stale scratch tool | Open. `tools/scratch_b18_two_osc_matrix.cpp`, no CMake target. |
| L6 two audit directories | Open. A root-level `audits/` directory still holds one file from 2026-08-13 beside `docs/audits/`. |

Closed since: H1 (both gates wired, `verify:873-874`), H2 (`src/hypersaw_debug.h`), H3
(parked, `docs/PARKED.md` 21), H4 (the binary is gone), H5 (`docs.yml`), M2 (held by
`playbook_check`: "morphLayout 9 at 4 writers"), M4 (`binary_hygiene_check`), M6 (no
dead card tonight). M8 was corrected and has rotted again (H5).

---

## LOW

### L1 — Five functions nothing calls

Found by a sweep for definitions whose name occurs exactly once across `src/`, `tools/`
and `h2/`, then read:

| function | where | introduced |
|---|---|---|
| `typeOf(int slot)` | `src/fx_rack.h:274` | not traced |
| `shadowTypeOf(int slot)` | `src/fx_rack.h:373-377` | B117 (`e41643e`, 2026-09-13) |
| `processSlotWet(...)` | `src/fx_rack.h:564-568` | ADR-095 (`ed30fd8`, 2026-08-19) |
| `depthOf(int i)` | `src/undo_tree.h:192` | not traced |
| `bandTime(int i)` | `src/time_core.h:374` | not traced |

About 20 lines. Deleting a function from a frozen legacy header is small but is still a
`src/` edit: bundle it, do not dispatch it alone.

### L2 — The oscillator stride is a named constant in 23 places and the literal 1000 in four

`kOscStride` is defined at `src/hypersaw_clap.cpp:832`. Four sites write the number:
`:3436`, `:3485`, `:7476` (the preset writer) and `:7810` (the preset reader). The chunk
writer and reader use the constant (`:10166`, `:10391`). Two doors for one layout, one of
them spelled by hand. Net zero lines.

### L3 — A load asks the host to flush once per queued entry (estimated; bit-identical to change)

`enqueueParam` calls `request_flush` on every entry (`src/hypersaw_clap.cpp:6087`),
including inside an open batch whose head is not yet published (`:6085`), where a flush
would find nothing. `endQueueBatch` then asks again (`:6103`). By the comment at
`:2497-2498` one load peaks at 1,471 entries, so a queued load makes about 1,470 host
callbacks where one is useful. Not measured: no probe times the load path.
`load_handoff_check`'s STOP row uses the first of those callbacks as its hook, so "the
first entry and the end of the batch" keeps the rig and removes the rest. Output cannot
change: the queue contents are the same.

### L4 — 26 executables and three plugin bundles are built by every build and run by no gate

**Builds on** B454 item 2 (`registry_dump` is broken and unrun) and answers the lead's
question 4.

- The 26: `registry_dump`, `corner_probe`, `enable_probe`, `user_patch_bench`,
  `shell_bench`, `save_bench`, `glidepath_probe`, `tailprobe`,
  `paramfunc_smoke`, `paramleak_probe`, `ratchet_probe`, `robustness_matrix`,
  `pitch_probe`, `morphscope_probe`, `bendsweep_probe`, `gain_probe`,
  `render_neutral_digest`, `preset_probe`, `cpu_bench`, `renderer_bench` and six
  `measure_*` sources, two of which are built a second time (`cpu_check`,
  `measure_h2_engine_stages`). `vst3_save_bench` is the macOS-only twenty-seventh.
  `renderer_bench` runs in CI with its exit ignored (`ci.yml:204`); `cpu_bench` and
  `user_patch_bench` are rebuilt by the hand-triggered `cpu-derate` workflow.
- **What keeping them costs in time (MEASURED):** each translation unit compiles in 0.4
  to 1.3 s at -O2 here, except `measure_h2_engine` at 5.0 s (and it is built twice).
  24.1 s of serial compile for the 26 timed (three of them stopped early under the
  audit's ad hoc flags, so the true figure is a little higher), a few seconds of wall
  time at -j8. The parked shell `src/swarmfx_clap.cpp` compiles in 0.85 s at -O3 and its
  three bundles are 1.1 MB.
- **What it costs in truth.** Nothing runs them, so "it builds" is the only thing known
  about each. `registry_dump` is the proof: it aborts, and the first anyone knew was a
  flag-pinning exercise that happened to execute it. `verify:940-942` already names the
  class: "a built, passing probe reads as coverage".
- **SWARM-FX's delete trigger can no longer fire.** `docs/PARKED.md:27` (entry 21) says
  the shell is deleted when "Track E1 folds into horde's rack (B50)". The legacy rack is
  frozen (ADR-186), and ADR-190 A7 places comb and notch in horde 2 as filter types, not
  as E1 modules. The entry's other trigger is the human wanting a standalone FX product.
  So the shell compiles on three platforms until a ruling that nothing is waiting for.
  An exclusion with no expiry.

**Minimal delta.** No deletion is proposed here (a human gate each). Two cheap acts:
(1) a smoke row, each never-run target executed once with `--help` or its shortest mode
in `verify full`, exit code only, so "builds" becomes "starts"; and (2) a ruling from
the human on SWARM-FX now that its trigger is dead.

### L5 — Three tool files with no build target and no invoker

`tools/scratch_b18_two_osc_matrix.cpp` (107 lines; cited by one research note),
`tools/blep_alias_incommensurate_probe.cpp` (190; cited by `docs/MEASUREMENTS.md` as the
source of a number; its own header calls it a scratch tool, and nothing in the tree
builds it), `tools/port_gap.py` (165; referenced only from ROADMAP and traces). 462
lines. The second matters more than the others: a published measurement whose tool no
target builds is the shape the last audit's H4 found in `dist/`.

### L6 — Two small things in `h2/`

- `h2/engine/engine.h:78-133` defines eight function-like macros (`H2E_KERNEL_HIT`,
  `H2E_FAULT`, `H2E_EPS`, `H2E_SKIP` and their siblings) and never undefines them, so
  they are visible in every file that includes the engine. Harmless today with one
  includer per target; the h2 shell (B398) will be the first translation unit that
  includes the engine beside other code.
- `h2/README.md` says an output-neutral PR "re-runs `tools/h2_libm_count.py --write` so a
  moved libm call shows in its diff". Nothing compares the committed
  `h2/engine/libm-calls.*.txt` with a fresh count, and the tool declares itself UNWIRED.
  The relationship is prose. Either the file is context nobody should rely on (say so in
  its first line) or `verify full` regenerates and compares it.

### L7 — Where checks overlap (the lead's question 1)

| pair or group | what is asserted twice | proposal |
|---|---|---|
| `samplerate_check` and `sr_check` | envelope-attack drift across rates: `sr_check` re-measures it as its own must-read-zero control | Keep both; `samplerate_check` is named by a sibling's criterion. One header line saying which owns the bar. |
| `subdiv_check` and `blocksize_check` (351 lines) | chunk invariance at 44.1 kHz for the six chunk sizes both lists hold | One rig taking the rate as a parameter; about 120 lines fewer, the same rows. |
| the five golden-parity rigs `filter`, `notch`, `time`, `spectra`, `swarmalator` (884 lines) | each re-implements the manifest reader, the float reader, the RMS loop and the verdict line | One shared header. It is also where H1's floor would live once instead of eight times. |
| `nan_latch_check.py`, `paste_cap_check.py`, `embedded_page_policy_check.py` (325 lines) | each compiles one C++ check with the host compiler and runs planted variants | One driver of about 40 lines. |
| nine comment or string blankers in eight Python gates (`banned_api_check.py:49`, `build_flags_check.py:83` and `:139`, `choc_patch_check.py:292`, `h2_rules_check.py:73`, `gui_sink_check.py:279`, `playbook_check.py:119`, `param_id_lock_check.py:62`, `tolerance_registry_check.py:150`) | "judge code, not comments" | One module. A blanker that misses a string form blinds its gate, and nine copies are nine chances. |
| three parsers of "what `verify` invokes" (`test_table_check.py:91`, `armor_coverage_check.py:96`, `sanitize_oracles.sh:32`) | the gate list | One parser. The third already drops arguments (H3). |
| `parity_check` and `h2_swarm_parity_check` | everything | Leave: a verbatim copy by ruling, ledgered and held by `h2_lift_check`. |
| `rtsafety_probe` and `rtsan_check` | no allocation on the audio thread | Leave: the first runs anywhere over ids 1 to 99, the second needs the sanitizer toolchain and covers 397. |

Nothing here removes an assertion. Estimated net: about 500 lines fewer and three shared
files more.

### L8 — Loop hygiene, and one number for the human

- `LIBRARY.md` holds 70 entries. The charter's consolidation trigger is "exceeds ~30".
- The auditor cadence is seven days (CLAUDE.md §Domain). 21 days passed between repo
  audits, with three scoped audits in between (2026-09-26, 09-30, 10-02). B190 already
  proposes triggering on merges; this is the measurement that supports it.
- 175 trace files are dated since 2026-09-19 against 390 merge commits. The charter says
  every merged change gets a trace. Many of those merges are record-only PRs, so this is
  reported, not judged.
- Since the last audit, across `src/`, `tools/`, `h2/`, `verify` and `CMakeLists.txt`: 240
  files changed, 73,315 lines added, 930 removed. `tools/` went from 116 tracked files to
  235. The charter's rule is "every new abstraction must displace at least as much
  complexity as it introduces". Three weeks of security and armor work are additive by
  nature; the ratio is 79 to 1, and L7 is where some of it can come back.

---

## The lead's six questions, answered by finding

| question | answer |
|---|---|
| 1. Which checks overlap or assert the same thing twice? | L7 (eight groups; about 500 lines recoverable, no assertion removed). |
| 2. Which files does no check reach, and which checks pass with their subject deleted? | Reached by none: `src/swarmfx_clap.cpp`, `swarmfx_entry.cpp`, `swarmfx_entry.h` (parked, known). Every other `src/` and `h2/` file is compiled into or read by at least one wired gate, directly or through an include; the one exception is `src/hypersaw_clap_entry.cpp`, the bundle entry, which only CI's pluginval reaches. Four cores live only through their own check and are included by nothing in the product: `station_core.h`, `strata_core.h`, `svf_core.h`, `swarmalator_core.h`. Pass with the subject deleted: the eight gates of H1 (proven); the Stop gate (H2); 27 of 74 CI sanitizer verdicts (H3). |
| 3. Stale documentation? | H5 (README), M4 (CMake), M8 (`verify`), M9 and Appendix A (the rest). `CODEMAP.md` does not exist in the tree and never has; nothing links to it. The manual scaffold's citations are clean. |
| 4. Dead or parked code that still compiles, and its cost? | L4 (26 targets, about 24 s of serial compile, 1.1 MB), L1, L5. The cost is trust, not time. |
| 5. ROADMAP hygiene? | H6. |
| 6. `verify`'s runtime? | M2 (measured, with one bit-identical fix worth about 15 s and one that needs a ruling worth about 50 s). |

## What changed since the last audit

- **Closed:** eight of its 21 findings (list under M10). Its structural proposal, the
  wired-by-default rule, became ADR-180 §1 and is the reason 115 check files now declare
  and prove their wiring.
- **Open:** eight (M10), one of them worse. The other five were not re-checked (its L3,
  L4, L7, L8) or were fixed and have recurred (its M8, now H5).
- **The tree:** 1,193 commits, 390 merges. `verify` 421 to 1,168 lines. `tools/` 116 to 235
  files. `ROADMAP.md` 0.97 to 1.67 MB. A second product tree (`h2/`), a security overhaul
  (B446), a thread-handoff discipline (ADR-200) and the armor catalogue (B448) all landed.
- **The pattern that moved.** The last audit's leading class was "built and never run".
  ADR-180 §1 closed it for files named `*_check`. Tonight's leading class is one level
  up: gates that run and cannot fail (H1), run and are not looked at (H3, M3), or exist
  and are not reached (H2). Six of the nine shapes the charter lists were found again.
  Not found: insertion into positional data (`param_id_lock_check` and
  `morphlayout_check` hold it), absent-means-keep (held since B192), and nothing new
  under reachable-but-unrecorded beyond B193, which is known.

## Reduction budget

| finding | removes | adds |
|---|---|---|
| H1 | 0 | 16 lines (8 floors, 8 constants) |
| H2 | 0 | about 12 lines in two hooks; about 10 rows in an existing check |
| H3 | 0 | about 10 lines in the driver; the `src/` move is net zero |
| H4, M5 | 0 | 3 rows in `load_handoff_check` (about 90 lines); fixes only on evidence |
| H5 | 5 transcribed counts | 1 paragraph, 2 Map rows |
| H6 | 2 duplicate rows, 20 stale statuses | 1 check, about 40 lines |
| M1 | 0 | 3 lines |
| M2 | 13 to 17 s from every `fast` run | 5 lines |
| M3 | 0 | about 15 lines |
| M4 | 14 lines | 0 |
| M6 | 0 | 1 catalogue row; about 6 lines when the lock takes defaults |
| M7, M8 | 0 | 3 row edits; about 12 comment lines rewritten |
| L1 | about 20 lines | 0 |
| L5 | 462 lines, 3 files (a human gate each) | 0 |
| L7 | about 500 lines | 3 shared files |
| **total** | **about 1,000 lines, 3 files, 15 s per fast run** | **about 210 lines, 4 files, 1 check, about 13 rows** |

No finding removes a check, an assertion or a tolerance.

## A clean bill, where the sweep found one

- **The manual scaffold** (`docs/manual/`): every cited ROADMAP id and ADR exists.
- **The lab index**: every page under `docs/design/` is indexed and no card is dead.
- **The directory-driven gates have floors**: `statefix_check`, `bank_check` and
  `offcorner_check` all go red on an empty directory (run tonight).
- **`h2_engine_parity_check`** pins its scenario set and its truncation control is
  planted on every run.
- **`sandboxed_node.mjs`** refuses any script without a profile row, and `sandbox_check`
  scans for evaluators that skip the guard. A hand list, held by a scanner: the right way
  round.
- **No dead function** was found in `src/hypersaw_clap.cpp` (325 definitions),
  `src/swarm_core.h` or the four `h2/engine/` headers, and there is no TODO, FIXME or XXX
  in `src/` or `h2/engine/`.
- **By reading only:** `utf8SeqLen` (`src/input_guards.h:28-47`) rejects overlong forms,
  surrogates, values past U+10FFFF and truncated sequences; `deferEvents`
  (`src/hypersaw_clap.cpp:2726-2736`) bounds a host-claimed size before rounding it.

## Not swept, stated rather than guessed

- **`verify full` was not run.** This worktree has no build tree and no submodules, and
  the run exceeds the command watchdog. The eleven binaries in H1 came from the main
  checkout's existing Release build; nothing was built for this audit except single
  translation units into a scratch directory for L4's timings.
- **Per-sample cost** in `h2/engine/engine.h` and the legacy cores was not measured. The
  CPU campaign (B441) keeps its own ledger, and a timing taken tonight under a load
  average of 6 to 10 would not be worth recording.
- **H4 and M5 were not executed.** They are read from the code. The private notes that
  record ADR-200's accepted risks are outside the tracked tree and were not read, so
  either finding may restate something already accepted there. The lead should check
  before turning them into rows.
- **Storage branching** (a to b to a) was taken on `undo_check`'s word, not re-derived.
- **`reference/`**, the lab pages' content and the 434 traces were not read.

## Proposed ROADMAP rows

The lead assigns ids and wording. Each is one dispatchable item with its acceptance test.

| # | row | from | acceptance |
|---|---|---|---|
| 1 | **A scenario floor on the eight golden-parity gates** | H1 | Each gate run against an empty manifest exits non-zero; each prints the floor it holds; `verify full` green. |
| 2 | **The Stop gate sees shell edits and an absent record** | H2 | A planted shell edit in a scratch repo with no record blocks; a record from another commit blocks; both rows live in `deny_hook_check`. A brief to the kit carries the same change. |
| 3 | **B256, re-scoped: 18 oracles never run on CI** | H3 | The sanitize job reports 0 "not built" for the debug-export cause; the driver passes every argument `verify` passes; rows R2, R11 and SEC-input state the real count until then. |
| 4 | **Three rows for idle loads** | H4, M5 | `load_handoff_check` gains I-SAVE, I-SUPERSEDE and HOOK-ENGINE-IDLE, each with its control. A red row becomes its own fix row; a green one closes the finding with the measurement. |
| 5 | **README and CLAUDE.md §Domain say what the repo is** | H5, M9, B403 | No transcribed count remains in either; horde 2 and `h2/` appear in both; "last verified" is the day it was re-read; `TESTING.md`, `CHANGELOG.md` and `SESSION.md` each carry a status line or are retired by the human. |
| 6 | **ROADMAP gets a shape and a check** | H6, B403 | Unique ids; a status token from a closed list on every row; the 20 contradictory rows resolved; `roadmap_check` in `verify fast`. |
| 7 | **Widen wired-or-explained to every check and probe** | M1 | The glob covers `tools/**`; the two late declarations are moved into the window; the count the gate prints rises by eight. |
| 8 | **`verify fast` cost** | M2 | `build_flags_check` under 2 s with identical rows; the cost comments re-measured (a sanction); the human rules on running the node gates concurrently. |
| 9 | **A skip census** | M3 | A green run prints how many gates ran and names each one skipped; the dashboard shows the last run's skips. |
| 10 | **One dead-and-stale PR** | M4, M8, L1, L2, L5 | The ten CMake sentences gone; the four `verify` comments corrected (a sanction); five functions and four literals gone; the three orphan tools ruled on. |
| 11 | **A gate for the default-change law** | M6 | A catalogue row for edit E now; `default` in the id lock when the h2 manifest lands. |
| 12 | **Catalogue rows R7, R11, R12** | M7 | The three `gaps` texts match `verify`; `armor_coverage_check` prints gates added since the catalogue last changed. |

## Appendix A — false statements, with the fact that contradicts each

README's eleven are in H5, `CMakeLists.txt`'s ten in M4, `verify`'s four in M8. B403
already records that CLAUDE.md §Domain is stale; this is the line-level list it asks for.

| file:line | says | the tree says |
|---|---|---|
| `CLAUDE.md:76-78` | the formant engine is "the first new member ... not yet in the shell" | Archived 2026-09-28 (B330; `docs/PARKED.md` entry 23; `docs/H2-PLAN.md:626`). |
| `CLAUDE.md:81`, `:96-98` | "MAW is FX-C now", "named MAW 2026-09-16" | Renamed: "Shriek (formerly MAW)" in the frozen roster (ADR-190, `DECISIONS.md:6859`). |
| `CLAUDE.md:90-95` | NETWORK's placement "is the human's open ruling on B127" | Ruled: it is its own sibling project, hosted under the rack-slot contract (B127's own row, `ROADMAP.md:7113`; ADR-201). |
| `CLAUDE.md:100` | ADR-169 "PROPOSED" | Ruled and amended five times; Amendment 5 is dated 2026-10-09 (`DECISIONS.md:7441`). |
| `CLAUDE.md:104` | SCALPEL is "an eighth CANDIDATE" whose "integration shape ... is the open question" | It is horde 2's one engine, built at `h2/engine/` (ADR-186, B327, B385; `h2/README.md:5-9`). |
| `CLAUDE.md:113` | "`libs/` submodules pinned: clap 1.2.10, clap-wrapper v0.15.1" | Three submodules; `libs/choc` is the third (`.gitmodules:8`) and carries patches (ADR-196). |
| `CLAUDE.md:116` | "seven single-file HTML prototypes plus one CANDIDATE" | `reference/` holds 17 HTML files and two packet directories. |
| `CLAUDE.md:123-126` | "Ported C++ cores so far" names six | `src/` holds 17 `*_core.h`; `h2/` holds four core headers and the engine. |
| `CLAUDE.md:126` | "The E1 effects ship today only in a SECOND CLAP shell" | `src/fx_rack.h:30-31` includes `notch_core.h` and `time_core.h`; Notch, Echo and Room are rack slot types (`notchslot_check`, `verify:1024`). |
| `CLAUDE.md:164` | "the prototype HTMLs — now seven" | The same sentence goes on to name nine more. |
| `CLAUDE.md:181` | "golden render fixtures once they exist" | They exist: `tests/state_fixtures/`, `tests/morph_order.txt`, `h2/engine/selfdigest.*.txt` (their header: "A golden fixture (protected)"), the five blobs pinned in `h2/README.md`. None is named in the protected-paths list. |
| `CLAUDE.md:183-186` | "`fast`: leak gate + structure/manifest sanity now; grows the L0 suite (parity + trajectories) from Phase 1" | `fast` runs 43 commands and no parity; parity is in `full` (`verify:641`). |
| `CLAUDE.md` §Domain, Stack and Protected paths | (silent) | Neither names `h2/`. The one mention of horde 2 is `:138`. |
| `CLAUDE.md:155` | a repo audit "newer than seven days" | 21 days elapsed (L8). |
| `h2/README.md:20` | "Last verified: 2026-10-01" | Predates the CPU campaign C1 to C3b (B441), the self-digest gates' coverage rows and the os default flip (ADR-191). |
| `h2/README.md:61` | the divergence ledger "holds 7 entries" | Eight: D5 is planned (`docs/port/divergences.json`). |
| `h2/README.md:80-81` | "The one declared exception is `h2_scalpel_fma_control`" | Two: `h2_engine_fma_control` as well (`CMakeLists.txt:989`; `build_flags_check` prints two). |
| `docs/ENGINEERING.md:49-50` | "117 RULING and 70 ENCODING rows, 103 agentic and 84 human" | 204 rows, 116 agentic, 88 human. |
| `docs/ENGINEERING.md:51` | "the 14 rows with `oracle=none`" | 16. |
| `docs/ENGINEERING.md:62` | "159 `## ADR-` headers" | 209. |
| `docs/ENGINEERING.md:65` | "`traces/` (181 entries)" | 433. |
| `docs/ROBUSTNESS.md:3` | "Last measured: 2026-09-10 ... If this date is old, regenerate" | 30 days; `robustness_matrix` is built and run by nothing (L4). |
| `docs/ROBUSTNESS.md:72` | pan motion is the "known exclusion `subdiv_check` prints every run" | Retired by ADR-177 §1; `tools/subdiv_check.cpp:182`. The level variation that paragraph explains by it is now unexplained. |
| `docs/ROBUSTNESS.md:118-119` | "23/24 oracles GREEN; `state_check` NOT BUILT" | 53 passed, 18 not built (H3). |
| `docs/ROBUSTNESS.md:153-163` | "ASan and TSan cannot be measured on this Mac ... no Homebrew LLVM" | Installed 2026-10-09 (ADR-199); `rtsan_check` and `tsan_stress_check` run here (`verify:975`, `:988`). |
| `docs/ROBUSTNESS.md:176-178` | the parameter queue "is not exercised by any oracle and is UNMEASURED by TSan" | `tsan_stress_check` exercises it (ADR-200). |
| `docs/ROBUSTNESS.md:182-183` | a search of `src/` for denormal handling "returns no hits" | It returns hits: `src/subosc_core.h:198-200`, `src/station_core.h:584`. |
| `docs/PARKED.md:27` | SWARM-FX is deleted when "Track E1 folds into horde's rack (B50)" | That trigger cannot fire (L4). |
| `TESTING.md:3-8` | every PR that changes human-testable behaviour updates this file; build under test is the one after PR 135 | Last commit 2026-08-03; main is at PR 1021. |
| `CHANGELOG.md:5` | "Each line is one merged change set" | Last touched 2026-09-10; 521 merges since. |
| `SESSION.md:1` | "closed 2026-08-28" | A root-level handoff note 43 days old; nothing marks it historical. |

## The five to act on first, and one question

1. **H1** — sixteen lines put a floor under L0-1 and seven other parity gates. Nothing
   else here buys as much for as little.
2. **H2** — the Stop gate is what every "verified before finishing" claim rests on, and
   three of the four agent roles can only write through the path it cannot see.
3. **H4's rows** — measure before anyone argues. Two rows in an existing check say
   whether an idle load can be saved or superseded half-applied.
4. **H3** — B256 with its real size. The checks written for hostile input are the ones
   CI never runs.
5. **H6** — the ROADMAP's shape. Every other row in this report becomes a line in a file
   no session can read whole.

**The one question for the human.** The legacy shell is frozen (ADR-186), and four
findings here end in a change to it: H3's debug exports, H4 and M5 if their rows go red,
and L1. Does the freeze admit correctness fixes to the legacy state and load path, or is
legacy now changed only for security, with everything else recorded as a rule for the
horde 2 shell (B398)? The answer decides whether those four are fix rows or rule rows.
