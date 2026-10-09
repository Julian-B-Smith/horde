# h2-plan-refresh — H2-PLAN and its map re-verified against the records through B451 and ADR-199

- **Queue item:** B387 (the plan of record, OPEN), at the human's request of 2026-10-09 ("start the
  h2 refresh"). No row of its own; the dispatch brief did not quote ROADMAP acceptance criteria, so
  B387's text ("every part of horde 2, its governing rows and ADRs, its status, its dependencies and
  the order of work") was taken as the criterion. Flagged to the lead.
- **Why:** `docs/H2-PLAN.md` was last verified 2026-10-01 and had gone stale: B451's manual scaffold
  had to follow ADR-190's roster over it (the noise oscillator and MPE in 1.0). A plan that lies
  about the roster is a bug of the same severity as a failing test.
- **What changed:** the parts table and the map's `DATA` (kept identical; map self-check 4).
  15 parts added (`mts`, `stereo`, `master`, `echo`, `bulwark`, `eq`, `fxfilter`, `drive`, `armor`,
  `distrib`, `manual`, `sampler`, `seqgen`, `vintage`, `standalone`); `fxsimple` retired into
  `eq`/`fxfilter`/`drive` (ADR-190 A7); `noise`, `arps`, `ott`, `maw`, `reverb` re-scoped in place
  with ids kept, because `docs/manual/figures.json` cites `arps`, `maw` and `reverb`. Status
  changes: `audit` in progress → ruled (rounds 1–2 decided 2026-10-02, roundup ratified 10-03);
  `intent` planned → ruled (ADR-192); `noise` post-1.0 → planned. A 1.0 column added to the table
  (in / conditional / open / out / parked); both parsers ignore it. Order of work, critical path,
  What 1.0 is, Post-1.0, Open human decisions (41 → 47; the answered ones are named and removed) and
  Inconsistencies (12 re-checked, 10 new) rewritten.
- **Evidence consulted:** ROADMAP rows B403–B451 and every row the table cites (latest dated
  sentences after 2026-10-01, by script); DECISIONS ADR-186–ADR-199 with ADR-190 Am. 1–2, ADR-169
  A3–A4; `docs/proposals/rack-slot-contract.md` header (PROPOSED); `docs/manual/figures.json`
  `depends_on` ids; `docs/audits/2026-10-02-engine-audit-round*-decisions.json`; `h2/README.md`;
  `git tag --list` (no legacy tag); `gh pr view 1007` (merged).
- **Self-check, outside a browser:** the map's own script evaluated in `node:vm` with stub DOM,
  calling its `checkCitations/checkAcyclic/checkRefs/checkDoc/checkCrit` against the files on disk,
  with the same planted controls `runChecks` uses: 5/5 pass, 5/5 controls fire. Critical path
  unchanged at 12 parts; an enumeration found four tied chains of 12 (`filters` for `sub`, `gui3`
  for `history`), now stated in the plan.
- **Alternatives rejected:** renaming the `maw`/`reverb`/`noise` ids to Shriek/Scape/noiseosc
  (breaks the manual's citations, out of scope); keeping `fxsimple` as an umbrella (the brief asks
  for EQ, the FX filter and drive as parts, and ADR-190 A7 names them); folding the conformance and
  legal rows into `release` (they are ship-blocking work with their own owner rows, B427–B433).
- **Verify:** `./verify fast` exit 0 at `b2177c2` (working tree with this change), 2026-10-09;
  `manual_scaffold_check` GREEN; `lab_load_check` loads `h2-plan-map.html` OK; leak check empty.
- **Open questions:** (1) the brief described B450's contract as ratified; ROADMAP B450 and the
  document's header say PROPOSED, and the plan follows ROADMAP. (2) Whether the human wants the
  manual in 1.0 (B451's default) decides whether `manual` joins the critical-path computation.
  (3) The noise oscillator, EQ, FX filter and drive have no ROADMAP row; the plan cites ADR-190 and
  the nearest design rows. (4) The browser self-check was not run; the node harness runs the same
  functions.
