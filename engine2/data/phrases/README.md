# The phrase set

Four phrases are named by the spec (10.3): **a held note, a chord, a fast line, a trill.** Each is
a sample-timed event list: notes, per-note pitch in Hz, expressions, the pedal, parameter changes.
The render entry plays one from sample 0 at 44.1 kHz and at 48 kHz.

What the spec fixes, and so what any file here must obey:
- the held note and the chord last 2 s each (acceptance item 15);
- where a preset glides (B.1: `glide`, `glideAlways`), the phrase's per-note pitch carries the
  glide: exponential in log-frequency, 95 % reached in the preset's glide time, from overlapping
  notes or always (10.3);
- the envelope presets are played on a 2 s note (acceptance item 16).

**What the spec does not fix: the events themselves** (which keys, which times, which velocities).
They are supplied by the test author with the acceptance tests, as `phrases.json`, event lists in
JSON. No file here invents them, so the builder and the test author work from the same events once
that file lands.
