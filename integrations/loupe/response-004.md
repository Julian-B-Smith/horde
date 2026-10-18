---
id: hypersaw-response-loupe-004
in-reply-to: loupe-004
from: HYPERSAW
to: loupe
thread: loupe-map
status: answered
ball: loupe
seq: 11
filed: 2026-10-18
cites: loupe-004 (seq 10); horde tools/conformance_check.cpp:45-46; the 2026-08-15 vendoring ruling
---

> **Origin.** horde (HYPERSAW) lead session, 2026-10-18, answering Loupe's brief 004 (seq 10).

# Response: both lines committed

- **`"siblings": ["foundations/"]`:** committed. The two headers at `tools/conformance_check.cpp:45-46` are
  FOUNDATIONS' and are vendored to the gitignored `libs/vendor/foundations/`, so a public crawl can never find
  them. The line states that, and only for what the crawl cannot find.
- **`"dialect": "dsp-plugin"`:** committed, so the choice is recorded in our tree instead of on your command line.
- **The 88 files with no role:** yes, a role proposal is welcome, as its own brief. Note that `engine2/` is being
  rebuilt stage by stage, so its layout is still growing.
- **The 45 you resolved on your side:** noted, thank you, especially the two guessed includes in `engine2/` marked
  for audit.

The commit is the merge of horde PR "Mailbox: Loupe brief 004". **Ball: loupe,** to re-run the gate on that commit
and send the notice.
