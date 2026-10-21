# Repo audit — 2026-10-21

**Commit audited** `900963cdbc18312f6b1b53a7a71cd7a89116cd1e` (`origin/main`, PR #1169 merged).
**Cause** The auditor cadence (CLAUDE.md §Domain "Auditor cadence", B159, ADR-179 §3), dispatched
by the lead at the first session of 2026-10-21. The previous repo-wide audit is
`docs/audits/2026-10-10-repo-audit.md`, 11 days old against a 7-day cadence. Two scoped audits
sit between them (`2026-10-15-engine-rebuild-audit.md`, `2026-10-17-module-cutoff-audit.md`).
**Scope** The whole repository, read-only, hardest where the lead asked: spec rounds 17 to 27
(ADR-240 to ADR-260), the design-system and interaction-spec ingestion (ADR-259 to ADR-262), the
rack-slot contract's amendments (§12, above all §12.5), the clean-room rulings log
(`docs/port/cleanroom-rulings.md`), and doc drift across README, CLAUDE.md §Domain, H2-PLAN,
ROADMAP and DECISIONS.
**Oracle** `nice -n 10 ./verify fast` in this worktree at `900963c`: **exit 0**, 148.2 s wall
(load average 4.2 to 4.7 on 8 cores, clean-room jobs running). Run once, as the lead allowed.
In a worktree it is reduced: `sluice_hold` and `mailbox_delivery` print SKIPPED, and the license
and choc-patch rows print "DID NOT RUN" because `libs/` is not checked out (M10).
**CI evidence** GitHub Actions run 39632849753 (`main`, the merge of PR #1167), read with
`gh run view --log`.

Every finding is a **proposal for the lead**. Nothing outside this file was edited. Nothing
here proposes weakening a gate. A delta on a protected path is labelled as a sanction the
human would have to give.

**Rules of evidence.** MEASURED: a command ran today and printed the number. BY READING: the
claim follows from the cited lines and was not executed. Every count below was produced by a
command in this session; none is copied from another document. The two clean-room repositories
were read only for their git metadata (remote, commit count, spec blob) and through the
clean-room check's own `audit_tree` function (tags only). No engine fact or number from either
repository is recorded here.

## The findings at a glance

| id | rank | class | one line |
|---|---|---|---|
| H1 | HIGH | storage: one copy | The accepted engine (stages 1 to 5) and its suite exist only in two local repositories with no remote; horde's `engine2/`, which B502 names "the record", is still 137 lines of empty stubs |
| H2 | HIGH | prose asserting a relationship | The spec says its provenance is kept by requirement id; the provenance file stopped at round 16, and 12 requirements have no entry at all |
| H3 | HIGH | an exclusion with no expiry | ADR-247's two preconditions for resuming the clean room are still OPEN rows while stage 6 is under way; five relaxations stand with no recorded ruling |
| H4 | HIGH | a hand list beside a glob | Seven armor and discovery checks root at `src/` and `h2/`; none sees `engine2/`, the engine ADR-223 says replaces `h2/engine` |
| H5 | HIGH | two sources disagree | `docs/design-system/README.md` still rules the window, glyphs, re-theming and the dark chassis the opposite way to ADR-259, while B531 builds the design system |
| H6 | HIGH | green when it never ran (carried) | CI's sanitize job is green with 19 oracles not built (18 on 2026-10-10); `preset_pitch_check` joined the set |
| M1 | MEDIUM | one rule, two statements | Rack-slot contract §12.5 contradicts RS-5.6 without amending it; SPEC-MODULE-MACROS and §12.5 define the modify law's base differently |
| M2 | MEDIUM | stale record | The rulings log misses ADR-246, ADR-255 and most of ADR-258 against B516's own acceptance; B516, B524 and B525 statuses lag the tree |
| M3 | MEDIUM | green when it never ran | The nightly job list (six jobs, including the 20 000-case fuzz and two soaks) has never run: the workflow is a draft the human was to install on 2026-10-11 |
| M4 | MEDIUM | doc drift | README's rebuild table says the tests are "Next" and the build "Not started"; stages 1 to 5 are accepted. Five false statements |
| M5 | MEDIUM | doc drift | H2-PLAN "last verified 2026-10-17, through ADR-236"; `sub`, `noise` and `gui3` statuses predate ADR-255, ADR-258 and ADR-259 to ADR-262 |
| M6 | MEDIUM | reachable but unrecorded | ADR-261 and ADR-262 add six kinds of new user state; none has a storage, undo or branching decision |
| M7 | MEDIUM | doc drift (carried) | CLAUDE.md §Domain: all twelve statements Appendix A of the last audit called false are still in it; it was edited on 2026-10-21 and names neither `h2/` nor `engine2/` |
| M8 | MEDIUM | truth contract (carried) | ROADMAP.md grew from 1.67 MB to 1.90 MB in 11 days; B147 and B155 are still duplicated |
| M9 | MEDIUM | stale status | SPEC-ENGINE's status line is dated 2026-10-16 (11 rounds ago); ADR-232 still reads PROPOSED; ADR-255 A1 sits after ADR-256 |
| M10 | MEDIUM | green when it never ran (carried) | The verify record still holds `"gates":[]`; a green run says nothing about what it skipped |
| L1-L7 | LOW | count, glob, stale prose | Hand-kept §5.8 counts, six check files outside the wiring glob, three stale CMake sentences, 145 of 186 drifted playbook citations, root drops, ROBUSTNESS, cadence |

---

## CRITICAL

**None.** No finding shows wrong audio shipped, a violated invariant in a shipped product, or
an irreversible loss that has happened. H1 is the nearest: it is an irreversible loss that has
**not** happened, and its only guard is one disk.

---

## HIGH

### H1 — The accepted engine and its suite exist on one disk, with no remote; horde's record of them is empty

**Claim.** Five build stages of the new engine have been accepted by the lead and the human
(stage 4: ROADMAP `ROADMAP.md:7516`, "The builder's stage 4 is ACCEPTED"; stage 5: ADR-253,
`DECISIONS.md:8743`). The code and the acceptance suite live only in the two clean-room
repositories. Neither has a remote. horde's `engine2/`, which B502 names as the record, holds
none of it.

**Evidence (MEASURED).**
- `git remote -v` in both clean-room repositories prints nothing. Their histories hold 70
  commits (the builder's) and 81 commits (the test author's), both last committed 2026-10-21,
  and both have uncommitted changes in their working trees.
- `find engine2/core engine2/tests -type f | xargs wc -l` in horde: **137 lines in total**, every
  core header two lines long (a purpose line). `engine2/README.md:5` "Nothing is built yet."
  `./verify fast` today printed `suite: 0 items, 0 failed`.
- B502, `ROADMAP.md:7524`: "The lead carries results back into horde's `engine2/` by PR, first
  the tests (B498), then each build stage. The two repositories are workspaces; horde's
  `engine2/` stays the record."
- B494, `ROADMAP.md:7516` (2026-10-17): "**Tests land in horde with their code:** stages 1 to 3
  come in as one PR once green." Stages 1 to 5 are green; no such PR exists
  (`git log 4a3dbaf..origin/main -- engine2/core engine2/tests` shows only the scaffold).
- B502 still names the repositories' first parent folder. That folder has been empty since
  2026-10-19; the repositories now live elsewhere, and no tracked file says where. The only
  trace of their current names is commit-hash citations ("cleanroom-tests 69c640b",
  `DECISIONS.md:8632`).

**Why it matters.** Storage, sweep item 7, applied to the project rather than a patch: the
product's 1.0 engine and the only acceptance suite written for it have one copy. The record
the truth contract requires (ROADMAP "Done = ... a trace entry") points at an empty tree.
Every append-only promise the spec makes about ids (RC-9.1, `specs/SPEC-ENGINE.md:755`) is held
in a manifest that horde cannot see, so no horde gate can check it.

**The tension, stated rather than guessed.** Landing the builder's code in horde's `engine2/`
would put it in every future snapshot the recipe (`tools/engine2_cleanroom.sh`) makes, and so
within reach of a fresh test-author tree. That may be why nothing has landed. It does not
explain why there is no off-machine copy.

**Minimal delta.** Two parts, for the human to choose between or combine:
(a) a private remote for each clean-room repository (no change to horde; the human creates
them, as with the machine account in ADR-210); or (b) a `git bundle` of each repository written
at every stage acceptance to a location the lead names, recorded in B502 with its hash.
Then correct B502's text: where the repositories live, and whether "horde's `engine2/` stays the
record" still holds or is replaced by (a) or (b). This is a decision about the clean room's
independence, so it is the human's (the question at the end).

---

### H2 — The spec says its provenance is kept by requirement id; the record stopped at round 16

**Claim.** `specs/SPEC-ENGINE.md:7`: "The record behind every requirement and law is kept outside
this spec, by requirement id, in `docs/port/spec-engine-provenance.md`." That file was last
changed by round 16 (`9d5f3fd`, ADR-239, 2026-10-17). The spec has had 15 commits since,
carrying rounds 17 to 27.

**Evidence (MEASURED).**
- `grep -c "round N"` in the provenance file, for N = 17 to 27: **0** each. In the spec: 1, 0,
  8, 3, 3, 5, 3, 1, 2, 13 and 5.
- `grep -c "ADR-NNN"` in the provenance file for ADR-240, 241, 243, 244, 245, 248, 250, 252,
  253, 256, 258 and 260: **0** each. The newest ADR the file cites is ADR-239.
- Requirement ids **defined** in the spec (a table row `| SN-5 |`, a bold bullet or an `####`
  heading) and **absent** from the provenance file: **12 of 151**: EN-5, QS-11, SN-5, SN-6,
  SN-7, SN-8, SN-9, SN-10, SN-11, SN-12, SN-13 and SN-14. Every one of them was written by round
  26 (ADR-258), the sub and noise oscillator.
- Round 26's laws came "from the audition lab's laws (form and numbers only)"
  (`DECISIONS.md:8881`). That lab is not a tracked file: no path in `git ls-files` matches it,
  and B514 (`ROADMAP.md:7536`) names it only as an Artifact. Those numbers' source is outside
  the repository.
- B518 (`ROADMAP.md:7547`, OPEN) needs exactly this file: it "marks every spec sentence that
  came from a builder proposal (rounds 16, 17 and 19–21, and every one from now on)". The file
  has no entry for rounds 17 or 19 to 21, so B518 cannot start without first back-filling them.
- No check reads the provenance file (`grep -l spec-engine-provenance tools/* verify`: none).

**Criterion violated.** Sweep item 6, "does any claim a file makes about its own place in the
system survive being verified". ADR-247's independence audit rests on telling builder-origin
text from the rest. This file is where that distinction was to be kept.

**Minimal delta.** A spec-only, engine-free check (about 40 lines of Python, in
`./verify fast`): every requirement id the spec defines has at least one provenance row, and
every provenance row names an id the spec still defines. It needs no engine and cannot see one.
Then back-fill rounds 17 to 27 from ADR-240 to ADR-260. Both are additive (ADR-180 §1).

---

### H3 — ADR-247's preconditions for resuming the clean room are open rows while stage 6 runs; five relaxations stand unruled

**Claim.** The human's ruling of 2026-10-19 says, verbatim: "**The clean room resumes only after
B525's filesystem separation** and B519's relay rules are in force" (`DECISIONS.md:8627`). The
clean room has resumed: stage 5 was accepted on 2026-10-20 (ADR-253), and stage 6's spec
rounds 26 and 27 were delivered on 2026-10-20 and 2026-10-21 (ADR-258, ADR-260). B525 still
reads OPEN, and B519 carries no status at all.

**Evidence.**
- B525, `ROADMAP.md:7554`: "**OPEN 2026-10-19** (needs the human to reopen both sessions)". Its
  own deadline: "Done at the current pause, before stage 6."
- B519, `ROADMAP.md:7548`: no status token in the row.
- MEASURED: the repositories did move on 2026-10-19 (their old parent folder is empty, modified
  2026-10-19 12:33). They are now **siblings under one parent folder** again, which is the
  condition B525's first sentence was written to end ("The two repos stop being siblings under
  one parent folder"). Whether either session can still reach the other's tree was not tested
  here. It needs the sessions, not an auditor.
- The rulings log, `docs/port/cleanroom-rulings.md:51`: "**Lead readings with no recorded ruling
  (B520: owed to the human):** B-12, B-13, B-27, and Q-TA-1/6". ADR-247 ruling 1: a gate moves
  "only with evidence produced without the engine ... Each such move also needs the human's
  ruling, recorded in the rulings log". B520's acceptance (`ROADMAP.md:7549`): "each entry
  carries a ruling and engine-free evidence, **or is reverted**". Two days later none has a
  ruling and none is reverted.
- Rows 12 and 15 of the rulings log still carry "engine only" or "engine (law at the engine's
  phases)" evidence, each marked "**re-derive (B520)**".

**Criterion violated.** Sweep item 1, "a gate whose exclusion outlived its ruling", and the
charter's oracle discipline ("Never weaken a gate ... without an explicit human decision"). If
the separation was done, this is a stale status. If it was not, the build resumed past a
precondition the human set. Either way the record cannot tell which.

**Minimal delta.** One records PR: B525 gets a dated status saying what was done, and whether
each session's working directory can reach the other's tree, tested by each session itself.
B519 gets a status. Each of the five B520 items goes into the human's next listening or ruling
batch, and each is either ruled or reverted to gated in the test author's suite. No horde gate
changes.

---

### H4 — The armor's discovery checks cannot see `engine2/`

**Claim.** Blind-Spot Armor (ADR-197, CLAUDE.md §Domain) makes every risk row "a named gate in
`./verify` or a visible hole on the dashboard". The checks that find units, files and goldens
by rule all root at `src/` and `h2/`. `engine2/` is the engine ADR-223 says replaces
`h2/engine`, and it sits outside both roots.

**Evidence (MEASURED, `grep -c engine2` on each source).**

| check | what it discovers | roots | mentions `engine2` |
|---|---|---|---|
| `tools/engine_enrolment_check.py` | every engine and module dir, for fuzz, soak and the invariance matrix | `h2` | 0 |
| `tools/banned_api_check.py` | strcpy, sprintf, gets ... ("54 files under src/h2" today) | `src`, `h2` | 0 |
| `tools/h2_rt_lint_check.py` | real-time violations | `h2` | 0 |
| `tools/include_check.py` | MSVC header portability (167 files today) | `h2/**`, `src/*` | 0 |
| `tools/weakening_check.py` | skip and suppression markers | `h2`, … | 0 |
| `tools/golden_registry_check.py` | goldens, by rule | `h2/**/*.json`, … | 0 |
| `docs/armor/{catalogue,engines,goldens,tolerances}.json` | the registries | — | 0 each |

- `engine2/data/reference-renders/manifest.json` and `manifest-48k.json` (the 380 frozen
  reference renders, about 10 700 lines each) and `engine2/data/version0-states/` are goldens in
  the registry's own sense ("a stored expected result"). None is registered, and no rule can
  find them, so UNREGISTERED cannot fire.
- `portability_gate` (the `M_PI` grep, `verify:30-40`) does cover `engine2/`, because it greps
  `*.cpp`/`*.h` repo-wide. It is the one exception.

**Why now, while `engine2/` is empty.** The day H1's landing PR merges, these seven checks stay
green over code they never read. That is "green for the wrong reason" arriving on a schedule.

**Minimal delta.** Add `engine2` to each check's root list, in the same PR as a planted file
under `engine2/` that each check's control must catch. Register the two render manifests and
the version-0 states in `docs/armor/goldens.json`, and add the discovery rule that finds them.
This strengthens gates and is not gated (ADR-180 §1). `engine2_cleanroom_check`'s allow-list
already treats `engine2/` as one tree, so the deny lists can be reused.

---

### H5 — The design-system README rules five things the opposite way to ADR-259, while B531 builds the system

**Claim.** `docs/design-system/README.md` is where a reader learns "which document wins", and
it lists its conflicts "so nobody 'corrects' the implementation toward the losing side"
(`:61-62`). ADR-259 (`DECISIONS.md:8915`, 2026-10-21) ruled five of those conflicts, and four
of them the other way. The README was edited the same day for ADR-261 (`655fa3d`) and still
states the old answers.

| `docs/design-system/README.md` | ADR-259 |
|---|---|
| `:66` window "1180 × 820, min 1000 × 700", UI Spec **wins** | ruling 2: "**Window: 980×720, resizable.**" |
| `:71-79` glyphs "**unresolved — they need a human ruling**"; the UI Spec says "NO glyphs anywhere" | ruling 3: "**Corner glyphs: always** (closes ADR-117's open question)" |
| `:80-85` re-theming unresolved | ruling 4: "the chassis carries" corner colours; inside a screen, the screen's own glyphs |
| `:87` "Open question 3 — is there a DARK chassis?", a "derivation, not a ruling" | ruling 7: "**ratified after a contrast audit.** Light (cream) stays the default." |
| `:115-123` the 2.84:1 contrast finding, "Ruling wanted" | ruling 8: darker text and lines from the ORCHID scheme; 28 px targets |

**Criterion violated.** "Where two sources disagree ... that IS a finding." The README's own
rule ("UI Spec wins" on values) now points the design-system build at the ruled-against
window size.

**Minimal delta.** About 30 lines in one file: the conflict table and both open-question
sections say "Ruled by ADR-259 item n" with the ruled value; the "Status: SPECIFIED, NOT BUILT"
section (`:33`) names B531. The `.dc.html` files are not touched, so no protected path is
involved.

---

### H6 — CI's sanitize job is green with 19 oracles not built (carried from H3 of 2026-10-10, one worse)

**Evidence (MEASURED, run 39632849753).** The job is now sharded (ADR-233). Shard 1:
`27 oracle(s) passed, 11 not built on this platform`. Shard 2: `27 oracle(s) passed, 8 not built`.
Job conclusion: **success**. On 2026-10-10 it was 53 passed and 18 not built. (The thread leg now
runs `tsan_stress_check` instead of the oracle loop, by ADR-233 ruling 2. That is a ruling, not a
finding.)
- The new member is `preset_pitch_check`, added after the last audit. It fails at link for the
  same cause as the other 18: the `hypersaw_debug_*` exports sit inside the GUI block of
  `src/hypersaw_clap.cpp`.
- `identity_store_check` and `conformance_check` print "no target in this tree — SKIPPED".
  The first is macOS-only by design (`CMakeLists.txt:879-891`).
- `docs/ROBUSTNESS.md:119` still says "**23/24 oracles GREEN**".

B455 (`ROADMAP.md:7477`) lists H3 as "Still open from the audit, not yet scheduled". The set
that does not run is still the set that parses outside text: state, preset, bank, undo, the
load hand-off and hostile events.

**Minimal delta (unchanged, now with a ratchet).** The fix row B256 already describes. Until it
lands, a count that may only fall: the sanitize driver fails when "not built" exceeds 19. That
turns the next addition to the set red instead of silent, and strengthens a gate.

---

## MEDIUM

### M1 — Rack-slot contract §12.5 contradicts RS-5.6 without amending it; the macros spec and §12.5 define "base" differently

**Evidence (BY READING).**
- RS-5.6, `docs/proposals/rack-slot-contract.md:218`: "DAW exposure is the slots only ...
  **Module internals get no host parameter ids.**"
- §12.5, RS-5.8 (new), `:465`: a fixed control "is a module parameter, never a macro. **Each
  gets a host parameter id per FX slot**". §12.5's table amends RS-5.1, RS-5.3 and A12 and adds
  RS-5.8, RS-4.11 and A22. **It does not list RS-5.6**, and the §5 table still reads as
  unamended.
- §12.1 (`:413`) and §12.2 (`:429`) each say "Where a row above and this section disagree, this
  section holds". **§12.3, §12.4 and §12.5 do not.** A reader of §12.5 has no stated precedence.
- The status line (`:9-11`) mentions only ADR-218's amendment ("A4, A11, A16, RS-4.4 and RS-7.6
  are read with §12") and says "Last verified 2026-10-09". Four amendments have followed.
- The modify law's base. `specs/SPEC-MODULE-MACROS.md` (protected; amended by ADR-251,
  `817a539`): "`base` is the preset's `internal_defaults` value, or else the parameter's
  default." §12.5, RS-5.3 (`:466`): "The base is the parameter's value as the preset **and the
  user** set it". ADR-257's reading (`DECISIONS.md:8862`) marks this PROPOSED. The protected
  spec was not touched after 2026-10-19.
- **Storage consequence (item 7).** Under §12.5's reading, the base moves when the user edits.
  A state that saves the **heard** value rather than the base applies the macro contribution
  twice on reload. Neither document says which value the state writes.

**Minimal delta.** In the contract: add RS-5.6 to §12.5's table ("as amended: ... except fixed
controls, RS-5.8"), add the precedence sentence to §12.3 to §12.5, and rewrite the status line
to name §12.1 to §12.5. In SPEC-MODULE-MACROS: the same base definition as §12.5, plus one
sentence saying the state stores the base. **The second is a sanction the human would have to
give** (a protected path). Admission row A12's round trip could then hold a must-fail case: a
preset saved with a macro off its rest value.

### M2 — The rulings log falls short of B516's own acceptance, and three ADR-247 rows lag the tree

- B516's acceptance (`ROADMAP.md:7545`): "**every clean-room item from ADR-237 onward** is
  listed". MEASURED: the ADRs that cite B494, B514 or B521 from ADR-237 on, compared with the
  ADRs the log cites. **Missing: ADR-246** (the noise oscillator plays samples, the sampler's
  reservation) and **ADR-255** (nine sub and noise rulings, the source of stage 6). ADR-258
  appears only for its ruling 8 (row 30). Its rulings 1 to 7 and its addendum of 2026-10-21
  ("Its own filter, following its note") are absent. Ruling 7 is an exception to ED-1 ("S&H and
  LFSR stay raw: their steps are not band-limited, ED-1's one exception"), which is the kind of
  requirement change the log exists to keep.
- The log's header (`docs/port/cleanroom-rulings.md:4`) says it records "every question raised
  to the human". Row 32 is "Pinned by the lead ... (no human ruling ...)". That is a correct
  record, but it falsifies the header. Either widen the header or move lead pins to their own
  table.
- B516 reads "**OPEN 2026-10-19** ... Started: 21 rows". The file has 32.
- B524 (`ROADMAP.md:7553`, reasons inline) has acceptance "delivered in the stage-6 spec round
  and passed by its critic". ADR-258 says round 26 was written "with ... reasons inline (B524)"
  and passed three critics. The row still reads OPEN.

**Minimal delta.** About 12 log rows and three status cells. Records only.

### M3 — The nightly job list has never run

**Evidence (MEASURED).** `tools/nightly/jobs.json` holds six jobs: a one-hour legacy stability
run; `h2_fuzz_check --deep --cases 20000` (floor 20 000); two `h2_soak_check` runs (floors 899
and 149); and RealtimeSanitizer and ThreadSanitizer legacy jobs. `.github/workflows/` holds
`ci.yml`, `cpu-derate.yml` and `docs.yml` and no schedule (`grep -n "schedule\|cron"`: none).
`gh run list --workflow nightly.yml`: "HTTP 404: workflow nightly.yml not found on the default
branch". The workflow is `tools/nightly/nightly.workflow.yml`, a DRAFT
(`tools/nightly/README.md:15`: "The human installs it"). The human's install has been listed as
owed since 2026-10-11 (B448 Wave 1, the draft in PR #1049).
- `nightly_check` is in `fast` and green: it tests the **runner** against planted jobs. That is
  the correct control for the runner, and it says nothing about whether a night ever ran.
- ADR-209 item 4 lets a row be "green on a nightly result less than 48 hours old". No row can be.
- Three of the six jobs build and drive `h2_harness_render`, the composed engine ADR-223
  supersedes. When the nightly is installed it will soak the outgoing engine. QS-9's soak
  ("4 patches × 4 h, run before release", B498) has no job.

**Minimal delta.** For the human: install the workflow, or rule that the nightly waits for the
new engine. For the lead: a job entry for QS-9 once the engine lands (H1). The armor dashboard
already has a "visible hole" form, and `freshness.py` prints `NIGHTLY HOLE`; the lead's roundup
should print it every day until a run exists.

### M4 — README: the rebuild table is three stages behind (five false statements)

`README.md:583` "Last verified: **2026-10-17**, against main `5613885`".

| README | the tree |
|---|---|
| `:398` the spec is "amended since by ADR-230 and ADR-231" | Amended by rounds 13 to 27: ADR-232, ADR-237 to ADR-241, ADR-243 to ADR-245, ADR-248, ADR-250, ADR-252, ADR-253, ADR-256, ADR-258, ADR-260 |
| `:401` "The acceptance tests ... (B498) — **Next**" | The test author has written stages 1 to 8 and is on stage 6's items 24 to 33 (B494, `ROADMAP.md:7516`; ADR-260) |
| `:402` "The build, in eight stages ... — **Not started**" | Stages 1 to 5 accepted (ADR-253); stage 6 in progress |
| `:400` "Two history-free repositories ... their sessions are next" | Both sessions have run since 2026-10-17 (70 and 81 commits) |
| `:389` "beside horde's own ECHO, EQ, FX filter and drive" | ECHO is a sibling project (ADR-249, 2026-10-19) |
| `:576` "`docs/design/` — 36 labs" | 44 lab pages plus `index.html` (MEASURED: `ls docs/design/*.html`) |

**Minimal delta.** About 10 lines and a new "last verified" date. B455 named README (its H5)
"not yet scheduled". This is now the same finding a second time.

### M5 — H2-PLAN: verified through ADR-236; the parts table predates the sub, noise and design-system rulings

`docs/H2-PLAN.md:3-6`: "Last verified: 2026-10-17 ... ADR-213 to ADR-231 ... through B511 and
ADR-236". Since then: B512 to B532 and ADR-237 to ADR-262. It was partly updated (the ECHO row
`:132` cites B526), but:
- `:122` `sub` "planned", rows B399, B327, B278, B282, and `:123` `noise` "planned", rows B409,
  B327. The human's final rulings (ADR-255), spec round 26 (ADR-258) and stage 6's tests make
  both "in progress". Neither row cites B514 or B515.
- `:145` `gui3` "planned", without B531 (the design system, in progress) or B532 (ADR-261's
  specs). ADR-259 to ADR-262 appear nowhere in the file (`grep -c "ADR-2[5-6][0-9]"`: 0 outside
  the ECHO row).
- The file's own rule (`:16-18`) sends disagreements to its "Inconsistencies found" section;
  none of these is there.

**Minimal delta.** A refresh with the procedure the file already uses (traces
`2026-10-17-h2-plan-refresh*.md`), with the map's `?check=1` re-run.

### M6 — New user state from ADR-261 and ADR-262 has no storage decision

ADR-262 (`DECISIONS.md:8992`) and ADR-261 (`:8951`) add state a user can change. Against item
7's comprehensive half ("diff what the interface can change against what the state writer
emits"), none says whether the state is patch, session or application preference, whether it
is in undo, or what a branch does to it:

| state | ADR | stated |
|---|---|---|
| per-corner edit **lock** ×4 | 262 ruling 4 | refuses writes; nothing on saving |
| per-corner **edited flag** and saved snapshot, for revert ×4 | 262 ruling 4 | "an edited corner shows a mark" |
| corner **preset names** ×4 | 262 ruling 3 | shown; "Empty" when none |
| **map-mode mappings** | 261 | "writing into the mod matrix"; "not automatable"; "global mapping for 1.0" |
| the one **global corner pin** | 261 ruling 3 | adopted; nothing on saving |
| **keyboard mode**, **screen scheme** | 259 ruling 6, 262 ruling 2 | opt-in, user-selectable; nothing on saving |

Undo is ruled ("one step per gesture", B84, cited by ADR-261). The rest are open. Two of these
are positional (four corners). The corner-lock row in particular decides whether a → b → a
restores a lock: if the lock is in the patch it must, and if it is a GUI preference it must
not.

**Minimal delta.** One table in B532 (or a short ADR) giving each row its home: patch, session
or preference; in undo or not; written by the state writer or deliberately excluded. Recorded
now, while it costs a table and not a migration ("every default change carries an ADR AND a
migration", Blind-Spot Armor).

### M7 — CLAUDE.md §Domain: the twelve false statements of the last audit are all still there

MEASURED with `grep -n`: "not yet in the shell" `:78`, "MAW is FX-C now" `:81`, "open ruling on
B127" `:95`, "eighth CANDIDATE" `:104`, "seven single-file HTML" `:116`, "SECOND CLAP shell"
`:126`, "now seven" `:168`, and "golden render fixtures once they exist" `:185`. The facts that
contradict them are in Appendix A of `2026-10-10-repo-audit.md`. Nothing has made any of them
true since. New since: `:185` was extended on 2026-10-21 for ADR-261's protected paths and the
same sentence still ends "golden render fixtures once they exist". Stack and entrypoints names
neither `h2/` nor `engine2/` (`grep -c`: 0 and 0). The only mention of the new engine is the
ADR-224 paragraph (`:143-145`). ECHO's sibling status (ADR-249) and the clean-room rulings
log are absent. B403 (`ROADMAP.md:7572`) is "OPEN — lead".

**Minimal delta.** B403 as already written, plus `engine2/`, `h2/` and the rulings log in
Stack. CLAUDE.md is the human's to approve.

### M8 — ROADMAP.md: 1.90 MB, and growing at 20 KB a day (carried H6)

MEASURED: 1 668 552 bytes at `4a3dbaf` (2026-10-10) and **1 895 132** today (+13.6 % in 11
days). 451 rows then, **529** now. 527 unique ids: **B147** (`:7132`, `:7134`) and **B155**
(`:7142`, `:7143`) are still duplicated. The largest rows: B448 34 408 bytes, B50 28 439, B265
17 398, B446 14 233, B441 14 049, B494 13 470. Four rows carry both "**OPEN" and "MERGED" by this audit's pattern (the last audit
counted 20 by its own; the two methods may differ). There is still no closed status vocabulary
and no `roadmap_check`.

**Minimal delta.** Unchanged from the last audit's row 6. B494's 13 KB of dated progress is
the first candidate for a linked log file.

### M9 — Three stale statuses at the top of the records

- `specs/SPEC-ENGINE.md:3` "**Status.** The contract for horde's engine, 2026-10-16." Eleven
  rounds have changed it since. The spec carries no revision line, so a clean-room session
  cannot tell which round it holds except by blob hash. (The blobs match today: horde, the
  builder's and the test author's are all `ba4ae7a`. MEASURED.)
- `DECISIONS.md:8294` "ADR-232 — PROPOSED (ratified by merging its PR)". Its PR merged.
  H2-PLAN `:6` says it "now sits on main".
- `DECISIONS.md:8846` "ADR-255 A1" is placed after ADR-256, 57 lines below ADR-255. Every
  other amendment sits inside its ADR.

**Minimal delta.** Three one-line edits.

### M10 — A green verify still records nothing about what it skipped (carried M3)

MEASURED: `.harness/last-verify.json` after today's green run is
`{"target":"fast","exit":0,"git":"900963c",...,"tree_start":"","gates":[]}`. The `gates`
field now exists and is empty. The run printed two SKIPPED lines and four "DID NOT RUN"
warnings (sluice hold, mailbox delivery, the license rows for three submodules, and choc's
APPLY, ORDER and C1 to C38 rows). `verify fast` took **148 s** today against 122 to 132 s on
2026-10-10, under a lighter load (4.5 against 6 to 10), so roughly +20 s from new gates. The
brief `hypersaw-010` to autonomous (B455) proposes the receipt and is with autonomous until
2026-11-07.

---

## LOW

- **L1 — §5.8's counts are hand arithmetic, and have been wrong once.** `specs/SPEC-ENGINE.md:416-418`
  states 161 rows, 326 manifest parameters and 317 host parameters. Recomputed today from §5's
  tables (MEASURED): 18 + 10 + 47 + 30 (+2) + 27 + 27 = 161, which is correct. Commit `1da2106`
  ("§5.8's counts updated ... **the test author's finding**") shows they drifted once and were
  caught downstream. A 20-line count check beside H2's would be engine-free and would remove the
  hand step.
- **L2 — The wiring glob misses six check files.** `tools/test_table_check.py:186-187` covers
  `tools/*_check.{cpp,py,mjs}` and `tools/labharness/*_check.mjs` (widened since M1 of
  2026-10-10). Outside it: `tools/gui_webview_check.mm` and `tools/identity_store_check.mm` (no
  declaration), and `tools/patchspace/*_check.mjs` ×4, of which `listening_pass_check.mjs` has
  no declaration. All six are invoked by `verify`, so this is coverage of the declaration, not
  a missing gate.
- **L3 — Three of the ten stale CMake sentences remain** (carried M4). `CMakeLists.txt:469`,
  `:490` and `:511` say "NOT in ./verify" of `undo_check`, `ncap_check` and `tseed_check`.
  `verify` runs all three.
- **L4 — 145 of 186 playbook citations have drifted.** `playbook_check` today: "41 at their
  cited line, 145 drifted (advisory)". 78 % of the playbook's `file:line` anchors point at the
  wrong line, for example `src/hypersaw_clap.cpp:6472` now at `:7816`. The check finds every
  one by its anchor text, so dropping the line numbers loses nothing and stops the drift.
- **L5 — Root drops in the main checkout.** Untracked: five files byte-identical to tracked
  copies (`git hash-object` matched: three `.dc.html` files and two copies of
  `docs/design-system/support.js`), and one spec drop that no ROADMAP row names
  (`ROAM-spec.md`, 2026-08-31). The standing deletion permission covers merged mailbox
  duplicates only, so these are the human's to clear.
- **L6 — `docs/ROBUSTNESS.md`** still says "Last measured 2026-09-10" (`:3`) and carries every
  line Appendix A of the last audit listed.
- **L7 — Cadence.** This audit runs 11 days after the last, against CLAUDE.md's seven.

---

## A clean bill, where the sweep found one

- **The click detector's second copy is guarded.** `engine2/tools/click_detector.mjs` is
  byte-identical to `tools/click_detector.mjs` (`cmp`), and `engine2_cleanroom_check` item 1
  refuses an altered copy with a control. This is a second copy done right.
- **The builder's tree is clean by the repo's own test.** `engine2_cleanroom_check.audit_tree`
  run read-only over the builder's repository returns only 6 `outside-recipe` tags, all
  workspace files (brief, questions, answers, report, two relayed diffs). There is no
  old-engine identifier, no test-author-only data, no declared structure and no symlink.
- **Spec delivery is in sync.** All three copies of `SPEC-ENGINE.md` have blob `ba4ae7a`.
- **No placeholder corner name is product copy.** `git grep -i` for glass, grit, hollow and
  bloom in `src/`, `h2/` and `docs/design/` returns nothing (ADR-262 ruling 3).
- **Every `tools/*_check.*` file is named in `verify`.** The remaining unnamed tools are
  `*_probe` files driven by a wired `.py` check, or standalone by design.
- **The lab index is complete**: all 44 lab pages are linked from `docs/design/index.html`.
- **§11's open-item counts are right** (24 and 9, recounted).
- **The structure declaration tracks the spec**: `engine2_structure_check` is green on the
  current blob (65 blocks, 12 slots, 111 edges, 47 stages, 9 controls).
- **The 2026-10-10 audit's H1, H2, H4, M5 and M11 are closed** with measured controls
  (B455; `parity_floor_check`, `stop_gate_check`, `load_handoff_check` at 84 rows,
  `hostile_events_check` at 187 rows).

## What changed since the last audit (2026-10-10, `4a3dbaf`)

- **175 merges.** ADR headers rose from 209 to 268. ROADMAP rows from 451 to 529. Traces from
  434 to 490. No code has changed since 2026-10-18: every commit since then touched records,
  specs, mailboxes or design documents. That is why no trace is dated after 2026-10-17, and it
  is consistent with the trace rule.
- **Closed from the last audit:** H1 (13 parity gates floored), H2 (the Stop gate), H4 and M5
  (idle loads, ADR-208), M11 (mono velocity), the private finding (ADR-207). Partly: M1 (the
  glob gained top-level `.mjs`), M4 (10 stale CMake sentences down to 3), H5 (README refreshed
  2026-10-17, now stale again: M4 here).
- **Open and unchanged:** H3 (now H6, one oracle worse), H6 (now M8, 13.6 % larger), M3 (now
  M10), M7 and the CLAUDE.md list (now M7), ROBUSTNESS (L6).
- **New ground:** the engine rebuild (ADR-223), the clean room and its rulings log
  (ADR-247), spec rounds 9 to 27, the rack-slot contract's §12, the design system and GUI 3's
  interaction specs. Five of this report's six HIGH findings are in that new ground.

## Reduction budget

| finding | lines removed | lines added | files | checks |
|---|---|---|---|---|
| H1 | 0 (horde) | ~10 in B502 | 0 (a remote or a bundle, outside the tree) | 0 |
| H2 | 0 | ~40 check + ~150 provenance back-fill | +1 tool | +1 |
| H3 | 0 | ~6 status cells | 0 | 0 |
| H4 | 0 | ~7 root lists + ~30 controls + 3 golden entries | 0 | 0 (7 widened) |
| H5 | ~35 | ~30 | 0 | 0 |
| H6 | 0 | ~5 (ratchet) | 0 | 0 (1 tightened) |
| M1 | ~2 | ~8 contract; ~3 protected spec (sanction) | 0 | 0 |
| M2 | 0 | ~14 log rows + 3 cells | 0 | 0 |
| M3 | 0 | 1 workflow file (the human's) + 1 job | +1 (human) | 0 |
| M4, M5, M9, L6 | ~25 | ~35 | 0 | 0 |
| M6 | 0 | ~10 (one table) | 0 | 0 |
| M7 | ~40 | ~15 (the human's file) | 0 | 0 |
| M8 | B494's ~13 KB moved to a linked file; 2 duplicate ids renamed | 1 roadmap check (~80) | +1 log, +1 tool | +1 |
| L1 | 0 | ~20 | 0 (inside H2's tool) | 0 |
| L2 | 0 | ~4 glob + 3 headers | 0 | 0 |
| L3 | ~9 | 0 | 0 | 0 |
| L4 | 145 line numbers | 0 | 0 | 0 |
| L5 | 6 untracked files (the human's) | 0 | −6 untracked | 0 |
| **total** | **~256 lines, 6 untracked files** | **~460 lines** | **+3 tracked** | **+2 new, 8 widened or tightened** |

The added lines are mostly records and two engine-free checks. No gate is removed or relaxed.

## Not swept, stated rather than guessed

- **Per-sample cost** of `h2/engine/` and the legacy cores. The engine is being replaced, and a
  timing taken under a shared load would not be worth recording.
- **The clean-room code and tests themselves.** Reading them for findings would carry engine
  facts into horde's records, which ADR-247 and B519 forbid. Only metadata and the clean-room
  check's tags were taken.
- **Whether B525's separation holds** (H3). It needs each session to try.
- **The Artifact-hosted design system and the sub-and-noise lab** are not in the tree and were
  not read.
- **`reference/`, the 44 lab pages' contents, `integrations/`** beyond §12.5's cross-references,
  and the 490 traces.

## Proposed ROADMAP rows

The lead assigns ids and wording.

| # | row | from | acceptance |
|---|---|---|---|
| 1 | **The accepted engine gets a second copy, and B502 says where the record is** | H1 | Each clean-room repository has an off-machine copy whose hash is recorded at each stage acceptance; B502 names the repositories' current location and the record's rule, as the human rules. |
| 2 | **Spec provenance by requirement id, gated** | H2, L1 | A spec-only check in `verify fast` fails on a defined id with no provenance row and on wrong §5.8 counts, each with a must-fail control; rounds 17 to 27 back-filled; B518 then startable. |
| 3 | **ADR-247's preconditions discharged on the record** | H3, M2 | B519 and B525 carry dated statuses, with each session's own reachability test; the five B520 items each ruled or reverted; the rulings log covers ADR-246, ADR-255 and ADR-258. |
| 4 | **The armor sees `engine2/`** | H4 | The seven checks include `engine2`; a planted file under `engine2/` trips each; the render manifests and version-0 states are registered goldens. |
| 5 | **The design-system README carries ADR-259** | H5 | No conflict ADR-259 ruled is listed as open; the window reads 980×720. |
| 6 | **B256 with a ratchet** | H6 | The sanitize job fails above 19 not-built oracles now, and at 0 for the debug-export cause when B256 lands. |
| 7 | **Contract §12 coherent; the macro base stated once** | M1 | §12.5 amends RS-5.6; §12.3 to §12.5 carry the precedence sentence; the status line names all five amendments; SPEC-MODULE-MACROS matches (a sanction). |
| 8 | **Storage homes for the new GUI state** | M6 | Each row of M6's table has a home, an undo rule and a branch rule, in B532 or an ADR. |
| 9 | **Records refresh** | M4, M5, M7, M9, L3, L6 | README, H2-PLAN, CLAUDE.md (the human's), SPEC-ENGINE's status line, ADR-232's header, ROBUSTNESS and the three CMake sentences each match the tree on one date. |

## The five to act on first, and one question

1. **H1.** The only finding here that can lose work that cannot be rebuilt. A bundle is a
   minute's work.
2. **H3.** The human ruled the clean room's independence rules on 2026-10-19, and the record
   cannot show whether they were followed before stage 6 began.
3. **H2.** The independence audit's remedy (B518) depends on a file that stopped at round 16.
   A small engine-free check keeps it from stopping again.
4. **H4.** Widen seven roots before the first engine file lands in `engine2/`, not after.
5. **H5.** The design system is being built this week, against a README that rules the other way.

**The one question for the human.** ADR-247 makes the clean room's independence the condition
of trusting the new engine, and B502 says horde's `engine2/` is the record. Landing the builder's
code in horde would put it within reach of a fresh test-author tree. Should the accepted stages
land in horde only at cutover, with a private off-machine copy until then? Or should horde's
`engine2/` receive each accepted stage now, and the test author's tree stop being re-made from
horde's recipe?
