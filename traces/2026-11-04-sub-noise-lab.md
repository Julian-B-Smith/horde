# 2026-11-04 — A lab for the Sub and Noise pages' pictures (GB9)

**What changed.** New lab `docs/design/sub-noise-lab.html`, the design index, the ledger's lab entry, and GB9. Nothing in `gui3*`, `arp-face/` or the master changes.

**Why.** The human, through the lead: "I want the sub and noise pages to be much more stylistic and interesting than they currently are. Maybe we can come up with an interesting way to visualize each. Let's throw up a lab".

**Evidence consulted.** `specs/SPEC-ENGINE.md` 5.5, 5.6, SN-2, SN-4, SN-5, SN-8 to SN-14, IF-10 and RC-8. `docs/design/gui3.html`'s `vizView`. The lead's seeds: a driver cone, a cycle wrapped round a dial, a raster of the signal, a weather field.

**Two faults the lab's own looking found.** Tube's curve never reached the ceiling, so drive didn't flatten against the Dial's ring; the self-check caught it. The Raster drew the signal's size, so an LFSR, ±1 everywhere, was a solid block with no bands; the screenshot showed it, the check hadn't, so it has a check now.

**Not checked.** A plugin host, the light theme by eye, and every other screen scheme by eye. The pictures draw through `Viz.kit`'s screen roles, but only the default scheme was looked at. The pane throttled its frames, so motion was judged from stepped frames, not at 60 fps.

**Verify.** See the PR body.
