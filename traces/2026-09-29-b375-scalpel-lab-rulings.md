# b375-scalpel-lab-rulings — the default logo locked, the self-check on request, a full-cap policy, three heavy presets thinned

- **Queue item:** B375. I read the row verbatim from `origin/lead-records-149:ROADMAP.md`, with B310, B323, B368 and B372. The human, 2026-09-29:
  - "I love the drop/inset/hue gradient logo; let's lock it in."
  - On running the self-check only on request: "I agree with this."
  - On dense-chord overload: "Both of these make sense. Blocking new voices seems like a reasonable policy, though maybe we could include a toggle to test."
- **Why:** These rulings answer B372's ranked list and B323's open question (refuse or steal at the cap). The branch is based on #869's branch (`lab-scalpel-perf`), and #869 merges first.
- **Evidence consulted:**
  - the B372 trace (`traces/2026-09-29-b372-scalpel-lab-perf.md`) and its method;
  - the lab's E2 (logo), J/J2 (worklet shell, load loop), K (checkB317, checkB323, checkB334, checkPresets) and L (boot);
  - `docs/design/scalpel-horde-engine.js` (noteOn, tierPick, cull, renderPlain);
  - `reference/scalpel/prototype/razor-core.js` (read only: startVoice, noteOff, the ADSR);
  - `tools/labharness/{lab_load_check,lab_wheel_scroll_check,composed_engine_check,divergence_ledger_check,port_legacy_presets_check}.mjs` and `tools/patchspace/*` (for any reader of the lab);
  - `git grep` for `__b271` and `scalpel-interface-lab` across the tree.

## What changed (commit 1d28579)

1. **The default logo.**
   - `logoStyleLoad` returns `LOGO_DEFAULT` (DROP + INSET + HUE ↘, looked up by id) when nothing is stored or the id is unknown. It used to return index 0 (FLAT).
   - The cycle order, the stored choice and `?logo=` are unchanged.
   - checkB334's cleared-storage control now expects the default. It also requires that the default is not the stored GRADIENT, so the persistence reading still cannot coincide with the default.
   - checkB317's edge row failed under the new default. Its detector compares the stroke's mean colour with `logoEdgeColour()` over a flat fill, and the inset shadow and hue gradient change that colour. The row is pinned to FLAT, the style it was built on and measured before B375, and its style is restored afterwards.
   - This is the same measurement on the same style. It is not a relaxation. The new default's own passes are checked by checkB334's composite rows.
2. **Self-check on request.**
   - `bootLate` runs `runChecks` only with `?check=1`.
   - The audit line says "self-check: off (add ?check=1, or [run self-check])". The button runs `selfCheck()`, the boot's own sequence, through one delegated listener.
   - `window.__b271` is written only when `AUDIT.ran` is set.
   - The self-check had one side effect the page relied on: B293's guard that a preset failing its load check is not loaded. It is now `presetCheck(pr, i)`, run by `applyPreset` for a preset with no verdict yet. That takes a few ms, against 0.4–0.7 s for all 95.
   - `checkPresets` calls the same function with the same seeds, so its rows are unchanged.
3. **The full-cap policy.** It is an engine input, `capPolicy` (msg `{t:'capPolicy', n}`), in `ComposedEngine.noteOn`. It applies only when a note-on finds the cap full (`busy`) and tiers 1 and 2 find no releasing tail. So it covers exactly the case where B323 fell through to tier 3.
   - **0 REFUSE is the default.** It returns before `startVoice`, so it makes no random draws and touches no voice.
   - **1 STEAL** releases the oldest held voice, marks it `cull` (the 8 ms ramp in `renderPlain` frees it), and starts the note in a FREE pool slot. If there is no free slot (the pool is full of held voices), it is horde's full-pool tier 3, at once, as uncapped.
   - **2 REPLACE** is B323's behaviour as built, kept reachable and used by nothing.
   - With no cap (0), none of this runs. That covers every offline render and every parity and ledger gate.
   - This is **not a divergence from the oracle**: RazorCore has no cap, and the composed engine's voice law is horde's (B310). No ledger entry is needed.
   - **The lab side:**
     - a header chip, "full cap (test) REFUSE/STEAL" (`?steal=1` starts it on STEAL);
     - `capPolicyPush()` on a click and when the worklet node is made;
     - the load window posts `refused` and `stolen`, and the meter shows them.
4. **Heavy presets.**
   - `PRESET_THIN`, applied in `applyPreset` (bench presets only): Glass horde pad N 9→6, Undertone cathedral 7→6, Mirror-image spreads 7→6.
   - `reference/scalpel/data/presets.json` is protected and untouched. The preset row says so, and the Members knob restores the bench's N.
5. **New rows.**
   - `composed_engine_check`: 7 CAPPO rows, 3 of them must-fail controls. 147 → 154 rows, all green.
   - Page `checkB375`: 2 rows + 2 controls. The page self-check went from 120/120 (51 controls) to 124/124 (53 controls).

## Who loads the lab headless, and `?check=1`

No tracked tool depends on the in-page self-check, so none needed the flag:
- `lab_load_check.mjs` runs the scripts in a vm with no `fetch`. `CORE` never builds and `runChecks` returns at once, before and after.
- `lab_wheel_scroll_check.mjs` already stubs `runChecks` (`skip: ['runChecks']`). The run button sits in `#audit`, outside the `.scroll` it clicks through.
- `composed_engine_check`, `divergence_ledger_check`, `port_legacy_presets_check`, `listening_pass_check` and `tools/patchspace/*` read the engine or the lab's TEXT and never load the page.

`git grep __b271` outside the lab finds only traces. The recipes that read `__b271` are agents' session recipes, not tracked: the lead's must add `?check=1`.

## Measured

- **Load time, headless Chrome, M3, dpr 2** (navigation → first frame of the loop):
  - before: 14.4, 13.8, 13.8 s, with `__b271` at 120/120;
  - after, no flag: 0.23, 0.13, 0.11 s, with `__b271` undefined;
  - after with `?check=1`: 16.8 s, 124/124.
  - On demand, after 2.5 s of the frame loop running: the button gives 124/124 in 16.9 s.
- **Rows before vs after** (the `?check=1` page against a copy of HEAD's lab, served the same way):
  - Common rows are in the same order and all pass. There are 5 detail differences:
    - the logo's ink count (6540 → 7570 px, the default now has a drop shadow);
    - two Specimen/XY rows' sub-pixel numbers (k 12.130 → 11.933; marker y −24.13 → −24.62). This is a hypothesis: the page layout moved now that the 2,668 px results block is gone before boot. Both rows pass;
    - two timing numbers.
- **Budget, Node 24, the composed engine per 128-sample block** (budget 2.667 ms at 48 kHz). This is B372's method with the params captured from the page (`oracleParams(EFF)` after `applyPreset`), `mulberry32(0xB271)`, a held chord of 57/60/64/67/72, 100 warm-up blocks, then 1200 timed. Runs 1–3 interleaved before and after:

  | Preset | Before (5 voices) | After (5 voices) |
  |---|---|---|
  | Glass horde pad | N 9, 126 / 129 / 126 % | N 6, 85 / 82 / 80 % |
  | Undertone cathedral | N 7, 94 / 95 / 94 % | N 6, 85 / 80 / 80 % |
  | Mirror-image spreads | N 7, 90 / 97 / 92 % | N 6, 79 / 77 / 76 % |
  | Smear and strike (unchanged) | N 7, 94 / 89 / 96 % | 88 / 85 / 85 % |
  | Crushed bells (unchanged) | N 6, 86 / 87 / 87 % | 87 / 84 / 84 % |

  - The rule: thin where five voices ran over LOAD_HI (90%) in most runs, to the largest N that measured under it. An N sweep for Glass horde pad: N 8 109%, N 7 99%, N 6 81%, N 5 71%.
  - The machine drifts ±15% between sessions: B372 read Glass horde pad at 150% and Crushed bells at 99%.
- **The audible change:** fewer members is thinner. The chorus is narrower and less dense, and per-member spreads fall on fewer points. The Glass horde pad loses a third of its swarm.

## Alternatives rejected

- **Refusing even when a releasing tail could be taken.** B323's open question was about tier 3 ("tier 3 now"). Taking a tail at the cap keeps the count at the cap, and it is horde's ordinary law, so it stays.
- **A STEAL that fades the held voice without releasing it.** In sustain the ADSR pulls `env` back toward S every sample and fights the ramp. Releasing first makes the ramp exact against a released twin (7.8e-15).
- **Keeping `checkPresets` at boot** for B293's guard. It costs 0.4–0.7 s. The guard runs per preset instead.
- **Thinning Smear and strike and Crushed bells.** Their medians are under 90% here. That is left for a ruling.
- **Editing `presets.json`.** It is protected.

## Verify

- `./verify fast` on 1d28579: exit 0, GREEN. `.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"1d28579","ts":"2026-09-30T03:08:08Z"}`.
- `./verify full` runs on this trace's commit (the change plus this file). Its result is in the PR body, because recording it here would change the hash it verified.
- Chrome was not running during verify.

## Open questions

- REFUSE takes a releasing tail first. Is that the ruling's meaning of "blocking new voices", or should a full cap refuse even when a tail could be reused?
- Should Smear and strike (median 89%) and Crushed bells (B372: 99%) also be thinned? Both sit at the line on this machine.
- The in-page B375 rows check the chip's wiring and the table. The steal/refuse behaviour itself is checked in Node (`verify full`), and was not listened to.
- For horde 2 the cap stays a separate human decision (B323's lead note).
