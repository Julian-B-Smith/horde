# 2026-11-03 — One console for the whole chain: C1 and C2 in the compact mixer lab (GB1)

**What changed.** `docs/design/compact-mixer-lab.html` gains two forms: C1 (upright) and C2 (rows, two columns). Each folds the console and Gain staging into one console, as wide as Gain staging, with M2's arrow on the bars, on the real shell. The five strips with engine rows are the shell's own strips, moved; the other sixteen stages are drawn in the lab from the master's stage list. ROADMAP: a note on GB1, and new row GB4 for the FX mock-ups. No master, bundle or shell change, and no ruling yet.

**Why.** The human: "is there really any reason there should be separate mixers for the oscs in and out of the gain staging? Is there any way we could combine it all into a tightened version of M2 that is as wide as the gain staging? We could try a horizontal and a vertical version. Or if there's any good reason to keep them separate, please inform me."

**Evidence consulted.** `docs/design/gui3.html` (`consolePage`, `gainStaging`: the console's faders write `mix.*` and `out.level`, Gain staging's gains are held in the shell); `GainStaging.chain` and the Gain staging and Channel strip READMEs; ADR-230 and A1 (every stage has a meter, a lamp and a gain); ADR-333; B435 (the slots' gains never morph).

**What the lab found.** (1) The split is real and measured: moving the console's OSC 1 leaves Gain staging's OSC 1 where it was, and moving Gain staging's changes nothing. (2) The poll's "each meter on its own fader's scale" can't reach a ±24 dB stage, because a meter on that scale shows nothing under −24 dBFS. C1 and C2 put every strip on one ruler, −66 to +26 dB, and that is for the human to accept.

**What the check caught.** The first run failed on a reversed source order, a console narrower than Gain staging, readouts too wide for their strips, the reduction bar reading the unforced level, and a real fader's drawing squeezed by its box, which put Out's arrow 2.8 px off its bar.

**Not checked.** A plugin host, keyboard use of the sixteen drawn gains, and the light theme by eye. The levels past the sources are a model in the lab: the chain has no taps there yet.

**Verify.** See the PR.
