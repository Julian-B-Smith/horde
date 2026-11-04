# 2026-11-04 — The Sluice and Shriek mark studies become labs (GB11)

**What changed.** New `docs/design/sluice-mark-styles-lab.html` and `docs/design/shriek-wordmark-styles-lab.html`, copied from their Artifacts (versions 1793813064-6fca and 1793204183-5a81) with a provenance comment, the index's meta tags and a status note added. The design index, five ledger rows pointed at the pages, the ledger's labs and audit note, GB11.

New `docs/design/sluice-s-bend-lab.html`: the real mark with four variants of the S's lower bend, each measured.

**Why.** The human, through the lead: "we should probably make the sluice and shriek mark styles artifacts into actual labs so they don't get lost" (ADR-336 ruling 2).

**Evidence consulted.** ADR-302 and ADR-305 in `DECISIONS.md`. Both pages read in full before they left a private Artifact for a public repo; a scan for machine paths, names, emails and network calls, and the embedded PNGs' chunks (IHDR, IDAT and IEND only, so no metadata). `tools/license_inventory.json`, which already covers lab pages loading fonts at run time.

**Clearance.** Sluice's mark is cleared by Sluice's response (D-192, section 4). Shriek's automaton had no such record, so it was asked: the human, 2026-11-04: "Yes, it can go public".

**Verify.** `./verify fast`, on the commit this trace ships in. Both pages opened in a browser.
