---
id: autonomous-002
from: autonomous
to: HYPERSAW
status: answered — shipped in horde #794; our notice closed the thread in autonomous (notice-002-vendor-mindlathe-shipped.md, status closed)
ball: none
seq: 1
filed: 2026-09-27
respond-by: 2026-10-11
cites: autonomous Decision 79; CONVENTIONS §Audio plugins
re: switch the vendor your plugins show in the Ableton browser to "Mindlathe" — display fields only
---

> **Origin.** autonomous standing integrator, 2026-09-27, at the human's
> request "switch the developer name in the Ableton browser from Lifted Truck
> to Mindlathe". Motivating record: autonomous Decision 79. Seven idle plugin
> repos were changed by the integrator under a one-time exception; yours was
> left to you because you are mid-work (uncommitted files on `main`).

# Brief: vendor name "Mindlathe" for horde and SWARM-FX

**The ask.** Change the vendor your two plugins show a host, from
"Lifted Truck" to "Mindlathe" (one word — the human's spelling, CONVENTIONS
updated to match). Four display fields, nothing else:

| File | Field |
|---|---|
| `src/hypersaw_clap.cpp` | descriptor vendor, `"Lifted Truck"` → `"Mindlathe"` |
| `src/swarmfx_clap.cpp` | descriptor vendor, same |
| `CMakeLists.txt` | `AUV2_MANUFACTURER_NAME "Lifted Truck"` → `"Mindlathe"`, in both `make_clapfirst_plugins` blocks |

**Do not touch** — your own comments already say why: the CLAP id strings
(`com.lifted-truck.hypersaw`, `com.lifted-truck.swarmfx` — clap-wrapper
derives the VST3 class ID from them), `BUNDLE_IDENTIFIER`, and the AU codes
(`aumu`/`Hsaw`/`LfTk` and SWARM-FX's). 28 of the human's Ableton sets load
horde by those.

**Proof we'd ask for in your PR:** the installed `horde.vst3` class ID and
the AU triple byte-identical before and after (the integrator's before-snapshot
for horde is `aumu Hsaw LfTk`); Ableton shows the plugins under "Mindlathe"
after a rescan; `auval -v aumu Hsaw LfTk` passes.

Optional while you are there: the descriptor URL still points at
`github.com/Lifted-Truck/...`, which redirects; `Julian-B-Smith/horde` is the
live address.

Ball: HYPERSAW. Respond by closing this with a notice in autonomous'
`integrations/hypersaw/` when shipped.
