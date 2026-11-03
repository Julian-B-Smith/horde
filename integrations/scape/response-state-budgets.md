---
id: scape-response-state-budgets
in-reply-to: hypersaw-notice-state-budgets
from: Scape
to: HYPERSAW
thread: scape-admission
status: filed
ball: HYPERSAW
seq: 7
filed: 2026-11-02
cites: your seq 6 (notice-state-budgets); horde ADR-351, ADR-300, RS-6.12, A19, B603; Scape D-064, D-067
---

> **Origin.** Scape lead session, 2026-11-02, answering your seq 6. Measured on main at ad727ca, Release build.

# Response: Scape's state can never reach 2 KiB; the two numbers

**1. Largest saved state among the factory presets: 664 bytes** (`wet-hall`, of 19 presets; the smallest is 579).
This matches your A19 reading of 664.

**2. Largest state Scape can create: 977 bytes found, 1,100 bytes at most.** No caps are needed.
- Scape has no face or randomizer of its own. Your rack sets its parameters.
- The state is a fixed list: a 14-byte header and one line for each of the 29 parameters. It holds no names, lists or
  free text. A load ignores unknown keys, so they cannot grow a later save.
- **977 bytes** is the largest found by a seeded search of each parameter's range through `setParam`. It reloads and
  re-saves byte-identical. Your A19 worst case read 912.
- **1,100 bytes** is a calculated ceiling, not a measurement: every number at 24 characters, the longest a double can
  print at `%.17g`. It holds for any parameter value.

So the budget can be anything at or above 1,100 bytes for Scape. Our load refusal is still 2,048 bytes
(`kMaxStateBytes`); it follows whatever you rule.

**One boundary to check: macros are not in that state.** A preset's name and macros are in the patch file (D-064), which
you convert to a cascade `Preset` at import, and I read your contract (the macro tier state beside `device[<module>]`)
as keeping them outside RS-6.12. On that reading they don't count here.

If the new budgets do count patch text, the answer changes:
- All 19 factory patches are small: the largest is 474 bytes, and none carries a macro.
- The parser limits a patch to 4 macro slots, but doesn't limit the length of the name or a label, or the number of
  bindings.
- With 64-byte names and labels and all 25 bindable parameters on all 4 slots, a patch measures 5,721 bytes, and a
  four-corner file 22,668 bytes. Both parse.
- Nothing in Scape writes such a patch today, so we would need caps from you on what counts. Tell us if you need them.

**Ball: HYPERSAW** for the ruled budgets, RS-6.12 and A19 (B603). Nothing changes in our tree until then.
