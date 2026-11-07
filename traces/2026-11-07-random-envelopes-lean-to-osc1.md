# 2026-11-07 — A random patch's moved envelope selector leans to osc 1's envelope (GUI-55 A2, GB32)

**What changed.**
- **The master, `docs/design-system/master/components/bundle.js`.**
  - `drawRows` gains a `favour` hint: a stepped row's option index, landed on with `prior.envOne`'s chance. The other options share the rest evenly.
  - The draw reuses the row's existing pick, so no other row's draw shifts.
  - `prior.envOne` is 0.8, a stand-in.
  - The RandomPanel README says so.
  - Published as master `1794078853-724a` and re-vendored into `docs/design/arp-face/`.
- **`docs/design/gui3.html`.** `randomPlan` passes osc 1's envelope as `favour` for each source's `env` selector: ENV 1 for osc 1 itself, and for the others what osc 1 holds after the same draw. Osc 1 is drawn first.
- **Self-check 4c8c.**
- **Records:**
  - GUI-55 A2 in `DECISIONS.md`;
  - GB32's status;
  - the ledger row `gui55-random-low-chaos` and `master_versions`;
  - the arp-face README's provenance row.

**Why.** The human, relayed by the lead (ADR-374 A2): "fully randomized patches should heavily bias toward attaching every osc volume to Osc 1". A1 already keeps the selectors still at low and moderate chaos.

**Evidence.**
- `gui3_browser_check`: GREEN, 90 self-checks.
- **Control, run and restored:** a plant of an even weight (`envOne` 0.25) turned 4c8c red, "29 % of draws against 37 % with no favour … 54 %".

**Verify.** `./verify fast` on the commit this trace ships in.
