# b329-sluice-lab-spec-rebuild — the Sluice lab rebuilt to Sluice's own architecture spec, reading it in place

- **Queue item:** B329, which follows B321. Both rows were read verbatim from `origin/lead-records-130:ROADMAP.md` (records PR #824), with B319, B320 and B328 beside them. Dispatched by the horde lead on 2026-09-28.
- **Why:** The human reviewed B319 and found that "a lot of what's in this aesthetics lab doesn't fit the tool as it currently exists". Sluice then wrote the architecture spec of itself as horde will host it (their response seq 7). This change rebuilds the lab around that spec.
- **The hold:** the human's hold says nothing of Sluice's may be committed to horde's public tree. So the lab reads Sluice's spec and manifest at runtime, in place, through the gitignored link `local/sluice`.

## What changed

### `docs/design/sluice-horde-lab.html`

The file was rebuilt. Its `lab-review` meta is `B319 + B329 · 2026-09-28`.

**Where everything comes from.** The committed file names no Sluice module or parameter. It has no ranges, labels, laws, presets or macros of Sluice's. At load it walks the manifest by its schema (the field names) and builds everything from it:
- every module card, parameter row, bus entry and modulation / filter / patch-level group;
- the taper, morph law, sync kind, and Time and Tune roles of each entry;
- the global controls, from the manifest plus the spec's §6.1 table;
- the division list and the macro slot count.

It also reads, from the spec's markdown:
- the version and the section titles;
- the §6.1 control table;
- the §13 open items.

**The engine.** The engine is still horde's v0.1 NETWORK reference, sliced and called unedited. It carries the label "audio: horde's v0.1 reference, not Sluice's netcore — see B328".

**What v0.1 can voice is measured, not asserted.** Every manifest control is moved between two values and rendered twice. If not one sample changes, the control is drawn with a "NOT VOICED IN v0.1" badge.

**The faces (spec §11):**
- **Play** is the default face. It has:
  - randomize patch and randomize params, the Randomness knob, the seed, undo and redo;
  - the library: each patch and the snapshots it holds, save snapshot, rename, revert, delete, export and import, and a per-snapshot button for each corner;
  - the XY over four snapshots of one patch;
  - the Sluice-global controls with their §11.1 readouts;
  - macros 1–8, bound by order and carrying the patch's label.
- **Rack** is the machinery. It has:
  - a strip that holds every Play control;
  - a network view lit by the rendered taps;
  - the loop and post chain cards: enable, move, duplicate (mirroring macro bindings), remove, a tri-state lock, per-param SYNC with a division, and "+ add";
  - the feedback bus, modulation, filters, the patch mix and output (with normalize), and the macro editor.
- **Scope** has:
  - the live spectrum;
  - the offline impulse response;
  - the follower/gate trace, marked not voiced.

**The rules:**
- **One-patch rule (§9).** A corner may only take a snapshot of the same structure; anything else is refused. The default is forbid, as Sluice recommends, pending the human (§13 #5). Structural edits reach all four corners.
- **Morph.** The morph blends each number by the manifest's own law. Each discrete value snaps to the closest corner.
- **Structural changes (§6.3)** go through the renderer: a new engine and a linear 20 ms crossfade, with no tail carried over. A numeric change stays in place.
- **Resolution (§8)** happens on the message side: tempo sync and Time resolve first, Tune next, the dry/wet mix last. With Sync on, Tune leaves synced times alone.
- **Bypass** is §13 #3 and still open. Hard bypass is the default and spill-over can be switched on.

**Kept from B319 / B320:**
- the one renderer and its worklet route (L0064);
- the taps;
- the chassis;
- B320's `wantBacking` / `ctx2`, and the K10 check, now C15.

**The screen.** The screen is neutral chassis tokens, because B322 is still choosing the screen aesthetic.

**Removed, because the hold now forbids it.** B319 committed data copied from Sluice:
- the preset macros (`SLUICE_MACROS`, copied from Sluice's lab);
- the generator ranges (`RANDPATCH`, from their `random.h`);
- a line-for-line port of their `fz.h`.

All three are gone. The randomiser, the name generator and the macro author are now the lab's own. The macro author follows the spec's §7 authoring rule, reading the time and frequency roles from the manifest.

**The degraded state.** Without the link, the page builds nothing Sluice-shaped. It shows a "Sluice spec not available on this machine" card and runs only C14.

### `tools/labs_preview.sh`

After the preview worktree is created, the script links `local/sluice` in the preview to the main checkout's link, when that link exists.
- The main checkout is computed from `git rev-parse --git-common-dir`, so no machine path is committed.
- `local/` is gitignored.
- The step is idempotent: the worktree is rebuilt on every run, and `ln -sfn` replaces the link. A scratch test ran the extracted block twice.

## Self-checks

The page runs 18 self-checks, each with a must-fail control. All 18 pass in real time with the spec present. Measured in headless Chrome through a scratch wrapper page that holds the load event open for 20 s. C18 needs real time: under virtual time it shows "pending".

| Check | What it proves |
|---|---|
| C1 | The UI's parameter set equals the manifest's: 86 ids equal, plus 2 controls that are only in §6.1 |
| C2 | Play ⊆ Rack (33 controls) |
| C3 | The one-patch rule |
| C4 | The morph laws |
| C5 | Macros bind by order plus label |
| C6 | Globals at centre give the patch exactly as saved |
| C7 | Tempo and Sync: 220 pairs; a rendered echo at 11 024 samples, against a target of 11 025 ±1 |
| C8 | Tune leaves synced times alone |
| C9 | The structural crossfade: 0 of 8192 samples differ |
| C10 | Hard bypass is bit-identical |
| C11 | The randomiser is seeded and nested, and locks hold |
| C12 | Loop safety |
| C13 | Drawn = heard |
| C14 | Tapped = untapped |
| C15 | The canvases |
| C16 | horde's divisions equal the manifest's |
| C17 | The no-data scan: 0 hits over 530 needles |
| C18 | The real worklet |

**With the spec absent:** 1 / 1 passes (C14).

**C17 caught a real hit during the build.** A helper name contained one of the manifest's camelCase ids. The helper was renamed.

**The probe window was too short.** The first probe window (93 ms) was shorter than a default echo, so internal feedback and damping read as unvoiced. It is now 0.6 s.

## Evidence consulted

- ROADMAP B319, B320, B321, B328 and B329.
- The B319 and B320 traces.
- Sluice's spec v1, its manifest and its responses seq 4, 6 and 7. All were read in place and none was copied.
- `reference/network-lab-v0.html`.
- `tools/labharness/lab_load_check.mjs`.
- `tools/labharness/lab_wheel_scroll_check.mjs`: no wheel listener is added, so no inventory change is needed.
- `tools/gen_lab_index.py`.

## Alternatives rejected

- **Porting Sluice's generator and macros as data.** Rejected: the hold forbids it.
- **Hard-coding the global laws' targets by Sluice key.** Rejected: roles are matched by normalised name, and the one bus key the file names is the patch's own dry/wet.
- **Asserting which controls v0.1 voices.** Rejected: it is measured instead.
- **A `?nospec` switch to force the degraded state.** Rejected: the real absence was captured by moving the link aside.

## Verify

`./verify fast` on the committed hash. The exit code and git hash are in the PR and in `.harness/last-verify.json`. `lab_load_check` is GREEN with the link absent and GREEN with it present: 57 labs, 0 broken.

## Open questions

**For the human (§13):**
1. **#3 Bypass.** Hard or spill-over?
2. **#5 Corners whose module lists differ.** Forbid, or a structural crossfade?
3. **#6 What the host exposes.** Only the macros, or every pooled parameter?
4. **#7 Library storage.**

**For Sluice, v2 ambiguities.** The same list is on the page.
1. §6.1 has Tail and Clear, but the manifest's `globalKnobs` does not.
2. There is no `scope` field to tell a per-module bus entry from a patch-level one.
3. There is no post-chain role.
4. Several entries have no default: injection, the saturator and output-stage switches. The patch gain has no range at all.
5. The LFO's sync entry does not say which field it drives.
6. Time under Sync: §8 says "delay times", while the manifest says every delay-line length. Does a step past the range clamp or fold?
7. Division names: are they structure (§6.3) or numeric (§6.4)?
8. Tail ∞ against a finite gate release. Does Tail reach loaded patches, or only generated ones?
9. The Dry/Wet curve is not given as numbers.
10. When the XY sits between corners, which corner does a macro move write? Are macro values stored per snapshot?
11. Does blend-then-round round to the integer or to the step?
12. The feel family has no field of its own. The crossfade's shape is not stated.

**For the lead:** B319's commit (`d1b1ca2`) put Sluice's preset macros, its generator ranges and an `fz.h` port into the public history. This rebuild removes them from the tree, but not from the history.
