# b359-sluice-hold-gate — a tree-wide scan for private-sibling spec text, wired into `./verify fast`

- **Queue item:** B359 (from the B358 incident; related B333, B353, LIBRARY L0067).
- **Why:** the hold ("copy nothing from the private sibling into this public tree") was
  only checked by one lab's in-page scan of its own file, so spec phrases reached two
  traces and ROADMAP rows unseen. `tools/sluice_hold_check.py` scans every tracked text
  file against needles read at runtime through the gitignored `local/sluice` link, and
  never writes a needle anywhere: hits are reported as file, line, run count and a
  short hash. It SKIPS loudly where the link is absent (CI, other machines).
- **What changed:**
  - `tools/sluice_hold_check.py` (new, `WIRED: ./verify fast`). Normalisation after the
    lab's C17 idea (case, string joins, entities, comment and markdown marks,
    whitespace), reimplemented in Python. Needles: every 28-character window with at
    least 20 letters. Principled exclusions only: `integrations/sluice/**`, `libs/**`,
    path and id tokens (masked whole on both sides), and horde-origin windows (present
    in horde's tree at the last first-parent commit before the linked files' first
    commit, read from the sibling's own log, read-only). Seven controls run every
    time on in-memory copies. A dated PENDING table names two files (the lab, fixed by
    PR #850, and ROADMAP.md, lead-only); a listed file that reads 0 hits fails.
  - `verify`: one invocation beside `private_name_gate` (ADR-180 §1).
  - `traces/2026-09-28-b329-sluice-lab-spec-rebuild.md` (four fragments) and
    `traces/2026-09-28-b337-b338-sluice-lab-round2.md` (one) paraphrased. This edits
    two prior traces, which the provenance rule otherwise forbids; B359 orders it,
    because a privacy redaction cannot be done by appending. The meaning of each
    sentence is kept; only the wording changed.
- **Evidence consulted:** ROADMAP B333, B353, B358, B359 (branch `lead-records-141`);
  LIBRARY L0067; the C17 section of `docs/design/sluice-horde-lab.html` on
  `lab-sluice-r4` (PR #850); `tools/test_table_check.py` (wiring grammar);
  `tools/mailbox_delivery_check.py` (sibling-reading idiom); the sibling's git log for
  the two linked files (first commit 2026-09-28T00:35-04:00, one later revision).
- **Classification (scratch analysis, not committed):** every counted hit in ROADMAP and
  in the two traces is present in the sibling's FIRST spec revision, which predates
  the horde text carrying it; some of it was later echoed in horde's own filings in the
  sibling's mailbox, all dated after that revision. So none is horde-origin. The B328
  row was added at 02:39 on 2026-09-28, after the spec's first commit, and its two
  matches are in that first revision. The only credited horde-origin windows (17) come
  from horde's NETWORK spec and a quantum-morph phrase, present before the cut-off;
  they account for all 4 runs in `specs/SPEC-FX-NETWORK.md` and 5 in
  `docs/design/fx-design-lab.html`.
- **Alternatives rejected:** a substring regex mask (3x slower: it retries each start
  inside long words); scanning every offset of every file (about 3 s more; a word-start
  probe first, every offset only for files that hit, with the gap documented in the
  header); a cached gitignored corpus (not needed: blobs unchanged since the cut-off
  reuse this run's own scan, so only about 58 changed blobs are read twice); crediting
  horde text filed into the sibling's mailbox (it is not in horde's history, and a
  false credit would hide a copy).
- **Verify:** see the PR body and `.harness/last-verify.json` for the committed hash;
  the gate alone runs in about 5 to 7 s on this machine.
- **Open questions:** the gate is named `*_check.py` (the brief said `sluice_hold_gate.py`)
  so `test_table_check` enforces its wiring declaration. DECISIONS.md reads 0 counted
  hits, so it has no PENDING row. The word-start probe can miss a copy of 28 to about
  40 characters.
