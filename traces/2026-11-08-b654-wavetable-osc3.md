# B654: the wavetable contract plans three oscillators with a PM matrix (ADR-385 A1)

**What changed.** `specs/SPEC-WAVETABLE.md` gains section 6a (WT-60 to WT-70): the oscillator count K is a build parameter from S1; a 3 × 3 PM matrix (rows modulated, columns modulator; Morphable corner values, audio-rate; the diagonal is each oscillator's existing `feedback`, C.14); a fixed render order osc 3 → 2 → 1, forward edges at the same sample, back-edges one sample late averaged as C.14's feedback; segment mode while no back-edge is live and interleaved mode while one is, bit-identical when every back-edge is 0 (new gate WT-F14); every modulator's signal carries its envelope, on-fade, PA-5 mix and unison remap; a counted back-edge clamp I_back (4 until the stability map). `docs/wavetable/CONTRACT.md`: rows `oscN.wt.pmM` at offsets 0x039–0x03A (the merged draft's `wt.pm` renamed; nothing ratified moved), the internal modulator's rows PROPOSED at 0x03B–0x041 pending the B652 lab, memory for three table sets, the per-oscillator cap restated as 24 MiB (72 MiB for three), S5 split into S5a and S5b with cells 4 and 5, an operator-stack listening batch, §10 listing what the real engine's third oscillator round would change, and open items H1–H9. Commit 1c40c6aa (the spec drafter, Opus).

**Why.** The human, 2026-11-08, by poll: "Plan 3 with a PM matrix" (ADR-385 A1, recorded in #1466).

**Evidence consulted.** The clean-room manifest at e72a092 (330 ids; none in 0x1030–0x104F or 0x2030–0x204F); a grep of specs, docs, engine2 and tools for those ids; the clean-room `Voice` segment render and `Swarm::render` (the interleave split); SPEC-ENGINE C.14, MO-26/D.8, RC-6.4, PF-1/PF-2.

**Open for the human.** CONTRACT §7 H1–H9, and the restated memory cap (the merged draft proposed 48 MiB per instance; three oscillators at one level per octave under strategy (a) need about 60 MiB).

**Verify.** `./verify fast` on this tree; see the PR.
