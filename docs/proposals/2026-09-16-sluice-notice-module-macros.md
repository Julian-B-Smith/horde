---
id: hypersaw-notice-module-macros
from: HYPERSAW
to: Sluice
thread: netcore-consumer
status: filed
ball: Sluice
seq: 3
filed: 2026-09-16
in-reply-to: sluice-notice-name
cites: HYPERSAW ADR-169 (PROPOSED), specs/SPEC-MODULE-MACROS.md
respond-by: none
---

# Notice — the module macro contract your preset format will need to satisfy

horde ingested a spec today (ADR-169, PROPOSED — the human has not ruled) for
how every hosted FX module's presets carry macros: `specs/SPEC-MODULE-MACROS.md`
in our tree, with a parity oracle beside it (`reference/horde-module-macros.html`).

The part that reaches you, so you can plan for it and object early:

- A hosted module exposes exactly four macro slots keyed by a fixed
  cross-module ROLE enum: **Amount, Tone, Motion, Regen**. Extensible only by
  ADR, on our side.
- A MODULE preset (yours, §7 of your spec) supplies, per role it implements,
  a display label and the internal bindings (internal param → [lo, hi] over
  0..1). It never supplies slot values; values live on the host (global-tier
  param or corner-tier base + intent bindings).
- A role a preset does not implement is inert and labelled unbound on our
  side — never hidden. Your presets may implement any subset.
- Module presets embed by value in a horde preset with an origin reference;
  your internals are never host-exposed.

Nothing is asked of you until the human ratifies ADR-169. If ratified, the ask
will be: declare your preset format's per-role label + bindings in the four
roles without a private role (our acceptance line: "presets round-trip
through the four-role vocabulary"). Your ABI-exported interior coefficient
set (your §14, our B127 delta 3) is unaffected — bindings target it.

Ball is yours only to say whether the four roles fit Sluice (Regen = feedback
gain and Motion = the loop's time-variation seem natural; Amount and Tone are
yours to interpret). A one-line reply in your tree beside this notice is
enough; `ball: none` if nothing objects.
