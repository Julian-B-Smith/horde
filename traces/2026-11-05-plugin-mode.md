# 2026-11-05 — GUI 3's plug-in mode, hello, and adopting the engine's sound (B628, GB26, GUI-46 A1)

**What changed.**
- **Plug-in mode** (`html.plugin`, set by a head script when `hordeNativeSend` exists): the lab chrome is hidden and the 980 × 720 device sits at the view's top-left.
- **`hello`** is the first message. The adapter on a bridge sends nothing before it, and adopts the answer, every engine row, as a load with a fresh history. The mismatch banner and the build label read the bridge's answer.
- **The manifest hash's integer-option rule**, the lead's ruling: the page's hash is the engine's, `ca64bf73…`.
- **The preset bar** follows `presets.index`, and the Voices stepper is mounted on its own host (it had landed on the build label).
- **The replay check** asserts only what a recording holds, and the preset by its own index. A new plug-in-mode run stands in for the bridge.
- **The carpet and voice map caption** says which is the engine's.

**Why.** The lead's six items from its implementer's run of the page in the plug-in's window (B628). The adoption is this session's finding: the page pushed its defaults at load, which on the real engine would have overwritten the preset the plug-in had loaded.

**Evidence.**
- `gui3_seam_check` GREEN, 106 plants. New ones: a bridge adapter that pushes its defaults, and one that never adopts (it read green until the test also required the corners, which is how a load differs from automation).
- `gui3_browser_check` GREEN, including "in plug-in mode the 980 x 720 device fills the view's top-left with no lab chrome, says hello with its hash, and shows and clears a mismatch". Plant: no plugin class read RED ("the lab chrome still shows", "the device starts at 146, 349").
- The hash: `--manifest-lines | shasum -a 256` = `ca64bf73f67efe067c87cfd739a2d4d007bfa9cd72c29d314913b5879f38fd29`, the engine's. With the default as a value instead it would be `0add9b8e…`, so the index reading is the one that matches.

**Verify.** `./verify fast` on the commit this trace ships in.
