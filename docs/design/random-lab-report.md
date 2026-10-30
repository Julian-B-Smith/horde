# The Random page: the workshop lab's report (B578, ADR-312)

*horde lab agent, 2026-10-29. Lab: [`random-lab.html`](random-lab.html). For the human to rule what a basic Random page in 1.0 contains.*

**What this report can and cannot claim.** Every number below was **measured** by the lab's own functions, run headless in Node. The renderer is the one the page plays (one second of the "chord stabs" phrase at 44.1 kHz, monitor 0.7), over seeds 1–60. Nothing here is **guaranteed**: the self-checks C1–C28 are what the lab guarantees, and each of them has a must-fail control. Nobody has listened yet. "Loudness change" means RMS only, so a change of timbre at equal level reads as 0 dB. The synthesis is a stand-in, not horde's engine. Your ears on the lab are the gate this report cannot pass for you.

## 1. How to use the lab

Press **PLAY** (no sound before that click). Peaks are held at −6 dBFS, and the guard readout says when it holds.

- **LEVELS** (left): each row has **ARCH** (randomize architecture) and **PARAMS** (randomize parameters). Both act on the **selected corner**, which is the quadrant the puck is in.
  - **S** (solo) and **M** (mute) sit on the four source rows: OSC 1, OSC 2, SUB and NOISE. They let you hear what one source contributes while you randomize it. Below the levels, the source meters show each source's RMS. See §4a.
- **XY PERFORMANCE GRID** (middle): four corners A–D, each bound to a snapshot. Buttons: **randomize patch**, **randomize all four**, **save snapshot**, **revert**. There is a lock and a revert per corner. In the snapshot list, A–D buttons affix a snapshot to a corner, and ★ marks the patch home.
- **BOUNDS** (right):
  - the **HOME ↔ CHAOS** knob;
  - **AMOUNT**;
  - where "home" is: the corner's snapshot, the patch ★, or current (walk);
  - per-group chaos and amount scales, and group locks;
  - the selected parameter's **distribution**, drawn with 400 seeded preview draws, with low and high bounds;
  - per-parameter locks, and dice chips on structural choices: a struck value is never drawn.
- **MOD-MAPPING RULESETS**: five built-in rulesets. You can edit each rule's on/off, weight and amount range, add your own source → destination rules, and save and load rulesets in this browser (localStorage).
- **Determinism**: the seed is shown and settable. Every stored change is an action in the log. **Replay from seed** rebuilds the session and compares fingerprints.

## 2. The stand-in patch

| Part | Structure (ARCH) | Numbers (PARAMS) |
|---|---|---|
| OSC 1, OSC 2 (Scalpel-like: a unison stack with a blade window that cuts each cycle and replaces its span) | on; wave (saw, square, tri, sine); unison 1/3/5/7; octave; semitones (−12 … +12, weighted to 0, +7, +12); blade (off, gap, mirror, fold) | fine; detune; stereo spread; pulse width (square only); blade position and width (blade on only); level |
| SUB | on; sine or square; −1 or −2 octaves | level |
| NOISE | on; white or pink | level |
| FILTERS | A type; B on and its type; A and B serial or parallel; each source into A, B or A+B | cutoffs (log), resonance, drive, keytrack |
| ENVELOPES | none | ENV 1 (amp), ENV 2, ENV 3: A, D, S, R (log times) |
| LFOS | two shapes (incl. S&H) | two rates (log) |
| MOD MATRIX | 8 slots: source, destination, rule (drawn by the ruleset) | each slot's amount (inside its rule's range) |
| FX ROUTING | order (24 permutations); topology serial, parallel or 2 + 2; each module on | each module's mix |
| SHRIEK (pass-through stub), CHORUS, DELAY, REVERB | curve; chorus or flanger; stereo or ping-pong and a tempo division; room or hall | drive and tone; rate and depth; feedback and damping; size and damping |
| OUTPUT | — | gain, locked: never drawn |

## 3. The laws the lab tries

The full text is in the lab's header comment.

- **Counter-based draws.** Every random number comes from mulberry32, seeded by a hash of (seed, click, corner, key, draw index). A key's draw never depends on its position or on how many keys exist. This is ADR-186 §5(g) / B308 H3, and C2 shows it against a positional control.
- **The bell.** A truncated normal around home, in the parameter's own taper, cut to its bounds, drawn from one uniform by inverse CDF. Its width is σ(c) = 4·c·50^(c−1) (see finding 3).
- **Amount.** Every key draws a threshold on every click and changes only where the threshold is below AMOUNT. At the same seed and click, a higher AMOUNT changes a superset of the keys, with the same values (C3). This copies the behaviour of Sluice's randomiser, not its code.
- **Bounds are for the next draw only** (ruled 2026-10-29; see §3a). CHAOS, AMOUNT, the per-group scales, the per-key bounds, the dice, the locks and the home choice shape the next click. They never change the current sound or any corner's values (C14).
- **Structure.**
  - An unordered choice keeps home with probability 1 − c; otherwise it draws by taste weight over the lit values.
  - An ordered choice (unison, octave, delay time) is a bell in index space.
- **Morph.** Numbers blend in their taper. Structure flips at the dominant corner. A slot's amount blends only between corners that hold the same route.
- **Dependencies.** A parameter that can't be heard is not drawn, and a rule only routes to a live destination. This is B316's dependency tree in miniature.

## 3a. The clarified rule: bounds set the next draw, never the current sound (2026-10-29)

The human, playing the first round: "I'm a little confused by the chaos slider. At the moment it seems to dynamically change the actual character of the sounds; I assumed it just set the bounds on patch or parameter randomization."

The first round re-applied the last click whenever a bound moved, as Shriek's Randomness knob does. That made CHAOS and AMOUNT behave like sound controls. **The rule now** (relayed by the lead, 2026-10-29):
- CHAOS, AMOUNT, the per-group scales and the per-key bounds are **bounds for the next draw only**. Moving them never changes the current sound or the current corner's values.
- The lab applies the same rule to the dice, the locks and the home choice. They are bounds of the same kind, and a lock that silently reverted a value would be the same surprise.
- The bell-curve view still updates live, to show the bounds the next draw will use.
- Live re-application of the last draw is **removed**, not offered as a toggle. It is listed as a post-1.0 idea (§5).
- **C14** moves CHAOS 0 → 1 → 0, then AMOUNT, a group scale, a key bound, the home choice, a lock and a die. The fingerprint is unchanged after each of the 10 moves. Its control is a stand-in that re-applies the last draw on a bound change; that moves the fingerprint, so it fails as it must.
- A headless Chrome run confirms it: dragging the knob and the sliders left both the fingerprint and the heard patch unchanged, the distribution view showed the new bounds, and the next click drew within them.

**Consequence for the chaos curve (Q9).** CHAOS can no longer be judged by sweeping it over one sound. It is tuned by ear **at the next draw only**: set CHAOS, click, listen, and repeat.

## 4. Each level: what it does and what playing it showed

One click from home, at the defaults (chaos 0.35, amount 0.6), with "ARCH must change" off:

| Level | ARCH: keys changed (mean) / no change | PARAMS: keys / no change | Guard engaged | Median loudness change |
|---|---|---|---|---|
| Global patch | 21.1 / 0% | 27.0 / 0% | 3% · 5% | 1.8 · 1.3 dB |
| Voice section | 3.1 / 3% | 17.9 / 0% | 2% · 0% | 1.6 · 1.4 dB |
| OSC 1 | 0.5 / **62%** | 2.2 / 2% | 0% | 0.0 · 0.1 dB |
| OSC 2 | 0.9 / **42%** | 4.1 / 0% | 0% | 0.3 · 1.3 dB |
| SUB | 0.3 / **72%** | 0.6 / 45% | 0% | 0 dB |
| NOISE (off at home) | 0.2 / **83%** | 0.0 / **100%** | 0% | 0 dB |
| Filters | 0.8 / 45% | 2.4 / 3% | 5% · 0% | 0.0 · 0.1 dB |
| Envelopes | — (no structure) | 7.2 / 0% | 0% | 0.3 dB |
| LFOs | 0.4 / 60% | 1.3 / 15% | 0% | 0 dB |
| Mod mappings (ruleset "Common") | 17.3 / 0% | 1.9 / 5% | 0% | 1.8 · 0.1 dB |
| FX routing | 0.6 / 53% | 2.4 / 2% | 5% · 2% | 0.0 · 0.5 dB |
| Shriek (stub) | 0.1 / **93%** | 1.2 / 17% | 0% | 0 dB |
| Chorus | 0.1 / **92%** | 1.1 / 25% | 0% | 0.1 dB |
| Delay | 0.2 / **83%** | 1.2 / 18% | 0% | 0 dB |
| Reverb | 0.1 / **95%** | 1.4 / 13% | 0% | 0.1 dB |

Randomize patch: a new structure and one sound on all four corners.

| Chaos / amount | Guard engaged | Inaudible (RMS < −50 dBFS) | Median loudness change |
|---|---|---|---|
| 0.35 / 0.6 | 3% | 0% | 2.0 dB |
| 0.6 / 0.8 | 15% | 2% | 6.6 dB |
| 1 / 1 | **27%** | **7%** | 8.1 dB |

### Findings

1. **At moderate chaos, a small module's ARCH click usually changes nothing.** The rates are 42–95% (OSC 2 to Reverb). Each structural choice keeps home with probability 1 − chaos, and must also pass the amount threshold. With two or three choices, both hurdles usually win. A button that often does nothing reads as broken.
   - The lab carries a **PROPOSAL, "ARCH must change"**: if no structural choice moved, the unlocked choice with the lowest threshold takes another lit value, by taste.
   - With it on, the no-change rate is 0 of 60 on every module, and about one key changes (C11).
   - Global and voice ARCH rarely needed it (0% and 3%).
2. **PARAMS on a switched-off module does nothing, correctly.** NOISE is off at home, so its level is inactive and is never drawn (100% no change). The lab says so in the status line. The page must show it too, or a dependency-aware randomizer looks broken.
3. **The first chaos law left the lower third of the knob inaudible.** With σ = 4·c·200^(c−1), the mean move at chaos 0.1 was 0.3% of a parameter's range, and 1.5% at chaos 0.25. The lab now uses base 50:

   | Chaos | σ | Mean move (fraction of range) |
   |---|---|---|
   | 0.1 | 0.012 | 0.009 |
   | 0.25 | 0.053 | 0.041 |
   | 0.5 | 0.28 | 0.18 |
   | 0.75 | 1.13 | 0.28 |
   | 1 | 4.0 | 0.29 (flat) |

   - Above about 0.75 the bell is already near-flat, so the top quarter of the knob does little more.
   - The curve is a tuning question for your ears, not settled here.
4. **High-chaos random patches need loudness handling and musical bounds.** At chaos 1, 27% of random patches engaged the −6 dBFS guard and 7% were near-silent. Looking at the silent ones, the causes were:
   - a high-pass filter at 2.8–8.5 kHz on a patch whose only sources were the sub and one oscillator;
   - an amp attack of 0.65–1 s on notes shorter than that;
   - oscillator levels drawn near zero.

   A shipping random patch needs both of these, and the lab has neither:
   - **level matching** after a draw (Shriek keeps its corners level-matched with measured auto-gain);
   - **bounds fitted to musical yield**: B316's P4, the bounded distributions fitted from the patch-space gauntlet. The lab's taste weights are hand-set.
5. **The ruleset draws obey their rules, and the rules compete for sources.** C6 checked 327 routes over 60 draws: 0 broke a rule. Over 200 draws per built-in ruleset, routes per draw matched the wanted count closely (2.86 of 2.88 for Common, 3.85 of 3.88 for Hyperpop lead). Two findings:
   - **Source contention.** horde-scale patches have only two LFOs and two free envelopes. "Wobble and motion" has three LFO-shaping rules, and 113 of its tries found their LFO already shaped by another rule. That is why it drew 2.61 routes of 2.88 wanted. A rule that shapes its source (a pitch envelope is only a pitch envelope when the envelope is fast) must **claim** that source, so a second rule cannot reshape it. The lab does this. The number of modulators is therefore a real limit on how many shaped rules a ruleset can carry.
   - **Dependencies matter.** On an all-saw patch with the blades off, the PWM rule found no live destination 355 times in 200 draws and drew nothing, correctly. A dependency-blind draw would have routed it to inaudible destinations. The draw must therefore run after the structure is decided, which the lab does on GLOBAL ARCH.
6. **Corners with different structure turn the pad into a switch.** The test randomized GLOBAL ARCH on each corner in turn, 60 seeds.
   - With ARCH writing **all four** corners (Sluice's and Shriek's one-patch rule), 0 structural keys differed between corners.
   - With **selected corner** only (horde's per-corner patches, ADR-188), a mean of 37.3 differed. The pad then flips most of the patch at its centre lines.
   - The lab offers both. My recommendation is in Q3.
7. **"Walk" drifts.** After 10 voice PARAMS clicks, the mean distance from home was 0.107 of the range with home = current (walk), and 0.052 with home = the corner's snapshot. Anchoring is what makes the bell meaningful; walk is an explorer's mode.
8. **Live re-application of the last click worked, and was removed by ruling** (§3a). In the first round, CHAOS, AMOUNT, bounds, dice, locks and the home choice all re-applied the last click's draws, because each key's uniforms were stored with the click. The human found that confusing: CHAOS read as a sound control. Bounds now act on the next draw only (C14). The counter-based draw still makes a later version of the idea possible (post-1.0).

## 4a. Solo and mute on the voice sources (added 2026-10-29, at the human's request)

The human asked: "Can we also add 'solo' controls to the voice section so you can hear what each is contributing when you randomize individually?" OSC 1, OSC 2, SUB and NOISE each carry **S** and **M**. They follow SPEC-ENGINE IF-6, as the engine's own `mute` and `solo` rows do (§5.3, §5.5–5.6):
- a muted source is silent;
- while any source is soloed, every source that is not soloed is silent;
- a soloed source that is also muted is silent;
- the gate sits on the source, before the filters (IF-6's meter tap: after level, pan and gate);
- a gated source keeps rendering, so un-muting is phase-continuous.

**Display.** A source silenced by another source's solo shows its M dimmed and hatched, and its row dims. This is distinct from its own mute, which is lit. That is the legacy mixer's convention.

**The ramp follows IF-6, not the legacy mixer.** The gate moves by **5 ms with a raised-cosine shape**, as SPEC-ENGINE's fade list (ADR-239) and its ramp table require of IF-6's gate. The brief allowed the legacy mixer's 8 ms or PA-1's smoother, but asked for IF-6 exactly, and IF-6 names 5 ms.

**Monitoring state, not patch content.** Solo and mute:
- are never randomized;
- are never stored in a corner or snapshot;
- are never logged;
- are listed under OUTPUT as always locked.

The engine gives `mute` and `solo` class `D` (Device: outside the morph), and the rack-slot contract never draws a Device-class key (RS-4.5), so all three agree. The headless Chrome run confirmed it: soloing and muting added nothing to the action log, a randomize left them as they were, and no corner holds a solo or mute key.

**Checks.**
- **C12.** With OSC 1 soloed on a patch whose four sources are all on, the per-source meter taps read OSC 1 at −5.1 dBFS and the other three at −∞. The output equals a render of OSC 1 alone, with a max |Δ| of 0. The must-fail control applies solo *after the mix*, as one gain on the whole bus. It reads −5.1, −5.5, −10.0 and −18.5 dBFS and differs by 0.33, so it fails as it must.
- **C13.** A muted OSC 1 meters zero, and its phases match an unmuted render's exactly: it keeps running. The gate's largest step is 0.00711 ≤ π/(2·221), at 221 samples = 5 ms, and it settles at 0. Solo plus mute is silent. The control, an instant gate, steps by 1.00.

**Recommendation: solo and mute on each voice source belong in the basic 1.0 Random page.** The engine already provides them (IF-6), so the page only shows them. They make per-module randomizing audible.

## 4b. Round 3 (ADR-320, 2026-10-29)

The human played round 2 and ruled the five items below; ADR-320 quotes their words.

1. **"'arch writes' should default to 'selected corner'".** Built: the default is now the selected corner, and "all four corners" stays as an option. This reverses the lab's own round-1 recommendation (Q3, now answered).

2. **Rule collections.** Built, defined once in the lab's data and read by every rule, the custom-rule builder, the draw and C6. This follows the single-source principle of ADR-319 item 4: no rule keeps its own list.
   - A rule names **one** source token and **one** destination token. Each token is a specific id or a collection.
   - **Source collections:** any envelope; any LFO; any modulator; any performance control (velocity, mod wheel, key); and **the intent bus** (SPEC-INTENT-BUS, ADR-192's four fixed intents Tone, Space, Time, Motion). The intents are bipolar performance sources, set by four sliders in the audio bar.
   - **Target collections:** any oscillator pitch; any filter cutoff; any oscillator's timbre; any pulse or blade width; any unison detune; level or pan; the FX sends; any FX amount; any destination. A collection may weight its members (e.g. filter A over filter B).
   - **The rules were rewritten onto collections.** Pitch envelope is any envelope → any oscillator pitch; filter LFO is any LFO → any filter cutoff. The old two-target velocity rule split into "Velocity → level" and "Velocity → brightness", so that each side names one token.
   - **Four intent rules:** Tone → brightness, Space → FX sends, Motion → timbre, and the intent bus → anything.
   - **A new built-in ruleset, "Intent-driven".**
   - **The custom-rule builder** offers the collections first, then the specific ids.
   - A round-1 saved custom rule, stored as arrays, is read as its first id.
   - C6 still finds 0 broken routes (330 over 60 draws).

3. **The morph law on the pad.** Built: MORPH LAW (BLEND, QUANTUM, STEPPED), with temperature, cohesion and a step count, and a map of the selected parameter's regions drawn on the pad.
   - **QUANTUM** (ADR-104): every number and every structural key takes its value whole from one corner. The corner is the argmax of log(w_k)/T + (1 − c)·g_k + c·g_shared. The Gumbel draws are counter-based and keyed by the parameter id, so each key flips at its own seeded boundary and an appended key moves no boundary (ADR-185). Mod slots flip as units.
   - **BLEND:** numbers blend. Architecture flips at each key's own seeded point, which is today's rule, labelled **pending B585**. ADR-317's crossfade is B585's to design, and its demo is not rebuilt here.
   - **STEPPED** (B424): the puck snaps to an n × n grid, then the pad behaves as under BLEND at the snapped point.
   - **C15:** a sweep from A to D under QUANTUM changes values at 84 distinct positions out of 400, structural keys at 35. The control is one shared boundary (cohesion 1): 74 changes at 1 position, so it fails as it must.
   - **No hysteresis band.** horde's is 0.1. The lab is stateless in the puck, and a band would hide the boundaries the human asked to hear.
   - **What to listen for:** under QUANTUM, drag across a pad whose corners differ (randomize ARCH on each corner, then randomize all four). Each parameter and each structural choice changes at its own place. The census under the pad counts which corner each disagreeing key comes from at the puck.

4. **Snapshot fixes.** Built.
   - **The ✕ bug.** Round 2 refused to delete a snapshot bound to a corner, or marked home. A just-saved snapshot is always bound, so the ✕ almost never worked, and the refusal only reached the status line. Now the ✕ deletes the snapshot:
     - a corner bound to it keeps its sound, re-bound to the home as a draft;
     - a deleted home passes ★ to the first remaining snapshot;
     - only the last snapshot stays, and its ✕ is disabled with the reason.
   - **C16** is the regression check. It passes now, and its control, round 2's rule, fails: "deleted false".
   - **"save all four"** saves the four corners together as one named set of four snapshots ("Evening · A" … "· D"), each corner bound to its own. A set row loads all four back onto the unlocked corners. Deleting a set keeps its snapshots.
   - **C17** checks that the set holds four snapshots, all bound, with no drafts; that it loads back exactly; and that a locked corner keeps its sound. Its control saves one corner only.

5. **Drafted, not built (the human has not ruled 1.0 scope for these).**

   **5a. Arpeggiation randomization.** Arp is a hosted sibling device (its parameters are in `arp-face/params.snapshot.json`, 47 settings, and its bank's patterns).
   - **What it needs:**
     - an ARP level on the Random page with ARCH and PARAMS:
       - ARCH: pattern, mode, octave range, rate division, latch, the swing on/off;
       - PARAMS: gate length, swing amount, velocity shaping, and any step probabilities;
     - Arp's manifest classes, read through the one parameter table (ADR-319 item 4), so the page keeps no list of its own;
     - pass-through: if Arp declares its own randomize verbs (the optional rack verbs of §6), the buttons call those;
     - the Arp's bank patterns as a dice set ("lit = may be drawn");
     - rhythm-safe bounds by default. Rate divisions stay within one step of home unless CHAOS is high, so a random arp stays in time with the song.
   - **Recommendation: post-1.0.** Arp joins horde through B539. Its randomize should follow its own taste (a pattern library), which the Arp sibling owns, so the clean path is the pass-through once Arp declares the verbs.

   **5b. Explicit exemptions** (the human: "like scale or an oscillator's pitch offset").
   - **What it needs:**
     - an EXEMPT flag per parameter, which is a lock that **defaults on** for a declared class of keys. It is the same mechanism as the lab's locks, with a default. This lab already holds OUTPUT that way.
     - a column in the one parameter table, `random_default: exempt | free`, set in SPEC-ENGINE §5's generated table, so every page reads it;
     - first members: the scale and root (once horde has a global scale; the glide system's scale quantize, ADR-232), each oscillator's octave and semitone offset, the master tune, the voice mode, the output level and the arp's tempo division;
     - a visible distinction between "locked by you" and "exempt by default": the user can un-exempt a key, and the profile (ADR-318) can carry exemptions.
   - **Recommendation: in 1.0.** It is one column and one default on a mechanism the 1.0 set already contains (locks). Without it, randomize patch knocks the patch off the song's key and tuning, which is the most common way a random patch is unusable.

   **5c. A deeper pitch ruleset** (the human: "to make randomization more musical in more cases").
   - **What it needs:**
     - **Pitch as its own draw family.** Interval-aware choices replace the bell for pitch-valued keys:
       - an oscillator's semitone offset is drawn from a weighted interval set, with weights by consonance (unison, octave, fifth, fourth, third, sixth; tritone and seconds rare);
       - the two oscillators' offsets are drawn **jointly**, so their interval is musical, not just each one alone;
       - detune is drawn relative to unison count;
       - a pitch-envelope amount is drawn in musical units (an octave or a fifth more often than 7.3 st);
       - vibrato depth is bounded by rate.
     - **Scale-awareness** where a scale exists. Quantized destinations (an LFO to pitch through the glide system's scale quantize, ADR-232) prefer in-scale results.
     - **A pitch profile per role** (ADR-318's role set). A bass keeps OSC 1 at 0 and OSC 2 at 0/−12/+7; a pad allows thirds and sixths; a lead allows wider intervals and pitch envelopes.
     - **The measure:** B316's root-presence and roughness metrics, run over seeded random patches, so "more musical" is measured (Layer-E), not asserted.
   - **Recommendation: a minimal version in 1.0, the rest after.** For 1.0: weighted intervals for the semitone offsets, the joint draw of the two oscillators' interval, and musical-unit pitch-envelope amounts. That is a few tables in the profile, and it removes the commonest unmusical random results. Scale-awareness waits for a 1.0 scale, and the per-role pitch profiles ride ADR-318's role set as it is authored.

## 4c. Round 4 (ADR-322, 2026-10-29): copy a level between corners; one Morph page

**Copy a level** (ADR-322 item 1; the human: "there should also be a way on the random page to copy a level's settings from one corner to another independently"). Built:
- **Where:** every level row has a ⇄ button. It opens a strip with "copy <level> from [A–D] to [A–D]" and two choices, **ARCH** (the structure only) and **PARAMS** (the numbers only).
- **What moves:** only that level's keys. For MOD MAPPINGS, ARCH copies the routes and PARAMS the amounts. Every other level on the target stays as it was.
- **Locks:** a locked key on the target (by group, key or mod slot) is kept, and a locked target corner refuses.
- **History:** a copy is one action, so one history step, and it replays from the seed.
- **Drafts:** the target's binding is untouched, so a bound corner becomes a draft of its snapshot, the same rule as an edit.
- **C18 PASS:** copying OSC 1's parameters from A to B moves only OSC 1's parameter keys on B. B's OSC 1 architecture, every other level and the other corners stay. A locked key stays, and B becomes a draft. An ARCH copy touches only OSC 1's structure. The control, a whole-corner copy, moves keys outside OSC 1's parameters and fails as it must.

**One Morph page** (ADR-322 item 2). The Random page folds into the Morph page: one pad, one set of corners, snapshots and sets, the morph laws, and a **Randomize panel** (levels, chaos and amount, locks, randomization presets). The Random tab goes. This lab is **not** restructured for that: it stays a research bench (ADR-316). Its pad, corners, snapshots and laws are the Morph page's own, shown here beside the levels so the two can be tried together. Wherever this report says "the Random page", read "the Morph page's Randomize panel".

## 4d. Round 5 (2026-10-30): the envelopes as their own level

The human: "'osc envelopes' should be its own level separate from the rest of the oscillator details, and each individual oscillator's envelope could be randomized separately." The rest was ruled by poll: "Params edit shared; Arch can split", and for the architecture "I'd like to test both options".

**Correcting the record first.** Round 5's brief asked for level matching "as before". It was never built before: rounds 1–4 recommended it (finding 4, Q7) and only measured the problem. It is built now (below, C27).

**The §4 numbers predate this round.** Every number in §4 was measured on the round-1 stand-in: one amp envelope after the filters, no hold, no curves. Round 5's envelopes change the sound, so treat §4's figures as relative findings, not current measurements.

### The model the stand-in now follows (SPEC-ENGINE EN-1..4, C.12, RC-6.5)

- **ENV 1 to ENV 4** are attack–hold–decay–sustain–release envelopes. Each timed stage follows C.12's curve law, y = (1 − e^(−c·t/T)) / (1 − e^(−c)) with c = 4u, and lands exactly on its target.
- **One-shot (ENV 2–4):** a key-up is ignored until the decay ends.
- **The selector:** each source (OSC 1, OSC 2, SUB, NOISE) chooses the envelope that governs its level with its `env` selector, ENV 1 by default. Several sources may share one.
- **Placement in the voice:** the voice multiplies each source by its governing envelope before the filter (VO-9).
- **Freeing:** a voice is freed when every governing envelope of its on sources has ended.
- **Modulation:** ENV 2–4 stay modulation sources, with ENV 4 appended. A mod rule never shapes an envelope that governs an on source, because shaping it would change that source's level, not a modulation.
- **Lab simplification:** the lab groups C.12's three curve values into one structural **curve family** per envelope (default, linear, exponential, punchy, swell). The spec has three continuous curves; the family is a stand-in for them.

### 1. The ENVELOPES level, a row per source

ENVELOPES has four rows, ENV OSC 1, ENV OSC 2, ENV SUB and ENV NOISE. Each acts on the envelope that source follows now, and under the row the lab shows who shares it: "OSC 1 follows ENV 1 · shared with OSC 2, SUB".

- **PARAMS on a row** draws the stages of that envelope. If the envelope is shared, every source on it hears the change, and the row and the message say "SHARED with …". PARAMS never splits.
- **ARCH on a row** (placement a) may **split** the source off a shared envelope first. It moves onto a free envelope, one that no on source follows and no mod route reads. The free envelope first takes a copy of the shared one, so nothing jumps. Only then does ARCH draw the free envelope's curve family and one-shot. The message says "OSC 1 SPLIT off ENV 1 (shared with OSC 2, SUB) onto free ENV 3, a copy of it". With no free envelope it says so and draws the shared one. Nothing splits silently.
- **C19:** a shared PARAMS draw changes the shared envelope, the sources stay shared, and both the row and the message flag it. The control, a silent split, fails.
- **C20:** ARCH splits OSC 1 from ENV 1 to ENV 3 as a copy, and OSC 2 and ENV 1 are left untouched. The control, a split without the copy, fails.

### 2. The two placements of envelope architecture (a lab toggle, both end to end)

| | (a) the Envelopes level owns it | (b) each source's ARCH keeps its choice |
|---|---|---|
| Who draws a source's envelope selector | ENVELOPES ARCH (all four), or the source's envelope row (by splitting); VOICE and GLOBAL ARCH | that source's own ARCH (OSC 1 ARCH may move OSC 1 to ENV 3); VOICE and GLOBAL ARCH |
| One-shot and curve family | ENVELOPES ARCH, and each envelope row | VOICE and GLOBAL ARCH only (the Envelopes level and its rows have PARAMS only) |
| OSC 1 ARCH alone, 60 seeds, default knobs | moved OSC 1's envelope 0 times | moved it 10 times |
| What a move sounds like | a split is a copy first: no jump; the new envelope then changes only by its own draws | the source lands on whatever that envelope holds: a median 2.0× jump in its stage times (VOICE ARCH, 16 moves over 60 seeds) |
| Check | C24 | C25 |

In both placements a VOICE ARCH click leaves at least one shared envelope in 55 of 60 patches, because the selector's taste favours ENV 1. So sharing is the common case, and its handling decides how the page feels.

**Recommendation: (a), the Envelopes level owns envelope architecture.**
- **Sharing belongs to no single source.** The rule "Params edit shared; Arch can split" needs one place that sees every selector, decides when a split happens and announces it. (a) is that place; under (b) a split is a side effect of some other source's ARCH.
- **No silent jumps.** Under (b), randomizing OSC 1's architecture also re-times OSC 1, by a median 2×, with no message tying it to the envelope. Under (a), OSC 1's ARCH changes only its waveform structure, which matches the human's request that envelopes be "its own level separate from the rest of the oscillator details".

(b) is not wrong: it makes "randomize this oscillator completely" one click. If the human wants that, the cleaner form is (a) plus a shortcut ("ARCH incl. envelope") on the oscillator row that calls the envelope row's ARCH. That keeps the split rule in one place. Please compare them by ear with the toggle.

### 3. The four brainstorm ideas

- **Shape archetypes as envelope rules.**
  - The six are pluck, pad/swell, stab, gate, reverse swell and transient-only. Each is a set of stage ranges, a curve family and a one-shot setting.
  - With any archetype weighted, an envelope's PARAMS draw picks one archetype by weight and draws its stages inside its ranges; the bell and AMOUNT step aside for that envelope. Its ARCH draw takes the archetype's family and one-shot.
  - They are selectable (weights in ENVELOPE RULES) and they are part of the role presets.
  - The **role presets** (ADR-318) are Mono bass, Pad, Pluck, Lead and Arp, added beside Common and Everything (Anything). Each carries archetypes, a relationship, a release bound and, for Arp, tempo-relative times.
  - **C28:** with only pluck weighted, every drawn envelope falls inside pluck's ranges, 40/40; the control is 0/40.
- **Relationships between sources** apply on GLOBAL, VOICE and ENVELOPES draws:
  - a noise transient against a slow oscillator swell (noise on its own fast, one-shot, zero-sustain envelope; oscillator attacks of at least 150 ms);
  - the sub never attacks slower than the oscillators;
  - a stagger of at least 30 ms from OSC 1 to OSC 2;
  - one shape with small jitter per source (each source on its own envelope, within ±10% of OSC 1's).

  A rule that needs a source on its own envelope splits only on an ARCH or randomize-patch draw, and always announces it; on a PARAMS draw it says it needs ARCH. **C21:** each rule holds on 60/60 random patches with all four sources on. The controls, rules off, give 0, 27, 14 and 0 of 60.
- **Tempo-relative bounds** (in ENVELOPE RULES). Envelope times snap to the standard divisions at the patch tempo. A time shorter than the shortest division stays in seconds, a lab choice: a 1 ms attack has no note value.
  - Triplet and dotted toggles add those rows, exactly as the table specifies (they filter the menu only).
  - The table is RC-7.4 of `docs/proposals/morph-appendix-spec-text.md` on origin/b585-appendix (horde #1275, 22 rows). It is labelled in the page as **PROPOSED, not ratified**.
  - **C22:** every drawn time is on the grid at 120 bpm (237/237) and at 90 bpm (225/225), and the 90-bpm lengths differ. Triplets appear only when toggled. The control, tempo off, puts 0/237 on the grid.
- **Envelope copy and overlay.**
  - Copy takes one source's governing envelope (stages, family, one-shot) and puts it into the envelope another source follows, in the same corner or another. Only that envelope changes. A shared target is flagged.
  - The ⇄ copy on an envelope row does the same for one kind (structure or numbers).
  - **C23:** 5 keys move, all of ENV 3, equal to A's ENV 1; the control, copying every envelope, moves 21.
  - The **overlay** draws every on source's governing envelope as the puck hears it, by C.12's law, through the same envelope code the engine runs: one beat at the tempo, then the release. The legend names who follows each envelope and marks the shared ones.

### 4. Musical safety

- **Sustain floor** (default 0.15, in the profile). When no archetype is active, at least one on source's envelope keeps sustain at or above the floor after every draw.
- **Releases bounded per role:** Mono bass 0.25 s, Arp 0.4 s, Pluck 0.6 s, Lead 1.2 s, Pad 6 s, the others 8 s.
- **C26:** with the floor, 60/60 random patches keep a sounding sustain; the control, floor 0, gives 52/60. Under Mono bass every governing release is at most 0.25 s, 60/60; the control, Anything, gives 18/60.
- **Level matching:** the heard patch is rendered offline and a make-up gain is applied at the output, aiming at −20 dBFS RMS, within ±12 dB. It is measured when the puck lands, not while it is dragged. It is monitor-side, like the mixer's make-up (IF-6, RC-2), and never changes a stored value; the button shows the gain. **C27:** two patches 8.4 dB apart come out 0.00 dB apart. This check proves the arithmetic only; RMS is not loudness as heard, and the human's ears decide whether −20 dBFS RMS sounds level across very different patches.

### What I would put in 1.0

- **The Envelopes level with per-source rows, under placement (a)**, with the split rule and its announcements.
- **The archetypes as part of the role presets** (ADR-318's role set needs them to mean anything).
- **The sustain floor, the release bound per role, and level matching.**
- **Envelope copy** (it is round 4's level copy applied to envelopes).
- **The overlay**, which the Morph page's Randomize panel needs in order to show what a draw did to the envelopes.
- **After 1.0:**
  - the relationship rules beyond "the sub never slower" (that one is a safety rule and can ship);
  - user-edited archetypes;
  - tempo-relative times, until RC-7.4 is ratified and the instrument has a tempo-synced envelope at all.

## 5. Proposed basic 1.0 set, and what comes after

**Basic 1.0** (recommended):
1. **Randomize patch** and **randomize parameters** for the global patch, with a corner target (the selected corner), plus **randomize all four**, **revert** and **save snapshot**, on horde's own morph corners (see Q1).
2. **Per-module ARCH and PARAMS** buttons for the voice parts (OSC 1, OSC 2, SUB, NOISE, FILTERS, ENV, LFO), with **ARCH must change** on (finding 1).
3. **FX routing ARCH and PARAMS**, and each hosted FX module's randomize **passed through** where the module has its own (Shriek, Sluice), else drawn from its manifest (see §6, rack).
4. **One CHAOS knob** (the bell) and **one AMOUNT** slider, both **bounds for the next draw only** (§3a): moving them never changes the current sound. Home is the corner's snapshot, with no mode switch.
5. **Locks**: per group and per parameter, plus the existing per-corner lock (ADR-262). OUTPUT is always locked.
6. **Mod mappings** drawn from the randomization profile's rules (ADR-318: a small role set in 1.0), reading the one set of **collections** (§4b.2), with per-slot locks. No ruleset editor yet.
7. **Seeded and visible**: the seed and click shown, and a randomize as one history step.
8. **Level matching after a draw**, and the dependency tree, so nothing inaudible is drawn and no inaudible route is made.
9. **Solo and mute** on OSC 1, OSC 2, SUB and NOISE (§4a): IF-6's own gates, as monitoring state that is never drawn or stored.
10. **ARCH writes the selected corner by default** (ADR-320 item 1), with "all four" as an option.
11. **The pad plays the morph's law** (BLEND, QUANTUM, STEPPED), so randomized corners are heard through horde's own morph (§4b.3). Architecture under BLEND follows B585.
12. **Save all four** as a named set, and a working ✕ (§4b.4).
13. **Exemptions** that default on for scale and tuning keys (§4b.5b), and the minimal pitch draw (§4b.5c). Both are recommended; neither is ruled.

**After 1.0**:
- the ruleset editor, custom rules, and saved ruleset presets;
- per-group chaos and amount scales, per-parameter bounds, and the distribution view;
- dice per structural choice;
- the "home is patch / walk" modes;
- sweeping bounds live after a click: re-applying the last draw when a bound moves. This is an idea only. It was removed by the 2026-10-29 ruling (§3a), is not offered as a toggle, and would need its own ruling;
- the Random page on the FX device (ADR-312's FX half).

## 6. What horde needs from each part

- **Patch model (`patch`, B263).**
  - Every key needs: a stable key, range, taper, step, home (default), a structure-or-number class, and an "active when" condition.
  - Corners need to hold whole patches with a draft state (working copy versus bound snapshot), a home ★ per patch, and per-group and per-key lock state. Lock state is new user state, so ADR-277 must say whether it is preset or session state.
  - The draw needs (seed, click) saved with the patch, so a sound heard can be traced.
- **Morph (`morph`).**
  - The dominant-corner flip for structure; a route's amount blending only when the route matches; the corner lock (ADR-262).
  - The **counter-based draw ADR (B308 H3)** is a prerequisite: replay, position-free locks (C2) and nested amounts (C3) depend on it. So would any post-1.0 live sweep.
- **Mod matrix (`modmatrix`).** Slot locks; a "rule" tag per route, so PARAMS can re-draw amounts inside the route's range; destination liveness from the dependency tree; a source-shaping write into ENV and LFO parameters, made by the same action.
- **The rack and its slot contract (`rack`, docs/proposals/rack-slot-contract.md).** The contract has **no randomize verbs today**. Its parameter classes already split the work:
  - **flip**-class keys are ARCH;
  - **morph**-class keys are PARAMS;
  - **device**-class keys are never drawn (RS-4.5).

  So horde can draw any module generically from its manifest. The owed addition is an **OPTIONAL** pair of verbs, modelled on RS-4.10's optional morph-position call: `randomizeStructure(seed, click, chaos, amount, locks)` and `randomizeParams(…)`. A module with its own taste (Sluice's randomiser, Shriek's dice) declares them, and horde passes its buttons through. Edits arriving from inside a module's own face are already covered as gestures by RS-6.14. This is a proposal to file with the rack work, not a ruling.
- **Dependency tree (B316 P1) and fitted bounds (B316 P4).** The tree for what is drawn and which routes are live; the fitted bounds for the taste weights and ranges (finding 4).

## 7. Questions for the human

1. **Q1. Where does the Random page's XY grid live?**
   - (a) It is horde's own morph pad, the same four corners, shown on the Random page.
   - (b) It is a separate grid of snapshots.

   ADR-306 rejected Sluice's buttons on horde's morph corners "not asked for now", and ADR-312 now asks for a grid like Sluice's and Shriek's. **Recommendation: (a).** A second four-corner field would be a second morph system to keep in sync. The lab's grid is built as (a).
2. **Q2. Should a module's ARCH button always change something?** **Recommendation: yes, on by default** (finding 1: otherwise 42–95% of clicks do nothing at the default chaos).
3. **Q3. When ARCH is pressed, does it write the structure to all four corners or only the selected one?** **RULED (ADR-320 item 1): the selected corner by default**, with "all four" kept as an option. The lab's earlier recommendation was all four (finding 6). Under QUANTUM the per-corner structure now flips key by key at seeded boundaries (C15), not all at the centre lines.
4. **Q4. What is "home" for the bell?** **Recommendation: the corner's saved snapshot, only, in 1.0** (finding 7). "Save snapshot" moves home.
5. **Q5. Is one fixed ruleset enough for 1.0, with the editor and saved rulesets after?** **Recommendation: yes.** "Common" draws its routes cleanly (finding 5). The editor's real difficulty, source contention and shaping, deserves its own round.
6. **Q6. Should the ruleset draw be part of GLOBAL ARCH and randomize patch?** **Recommendation: yes,** drawn after the structure, as the lab does (finding 5: dependency).
7. **Q7. Should a random patch be level-matched?** **Recommendation: yes, before 1.0** (finding 4: 27% hit the guard and 7% were near-silent at chaos 1). A measured make-up gain, not a guess.
8. **Q8. Locks: per group and per parameter in 1.0, labelled a proposal until you rule?** **Recommendation: yes.** Sluice's randomiser exclusions show that players want them, and the lab shows they cost nothing once draws are counter-based (C2, C5).
9. **Q9. The CHAOS curve.** σ is now 4·c·50^(c−1). It is tuned by ear **at the next draw only** (§3a): at CHAOS 0.1, 0.25, 0.5 and 0.75, press PARAMS a few times on the arpeggio, and say where a click first does something audible and where higher CHAOS stops adding. **Recommendation:** tune the curve from that listening (finding 3).
10. **Q10. Should the rack-slot contract gain the optional randomize verbs (§6)?** **Recommendation: yes,** filed as a proposal with the rack work, with Sluice and Shriek as the first declarers.

## 8. What the lab could not do well (grounded refusals)

- **Listening.** I cannot hear the lab. Loudness and guard rates are measured; whether a draw sounds good is not. Findings 3 and 4 need your ears.
- **horde's engine.** The oscillators are a Scalpel-like stand-in: naive blade edges that alias, and no swarm. The lab says what randomizing a structure does; it cannot say what the SCALPEL engine sounds like randomized.
- **Real-time audio.** The audio is a ScriptProcessor on the main thread. Heavy repainting while the puck is dragged can underrun it. Browser meters cannot see underruns, so trust what you hear.
- **The navigator's heading.** The lab is filed under "Morph and state", because the navigator's part list (`tools/gen_lab_index.py`, outside this lab's scope) has no `random` part yet. ADR-312 adds the part to H2-PLAN.

## 9. Evidence

- **Self-checks** (in the page, and run headless at load by `lab_load_check`):
  - C1 replay determinism;
  - C2 position-free draws;
  - C3 amount nesting;
  - C4 the chaos ends (KS distance 0.014 from flat at chaos 1);
  - C5 locks;
  - C6 rule conformance;
  - C7 the −6 dBFS guard (a +9 dBFS pre-guard patch is held at −6.02 dBFS);
  - C8 morph;
  - C9 snapshots, draft, revert and corner lock;
  - C10 every level × kind plays;
  - C11 ARCH must change;
  - C12 solo isolates a source (control: solo after the mix);
  - C13 mute keeps the source running, and the 5 ms raised-cosine gate;
  - C14 bounds are for the next draw only (control: a stand-in that re-applies the last draw);
  - C15 QUANTUM flips each key at its own seeded boundary (control: one shared boundary);
  - C16 the ✕ deletes a saved snapshot (control: round 2's refusing rule);
  - C17 save all four (control: saving one corner);
  - C18 copy a level between corners (control: a whole-corner copy);
  - C19 a shared envelope's PARAMS draw (control: a silent split); C20 ARCH splits onto a free envelope (control: no copy);
  - C21 the four relationship rules over 60 seeds (control: each rule off); C22 tempo-relative times at 120 and 90 bpm (control: tempo off);
  - C23 envelope copy moves only that envelope (control: copying every envelope); C24 and C25 the two placements, each with the other as its control;
  - C26 the sustain floor and the role's release bound (controls: floor 0, the Anything role); C27 level matching (control: unmatched); C28 archetypes (control: none).

  Each has a must-fail control.
- **A headless Chrome run** clicked every level's ARCH and PARAMS and the grid, ruleset and lock buttons. It raised no error, and the replay matched (42 actions).
- **The numbers in §4** come from a Node script that runs the lab's own functions (no copy), over seeds 1–60. Its output is quoted in the trace (`traces/2026-10-29-b578-random-lab.md`).
