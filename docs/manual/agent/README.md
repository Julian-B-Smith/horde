# horde engine: the operating reference, for agents

You turn a musician's description into a horde patch: every setting named by its key, with a value in its row's unit, valid and complete. This folder is for that job. The musician's manual is the chapters one level up; this layer is not written for them (ROADMAP B629).

**Authority.** `specs/SPEC-ENGINE.md` is the source of every fact here. Where this folder and the spec differ, the spec wins; report the difference.

| file | read it for |
|---|---|
| `README.md` | the patch format, the method, what not to do, how to validate |
| `concepts.md` | what each part does to the sound, with its spec row |
| `parameters.md` | every key: unit, values, law, default, what it does, when it is live. Generated; never edit it |
| `recipes.md` | eight worked translations, each from a bank preset |

## What the engine has today

- **Built:** two swarm oscillators, each with two blades; the sub; the noise oscillator; ENV 1 to ENV 4; the source levels and the output level.
- **Not built:** filters, FX, the modulation matrix (LFOs and every route, envelope routes included), the morph, and the pitch system (glide, bend). They have no keys in `parameters.md`, so no patch can set them. Say so to the musician instead of approximating them.
- **Reserved:** 13 keys hold a place for later features (`parameters.md`, status `reserved`). The engine refuses them in 1.0 (5.1, PA-7).

## The patch format

```json
{ "format": "horde-patch/1", "base": "init",
  "values": { "osc1.unison": 5, "osc1.detune": 0.2, "osc1.blade1.on": "on", "osc1.blade1.mode": "Sync" } }
```

- `base` is `"init"` (every row at its default) or the exact name of a bank preset in `engine2/data/bank/presets.json`, translated by SPEC-ENGINE B.1: osc 1 only, with osc 2, the sub and the noise oscillator off.
- `values` overrides the base, key by key. Name only what you change.
- **A continuous row** takes x, in the numbers its values column states. A `%` row whose range is 0..1 takes 0..1; `oscN.phaseRandom` takes 0..100. A dB row also takes `"-inf"` for silence.
- **A step row** takes one of its option names as a string (`"FM free"`, `"twin −"`), or the option's 0-based index. ASCII forms match: `"x12"` for `×12`, `"sine->saw"`, `"twin -"`.
- **A step row whose options are numbers** (octave, semitone, unison, polyphony, the bump harmonics, the noise's length) takes the number itself, never an index: `"osc1.octave": -1`, `"noise.length": 127`.
- No `true` or `false`: an on/off row takes `"on"` or `"off"`.
- Not expressible in this format: the custom cut-rule list (`ratios`, oscillator state, 5.3), routes, glide.

## The method

1. **Split the description** into character (the steady timbre), motion (what moves while a note holds), shape in time (attack, decay, release) and space (width, stereo).
2. **Pick a base.** Find the nearest bank preset in `recipes.md`, or scan the bank's names and check one with `--resolve`. Use `"init"` when nothing is close.
3. **Oscillator character** (`concepts.md`, The swarm): `oscN.unison`, the detune law and amount, `oscN.coupling` for lock (+0.75 and up), no lock (+0.6 and below) or splay (negative), `oscN.baseWave`.
4. **Blades:** turn one on, choose its mode (Sync, FM, Noise, Fold, Ring, Crush), then cut rate, window and depth. Add mirror, frame, rotation and spreads only for what the description asks.
5. **Motion:** drift, inertia, rotation, the spread law, gravity, onset scatter.
6. **Envelopes and the mix:** ENV 1 for the level; a source on another envelope through its `env`; `mix.*` for the balance; the sub and noise for weight and air.
7. **Validate** (below). Fix every error. Read every warning: an inert row is a setting that does nothing.
8. **Report** what the patch cannot do yet (a filter sweep, a velocity-to-brightness route, glide) in one line.

## What not to do

- Never invent a key. If it is not in `parameters.md`, it does not exist.
- Never set a reserved row.
- Never expect sound from an inert row. A row is live only when PA-8's condition holds: a blade row needs its blade on, a source's rows need the source on, detune and coupling need more than one member, and so on. `patch_check.py` warns on each.
- Never route an envelope to a blade. The bank did this (B.1); routes belong to the modulation system, which is not built.
- Never set a dB row to its −60 bottom expecting −60 dB: the law's v = 0 is silence (5.1).
- Never guess what a setting sounds like beyond the spec's words. Cite `parameters.md` or `concepts.md`.

## Validate

```
python3 tools/patch_check.py my-patch.json             # errors and inert warnings; exit 1 if invalid
python3 tools/patch_check.py my-patch.json --resolve   # every setting that differs from init, inert ones marked
python3 tools/patch_check.py my-patch.json --set       # the page's set message, every value converted to v
```

The `--set` message has the shape `{"type": "set", "seq": 1, "values": {"<key>": v}}`, the GUI 3 transport's (`docs/design/gui3/engine-stub.js`, TRANSPORT): every non-reserved key, so the engine holds the whole patch and nothing from before it.

## Into the test plug-in

Pending: the Import patch box.
