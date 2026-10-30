# The morph appendix: spec text for SPEC-ENGINE (B585 step 4's input)

*Origin: horde lead session, 2026-10-29. ADR-324 (records branch `lead-records-369`, horde #1274) rules B585's twelve
questions and places the morph in one normative appendix of SPEC-ENGINE. Drafted by a scoped implementer from
`docs/proposals/morph-architecture-blend.md` (horde #1265). Status: **PROPOSED**. It goes to a critic, then to the human;
the lead applies it to `specs/SPEC-ENGINE.md`. This file does not touch `specs/`.*

**How to apply it.**
- §1 is the new **Appendix D**. It goes after Appendix C.
- §2 lists every amended row, as before → after.
- §3 holds the suite's new items.
- §4 is the diff summary; §5 lists the open points.

Text to paste sits in quoted blocks or tables; everything else is commentary for the critic.

**What ADR-324 settles and this text takes as given:**
- **H1:** "at most two" and "up to four" are a global morph setting and a per-parameter option, layered with the law.
- **H2:** structure renders per structural group, with linear stages blending by gain.
- **H3:** pitch and count keys blend by layering.
- **H4:** the whole pad's bilinear weights.
- **H5:** CPU is reported, never limited by measurement.
- **H6:** Quantum is per key, by its existing draw.
- **H7:** under stepped, the mix steps.
- **H8:** per-key modes compose into the group.
- **H9:** a woken second swarm starts fresh.
- **H10:** an FX slot blends two instances.
- **H11:** equal gain everywhere.
- **H12:** the flip-only list.
- **Placement:** one appendix, one round.

**One correction to the proposal (#1265), from a check made for this text.** The proposal said "at most two" turns into a
switch "on any line where the second and third settings tie". That is wrong. On such a line both tied settings have
weight exactly 0, approached from either side, so the law is continuous across it. A grid scan of the law (801 × 801
points, the demo's own `groupTargets`) finds exactly one discontinuity:
- **Four distinct settings:** it sits at the centre. Outside a radius of 0.1 around it, no neighbouring pair of grid
  points (0.00125 apart) differs in any weight by more than 0.015.
- **Three settings, where the corners' loop visits all three** (C = D): it sits at (0.5, ⅓).
- **Three settings with B = C:** there is none. The largest neighbour change is 0.005.

The demo's −15.8 dB "switch" on the diagonal came from a path passing exactly through that point. Some such point is
unavoidable. A law that renders at most two settings and is exact on each edge maps the pad's boundary loop, A → B → D →
C → A, into the graph of settings and their pairwise mixes. When that loop visits three or more distinct settings it
cannot be contracted inside the graph, so the interior must break at least once. This is entailed, not measured. MO-12's
slot rule is therefore written for that point, and for PA-1's lag at a change of pair, which the demo measured (148 ms
of overlap on one sweep).

---

## 1. The new appendix (PROPOSED)

> ## Appendix D. The morph (normative)
>
> The morph holds four **corners**, each a full set of the manifest's values, and a **position** on a square **pad**.
> This appendix states how a position becomes the values and the structure the engine renders: the laws (Blend, Quantum,
> Stepped), the quantum draw, each parameter's own morph mode, how many structural settings may sound at once, and how a
> structural difference is rendered. Its rows are numbered MO (the morph's resolution) and SB (the engine's rendering of
> structure). [X28]
>
> ### D.0 Who computes what
>
> | id | requirement |
> |---|---|
> | MO-1 | **Two halves.** The **resolver** maps the corners, the position, the morph settings (D.2) and each parameter's morph mode (D.3) to (a) one value for each Morphable key, (b) one value for each Structural key that resolves to one value (D.5), and (c) a **setting set** for each structural group that blends (D.5). It is a pure function of its inputs and its own state (the stepped hold, MO-9; the quantum latch, MO-7), reads no clock, and allocates nothing after prepare. The **engine** applies (a) and (b) as parameter changes (PA-1, PA-5) and renders (c) by SB-1 to SB-9. The corners, the position and the morph settings are the morph system's state (RC-11.1) and reach the resolver as inputs. [X28] |
> | MO-2 | **Device keys** stay outside the morph: never blended, never flipped (5.1). |
>
> ### D.1 The position and the weights
>
> | id | requirement |
> |---|---|
> | MO-3 | **Corners and weights.** The corners sit at A = (0, 0), B = (1, 0), C = (0, 1), D = (1, 1). At a position (x, y) in [0, 1]², the **corner weights** are bilinear: w = [(1 − x)(1 − y), x(1 − y), (1 − x)y, xy], in that order, each in [0, 1] and summing to 1. Every law reads these weights; no law reads the position otherwise. |
> | MO-4 | **Morph glide.** Where `morph.glide` > 0, every value the resolver emits (each Morphable value in its normalised form v, and each weight of a setting set) passes through a one-pole of time constant `morph.glide` seconds, stepped on the engine's sample clock, before PA-1's smoother. A Structural key that resolves to one value is not glided: it flips by PA-5. At 0 the one-pole is bypassed. [X31] |
>
> ### D.2 The morph settings
>
> The morph's own settings belong to the patch (saved with it), not to the engine's manifest, and are never morphed.
>
> | key | label | range | default | what it does |
> |---|---|---|---|---|
> | `morph.law` | Morph mode | Blend, Quantum, Stepped | Quantum | The law every Follow parameter obeys (D.3). |
> | `morph.count` | Architectures | Two, Four | Four | How many settings of a structural group may sound at once under Blend and Stepped (D.5). [X29] |
> | `morph.temperature` | Temperature | 0.02 to 4, log | 1 | How sharply Quantum's corners own the pad: low, the nearest corner owns almost everywhere; high, the draw decides more (MO-6). |
> | `morph.cohesion` | Cohesion | 0 to 1, lin | 0.3 | How far all keys pick the same corner together under Quantum (MO-6). |
> | `morph.seed` | Morph seed | 1 to 9,999, step | 1024 | Seeds the quantum draw (MO-5). |
> | `morph.stepRate` | Step rate | 0.25 to 32 steps per second, log; or a tempo division when `morph.stepSync` is on | 4 per second | How often Stepped moves the held position (MO-9). |
> | `morph.stepSync` | Sync | off, on | off | On: Stepped's steps fall on tempo divisions of the host's musical position (RC-7). |
> | `morph.glide` | Morph glide | 0 to 5 s, pow | 0.008 s | MO-4. |
>
> The ranges and defaults of temperature, cohesion, seed and glide are the legacy shell's (its ids 154, 155, 156 and 158),
> as is the default law, Quantum. Step rate and sync are B424's and ADR-266 item 10's; their range is PROPOSED.
>
> ### D.3 The laws
>
> | id | requirement |
> |---|---|
> | MO-5 | **The quantum draw** (ADR-104; ADR-185's rule in ADR-186 §5(g)'s form). For each key, four Gumbel draws gₖ[c] = −ln(−ln u) are taken, one per corner c, with u the successive outputs of a mulberry32 stream seeded from `morph.seed` and the key (DT-4's form: named by purpose, here the key), each u clamped to [10⁻⁹, 1 − 10⁻⁹]. A **shared** draw g*[c] comes the same way from the purpose `morph.shared`. **No draw depends on a key's position in any list or on how many keys exist**, so appending a key to the manifest never changes how an existing patch sounds (ADR-185). Keys the manifest declares as one unit (ADR-109 A1) draw as their lead key. Blend and Stepped read no draw. [X32] |
> | MO-6 | **Quantum's pick.** For a key k at weights w, each corner scores sₖ[c] = λ[c] + (1 − h)·gₖ[c] + h·g*[c], with λ[c] = ln(w[c])/T where w[c] > 10⁻⁹, and −10¹² otherwise; T = max(0.02, `morph.temperature`) and h = `morph.cohesion`. The key's **pick** is the corner with the highest score (the lowest index on a tie). Its value is that corner's value: a Morphable key emits it as its v; a Structural key resolves to it as one value. |
> | MO-7 | **The latch (hysteresis band 0.1).** A key keeps the corner it holds until another corner's score exceeds the held corner's by more than m = ln(0.55/0.45)/T. On an edge where two corners hold equal draws, that is a band of exactly 0.1 of the pad around the even split; elsewhere it is the same margin in score. A key holds no corner before its first resolution and takes its pick then. The latch applies to Structural keys only; a Morphable key under Quantum follows its pick at once, glided by MO-4 and PA-1. [X33] |
> | MO-8 | **Blend.** A Morphable key emits Σc w[c]·v[c], its corner values blended in their normalised form (the shortest way round for circular keys). A Structural key blends by D.5. |
> | MO-9 | **Stepped** (B424). The resolver holds a **stepped position**: at each step it takes the current position, and between steps it holds it. Steps fall every 1/`morph.stepRate` seconds of the engine's sample clock since reset, or, with `morph.stepSync` on, on every boundary of the chosen tempo division of the host's musical position (RC-7). [X34] Stepped is Blend (MO-8, D.5) evaluated at the stepped position, so values and structure weights move in steps, each step's edge glided by MO-4 and PA-1. |
>
> ### D.4 Each parameter's own morph mode (B557)
>
> | id | requirement |
> |---|---|
> | MO-10 | **The menu.** Every Morphable and Structural key carries two per-patch choices, saved with the patch and never morphed. **Mode:** Follow (the default: it obeys `morph.law`), Quantum (it flips at its quantum boundaries, MO-6, whatever `morph.law` is) or Exempt (it holds one value, the patch's, and neither blends nor flips). **Architectures**, on Structural keys only: As the morph (the default: `morph.count`), Two or Four; it acts only while the key blends (Follow under Blend or Stepped) and is shown inactive otherwise. [X30] |
> | MO-11 | **What Quantum on a Structural key does.** A Structural key whose mode is Quantum resolves to one value under every law, flipping at its own quantum boundary (MO-6, MO-7) by PA-5: under Blend this is the behaviour before ADR-317. |
>
> ### D.5 Structure
>
> | id | requirement |
> |---|---|
> | MO-12 | **Structural groups.** The Structural keys form these groups, each rendered as a whole (SB-1). (1) **By gain:** each source's `env`; `panImage`; `panInvert`. (2) **Each oscillator's kernel:** `baseWave`, `stacking`, `spreadLaw`, `quantize`, and every row of 5.4 for both blades. (3) **Each oscillator's swarm:** `unison`, `distribution`, `seed`, `detuneLaw`, `detuneRange`, `pivot`, `octave`, `semitone`. When this group holds more than one setting, the kernel group's keys join it, and the two groups resolve as one. (4) **The sub's chain:** its Structural rows but `voiceMode`, `priority` and `env`. (5) **The noise's chain:** its Structural rows but `env` and the reserved rows. (6) **The filter slot:** the filter's own Structural keys (RC-1). (7) **Each FX slot:** its module's kind and its flip-class keys (D.6). A **setting** of a group is one value for each of its keys. |
> | MO-13 | **Flip-only keys.** `voice.mode`, `oneShot`, `retrigger`, `sub.voiceMode`, `sub.priority` and every reserved row never blend: they act on the voice law or at a voice's start, where one note cannot hold two values. Each resolves to one value under every law and every mode: its own pick and latch (MO-6, MO-7) at the current position, or at the stepped position under Stepped. Under Blend this is the seeded point of IF-4 before ADR-317. It changes on sounding voices by PA-5, or by VO-14 for `voice.mode`. |
> | MO-14 | **Which keys of a group blend.** A key of a group **blends** when it is Structural, not flip-only, and its mode is Follow under Blend or Stepped. Every other key of the group resolves to one value (its pick under Quantum, its held value under Exempt) and that value stands in every setting of the group. A group whose blending keys hold one setting across the four corners renders one setting. |
> | MO-15 | **The setting set.** A blending group's candidate settings are the distinct settings its four corners hold (blending keys from the corner, the other keys from MO-14). Each candidate's **pooled weight** Wₛ is the sum of the corner weights (MO-3, at the stepped position under Stepped) of the corners holding it. The group's **count** n is the smallest Architectures value among its blending keys (MO-10; As the morph reads `morph.count`), and is 2 for an FX slot whatever the setting (D.6). With n = 4 (or with at most two candidates), the setting set is every candidate with Wₛ > 0, at weight Wₛ. With n = 2 and three or more candidates, order the candidates by Wₛ, highest first (the lowest corner index on ties): with W₁ ≥ W₂ ≥ W₃, the set is the first two, at weights (W₁ − W₃)/(W₁ + W₂ − 2W₃) and (W₂ − W₃)/(W₁ + W₂ − 2W₃); where W₁ + W₂ − 2W₃ = 0 (the top three tie), the set is the first alone, at weight 1. The weights of a set sum to 1. |
> | MO-16 | **Continuity of the two-setting law.** The n = 2 law is continuous wherever the top three pooled weights do not all tie: where the second and third trade places, both have weight 0. It is discontinuous only at a point where they all tie (the pad's centre when the four corners hold four settings), and the weights turn steeply near it. A path through that point changes setting set there, and SB-10's slot rule carries the change. |
>
> ### D.6 Hosted FX modules (H10)
>
> | id | requirement |
> |---|---|
> | MO-17 | **An FX slot's structure follows the law**, by two module instances. Where a slot's corners differ in the module's kind or in a flip-class key that blends (MO-14), the slot runs one instance per setting, at most two (rack contract RS-6.3), each instance holding its setting's flip-class values and both receiving the same blended Morph-class values; their outputs are mixed with equal gain (RS-3.6) at the setting set's weights, which glide by MO-4 and PA-1. A flip that remains (a key that resolves to one value, MO-14, and every structural change under Quantum) is made by the module's declared `switch` (ADR-309) for a flip-class key, inside the instance that holds it, or by the slot's flip between module patches (ADR-188 item 2, RS-6.2) for a change of kind. SB-10's slot rule applies with two slots. |
>
> ### D.7 Rendering a setting set (the engine's half)
>
> | id | requirement |
> |---|---|
> | SB-1 | **One render per setting, per group.** For each setting of a group's set, the engine renders the group once: (1) by gain, no render: the stage applies the weighted sum of the settings' gains, Σ wₛ·gainₛ, which is the same sound as mixing the renders because the stage is linear; (2) the kernel, the member kernel (the base wave, both blades and their edge corrections) from the oscillator's one member state; (3) the swarm, a whole instance of the oscillator with its own member state; (4) and (5) the sub's or the noise's chain, at the points PA-5 names for them; (6) the filter slot, one filter instance per setting. Settings are never rendered as a product of groups: an oscillator whose swarm blends renders whole settings of its swarm and kernel together (MO-12 item 3). |
> | SB-2 | **The mix.** A group's output is Σₛ wₛ·yₛ over its settings, with equal gain (RS-3.6). Each weight is a value glided by PA-1 (after MO-4); PA-1's smoother is linear, so the glided weights still sum to 1. |
> | SB-3 | **One member state for the kernel.** Every setting of a kernel group renders from the oscillator's one member state, as PA-5's crossfade does. Where the member state reads the output (feedback and cross-mod, C.14) it reads the mixed output. |
> | SB-4 | **A setting at weight 0 costs nothing.** A setting whose heard weight is exactly 0 renders nothing and counts nothing (IF-9). Where a group's set holds one setting, and at every corner, exactly one setting renders. |
> | SB-5 | **Waking.** A setting whose weight rises from 0 while a voice sounds starts from its fresh-start state (for the swarm, a fresh start of its members: H9), at the weight PA-1 raises from 0. Its per-note streams (DT-4) are keyed by purpose, note counter and member, never by the slot that renders it, so a setting renders the same whichever slot holds it. |
> | SB-6 | **A voice that starts in a blend.** A voice's fresh start starts every setting whose weight is above 0, each from its fresh-start state, so the note is the blend from its first sample. A legato change keeps every setting running. |
> | SB-7 | **A flip inside a blend.** When a key that resolves to one value (MO-14) flips while its group holds several settings, every live setting of the group crossfades by PA-5's law at once; during it, at most twice the group's settings render. A second flip waits until it ends, as PA-5 says. |
> | SB-8 | **Limits.** At most four settings per group sound, at most two for a group whose count is 2 (SB-10), and twice those during SB-7's crossfade. The engine is sized for this (RT-1). |
> | SB-9 | **Corner exactness.** At each corner, and wherever every group's set holds one setting, the output is bit-identical to the same patch with the morph off and those values stored. |
> | SB-10 | **The slot rule for a count of 2.** A group whose count is 2 has two **slots**. At each control interval the engine takes the resolver's set (MO-15): (a) each setting of the set already in a slot gets the set's weight as its target; (b) a setting of the set not in a slot enters a slot that is **free** (empty, or holding a setting whose target is 0 and whose heard weight is below 10⁻⁴), from its fresh-start state (SB-5); a freed setting's remaining heard weight passes to the other slot's setting, so the weights still sum to 1 and the output steps by under 10⁻⁴ of the two settings' difference (below −80 dB); (c) while a setting of the set waits for a slot, its weight goes to the slot settings that are in the set, in proportion to their set weights; if neither slot setting is in the set, the slot setting with the larger heard weight takes target 1 and the other target 0, so it empties and frees its slot. So a change of set, including across MO-16's point, is a sequence of PA-1 glides with at most two settings sounding at every sample. |

**Where B557 lands.** The per-parameter mode menu has no spec row today. It lands as **MO-10** (the menu, its two
choices, its defaults) and **MO-11** (what Quantum on a Structural key means), in D.4. Its values per key are patch state
beside the corners (MO-1), not rows of the engine's manifest. So the manifest, the host-parameter lockfile and IF-4's
state text are unchanged by this round.

---

## 2. Every amended row, before → after (PROPOSED)

Line numbers are those of `specs/SPEC-ENGINE.md` on `origin/main` at `2af41d8`. Where only part of a row changes, the
unchanged rest is marked *(unchanged)*.

### 2.1 The front matter

**Line 9, "The laws are normative".**
- Before: "…Appendix C states it as math with its constants and units. …"
- After: "…Appendix C states it as math with its constants and units, **and Appendix D states the morph's laws the same
  way.** …" *(rest unchanged)*

**Line 12, "How to read it".**
- Before: "Requirements are numbered (`SW-4`, `VO-3`, `RC-2`, …); laws are numbered `C.0` to `C.16`. Each is testable."
- After: "Requirements are numbered (`SW-4`, `VO-3`, `RC-2`, `MO-5`, `SB-1`, …); laws are numbered `C.0` to `C.16`, and
  the morph's are Appendix D's rows. Each is testable."

**Line 34, §1 "Out of scope".**
- Before: "…the macros and intent bus, the morph engine, the pitch system…"
- After: "…the macros and intent bus, **the morph's editing, its corner storage and its pad (its laws are Appendix D, and
  the engine renders their structure, D.7)**, the pitch system…" *(rest unchanged)*

**§1 "In scope" gains a bullet** (after "The engine's interface to the rest of the instrument…"):
> - The **morph's laws** (Appendix D): the resolver that turns the corners and a pad position into values and structural
>   setting sets, and the rendering of those sets in the voice. [X28]

**§3, "What the build delivers", gains a row** (after "The engine's API"):
> | The morph resolver | Appendix D's resolver (MO-1 to MO-17), a pure unit with its own call: corners, position, morph settings and per-key modes in; values and setting sets out. [X28] |

### 2.2 The glossary (line 68), and three new terms

- Before: "| **Morphable / Structural / Device** | The three parameter classes: Morphable values blend between morph corners;
  Structural values are owned by one corner and flip; Device values stay outside the morph. |"
- After: "| **Morphable / Structural / Device** | The three parameter classes: Morphable values blend between morph corners;
  **Structural values follow the morph's law: under Blend each corner's setting is rendered and the renders are mixed,
  under Quantum they flip (Appendix D)**; Device values stay outside the morph. |"
- Added rows:
> | **corner**, **pad**, **position** | The morph's four full sets of values (A, B, C, D), the square they sit on, and the point on it that is heard (MO-3). |
> | **structural group** | The Structural keys that are rendered together (MO-12): the kernel, the swarm, a chain, the filter slot, an FX slot, or the keys that act by gain. |
> | **setting set** | For a structural group that blends, the distinct settings its corners hold that sound at a position, each with a weight, the weights summing to 1 (MO-15). |

### 2.3 §4.3 (line 169)

- Before: "| LFOs, the matrix and its routes (ENV 2 to 4's included), macros, intents, morph | the modulation and morph systems |
  IF-3, IF-4, RC-4 |"
- After: "| LFOs, the matrix and its routes (ENV 2 to 4's included), macros, intents | the modulation system | IF-3, RC-4 |"
  plus a new row: "| the morph's laws; its structure's rendering | the morph resolver; the engine (voice) | IF-4, Appendix D |"

### 2.4 §5.1, the Class bullet (line 194)

- Before: "- **Class**: `M` Morphable, `S` Structural (flips at a morph corner boundary), `D` Device (outside the morph). [Q17]"
- After: "- **Class**: `M` Morphable, `S` Structural (follows the morph's law: rendered per corner setting under Blend and
  Stepped, flipped under Quantum; flip-only keys always flip, MO-13), `D` Device (outside the morph). [Q17]"

### 2.5 PA-1 (line 549)

- Before: "…A change of any continuous parameter from the host, automation, a macro, an intent or the morph glides to its new
  value…"
- After: "…A change of any continuous parameter from the host, automation, a macro, an intent or the morph, **and each weight
  of a structural setting set (SB-2)**, glides to its new value…" *(rest unchanged)*

### 2.6 PA-5 (line 553)

Three edits; the rest is unchanged.
1. Before: "…from the old setting's sound to the new; the same holds when the morph flips one."
   After: "…from the old setting's sound to the new; the same holds when the morph flips one (a key that resolves to one
   value, MO-14, or a flip-only key, MO-13)."
2. Before: "…(changes in between are skipped), so at most two settings ever sound at once."
   After: "…(changes in between are skipped), so at most two settings of a key ever sound at once **outside a structure
   blend; inside one, SB-7 and SB-8 hold**."
3. Before: "`octave` and `semitone` change the frequency at once with every phase continuous, so they need no crossfade;"
   After: "`octave` and `semitone` change the frequency at once with every phase continuous, so they need no crossfade
   (under a blend they are settings of the swarm group, MO-12, and each setting keeps its own phases);"

### 2.7 IF-4 (line 671)

- Before: "The manifest's class column is the morph's input: Morphable values blend; Structural values flip at the morph's
  seeded point (hysteresis band 0.1) and change on sounding voices by PA-5; Device values stay out. The engine reports the
  heard value of every stepped key per oscillator, so the GUI shows what is heard where the morph and the stored patch
  differ."
- After: "The manifest's class column is the morph's input, **and Appendix D is its law**: Morphable values blend or flip
  by the law and the key's own mode (MO-5 to MO-11); a Structural key either resolves to one value, which changes on
  sounding voices by PA-5, or belongs to a structural group that blends, which the morph hands the engine as a **setting
  set** (MO-15) and the engine renders by D.7; Device values stay out. **A setting set** reaches the engine as an event
  per group, timed like a parameter change: up to four settings (each one value per key of the group, in its normalised
  form v) with a weight each, summing to 1. The engine reports the heard value of every stepped key per oscillator, **and
  the heard setting set of every structural group with each setting's weight**, so the GUI shows what is heard where the
  morph and the stored patch differ." *(the State part, from "**State** is a key-value document…", unchanged)*

### 2.8 IF-9 (line 676)

- Before: "**Operation counters**, deterministic and readable: member-samples rendered, edges corrected, edges left over
  (RT-2), envelope steps, voices sounding. [Q27]"
- After: "**Operation counters**, deterministic and readable: member-samples rendered, edges corrected, edges left over
  (RT-2), envelope steps, voices sounding, **and structure setting-samples rendered per group kind (MO-12), the count of
  settings beyond one that each group renders, summed over samples and voices (SB-4)**. [Q27]"

### 2.9 IF-10 (line 677)

- Before: "…and the heard stepped values; per voice, the four envelopes' levels and stages;…"
- After: "…and the heard stepped values; **per voice and structural group, the heard setting set: each setting's values,
  its target and heard weight, and its slot (SB-10)**; per voice, the four envelopes' levels and stages;…" *(rest unchanged)*

### 2.10 RT-1 (line 621)

- Before: "…room for PA-5's second setting on every oscillator, a second voice state for VO-6's mono re-run crossfade,…"
- After: "…room for PA-5's second setting on every oscillator, **for up to four settings of every structural group and
  SB-7's second of each (the kernel's on the one member state; the swarm's as up to four whole oscillator instances per
  oscillator per voice, eight during SB-7), on both oscillators, the sub, the noise and the filter slot**, a second voice
  state for VO-6's mono re-run crossfade,…" *(rest unchanged)*

### 2.11 RC-5, the FX rack (§7.2)

- Before: "- **Deferred:** the rack, its modules, and the device's master limiter (which holds the device's peaks, OU-1)."
- After: "- **Deferred:** the rack, its modules, and the device's master limiter (which holds the device's peaks, OU-1).
  **The FX slots' structure across the pad follows MO-17**, with two module instances per slot at most (the rack
  contract's RS-6.3)."

### 2.12 RC-10 (line 775)

- Before: "- **What a later tier may vary:** a cap on the members per swarm, a longer control interval, the quality (High to
  Standard), a cheaper edge table."
- After: "- **What a later tier may vary:** a cap on the members per swarm, a longer control interval, the quality (High to
  Standard), a cheaper edge table, **a lower count of structure settings per group (MO-15's n = 2, or one setting by
  MO-6's pick)**." *(the "What no tier may vary" sentence unchanged)*

### 2.13 RC-11.1 (line 788)

- Before: "The morph position, the macro values and the intents are the morph, macro and intent systems' own state (RC-4);
  they reach the engine only as parameter values, which a snapshot holds."
- After: "The morph position, **the corners, the morph settings (D.2), each key's morph mode (MO-10)**, the macro values and
  the intents are the morph, macro and intent systems' own state (RC-4); they reach the engine only as parameter values
  **and setting sets (IF-4)**, which a snapshot holds."

### 2.14 QS-4 (line 807)

- Before (the end of the row): "…any other mute or solo (IF-6) and all-sound-off (VO-13); the swarm's own grain (SW-16 at
  1) is measured and reported, not judged."
- After: "…any other mute or solo (IF-6) and all-sound-off (VO-13), **and a sweep of a setting set's weights from one
  setting to another (SB-2), on the probe rule's patch with only the blade or source whose setting differs**; the swarm's
  own grain (SW-16 at 1) is measured and reported, not judged. **A morph flip inside a blend (SB-7) is a stepped-key
  change by the probe rule: reported, and judged by PA-5's law test.**"

### 2.15 PF-4 (line 825)

- Before: "…and across a morph between corners with different stepped keys; no patch falls…"
- After: "…and across a morph between corners with different stepped keys, **reported with IF-9's structure counter at
  every corner, every edge's midpoint and the centre, at both counts (MO-15)**; no patch falls…" *(rest unchanged)*

### 2.16 PF-7 (line 828)

- Before: "The worst-case memory per instance, at 64 voices of 16 members, is stated in the manifest and reported."
- After: "The worst-case memory per instance, at 64 voices of 16 members **and every structural group at four settings
  (RT-1)**, is stated in the manifest and reported."

### 2.17 C.0 (line 1083)

- Before: "…PA-5's stepped-key crossfade, RC-10.1's tier change and RC-11.4's restore. …"
- After: "…PA-5's stepped-key crossfade, SB-7's flip inside a blend, RC-10.1's tier change and RC-11.4's restore. **A
  structural setting set's weights are values, not fades: they move with the pad and glide by PA-1 (SB-2), and SB-10's
  slot hand-over is such a glide.** …" [X35]

### 2.18 §10.1, two new items (after item 33)

> 34. **the morph resolver** (Appendix D), engine-free on its pure unit: the weights (MO-3), the draw's independence of
>     list position (MO-5), the pick and the latch (MO-6, MO-7), Blend and Stepped (MO-8, MO-9), the modes (MO-10, MO-11),
>     the groups, flip-only keys and setting sets at both counts (MO-12 to MO-16), and the FX slot's count (MO-17);
> 35. **the structure blend** (D.7): corner exactness, agreement and weight-0 cost, the mix law, waking, a voice starting
>     in a blend, a flip inside a blend, the limits and the slot rule, through the observation view and the counters
>     (SB-1 to SB-10).

### 2.19 §11.1, new open items (PROPOSED; each built as written until ruled)

> | X28 | who builds the morph resolver | the clean room, as a pure unit beside the engine (MO-1, §3) | MO-1, §1, §3 |
> | X29 | the default of `morph.count` | Four | D.2 |
> | X30 | Architectures on Morphable keys, and a group's count | Structural keys only; a group's count is its blending keys' smallest | MO-10, MO-15 |
> | X31 | where morph glide acts | on the resolver's values and weights, before PA-1; not on single-value structural flips | MO-4 |
> | X32 | the quantum draw, keyed by key (B308 H3) | MO-5 as written; legacy presets' quantum patchwork is not reproduced | MO-5 |
> | X33 | the latch's margin | ln(0.55/0.45)/T in score | MO-7 |
> | X34 | synced steps while the transport is stopped | steps at the division's length at the current tempo, on the sample clock | MO-9 |
> | X35 | weight glides under C.0's raised-cosine rule | weights are values glided by PA-1, not fades | C.0, SB-2, SB-10 |

The §11.1 heading count changes from (24) to (32).

---

## 3. The test items (engine-free, each with a must-fail control)

These revise the proposal's twelve for H1's layered count. Items R1–R10 test the resolver, a pure function: corners,
position, settings and modes in; values and setting sets out (§10.1 item 34). Items E1–E13 test the engine given setting
sets (item 35).

### 3.1 The resolver (item 34)

| # | Item | Pass | Must-fail control |
|---|---|---|---|
| R1 | Weights (MO-3) | At 10,000 seeded positions the weights equal the bilinear formula within 10⁻¹² and sum to 1 | A transposed corner order (B and C swapped) fails |
| R2 | The draw's independence (MO-5) | Picks for every key at 27 positions are identical with and without 200 keys appended to the key list, and with the list reversed | A draw taken from one stream in list order changes picks |
| R3 | Seed and determinism (MO-5, MO-6) | Same seed and position: the same picks twice; the next seed changes at least one pick of 128 keys at an off-corner position | A pick that ignores the seed fails |
| R4 | Temperature and cohesion (MO-6) | At T = 0.02, every key picks the heaviest corner wherever that corner's weight is at least 1.9 times the next (ln 1.9 / 0.02 ≈ 32 exceeds the largest possible gap of two clamped Gumbel draws, ≈ 23.8, so this holds for every seed); at cohesion 1, every key picks the same corner at every position | A pick without the temperature term fails the first |
| R5 | The latch (MO-7) | A Structural key dithered across a position where two corners' scores tie, so that their score difference stays within ±m, flips at most once; carried until the difference exceeds m, it flips; a Morphable key under Quantum follows its pick at every crossing, with no latch | A Structural key with no latch flips at every crossing of the tie |
| R6 | Blend (MO-8) | Morphable values equal the weighted sum in v within 10⁻¹²; a circular key takes the shortest way | A sum in the law's unit (not v) fails |
| R7 | Stepped (MO-9) | The output is constant between steps and equals Blend at the stepped position; steps fall every 1/rate s on the sample clock, independent of block size; synced steps fall on the division boundaries of a scripted transport | Steps counted in blocks fail at uneven block sizes |
| R8 | Modes (MO-10, MO-11, MO-14) | An Exempt key emits its held value at every position; a Quantum key under Blend emits its pick; a Structural Quantum key stands as one value in every setting of its group; a Follow key under Quantum flips | A mode applied per group, not per key, fails |
| R9 | Groups and sets at both counts (MO-12 to MO-15) | At 2,000 seeded positions and corner layouts: candidates pool identical settings; at n = 4 the set is every candidate with weight above 0 at its pooled weight; at n = 2 at most two, with MO-15's weights; weights sum to 1 within 10⁻¹²; flip-only keys (MO-13) are never in a set; a group's count is its blending keys' smallest (a "Two" key makes its group 2); an FX slot is 2 whatever the setting; a swarm group's set carries the kernel's keys | A renormalised top-two law (W₁/(W₁+W₂)) fails continuity (R10); an equal-power set fails the sum |
| R10 | Continuity at n = 2 (MO-16) | With four distinct settings, on an 801 × 801 grid, no two neighbouring points farther than 0.1 from the centre differ in any weight by more than 0.02; the only point with a jump is where the top three tie | The renormalised top-two law jumps on the diagonals |

### 3.2 The engine (item 35)

| # | Item | Pass | Must-fail control |
|---|---|---|---|
| E1 | Corner exactness (SB-9) | A one-setting set renders bit-identically to the patch with the morph off and those values stored | A planted weight of 10⁻³ on another setting differs |
| E2 | Agreement and weight 0 cost nothing (SB-4) | With every set holding one setting, IF-9's counters equal the no-morph patch's; a setting at weight 0 adds zero setting-samples | A build that renders every candidate reads more |
| E3 | The mix law (SB-1, SB-2) | At a static set with settled weights, on a patch with no feedback, no cross-mod and an empty filter slot: the output equals Σ wₛ·(the render of setting s alone, same values) within 10⁻⁶ of its RMS, for 2, 3 and 4 settings | An equal-power mix fails |
| E4 | By gain costs nothing (SB-1 item 1) | A set on `env`, `panImage` or `panInvert` renders one setting (counters) and meets E3's law | A two-render build reads double |
| E5 | Weights glide and sum to 1 (SB-2) | Observed heard weights sum to 1 within 10⁻⁵ at every sample of a moving set; a weight jump follows PA-1's curve | Weights applied as steps fail PA-1's curve |
| E6 | No click across a blend (QS-4) | A 1 s sweep of a two-setting set's weights from one setting to the other, on the probe rule's patch (a sine probe plus the one blade whose mode differs), makes no detected click | A planted bare switch at the midpoint is detected |
| E7 | Waking (SB-5) | A setting woken mid-note renders from its fresh-start state: equal, from the wake, to the setting started fresh at that sample; the same render at block sizes 1, 64 and 4,096 is bit-identical | A wake that resumes stale state differs |
| E8 | Slot independence (SB-5) | Setting B rendered in either slot, or alone, gives bit-identical samples from the same start | Streams keyed by slot differ |
| E9 | A voice that starts in a blend (SB-6) | With no feedback, a note started at a static set equals Σ wₛ·(the note started in each setting alone) from its first sample | A voice starting the heaviest setting only fails |
| E10 | A flip inside a blend (SB-7) | A one-value key flipping in a two-setting group crossfades both settings by PA-5's law (QS-4's law test); at most four settings render during it, and a second flip waits | Flipping one setting only fails the law test |
| E11 | Limits (SB-8) | Never more than four settings per group, two for a count-2 group, and twice those during SB-7 (counters, observation) | A five-setting set is refused and counted (PA-7) |
| E12 | The slot rule (SB-10) | For scripted set sequences, among them a jump from {A, B} to {C, D} and a jump from {A: 1} to {D: 1} (MO-16's point): at most two settings sound at every sample; heard weights sum to 1 within 10⁻⁵; each slot is freed only below 10⁻⁴; the output steps by no more than 10⁻⁴ of the settings' difference; E6's detector reads no click on the probe patch | A build that admits the incoming setting before a slot frees renders three; one that frees without passing the residual breaks the sum |
| E13 | Determinism (DT-1, DT-2) | A moving set, renders at block sizes 1 to 4,096, bit-identical | — |

---

## 4. Diff summary

**Rows added.**

| Where | Rows | Count |
|---|---|---|
| Appendix D (new) | D.0–D.7; MO-1 to MO-17; SB-1 to SB-10; the D.2 table of 8 morph settings | 27 rows + 1 table |
| Glossary | corner / pad / position; structural group; setting set | 3 |
| §1 In scope | the morph's laws | 1 bullet |
| §3 | the morph resolver | 1 deliverable |
| §4.3 | the morph's laws and their rendering | 1 row (split from one) |
| §10.1 | items 34 (resolver) and 35 (structure blend) | 2 items |
| §11.1 | X28–X35 | 8 open items |

**Rows changed.**

| Row | Change |
|---|---|
| Line 9 (laws are normative) | names Appendix D |
| Line 12 (how to read) | MO and SB numbering |
| §1 Out of scope | "the morph engine" narrowed to its editing, storage and pad |
| Glossary, line 68 | Structural follows the law |
| §4.3, line 169 | the morph moves to its own row |
| §5.1 Class, line 194 | `S` follows the law; flip-only keys |
| PA-1 | setting-set weights glide |
| PA-5 | three edits: which flips, "at most two" scoped, octave/semitone under a blend |
| IF-4 | Appendix D is the law; setting sets as events; heard sets reported |
| IF-9 | structure counter |
| IF-10 | heard setting sets in the observation view |
| RT-1 | sizing for four settings and SB-7 |
| RC-5 | FX slots follow MO-17 |
| RC-10 | a tier may lower the count |
| RC-11.1 | corners, morph settings and modes are morph state |
| QS-4 | blend sweep judged; flip inside a blend by the probe rule |
| PF-4 | structure counter at named pad positions, both counts |
| PF-7 | memory at four settings |
| C.0 | SB-7 listed; weights are values, not fades |
| §11.1 heading | (24) → (32) |

That is 20 rows changed.

**Test items added.** R1–R10 (resolver, item 34) and E1–E13 (engine, item 35): 23 items, 22 with a must-fail control.
E13 has none, as with DT-2's existing item 3.

**Unchanged by design:**
- the manifest and its ids;
- the host-parameter lockfile;
- IF-4's state text;
- every Appendix C law;
- PF-1's target.

At a corner, or with the morph off, the engine is bit-identical to today (SB-9).

---

## 5. Open points (what ADR-324 does not settle), each with a recommendation

| # | Question for the human | Recommendation |
|---|---|---|
| X28 | **Who builds the resolver?** §1 put the morph engine out of the engine's scope. One appendix "in SPEC-ENGINE" makes its laws normative there, but does not say who builds them. | **The clean room, as a pure unit beside the engine** (MO-1), tested engine-free by item 34. The corners, the position and the settings stay the shell's state (RC-11.1), so the state format does not change in this round. |
| X29 | **The default of `morph.count`**: Four or Two? | **Four.** No discontinuity anywhere on the pad. "Two" is the per-key or per-patch economy. |
| X30 | **Is Architectures offered on Morphable keys too?** On a Morphable key it would mean "blend only the two heaviest corners", a sharper morph at no cost. And when a group's keys choose different counts, which wins? | **Structural keys only, in 1.0. A group takes its blending keys' smallest count**, so a key set to Two never sounds more than two. |
| X31 | **Where does morph glide act?** On the position, so all keys glide alike and a Quantum flip is only delayed, or on the resolver's values? | **On the values and weights, before PA-1**, as the legacy shell's one-pole did (`morphGlide`, applied per value). It softens Quantum's continuous flips and Stepped's step edges, as B424 asks. Single-value structural flips keep PA-5. |
| X32 | **The draw keyed by key.** ADR-186 §5(g) requires a position-free draw seeded by (seed, key), "its own ADR before the first horde 2 patch is saved" (B308 H3). ADR-324 H6 says "by its existing draw (ADR-185), so no existing patch moves". The legacy draw is positional (the layout-9 prefix), so legacy presets ported to horde 2 cannot keep their quantum patchwork under a keyed draw. | **Rule MO-5 as written, and record it as B308 H3's ADR.** The legacy plugin keeps its own draw (ADR-186 item 1). Ported presets' quantum patchwork changes once, at the port, which is allowed before the stability line (ADR-186 (f)). Every horde 2 patch after that keeps its patchwork forever (ADR-185's rule, by construction). |
| X33 | **The latch's definition.** IF-4's "hysteresis band 0.1" was never defined, and the legacy shell has none (`src/morph_core.h` picks with no hysteresis). | **A score margin ln(0.55/0.45)/T** (MO-7): exactly a 0.1 band on an even edge split, and cheap. The demo uses it. |
| X34 | **Synced steps with the transport stopped.** | **Steps at the division's length at the current tempo, on the sample clock**, so Stepped keeps stepping while auditioning. |
| X35 | **C.0 says every crossfade is raised-cosine (ADR-239).** Setting-set weights, and SB-10's hand-overs, glide by PA-1 instead. | **Treat the weights as values, not fades** (C.0's amendment). They move with the pad, and PA-1's glide of a full-range jump already makes no click by QS-4's detector. SB-7's flip inside a blend stays raised-cosine. |

**Not in this round, and why** (for the critic, not for ruling). Each acts on Morphable values and has a lab but no
spec text yet:
- waypoints and their law (B235, B272);
- Quantum boundaries edited by hand, with locks (B269 item 5);
- authored bias and pin (ADR-109);
- the off-corner rule (B232);
- the intent bus's reshaping of the field (B170).

They stack on MO-6 and MO-8 without changing D.5 to D.7, because a Structural key has no value to pin or bias in between
corners. Each should join Appendix D by its own round. The human chose one appendix, so they belong there, not in a
second spec.

**Evidence consulted:**
- ADR-324, ADR-317, ADR-104, ADR-109 (and A1), ADR-185, ADR-186 §5(g), ADR-188, ADR-239 (through C.0), ADR-266 item 10,
  ADR-291 and ADR-309, on `origin/lead-records-369`;
- ROADMAP B308, B424, B557, B585 on the same branch;
- `specs/SPEC-ENGINE.md` at `2af41d8` (read only), at every line cited above;
- `src/morph_core.h` (no hysteresis; the score, the clamps) and `src/hypersaw_clap.cpp` lines 525–536 (the legacy
  morph settings' ranges and defaults; read only);
- `docs/proposals/morph-architecture-blend.md` and `docs/design/morph-blend-demo.html`, whose `groupTargets` the
  continuity scan ran (801 × 801 points, the results above).

The clean-room repositories were not read.
