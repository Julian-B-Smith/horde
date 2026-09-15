# 2026-09-14 — NETWORK intake (ADR-166 proposed, B127)

Two files at the root (`network-lab-v0.html`, `SPEC-fx-network.md`) moved to
`reference/network-lab-v0.html` and `specs/SPEC-FX-NETWORK.md`; protected
paths and the candidate sentence in CLAUDE.md §Domain.

The spec is a pre-spin-up spec for its own project (three surfaces, five
milestones, its own verify). The consequential decision — horde's FX rebuild
vs a sibling project horde consumes vs both in order — is laid out on B127
with the lead's recommendation (C) and the divergences to reconcile
(one-instance-per-type vs free composition being the substantive one).

Four unseeded draws named as sanctioned edits. One intake edit made to the
spec itself under the ADR-014 alias rule: §9.9 named three private siblings
by their real names; the leak gate caught it and the line now carries the
aliases (the map lives in the untracked PRIVATE-NOTES.md). `./verify fast` exit 0.
