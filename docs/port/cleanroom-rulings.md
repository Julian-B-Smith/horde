# Clean-room rulings log (B516)

> **Origin.** horde lead, 2026-10-19. Created under ADR-247 (audit P1). **Never delivered to either clean-room
> repository.** It records every question raised to the human during the clean-room build of the new engine
> (B494, B502), the ruling, the spec gap the question exposed, and whether the builder's wording or numbers
> entered the spec. It is the input to the next spec revision.
>
> **Columns.**
> - **Builder-origin:** **yes** if the ruled text or its numbers came from a builder proposal; such rows are
>   judged by outcome standards only (B518).
> - **Evidence:** **engine-free** if made without the engine and reproduced by the lead or the test author;
>   **engine** if computed from the engine's output or state.
>
> From ADR-247 on, a gate moves only with engine-free evidence and a ruling here.

| # | Question | Ruling | Spec gap it exposed | Builder-origin | Evidence | Record |
|---|---|---|---|---|---|---|
| 1 | B-3: old patches' glide and blade envelopes | Preserve them | B.1/X22: version-0 keys with no destination | no | — | ADR-237.9 |
| 2 | Q-TA-12/32/33: stage bounds | Ratified | QS-8 and item 14's thresholds left to the test author | no | engine-free | ADR-237.4 |
| 3 | QS-8's P_ref, 12 dB margin; QS-9's soak | Ratified; recheck at the bank | QS-8 and QS-9 had no values | no | reference renders | B498 |
| 4 | Q-TA-8, 15, 20, 37, 38: clarifications | Approved | B.1's σ, voice age, C.12's fraction, PA-8 blade 2, SN-2's square | no | — | ADR-237.5, ADR-238 |
| 5 | B-10: SW-4's 7th harmonic | Power ratio, gated ≥ 4.0 | An amplitude bound that cannot be met | partly | engine | ADR-238 |
| 6 | B-15: fades fire the detector | Raised-cosine fades, a 2nd-order smoother, onsets reported | The spec's fade shapes contradicted QS-4 | yes | engine-free (synthetic) | ADR-239 |
| 7 | B-14: QS-1's reference design | Figure corrected; the builder's design named as reference | A stated figure never measured | yes | engine | ADR-240 |
| 8 | B-16: four no-click cases | Reported | QS-4 against the spec's own ideal laws | partly | engine plus an ideal law | ADR-241 |
| 9 | B-22, B-23, B-26.7: edge construction | The builder's construction ratified | ED-1/3/6's methods missed QS-1 | **yes** | engine | ADR-243.1 |
| 10 | B-25, B-26.6: FM-free DC | Average over the future entries | C.16 wrong at whole-number rates | partly | engine, then critic | ADR-243.2 |
| 11 | B-26.1/.3/.5: three clarifications | Accepted | IF-2's order, BL-8, the blocker under PA-8 | yes | — | ADR-243.3 |
| 12 | B-24: Sync cut change and frame crossing | Reported | QS-4 on blades made of steps | partly | engine only (no ideal law computed) | ADR-243.4; **re-derive (B520)** |
| 13 | B-21: loop chaotic at full amounts | "Chaos as an option": `loopDrive` 0.14–1, default 0.3 | C.14's loop gain; BL-12's claim | **yes** (the scales) | engine | ADR-244; premise corrected by row 19 |
| 14 | B-21 at High | The loop at the host rate | C.14 depended on the rate | yes | engine | ADR-245.1 |
| 15 | B-28: FM-reset twins | Reported | QS-4 on carrier blades | partly | engine (law at the engine's phases) | ADR-245.2; **re-derive (B520)** |
| 16 | B-29: short tapers | Ratified in principle; the row held | ED-6 on short tapers | yes (concept) | engine | ADR-245.3; B-29a pending, numbers engine-free |
| 17 | B-30: carrier rows | Carrier modes only | PA-8's table | no | — | ADR-245.4 |
| 18 | Audit: the default on a failing gate | Keep the gate; adopt P1–P10 | The loop from builder to spec | no | — | ADR-247 |
| 19 | B521: BL-12 measured without the engine | BL-12 made real: contraction judged below 0.5; repetition judged at width 0.25; the one-pole is impulse-invariant | BL-12 could not fail; round 20's premise came from the engine | no | **engine-free** | ADR-248.3 |
| 20 | B-31: Fold twins in 9.3 | "One rule": rich-tone within-cycle changes are reported and judged by their own law tests | The detector cannot tell such changes from clicks on rich tones | no | **engine-free** (cleanroom-tests 69c640b) | ADR-248.1 |
| 21 | B-31: a Fold or Crush cut under Quantize | At the window's next entry | BL-16 assumed a carrier | no | engine-free | ADR-248.2 |
| 22 | B-33: band-limiting inside the loop | Loop reads pre-AA; BL-12 on a lighter grid | C.14 let the edge corrections into the loop, so BL-12's claim held for the naive law only | no | **engine-free** (cleanroom-tests b50798f) | ADR-250 |
| 23 | B-35: the start-up transient in 10.b6 | Add a settle (0.25 s before the note) | BL-12's measure timed from a note started mid-glide | no | **engine-free** (cleanroom-tests 8839f1d) | ADR-252 |

**Lead readings with no recorded ruling (B520: owed to the human):** B-12 (QS-4's must-fire signals), B-13 (the
saw no-click set moved to reported), B-27 (`env1.decayCurve` reported), and Q-TA-1/6 (the frozen no-click set).

**Exposures.**
- **2026-10-19, the builder's opener pasted into the test author's folder.** That session saw a few builder facts:
  that stage 5 was committed, the commit ids, and the file name `swarm.cpp`. It was closed and replaced by a fresh
  test-author session, so no builder fact reached the active test author.
