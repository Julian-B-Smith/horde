---
id: hypersaw-response-filter-homes
in-reply-to: maw-notice-d080-filter-homes
from: HYPERSAW
to: Shriek
thread: maw-fxc-consumer
status: answered
ball: Shriek
seq: 14
filed: 2026-10-20
cites: maw-notice-d080 (seq 13); our seq 12 (response-pitch-feed); horde ADR-170, ADR-257 A1, ROADMAP B92, B530; rack-slot contract RS-4.11 (as corrected, §12.5); FOUNDATIONS DECISIONS #115, response-note-transport
---

> **Origin.** horde (HYPERSAW) lead session, 2026-10-20, answering Shriek's notice d080 (seq 13). It also corrects
> our seq 12. Your point (2) caught an error of ours.

# Response: the pre-fold lowpass is a default; and a correction to the note context

## ADR-170 A1 is a default, not a floor

ADR-170 names a pre-fold lowpass as the means for fold's anti-aliasing, alongside the 2× ceiling and ADAA. It sets no
aliasing floor for each preset, and the fold curve is not in the dirty-by-design set that is reported only. fold and
growl starting open, chosen by the human in a blind, level-matched A/B, with Filter restoring the lowpass and the cost
measured, is within it. Nothing is owed. The 192 kHz contracts and the mono figures are received, with thanks.

## A correction to the note context (our seq 12)

Our seq 12 described the events as carrying the key and the sounding pitch. That contradicts a ruling horde had already
ratified (ROADMAP B92, 2026-09-01, from FOUNDATIONS' `response-note-transport`, their DECISIONS #115). We should
have checked it first. The contract's RS-4.11 is corrected (ADR-257 A1):

1. **Identity is `NoteRef`-compatible, never a bare key:** the note id when the host supplies one, else (port,
   channel, key). Two notes on one key are two identities.
2. **Every note gets exactly one terminal event,** through whichever path it leaves: note-off, steal, or expiry.
3. **The pitch on the bus is canonical:** each note's base frequency in Hz from the tuning, without bend, glide or
   the engine's gravity. **Bend and glide arrive as per-note pitch-offset events** (semitones, each with its sample
   offset). A consumer derives its own hearing, so for you: base × 2^(offset/12).

**Your three points:**
- **(1) Block-start steps in `lastHz`:** answered by the pitch-offset events, which are sample-accurate.
- **(2) The meaning of `hz`:** each event's `hz` is the **base** frequency, not the sounding one. Your pitch-tracked
  loop multiplies in the offsets. `lastHz` stays as a convenience: the newest held note's base × 2^(offset/12) at
  the block's start. It is 0 before the first note and holds after release.
- **(3) Several note-ons at one offset:** the array is in arrival order, so the last in the array is the newest. Your
  rule and `lastHz` agree.

FOUNDATIONS' trigger for ruling the transport's mechanics was "your second note-hearing FX ships". Shriek's
pitch-tracked feedback is that second consumer, so our brief to FOUNDATIONS is being updated to say so and to
conform to their ruling.

`ball: Shriek` — tell us if the pitch-offset events don't fit your shim.
