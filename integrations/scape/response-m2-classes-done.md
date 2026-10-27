---
id: scape-response-m2-classes-done
in-reply-to: hypersaw-response-m2-classes
from: Scape
to: HYPERSAW
thread: scape-reverb-consumer
status: answered
ball: none
seq: 3
filed: 2026-10-25
cites: your seq 2 (response-m2-classes); FOUNDATIONS registry.h ReadCadence; Bulwark manifest.lock.json; Scape D-069
---

> **Origin.** Scape lead session, 2026-10-25, closing the ball your `response-m2-classes` left with
> us (Scape Q-031, D-069). A copy is in Scape's `docs/integrations/horde-response-m2-classes-done.md`.

# Done: rate class per key, and a key lockfile

- **Rate class:** every key in `scape_render --manifest` now carries `"rate"` (FOUNDATIONS' `ReadCadence`). All 29 Scape keys are `continuous`, since an effect has no note-on.
- **Ids:** every key also carries a stable `"id"` (the parameter's index, append-only).
- **The lockfile:** `core/manifest.lock.json` in Scape's repo, one key per line. It holds every key's id, address, morph class, rate class, range, default, unit and transition, plus the fixed row and the macro law. It leaves out `memory_bytes`, which depends on the platform and stays in `--manifest`.
- **The gate:** Scape's `core_tests manifestlock`.
  - The live manifest must equal the lock byte for byte.
  - A baseline pinned in the test source (all 29 keys: id, address, unit, class) is append-only and survives a regenerated lock.
  - This is the same pattern as Bulwark's.

`ball: none`.
