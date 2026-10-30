# Mod card header

The header every modulator card shares: the kind swatch, the modulator's name and number, its kind and scope tags, and the retrigger switch (ADR-266 ruling 7).

**The consumer provides** `name` with its number ("LFO 1", "ENV 2"); `kind` (`lfo`, `env`, `macro` or `engine`), which picks the colour; an optional `kindLabel` and `scope` ("GLOBAL", "PER VOICE"); `retrig` (true or false shows the switch, null leaves it out); `onRetrig(on)` and `onCommit`; and `retrigHint`, one line on what a note-on does to this modulator.

**Anatomy** (on a `.hd-modcard`: `panel`, 1.5 px `ink` edge, `radius`, 6/10/10 px padding)
- 28 px row (`size-hit`), 7 px gaps, wrapping only with the width.
- Kind swatch: 10 × 10, radius 3, `src-<kind>` with a 1 px `mark-edge` ring.
- Name: Archivo 800 11 px, tracking 0.1 em, uppercase, `ink`, ellipsis at 16 ch.
- Kind tag: Archivo 700 8.5 px in `src-<kind>`, 1 px edge in the same colour, `radius-track`. The kind colour is text here only on `panel` (4.85:1 or more); never put it on `bg`, where `src-env` reads 4.34:1.
- Scope tag: the house `.vtag`.
- Retrigger: the Switch toggle (`hd-toggle`, `role="switch"`, 32 × 17 face, 28 px hit) behind a "Retrig" label, aria-label "<name> · Retrigger".

**Rules**
- Colour comes by kind only (ADR-266 ruling 5). Two modulators of one kind share the colour and differ by name and number.
- Every modulator card carries the switch: LFO, envelope, the S&H clock, ORBITAL.
- The MIDI trackers card has none: its trigger is the source of retriggering, and it carries the EVERY / FIRST mode instead.

**Gestures** click toggles; one click is one undo step. The switch is a tab stop like any button.

**Placeholders**
- What retrigger does per kind is assumed from the lab and has not been ruled:
  - LFO: a note-on starts a new cycle at φ, which is the next unused draw for S&H and RANDOM.
  - Envelope: ON restarts from the level it has reached; OFF is legato.
  - S&H clock: restarts the clock and takes a draw.
  - ORBITAL: resets the field to its initial conditions.
- That the trackers card carries no switch is assumed. The human rules both.

**Sources** docs/design/shape-lab-mod.html :381 (LFO `retrig` FREE-RUN / ON NOTE), :595 (round 2, question 2: the trigger as a retrig); ADR-266 ruling 7.
