# adr341-a1-polarity-chord — the polarity chord becomes Shift+Option-click (ADR-341 A1), into the master and the bundle

- **Queue items:** B589, B595. GUI session.
- **Why:** the human wrote, "I'm not sure about command being part of MP-c actually, since windows keyboards don't have the button". Asked to choose, the human picked Shift+Option-click over keeping ⌘⌥ / Ctrl+Alt and over having no chord.
- **Evidence consulted:**
  - DECISIONS.md ADR-341 r4 and ADR-340 r9's amendment, where the human first suggested shift+opt+click;
  - the master's `chordHit` (bundle.js), and every README and preview that named the chord.
- **What changed:**
  - **DECISIONS.md:** ADR-341 A1.
  - **The master:**
    - `chordHit` is now Shift+Option with Command and Ctrl up. The platform test is gone, since one chord serves both platforms.
    - The Knob, MapControl and RouteRow READMEs, the Knob preview hint, the master README and `design-system.json`'s lastChange.
  - **Published** 1793556326-fe52; VERSION recorded; `docs/design/arp-face/` re-vendored, with its provenance row.
  - **The ledger:** the new master version, and adr341-mp-c re-stamped with A1.
- **Verify:**
  - In the browser, on the MapControl preview: Shift+Option-click on a mapped knob cycled BI → INV. The control, ⌘⌥-click, no longer cycles; it acts as an Option-click and moves focus.
  - `design_chain_check` GREEN, 165 files at 1793556326-fe52.
- **Not verified:**
  - whether a host keeps Shift+Option-click in a plugin window;
  - whether Windows' Shift+Alt layout hotkey fires around the click.
