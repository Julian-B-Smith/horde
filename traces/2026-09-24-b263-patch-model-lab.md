# b263-patch-model-lab — the patch model, pared down to a lab that proves it simple and bounded

- **Queue item:** B263. The row is carried in PR #752 (`lead-records-88`) and was dispatched 2026-09-24 by the horde lead. The human, verbatim, on reading the model: "I like this. Let's do a pared down lab prototype for it." Their standing constraint: "I want to simplify everything while preserving the element of discovery that comes with the morph system."
- **Why:** The accepted model has five rules: values per corner, shared lists that only grow, a rack lineup per side, mod-bank zones, and locks. It needed a working surface on which each rule is visible and checked before it becomes an ADR. The lab is that surface, kept small enough that every consequence fits on one screen.

## What was built

`docs/design/patch-model-lab.html` is a new, self-contained file. It carries `lab-review` "B263 · 2026-09-24" and a `.tagline`.

- **Scale.** The lab has:
  - one oscillator with five values (on, wave [stepped, 4], pitch, detune, level);
  - three module types (DRIVE 2 values, FILTER 2, DELAY 3) in 2-slot racks;
  - an 8-slot bank in four zones (source 3, rack A 2, rack B 2, global 1);
  - an 8-route list.

  Presets:
  - three oscillator presets (Glass, Razor, Chip), each with 1–2 modulators and routes;
  - three rack presets (Grit, Space, Crunch) plus an `empty` one;
  - one global part preset, used only inside patches;
  - two patches: Duet (two racks) and Solo (one rack).

  The brief suggested this scale, and it was kept as given. The route cap of 8 makes the full-list refusal reachable in two clicks: Duet, then Chip → C.
- **Model / page split.** Script block 1 is pure. Every button goes through one of four functions (`loadPart`, `loadPatch`, `setLock`, `prune`), and the self-check calls the same four. Patches are recorded `loadPart` calls, so the model has one write path. Atomicity means "commit the clone only on success". The `atomic=false` flag exists only so the self-check can build the naive loader from the same code as its control.
- **Audible.** POWER plays one WebAudio voice: 3 detuned oscillators → both racks (stock nodes standing in for the modules) → an equal-power X crossfade → a global tilt shelf pair → master → analyser. A lineup change rebuilds only that side's chain.
- **Cost.** The readout uses B262's MEASURED C++ worst-case numbers for the modules of the same name (Drive 0.037, Filter 0.009, Delay 0.037 % of a core per slot at 44.1 kHz). It is labelled as not a measurement of the page's WebAudio nodes. Duet costs 0.092 %, Solo 0.074 %, and any patch has a ceiling of 0.148 %.
- **Colour roles** (ADR-116):
  - `--value` for authored cells and the puck;
  - `--physics` for modulator motion, the modulated live tick and the crossfade gains;
  - `--meter` / `--scr-meter` for the scope and the cost trace;
  - `--alarm` for CLIP only.

  A refusal is `--caution`, because it is atomic and loses nothing. The tokens are copied from `src/gui/gui2.html`, like the station lab's copy.

## Self-check (runs at load; also run headless via a scratch probe that slices block 1)

Each of the 10 claims HOLDS, and each planted must-fail control is CAUGHT:

1. **Osc → one corner.** Control: a loader that writes all four.
2. **Osc → ALL.** Control: a loader that writes only A.
3. **New routes read 0 in the other corners; a route the corner no longer uses reads 0 there.** Control: a planted depth of 0.1 in C.
4. **Rack → one side, nothing else changes** (projection of everything outside the side is byte-identical). Control: a loader that also swaps side A.
5. **Full list refuses with a message and the state is byte-identical.** Cases: the route list (Duet + Chip → C: "7/8 in use… needs 2 new routes and 1 is free") and the zone (a synthetic 4-LFO preset into the 3-slot source zone). Control: the naive loader that writes until the cap.
6. **Zones never collide.** 400 seeded operations (mulberry32 0xB263) with 15 full-list refusals along the way. Control: a rack-A LFO planted in source slot M2.
7. **Lock holds across a 33×33 sweep with the corners planted apart.** Control: the same corners with the lock flag cleared (moves by 0.800).
8. **A load skips a locked value and says "kept 1 locked value"** (a refused load does not count). Control: the same load unlocked.
9. **Equal-power X crossfade** (power error 2.2e-16). Control: a linear fade (off by 0.500).
10. **Cost constant across 1089 pad positions**, 2 racks for Duet and 1 for Solo. Control: a plan that skips the silent rack.

**Mutation check on the real side** (scratch, not committed): eight model mutations each turned their own claim red:

| mutation | claim(s) turned red |
|---|---|
| non-atomic default | 5 |
| new routes at 0.5 | 3 |
| bank-wide allocation | 4, 6 |
| lock ignored by morph | 7 |
| loads write through locks | 8 |
| plan skips the silent rack | 10 |
| linear fade | 9 |
| osc writes every corner | 1, 3 |

So the claims are not satisfied by construction alone (L0-style detector-shares-assumption check, per the user memory note).

## Evidence consulted

- The ROADMAP B263 and B262 rows on `origin/lead-records-88`.
- DECISIONS.md ADR-109: exempt writes the live value into all four corners, and the weighted edit.
- `docs/design/morph-editor-lab.html:1030`: the bilinear weights. `:818ff`: stepped params "resolve atomically".
- `docs/design/station-page-lab.html`: the model lab for tokens, idiom and `?theme=dark`.
- `src/gui/gui2.html:26-212`: tokens.
- `tools/labharness/lab_load_check.mjs` and `tools/gen_lab_index.py`: the tagline regex needs a `<div>`, and only the first 260 characters are used.

## Alternatives rejected

- **A one-rack patch that spans both sides.** It would add a third lineup state. An empty side is simply a rack with no modules, so the lab uses that (open question 5).
- **A new modulator slot per load.** It would make the lists unbounded and cross-fade two static LFOs instead of morphing one. The lab merges by (kind, ordinal) instead (open question 3).
- **Automatic pruning.** It would drop a route the moment it reaches 0 in the last corner. The lab uses an explicit PRUNE (open question 4).
- **Partial loads on a full list.** They are the refusal that loses data. The lab refuses the whole load (open question 8).

## Open questions (in the lab, each with a recommendation)

1. Rack swap vs that side's corner values.
2. Zone sizing.
3. Merge vs claim for modulators.
4. How lists shrink.
5. One-rack patches.
6. Locks vs preset loads.
7. Modulation vs locks.
8. Atomic vs partial loads.
9. A stale modulator rate in a corner that does not use the slot.
10. Routes between parts: only the global zone should cross.

## Verify

`./verify fast` is run on the committed hash; the result is recorded in the PR body and the report back. The trace is written before that commit, so its own hash cannot appear here.

## Screenshots

These are in the session scratchpad only (not committed): `patch-model-light.png`, `patch-model-dark.png`. Headless Chrome at 1600×2200 with `?demo=refuse`, served from this worktree by `tools/serve_labs.py 8263`. Headless Chrome hung at exit, as recorded for the station lab. The first re-shoot silently failed on the stale profile lock (exit 21, no request reached the server). Using a fresh profile dir fixed it; the server log confirms both GETs.
