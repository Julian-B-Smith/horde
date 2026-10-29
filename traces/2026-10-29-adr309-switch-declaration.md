# 2026-10-29 — ADR-309: the switch declaration; Scape's A11 unpinned

**What changed.**
- `tools/admission/scape_adapter.cpp`: the hand-copied switch table, which transcribed Scape's
  spec prose, is gone. `switchOf()` maps Scape's declared `ParamDecl::switchMethod` (what its
  manifest is written from) to the contract's methods. An undeclared key reads Undeclared.
- `tools/admission_check.py`: A11's pin for Scape's Era (ADR-296) is removed.
- `docs/proposals/rack-slot-contract.md` §12.7, `DECISIONS.md` ADR-309, ROADMAP B561 and B562.
- Scape's response seq 4 landed in `integrations/scape/`.
- Wakeup housekeeping on the same branch: REFLECTIONS pruned, and LIBRARY L0080 graduated.

**Why.** Scape fixed Era's switch (its `11e1698`, D-073) and declared it in its manifest. The
human ruled that declaration the contract's format ("Adopt as the contract format"). The check
was still failing A11 only because its adapter carried Scape's OLD prose ("dip").

**Evidence.** `tools/admission_check.py --module scape` on a clean `git archive` export of Scape
`11e1698`:
- with the old adapter: A11 FAIL PINNED (method read as "dip"); click reading worst 2.55 dB
  against the ideal's 3.07 dB;
- with the new adapter and the pin kept: RED, "STALE PIN — method[verb.era] no longer fails",
  which shows the pin really stops failing;
- with the pin removed: GREEN, all 16 module-only rows pass, and every control fires, A11's
  `flip-crossfade-removed` included.

**Verify.** See the commit: `./verify fast` on this tree before push. The admission check itself
runs in `./verify full`.
