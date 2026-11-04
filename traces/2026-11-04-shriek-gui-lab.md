# 2026-11-04 — Shriek's full GUI, in a lab (GB14, GUI-23)

**What changed.** New `docs/design/shriek-gui-lab.html` and its ledger entry. The design index waits until the open lab PRs merge, so they don't conflict on it.

**Why.** The human: "Let's spin up proper full Sluice and Shriek GUI labs that incorporate these dynamic wordmarks and coordinate functionality with the Sluice and Shriek agents."

**Evidence consulted.** `docs/design/gui3/vendor/shriek.manifest.lock.json`; ADR-302 rulings 3 and 4, ADR-273, ADR-170, ADR-169 A5, ADR-251, ADR-300, ADR-332, ADR-343, ADR-351; B373, B453, B563; the master's `screens.md`; Shriek's tree, read only. Presets, factory patches and curve laws come from horde's own tracked `docs/design/maw-horde-lab.html`, compared read-only against Shriek's. The automaton's code is cleared to be public (the human, 2026-11-04: "Yes, it can go public").

**Rejected.** A DOM-drawn glass: a 1-bit framebuffer holds two colours by construction, and every check can read it without a browser.

**Verify.** `./verify fast`, on the commit this trace ships in. In a real browser: 26 of 26 self-checks and no console errors; a real click on the glass's soft key changed the page. GUI-21's rules were checked by rendering the JS core in 73 states: one known exception, an off stage's Noise curve moves the output when auto-gain is above 0, which is a question for Shriek.

**Not checked.** Every page by hand, the x3 scale, and the title's speed on displays other than 60 Hz.
