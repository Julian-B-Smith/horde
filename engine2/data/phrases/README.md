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
They are supplied by the lead (ROADMAP B497 part 2) as `phrases.json`, event lists in JSON, and the
reference renders and the tests share them: the builder and the test author work from the same
events.

## `phrases.json`

Times are given in samples at 44.1 kHz (the truth) and in seconds (the same instant, for reading).
Every event is a note-on (`note_id`, `channel`, `key`, `hz`, `velocity`) or a note-off (`note_id`,
`channel`, `key`). Pitch is 12-TET from the key, A4 = 440 Hz (IF-2), written out as `hz`. At one
sample, key-ups come before key-downs; the list is sorted by sample. A phrase's `length_samples` is
the sample of its last note-off. No phrase uses an expression, the pedal, a release velocity or a
parameter change.

| id | what | notes | length |
|---|---|---|---|
| `held` | a held note | key 60, velocity 0.85, 0 to 2.0 s | 2.0 s |
| `chord` | a chord | keys 60, 64, 67 and 71 (OU-2's chord), velocity 0.85, all 0 to 2.0 s | 2.0 s |
| `fast_line` | a fast line | 16 notes 0.1 s apart (sixteenths at 150 BPM), each held 0.09 s (a 10 ms gap), keys 48 50 51 53 55 56 58 60 62 60 58 56 55 53 51 50, velocities alternating 0.9 and 0.7, starting at 0.9 | 1.59 s |
| `trill` | a trill | 24 notes alternating keys 62 and 63, starting on 62, 0.08 s apart, each held 0.09 s (a 10 ms legato overlap with the next note), velocity 0.8 | 1.93 s |

The reference renders play each phrase for its length plus a 2 s tail, so that a release is heard.

## Glide

Glide is not encoded in the events: it depends on the preset, and the pitch system that would
apply it (RC-3) does not exist yet. 10.3's rule, for a test that needs the pitch of a gliding note:
a note that glides moves from the pitch it starts from (the previous sounding pitch) to its own
pitch `f` along `ln f(t) = ln f + (ln f_from − ln f) · e^(−3t/T)`, t seconds after its note-on and
T the preset's `glide` in seconds (the move is 95 % done at t = T). A note glides when it starts
while a note started on an earlier sample is held, or, with `glideAlways`, after any note started
on an earlier sample, and it glides from the most recent such note. Notes that start on the same
sample, a chord's, never glide from one another (round 39, the human's ruling, ADR-285). The
trill's overlaps are what make a gliding preset glide; the chord, with no earlier note, starts on
pitch; the fast line's notes are detached, so it glides only with `glideAlways`.
