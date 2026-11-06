# horde engine: recipes, for agents

Eight worked translations from a description to a patch. Each starts from a bank preset (`engine2/data/bank/presets.json`, translated by SPEC-ENGINE Appendix B.1), changes what the description asks for, and says why.

- **Set here** lists every value the patch writes, one line each.
- **From the base** lists the base's settings that carry the description, so you can see why that preset was chosen. Run `python3 tools/patch_check.py <patch> --resolve` for every setting a base brings.
- Every line of the form `` `key` = value `` is checked against the resolved patch by `tools/agent_manual_check.py`, and every patch here validates with no warning.
- B.1 puts each preset on osc 1 alone, with osc 2, the sub and the noise oscillator off. Its glide is the pitch system's (RC-3) and is not in the patch.

## 1. Pad: wide, slowly beating, with a glassy top

> "A wide, slowly beating pad with a glassy top that blooms when I hit harder."

Base: **Glass horde pad** (Showcase / Pads).

```json
{ "format": "horde-patch/1", "base": "Glass horde pad",
  "values": { "osc1.detuneLaw": "Hz", "osc1.detune": 0.05 } }
```

Set here:
- `osc1.detuneLaw` = "Hz": the Hz law gives "the same beat everywhere" (5.3), so the beating is as slow high up as low down.
- `osc1.detune` = 0.05: under the Hz law a member at placement x sits x·detune·range·20 Hz from the pitch (C.3); with even placement the outermost members are at x = ±1 (C.2), so ±1 Hz.

From the base:
- `osc1.distribution` = "even": the placement the line above relies on.
- `osc1.unison` = 9: nine members (SW-1).
- `osc1.width` = 1: the members spread across the whole stereo field (5.3).
- `osc1.coupling` = 0.35: under SW-4's lock anchors (+0.75 locks, +0.6 does not), so the members keep beating.
- `osc1.blade1.cutIn` = "Hz": blade 1's carrier is a fixed frequency; blade 2's `cutIn` is "as blade 1", so its carrier is fixed too (5.4, PA-8).
- `osc1.blade2.mode` = "Sync": hard sync inside blade 2's window (BL-2).
- `osc1.blade2.wave` = "sine→saw": the carrier's wave, morphed by `shape` (BL-3).
- `osc1.blade2.cutHz` = 2200: a 2200 Hz carrier that does not follow the note, a formant (5.4): the bright top.
- `env1.attack` = 0.9: 0.9 s to full level.
- `env1.release` = 5.756463: B.1's 2500 ms release, scaled by ln(10⁴)/4.

Not possible today: a top that opens with velocity needs a velocity route, and routes are the modulation system's, not built (RC-4). Velocity scales the level only:
- `env1.velocity` = 1: level proportional to velocity (VO-8).

## 2. Pluck: short and hollow

> "A short, hollow pluck that dies away even while I hold the key."

Base: **Hollow twin** (Starting points).

```json
{ "format": "horde-patch/1", "base": "Hollow twin",
  "values": { "env1.sustain": 0, "env1.decay": 0.5, "env1.release": 0.3 } }
```

Set here:
- `env1.sustain` = 0: the decay falls all the way to silence, so a held key does not sustain (5.2).
- `env1.decay` = 0.5: the decay ends exactly 0.5 s after full level (5.2, EN-1).
- `env1.release` = 0.3: a short tail after key-up.

From the base:
- `env1.decayCurve` = 2.302585: the bank's exponential fall (5.2).
- `osc1.baseWave` = "sine": a half-wave-symmetric base.
- `osc1.blade1.mirror` = "twin −": on a sine base, twin − removes the even harmonics: the hollow tone (BL-6).
- `osc1.blade1.mode` = "Sync": hard sync in the window (BL-2).
- `osc1.blade1.cut` = 7: the carrier runs at 7 × the member's frequency (BL-4).
- `osc1.unison` = 3: three members.
- `osc1.detune` = 0.1: the outermost members at ±10 c (SW-3).

## 3. Bass: mono, solid sub, folded top

> "A mono bass: a solid sine sub under a folded, gritty top."

Base: **Folded slice** (Starting points).

```json
{ "format": "horde-patch/1", "base": "Folded slice",
  "values": { "voice.mode": "mono", "sub.on": "on", "sub.wave": "sine", "mix.sub": 0 } }
```

Set here:
- `voice.mode` = "mono": one voice; every note restarts its envelope (5.2, VO-6).
- `sub.on` = "on": B.1 leaves the sub off.
- `sub.wave` = "sine": a clean fundamental (SN-2).
- `mix.sub` = 0: the sub at 0 dB, up from its −2 dB default.

From the base:
- `sub.octave` = -1: the sub sits an octave under the note (SN-1).
- `osc1.blade1.mode` = "Fold": the window's input is folded by sin(π/2·(1 + (k − 1)/4)·x) (BL-2).
- `osc1.blade1.cut` = 14: k = 14, so the fold's gain is π/2 × 4.25 (BL-2).
- `osc1.blade1.width` = 0.5: the fold covers half of each cycle.
- `osc1.baseWave` = "sine": a smooth wave for the fold to bend.
- `osc1.coupling` = 0.6: no sustained lock (SW-4), so the three members are not held to one pitch.

## 4. Lead: focused, hollow, legato

> "A focused, hollow square lead, played legato, that wanders slightly in pitch."

Base: **Hollow square lead** (Showcase / Leads).

```json
{ "format": "horde-patch/1", "base": "Hollow square lead",
  "values": { "osc1.driftDepth": 6 } }
```

Set here:
- `osc1.driftDepth` = 6: each member wanders up to 6 c (SW-7, C.4).

From the base:
- `voice.mode` = "legato": the envelope restarts only when no key is held (5.2, VO-6). Glide is the pitch system's and not built (RC-3).
- `osc1.coupling` = 0.8: above SW-4's +0.75 anchor, a steady lock to one pitch: the focus.
- `osc1.baseWave` = "square": a half-wave-symmetric base.
- `osc1.blade1.mirror` = "twin −": on a square base, removes the even harmonics: hollow (BL-6).
- `osc1.blade2.on` = "on": a second blade.
- `osc1.blade2.mode` = "Ring": blade 2 multiplies its input by its carrier (BL-2).

## 5. FM bell: glassy, shimmering, long ring

> "A glassy FM bell, slightly detuned so it shimmers, with a long ring."

Base: **FM bell** (Showcase / FM sines).

```json
{ "format": "horde-patch/1", "base": "FM bell",
  "values": { "osc1.unison": 2, "osc1.detune": 0.04, "env1.release": 6 } }
```

Set here:
- `osc1.unison` = 2: a second member. The base has one, so detune and coupling were inert (PA-8: U > 1).
- `osc1.detune` = 0.04: cents law, even placement: the two members at ±4 c (SW-3).
- `env1.release` = 6: the ring after key-up lasts 6 s.

From the base:
- `osc1.coupling` = 0.5: now live; under SW-4's lock anchors, so the two members beat.
- `osc1.blade1.mode` = "FM free": FM with a free-running modulator (BL-2).
- `osc1.blade1.wave` = "sine": a sine carrier.
- `osc1.blade1.cut` = 1: the carrier at the member's own frequency (BL-4).
- `osc1.blade1.fmDepth` = 2.4: the FM index (BL-5).
- `osc1.blade1.modRate` = 3.5: the modulator at 3.5 × the member's pitch (BL-5).
- `env1.sustain` = 0: no sustain; the note always decays.
- `env1.decay` = 5.986721: B.1's 2600 ms decay, scaled by ln(10⁴)/4.

Not carried: this preset's blade envelope, ENV 2 routed to blade 1's width, is a route of the modulation system (B.1, RC-4).

## 6. Noisy texture: breath through a buzz

> "An airy, breathy texture: noise blowing through a buzzing tone, wide."

Base: **Breath and bite** (Two-blade).

```json
{ "format": "horde-patch/1", "base": "Breath and bite",
  "values": { "noise.on": "on", "noise.whisper": "on", "noise.vowel": 0.5, "noise.wind": "on",
              "noise.gust": 0.6, "noise.spread": 0.7, "mix.noise": -6 } }
```

Set here:
- `noise.on` = "on": B.1 leaves the noise oscillator off.
- `noise.whisper` = "on": three vowel formants on the noise (SN-9).
- `noise.vowel` = 0.5: 4 × 0.5 = 2 on the a–e–i–o–u sweep: "i" (SN-9).
- `noise.wind` = "on": a band that wanders like wind (5.6).
- `noise.gust` = 0.6: how far and how fast the band wanders (5.6).
- `noise.spread` = 0.7: the noise's two channels part (SN-12).
- `mix.noise` = -6: the noise 6 dB up from its −12 dB default.

From the base:
- `osc1.blade1.mode` = "Noise": held random values inside blade 1's window (BL-2).
- `osc1.blade1.cut` = 40: the held value changes at 40 × the member's frequency (BL-2, BL-4).
- `osc1.blade1.width` = 0.08: a narrow window.
- `osc1.blade1.depth` = 0.55: blends the noise with the base wave (C.9).
- `osc1.blade2.mode` = "Sync": blade 2 hard-syncs: the buzz.

## 7. Sync scream: wide and unstable

> "A screaming, unstable sync lead, wide, played legato."

Base: **Feedback screech** (Starting points).

```json
{ "format": "horde-patch/1", "base": "Feedback screech",
  "values": { "osc1.unison": 5, "osc1.detune": 0.15, "osc1.width": 1, "voice.mode": "legato" } }
```

Set here:
- `osc1.unison` = 5: five members.
- `osc1.detune` = 0.15: even placement, cents law: outermost at ±15 c (SW-3).
- `osc1.width` = 1: the members across the whole field (5.3).
- `voice.mode` = "legato": one voice; held-note changes do not restart it (VO-6).

From the base:
- `osc1.blade1.mode` = "Sync": hard sync at each window entry (BL-2).
- `osc1.blade1.wave` = "sine": a sine carrier.
- `osc1.feedback` = 0.75: each member's previous output bends its own carrier phase (BL-12).
- `osc1.loopDrive` = 1: above the sine carrier's stable limit of about 0.42, so the loop is chaotic where the window is long enough, each setting its own noise (5.3, BL-12).
- `osc1.coupling` = 0.35: now live with five members; no lock (SW-4).

## 8. Drone: slowly turning and evolving

> "A drone that slowly turns and evolves, with a low sine sub."

Base: **Orbiting cuts** (Showcase / Movement).

```json
{ "format": "horde-patch/1", "base": "Orbiting cuts",
  "values": { "env1.attack": 4, "env1.release": 8, "osc1.driftDepth": 12, "osc1.driftRate": 0.1,
              "osc1.blade1.rotate": 0.05, "osc1.spreadLaw": "drift", "osc1.spreadDrift": 0.05,
              "sub.on": "on", "sub.wave": "sine", "mix.sub": -6 } }
```

Set here:
- `env1.attack` = 4: a 4 s swell.
- `env1.release` = 8: an 8 s tail.
- `osc1.driftDepth` = 12: each member wanders up to 12 c (SW-7).
- `osc1.driftRate` = 0.1: the walk's rate is 0.2 + 8 × 0.1 = 1.0 (C.4).
- `osc1.blade1.rotate` = 0.05: the window turns around the cycle at 0.05 Hz, once in 20 s (BL-8).
- `osc1.spreadLaw` = "drift": the spread values wander instead of holding (C.10).
- `osc1.spreadDrift` = 0.05: the wander's rate, in Hz (C.10 item 3).
- `sub.on` = "on": B.1 leaves the sub off.
- `sub.wave` = "sine": a plain low fundamental (SN-2).
- `mix.sub` = -6: the sub under the swarm.

From the base:
- `osc1.blade1.rotateSpread` = 0.5: fans the rotation rate across the members, so the windows drift apart (5.4).
- `osc1.crossMod` = 0.25: each member's previous output bends the next one's carrier phase (BL-12).
- `osc1.loopDrive` = 1: B.1 gives every bank loop 1; with a reverse saw carrier the loop is stable nearly throughout (5.3, BL-12).
- `osc1.blade1.wave` = "reverse saw": the carrier's wave.
- `osc1.unison` = 6: six members.
