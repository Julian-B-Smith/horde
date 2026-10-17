# Module cutoff audit: where each 1.0 FX module stands against the module 1.0 bar (B509)

**Audit date:** 2026-10-17. **Cause:** ROADMAP B509 (the human, 2026-10-17: "Module cutoff date:
we should probably run a quick audit to see what's reasonable"), under ADR-190 A1.
**horde read at:** `origin/main` `7891327`, plus the records commit `ec16e9f` (branch
`lead-records-275`, PR #1124), which carries B508, B509 and the Shriek and Scape notices of
2026-10-17. Rows cited as "B5xx" or "ADR-235" come from that commit.
**Sibling repos read:** read-only, through `git show` and `git log`, with no checkout, fetch or
write. Sluice at `origin/main` `a99f7ad`; Shriek (repo `Maw`) at `main` `57eac7c`; Scape at `main`
`48caf22`; Bulwark at `main` `7f41c47`. Feature branches are named where they are cited. A row met
only on a feature branch counts as **in flight**, not met.
**Scope:** read-only. This file is the only change.

**Citation form.** horde paths are repo-relative. A sibling citation is `<Repo> <branch>:<path>:<line>`,
for example `Bulwark main:ROADMAP.md:173`. "no record" means the module's records are silent on
the row. It does not mean the row fails.

---

## Summary for the human

**The short answer.** I propose **Friday 2026-12-11** as the module cutoff. Three of the four
sibling modules can meet their own side of the bar by then at the pace they have actually kept.
The earlier option, **2026-11-13**, would keep only Bulwark. The later option, **2027-01-15**, is
the first date at which horde's own four simple FX could make it with some margin.

**Three findings change what the date means.**

1. **About a third of the bar can only be met inside horde, and horde has not built those parts
   yet.** The admission test (rack slot contract), the GUI 3 face, undo and the CPU reading on
   horde's bench are horde's to build. Each waits on horde's rack, shell, GUI 3 or history, and
   none of those has a date. On 2026-10-09 we told Bulwark there was "no honest date" for the
   rack (B450). If the cutoff judges those rows as well, **no module can meet any date** until
   horde catches up. **My recommendation:** on the cutoff date, judge the rows a module meets in
   its own repo. Judge the horde-side rows when horde hosts the module, and do not count horde's
   lateness against the module. This is a reading for you to rule on (Q1 below).
2. **horde's own four simple FX are the most at risk, not the siblings.** ECHO, EQ, the FX filter
   and drive have no C++, no row of their own (B452) and no scope sync. Their lab sync is stage
   S5, after the rack. If horde's lab sessions keep the pace they had from 2026-09-28 to
   2026-10-16, S5 starts around mid-November. That leaves about four weeks to build all four
   before 2026-12-11. If 1.0 without a delay, EQ, filter or drive is unacceptable, either pick
   2027-01-15 or bring their build forward.
3. **The Kuramoto chorus misses every option.** It is conditional, and nothing has started: no
   lab, no repo, no row beyond B401.

**Where each module stands** (module-side MUST rows only; the full tables follow):

| Module | Status today | Realistic date for its own MUST rows | Misses 2026-11-13? | Misses 2026-12-11? | Misses 2027-01-15? |
|---|---|---|---|---|---|
| **Bulwark compressor** | nearly ready: 15 of 19 met; switch crossfades (T-040) in progress | **2026-10-31 to 2026-11-07** | no | no | no |
| **Bulwark multiband + ATM** (conditional) | built and signed by ear; ATM quiet-material stress case queued | **2026-11-07** | no | no | no |
| **Bulwark master limiter** | built; the human's listening sign-off open since 2026-10-09 | when the human listens | no (human-paced) | no | no |
| **Scape** | C++ core built (M1 open); M2 work (I/O gain, macros, presets, classes) queued or not started | **2026-11-14 to 2026-11-28** | **likely misses** | no | no |
| **Shriek** | C++ is the golden; contract gaps half closed on a draft PR; own 2× waits on horde's rulings (B508) | **2026-11-21 to 2026-12-04**, if horde rules B508 by 2026-10-31 | **misses** | at risk if B508 slips | no |
| **Sluice** | most features built; contract gaps (Q-057) "not scheduled"; horde takes its code only at Sluice V1, which has no date | **2026-12-04 to 2026-12-18**, if Q-057 is scheduled now; V1: no record | **misses** | **at risk** | at risk (V1 undated) |
| **ECHO, EQ, FX filter, drive** (horde's own) | labs only; no C++; no rows | **projected mid-December at the earliest** | **misses** | **likely misses** | at risk |
| **Kuramoto chorus** (conditional) | not started | no record | **misses** | **misses** | **misses** |

**What you are asked to decide.** The date (Q2). The two-part reading (Q1). And four readings the
audit found open in the bar itself (Q3 to Q6, at the end).

---

## 1. The bar's MUST rows, numbered for the tables

From `docs/proposals/module-1.0-bar.md` (ratified 2026-10-04), with ADR-201, ADR-203, ADR-218 and
ADR-234 applied. The column "judged where" is this audit's reading of where the evidence can
exist today.

| # | MUST row | Bar line | Judged where |
|---|---|---|---|
| R1 | Scope list, frozen at the module's lab sync | 23 | module writes it; it freezes at horde's lab sync (ADR-190 A1, stage S5 of `docs/H2-PLAN.md`) |
| R2 | Manifest: key, unit, range, default, morph class, rate class; keys frozen with a lockfile gate | 24 | module |
| R3 | Rack slot contract: admission, bypass, off, latency, tail | 30 | module-side conformance in the module; **the admission test is horde's**, unbuilt (contract Q13, `docs/proposals/rack-slot-contract.md:369`) |
| R4 | I/O gain standard, tests 1–7 | 31 | module, except the slot double-gain test, which needs horde's admission test |
| R5 | Macros, with a round-trip test | 32 | module |
| R6 | Morph and presets: patch per corner, Device class never morphs, preset save and load | 33 | module, then horde's corner test (A13) |
| R7 | Deterministic | 39 | module |
| R8 | Sample-rate (44.1 to 192 kHz) and block-size (1 to 4096) independence | 40 | module |
| R9 | Real-time safe: no allocation, locks or syscalls; FTZ/DAZ; non-finite injection | 41 | module |
| R10 | Its own fidelity suite, with must-fail controls | 42 | module |
| R11 | Silence in gives silence out; tails reach exact zero | 43 | module |
| R12 | Mono-safe: B408 metrics measured and stated | 44 | module |
| R13 | CPU in Release, ratio to the calibration loop, within 2 % / 4 % of min-spec (appendix); memory per instance is also a MUST (ADR-201 Q11) | 51, 107 | module measures; **horde measures on its own bench** (ADR-201 Q11) |
| R14 | A face and expanded view in GUI 3, keyboard-reachable; every key a display name and a normalised law (ADR-203) | 58 | names and laws: module. **The face: horde's GUI 3** (stage S6) |
| R15 | Undo covers every control | 59 | **horde's history** (B389), plus the module's gesture brackets (contract RS-6.14) |
| R16 | Factory presets: an init plus a small set | 60 | module; the set's size is set by horde's presets lab (B395, stage S7) |
| R17 | Independent critic review, no open HIGH (ADR-203: a fresh-context same-family critic before 1.0) | 67 | module or horde |
| R18 | The human's listening sign-off: module, factory presets, one stress case | 68 | the human, recorded in horde's ROADMAP |
| R19 | Name cleared (B433) | 69 | horde ran zero-cost knockout screens on 2026-10-04 (B433); whether those count as "the clearance memo" is open (Q5) |

**Horde-side rows today.** horde's rack is unbuilt: "no honest date, the rack is stage S5 and we
are at about S1–S2" (B450, 2026-10-09). The shell skeleton is not built (`docs/H2-PLAN.md`
§Shell). GUI 3 is planned (stage S6). History is planned (B389). The presets lab is "OPEN — later"
(B395). So R3's admission test, R14's face, R15 and R13's bench reading are **not started on
horde's side for every module**. The tables below say this once per row as "(horde: not built)"
rather than marking every module down for it.

---

## 2. Bulwark (compressor; the master limiter; multiband + ATM)

**Status.** The closest module to admission. Spun up 2026-10-03. M1 to M3 are closed and M4's
module-side work was done by 2026-10-12 (`Bulwark main:ROADMAP.md:173-182`). Its readiness notice
(seq 18) was answered by ADR-218. Its `switch-classes` brief (seq 20) was answered by ADR-234 on
2026-10-17 (seq 21, ball Bulwark). Bulwark also ran its own audit of this bar on 2026-10-09
(`Bulwark main:docs/audits/2026-10-09-module-bar-audit.md`, verdict "REWORK for bar readiness").
Every PARTIAL or GAP it found was closed by T-021 to T-024 and T-027 (`Bulwark
main:ROADMAP.md:166-171`), except name clearance, the limiter's listening and the ATM stress case.

**Compressor face.**

| # | Status | Evidence | Citation |
|---|---|---|---|
| R1 | met (list); freeze: no record | scope list written; multiband/ATM marked conditional; no lab-sync freeze yet | `Bulwark main:spec/SPEC-BULWARK.md:214-226` |
| R2 | met | 56 keys with unit, range, default, morph, rate, name and law; a lock gate diffed against a pinned baseline | `Bulwark main:core/manifest.lock.json`; `Bulwark main:ROADMAP.md:166-168` (T-021, T-023) |
| R3 | in flight | module-side self-admission suite A1–A20 with must-fail controls (T-031); detector-switch crossfades and the switch table (T-040) in progress after ADR-218 / ADR-234; `comp.scOn` to become a Device key (ADR-234 item 1). (horde: not built) | `Bulwark main:ROADMAP.md:176,185`; DECISIONS.md ADR-234 |
| R4 | met (module side) | `io.inGain` / `io.outGain` ±24 dB, ramp, pre/post taps with latches and plants; the slot double-gain test waits on horde | `Bulwark main:docs/audits/2026-10-09-module-bar-audit.md:71-83` |
| R5 | met | four ordered, labelled slots; round-trip test; `exp` curve added | `Bulwark main:ROADMAP.md:153,167` (T-012, T-022) |
| R6 | met (module side) | corner-flip test with Device keys untouched; presets as a checked-in `ModulePreset` file with an A12 round trip | `Bulwark main:ROADMAP.md:153,175` (T-030) |
| R7 | met | 192 exact golden hashes; a static scan of `core/` for RNG and clocks | `Bulwark main:ROADMAP.md:162,172` |
| R8 | met | chunked bit-identity at n ∈ {1, 7, 64, 256, 4096} at all four rates | `Bulwark main:docs/audits/2026-10-09-module-bar-audit.md:45` |
| R9 | met, with a caveat | allocation probe sees nothrow `new` and `malloc`; FTZ and non-finite tests with plants; locks and syscalls checked by reading, not probed | `Bulwark main:core/tests/alloc_probe.h:1-30`; `Bulwark main:ROADMAP.md:172` |
| R10 | met | 29 of 29 M3 mutants caught; the M4 critic's seven surviving mutants each caught after T-035 | `Bulwark main:ROADMAP.md:179,181` |
| R11 | met | exact-zero tests; the at-rest tail (A13) renders bit-identically to a fresh instance | `Bulwark main:ROADMAP.md:181` (T-037) |
| R12 | met | B408 report covers single-band, multiband and limiter | `Bulwark main:docs/stereo/2026-10-09-b408-report.md` |
| R13 | met (reported) | normalised % of min-spec: defaults 1.23, worst 1.33 (budget 2 / 4). The calibration loop read about 194 ms against horde's 111–119 ms, so the figures are normalised, not unloaded. Memory 145,072 B at 44.1 kHz. (horde bench: not built) | `Bulwark main:docs/cpu/2026-10-08-multiband.md:88-104`; `Bulwark main:docs/cpu/2026-10-09-memory.md` |
| R14 | names/laws met; face not started | display names and laws in the manifest (T-021); the face is horde's GUI by Bulwark's spec | `Bulwark main:ROADMAP.md:166`; `Bulwark main:spec/SPEC-BULWARK.md:236-250` |
| R15 | no record | state API exists; undo is horde's | `Bulwark main:docs/audits/2026-10-09-module-bar-audit.md:57` |
| R16 | met | nine factory presets, Init among them | `Bulwark main:core/include/bulwark/presets.h:259-268` |
| R17 | met | M1, M2, M3 and M4-ready critics; no open HIGH; a different-family review is owed before any binary ships (T-039) | `Bulwark main:ROADMAP.md:184`; ADR-203 item 1 |
| R18 | met for the compressor | every factory preset "ok" and STRESS accepted (D-044, 2026-10-08) | `Bulwark main:DECISIONS.md:1086-1118` |
| R19 | no memo in Bulwark | a web collision search only (2026-10-03); horde's B433 knockout screen read Bulwark **LOW** (2026-10-04) | `Bulwark main:docs/prior-art.md:183`; ROADMAP B433 |

**Master limiter** (bar §"The master limiter"; budget ≤ 0.5 %). CPU 0.06 % at defaults and 0.13 %
worst (`Bulwark main:docs/cpu/2026-10-08-multiband.md:103-104`). Critic met (D-039). The clip latch
reads the pre-limiter tap. **Open:** the human's listening sign-off and stress case (T-019, HIGH;
renders on the sign-off page since 2026-10-09, `Bulwark main:ROADMAP.md:164`), and the must-fail
control for `lim.autoRel`'s declared seeded jump (ADR-234 item 2; T-040).

**Multiband + ATM (conditional).** Built, M3 closed 2026-10-08 (D-050); critic met; ATM signed by
ear at its starting values (D-048); CPU 1.23 % / 1.36 % (`Bulwark
main:docs/cpu/2026-10-08-multiband.md:101-102`). **Open:** T-025, the ATM stress case on quiet
material (queued), and the multiband group-delay statement (ADR-234 item 3).

**Pace.**
- **Milestones:** Phase 0 to Phase D closed 2026-10-03 to 2026-10-04. M1 was merged 2026-10-04 and
  closed 2026-10-08. M2 was built and critic-met on 2026-10-05. M3 closed 2026-10-08. M4's module
  side was merged 2026-10-12 (`Bulwark main:ROADMAP.md:143-182`).
- **Rows closed:** about 25 ROADMAP rows from 2026-10-03 to 2026-10-09, and 8 from 2026-10-10 to
  2026-10-16.
- **No Bulwark commit on 2026-10-14, 10-15 or 10-16.** It was waiting on horde's ruling, which
  came as ADR-234 on 2026-10-17 (`git log` on Bulwark).

**Realistic date.** T-040 is one chain (revise A14, make `comp.scOn` a Device key, the switch
table, a critic, a notice: `Bulwark plan-map:DECISIONS.md:1964-1985`, D-081, feature branch). M4's
module side took Bulwark two days. So **2026-10-31 to 2026-11-07**, plus the human's limiter
listening.

**Records gap.** Bulwark's `main` does not carry horde's inbound `brief-engine-rebuild-audit`. It
sits only on `origin/mailbox-hypersaw-brief-engine-rebuild-audit` (`811b344`, 2026-10-15). Bulwark's
D-081 says "no such brief is in our mailbox" (`Bulwark plan-map:DECISIONS.md:1964-1985`). The
brief's ball is Bulwark's.

---

## 3. Scape (the reverb)

**Status.** Spun up 2026-10-02. Phases 0, A and G are closed. M1, the C++ core, is open: Q-017
waits on FOUNDATIONS' reply, due 2026-10-30, and Q-018, the swap, is queued. M2, hosting in horde,
has not started, apart from prep on `m2/prep` (`3c8dc14`, unmerged). Its engine-rebuild notice
(seq 2, 2026-10-17, ball none) says that, as far as the contract is built and tested, it conforms
(`integrations/scape/notice-engine-rebuild-audit.md`, records commit). Scape has no audit of this
bar.

| # | Status | Evidence | Citation |
|---|---|---|---|
| R1 | no record | no ship / post-1.0 list; the echo-range half of OQ-4 open | `Scape m2/prep:ROADMAP.md:307` |
| R2 | in flight | key, min, max, default for 27 parameters; unit, rate class and a lockfile gate: no record; classes ratified but not built (Q-023) | `Scape main:core/src/reverb.cpp:54-61`; `Scape m2/prep:docs/proposals/m2-parameter-classes.md:13-24` |
| R3 | in flight | FX-operator surface, latency 0, declared tail, `min_block` 1, latching guard, reset identity, bounded state, no allocation (D-052); owed: classes, I/O gain, presets and macros, the click readings. (horde: not built) | `Scape main:DECISIONS.md:814-834`; `Scape m2/prep:docs/audits/2026-10-17-engine-rebuild-audit.md:30-48` |
| R4 | not started | Q-022 queued | `Scape m2/prep:ROADMAP.md:282-287` |
| R5 | not started | in the M2 row only; no code, no test | `Scape main:ROADMAP.md:27` |
| R6 | not started | in the M2 row only; Era ruled flip-class with a 20 ms switch | `Scape main:ROADMAP.md:27`; `Scape m2/prep:DECISIONS.md:835` |
| R7 | met | two-instance bit-identity with modulation on; static ban on clocks and RNG; both in `./verify fast` | `Scape main:core/tests/core_tests.cpp:123` |
| R8 | in flight | blocks 1, 7, 64, 333, 4096 bit-identical; 44.1, 48 and 96 kHz within 0.25 dB energy decay; 192 kHz on the C++: no record | `Scape main:core/tests/core_tests.cpp:95-121` |
| R9 | in flight | allocation counting with a plant; NaN/Inf injection with a latching guard; FTZ by an in-algorithm floor; locks/syscalls probe and a CPU FTZ/DAZ mode: no record | `Scape main:core/tests/core_tests.cpp:154,199-236`; `Scape main:core/src/dsp_math.h:18,29` |
| R10 | met | fidelity suite with clean and known-bad controls for every check, gated in fast and full | `Scape main:DECISIONS.md:492` (D-029) |
| R11 | met | exact silence at maximum feedback with an impulse control; tails reach exact 0 | `Scape main:core/tests/core_tests.cpp:131,140` |
| R12 | not started | mono-sum safe by construction, stated; no B408 metric measured | `Scape main:docs/prior-art.md:54-61`; `Scape main:ROADMAP.md:27` |
| R13 | in flight | horde's frozen protocol, M3 × 1.5: Clean defaults **2.20 %** at 44.1 kHz (budget 2 %, **over by 10 %**), 2.40 % at 48 kHz; worst 2.23 % (budget 4 %); Decay automated 10.33 % (the budget has no row for it); memory 6.57 MiB; the fix is Q-019, queued next | `Scape m2/prep:docs/audits/2026-10-17-engine-rebuild-audit.md:60-74` |
| R14 | no record | `ParamInfo` has name, min, max, default only | `Scape main:core/include/scape/reverb.h:54` |
| R15 | no record | | |
| R16 | not started | "still to be written, M2" | `Scape m2/prep:docs/audits/2026-10-17-engine-rebuild-audit.md:107-108` |
| R17 | in flight | the lab's critic re-audit passed with no CRITICAL/HIGH; the port's critic is Q-018 criterion 1, queued | `Scape main:DECISIONS.md:532-541`; `Scape main:ROADMAP.md:237-241` |
| R18 | not started | the human's listening rulings cover lab features; no module, preset or stress sign-off | `Scape main:DECISIONS.md:532` |
| R19 | not started in Scape | a trademark search is owed in Phase N; horde's B433 screen read Scape **MEDIUM** ("fine as a module label and not as a standalone product") | `Scape main:ROADMAP.md:28`; ROADMAP B433 |

**Pace.**
- **Milestones:** Phase A closed 2026-10-08, Phase 0 on 2026-10-09 and Phase G on 2026-10-12. M1
  opened 2026-10-12 (`git log` on Scape).
- **Queue:** 16 items closed in the 13 days from 2026-10-02 to 2026-10-14. About 13 of them closed
  by 2026-10-09 and 3 from 2026-10-10 to 2026-10-17, so **about 3 a week recently**.
- **Commits on main:** one a day on 2026-10-14, 10-15 and 10-16.

**Realistic date.** About 7 queued items (Q-017 to Q-023), plus about 5 that are not yet queued
(macros, presets, corners, B408, the module listening). At 3 a week that is about 4 weeks; at its
lifetime pace, about 2. FOUNDATIONS' reply is due 2026-10-30. So **2026-11-14 to 2026-11-28**.

---

## 4. Shriek (formerly MAW)

**Status.**
- **Integration branch:** `main`. `origin/spinup` is the original spin-up branch and is not an
  ancestor of main.
- **M1:** first marked "complete" on 2026-10-17, and only on the draft PR #76 branch
  `q023-contract-gaps`. Main's header still reads "M1, about two-thirds through" (`Shriek
  main:ROADMAP.md:8` against `Shriek q023-contract-gaps:ROADMAP.md:8`).
- **Contract work:** PR #76 (draft) closes half the contract gaps: FTZ in `process`, a counting,
  latching NaN guard, the memory figure, the handle.
- **Plan map:** "what is left before horde admits Shriek" is on the same branch
  (`docs/design/shriek-plan-map.html`).
- **Two questions to horde:** both carry `ball: horde`. They are notice `engine-rebuild-audit`
  (seq 2) and brief `maw-004` (seq 3, respond by 2026-10-31). horde carries both as B508.

| # | Status | Evidence | Citation |
|---|---|---|---|
| R1 | not started | no scope section and no queue row (Shriek's own plan map) | `Shriek q023-contract-gaps:docs/design/shriek-plan-map.html:394-398` |
| R2 | in flight | key, range, default in `param_table.h`; addresses append-only (D-037); morph class is only stepped vs scalar; no lockfile diff gate | `Shriek main:docs/param-table.md:17-35`; `Shriek q023-contract-gaps:verify:68-84` |
| R3 | in flight | on the draft branch: FTZ, the latching guard, `memoryBytes`, the handle; open: `tail()` and the morph / flip / Device classes. (horde: not built) | `Shriek q023-contract-gaps:ROADMAP.md:637-676` |
| R4 | in flight | the B435 20 ms ramp is built (D-049); the pair itself is the slot's (D-050); tests 1–7: no record | `Shriek q023-contract-gaps:ROADMAP.md:579-584` |
| R5 | met | eight ordered slots, `lin` / `exp` / `log`, labels per preset, `macro_check` ctest on main; Q-020's formal close is the human's | `Shriek main:mawcore/CMakeLists.txt:85`; `Shriek q023-contract-gaps:ROADMAP.md:586-603` |
| R6 | in flight | module presets file exists; no Device class declared; corner test unknown | `Shriek q023-contract-gaps:docs/design/shriek-plan-map.html:485-503` |
| R7 | met | static RNG/clock ban, same-seed bit-identity and reset determinism | `Shriek main:tools/lab_smoke.mjs:1-20`; `Shriek main:mawcore/tests/abi_test.cpp:9` |
| R8 | in flight | blocks 1, 7, 64, 256 bit-identical; 44.1, 48 and 96 kHz in the lab gates; 192 kHz and a stated tolerance: no record | `Shriek q023-contract-gaps:ROADMAP.md:210` |
| R9 | in flight | main: zero allocations in the timed loop, non-finite recovery with a plant; FTZ and the counting guard only on the draft branch | `Shriek main:mawcore/tests/abi_test.cpp:13`; `Shriek q023-contract-gaps:ROADMAP.md:639-643` |
| R10 | met, with a caveat | property battery 71 pass plus 3 declared fails, ledger-held; the aliasing ceiling must be re-proved for its own 2× (Q-026) | `Shriek q023-contract-gaps:ROADMAP.md:24-26,259-270` |
| R11 | in flight | silence in gives exact silence; tails: 8 of 13 presets reach exact zero, 5 hold 3e-308 to 5e-307 forever; it needs a rest mechanism, "a design choice for the human" | `Shriek q023-contract-gaps:ROADMAP.md:653-662` |
| R12 | no record | no B408 report | |
| R13 | in flight | C++ at E-6, M3 × 1.5: at 1×, every preset ≤ 2 % after the 2026-10-17 speed-up (fractal grit 1.80 %); stress cases 3.7–4.4 %; at its own 2× (the human's chosen default, Q-026), 10 of 13 presets over 2 % and all within 4 %. Memory 722,784 B at 44.1 kHz (draft branch). Not yet a ratio to horde's calibration loop | `Shriek main:docs/audits/2026-10-17-engine-rebuild-audit.md:100-136`; `Shriek main:traces/2026-10-17-speedup.md:23-27` |
| R14 | no record | no display-name or law field found | |
| R15 | no record | | |
| R16 | in flight | 13 presets with a neutral Init; Q-010 open on the human's criterion 3; Q-026 will change every preset's default mode | `Shriek q023-contract-gaps:ROADMAP.md:314-341,720-726` |
| R17 | not started | build-review critics exist, none of the bar's shape | `Shriek q023-contract-gaps:docs/design/shriek-plan-map.html:407-411` |
| R18 | not started | many listening rounds shaped the presets; no sign-off of this form | `Shriek q023-contract-gaps:docs/design/shriek-plan-map.html:412-416` |
| R19 | no memo in Shriek | horde's B433 group-D screen read SHRIEK **LOW** ("every exact SHRIEK mark is dead") | `Shriek q023-contract-gaps:DECISIONS.md:2105-2108`; ROADMAP B433 |

**Pace.**
- **Milestones:** spun up 2026-09-19. M1's gates 1 to 3 were merged 2026-09-20 to 2026-09-27.
- **PR merges on main, per week from 2026-09-20:** 18, 18, 22, 12.
- **Queue items closed, same weeks:** about 10, 4, 4, 2 (a hand count; approximate).

**Realistic date.** The critical path, from Shriek's own plan map, runs: the human's
rest-mechanism ruling, then `tail()` and the classes, then its own 2× once horde answers B508,
then the critic, the listening and the name
(`Shriek q023-contract-gaps:docs/design/shriek-plan-map.html:572-585`). Q-026 (its own 2× in C++
and the lab twin) is the largest item. At 2 to 4 items a week, with about 10 rows open, that gives
**2026-11-21 to 2026-12-04**. This holds only if horde answers B508 by its respond-by date,
2026-10-31.

**Records gap (horde's side).** horde still projects Shriek at about 9.7 % per instance: the bar's
appendix (`docs/proposals/module-1.0-bar.md:123-124`) and the contract's §11
(`docs/proposals/rack-slot-contract.md:396`). Shriek traces that figure to our B262 run, which
timed the JS lab at 8× per-stage oversampling. The C++ has no oversampler and measures ≤ 2 % at
1× (`Shriek main:docs/audits/2026-10-17-engine-rebuild-audit.md:135-136`).

---

## 5. Sluice

**Status.** Spun up 2026-09-15, the oldest sibling, with the most features built (26 presets, 5
factory patches, I/O gain, macros, per-corner patches). Its records never mention this bar, apart
from quoting our macro row once (`Sluice origin/main:integrations/hypersaw/notice-macros-corrected.md:52`).
Its contract-gap row Q-057 reads "OPEN, not scheduled" (`Sluice origin/main:ROADMAP.md:368-369`).
**The consumption rule adds a gate no other module has:** horde takes Sluice's code "once it hits
V1", "At Sluice V1 (public)" (Sluice D-096, `Sluice origin/main:DECISIONS.md:3238-3246`). V1 has
no recorded criteria or date. A public release first needs Sluice's Phase N, a prior-art and IP
re-scan with the human's go or no-go (`Sluice origin/main:ROADMAP.md:110-113`). Sluice is on
horde's critical path (`rack` → `sluice` → `fxmorph`, `docs/H2-PLAN.md` §Critical path).

| # | Status | Evidence | Citation |
|---|---|---|---|
| R1 | no record | only parked items ("until after horde 1.0") | `Sluice origin/main:ROADMAP.md:623,641,721` |
| R2 | in flight | generated manifest with key, unit, range, default, taper, smoothing, morph law; not horde's morph / flip / Device classes (Q-057 item 4); G-102 is a drift check, not a key-freeze lockfile; host ids frozen (G-97) | `Sluice origin/main:docs/horde/sluice-manifest.json`; `Sluice origin/main:ROADMAP.md:368-388` |
| R3 | in flight | 2026-10-16 self-audit: non-finite guard, FTZ and exact-zero tails, `tail()` and the flip-class switch declaration **not met**; the limiter's look-ahead now fixed at activate and reported (D-140). (horde: not built) | `Sluice origin/main:docs/audits/2026-10-16-engine-rebuild-audit.md:90-116` |
| R4 | met (module side) | `io.inGain` / `io.outGain`, ±24 dB, 20 ms ramps, meters with clip latches, each check red on a sabotage (Q-054); tests 1–7 not mapped one by one | `Sluice origin/main:ROADMAP.md:322-333` |
| R5 | met | macros by order with the patch's labels and curves; round trip in G-69, G-70, G-108; four slots (the bar allows up to eight) | `Sluice origin/main:docs/acceptance.md` G-69, G-70, G-108 |
| R6 | in flight | per-corner patches built (D-105); the Device-class `os` cannot restore exactly in a slot (its seq 44); corner-flip and horde save/load tests: no record | `Sluice origin/main:docs/horde/SLUICE-IN-HORDE.md:216-226,403` |
| R7 | met | G-01 bit-exact; static RNG/clock ban and same-seed identity in `./verify fast` | `Sluice origin/main:tools/lab_smoke.mjs:1-12`; `Sluice origin/main:verify:86-90` |
| R8 | in flight | chunked render bit-identical for splits 1 to 4096; 44.1, 48 and 96 kHz claims; 192 kHz: no record | `Sluice origin/main:netcore/tests/node_contract.cpp:143,529` |
| R9 | in flight | allocation counter; FTZ not met; NaN/Inf zeroed silently with no counter; stability fuzz not gated | `Sluice origin/main:netcore/tests/node_contract.cpp:43,82`; `Sluice origin/main:verify:139-142` |
| R10 | met, with a caveat | gauntlet of 116 claims (110 pass, 1 declared fail, 5 untestable), its ledger gated in `./verify fast`; the separate fidelity report is "a report, not a gate" | `Sluice origin/main:docs/gauntlet-report.md:5`; `Sluice origin/main:docs/fidelity-report.md:3` |
| R11 | in flight | silence in gives exactly 0; of 19 non-endless presets, 8 reach exact 0 and the rest sit at denormals | `Sluice origin/main:docs/audits/2026-10-16-engine-rebuild-audit.md:108` |
| R12 | no record | only dual-mono exactness (G-40) | |
| R13 | in flight | indicative M3 figures, not a calibration-loop ratio: at 1×, **12 of 26 presets over 2 %** of min-spec (median 0.98 % of an M3 core); the full chain cap at 1× reaches **14.4 %** of min-spec against 4 %; at 4×, 17 of 26 presets over 6 %. Memory measured (met). Its own perf gate awaits a human ruling (Q-024) | `Sluice origin/main:docs/audits/2026-10-16-engine-rebuild-audit.md:118-156` |
| R14 | no record | lab views and a plugin window; no GUI 3 or ADR-203 record | `Sluice origin/main:ROADMAP.md:798` |
| R15 | not started | a ruling only (patch JSON per step, D-063) | `Sluice origin/main:DECISIONS.md:1706` |
| R16 | in flight | 26 presets plus 5 factory patches as 31 horde patch files (G-115); an Init: no record | `Sluice origin/main:docs/acceptance.md` G-115 |
| R17 | no record (module level) | per-feature critic reviews; none of the whole port and its oracles | `Sluice origin/main:DECISIONS.md:2900,3003,4588` |
| R18 | no record | per-feature ear rulings; no module, preset or stress sign-off | `Sluice origin/main:DECISIONS.md:4401` |
| R19 | not started in Sluice | "not a trademark search", Phase N; horde's B433 screen read Sluice **LOW** | `Sluice origin/main:DECISIONS.md:146`; ROADMAP B433 |

**Pace.**
- **PR merges on main, per week from 2026-09-19:** 39, 42, 33, 27.
- **Queue rows closed, same weeks:** about 7, 13, 5, 5.
- **Kind of work:** recent work is mostly features (presets, tags, factory patches, duck routing).
  Contract-gap work is a small share.
- **CI:** cloud CI off since 2026-10-13 (D-128).

**Realistic date.** About 15 rows are open or unrecorded. At its recent 5 rows a week, that is
about three weeks, if Q-057 and the bar rows are scheduled ahead of features. `tail()` also waits
on FOUNDATIONS slice 2 (`Sluice origin/main:ROADMAP.md:293`). So **2026-12-04 to 2026-12-18**, if
the work starts now. **V1, which horde's intake waits on, has no record of a date.**

**Records gap.** Sluice's seq 47 and 48 ask horde to reconcile ADR-166 A1 and ADR-202 with the
human's ruling that horde inherits Sluice's XY. Both carry ball HYPERSAW
(`Sluice origin/main:integrations/hypersaw/notice-xy-and-presets.md:20-33`). horde's own mailbox
copy of `integrations/sluice/` was not cross-read for those two in this audit.

---

## 6. horde's own modules: ECHO, EQ, FX filter, drive; and the Kuramoto chorus

These have no repo and are measured from horde's records only.

| # | ECHO (B425) | EQ, FX filter, drive (B452) | Kuramoto chorus (B401, conditional) |
|---|---|---|---|
| Overall | **planned**; scope sketched ("a clean stereo delay with tempo sync, ping-pong, mid/side, feedback filtering and modulation"); built in horde, not a sibling (ruled 2026-10-04) | **planned**, "each with no row of its own" (`docs/H2-PLAN.md` §FX); each "needs a design lab and its module 1.0 bar evidence before the module cutoff" | **planned**; "designed in its own lab", not started |
| R1 | in flight: B425's one-line scope; no frozen list | not started; the EQ design exists in the FX lab round 2 (B234), and drive and filter in round 1 (B210) | not started |
| R2–R13 | not started: no horde 2 C++. The legacy rack's C++ Echo, Delay, Drive and Filter exist (`src/fx_rack.h`, measured in B262), but no record says horde 2 reuses them | not started (same) | not started |
| R14 | not started; screen styles ruled "one glass each" (ADR-235 item 1, B336); names open (B336) | not started; names open (B336) | not started |
| R15–R18 | not started | not started | not started |
| R19 | B433: ECHO "fine as a plain descriptive label and not as a brand" | no names yet (B336) | no name |

**Pace.** There is no build pace to measure: nothing has started. Their lab sync is stage S5
("FX modules and screens: the rack-slot contract first, then the hosted siblings and horde's own
(ECHO, EQ, FX filter, drive); the conditionals by the module cutoff", `docs/H2-PLAN.md` §Order of
work, Track B). S5 follows S2, S3 and S4. **horde's measured lab-sync pace:**
- **S0 and S1:** S0 was done on 2026-10-02 (B407). S1's engine decisions were made 2026-10-02/03
  (B376, B386). S1 then grew into the engine re-spec, merged 2026-10-16 (#1097). That is about 18
  days for two stages, from B331's proposal on 2026-09-28.
- **S2:** its head, the mod matrix lab, has read "OPEN — priority" since 2026-10-01 (B392) and has
  not started.
- **Projection:** at about 9 days a stage, S2 to S4 take about 27 days, so S5 starts **around
  2026-11-13**. That is arithmetic on one data point, not a plan.
- **Build analogue:** a dedicated session took Bulwark from spin-up (2026-10-03) to its module-side
  bar (2026-10-12) in about 9 days.

**Realistic date.** ECHO, EQ, filter and drive could finish **mid-December at the earliest**. That
needs S5 to start on the projection and the four to be built in parallel.

The Kuramoto chorus: **no record** to date it from.

---

## 7. The proposed cutoff, and two alternatives

**The constraint from above.** ADR-190 A1 puts the cutoff "before the release candidate". The
release needs the presets lab and factory bank (stage S7), and those "need first" every 1.0 FX
module (`docs/H2-PLAN.md` §Presets). So the cutoff must fall **before the presets lab starts**,
or the bank is built twice.

**The constraint from below.** The cutoff cannot usefully fall before a module can finish its own
rows at its measured pace (§2 to §6).

| Option | Date | Meets it (module-side rows) | Misses it |
|---|---|---|---|
| Earlier | **Fri 2026-11-13** | Bulwark compressor, multiband + ATM; the limiter if the human listens | Scape (likely; at its recent pace it needs to about 2026-11-14 to 11-28); Shriek; Sluice; ECHO, EQ, FX filter, drive; the Kuramoto chorus |
| **Proposed** | **Fri 2026-12-11** | Bulwark (all three faces); Scape; Shriek, if horde rules B508 by 2026-10-31 | **At risk:** Sluice (only if Q-057 is scheduled now; V1 has no date); ECHO, EQ, FX filter, drive (only if S5 starts by about 2026-11-13 and the four are built in parallel). **Misses:** the Kuramoto chorus |
| Later | **Fri 2027-01-15** | the above, plus margin for horde's four simple FX and for Sluice's module-side rows | **At risk:** Sluice's V1 (no date in its records). **Misses:** the Kuramoto chorus, unless its lab starts by about December |

**Why 2026-12-11.**
- Every sibling with a measured pace can meet its own rows by then, with one to three weeks of
  slack (Bulwark about five weeks, Scape about two, Shriek about one).
- It sits before any plausible start of the presets lab, which follows S5 and S6.
- It names the two real risks plainly: Sluice's scheduling and V1, and horde's own four modules.
  Neither is a sibling's pace problem. Both can be moved by a decision rather than by waiting.

**What would move each risk.**
- **Sluice:** schedule Q-057 and the unrecorded bar rows. Ask Sluice for V1's criteria and date.
  Or rule that horde may admit Sluice's code before V1.
- **ECHO, EQ, filter, drive:** split B452 into rows, and run their labs before S2 to S4, which
  B331's order does not forbid. Or accept 2027-01-15.
- **Shriek:** answer B508 by 2026-10-31.

---

## 8. Open questions for the human

| Q | Question | The audit's lean |
|---|---|---|
| Q1 | On the cutoff date, are the horde-side rows (R3's admission test, R14's face, R15, R13 on horde's bench, R4's slot double-gain test, R6's corner test in horde) judged as well, or judged at hosting? | **At hosting.** Judge the module-side rows on the date. A module whose module-side rows are met is not cut because horde's rack, shell, GUI 3 or history is late. |
| Q2 | The date | **2026-12-11**; earlier 2026-11-13; later 2027-01-15 |
| Q3 | R13's budget is a MUST, but the bar says CPU is "never a verify gate" (`module-1.0-bar.md:119`). Does a module over budget at the cutoff miss 1.0? The live cases: Scape's Clean default at 2.20 %; Sluice's 12 of 26 presets over 2 % and its chain cap at 14.4 % worst; Shriek's own 2× defaults, which wait on its B439 amendment (B508 Q2) | needs a ruling; the audit does not lean |
| Q4 | The budget has no row for a module while its design parameters move (Scape measures 10.33 % with Decay automated, and under morph they move continuously) | needs a ruling or a measurement row |
| Q5 | Does horde's B433 zero-cost knockout screen (2026-10-04, in the gitignored `local/trademark/`) count as R19's "clearance memo" for 1.0? No sibling has a memo of its own; each defers to its Phase N | needs a ruling; B433 itself says it is "not a legal clearance" |
| Q6 | The bar lists memory as a SHOULD (`module-1.0-bar.md:52`); ADR-201 Q11 made it a MUST. Which governs? | ADR-201 is the later ruling, so the MUST. The bar's text should say so. |

## 9. Gaps in the records

- **No module records a date** for its admission, V1 or release: not Bulwark, Scape, Shriek or
  Sluice. Every date above is this audit's arithmetic on git history, labelled as such.
- **No module has a frozen scope list.** R1 freezes at horde's S5 lab sync, which has not happened
  for any module.
- **horde's stage S5 has no date** (B450, 2026-10-09), and B331's stage order still awaits the
  human's approval (`docs/H2-PLAN.md` §Order of work).
- **Stale records:**
  - horde: Shriek's 9.7 % projection (bar appendix; contract §11).
  - Shriek: main's ROADMAP header.
  - Sluice: ROADMAP Status, not refreshed since 2026-10-09.
  - Bulwark: T-036 reads "awaiting horde's answer" on main after seq 19 answered it.
- **Mailbox:** horde's `brief-engine-rebuild-audit` to Bulwark is on a mailbox branch in Bulwark's
  repo, not its main. Bulwark's D-081 says no such brief is there.
- **R12 (B408):** only Bulwark has measured it. Shriek, Sluice and Scape have no record.
- **R8 at 192 kHz:** only Bulwark tests it in C++.
- **R17 and R18 at module level:** only Bulwark has them. The others have per-feature critics and
  ear rulings, which are not the bar's shape.
- **Re-measurement:** this audit ran no sibling's `./verify`. Every sibling figure is as that
  sibling recorded it.
