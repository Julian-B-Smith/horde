# b628-polyphony-audit — Polyphony is the one voice control, a preset load caps it at 8, and every row's mapping is audited

- **Queue item:** B628. The human played the installed build ("poly says 16 but I can hear that it's 4") and ruled by poll: "One control: Polyphony". Relayed by the lead with a full-row mapping audit (follows `traces/2026-11-05-b628-bridge-loads.md`).
- **Why:** the plug-in kept a separate Voices limit (a VoiceCap of 4) beside the engine's `voice.polyphony`, so the page showed 16 while 4 played. Changes:
  - **Voices is gone:** the VoiceCap, host parameter `0xB00002` (49 → 48 host parameters), its line in the saved state and `plugin.voices`. `plugin: {build}` stays.
  - **Old inputs still work:** an old page's `plugin` message is acknowledged and held, and a chunk with a `voices` line still loads, ignoring it.
  - **The plug-in's own safety rule:** a preset load asking for more than 8 voices (page, host Preset, saved state or the default at activation) gets 8, sent through the engine's Param path before the load completes, so the completing frame shows 8. On 1ad270a all 83 bank presets ask for 16.
  - **Hand-set values stand,** at any value, and a saved state keeps them.
  - **The AppKit fallback** turns its Voices menu into a Polyphony menu on the engine row (through the edit queue, not a host parameter), so it keeps the one voice control.
- **Evidence consulted:** the engine's manifest (`voice.polyphony`: Step, lo 1, 64 steps, default index 15); a probe of every bank preset's polyphony (83 of 83 ask for 16); origin/main's `tools/labharness/gui3_browser_check.mjs` (line 212 compares the page's Voices with the recording's `plugin.voices`); GUI 3's page from origin/main de0f664a.
- **Alternatives rejected:**
  - Adding `voice.polyphony` as a host parameter: the ruling's count is 48.
  - Testing "a preset at or below 8 keeps its own" on the bank: no such preset exists. The rule is held in bridge_test as a pure function (16 → 8; 8 and 6 kept), and the smoke test loads saved states with polyphony 6 and 12 to show a hand-set value stands.
  - Committing the re-recorded fixture now: it would turn main's `gui3_browser_check` red (see Open questions).
- **Verify:** `./verify fast` exit 0, with the committed fixture. `build.sh 1ad270a`:
  - bridge_test, smoke and window GREEN with the stand-in page, with GUI 3's page from origin/main (inlined sha256 f7839a8b…) and with the AppKit fallback.
  - Mapping audit: 317 of 317 rows read back exactly. The control, a key map shifted by one row, fails 317.
  - 12 voices sound at polyphony 12. The control, the old limit of 4 put back, sounds 4.
  - The `frames:` step reads RED, as it must: the fresh dump differs from the committed fixture only in frame 0's `voice.polyphony` (15/63 → 7/63) and `plugin` losing `voices`; every `observe` and the display file are unchanged.
- **Open questions:**
  1. **The re-recorded fixture is held back.** It is at `/tmp/claude-501/hep-loads/gui3-replay-rerecorded.json`, identical across two build directories. Main's `gui3_browser_check.mjs:212` reads RED on it ("replay: Voices reads 4, not undefined"), and that check runs in `./verify fast`. The GUI session's check needs `pl.voices !== undefined` before it compares. Their page's Voices stepper (`gui3.html` 2031–2033) now sends only held messages, so it should become Polyphony or go. Once their check is changed, commit the fixture (a human gate).
  2. The smoke log's header no longer prints a voices count.
