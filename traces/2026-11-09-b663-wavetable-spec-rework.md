# 2026-11-09 — B663: the wavetable spec rework (ADR-387, ADR-388, ADR-389, GN-57 to GN-59, the test plan)

> Origin: horde lead session, 2026-11-09, B663. The human: "go ahead and start the spec rework now". Drafted by an
> Opus agent on the lead's brief.

**What changed.** `specs/SPEC-WAVETABLE.md` and `docs/wavetable/CONTRACT.md`, amended in place; a new
`docs/wavetable/BLADE-BAKE.md`; this trace. No ROADMAP, DECISIONS, SPEC-ENGINE, tool, verify or engine edits.

**Why.** ADR-387 (the wavetable member replaces blades; three oscillators in 1.0), ADR-388 (blade baking, the budget
after the timed run, the `retired` class, the seam at three oscillators, osc 3's ids, the base/table switch
test-only, five sources), ADR-389 (sync always on, softness) and the GUI session's GN-57 to GN-59 left the spec and
the contract behind; the test plan (B657, Q-9) asked for TP-1 to TP-23 to be folded into the spec before S0 closes.

**How.**
- SPEC: the status and ruled lists; section 1's "beside" reduced to one paragraph of history; WT-19 a test switch
  gone in 1.0; WT-24 (no filter E on a table member; a plain residual only on restarts with a step, continuous across
  integer ratios), WT-32 (sync always on), WT-33 (the softness taper replaces the window), WT-37 (short tapers),
  WT-38, WT-39; WT-48 points to the blade bake; WT-60 and WT-69 for three oscillators in 1.0; WT-22 marked OPEN (I6);
  WT-F15 to WT-F37 from TP-1 to TP-23, WT-F38 to WT-F41 for sync; WT-P1 PENDING the night's timed run with 4 %
  visible as provisional; PF-1 re-anchored and PF-2 as ADR-388 #2 rules.
- CONTRACT: §1.2 to §1.3 for replace; §1.7 history; §1.9 the GUI observations (GN-57 to GN-59, computed only while a
  page asks); §2 rewritten (`wt.syncIn` × pitch | Hz, `wt.sync` default 1, 0x036 withdrawn and left empty,
  `wt.syncSoft` at 0x042, `wt.modMix` if adopted at 0x043, osc 3 at 0x8000, `mix.osc3` at 0x0013, the counts, the
  `retired` class); §3's bounds for bake recipes; §4 five sources and MO-27; §5.4 the budget pending; §5.6 S6b (the
  bake) and S7/S8 at three oscillators, with S6b's clips; §7 rewritten as the one list (answered items marked with
  their ADRs, INTEGRATION's open decisions as group I under their numbers, new items as group J); §8 points to
  INTEGRATION §2.8 and adds what this contract fixes; §10 for the ruled third oscillator.
- BLADE-BAKE.md: a file of its own (it is a generator with its own law, inputs, migration and gates): inputs, the law
  (SPEC-ENGINE C.9 to C.11 through a frozen copy of the clean-room engine, one member, 8,192 samples per cycle at
  192 kHz, Hz settings at C4), the frame axis, the mappings and load report by B652's families, the recipe (≤ 3 KiB),
  where it runs (load, main thread), versions, goldens, gates BB-F1 to BB-F9.

**Evidence consulted.** DECISIONS ADR-386 to ADR-389; `docs/wavetable/INTEGRATION.md` §1a, §2 to §6, §12;
`docs/wavetable/TEST-PLAN.md` §2.1, §2.2, §9; traces b652, b661, b662; ENGINE-NEEDS GN-57 to GN-59 and GUI-63;
SPEC-ENGINE 5.1, B.1, ED-1, ED-3, ED-7, IF-10, QS-1; `reference/scalpel/prototype/razor-core.js` (read only, for its
function list). **Ids:** the clean-room manifest at `e72a092` (330 rows, dumped by compiling `manifest.cpp` alone,
2026-11-07; the engine has not moved since); `git grep` of horde's `specs`, `docs`, `engine2`, `tools` for the new
ranges (only the wavetable docs, hash constants and an FTZ mask); `tools/param_id_lock.json` (its `h2` section is a
stub with no ids); the test plug-in's host ids (`preview_plugin.cpp` line 81, `filter_core.h` line 20,
`preview_gui.h` line 13).

**Not done.** No measurement: every new number is PROPOSED or cited. No heavy command was run (the 2–6 am window was
kept quiet). `./verify` not run (the lead runs it after 06:00).

**Verify.** Left to the lead.
