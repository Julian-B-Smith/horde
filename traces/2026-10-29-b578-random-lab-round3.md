# b578-random-lab-round3 — selected-corner default, rule collections, the morph law on the pad, snapshot fixes, drafts

- **Queue item:** B578, round 3, from ADR-320 (horde PR #1264, read on `origin/lead-records-365`). Branch `lab-b578-random-3` from origin/main at e87ca9b, after #1262 merged.
- **Why:** these are the human's rulings after playing round 2:
  - "'arch writes' should default to 'selected corner'";
  - general source collections (any envelope, any modulator, the intent bus) and structured target collections;
  - "I'm still not hearing stochastic (quantum) parameter or architecture boundaries on the random morph xy";
  - "Clicking the x next to a saved snapshot doesn't delete it" and "a button that saves all four snapshots together";
  - for the report only: arps, exemptions and a deeper pitch ruleset.
- **What changed:**
  - **`docs/design/random-lab.html`:**
    1. `archScope` defaults to `'one'`.
    2. `SRC_COLL` / `DST_COLL` are defined once, with `expandSrc` / `expandDst`. Every rule names one token per side, and the draw, C6, the rules panel and the custom-rule builder all read the collections.
       - Four intent sources are appended to MSRC; appending is safe because slots store indices. Four intent sliders were added, plus four intent rules and an "Intent-driven" ruleset.
       - The velocity rule is split in two.
       - C6 finds its rulesets by name, not position.
    3. `heardOf` now takes a morph law:
       - QUANTUM: per-key Gumbel-max, with draws counter-based and keyed by parameter id;
       - BLEND: numbers blend, and structure flips at its own seeded point, labelled pending B585;
       - STEPPED: an n × n snap, then blend.
       The pad got the law pills, temperature, cohesion, steps, a region map for the selected key, and a census. C8 was updated, and C15 added.
    4. `delSnap` deletes bound and home snapshots (the corner keeps its sound, as a draft); the last snapshot's ✕ is disabled with its reason. New actions `saveAll`, `loadSet` and `delSet`; the fingerprint includes the sets. C16 and C17 added.
  - **`docs/design/random-lab-report.md`:** new §4b (items 1–4, and drafts 5a–5c with recommendations); the basic-1.0 set updated (items 6, 10–13); Q3 marked ruled; evidence C15–C17.
- **Evidence consulted:**
  - ADR-104 (the quantum law: Gumbel-max, temperature, cohesion);
  - ADR-185 (a frozen per-key draw);
  - ADR-317 and B585 (architecture follows the law; Blend's crossfade is B585's);
  - B424 (the stepped hybrid);
  - ADR-318 (profiles, the role set);
  - ADR-319 item 4 (single source);
  - ADR-320;
  - `docs/design/morph-editor-lab.html` (`pickCorner`, `logw`, STEPPED reading (a), the default T = 1 and cohesion 0.3, mod units flipping as one);
  - `specs/SPEC-INTENT-BUS.md` §3.1;
  - `docs/design/arp-face/README.md`.
  - `docs/design/morph-blend-demo.html` is NOT on main yet, so the report only points at it.
- **Alternatives rejected:**
  - **Rules holding token arrays:** that would be a per-rule list, which ADR-319 item 4 forbids.
  - **A hysteresis band on the pad:** it hides the boundaries asked for, and the lab has no puck history.
  - **Rebuilding Blend's architecture crossfade:** that is B585's.
  - **Refusing the ✕ for a bound snapshot with a louder message:** the human asked that it delete.
- **Checks:** all 17 self-checks pass headless.
  - **C15:** 120 value changes at 84 distinct positions of 400, structural keys at 35. The control (cohesion 1) gives 74 changes at 1 position, and fails as it must.
  - **C16:** deleted, sound kept, now a draft, home deletable, last stays. The control (round 2's rule) gives "deleted false", and fails as it must.
  - **C17:** a set of 4, all bound, loads back exactly, locked corner kept. The control (saving one corner) is false.
  - **C6:** 330 routes, 0 bad.
  - **Headless Chrome:**
    - the scope default is "selected corner";
    - the ✕ on a just-saved snapshot deleted it ("corner A kept its sound, now a draft");
    - the last ✕ is disabled;
    - the census at the pad's centre differs between BLEND (48 blended) and QUANTUM (0 blended; A 56 · B 18 · C 13 · D 13);
    - "save all four" made the set "Evening" and "load all four" loaded it;
    - the collections are listed;
    - a custom rule "any modulator → any filter cutoff" was stored as tokens;
    - replay ✓ (14 actions).
- **Verify:** `./verify fast`, exit 0 (the record is in the report back to the lead; this trace is written before the run, so the run includes it).
- **Open questions:**
  - 1.0 scope for 5a–5c (the report recommends: arps post-1.0; exemptions in 1.0; a minimal pitch draw in 1.0).
  - Whether the pad's census and region map belong on the final page.
  - Nobody has listened to the quantum boundaries yet.
