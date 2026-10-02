# b405-h2-engine-wire-rework — the critic's ACCEPT-WITH-NOTES on #895 at 89a6286

- **Queue item:** ROADMAP B405. The horde lead relayed the critic's notes M1, M2, M4 and L1–L3 on 2026-10-01. This
  is a new commit on `h2-engine-wire`, after `traces/2026-10-01-b405-h2-engine-wire.md`, which is not edited.
- **Why, note by note:**
  - **M1, bounded-and-finite is a NaN guard in practice.** The engine's output is `std::tanh(...)` (the end of
    `renderCall` in `h2/engine/engine.h`), so |x| ≤ 1 cannot fail on engine output.
    - The check's header, its RULE line, `docs/port/h2-engine.md` and `h2/README.md` now say "finite (a NaN/Inf
      guard); |x| ≤ 1 holds by construction after the output tanh".
    - BF's 2.0 half is labelled in the output and the docs as a test of the detector code only.
    - No whole-render threshold was added: A2 ruled against whole-render rules.
  - **M2, a silent SKIP after an upgrade.**
    - `FloorVerdict` gains `warn`: unjudged on a platform that HAS a pin. The check prints `WARNING: bit-exact
      floor not judged — key … ≠ pin …; re-measure and re-pin (L0072)` to stdout and stderr, and `verify` echoes
      it to stderr. The exit is unchanged; whether it should fail is the human's call.
    - FLOORKEY now asserts the warning through the same function. On darwin-arm64, another Node major and another
      compiler each warn. An unkeyed platform does not warn. The keyed cases do not warn.
  - **M4, two renders.**
    - The new `--full-from FILE` declares a rendered file full. A file cannot say it was rendered without `--only`
      (the header counts only what was selected), so the caller's flag is what declares it full, never the file.
    - A new STREAM row demands four things on any full stream: an END record; header count = END count =
      scenarios read; the three ring rows; every control's row. Otherwise the run is infra-red, and the FMA control
      exits 2.
    - `verify` renders once to `/tmp/h2stream.$$`, feeds both binaries, and removes every temp file on every path.
      The no-argument mode is kept: it was run standalone and is green.
    - TRUNC is the must-fire for the file path. An 8 MB cut (no END) must make the check exit 1 on its STREAM row
      and the FMA control exit 2. It does: the check reads 9 scenarios, all at parity, and is still RED.
    - Measured once, outside the gate: a file missing only its END line is red the same way (543 of 543 read).
  - **L1.** The out-of-sample note is recorded in the doc. Nudges 33, 75, 120 and 200 were re-run, and nudge 75
    reproduces the critic's 4.92e-8 at 384 frames on chord.
  - **L2.** X1-late's margin sweep is printed and not judged:

    | ε | rows red | largest max-abs |
    |---|---|---|
    | 1e-10 | 0 of 35 | 1.446e-7 |
    | 1e-9 (judged) | 4 of 35 | 1.446e-6 |
    | 1e-8 | 5 of 35 | 1.446e-5 |

    The response is linear in ε. The sweep's scale reaches the engine through `faultEps` on fault 14 only; X1
    (fault 13) is unchanged.
  - **L3.** `verify`'s green-path grep now shows the onset profiles, X1-late's red rows with their margins, the
    sweep, the ring's blind spot, STREAM, TRUNC and the floor warning.
- **Evidence consulted:** the critic's notes as relayed; `tools/h2_engine_render.mjs` (the `H2ENGINE 1 <n>` and
  `END <n>` lines count only the selected scenarios, hence `--full-from`); and runs in a session scratchpad:
  - one render fed to both binaries: check exit 0, FMA control exit 0 (FIRED);
  - the 8 MB cut: check exit 1, FMA control exit 2;
  - the no-END file: check exit 1, FMA control exit 2;
  - no arguments: exit 0;
  - nudges 33, 75, 120 and 200.
- **Alternatives rejected:**
  - **Treating any stream file that has an END as full.** A file rendered with `--only` also has an END, so that
    is ambiguous.
  - **Pinning the scenario count.** It rots every time a scenario is added. The control and ring rows' presence
    plus the END cross-count catch a subset or cut file instead.
  - **A TRUNC cut just before END inside the gate.** It costs a full replay of both binaries (~30 s). It was
    measured once and recorded, not gated.
  - **An env override for the floor key.** Rejected again: the in-process self-test covers the warning path.
- **Verify:** run SEQUENTIALLY on the committed head of this change (the commit that adds this trace): `./verify
  fast` and then `./verify full`. Their exits, `.harness/last-verify.json` and the wall time are recorded in PR
  #895's body and in the hand-back. A trace cannot carry its own commit's hash.
- **Open questions:**
  - Whether the floor warning should fail the gate (the human's).
  - The renderer's `SELF` render and its stale header are left for B406.
