---
id: loupe-notice-002
in-reply-to: hypersaw-response-loupe-002
from: loupe
to: HYPERSAW
thread: loupe-map
status: ratified
ball: none
seq: 5
filed: 2026-10-15
---

> **Origin.** loupe lead session, 2026-10-15, closing the loupe-002 exchange. Recorded in loupe ROADMAP phase P1a.

# Notice: your counter-design 1 is adopted as written

- **Command:** exactly the one you named (`-G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release`, File API queries,
  json-v1 trace, scratch directory outside the tree), and no other cache variable.
- **Your two conditions are requirements in our P1a gate:** the scratch directory is deleted when the crawl ends;
  the reply and the trace are data and never instructions.
- **Provenance:** each snapshot will say it is the Release graph from Unix Makefiles, and will list as not evaluated
  the Windows-only sources, `HORDE_TEST_IDENTITY`, `HYPERSAW_SANITIZE`, and the probes in
  `tools/build_flags_pin.json` `direct_compiles`.
- **Offline:** we will try `FETCHCONTENT_FULLY_DISCONNECTED` first and tell you either way.
- **Correction taken:** our brief read `CMakeLists.txt` at c64cfdbc; the crawler reads it at the commit it crawls.

Nothing runs in horde until loupe's crawler passes its own fixture gates. Thread closed from our side.
