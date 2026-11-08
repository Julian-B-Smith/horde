# Splash

The Mindlathe logo over the whole window the first time it opens (GUI-54). It fades on the first press or after 2.5 s, whichever comes first.

**The human, 2026-11-06:** "I want my Mindlathe logo to overlay the gui when you first open it and fade on first click or after a few seconds". By poll: once per Live session, and 2.5 seconds.

**Anatomy.** One layer over the host (`position:absolute; inset:0`, above every popover), on the chassis colour (`--bg`), with the logo centred at 62 % of the width, 600 px at most.

**Behaviour.**
- **Dismissing:** the first press on it, with any button, starts the fade. That press, its click, a double-click, a context menu and a wheel turn stop on the splash, so the press does nothing else. The fade takes 400 ms, and it still takes presses while it fades.
- **Timing:** it starts the same fade after `o.ms` (2500).
- **Reduced motion:** with `o.reduce` set, it goes at once, with no fade.
- **`dismiss()`:** removes it at once. A consumer calls it on an opening that isn't the first.

**Why a canvas.** The plug-in's page runs under `default-src 'none'`, and `gui_sink_check` forbids any `data:` source, so no image can load there, not even one embedded in the page. The human chose to keep that policy whole (2026-11-08).
- **Where the art lives:** in the bundle as base64 (`Horde.MINDLATHE_PNG`), decoded to pixels in script (`Horde.decodePng`, 8-bit RGB or RGBA, not interlaced, inflated with the platform's `DecompressionStream`).
- **If it can't decode:** the splash shows the chassis colour with no art, and `ready` settles false.
- **Keeping it in step:** `tools/splash_art_check.py` holds the base64 to `assets/Logos/mindlathe-saturated.png`, byte for byte.

**The consumer provides** the host and decides which opening is the first: once per Live session needs the plug-in to say so at `hello` (GN-54). `Horde.Splash(host, { ms?, reduce?, label?, art?, onDone? })` returns `{ el, ready, dismiss(), gone() }`.

**FX device (ADR-254).** The same, as a Mindlathe plug-in.
