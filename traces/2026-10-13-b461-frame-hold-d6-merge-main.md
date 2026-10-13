# b461-frame-hold-d6-merge-main — D6 `frameHold`: main's Breathing pad re-pin merged in, the self-digests regenerated on main's

- **Queue item:** ROADMAP B461 (draft PR #1072). Round 3, dispatched by the lead after the human merged PR #1074 (ROADMAP B462, ADR-217 with A2). It continues `traces/2026-10-12-b461-frame-hold-d6-rework2.md`. No law change.
- **Why:** main now holds the bank's Breathing pad at detune 0 and K -0.3, with its pins, the harness copy and the three Breathing pad self-digest rows re-pinned. This branch touches the same two self-digest files and the same three documents, so it had to take main in and regenerate on top. The branch is pushed, so main was MERGED, not rebased onto.

## The merge (`origin/main` at `68e0227`)

Four files conflicted; everything else merged by itself, and `reference/scalpel/data/presets.json`, `h2/harness/patches.json`, `tools/h2_scalpel_render.mjs` and `docs/port/scalpel-phase-1a.md` are main's, untouched.

| file | resolution |
|---|---|
| `h2/README.md` (the status row) | this branch's row, with main's one change in it: the bank's pin `44b48d72…` → `45c4f1d5cff97922b88b1fab778eccca3d511bcd`. Main's file differs from the merge base in that pin and nothing else (compared byte for byte). |
| `docs/port/h2-engine.md` (two rows of the pin table) | main's bank pin; this branch's pin of the lab page (`65d587e9…`). |
| the two `h2/engine/selfdigest.*.txt` | not merged by hand: main's version of each file was taken as the base, and the self-digest tool wrote this branch's rows on top (`--repin`). |

`docs/armor/goldens.json` merged by itself and carries both sides' facts.

## The self-digest tool's lines

- Regenerating on main's two files, each build: `0 changed, 22 added, 0 removed, 543 unchanged`.
- Judged against main's own two files with no re-pin (the branch's then restored byte for byte), each build: `543 of 565 scenarios bit-identical`, 22 `added`, none changed, none removed.
- The three Breathing pad rows are main's new digests: `af73b143736abc8f` (chord), `8a3d9b2e9c304fbf` (repeat), `9dac2ff2aa5ee0b7` (arp).
- New `TOTAL`: `5b3a91ffac2f6590 565`.

## What the bank edit did to this branch's rows and check: nothing

- **The 22 `C/D6` parity rows:** their digests are round 2's byte for byte, in both builds (the 22 lines compared). None is built on Breathing pad. Against round 2's reference the regenerated file differs in exactly the bank's ORACLE line, the three Breathing pad rows and `TOTAL`.
- **`tools/h2_frame_hold_check.mjs`:** its whole output is byte-identical to round 2's, 48 lines: every JUMP, CONTROL, B460 NOTE, ATTRIB, LIVENESS and LOCKED line. It pins `detune` 14 and `K` 0.45 over `ledger/breathing-pad` on the pad jobs and overrides both on the LOCKED jobs, so the harness copy's new values never reach it. That is what the pinning was for.
- Parity on the merged tree: 562 of 565, the 3 ring rows on their onset window, 0 red; mean bit-exact 36.49%.

## Also

- Three statements the merge made untrue are corrected: `h2/README.md`'s "Last verified" line, and the two places that said ADR-217's bank edit was still to land (`docs/port/h2-engine.md`, a comment in `tools/h2_engine_render.mjs`).

- **Evidence consulted:** the lead's round-3 brief; `git log bded5c7..origin/main`; main's and the merge base's `h2/README.md`.
- **Alternatives rejected:** rebasing (the branch is pushed); merging the self-digest rows by hand (the tool is the only writer of those files).
- **Verify:** `./verify fast` and `./verify full` on the merge commit's staged tree; exit codes and `.harness/last-verify.json` are in the hand-back. Pushed only on `full` exit 0.
- **Open questions:** unchanged from round 2: what the frame does after a crossing (with the human); B460 owns the pad-as-shipped rows; the lab toggle has not been clicked in a browser.
