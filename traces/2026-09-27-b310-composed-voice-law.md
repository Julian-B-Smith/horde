# b310-composed-voice-law — a repeated note's first release keeps ringing (horde's voice law in the composed engine)

- **Queue item:** B310 (records PR #798, branch `lead-records-115`), dispatched by the horde lead 2026-09-27. The human: "when I play the same note twice in a row, the release of the first note doesn't continue and instead the note gets stolen".
- **Why:** The composed engine inherited RazorCore's poly `noteOn`, which reuses any active voice holding the same note, releasing voices included (`reference/scalpel/prototype/razor-core.js:372`). horde's allocator (ADR-083, `src/swarm_core.h` `alloc()`) has no same-note reuse. The voice model is horde's, so horde's law wins. The oracle is protected, so the law is an override in the engine.
- **What landed:**
  - `docs/design/scalpel-horde-engine.js`: `ComposedEngine.noteOn` overrides the poly path only. `d.polyMode` still delegates to RazorCore (mono/legato unchanged). Over RazorCore's pool (`voices[0..d.poly-1]`) it takes:
    1. a FREE slot, `!v.gate && v.env < 1e-3`, oldest `v.age` first;
    2. else a RELEASING tail (`!v.gate`), lowest `v.env` first, age as the tiebreak;
    3. else the oldest held voice.
    "Faded" is RazorCore's ADSR `v.env` (`razor-core.js:777-779`, the voice's output gain `v.env*v.vel*norm`, `:862`), tested at horde's 1e-3. RazorCore's `!active` (env < 1e-4) is a subset of it. Every allocated slot runs `startVoice(…, fresh = true, retrig = true)`, then RazorCore's tail (`:376-379`) unchanged. The header's composition list gains a "voice law (B310)" line.
  - Note-off is inherited unchanged: RazorCore releases every active voice for the note that is still gated (`razor-core.js:433`). That is horde's rule too: `SwarmCore::noteOff` releases every gated voice matching the key, and the shell releases by key (`hypersaw_clap.cpp` `noteOffAll`).
  - `tools/labharness/composed_engine_check.mjs`: a VOICE section with 11 rows (VL, VL1, VL2, VL3, and the VLc/VL2c controls). The header documents it. The section is already wired in `verify full` through the existing block.
- **Evidence (verified, `node tools/labharness/composed_engine_check.mjs`, exit 0, 71 rows):**
  - **VL repeat:** A on, off at 0.2 s, A again at 0.3 s.
    - At 0.5 s there are 2 active voices. Slot 0 is releasing (gate false, env 1.18e-2); slot 1 is gated.
    - The first voice's env matches the same A released alone: max|Δ| 0 over 188 blocks.
    - **Control:** RazorCore's own law, through the same detector, leaves 1 active voice with slot 0 re-gated.
  - **VL note-off:** a second A-off releases slot 1 (stage 4, env 0.214) and slot 0's tail is still active (env 1.18e-2). With A held twice and one off, 2 voices hold A and 0 are still gated.
  - **VL1 tier 1:** the tails of notes 60 and 61 read 6.69e-4 and 3.28e-4. Both are faded and both are still active, and the older one is louder. Slot 0 (the oldest faded) is taken.
  - **VL2 tier 2:** held notes 60-62, and tails 63/64/65 at env 0.308/0.175/0.100. Note 70 takes slot 5 (65, the quietest and the YOUNGEST). The held notes are kept. The stolen slot starts fresh: max|φ_S − ½| goes from 0.3956 to 0, and the swarm glide re-snaps. **Control:** the oracle's law steals slot 0, which holds 60, a held note.
  - **VL3 tier 3:** all 6 slots gated, with slot 0 re-struck as 66, so the oldest held voice is slot 1 and not the first index. Note 70 takes slot 1 (61, age 2). The stolen slot starts fresh (0.4843 → 0).
  - **O1, O2, O3, DET and API unchanged:** the check was run with the origin/main engine (a scratch copy of the tree) and with this engine. Every pre-existing row prints byte-identical text, all Δ 0 as before. Why: they strike one note, or distinct notes onto an empty pool, where both laws pick slots 0, 1, … in order.
  - **Planted faults** (scratch copies, not committed): 8 of 8 caught.

    | Fault | Caught by |
    |---|---|
    | tier 2 takes the oldest tail | VL2 |
    | tier 2 skipped | VL2, VL2c |
    | tier 3 in index order | VL3 |
    | tier 3 takes the newest | VL3 |
    | same-note reuse restored | 4 VL rows |
    | whole override dropped | 6 rows |
    | tier 1 tests `!active` | VL1 |
    | `fresh = !active` on a steal | VL2, VL3 |
- **Alternatives rejected:**
  - Note-off to the MOST RECENT voice for the note: it diverges from horde's by-key release. A host that merges two offs into one would also strand a stuck note.
  - A selectable voice-law switch (`this.voiceLaw`): the control is RazorCore itself, so no new knob is needed.
- **Verify:** see the PR body for `./verify fast` and `./verify full` on the committed hash (`.harness/last-verify.json`).
- **Open questions:**
  - A stolen ACTIVE slot keeps RazorCore's current `v.env` and attacks from there, because `startVoice` zeroes env only when `!active`. horde's `initVoice` (`swarm_core.h:622`) does not touch `env` either, so the two agree. The steal-click behaviour has not been measured.
  - `docs/design/composed-engine-check.html` plays RazorCore (A) against composed (B) live, so a repeated note now differs audibly between A and B. That is by design, and nothing there asserts the old law.
