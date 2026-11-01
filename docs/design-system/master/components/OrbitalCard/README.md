# ORBITAL card

The ORBITAL modulator's card: an N-body gravity field whose bodies are sources, with its presets, actions and field controls, and its mappings, each continuous or sample-and-hold on its own rate and sync (ADR-266 ruling 6).

**The consumer provides**
- `field`: the field's state and writers:
  - `nodes [{x, y, m, pinned}]` (sim coordinates, y down);
  - `tr` (the trail frames);
  - `P {G, eps, ts, damp, smooth, kick, kickBody, bounds, com}`;
  - `obs(b, q)` (q: x, y, speed, near, angle; 0..1);
  - `load(name)`, `add()`, `reset()`, `kick()`, `steps`, `energy()`.
  The preview's `Horde.Synth.Orbital` is a seeded stand-in transcribed from the lab.
- `maps [{ b, q, dest, mode: 'cont'|'sh', sync, hz, beats }]`.
- `read(i)` → `{ src, out, tick }`: what was rendered for mapping i. The card never recomputes it.
- `bpm`, `retrig`, `onRetrig`, `onChange`, `onCommit`, `keys()`.
- Call `log()` once per rendered frame and `draw()` to paint.

**Anatomy** a `.hd-modcard` with:
- the Mod card header (kind `lfo`, "MOD", GLOBAL, retrigger);
- a 2.3 : 1 glass:
  - left, the field square with an 8 × 8 grid. Trails are `scr-physics` at 35%, broken at a wrap or a bounce.
  - Bodies: radius 3 + 3√m with a bloom. A pinned body is a ring. A body that a mapping reads is in `scr-meter`, the others in `scr-physics`.
  - Each body's x and y observables are ticks on the bottom and left edges.
  - Right, one lane per mapping: the source thin in `scr-physics`, and the mapping's output in `scr-meter`. Under S&H the output is stepped, with its clock's ticks in `scr-marker`.
  - Bottom right: t, E and the body count.
- Field: BINARY · SUN + PLANETS · FIGURE EIGHT · FOUR-BODY CHAOS.
- Act: ADD BODY (seeded by seed and add count; 8 at most), RESET, KICK.
- Sliders:
  - Gravity G 0.001–1 (log);
  - Soften ε 0.005–0.5 (log);
  - Time × 0–4;
  - Drag 0–1;
  - Smooth 0–50 ms;
  - Kick 0–1 u/s.
- Edges BOUNCE · WRAP · OPEN; a COM lock switch; a Kick body `Horde.Stepper` (one option per body, "body 3"; remounted when bodies are added).
- Mappings: each row is "A · body 1 x → Cutoff", then CONT · S&H, then its own RateSync. The RateSync keeps its place under CONT, with dashed faces.

**Gestures** the house rules throughout. The sliders take 160 px for the full range; every action and mode change is one undo step.

**Screens** six screen roles; it follows the Screen scheme. **Reduced motion** one still frame, redrawn on an edit.

**Placeholders**
- Retrigger is assumed to be a reset to the initial conditions on note-on.
- How the S&H mode samples is assumed: the held value is the source at the clock's wrap, with no smoothing.
- How many bodies and observables are exposed is B126's open collision (the lab exposes bodies 1 to 4).
- The normalisation of speed and energy is the lab's round 2, question 6.
- `accel` is not built.

**Sources** docs/design/shape-lab-mod.html :531 (the card), :1025 (`orbPreset`), :1040 (`OrbitalField`), :2397 (`buildOrb`), :2418 (`paintOrb`); ADR-165; ADR-266 ruling 6.
