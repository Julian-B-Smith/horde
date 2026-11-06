# A plan for GUI 3 on the clean-room engine (B628)

**What changed.** New `docs/proposals/gui-on-engine.md` and a ROADMAP row. Nothing is built.

**Why.** The human asked whether the engine should run under a GUI mock-up during the build, then: "yes, plan the GUI-on-engine build with the GUI session".

**Evidence consulted.** The GUI session's answers of 2026-11-05: `engine-stub.js` TRANSPORT, its GN mapping, the page's frame sources and rates, its inert mechanisms, and the old-engine risks. The engine's API at `1ad270a` (`tests/harness/horde_engine.h`: the manifest, `param_v`, `read_gui_snapshot`, `observe_voice`). `tools/engine-preview` (B624). `docs/proposals/h2-shell-acceptance.md` (B398).

**Verify.** `./verify fast` on this tree; see the PR.
