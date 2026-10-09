# The engine: SCALPEL on the Kuramoto swarm

Purpose: explain horde's sound source well enough that a reader can predict what a control will
do before touching it. Each voice is a swarm of detuned oscillators that pull on one another's
phase (Kuramoto coupling), and each member carries SCALPEL blades: windows of its cycle where the
wave is cut and replaced by another process. Two such oscillators make the engine. The parameter
surface waits on the engine audit, so names here are provisional until it lands.

## Two oscillators, one engine

What OSC 1 and OSC 2 are, what they share, and how they reach the mixer.

Status: PENDING — B327, B376

[[FIG:engine.overview]]

## Inside a voice

The path one note takes: swarm members, blades, edge correction, summing, the output stage.

Status: PENDING — B385, B378

[[FIG:engine.signal-flow]]

## The swarm

The coupled oscillators that give horde its motion and its lock.

Status: PENDING — B376, B298

### Members and detune

How many oscillators a voice runs and how far apart they are tuned.

Status: PENDING — B376

[[FIG:engine.detune-spread]]

[[AUD:engine.members-compare]]

### Coupling: locking and repulsion

Positive coupling pulls members into phase lock; negative coupling pushes them apart into an even
splay. The single most characteristic control.

Status: PENDING — B376, ADR-184

[[FIG:engine.coupling-phases]]

[[AUD:engine.coupling-sweep]]

### Coupling time: seconds or cycles

Whether coupling acts in absolute time or per cycle, and why cycles keep the lock the same across
the keyboard.

Status: PENDING — B376, ADR-184

### Note start: settled or free

How a fresh note's members begin, already settled or still finding each other.

Status: PENDING — B298, B335

[[AUD:engine.onset-settled-vs-free]]

### Drift, inertia and gravity

The slower behaviours the legacy swarm brought into the engine.

Status: PENDING — B298, B335

## Blades

The SCALPEL idea: a window in each member's cycle where the wave is replaced.

Status: PENDING — B376, ADR-184

### Anatomy of a blade

Position, width, edge hardness and depth, drawn on one cycle.

Status: PENDING — B376, ADR-184

[[FIG:engine.blade-anatomy]]

### Blade modes

Sync, FM reset, FM free, noise, fold, ring and crush: what each does inside the window.

Status: PENDING — B376, ADR-184

[[TAB:engine.blade-modes]]

[[FIG:engine.blade-mode-waveforms]]

[[AUD:engine.blade-modes-tour]]

### Cut rate

How fast the process inside the blade runs, per blade or in Hz, and why it sweeps like a formant.

Status: PENDING — B376, ADR-184

[[AUD:engine.cut-rate-sweep]]

### Waves and modulator shapes

The waves a blade can carry, including the sine-to-saw morph.

Status: PENDING — B376, ADR-184

[[TAB:engine.blade-waves]]

### Crush

The stepped, averaged hold levels and their slew.

Status: PENDING — B376, ADR-184

### Mirror and twin

Reflecting a blade, and twin blades half a cycle apart that can cancel even harmonics.

Status: PENDING — B376, ADR-184

[[FIG:engine.mirror-twin]]

[[AUD:engine.twin-even-harmonics]]

### Rotation

Moving the blade around the cycle, restarted per note or free-running.

Status: PENDING — B376, ADR-184

[[FIG:engine.rotation]]

[[AUD:engine.rotation-demo]]

### Blade 2 and blade interplay

The second blade, and how two blades compose and collide.

Status: PENDING — B311, B376

[[FIG:engine.blade-interplay]]

[[AUD:engine.blade-interplay-demo]]

### Blade envelopes

The per-blade attack-decay envelopes that move cut rate and width with each note.

Status: PENDING — B366, B377

[[FIG:engine.blade-envelope]]

## Spreads across members

Giving each member a different blade, so the swarm becomes a chord of timbres.

Status: PENDING — B376, ADR-184

### Spread laws

Gradient, random and drift: how each member's offset is chosen.

Status: PENDING — B376, ADR-184

[[FIG:engine.spread-laws]]

### What each spread moves

Position, cut rate, cut rule, width, depth, shape, FM index and rotation.

Status: PENDING — B376, ADR-184

[[TAB:engine.spreads]]

### Cut rules

Harmonic, undertone, octave, chord, golden, prime and custom ladders across the members.

Status: PENDING — B376, ADR-184

[[TAB:engine.cut-rules]]

[[AUD:engine.cut-rules-demo]]

## Cross-member modulation and feedback

Members modulating their neighbours' blades, and a member feeding back into itself.

Status: PENDING — B376, ADR-189

[[FIG:engine.cross-member-ring]]

## Stereo inside the engine

How members are panned (balanced or fan order) and what true stereo adds inside a voice.

Status: PENDING — B408, B376

[[FIG:engine.pan-order]]

## Engine quality

Edge correction and oversampling as they affect the engine's sound; the full story is in
chapter 16.

Status: PENDING — B383, ADR-191

## Engine parameters

A pointer to the generated parameter reference (chapter 19) rather than a second table here.

Status: PENDING — B376, B275
