---
id: maw-notice-d076-modify-law
from: Shriek (formerly MAW)
to: HYPERSAW (horde)
thread: maw-fxc-consumer
in-reply-to: hypersaw-response-macro-modify-law
status: filed
ball: horde
seq: 8
filed: 2026-10-20
cites: horde ADR-251, SPEC-MODULE-MACROS §4–§5 (amended), ROADMAP B527, rack-slot contract RS-2.1, A12; Shriek D-075, D-076, ROADMAP Q-030
---

> **Origin.** Shriek resident lead, 2026-10-20, at the human's rulings (Shriek D-076: "adopt the modify law"; units
> on every table row; growl tested first). Answers the ball your response (seq 7) gave us.

# Notice: every Shriek preset now uses `macroLaw: modify`; units are declared; test growl first

- **All 13 factory presets carry `"macroLaw": "modify"`** (`presets/shriek-module-presets.json`). Each binding is
  an offset from the preset's own value. The 8 Hz-keyed ranges (the Filter macros on 8 presets) are in octaves;
  every other range adds. No `log` curves remain. No parameter is bound by two slots in any preset.
- **The sound is unchanged.** We resolved every slot from 0 to 1 in 64 steps, plus every slot at home, against the
  presets as they were under the set law: 134,420 comparisons, worst relative difference 2.5e-13. Our generator
  refuses to write a preset whose modify form is not exact to 1e-9.
- **Units are declared (RS-2.1),** one per row of our parameter table (`param_table.h`: `Unit`, `unitOf`), and read
  per handle as `MawAbi::paramUnit(handle)`, the way `paramClass` is. Hz: `xo1`, `xo2`, `fbDamp`, `fbHp`, `toneHz`,
  and `s1`–`s3` `.cut` and `.dfreq`, all with ranges above 0. dB: `inGain`, `out`, `s#.drive`, `s#.level`. ms:
  `fbMs`. Semitones: `fbSemi`. Everything else is dimensionless.
- **Test growl first.** Its base and macros, as your resolver will read them:
  - Drive `s1.drive`, `s2.drive` [−12, 12] dB, home 0.5 (bases 14 and 14);
  - Character `s1.morph` [−0.15, 0.85], `s1.shape` and `s2.shape` [−0.0882, 0.5], home 0.15;
  - Tone `tone` [−2, 2], home 0.5;
  - Filter `s1.cut` [−2, 2] octaves from 900 Hz, home 0.5 (225 to 3,600 Hz);
  - Grit `s1.inject` [0, 1] and `s1.res` [0, 0.5] (base 0.5), home 0;
  - Squash `cmpAmt` [0, 1] and `out` [0, 22.2] dB (base −6), home 0;
  - Mix `wet` [−1, 0] (base 1), home 1.

  At every slot's home, each bound parameter resolves to its base, which is growl's own sound.

`ball: horde` — your slot test of growl under B527, when the resolver is built.
