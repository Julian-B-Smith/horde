# B615: the source slot RC-6 promises is not in the code

**What changed.** ROADMAP gains B615. No code, spec or test changes.

**Why.** The human asked whether the clean-room build makes it straightforward to add engines after 1.0. Reading the builder's code to answer found that RC-6.1's slot was never built and that no test can see it.

**Evidence consulted.** `specs/SPEC-ENGINE.md` RC-6, RC-6.1 to RC-6.7 and item 18's line; in the builder's tree at `6799bbb`: `engine2/core/sources/slot.h` (two lines, included by nothing), `voice/voice.h`, `voice/voice.cpp` (lines 38 and 659 to 672), `api/engine.cpp` (lines 132 to 137), `manifest/manifest.cpp` (line 333); `tests/items/item18_source_slot.cpp`, header; a search of ROADMAP, DECISIONS, the rulings log and the builder's QUESTIONS and ANSWERS for an earlier ruling, which found none.

**Verify.** `./verify fast` on this tree; see the PR.

## B616: the GUI rulings' engine needs

**What changed.** ROADMAP gains B616; the root charter's opening states GUI-1.

**Why.** The GUI session handed over its list and six conflicts (GB8). The morph comes right after the CPU pass, so the morph needs have to be placed first.

**Evidence consulted.** `docs/design/gui3/ENGINE-NEEDS.md` on `gui-morph-page`; RC-11 and its Must-not-require line; `engine2/core/api/history.h`; RS-4.7 and RS-4.8 in `docs/proposals/rack-slot-contract.md`; ChainConsole's README on `gui-morph-page`; ADR-176 item 4, ADR-192, GUI-10, GUI-18, GUI-20. The list's other rows were not rechecked.
