# 2026-11-03 — The C1 console in the master and on Mix (GB1, GUI-14, GUI-16, GUI-18)

**What changed.** The master's new `ChainConsole` (`components/bundle.js`, `bundle.css`, `ChainConsole/README.md` and `preview.html`), published as `1793767794-9f75` and re-vendored. `docs/design/gui3.html`: Mix mounts it in place of the Channel strip, Gain staging and Chain line, with mock flow controls outside the window and self-check 4n. `gui3/layout.json`'s note, the port tab, the ledger, GUI-18, GB1. `compact-mixer-lab.html` retired.

**Why.** The human approved C1 and its flow picture, then, 2026-11-03: "Yes, port C1".

**Evidence consulted.** `docs/design/compact-mixer-lab.html`, ported rather than redrawn: its flow, its picture, its strips and its CSS. The lab's checks are rebuilt in 4n on the shell's own console.

**Not checked.** A plugin host. How it sounds: there's no sound, and every level past the sources is the flow's model, not a measurement.

**Verify.** See the PR body.
