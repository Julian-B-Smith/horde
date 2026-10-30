# S&H clock card

What clocks an S&H: its own LFO's cycle (the ruled default), or one shared clock and one draw broadcast to every attached S&H, or a rotor voice's wrap; with the held values, their sample instants and the rotor.

**The consumer provides**
- `clock` (0 CYCLE, 1 GLOBAL, 2 KURO), `bind` (rotor voice 0–3);
- `sync`, `hz`, `beats`, `bpm` for the clock's RateSync;
- `retrig`, `onRetrig`, `onChange`, `onCommit`, `keys()`.
- Per frame: `push([heldA, heldB, firedA, firedB])` and `draw({ last, attached, rotor: {phase, out} })`.

**Anatomy**
- A `.hd-modcard` with the Mod card header (kind `lfo`, "LFO · S&H", retrigger).
- A 2.2 : 1 glass:
  - A held stepped in `scr-value` and B held stepped in `scr-physics`, both ±1;
  - their sample instants as ticks underneath (A in `scr-marker`, B in `scr-meter`).
- Beside it, the rotor: the LfoRotor painter (`Viz.rotor`) reused, never rebuilt, captioned "KURO's rotor". It is dashed while KURO is not in force.
- Clock: CYCLE · GLOBAL · KURO.
- The clock's RateSync, dashed under KURO, where the rotor clocks it.
- Bind to: a stepper K1–K4, dashed unless KURO.
- Last sampled (7 ch) and Attached (3 ch), fixed widths.
- One reserved line says what the clock means.

**The kinds**
- CYCLE: each S&H samples at its own LFO's cycle start, with its own draw.
- GLOBAL: N attached S&Hs carry one stream between them, so they are perfectly correlated.
- KURO: N streams; coupling locks WHEN they sample, never WHAT they sample.

**Placeholders**
- ADR-266 ruling 6 made S&H an LFO shape, so CYCLE is the ruled clock. GLOBAL and KURO are the superseded lab's clock kinds. Whether either survives as a choice is open; the human rules.
- Retrigger is assumed to restart the clock and take a draw.

**Sources** docs/design/modulator-lab.html :196 (the global clock card), :429 (`GlobalSH`), :438–462 (the two kinds), :148 and :379 (the rotor); ADR-266 ruling 6. The rotor painter is LfoRotor's.
