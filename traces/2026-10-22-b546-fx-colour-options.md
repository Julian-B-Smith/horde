# fx-colour-options — one comparison page of six FX module colour options

- **Queue item:** ROADMAP B546 (the human's request, 2026-10-22). Filed here, not in the repo's traces/, because the brief scoped every file to SP/fxcolour2/; the lead may copy it.
- **Why:** The human found the fx- tokens (ADR-266 A1) do not fit the cream aesthetic; the page lets them choose among baseline A, plain chassis with screen insets B, screen schemes C, and new palettes D clay and moss, E corner family, F ink tint.
- **Evidence consulted:** hds/project/tokens.json (fx-, corner, source, scr- and scheme tokens, themes), README.md colour rules, FxRack/FxMatrix/FxPanel/FxCables/ModuleCard/ScreenScheme READMEs, fxcolour/fx-colour.html, docs/design/fx-screens-workshop.html (skin names). Measured L* for A matches the FxCables README table (15.0, 20.2, 24.7, 29.8, 62.0, 66.9, 72.0, 82.0).
- **Alternatives rejected:** C as scheme grounds as fills (pale grounds all sit above L* 93 and cannot be told apart by lightness); one dark variant per option chosen by hand (used L' = 14 + 0.8 L, chroma x0.95, so order and spacing hold).
- **Verify:** node --check on the inline script: OK. check/render_check.py (playwright): 360 and 1200 px, light and dark, no overflow, no console errors, toggle cycles. No ./verify run: nothing in the repo changed.
- **Open questions:** B's six stand-in display styles are invented placeholders; the card mini pictures are generic.
- **Landed by the lead, 2026-10-22:** published as the FX Module Colours artifact (version 1); the human picks by letter.
