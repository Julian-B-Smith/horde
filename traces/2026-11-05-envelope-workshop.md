# 2026-11-05 — The envelope workshop lab (GB23)

**What changed.** `docs/design/envelope-workshop-lab.html`: how a source gets a different envelope, and where envelopes sit on Main and an Osc page, with four candidates each and this session's pick marked. No shell or master change.

**Why.** The human: "we still need to figure out an elegant way to add secondary envelopes or overrides, maybe looking at how Serum 2 does it. And we should also workshop how the envelopes show up on the main and osc pages".

**Evidence consulted.** RC-6.5, ADR-255 r5, the hierarchy lab's section 1, `scalpel-envelope-conflicts.md` decision 15. On Serum 2: Xfer's "What's New" document (four envelopes) and an Xfer staff answer on its forum (the MIX page's Env button). Its user guide itself wasn't read in this session.

**Verify.** `./verify fast` on the commit this trace ships in. The lab's four checks pass, each with a control, and a real mouse used Override and dragged a node.

**Not verified.** Anything by the human's eye. Nothing here is heard.
