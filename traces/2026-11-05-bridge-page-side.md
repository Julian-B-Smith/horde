# 2026-11-05 — The bridge's page side: GUI 3 ready for the clean-room engine in the test plug-in (B628, GB26, GUI-46)

**What changed.**
- `docs/design/gui3/engine-stub.js`: the signed message schema (`checkMessage`, `MESSAGES`), `decode` of RC-8's frame, `NativeTransport` (`hordeNativeSend` out, `hordeNativeSnapshot` in) and `ReplayTransport` (recorded frames on the page's own frame time, acknowledging every seq). SyncTransport acknowledges `want`, `preset` and `plugin` and reports its `sides`.
- `docs/design/gui3/device.js`: `sides`, `want`, `presets`/`loadPreset`, `plugin`/`setVoices`, and the Specimen's feed from an engine frame (never from the stub's MOCK meters).
- `docs/design/gui3.html`: the transport chosen by what the page finds; the self-checks skipped on a bridge; `want` per page; the inert look (GUI-46); the bank's names in the PresetBar; a Voices stepper.
- `tools/gen_gui3_controls.py`: `manifest_hash` in `shell-data.js` by section 3's tuple, and `--manifest-lines`.
- `tools/gui3_inline.py`: the page as one file for the plug-in's build.
- Tests: `gui3_seam_check`'s `bridge` (schema, acks, decode, feed, the hash recomputed in JS with C's %.17g) with four plants; `gui3_browser_check` replays SYNTHETIC frames on the one-file page under `embed_file.py`'s policy, with a frozen replay as its control.

**Why.** The human: "build it now, I want it for sound testing"; by poll, "Dimmed, still visible". The lead's plan, `docs/proposals/gui-on-engine.md` (#1407), section 3, signed by both sessions.

**Evidence.**
- `gui3_browser_check` GREEN: the 83 self-checks and the pictures on the stub as before, and on the one-file page under its policy: no violation, every message with a seq, Main's `want` osc [1,2] with members and Osc 1's [1], the morph switch disabled, a rack row dimmed and `osc1.detune` live, the bank in the preset bar and its next button sending `preset` 1, Voices reading the frame's 6, and the feed's gates changing while a frozen replay shows one set.
- Plants, each turned it RED and was restored: a replay that never advances and no inert marking ("inert state wrong", "the feed's gates never changed"); the font `@import` let through ("blocked by the page policy: style-src-elem https://fonts.googleapis.com/…").
- `gui3_seam_check` GREEN with the `bridge` test; its four plants (any type admitted, a held message unacknowledged, a linear peak passed as dB, the feed read from the stub) are in its plant list.
- The hash: `shell-data.js`'s `manifest_hash` equals `--manifest-lines | shasum -a 256` and the JS recomputation.

**Not done.** The oscillator pictures still draw the page's sample swarm, not RC-8's members. The frames are synthetic until the lead's recorded fixture lands at `tools/labharness/fixtures/gui3-replay.json`, which the browser check uses when present. The inert look is in the shell with the master's token; the master's control state is next.

**Verify.** `./verify fast` on the commit this trace ships in.
