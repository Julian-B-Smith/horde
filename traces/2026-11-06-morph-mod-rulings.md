# 2026-11-06 — GUI-51 (the morph's eight answers, Main's envelope fit, page-chrome closed) and GUI-52 (ADR-371 on the Mod page)

**What changed.**
- **GUI-51** records the human's poll on the morph questions lab: pins 8 per setting and 64 per patch, dormant across a mode change; a region lock keeps the shape only; X and Y are host parameters; mappings are per corner (not the recommended option); RAMPS dropped; corner presets in the patch and in a user library; Save current sound stores what you hear.
- It also records the Labs facilitator's batch 5: GUI-47's fit is (c), and page-chrome is closed with GUI-28's readings kept.
- **GUI-52** records what ADR-371 changes on the Mod page.
- **ENGINE-NEEDS:** GN-6, 7, 9, 23, 25, 27, 35, 37, 39, 49 and 51 cite the rulings.
- **ROADMAP:** GB23's fit and GB29.
- **Ledger:** rows for both rulings, and page-chrome harvested with its pin off. The index is regenerated.

**Why.** The human's poll answers in this session; the facilitator's batch 5; the lead's ADR-371 message ("Mod-page consequences are yours to record as GUI-n").

**Not done here.**
- The builds: Main's envelope strip, the route curves and corner depths on the Mod page, RAMPS out of the master.
- The Map Mode spec's "global for 1.0" (a protected path) needs the human's edit.
- GN-52 is cited only in the text, since its row lands with #1421.

**Evidence.** `engine_needs_check` ok; `design_chain_check` ok.

**Verify.** `./verify fast` on the commit this trace ships in.
