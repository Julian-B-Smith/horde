# 2026-11-07 — LFO pictures show their lower half; a guard on the morph law the plug-in reports

**What changed.**
- **`docs/design/gui3.html`.**
  - An LFO's `f(t)` for the master's modulator tiers is now the drawn shape mapped from −1..+1 to 0..1. The tiers clamp a source's `f` to 0..1, and a bipolar source rests at 0.5 (`ModPicture.paint` in `docs/design/arp-face/bundle.js`). Read raw, the shape's lower half was clamped flat in every LFO picture.
  - A new self-check (4c6b): LFO 1's `f` stays within 0..1 and its sine reaches below 0.1 and above 0.9.
- **`tools/labharness/gui3_browser_check.mjs`.** The bridge probe reads the Morph mode control:
  - at the start, it reads D.2's Quantum;
  - after a frame reporting Blend, it reads Blend;
  - after a frame reporting Quantum, it reads Quantum again.

**Why.**
- **The LFO pictures.** The human, 2026-11-07: LFO outputs looked "rectified (only measured between 0 and 1)". The LFO card's own editor draws −1..+1. The rows in the tiers, and the card's large trace, cut off everything below 0.
- **The law guard.** The lead relayed a report from build 9: the Morph page "says it starts in quantum mode but it starts in blend mode". It wasn't reproduced:
  - Every default is Quantum: D.2, `morph_core.cpp`, `MorphSettings` and the page's `default_v`.
  - On a stand-in bridge built like `preview_plugin.cpp`, the page and the plug-in both held Quantum, and the page sent no law. The stand-in sends the morph member only after `want.morph`, and its settings in full on the first member and then on change.
  - When the stand-in restored Blend, the page showed Blend once its Morph page opened.
  - The human then could not recreate it either.
  - The guard holds the adoption path, as the lead asked.

**Evidence.**
- `gui3_browser_check`: GREEN, 89 self-checks.
- **Controls, each run and restored:**
  - The shape read raw again turned 4c6b red, spanning −1.00 to 1.00.
  - A page that ignores a frame's `settings` turned the law guard red: "Quantum after a frame reporting Blend".
- Not seen in the repro: with no row selected, the Morph pad draws the XY pad's four-colour wash under the QUANTUM banner. That is the master's QuantumPad rule (regions belong to one row), the same on the stub, and not changed here.

**Not done here.**
- The LFO row's value readout still reads the Session's live value. That is 0, since no engine LFO exists yet (RC-4 deferred; the card says so).

**Verify.** `./verify fast` on the commit this trace ships in.
