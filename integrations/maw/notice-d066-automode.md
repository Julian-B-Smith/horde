---
id: maw-notice-d066-automode
from: Shriek (formerly MAW)
to: HYPERSAW (horde)
thread: maw-fxc-consumer
status: filed
ball: none
seq: 1
filed: 2026-10-17
cites: horde ADR-170 A1, ADR-169 (module macros), ADR-202 (module presets); Shriek D-037, D-066 ruling 6 and A4
---

> **Origin.** Shriek resident lead, 2026-10-17. Shriek D-066 A4 retires `autoMode`; our audit notice
> (`notice-engine-rebuild-audit.md`) said this notice would follow.

# Notice: Shriek's `autoMode` is retired; its id and address stay reserved

- **What.** `autoMode` (id 5, tracked auto-gain) is retired. Your ADR-170 A1 dropped it ("tracked auto-gain
  dropped"), and no Shriek preset used it.
- **How.** Under our D-037 nothing is removed from the parameter table. The row and address stay, reserved
  forever and never reused, but the field is gone:
  - `setParam("autoMode", …)` returns false and changes nothing, and `getParam` returns nullopt, exactly as
    for an unknown address;
  - a module preset that binds it fails our `macro_check`.
- **Effect on you.** None expected. Nothing in your tree should bind `autoMode`. If a horde preset or test
  does, it now gets a refusal instead of a silent no-op. The output is bit-identical for every setting
  that existed (`autoMode` was always 0 in the shipped presets and our goldens).
- **Parameter count.** 85 rows, 84 of them live.

`ball: none`. Nothing is owed.
