# Architecture follows the morph law: the design proposal (B585 step 1)

*Origin: horde lead session, 2026-10-29, ADR-317 and ROADMAP B585 (records branch `lead-records-363`). Written by a
scoped implementer. Status: **PROPOSAL** for the human's rulings (B585 step 3). Every spec text below is **PROPOSED**;
nothing in `specs/` is changed by this document. The demonstration (B585 step 2) is
[`docs/design/morph-blend-demo.html`](../design/morph-blend-demo.html).*

**How to read the claims.** A number marked **measured** came from the demo's own functions, run headless in Node and
shown on the page. It was measured on a **stand-in voice** (one member, one blade, one filter), not on horde's engine. A
number marked **entailed** is arithmetic on figures already in ROADMAP. Its premises are labelled, and it is as good as
they are. Anything else is a reading of the record, with its source cited, or a recommendation. **Nobody has listened yet.**
The demo plays every law; your ears are the gate this document cannot pass for you.

---

## 0. The answers in one table

| # | Question | Recommendation |
|---|---|---|
| Q1 | How many architectures sound at once inside the pad | **Up to four**: every distinct corner setting whose weight is above 0. It is the only law with no switch line anywhere. |
| Q2 | How fine the dual render is | **Per structural group** (§2): a blade-and-wave kernel on the one shared member state, a whole oscillator only when its swarm differs, by gains where the stage is linear. Never per key. |
| Q3 | The blend region | **The whole pad's bilinear weights**, the same weights a Morphable value gets. No band, and no draw is read. |
| Q4 | The CPU budget, and past it | **Reported, never limited by measured CPU** (PF-4 with a render count). The author's lever is the per-parameter Quantum mode (B557). A later quality tier (RC-10) may cap it. |
| Q5 | Where Quantum and stepped place each flip | **Quantum:** each key at its own quantum boundary, with its existing draw, band 0.1, by PA-5. **Stepped:** Blend's weights at the stepped position, so the mix steps. |
| Q6 | The per-parameter mode (B557) | **Per key, as ruled.** A group's settings are built from its Follow keys; a Quantum or Exempt key gives every setting one value. Quantum on a structural key restores today's flip. |
| Q7 | A voice that starts inside a blend region | **It starts every setting with weight above 0** from the first sample, each from its fresh-start state. A new note sounds like the held ones. |
| Q8 | Hosted FX modules (a declared `switch`, ADR-309) | **The slot blends two instances** at the weights, equal gain, at most two (RS-6.3's cap). The declared `switch` still governs every flip that remains. An optional in-module blend is a later contract addition. |

The measured core of it, on the demo's default case (Sync + LP at A, FM + BP at B, the puck swept from A to B in 3 s):

| law | click (dB) | switch (dB) | structure renders per voice, mean while moving |
|---|---|---|---|
| flip, no crossfade (the must-fire control) | +8.6 | +1.5 | 2.00 |
| **flip at the seeded point (today, IF-4)** | +0.2 | **+1.2** | **2.00** |
| **Blend, up to four (ADR-317)** | +0.0 | **−46.1** | **4.00** |
| Blend, at most two | +0.0 | −46.1 | 4.00 |
| Quantum | +0.1 | +1.5 | 2.00 |
| Stepped, 4 Hz | +0.1 | −23.2 | 4.00 |

Today's flip makes no click (PA-5 works), but it is still a switch: the whole sound changes within about one cycle
(switch reading +1.2 dB). Blend spreads the same change over the whole move (−46.1 dB). The cost is twice the
structure renders through the region. Section 4 has the full tables.

---

## 1. What changes, and who owns which half

**Today.** SPEC-ENGINE IF-4 says: "Morphable values blend; Structural values flip at the morph's seeded point (hysteresis
band 0.1) and change on sounding voices by PA-5". The glossary (line 68) and §5.1's class column (line 194) say the same:
Structural "flips". PA-5 (line 553) crossfades a flip over 5 ms, equal gain, "both settings render from one shared member
state", and "at most two settings ever sound at once".

**The ruling (ADR-317).** Under Blend, a structural difference crossfades: both architectures run, with an equal-gain
fade across the pad (RS-3.6). Under Quantum it flips at the quantum boundaries. Under the stepped hybrid (B424) it steps.
A parameter's own morph mode (B557: Follow, Quantum, Exempt) still overrides.

**Two halves.** SPEC-ENGINE §1 puts "the morph engine" out of the engine's scope: "the engine accepts their outputs and
does not compute them". So the ruling has two halves:
- **The morph's half.** For each structural key, the resolver must hand over a **setting set**: up to four settings and
  their weights, not one value. For a Morphable key it hands over one blended value, as before.
- **The engine's half.** The engine must render every setting in the set and mix them. IF-4 is the seam between the two,
  so the spec round changes IF-4 and adds a short requirement block (§5 here: SB-1 to SB-10).

The morph's resolver has no spec of its own in horde 2 yet. Its laws live in ADR-104, ADR-109, ADR-185, ADR-291 and the
morph editor lab. That is a risk (§7, R6).

---

## 2. The structural keys, sorted by how they can follow Blend

§5.2 to §5.6 hold 59 rows of class `S` (counted once per table, before the per-oscillator and per-blade copies), of
which 9 are reserved. Each falls into one of four kinds. The kind decides
what a blend costs and whether it can be exact. **This table is the backbone of Q2.**

| Kind | Keys (per oscillator unless named) | How it blends | Extra cost per extra setting |
|---|---|---|---|
| **G: by gain** (the stage is linear in the signal) | each source's `env`; `panImage`; `panInvert` | Mix the settings' gains, not their renders. `Σ wₖ·(gainₖ·x)` equals `(Σ wₖ·gainₖ)·x`, so one render gives exactly the crossfade. For `env`, all four envelopes already run. | ≈ 0 |
| **K: the member kernel** (reads the shared member state) | `baseWave`, `stacking`, `spreadLaw`, `quantize`; every row of §5.4 for both blades (`on`, `mode`, `wave`, `cutIn`, `mirror`, `frame`, `rotation`, `fmType`, `modShape`, `modIn`, `cutRule`, `fmSource`, `spreadSource`) | Render the kernel (the base wave and both blades, with edges) once per setting from **one** member state, as PA-5 already does for 5 ms. Mix at the kernel's output. | The kernel: about 80–88 % of a heavy voice (B441, on the composed engine) |
| **S: the swarm** (changes the member state itself) | `unison`, `distribution`, `seed`, `detuneLaw`, `detuneRange`, `pivot`, `octave`, `semitone` | Two settings cannot share members, because the pitches, counts or placements differ. Render a whole instance of the oscillator per setting, with its own member state. | A whole oscillator |
| **Chains** | the sub's `order`, `wave`, `bumpHarmonic`, `bump2Harmonic`, `octave`, `semitones`, `keytrack`, `character`; the noise's `type`, `length`, `lfsrKeytrack`, `resonator`, `whisper`, `wind`, `filter`; the filter slot's own keys (RC-1) | Render the chain once per setting, at the points PA-5 already names (the sub's whole chain for `order`; a noise stage's output against its input; the noise filter's outputs). | The sub is small (PF-1's arithmetic puts it at about 2 % of the sources). The noise is moderate. The filter slot is the filter's own cost. |
| **F: flip-only** (cannot hold two values) | `voice.mode`, `oneShot`, `retrigger`, `sub.voiceMode`, `sub.priority`, and every reserved row | Never blends. These act on the voice law or at a voice's start, where one note cannot have two values. `voice.mode` even frees every voice (VO-14). | — |

**Two readings to check.**
- `quantize` ("snaps the cut-rate spreads and rules to whole harmonics") and `spreadLaw` act on the blades' values, not on
  the member state, so I placed them in K.
- `pivot` changes the coupling's target, so the members' phases differ, so I placed it in S.

Both are readings of the manifest's one-line descriptions, not of the laws in Appendix C.

---

## 3. The eight questions

Costs use these columns: **CPU** per voice, relative to today's one setting; **memory**; **latency**; **click risk**;
**spec and test churn**; **draw** (the effect on the frozen quantum draw).

**About the draw.** ADR-185's freeze is the legacy shell's (layout-9 prefix). ADR-186 freezes the legacy plugin. horde 2's
draw is meant to be position-free, keyed per key (B308 H3; the Random lab treats it as a prerequisite, not yet ruled). The
recommended options below **consume no new draw**, so they are neutral under either draw design.

### Q1. How many architectures may sound at once at a point inside the pad?

| Option | What it does | CPU | Memory | Click risk | Churn | Draw |
|---|---|---|---|---|---|---|
| **(a) Up to four** | Every distinct corner setting of a group with weight above 0 renders, at its bilinear weight. Corners holding the same setting pool their weight, so a pad whose corners hold two settings renders two everywhere. | ×(number of distinct settings), at most ×4, and only off the corners | Room for four settings per group (RT-1) | None: weights are continuous everywhere | IF-4 and PA-5's "at most two" change | none |
| (b) At most two, the top pair renormalised | The two heaviest only, weighted `w₁/(w₁+w₂)` | at most ×2 | Room for two (PA-5 already sizes one extra setting) | **A jump** wherever the second and third trade places | as (a) | none |
| (c) At most two, by excess over the third | The two heaviest, each weighted by its excess over the third (the demo's "at most two") | at most ×2 targets, but PA-1's glide overlaps a third where the pair changes | Two, plus a slot rule (below) | Continuous where the 2nd and 3rd trade places. **A switch on any line where they tie**: the diagonals, when all four corners differ. | as (a) | none |

**Measured (c) on the demo** (four distinct corners; Sync, FM, Fold and Ring blades; three filter types):
- **Along the diagonal A→D:** B and C tie all the way, so only A sounds until the centre, then only D. The switch reading is
  **−15.8 dB at the centre**: a flip glided by PA-1. Up to four reads −42.1 dB on the same path.
- **On a path near the centre (y = 0.4):**
  - a **+5.9 dB level bump**, where up to four reads none;
  - a third setting **overlapping for 148 ms** of the sweep while PA-1 glides each change of pair (self-check C4).

So a hard cap of two needs a slot rule as well: a new setting waits until an outgoing one has decayed. That is a
second law to specify and test.

**Recommendation: (a), up to four.**
- It is the only option with no switch anywhere on the pad, which is what "the sound fades between them across the pad"
  asks for.
- Its cost is bounded by the corners, not by the keys.
- In the common case, where corners differ along one axis, it is two.
- (c) stays available as a later quality tier (Q4).

### Q2. The granularity of the dual render

| Option | What renders twice | CPU (heavy voice, one blade's mode differs) | Memory | Exactness | Churn |
|---|---|---|---|---|---|
| (a) The whole voice | Every source and the filter slot, per corner setting of the whole voice | ≈ ×2 | A second voice state per voice (RT-1 has one only for mono's re-run) | A true mix of whole voices. But members follow separate states, so they decorrelate (level dips, comb). | Small text, large memory |
| **(b) Per structural group (§2)** | Only the group that differs: the kernel on the shared member state; a whole oscillator only when its swarm differs; a chain; or nothing extra for the gain kind | ≈ +80–88 % (the kernel dominates a heavy voice); ≈ +2 % for a sub difference; ≈ 0 for `env` or `panImage` | Per-setting blade, chain and filter state. Member state is duplicated only for kind S. | Exact at every corner. In between, a defined mix that keeps the members' phases shared, so the settings stay correlated. | One requirement block (SB) |
| (c) Per key | Each differing key separately | A product: mode × wave × mirror can reach 4³ | unbounded | Builds combinations no corner holds | Large |

**The nesting rule (part of (b)).** Groups nest: voice ⊃ oscillator ⊃ swarm ⊃ kernel. Settings are formed at the
**outermost** group that differs between corners. When an oscillator's swarm differs, the oscillator renders whole corner
settings, at most four, and its kernel does not blend separately inside each one. So no product ever forms.

**A finer cut the builder may take, not one the spec requires.** Rendering blade 1 and blade 2 as separate groups is the
same sound as the kernel tuple only when:
- the blades do not read each other (`upperHears`, collision, `fmSource` or `spreadSource` set to "blade 1's"); and
- their windows are the same in every setting (`mirror`'s twins add windows).

The spec's preamble already lets the builder compute fast where the math is unchanged. So the spec states the kernel
tuple, and an output-neutral split is the builder's optimisation.

**Recommendation: (b).** It is the cheapest way to run "both architectures" with the members shared. Sharing is what keeps
two blade settings correlated. The demo's Sync-to-FM blend dips 3.4 dB (at 0.65 of the path) even with a shared phase (§4); separate
member states would be worse.

### Q3. The blend region

| Option | What it does | CPU | Click risk | Churn | Draw |
|---|---|---|---|---|---|
| **(a) The whole pad's weights** | The setting weights are the bilinear corner weights, exactly as a Morphable value gets them | Two or more settings render everywhere off the corners and edges where the corners differ | None | IF-4 only | **none: Blend reads no draw** |
| (b) A band around each key's seeded point | One setting outside a band of width b around the key's seeded boundary; inside it, a crossfade across the band | Extra renders only inside the band, a small part of the pad | None inside; the pad is still in regions | IF-4 plus the band's law and width | **A new consumer:** the seeded boundary must exist under Blend |
| (c) The whole pad with a "sharpness" curve | Weights pass through a curve that flattens near the corners | Between (a) and (b) | None | A new parameter (a curve) | none |

(b) is a soft flip, not a blend. With a wide band it becomes (a); with a narrow one it becomes today's switch with a longer
crossfade. That is close to the rejected alternative "architecture flips at its seeded point", only softened. (c) invents
a control nobody asked for.

**Recommendation: (a).**

### Q4. The CPU budget (PF-4), and what happens past it

**What the record says.**
- PF-1's target is about **10.7 % of an M3 core** for the heaviest bank patch's swarm at 8 voices.
- The clean engine was estimated at **11.5 %** (7.8 % best, 15.3 % cautious; ROADMAP B494). There is no headroom.
- The blade kernel is about **80–88 % of a heavy voice** (B441 phase 1(B), op counts on the composed engine, not the clean
  room).
- CPU is "measured and reported, never a pass-fail gate" (PF-3).
- The engine never limits itself from measured CPU: VO-4 takes a voice cap "as an input, never a clock reading", and
  RC-10 "must not require a tier chosen inside the engine from measured CPU".

**Entailed** from those three figures, all hypotheses about the clean engine, for the heaviest patch at 8 voices **while the
puck sits where blade settings differ**:

| Case | Estimate, % of an M3 core |
|---|---|
| today, one setting | ≈ 11.5 |
| two kernel settings | ≈ 20.7–21.6 |
| two whole-voice settings | ≈ 23 |
| four kernel settings | ≈ 39–42 |

**At a corner, or when the corners agree, the cost is today's exactly**, by SB-5 (a setting at weight 0 costs nothing). So
PF-1 does not move, as long as its patch is measured at a corner or with its morph off.

**Options:**

| Option | Past the budget | Deterministic | Churn |
|---|---|---|---|
| **(a) Report, plus the author's lever** | Nothing changes in the engine. PF-4 reports the patch by name, with the count of structure settings rendered. The preset's designer sets the costly key to **Quantum** (B557), which restores today's flip and today's cost. | yes | PF-4, IF-9 (a counter) |
| (b) A fixed cap of two | The engine always uses Q1(c) | yes | Q1(c)'s slot rule |
| (c) A later quality tier (RC-10) | A tier may cap structure at two, or flip it. It is chosen by the device or the player, never from measured CPU. | yes | RC-10's "may vary" list gains one item |
| (d) The device's voice cap (VO-4) | Fewer voices, not fewer settings | yes | none (exists) |
| (e) Thin by measured CPU | — | **no**: it breaks DT-5 and RC-10 | rejected |

**Recommendation: (a) for 1.0, and (c) recorded as deferred.** PF-4's report gains the render count at named pad positions,
the centre among them. The lever that keeps a heavy patch cheap is already ruled: B557's Quantum.

### Q5. Where Quantum and the stepped hybrid place each structural key's flip

**Quantum.**

| Option | What it does | Draw | Churn |
|---|---|---|---|
| **(a) Per key, at its own boundary** | Each structural key flips at its own quantum boundary: its Gumbel-max pick at the morph's temperature and cohesion (ADR-104), with band 0.1 and PA-5's crossfade. This is what a Morphable key does under Quantum. | **unchanged**: today's seeded point already is this key's draw | none beyond IF-4's wording |
| (b) Per group, atomically | The group's lead key decides, and the whole tuple flips together (ADR-109 A1's atomic-group precedent: the scale) | **changes**: inner keys stop using their own draws, so existing quantum patches sound different (allowed before the stability line, ADR-186 (f), but it must be said) | the group list enters the morph's draw rules |
| (c) At the dominant corner | argmax of the weights, no draw | — | **rejected by ADR-317** ("architecture flips at the quantum draw's boundaries under every law" was rejected; this is the opposite and also not "quantum") |

(a) can assemble a blade no corner holds, such as Sync's mode with FM's wave. Under Quantum that patchwork is the law's
identity for every other parameter.

**Recommendation: (a).** Whether chimeras of structure are wanted is question H6.

**The stepped hybrid (B424).**

| Option | What it does |
|---|---|
| **(a) Blend in steps** | The position is held for 1/rate seconds (ADR-266 item 10: rate plus sync). The setting weights are Blend's, at the held position. Each step's edge is glided by PA-1 (and by morph glide where set), so the mix steps between weight levels. |
| (b) Quantum at each step | The structural key flips only at step instants, at its quantum boundary evaluated at the held position |
| (c) The morph editor lab's reading (b) | Each value snaps to steps of its own range. For structure, the weights would snap to multiples of 1/n. |

**Measured (a)** at 4 Hz on the default case: switch −23.2 dB, a step every 250 ms, against Blend's −46.1 dB and the
flip's +1.2 dB. Click +0.1 dB.

**Recommendation: (a).** It matches the morph editor lab's own words ("STEPPED is blend in steps", lab line 587), and it
keeps one law for Blend and stepped.

### Q6. How a parameter's own morph mode (B557) overrides it

| Option | Rule | Churn |
|---|---|---|
| **(a) Per key, composed into the group** | A group's settings are the distinct corner tuples **over its Follow keys only**. A Quantum key gives every setting its one picked value. An Exempt key gives every setting its one held value. If no key in the group still differs, the group renders one setting. | SB-8 (a flip inside a blending group) |
| (b) Per group | The menu on any key of a group sets the whole group | Contradicts B557's "per parameter" |
| (c) Per key, but one Quantum key makes its whole group flip | — | Surprising: one key's menu silently changes its neighbours |

**SB-8.** In (a), a Quantum key inside a blending group still flips at its boundary. When it does, every live setting of
the group crossfades by PA-5 at once. For those 5 ms, up to twice the group's settings render: eight for a group at four.

**Flip-only keys (kind F) under Blend** keep today's rule: one setting, flipping at the seeded point (their own draw),
band 0.1, by PA-5 or VO-14. Their menu can show Follow, but they resolve as Quantum, with the reason shown.

**This answers B555's open question (2).** "Is Quantum on a structural parameter disabled with a reason, or a no-op" is
neither, now. Quantum on a structural key is meaningful: it restores today's flip, and today's cost.

**Recommendation: (a).**

### Q7. Voices that start inside a blend region

| Option | What it does | Click risk | Consistency |
|---|---|---|---|
| **(a) Start every setting with weight above 0** | Each from its fresh-start state, sharing the voice's fresh member state (kind K), or each with its own (kind S). The note is the blend from its first sample. | none beyond the patch's own attack | A new note sounds like the held notes at the same position |
| (b) Start the dominant only, then wake the others | The others would enter at their full weight, not from 0 | **a step**: their weight is not 0 | — |
| (c) The dominant only, until the puck moves | — | none | **Inconsistent**: a chord played into a blend region sounds a flipped architecture while the held notes blend |

**Related rules**, which the spec must state:
- **Waking mid-note.** When the puck enters a region, a setting whose weight rises from 0 starts from its fresh-start
  state. It cannot click, because PA-1 raises its weight from 0.
- **For kind S** (a second swarm), a fresh start means the second instance's phases are unrelated to the first's. That is a
  level and comb risk (R2). The alternative is to copy the sounding instance's member state where the keys allow (the
  shared members of a `unison` change, as PA-6 keeps them). That is more law for a measured, not guaranteed, gain;
  question H9.
- **Per-note streams** (DT-4) are keyed by purpose, note counter and member, **never by which slot renders the setting**.
  So setting B rendered as the second of two is setting B rendered alone. This is what makes corner exactness (SB-10)
  hold.

**Recommendation: (a)**, with fresh-start waking.

### Q8. The hosted FX modules across the pad (a module's declared `switch`, ADR-309)

**What the contract already has.**
- A module's flip-class keys switch by the module's own declared `switch`: `crossfade`, `gate`, `seeded-jump` or `silent`
  (rack contract §12.7).
- Different module patches at horde's corners flip with band 0.1 where they lack parity (ADR-188 item 2, RS-6.2).
- At most two live instances exist, "only during a flip or a spill-over tail" (RS-6.3).
- The second instance counts against the module's budget (RS-8.4).
- Static nodes keep every module resident (RS-8.5).

| Option | What it does | CPU | Memory | Latency | Churn |
|---|---|---|---|---|---|
| (a) Modules keep flipping | Under every law, a module's structure flips at the seeded point by its declared `switch`. Blend never reaches inside a module. | none | none | none | none. But **FX structure does not follow the law**, against ADR-317's FX consequence |
| **(b) The slot blends two instances** | Where a slot's corner patches differ in a flip key, or in kind (parity fails), the slot runs two instances, each holding one corner setting's flip keys and both taking the blended Morphable values. Their outputs mix with equal gain at the setting weights. Capped at **two**, by Q1(c)'s law. The declared `switch` still governs every flip that remains (Quantum, flip-only, three or more settings). | ×2 of the module inside the region; RS-8.1's 2 % / 4 % becomes up to 4 % / 8 % there | A second resident instance per slot. RS-6.3 already allows one, so whether it is reserved today is a hypothesis to check. | Unchanged: both instances share the Device settings that fix latency (RS-7.1) | RS-6.3's "only during a flip" widens to "or in a blend region"; A-rows for the mix |
| (c) The module blends inside itself | A new optional declaration (e.g. `"switch": {"method": "blend"}`). The module accepts a held weight between two settings of a flip key and blends internally, as Scape's Era input crossfade does over 20 ms, but held open. | Often less than ×2 (shared state) | the module's | the module's | A contract addition and a brief to every module |

**Recommendation: (b) as horde's default, (c) as a later optional declaration.**
- (b) works for every module without its cooperation, inside RS-6.3's existing two-instance cap.
- (c) is filed with the rack work (§12 of the contract), after 1.0, through the mailboxes.
- The FX slot is capped at two, unlike the voice's four, because a module instance can be megabytes (a Sluice delay is
  12.9 MiB at 96 kHz ×4, RS-8.5). That difference is question H10.
- **FX device (ADR-254):** the same rule, since the FX device hosts every module through the same slot and manifest.

---

## 4. What the demonstration measured

The demo is [`docs/design/morph-blend-demo.html`](../design/morph-blend-demo.html).

**The setup.**
- One member at 220.5 Hz, an exact 200-sample period at 44.1 kHz, so a steady render repeats and both readings have a
  true floor.
- One blade (Sync, FM, Fold or Ring, with polyBLEP edges) on the shared member phase, then a state-variable filter.
- **The sweep:** 0.5 s held, 3 s moving, 0.5 s held.

**The readings** (reported, never judged; neither is QS-4's detector):
- **click:** the worst one-cycle window's second-difference RMS, against its local median (B265's shape).
- **switch:** the worst cycle-to-cycle change, against the render's RMS.
- **level:** the worst dip and bump against the straight line between the held ends.
- **renders:** structure settings per voice-sample while moving (blade + filter; today's is 2).

Deep links open each case: `#law=…&kind=two|four&path=AB|AD|mid`.

**Two architectures** (A = C: Sync + LP; B = D: FM + BP). Path A→B; the same holds at any y.

| law | click dB @path | switch dB @path | level dip / bump dB | renders max / mean |
|---|---|---|---|---|
| flip, no crossfade | +8.6 @0.71 | +1.5 @0.71 | −2.0 / +4.9 | 2 / 2.00 |
| flip at seeded point (today) | +0.2 @0.74 | +1.2 @0.71 | −3.9 / +4.9 | 3 / 2.00 |
| Blend, up to four | +0.0 | −46.1 | −3.4 / +0.0 | 4 / 4.00 |
| Blend, at most two | +0.0 | −46.1 | −3.4 / +0.0 | 4 / 4.00 |
| Quantum | +0.1 @0.74 | +1.5 @0.71 | −2.9 / +5.0 | 3 / 2.00 |
| Stepped, 4 Hz | +0.1 | −23.2 | −3.4 / +0.5 | 4 / 4.00 |

**Four architectures** (A Sync + LP, B FM + BP, C Fold + HP, D Ring + LP):

| law | A→D (the diagonal): switch dB @path · level dip dB · renders mean | y = 0.4 (near the centre): switch dB · level dip / bump dB · renders max / mean |
|---|---|---|
| flip at seeded point (today) | −5.0 @0.62 · −0.8 · 2.00 | +1.1 · −3.5 / +5.2 · 3 / 2.00 |
| Blend, up to four | −42.1 · **−6.7** · 7.00 | −53.4 · −1.0 / +0.0 · 7 / 7.00 |
| Blend, at most two | **−15.8 @0.50** · −1.0 · 2.03 | −31.4 · 0.0 / **+5.9** · 6 / 4.07 |
| Quantum | −6.1 @0.62 · −1.5 · 2.00 | +2.3 · −3.6 / +5.3 · 3 / 2.00 |
| Stepped, 4 Hz | −19.7 · −6.6 · 7.00 | −30.2 · −1.0 / +0.1 · 7 / 7.00 |

**What it shows.**
1. **Today's flip makes no click but is a switch.** PA-5 holds the click reading at +0.2 dB, against the bare jump's
   +8.6 dB. But the switch reading is +1.2 dB: the whole sound changes within about one cycle. That is the Random lab's
   finding 6 ("the pad flips most of the patch at its centre lines"), measured.
2. **Blend removes the switch.** The switch reading is −46.1 dB, and the click is at the floor.
3. **Blend's cost is the render count.** It is 4 (two architectures) or 7 (four blades, three filters), against today's 2,
   wherever the corners differ.
4. **Equal gain dips where the settings decorrelate.** −3.4 dB (at 0.65 of the A→B path) for Sync + LP against FM + BP. **−6.7 dB at the
   centre** for four different blades. RS-3.6's law is right for correlated paths; these are partly correlated. This is a
   listening question (H11), not a defect: equal power would bump correlated settings by up to +3 dB instead.
5. **"At most two" is a switch again** on lines where the second and third settings tie (−15.8 dB at the centre of the
   diagonal), and it bumps the level near the centre (+5.9 dB).
6. **Stepped steps.** −23.2 dB per 4 Hz step, between Blend and the flip.

**The self-checks**, each with a must-fail control (6 of 6 pass on the page):
- C1: seeded and repeatable; the next seed moves the seeded point.
- C2: at every corner, Blend is bit-identical to that corner alone; a 10⁻³ weight leak differs.
- C3: the weights sum to 1 before and after PA-1's smoother (1.5·10⁻⁶ after); equal-power weights do not.
- C4: the counts, with "at most two"'s third-setting overlap reported (148 ms).
- C5: the −6 dBFS guard holds and reports.
- C6: the readings can tell the bare jump from Blend (≥ 6 dB click), and today's flip from Blend (≥ 20 dB switch).

---

## 5. Draft spec text (PROPOSED)

Each block replaces or adds to SPEC-ENGINE as marked. Wording follows the spec's own style.

**Glossary, line 68 (PROPOSED).** "**Morphable / Structural / Device** | The three parameter classes: Morphable values
blend between morph corners; Structural values follow the morph's law, blending by rendering each corner's setting (IF-4,
SB) or flipping where the law or the key's own morph mode says so; Device values stay outside the morph."

**§5.1, the Class bullet, line 194 (PROPOSED).** "`S` Structural (follows the morph's law: rendered per corner setting
under Blend, flipped under Quantum; IF-4, SB)".

**IF-4 (PROPOSED, replaces its first two sentences):**

> | IF-4 | **Morph and state** | The manifest's class column is the morph's input. Morphable values blend. Device values
> stay out. **Structural values follow the morph's law** (ADR-317): the morph resolves each structural group (SB-1) to a
> **setting set**, one to four settings of the group, each with a weight in [0, 1], the weights summing to 1. Under Blend
> and the stepped hybrid, a group's settings are the distinct settings its corners hold over the keys whose own morph mode
> is Follow, and each weight is the bilinear corner weights summed over the corners holding that setting (at the stepped
> hybrid's held position). A key whose own mode is Quantum, every key under the Quantum law, and every flip-only key
> (SB-2) under every law hold one value, which flips at the key's quantum boundary with a hysteresis band of 0.1 and
> changes on sounding voices by PA-5 (SB-8 inside a blend). An Exempt key holds one value. The engine renders a setting set
> by SB-3 to SB-10. The engine reports the heard setting set of every structural group per oscillator (each setting's
> stepped values and its weight), so the GUI shows what is heard where the morph and the stored patch differ. **State** …
> *(unchanged)* |

**A new block, §6.11 Structure blend (SB) (PROPOSED):**

| id | requirement |
|---|---|
| SB-1 | **Groups.** The structural keys form these groups. (1) **By gain:** each source's `env`, `panImage`, `panInvert`. A setting set acts on the stage's gains (Σ wₖ·gainₖ), with no second render: the stage is linear, so this equals the mix of the renders. (2) **Each oscillator's kernel:** `baseWave`, `stacking`, `spreadLaw`, `quantize` and every row of §5.4 for both blades. Each setting renders the member kernel (the base wave, both blades and their edge corrections) from the oscillator's one member state. (3) **Each oscillator's swarm:** `unison`, `distribution`, `seed`, `detuneLaw`, `detuneRange`, `pivot`, `octave`, `semitone`. Each setting renders a whole instance of the oscillator with its own member state. When this group holds more than one setting, the kernel's keys join it, and the oscillator renders whole corner settings, never a product. (4) **The sub's chain:** its structural rows but `voiceMode`, `priority` and `env`. (5) **The noise's chain:** its structural rows but `env` and the reserved rows. (6) **The filter slot:** the filter's own structural keys (RC-1). |
| SB-2 | **Flip-only keys.** `voice.mode`, `oneShot`, `retrigger`, `sub.voiceMode`, `sub.priority` and every reserved row never blend: they act on the voice law or at a voice's start, where one note cannot hold two values. Under every law each holds one value, flipping at its seeded point (hysteresis 0.1) by PA-5, or by VO-14 for `voice.mode`. |
| SB-3 | **The mix.** A group's output is Σ wₖ·yₖ over its settings, with equal gain (RS-3.6). Each weight is a continuous value glided by PA-1. The smoother is linear, so the glided weights still sum to 1. |
| SB-4 | **One member state.** Every setting of a kernel group renders from the oscillator's one member state (as PA-5). Where the member state reads the output (feedback and cross-mod, C.14), it reads the mixed output. |
| SB-5 | **Cost.** A setting whose heard weight is exactly 0 (PA-1's snap) renders nothing and costs nothing (IF-9's counters). Wherever a group's corners hold one setting, and at every corner, exactly one setting renders. |
| SB-6 | **Waking.** A setting whose weight rises from 0 while a note sounds starts from its fresh-start state. Its per-note streams (DT-4) are keyed by purpose, note counter and member, never by the slot that renders it. |
| SB-7 | **A voice that starts in a blend.** A voice's fresh start starts every setting whose weight is above 0, each from its fresh-start state, so the note is the blend from its first sample. |
| SB-8 | **A flip inside a blend.** When a one-value key (a Quantum key, SB-2) flips inside a group that holds several settings, every live setting of the group crossfades by PA-5's law at once. During the crossfade, at most twice the group's settings render. A second flip waits, as PA-5 says. |
| SB-9 | **Limits.** At most four settings per group, and at most eight during SB-8's crossfade. The engine is sized for it (RT-1). |
| SB-10 | **Corner exactness.** At each corner, and wherever every group holds one setting, the output is bit-identical to the same patch with the morph off and those settings stored. |

**PA-5 (PROPOSED, two edits).**
- "…the same holds when the morph flips one" becomes "…the same holds when the morph flips one (IF-4: a key under the
  Quantum law or mode, or a flip-only key, SB-2)".
- "…so at most two settings ever sound at once" becomes "…so at most two settings of a key ever sound at once outside a
  structure blend; inside one, SB-8 and SB-9 hold".

**PF-4 (PROPOSED).** "…and across a morph between corners with different stepped keys, **reported with the count of
structure settings rendered per voice (IF-9) at each measured pad position: every corner, every edge's midpoint and the
centre**; no patch falls…"

**RT-1 (PROPOSED).** "room for PA-5's second setting on every oscillator" becomes "room for up to four settings of every
structural group, and SB-8's second setting of each, on every oscillator, the sub, the noise and the filter slot; for the
swarm group, up to four whole oscillator instances per oscillator per voice, and eight during SB-8".

**IF-9 (PROPOSED).** It adds a counter: "structure settings rendered (setting-samples), per group kind".

**IF-10 (PROPOSED).** The observation view adds "per voice and group, the heard setting set and each setting's weight".

**RC-10 (PROPOSED, deferred).** "What a later tier may vary" gains "the structure blend's limit (four, two, or one
setting per group)".

**QS-4 (PROPOSED).** Two additions:
- A Blend sweep across a structural difference is judged by its own law test (SB-3's mix law, test T3 below), under the
  probe rule's minimal patch. Its click reading is **reported**, beside the reading of PA-5's flip for the same corners.
- A sweep under Blend on the pure-sine probe plus the one blade whose mode differs makes **no detected click**: judged.

---

## 6. Test items for the clean room's test author (engine-free)

The engine's suite tests the engine half: given setting sets and weights, render. The morph's resolver (weights from a pad
position, the quantum draw, the stepped hold) belongs to the morph's own suite.

| # | Item | Pass | Must-fail control |
|---|---|---|---|
| T1 | Corner exactness (SB-10) | Each corner's single setting renders bit-identically to the patch with the morph off and those settings stored | A planted weight leak of 10⁻³ to another setting differs |
| T2 | Agreement costs nothing (SB-5) | Where a group's corners agree, IF-9's counters equal the no-morph patch's; a weight-0 setting renders zero setting-samples | A build that renders every setting reads more |
| T3 | The mix law (SB-3) | At a static set with settled weights, on a patch with no feedback, no cross-mod and an empty filter slot: output = Σ wₖ·(the render of setting k alone, with the same values) within 10⁻⁶ relative RMS, for two and for four settings | An equal-power mix fails |
| T4 | Weights are glided and sum to 1 | Observed weights sum to 1 within 10⁻⁵ at every sample of a moving set; a weight jump reaches its target along PA-1's curve | Weights applied as steps fail the PA-1 curve |
| T5 | No click across a blend (QS-4) | A sweep of the setting weights from one setting to another over 1 s, on the probe rule's patch (a sine probe plus the one blade whose mode differs), makes no detected click | A planted bare switch at the midpoint is detected |
| T6 | Waking (SB-6) | A setting woken mid-note starts from its fresh-start state; renders with the wake at different block splits are bit-identical (DT-2) | A wake that resumes stale state differs from the fresh-start reference |
| T7 | A voice that starts in a blend (SB-7) | With no feedback, a note started at a static set equals Σ wₖ·(the note started in each setting alone) from its first sample | A voice starting with the dominant setting only fails |
| T8 | Flip-only keys (SB-2) | `voice.mode`, `oneShot`, `retrigger`, `sub.voiceMode`, `sub.priority` never hold more than one setting (observation view) | A set of two for `voice.mode` is refused and counted (PA-7) |
| T9 | A flip inside a blend (SB-8) | A one-value key flipping inside a two-setting group crossfades both settings by PA-5's law (the law test of QS-4's probe rule); at most four settings render during it | A flip of one setting only is detected by the law test |
| T10 | Limits (SB-9) | Never more than four settings per group, nor eight during SB-8 (counters) | A five-setting set is refused and counted |
| T11 | Groups (SB-1) | A set on a gain-kind key renders one setting (counters) and equals the two-render mix (T3's law) | — |
| T12 | Determinism | DT-1 and DT-2 with a moving set, across block sizes 1 to 4,096 | — |

---

## 7. Risks

- **R1. CPU.**
  - A blade difference roughly doubles a heavy voice inside the region (§2, Q4), and PF-1 has no headroom.
  - A morph patch that blends a heavy blade at 8 voices could overrun a host's block where today's flip would not.
  - The mitigations are reporting and B557's Quantum. Nothing in the engine adapts.
- **R2. The level of the fade.**
  - Equal gain dips where the settings decorrelate: −3.4 dB measured on Sync/FM (at 0.65 of the path), −6.7 dB measured with four
    blades at the centre.
  - It will be worse for kind S (two swarms with independent phases).
  - The human's ears decide whether the dip matters, and whether an equal-power law should apply to kind S.
- **R3. "Both running" may sound like layering, not morphing.** An `octave` or `semitone` difference under Blend becomes an
  octave layer across the pad. A `unison` difference becomes two stacks. The ruling asks for this, but it may surprise.
  B557's Quantum is the per-key escape (H3).
- **R4. Memory.** The swarm group at four settings, plus SB-8's eight, multiplies the member state per oscillator per
  voice (64 voices × 16 members). The bytes are not known here; PF-7 reports them.
- **R5. Churn mid-build.**
  - The clean room is building the bank stage on the current spec (ADR-317: "The bank stage continues on the current spec
    until that round lands").
  - PA-5's machinery is built for two settings. SB widens it to four and adds groups, so built stages are revisited.
  - Both clean-room repos must get the round the same day (B585 step 5; the clean-room spec sync rule).
- **R6. The resolver has no spec.** IF-4 now carries more of the morph's law: setting sets, pooled weights, the stepped
  hold. The morph's own rules (temperature, cohesion, hysteresis, the draw) live only in ADRs and labs. If the engine spec
  states them, it grows past its scope. If nobody states them, the two halves can disagree. A short morph-resolution
  appendix, or a morph spec, is the cure. That is a scoping question for the lead.
- **R7. The demo is a stand-in.** One member, no swarm, no coupling, no edge correction, a stand-in filter. Its readings
  are not QS-4's. Its figures show the shape of the trade, not horde's numbers.
- **R8. The detector shares my assumptions.** I wrote both readings. The must-fire control (the bare jump, +8.6 dB against
  +0.0) and the must-read-floor case (a steady render reads exactly 0 dB click) are why they can be trusted this far, and
  no further.
- **R9. Two at the FX slot, four in the voice.** Different caps in one instrument may confuse. Q1(c)'s tie lines then
  apply to FX blends (H10).

---

## 8. Questions for the human (each with a recommendation)

| # | Question | Recommendation |
|---|---|---|
| H1 | Q1: up to four settings at once, or at most two? | **Up to four.** No switch anywhere; two in the common case. |
| H2 | Q2: render per structural group (§2), with the kernel on the shared member state? | **Yes.** |
| H3 | Should pitch and count keys (`octave`, `semitone`, `unison`, `seed`, the detune laws) blend by running two swarms, or stay flip-only? | **Blend, as ruled**, with B557's Quantum as the per-key escape. Listen first: it is the layering case (R3). |
| H4 | Q3: the whole pad's weights, or a band around each seeded point? | **The whole pad.** |
| H5 | Q4: past the budget, report it and rely on B557's Quantum in 1.0, with a quality tier later? | **Yes.** |
| H6 | Q5: under Quantum, does each structural key flip at its own boundary (chimeras allowed), or does each group flip as one? | **Each key at its own boundary.** It is quantum's patchwork, and no existing patch moves. |
| H7 | Q5: under stepped, does the mix step (Blend in steps), or does structure flip at step instants? | **The mix steps.** |
| H8 | Q6: per-key modes composed into the group, with Quantum on a structural key restoring today's flip? This also answers B555's question (2). | **Yes.** |
| H9 | Q7: does a second swarm woken mid-note start fresh, or copy the sounding swarm's members where it can? | **Fresh** for 1.0 (one rule, deterministic). Revisit if the dip (R2) is heard. |
| H10 | Q8: FX slots blend two module instances (capped at two), the module's declared `switch` handling every remaining flip, and an optional in-module blend filed later? | **Yes.** |
| H11 | The fade's law: keep equal gain everywhere (as ruled), knowing it dips 3 to 7 dB where settings decorrelate? | **Keep equal gain, listen on the demo first.** If the dip is heard, consider equal power for kind S only. |
| H12 | The flip-only list (SB-2): `voice.mode`, `oneShot`, `retrigger`, `sub.voiceMode`, `sub.priority`, the reserved rows. Right? | **Yes.** |

---

## 9. Evidence consulted

- **The spec:** `specs/SPEC-ENGINE.md`:
  - the glossary (line 68) and §5.1 (line 194);
  - §5.2 to §5.6 (every class-`S` row);
  - VO-4, VO-11, VO-14 and the start table (§6.4);
  - PA-1, PA-5, PA-6 (lines 549 to 554);
  - DT-4, DT-5; RT-1 (line 621);
  - IF-4 (line 671), IF-9, IF-10;
  - RC-1, RC-10;
  - QS-4 (line 807); PF-1 to PF-7 (lines 822 to 828).
- **The decisions** (`lead-records-363`): ADR-104, ADR-109 (and A1), ADR-185, ADR-186, ADR-188, ADR-266 item 10, ADR-291,
  ADR-300, ADR-309, ADR-317.
- **The ROADMAP rows:** B424, B441, B494, B555, B557, B578, B585.
- **The rack contract:** `docs/proposals/rack-slot-contract.md` §4.1, §6.1, §8 and §12 (12.1, 12.2, 12.7).
- **The labs:**
  - `docs/design/random-lab.html` and `random-lab-report.md` (the stand-in and finding 6);
  - `docs/design/morph-editor-lab.html` (the ported quantum resolver, STEPPED's readings).
- **Not read:** the clean-room repositories (by the brief).
