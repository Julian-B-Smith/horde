# 2026-11-05 — One voice control: Polyphony (GUI-46 A2, B628)

**What changed.** The Voices stepper is gone from the Set page; a "The test plug-in" row with the build label is there instead. `D.setVoices` and the schema's `plugin {voices}` are removed: the seam check's `bridge` test now asserts the schema refuses it. The browser check no longer reads Voices, and its stand-in bridge sends `plugin {build}` only.

**Why.** The human heard 4 voices while the page showed Polyphony 16, and answered the lead's poll: "One control: Polyphony".

**Evidence.** `gui3_seam_check` GREEN, 106 plants. `gui3_browser_check` GREEN, the recorded frames included.

**Verify.** `./verify fast` on the commit this trace ships in.
