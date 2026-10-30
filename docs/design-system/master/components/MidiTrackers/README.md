# MIDI trackers

The MIDI trackers card: the note-on gate and trigger and six trackers, each a source with its range, its rule and its live value.

**The consumer provides** `cfg { mode (0 EVERY, 1 FIRST), orderN (2–8), sinceT (s) }`, which the card edits; `read()` → `{ values {gate, note, vel, rel, held, order, since}, fired }`, from the host's tracker (the preview's `Horde.Synth.Tracker`, with the lab's rules); `keys()`; `onChange` and `onCommit`.

**The sources** (global scope: one per instrument, last-note priority)

| Source | Range | Rule |
|---|---|---|
| GATE | 0 / 1 | 1 while any key is held; legato: it never dips and never re-attacks |
| TRIGGER | event | EVERY fires on each note-on; FIRST fires only on the one that opens the gate |
| NOTE | ±1 | (n − 60) ÷ 48, last-note priority, holds |
| VELOCITY | 0..1 | the NOTE key's velocity |
| REL VEL | 0..1 | the latest note-off's velocity, held |
| HELD | 0..1 | keys held ÷ 8 |
| ORDER | 0..1 | the k-th note since the gate opened, wrapping at N |
| SINCE | 0..1 | seconds since the TRIGGER ÷ T, counted in samples; 1 before the first |

An event is not a level: the TRIGGER reaches a level only through an action (Event lanes).

**Anatomy**
- A `.hd-modcard`. Its Mod card header uses kind `engine` and "TRACKERS", and has NO retrigger switch: this card is the trigger's source.
- Trigger: EVERY NOTE-ON · FIRST · LEGATO.
- Order N stepper; Since T, 0.1–8 s (log, the house slider).
- The source table:
  - mono 9.5 px, columns 72 px / 44 px / the rule / 9 ch;
  - the rule column ends in an ellipsis, with the full text in its title;
  - the live "now" column is fixed-width, tabular and fixed-format, so the table never moves.

**Placeholders**
- The ranges are stated, not ruled: NOTE's scale and priority, ORDER's wrap, and SINCE resetting on the trigger.
- Which trackers get a per-voice twin, a release trigger, and whether the sustain pedal holds the gate are all open (the lab's round 2, questions 3 to 5).
- That this card carries no retrigger switch is assumed.

**Sources** docs/design/shape-lab-mod.html :547 (the card), :1200 (`NoteTracker`), :1241 (`PHRASE`), :2471 (`MIDI_ROWS`), :2482 (`buildMidi`).
