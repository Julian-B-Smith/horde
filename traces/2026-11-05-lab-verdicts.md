# 2026-11-05 — The human's lab verdicts, batches 1 to 3, recorded on the GUI side (ADR-369)

**What changed.**
- **Review pins removed from 16 labs:**
  - arp-face, composition rounds 1 to 3, sub-noise, random-home, morph-blend-demo, sluice-mark-styles, fx-chain-morph rounds 2 and 3;
  - at the lead's request: sluice-horde, edge-correction, engine-audit, envelope-hierarchy, freeze-proposal and legacy-roundup.
  - Only each pin was removed; no lab's content changed.
  - **Not scalpel-interface-lab:** it is a blob-pinned parity source (`h2/README.md`, ADR-187 item 3), and removing its pin failed `golden_pin_check` ("re-pin on purpose, never by editing the pin to match"). The file is untouched, so its index pin stays. Clearing it needs a deliberate re-pin in `h2/`, which is the lead's.
- **The ledger's `labs`:**
  - Retired, each with its reason: edge-correction, engine-audit, envelope-hierarchy, freeze-proposal, morph-blend-demo, sluice-horde, sluice-mark-styles, random-home.
  - composition r2 is now harvested; its one open item moved to B583.
  - Harvested, confirmed: arp-face, composition r1 and r3. sub-noise is fully harvested, with "Weather is settled".
  - scalpel-interface is harvested and kept as a source, not archived.
  - fx-chain-morph rounds 1 to 3 are closed, and the per-corner shuffle stays open research on B265.
- **GUI-36 A1:** "Yes, always animate". The FX page's cable views move with the morph whether or not sound plays, which GUI-41's patch view already does.
- **GB9's status line:** the Noise picture is settled.
- **B583** gains composition r2's right-click polarity item.
- The design index is regenerated: 12 labs remain pinned: the open ones and the pinned scalpel-interface source.

**Why.** The human's poll answers, relayed by the Labs facilitator session from the lead's brief: "Yes, close all", "Weather is settled", "Close, menu work carries it", "Retire it", "Close all three rounds", "Yes, always animate". ADR-369 (the lead's records PR) records them, and the lead asked this session to change the lab files and the ledger's `labs`, so one owner touches them.

**Evidence.** `design_chain_check`: 75 labs, with 14 exploring, 19 harvested, 28 retired, 11 ruled and 3 not a lab. `gen_lab_index`: 12 pinned. The remaining pins are the envelope workshop, FX screen marks, oscillator layout, page chrome, random, Sluice and Shriek full GUIs, bank morph lane and morph rail, plus GUI 3 itself and the lead's plan map. Read in the patch view's code: it steps every frame from the morph's weights, with no sound gate.

**Verify.** `./verify fast` on the commit this trace ships in.
