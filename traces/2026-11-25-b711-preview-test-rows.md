# 2026-11-25 — B711: the test rows reach the page (the bridge carries the test filter's six and Scape's thirty)

**What changed** (`tools/engine-preview/` only):
- **`test_rows.h` (new):** one descriptor per test row, built from the same tables `params_get_info` reads, keyed `preview.filter.<name>`, `preview.scape.on` and `preview.scape.<Scape key>`.
- **`bridge_core`:** `TestKeys`; `set` and `gesture` resolve `preview.*` keys there, and an unknown one is refused in `refused.unknown`. The hello reply carries `hello.preview` with the value rule and the rows. That list sits outside the manifest's rows and outside its hash.
- **`preview_plugin.cpp`:** a page `set` on a test row becomes `Edit::Value` on its host id, through the existing `drain_edits`/`set_param` path, bracketed as begin, value, end unless a page gesture is open. Frames carry every test row on hello and on preset load, and the changed ones after that.
- **The plant `preview-plant-droptest`:** the page's test-row sets compiled out. `preview-plant-map`'s one-row shift now covers test keys too.
- **Tests:** `bridge_test` and `smoke` coverage, with the plants that must fail.
- **README:** the bridge section is new. The "In Live" notes are corrected.

**Why.** The human, 2026-11-25, couldn't switch Scape on in build 13. The plug-in has 356 host parameters, so Live opens an empty panel and adds a parameter only when it is touched in the plug-in's window. GUI 3's page never touches the test rows, and its FX page is a mock. The human chose by poll: "Wire it properly". The panel itself is the GUI session's (ADR-336), and it accepted the key shape and asked for (a) the descriptors in their own hello list and (b) values on hello and preset load; both are done. Its ask (c), plain values, is answered in the other direction: test rows travel as v in 0..1, as engine rows do (`docs/design/gui3/device.js`, `v_to_host` in `preview_plugin.cpp`), with host = min + v·(max − min) and a stepped row rounded to its option.

**A correction.** The README had told the human to pick the test rows from "the plug-in's parameter list" in Configure mode. Live has no such list: Configure learns a parameter only from a touch in the plug-in's window. The lead relayed that wrong instruction for build 13, so the human couldn't reach Scape at all. Both "In Live" notes now say the rows are unreachable until the page draws them.

**Evidence.**
- **The preview build:** `build.sh 6cd98b5` into a fresh output folder (label "engine 6cd98b5, build 2"; not installed). It read `bridge_test: GREEN (0 failed)`, `engine-preview smoke: GREEN (0 failed)` and `engine-preview gui: GREEN (0 failed)`, with 358 `ok` lines and 0 failures.
- **The audit:** all 36 rows set from the page reach the host as exactly [begin, value, end], read back, and return as the page's v. Both plants fail all 36.
- **Gestures:** a gesture-bracketed drag is heard as [begin, value, value, end].
- **Host automation to 6 s** returns as (6 − 0.2)/11.8.
- **Unknown keys** are refused and produce no host events.
- **The state chunk** for a page set equals the chunk for host automation of the same value.
- **Bit-identity:** with Scape off and the filter off, every preset is still bit-identical to the plants without them (B660, B710).
- **Unchanged:** no engine row, key, id or manifest hash.
- **Tolerance registry:** 0 changed (158 registered).
- **Review:** the implementation was by an Opus implementer. Two Opus reviewers took separate lenses (protocol and host path; oracle and plants) and reported no findings. The lead checked the value convention against `device.js` and `v_to_host` and corrected the README.

**Verify.** `./verify fast`: in the PR body.

**Open.**
- Build 14 is installed after the GUI session's Scape panel (GUI-n) lands.
- Fresh frame dumps now carry the 36 test rows in their first frame. The committed fixtures are from `1ad270a`, and re-recording them is a human gate.
