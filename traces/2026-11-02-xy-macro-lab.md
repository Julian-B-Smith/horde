# 2026-11-02 — XY and macro lab

**What changed.** `docs/design/xy-macro-lab.html`, new: four ways to pick the XY's two parameters (X1 to X4) and four ways a macro knob maps (K1 to K4), all on the real shell. The design index and ledger list it as exploring. ROADMAP rows GB1 (compact mixer), GB2 (Arp's button and page), GB3 (History page) hold the human's other three notes.

**Why.** The human, 2026-11-02: "after the overlap fix, we can do the XY and macro page". GUI-2 left the XY's picker undrawn and GUI-8 left the knobs without a Map button.

**Evidence consulted.** `specs/SPEC-INTENT-BUS.md` principles 1 and 2 and §3.1, §5; ADR-192; GUI-2; GUI-8; `src/gui/gui2.html` `renderHistory`; `docs/design/gui3/device.js` routes; the lab's self-check.

**What real use found.** Real clicks in the pane, after the self-check was green: a click on a macro's name never reached the name, because the knob captures the pointer on press. The synthetic click had passed. The lab acts on pointerdown now and its check uses it. Mapping also exposed the shell's macro knobs crushed to 6 px under MAP's inspector, fixed on the Main PR with a measure.

**Not checked.** A plugin host. The real macro law, per-corner bindings on the intent bus, isn't in the engine, so a mapping here stands for a binding.

**Verify.** See the PR.
