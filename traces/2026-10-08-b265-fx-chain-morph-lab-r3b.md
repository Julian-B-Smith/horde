# b265-fx-chain-morph-lab-r3b — round 3 follow-ups: reversal click, gain staging, six cable layouts, Sluice's blend-law points

- **Queue item:** B265, round 3 follow-up. Dispatched by the horde lead on 2026-10-08 after the human's listening pass on PR #980 (verbatim): "Round 3 is definitely sounding better (the specific example sound is kind of muddy and blown out, but I'm no longer hearing the jarring clicks I had been hearing except when a flip reverses the direction of the connection between the filter and the drive). I would like to see several alternative layouts for the cables view though; this one isn't quite landing." Two additions came mid-task: Sluice's blend-law answer (seq 36, read as prose only; nothing copied), and a note that the human had merged #980.
- **Why:** Remove the last click the human heard, make the demo clean by the numbers, give the human layouts to choose from, and weigh each of Sluice's four points by measurement.
- **This trace extends** `traces/2026-10-08-b265-fx-chain-morph-lab-r3.md`, which stays as written.

## The merge, and what this branch carries

#980 was merged as `9243a70`. That merge already contains two follow-up commits I had pushed to `lab-fx-chain-morph-r3` before the merge: `fdab297` (the reversal fix and claim 9) and `10a9d9f` (parallel hand-over and gain staging). They reached `main` with that merge, not through a review of their own. They are described below, and the new lab PR's body repeats them.

The new branch `lab-fx-chain-morph-r3b` (cut from `origin/main`) carries:
- `1bfe697`: the layouts, cherry-picked;
- `9006ef8`: Sluice's points.

## 1. The Filter ↔ Drive reversal click

**Reproduced** as the human heard it: 48 kHz, the saw chord, the puck slewed over the boundary. Under (a), round-3 v1 read 10.1–12.3 dB, against 0.2–0.8 dB under (b).

**Cause.** The weights were smooth through the event, so neither the pair draw, the bound's scaling, nor the normaliser was stepping. With the bound off the click was still 10.6 dB. The transient two-node loop itself was the cause: Drive's output came back into Filter one tick late and rang on the saw edges.

**Fix.** A reversal between two modules with no delay line now waits, in both strategies. While it waits, the pair runs in parallel: the old outer cables are held at half while the middle cable retreats (series → parallel → series). Two first attempts measured worse and are recorded in the lab's comments:
- a full dry bypass on any wait: 8.5 dB, from the dry saw flashing in;
- a bypass that opened only as the lane's reach fell: 12 dB, because the lane sagged first and the normaliser then drove the Drive hot.

Pairs with a delay line (ECHO, Scape, Sluice) keep (a)'s backward part, because there waiting measured worse.

**Measured** (live-like reversal metric, floored at what either chain makes held still):

| | worst Filter ↔ Drive | worst reversal of any pair |
|---|---|---|
| round-3 v1 at round 2's levels (as heard) | 10.1–12.3 dB | — |
| round-3 v1 as shipped, at the new levels | 7.6 dB | — |
| now (parallel hand-over, p = 1) | 4.4 dB | 5.9 dB (close, (b), Drive → ECHO) |

**Self-check 9:** every reversal is ≤ 7 dB. The plant is v1 as shipped (the loop, at p = 2).

## 2. Gain staging

**Before** (four presets, circle sweep, 48 kHz, saws, pulse on):
- output: peak −7.0 dBFS, true peak −7.0 dBTP, RMS −18.6 dBFS, no clipped samples;
- inside the chain: EQ +4.1 dBFS, Filter +1.7, Bulwark +0.8.

The sound was "blown out" because it was saturated, not clipped. Every Drive ran at 3–8 on a chord already at its 0.5 reference; the Sub sine sat at −9 dBFS into a Drive; reverb mixes reached 0.5–0.6.

**Changes.**
- Lanes: saw voices 0.16 → 0.10 per voice, Sub 0.35 → 0.22. Both engines share these, so the A/B stays fair.
- Every corner's drive is halved; Shriek's gain is 3; the EQ boost is +4 dB.
- The two most resonant filters: 800/900 Hz at resonance 0.65/0.7 become 1100/1300 Hz at 0.5/0.55.
- No reverb mix is above 0.4.
- Output trim 0.4 → 0.5.
- An honest output meter: sample peak and 4× true peak, held 2 s.
- No limiter.

**After** (28 sweeps: 4 sets × 7 paths):

| | before | after |
|---|---|---|
| worst output peak | −7.0 dBFS (one sweep) | −6.4 dBFS |
| worst true peak | −7.0 dBTP (one sweep) | −6.4 dBTP |
| RMS | −18.6 dBFS (one sweep) | −23.9 to −19.6 dBFS |
| hottest internal node | +4.1 dBFS (EQ) | −0.4 dBFS |

Nothing clips. Still hot: the Drives sit at about −5.5 dBFS, their normalised ceiling, by design.

## 3. Six cable-view layouts

All six draw one `cableData` state: the object the audio plays (L0064). The choice persists in localStorage, wrapped in try/catch, and a `layout=` query overrides it.

- **RING:** round 3's ring.
- **RACK:** a signal-flow row; left → right cables arc above, right → left below.
- **MATRIX:** a FROM × TO patchbay; cell fill is the weight, unavailable cells are hatched, loops outlined, waits dotted.
- **FLOW:** a Sankey per lane; ribbon width is the lane's transmission along each cable.
- **PANEL:** a Eurorack case with jacks and LEDs (lit = awake); patch cables sag between jacks.
- **ROLL:** my own: a scrolling history of every live cable's weight, so cables emerging and retreating read as a timeline.

All six work in both themes. Under `prefers-reduced-motion` the flow dots are static and the roll moves only on change. Phone-width CSS was added. Headless Chrome's smallest window is 500 px: at 500 px the page has no horizontal overflow and the PANEL is legible.

## 4. Sluice's points (seq 36), each measured

1. **Equal-gain.** p = 1 against p = 2:
   - the four presets' worst step click 4.3 → 1.7 dB (mean 1.18 → 0.34, over 3 dB 5 → 0);
   - live reversals 6.2 → 4.4 dB;
   - the normaliser's makeup ×2.11 → ×1.78.

   A reversal's parallel hand-over is exactly a correlated swap. **The lab default is now p = 1;** the selector is kept, because D8 leaves p to the ear.
2. **A one-sample feedback delay.**
   - **Correction to the premise:** v1's delay was one 64-sample control tick, not one host buffer. Every driver splits host buffers into 64-sample ticks.
   - **Now:** while a cycle is live, the rack runs per sample and a backward cable reads the previous sample.
   - **Self-check 11:** renders in host buffers of 64, 128 and 512 samples are bit-identical while a cycle is live. The plant, a naive host-block driver, differs by up to 0.437.
   - **CPU while a cycle is live:** 39.1 against 25.3 ms of rack time per second of 48 kHz audio.
   - **Sound:** no click difference on the cyclic transitions (0 dB either way).
3. **A loop limiter.**
   - **Measured:** it changes the output by a difference 36 dB under the signal (peak 0.012), with the bound on and off. No click changes.
   - **Why not needed:** with the bound, circulating energy already falls at least 6 dB per round trip.
   - **Kept as an off dev toggle that counts its engagements.** On by default it would shape silently, which ADR-197 forbids.
4. **No start-up sweeps.**
   - **Measured:** a woken ECHO started up to 33.0 ms off its delay time (round 1's top edge, 207 → 240 ms) and glided there.
   - **Now:** woken and first-fed modules snap their smoothers (ECHO's time, Bulwark's band crossfade). **Self-check 10:** 0.0 ms; the no-snap plant reads 33.0 ms.
   - **Bug exposed:** snapping exposed a latent NaN in round 2's ECHO wrap (`r + N` rounding to N), now fixed.
   - **Loud sleeps:** a never-decaying module is now flushed as it sleeps, instead of keeping its howl and ramping it back in.
   - **Releases already end in silence:** a released cable reaches −60 dB in 277 ms and is set to exactly 0 at 427 ms (a 1e-5 final step). A linear tail was not added.

## Self-check: 11/11 claims hold, 11/11 planted faults caught (headless node; 247 steps, longest about 2.8 s)

Claims 1–8 are as in round 3 (claim 7 now reads (a) 1.7, (b) 1.7, round 2 45.6 dB worst). Claim 9 is reversals ≤ 7 dB, claim 10 is no start-up sweeps, and claim 11 is block-size invariance.

## Evidence consulted

- The round-3 lab and its trace
- ADR-195
- ADR-197, the Blind-Spot Armor charter text in `CLAUDE.md` (silent clamps; default changes)
- The lead's relay of Sluice seq 36 (prose)
- `tools/serve_labs.py` and `tools/gen_lab_index.py`

## Alternatives rejected

- A full dry bypass for reversals: 8.5 dB.
- A reach-proportional bypass: 12 dB.
- The limiter on by default: −36 dB colouring, and a silent clamp.
- A linear release tail: zero is already reached exactly, with a −100 dB final step.

## Verify

`./verify fast` and `./verify full` run on the pushed head. Their JSON lines go in the PR body and the report back. This trace is committed before those runs.

## Open questions

1. Which layout lands.
2. p = 1 is a lab default changed on measurement; a product default needs an ADR and a migration (ADR-197).
3. Whether the demo still reads muddy; the stand-ins are crude.
4. The reversal ceiling is close to its plant (5.9 / 7 / 7.6).
5. (a) and (b) now differ only on cycles through a delay line or of three modules or more.
6. Still open from round 3: glide τ, the per-cable-threshold reading, the declared gains, ADR-195's "brief".
7. The self-check is not wired into `./verify`.
8. The two follow-up commits reached `main` through the merge of #980 without a separate review.
