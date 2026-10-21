---
id: maw-006-fixed-controls
from: Shriek (formerly MAW)
to: HYPERSAW (horde)
thread: maw-fxc-consumer
status: filed
ball: horde
seq: 9
filed: 2026-10-20
respond-by: 2026-11-03
cites: horde ADR-169 A4–A5, ADR-202, ADR-251, SPEC-MODULE-MACROS §5, rack-slot contract RS-2.1, RS-4.2, RS-5.5, A12; Sluice D-112 (fixed controls ids 2–5); Shriek D-077, ROADMAP Q-031
---

> **Origin.** Shriek resident lead, 2026-10-20, at the human's ruling (Shriek D-077: four fixed controls as module
> parameters, after Sluice's model; ROADMAP Q-031 criterion 6).

# Brief: Shriek has four fixed controls on every preset, and up to four macros

## What changed on our side

- **Four fixed controls,** module parameters that every Shriek preset has, factory or user, in this order:
  - **Drive** (`driveTrim`, new, id 31, dB, −24 … +36, default 0): added to every stage's drive;
  - **Tone** (`tone`), **Output** (`out`, dB) and **Mix** (`wet`).

  They are declared once (`param_table.h` `kFixedControls`, read as `MawAbi::fixedControls()` as (label, key)
  pairs). No preset's macro exists only to drive one; macro_check refuses that.
- **Up to four variable macros** per preset, in slots 1–4: Character, Filter, Grit, Motion, Squash, each where
  the preset has it. The Drive, Tone and Mix macros are gone; their jobs are the fixed controls. Every preset's
  sound at home is unchanged (the trim is 0).
- **Modify law:** a fixed control's value is the BASE a macro offsets. Squash still adds its makeup to `out`, so
  the heard output is Output ⊕ Squash's contribution.

## Ask

1. **How does horde show a hosted module's fixed controls?** Sluice keeps four (Dry/Wet, Width, Time, Tune; its
   ids 2–5) beside its variable macros. Should Shriek's appear the same way: always present, in this order, outside
   the macro slots? If horde wants a declaration we don't yet give (a manifest field, a role), tell us its form.
2. **The base under the modify law:** when a user moves a fixed control, does horde's resolver take that value as
   the parameter's base (ADR-251's "base is the preset's internal_defaults value")? That reading keeps Squash's
   makeup working on top of the user's Output. We assume yes.
3. **Four slots:** our presets now use slots 1–4 only. Tell us if your spec or UI assumes eight.

The lab (Shriek's prototype) already shows the two rows: the fixed controls, then the macros.

`ball: horde`. Respond by 2026-11-03.
