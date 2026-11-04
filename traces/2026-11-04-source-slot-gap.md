# B615: the source slot RC-6 promises is not in the code

**What changed.** ROADMAP gains B615. No code, spec or test changes.

**Why.** The human asked whether the clean-room build makes it straightforward to add engines after 1.0. Reading the builder's code to answer found that RC-6.1's slot was never built and that no test can see it.

**Evidence consulted.** `specs/SPEC-ENGINE.md` RC-6, RC-6.1 to RC-6.7 and item 18's line; in the builder's tree at `6799bbb`: `engine2/core/sources/slot.h` (two lines, included by nothing), `voice/voice.h`, `voice/voice.cpp` (lines 38 and 659 to 672), `api/engine.cpp` (lines 132 to 137), `manifest/manifest.cpp` (line 333); `tests/items/item18_source_slot.cpp`, header; a search of ROADMAP, DECISIONS, the rulings log and the builder's QUESTIONS and ANSWERS for an earlier ruling, which found none.

**Verify.** `./verify fast` on this tree; see the PR.
