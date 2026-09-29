# b366-serum-env1-model: the inventory weighs a Serum-style ENV 1 (per-source opt-out, HOLD, per-envelope retrigger)

- **Queue item:** B366, a second addendum. It corrects the decision count in
  `traces/2026-09-29-b366-env1-onset-scatter.md` from 14 to 17.
- **Why:** mid-task direction from the lead, 2026-09-29. The lead read the Serum 2 User Guide and
  recorded it on ROADMAP B370 ("don't copy manual text into the repo"). It asked that three things be
  reflected in the inventory, as analysis only:
  1. a per-source ENV 1 opt-out, including what "the voice lasts until the longest release of any
     envelope" means under the swarm, B310's voice law and B323's cull;
  2. whether the master envelope should include HOLD;
  3. a per-envelope legato invert, for the mono and legato question.
- **What changed:**
  - `docs/design/scalpel-envelope-conflicts.md` gains the section "What a Serum-style ENV 1 adds (the
    lead's reading, B370)", in our own words from the lead's summary, with no manual text, and
    decisions 15-17.
  - The lab's notes item now counts seventeen decisions. No behaviour changed.
- **The findings that need a rule:**
  - **The cull writes into ENV 1** (`v.env *= f`, `docs/design/scalpel-horde-engine.js:1051`, and
    the member envelopes at `:1052`). A source that opts out of ENV 1 would therefore ignore the
    cull. So the cull and every steal fade must be a per-voice gain after the sources.
  - **B310's tiers read ENV 1 alone** (`:522-530`). With an opt-out they must read the voice's
    audible envelope, or tier 1 takes a sounding voice with no fade.
  - **Opting a swarm out of ENV 1 removes its onset scatter** under the spread-read model.
  - **HOLD is a divergence.** Hold 0 is bit-identical, and it belongs with D5.
  - **A per-envelope retrigger policy** ("follow the mode" or "always"), plus one "from the current
    level" rule, settles decision 5.
- **Evidence consulted:** the lead's summary of the Serum 2 User Guide (as relayed; the manual itself
  was not read here); `docs/design/scalpel-horde-engine.js:522-530` and `:1040-1052`;
  `reference/scalpel/prototype/razor-core.js:779`; F9, F10 and F11 of the inventory.
- **Alternatives rejected:** none considered. This is analysis only, and the build scope is
  unchanged.
- **Verify:** `./verify fast` and `./verify full` were re-run on this trace's commit, with nothing
  else running. They are reported verbatim in the handback and the PR.
- **Open questions:** decisions 15-17 in the inventory.
