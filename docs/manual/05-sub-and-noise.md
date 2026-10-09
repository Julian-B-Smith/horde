# The Sub and the noise oscillator

Purpose: cover horde's two plain sources. The Sub is an honest sub oscillator with no swarm,
there to anchor the low end; the legacy Sub is not carried over as it was, and the new one is
still being workshopped. The plain noise oscillator is in 1.0 by ruling and has no design yet.

## The Sub

What the Sub is for and how it sits under the engine.

Status: PENDING — B399, B327

[[FIG:sub.signal-path]]

### Shapes

The sub's waveforms, the BUMP shape among them.

Status: PENDING — B399, ADR-178

[[TAB:sub.shapes]]

[[FIG:sub.shape-gallery]]

[[AUD:sub.shapes-demo]]

### Pitch and octave

Octave, semitone and fine offsets, and how the Sub follows bend and glide.

Status: PENDING — B399, B278

### Tone

Shaping the Sub's brightness.

Status: PENDING — B399, ADR-178

### Level, velocity and start phase

How loud, how velocity-sensitive, and where each note's cycle starts.

Status: PENDING — B399, ADR-178

### Mono and overlapping notes

Clicks and re-strikes when notes overlap, and what the new Sub guarantees.

Status: PENDING — B399, B202, B205

[[AUD:sub.mono-overlap]]

## The noise oscillator

A plain noise source, separate from the post-1.0 sampler. It has no row or design of its own
yet; the roster ruling is all that exists.

Status: PENDING — ADR-190

### Noise types

The colours or kinds of noise offered.

Status: PENDING — ADR-190

[[TAB:noise.types]]

[[AUD:noise.types-demo]]

### Using noise in a patch

Noise for attacks, breath and texture, and where it enters the mixer.

Status: PENDING — ADR-190, B402

[[AUD:noise.in-a-patch]]
