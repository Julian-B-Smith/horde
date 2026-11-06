# What the GUI rulings need under the hood

The GUI session's handoff to the lead (GB8; the human, 2026-11-04: "let's make sure the main horde agent knows about any decisions we've made during the design process that will affect functionality and need to be wired in under the hood as well").

**Last verified 2026-11-04, against GUI-1 to GUI-33, `specs/SPEC-ENGINE.md` and `ROADMAP.md` on main.** The GUI session keeps this current: each new ruling that needs wiring adds its row in the same PR. "Not recorded" means a search of the spec and the roadmap found nothing, so it's the lead's to place. The rows are the lead's to schedule (B616 cites this file), and nothing here is a ruling on how to build them.

**Timing (the lead, 2026-11-04).** The morph comes straight after the CPU pass, and the spec has no waypoints or region locks. If the design rests on them, they need a spec round before the builder starts the morph, and that round waits on the human's answers to the two questions under "Open for the human".

**The IDs (the lead, 2026-11-04).** `specs/SPEC-ENGINE.md` alone decides how the engine behaves. A GUI ruling decides how something looks and feels, so a need here is a request until a spec round adopts it, and the shell shows it as mock until then. Each row has a `GN-n` the lead schedules by, and the ledger's `engine_impact` says which rulings create or rely on it. An ID is never reused: a finished or dropped need keeps its row and its ID.

A line like GUI-14 r3 is a ruling in `DECISIONS.md`. Every row below is MOCK, a stand-in or refused in the shell today, as its third column says.

## Not recorded for the engine side

These are the ones nothing on the engine side knows about yet.

| ID | Need | Decided by | The shell today |
|---|---|---|---|
| GN-1 | A solo tap at every stage: plays that stage's output directly, one at a time, combinable with a source's solo, still through the limiter. A new route to the limiter. Unruled: whether a stage solo saves, undoes or is a host parameter. A source's solo is a saved, undoable Device row, a stage's is unsaved session state today, and the two combine. | GUI-14 r4, GUI-16, GUI-18 | The stage's S has no effect on any audio |
| GN-2 | A bypass key per FX slot. See Bypass, under Open for the human. Also unruled: whether it saves and undoes, is a host parameter, and is morphable per corner. | GUI-14 r3, GUI-16, GUI-18 | BYP is drawn on the console; mock |
| GN-3 | The rack's cable state published to the view, for the new rack (only legacy B265 has anything like it). | GUI-14, GUI-16, GUI-18, GUI-36, GUI-41 | A serial stand-in |
| GN-4 | A summed-output signal for the foot row's scope and spectrum. IF-10 is per voice and per oscillator; RC-8 has peaks only. | GUI-8 | `dev.summed()` is mock |
| GN-5 | A Control route stores a sweep: a low end and a high end. Option+Shift-drag moves the high end. ADR-266 r2's law goes with it: a Control ignores the knob's value, there's one per parameter, other polarities add on top, clamped once, and the knob's value returns when it's removed. The stand-in doesn't enforce one per parameter. | GUI-17 | Held in the shell's mapping band |
| GN-6 | Row-level region locks in the morph: a lock keeps the row's own four offsets. What a lock stores is open. | GUI-20, B269 | Held in the device stand-in, with undo |
| GN-7 | Waypoints: pins per row, in plugin state, with per-row and per-patch caps and the TPS·Δ or TIN·Δ law and `clamp` per patch. Pins stay dormant across a law change, and how they meet per-key mode (GN-29) is unstated. The spec has no waypoints; B235 and B272 are legacy rows. | GUI-20, GUI-21, B272 | Held in the device stand-in with undo, drawn but not heard |
| GN-8 | The corner import list: each corner's global parameters, their eligibility, an import button. | ADR-192 item 4, GUI-19 | Not built |
| GN-9 | A device preset seam: list, load and save global presets, and read the legacy-format factory bank. Double-click's home is the loaded preset's value, and both GUI-12 questions are open: does a save move home, and is a corner's saved preset its home? Also unruled: what a load resets, member by member (the stand-in's Init resets corners, Device rows, routes, names and saved presets, and keeps regions, pins, the law, morph on or off, audible and position), and whether home survives reopening a project, and per corner. | GUI-6, GUI-12, GUI-50 | Control is mock; home is the default |
| GN-10 | The XY assignment and the routes save with the patch. IF-4 and RC-11.1 have no state member for them. Whether the puck's and macros' values save, undo or reset is also unruled: moving the puck makes no undo node, and since it stays where it's left (GUI-9), its position is part of the sound. | GUI-2 r2, GUI-9 r1 | In the device's state, for the session |
| GN-11 | Whether X and Y (and the intents) are host parameters: "the lead's and the human's call". | GUI-9 | Not host parameters |
| GN-12 | `out.level`'s label becomes SUM · VOICES; the key stays. | GUI-14 r6, GUI-18 | The shell uses the word |
| GN-13 | Notes reaching the GUI, for Arp's piano roll and pictures. | GUI-11 | A sample chord at 120 BPM |
| GN-14 | The hardware bend and mod wheels: which side feeds them from the host. Also what a wheel moved with the mouse sends, and whether the mod wheel is a route source, as B70's example assumes. | GUI-8 r5 | The bank's wheels are mock; nothing feeds them |
| GN-33 | A Shriek patch can keep more than four snapshots: a save adds one and never writes over one. ADR-300's state holds four corner snapshots in `blobs["shriek.pad"]`, and ADR-351 caps at creation with a number B603 hasn't ruled. Which snapshots a state keeps, and the cap. The list also stores a factory mark per snapshot, each corner's binding and a name counter, and deleting a snapshot rebinds its corners to the first, which changes their sound. If each horde corner carries its own blob, an uncapped list counts up to four times against ADR-359 r2's ceiling (a hypothesis). | GUI-24 | Page state in the Shriek lab, with no cap |
| GN-34 | Who holds the device layer while the window is closed: corners, routes, morph position and on/off, and the undo tree all live in the page today, and ADR-160 r1 says "GUI-side dies with the window". The plugin needs an owner for all of it, including what feeds the resolver when the host automates the position with the window shut. GN-22's "the shell" has to say the plugin's shell, not GUI 3's. | ADR-160 r1, GUI-10 A1 | `device.js`, in the page; closing drops only pending modulators |
| GN-35 | What "Save current sound" stores. The stand-in writes each key's blended value, with no macro or XY offsets and no route modulation, and it doesn't bake or re-centre. SPEC-INTENT-BUS's commit bakes the offsets, changes only keys the corner owns, and refuses a save with non-zero offsets. IF-4 keeps only the heaviest setting of a blending group, so one corner can't hold a two-setting sound. The resolver has to supply the resolved values. See Open for the human. | ADR-321 r1, GUI-20 | `dev.saveSoundTo` writes `heard()`, on the stub's bilinear blend |
| GN-36 | A host edit with morph off. IF-12 routes host edits to corners only while morph is on, and the resolver emits nothing while it's off, so the audible corner in the state document and the value the engine plays can disagree. It needs a rule, such as a host edit with morph off writing the audible corner. | ADR-319 r3, ADR-160 r3 | The edit only overrides the display until the next user write, so the corner stays stale |
| GN-37 | The morph system's saved document as one list of members: each corner's values and name, saved presets, D.2's settings, modes and counts, regions, pins. RC-11.1 names some of these and IF-4 has only `version`, `params` and `ratios`. Its size needs a ruled home too. An estimate, not a measurement: about 60 KB for four corners of 544 members, about 120 KB with all four presets saved, against ADR-351's 64 KiB paste cap and ADR-359 r2's open ceiling. | ADR-321 r2, GUI-20, GUI-50 | In the device's state, for the session; never saved |
| GN-38 | ADR-277's second tier: the corner pin, each corner's lock and MAP on or off are saved with the project and stay outside the sound. IF-4 has one document, so the state has to split, and a preset save has to leave this tier out. | ADR-277 | In the bundle's session, unsaved |
| GN-39 | Whether an unrouted modulator's settings stay in the patch. "Active" means "has a route" today, and that has no answer once LFOs have settings of their own (RC-4, ADR-349). A hypothesis until they land. | ADR-322 item 3 | Sources have no settings; pending ones are dropped on close |
| GN-40 | What Exempt means past MO-12. MO-12 covers what it holds. The shell also reads: an Exempt key still takes modulation; an edit, by hand or by a host, moves all four corners, even with morph off; any locked corner refuses that edit whole; and revert leaves it alone. | GUI-28 | On the device's stand-in |
| GN-41 | One heard cycle per member, per voice and oscillator, for the waveform's three views. RC-8 has the phases and each blade's window, and no member's cycle; IF-10's y⁰ is for tests (RC-8.3). Also unruled: which voice an oscillator's page draws when several are sounding. | GUI-30, ADR-101 | `Synth.cycle`, a SAMPLE sketch of the settings |
| GN-42 | Each member's pan and detune, placed and actual, for the voice map. RC-8 has neither; IF-10 has the frequencies and pan gains. | GUI-30, ADR-118 A6 | SAMPLE, placed from Detune and Width |
| GN-43 | A spectrum feed for an Osc page. GN-4 asks for the summed output; this page reads as one oscillator's, so it needs a tap per oscillator, or a ruling that it shows the output. | GUI-30 | SAMPLE, the bands of the sketched cycle at A2 |
| GN-44 | Nothing to store for the waveform's view and mapping, as read: they're per machine, in the webview's storage, and a patch doesn't carry them, as with a fold's open state. Say if the mapping should save with a patch or the plugin's state: IF-4 has no member for it. A drag writes parameter rows as one undo step, with no new state. | GUI-30 | Kept in `localStorage` |
| GN-45 | One answer per snapshot: is any voice sounding? The pictures rest while none is. RC-8's per-voice level can give it, with B365's -90 dBFS as the threshold (GUI-34), and a release tail counts as sounding. | GUI-33, GUI-34 | `dev.sounding()` reads `dev.feed()`, which is null in the stub |
| GN-46 | One batched frame across the GUI bridge, at the pictures' 30 fps, in binary where the bridge allows it. GN-41's cycles are about 256 x members x voices of numbers a frame, and B76 already found three binds a frame expensive. Its rate and whether WKWebView's bridge can carry binary are unmeasured. | GUI-33 | No bridge: the stub's in-page | B76, RC-8 |
| GN-47 | Each voice's note and gate in the snapshot, with its envelope's level, for the Specimen to strike: a fresh gate is a strike, the wavelength comes from the note, and the loudest envelope blooms the film. RC-8 has each voice's level and its envelopes' stages, R and the peaks, and no note or gate. | GUI-34, ADR-139 | `dev.feed()` is null in the stub; a test build fills it | RC-8 |
| GN-48 | The sub's output after its chain, per voice or for the mono sub, with a peak or flag for the drive's ceiling, for the Dial. IF-10's y⁰ is before the tone, the drive and the envelope. | GUI-35, GB9 | `Synth.subCycle`, a SAMPLE from the settings | IF-10, RC-8 |
| GN-49 | The LFOs: the modulators' spec, how many, and each shape as a shared point list with per-corner time, level and bend, stored as corner state rather than host parameters. | GUI-37, ADR-349 | `ShapeMorph` in the shell, heard by nothing | RC-4, B601 |
| GN-50 | Each oscillator, the sub, the noise and each filter slot as its own input to the FX rack, so each can feed modules on its own; and each corner's wire set as morph state, with the rule that no admitted graph has a loop at rest. | GUI-41 | `FxGraph` in the shell, heard by nothing | RC-5, ADR-201, B265 |
| GN-52 | A library of corner presets: list the saved ones, load one into a corner by name, save a corner as one; and, for every kind of preset (global, corner, a module's, Arp's pattern), the values it was loaded with, so a name can carry an asterisk when anything has moved since. | GUI-50 | Corner names and a per-corner saved copy in the page (the stand-in); no library | GN-9, GN-37 |
| GN-53 | Per-corner mappings' loop mode as a per-patch setting (refuse any possible loop, refuse inside a corner, allow one step late; default allow), and a corner preset load's report of each mapping it dropped to keep a refusing mode, so the page can name them. | GUI-53 | Not built; mappings are one global set in the stand-in |
| GN-54 | Whether this is the window's first opening since the plug-in loaded, told to the page at `hello`, so the Mindlathe splash shows once per Live session (the page itself dies with the window). | GUI-54 | Not built |

## Partly recorded

| ID | Need | Decided by | The shell today | Recorded where |
|---|---|---|---|---|
| GN-15 | A tap, peak, clip latch and gain at every stage, for the console and its lamps. | GUI-14, GUI-16, GUI-18 | Lamps latch on a model, 24 dB under the stub's tone | RC-2, RC-8, IF-6, RS-4.7, B584, B402, B499; the filter and FX slot gains are deferred |
| GN-16 | Filter routing as rows: sources into filters, serial or parallel, so the flow picture follows the signal. | GUI-14 r5, GUI-16, GUI-18 | Mock controls outside the window, unsaved | RC-1, X13; not a manifest row |
| GN-17 | Rows for the Filter, Bend and Scale deck tabs. | GUI-4 | Mock, each tab says why | RC-1, B567, B580; RC-3; B260 |
| GN-18 | The resolver publishes each key's owning corner and quantum-resolved corner, so the field, pad and knobs name one owner, and a boundary drag edits what it publishes. | GUI-20, ADR-261 | A hash plus Cohesion, seed and locks | B532(b) only; not in MO-7 or RC-8 |
| GN-19 | The bus carries X and Y as intents, ten in all; the macros are live sources; X and Y offset from each corner's rest and write no parameter; a macro maps many parameters, each with its own depth. | GUI-2 r4, GUI-8 r5, GUI-9 | On the device's routes, mock | B89, B581; RC-4 and IF-3 defer it for the new engine |
| GN-20 | A rerouted wire keeps its route id, and a second wire for one source and destination is refused. | GUI-3 | The shell does both (the bank's drag missed the refusal until GUI-27's audit) | B72 proposes the identity; its morph rule isn't written, and ADR-136's SUM law sums duplicates today |
| GN-21 | Randomize: levels, chaos and amount, locks and exemptions, copying a level between corners, deleting a snapshot, saving all four (the last two are on the Morph page's corners, GUI-28). Unruled: whether a draw may write a frozen corner with morph off (ADR-321 r2's exception is for an explicit save), and whether it may write a control GUI-21 has hidden. | ADR-318, ADR-320, ADR-321, ADR-322, GUI-19, GUI-28, GUI-38 | A placeholder card on the Random page | B578 |
| GN-22 | History: 200 states, forks, click to restore, the pad out of history. The tree stays in the shell at stage 8, and each state holds an engine snapshot. RC-11.2 leaves corners out of that snapshot, so the tree carries them itself. Each state also holds routes, names, saved presets, regions, pins and the law, and it will need key modes (GN-29), macro bindings and module patches: ADR-339 r2 makes copying a module to all corners one undo step. RC-11.4 says a restore isn't a load, so an undo across a module patch has no rule yet. ADR-160 r5 preallocates 200 states at 35 KB; the stand-in uses JSON strings. | GUI-10 A1 | The tree lives in `device.js` | RC-11, `history.h` |
| GN-23 | A retrigger row per modulator. | ADR-266 r7 | Switch not mounted | B584 only; RC-4 defers LFOs |
| GN-24 | PA-8's conditions, machine-readable, because a control whose condition fails is no longer drawn. The shell transcribes them, so a PA-8 change now shows or hides a control. B587 says "greys"; GUI-21 hides, and the table it asks for stands. The FX modules and Arp need theirs from their own manifests (GB10). A source's on switch hides nothing since GUI-29, and a blade's since GUI-30, though PA-8 still calls those rows inert, so a machine table has to say which conditions a page obeys. PA-8 doesn't say whether a condition reads the base or the modulated value. The shell reads one shown value, so it can hide a control that's acting: `driftRate` under a route on `driftDepth` with its base at 0, or a row live only under the lighter setting of a blending group. See Open for the human. | GUI-21, GUI-29, ADR-319 r6, GUI-30 | `layout.json`'s `live_when`, reading the shown value | PA-8, B587 |
| GN-25 | The master's third kind of row, RAMPS, has no member, because SPEC-ENGINE has Morphable and Structural rows only. It's open for the human, below. | GUI-20 | The Find pane's pill counts 0, and `KindLegend` isn't mounted | B616, open |
| GN-26 | A manifest flag for a numeric stepped row (`arp.seed`, 2^32 values), if the human wants one. | GUI-20 | The shell treats a stepped row without named options as a value | B616, open |
| GN-27 | Which corner owns a mapping while morph is on: SPEC-INTENT-BUS binds per corner, and the Map Mode spec keeps routes global for 1.0. | GUI-9, GUI-53 | One global set of routes | B616, open |
| GN-51 | Keys and rows for the pad position's x and y in Appendix D.2's manifest (range 0 to 1, linear, continuous), so the Morph page can draw them as knobs and a host can automate them. | GUI-45 r3 | Not drawn: the shell's gate draws parameter knobs only from rows | IF-11 names them host parameters; D.2 has no rows |

## Recorded; here so the GUI's dependence is visible

| ID | Need | Decided by | Recorded where |
|---|---|---|---|
| GN-28 | The real law: Quantum, Stepped, count, glide, the draw. Under Quantum the field's HEARING reads the owner while a knob reads the stub's blend, and they meet when this lands. | GUI-20 | MO-6 to MO-10, D.2, IF-12 |
| GN-29 | Per-key mode and count. The menu is in the shell (GUI-28): Exempt and Quantum play on the stand-in, and the count is held and sent to the resolver, but nothing plays it until the real resolver. | GUI-19, GUI-28, GB7 | MO-11 to MO-13, IF-12 |
| GN-30 | Depth routes, one level only. Deleting a wire removes the depth wires on it. | GUI-3 | B70; RC-4 defers routes |
| GN-31 | Rail and per-parameter distribution. | GUI-19, GB7 | B133, B28; no design ruling |
| GN-32 | Engine phases for the Specimen. Its cost in the plugin webview is measured before 1.0. | GUI-2 r3 | IF-10, RC-8, ADR-140 |

## Host and plugin checks

Nothing has been tried in a host. Each of these needs a plugin window.
- Whether a host keeps right-click and Delete inside the plugin window (GUI-3).
- Whether a host or Windows takes Option+Shift, the Control sweep (GUI-17).
- How the wheel behaves in the webview (GUI-15).
- Whether the webview's `localStorage` persists in a plugin. The Specimen, sidebar, folds and logo style keep their preferences there (ADR-147).

## Open for the human

The lead can't write the morph's spec round until the first two are answered.

1. **What a region lock stores (GN-6).** The lab's proposal: a row's four offsets, sparse, keyed by the row, or by its unit where rows flip together.
2. **The waypoint caps (GN-7).** The lab's proposal: 8 per row and 64 per patch, stored in order, because the order breaks the triangulation's ties and fixes the TPS solve's rounding.
3. **Bypass (GN-2).** RS-4.7 gives two cases. A module that declares its gains owns them, and routing round it takes them out either way. Where the slot provides the gains, RS-4.8 keeps them, and GUI-18's reading takes them out, so under RS-4.8 the same button would act differently by module. The lead is raising this with the human and carries any change to RS-4.8. The console stays as it is meanwhile.
4. **Which corner owns a mapping while morph is on (GN-27).** SPEC-INTENT-BUS binds per corner, and the Map Mode spec keeps routes global for 1.0. GUI-9 left it open, and it's open on B616.
5. **The kinds of row (GN-25, GN-26).** The master's third kind, RAMPS, has no member, because SPEC-ENGINE has Morphable and Structural rows only. That's the human's call. A manifest flag for a numeric stepped row (`arp.seed`, 2^32 values) would be a spec change, so it goes to the lead if the human wants it.
6. **Does a saved snapshot ever overwrite on horde's own corners (GUI-24)?** The ruling covers the Shriek lab. `Save current sound` and `Save preset` still overwrite a corner, with undo as the only protection, as ADR-319 and ADR-321 ruled. A list would change the state document, history and ADR-339 r1's sharing.
7. **What Save current sound stores (GN-35).** Which keys, whether offsets are baked, and what a blending group saves.
8. **Whether a control under modulation or a blend stays drawn (GN-24).** GUI-21 hides on one shown value.
9. **Whether the puck and macro values save and undo (GN-10, GN-19).**
10. **Whether Randomize may write frozen corners or hidden controls (GN-21).**

## Settled

- **History (GUI-10 A1).** The earlier reading was wrong, and the row above is the corrected one.
- **X and Y as intents.** ADR-176 item 4 ratified ten slots: X, Y and the eight macros. ADR-192 names only the eight, and nothing revokes X and Y. GUI-9's sentence is true but doesn't reopen them.
- **The root `CLAUDE.md`'s opening.** The lead fixed it on #1370.

## FX device (ADR-254)

- The console starts at the audio input, and its flow isn't built.
- The Specimen needs an audio-driven stand-in.
- The Morph page takes the same pad and field with its own rows, and no source levels in Randomize.
- `BlendMaps` reads parameter values, so it works as it is.
