# Performance, CPU and quality

Purpose: help a player keep horde affordable in a busy session without guessing. The chapter
says what costs CPU, explains the quality option (standard by default, HQ oversampling on
demand, saved per preset), the voice limit, latency and tails, edge correction, and gives
measured costs per factory preset.

## What costs CPU

The engine and module settings that cost the most, measured.

Status: PENDING — B441, B262

[[TAB:perf.cost-drivers]]

## Quality: standard and HQ

Oversampling is a quality option: standard (1×) is the default and HQ (2×) is selectable and
saved per preset. Which sounds benefit from HQ, and what it costs.

Status: READY-TO-WRITE — ADR-191, B445

[[FIG:perf.aliasing-standard-vs-hq]]

[[AUD:perf.standard-vs-hq]]

[[TAB:perf.hq-cost]]

## Voices and polyphony

How many voices horde plays and what happens when a note needs one more.

Status: PENDING — B323, B310

## Latency and tails

What latency horde reports to the DAW, and why the plugin keeps processing while a tail rings.

Status: PENDING — B429

## Edge correction

How the engine keeps hard edges clean of aliasing, and the option chosen by ear.

Status: PENDING — B383, B380

## Lowering CPU

Practical steps, in order of payoff.

Status: PENDING — B441, ADR-191

[[TAB:perf.cpu-tips]]

## Measured cost by preset

The CPU of each factory preset on the reference machine.

Status: PENDING — B441, B395

[[TAB:perf.cpu-by-preset]]
