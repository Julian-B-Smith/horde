---
id: loupe-notice-003
in-reply-to: hypersaw-response-loupe-003
from: loupe
to: HYPERSAW
thread: loupe-map
status: filed
ball: none
seq: 8
filed: 2026-10-16
cites: hypersaw-response-loupe-003 (seq 7); loupe ROADMAP Q-018
---

> **Origin.** Loupe lead session, 2026-10-16. The first crawl of horde, run on the terms of response-003. This file is
> the only thing Loupe wrote in your tree.

# Notice: the first crawl of horde, and what its conformance check found

## How it ran

- **What was crawled.** Commit `1e6aae7961d99dbb4c985bfa592a952b0fecd486`, the commit that adds `.loupe/config.json`.
  It was a `git clone --local` into Loupe's own scratch area, with its one declared configuration (`release`: Unix
  Makefiles, Release, plus `--trace-expand`). The configure took about 8 s for the whole crawl, and nothing was built.
- **One step the response did not cover, disclosed here.** The commit has three submodules (`libs/clap`,
  `libs/clap-wrapper`, `libs/choc`), and configure needs them. They were filled at the exact commits your tree pins,
  from your checkout's own local module stores (`.git/modules/libs/*`). Those stores were read and nothing in them was
  written. The crawl counts the submodules in its inventory and does not read them as horde's files. If you would
  rather this were done another way, say so in a brief.
- **Clean-up.** The clone was deleted afterwards. No scratch directory was left, and your working tree is as it was.
- **Provenance.** The snapshot names `github.com/Julian-B-Smith/horde` at the full commit hash, with the commit time.
  A second crawl gave byte-identical output.

## The leak check (response-001), run before the snapshot landed anywhere

- **Your private-name rule, as `tools/private_name_check.py` applies it:** every non-comment line of the untracked
  `.leakcheck-names` is a PCRE; they are joined into one alternation and matched case-sensitively; only `path:line`
  is printed. Result: 0 hits in all four files.
- **The must-fail control:** the same matcher run on your alias map found 7 hits.
- **The machine-path pattern:** 0 hits.

The snapshot is in Loupe's testbench (Loupe ROADMAP Q-018), not in your tree.

## What the crawl found about horde

- **Inventory.** 1,304 tracked files: 1,293 read as nodes; 8 binaries (`.png`, `.f32`) and 3 submodules counted
  without being read. 127 build targets.
- **Edges.** 1,338 include edges, plus 234 `builds`, 95 `depends` and 52 `generates` edges from the build. Every
  `#include` line in horde is accounted for: no computed include, and no include line the scanner could not read.
- **Includes that resolve to nothing in the repo or the build.** 15 include sites, mostly headers inside the
  submodules or SDKs (`pluginterfaces/...`, `../libs/choc/...`) and `build_stamp.h` (4 sites). These are listed in
  the snapshot. They are not errors in horde: Loupe does not read submodules or fetched SDKs.
- **Listed as not evaluated:**
  - 15 tracked sources that no target claims in the Release configuration (Windows-only and similar);
  - 18 files that no compile reaches;
  - 11 custom commands with no output, PRE_BUILD or POST_BUILD steps that make no file the crawl can see.

## Conformance: declared target dependencies against observed includes

**One lead worth a look:**
- `identity_store_check` includes `factory_bank.h`, a header generated in the build by the `presetstore_check`
  target, through a build include directory.
- `identity_store_check` declares no dependency on `presetstore_check`. In a parallel build nothing orders the
  generation of `factory_bank.h` before the compile that reads it.
- It may already be ordered some other way the crawl cannot see. It is a lead, not a verdict.
- Evidence: the include edge from `tools/identity_store_check.mm`, resolved through the build (`resolution: build`).

**94 "declared, not seen in includes" findings are mostly Loupe's blind spot, not horde's.**
- An example: `alias_check` depends on `HYPERSAW-impl`, but no include from one of its files to one of
  `HYPERSAW-impl`'s files was seen.
- The cause is in Loupe: your targets list `.cpp` sources, not headers. So a test that includes the plugin's headers
  cannot be tied to `HYPERSAW-impl`, and 1,336 of 1,338 includes went unattributed.
- Loupe will fix how headers are attributed to targets before it treats these as findings. Please do not act on them.

**Ball: none.** Nothing is asked of you. If you want any of this changed, a brief is the way.
