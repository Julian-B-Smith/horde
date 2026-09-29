# b353-sluice-lab-round4 — Sluice's XY back to one patch; horde's morph XY (a model) holds a Sluice patch per corner; macros per patch with per-snapshot ranges; tails in SETTINGS; C17 reads the spec's prose

- **Queue item:** B353, with B352 and B354. I read the rows verbatim from `origin/lead-records-141:ROADMAP.md` (records PR #849), with B337, B347 and B348, and read ADR-188 and its amendment A1 from the same branch's DECISIONS.md. The horde lead dispatched it on 2026-09-29, and sent two follow-ups while it was being built:
  - A1 (B354): per-snapshot macro ranges.
  - The human's confirmations: "I did mean per parameter", and the fixed four take no per-corner ranges.
- **Why:** The human corrected round 3: "I don't think the Sluice XY should allow for different patches on the same XY. I do, however, believe the global Horde morph XY should allow for different sluice patches at each corner (with full access to their constituent XY pads, which can be modulated or mapped to global Horde macros)." They also ruled on round 3's four questions:
  - "Buffer zone approved".
  - "Dry/wet glide is a good idea".
  - "macros should be for entire patches, not corners".
  - A load should have "the option of letting the old tail ring".

  Separately, two copied spec phrases had passed C17's manifest-only scan (#838, #848).
- **Branch:** `lab-sluice-r4`, from `origin/lab-sluice-r3`. PR #848 was NOT merged to main when I started (`git log origin/main` has the records commit for B348 but not the lab commits), so this branch stacks on round 3.
- **Files:** `docs/design/sluice-horde-lab.html` and this trace. I touched nothing else and deleted no file. `docs/design/index.html` was not committed. The `local/sluice` link was made for the build (gitignored) and removed at the end. `lab-review` is `B319 + B329 + B337 + B338 + B343 + B348 + B353 · 2026-09-29`.

## Item by item

1. **Sluice's XY holds ONE patch.** `LAB.cp` is gone. `LAB.corners` is the focused horde corner's patch's four working copies, and `LAB.xy` is that corner's Sluice-XY position object, so B337's library, bindings, drafts, randomize, save and discard act on it unchanged. The following are gone from Sluice's XY, the library and the acts:
   - the per-corner patch picker;
   - "new patch → X" and "sibling → X";
   - the library's "→ X";
   - the parity badges, flip line and class tint.

   Loading a patch replaces all four corners at once. The per-corner parity code (`patchParity`, `parityClasses`, `pickLive`, `morphRig`) is unchanged and runs one level up. `parityClasses` and `morphRig` take corner names so that horde's classes read ≡1…≡4. `siblingOf` stays for C28 and C29.
2. **Macros are patch-level (ADR-188 §5, A1).**
   - The MAPPING (label, targets, order, curve) is the patch's. `forPatch` writes it into every working copy and snapshot.
   - The VALUE lives in the entry (`E.mv`), never in a snapshot, so a move is never a draft.
   - Each snapshot holds its own `[lo, hi]` per binding. `setCornerRange` writes only those two numbers into one corner's working copy, which makes it a draft.
   - `blendW` interpolates the ranges bilinearly through one law, `lerpRange`, which the range bar also calls. `sluiceEval` then writes each target as lo(xy) + curve(v)·(hi(xy) − lo(xy)).
   - A new patch's `E.mv` starts at the value that reproduces corner A as stored. Randomize params re-centres a corner's range for a param a macro holds, so the draw is not silent (a question for the human).

   The UI:
   - a range bar under each variable knob (the four corners thin, the interpolated range thick, the heard value as a tick);
   - an editor split into MAPPING (patch) and a RANGES table (a min and max per corner per binding, number inputs only);
   - a "HELD · Mk" tag on rack rows.

   The round-3 scope toggle, `macroWrite` and `macroScopeTargets` are removed, and a tag reads "SCOPE: PATCH (RULED)". Per binding is confirmed ("I did mean per parameter").

   The fixed four take no per-corner range. They keep spec §6.1's own law, which I read in place and which matches the human's description: the centre plays the patch as it stands and the ends are fixed. The lab already applied that law to the XY-interpolated patch. The page says so in its own words and quotes the human.
3. **Horde's morph XY: a MODEL (the MAIN tab, section K2).** It is labelled as not horde's quantum-morph field: no Gumbel-max owner per parameter, no salience and no temperature.
   - Four horde corners, `LAB.hc[h] = {e, x, y}`, each evaluated by `sluiceEval` (Sluice's snapshot morph at its position, then its macros).
   - `morphRig` blends the evaluated patches under parity and otherwise flips with hysteresis 0.1 (the band slider stays; the rule pills are gone).
   - The class map is drawn: tints, the hatched buffer, the flip line and a "QUANTUM FLIP" flash.
   - Each corner card has a library picker (a LOAD), "rand" (a new patch, also a LOAD), "edit →", a mini Sluice XY (draggable, showing base and heard position) and two position knobs.
   - Across a flip the fixed four are untouched (Sluice-global), and the patch's dry/wet GLIDES. The glide happens in the renderer: `{glide:s}` retells v0.1's `mix` every 1 ms from the old value to the new, linearly, counted in samples (a 128-sample worklet and 1024-sample page hops agree). An in-place patch during a glide retargets it.
   - The glide time is a SETTINGS slider, 0–1000 ms, default 250 ms. The tail meter traces the mix the renderer last told the engine (dashed), so the ramp is visible.
4. **Sluice-XY positions are parameters.** Each corner has two knobs, which draw the physics-ink modulation span and a "now" tick when mapped. `effPos` applies horde-macro maps as lo + v·(hi − lo). The model has two horde macros, a mapping list and a demo button: H-MACRO 1 drives the Sluice X of corners 1 and 2 in opposite directions.
5. **Tails are a SETTINGS page.**
   - Spill-over or hard applies to a flip and a bypass.
   - A separate LOAD toggle is off by default. `patchMsg` decides spill and glide per event.
   - The schematic tail meter, the engine count and the "second instance only while an outgoing patch rings or crossfades" note are kept.
   - The FX rack row now only reports the setting.
6. **C17 reads the spec's prose.** Every 28-character run of `local/sluice/SLUICE-IN-HORDE.md` is read at runtime and must be absent from the file as served. Both sides are normalised: lower case, `' + '` joins removed, HTML entities and `* _ \` | # > / \` turned to spaces, whitespace collapsed. A run needs 20 or more letters to count. A run shared with horde's v0.1 reference is reported apart.
   - **Catches:** verbatim text, text wrapped across lines (in a comment or not), a change of case, a phrase split over a string concatenation, and any 28-character piece of a longer copy.
   - **Misses:** paraphrase, a copy under 28 characters, a word changed at least every 27 characters, a copy broken by a tag or escape, and a copy assembled at runtime.

   The first run found **18 copied phrases** already in round 3's file. One of them was wrapped across a comment line in the round-3 header, which is exactly what the old scan could not see. Every one was paraphrased, and the offline twin and the page both now read 0.
7. **Kept and adapted.**
   - The GBC skin shows horde state: "H1 LIVE"/"H1 =3 <patch>" on the title row, "HORDE: BLEND" or "HORDE 2CL +-.1", FLIP, and the base-to-heard position line. Its Sluice-XY flip line is removed because that XY has one patch.
   - C14, C15, C27 and `glyphAudit` are unchanged.
   - C17 is widened and C28 reads the badges on MAIN.
   - C22, C24, C25 and C26 read the one-patch and E.mv model. C24's "definition" is now the mapping; the ranges are held by C37 and C38.
   - C5 reads its value through `macroShown`, over all bindings as before.
   - C13 also compares the meters' `mix`, `glide` and `glides`.
   - Only text changed in C3, C29 and C31.

## Checks

- **Retired:** C34 (macro scope), because the feature it checked was removed by ruling (B352). A note in its place points to C37.
- **Added (each with its must-fail control):**

| Check | What it proves | Must-fail controls |
|---|---|---|
| C36 | Sluice's XY holds one patch. On the page every horde corner has one structure. A foreign snapshot bound, or a foreign working copy set, is refused. A LOAD replaces all four corners. No per-corner controls exist on any face. | Round 3's corners from two patches; a binder that skips the gate; a planted per-corner picker |
| C37 | Macros are the patch's. One mapping everywhere. A move changes E.mv and what is heard, with no draft and the snapshots byte-identical. A range edit makes a draft of that corner only, with the mapping unchanged. The range table has no select. There is no scope toggle. | Round 3's writer (into the selected corner) drafts; a corner that retargets splits the mapping; a planted toggle |
| C38 | Ranges interpolate bilinearly. This is checked against the formula written out (not `lerpRange`) on a 9×9 grid with four different corner ranges and a log curve. The relative place equals v everywhere, corners are exact, and the bar's law equals it. | Nearest-corner ranges; an absolute value |
| C39 | Horde blends and flips. Horde corners 1 and 2 hold one patch at two Sluice positions; 3 and 4 hold another structure. Inside ≡1, corner 1 = P1, corner 2 = P2 and the mid-edge values are their mean. Moving corner 2's Sluice XY changes what is heard. Swept at x = 0.5, the flip comes past 0.6 going up and under 0.4 coming down. Perturbing ≡3 does not change ≡1. | Band 0 (the flip moves to 0.5); a leaky bus blend |
| C40 | Across the flip the fixed four act by one law on both sides. The told mix equals a + (b−a)·n/N at every block end, then b exactly. The samples equal two fresh engines driven by that schedule (0 differ). The SETTINGS slider and the flip message carry the page's glide time. | Glide 0 (a step); twice as fast |
| C41 | The demo map puts corners 1 and 2 at lo + v·(hi−lo). Every unmapped axis stays exactly at its base. One knob moves both. Corner 1's evaluated patch moves, corner 3's does not. On the page, only the mapped knobs draw a span. | A mapper that ignores maps; a map one corner off |
| C42 | The load message spills iff the load option is on, and never glides. Rendered, ON gives 1 tail, 2 engines and a ringing meter; OFF gives 0 tails, 1 engine and −150 dB. The SETTINGS checkbox is the switch. | A maker that ignores the option; one that spills loads under the flip preference |

- **Count:** 41 self-checks (35 − C34 + 7), **41/41** with every control firing. This was read over CDP from headless Chrome on the default load and in all 12 screenshot states. C17 reads MANIFEST 0 of 530 and PROSE 0 of 13 491 runs, and all three prose controls are caught (wrapped, split, bare).
- **Served-copy must-fail:** I built a scratch serve tree (`scratchpad/b353/servetree`, never committed) whose lab copy carries one 40-character spec phrase from §9, wrapped across a comment line. C17 FAILS on it with "FOUND 13 in 1 phrase(s)". On the same copy the manifest part reads 0 found, so the old scan alone would have passed it.
- **`lab_load_check`:** GREEN with the link (58 labs, 0 broken, 1 skipped) and GREEN with it moved aside (same). With the link absent the page shows the no-spec face, only FX is live and C14 passes.
- **Private names:** the case-sensitive pattern, run as the gate runs it, gives 0 matches (the gate skips itself in a worktree). The file contains no machine path, and the Play face's name is never spelled (C32).

## Screenshots (scratch, not committed; `scratchpad/b353/shots/`, 2× DPR)

- `01`: Sluice's one-patch XY with a draft.
- `02`: horde's morph XY, with two structures, the band and a flip.
- `03`: a parity blend (all four corners one patch).
- `04`: the dry/wet glide on the tail meter.
- `05`: the Sluice-XY-to-macro mapping.
- `06`: the tail settings (a flip, a load that rings, a bypass).
- `07` and `08`: the GBC skin, light and dark.
- `09` and `10`: dark, the morph page and the Play face.
- `11` and `12`: the macro ranges editor.
- `13`: no link.

## Alternatives rejected

- **The mapping kept per patch at the entry level (`E.macros`) rather than in each snapshot.** It would be safer by construction, but every structural edit, generation and sibling remap would have been rewritten with index alignment. I kept B337's `forPatch` single writer instead, and added C37 to prove the mapping is identical everywhere and that the corner editor cannot reach it.
- **Doing the glide outside the engine.** v0.1's mix sits inside its output nonlinearity, so it cannot be separated. The glide re-tells the engine through `applyPatch`, which allocates inside v0.1 during a glide only; that is a stand-in cost.
- **Putting horde's morph above the Sluice node on the FX page.** The 720 px window has no room for it, so it went on horde's MAIN tab.

## Verify

`./verify fast` was run on the committed lab hash. The result and hash are in the PR and the report, read from `.harness/last-verify.json`.

## Open questions

- **For the human:**
  - Should the load-tail option apply only under spill-over?
  - Is a 250 ms linear glide right?
  - Should a horde macro map replace or offset the base position?
  - Should randomize re-centre the range of a param a macro holds?
  - Where does the morph XY live in horde's own window?
- **For Sluice:**
  - Should ranges interpolate in value space or in taper space?
  - What happens to the stored value of a param a macro holds?
  - Can netcore evaluate a patch at an XY and hand back plain numbers?
  - Should the mix take a slew target rather than per-millisecond re-tells?
- **Not verified:** only still frames were checked, and no audio was listened to. The glide is measured (C40), not heard.
