# A test plug-in of the new engine for Live (ADR-366, B624)

**What changed.** New `tools/engine-preview/`: the shell, its window, a bank generator, a smoke test, a window test, a build script and a README. DECISIONS (ADR-366), ROADMAP (B624, B625) and the design ledger. The main build, `./verify` and every plug-in identity are unchanged. No engine code or test.

**Why.** The human ruled it: "yes, build the test plug-in after PF-3". The engine had never been played in a host.

**Evidence consulted.** The engine's API and bank harness at the builder's `be7105c`, read in its repo. Live's manual on plug-in panels (up to 64 parameters). The smoke test's output and `auval` on the installed bundles. The builder's PF-3 report (`a02d123`).

**Verify.** `./verify fast` on this tree; see the PR.
