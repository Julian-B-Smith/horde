# B657: the wavetable member's test plan (ADR-387)

**What changed.** New `docs/wavetable/TEST-PLAN.md` (PROPOSED). It collects SPEC-WAVETABLE's WT-F and WT-P gates
into one per-stage table (method, must-fail plant, must-read-zero control, threshold status, the stage that
introduces each); lists the WT rows with no test and proposes 23 gates for them (TP-1 to TP-23, among them WT-5's
"a glide never steps", which has no meter today, and a `./verify fast` guard against tracked competitor files); sets
out the listening loop; specifies a repeatable fidelity protocol against Serum (ten tables, four of them horde's own
exported the other way; eight exact test pieces P-1 to P-8; the render vehicle per stage; a test-only importer; ten
measures, each marked judged or reported, and why horde's gates never hang on Serum's numbers); thirteen end-to-end
plug-in tests for the 1.0 integration; nine regression controls; the cadence with ADR-380 A1's caps; the handling
rules for competitor material; fifteen open questions with recommendations; and a design note for recording in
`tools/au-bench` (Appendix A). No code.

**Why.** ADR-387 (the human, 2026-11-08): the wavetable member replaces blades for 1.0 and the plug-in has three
oscillators; "then maybe think up some tests (export wavetables from Serum and record the different compositions on
both to test fidelity, etc.)". ROADMAP B657.

**Evidence consulted.** `specs/SPEC-WAVETABLE.md` (WT-1 to WT-70, WT-F1 to WT-F14, WT-P1 to WT-P7);
`docs/wavetable/CONTRACT.md` (§1.5 memory, §5.4 CPU shares, §5.5 and §5.6 stages and clips, §6 gate rules, §7 open
items); SPEC-ENGINE QS-1 to QS-11, PF-1 to PF-7, DT-1 to DT-6, OU-2; DECISIONS ADR-380 and A1, ADR-384, ADR-385 and
A1, ADR-386; ADR-387 as recorded on the `lead-records-421` branch (not yet on main at this commit); ROADMAP's
competitor-reference convention (ratified 2026-08-01) and B655 to B657; `tools/au-bench/README.md` and
`au_bench.cpp` (`run_once` discards the audio); `tools/auhost/auhost.cpp`'s header (the script format, WAV out,
priming); `docs/design/serum-gauntlet-protocol.md`; `tools/measure_alias.cpp` (the Kaiser β 19 method for
MIDI-pitched renders); the B648 traces (rebuild rates, instruction counts) and the B652 trace (offline render and
the per-cycle pitch estimator against SCALPEL); the lab's constants (1,024 harmonics, 16 frames, 6-point read);
`.gitignore` (`local/` ignored; no tracked audio or preset file today); LIBRARY L0016, L0017, L0032, L0063, L0065,
L0068.

**Decisions taken in drafting, for review.** Serum material lives outside the repos by recommendation (the
gauntlet's rule 3), with the root checkout's `local/` as the fallback the brief named (Q-1). Recording extends
au-bench with auhost's script format rather than giving auhost a preset loader (Q-3). "At least as clean as Serum"
is reported, not gated (Q-4). The A/B listening page stays local, never published.

**Open for the human.** TEST-PLAN.md section 9, Q-1 to Q-15. B656's integration inventory is still in progress;
section 5's tests are written to be narrowed to what it rules.

**Verify.** Not run, per the brief (documents only). The plan file was checked against the private-name patterns
case-sensitively, as `private_name_check` does: no hit; no machine path.
