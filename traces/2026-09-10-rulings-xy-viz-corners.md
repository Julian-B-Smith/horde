# 2026-09-10 — 1.0 ratified; XY reversal (ADR-156); B104–B107 filed

**What changed.** The human ratified the 1.0 definition and ruled the
collisions: the FX reroute (B50) is IN 1.0; QM-4 is 1.1 but the macro/XY
system is fixed for 1.0 by reversal — per-osc pads control K/detune directly,
only MAIN's pad is a macro (ADR-156; B104 carries the build incl. reversing
the floor defaults and re-aligning the oracles that encode them). Filed from
the human's UI notes: B105 (second modulator to a routed param — verified
GUI-only: modAddRoute has no duplicate refusal, evaluate sums per dest;
plus a manual destination picker on MOD), B106 (MAIN shows both oscs'
waveforms + phase carpets; nothing active-osc on MAIN), B107 (corner-preset
save clears the corner dropdown selections). Docs only.

**Verify.** `./verify fast` — docs only.
