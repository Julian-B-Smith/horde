# Noise weather

The Noise page's picture (GUI-40, GB9): the noise's settings drawn as a sky, from `sub-noise-lab.html`, where the human picked it.

**What it shows.** Colour sets the grain's size, from fine for white to coarse for brown. Wind blows it sideways in gusts. Whisper's vowels are three slow fronts, and only the grain near a front stays lit. The noise filter is a horizon in `scr-marker`: the grain beyond it fades. Spread opens the field. Each source falls its own way: Clean drifts, S&H holds in level ledges (long at a low rate, flecks at a high one), LFSR stands in a lattice that repeats with the note, and Crackle flashes, one ring per pop at Density's rate.

**Where it goes.** A band across the Noise page, above the controls, as the Dial is on the Sub page, with a smaller version on Main's Noise tab and the Dashboard's Noise card.

**The consumer provides** the canvas, the noise's settings in their units (`type, density, rate, length, colour, resonator, feedback, whisper, vowel, wind, gust, filter, cutoff, resonance, pitch, spread`) and `st`, which keeps the grain: `{ t, level }`, seconds and 0..1. Call `Horde.Viz.noiseWeather(canvas, noise, st)`. `Viz.off` covers it when the noise is off (ADR-105 A2).

**A drawing of the settings, never the noise's signal.** So it needs nothing new from the engine, and it's the cheapest of the lab's three to run. The host labels it under the glass.

**Reduced motion** one still frame: the host holds `t` at 0.

**FX device (ADR-254).** It hosts no sources, so this doesn't appear there.
