# Ownership tier

The modulator ownership tier, made concrete: which modulators are baked (their identity is fixed by where they come from), derived (a function of another), or modifiable (their own parameters, stored in the preset), and what a toggle refuses or removes.

**The consumer provides** `rows [{ id, kind, tier: 'baked'|'derived'|'mod', from }]`, where `from` says where a fixed identity comes from, plus `onChange(rows)`, `onCommit` and `onRefuse(row)`.

**Anatomy**
- A table, one 28 px row per modulator:
  - the kind swatch and the name (mono 500 10 px, `ink`);
  - the tier chip, 92 px, capsule:
    - modifiable: `panel` ground with an `ink` edge;
    - baked: `well-alt` ground with `ink` text;
    - derived: a dashed edge with `t2` text;
  - "identity comes from" (`t2`, ellipsis);
  - "In preset" (yes or no).
- A message line (`hd-msg`, two lines reserved).

**Gestures**
- Click a chip.
- A baked or derived row REFUSES, says why ("K2: refused. Its identity is the rotor (rate = detune spread): there is no knob to unlock."), and changes nothing.
- A modifiable row toggles to baked and its parameters leave the preset; it toggles back.

**Placeholders** the question is open (B16), and its three readings are not ruled:
- (i) tier as editability: the refusal is built;
- (ii) tier as preset ownership: "In preset" is built;
- (iii) tier as precedence between writers: not built, deliberately.

**Sources** docs/design/modulator-lab.html :268 (the card), :503 (`SRC`), :874 (`buildTier`), :883 (the toggle).
