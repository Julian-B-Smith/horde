# SCALPEL's master envelope: the conflict inventory (B366)

Origin: horde lead session, ROADMAP B366 (records branch `lead-records-144`), 2026-09-29. Written by
the implementer on branch `scalpel-master-env`; trace `traces/2026-09-29-b366-scalpel-master-env.md`.

The human, 2026-09-29: "could you please add a master envelope and put it on both main and the osc
page for Scalpel? ... This may surface some conflicts with the way envelopes currently work and the
way they need to before it's ready for prime time."

**Direction since (the lead, 2026-09-29).** The human: "I think want to follow the Serum approach of
having ENV 1 be a global envelope that applies to all pre-FX sound-generating devices (including
filters). Then we need to determine the envelopes work for things like onset scatter (an important knob
I would like to preserve; but I'm frustrated by the trade-off of eating up an envelope slot with it
versus giving it a secret proprietary envelope which may clash with certain settings)."

So the master envelope below is the **prototype of ENV 1**. It is the voice's amplitude envelope,
applied after the per-voice sources and, in horde 2, the filters, and before FX. The full envelope
hierarchy gets its own lab (B370). The section "Onset scatter under ENV 1" analyses the lead's
candidate for onset scatter. It is analysis only; nothing of it is built here.

This document lists every mechanism that already shapes a SCALPEL voice's amplitude or its note
timing. For each one it says what the mechanism does, where it lives, and how it meets the master
envelope. It ends with the decisions that are needed before prime time, as questions, each with a
recommendation.

## What the master envelope is (and why it is not a divergence)

The master envelope is the voice's own amplitude ADSR. It **already exists** in the SCALPEL oracle,
and the composed engine keeps it unchanged. It has four parameters, `A`, `D`, `S` and `R`:

- a **linear** attack over `A`;
- an **exponential** decay toward `S`, `exp(−4t/D)`;
- an **exponential** release, `exp(−4t/R)`;
- the voice is freed under `1e-4`.

Every coefficient is recomputed from its time and the sample rate on each render call, which is
ADR-009's rule (`reference/scalpel/prototype/razor-core.js:740-742`, `:777-779`). The composed engine
says so in its header (`docs/design/scalpel-horde-engine.js:174`, "NOT COMPOSED THIS ROUND").

So B366 **exposes** the envelope. It adds no engine behaviour and no ADR-187 divergence. The ledger
(`docs/port/divergences.json`) is unchanged.

In the lab (`docs/design/scalpel-interface-lab.html`, section E4):

- **Controls.** `A D S R` are one control set drawn on MAIN and on OSC. They left One level down,
  where the same keys sat before. Times display in seconds; the stored value stays the oracle's ms key.
- **The drawing.** The curve is the oracle's law at the knobs' values.
- **The stage indicator.** It follows the newest voice in what was **rendered**: the audio's own feed
  while it runs, otherwise the monitor, as every other view does (L0064). The feed is B365's
  `feedOf`.
- **Gated in-page by `checkB366`:**
  - one state in two views;
  - the law in seconds at 44.1 and 48 kHz;
  - the stage indicator following the rendered voice;
  - the compact macros;
  - the twelve envelope presets.

## Measured facts

Two kinds of numbers appear below.

- **[gated]** numbers are re-measured by the lab's self-check at every load (`checkB366`, each row
  with a must-fail control).
- **[measured]** numbers are one-off scratch renders of the composed engine in Node, at 48 kHz,
  seeded. The method is given in each row. They are not gated.

| # | Fact | Number | Kind |
|---|---|---|---|
| F1 | A 0.1 s, D 0.4 s, S 0.5, R 0.28 s, at 44.1 and 48 kHz | attack reaches 1 at 100.00 ms; at D the level is 0.5092 (law 0.5092); sustain 0.5000; at R after key-off, 0.0183 of the start (e^−4); "faded" for B310's tier 1 (env < 1e-3) at 435.0 ms; **freed at 596.2 ms**; then under −90 dBFS (−113 dBFS, the 2× decimator ringing out). Same at both rates | [gated] |
| F2 | Oracle defaults (A 4 ms, D 0.4 s, S 0.85, R 0.28 s) | the release knob's "0.28 s" is the time to −35 dB. Time to −60 dB: 484 ms. Time to tier-1 "faded": 472 ms. Voice freed: **633 ms (2.26 × R)** | [measured], law |
| F3 | Legacy horde's law with the same numbers (ADR-021, one-pole: the knob is τ) | R 0.28 s reaches −60 dB at **1934 ms** and is culled (−80 dB) at 2533 ms, **4× SCALPEL's**. A 4 ms reaches 0.995 at 21 ms (SCALPEL: 4 ms, linear). A 2 s reaches 0.995 at 10.6 s | [measured], law |
| F4 | A held pluck (S 0), 3 s after the key went down | env 1.45e-21, still active and gated: **it holds its voice until the key-off** | [measured] |
| F5 | B323's cull on a 5 s release | a tail with 11.15 s of release left is freed **10.7 ms** after the cap drops (the 8 ms linear ramp, rounded to 128-sample blocks) | [measured]: two released notes, cap set to 1 |
| F6 | At the cap, a note-on takes a sounding tail's slot | the tail (env 0.747) is replaced at once, with **no fade**. B316's click metric reads 0 clicks (worst frame 2.4 dB over the median; the uncapped control reads 3.8 dB) | [measured] |
| F7 | B310 tier-2 steal on a six-voice pool | the 7th note takes the quietest releasing tail: 0 clicks, worst 1.9 dB (control with 8 voices and no steal: 1.8 dB). The pile-up preset keeps its held C3 and loses the two quietest tails | [measured], [gated] (pile-up) |
| F8 | A stolen slot keeps its level (`razor-core.js:397`, `if (!v.active) v.env = 0`) | a pad (A 0.9 s) whose two voices were released 0.2 s earlier: the new note **starts at 0.646**, so its "0.9 s" attack takes 0.32 s | [measured] |
| F9 | A blade envelope longer than the amp release (benvD 5 s, R 0.15 s, S 0.6, a 0.2 s note) | the voice is freed 328 ms after the key-off with its blade envelope still at **0.656**. The cut-rate multiplier is still 4.28×: the sweep is cut off, not finished | [measured] |
| F10 | Mono retrigger 0.1 s into a note (blade D 0.4 s, benvK 0.8) | the amp attacks on from its level (0.634 → 0.654). **The blade envelope restarts from 0** (0.372 → 0.021), so the cut rate jumps from 2.28× to 1.05× in one sample. Worst click frame 6.4 dB (the benvK 0 control: 2.6 dB; a click is > 20 dB) | [measured] |
| F11 | Legato, an overlapping key 0.3 s into a 0.6 s attack | the attack runs on (0.500 → 0.667, still stage 1): **no retrigger** of the amp or the blade envelope | [measured] |
| F12 | Onset scatter 20 ms, attack 1 ms, 7 members | members enter at 0 to 83 ms, and the output reaches 90% of its peak at **52 ms** (0.9 ms unscattered) | [measured] |
| F13 | Per-partial envelope (voiceEnv) with attack and release scatter 1 (A 100 ms, R 500 ms, 7 members) | member attacks 30 to 222 ms; member releases 75 to 665 ms | [measured] |
| F14 | Onset lock under a 2 s swell (dissolve 0.63 s) | when the swell is within 3 dB (1.42 s), **10.5%** of the lock burst is left | [measured], law |
| F15 | Velocity | 0.5 against 1 reads −6.00 dB (linear, full depth) | [measured] |
| F16 | Sustain against the output tanh | S 0.5 against S 1 reads −5.76 dB at the output at the default gain 0.35, and −4.54 dB at gain 1 (the envelope says −6.02) | [measured] |

## The mechanisms

### The gain chain, in order (per sample)

1. **Blade shape.** Each member's sample comes from `voice()`. The blade envelope scales its cut rate
   and width (kE, wE).
2. **Per-member gain** (B335, only when onset scatter or voiceEnv is on). The member's gain is `pg`:
   0 while it waits, then its entry ramp or its own ADSR over the loudest.
3. **Voice sum.** Members are panned and summed.
4. **Voice gain.** The sum is multiplied by `amp = env · vel · 1/√N` (`razor-core.js:862`). `env` is
   the master envelope, and B323's cull ramp multiplies into it.
5. **Output sum.** Voices are summed, then run through the 2× decimator, then the DC blocker (with
   cross-mod or feedback).
6. **Output stage.** Then `tanh(y · gain · 1.6)`, then the lab's Master.

### Who frees a voice

- **The master envelope.** Under 1e-4 in its release (`razor-core.js:779`).
- **B323's cull.** At the end of the 8 ms ramp (`scalpel-horde-engine.js:1046`).
- **A steal takes a slot** without freeing it. This is tier 2 (a tail), tier 3 (the oldest held), or
  B323's at-cap note-on.
- **With voiceEnv on,** the voice's `env` is the loudest member's (`scalpel-horde-engine.js:729`), so
  the voice is freed when the last member's own release ends.

Nothing else frees a voice. A held voice is never freed, even at S 0 (F4).

### 1. The oracle's voice amplitude stage: the master envelope

- **What it does:** ADSR as above; velocity scales it linearly (F15). Retrigger (mono, or a
  re-struck slot) attacks from the current level. Legato does not retrigger (F11). Changing
  `polyMode` releases every voice (`razor-core.js:357-358`), and so does panic (`:354`).
- **Parameter smoothing:** A, D, S and R pass through the parameter smoother (12 ms one-pole, every
  16 samples, `razor-core.js:746`). A note struck within about 50 ms of a preset change reads times
  part-way between the two presets.
- **Where it lives:** `razor-core.js:740-742`, `:777-779`, `:862`; `startVoice` `:394-417`;
  `noteOff` `:419-434`.
- **Against a master envelope:** it IS the master envelope.

### 2. Blade envelopes (`benvA/D/K/W/Vel`, blade 2's with `b2env`)

- **What it does:** a separate attack-decay envelope with no sustain and no release. It scales cut
  rate (`2^(4·benvK·e)`) and width (`2^(3·benvW·e)`), with its own velocity depth (`benvVel`).
- **Where it lives:** `razor-core.js:754-776`; restarted with the amp at `:406-409`.
- **Against the master envelope:**
  - It is not an amplitude stage, so there is no double gain.
  - It runs on through the key-off and dies with the voice. A blade decay longer than the amp's tail
    is cut off (F9).
  - A mono retrigger restarts it from 0 while the amp continues (F10).
  - Legato restarts neither.

### 3. B335: onset scatter, timing correction, entry ramp

- **What it does:**
  - Members wait a scattered delay before they enter (ms, persistent across notes; ADR-077).
  - Each then ramps in on a one-pole at the master attack time.
- **Where it lives:** `scalpel-horde-engine.js:643-680` (armMembers, draws transcribed from
  `src/swarm_core.h:646-684`); `:696-733` (memberStep).
- **Against the master envelope:**
  - The gain is a per-member factor under the master gain (chain step 2).
  - It delays the note's effective attack beyond `A` (F12).
  - The entry ramp reads `A` at note-on only: a later change to A does not reach a waiting member.
  - A mono retrigger keeps the draws (B335's open question).

### 4. B335: per-partial envelope (voiceEnv), attack and release scatter

- **What it does:** each member runs the master ADSR with its own attack and release times, drawn by
  the C++'s law (F13). The voice's `env` becomes bookkeeping: the loudest member.
- **Where it lives:** `scalpel-horde-engine.js:696-733`; law `:710` (floors at 2 ms, as
  `swarm_core.h:659`).
- **Against the master envelope:**
  - The master A and R become the centre of a spread. The drawn curve is then the unscattered one
    (a band is not drawn).
  - The voice is freed when the last member's release ends.
  - B335 notes that it uses SCALPEL's envelope law, not horde's, and that this needs ratification.

### 5. B310: horde's voice law in the composed engine (ADR-083's tiers)

- **What it does:** a note-on takes a free slot, else the quietest releasing tail, else the oldest
  held voice. A repeated note gets a new voice, so the first release rings on.
- **Where it lives:** `scalpel-horde-engine.js:501-530`; ADR-083; `src/swarm_core.h:1669-1690`.
- **Against the master envelope:**
  - "Faded" is env < 1e-3 (−60 dB); the master frees at 1e-4 (−80 dB). A tail between those two
    levels is still sounding (F1: 435 to 596 ms), and tier 1 takes it at once with no fade. It is at
    or under −60 dB, so this is inaudible by design.
  - A stolen slot keeps its level: the new note's attack starts from the old tail's level (F8). A
    slow pad then loses its swell whenever it steals a loud tail.
  - Long releases (R 5 s: tails of about 11 s) fill the pool, so steals become routine (the pile-up
    preset).
  - A held S 0 note keeps its slot (F4). Only tier 3 can take it.

### 6. B323: the voice cap and the 8 ms cull fade

- **What it does:** a load-driven cap, in the lab only. Over the cap, the quietest releasing tails
  fade out over 8 ms (a linear ramp multiplied into `env`) and are freed. At the cap, a note-on
  **replaces** a sounding tail in place.
- **Where it lives:** `scalpel-horde-engine.js:237-244` (CULL_FADE), `:533-550`, `:1040-1052`.
- **Against the master envelope:**
  - It overrides the release: an 11 s tail ends in 8 ms (F5).
  - The at-cap replacement has no fade at all (F6). It did not click on B316's metric here, but it is
    not the same rule as the cull.
  - The cap depends on machine load, so a long release sounds different on a loaded machine (B323's
    note).

### 7. B325: the provisional first tick

- **What it does:** a look-ahead swarm tick at note-on, so that the first sample's member
  frequencies, the Hz-unit cut rates and the DC estimate are right. It is undone before the real
  first tick.
- **Where it lives:** `scalpel-horde-engine.js:580-599`, `lookAhead`.
- **Against the master envelope:** it is not an amplitude stage. It matters most where the attack is
  shortest (plucks, A 1 ms), because only then are the first samples loud. There is no conflict. It
  must survive any attack-shape change.

### 8. The swarm's onset lock and dissolve (SwarmSynth `Kenv`)

- **What it does:** at note-on, a coupling burst of `8·onset²·σ` that decays with τ = dissolve
  (0.63 s by default) from the note-on. It is a timbre envelope, not an amplitude one.
- **Where it lives:** `reference/swarmsaw.html:355`, `:387`; legacy `src/swarm_core.h:635`
  (bipolar, ADR-056).
- **Against the master envelope:**
  - It is clocked from the note-on, not from the amp. Under a slow attack the burst is mostly gone
    before the note is heard (F14).
  - The composed engine never note-offs the swarm, so the key-off does nothing to it.

### 9. The lab's own controls and views

- **Before B366:** `A D S R` sat in One level down's "SWARM — survivors" list (T2 sliders, in ms).
  They are now the face's master envelope on both pages (T1 by the human's ask, which the tier table
  reports as promoted).
- **Keyboard velocity:** the lab's keyboard always sends velocity 0.85 (`noteOn`), so velocity cannot
  be auditioned here.
- **The monitor:** it holds its note for ever, so every view it feeds (the ring, the cycle view, the
  carpet) still draws a held note after a release. B365 moved the Specimen, and B366's stage
  indicator, to the audio's rendered feed.

### 10. Legacy horde's amplitude envelope (ADR-021) and ADR-083 / ADR-084

- **What it does:**
  - One-pole attack, decay and release in seconds: each knob is τ.
  - The attack ends at 0.995; at S ≥ 1 the decay is skipped (the reference's AR).
  - The voice is culled at `voiceCull` (−80 dB by default).
  - Velocity is linear, times MPE pressure (ADR-084).
  - Steals use ADR-083's tiers.
  - Re-struck slots attack from their current level: `initVoice` does not reset `env`.
- **Where it lives:** `src/swarm_core.h:160-165`, `:1000-1024`, `:1194-1228`, `:1669-1690`,
  `:622-641`.
- **Against the master envelope:**
  - ACCOUNTING rows 17-20 (`docs/scalpel/ACCOUNTING.md:148-151`, `:396`) rule that A, D, S and R
    **MERGE** into horde's rows and that **horde's law wins** ("SCALPEL presets translate ms → s; the
    linear-vs-one-pole attack shape differs (listen, not map)").
  - The composed engine, and so this lab, plays SCALPEL's law today. The same numbers mean different
    times (F3), so the lab does not yet predict horde 2's envelope.
  - A velocity-0 note-on must reach the engine as a note-off. ADR-021's field report found an
    AUv2 path that struck a silent voice instead, and in this engine such a voice would hold a slot
    for ever.

## Onset scatter under ENV 1

### The candidate

This is the lead's proposal. Onset scatter is **not a separate envelope**. It is a **spread
dimension** of how ENV 1 is read per swarm member:

- Member *i* reads ENV 1 at its own time offset δᵢ (onset scatter).
- Optionally, it also reads at its own time scale sᵢ (attack and release scatter).
- The timing correction (`onsetAlpha`, ADR-077's persistent memory) governs how the δᵢ are drawn.

The result: no extra slot, no hidden envelope, and one shape everywhere. In a formula, member *i*'s
gain is `ENV1((t − δᵢ) / sᵢ)`. The voice's gain is then the sum of its members' reads, as today.

### What already approximates it

B335's `voiceEnv`, with onset scatter on, is close to the candidate
(`scalpel-horde-engine.js:643-733`). Each member:

- waits δᵢ, drawn by ADR-077's law with `onsetAlpha`, shifted so the earliest member is at 0;
- then runs **the voice's own ADSR** from its own entry, with its attack and release times scaled by
  drawn factors (`aMul`, `rMul`, floored at 2 ms).

The voice's `env` is bookkeeping: the loudest member. The differences from the candidate:

1. **The decay is not scaled.** It uses D for every member; only A and R are scaled. The candidate's
   sᵢ could scale the whole time axis, or keep attack and release scatter separate as today. That is
   a choice to make.
2. **Onset scatter without voiceEnv is a different model.** A late member there is not a late read of
   the envelope. It fades in (a one-pole at the attack time) on top of the SHARED envelope, at
   wherever that has reached.
   - Reasoned out on F12's draw: members enter at 0, 25, 41.5, 42, 43.5, 46.7 and 83.1 ms. On the
     "Pluck · sync blade" envelope (A 1 ms, D 0.26 s, S 0), the 83.1 ms member enters when the shared
     envelope stands at 0.283 (−11.0 dB). It never has its own pluck.
   - Under the candidate it reads its own full attack, peaking at 1.0.
   - So the two modes of today's engine disagree about what a late member is. The candidate picks
     voiceEnv's answer.
3. **The key-off is not spread.** Every member starts its release at the key-off. Only the release
   time is scaled.

### Where it could still clash

Each item below is reasoned out, or measured where marked.

- **A per-voice filter (or anything per-voice that ENV 1 modulates).** A filter after the member sum
  gets one control signal, but under the candidate ENV 1 is read seven ways. The choices, on F12's
  draw with the pluck above:
  - **The unscattered read, ENV1(t).** This is also the earliest member, since the draw puts the
    earliest at δ = 0 and sᵢ = 1. It peaks at 1.0 at 1 ms and is at 0.283 by the time the last member
    enters. The filter has closed by −11 dB of its envelope depth before that member's pluck, which
    would then be heard through a closing filter. The late members' attacks are dulled: the clash the
    human fears.
  - **The ensemble mean, (1/N)·Σ ENV1(t − δᵢ).** It peaks at 0.716 at 47.7 ms. At 25 ms it is 0.099
    against the unscattered 0.691; at 100 ms it is 0.432 against 0.218. It matches the SUMMED level
    the filter actually receives, so the filter opens with the ensemble. But its peak is 0.716, not
    1: a full-depth ENV 1 route never reaches full depth while scatter is on.
  - **The mean normalised to peak 1.** This keeps the depth and follows the ensemble, at the cost of
    one division per voice.
  - *Recommendation:* ENV 1 as a per-voice modulation source is the ensemble-mean read, normalised.
    The per-member reads are computed anyway, so it costs one sum. It is then "one shape" in timing
    as well as in shape. A per-MEMBER destination (a blade parameter) reads its own member's ENV 1.
- **Release: the voice lives until the last member ends.**
  - With release scatter, the voice's tail is the longest member's. On F13's draw (releases 75 to
    665 ms around 500) the voice lives 1.33× the nominal tail. The pool then holds tails longer, and
    B310's tier 2 steals sooner.
  - If the key-off were spread too (δᵢ on the release), each member would release up to max δᵢ late.
    That is +83 ms on F12's draw, and the voice lives that much longer again.
  - *Recommendation:* spread the onset only, not the key-off (as today). Keep "faded" and freeing on
    the loudest member (as voiceEnv's bookkeeping already does), and draw the tail on the curve as the
    longest member's.
- **Mono and legato retrigger.** Read as a pure function of time since the note-on, a retrigger would
  set every member back to `t − δᵢ < 0`, that is to silence. The late members would drop out and
  re-enter: a dropout, and a click risk, on every mono retrigger.
  - Today, a non-fresh retrigger keeps the draws, and each member re-enters its attack from its own
    level (`scalpel-horde-engine.js:568`, `m.eS = 1`). There is no wait again, so no dropout.
  - Legato does not retrigger at all.
  - *Recommendation:* ENV 1 is a per-member STATE MACHINE (a stage and a level, as the oracle's is),
    not a pure function of time. δᵢ applies only to a FRESH note. A retrigger restarts each member's
    attack from its own level, with no wait. Whether a retrigger re-draws δᵢ, which ADR-077's memory
    would favour, stays B335's open question.
- **Voice stealing and B323's cull.**
  - A stolen slot keeps the old note's level (F8), and under scatter each waiting member of the new
    note is silent until its δᵢ. So a steal drops every late member of the old sound to silence at
    once, while the early ones continue at the old level (B335's open item: "a stolen voice's waiting
    member drops to silence"). That is a discontinuity at the steal.
  - Decision 6's rule removes it: the old voice fades over 8 ms in a spare slot, and the new note
    enters fresh from 0.
  - B323's cull multiplies the voice's gain after the member reads, so it does not clash. Its "quietest
    tail" key is the loudest member's level, as voiceEnv keeps it.
- **One shape everywhere.** This holds for the shape. It holds for the timing only if the per-voice
  source is the ensemble read (the first bullet). Otherwise ENV 1 on a filter and ENV 1 on the
  amplitude disagree by exactly the scatter.
- **Cost.** A per-member envelope costs N envelope steps per voice (N ≤ 9). voiceEnv already pays it
  (memberStep), and the engine skips the member step entirely when neither onset scatter nor voiceEnv
  is on.

### Verdict

The candidate resolves the human's trade-off:

- Onset scatter becomes a section of ENV 1 (onset scatter in ms, timing correction α, attack scatter,
  release scatter), not a slot and not a hidden envelope.
- voiceEnv is most of the mechanism already.

What must be ruled with it:

1. ENV 1 as a per-voice source is the normalised ensemble read (the filter clash).
2. δᵢ applies to fresh notes only, and ENV 1 is a per-member state machine (the retrigger clash).
3. Steals fade the old voice (the steal clash, decision 6).
4. The key-off is not spread.
5. Onset scatter WITHOUT per-member envelopes (today's entry-ramp mode) is retired in favour of the
   read, because the two disagree about what a late member is.

## Decisions needed before prime time

Each question has a recommendation. **The first one gates most of the rest.**

1. **Which law is the master envelope's: SCALPEL's or horde's?** SCALPEL's is a linear attack, with
   decay and release times meaning 4τ and the voice freed at −80 dB. Horde's is one-pole, each knob
   τ, as ACCOUNTING rows 17-20 already ruled ("horde law wins"). With the same numbers, horde's
   release is 4× longer and its attack about 5× longer to full (F3).
   *Recommendation:* keep ACCOUNTING's ruling for horde 2, for rev-1 preset identity (B312's porter
   and 28 sets depend on horde's timings). Build it as the composed engine's divergence D5: a flag,
   default off, an ADR-187 ledger entry, in its own PR. Then A/B the twelve envelope presets by ear
   before the lab turns it on (ADR-187 A1: one default flip per PR). If the ear prefers SCALPEL's
   linear attack, rule an attack-shape switch rather than carrying two laws.

2. **What does the Release number mean on the panel?** The options: time to −35 dB (SCALPEL's knob),
   τ (horde's knob), or time to silence (−80 dB, 2.26 × R, F2).
   *Recommendation:* keep the law's own parameter on the knob, and always show the tail (key-off to
   voice freed) next to it, as the lab's curve now does ("tail 0.63 s").

3. **Velocity.** Both engines apply full-depth linear velocity (F15), with no amount and no curve.
   *Recommendation:* add a master-envelope velocity amount (0..1, default 1, which is bit-identical)
   beside `benvVel`, which already exists for the blade envelope. Leave the curve for later (ADR-084
   planned one). Map a velocity-0 note-on to a note-off in the shell.

4. **Blade envelopes against the master.** They are attack-decay only, run through the key-off and
   are cut when the voice ends (F9).
   *Recommendation:* keep them independent for 1.0 and document the truncation. Later, expose them
   (and the master) as modulation sources under B70's cycle rule. Do not give them a release stage
   yet.

5. **Mono retrigger.** The amp attacks from its level, but the blade envelope restarts from 0, so the
   cut rate jumps (2.28× to 1.05× in one sample, F10).
   *Recommendation:* retrigger the blade envelope from its current level too, matching the amp (a
   divergence to ledger). Alternatively, make "retrigger from zero" one explicit choice that applies
   to both.

6. **Steals and level continuity.** A stolen slot starts its attack at the old tail's level (F8), so
   slow attacks lose their swell. The at-cap replacement is instant while the cull fades over 8 ms
   (F5, F6).
   *Recommendation:* one rule for every steal. The old voice fades over B323's 8 ms ramp and the new
   note attacks from 0. This needs a spare slot per voice (a small divergence), so ask before
   building it.

7. **Held notes at sustain 0.** A held pluck keeps its voice indefinitely (F4).
   *Recommendation:* at S = 0, let a gated voice whose envelope is under −80 dB count as tier 1
   ("faded"). This is a voice-law change, so it needs its own ADR-083 amendment.

8. **Per-partial envelope and scatter.** With voiceEnv on, the master A and R are the centre of a
   spread (F13). B335's "voiceEnv uses the composed engine's envelope law" is unratified.
   *Recommendation:* ratify it together with decision 1 (it must follow the same law), and draw the
   spread as a band on the master curve when scatter is on.

9. **Onset scatter and the attack.** Scatter delays the note's effective attack by tens of
   milliseconds (F12).
   *Recommendation:* keep it independent (it is the ensemble's timing, ADR-077). Draw it as a lead-in
   on the curve when it is on.

10. **Onset lock under slow attacks.** The lock burst is clocked from the note-on, so under a swell
    it is inaudible (F14).
    *Recommendation:* ask the ear. If it matters, start the dissolve at the end of the attack. This
    is a divergence from SwarmSynth.

11. **The voice limit for horde 2.** Should it be B323's load-adaptive cap, which is lab-only today
    and cuts releases (F5), or a fixed count?
    *Recommendation:* a deterministic polyphony count for the instrument, with steals by decision 6.
    Any CPU-adaptive mode should be explicit and documented (B323's note).

12. **Envelope parameter smoothing.** A, D, S and R glide over the 12 ms smoother, so a note struck
    during a preset change reads intermediate times.
    *Recommendation:* take the times A, D and R unsmoothed, since a glide on a time constant buys
    nothing. Legacy horde's core rebuilds its coefficients from the seconds on every render call
    (ADR-021's field report); whether its shell smooths those rows was not checked here. Keep S
    smoothed, because it is a level and a step would click.
    The oracle smooths all four, so this is a small divergence.

13. **Sustain against the output tanh.** The sustain level is not the output level at hot gains
    (−6.02 dB of envelope reads −4.54 dB at gain 1, F16).
    *Recommendation:* no change for 1.0. Note it in the manual. It becomes moot if the output stage is
    reworked.

14. **ENV 1 and onset scatter** (the section above). Adopt the spread-read model, and with it these
    four rules:
    - ENV 1 as a per-voice source is the normalised ensemble read;
    - δᵢ applies to fresh notes only;
    - the key-off is not spread;
    - today's entry-ramp mode is retired.

    *Recommendation:* yes. Workshop it in B370's envelope-hierarchy lab, with voiceEnv's code as the
    starting point.
