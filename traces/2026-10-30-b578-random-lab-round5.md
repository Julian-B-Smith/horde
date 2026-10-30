# b578-random-lab-round5 — the envelopes as their own level

- **Queue item:** B578, round 5 (the lead's brief, 2026-10-30). Branch `lab-b578-random-5` from origin/main at a03701a.
- **Why:** the human: "'osc envelopes' should be its own level separate from the rest of the oscillator details, and each individual oscillator's envelope could be randomized separately." By poll: "Params edit shared; Arch can split", and for the architecture's placement, "I'd like to test both options".
- **What changed:**
  - **`docs/design/random-lab.html`:**
    1. **The stand-in follows SPEC-ENGINE's envelope model.**
       - ENV 1–4 are AHDSR with C.12's curve law (`envCoef`, `envTick`, `envKeyUp`); ENV 2–4 have one-shot.
       - Each source has an `env` selector, and its envelope is applied before the filter (VO-9).
       - A voice is freed when its governing envelopes end; ENV 4 is a mod source.
       - A mod rule never shapes an envelope that governs an on source.
       - Curve families group C.12's three curves into one choice; this is labelled a lab simplification.
       - The tempo comes from the patch (`S.bpm`).
    2. **An ENVELOPES level with a row per source**, each showing who shares its envelope.
       - PARAMS on a row edits the shared envelope and flags it.
       - ARCH on a row may split the source onto a free envelope, as a copy, and announces the split.
    3. **A placement toggle**, `S.envPlacement` (a)/(b), used by `levelKeys`, `selInLevel`, `archOff` and the locks.
    4. **An envelope profile inside the ruleset (ADR-318):**
       - six archetypes;
       - four relationship rules (`RELS`);
       - `relMax`, a release bound per role;
       - a sustain floor;
       - tempo-relative times on RC-7.4's 22 divisions, labelled PROPOSED from horde #1275.
    5. **Five role presets** (Mono bass, Pad, Pluck, Lead, Arp), each carrying these envelope settings.
    6. **Envelope copy, an overlay drawn by the same law, and level matching** (a monitor-side make-up gain).
    7. **Checks C19–C28.**
  - **`docs/design/random-lab-report.md`:**
    - new §4d covers the model, the level, the two placements compared, the four ideas, safety, and what goes in 1.0;
    - a record correction: level matching had not been built before;
    - a note that §4's numbers predate the C.12 envelopes.
- **Evidence consulted:**
  - `specs/SPEC-ENGINE.md` EN-1..5, C.12, RC-6.5, VO-9, 6.4, and the ENV rows of 5.2;
  - RC-7.4 in `docs/proposals/morph-appendix-spec-text.md` (origin/b585-appendix);
  - ADR-318 (profiles, the role set).
- **Checks:** all 28 self-checks pass headless.
  - **C19:** the shared envelope changed and was flagged; the control (a silent split) fails.
  - **C20:** OSC 1 split from ENV 1 to ENV 3 as a copy, and OSC 2 was kept; the control (no copy) fails.
  - **C21:** each of the four relationship rules held 60/60 with the rule on. The controls (rules off) held 0, 27, 14 and 0 of 60.
  - **C22:** 237/237 times on the grid at 120 bpm and 225/225 at 90 bpm; triplets appear only when toggled; the control (tempo off) put 0/237 on the grid.
  - **C23:** 5 keys moved, all in ENV 3; the control (copying every envelope) moves 21.
  - **C24 and C25:** each placement passes its own test, and fails the other's.
  - **C26:** the floor held 60/60 (control, floor 0: 52/60); the Mono bass release bound held 60/60 (control, Anything: 18/60).
  - **C27:** two patches 8.4 dB apart came out 0.00 dB apart. This proves the arithmetic only: RMS is not perceived loudness.
  - **C28:** pluck 40/40; the control (no archetypes) 0/40.
  - **Placement measure** (60 seeds, default knobs):
    - OSC 1 ARCH moved OSC 1's envelope 0/60 times under (a) and 10/60 under (b);
    - a selector move without a copy jumps the stage times by a median 2.0×;
    - after a VOICE ARCH click, 55/60 patches share an envelope.
  - **Headless Chrome:** clicked the envelope rows, a split, the placement (b) refusal, an envelope copy, the Pluck role, a stagger, tempo at 96 bpm, randomize patch and an archetype weight. No error, and replay ✓.
- **Alternatives rejected:**
  - **Splitting on PARAMS draws:** the human ruled "Params edit shared". Relationship rules split only on ARCH or randomize-patch draws, and say so otherwise.
  - **Three continuous curves per envelope:** replaced by one family, to keep the structural draw small. Labelled.
  - **Snapping sub-1/32 times to a note value:** a 1 ms attack has no note value, so those times stay in seconds.
- **Verify:** `./verify fast`, exit 0, run after the last edit. The record is in the report back to the lead; this trace is written before the run, so the run includes it.
- **Open questions:**
  - The placement: I recommend (a), with an optional "ARCH incl. envelope" shortcut on the oscillator rows.
  - 1.0 scope for relationships and tempo-relative times.
  - The level-match target needs ears.
