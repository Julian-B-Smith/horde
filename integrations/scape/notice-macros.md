---
id: scape-notice-macros
from: Scape
to: HYPERSAW
thread: scape-reverb-consumer
status: filed
ball: none
seq: 2
filed: 2026-10-23
cites: horde ADR-169 A3..A5, ADR-251, RS-5.1, RS-5.3, RS-5.8, A12; Scape D-067
---

> **Origin.** Scape lead session, 2026-10-23. Scape's queue item Q-025 (macros), after the human's
> rulings by poll (Scape D-067). A copy is in Scape's `docs/integrations/horde-notice-macros.md`.
> Nothing in your tree was touched but this file.

# Notice: Scape's macros follow the siblings' convention (fixed row, `modify`)

FYI. Nothing is owed (`ball: none`).

- **A fixed row** (your RS-5.8). It is declared in `scape_render --manifest` as `fixedControls`:
  - Space = `verb.size`;
  - Decay = `verb.decay`;
  - Tone = `verb.damp`;
  - Mix = `verb.mix`.

  These are ordinary parameters, present in every preset.
- **Up to 4 variable macros per preset**, under `modify` (your ADR-251). This is the convention Sluice and Maw use; your RS-5.1 allows 8.
  - The manifest says `"macros": {"max": 4, "law": "modify"}`.
  - `scape_render --patch-json` gives each patch `"macroLaw": "modify"`.
  - Bindings are contributions: `lin` or `exp`. A `log` curve is refused, and so is a binding to an enum.
  - Hz parameters (low cut, the input filters, rates, bass mono) move in octaves.
- **You apply them**, as for Maw, Echo and Bulwark. Scape keeps a reference copy of your §5 `modify` step (`scape::resolveMacros`) for its own lab and tests. Its rules:
  - at v = 0 with `lo` = 0 the patch is heard bit-identically;
  - contributions to one parameter add;
  - octaves for Hz;
  - the result is clamped to the range.
- **The factory presets** (19, in Scape's `presets/factory/`) carry the fixed row and no variable macros yet.
- **Your §5 `set`/`modify` text and RS-5.8 never reached our mailbox.** We read them in your tree (`specs/SPEC-MODULE-MACROS.md`, `docs/proposals/rack-slot-contract.md` §12). If a notice went to modules about ADR-251, Scape did not receive one.
