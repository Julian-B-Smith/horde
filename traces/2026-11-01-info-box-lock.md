# info-box-lock — ADR-345: the foot row is F1, a selection locks the info box, long text scrolls itself, an Info sidebar in Settings (B589, B582, B584)

- **Queue items:** B589, B582, B584. GUI session. The human picked F1 from `docs/design/foot-row-lab.html`, asked for the lock, the auto-scroll and the sidebar, and answered three follow-up questions by poll.
- **The chain:** ruling ADR-345 → master `InfoBox` at `1793569105-ed02` → vendored → shell. The info box had no master component since ADR-315, a hop the ledger recorded as skipped, so the ruling built it there first (ADR-316).
- **Evidence consulted:** ADR-315, ADR-335, ADR-338 r7 and r11, the foot-row lab's table, `Session` in the bundle (`S.selected` is set on a press, `escapeLayers`, `S.now`), `gui3.html`.
- **What changed:** see the README's section "The foot row and the info box's lock" and the ledger's four `adr345-*` rows. Two older rows (`adr315-info-box`, `adr335-fixed-info-box`) moved from `ruled` to `in-shell`. The lab moved from `exploring` to `harvested`.
- **The lab reads the live shell,** so each option now states its own foot row, F3 borrows the shell's `#rail`, and its fit control reads Osc 1's height from the shell's fit line, because the old "scroll N px" text is gone. It adds a control that F1 equals the shell as shipped.
- **A check tightened:** the shell's fit check compared pages to `max(area, 472)`; it now compares them to the real page area (468 px). Osc 1 needs 467.
- **Found by driving the preview:** the first published version (`1793568967-6a34`) stopped its auto-scroll at about 2 px, because comparing `scrollTop` to the unrounded position read as a hand on the scroll. It now compares to what the browser stored. The stall guard was also loosened (0.1 s to 0.5 s) so a slow frame rate keeps its speed.
- **Checks:**
  - the shell's self-checks, 45 of 45, four new with controls (lock, reach, auto-scroll, sidebar);
  - `gui3_seam_check`: new test `infolock` with seven plants;
  - with a real pointer in the browser: a press on Attack held through crossing another knob and onto the box, and a press on the Set tab released; the sidebar with morph on; Voicing's scroll on real frames, stopping once the description was in view;
  - the InfoBox preview: a lock, then a steady scroll.
- **Not checked:** auto-scroll's speed at a normal frame rate: the pane was hidden and ran at about 3 frames per second, so the 8 px per second is the code's number, not a measurement. Other window scales, and the light scheme. A plugin host.
- **Verify:** `./verify fast` exit 0 on this commit's tree. It first went red on `gen_gui3_controls`, because the port ledger's edit left `shell-data.js` stale; regenerated, no gate edited.
