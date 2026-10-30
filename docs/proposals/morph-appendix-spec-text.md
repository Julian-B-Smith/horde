# The morph appendix: spec text for SPEC-ENGINE (B585 step 4's input), revision 2

*Origin: horde lead session, 2026-10-29.*
- *ADR-324 (horde #1274) rules B585's twelve questions and places the morph in one normative appendix of SPEC-ENGINE.*
- *Revision 1 (`c75fa9f`, horde #1275) was reviewed by an independent critic: "Not ready; ready after the listed fixes".*
- *The human ruled its open points X28–X35 (ADR-325).*
- *This revision applies the critic's findings and the rulings. §6 maps each finding to where it is fixed.*

*Drafted by a scoped implementer. Status: **PROPOSED**. The lead applies it to `specs/SPEC-ENGINE.md` after the critic's
re-read and the human's ratification. This file does not touch `specs/`, `engine2/` or the rack contract.*

**How to apply it.**
- §1 is the new **Appendix D**. It goes after Appendix C.
- §2 lists every amended row, as before → after, at `2af41d8`.
- §3 holds the suite's new items.
- §4 is the diff summary. §5 lists the open points that remain. §6 is the fix table.

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
> **resolver**, built by the clean room beside the engine (ADR-325). Rows SB-1 to SB-12 are the engine's rendering of
> structure.
>
> ### D.0 Who computes what, and when
>
> | id | requirement |
> |---|---|
> | MO-1 | **Two halves.** The **resolver** maps the corners, the position, the morph settings (D.2) and each key's mode and count (D.4) to events for the engine (MO-20): one value for each Morphable key and each Structural key that resolves to one value, sent as `Param` events, and a **setting set** for each structural group that blends, sent as a `SettingSet` event (IF-12). The engine applies `Param` events as any (PA-1, PA-5, PA-6) and renders setting sets by SB-1 to SB-12. The corners, the position, the morph settings and the modes are the morph system's state (RC-11.1); the resolver receives them through IF-12 and saves none of them. |
> | MO-2 | **Device keys** are outside the morph. The resolver never emits them; they are never blended and never flipped (5.1). |
> | MO-3 | **Clock, cadence, prepare and reset.** The resolver keeps a sample clock of its own, advanced by the samples it is asked to process and counted from its reset exactly as PA-3 counts the engine's, so its control intervals coincide with the engine's. It resolves once at the first sample of every control interval. An input (a position, a transport event, a setting, a mode, a corner edit) takes effect at the first interval boundary at or after its sample time. Its events are timed at that boundary. Prepare (given the host rate) allocates everything it will use (RT-1). Reset sets its clock to 0, clears every latch (MO-8: no corner held), clears the stepped hold (MO-10), snaps the glide (MO-5) to its targets, and sets the step phase to 0. After reset it resolves exactly as a freshly prepared resolver with the same inputs (DT-6's rule). It reads no clock (DT-5), allocates nothing after prepare, and its output is independent of block size (DT-2). |
>
> ### D.1 The weights and the glide
>
> | id | requirement |
> |---|---|
> | MO-4 | **Corners and weights.** The corners sit at A = (0, 0), B = (1, 0), C = (0, 1), D = (1, 1). At a position (x, y) in [0, 1]², the **corner weights** are bilinear: w = [(1 − x)(1 − y), x(1 − y), (1 − x)y, xy], in that order, each in [0, 1], summing to 1. Every law reads the position only through these weights, at the stepped position under Stepped (MO-10). |
> | MO-5 | **Morph glide** (ADR-325: on the resolver's output, before PA-1). Two glides, each a one-pole of time constant `morph.glide` seconds, stepped once per sample on the resolver's clock with the coefficient 1 − e^(−1/(f_host·`morph.glide`)), and bypassed at 0. **(a) The weights:** the four corner weights w are glided together, before every law, and snap together to their targets once all four are within 10⁻⁶ of them. The glided weights stay in [0, 1] and sum to 1, and every setting set (MO-17) is computed from them. **(b) The Morphable values:** each Morphable value the resolver emits is glided in its normalised form v (circular keys the shortest way round), and snaps to its target once within 10⁻⁶. Under Blend, (b) applied to the blend of fixed corner values gives the same value as the blend at the glided weights. Under Quantum it softens a Morphable key's flip. **Never glided:** a setting set's own weights (the engine glides them, PA-1, SB-2), Quantum's picks (MO-7 reads the unglided weights), and Structural keys that resolve to one value (they flip by PA-5). |
>
> ### D.2 The morph settings
>
> The morph's settings belong to the patch, are saved with it by the morph system (RC-11.1), and are never morphed.
>
> | key | label | range | default | what it does |
> |---|---|---|---|---|
> | `morph.on` | Morph | off, on | off | While off, the resolver emits nothing after MO-20's collapse (ADR-104: morph ships off). |
> | `morph.law` | Morph mode | Blend, Quantum, Stepped | Quantum | The law every Follow key obeys (D.3). |
> | `morph.count` | Count | Two, Four | Four | How many corner settings may sound, or blend, at once under Blend and Stepped (MO-9, MO-17). |
> | `morph.temperature` | Temperature | 0.02 to 4, log | 1 | How sharply each corner owns the pad under Quantum (MO-7). [X36] |
> | `morph.cohesion` | Cohesion | 0 to 1, lin | 0.3 | How far all keys pick the same corner together under Quantum (MO-7). |
> | `morph.seed` | Morph seed | 1 to 9,999, step | 1024 | Seeds the quantum draw (MO-6). |
> | `morph.stepRate` | Step rate | 0.25 to 32 steps per second, log; with `morph.stepSync` on, one of the tempo divisions [X37] | 4 per second; 1/16 when synced | How often Stepped takes a new position (MO-10). [X36] |
> | `morph.stepSync` | Sync | off, on | off | On: steps follow the host's musical position (MO-10, RC-7). |
> | `morph.glide` | Morph glide | 0 to 5 s, pow | 0.008 s | MO-5. [X36] |
>
> The ranges and defaults of on, law, temperature, cohesion, seed and glide are the legacy shell's (its ids 151, 154 to
> 158; law 0 was quantum). The tapers, the step rate's range and the division list are PROPOSED.
>
> ### D.3 The draw and the laws
>
> | id | requirement |
> |---|---|
> | MO-6 | **The quantum draw** (ADR-104; ADR-325 for X32: keyed per key, permanently, which is ADR-186 §5(g)'s ADR; ADR-185's rule then holds by construction). A stream for a purpose p and the seed S = `morph.seed` is a mulberry32 generator whose 32-bit state starts at h, the FNV-1a hash (offset basis 0x811C9DC5, prime 0x01000193, arithmetic modulo 2³²: h ← (h XOR b)·0x01000193 for each byte b) of the UTF-8 bytes of p followed by the four bytes of S, least significant first. Each step does: a ← a + 0x6D2B79F5; t ← (a XOR (a >> 15))·(a OR 1); t ← (t + (t XOR (t >> 7))·(t OR 61)) XOR t; out ← t XOR (t >> 14). Every operation is modulo 2³² on unsigned values, and >> is a logical shift. The draw is u = out / 2³², clamped to [10⁻⁹, 1 − 10⁻⁹]. A key k's draws are gₖ[c] = −ln(−ln u), the first four outputs of the purpose `morph/key/` followed by k's manifest key, for c = A, B, C, D in order. The **shared** draws g*[c] come the same way from the purpose `morph/shared`, which no manifest key can produce, since keys hold no `/`. **No draw depends on a key's position in any list or on how many keys exist**, so appending a key never changes how an existing patch sounds. Blend and Stepped read no draw. |
> | MO-7 | **Quantum's pick.** For a key k at the unglided weights w (the stepped position's under Stepped), each corner scores sₖ[c] = λ[c] + (1 − h)·gₖ[c] + h·g*[c], with λ[c] = ln(w[c])/T where w[c] > 10⁻⁹ and −10¹² otherwise; T = max(0.02, `morph.temperature`) and h = `morph.cohesion`. The key's **pick** is the corner with the highest score (the lowest corner on a tie). A Morphable key emits the pick's value (glided by MO-5 (b), then PA-1). A Structural key resolves to it as one value, held by MO-8. |
> | MO-8 | **The latch: a band of 0.1** (ADR-325). A Structural key keeps the corner it holds until another corner's score exceeds the held corner's by more than m = ln(0.55/0.45)/T. On an edge where two corners' draw terms are equal, that is a band of exactly 0.1 of the pad, centred on the even split; elsewhere it is the same margin in score. A key holds no corner before its first resolution after reset, and takes its pick then. Morphable keys have no latch. |
> | MO-9 | **Blend.** A Morphable key's value at count 4 is the blend of its four corner values at MO-4's weights, unglided (MO-5 (b) then glides the value): Σc w[c]·v[c] in the normalised form v. For a key whose law is `lin, wraps` (a circular key), it is the angle of the weighted mean of unit vectors, frac(atan2(Σc w[c]·sin 2πv[c], Σc w[c]·cos 2πv[c]) / 2π); where that mean's length is below 10⁻⁹, the value of the heaviest corner (the lowest on a tie). At count 2 (MO-11), the same sums run over the two corners MO-17's two-setting law selects, with that law's weights; its candidates are the distinct corner values, pooled as MO-17 pools settings. A Structural key blends by D.5. |
> | MO-10 | **Stepped** (B424). The resolver keeps a **step phase** φ and a **stepped position**. At reset, φ = 0 and the stepped position is the position at the first resolution. When `morph.stepSync` is off, φ advances by `morph.stepRate`/f_host each sample, so a step falls whenever ⌊φ⌋ increases, without rounding drift at fractional samples per step; a rate change changes only the advance from that sample on, and φ never jumps. When it is on and the transport is playing, φ = (the host's musical position in quarter notes, RC-7.2)/(the division's length in quarter notes, a bar taking the time signature's length, RC-7), and a jump (RC-7) sets φ to the new position's. When the transport is stopped (ADR-325, X34), φ advances by tempo/(60·f_host·the division's length) per sample from where it stood. At the first resolution of an interval in which ⌊φ⌋ has changed, the stepped position takes the current position. Between steps it holds. Stepped is Blend (MO-9, D.5) at the stepped position. **Every key reads the stepped position under Stepped: Follow keys, Quantum-mode keys' picks, and flip-only keys.** Values and structure weights therefore move in steps, each step's edge glided by MO-5 and PA-1. |
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
> | MO-14 | **Structural groups.** The Structural keys that are not flip-only (MO-15) form these groups, each named by its id. (1) **By gain:** `oscN.env` (that oscillator's `env`), `noise.env` (the noise's `env`), and `oscN.pan` (`panImage` and `panInvert`). (2) **`oscN.kernel`:** `baseWave`, `stacking`, `spreadLaw`, `quantize`, and every row of 5.4 for both blades. (3) **`oscN.swarm`:** `unison`, `distribution`, `seed`, `detuneLaw`, `detuneRange`, `pivot`, `octave`, `semitone`. If the four corners hold more than one setting of this group's blending keys, the kernel's keys join `oscN.swarm` and `oscN.kernel` does not exist. This is decided over the four corners, so it is fixed for the patch and changes only with a corner edit or a change of mode. It does not change with the position. (4) **`sub.chain`:** the sub's Structural rows but `voiceMode` and `priority`, with `env` included. The sub applies its envelope inside its chain ahead of the drive (SN-3), so `sub.env` cannot act by gain. (5) **`noise.chain`:** the noise's Structural rows but `env` and the reserved rows. (6) **`filter`:** the filter's own Structural keys, when a filter is assigned (RC-1). (7) **`fxN`:** each FX slot's structure, whose keys the rack defines (RC-5). A **setting** of a group is one value for each of its blending keys (MO-16). |
> | MO-15 | **Flip-only keys.** `voice.mode`, `oneShot`, `retrigger`, `sub.voiceMode`, `sub.priority` and every reserved row never blend: they act on the voice law or at a voice's start, where one note cannot hold two values. Each resolves to one value under every law and every mode **but Exempt** (MO-12). That value is its own pick and latch (MO-7, MO-8), at the stepped position under Stepped, so under Blend it is the seeded point of IF-4 before ADR-317. It changes on sounding voices by PA-5, or by VO-14 for `voice.mode`. |
> | MO-16 | **Which keys blend.** A key of a group **blends** when its mode is Follow and the law is Blend or Stepped. Every other key of the group resolves to one value (its pick under Quantum, its held value under Exempt), which the resolver emits as a `Param` event. Settings are formed over the blending keys only. |
> | MO-17 | **The setting set.** A blending group's candidates are the distinct settings its four corners hold. A candidate's **id** is the lowest corner index (A = 0 … D = 3) holding it. Its **pooled weight** Wₛ is the sum of the glided corner weights (MO-5 (a)) of the corners holding it. The group's **count** n is the smallest Count among its blending keys (As the morph reads `morph.count`; ADR-325), and is 2 for `fxN` whatever the choice. **At n = 4**, or with at most two candidates, the set is every candidate with Wₛ > 0, at weight Wₛ. **At n = 2** with three or more candidates, the candidates are ordered by Wₛ, highest first, ties broken by the lower id. With W₁ ≥ W₂ ≥ W₃, the set is the first two, at weights (W₁ − W₃)/(W₁ + W₂ − 2W₃) and (W₂ − W₃)/(W₁ + W₂ − 2W₃); where W₁ + W₂ − 2W₃ = 0 (the top three tie), the set is the first alone, at weight 1. A set's weights sum to 1. |
> | MO-18 | **Continuity at n = 2.** The two-setting law is continuous wherever the top three pooled weights do not all tie. Where the second and third trade places, both have weight 0. It is discontinuous only at a point where they all tie: the pad's centre when the corners hold four settings, or (0.5, ⅓) when they hold three with C = D. Near that point the weights turn steeply. **A path through that point is a switch there.** With four distinct corners the A–D and B–C diagonals pass through it: that is correct, and it is what the Two setting trades for its cost. The engine's slot rule (SB-11) carries the change. |
> | MO-19 | **An FX slot's set** (ADR-324 H10). An `fxN` group is resolved like any other, with n = 2, so at most two settings of a slot sound at once. How a slot renders its set is RC-5's. |
> | MO-20 | **What the resolver emits.** At each control interval it emits, as needed: a `Param` event for each Morphable key whose glided value changed, and for each Structural key that resolves to one value whose value changed; and a `SettingSet` event (IF-12) for each blending group whose set (ids, values or weights) changed. A group that stops blending (a mode change, or the corners coming to agree) gets one last set of one setting, which equals `Param` events setting its values. When `morph.on` turns off, it emits for each blending group a set of one setting, its heaviest (the lower id on a tie), and then nothing. |
>
> ### D.6 Rendering a setting set (the engine's half)
>
> | id | requirement |
> |---|---|
> | SB-1 | **One render per setting.** For each setting of a group's set, the engine renders the group once. (1) **By gain:** no render. The stage applies Σₛ wₛ·gainₛ, which is the same sound as mixing renders because the stage is linear in the signal. (2) **`oscN.kernel`:** the member kernel (the base wave, both blades and their edge corrections), once per setting, from the oscillator's one member state. (3) **`oscN.swarm`:** a whole oscillator instance per setting (SB-4). (4) and (5) **`sub.chain`, `noise.chain`:** the chain, once per setting, at the points PA-5 names for each. (6) **`filter`:** one filter instance per setting (RC-1). (7) **`fxN`:** by RC-5. A set is never rendered as a product of groups (MO-14 item 3). |
> | SB-2 | **The mix.** A group's output is Σₛ wₛ·yₛ, with equal gain (RS-3.6 of the rack's contract, ADR-324 H11). Each weight is a value glided by PA-1, per voice. The glided weights sum to 1 within 10⁻⁵. |
> | SB-3 | **The kernel's one member state.** Every setting of a kernel group renders from the oscillator's one member state, as PA-5's crossfade does. Where the member state reads the output (feedback and cross-mod, C.14), it reads the mixed output Σₛ wₛ·yₛ. |
> | SB-4 | **Several swarm instances.** While `oscN.swarm` holds several settings, each setting is a whole oscillator instance, with its own members (SW-1), its own coupling, drift and frame, and its own feedback and cross-mod loop, which reads that instance's output only. (a) Its per-note streams (DT-4) are seeded from that setting's `seed`. The noise oscillator's seed ("the voice's seed", osc 1's) is the `seed` of osc 1's heaviest setting at the voice's fresh start (the lower id on a tie), and is kept for the note. (b) The persistent placement and ensemble state (DT-4, C.2, C.7) is kept once per distinct `seed` among the four corners, and each instance uses the state of its own seed. (c) The sources the engine publishes for the oscillator (IF-3) are the mixture's: R·e^{iψ} = Σₛ wₛ·Rₛ·e^{iψₛ}, and the onset burst's level is Σₛ wₛ·levelₛ. (d) The observation view (IF-10) reports members per instance, with the instance's setting id. |
> | SB-5 | **A setting at weight 0 costs nothing.** A setting whose heard weight is exactly 0 renders nothing and counts nothing (IF-9). Where a group's set holds one setting, and at every corner, exactly one setting renders. |
> | SB-6 | **Waking.** A setting that enters a slot (SB-11) while a voice sounds starts from its fresh-start state, at the weight PA-1 raises from 0. For a swarm that is a fresh start of its members (ADR-324 H9). Its per-note streams are keyed by purpose, note counter, member and (for the swarm) its seed, never by the slot that renders it, so a setting renders the same in any slot. |
> | SB-7 | **A voice that starts in a blend.** A voice's fresh start loads every setting of each group's current set into its slots, each from its fresh-start state, with its weight set to the target, not glided. The note is the blend from its first sample. **A legato change** (VO-6) keeps every setting running, with its phases and states continuous, as it keeps one setting today. |
> | SB-8 | **A flip inside a blend.** When a key of a blending group that resolves to one value (MO-16) changes, every live setting of the group changes with it at once: by PA-5's 5 ms raised-cosine crossfade, except that `octave` and `semitone` change the frequency at once with every phase continuous (PA-5), and `unison` fades members in and out by PA-6. During a PA-5 crossfade, at most twice the group's settings render. A second change waits until it ends, as PA-5 says. |
> | SB-9 | **Limits.** At most four settings per group sound, at most two for a group whose count is 2, and twice those during SB-8's crossfade. The engine is sized for this (RT-1). |
> | SB-10 | **Corner exactness.** At each corner, and wherever every group's set holds one setting, the output is bit-identical to the same patch with the morph off and those values stored. |
> | SB-11 | **The slot rule.** Each voice keeps, per blending group, as many **slots** as the group's count n. At each control interval the engine applies the group's current set as follows. (a) **Matching settings.** An entry whose id and values match a slot's setting gives that setting its weight as the target. An entry whose id matches but whose values differ (a corner edit) is a new setting, and the old one's target becomes 0. (b) **Entering.** A setting not in a slot enters a **free** slot: an empty one, or one whose setting has target 0 and a heard weight below 10⁻⁴. It starts by SB-6. A freed setting's remaining heard weight r is added to the heard weight of the slot setting with the largest heard weight (the lower id on a tie), which then glides back to its target by PA-1. So the weights still sum to 1, and the output steps by under 10⁻⁴ of the two settings' difference (below −80 dB). (c) **Waiting.** While a setting of the set waits for a slot, its weight goes to the slot settings that are in the set, in proportion to their set weights. If no slot setting is in the set, the slot setting with the largest heard weight (the lower id on a tie) takes target 1 and the others 0, so the others empty and free their slots. (d) **A lower count.** When a group's count falls, slots above the new count admit no setting and are removed once freed. So any change of set, including the path through MO-18's point, a corner edit, a chimera that Quantum-mode keys make, and a change of count, law or mode, is a sequence of PA-1 glides, with never more settings sounding than SB-9 allows. |
> | SB-12 | **Per voice, and the mono sub.** Sets are the instrument's (one pad). Slots, heard weights and settings' states are per voice. The mono sub (SN-7, RC-6.6) keeps its `sub.chain` slots once for the instrument. |

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
> Every accepted item (0 to 33, and 15) stays green. Item 19 is judged as RC-7.3 now reads. The bank stage does not wait
> on it [X38];

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
- After: "…**a `Param` event whose id names no host parameter (an unknown id, or a reserved row's, §5.1) is refused and
  nothing is stored** (round 28, the counter `refused_param_id`); **a `SettingSet` event (IF-12) is refused whole, and
  nothing of it is stored, when it names no group, gives n other than 2 or 4 (or other than 2 for an FX slot), holds more
  entries than n, repeats an id, names an id outside 0 to 3, lists keys that are not that group's or a key twice, gives a
  value outside a key's range or options, or gives a weight that is not finite, is negative, or makes the weights' sum
  differ from 1 by more than 10⁻⁶ (the counter `refused_setting_set`); a `Param` event for a blending key of a group whose
  set holds more than one setting is refused (the counter `refused_param_in_set`)**; each refusal…" *(rest unchanged)*

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
  resent at the same samples. The engine reports the heard value of every stepped key per oscillator, **and the heard
  setting set of every structural group, with each setting's id and weight**, so the GUI shows what is heard where the
  morph and the stored patch differ." *(the State part, from "**State** is a key-value document…", unchanged)*

### 2.12 RT-1 (line 621)

- Before: "…room for PA-5's second setting on every oscillator, a second voice state for VO-6's mono re-run crossfade,…"
- After: "…room for PA-5's second setting on every oscillator; **for every structural group, up to four settings and SB-8's
  second of each: the kernel's on the one member state, the swarm's as up to four whole oscillator instances per
  oscillator per voice (eight during SB-8), with persistent state for up to four seeds (SB-4), and the sub's and the
  noise's chains; the filter slot's settings are sized by the filter once one is assigned (RC-1), and none while the slot
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

> | IF-12 | **The morph** | **The resolver's calls** (MO-1, MO-3): prepare (the host rate); reset; the corners (four maps of manifest key → v); the morph settings (D.2); each key's mode and count (MO-11); the position (x, y), sample-timed; the transport, as RC-7's events; and process (a sample count), which advances the resolver's clock and returns its events for those samples, each timed at a control-interval boundary. The shell passes them to the engine in the same block. **The `SettingSet` event**, timed like a `Param` event (PA-3): a group id (MO-14: `osc1.kernel`, `osc1.swarm`, `osc1.pan`, `osc1.env`, the same for `osc2`, `noise.env`, `sub.chain`, `noise.chain`, `filter`, `fx1`, …); the count n (2 or 4); the group's blending keys, by manifest key, in ascending manifest id order; and up to n entries, each an id (0 to 3, MO-17), a weight, and one value v for each listed key. A key that resolves to one value is never in a set: it arrives as a `Param` event and applies to every setting of its group (SB-8). Events apply in arrival order (PA-3), whatever their source. While the morph is on, the morph system routes the host's and the GUI's edits of morphing keys to the corners (ADR-109), so the resolver is their only source; a `Param` event for a blending key whose group's set holds more than one setting is refused (PA-7). Checked on entry by PA-7. |

### 2.16 RC-1, the filters

- Before: "- **Deferred:** the filter types, their count and how sources route to them [X13] …"
- After: before it, a new bullet:
> - **Must provide, when built:** a filter instance per setting of the `filter` group's set (SB-1), each cleared at its
>   start (SB-6), their outputs mixed by SB-2; its structural keys declared to the morph as the `filter` group (MO-14).

### 2.17 RC-5, the FX rack

- Before: "- **Deferred:** the rack, its modules, and the device's master limiter (which holds the device's peaks, OU-1)."
- After: that bullet unchanged, and a new one after it:
> - **Must provide, when built:** for each slot, the keys of its `fxN` group (MO-14: the module patch's structure, as the
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

### 2.20 RC-10 (line 775)

- Before: "- **What a later tier may vary:** a cap on the members per swarm, a longer control interval, the quality (High to
  Standard), a cheaper edge table."
- After: "- **What a later tier may vary:** a cap on the members per swarm, a longer control interval, the quality (High to
  Standard), a cheaper edge table, **a lower count of structure settings per group (MO-17's n = 2, or one setting by
  MO-7's pick)**." *(the "What no tier may vary" sentence unchanged)*

### 2.21 RC-11.1, RC-11.2 and RC-11.4

- RC-11.1, before: "The morph position, the macro values and the intents are the morph, macro and intent systems' own state (RC-4);
  they reach the engine only as parameter values, which a snapshot holds."
- RC-11.1, after: "The morph position, **the corners, the morph settings (D.2), each key's mode and count (MO-11)**, the macro
  values and the intents are the morph, macro and intent systems' own state (RC-4); they reach the engine only as
  parameter values **and setting sets (IF-12)**, and a snapshot holds the parameter values **(a blending group's as its
  heaviest setting's, IF-4)**."
- RC-11.2: after "(every parameter's v and the `ratios` text)" insert "**(no setting set: a set is an input, IF-4)**".
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
> 34. **the morph resolver** (Appendix D, MO-1 to MO-20), on its own calls (IF-12): items R1 to R16 of the B585 spec text;
> 35. **the structure blend** (D.6, SB-1 to SB-12; IF-12's event; PA-7's refusals): items E1 to E17.

### 2.27 §11

No new X items for ADR-325's rulings: X28 to X35 are written into the rows above. The open items that remain are §5's:
X36 to X40. They are added to §11.1, and its heading's count changes from (24) to (29).

### 2.28 `engine2/structure/engine-structure.json` (proposed as text; the file is not edited)

- **New items:**
  - `morph_resolver` (block, parent `instrument`, scope `engine`, cites MO-1 to MO-20 and IF-12, label "the morph
    resolver: weights, glide, the draw, the laws, modes and counts, setting sets");
  - `voice.structure_slots` (block, parent `voice`, scope `engine`, multiplicity "per structural group, up to its count
    (2 or 4)", cites SB-2, SB-5, SB-6, SB-7, SB-11, SB-12, label "a voice's structural slots: settings, heard weights,
    the slot rule").
- **Changed items:**
  - `modulation`: its label drops "and morph systems", and it adds "and the morph's editor and storage";
  - `osc`: its multiplicity reads "2: osc 1 and osc 2; × up to four swarm settings while `oscN.swarm` blends (SB-4)";
  - `member.base_wave`, `member.blade1`, `member.blade2`: multiplicity "× up to four kernel settings on one member state
    (SB-3)";
  - `sub` and `noise`: their chains are "× up to four `sub.chain` / `noise.chain` settings".
- **New edges:**
  - `modulation->morph_resolver` (control: corners, position, settings, modes);
  - `transport->morph_resolver` (event: RC-7, MO-10);
  - `morph_resolver->parameters` (control: `Param` events, MO-20);
  - `morph_resolver->voice.structure_slots` (control: `SettingSet` events, IF-12);
  - `voice.structure_slots->observation_view` (observation: IF-10);
  - `voice.structure_slots->counters` (observation: IF-9).
- **Spec blob:** updated to the ratified text's blob, when the round lands (B585 step 5).

---

## 3. The test items (engine-free, each with a must-fail control)

Items R1–R16 test the resolver through its own calls (IF-12): corners, position, settings, modes and transport in; events
out (§10.1 item 34). Items E1–E17 test the engine given `SettingSet` and `Param` events (item 35). **R6 to R11 and R15 run
at `morph.glide` 0**; R12 tests the glide.

### 3.1 The resolver (item 34)

| # | Item | Pass | Must-fail control |
|---|---|---|---|
| R1 | Weights (MO-4) | At 10,000 seeded positions the weights equal the bilinear formula within 10⁻¹² and sum to 1 | A transposed corner order (B and C swapped) fails |
| R2 | The draw's independence (MO-6) | Picks for every key at 27 positions are identical with and without 200 keys appended to the corners' maps, and with the maps' insertion order reversed | A draw taken from one stream in list order changes picks |
| R3 | The draw and the pick, exactly (MO-6, MO-7) | At the position (0.35, 0.6), T = 1, cohesion 0.3, these keys pick these corners: `osc1.blade1.mode`, `osc1.baseWave`, `osc1.unison`, `osc2.blade2.wave`, `sub.wave`, `noise.type`, `env1.attack`, `osc1.coupling` → seed 1: C C D B A C D D; seed 1024: C C C B A C C C; seed 9999: C B C A C D D C. The first draw of `morph/shared` at seed 1024 is u = 0.5761699904687703 (its FNV-1a state 0x5EE6354F). | A stream seeded with the seed before the purpose bytes, or a signed shift, misses the pins |
| R4 | Temperature and cohesion (MO-7) | At T = 0.02, every key picks the heaviest corner wherever that corner's weight is at least 1.9 times the next (ln 1.9 / 0.02 ≈ 32 exceeds the largest gap of two clamped Gumbel draws, ≈ 23.8, so this holds for every seed). At cohesion 1, every key picks the same corner at every position. | A pick without the temperature term fails the first |
| R5 | The latch (MO-8) | A Structural key dithered across a position where two corners' scores tie, so that their difference stays within ±m, flips at most once; carried until it exceeds m, it flips. On an edge with equal draw terms the band is 0.45 to 0.55. A Morphable key under Quantum follows its pick at every crossing. | A Structural key with no latch flips at every crossing |
| R6 | Blend at count 4 (MO-9) | Morphable values equal Σ w·v within 10⁻¹². A circular key equals the unit-vector mean within 10⁻¹², and at a zero resultant (corners 0 and 0.5 at equal weight) equals the heaviest corner's (the lower on a tie). | A linear sum on a circular key fails at the wrap; a sum in the law's unit (not v) fails |
| R7 | Count 2 on a Morphable key (MO-9, MO-11) | A key set to Two emits the two-setting law's blend of its pooled corner values. It is continuous away from MO-18's point and jumps through it. | A renormalised top-two law jumps on the diagonals |
| R8 | Stepped (MO-10) | The output is constant between steps and equals Blend at the stepped position. Steps fall where ⌊φ⌋ increases, at 3 steps per second at 44.1 kHz (14,700 samples each) and at 7 per second (fractional), with no drift over 10⁶ samples. The first held position is the reset one. A rate change mid-run shifts no step already passed. Synced: steps fall on the division boundaries of a scripted transport, and keep falling at the tempo's division length while it is stopped. Quantum-mode keys and flip-only keys read the stepped position. | Steps counted by a rounded period drift; a flip-only key reading the live position fails |
| R9 | Modes and counts (MO-11 to MO-13, MO-16) | An Exempt key emits corner A's value at every position, under every law, flip-only keys included. A Quantum key under Blend emits its pick. A Structural Quantum key arrives as a `Param` event and is never in a set. A Follow key under Quantum flips. One Two key makes its group's n 2. | A mode applied per group fails; the largest count winning fails |
| R10 | Groups and sets (MO-14, MO-17, MO-19) | At 2,000 seeded positions and corner layouts: candidates pool identical settings; ids are the lowest corner holding them; at n = 4 the set is every candidate with weight above 0 at its pooled weight; at n = 2 at most two, with MO-17's weights and tie-break; weights sum to 1 within 10⁻¹²; flip-only keys are never in a set; `oscN.swarm` carries the kernel's keys exactly when the corners differ in the swarm's keys, whatever the position; `sub.env` is in `sub.chain`; an `fxN` set has n = 2 whatever the choice | A merge decided at the position fails at the centre; an equal-power set fails the sum |
| R11 | Continuity at n = 2 (MO-18) | With four distinct settings, on an 801 × 801 grid, a setting not in the set counting as weight 0: no two neighbouring points farther than 0.1 from the centre differ in any weight by more than 0.02; the only jump is at the triple tie | The renormalised top-two law jumps on the diagonals |
| R12 | The glide (MO-5) | A step of the position from A to D reaches 1 − 1/e of the way, in every Morphable value and corner weight, after `morph.glide` seconds within one sample, at 44.1, 48 and 96 kHz, at block sizes 1, 64 and 4,096 alike. The four weights sum to 1 within 10⁻¹², snap together, and reach exactly [0, 0, 0, 1]. Quantum's picks read the unglided weights. | A coefficient computed per block fails at uneven blocks; per-weight snapping breaks the sum |
| R13 | Device keys (MO-2) | Over every Device row, at 1,000 positions under every law and mode, the resolver emits no event for a Device key | A resolver that blends `quality` emits one |
| R14 | Block size and transport (MO-3, RC-7.3) | A scripted session (moving position, setting and mode changes, transport events, corner edits) gives the same events at the same samples at block sizes 1 to 4,096. With `morph.stepSync` off, it gives the same events with and without the transport events. | Resolving once per block fails |
| R15 | The events (MO-20) | One-value keys arrive as `Param` events and blending groups as `SettingSet` events, with ids, key lists and weights by MO-17. A group that stops blending gets one last one-setting set. `morph.on` turning off sends each blending group its heaviest setting, then nothing. | A resolver that sends a one-value key inside a set fails |
| R16 | Reset (MO-3) | After reset, the events equal a freshly prepared resolver's for the same inputs | A latch kept across reset fails |

### 3.2 The engine (item 35)

| # | Item | Pass | Must-fail control |
|---|---|---|---|
| E1 | Corner exactness (SB-10) | A one-setting set renders bit-identically to the patch with the morph off and those values stored | A planted weight of 10⁻³ on another setting differs |
| E2 | Weight 0 costs nothing (SB-5, IF-9) | With every set holding one setting, the counters equal the no-morph patch's. A setting at weight 0 adds zero setting-samples. By-gain groups never add setting-samples. | A build that renders every listed entry reads more |
| E3 | The mix law (SB-1, SB-2) | At a static set with settled weights, on a patch with no feedback, no cross-mod and an empty filter slot, the output equals Σ wₛ·(the render of setting s alone, same values) within 10⁻⁶ of its RMS, for 2, 3 and 4 settings | An equal-power mix fails |
| E4 | By gain (SB-1 item 1) | A set on `oscN.env`, `noise.env` or `oscN.pan` renders one setting (counters) and meets E3's law | A two-render build reads double |
| E5 | Weights glide and sum to 1 (SB-2) | Heard weights sum to 1 within 10⁻⁵ at every sample of a moving set, and a weight jump follows PA-1's curve | Weights applied as steps fail PA-1's curve |
| E6 | No click across a blend (QS-4) | A 1 s sweep of a two-setting set from one setting to the other, on the probe rule's patch (a sine probe plus the one blade whose mode differs), makes no detected click | A planted bare switch at the midpoint is detected |
| E7 | Waking (SB-6) | A setting that enters mid-note equals, from its entry, the same setting started fresh at that sample. The same render at block sizes 1, 64 and 4,096 is bit-identical. | A wake that resumes stale state differs |
| E8 | Slot independence (SB-6) | A setting rendered in either slot, or alone, gives bit-identical samples from the same start | Streams keyed by slot differ |
| E9 | A voice that starts in a blend (SB-7) | With no feedback and no cross-mod, a note started at a static set equals Σ wₛ·(the note started in each setting alone) from its first sample | A voice that starts its heaviest setting only fails |
| E10 | Legato in a blend (SB-7) | A legato change during a two-setting set keeps both settings' phases and states continuous: the output equals a render in which only the pitch moved | A build that restarts settings on a legato change fails |
| E11 | A flip inside a blend (SB-8) | A one-value `mode` change in a two-setting kernel group crossfades both settings by PA-5's law (QS-4's law test), with at most four settings rendering. An `octave` change steps every setting's frequency at once, phases continuous. A `unison` change follows PA-6 in every instance. A second change waits. | Flipping one setting only fails the law test |
| E12 | Limits (SB-9) | Never more than four settings per group, two in a count-2 group, and twice those during SB-8 (counters, observation) | A planted build that admits a third setting into a count-2 group before a slot frees reads three |
| E13 | The slot rule (SB-11) | Scripted sequences: {A, B} → {C, D}; {A: 1} → {D: 1} (MO-18's point); a corner edit replacing B's values while four settings sound; count 4 → 2 while four sound. At most the allowed settings sound at every sample, heard weights sum to 1 within 10⁻⁵, slots free only below 10⁻⁴, the output steps by no more than 10⁻⁴ of the settings' difference, and E6's detector reads no click on the probe patch. | A build that frees a slot without passing its residual breaks the sum |
| E14 | The kernel's shared state (SB-3) | With feedback on, in a two-setting kernel group, the mixed y⁰ that the loop reads (IF-10) equals Σ wₛ·y⁰ₛ of the settings' y⁰ within 10⁻⁶ at every sample | A build that feeds back the heaviest setting's output alone fails |
| E15 | Swarm instances (SB-4) | Each instance's start phases and drift equal those of its setting rendered alone. The noise's streams follow osc 1's heaviest setting's seed at the fresh start, and keep it when the weights cross. Persistent placement follows each instance's seed. Published R, ψ and the onset level equal the mixture's within 10⁻⁶. An instance's feedback reads its own output. | Streams shared across instances fail; a published value of one instance fails |
| E16 | Sets as inputs (IF-4, IF-12, PA-7, RC-11) | The state and the snapshot hold the heaviest setting's values and no set. A save and load mid-blend, with the sets resent at the same samples, renders bit-identically. Each PA-7 case is refused whole and counted. A `Param` for a blending key in a multi-setting set is refused and counted. | Accepting an oversize set, or storing a set in the state, fails |
| E17 | Determinism (DT-1, DT-2) | A moving set at block sizes 1 to 4,096 is bit-identical | — |

---

## 4. Diff summary

**Rows added.**

| Where | Rows | Count |
|---|---|---|
| Appendix D (new) | D.0–D.6: MO-1 to MO-20, SB-1 to SB-12, the D.2 table of 9 morph settings | 32 rows + 1 table |
| §7.1 | IF-12, the morph (the resolver's calls; the `SettingSet` event) | 1 |
| Glossary | corner / pad / position; structural group; setting set | 3 |
| §1 In scope | the morph resolver | 1 bullet |
| §3 | the morph resolver (deliverable); `morph/` in the layout | 1 row, 1 line |
| BD-8 | the stage 7 amendment | 1 clause |
| §4.2 | a bullet on blending stages | 1 |
| §4.3 | the morph's laws and their rendering | 1 row (split from one) |
| VO's start table | structural settings | 1 row |
| RC-1, RC-5 | "must provide, when built" | 2 bullets |
| §10.1 | items 34 (resolver) and 35 (structure blend) | 2 items |
| §11.1 | X36 to X40 | 5 open items |
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
| PA-7 | `SettingSet` refusals; `Param` in a set refused |
| IF-3 | published R, ψ of the mixture |
| IF-4 | Appendix D is the law; sets are inputs; stored values are the heaviest setting's; resend after load; heard sets reported |
| IF-9 | structure setting-samples, defined once |
| IF-10 | heard sets; each kernel setting's y⁰ and the mixed y⁰; members per instance |
| RT-1 | sizing for sets, instances, seeds, the second voice state; no filter instances while the slot is empty |
| RC-6 | sets handed to a source |
| RC-7 "Accepted now", RC-7.3 | the morph's synced steps, its first consumer |
| RC-10 | a tier may lower the count |
| RC-11.1, RC-11.2, RC-11.4 | morph state; no set in a snapshot; resend at restore |
| QS-4 | blend sweep judged; flip inside a blend by the probe rule |
| PF-4 | structure counter at named pad positions, both counts |
| PF-7 | memory at four settings, eight during SB-8 |
| C.0 | SB-8 listed; weights are values |
| §10.1 item 19 | transport, with synced steps |
| §11.1 heading | (24) → (29) |

That is 28 rows changed (the RC-11 trio counted as three, the RC-7 pair as two).

**Test items added.** R1–R16 and E1–E17: 33 items, 32 of them with a must-fail control. E17 has none, as with DT-2's
existing item 3.

**Unchanged by design:**
- the manifest and its ids;
- the host lockfile;
- IF-4's state text;
- Appendix C's laws;
- PF-1's target;
- the rack contract, which gets a follow-up (§5).

At a corner, or with the morph off, the engine is bit-identical to today (SB-10).

---

## 5. Open points (what ADR-324 and ADR-325 do not settle), each with a recommendation

| # | Question for the human | Recommendation |
|---|---|---|
| X36 | **The morph settings' ranges and tapers:** step rate 0.25 to 32 per second on a log taper, temperature on a log taper, glide on a pow taper (legacy listed ranges, not tapers). | **As PROPOSED.** Log for rate and temperature, so a knob's middle is musical; pow for glide, finer near 0, as the manifest's time rows are. |
| X37 | **The synced division list** | **1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4T, 1/4, 1/2, 1 bar, 2 bars, 4 bars**, a bar being the time signature's (RC-7 carries it). |
| X38 | **Does the bank stage wait on the stage 7 amendment?** | **No, as with stage 5's** (ADR-317: "The bank stage continues on the current spec until that round lands"). Items 0–33 stay green, so the bank's renders are unaffected at a corner or with the morph off (SB-10). |
| X39 | **The morph settings as host parameters.** The legacy shell exposed position X and Y and ids 151 and 154–158 to the host. In this text they are the morph system's state, outside the engine's manifest and its lockfile. | **The shell exposes the position (x, y), `morph.on`, law, count, temperature, cohesion, step rate, sync and glide as host parameters,** from a morph manifest appended to the lockfile (ids below 0xB00000, IF-11). The engine never receives them. The seed stays patch state, not automatable. This is a shell row, not an engine one, so it is filed with the shell's acceptance (B308 H5). |
| X40 | **The `ratios` text across corners.** It is a per-oscillator text (custom `cutRule`), not a v, so it cannot sit in a setting. | **Corner A's text, never morphed, in 1.0**, as MO-12 treats an Exempt key. Corners that differ in it are reported by the editor. |

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

## 6. The critic's findings and where each is fixed

| Finding | Fixed in |
|---|---|
| **B1** glide on set weights undefined | MO-5: the four corner weights glide together before every law, with a joint snap; sets are computed from them; Morphable values glide separately with a snap; set weights are never glided in the resolver (PA-1 does it); R6–R11 and R15 at glide 0; R12 added |
| **B2** the setting-set interface | IF-12: the `SettingSet` event, with group id, n, key list, ids and values; one-value keys as `Param`; precedence by arrival order, with the shell routing edits to corners; PA-7 refusals; IF-4, RC-11.1, RC-11.2, RC-11.4: sets are inputs, the heaviest setting's values are stored, and sets are resent; identity by id plus values (SB-11 (a)), so SB-8's flips arrive as `Param` and are distinct from new settings |
| **M1** the resolver's clock, cadence, reset, transport | MO-3; MO-10 ("every key reads the stepped position"); RC-7 consumers and RC-7.3; item 19; R14, R16 |
| **M2** `sub.env` by gain is false | MO-14 item 4 (`sub.env` in `sub.chain`, with the reason); R10 |
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
