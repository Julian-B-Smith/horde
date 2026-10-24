# b536-design-system-readme — docs/design-system/README.md brought in line with ADR-259 to ADR-273 and pointed at the published design system

- **Queue item:** ROADMAP B536 (repo audit 2026-10-21, finding H5). Written by the implementer for the horde lead, who opens the PR. Draft trace.
- **Why:** the README still ruled the window, glyphs, re-theming, the dark chassis and contrast the opposite way to ADR-259, so its own "UI Spec wins" rule pointed a design-system build at the ruled-against window size. The ruled answers replace the old statements, each cited to an ADR; the published design system is named as the living reference and the `.dc.html` files as earlier documents.
- **Evidence consulted:** `docs/design-system/README.md` (before); `docs/audits/2026-10-21-repo-audit.md` H5 (:224-247); `DECISIONS.md` ADR-259 (:8915), ADR-261 and A1 (:8951, :8986), ADR-262 (:8992), ADR-266 and A1 (:9070, :9157), ADR-272 and A1 (:9206, :9240), ADR-273 and A1 (:9222, :9238); ROADMAP B37, B302, B531, B532, B544 to B549; `src/gui/gui2.html` (`--bg:#F2EDE2` cream :28, `body.dark` :187); ROADMAP.md:86-93 for the STATION page row.
- **Changed statements (old to new):**
  - window "1180 x 820, min 1000 x 700, UI Spec wins" to "980 x 720, resizable" (ADR-259 r2);
  - glyphs "unresolved, need a human ruling" to "always", pips shape-coded (ADR-259 r3, ADR-261 r1);
  - corner names GLASS/GRIT/HOLLOW/BLOOM to real preset names, never placeholders (ADR-262 r3);
  - re-theming "unresolved" to "the chassis carries corner colours; screens use their own glyphs or A-D" (ADR-259 r4);
  - open question 3 "dark chassis, a derivation" to ratified after a contrast audit, cream default (ADR-259 r7);
  - the 2.84:1 finding "ruling wanted" to darker text and lines from ORCHID, 28 px targets (ADR-259 r8), the finding kept as history;
  - "Status: SPECIFIED, NOT BUILT / shipped interface is dark" to partly built (gui2 cream per B37 increments) with GUI 3 on the published system (B302, B531);
  - screens, keyboard mode, fonts, knob sweep added (ADR-259 r1, r5, r6; ADR-261 A1; ADR-262 r2);
  - FX module colour: cream with a small screen each, `fxs-` tokens, cable tokens (ADR-273, A1); Chain line gain bars (ADR-272 A1);
  - UI Spec / Design System "normative for values / layout" qualified: rulings first, the `.dc.html` files are earlier documents.
- **Kept as it was:** the table rows for the two protected interaction specs, the three prototypes and `support.js`; the "kept as delivered" rationale; the UI Spec precedence quote; the shadow, meter-fill and `well` value conflicts. Added: a STATION page row (the file was unlisted) and a "Last verified" line.
- **Alternatives rejected:** deleting the whole "where they disagree" table (the three untouched value conflicts are still true and still protect against backwards correction); transcribing the published system's tokens into the README (a second copy of normative values, the failure the README already warns about).
- **Verify:** `nice -n 10 ./verify fast` in the worktree, base main `d517cd5`; exit code recorded in the lead report.
- **Open questions:** (1) the published artifact's tokens were not read (private; not reachable from this session), so the README says a difference from the UI Spec that no ruling explains is a defect to file against B531, not which side is right. (2) The minimum window size is not ruled in ADR-259; the README states none. (3) Whether gui2 already meets ruling 8's 28 px floor (B37 increment 2 reports a 28x28 hit floor over 188 controls) was not re-checked; the README claims neither way.
