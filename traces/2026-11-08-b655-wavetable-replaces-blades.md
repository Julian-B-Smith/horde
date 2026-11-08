# B655: the wavetable member replaces blades for 1.0; three oscillators in 1.0 (ADR-387)

**What changed.** DECISIONS gains ADR-387. The charter's §Domain line "every oscillator is a swarm cut by blades" now reads "a swarm whose members read wavetables" with the ruling cited. ROADMAP gains B655 (the ruling), B656 (the integration inventory) and B657 (the test plan). The design ledger marks ADR-387 `not_design`, with its page changes routed through the GUI session.

**Why.** The human, 2026-11-08, by poll, after playing wavetable labs v2 and v3: "Replace, for 1.0" and "In 1.0".

**Evidence consulted.** B645's demo bank and its engine insights; B648's cost bench (instruction counts, uncalibrated; the timed run is still pending); B652's estimate of SCALPEL's presets on wavetables; CONTRACT §7 A1 and H9; ADR-184, ADR-384, ADR-385 and A1, GUI-1.

**Not decided here.** How an old patch's blade settings load (mapped or flagged), osc 3's id block (CONTRACT §10 names 0x8000 as apparently free), and the WT-BUDGET confirmation (the timed run). B656 proposes these.

**Verify.** `./verify fast` on this tree; see the PR.
