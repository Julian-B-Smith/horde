# The master strip

Purpose: explain the last stage before horde's output: a fixed limiter (Bulwark's limiter, never
a rack slot), its ceiling and release, the gain-reduction meter, the master clip latch, and the
Volume control. It also explains, for a player, why the default ceiling is −1.0 dBFS rather than
0.

## The master strip at a glance

Where the master strip lives on the mixer page and what each control does.

Status: PENDING — B438, B402

[[FIG:master.strip]]

[[TAB:master.controls]]

## The limiter

One limiter, always last, with simple controls.

Status: PENDING — B438

### Limiter on and off

What switching the limiter off changes.

Status: PENDING — B438

### Ceiling

The highest sample level the limiter lets through; default −1.0 dBFS.

Status: READY-TO-WRITE — ADR-195

[[AUD:master.ceiling]]

### Release

Automatic or in milliseconds, and how it changes the sound of hard limiting.

Status: PENDING — B438

### The gain-reduction meter

Reading how hard the limiter is working.

Status: PENDING — B438, B225

[[FIG:master.gr-meter]]

## Volume

The final output level, from silence to 0 dB; it only ever turns down.

Status: PENDING — B438, B402

## The master clip latch

The warning that lights when the output clips and stays lit until clicked.

Status: PENDING — B225, B438

## Inter-sample peaks and delivery

Why a ceiling of −1.0 dBFS leaves room for the peaks a converter or encoder creates between
samples, and what that means when delivering a mix.

Status: READY-TO-WRITE — ADR-195

[[FIG:master.intersample-peak]]
