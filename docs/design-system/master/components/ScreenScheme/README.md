# Screen scheme

The screen axis's selector: gui2's SCR chip, cycling the seven screen schemes, each of which redefines the same six screen roles and nothing else (ADR-121).

**The schemes** (tokens `<scheme>-ground`, `-value`, `-meter`, `-physics`, `-marker`, `-grid`, plus `scan-<scheme>` and `alpha-<scheme>`): TUBE (deep violet, the dark chassis's default), ORCHID (pale violet, the cream chassis's default), FROST, EMBER, DUSK, and LUMEN and PAPER. LUMEN and PAPER were retired in gui2 on 2026-08-24 and are restored from its history as selectable schemes; LUMEN is the yellow the human then called too harsh.

**Contrast** each role's ratio on its ground is in its token note. Glass text uses a role at 4.5:1 or more; a role under that draws lines and marks only. FLAGGED under 4.5: ORCHID value 4.45 and meter 4.499; LUMEN value 4.15, meter 4.10, marker 4.24; DUSK grid 3.82. Every role clears 3:1 on its ground.

**Behaviour** THEME (the default) follows the chassis: cream pairs ORCHID, dark pairs TUBE. A click on the chip steps TUBE → ORCHID → FROST → EMBER → DUSK → LUMEN → PAPER → THEME; the list form picks one directly. The choice sets the six `scr-` roles on the container it governs, so every glass, well, the XY pad and the Specimen inside it follow at once. A pale screen inside a dark chassis (or the reverse) is allowed: that comparison is the point of two axes.

**The consumer provides** the container it governs (the plugin window), and persistence as a GUI preference, not patch state.

**Accessibility** a button named "Screen scheme: ORCHID. Click for the next"; the list is a radio group.
