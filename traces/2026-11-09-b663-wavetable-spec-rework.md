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

## Rework on the critic's review (REWORK, scoped), 2026-11-09 01:10

Text edits only; no computation (the 2–6 am quiet window). What needs a number is an S0 item with its method
(CONTRACT §5.6, "S0's pending engine-free items").

- **B1:** WT-24 and §7 J2 rewritten on the right history: round 42's filter E fixed B-51 for DT-3 (ADR-289), and a
  synced table read is a curved smooth part with steps. Recommended, pending S0 item 1: delay the table read itself
  by τ on every path at every ratio (not a filter; nothing switches). WT-F42 applies DT-3 to synced members with a
  mutation plant; WT-25 gains the residuals' mean term.
- **M1:** Δ = g·(…), g = 1 at softness 0; WT-F41 runs at softness 0. **m8:** Δ read at the actual read phases.
- **M2, M3, M6:** the bake is a focused port with the frozen engine as an oracle in the build repo (BB-12, BB-60,
  BB-F12, J8); the oracle's tap via `HordeEngine::prepare(192000, …)`; every pitch- or rate-dependent law listed and
  evaluated at C4 and 44.1 kHz (BB-13).
- **M4:** frame routes at depth 1 from base 0, frames along the swing's sign; BB-F10. **m14:** BB-20 (d).
- **M5:** J9 for the human; BB-F11 reported at U = 9; the spread-frame mapping. **M9:** `s0-1-unison-at-rest`.
- **M7:** 380.2 per second at 44.1 kHz in WT-F19 and TEST-PLAN TP-5.
- **M8:** S6b restored to the C++ generators; the bake is S6c; BB-F3 (b), F4, F9, F11 run at S8.
- **m1, m7, m15:** WT-38, WT-F39 (`bandLimit` 0), WT-F38 (a glide back to 1 lands exactly).
- **m2, m11, m12, m13, m16:** J10, K table sets, osc 3's seed 1236 (J11), the migration A/B exemption (J12), C2a.
- **m3, m4, m5, m6:** BB-30 reports live rows and the base wave; BB-F3's detector is harmonic 2; BB-41 keys by id
  (worst case about 2.6 KiB, S0 item 4); noise draws from the taken cycle.
- **m9, m10:** GN-57 blends only the 63 harmonics 128 points show; GN-59 a gated accumulator with a 16,384-sample
  ring.
- **m17:** CONTRACT §7 is one table of 72 rows, duplicates merged, the PROPOSED numbers listed by gate.
