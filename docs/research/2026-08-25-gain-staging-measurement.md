# Gain staging measured — why it is quiet, and why that is not a missing-gain problem

**Date** 2026-08-25 · **Trigger** human: *"I feel like the synth is in general a
little too quiet on many settings."* · **Instrument** `tools/gain_probe.cpp`
(links the real plugin static lib, drives real `process()`, measures peak and
RMS dBFS over the second second of a held note).

The feel is correct. The cause is not the one the phrasing suggests, and the fix
is a ruling on a protected path rather than an edit.

## Control first

| case | peak dB | rms dB |
|---|---|---|
| `vol = 0` (must read silence) | −240.00 | −240.00 |

The probe reads zero when it should. Every number below is therefore measuring
the instrument and not the meter.

## The headline

| case | peak dB | rms dB |
|---|---|---|
| **shipped default, 1 osc, 1 note** | **−10.94** | **−21.39** |
| 4-note chord, default | −4.37 | −14.67 |
| 2 osc, both at default `vol` | −4.92 | −15.37 |
| 2 osc, both at `vol = 1` | **+1.91 — clipping** | −7.71 |
| everything up (`vol 1`, master 1.5, n 32, K 1) | **+1.88 — clipping** | −1.51 |

**The instrument is not short of gain.** A four-note chord at the shipped
default already peaks at −4.4 dBFS, and two oscillators at full `vol` clip. What
is quiet is one specific condition: **a single oscillator holding a single note
at the shipped default** — which is exactly the condition under which a patch is
auditioned while it is being designed. The default reserves ~11 dB of headroom
for polyphony and a second oscillator that a held single note never spends.

## Where the level goes — three factors, all inherited verbatim

All three are the JS reference's, at `reference/swarmsaw.html:210` (the defaults object)
and `reference/swarmsaw.html:583` (`const gain = p.vol * 0.9 / Math.pow(n, p.normExp)`),
mirrored at `src/swarm_core.h:143,796`:

| factor | cost at the default patch | note |
|---|---|---|
| `vol = 0.4` | **−7.7 dB** (measured: −21.39 → −13.73 at `vol = 1`) | a knob the player can already turn up |
| `normExp = 0.75` | **−4.6 dB at n = 7**, growing to −9.7 dB at n = 32 | mistuned — see below |
| `* 0.9` | −0.9 dB | fixed headroom trim |

## The finding worth acting on: `normExp = 0.75` is right at neither end

`normExp` is the exponent in the density compensation. The correct value is not
a constant — it depends on whether the swarm is summing **coherently** (locked,
amplitude ∝ n, correct exponent 1.0) or **incoherently** (splayed, amplitude
∝ √n, correct exponent 0.5). Measured, both ends:

| n | RMS at `normExp = 0.5`, K = 0 | RMS at `normExp = 1.0`, K = 1 |
|---|---|---|
| 1 | −16.83 | −16.83 |
| 4 | −16.70 | −18.54 |
| 7 | −17.26 | −18.66 |
| 16 | −16.58 | −18.64 |
| 32 | −19.07 | −18.65 |

Both predictions hold. At K = 0 the level is flat within **0.7 dB across n = 1…16**
when the exponent is 0.5; at K = 1 it is flat within **0.15 dB across n = 4…32**
when the exponent is 1.0. The exponent is not arbitrary — each end has a right
answer, and 0.75 is neither of them.

**The shipped default is K = 0** (`hypersaw_clap.cpp:138`), the splayed end where
0.5 is correct. So the shipped configuration runs the compensation 0.25 too high,
and the error grows with voice count:

| n at K = 0 | RMS | vs n = 1 |
|---|---|---|
| 1 | −16.83 | — |
| 4 | −19.63 | −2.8 dB |
| 7 (shipped) | −21.39 | **−4.6 dB** |
| 16 | −22.43 | −5.6 dB |
| 32 | −26.48 | **−9.7 dB** |

### Why this is a bug in kind and not only in degree

**The level moves the wrong way against the voices knob.** At the shipped K,
adding voices makes the instrument *quieter* — the opposite of every other
synth's stack control, and the opposite of what the knob's own name implies.
Meanwhile adding *notes* makes it louder. So there is no single `vol` setting
that is right: the player raises `vol` to compensate for a splayed 16-voice
patch, then plays a chord and clips. That is the mechanism behind "quiet on many
settings" — not a constant offset but a level that drifts under two controls in
opposite directions.

At moderate K the coupling axis itself barely moves the level (K = 0 → 0.5 spans
0.6 dB at n = 7); the K dependence only appears above ~0.75. The voice-count axis
is the one that bites.

## What can and cannot be changed

**No golden sets `vol` or `normExp` explicitly** — verified against
`tools/golden/gen_goldens.mjs`; every case including `{ name: 'defaults', p: {} }`
inherits them from the reference. So changing either default is a **reference
change on a protected path** (`reference/swarmsaw.html`), requires an ADR, and regenerates
every golden. It is a human gate, not an optimisation commit.

### Options, ranked

1. **Available today, zero code.** `normExp` is already exposed as the *Density
   Comp* knob, range 0.5–1. Setting it to 0.5 buys **+5.9 dB at n = 16** and makes
   level voice-count-invariant when splayed. If the factory patches ship at 0.5
   instead of 0.75, most of the complaint goes away without touching the core.
   **This is the cheapest real answer and it is a preset decision, not a DSP one.**

2. **Move the reference defaults** (`vol` 0.4 → ~0.7, `normExp` 0.75 → 0.5). The
   most direct fix, and the most expensive: protected-path edit, ADR, full golden
   regeneration. Note the headroom check above — at `vol = 0.7` a two-oscillator
   patch would sit near clipping, so this cannot be done on `vol` alone without
   also deciding the polyphony headroom budget.

3. **An AUTO density comp, parity-safe as a superset.** The order parameter R is
   *already computed* every control tick, and |Σe^{iθ}| = n·R **exactly** — so
   dividing by n·R is correct at both ends by construction, with no exponent to
   choose. Inert if it is a new value on the existing knob and the default stays
   0.75. **Caveat, stated because it is not yet evidence:** R measures
   *fundamental-phase* coherence, while the audible sum is a saw whose harmonics
   each have their own coherence. It should behave well but this is reasoning, not
   a measurement, and this repo's history says measure first.

   **Interaction to flag:** B41 audit finding 3 proposes *skipping* the
   order-parameter trig at K = 0 to recover 15–20% of the render bill. If R becomes
   load-bearing for gain, that option closes. The two decisions are coupled and
   should be made together.

## Falsifier

If a listening test finds the default patch subjectively fine at −21.4 dBFS RMS
and the complaint traces to something downstream (a host gain-staging habit, the
FX rack, the mixer defaults), then the analysis above is measuring a real number
that is not the cause, and option 1 should be tried before anything else is
considered. Likewise, if setting *Density Comp* to 0.5 does **not** audibly fix
the "quiet with many voices" case, the exponent diagnosis is wrong despite the
flat curves, and the deficit is elsewhere in the chain.
