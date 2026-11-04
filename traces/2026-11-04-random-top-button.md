# 2026-11-04 — Random gets its own top button; Ledger moves under Set (GB12, GUI-25)

**What changed.** GUI 3's page bar gains Random after Morph and loses Ledger, which is now a second tab under Set (`docs/design/gui3/layout.json`, regenerated data). The Randomize placeholder moves from the Morph page's foot to the new Random page (`docs/design/gui3.html`), and self-check 18b holds all of it. `random-home-lab.html` is marked ruled: its top option is now the shell untouched, its second option rebuilds the tab layout from the new page list, and the third, the old foot of the page, is gone. GUI-25, the ledger, the port ledger's row and the README record it. The design index is regenerated, which also lists the Sluice, Shriek and FX labs merged earlier today.

The seam check's `settab` test gains a constructed group. Its plant tested a one-page group whose id differs from its page, and when Set took a second page no real group had that shape, so the plant stayed green. The test now keeps that case in view, and every real group is still clicked.

The new shell check and the lab's control read each other: on the lab's option not taken, with no Random in the bar, GUI-25's line reads FAILED and every other shell check passes.

**Why.** The human, 2026-11-04: "Its own top button", the option they described as "me might have to move ledger to a second tab within settings, especially since it isn't planned to be a part of the actual GUI at all". It amends ADR-322 item 2.

**Evidence consulted.** ADR-322 item 2, `docs/design/random-home-lab.html` as the human tried it, and the generator's page count: 16 pages. The README and the shell's header said twelve, which had been out of date for some time, so both now say sixteen.

**Verify.** `./verify fast`, on the commit this trace ships in. The shell and the lab opened in a real browser.
