# b585-morph-appendix-spec-text — Appendix D for SPEC-ENGINE, PROPOSED (B585 step 4's input)

- **Queue item:** B585 step 4's input (ADR-324). The lead's brief asked for:
  1. the appendix itself, in numbered rows, with the "at most two" slot rule for the diagonal case;
  2. every amended row, as before → after, with B557's landing place;
  3. engine-free test items with must-fail controls, revised for H1's layered count;
  4. a diff summary;
  5. open points, each with a recommendation, limited to what ADR-324 does not settle.
- **Why:** the human ruled the twelve questions and chose one normative morph appendix in SPEC-ENGINE (ADR-324). The spec text is drafted before a critic reviews it and the human ratifies it.
- **What changed:** `docs/proposals/morph-appendix-spec-text.md` (new). It holds:
  - **Appendix D:**
    - D.0–D.7;
    - MO-1 to MO-17 (the resolver: weights, morph glide, the eight morph settings, the quantum draw keyed by key, the pick, the latch, Blend, Stepped, the per-parameter menu with its Architectures choice, structural groups, flip-only keys, setting sets at counts 4 and 2, continuity, FX slots);
    - SB-1 to SB-10 (the engine's rendering, with the count-2 slot rule).
  - **20 amended rows, as before → after,** quoted from `specs/SPEC-ENGINE.md` at `2af41d8`.
  - **23 test items:** R1–R10 for the resolver and E1–E13 for the engine.
  - **A diff summary.**
  - **Eight open points,** X28–X35.
  - **No edit to `specs/`.**
- **A correction to the proposal (#1265), found while drafting.** The two-setting law is not "a switch on any line where they tie". A grid scan of the demo's own `groupTargets` (801 × 801 points) finds one discontinuity:
  - **Four distinct settings:** it is at the centre (0.5, 0.5). Outside r = 0.1 the largest neighbour change is 0.0152 per 0.00125 step.
  - **Three settings, C = D:** it is at (0.500, 0.333).
  - **Three settings, B = C:** none (0.005). With two settings: none (0.001).

  The demo's −15.8 dB on the diagonal was its path passing through the centre point. The topological argument for one unavoidable point is entailed, not measured. The text says so.
- **Evidence consulted:**
  - **Decisions and roadmap, on `origin/lead-records-369`:** ADR-324, ADR-317, ADR-104, ADR-109 (and A1), ADR-185, ADR-186 §5(g), ADR-188, ADR-266 item 10, ADR-291, ADR-309; ROADMAP B308, B424, B557, B585.
  - **The spec:** `specs/SPEC-ENGINE.md` at `2af41d8`, read only: lines 9, 12, 31–36, 68, 169, 194, 549, 553, 613, 621, 671, 676, 677, 772–788, 807, 825, 828, 838–876, 922–976, 1075–1083.
  - **The legacy shell:** `src/morph_core.h` (no hysteresis; T ≥ 0.02; weights at or below 10⁻⁹ score −10¹²) and `src/hypersaw_clap.cpp:134,525–536` (legacy morph settings: temperature 0.02–4 default 1, coupling 0.3, seed 1–9999 default 1024, glide 0–5 s default 0.008, mode 0 = quantum).
  - **Not read:** the clean-room repositories.
- **Alternatives rejected:**
  - New open-item tags with an M prefix: they would change the spec's "How to read it" rule. X28–X35 continue the existing series.
  - Putting the resolver's state (corners, settings) into IF-4's state document: that would change the state format in this round. RC-11.1 keeps it as morph state.
  - A raised-cosine handover in SB-10: it would sound three or four settings at once and break the count of 2.
- **Verify:** `./verify fast` exit 0 on HEAD `2af41d8` with this change staged (`.harness/last-verify.json`: tree `ed88de6f…`, 2026-10-30T00:21:30Z).
- **Open questions:** X28–X35 in the document §5, for the human. Not verified here: whether horde 2's shell has any stored morph state yet that X32 would affect. The step-rate range (0.25–32 per second) is PROPOSED with no source.
