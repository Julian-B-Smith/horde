# What the GUI rulings need under the hood

The GUI session's handoff to the lead (GB8; the human, 2026-11-04: "let's make sure the main horde agent knows about any decisions we've made during the design process that will affect functionality and need to be wired in under the hood as well").

**Last verified 2026-11-04, against GUI-1 to GUI-20, `specs/SPEC-ENGINE.md` and `ROADMAP.md` on main.** The GUI session keeps this current: each new ruling that needs wiring adds its row in the same PR. "Not recorded" means a search of the spec and the roadmap found nothing, so it's the lead's to place. The rows are the lead's to schedule (B616 cites this file), and nothing here is a ruling on how to build them.

**Timing (the lead, 2026-11-04).** The morph comes straight after the CPU pass, and the spec has no waypoints or region locks. If the design rests on them, they need a spec round before the builder starts the morph, and that round waits on the human's answers to the two questions under "Open for the human".

A line like GUI-14 r3 is a ruling in `DECISIONS.md`. Every row below is MOCK, a stand-in or refused in the shell today, as its third column says.

## Not recorded for the engine side

These are the ones nothing on the engine side knows about yet.

| Need | Decided by | The shell today |
|---|---|---|
| A solo tap at every stage: plays that stage's output directly, one at a time, combinable with a source's solo, still through the limiter. A new route to the limiter. | GUI-14 r4, GUI-18 | The stage's S has no effect on any audio |
| A bypass key per FX slot. See the first conflict below. | GUI-14 r3, GUI-18 | BYP is drawn on the console; mock |
| The rack's cable state published to the view, for the new rack (only legacy B265 has anything like it). | GUI-14, GUI-18 | A serial stand-in |
| A summed-output signal for the foot row's scope and spectrum. IF-10 is per voice and per oscillator; RC-8 has peaks only. | GUI-8 | `dev.summed()` is mock |
| A Control route stores a sweep: a low end and a high end. Option+Shift-drag moves the high end. | GUI-17 | Held in the shell's mapping band |
| Row-level region locks in the morph: a lock keeps the row's own four offsets. What a lock stores is open. | GUI-20, B269 | Held in the device stand-in, with undo |
| Waypoints: pins per row, in plugin state, with per-row and per-patch caps and the TPS·Δ or TIN·Δ law per patch. The spec has no waypoints; B235 and B272 are legacy rows. | GUI-20, B272 | Held in the device stand-in with undo, drawn but not heard |
| The corner import list: each corner's global parameters, their eligibility, an import button. | ADR-192 item 4, GUI-19 | Not built |
| A device preset seam: list, load and save global presets, and read the legacy-format factory bank. Double-click's home is the loaded preset's value, and both GUI-12 questions are open: does a save move home, and is a corner's saved preset its home? | GUI-6, GUI-12 | Control is mock; home is the default |
| The XY assignment and the routes save with the patch. IF-4 and RC-11.1 have no state member for them. | GUI-2 r2, GUI-9 r1 | In the device's state, for the session |
| Whether X and Y (and the intents) are host parameters: "the lead's and the human's call". | GUI-9 | Not host parameters |
| `out.level`'s label becomes SUM · VOICES; the key stays. | GUI-14 r6, GUI-18 | The shell uses the word |
| Notes reaching the GUI, for Arp's piano roll and pictures. | GUI-11 | A sample chord at 120 BPM |

## Partly recorded

| Need | Decided by | The shell today | Recorded where |
|---|---|---|---|
| A tap, peak, clip latch and gain at every stage, for the console and its lamps. | GUI-14, GUI-18 | Lamps latch on a model, 24 dB under the stub's tone | RC-2, RC-8, IF-6, RS-4.7, B584, B402, B499; the filter and FX slot gains are deferred |
| Filter routing as rows: sources into filters, serial or parallel, so the flow picture follows the signal. | GUI-14 r5, GUI-18 | Mock controls outside the window, unsaved | RC-1, X13; not a manifest row |
| Rows for the Filter, Bend and Scale deck tabs. | GUI-4 | Mock, each tab says why | RC-1, B567, B580; RC-3; B260 |
| The resolver publishes each key's owning corner and quantum-resolved corner, so the field, pad and knobs name one owner, and a boundary drag edits what it publishes. | GUI-20, ADR-261 | A hash plus Cohesion, seed and locks | B532(b) only; not in MO-7 or RC-8 |
| The bus carries X and Y as intents, ten in all; the macros are live sources; X and Y offset from each corner's rest and write no parameter; a macro maps many parameters, each with its own depth. | GUI-2 r4, GUI-8 r5, GUI-9 | On the device's routes, mock | B89, B581; RC-4 and IF-3 defer it for the new engine |
| A rerouted wire keeps its route id, and a second wire for one source and destination is refused. | GUI-3 | The shell does both | B72 proposes the identity; its morph rule isn't written, and ADR-136's SUM law sums duplicates today |
| Randomize: levels, chaos and amount, locks and exemptions, copying a level between corners, deleting a snapshot, saving all four. | ADR-318, ADR-320, ADR-321, ADR-322, GUI-19 | A placeholder card | B578 |
| History: 200 states, forks, click to restore, the pad out of history. The tree stays in the shell at stage 8, and each state holds an engine snapshot. RC-11.2 leaves corners out of that snapshot, so the tree carries them itself. | GUI-10 A1 | The tree lives in `device.js` | RC-11, `history.h` |
| A retrigger row per modulator. | ADR-266 r7 | Switch not mounted | B584 only; RC-4 defers LFOs |

## Recorded; here so the GUI's dependence is visible

| Need | Decided by | Recorded where |
|---|---|---|
| The real law: Quantum, Stepped, count, glide, the draw. Under Quantum the field's HEARING reads the owner while a knob reads the stub's blend, and they meet when this lands. | GUI-20 | MO-6 to MO-10, D.2, IF-12 |
| Per-key mode and count. The menu's design is B557's open questions. | GB7 | MO-11 to MO-13, IF-12 |
| Depth routes, one level only. Deleting a wire removes the depth wires on it. | GUI-3 | B70; RC-4 defers routes |
| Rail and per-parameter distribution. | GB7 | B133, B28; no design ruling |
| Engine phases for the Specimen. Its cost in the plugin webview is measured before 1.0. | GUI-2 r3 | IF-10, RC-8, ADR-140 |

## Host and plugin checks

Nothing has been tried in a host. Each of these needs a plugin window.
- Whether a host keeps right-click and Delete inside the plugin window (GUI-3).
- Whether a host or Windows takes Option+Shift, the Control sweep (GUI-17).
- How the wheel behaves in the webview (GUI-15).
- The hardware bend and mod wheels: nothing feeds them to the shell yet, and which side feeds them from the host isn't written.

## Open for the human

The lead can't write the morph's spec round until the first two are answered.

1. **What a region lock stores.** The lab's proposal: a row's four offsets, sparse, keyed by the row, or by its unit where rows flip together.
2. **The waypoint caps.** The lab's proposal: 8 per row and 64 per patch, stored in order, because the order breaks the triangulation's ties and fixes the TPS solve's rounding.
3. **Bypass.** RS-4.7 gives two cases. A module that declares its gains owns them, and routing round it takes them out either way. Where the slot provides the gains, RS-4.8 keeps them, and GUI-18's reading takes them out, so under RS-4.8 the same button would act differently by module. The lead is raising this with the human and carries any change to RS-4.8. The console stays as it is meanwhile.
4. **Which corner owns a mapping while morph is on.** SPEC-INTENT-BUS binds per corner, and the Map Mode spec keeps routes global for 1.0. GUI-9 left it open, and it's open on B616.
5. **The kinds of row.** The master's third kind, RAMPS, has no member, because SPEC-ENGINE has Morphable and Structural rows only. That's the human's call. A manifest flag for a numeric stepped row (`arp.seed`, 2^32 values) would be a spec change, so it goes to the lead if the human wants it.

## Settled

- **History (GUI-10 A1).** The earlier reading was wrong, and the row above is the corrected one.
- **X and Y as intents.** ADR-176 item 4 ratified ten slots: X, Y and the eight macros. ADR-192 names only the eight, and nothing revokes X and Y. GUI-9's sentence is true but doesn't reopen them.
- **The root `CLAUDE.md`'s opening.** The lead fixed it on #1370.

## FX device (ADR-254)

- The console starts at the audio input, and its flow isn't built.
- The Specimen needs an audio-driven stand-in.
- The Morph page takes the same pad and field with its own rows, and no source levels in Randomize.
- `BlendMaps` reads parameter values, so it works as it is.
