# 2026-11-06 — A layout group under the Morph page's pad, ready for X and Y (GUI-45 r3, GN-51)

**What changed.**
- **The layout's `side` flag.** A Morph page group marked `"side": true` is drawn in the pad's sticky column, after the pad and its law's banner, and is left out of the page's grid. Each control is still mounted once. (`docs/design/gui3.html`: `paramPage` and the Morph page's assembly.)
- **`tools/gen_gui3_controls.py`.**
  - The flag reaches the shell's data.
  - A new `side` gate turns RED on a side group anywhere but the page that has the pad, where it would otherwise be dropped silently.
  - A must-fail control covers that gate (45 controls now).
- **Self-check 4b2e.** Every side group must sit below the pad and its banner, and appear once. Until the rows land, no group is marked, and the line says so.
- **Ledger.** The `gui45-pad-xy-knobs` note says the place is ready.

**Why.** The lead, 2026-11-06, asked for `morph.x` and `morph.y` in the layout so round 61 can land D.2's rows. `gen_gui3_controls --check` makes the order a deadlock:
- a layout key that is not yet a row is RED (`unknown-key`);
- a host row that is not yet placed is RED (`unreached`).

So the rows and their group land together in round 61's PR. This PR lands the placement first, with no morph keys, and it is green on main by itself. The lead agreed this plan. The group to add is:
`{"title": "Position", "tag": "IF-11", "side": true, "keys": ["morph.x", "morph.y"], "abbr": {"morph.x": "X", "morph.y": "Y"}}`, after the Morph page's "Morph" group.

**Evidence.**
- A scratch run marked the Quantum group `side`, regenerated, and loaded the page. All 84 self-checks passed, 4b2e among them ("Quantum below the pad and its banner"), and the screenshot shows Quantum under the pad's banner. The scratch change was reverted.
- `gen_gui3_controls --check` is GREEN with 45 must-fail controls red. The new one is "a group under a pad on a page without one".

**Verify.** `./verify fast` on the commit this trace ships in.
