# B662: sync always on, with a softness taper (ADR-389); ROADMAP rows for B659–B661

**What changed.** DECISIONS gains ADR-389. ROADMAP gains B659, B660 and B661 (the Weir-filter lab, the test plug-in filter and the sync lab, all merged, whose rows were missing) and B662 (the sync ruling, carried into the spec rework with GN-57/58/59). The design ledger marks ADR-389 `not_design`.

**Why.** The human, 2026-11-09, by poll: "Always on, rule it now" and "Taper, fade-in 0.25", after lab B661.

**Evidence consulted.** `traces/2026-11-08-b661-lab-soft-sync.md` (the neutrality self-checks and their controls, the cost ratios, the morph sweep, the short-taper aliasing table); SPEC-WAVETABLE WT-24, WT-32, WT-33, WT-36 to WT-39; the GUI session's GUI-63, GUI-65, GUI-66, GUI-67 messages (GN-57, GN-58, GN-59).

**Verify.** `./verify fast` on this tree; see the PR.
