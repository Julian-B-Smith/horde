# b632-pitch-automation-scope — the test plug-in gains pitch bend, the bank's glide, every row automatable and a real output tap

- **Queue item:** B632 (ADR-370, the human: "yes to all ... start everything").
- **Why:**
  - **Pitch bend and the bank's glide:** a stand-in for the pitch system (RC-3) until B504.
    - **Bend:** MIDI pitch bend (CLAP's MIDI dialect) at a fixed ±2 semitones, 14 bits, the extremes reaching exactly ±2.
    - **Glide:** each preset's B.1 `glide`/`glideAlways`, embedded by `gen_bank`. It is played by 10.3's law exactly as `tests/wasm/item15b.mjs` writes it for the renders: `ln f(t) = ln f + (ln from − ln f)·e^(−3t/T)`, from the most recently started note at the pitch it sounds then, when a note is held or always with `glideAlways`, a chord note by note.
    - **End of a run:** when its float value equals the note's own float pitch, where one exact NotePitch at f takes over. item 15b ends its double runs within 1e-9 of ln f, which the float stream can't resolve. The first version used that 1e-9 literally; `tolerance_registry_check` flagged it as an unregistered tolerance. Rather than register it (the human's call) or hide it in a named constant (a blind spot the check names), the rule became an exact comparison with no threshold.
    - **Delivery:** the plug-in takes over a note's pitch only once it glides or is bent, folding its tuning expression in from then on. A moving pitch goes in through IF-2's audio-rate stream (float32). One NotePitch at the note-on sample carries the glide's first value, so the voice is set up at its start pitch. Per-sample events, which the offline render uses, would have cut the engine into one-sample segments for each glide's ~0.4 s, a CPU cost the human would hear.
  - **Every non-reserved engine row a host parameter:** 317 plus Preset, each under its engine id and key, in manifest order. `kExposedKeys` is deleted.
  - **GN-4's output tap:** a lock-free ring of (L + R) / 2 written after the output guard, zeros while silence is skipped. With `want.summed`, frames carry `observe.summed = {rate, samples}`: the newest 2048 samples, every 16th, written to nine digits so a float reads back exactly.
- **Evidence consulted:**
  - **Spec:** SPEC-ENGINE 10.3, IF-2 and RC-3 (the builder's copy, read only).
  - **Engine at 1ad270a:** `tests/wasm/item15b.mjs` and `data/phrases/README.md` "Glide" (the law's generator); `core/voice/allocator.cpp` and `voice.cpp` (the stream path; `Note::hz`, a set pitch overriding the tuning expression).
  - **Bank:** all 83 presets glide (0.035 to 0.12 s); none has `glideAlways`.
  - **Plan:** `docs/proposals/gui-on-engine.md` section 3 on main has no scope shape; the page's stub mocks `observe.sum`.
- **Alternatives rejected:**
  - Per-sample NotePitch events: CPU, see above.
  - Keeping item 15b's 1e-9 as a literal or a named constant: see End of a run above.
  - Relaxing the smoke test's bit-exact comparison when the chord began to glide: that would weaken a gate. Instead the reference phrase carries the same glide, generated in the test from item 15b's rule, with run values rounded through float as the stream carries them. All five presets stay bit-identical.
  - Smoothing the bend, a bend range setting, a travel law: none is ruled; they are B504's.
  - Re-recording the replay fixture: the lead's instruction, it needs the human's OK.
- **Verify:** `./verify fast` and `build.sh 1ad270a` (stand-in page, and GUI 3's page from origin/main 5f16cf60, inlined sha256 514cfb6f…): see the report. bridge_test, smoke and window GREEN. The `frames:` step is RED by design until the recording is re-approved.
- **Open questions:**
  1. **The recording changes:** only `observe`, in all 30 frames (phases, psi, meter peaks), because Trance jitter's recorded chord now glides notes 2 to 4. `params`, `presets`, `plugin` and `gui3-display.json` are unchanged. Re-recording needs the human's OK.
  2. **`observe.summed` and `want.summed` are new.** The GUI session must match them; the page's mock reads `observe.sum = {scope, bands}`.
  3. **Bend** follows MIDI channels; MPE member channels would want a wider range, which is B504's per-patch bend range.
  4. **Played pitch only:** the bend check holds the pitch input (`played_pitch`) exactly. The sounding pitch is reported, not held, since gravity can move it.
