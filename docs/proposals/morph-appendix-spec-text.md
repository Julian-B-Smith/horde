# The morph appendix: spec text for SPEC-ENGINE (B585 step 4's input), revision 4

*Origin: horde lead session, 2026-10-29.*
- *ADR-324 (horde #1274) rules B585's twelve questions and places the morph in one normative appendix of SPEC-ENGINE.*
- *Revision 1 (`c75fa9f`, horde #1275) was reviewed by an independent critic: "Not ready; ready after the listed fixes".*
- *The human ruled its open points X28–X35 (ADR-325).*
- *Revision 2 (`6e6bf1e`) applied those findings and rulings (§6). The critic's re-review found no blockers: "Ready once
  the five MAJOR fixes are made". It independently reproduced all 24 of 34.3's quantum picks.*
- *The human ruled X36–X40 (ADR-326), and ADR-319 item 1 settles the morph off.*
- *Revision 3 (`4b1aa93`) applied them (§7). The critic's diff check found its five majors and every minor fixed, and four
  new majors in its new material. The human ruled X41 and X43, and re-ruled X42 ("With the other corner values").*
- *This revision 4 applies those four majors, six minors and the rulings. §8 maps each to where it is fixed.*

*Drafted by a scoped implementer. Status: **PROPOSED**. The lead applies it to `specs/SPEC-ENGINE.md` after the critic's
re-read and the human's ratification. This file does not touch `specs/`, `engine2/` or the rack contract.*

**How to apply it.**
- §1 is the new **Appendix D**. It goes after Appendix C.
- §2 lists every amended row, as before → after, at `2af41d8`.
- §3 holds the suite's new items, paste-ready for §10.1.
- §4 is the diff summary. §5 lists the open points that remain. §6, §7 and §8 are the fix tables of revisions 2, 3 and 4.

Text to paste sits in quoted blocks or tables; the rest is commentary.

**Taken as given.**
- **ADR-324:** H1–H12, and one appendix.
- **ADR-325:**
  - X28: the clean room builds the resolver, beside the engine.
  - X29: `morph.count` defaults to Four.
  - X30: the count is on every parameter, and the smallest wins.
  - X31: morph glide acts on the resolver's output, before PA-1.
  - X32: the draw is keyed per parameter, permanently. This is B308 H3's ADR; ported legacy presets change once.
  - X33: a 0.1 band.
  - X34: synced steps run while the transport is stopped, and RC-7.3 is amended.
  - X35: weights are values glided by PA-1; a flip inside a blend stays raised-cosine.
- **ADR-326:**
  - X36: the ranges and tapers as proposed.
  - X37: the divisions run from 1/32 to 4 bars, with triplet and dotted toggles, standard across every part that syncs
    (B590; RC-7.4).
  - X38: the bank carries on.
  - X39: the morph's settings and position are host parameters, but not the seed.
  - X40: each corner keeps its own `ratios`, which is a key of the swarm group under the law.
- **ADR-319 item 1:** with the morph off, the chosen audible corner sounds (A by default).
- **The human, on revision 3's points:**
  - X41: per control, saved with the patch; triplets 1/16T to 4 bars, dotted values 1/32 to 2 bars; the toggles filter the menu only.
  - X43: `morph.audible` is a page choice, not a host parameter.
  - X42: "With the other corner values". Each corner's `ratios` texts are morph state beside the corners' numbers (RC-11.1), and IF-4's state does not change.

**The two-setting law's continuity**, which the critic confirmed. It is discontinuous only where the top three pooled
weights all tie: the centre with four distinct settings, (0.5, ⅓) when C = D, and nowhere when B = C. A path through that
point (the A–D diagonal with four distinct corners) is a switch there. That is the price of the Two setting, and MO-18
says so.

---

## 1. The new appendix (PROPOSED)

> ## Appendix D. The morph (normative)
>
> The morph holds four **corners**, each a full set of the manifest's values, and a **position** on a square **pad**.
> This appendix states how a position becomes the values and the structure the engine renders: the morph's clock and
> glide, its settings, the quantum draw, the laws (Blend, Quantum, Stepped), each parameter's own morph mode and count,
> how many structural settings sound at once, and how a structural difference is rendered. Rows MO-1 to MO-20 are the
> **resolver**, built by the clean room beside the engine (ADR-325). Rows SB-1 to SB-13 are the engine's rendering of
> structure.
>
> ### D.0 Who computes what, and when
>
> | id | requirement |
> |---|---|
> | MO-1 | **Two halves.** The **resolver** maps the corners, the position, the morph settings (D.2) and each key's mode and count (D.4) to events for the engine (MO-20): one value for each Morphable key and each Structural key that resolves to one value, sent as `Param` events, and a **setting set** for each structural group that blends, sent as a `SettingSet` event (IF-12). The engine applies `Param` events as any (PA-1, PA-5, PA-6) and renders setting sets by SB-1 to SB-13. The corners, the position, the morph settings and the modes are the morph system's state (RC-11.1); the resolver receives them through IF-12 and saves none of them. |
> | MO-2 | **Device keys** are outside the morph. The resolver never emits them; they are never blended and never flipped (5.1). |
> | MO-3 | **Clock, cadence, prepare and reset.** The resolver keeps a sample clock of its own, advanced by the samples it is asked to process and counted from its reset exactly as PA-3 counts the engine's, so its control intervals coincide with the engine's. It resolves once at the first sample of every control interval. An input (a position, a transport event, a setting, a mode, a corner edit) takes effect at the first interval boundary at or after its sample time. Its events are timed at that boundary. Prepare (given the host rate) allocates everything it will use (RT-1). Reset sets its clock to 0, clears every latch (MO-8: no corner held), clears the stepped hold (MO-10), snaps the glide (MO-5) to its targets, and sets the step phase to 0. After reset it resolves exactly as a freshly prepared resolver with the same inputs (DT-6's rule). It reads no clock (DT-5), allocates nothing after prepare, and its output is independent of block size (DT-2). |
>
> ### D.1 The weights and the glide
>
> | id | requirement |
> |---|---|
> | MO-4 | **Corners and weights.** The corners sit at A = (0, 0), B = (1, 0), C = (0, 1), D = (1, 1). At a position (x, y) in [0, 1]², the **corner weights** are bilinear: w = [(1 − x)(1 − y), x(1 − y), (1 − x)y, xy], in that order, each in [0, 1], summing to 1. Every law reads the position only through these weights, at the stepped position under Stepped (MO-10). |
> | MO-5 | **Morph glide** (ADR-325: on the resolver's output, before PA-1). Two glides, each a one-pole of time constant `morph.glide` seconds, stepped once per sample on the resolver's clock with the coefficient 1 − e^(−1/(f_host·`morph.glide`)), and bypassed at 0. **(a) The weights:** the four corner weights w are glided together, before every law, and snap together to their targets once all four are within 10⁻⁶ of them. The glided weights stay in [0, 1] and sum to 1, and every setting set (MO-17) is computed from them. **(b) The Morphable values:** each Morphable value the resolver emits is glided in its normalised form v (circular keys the shortest way round), and snaps to its target once within 10⁻⁶. Under Blend, for a key whose law is not `lin, wraps` and whose count is 4, (b) applied to the blend of fixed corner values gives the same value as the blend at the glided weights; for circular keys and at count 2 it is a glide of the value, not of the weights. Under Quantum it softens a Morphable key's flip. **For structure (ADR-325, X31):** at count 4, gliding the corner weights is exactly a glide of the set's weights on the output, since the set's weights are sums of corner weights; at count 2 it is a glide in weight space, because a glide of the output weights is undefined when the pair changes. **Never glided:** a setting set's own weights (the engine glides them, PA-1, SB-2), Quantum's picks (MO-7 reads the unglided weights), and Structural keys that resolve to one value (they flip by PA-5). |
>
> ### D.2 The morph settings
>
> The morph's settings belong to the patch, are saved with it by the morph system (RC-11.1), and are never morphed.
>
> | key | label | range | default | what it does |
> |---|---|---|---|---|
> | `morph.on` | Morph | off, on | off | While off, corner `morph.audible` sounds and the resolver emits nothing after MO-20's hand-over (ADR-104: morph ships off; ADR-319 item 1). |
> | `morph.audible` | Audible corner | A, B, C, D | A | The corner that sounds while the morph is off, whatever the position (ADR-319 item 1). |
> | `morph.law` | Morph mode | Blend, Quantum, Stepped | Quantum | The law every Follow key obeys (D.3). |
> | `morph.count` | Count | Two, Four | Four | How many corner settings may sound, or blend, at once under Blend and Stepped (MO-9, MO-17). |
> | `morph.temperature` | Temperature | 0.02 to 4, log | 1 | How sharply each corner owns the pad under Quantum (MO-7). |
> | `morph.cohesion` | Cohesion | 0 to 1, lin | 0.3 | How far all keys pick the same corner together under Quantum (MO-7). |
> | `morph.seed` | Morph seed | 1 to 9,999, step | 1024 | Seeds the quantum draw (MO-6). |
> | `morph.stepRate` | Step rate | 0.25 to 32 steps per second, log | 4 per second | How often Stepped takes a new position while `morph.stepSync` is off (MO-10). |
> | `morph.stepSync` | Sync | off, on | off | On: steps follow the host's musical position at `morph.stepDivision` (MO-10, RC-7). |
> | `morph.stepDivision` | Division | an index into RC-7.4's table, 0 to 21, step | 3 (1/16) | The step's length while `morph.stepSync` is on. |
> | `morph.stepTriplet` | Triplets | off, on | off | Offers the table's triplet rows in the Division menu (RC-7.4); changes no sound. |
> | `morph.stepDotted` | Dotted | off, on | off | Offers the table's dotted rows in the Division menu (RC-7.4); changes no sound. |
> | `morph.glide` | Morph glide | 0 to 5 s, pow | 0.008 s | MO-5. |
>
> The ranges and defaults of on, law, temperature, cohesion, seed and glide are the legacy shell's (its ids 151, 154 to
> 158; law 0 was quantum); the tapers and the step rate's range are ADR-326's (X36). **Every setting but `morph.seed` and
> `morph.audible` is a host parameter**, and so are the position's x and y: they come from a morph manifest appended to
> the lockfile (IF-11; ADR-326, X39; `morph.audible` is a page choice, the human's ruling on X43). Each is one lockfile row,
> with one range, taper and step count, which is why the rate and the division are two settings. The engine never
> receives them; they are the resolver's inputs (IF-12). [X44]
>
> ### D.3 The draw and the laws
>
> | id | requirement |
> |---|---|
> | MO-6 | **The quantum draw** (ADR-104; ADR-325 for X32: keyed per key, permanently, which is ADR-186 §5(g)'s ADR; ADR-185's rule then holds by construction). A stream for a purpose p and the seed S = `morph.seed` is a mulberry32 generator whose 32-bit state starts at h, the FNV-1a hash (offset basis 0x811C9DC5, prime 0x01000193, arithmetic modulo 2³²: h ← (h XOR b)·0x01000193 for each byte b) of the UTF-8 bytes of p followed by the four bytes of S, least significant first. Each step does: a ← a + 0x6D2B79F5; t ← (a XOR (a >> 15))·(a OR 1); t ← (t + (t XOR (t >> 7))·(t OR 61)) XOR t; out ← t XOR (t >> 14). Every operation is modulo 2³² on unsigned values, and >> is a logical shift. The draw is u = out / 2³², clamped to [10⁻⁹, 1 − 10⁻⁹]. A key k's draws are gₖ[c] = −ln(−ln u), the first four outputs of the purpose `morph/key/` followed by k's manifest key, for c = A, B, C, D in order. The **shared** draws g*[c] come the same way from the purpose `morph/shared`, which no manifest key can produce, since keys hold no `/`. **No draw depends on a key's position in any list or on how many keys exist**, so appending a key never changes how an existing patch sounds. Blend and Stepped read no draw. |
> | MO-7 | **Quantum's pick.** For a key k at the unglided weights w (the stepped position's under Stepped), each corner scores sₖ[c] = λ[c] + (1 − h)·gₖ[c] + h·g*[c], with λ[c] = ln(w[c])/T where w[c] > 10⁻⁹ and −10¹² otherwise; T = max(0.02, `morph.temperature`) and h = `morph.cohesion`. The key's **pick** is the corner with the highest score (the lowest corner on a tie). A Morphable key emits the pick's value (glided by MO-5 (b), then PA-1). A Structural key resolves to it as one value, held by MO-8. |
> | MO-8 | **The latch: a band of 0.1** (ADR-325). A Structural key keeps the corner it holds until another corner's score exceeds the held corner's by more than m = ln(0.55/0.45)/T. On an edge where two corners' draw terms are equal, that is a band of exactly 0.1 of the pad, centred on the even split; elsewhere it is the same margin in score. A key holds no corner before its first resolution after reset, and takes its pick then. Morphable keys have no latch. |
> | MO-9 | **Blend.** A Morphable key's value at count 4 is the blend of its four corner values at MO-4's weights, unglided (MO-5 (b) then glides the value): Σc w[c]·v[c] in the normalised form v. For a key whose law is `lin, wraps` (a circular key), it is the angle of the weighted mean of unit vectors, frac(atan2(Σc w[c]·sin 2πv[c], Σc w[c]·cos 2πv[c]) / 2π); where that mean's length is below 10⁻⁹, the value of the heaviest corner (the lowest on a tie). At count 2 (MO-11), the candidates are the key's distinct corner values, each with the pooled sum of the unglided weights of the corners holding it (as MO-17 pools settings); MO-17's two-setting law selects at most two of them and weighs them, and the same sums run over those, with those weights. A Structural key blends by D.5. |
> | MO-10 | **Stepped** (B424). The resolver keeps a **step phase** φ and a **stepped position**. At reset, φ = 0 and the stepped position is the position at the first resolution. **φ in closed form.** When `morph.stepSync` is off, φ(n) = φ₀ + (n − n₀)·`morph.stepRate`/f_host, where n₀ is the sample of the last change of the rate, of sync, or of the transport's play state, and φ₀ is φ(n₀); so φ never jumps at a rate change, and there is no rounding drift at fractional samples per step. When it is on and the transport is playing, φ(n) = (the host's musical position at n in quarter notes, RC-7.2)/(the length of `morph.stepDivision`'s row of RC-7.4's table, in quarter notes). When it is on and the transport is stopped (ADR-325, X34), φ(n) = φ₀ + (n − n₀)·tempo/(60·f_host·that length). A jump (RC-7) sets φ to the new position's. **A step falls at the first resolution at which ⌊φ⌋ differs from its value at the previous resolution**, so a backward jump (a loop's wrap) that changes ⌊φ⌋ is a step too, and the stepped position takes the current position. Between steps it holds. Stepped is Blend (MO-9, D.5) at the stepped position. **Every key reads the stepped position under Stepped: Follow keys, Quantum-mode keys' picks, and flip-only keys.** Values and structure weights therefore move in steps, each step's edge glided by MO-5 and PA-1. |
>
> ### D.4 Each key's own mode and count (B557)
>
> | id | requirement |
> |---|---|
> | MO-11 | **The menu.** Every Morphable and Structural key carries two per-patch choices, saved by the morph system and never morphed. **Mode:** Follow (the default: it obeys `morph.law`), Quantum (it flips at its quantum boundaries, MO-7, whatever `morph.law` is) or Exempt (MO-12). **Count:** As the morph (the default: `morph.count`), Two or Four. Count acts only while the key blends: Follow under Blend or Stepped. For a Morphable key it chooses MO-9's count; for a Structural key it bounds its group's count (MO-17: the smallest wins, ADR-325). |
> | MO-12 | **Exempt.** An Exempt key holds corner A's value at every position and never blends or flips. The morph's editor writes the held value into all four corners when a key is made Exempt (ADR-109), so corner A's is the held value. |
> | MO-13 | **Quantum on a Structural key.** A Structural key whose mode is Quantum resolves to one value under every law, flipping at its own quantum boundary (MO-7, MO-8) by PA-5. Under Blend this is IF-4's behaviour before ADR-317. |
>
> ### D.5 Structure
>
> | id | requirement |
> |---|---|
> | MO-14 | **Structural groups.** The Structural keys that are not flip-only (MO-15) form these groups, each named by its id. Group ids use a `/`, so no id is a manifest key. (1) **By gain:** `oscN/env` (that oscillator's `env`), `noise/env` (the noise's `env`), and `oscN/pan` (`panImage` and `panInvert`). (2) **`oscN/kernel`:** `baseWave`, `stacking`, `spreadLaw`, `quantize`, and every row of 5.4 for both blades. (3) **`oscN/swarm`:** `unison`, `distribution`, `seed`, `detuneLaw`, `detuneRange`, `pivot`, `octave`, `semitone`, and the oscillator's **`ratios` text**, named `oscN.ratios` for the morph (ADR-326, X40: each corner keeps its own, in the morph system's corner state, RC-11.1). Two texts are the same setting when their **parsed** lists are equal: the same count of ratios, each the same number, so spacing and spelling (`1, 5/4` and `1,5/4`, or `5/4` and `1.25`) do not matter. A text is at most 256 bytes of UTF-8 [X45]. `oscN.ratios` carries MO-11's mode and count like any key. If the four corners hold more than one setting of this group's blending keys, the kernel's keys join `oscN/swarm` and `oscN/kernel` does not exist. This is decided over the four corners, so it is fixed for the patch and changes only with a corner edit or a change of mode. It does not change with the position. (4) **`sub/chain`:** the sub's Structural rows but `voiceMode` and `priority`, with `env` included. The sub applies its envelope inside its chain ahead of the drive (SN-3), so `sub.env` cannot act by gain. (5) **`noise/chain`:** the noise's Structural rows but `env` and the reserved rows. (6) **`voice/filter`:** the filter's own Structural keys, when a filter is assigned (RC-1). (7) **`fx/N`:** each FX slot's structure, whose keys the rack defines (RC-5). A **setting** of a group is one value for each of its blending keys (MO-16). |
> | MO-15 | **Flip-only keys.** `voice.mode`, `oneShot`, `retrigger`, `sub.voiceMode`, `sub.priority` and every reserved row never blend: they act on the voice law or at a voice's start, where one note cannot hold two values. Each resolves to one value under every law and every mode **but Exempt** (MO-12). That value is its own pick and latch (MO-7, MO-8), at the stepped position under Stepped, so under Blend it is the seeded point of IF-4 before ADR-317. It changes on sounding voices by PA-5, or by VO-14 for `voice.mode`. |
> | MO-16 | **Which keys blend.** A key of a group **blends** when its mode is Follow and the law is Blend or Stepped. Every other key of the group resolves to one value (its pick under Quantum, its held value under Exempt), which the resolver emits as a `Param` event. Settings are formed over the blending keys only. |
> | MO-17 | **The setting set.** A blending group's candidates are the distinct settings its four corners hold. A candidate's **id** is the lowest corner index (A = 0 … D = 3) holding it. Its **pooled weight** Wₛ is the sum of the glided corner weights (MO-5 (a)) of the corners holding it. The group's **count** n is the smallest Count among its blending keys (As the morph reads `morph.count`; ADR-325), and is 2 for `fx/N` whatever the choice. **At n = 4**, or with at most two candidates, the set is every candidate with Wₛ > 0, at weight Wₛ. **At n = 2** with three or more candidates, the candidates are ordered by Wₛ, highest first, ties broken by the lower id. With W₁ ≥ W₂ ≥ W₃, the set is the first two, at weights (W₁ − W₃)/(W₁ + W₂ − 2W₃) and (W₂ − W₃)/(W₁ + W₂ − 2W₃); where W₁ + W₂ − 2W₃ = 0 (the top three tie), the set is the first alone, at weight 1. A set's weights sum to 1. |
> | MO-18 | **Continuity at n = 2.** The two-setting law is continuous wherever the top three pooled weights do not all tie. Where the second and third trade places, both have weight 0. It is discontinuous only at a point where they all tie: the pad's centre when the corners hold four settings, or (0.5, ⅓) when they hold three with C = D. Near that point the weights turn steeply. **A path through that point is a switch there.** With four distinct corners the A–D and B–C diagonals pass through it: that is correct, and it is what the Two setting trades for its cost. The engine's slot rule (SB-11) carries the change. |
> | MO-19 | **An FX slot's set** (ADR-324 H10). An `fx/N` group is resolved like any other, with n = 2, so at most two settings of a slot sound at once. Its sets go to the rack, never to the engine (IF-12). How a slot renders its set is RC-5's. |
> | MO-20 | **What the resolver emits, and in what order.** At each control interval boundary it emits, as needed, **first** the `SettingSet` events (IF-12) of the blending groups whose set (key list, values or weights) changed, **then** a `Param` event for each Morphable key whose glided value changed and for each Structural key that resolves to one value whose value changed. A one-value `oscN.ratios` arrives in the `Param` phase as the engine's `ratios` text input, the same input IF-4's `ratios` feeds. So a key that stops blending is already absent from its group's latest set when its `Param` event arrives (PA-7). **The order of several sets at one boundary** is fixed: osc 1's groups, then osc 2's, then `noise/env`, `sub/chain`, `noise/chain` and `voice/filter`. Within an oscillator the order is `oscN/pan`, `oscN/env`, `oscN/kernel`, `oscN/swarm`, except at an un-merge. **A change of merge** (MO-14 item 3, made by a corner edit or a mode change) is sent in an order the engine can accept (PA-7). At a **merge**, `oscN/kernel` first gets a one-setting set (its current heaviest kernel setting, the lower id on a tie), and then comes the merged `oscN/swarm` set, which lists the kernel's keys. At an **un-merge**, the `oscN/swarm` set without the kernel's keys comes first, then the `oscN/kernel` set. A group that stops blending (a mode change, or the corners coming to agree) gets one last set of one setting, which equals `Param` events setting its values. **When `morph.on` turns off** (ADR-319 item 1), the resolver emits the values of corner `morph.audible`, whatever the position: for each blending group a set of one setting equal to that corner's setting, then a `Param` event for every other key the morph resolves (the `ratios` texts included). A change of `morph.audible` while the morph is off emits the new corner's values the same way. Its Morphable values glide by MO-5 (b), and its Structural values change by PA-5. Otherwise the resolver emits nothing while the morph is off. `fx/N` sets go to the rack (IF-12). |
>
> ### D.6 Rendering a setting set (the engine's half)
>
> | id | requirement |
> |---|---|
> | SB-1 | **One render per setting.** For each setting of a group's set, the engine renders the group once. (1) **By gain:** no render. The stage applies Σₛ wₛ·gainₛ, which is the same sound as mixing renders because the stage is linear in the signal. (2) **`oscN/kernel`:** the member kernel (the base wave, both blades and their edge corrections), once per setting, from the oscillator's one member state. (3) **`oscN/swarm`:** a whole oscillator instance per setting (SB-4). (4) and (5) **`sub/chain`, `noise/chain`:** the chain, once per setting, at the points PA-5 names for each. (6) **`voice/filter`:** one filter instance per setting (RC-1). (7) **`fx/N`:** by RC-5. A set is never rendered as a product of groups (MO-14 item 3). |
> | SB-2 | **The mix.** A group's output is Σₛ wₛ·yₛ, with equal gain (RS-3.6 of the rack's contract, ADR-324 H11). Each weight is a value glided by PA-1, per voice. The glided weights sum to 1 within 10⁻⁵. |
> | SB-3 | **The kernel's one member state.** Every setting of a kernel group renders from the oscillator's one member state, as PA-5's crossfade does. Where the member state reads the output (feedback and cross-mod, C.14), it reads the mixed output Σₛ wₛ·yₛ. |
> | SB-4 | **Several swarm instances.** While `oscN/swarm` holds several settings, each setting is a whole oscillator instance, with its own members (SW-1), its own coupling, drift and frame, and its own feedback and cross-mod loop, which reads that instance's output only. (a) Its per-note streams (DT-4) are seeded from that setting's `seed`. The noise oscillator's seed ("the voice's seed", osc 1's) is the `seed` of osc 1's heaviest setting at the voice's fresh start (the lower id on a tie), and is kept for the note. (b) The ensemble's persistent state (DT-4, C.7) is kept once per distinct swarm setting among the four corners (its full tuple of values; at most four per oscillator), each with its own stream seeded from that setting's `seed`; placement (C.2) is computed per instance from its own `unison`, `distribution` and `seed`. An instance uses its setting's state; a corner edit that makes a new setting starts its state fresh. (c) The sources the engine publishes for the oscillator (IF-3) are the mixture's: R·e^{iψ} = Σₛ wₛ·Rₛ·e^{iψₛ}, and the onset burst's level is Σₛ wₛ·levelₛ. (d) The observation view (IF-10) reports members per instance, with the instance's setting id. |
> | SB-5 | **A setting at weight 0 costs nothing.** A setting whose heard weight is exactly 0 renders nothing and counts nothing (IF-9). Where a group's set holds one setting, and at every corner, exactly one setting renders. |
> | SB-6 | **Waking.** A setting that enters a slot (SB-11) while a voice sounds starts from its fresh-start state, at the weight PA-1 raises from 0. For a swarm that is a fresh start of its members (ADR-324 H9). Its per-note streams are keyed by purpose, note counter, member and (for the swarm) its seed, never by the slot that renders it, so a setting renders the same in any slot. **Each kernel setting holds its own copy of the per-note blade-noise streams** (BL-18), started from the note's seed at the setting's start, so settings never consume each other's draws. |
> | SB-7 | **A voice that starts in a blend.** A voice's fresh start loads every setting of each group's current set into its slots, each from its fresh-start state, with its weight set to the target, not glided. The note is the blend from its first sample. **A legato change** (VO-6) keeps every setting running, with its phases and states continuous, as it keeps one setting today. |
> | SB-8 | **A flip inside a blend.** When a key of a blending group that resolves to one value (MO-16) changes, every live setting of the group changes with it at once: by PA-5's 5 ms raised-cosine crossfade, except that `octave` and `semitone` change the frequency at once with every phase continuous (PA-5), and `unison` fades members in and out by PA-6. During a PA-5 crossfade, at most twice the group's settings render. A second change waits until it ends, as PA-5 says. |
> | SB-9 | **Limits.** At most four settings per group sound, at most two for a group whose count is 2, and twice those during SB-8's crossfade. The engine is sized for this (RT-1). |
> | SB-10 | **Corner exactness.** At each corner, and wherever every group's set holds one setting, the output is bit-identical to the same patch with the morph off and those values stored. |
> | SB-11 | **The slot rule.** Each voice keeps, per blending group, as many **slots** as the group's count n. At each control interval the engine applies the group's current set as follows. (a) **Matching settings, by value.** An entry matches a slot's setting when their values agree on every key the entry lists, the setting's value for a key being its value in the set that brought it, or, for a key it received since by a `Param` event (a key that has joined the list, Quantum to Follow), that `Param`'s value. `oscN.ratios` compares parsed lists (MO-14). The id is only a label. A matched setting takes the entry's id, and its weight as the target. Where several slot settings match one entry, the one with the largest heard weight takes it (the lower slot on a tie) and the others take target 0; where one slot setting matches several entries, the entry with the lower id takes it. An entry that matches no slot setting is a new setting, and a slot setting that matches no entry takes target 0. So a corner edit that only relabels a setting (A and B hold X with id 0, and A is edited, so X's id becomes 1) keeps it running, with no restart. **A key that leaves a group's key list** (a mode change from Follow) arrives as its `Param` event and applies to every live setting by SB-8; the settings keep their slots and match on the remaining keys, so none re-enters. **A change of merge** (an `oscN/swarm` set whose key list gains or loses the kernel's keys) replaces both groups' settings: every slot setting of `oscN/kernel` and `oscN/swarm` counts as matching no entry, so the old settings fade by PA-1 and the new sets' settings enter by (b) and (c), with no matching across the change. (b) **Entering.** A setting not in a slot enters a **free** slot: an empty one, or one whose setting has target 0 and a heard weight below 10⁻⁴. It starts by SB-6. A freed setting's remaining heard weight r is added to the heard weight of the slot setting with the largest heard weight (the lower id on a tie): both of PA-1's error states of that setting rise by r, exactly as if its target had stepped down by r, so it glides back to its target along PA-1's curve. So the weights still sum to 1, and the output steps by under 10⁻⁴ of the two settings' difference (below −80 dB). (c) **Waiting.** While a setting of the set waits for a slot, its weight goes to the slot settings that are in the set, in proportion to their set weights. If no slot setting is in the set, the slot setting with the largest heard weight (the lower id on a tie) takes target 1 and the others 0, so the others empty and free their slots. (d) **A lower count.** When a group's count falls, slots above the new count admit no setting and are removed once freed. So any change of set, including the path through MO-18's point, a corner edit, a chimera that Quantum-mode keys make, and a change of count, law or mode, is a sequence of PA-1 glides, with never more settings sounding than SB-9 allows. |
> | SB-12 | **Per voice, and the mono sub.** Sets are the instrument's (one pad). Slots, heard weights and settings' states are per voice. The mono sub (SN-7, RC-6.6) keeps its `sub/chain` slots once for the instrument. |
> | SB-13 | **A voice's life while an envelope selector blends.** While a source's `env` group (`oscN/env`, `noise/env`) or the sub's chain holds several settings, the source follows each envelope its settings name, and each counts among the voice's governing envelopes (6.4) while its setting's heard weight is above 0. So the voice is freed only when every such envelope has finished (6.4's rule), and an envelope whose setting has faded to weight 0 no longer holds the voice. |

---

## 2. Every amended row, before → after (PROPOSED)

Line numbers are those of `specs/SPEC-ENGINE.md` on `origin/main` at `2af41d8`. The critic checked every before-text of
revision 1 verbatim; the ones new in this revision are quoted from the same blob. Where only part of a row changes, the
rest is marked *(unchanged)*.

### 2.1 The front matter and the build

**Line 9, "The laws are normative".**
- Before: "…Appendix C states it as math with its constants and units. …"
- After: "…Appendix C states it as math with its constants and units, **and Appendix D states the morph's laws the same
  way.** …" *(rest unchanged)*

**Line 12, "How to read it".**
- Before: "- Requirements are numbered (`SW-4`, `VO-3`, `RC-2`, …); laws are numbered `C.0` to `C.16`. Each is testable."
- After: "- Requirements are numbered (`SW-4`, `VO-3`, `RC-2`, `MO-6`, `SB-1`, …); laws are numbered `C.0` to `C.16`, and
  the morph's are Appendix D's rows. Each is testable."

**Line 34, §1 "Out of scope".**
- Before: "…the macros and intent bus, the morph engine, the pitch system…"
- After: "…the macros and intent bus, **the morph's editor, its corner storage and its pad (its resolver is in scope,
  Appendix D)**, the pitch system…" *(rest unchanged)*

**§1 "In scope" gains a bullet,** after "The engine's interface to the rest of the instrument…":
> - The **morph resolver** (Appendix D, ADR-325): beside the engine, it turns the corners and a pad position into values
>   and structural setting sets; and the engine's rendering of those sets in the voice.

**§3, "What the build delivers", gains a row,** after "The engine's API":
> | The morph resolver | Appendix D's resolver (MO-1 to MO-20), beside the engine, with its own calls (IF-12): corners, position, settings, modes and transport in; `Param` and `SettingSet` events out. |

**§3's proposed layout gains a line:**
> `  morph/      the resolver: weights, glide, the draw, the laws, the modes, setting sets (Appendix D)`

**BD-8 gains a stage 7 amendment,** in the style of stage 5's. Insert it after "(7) modulation, morph and state (item 13's
render half: reset to fresh, migration rendered; items 14, 20, 21, 23);":
> with a **stage 7 amendment** (B585, ADR-324, ADR-325): the morph resolver and the structure blend (Appendix D), built on
> the accepted stages and judged by items 34 and 35. It reopens:
> - **stage 2:** the voice's slots per structural group, SB-7 and SB-12, VO-6's second voice state per setting, and RT-1's sizing;
> - **stage 3:** a swarm's instances per setting, SB-4;
> - **stage 5:** the kernel per setting on one member state, SB-3;
> - **stage 6:** the sub's and the noise's chains per setting, and the envelope selectors by gain.
>
> Every accepted item stays green. Item 19 is judged as RC-7.3 now reads. The bank stage carries on and does not wait on
> it (ADR-326, X38); the amendment reopens the stages above when it lands, and a corner, or the morph off, stays
> bit-identical (SB-10);

**§4.2, the per-voice signal path, gains a bullet** under the diagram:
> - Where a structural group blends (Appendix D), the stage it names runs once per setting and the renders are mixed at
>   its output (SB-1); a blending swarm runs whole oscillators, one per setting (SB-4). The diagram shows one setting.

### 2.2 The glossary (line 68), and three new terms

- Before: "| **Morphable / Structural / Device** | The three parameter classes: Morphable values blend between morph corners;
  Structural values are owned by one corner and flip; Device values stay outside the morph. |"
- After: "| **Morphable / Structural / Device** | The three parameter classes: Morphable values blend between morph corners
  or flip, by the morph's law; Structural values follow the morph's law too: where corners differ, Blend and Stepped render
  each differing setting and mix the renders, and Quantum flips between them, while a few keys only flip (Appendix D);
  Device values stay outside the morph. |"
- Added rows:
> | **corner**, **pad**, **position** | The morph's four full sets of values (A, B, C, D), the square they sit on, and the point on it that is heard (MO-4). |
> | **structural group** | The Structural keys rendered together (MO-14): a kernel, a swarm, the sub's or the noise's chain, the filter, an FX slot, or the keys that act by gain. |
> | **setting set** | For a structural group that blends, the distinct settings its corners hold that sound at a position, each with an id and a weight, the weights summing to 1 (MO-17). |

### 2.3 §4.3 (line 169)

- Before: "| LFOs, the matrix and its routes (ENV 2 to 4's included), macros, intents, morph | the modulation and morph systems |
  IF-3, IF-4, RC-4 |"
- After: "| LFOs, the matrix and its routes (ENV 2 to 4's included), macros, intents | the modulation system | IF-3, RC-4 |",
  plus a new row: "| the morph's laws; its structure's rendering | the morph resolver; the engine (voice) | IF-4, IF-12,
  Appendix D |"

### 2.4 §5.1, the Class bullet (line 194)

- Before: "- **Class**: `M` Morphable, `S` Structural (flips at a morph corner boundary), `D` Device (outside the morph). [Q17]"
- After: "- **Class**: `M` Morphable, `S` Structural (follows the morph's law: rendered per corner setting under Blend and
  Stepped, flipped under Quantum; flip-only keys always flip, MO-15), `D` Device (outside the morph). [Q17]"

### 2.5 SW-1 (line 435)

- Before: "| SW-1 | Each oscillator of each voice is a swarm of U members (`unison`), each a phase-continuous oscillator at its own
  frequency around the oscillator's pitch. …"
- After: "| SW-1 | Each oscillator of each voice is a swarm of U members (`unison`), each a phase-continuous oscillator at its own
  frequency around the oscillator's pitch; **while its swarm group blends, it is one such swarm per setting (SB-4)**. …"
  *(rest unchanged)*

### 2.6 VO-6, and the start table

VO-6 is unchanged in its words. RT-1 (§2.12 below) sizes "a second voice state for VO-6's mono re-run crossfade" to
include that state's settings.

The start table (after VO-14) gains a row:
> | the structural settings (SB-7) | each group's current set loaded, each setting fresh, weights at their targets | every setting continues |

### 2.7 PA-1 (line 549)

- Before: "…A change of any continuous parameter from the host, automation, a macro, an intent or the morph glides to its new
  value…"
- After: "…A change of any continuous parameter from the host, automation, a macro, an intent or the morph, **and each weight
  of a structural setting set (SB-2)**, glides to its new value…" *(rest unchanged)*

### 2.8 PA-5 (line 553), three edits

1. Before: "…from the old setting's sound to the new; the same holds when the morph flips one."
   After: "…from the old setting's sound to the new; the same holds when the morph flips one (a key that resolves to one
   value, MO-16, or a flip-only key, MO-15)."
2. Before: "…(changes in between are skipped), so at most two settings ever sound at once."
   After: "…(changes in between are skipped), so at most two settings of a key ever sound at once **outside a structure
   blend; inside one, SB-8 and SB-9 hold**."
3. Before: "`octave` and `semitone` change the frequency at once with every phase continuous, so they need no crossfade;"
   After: "`octave` and `semitone` change the frequency at once with every phase continuous, so they need no crossfade
   (in every live setting at once under a blend, SB-8);"

### 2.9 PA-7 (line 555)

- Before: "…**a `Param` event whose id names no host parameter (an unknown id, or a reserved row's, §5.1) is refused and
  nothing is stored** (round 28, the counter `refused_param_id`); each refusal…"
- After: "…**a `Param` event whose id names no engine parameter (an unknown id, a reserved row's, §5.1, or one of the
  morph's host parameters, IF-11, which the shell routes to the resolver) is refused and nothing is stored** (round 28,
  the counter `refused_param_id`); **a `SettingSet` event (IF-12) is refused whole, and
  nothing of it is stored, when it names no group, gives n other than 2 or 4 (or other than 2 for an FX slot), holds more
  entries than n, repeats an id, names an id outside 0 to 3, lists keys that are not that group's or a key twice, gives a
  value outside a key's range or options (or a `ratios` text its parser refuses), or gives a weight that is not finite,
  is negative, or makes the weights' sum differ from 1 by more than 10⁻⁶; or names an `fx/N` group, whose sets go to the
  rack; or is an `oscN/kernel` set while that oscillator is **merged** (its latest `oscN/swarm` set lists kernel keys,
  MO-14 item 3), or an `oscN/swarm` set that lists kernel keys while that oscillator's `oscN/kernel` group is **live**
  (its latest received set holds more than one setting; MO-20 sends it one setting first at a merge) (the counter
  `refused_setting_set`). A `Param` event for a key listed in its group's latest received set, when that
  set holds more than one setting, is refused (the counter `refused_param_in_set`); "holds" means the latest set
  received, not settings still fading in slots (SB-11)**; each refusal…" *(rest unchanged)*

### 2.10 IF-3 (line 670)

- Before: "Sources the engine publishes per voice: the levels of ENV 1 to ENV 4 (EN-5), R and ψ of each oscillator's swarm,
  the onset burst's level, …"
- After: "Sources the engine publishes per voice: the levels of ENV 1 to ENV 4 (EN-5), R and ψ of each oscillator's swarm
  **(of the mixture while its swarm blends, SB-4)**, the onset burst's level, …" *(rest unchanged)*

### 2.11 IF-4 (line 671)

- Before: "The manifest's class column is the morph's input: Morphable values blend; Structural values flip at the morph's
  seeded point (hysteresis band 0.1) and change on sounding voices by PA-5; Device values stay out. The engine reports the
  heard value of every stepped key per oscillator, so the GUI shows what is heard where the morph and the stored patch
  differ."
- After: "The manifest's class column is the morph's input, **and Appendix D is its law**. Morphable values blend or flip
  by the law and the key's own mode (MO-6 to MO-13). A Structural key either resolves to one value, which changes on
  sounding voices by PA-5, or belongs to a structural group that blends, which reaches the engine as a **setting set**
  (IF-12) and is rendered by D.6. Device values stay out. **A setting set is an input, never state.** While a group holds
  one, the stored value of each of its blending keys is that of the set's heaviest setting (the lower id on a tie), and
  the state and the snapshot (RC-11.2) hold those values. After a load or a restore, the morph system resends every
  group's set at the load's or restore's sample (IF-12), and until a group receives one it renders the one setting its
  stored values name. Saving, loading and saving again therefore gives bit-identical renders whenever the same sets are
  resent at the same samples. **Values the morph sends (its `Param` events, and a stored value that changes because the
  heaviest setting changed) are not reported to the host as parameter changes**: they are the morph's output, not edits;
  the host automates the morph's own parameters instead (IF-11). The engine reports the heard value of every stepped key per oscillator, **and the heard
  setting set of every structural group, with each setting's id and weight**, so the GUI shows what is heard where the
  morph and the stored patch differ. **Each corner's `ratios` texts are the morph system's state, beside the corners'
  numbers (RC-11.1; ADR-326, X42); the engine receives a text only inside a `SettingSet` entry or as its `ratios` input
  from the morph (MO-20), and its state holds only the stored texts.**" *(the State part, from "**State** is a key-value
  document…", unchanged)*

### 2.12 RT-1 (line 621)

- Before: "…room for PA-5's second setting on every oscillator, a second voice state for VO-6's mono re-run crossfade,…"
- After: "…room for PA-5's second setting on every oscillator; **for every structural group, up to four settings and SB-8's
  second of each: the kernel's on the one member state, the swarm's as up to four whole oscillator instances per
  oscillator per voice (eight during SB-8), with up to four ensemble states per oscillator, one per distinct swarm
  setting (SB-4), a copy of the per-note blade-noise streams per kernel setting (SB-6), and the sub's and the noise's
  chains; the filter slot's settings are sized by the filter once one is assigned (RC-1), and none while the slot
  is empty;** a second voice state for VO-6's mono re-run crossfade, **with its settings**,…" *(rest unchanged)*

### 2.13 IF-9 (line 676)

- Before: "**Operation counters**, deterministic and readable: member-samples rendered, edges corrected, edges left over
  (RT-2), envelope steps, voices sounding. [Q27]"
- After: "**Operation counters**, deterministic and readable: member-samples rendered, edges corrected, edges left over
  (RT-2), envelope steps, voices sounding, **and structure setting-samples: for each voice, each sample and each
  structural group that renders per setting (every kind but by gain, SB-1), the number of settings rendered beyond the
  first; the counter is kept per group kind. A swarm instance's members also count as member-samples**. [Q27]"

### 2.14 IF-10 (line 677)

- Before: "…and the heard stepped values; per voice, the four envelopes' levels and stages;…"
- After: "…and the heard stepped values; **per voice and structural group, the heard setting set (each setting's id, values,
  target and heard weight, and its slot, SB-11), each kernel setting's uncorrected output y⁰ beside the mixed y⁰ that the loop reads (SB-3), and while a swarm
  blends, the member values above per instance with its setting id (SB-4)**; per voice, the four envelopes' levels and stages;…" *(rest unchanged)*

### 2.15 §7.1 gains IF-12, the morph (new row)

> | IF-12 | **The morph** | **The resolver's calls** (MO-1, MO-3): prepare (the host rate); reset; the corners (four arrays of v indexed by manifest id, sized at prepare from the manifest, and each corner's two `ratios` texts, at most 256 bytes each, MO-14); the morph settings (D.2); each key's mode and count (MO-11); the position (x, y), sample-timed; the transport, as RC-7's events; and process (a sample count), which advances the resolver's clock and returns its events for those samples, each timed at a control-interval boundary. The shell passes them to the engine in the same block. **The `SettingSet` event**, timed like a `Param` event (PA-3): a group id (MO-14: `osc1/kernel`, `osc1/swarm`, `osc1/pan`, `osc1/env`, the same for `osc2`, `noise/env`, `sub/chain`, `noise/chain`, `voice/filter`, `fx/1`, …); the count n (2 or 4); the group's blending keys, by manifest key, in ascending manifest id order; and up to n entries, each an id (0 to 3, MO-17; a label and a tie-break, SB-11), a weight, and one value for each listed key: v, or for `oscN.ratios` its text. **`fx/N` sets go to the rack (RC-5), never to the engine.** A merged oscillator's `oscN/swarm` set lists its kernel keys too (MO-14 item 3). A key that resolves to one value is never in a set: it arrives as a `Param` event and applies to every setting of its group (SB-8). Events apply in arrival order (PA-3), whatever their source. While the morph is on, the morph system routes the host's and the GUI's edits of morphing keys to the corners (ADR-109), so the resolver is their only source; a `Param` event for a key in its group's latest multi-setting set is refused (PA-7). The morph's own settings and position are host parameters of the shell (IF-11), never events to the engine. Checked on entry by PA-7. |

### 2.15a IF-11 (line 678)

- Before: "Through the shell: the manifest generates the host parameter table from its non-reserved rows (5.1), committed as a
  lockfile (id, key, min, max, default, taper, steps), ids explicit, append-only, below 0xB00000."
- After: "Through the shell: the manifest generates the host parameter table from its non-reserved rows (5.1), **and the
  morph's manifest (D.2) appends the morph's host parameters to it: the position's x and y, `morph.on`, `morph.law`,
  `morph.count`, `morph.temperature`, `morph.cohesion`, `morph.stepRate`, `morph.stepSync`, `morph.stepDivision`,
  `morph.stepTriplet`, `morph.stepDotted` and `morph.glide`, never `morph.seed` or `morph.audible` (ADR-326, X39; the
  human's ruling on X43); each one row of one range, taper and step count**, committed as a lockfile (id, key, min, max, default, taper, steps), ids explicit,
  append-only, below 0xB00000. **The morph's parameters reach the resolver, never the engine (IF-12).**" *(rest
  unchanged)*

### 2.16 RC-1, the filters

- Before: "- **Deferred:** the filter types, their count and how sources route to them [X13] …"
- After: before it, a new bullet:
> - **Must provide, when built:** a filter instance per setting of the `voice/filter` group's set (SB-1), each cleared at its
>   start (SB-6), their outputs mixed by SB-2; its structural keys declared to the morph as the `voice/filter` group (MO-14).

### 2.17 RC-5, the FX rack

- Before: "- **Deferred:** the rack, its modules, and the device's master limiter (which holds the device's peaks, OU-1)."
- After: that bullet unchanged, and a new one after it:
> - **Must provide, when built:** for each slot, the keys of its `fx/N` group (MO-14: the module patch's structure, as the
>   rack defines it) and its rendering of a two-setting set (MO-19): two module instances, each holding one setting, both
>   receiving the blended Morphable values, their outputs mixed by SB-2's law at the set's weights, with SB-11's slot rule;
>   a change that resolves to one value is made by the rack's own switching. **FX device (ADR-254):** the same.

### 2.18 RC-6, the source slot

- Before: "- **What each voice hands a source:** its published sounding pitch and master phase (IF-2), …"
- After: "- **What each voice hands a source:** its published sounding pitch and master phase (IF-2), **its structural
  groups' setting sets and slots (Appendix D)**, …" *(rest unchanged)*
- RC-6.4's test build adds a source with no structural group, so it is unchanged.

### 2.19 RC-7 (lines 737–745)

- The **Accepted now** bullet (line 739) gains a closing sentence: "**In 1.0 its first consumer is the morph resolver's
  synced steps (MO-10, ADR-325), which also receive its jumps.**"
- RC-7.3, before: "In 1.0 no sound of the engine depends on the transport: a render is bit-identical with and without any
  transport events (10.1 item 19)."
- RC-7.3, after: "In 1.0 no sound of the engine depends on the transport **except through the morph's synced steps
  (MO-10; ADR-325)**: **with `morph.stepSync` off, or the morph off,** a render is bit-identical with and without any
  transport events (10.1 item 19)."
- **RC-7.4 is new: the standard tempo divisions** (ADR-326, X37; B590 points here). Paste after RC-7.3:
> - **RC-7.4 The standard tempo divisions.** Every part of the instrument that syncs to tempo (the morph's stepped hybrid,
>   and the LFOs, the arpeggiator and the delays when they are specified) offers one set of divisions, the table below,
>   and no part keeps a list of its own. **The table is fixed:** an index always names the same row, whatever the time
>   signature, and the menu lists the rows in the table's order. The order is by length in 4/4. A note value's length is in
>   whole notes; a bar value's length is in bars, a bar being the time signature's (RC-7). So in other meters a row's
>   length may tie or cross its neighbour's (in 3/4 a dotted 1/2 equals 1 bar), and neither the index nor the order
>   changes. **Base** rows are always offered. **Triplet** rows (⅔ of their base) are offered when the control's triplet
>   toggle is on, and **dotted** rows (3/2 of their base) when its dotted toggle is on. The two toggles belong to each
>   synced control and are saved with the patch (the human's ruling on X41). They filter the menu only, so a stored index,
>   and the sound, never change with them.
>
>   | index | name | kind | length |
>   |---|---|---|---|
>   | 0 | 1/32 | base | 1/32 whole note |
>   | 1 | 1/16T | triplet | 1/24 whole note |
>   | 2 | 1/32D | dotted | 3/64 whole note |
>   | 3 | 1/16 | base | 1/16 whole note |
>   | 4 | 1/8T | triplet | 1/12 whole note |
>   | 5 | 1/16D | dotted | 3/32 whole note |
>   | 6 | 1/8 | base | 1/8 whole note |
>   | 7 | 1/4T | triplet | 1/6 whole note |
>   | 8 | 1/8D | dotted | 3/16 whole note |
>   | 9 | 1/4 | base | 1/4 whole note |
>   | 10 | 1/2T | triplet | 1/3 whole note |
>   | 11 | 1/4D | dotted | 3/8 whole note |
>   | 12 | 1/2 | base | 1/2 whole note |
>   | 13 | 1 bar T | triplet | 2/3 bar |
>   | 14 | 1/2D | dotted | 3/4 whole note |
>   | 15 | 1 bar | base | 1 bar |
>   | 16 | 2 bars T | triplet | 4/3 bar |
>   | 17 | 1 bar D | dotted | 3/2 bar |
>   | 18 | 2 bars | base | 2 bars |
>   | 19 | 4 bars T | triplet | 8/3 bar |
>   | 20 | 2 bars D | dotted | 3 bars |
>   | 21 | 4 bars | base | 4 bars |

### 2.20 RC-10 (line 775)

- Before: "- **What a later tier may vary:** a cap on the members per swarm, a longer control interval, the quality (High to
  Standard), a cheaper edge table."
- After: "- **What a later tier may vary:** a cap on the members per swarm, a longer control interval, the quality (High to
  Standard), a cheaper edge table, **a lower count of structure settings per group (MO-17's n = 2, or one setting by
  MO-7's pick)**." *(the "What no tier may vary" sentence unchanged)*

### 2.21 RC-11.1, RC-11.2 and RC-11.4

- RC-11.1, before: "The morph position, the macro values and the intents are the morph, macro and intent systems' own state (RC-4);
  they reach the engine only as parameter values, which a snapshot holds."
- RC-11.1, after: "The morph position, **the corners (their numbers and their `ratios` texts, ADR-326 X42), the morph settings (D.2),
  each key's mode and count (MO-11)**, the macro
  values and the intents are the morph, macro and intent systems' own state (RC-4); they reach the engine only as
  parameter values **and setting sets (IF-12)**, and a snapshot holds the parameter values **(a blending group's as its
  heaviest setting's, IF-4)**."
- RC-11.2: after "(every parameter's v and the `ratios` text)" insert "**(no setting set: a set is an input, IF-4; the
  corners, their `ratios` texts included, are snapshotted and undone by the morph system's own history, never by this
  snapshot)**".
- RC-11.4: append "**The morph system resends every group's setting set at the restore's sample (IF-4), and the sets reach
  sounding voices by SB-11.**"

### 2.22 QS-4 (line 807)

- Before (the end of the row): "…any other mute or solo (IF-6) and all-sound-off (VO-13); the swarm's own grain (SW-16 at
  1) is measured and reported, not judged."
- After: "…any other mute or solo (IF-6) and all-sound-off (VO-13), **and a sweep of a setting set's weights from one
  setting to another (SB-2), on the probe rule's patch with only the blade or source whose setting differs**; the swarm's
  own grain (SW-16 at 1) is measured and reported, not judged. **A flip inside a blend (SB-8) is a stepped-key change by
  the probe rule: reported, and judged by PA-5's law test.**"

### 2.23 PF-4 (line 825)

- Before: "…and across a morph between corners with different stepped keys; no patch falls…"
- After: "…and across a morph between corners with different stepped keys, **reported with IF-9's structure setting-samples
  at every corner, every edge's midpoint and the centre, at both counts (MO-17)**; no patch falls…" *(rest unchanged)*

### 2.24 PF-7 (line 828)

- Before: "The worst-case memory per instance, at 64 voices of 16 members, is stated in the manifest and reported."
- After: "The worst-case memory per instance, at 64 voices of 16 members **and every structural group at four settings and
  up to eight during SB-8 (RT-1)**, is stated in the manifest and reported."

### 2.25 C.0 (line 1083)

- Before: "…PA-5's stepped-key crossfade, RC-10.1's tier change and RC-11.4's restore. …"
- After: "…PA-5's stepped-key crossfade, SB-8's flip inside a blend, RC-10.1's tier change and RC-11.4's restore. **A
  structural setting set's weights are values, not fades (ADR-325): they move with the pad and glide by PA-1 (SB-2), and
  SB-11's hand-overs are such glides.** …"

### 2.26 §10.1: item 19 amended, two items added

- Item 19, before: "transport events at any sample times and block sizes leave the render bit-identical (RC-7.1, RC-7.3);"
- Item 19, after: "transport events at any sample times and block sizes leave the render bit-identical **with `morph.stepSync`
  off** (RC-7.1, RC-7.3), **and with it on, synced steps fall on the division boundaries at any block size (MO-10)**;"
- New items, after item 33:
> 34. **the morph resolver** (Appendix D, MO-1 to MO-20, RC-7.4), on its own calls (IF-12), as items 34.1 to 34.17 (§3
>     below, pasted in full);
> 35. **the structure blend** (D.6, SB-1 to SB-13; IF-12's event; PA-7's refusals), as items 35.1 to 35.18 (§3 below,
>     pasted in full).

### 2.27 §11

ADR-325's X28–X35, ADR-326's X36–X40, and the rulings on X41–X43 are written into the rows above, and none of them is
listed. The open items this revision raises are §5's: X44 and X45. Paste them into §11.1, whose heading count changes
from (24) to (26):
> | X44 | whether the Division menu's two toggles are host parameters | yes, as X39's letter says, though they change no sound | D.2, IF-11 |
> | X45 | the `ratios` text's maximum length | 256 bytes of UTF-8 | MO-14, IF-12, PA-7 |

### 2.28 `engine2/structure/engine-structure.json` (proposed as text; the file is not edited)

- **New items:**
  - `morph_resolver` (block, parent `instrument`, scope `engine`, cites MO-1 to MO-20 and IF-12, label "the morph
    resolver: weights, glide, the draw, the laws, modes and counts, setting sets");
  - `voice.structure_slots` (block, parent `voice`, scope `engine`, multiplicity "per structural group, up to its count
    (2 or 4)", cites SB-2, SB-5, SB-6, SB-7, SB-11, SB-12, SB-13, label "a voice's structural slots: settings, heard weights,
    the slot rule").
- **Changed items:**
  - `modulation`: its label drops "and morph systems", and it adds "and the morph's editor and storage";
  - `osc`: its multiplicity reads "2: osc 1 and osc 2; × up to four swarm settings while `oscN/swarm` blends (SB-4)";
  - `member.base_wave`, `member.blade1`, `member.blade2`: multiplicity "× up to four kernel settings on one member state
    (SB-3)";
  - `sub` and `noise`: their chains are "× up to four `sub/chain` / `noise/chain` settings".
- **New edges:**
  - `modulation->morph_resolver` (control: corners, position, settings, modes);
  - `transport->morph_resolver` (event: RC-7, MO-10);
  - `morph_resolver->parameters` (control: `Param` events, MO-20);
  - `morph_resolver->voice.structure_slots` (control: `SettingSet` events, IF-12);
  - `voice.structure_slots->observation_view` (observation: IF-10);
  - `voice.structure_slots->counters` (observation: IF-9).
- **The `counts` block:** `items` from `{"block": 65, "slot": 12}` to `{"block": 67, "slot": 12}`; `scope` from
  `{"engine": 66, "external": 11}` to `{"engine": 68, "external": 11}`; `edges` from `{"audio": 46, "control": 44,
  "event": 7, "modulation": 5, "observation": 9}` to `{"audio": 46, "control": 47, "event": 8, "modulation": 5,
  "observation": 11}` (111 edges → 117); `stages` stays 47 (§4.2's diagram keeps one setting's path).
- **Spec blob:** updated to the ratified text's blob, when the round lands (B585 step 5).

---

## 3. The suite's new items, paste-ready for §10.1 (engine-free, each with a must-fail control)

The test author writes from SPEC-ENGINE alone (BD-10), so these items are spec text. Paste them into §10.1 after item 33,
as items 34 and 35 (§2.26).
- Items 34.x test the resolver through its own calls (IF-12): corners, position, settings, modes and transport in;
  events out.
- Items 35.x test the engine given `SettingSet` and `Param` events.
- Every item names its must-fail control after **Must fail:**.

> **34.** **The morph resolver** (Appendix D, RC-7.4), on its own calls (IF-12). Items 34.6 to 34.11 and 34.15 run at
> `morph.glide` 0; 34.12 tests the glide.
>
> - **34.1 Weights (MO-4).** At 10,000 seeded positions the weights equal the bilinear formula within 10⁻¹² and sum to 1.
>   **Must fail:** a transposed corner order (B and C swapped).
> - **34.2 The draw's independence (MO-6).** Picks for every key at 27 positions are identical with and without 200 ids
>   appended to the manifest, and with the corners' entries written in reverse order. **Must fail:** a draw taken from one
>   stream in list order.
> - **34.3 The draw and the pick, exactly (MO-6, MO-7).** At the position (0.35, 0.6), with T = 1 and cohesion 0.3, the
>   keys `osc1.blade1.mode`, `osc1.baseWave`, `osc1.unison`, `osc2.blade2.wave`, `sub.wave`, `noise.type`, `env1.attack`
>   and `osc1.coupling` pick, in that order:
>   - at seed 1: C C D B A C D D;
>   - at seed 1024: C C C B A C C C;
>   - at seed 9999: C B C A C D D C.
>
>   The first draw of `morph/shared` at seed 1024 is u = 0.5761699904687703 (its FNV-1a state 0x5EE6354F). **Must fail:**
>   a stream that hashes the seed before the purpose's bytes, or that uses an arithmetic shift.
> - **34.4 Temperature and cohesion (MO-7).** At T = 0.02, every key picks the heaviest corner wherever that corner's
>   weight is at least 1.9 times the next. This holds for every seed: ln 1.9 / 0.02 ≈ 32 exceeds the largest gap of two
>   clamped Gumbel draws, ≈ 23.8. At cohesion 1, every key picks the same corner at every position. **Must fail:** a pick
>   without the temperature term.
> - **34.5 The latch (MO-8).** A Structural key dithered across a position where two corners' scores tie, with their
>   difference staying within ±m, flips at most once. Carried until the difference exceeds m, it flips. On an edge with
>   equal draw terms the band is 0.45 to 0.55. A Morphable key under Quantum follows its pick at every crossing.
>   **Must fail:** a Structural key with no latch.
> - **34.6 Blend at count 4 (MO-9).** A Morphable value equals Σ w·v within 10⁻¹². A circular key equals the unit-vector
>   mean within 10⁻¹²; at a zero resultant (corners 0 and 0.5 at equal weight) it equals the heaviest corner's value (the
>   lower corner on a tie). **Must fail:** a linear sum on a circular key, which fails at the wrap; a sum in the law's
>   unit instead of v.
> - **34.7 Count 2 on a Morphable key (MO-9, MO-11).** A key set to Two emits the two-setting law's blend of its pooled
>   distinct corner values, at the unglided weights. It is continuous away from MO-18's point and jumps through it.
>   **Must fail:** a renormalised top-two law, which jumps on the diagonals.
> - **34.8 Stepped (MO-10, RC-7.4).**
>   - The output is constant between steps and equals Blend at the stepped position.
>   - φ follows its closed form: steps fall where ⌊φ⌋ changes, at 3 steps per second at 44.1 kHz (every 14,700 samples)
>     and at 7 per second (fractional), with no drift over 10⁶ samples.
>   - The first held position is the one at reset. A rate change mid-run moves no step already passed.
>   - Synced, steps fall on the division boundaries of a scripted transport. A loop's backward jump that changes ⌊φ⌋ is a
>     step. While the transport is stopped, steps keep falling at the tempo's division length.
>   - Quantum-mode keys and flip-only keys read the stepped position.
>
>   **Must fail:** steps counted by a rounded period, which drifts; a flip-only key that reads the live position.
> - **34.9 Modes and counts (MO-11 to MO-13, MO-16).**
>   - An Exempt key emits corner A's value at every position and under every law, flip-only keys included.
>   - A Quantum key under Blend emits its pick. A Structural Quantum key arrives as a `Param` event and is never in a set.
>   - A Follow key under Quantum flips.
>   - One key set to Two makes its group's n 2, and a Morphable key's Two acts on that key alone.
>
>   **Must fail:** a mode applied per group; the largest count winning.
> - **34.10 Groups and sets (MO-14, MO-17, MO-19).** At 2,000 seeded positions and corner layouts:
>   - candidates pool identical settings, and an id is the lowest corner holding its setting;
>   - at n = 4 the set is every candidate with weight above 0, at its pooled weight;
>   - at n = 2 the set has at most two settings, with MO-17's weights and tie-break;
>   - the weights sum to 1 within 10⁻¹², and flip-only keys are never in a set;
>   - `oscN.ratios` is in `oscN/swarm`, compared as text, and `sub.env` is in `sub/chain`;
>   - an `fx/N` set has n = 2 whatever the choice;
>   - `oscN/swarm` carries the kernel's keys exactly when the four corners differ in the swarm's keys. On the A–B edge
>     (y = 0) of a patch whose A and B agree on the swarm's keys and whose C and D differ from them, the swarm set lists
>     the kernel's keys.
>
>   **Must fail:** a merge decided at the position, which lists none on that edge; an equal-power set, which fails the sum.
> - **34.11 Continuity at n = 2 (MO-18).** With four distinct settings, on an 801 × 801 grid, a setting outside the set
>   counts as weight 0. No two neighbouring points farther than 0.1 from the centre differ in any weight by more than
>   0.02, and the only jump is at the triple tie. **Must fail:** the renormalised top-two law, which jumps on the
>   diagonals.
> - **34.12 The glide (MO-5).** After a step of the position from A to D, with n₀ the first control boundary at or after
>   the step (MO-3), at every control boundary n before
>   the snap, each Morphable value and each corner weight equals target − (target − start)·e^(−(n − n₀)/(f_host·`morph.glide`))
>   within 10⁻⁹. This holds at 44.1, 48 and 96 kHz, at block sizes 1, 64 and 4,096 alike. The four weights sum to 1
>   within 10⁻¹², snap together, and reach exactly [0, 0, 0, 1]. Quantum's picks read the unglided weights. **Must fail:**
>   a coefficient computed per block, which fails at uneven blocks; snapping each weight alone, which breaks the sum.
> - **34.13 Device keys (MO-2).** Over every Device row, at 1,000 positions, under every law and mode, the resolver emits
>   no event for a Device key. **Must fail:** a resolver that blends `quality`.
> - **34.14 Block size and transport (MO-3, RC-7.3).** A scripted session gives the same events at the same samples at
>   block sizes 1 to 4,096. The session holds a moving position, changes of settings and modes, transport events with a
>   loop's jump, and corner edits. With `morph.stepSync` off, it gives the same events with and without the transport
>   events. **Must fail:** resolving once per block.
> - **34.15 The events and their order (MO-20).**
>   - One-value keys arrive as `Param` events, and blending groups as `SettingSet` events, with ids, key lists and weights
>     by MO-17.
>   - At each boundary the sets come before the `Param` events.
>   - When a key stops blending (Follow to Quantum, with the group holding two settings), its group's new set (without
>     that key) precedes its `Param` event, so the engine accepts it.
>   - A group that stops blending gets one last one-setting set.
>   - When `morph.on` turns off, with the position near D and `morph.audible` A, the resolver emits corner A's setting for
>     each blending group, then corner A's values for every other key, and then nothing. Changing `morph.audible` to C
>     while off emits corner C's the same way, its Morphable values gliding by MO-5 (b).
>   - Sets at one boundary come in MO-20's fixed order. A corner edit that merges a blending kernel into its swarm sends
>     `oscN/kernel` a one-setting set, then the merged swarm set; the reverse edit sends the swarm set without kernel keys
>     first.
>   - A one-value `oscN.ratios` arrives as the `ratios` input in the `Param` phase.
>
>   **Must fail:** a resolver that emits the `Param` first, which the engine refuses and which is never resent; a resolver
>   that sends the heaviest setting at morph off; a merge sent swarm-first, which the engine refuses.
> - **34.16 Reset (MO-3).** After reset the events equal a freshly prepared resolver's for the same inputs. **Must fail:**
>   a latch kept across reset.
> - **34.17 The standard divisions (RC-7.4).**
>   - Every synced control's options are RC-7.4's table: 22 rows, each with its index, name, kind and length, in the
>     table's order.
>   - The triplet and dotted toggles change only which rows are offered.
>   - A stored index keeps its row in 4/4, 2/4, 3/4 and 2/8, and its render is bit-identical whatever the toggles. In 3/4,
>     index 14 (1/2D) and index 15 (1 bar) are both 3/4 of a whole note, and they keep their order.
>   - `morph.stepDivision` 3 steps every 1/16 note.
>
>   **Must fail:** an option list sorted by length in the current meter, which swaps rows in 2/8; an option list rebuilt
>   by the toggles, which moves a stored index.
>
> **35.** **The structure blend** (D.6, SB-1 to SB-13; IF-12's event; PA-7's refusals), given `SettingSet` and `Param`
> events.
>
> - **35.1 Corner exactness (SB-10).** A one-setting set renders bit-identically to the patch with the morph off and those
>   values stored. **Must fail:** a planted weight of 10⁻³ on another setting.
> - **35.2 Weight 0 costs nothing (SB-5, IF-9).** With every set holding one setting, the counters equal the no-morph
>   patch's. A setting at weight 0 adds zero setting-samples, and by-gain groups never add any. **Must fail:** a build
>   that renders every listed entry.
> - **35.3 The mix law (SB-1, SB-2).** At a static set with settled weights, on a patch with no feedback, no cross-mod and
>   an empty filter slot, the output equals Σ wₛ·(the render of setting s alone, with the same values) within 10⁻⁶ of its
>   RMS, for 2, 3 and 4 settings. **Must fail:** an equal-power mix.
> - **35.4 By gain (SB-1 item 1).** A set on `oscN/env`, `noise/env` or `oscN/pan` renders one setting (by the counters)
>   and meets 35.3's law. **Must fail:** a two-render build, which reads double.
> - **35.5 Weights glide and sum to 1 (SB-2).** Heard weights sum to 1 within 10⁻⁵ at every sample of a moving set, and a
>   weight jump follows PA-1's curve. **Must fail:** weights applied as steps.
> - **35.6 No click across a blend (QS-4).** A 1 s sweep of a two-setting set from one setting to the other makes no
>   detected click. It runs on the probe rule's patch: a sine probe plus the one blade whose mode differs. **Must fail:**
>   a planted bare switch at the midpoint is detected.
> - **35.7 Waking (SB-6).** A setting that enters mid-note equals, from its entry, the same setting started fresh at that
>   sample. The same render at block sizes 1, 64 and 4,096 is bit-identical. **Must fail:** a wake that resumes stale state.
> - **35.8 Slot independence and blade noise (SB-6).** A setting rendered in either slot, or alone, gives bit-identical
>   samples from the same start. Two kernel settings in the Noise mode draw from separate copies of the blade-noise
>   streams: each equals its render alone. **Must fail:** streams keyed by slot; one stream shared by the settings.
> - **35.9 A voice that starts in a blend (SB-7).** With no feedback and no cross-mod, a note started at a static set
>   equals Σ wₛ·(the note started in each setting alone), from its first sample. **Must fail:** a voice that starts its
>   heaviest setting only.
> - **35.10 Legato in a blend (SB-7).** A legato change during a two-setting set keeps both settings' phases and states
>   continuous: the output equals a render in which only the pitch moved. **Must fail:** restarting settings on a legato
>   change.
> - **35.11 A flip inside a blend (SB-8).**
>   - A one-value `mode` change in a two-setting kernel group crossfades both settings by PA-5's law (QS-4's law test),
>     with at most four settings rendering.
>   - An `octave` change steps every setting's frequency at once, with phases continuous.
>   - A `unison` change follows PA-6 in every instance.
>   - A second change waits.
>
>   **Must fail:** flipping one setting only.
> - **35.12 Limits (SB-9).** Never more than four settings per group, two in a count-2 group, and twice those during SB-8,
>   by the counters and the observation view. **Must fail:** a planted build that admits a third setting into a count-2
>   group before a slot frees.
> - **35.13 The slot rule (SB-11).** Scripted sequences:
>   - {A, B} → {C, D};
>   - {A: 1} → {D: 1}, MO-18's point;
>   - a corner edit replacing one setting's values while four sound;
>   - count 4 → 2 while four sound;
>   - **the relabel case:** A and B hold X (id 0) and C and D hold Y. Edit A to Z, so X's id becomes 1. X keeps its slot
>     with no restart, and Z enters new;
>   - a key leaving the key list (Follow to Quantum) while two settings sound: its `Param` applies to both, and neither
>     re-enters;
>   - a key joining the list (Quantum to Follow): a setting whose `Param`-given value equals the new entry's matches it,
>     and does not re-enter;
>   - **a merge while the kernel blends:** a corner edit makes `osc1/swarm` differ while `osc1/kernel` holds two
>     settings. The kernel's one-setting set, then the merged swarm set, are both accepted. Every old setting of both
>     groups fades by PA-1 while the new instances enter by the slot rule, with the limits held;
>   - two `ratios` texts that parse alike (`1, 5/4` and `1,5/4`) are one setting.
>
>   Throughout: at most the allowed settings sound at every sample; heard weights sum to 1 within 10⁻⁵; slots free only
>   below 10⁻⁴; the output steps by no more than 10⁻⁴ of the settings' difference; and 35.6's detector reads no click on
>   the probe patch. **Must fail:** a build that matches by id, which restarts X in the relabel case; a build that frees
>   a slot without passing its residual, which breaks the sum; a build that matches across a merge.
> - **35.14 The kernel's shared state (SB-3).** With feedback on, in a two-setting kernel group, the mixed y⁰ that the loop
>   reads (IF-10) equals Σ wₛ·y⁰ₛ of the settings' y⁰ within 10⁻⁶ at every sample. **Must fail:** feeding back the
>   heaviest setting's output alone.
> - **35.15 Swarm instances (SB-4).**
>   - Each instance's start phases and drift equal those of its setting rendered alone.
>   - The noise's streams follow the seed of osc 1's heaviest setting at the fresh start, and keep it when the weights
>     cross.
>   - Two settings with the same seed and different `unison` keep separate ensemble states, each equal to its setting's
>     alone.
>   - Two settings that differ only in `osc1.ratios` render whole instances, each with its own text.
>   - Published R, ψ and the onset level equal the mixture's within 10⁻⁶.
>   - An instance's feedback reads its own output.
>
>   **Must fail:** streams shared across instances; ensemble state keyed by seed alone; a published value taken from one
>   instance.
> - **35.16 Sets as inputs (IF-4, IF-12, PA-7, RC-11).**
>   - The state and the snapshot hold the heaviest setting's values, `ratios` included, and no set, and no corner's texts:
>     the state's text has exactly IF-4's members.
>   - Handing the same corners (their texts included) back to the resolver after a load (IF-12) renders bit-identically.
>     The corners' own save and undo are the morph system's, judged with the shell's acceptance (RC-11.1).
>   - A `Param` event naming one of the morph's host ids is refused as an id that names no engine parameter.
>   - A save and load mid-blend, with the sets resent at the same samples, renders bit-identically.
>   - Each PA-7 case is refused whole and counted, among them an `fx/N` set, an `oscN/kernel` set while merged, and a bad
>     `ratios` text.
>   - A `Param` for a key in the latest multi-setting set is refused and counted; one that follows a set without that key
>     is accepted.
>   - No value the morph sends is reported to the host as a parameter change.
>
>   **Must fail:** accepting an oversize set; storing a set, or a corner's text, in the state; refusing on slots still
>   fading; a "live" kernel judged by slots still fading, which refuses the merged swarm set.
> - **35.17 Determinism (DT-1, DT-2).** A moving set at block sizes 1 to 4,096 is bit-identical.
> - **35.18 A voice's life while an envelope selector blends (SB-13).** With `osc1/env` blending ENV 1 (release 0.1 s) and
>   ENV 2 (release 2 s), the voice is freed only after ENV 2 has finished while ENV 2's setting has weight above 0, and
>   after ENV 1 has finished once ENV 2's weight has reached 0. **Must fail:** a build that governs by ENV 1 alone, which
>   cuts ENV 2's tail.

35.17 has no must-fail control, as with DT-2's existing item 3; every other item has one. Revision 2's R1–R16 and E1–E17
are these items, renumbered 34.1–34.16 and 35.1–35.17. 34.17 and 35.18 are new.

---

## 4. Diff summary

**Rows added.**

| Where | Rows | Count |
|---|---|---|
| Appendix D (new) | D.0–D.6: MO-1 to MO-20, SB-1 to SB-13, the D.2 table of 13 morph settings | 33 rows + 1 table |
| §7.1 | IF-12, the morph (the resolver's calls; the `SettingSet` event) | 1 |
| RC-7 | RC-7.4, the standard tempo divisions (B590), with its fixed 22-row table | 1 + 1 table |
| Glossary | corner / pad / position; structural group; setting set | 3 |
| §1 In scope | the morph resolver | 1 bullet |
| §3 | the morph resolver (deliverable); `morph/` in the layout | 1 row, 1 line |
| BD-8 | the stage 7 amendment | 1 clause |
| §4.2 | a bullet on blending stages | 1 |
| §4.3 | the morph's laws and their rendering | 1 row (split from one) |
| VO's start table | structural settings | 1 row |
| RC-1, RC-5 | "must provide, when built" | 2 bullets |
| §10.1 | items 34 (34.1–34.17) and 35 (35.1–35.18), pasted in full | 2 items, 35 sub-items |
| §11.1 | X44, X45 | 2 open items |
| `engine-structure.json` (as text) | 2 items, 6 edges | — |

**Rows changed.**

| Row | Change |
|---|---|
| Line 9 | names Appendix D |
| Line 12 | MO and SB numbering |
| §1 Out of scope | the morph's editor, storage and pad only |
| Glossary, line 68 | Structural follows the law |
| §4.3 | the morph moves to its own row |
| §5.1 Class | `S` follows the law; flip-only keys |
| SW-1 | one swarm per setting while blending |
| PA-1 | setting-set weights glide |
| PA-5 | three edits |
| PA-7 | `Param` ids naming no engine parameter (the morph's host ids included); `SettingSet` refusals (`fx/N`, the merged swarm with "live" defined, bad `ratios` text included); a `Param` for a key in the latest multi-setting set refused |
| IF-3 | published R, ψ of the mixture |
| IF-4 | Appendix D is the law; sets are inputs; stored values are the heaviest setting's; resend after load; no host echo; heard sets reported; corners' texts are morph state (its State part unchanged) |
| IF-11 | the morph's host parameters, from a morph manifest appended to the lockfile |
| IF-9 | structure setting-samples, defined once |
| IF-10 | heard sets; each kernel setting's y⁰ and the mixed y⁰; members per instance |
| RT-1 | sizing for sets, instances, ensemble states per swarm setting, blade-noise copies, the second voice state; no filter instances while the slot is empty |
| RC-6 | sets handed to a source |
| RC-7 "Accepted now", RC-7.3 | the morph's synced steps, its first consumer |
| RC-10 | a tier may lower the count |
| RC-11.1, RC-11.2, RC-11.4 | morph state; no set in a snapshot; resend at restore |
| QS-4 | blend sweep judged; flip inside a blend by the probe rule |
| PF-4 | structure counter at named pad positions, both counts |
| PF-7 | memory at four settings, eight during SB-8 |
| C.0 | SB-8 listed; weights are values |
| §10.1 item 19 | transport, with synced steps |
| §11.1 heading | (24) → (26) |

That is 29 rows changed (the RC-11 trio counted as three, the RC-7 pair as two).

**Test items added.** 34.1–34.17 and 35.1–35.18, written into §10.1 itself: 35 items, 34 of them with a must-fail
control. 35.17 has none, as with DT-2's existing item 3.

**Unchanged by design:**
- the manifest and its ids;
- the engine manifest's rows in the host lockfile (the morph's rows are appended after them);
- Appendix C's laws;
- PF-1's target;
- the rack contract, which gets a follow-up (§5).

At a corner, or with the morph off, the engine is bit-identical to today (SB-10).

---

## 5. Open points (what ADR-324, ADR-325, ADR-326 and the rulings on X41–X43 do not settle), each with a recommendation

X36–X40 are ruled (ADR-326), and so are X41–X43 (the human, on revision 3); all are written into the rows. Two points
remain:

| # | Question for the human | Recommendation |
|---|---|---|
| X44 | **Are the Division menu's triplet and dotted toggles host parameters?** X39's letter makes every morph setting but the seed automatable, and the lead's brief kept all four step settings. The toggles change no sound, only which divisions the menu offers. | **Yes, as X39's letter says.** It is one rule with no exception; an automated toggle is harmless (it moves no stored index, RC-7.4). If the host's parameter list should stay lean, they are the first two to drop. |
| X45 | **The `ratios` text's maximum length.** The spec states none, and a set entry and a corner both carry it. | **256 bytes of UTF-8,** checked by PA-7. That is room for every bank text, and for 32 ratios written as fractions. |

**For the lead to file (not for the human).**
- **Rack contract.** RS-6.2 and RS-6.3 allow two module instances "only during a flip or a spill-over tail". MO-19 and
  RC-5's new clause need them "or while a slot's set holds two settings". RS-8.4's cost row needs the same. This goes as
  a §12 amendment and a notice to every module, as ADR-309 went.
- **The spec blob.** The `engine-structure.json` changes of §2.28 land with the round (B585 step 5).

**Not in this round** (for the critic, not for ruling). Each acts on Morphable values and stacks on MO-7 and MO-9
without touching D.5 or D.6:
- waypoints and their law (B235, B272);
- quantum boundaries edited by hand, with locks (B269 item 5);
- authored bias and pin (ADR-109);
- the off-corner rule (B232);
- the intent bus's reshaping (B170).

Each joins Appendix D by its own round.

**The FX device (ADR-254).** Its morph pad and per-parameter menus follow MO-1 to MO-20 unchanged. Its slots render by
RC-5's new clause, and the rack contract's follow-up applies to it too. It has no voice, so D.6's voice rows (SB-7,
SB-12) do not apply.

---

## 6. The first review's findings (revision 2) and where each is fixed

In this table, revision 2's test names map as Rn = 34.n and En = 35.n.

| Finding | Fixed in |
|---|---|
| **B1** glide on set weights undefined | MO-5: the four corner weights glide together before every law, with a joint snap; sets are computed from them; Morphable values glide separately with a snap; set weights are never glided in the resolver (PA-1 does it); R6–R11 and R15 at glide 0; R12 added |
| **B2** the setting-set interface | IF-12: the `SettingSet` event, with group id, n, key list, ids and values; one-value keys as `Param`; precedence by arrival order, with the shell routing edits to corners; PA-7 refusals; IF-4, RC-11.1, RC-11.2, RC-11.4: sets are inputs, the heaviest setting's values are stored, and sets are resent; identity by id plus values (SB-11 (a)), so SB-8's flips arrive as `Param` and are distinct from new settings |
| **M1** the resolver's clock, cadence, reset, transport | MO-3; MO-10 ("every key reads the stepped position"); RC-7 consumers and RC-7.3; item 19; R14, R16 |
| **M2** `sub.env` by gain is false | MO-14 item 4 (`sub.env` in `sub/chain`, with the reason); R10 |
| **M3** swarm/kernel merge per patch | MO-14 item 3; R10's control |
| **M4** several swarm instances | SB-4 (streams, the noise's seed, persistent state per seed, published sources, own feedback, observation); SW-1; IF-3; IF-10; §4.2; RT-1; E15 |
| **M5** FX and filter slots | Appendix D keeps the resolver half (MO-14 items 6–7, MO-19); rendering moved to RC-1 and RC-5 "must provide, when built"; contract terms avoided; rack contract left as a follow-up (§5); RT-1: no filter instances while the slot is empty |
| **M6** Exempt | MO-12 (corner A's value); MO-15 "but Exempt"; MO-11 (two choices on every key) |
| **M7** circular keys | MO-9 (the weighted unit-vector mean, the zero-resultant rule); R6 |
| **M8** the slot rule for n slots | SB-11 (n slots; corner edits, chimeras, lowered counts); E13 |
| **M9** the draw exactly | MO-6 (FNV-1a over the purpose and the seed, mulberry32 written out, u = out/2³²); R3 golden picks (computed independently in Python and JavaScript, which agree); the "one unit" clause deleted; the shared purpose not key-shaped |
| **M10** rulings written in | X28 (MO-1, §1, §3), X29 (D.2), X30 (MO-11, MO-17), X31 (MO-5), X32 (MO-6), X33 (MO-8), X34 (MO-10, RC-7.3), X35 (C.0, SB-2); none left open |
| **M11** BD-8 and the structure file | §2.1 (the stage 7 amendment; stages 2, 3, 5 and 6 reopened; items 0–33 and 15 green; items 34 and 35); §2.28 (structure items and edges as text) |
| **M12** tests | R12 (glide), R13 (MO-2), R14 (block size), E12 (a real plant), E14 (SB-3), E10 (legato), E9 (no cross-mod) |
| Minor: stepped timing | MO-10 (phase accumulator, the first held position, rate changes, stopped transport); X37 (divisions) |
| Minor: PROPOSED ranges and tapers | X36 |
| Minor: tie-break; R10's non-members | MO-17 (the lower id); R11 (non-members count as 0) |
| Minor: SB-2 "within 10⁻⁵" | SB-2 |
| Minor: PF-7, eight during SB-8 | PF-7 |
| Minor: PA-5 and PA-6 named in SB-8 | SB-8 |
| Minor: IF-9 defined once; by-gain | IF-9 (by-gain groups excluded); E2 |
| Minor: glossary wording; shared purpose; MO-4 against MO-10 | §2.2; MO-6; MO-4 ("only through these weights, at the stepped position") |
| Minor: per voice or per instrument; the mono sub | SB-12 |
| Minor: R3, a fixed position and seed list | R3 |
| Minor: RC-6, VO-6, §4.2 | §2.18, §2.6, §2.1 |
| Minor: host automation of morph settings | X39 |
| Minor: the FX-device consequence | §5 |
| Minor: the A–D diagonal at n = 2 | MO-18 |

## 7. The re-review's findings and ADR-326 (revision 3), and where each is fixed

| Finding | Fixed in |
|---|---|
| **MAJOR 1** the tests in the spec | §3 is now paste-ready §10.1 text: items 34.1–34.17 and 35.1–35.18 (§2.26 points to them) |
| **MAJOR 2** match by value, not id | SB-11 (a): an entry matches on the keys both lists share; the id is a label and a tie-break; a key leaving the list applies by SB-8 with no re-entry; 35.13 has the relabel case and the leaving-key case, with "match by id" as its must-fail control |
| **MAJOR 3** event order | MO-20: sets before `Param` events at a boundary; PA-7: "holds" means the latest received set; 34.15 has the stop-blending case, with "`Param` first" as its control; 35.16 too |
| **MAJOR 4** ensemble per swarm setting | SB-4 (b): one ensemble state per distinct swarm setting (≤ 4), seeded from its seed; placement per instance from (U, distribution, seed); RT-1 sizes four; 35.15 has the same seed with different `unison` |
| **MAJOR 5** morph off (ADR-319 item 1) | D.2 gains `morph.audible`; MO-20 emits the audible corner's sets, then its values, then nothing; 34.15 |
| X36 | D.2 (tapers ruled, tags removed) |
| X37 | RC-7.4 (new, the standard set B590 points to); D.2 and MO-10 cite it; 34.17; X41 (its edges and the toggles' scope) |
| X38 | BD-8's amendment: the bank carries on |
| X39 | IF-11 (§2.15a); D.2's note; IF-12; X43 for `morph.audible` |
| X40 | MO-14 item 3 (`oscN.ratios` in the swarm group, as text); IF-12 (text values; corners carry their texts); PA-7 (a bad text refused); IF-4's `cornerRatios`; 35.15, 35.16; X42 (the `cornerRatios` part superseded in revision 4: X42 re-ruled, §8) |
| Minor: R12 in closed form | 34.12: the closed form at each control boundary, within 10⁻⁹ |
| Minor: R10's control | 34.10: on the A–B edge |
| Minor: MO-10 | φ in closed form from the last change; a backward jump that changes ⌊φ⌋ steps |
| Minor: MO-5 (b), MO-9 at count 2 | MO-5: "the same value" for non-circular keys at count 4 only; MO-9: the pooled unglided weights, with the law selecting pooled candidates |
| Minor: group ids colliding with keys | every group id uses `/` (MO-14, IF-12, SB-1, RC-1, RC-5, §2.28, the tests) |
| Minor: PA-7 under a merged swarm | PA-7 and IF-12: a merged swarm set lists kernel keys; a kernel set is refused while merged |
| Minor: `fx/N` sets | IF-12, MO-19, MO-20, PA-7: they go to the rack; the engine refuses them |
| Minor: IF-12's corners | arrays indexed by manifest id, sized at prepare, with the texts |
| Minor: IF-4's host reporting | IF-4: the morph's values are never reported to the host as changes |
| Minor: SB-11 (b)'s smoother | both of PA-1's error states take r, as a target step of −r |
| Minor: blade noise per kernel setting | SB-6; RT-1; 35.8 |
| Minor: voice lifetime while `oscN/env` blends | SB-13 (new); 35.18 |
| Minor: §2.27's paste-ready rows | §2.27 (X41–X43) |
| Minor: BD-8's parenthesis | removed |
| Minor: §2.28's `counts` | §2.28 |
| Minor: X31's sentence | MO-5 |

## 8. The diff check's findings and the rulings on X41–X43 (revision 4), and where each is fixed

| Finding | Fixed in |
|---|---|
| **MAJOR 1** merging a kernel into its swarm | PA-7: "live" means the latest received set holds more than one setting. MO-20: at a merge, `oscN/kernel` gets a one-setting set first, then the merged swarm set; at an un-merge, the swarm set without the kernel's keys comes first; a fixed order for several sets at one boundary. SB-11: a change of merge replaces both groups' settings, with no matching across it. Tests 34.15 (the order) and 35.13 (a merge while the kernel blends), with "swarm first" and "matching across a merge" as controls. |
| **MAJOR 2** two ranges in one host parameter | D.2: `morph.stepRate` (0.25–32 per second, log) and `morph.stepDivision` (an index into RC-7.4's table), plus `morph.stepTriplet` and `morph.stepDotted`. IF-11 lists all four as host parameters (X39's letter; X44 asks about the toggles). MO-10 reads the division's row. |
| **MAJOR 3** the index depended on the meter | RC-7.4 is now a fixed 22-row table (index, name, kind, length), ordered by the table. 34.17 checks the table in 4/4, 2/4, 3/4 and 2/8, with "sorted by length in the meter" as its control. |
| **MAJOR 4** `cornerRatios` removed (X42 re-ruled) | IF-4 is back to its State part unchanged. The corners' texts are morph state (RC-11.1). RC-11.2's snapshot excludes them; the morph system snapshots and undoes them. The engine gets texts only in sets and as its `ratios` input (MO-20). 35.16 tests that the state holds no corner text and that a resolver round trip renders bit-identically. |
| Minor 1: §2.26's counts | 34.1–34.17 and 35.1–35.18 |
| Minor 2: a key joining the list | SB-11 (a) compares every key the entry lists, using a `Param`-given value; 35.13 |
| Minor 3: `morph.audible` changed while off | MO-20: the new corner's values are sent again, Morphable ones gliding by MO-5 (b) and Structural ones changing by PA-5; 34.15 |
| Minor 4: `ratios` in sets | MO-14 item 3: at most 256 bytes [X45]; parsed lists compared; `oscN.ratios` has the mode and count menu. IF-12 and SB-11 (a) follow. 35.13 has the parse-alike case |
| Minor 5: the morph's host ids | PA-7: a `Param` naming no engine parameter (the morph's host ids included) is refused; 35.16 |
| Minor 6: 34.12's n₀ | 34.12: the first control boundary at or after the step |
| X41, X43 ruled | RC-7.4 (per control, saved with the patch; the ends kept); D.2 and IF-11 (`morph.audible` is not a host parameter) |

**Evidence consulted:**
- **The rulings:** ADR-324 and the critic's findings with ADR-325's rulings (from the lead's brief); ADR-104, ADR-109,
  ADR-185, ADR-186 §5(g), ADR-188, ADR-239 and ADR-309 (DECISIONS.md); B308, B424, B557 and B585 (`lead-records-369`).
- **The spec:** `specs/SPEC-ENGINE.md` at `2af41d8`, read only: lines 9, 12, 31–36, 68, 89–93, 169, 194, 435, 549, 553,
  555, 613, 621, 645 (SN-3), 670, 671, 676, 677, 721–745, 772–790, 807, 825, 828, 838–876, 913–917, 922–976, 1083.
- **The structure file:** `engine2/structure/engine-structure.json` (its items and counts; read only).
- **The legacy shell:** `src/morph_core.h` and `src/hypersaw_clap.cpp:134, 511–541` (read only).
- **The goldens:** R3's picks, computed by two independent scripts that follow MO-6 and MO-7's text (Python with masking,
  JavaScript with `Math.imul`), which agree on every pick.
- **Not read:** the clean-room repositories.
