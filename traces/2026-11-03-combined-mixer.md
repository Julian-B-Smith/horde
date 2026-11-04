# 2026-11-03 — One console for the whole chain: C1 in the compact mixer lab (GB1, GUI-14)

**What changed.** `docs/design/compact-mixer-lab.html`: C1 folds the console and Gain staging into one console on the real shell, as wide as Gain staging, with M2's arrow on one ruler. The chain's picture is in the same box with each node over its strip's bars, and its gain bars are gone. Every stage has a solo but GR and the master's OUT, each FX module has a bypass, and the picture follows a mock signal flow. "Engine" reads SUM · VOICES. C2 was dropped when the human chose C1. DECISIONS: GUI-14. Ledger: three `ruled` rows. ROADMAP: notes on GB1, new rows GB4 (FX mock-ups) and GB5 (morph and MAP together). No master, bundle or shell change.

**Why.** The human, quoted in GUI-14.

**Evidence consulted.** `docs/design/gui3.html` (`consolePage`, `gainStaging`); `GainStaging.chain` and the Gain staging, Channel strip and Chain line READMEs; ADR-230 and A1; B435; specs/SPEC-ENGINE.md OU-1 (the output stage scales the sum of the voices), IF-6 (mute and solo), X13 and RC-1 (the filter routing, deferred); FxRack and FxCables (the cables).

**What the lab found.** (1) Moving the console's OSC 1 leaves Gain staging's OSC 1 where it was, and moving Gain staging's changes nothing. (2) Each meter on its own fader's scale can't reach a ±24 dB stage, hence one ruler. (3) Morph and MAP together leave a page 519 of 933 px, measured on every page (GB5).

**What the check caught, and what it didn't.** The first runs caught a reversed source order, a console narrower than Gain staging, readouts too wide, and a real fader's drawing squeezed by its box. A check that "the picture changes with the flow" passed while serial and parallel still looked alike, because branches at one level draw on top of each other. Looking at it caught that. Branches now arc clear of the line, and a new check probes each arc on glass that is bare in plain series. That check then failed on its own test (a parallel route with the source sent to one filter draws no wire to the other), so the test was corrected, not the gate's threshold.

**Not checked.** A plugin host, keyboard use of the sixteen drawn gains, and the light theme by eye. The flow and every level past the sources are the lab's model: the engine has no routing rows, no solo tap and no taps there yet.

**Verify.** See the PR.
