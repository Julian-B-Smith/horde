# FX device audit: voice-state reads and modulation-source classes (B528, ADR-254)

> **Origin.** horde lead session, 2026-10-21. Owed by ADR-254 before the clean room builds the rack. A read-only
> sweep by an Opus explorer; findings reviewed by the lead. Paths are repo-relative.

## Summary

Nothing in `src/fx_rack.h` reaches into a voice or the engine. The voice ties sit in its caller,
`src/hypersaw_clap.cpp`, and in one deferred plan. The horde 2 composed engine (`h2/`) has no rack yet; its
structure (engine → make-up → rack → master) matches SPEC-ENGINE RC-5.

## (a) Voice-state reads on the FX side

| Site | What it reads | FX device |
|---|---|---|
| `src/fx_rack.h:210-244` `noteOn(key,freq)` → Comb's note-tuned lines (`:750-811`) | note key and Hz, pushed as an event | adapt: feed from the note context (RS-4.11) |
| `src/fx_rack.h:245-250` `noteOff` | nothing (a no-op the shell never calls) | adapt: RS-4.11 needs one terminal per note |
| `src/hypersaw_clap.cpp:9200-9201` | 12-TET Hz from the key, to `rack.noteOn` | adapt: note identity plus base Hz from the tuning |
| `src/hypersaw_clap.cpp:2418` `rack.reset()` in all-notes-off | a panic event | fine |
| `src/hypersaw_clap.cpp:9912` `rack.setTempo(core.p.bpm)` | host tempo, routed through engine state | adapt: take it from the transport |
| `src/hypersaw_clap.cpp:984`, `:9924-9937` | the routing matrix gathers osc 1, osc 2 and sub buffers into the slots | **blocker or ruling** (below) |
| `src/hypersaw_clap.cpp:8458-8465, 8802-8842` | rack parameters, mod-matrix writes via `applyParam` | fine: declared mod values |
| `src/notch_core.h`, `src/time_core.h` gravity anchors (`f0last`, default 110 Hz) | never fed by the rack | fine: could take `lastHz` |
| `src/swarmfx_clap.cpp:216-228` (parked SWARM-FX) | MIDI note-on → `setNoteFreq` | fine: an earlier MIDI-sidechain form of the idea |
| `docs/proposals/rack-slot-contract.md` RS-4.11 | terminals by steal or expiry; offsets include glide; base Hz from the tuning | adapt: without voices there is no steal or expiry; the device needs its own glide and tuning |
| `ROADMAP.md` E3 (deferred) | "FX cores cross-coupled to the carrier swarm" | **blocker if built as written** |

## (b) Modulation sources

| Source | Class | FX-device stand-in |
|---|---|---|
| ENV 1 | voice-bound | an envelope follower on the input |
| ENV 2–4 (EN-5) | voice-bound | one C.12 envelope per sidechain note; one-shot needs a sidechain gate |
| Velocity, keytrack | voice-bound | sidechain velocity; `lastHz` or a pitch tracker |
| Pressure, MPE expressions | voice-bound | sidechain, **but RS-4.11 carries no expression events** |
| Mod wheel, pitch wheel | free (MIDI) | sidechain CC and bend |
| Macros, intents, the pad, morph, hosted-module macros | free | — |
| LFO 1/2, the Kuramoto LFO | free | retrigger → sidechain note-on; sync → host tempo |
| Swarm R, ψ, σ, drift, slip, onset, coherence | voice-bound | none (onset → a transient detector) |
| Sub phase, mono sub's chosen note, noise stage gains | voice-bound, instrument-only | none |
| ORBITAL (global scope) | free | — |
| ORBITAL per-voice scope, note-on reset | voice-bound | sidechain note-on |
| MIDI trackers: gate, trigger, note number, release velocity, held count | voice-bound | sidechain, **but RS-4.11 has no release velocity** |
| Note-on randoms, spring displacement | voice-bound | a seeded draw per sidechain note; sidechain notes and bends |
| Dry-signal follower, FX-core observables | audio-derivable | native |
| Host tempo | free | — |

## Findings

1. **Per-source routing into the rack.** The legacy routing matrix feeds slots from osc 1, osc 2 and the sub
   separately. SPEC-ENGINE RC-5 hands the rack one mixed pair. That is the human's choice for horde 2, and it is
   open (B528).
2. **The deferred cross-coupling plan** (E3) must go through declared mod sources if it is built, never core to core.
3. **ADR-254's checklist, broken by the legacy code** (horde 2's rack must not repeat any of these):
   - three-source routing;
   - tempo reaching the rack through engine state;
   - bare-key, 12-TET note events with no terminal;
   - no latency or tail sums in the rack;
   - rack host ids split across several blocks.
4. **RS-4.11's gaps for a sidechain:** no events for pressure, MPE expressions, release velocity or the sustain
   pedal. Proposed for the contract when FOUNDATIONS answers the `note-context` brief.
5. **No stand-in exists** for the swarm observables or the sub and noise values. They are the FX device's list of
   gaps, by design.
6. **A stale record:** `integrations/maw/response-pitch-feed.md` (seq 12) gives key and sounding Hz. It stays as
   the record, superseded by seq 14 (ADR-257 A1).
