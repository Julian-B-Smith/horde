# The intent bus and macros

Purpose: explain horde's eight macros. Macros 1–4 are the fixed intents Tone, Space, Time and
Motion, with the same name and meaning in every preset. Macros 5–8 are named by the corners and
flip as units across the morph pad. The chapter covers what each does, how bindings are made
(depth and curve, per corner), how macros reach FX modules' own macros, and how they appear in a
DAW.

## Macros in horde

The eight macro knobs, where they live, and the idea of an intent: a musical direction that every
preset honours.

Status: READY-TO-WRITE — ADR-192

[[FIG:intent.macro-strip]]

## The fixed four: Tone, Space, Time, Motion

What each fixed intent means and which way it turns.

Status: READY-TO-WRITE — ADR-192

[[TAB:intent.fixed-four]]

[[AUD:intent.fixed-four-demo]]

## The named four

Macros 5–8, each a label plus its bindings, owned by a corner.

Status: READY-TO-WRITE — ADR-192

### Labels and the phrase list

How a named macro's label is chosen from a set list of phrases.

Status: PENDING — ADR-192, B443

[[TAB:intent.phrase-list]]

### Flips across the morph pad

Each named slot flips at its own point; the knob stays where it is and the incoming bindings take
over smoothly.

Status: READY-TO-WRITE — ADR-192

[[FIG:intent.named-flip-map]]

### Same phrase, continuous morph

Corners that share a phrase keep it in one slot, so it morphs continuously instead of flipping.

Status: READY-TO-WRITE — ADR-192

### Offset from rest

A named macro moves a parameter away from the current corner's own value, within that corner's
range.

Status: READY-TO-WRITE — ADR-192

[[FIG:intent.offset-from-rest]]

## Binding a macro

Choosing what a macro moves, how far, and along which curve, per corner.

Status: PENDING — B270, B354

[[FIG:intent.binding-editor]]

### Binding curves

The linear, exponential and logarithmic curves a binding can follow.

Status: READY-TO-WRITE — ADR-169

[[FIG:intent.binding-curves]]

[[TAB:intent.curve-laws]]

## Macros and FX modules

FX modules may carry up to eight macros of their own; horde binds nothing automatically, and a
designer binds horde's intents to module macros or parameters, per corner.

Status: READY-TO-WRITE — ADR-169, ADR-192

[[FIG:intent.module-macro-binding]]

## Macros in your DAW

How the macros are named for automation: the fixed four by name, the named four as Macro 5–8.

Status: READY-TO-WRITE — ADR-192

[[TAB:intent.daw-names]]
