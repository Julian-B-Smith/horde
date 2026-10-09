# b451-manual-scaffold — the horde 1.0 user manual as scaffolding, with a figure registry and its check

- **Queue item:** B451 (the row is on the lead's records branch `lead-records-222`, not yet on
  main at the time of writing; the human's words are quoted in its brief). Related: B413 (the
  manual follows the feature freeze).
- **Why:** The human asked for scaffolding that "should mark out all the chapters and all the
  needs for figures and diagrams and tables, etc." The manual targets horde 1.0 = horde 2
  (ADR-186), with the roster taken from ADR-190 (frozen 2026-10-07). Placeholders and a registry
  are two lists of one set, so a check holds them together in both directions.
- **What changed:** `docs/manual/README.md` (audience, conventions, status vocabulary, how to add
  a figure); twenty chapters `docs/manual/01-…20-*.md` (244 H2/H3 sections, each with a
  one-line purpose and a `Status:` line citing its row or ADR: 210 PENDING, 34 READY-TO-WRITE);
  `docs/manual/figures.json` (233 entries: 76 tables, 48 diagrams, 46 audio, 43 screenshots,
  20 figures; 101 needed, 94 pending-design, 38 ready-to-make; 71 generated);
  `tools/manual_scaffold_check.py`, wired into `verify fast` after `golden_pin_check`.
- **Evidence consulted:** `docs/H2-PLAN.md` (parts table, "What 1.0 is", open decisions);
  ROADMAP rows B327, B385, B398, B392, B396, B394, B389, B395, B302, B393, B402, B438, B225,
  B435, B439, B448, B449 (R1–R11 and the 2026-10-08 ruling), B445, B447, B408, B413, B424,
  B425, B428, B429, B431, B432, B433, B440, B443; DECISIONS ADR-169 A3/A4, ADR-188, ADR-190
  (+ A1, A2), ADR-191, ADR-192, ADR-193, ADR-195, ADR-198; `specs/SPEC-SCALPEL.md` §2–§6 and
  §13 (read only); `docs/design/` lab list (read only).
- **Alternatives rejected:** (1) one chapter for the Sub alone: ADR-190 A6 puts a plain noise
  oscillator in 1.0 with no row or design, so it shares chapter 05 with the Sub rather than
  getting an empty chapter. (2) Per-chapter parameter summary tables: they would duplicate the
  generated reference, so chapters point to chapter 19 instead. (3) MPE as a "pending" chapter
  on its own: ADR-190 A4 rules MPE IN, so it is a chapter with the arpeggiator, MIDI learn and
  automation, its sections PENDING on B388's design. (4) Checking only `depends_on` ids: the
  check also resolves every section status citation, because a status citing a row that does
  not exist is the same fault.
- **Verify:** `./verify fast` exit 0 at `9ebf45c` (`.harness/last-verify.json`:
  `{"target":"fast","exit":0,"git":"9ebf45c","ts":"2026-10-09T17:46:36Z"}`); the check prints
  `manual_scaffold_check: GREEN — 20 chapters, 233 registry entries, 7 must-fail controls red +
  positive control green`. Controls were also checked by hand to fail for their own reason
  (each planted fault yields exactly its one named error), plus a malformed status dash, an
  unknown status citation, a kind/prefix mismatch, a wrong chapter, and a zero-parts BLIND case.
  `verify full` not run (scaffolding only; no code path changes).
- **Open questions:** H2-PLAN (last verified 2026-10-01) predates ADR-190: its `noise` part is
  "noise osc / sampler, post-1.0", while ADR-190 A6 puts a plain noise oscillator IN; ECHO,
  EQ, the FX filter, the drive module, MTS-ESP, true stereo and the arpeggiator have no H2-PLAN
  part id, so their registry entries depend on rows/ADRs instead. The noise oscillator has no
  ROADMAP row of its own. B451 is not on main yet, so nothing here cites it.
