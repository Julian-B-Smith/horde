# Independent audit — 2026-11-10 (Fable 5.2)

**Origin.** horde lead session (Opus 5.5), 2026-11-10, written at the human's request: a general
audit by a fresh Fable 5.2 session, independent of the sessions that built the repo.
**Commit audited** `622b0cf3` (`origin/main`, PR #1490 merged), in a fresh worktree.
**Visual summary** [`2026-11-10-fable-audit.html`](2026-11-10-fable-audit.html): the top ten on one page.

**Method.** I formed my own view first and read `docs/audits/` only afterwards. Three subagents
read in parallel, each pinned (two Opus, one Sonnet), none shown the earlier audits: the engine's
hot path, gate quality, and doc drift. I re-checked every claim of theirs that appears below against
the code or a command, and say where I did not. `nice -n 10 ./verify fast` ran twice in the
worktree (both exit 0), once under `time` and once with every output line timestamped.
Nothing outside this file and its HTML summary was edited.

**Rules of evidence.** VERIFIED: I ran the command or read the cited lines today. HYPOTHESIS: it
follows from reading but was not measured. Every number below was produced in this session unless
it is attributed to someone else's record.

**Who decides.** *Human*: needs the human's ruling or hand (a gate weakened, a spec or protected
path, a workflow of theirs, a push only they make). *Lead*: the lead can schedule it.

---

## The ten, ranked by value against cost

| # | finding | cost | who | status |
|---|---|---|---|---|
| 1 | The clean-room engine and its tests have 85 and 69 commits on one disk only | two pushes | Human | VERIFIED |
| 2 | The nightly workflow was drafted on 2026-10-10 and never installed; its six jobs have never run | one file | Human | VERIFIED |
| 3 | The new engine rebuilds its whole parameter context several thousand times a second | S, bit-identical | Lead | VERIFIED by reading; share is the builder's figure |
| 4 | `./verify fast` takes 4 min 25 s and runs one gate at a time; five gates are 69 % of it | S to M | Lead | VERIFIED |
| 5 | 37 % of merged PRs are records-only, and the human merges every one | S, a workflow change | Human | VERIFIED |
| 6 | ROADMAP.md is 2.1 MB with no status field; no session can read it and GitHub cannot show it | M | Human, then lead | VERIFIED |
| 7 | CI's sanitize job is green with 19 oracles not built (fourth audit) | about 5 lines | Lead | VERIFIED |
| 8 | A gate that skips leaves the same green record as a gate that ran | S | Lead | VERIFIED |
| 9 | The compiled preset oracles run on no PR in CI, and a preset-only PR skips them locally too | S | Lead | VERIFIED by config |
| 10 | The CPU budget still has no gate, and the armor dashboard cannot change colour | M | Lead, human for numbers | VERIFIED |

---

### 1 — The clean-room engine exists on one disk

**Saw.** In the clean-room engine repo: `main...origin/main [ahead 85]`, `origin/main` last moved
2026-10-27, `HEAD` is 2026-11-07. In the clean-room tests repo: `[ahead 69]`, same dates. Both
remotes are private GitHub repos. That is every commit since stage 8: the bank stage, the morph for
1.0, the three CPU batches, and spec rounds up to 64.

**Why it matters.** This was H1 of the 2026-10-21 audit and H1 again on 2026-10-29 (then 12 and 15
commits). The rule that closed it (B533: push after each stage acceptance and record the heads) has
not been kept since 2026-10-24. A disk fault now costs two weeks of engine and test work that
cannot be rebuilt from horde.

**Fix.** The human pushes both repos. Then the lead's session roundup prints one line every session
until it reads zero: commits on each clean-room `main` that its origin lacks. Two `git rev-list
--count` calls; no gate involved.

**Cost.** Two commands now; about ten lines for the roundup.
**Wrong if.** A backup I cannot see (Time Machine, a second remote) already holds those commits.
**Who.** Human (the push), lead (the roundup line).

### 2 — The nightly has never run

**Saw.** `gh workflow list --all` shows `ci`, `cpu-derate`, `docs`. `python3
tools/nightly/freshness.py` prints `NIGHTLY UNKNOWN -- gh failed: HTTP 404: workflow nightly.yml not
found on the default branch`. `tools/nightly/README.md` calls `nightly.workflow.yml` a "DRAFT of
the workflow. The human installs it". The draft is dated 2026-10-10 (`git log`).

**Why it matters.** `tools/nightly/jobs.json` holds six jobs that run nowhere else on a schedule:
the one-hour legacy stability run, the deep engine fuzz, RTSan on the legacy `process()`, the TSan
stress test, and two long soaks. Armor rows R1, R2, R5 and R6 describe them as coverage. For a
month the runner, its drill and its freshness reader have been checked every `verify fast`
(`nightly_check.py`) while the thing they serve never ran. Carried from the 2026-10-29 audit (M8).

**Fix.** Copy `tools/nightly/nightly.workflow.yml` to `.github/workflows/nightly.yml` in one PR and
run the drill once. If the human would rather not spend the CI minutes, say so in the armor rows
and delete the six jobs; the present state claims both.

**Cost.** One file. CI minutes per night are the human's call (I did not estimate them).
**Wrong if.** The nightly runs from somewhere other than GitHub Actions.
**Who.** Human (workflow files are theirs to install, per the README).

### 3 — The engine rebuilds its parameter context thousands of times a second

**Saw (clean-room engine repo, `e72a092`).** `HordeEngine::process` calls
`voices_.run_actions(context(), …)` at every segment start (`engine2/core/api/engine.cpp:207`),
whether or not a voice has an action due. `tick()` builds it again at each control tick and each
grid tick (`:185`, `:189`), and each voice with its own offsets builds another. `context()` reads
every setting through `param_index`, which is a `std::lower_bound` over the manifest
(`engine2/core/manifest/manifest.cpp:370`), and log-scaled rows then take a `pow`.

**Why it matters.** The builder's own profile of a blades-off note start (`REPORT.md:125-131` in
that repo) puts `context()` for `run_actions` at about 18 % of `process()` and the control ticks
(context plus voice control) at about 28 %. Blades retire under ADR-387, so this path is what is
left: the lead's own figure is 13.9 % of a core for a plain nine-member swarm at eight voices
against PF-1's 10.7 %. The silent path already got this fix (the `rest()` change); the sounding
path did not. `context()` and `param_index` appear nowhere in horde's ROADMAP, DECISIONS or the
wavetable contract, so I believe it is not scheduled.

**Fix.** An id-to-index table built at `prepare`; build the `run_actions` context only when an
action is due; share one context per tick across voices. Output is bit-identical, so no golden
moves.

**Cost.** S, in the builder's tree, on the lead's brief.
**Wrong if.** A profile of a sounding plain-swarm patch shows `context` plus `param_index` under
5 %. Nobody measured that patch today; the 18 % and 28 % are the builder's, on a note start.
**Who.** Lead. These are builder numbers: under B519 they stay out of anything sent to the test
author.

Smaller items from the same read, all bit-identical unless noted, all HYPOTHESIS on size:
`turn_clocks` calls `std::exp` per sample (`api/engine.cpp:166`) and dies with the blades anyway;
the control tick does four trig calls per member, two of them for a value only tests read
(`swarm/swarm.cpp:166-177`); `swing_step` recomputes a `pow` and an `exp` per member that depend
only on the oscillator (`swarm/inertia.h:17,28`); the sub's 33-tap filter is scalar and runs twice
during an order fade (`sources/sub.cpp:300-347`). One I checked and dropped: the per-member 33-tap
filter E would dominate a table member, but `docs/wavetable/CONTRACT.md:66` already rules "no
filter E on a table member". Not found anywhere in the engine: flush-to-zero. That is harmless on
Apple silicon and a risk on PF-2's x64 minimum machine, where decaying one-poles go subnormal.

### 4 — `./verify fast` is four and a half minutes, run in series

**Saw.** `time nice -n 10 ./verify fast`: **real 264.66 s, user 266.01 s** (exit 0, load about 6).
User time equal to wall time means nothing ran concurrently. The timestamped second run took
242.5 s:

| gate | seconds | running total |
|---|---|---|
| `gui3_seam_check` | 56.3 | 23 % |
| `gui3_browser_check` | 49.8 | 44 % |
| `sandbox_check` | 25.7 | 54 % |
| `build_flags_check` | 22.4 | 64 % |
| `stop_gate_check` | 13.4 | 69 % |
| everything else | 74.9 | 100 % |

`verify:80` describes this leg as "Layer-0: seconds, not minutes", and `docs.yml` justifies its
existence with "`verify fast` is seconds". On CI the docs leg takes a median of 5.0 minutes (48
pull-request runs, 2026-11-07 to 11-10).

**Why it matters.** This is the Stop gate: any turn that edits a tracked file must run it before
the session can end, and every PR runs it again on CI. At about 16 merged PRs a day that is over
an hour of wall time a day in which an agent, and often the human, waits. A four-line ROADMAP edit
pays the same four minutes as a GUI change.

**Fix.** In order of safety:
1. Run the five slow gates as background jobs and `wait` for each exit code. None of them writes
   a file another reads, as far as I could see. With four slots the run should fall to about the
   longest gate plus the serial rest, roughly 90 s. Same gates, same inputs, same verdicts.
2. Record seconds per gate in `.harness/last-verify.json` (its `gates` array is empty today), so a
   gate that doubles in cost is visible in the PR that did it.
3. Only then consider skipping a gate whose declared inputs did not change. That is a weakening
   and needs the human's ruling; I would not start there.

**Cost.** S for step 1, S for step 2.
**Wrong if.** Two of the five share state (a port, a scratch directory, a browser profile) and
flake when run together. `gui3_browser_check` and `sandbox_check` are the ones to try first.
**Who.** Lead for 1 and 2 (no gate changes its verdict); human for 3.

### 5 — Over a third of what the human merges is a records-only PR

**Saw.** `gh pr list --state merged --limit 300`: 300 PRs merged between 2026-10-22 and 11-10,
**all by the human**; 110 came from `lead-records-N` branches (37 %), median 40 added lines across
3 files (a ROADMAP row, a design-ledger entry, a trace). Median time from open to merge is 56
minutes; the slowest tenth wait over 10 hours. Merges land in 21 of the 24 hours of the day.

**Why it matters.** The human is the only merger by design, which is sound. But a records PR
carries no decision: it writes down a ruling or a merge that already happened. Each one costs a
click, a five-minute CI run, and an hour of latency during which the next branch is cut from a
main that lacks it. That latency is where the renumbering and "rebuild the ROADMAP from main"
commits come from (9 in the last 30 days, and over 20 merges of `origin/main` back into records branches).

**Fix.** Either of:
- Put the records in the PR they record. The lead adds the row, ledger entry and trace as a last
  commit on the work branch; the trace cites the branch head instead of the merge commit. One PR,
  one click.
- Or batch them: one records PR at session close instead of one per event (110 would have been
  about 20).

**Cost.** S, but it changes the "PR per batch" habit and the charter's "lead is the only writer
of ROADMAP.md" in practice if implementers' branches gain a lead commit.
**Wrong if.** The human uses the separate records PR as the moment they read the ruling back. If
so, keep it and take the batching option only.
**Who.** Human.

### 6 — The "single source of truth" cannot be read

**Saw.**
- `ROADMAP.md` is 2 100 289 bytes (about 525 000 tokens); `DECISIONS.md` is 1 274 326. On
  2026-08-01 they were 132 KB and 124 KB. DECISIONS grew 300 KB in the last ten days.
- GitHub's page for `ROADMAP.md` returns its "can't show files that are this big" message.
- The queue is one table of 696 rows (658 `B`, 38 `GB`) under a three-column header
  (`ROADMAP.md:7041`). Status is free prose inside the third cell. 329 rows contain no status word
  at all (none of DONE, SHIPPED, MERGED, CLOSED, OPEN, QUEUED, PENDING, BLOCKED and the like); 30
  contain both a closed and an open word. 84 rows have only two cells (no status cell) and 51
  have four or more, so for one row in five the status is not where the header says. Ids `B147` and `B155` each appear twice (reported by every audit
  since 2026-10-10). 51 rows are over 5 KB; the largest is 34 KB.
- 303 of the 784 commits in the last 14 days touch `ROADMAP.md`.
- No tool reads row status. There is no `roadmap_check`.

**Why it matters.** CLAUDE.md's truth contract says ROADMAP wins over the conversation. A lead
session cannot load it, cannot list what is open without a grep that returns megabytes, and the
human cannot open it on GitHub. So in practice the truth lives in `SESSION.md` (closed 2026-10-29,
729 commits ago), memory files and the conversation, which is the failure the contract exists to
prevent. The same file being the busiest in the repo is why finding 5's conflicts happen.

**Fix.** Two steps, the first independent of the second.
1. A `roadmap_check` in `verify fast` (about 80 lines; the last three audits proposed it): unique
   ids, exactly three cells, and a status that starts with one word from a closed list. Run a
   records pass to make the 696 rows conform. That alone makes `grep '| OPEN'` a true answer.
2. Move closed rows and the pre-September narrative sections to `docs/roadmap-archive/`, leaving
   open rows and the 1.0 definition. By the keyword count at least 125 rows are closed; by eye far
   more. Target: a ROADMAP a session can read whole (under 150 KB). DECISIONS can take the same
   split by hundred (`ADR-001..100` and so on) with an index.

**Cost.** M: one records pass plus one small tool, then one mechanical move.
**Wrong if.** Sessions already get a reliable open list from somewhere I did not find. I looked
for a generator and found only `docs/roadmap-map.html`.
**Who.** Lead for step 1. Human for step 2, since it changes what "ROADMAP.md" means in the
charter.

### 7 — The sanitize job is green with 19 oracles missing (fourth audit)

**Saw.** CI run 42746638614 on `main`, 2026-11-10: shard 1 "27 oracle(s) passed, 11 not built on
this platform", shard 2 "27 oracle(s) passed, 8 not built … GREEN". `tools/sanitize_oracles.sh:153`
prints "DOES NOT BUILD on this platform — SKIPPED" and the script still exits 0. The same 19 as on
2026-10-21 and 2026-10-29, and they include `state_check`, `bank_check`, `hostile_events_check`
and `bridge_utf8_check`: the oracles that parse outside input, which is what ASan is for.

**Why it matters.** A twentieth oracle that stops linking on Linux leaves CI green. The fix row
(B256) has no status.

**Fix.** The ratchet the last two audits proposed: fail when the not-built count exceeds 19, and
print the list. Then fix the 19 at leisure.
**Cost.** About 5 lines. **Wrong if.** Nothing; the log is the evidence.
**Who.** Lead. Adding a ceiling tightens the gate; ADR-180 §1 lets a check be added without a ruling.

### 8 — A skip and a pass leave the same record

**Saw.**
- `.harness/last-verify.json` after today's green run: `"exit":0, …, "gates":[]`.
- `.kit/kit-gates.sh:90-148` provides a `gate` wrapper and a `KIT-GATE skipped:` line, built on
  this repo's own brief, so that the record can say ran, skipped or failed. `grep -c KIT-GATE
  verify` is 0; only the nightly tools use it.
- In this worktree (no submodules, like every agent worktree and both CI `verify-fast` jobs,
  `ci.yml:67-74`): `license audit: libs/choc is not checked out; the rows that read its files DID
  NOT RUN` three times, then `license_audit_check: GREEN`. `choc_patch_check` prints a warning that
  eleven controls did not run, then `OK`. Those rows run only on the root checkout.
- `verify full` lets `rtsan_check`, `tsan_stress_check`, `h2_engine_rtsan_check`,
  `admission_check` and `conformance_check` print SKIPPED and exit 0 (`verify:1408-1497`, by the
  gate reviewer's reading; I did not run `full`).

**Why it matters.** The Stop gate and the push gate read the record. "Green" on a machine without
Homebrew LLVM, without submodules or without a sibling checkout means less than "green" on the
root checkout, and nothing downstream can tell.

**Fix.** Wrap the skippable checks in `gate` and have each print `KIT-GATE skipped: <reason>`;
print the skipped count on verify's last line. Add `submodules: recursive` to the two CI
`verify-fast` jobs so the license and patch rows run somewhere other than one Mac.
**Cost.** S. **Wrong if.** A consumer of the record I did not find already parses SKIPPED lines.
**Who.** Lead.

### 9 — A preset-only PR skips the preset oracles

**Saw.** `ci.yml:48-50` ignores `docs/**` on both triggers; `docs.yml` runs only `verify fast`.
The factory bank lives under `docs/presets/factory/`. Its oracles (`bank_check`,
`preset_pitch_check`, `morphlayout_check`, `offcorner_check`; the compiled block of `full`,
`verify:948-976`) are compiled, so they run in `verify full` or CI's sanitize job,
neither of which a docs-only PR triggers. In `fast`, only `paste_cap_check` reads that folder.
And all four are among finding 7's nineteen not built on Linux, so CI runs them for no PR at all.

**Why it matters.** The human plans a session re-voicing the whole bank. Each of those PRs will be
docs-only and will reach main having been checked for paste size and nothing else, unless someone
runs `verify full` by hand on the Mac.

**Fix.** Carve `docs/presets/**` out of `ci.yml`'s `paths-ignore` for the Linux jobs only, or move
the bank out of `docs/`. Keep the macOS jobs ignored; that bill is why the filter exists.
**Cost.** S. **Wrong if.** The lead's routine already runs `verify full` before every preset PR
and records it; I found no record either way.
**Who.** Lead.

### 10 — The CPU budget has no gate; the armor dashboard has one colour

**Saw.**
- `python3 tools/armor_coverage_check.py`: `armor: 0/12 green, 12 partial, 0 hole`; signatures
  0/7; security 0/4. Of 23 rows, 21 are `partial` and all 21 are tracked by one of two rows (B448,
  B446). `partial` needs no expiry (`docs/armor/README.md`), so a row can rest there for good. The
  catalogue last changed 2026-10-23.
- Row R12 (worst-case CPU): "the CPU measurement is run by hand; no per-release gate on the
  worst-case patch."
- The engine reached 293 of 295 tests before its CPU was timed and then read 7 to 10 times over
  (the lead's own memory note, 2026-11-03). The wavetable work now has a ruled budget (WT-BUDGET,
  ADR-390) and a tool that counts retired instructions without root (`tools/c2a_remainder`,
  "Reported, never a gate; not wired").

**Why it matters.** The one risk that has already cost two weeks is still guarded by a habit. And
a dashboard on which every row is amber gives the human nothing to look at: it did not change
when the CPU miss happened and will not change when it is fixed.

**Fix.**
1. Gate on instruction counts, which are stable where wall time is not: a pinned count per bench
   cell with a few per cent of headroom, red on a rise, in `verify full`. The timed run stays a
   human-window measurement. The pin file gets the same owner rule as the tolerance registry.
2. Split each `partial` row's gap text into items, each with its own ROADMAP id and date, so the
   dashboard shows movement. `partial` with no dated next step should read as a hole.

**Cost.** M for 1 (the bench exists; the pin and the control are new), S for 2.
**Wrong if.** Instruction counts on this machine vary by more than the headroom between runs of
the same binary. `--count` twice would settle it; I did not run it (another timed run was
scheduled).
**Who.** Lead builds; the human rules the headroom, since it is a budget.

---

## Also found

Shorter, in rough order. All VERIFIED unless marked.

11. **The clock and RNG ban reads only the legacy shell.** `tools/playbook_check.py:198` globs
    `src/*.h` and `src/*.cpp`. `h2/` and `engine2/core/` are not scanned, so "no wall-clock reads
    anywhere in the core" has no check on the engine that will ship. The tree is clean today.
    Fix: widen the glob. S. Lead.
12. **Protected references are guarded by prose only** (carried, H2 of 2026-10-29). There is still
    no hash ledger and no check; `docs/armor/protected.json` does not exist. S to M. Human
    sanctions the list.
13. **Docs that describe the engine before ADR-387.** `README.md` (its rebuild section and stage
    table), `engine2/README.md` ("Nothing is built yet") and `h2/README.md` do not mention ADR-387;
    README's "Last verified" is 2026-10-17. `CHANGELOG.md` was last touched 2026-10-17.
    `SESSION.md` is "closed 2026-10-29". CLAUDE.md §Domain is 12.9 KB, loaded into every session,
    most of it candidate history (flagged by three earlier audits; the file is the human's).
    Neither `CODEMAP.md` nor `.claudeignore` exists, though the doctrine asks for both.
14. **LIBRARY is at 76 entries against its own rule of 30.** Last consolidated 2026-08-11
    (34 to 30). Two tier syntaxes coexist (`tier: candidate` and `| candidate |`). INDEX.md, read
    in full at every ORIENT, is 17 KB. Fixed context per session (the four CLAUDE.md files, the
    doctrine, INDEX, the memory index) is about 63 KB, roughly 16 000 tokens: not alarming, but
    §Domain and INDEX are two thirds of it.
15. **The weakening counter sees two spellings.** Its `soft_fail` pattern matches `|| true` and
    `|| :`; it does not match `|| echo WARNING` (which `verify:103` uses), `|| return 0`, or a
    Python check that prints SKIPPED and returns 0 (by the gate reviewer's regex probe). And
    `test_table_check.py:191` does not glob `tools/patchspace/*_check.mjs` or `*_check.mm`, so six
    wired checks can lose their `verify` line unnoticed (carried, L6).
16. **`specs/ACCEPTANCE.md:3`** says Layer-0 runs in `./verify fast` and blocks CI. The parity and
    trajectory oracles run in `full`, which CI never runs; they reach CI only through the sanitize
    job. L0-18 (filter-family stability) is named by no tool; only its number is pinned in
    `docs/armor/tolerances.json`. Protected path: the human's.
17. **The mailbox backlog** as the session-start hook printed it today: 2 overdue items (8 and 2
    days), 10 with the ball on horde, 13 responses in other repos not yet read. I did not open
    them. A count this size suggests the per-session roundup is being printed and not acted on.
18. **Weight.** `docs/design/arp-face/bundle.js` and `docs/design-system/master/components/bundle.js`
    are the same 1.4 MB file but for two lines. `docs/design/` is 13 MB and holds superseded lab
    rounds (`-r2`, `-r3`) beside their successors. `tools/scratch_b18_two_osc_matrix.cpp` (August)
    and `tools/port_gap.py` (September) are referenced by nothing.
19. **`parity_check` has no "golden is not silent" floor** (`tools/parity_check.cpp:138-140`),
    where `spectra_check.cpp:137` requires `refRms > 1e-4`. A scenario silent on both sides passes.
    Legacy engine, frozen: low. HYPOTHESIS that any such scenario exists.
20. **Approval references are free text.** `tolerance_registry_check` accepts any non-empty
    reference; 137 of 154 read "baseline 2026-10-08". `verify` itself is not in CODEOWNERS though
    the charter makes editing it a human gate. In a repo where one person merges everything this
    is low; it matters the day a second merger exists.

Checked and fine: `src/` and `h2/cores/swarm/` twins are held byte-identical by `h2_lift_check`;
`tools/click_detector.mjs` and its `engine2/` copy are identical; hooks and agent files reference
no dead paths; recent merges each have a trace; H3 of 2026-10-29 (tolerance exclusions without a
value) is fixed, 13 of 13 now carry one.

---

## Earlier audits: what was never acted on

| finding | first raised | today |
|---|---|---|
| Clean-room repos not pushed | 2026-10-21 (H1) | **Open, worse**: 85 and 69 commits (finding 1) |
| Sanitize job green with oracles not built | 2026-10-10 | **Open**, fourth audit (finding 7) |
| Duplicate ids B147, B155; no roadmap check | 2026-10-10 | **Open** (finding 6) |
| Nightly never run; verify record `gates:[]` | 2026-10-21 | **Open** (findings 2, 8) |
| Protected references guarded by prose | 2026-10-29 (H2) | **Open** (item 12) |
| CLAUDE.md §Domain false statements | 2026-10-10 | **Open** (item 13) |
| README "Last verified 2026-10-17" | 2026-10-21 | **Open** (item 13) |
| SESSION.md stale ids | 2026-10-29 (M9) | **Open**; the file has not been rewritten |
| Wiring glob skips `.mm` and `patchspace/` | 2026-10-21 | **Open** (item 15) |
| Tolerance exclusions pin no value | 2026-10-29 (H3) | Fixed |

A pattern worth the human's attention: the weekly audit produces a 40 KB report, its findings
become proposals for rows, and the cheapest ones (five lines, two pushes, one file) are still open
three and four audits later. The audit is not the bottleneck. I would stop running it weekly until
the carried list is empty, and give each carried finding a row with a date, or a written "won't
do".

---

## What I could not check

- **No engine timing of my own.** Every CPU share in finding 3 is the builder's or the lead's. I
  ran no bench: a timed run was scheduled and the brief capped me at one heavy job.
- **`./verify full`** was not run (the brief allowed `fast` only), so its duration and its skip
  list on this machine are unmeasured.
- **Whether the five slow `fast` gates are safe to run together** (finding 4). I read their
  invocations, not their internals.
- **GitHub branch protection** and whether a required-check rule exists.
- **The mailbox items** (item 17) and the sibling repos they point at.
- **Whether a backup** of the clean-room repos exists outside git.
- **The morph resolver and modulation system's audio-thread cost** in the new engine; the engine
  reviewer did not reach them.
- **About 15 of 25 sampled checks for a must-fail control.** A keyword scan flagged some twenty
  older compiled oracles (`delay_check`, `force_check`, `svf_check`, `mod_check` and others) with
  no "control" or "plant" wording. Candidates, not findings.
- Subagent reports were spot-checked, not re-derived: I re-ran or re-read every claim cited in
  findings 1 to 10 and items 11, 15, 16 and 19, and took items 13 and 18 from the drift sweep with
  only the sizes and dates re-checked.

**Token use.** About 520 000 across the three subagents and roughly 300 000 in this session, under
the 1 M cap.
