# gui74-scape-preview-panel — Scape and the test filter on the FX page, live on the test plug-in's bridge

- **Queue item:** GB40 (GUI-74). It pairs with the lead's B711, PR #1525 (the bridge's `preview.*` keys).
- **Origin:** the human could not test Scape in build 13 and ruled "Wire it properly" (lead's poll, 2026-11-25). The lead's messages that day gave the split and the message shape. The panel's design was this session's poll: B, the filter card before Scape.
- **Evidence consulted:**
  - The lead's PR #1525: `tools/engine-preview/test_rows.h`, `filter_core.h`, and its README section "The test rows on the bridge".
  - Scape's parameter table and manifest at `tools/engine-preview/scape.pin`'s commit (read only): `core/src/reverb.cpp` kParams, `core/manifest.lock.json` (params, fixedControls), and `lab/reverb-lab-v0.html`'s sections.
  - The shell's FX page: Scape's rack card from `layout.json`, through `gridSection` and the bundle's `Cluster`.
  - The bundle's Knob gesture, Session, `FieldSlider` and `Companion`.
  - `docs/design/gui3/device.js` and `engine-stub.js` (`checkMessage`).
- **Verify:** `./verify fast`, exit code in the PR.

## Variants shown (mockups on the plug-in's own page, 980 × 720, stand-in bridge)
- **A:** wire Scape's existing card as is, with an On switch, plus a Test filter card.
- **B:** A, plus a test strip at the top of FX.
- **C:** Scape's card regrouped as Scape's lab groups it, in words.

The human chose B, and the filter in its own card before Scape's.

## Design choices in the build (why, not what)
- **The shell's one session, with a guard, not a second Session.** The bundle's Knob gesture is wired into the session: MAP clicks create mappings, Option-drags set depths, and corners are latched.
  - The first build gave the preview knobs a Session of their own. `gui3_seam_check`'s mappage test refuses a second `new H.Session(` (one MAP state for the shell). That gate is the human's to loosen, not mine, so the build changed instead.
  - Now a capture-phase guard on each preview control stops a press under MAP or with Option, and says why.
  - The backstop is the existing rule: the device's matrix refuses a route to a key that isn't a row, and attachMap drops it.
  - A press restores `S.selected`, so the morph's selected row never becomes a preview key.
  - The guard has no check of its own; the matrix's refusal is the held rule.
- **The Knob, not FieldSlider or PlainSlider.** The ruling is "Scape's card as is", with tiny knobs and abbreviations. A locked Knob (`p.locked`, `g` bound to the preview store), via `H.SecondKnob` (the controls generator allows one direct `H.Knob`, in mountControl), is the house face for a single value.
- **The device keeps the preview store** (`D.preview`, `previewValue`, `previewSet`, `previewGesture`). Sends share the device's one `seq` counter, so the bridge's acks stay ordered for the device's own stale-frame guard. A frame older than our last set, or one arriving mid-drag, doesn't overwrite the value: the same rule the device applies to its rows.
- **Gestures come from pointer events on the control's host (capture phase).** The Knob has no begin or end hook. `previewGesture('end')` is ignored unless that key's gesture is open, so a stray `lostpointercapture` sends nothing.
- **`checkMessage` admits `preview.*` in a `set` only.** It is the page's half of the bridge schema, and B711 extends the bridge's half. A `corner` never may, because these keys are never morphed.

## Oracle evidence
- **This branch:** `gui3_browser_check: GREEN … on the bridge's preview keys the FX page draws the test strip, the Test filter card before Scape's and Scape's card bound to them, a Decay drag is one gesture of preview sets, a frame repaints it and Undo stays off (controls: no hello.preview draws none of it, and an engine drag turns Undo on)`.
- **Main's `gui3.html`, `device.js` and `engine-stub.js` with the new check** (the must-fail run): RED, "the strip lacks preview.scape.on, …; the Test filter card holds 0 controls, not 5; …; Scape's card binds 0 of 29, with 29 rack control(s) left and 29 dimmed; no Decay knob to drag; …". The files were restored afterwards.
- **Faults caught while building:**
  - `gen_gui3_controls` refused keys written in the shell (`fx.scape`). Scape's card is now found through its layout group's controls.
  - `gui3_seam_check` refused the second Session; see above.
  - The page's own schema refused `set` on `preview.*` until `checkMessage` admitted it. The probe printed: the bridge refuses "a set naming preview.scape.decay".
  - The engine-drag control first aimed at Osc 1's Detune, which isn't drawn on the Osc group's first page (the dashboard), so it read "unread". It now takes the first live knob showing.

## Engine and FX device
- **Engine impact:** none; the preview keys are the lead's bridge's.
- **FX device:** none.

## Open
- Build 14 needs both this PR and #1525. The lead builds from both branches together.
- The strip's knob size (utility) and the filter's labels are readings for the human (GUI-74).
