# Contrast audit

ADR-259 rulings 7 and 8, 2026-10-21: every text pair at least 4.5:1, and 3:1 for text 24 px and up, control edges, focus rings and meaningful icons, in both themes. WCAG 2 contrast, measured on the hexes in tokens.json, and checked again on every component preview in both themes (no text under its floor). Failures were fixed with the smallest change; a source pair is kept only where a ruling keeps it.

## Changed, for the human to review

| # | Pair | Was | Now | Ratio before → after | Theme |
|---|---|---|---|---|---|
| 1 | `hint` text on bg, panel, well | #9a93a3 | #6e6878 | 2.54 / 2.84 / 2.97 → 4.60 / 5.14 / 5.36 | cream |
| 2 | `focus-ring` on every ground | value at 45% alpha | solid `value-ink` #d6006b | 1.86–2.03 → 4.03–5.16 | cream |
| 3 | `focus-ring` on every ground | #ff3dad at 45% alpha | solid #ff3dad | 1.89–2.04 → 4.43–6.02 | dark |
| 4 | text on corner B's fill | #ffffff | `on-corner` #191521 | 3.50 → 5.12 | both |
| 5 | text and glyphs on any corner fill | `ink` #efe9fa | `on-corner` #191521 | A 1.18, B 2.95, C 1.56, D 1.15 → 12.8, 5.12, 9.7, 13.1 | dark |
| 6 | corner A text and lines (`cA-tx`) on panel | #d4a900 | #8a6a00 (marker's ink) | 2.12 → 4.86 | cream |
| 7 | corner B text and lines (`cB-tx`) | #ff2e88 | #d6006b (ORCHID's value ink) | 3.35 → 4.94 | cream |
| 8 | corner C text and lines (`cC-tx`) | #00b3a8 | #007a73 (ORCHID's meter ink) | 2.51 → 4.99 | cream |
| 9 | corner D text and lines (`cD-tx`) | #6fb200 | #4e7a00 (celebrate's ink) | 2.50 → 4.90 | cream |
| 10 | meter as a line or text | #00d5c8 | `meter-ink` #007a73 | 1.77 → 4.99 (lines 4.07 or more on every ground) | cream |
| 11 | value as text | #f5169c | `value-ink` #d6006b | 3.67 → 4.94 (panel), 5.16 (well) | cream |
| 12 | meter peak hold on well-alt | #f5169c | `value-ink` | 2.99 → 4.03 | cream |
| 13 | knob and slider value arc on its well-alt track | `value` #f5169c | neutral `ink` (morph knob spec) | 2.99 → 14.0 | cream |
| 14 | the selected corner chip, told from an idle one | fill only (corner A 1.34 against panel) | fill plus a 1.5 px ink outer ring | 1.34 → 17.2 | both |
| 15 | hit targets | 22 and 26 px in the labs | 28 px everywhere | — | both |

Rows 6–9 reuse inks already in the system rather than inventing new ones. Ruling 8 asked for ORCHID's inks; for A and D, ORCHID has none of the same hue, so they take gui2's own legible ink for that hue (marker, celebrate).

## Added with the morph knob and map mode specs

The `src-` family (`src-env` #b05500 / #ff9a3d, `src-lfo` #2f63e8 / #6e9bff, `src-macro` #a534c9 / #d77bff, `src-engine` #7a5230 / #c9976a) clears 3:1 as a line on every ground in both themes (4.34–8.7) and 4.5:1 as text on panel and well; `on-src` text on a source fill reads 4.85–6.54 cream and 6.2–8.7 dark. None is a corner hue (least OKLab distance to a corner 0.155, against 0.128 between corners C and D themselves). Flagged: `src-engine` and `src-env` are the closest pair (0.105 cream, 0.097 dark), told apart by lightness and the caption.

The `fxs-` family (the FX modules' screens, ADR-273, 2026-10-23; replaces the `fx-` fills of ADR-266 ruling 5, which are retired with `on-fx-` and `fx-edge`): four tokens per module, `fxs-<module>-ground`, `-ink`, `-mid` and (ADR-273 A1) `-cable`, one value in both themes because a screen is its own display. Generated from `hds-tools/fx_palette.py`, which refuses to write them unless every screen ink clears 4.5:1 on its own ground and the eight grounds sit at least 5 L* apart (CIE L*, as the comparison page measures it).

| Module | Ground | Ground L* | Ink | Ink on ground | Mid | Mid on ground (drawing only) | Ground vs panel cream / dark | Cable | Bare cable on well cream / dark |
|---|---|---|---|---|---|---|---|---|---|
| Bulwark | #030205 | 0.7 | #3de8ff | 13.98 | #1c6a78 | 3.33 | 19.83 / 1.24 | `#3de8ff` | 1.48 / 13.10 |
| Filter | #b9c4a6 | 77.6 | #2e3a2c | 6.55 | #8a9678 | 1.71 | 1.75 / 9.17 | `#6f8a5c` | 3.84 / 5.05 |
| EQ | #1b2f6b | 21.3 | #9fc4ff | 7.09 | #4d6bb8 | 2.47 | 12.08 / 1.33 | `#9fc4ff` | 1.78 / 10.91 |
| Drive | #3b4a3f | 29.9 | #b8ffc9 | 8.13 | #6b8a72 | 2.46 | 8.99 / 1.78 | `#7dff9a` | 1.26 / 15.37 |
| Shriek | #1a1208 | 6.0 | #f0a830 | 9.13 | #8a5a14 | 3.13 | 17.74 / 1.11 | `#f0a830` | 2.03 / 9.56 |
| Echo | #6f5e4a | 41.0 | #fbf0dc | 5.51 | #b09a7c | 2.30 | 5.96 / 2.69 | `#d9b98a` | 1.87 / 10.40 |
| Sluice | #9bbc0f | 71.5 | #0f380f | 6.02 | #306230 | 3.29 | 2.10 / 7.65 | `#8bac0f` | 2.62 / 7.39 |
| Scape | #2a2140 | 15.2 | #ff4fa3 | 4.96 | #8a6fd4 | 3.82 | 14.47 / 1.11 | `#ff4fa3` | 3.04 / 6.37 |

Least ink on ground 4.96:1 (Scape); least ground gap 5.38 L* (Bulwark and Shriek). The module's name is `ink` on the `panel` card (14.0:1 or more cream, 12.0:1 or more dark), so there is no coloured text. Flagged, and handled by rule: (1) the cable colour is the comparison page's, chosen as a display colour, so it is weak as a bare line on the cream `well` (Drive 1.26, Bulwark 1.48, EQ 1.78, Echo 1.87, Shriek 2.03, Sluice 2.62, Scape 3.04; only Filter, 3.84, clears 3) and reads bare on the dark one (5.05:1 or better; 4.36:1 on the dark `panel`). Every cable core is cased in `ink` (17.94:1 on the cream well, 16.35:1 on the dark), so every cord reads at 3:1 or more on both wells and both cards (tested, with a plant that removes the casing and one that drops a cable token). (2) Several grounds are close to the dark `panel` (1.11–1.33:1: Bulwark, EQ, Shriek, Scape), so the inset carries a 1 px `ink` edge. (3) `mid` is not text and not a meaning-carrying line; it is picture and grid. (4) Six of the eight screens are stand-ins until their skins are chosen.

## Selection surfaces (the human's values, 2026-10-21)

The `sel-` fills hold ink (10.6–16.9:1) and t2 (6.4–8.9:1) in both themes. FLAGGED, and handled by rule rather than by changing the human's values: light `cB-tx` on its tint reads 4.30:1, and dark `hint` on any selection fill 3.5–4.1:1, so on a selection surface corner-ink text turns ink (its glyph keeps the colour) and hints are not used. Dark corner tints (`cA-tint`…`cD-tint`), flagged missing before, now take these values.

## Kept, and flagged

- `ghost` (1.3–1.7 cream, 2.0–2.7 dark): the reference trace recedes by design; the wet trace carries the information, and a ghost is never the only carrier of a state.
- Corner fills alone on cream (A 1.20, B 3.00, C 1.58, D 1.17 on bg): kept exact because corners never re-theme; every corner fill carries its glyph and an ink edge, and corner text uses the `-tx` inks.
- `caution` (1.6–2.1 on cream): a fill only, never text or a meaningful line.
- `hair` and `div`: decorative structure, carrying no meaning.
- `dis`: disabled controls are exempt.
- On `bg`, `value-ink`, `marker`, `celebrate`, the corner `-tx` inks and the source colours read 4.0–4.5:1, so their notes allow text on `panel` and `well` only (controls live on cards). `hint` is not set on `well-alt` (4.19 cream, 3.94 dark).
- Inside a glass, ORCHID's value (4.45) and meter (4.499), LUMEN's value (4.15), meter (4.10) and marker (4.24), and DUSK's grid (3.82) draw lines and marks only; glass text uses `scr-physics` or `scr-grid`. LUMEN and PAPER are restored retired schemes; PAPER clears 4.5:1 on every role (5.46–8.36).

## The dark theme

Ratified after this audit (ruling 7). Its own values are gui2's derivation, unchanged, except rows 3 and 5 above. Every text token passes on bg, panel and well (ink 14.1–16.4, t2 8.5–9.8, hint 4.63–5.36, value 5.2–6.0, physics 5.4–6.3, alarm 5.4–6.3, the corner inks 6.5–15.7).

## Density and hierarchy (B589, 2026-10-30)

No colour was added: the page nav, section header, channel strip and the dense faces reuse existing tokens in their existing jobs. Each new pairing, measured on the tokens.json hexes (WCAG 2), cream / dark:

| Where | Pair | Cream | Dark | Floor |
|---|---|---|---|---|
| Sub-group labels (`sublabel`, 8.5 px), the readout slot at rest | `hint` on `panel` | 5.14 | 4.63 | 4.5 text |
| The page row (level 2 tabs) on the window ground | `t2` on `bg` | 8.10 | 9.34 | 4.5 text |
| A selected page tab, its 3 px underline | `ink` on `bg` | 15.37 | 15.54 | 4.5 text, 3 line |
| A selected group tab, the active dense pill, solo on, the section number | `bg` on `ink` | 15.37 | 15.54 | 4.5 text |
| The section identity rule (3 px) | `ink` on `panel` | 17.19 | 14.11 | 3 line |
| Mute on (the button's text on its fill) | `on-alarm` on `alarm` | 5.25 | 5.99 | 4.5 text |
| A page tab's on-dot (7 px, ink edge) | `value` on `bg` | 3.28 | 5.73 | 3 icon |
| The channel strip's peak readout, the "+n" badge | `ink`, `t2` on `panel` | 17.19, 9.06 | 14.11, 8.48 | 4.5 text |
| A sized knob's label on its selection tint | `t2` on `sel-cB` (the lowest tint) | 7.88 | 7.52 | 4.5 text |

Flagged, and handled by rule:
- A section switched OFF dims its body to `opacity-muted` (0.45), as a bypassed region already does (the `opacity-muted` token's job); its labels then read under 4.5:1 until it is switched on. The header line (toggle, name, number) is never dimmed, so the off state is always legible and reversible. The bench dims an off blade the same way (0.42).
- The dense faces change no colour pair: a 20 px pill keeps the classic pill's inks (`t2` on `panel` idle, `bg` on `ink` active).
- `hint` is still not set on a selection fill (dark 3.76:1 on `sel-neutral`): the slot and the sub-group labels sit on `panel`, never on a tint.
