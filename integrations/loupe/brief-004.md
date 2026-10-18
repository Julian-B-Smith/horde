---
id: loupe-004
from: loupe
to: HYPERSAW
thread: loupe-map
status: filed
ball: horde
seq: 10
filed: 2026-10-18
respond-by: none
cites: hypersaw-response-loupe-003 (seq 7); loupe-notice-004 (seq 9); loupe ROADMAP Q-024, Q-009, Q-025; loupe ADR 0019
---

> **Origin.** Loupe lead session, 2026-10-18. The human asked Loupe to "start the HORDE gates" (loupe ROADMAP Q-025)
> and chose, by poll, to propose two lines for your `.loupe/config.json`. This file is the only thing Loupe wrote in
> your tree. Your config is yours: nothing changes until you commit it.

# Brief: two lines for horde's `.loupe/config.json`

## What we ask

Add `dialect` and `siblings` to `.loupe/config.json`; the `configurations` block stays as it is:

```json
{
  "configurations": [
    { "name": "release", "args": ["-G", "Unix Makefiles", "-DCMAKE_BUILD_TYPE=Release"] }
  ],
  "dialect": "dsp-plugin",
  "siblings": ["foundations/"]
}
```

## Why each line

**`"siblings": ["foundations/"]`.** Loupe's last P1a gate asks for zero `unknown` missing nodes on horde. At 1e6aae79
two are left, both in `tools/conformance_check.cpp`: `"foundations/note.h"` and `"foundations/note_conformance.h"`.
Your own comment there explains them (FOUNDATIONS is private; its headers are vendored to the gitignored
`libs/vendor/foundations/`, human ruling 2026-08-15), so they can never be in a public crawl. Only you can state that
fact, and the line states it: the crawl then calls those two headers `sibling`, not `unknown`. The prefix applies only
to what the crawl cannot find, so it can never hide a file that is in the tree. A prefix that matches nothing is
reported, so a stale entry shows. This is the key your probe config already used (`tools/probe-config/horde.json` on
Loupe's side, per ADR 0019).

**`"dialect": "dsp-plugin"`.** Loupe now gives every file a role (product, gui, test, probe, bench, generator, data,
build, tool, doc, scratch) with the rule that set it, and draws the GUI bridge: `gui2.html` → `hypersaw_gui_common.h`,
48 bound names, the same count ADR 0018 made by hand. Today Loupe passes `--dialect dsp-plugin` on the command line,
and the graph says so in `notEvaluated`. With the line, the choice is yours and recorded in your tree. 88 files still
get no role (most of `engine2/`, `h2/`, `reference/`, plus `verify`, `install`, `index.html`); a role proposal for
those is a separate brief, if you want one.

## What the other 45 unknowns turned out to be (no action needed)

At 1e6aae79 there were 47 `unknown` missing nodes. Loupe resolved 45 on its side:

- 37 are files inside your submodules: 36 `clap-wrapper` sources the build lists, and the `choc` header that
  `tools/bridge_utf8_check.cpp` includes as `../libs/choc/...`. They are now `external`, naming the submodule and its
  pinned commit; the included header is checked on disk in the checked-out submodule.
- 6 are VST3 SDK headers `tools/vst3_save_bench.cpp` includes through the FetchContent include path. The crawl now
  lists the fetched SDK's headers during configure (names only), so they resolve to the fetched files.
- 2 are quoted includes in files no compile reaches: `engine2/tests/suite.h` and `engine2/core/api/engine.h`. They are
  now found by a nearest-parent-directory search, marked as a guess worth auditing.

## The gate, for your information

`./verify full` on Loupe's side now re-runs everything on your terms (response-003): a local clone of a named commit
in Loupe's scratch, your declared configuration, the clone deleted afterwards, your private-name rule and the
machine-path pattern before anything leaves the scratch. The name list is read in place from your checkout and never
copied. At 1e6aae79 everything holds except the two FOUNDATIONS headers:

- two crawls are byte-identical, and the inventory closes;
- the leak check finds 0;
- every edge the old probe found is found again, or has a written reason pinned to the commit;
- 5 references to the root `CMakeLists.txt` that the crawl had missed are now found (`ROOT / "CMakeLists.txt"`).

Once the two lines are committed, Loupe will re-run the gate on that commit and send a notice.
