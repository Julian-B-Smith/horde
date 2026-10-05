# Per-corner FX chains: the round-2 design brief (B265)

> **Origin.** horde lead session, 2026-10-05. This synthesises the five-lane research swarm the
> human dispatched ("Go with the full swarm") after asking for per-corner FX chains with "elegant
> math" (B265 reopened under ADR-192). The reports are gitignored, local to the lead's checkout
> (`local/research/reports/FX chain morph — lane 1…5`); this brief carries their conclusions, each
> tagged with its lane. **Status: PROPOSED.** Nothing here is ruled. Section 4 lists the human's
> decisions.

## 1. What round 1 settled, and what changed

Round 1 (lab #755) solved two endpoints. ONE shared router under any ordering algorithm holds the
edge invariants (I1 one per type, I2 acyclic but one bounded return, I3 no path that exists at no
end, I6 every lane reaches the output, I7 no output without input). The ordering algorithm only
chooses WHICH orders the chain passes through.

The new problem: four corners from unrelated presets (ADR-192's discovery model), one instance per
module type (the human's proposal), quantum and blend modes.

**Independent convergence.** Lanes 1 and 4 reached the same shape from different literatures:
compute a weighted TARGET from the four corners, CONSTRAIN it to approved chains, and use seeded
jitter that vanishes at the corners as the temperature.

## 2. The proposed pipeline (control rate, planned off the audio thread)

1. **Identity: one instance per type** (lane 5 verdict; precedent Ozone, Neutron, Serum 1).
   - The same module in two corners is the same object, so there is no correspondence problem.
   - The cap is a named constant (1 now), so it can be raised later with rank-order correspondence.
   - All module types stay resident and preallocated at `activate`, plus a small pool of S shadow
     instances for handing over stateful modules.
2. **Presence.** Each type's presence is a gain driven by the bilinear corner weights. Quantum mode
   flips it at a seeded threshold; blend mode fades it.
3. **Order target** (lane 1, F1). Each module scores the weighted mean of its rank across the four
   corners. Absent modules take a fixed "ghost" rank from a house order. Sort each tick.
   - Corners are reproduced exactly, and every event is one adjacent swap.
   - If all four corners put A before B, the result never puts B first. This held at 504,300 grid
     points, measured.
   - A seeded per-module jitter that vanishes at the corners cuts stacked events from up to 10 to 3,
     and is the temperature.
4. **Constrain** (lane 4, F1; lane 1, F2). Snap the target to the nearest chain in an approved set
   V: perturb-and-MAP decode, or one hop per slot on the approved-order graph.
   - Every intermediate is approved.
   - Lane 4's toy measured the lowest boundary jump of the options it tried.
   - **Corner sharpening** (weights w^γ) gives the untouched presets more of the pad: 3–6 % → 25 %
     at γ = 2, measured.
5. **Route** (round 1's router, plus lane 2):
   - one atomic event per "bubble" (a contiguous region that differs);
   - add downstream first, remove upstream first (agreeing with OSPF ordered-FIB, RFC 6976, and
     Reitblatt 2012);
   - each make is tied to its break in one split group, with edge weights u / ‖u‖_p, which keeps
     lane gain on a simplex. This fixes round 1's 2–3× loudness: worst case ±1.5 dB for two paths at
     p = 4/3. p = 1 or 2 avoid `pow`;
   - at most K = 2–3 open bubbles per lane;
   - plans are applied at a block boundary.
6. **Inside a module** (lane 3):
   - continuous parameters blend;
   - discrete ones flip at a seeded per-setting threshold with hysteresis, under a short wet dip
     (duck, swap, un-duck);
   - a dual-engine crossfade inside the module only where the dip is audible (reverb);
   - each module DECLARES each parameter's class (blend / flip / flip-with-dip / dual-engine).
     That is a contract for every FX sibling.
7. **Audio constraints** (lane 5):
   - **Latency is a plugin constant, never a morph variable.** CLAP allows a change only at
     `activate`; VST3's restart can interrupt playback. Modules are zero-latency in 1.0, or declare
     one fixed latency and leave a delay pad when absent.
   - Crossfade per module, never whole chains, which would double the FX CPU.
   - A leaving module rings out until quiet (Bitwig's FX Selector pattern).
   - A test counts audio-thread allocations over a scripted insert / remove / reorder morph.
   - Never signal `tail_changed`. The pinned clap-wrapper maps it to a VST3 latency restart, with
     no null check. This joins B429, alongside the known SLEEP-cuts-tails trap.
8. **Discovery beyond the morph** (lane 3): Reaktor-style per-parameter random merge and
   Synplant-style mutation produce states that exist at no corner. They therefore belong to an
   explicit "Discover" action that writes a NEW corner, never to the live morph.

## 3. Acceptance tests carried into the lab

- **Edge reduction** (lane 4, derived): on each pad edge, the four-corner scheme must reproduce
  round 1's two-corner metrics on the same endpoint pair.
- **Unanimity**: no pair order that all four corners agree on is ever inverted (lane 1).
- **Lane gain** inside [floor, +3 dB] at every position, with a break-before-make must-fail plant
  (lane 2, R1).
- **Coverage**: distinct intermediate chains, boundary jump sizes, and the pure-preset area fraction.
- **Allocation count**: zero on the audio thread across a scripted morph (lane 5).

## 4. Decisions for the human

| # | Question | Options | Lead's recommendation |
|---|---|---|---|
| D1 | What does "no audio path mid-morph that exists at no corner" (I3) mean? | (a) order level: no pair order all corners agree on is inverted; (b) adjacency level: module X feeds Y only if it does at some corner; (c) whole-path | **(a), plus "at most one switch point per transient path"** (lane 2's monotone sweep). Strict whole-path is unreachable for pairs like `ABC → BD` (two differing regions around a shared B) without a shadow instance, which breaks one-per-type (lane 2, computed). (b) cuts discovery by roughly an order of magnitude (lane 4). |
| D2 | How much of the pad should sound like the untouched corner presets? | γ = 1 (3–6 %), 2 (~25 %), 3 (~42 %), or per-preset "stickiness" | **γ = 2 by default**, per-preset stickiness later |
| D3 | How is the approved set V defined? | curated list; rule-based partial order; both | **A rule-based house partial order** (for example, a dynamics stage never after a reverb) **plus curated exceptions**, settled by a listening pass |
| D4 | Are two-change jumps at some boundaries acceptable? | accept; force single steps (the untested "band splice") | **Accept, measure and show them**; revisit after listening |
| D5 | When a removed delay or reverb is still ringing, does I7 allow the tail? | STRICT; TAIL-ALLOWED | **TAIL-ALLOWED** (Bitwig and Ableton both do it; lanes 3 and 5) |
| D6 | Latency | zero-latency modules only in 1.0; fixed declared latency per lookahead module | **Zero-latency morphable modules in 1.0**. The master limiter's 1.5 ms is constant and outside the morph. A lookahead mode elsewhere is a restart-time toggle. |
| D7 | Legacy presets that use duplicate modules (today's rack allows up to four of most types) | migrate to one-per-type; keep legacy's own rack | **horde 2 is one-per-type from the start**. A legacy import keeps the first instance of each type and reports what it dropped. |
| D8 | Normalisation p and open-bubble cap K | p ∈ {1, 4/3, 2}; K ∈ {2, 3} | **Decided by ear in the lab**; p = 2 as the starting default (no `pow`) |

## 5. Next

Lab round 2 (B265) builds the section 2 pipeline with four corners. Lane 1's F1 and lane 4's F1
decode run side by side, with the section 3 tests, the router changes, and a discrete-flip
demonstration. After the human's listening pass, a notice goes to every FX sibling with the
parameter-class contract (2.6) and the latency rule (2.7).
