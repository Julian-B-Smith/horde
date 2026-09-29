# b366-env1-onset-scatter: the master envelope framed as ENV 1's prototype; onset scatter under ENV 1

- **Queue item:** B366, an addendum. This corrects the record of
  `traces/2026-09-29-b366-scalpel-master-env.md`, which counted 13 decisions; the inventory now has
  14.
- **Why:** mid-task direction from the lead, 2026-09-29. The human, verbatim: "It might be worth
  workshopping the full envelope hierarchy in a lab (this would be a complement to the mod lab). I
  think want to follow the Serum approach of having ENV 1 be a global envelope that applies to all
  pre-FX sound-generating devices (including filters). Then we need to determine the envelopes work
  for things like onset scatter (an important knob I would like to preserve; but I'm frustrated by the
  trade-off of eating up an envelope slot with it versus giving it a secret proprietary envelope
  which may clash with certain settings)."
- **The lead asked for three things:**
  1. frame the master envelope as ENV 1's prototype;
  2. evaluate, as analysis only, the candidate: onset scatter as a spread dimension of how ENV 1 is
     read per swarm member, with offsets δᵢ, optional time scales sᵢ, and `onsetAlpha` governing the
     draws;
  3. say where the candidate could still clash.

  The envelope-hierarchy lab is B370.

## What changed (commit 3a6d0e8)

- **`docs/design/scalpel-envelope-conflicts.md`:**
  - the ENV 1 framing, with the human's words;
  - a new section, "Onset scatter under ENV 1": the candidate, what B335's voiceEnv already does, the
    clashes and a verdict;
  - decision 14.
- **`docs/design/scalpel-interface-lab.html`:**
  - the Envelope cell's subtitle is now "ENV 1 (prototype) · amp ADSR · the same four on OSC";
  - E4's header comment and the notes panel frame it as ENV 1;
  - the notes item counts fourteen decisions.
  - No behaviour changed. The self-check is 119/119, with the macros row unchanged at 79.9 px.

## The analysis

The numbers were reasoned from the engine's own law on F12's measured draw. They are not gated.

- **The draw:** members enter at 0, 25, 41.5, 42, 43.5, 46.7 and 83.1 ms. The envelope is the pluck
  (A 1 ms, D 0.26 s, S 0).
- **The unscattered read, ENV1(t):**
  - it is also the earliest member's read;
  - it stands at 0.283 (−11.0 dB) when the last member enters;
  - so a filter following it has closed that far before that member's pluck.
- **The ensemble mean, (1/N)·Σ ENV1(t − δᵢ):**
  - it peaks at 0.716 at 47.7 ms;
  - it reads 0.099 against 0.691 at 25 ms, and 0.432 against 0.218 at 100 ms.
- **Recommendation:** the normalised ensemble read as ENV 1's per-voice source.
- **Today's two modes disagree.**
  - With onset scatter and no voiceEnv (the entry ramp), the 83.1 ms member enters the shared,
    decayed envelope at −11 dB.
  - With voiceEnv, it reads its own full attack.
  - The candidate picks voiceEnv's answer.
- **The other clashes:**
  - **Release lifetime:** 1.33× the nominal tail on F13's draw, plus max δᵢ if the key-off is spread.
  - **Mono retrigger:** a pure time read drops late members to silence. The engine's state-machine
    re-entry (`scalpel-horde-engine.js:568`) avoids it.
  - **Steals:** waiting members drop to silence (B335's open item). Decision 6's fade-and-fresh rule
    removes it.
  - **The cull:** it acts after the member reads, so there is no clash.

## Evidence consulted

- `docs/design/scalpel-horde-engine.js:568` (retrigger), `:643-733` (armMembers, memberStep).
- B335's ROADMAP row ("a stolen voice's waiting member drops to silence").
- F12 and F13 of the inventory.

## Alternatives rejected

- **Building the spread now.** Rejected: the lead scoped this to analysis, and B370 is its lab.

## Verify

- `./verify fast` and `./verify full` were re-run on this trace's commit, with nothing else running.
- They are reported verbatim in the handback and the PR, because a trace cannot carry its own
  commit's result.

## Open questions

- Should ENV 1 as a per-voice source be the normalised ensemble read? That is decision 14 in the
  inventory.
- Should today's entry-ramp mode be retired?
- Should sᵢ scale the decay too?
