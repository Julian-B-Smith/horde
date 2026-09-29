# b348-sluice-lab-round3 — per-corner patches, parity morph against a quantum flip, the default face by its spec name, the full device, macro scope and spill-over tails

- **Queue item:** B348. I read it verbatim from `origin/lead-records-138:ROADMAP.md` (records PR #842), together with B329, B337, B338, B343 and B347. Dispatched by the horde lead on 2026-09-28.
- **Why:** B347 records the human's rulings on Sluice's open items (§13). Corners get their own patches, with a quantum flip unless patch parity is detected. The fixed four macros stay gradual. The default is the rapid random-patch face, with the full device available. Macro scope is to be workshopped. Bypass is provisionally spill-over. These rulings overturn spec v1's one-patch rule, so the lab builds them as a labelled horde proposal pending Sluice seq 11.
- **Files:** `docs/design/sluice-horde-lab.html` and this trace. Nothing else was touched and nothing was deleted. `docs/design/index.html` was not committed. The lab's `lab-review` meta is `B319 + B329 + B337 + B338 + B343 + B348 · 2026-09-28`.

## What changed, item by item

1. **Per-corner patches (section K).**
   - `LAB.cp[k]` names the library entry corner k plays. Corner k plays that entry's working copy k, bound to its binding k. This means B337's randomize, save, discard and bind functions act on the selected corner's own patch without being changed.
   - `LAB.corners` and `LAB.libSel` are now getters, so they cannot go stale. A check that builds over its own patch sets `LAB.cov` and clears it afterwards.
   - A corner row holds the corner's patch picker above its snapshot picker, then DRAFT/saved, its parity class and its weight.
   - Two new buttons:
     - "new patch → X" makes a new random structure for the selected corner only.
     - "sibling → X" makes another patch with the same roster and wiring, with every number re-drawn. It exists so the human can hear two different patches morph continuously, because generated rosters rarely coincide.
   - Library loads still fill all four corners (§9). A per-entry "→ X" loads a patch into one corner.
   - Structural edits, locks and sync names reach every corner that plays the edited patch, and no other patch.

2. **Patch parity (section F2, `patchParity`).**
   - **Definition:** two patches match when they have the same module types in the same order, in the loop chain and in the post chain. In a serial network inside one bus, that roster and wiring is all the wiring there is.
   - **Ignored:** numbers, module ids, names, macros, and the discrete values §9 takes from the nearest corner. It is exactly the one-patch rule's `structSig`.
   - **Classes and the flip:** corners fall into parity classes. The boundary rule `pickLive` chooses the LIVE class. `morphRig` blends inside that class with its weights renormalised. When all four corners share one class this is spec v1's morph exactly (C29). Crossing into another class is a discrete switch: the quantum flip.
   - **Badges:** there is one badge per corner pair, ≡ or ≠.

3. **The three boundary rules.** All three are drawn on the XY from the same `pickLive` the morph uses: tinted regions, the flip line, and the band hatched.
   - **NEAREST CORNER:** the quadrant lines.
   - **WEIGHT > 0.5:** a class's summed bilinear weight must pass 0.5; below that the class holds. The line bends round a lone corner (t ≈ 0.293 on the diagonal).
   - **HYSTERESIS:** a class is left only when another class passes 0.5 + band. This is the lab's default, with a band of 0.1.

4. **The fixed four stay continuous across a flip.** They remain Sluice-global, so a flip never moves them (C31). The fourth keeps the manifest's label "Tune". A note on the page asks the human whether it should read "tone"; this is not decided.

5. **The default face and the full device.**
   - **Default face:** the Play face's name is parsed from spec §11 at load, along with its gloss, §11.1's parenthesis and every spec sentence that uses the name (as the band's tooltip). The file never spells the name (C32 checks this, whitespace-flattened). The face is the first tab and the default. Its library is minimal: no tools and no JSON box.
   - **Full device:** this is the Rack face. It has a runtime count head: the selected corner's pool numbers, group fields, globals and variable slots. It now scrolls as a whole, because its taller strip had squeezed the rack body to a sliver.

6. **Macro scope.**
   - A SELECTED CORNER / EVERY CORNER toggle.
   - EVERY CORNER writes slot k of each corner that has it, by that patch's own binding. Binding is by order, so slot k exists in each patch.
   - A 4×4 grid shows every corner's value in every variable slot, with the cells the current scope would write outlined. It repaints live.

7. **Tails.**
   - One mode now covers both a bypass and a flip. The default is spill-over (B347, provisional); hard stays switchable.
   - **The renderer:** a spill-over flip moves the outgoing engine to a tail list and feeds it silence.
     - A tail is dropped after 0.25 s under 1e-6, or after 8 s with a 20 ms fade.
     - At most 3 tails ring at once.
     - The loop is allocation-free, using swap-remove.
     - A hard bypass cuts the tails.
   - Loads and rack edits keep §6.3's cut.
   - The tail meter is measured on horde's v0.1 stand-in and labelled "SCHEMATIC — … not Sluice's netcore". Flips and bypasses are marked on it.

8. **The GBC skin shows the new state.** It shows the live class and patch, the flip line and band in the XY field, "FLIP" for 0.6 s of rendered samples after a flip, each row's class and LIVE, and the rule with its class count. The title now reads SEQ 11. `glyphAudit` (C27) is unchanged.

## Evidence

- **VERIFIED: self-checks 35/35, each with its must-fail control firing.** They were read over CDP from headless Chrome in real time, served on 8348. They pass in these states:
  - the default load;
  - a mixed rig (`corner=2&act=N,c3,T6,…`);
  - FULL DEVICE in the dark theme with a new corner patch;
  - all 12 screenshot states.

  The eight new checks:

| Check | What it proves | Must-fail controls |
|---|---|---|
| C28 parity | Match: a patch with itself, with its numbers re-drawn, and with another patch of the same roster (fresh ids, both orders). No match: another roster, a swapped order, a loop module moved to the post chain, one type swapped. The 6 live badges agree. | A roster-as-set rival; a whole-content rival; a planted wrong badge |
| C29 continuous only under parity | Two patches with parity: one class, equal to spec v1's morph at all 201 points, largest step 0.0040 of the spread against a path step of 0.0048. AB against CD: 1 flip, the within-class hull holds, and perturbing the other class changes nothing heard. | A rig that also blends the bus over all four corners; the nearest corner played whole (step 1.0) |
| C30 exact boundary and no chatter | Nearest equals the brute-force answer at 1681 points on two rigs. Weight flips at 0.5025 and 0.4975. Hysteresis flips at 0.6025 and 0.3975. The lone corner flips at 0.5025 (nearest) and 0.2950 (weight). Hovering at 0.5 ± 0.04: hysteresis 0 flips, then exactly 2 on the excursion. | Weight gives 199 flips and band 0 gives 199; the two rules are told apart |
| C31 fixed four continuous across a flip | One global set either side. Time ×1.3947 and Tune ×1.2483 act by the same law on the params before and after. | Positions stored per patch: Time ×1.395 → ×1.000 at the flip |
| C32 default face | The name is parsed from §11, opens with no query, is the first tab, is shown on the face, and is never spelled in the served file. | A face query is honoured; the planted name is caught |
| C33 full device reaches every number | All 25 pool numbers of the live patch have a row; all group, global and variable ids are present; all 82 manifest ids appear in the catalogue; the default face carries none of the pool. | One row removed is caught |
| C34 macro scope | Selected writes B only. Every corner writes A, B, C (D has no slot and is untouched). The live grid outlines exactly those cells. | An every-corner move that writes only B; a selected move that writes all |
| C35 tails | Spill-over equals the new engine plus the old engine fed silence, 0 of 8192 samples differ, and the meter reads −30.7 dB. Hard equals the fresh engine alone after the fade (0 differ) and the meter reads −150. A spilled bypass rings; a hard bypass is cut. | An old engine that keeps hearing the input (4096 differ); a "spill" that cuts |

- **C17 and the private names:** C17 reads 0 of 530 needles. The private-name patterns, run by hand, give 0 matches; the gate skips them in a worktree because `.leakcheck-names` is absent there. A pre-existing B329 comment spelled the face's name across a line break. It was reworded, and C32's scan was tightened to whitespace-flattened text so a wrapped spelling is caught.
- **Existing checks adapted without weakening:**
  - C3 is renamed to state that inside one patch another roster is still refused. Its assertion is unchanged.
  - C22 and C25 now read each corner's binding in the patch that corner plays.
  - C13's meter comparison also compares the tail fields.
  - C23 and `catalogueDom` use `LAB.cov`.
- **`lab_load_check`:** GREEN with the link present and GREEN with it moved aside (58 labs, 0 broken, 1 skipped). With the link absent the page shows "Sluice spec not available on this machine" and C14 passes 1/1.
- **Verify:** `./verify fast` exited 0 on the lab commit. `.harness/last-verify.json` reads `{"target":"fast","exit":0,"git":"2a10eda","ts":"2026-09-29T00:10:19Z"}`. The trace commit is re-verified and its hash is in the PR.
- **Not verified:**
  - I checked only still frames. Drags, the flash timing and the GBC ghosting were checked in code, not watched.
  - I did not listen to any audio. A flip with a stacked tail is measured (C35), not heard.

## Screenshots (scratch, not committed; `scratchpad/b348/shots/`, 2× DPR)

- `01`: the XY with the flip line and two corners of another structure.
- `02`: the parity case, with three patches.
- `03` and `04`/`04b`: the default face against the full device.
- `05` and `06`: the two macro scopes.
- `07` and `08`: spill-over against hard.
- `09` and `10`: the GBC skin in light and dark.
- `11` and `12`: the dark theme.

## Alternatives rejected

- **Moving bind/work out of the entries into a per-corner rig.** Rejected: it would have rewritten B337's functions and C19–C24.
- **Counting modulation targets as wiring.** Rejected: §9 takes them from the nearest corner, so they blend like any other discrete value. This is asked of Sluice.
- **Spilling on every structural change.** Rejected: §6.3 cuts on a load; spill applies only to an XY flip and a bypass. This is asked of the human.
- **A drawn-only tail curve.** Rejected in favour of the level measured on v0.1, labelled as a schematic.

## Open questions

**For the human:**
1. "tone" or "Tune"?
2. Which boundary rule and band? I recommend HYSTERESIS at about 0.1: horde modulates its corner field, and each flip is a new engine and, in spill-over, a new tail.
3. Dry/Wet's centre is each patch's own mix, so the heard wet level can step at a flip. Should the patch mix glide across the flip?
4. Macro scope: selected corner or every corner?
5. Should a loaded patch also spill over?

**For the Sluice dialogue (seq 11, also on the page):**
1. Is parity = roster + order the right test?
2. Does a stored preset hold four patches, or four references?
3. Should netcore run the tail itself?
4. The cap on tails and the rule for dropping one.
5. Does Tail bound a spill-over tail?

**For the lead:** "sibling → X" is a small addition beyond the brief's list. It is the only way to show parity between two different patches.
