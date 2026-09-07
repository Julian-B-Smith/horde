# RECOMMENDATION — Kuramoto LFO: a coupled modulation source for HYPERSAW

> **Editor's note (archived into HYPERSAW, 2026-07-20).** This is an external
> brief from the authoring kit, kept verbatim below except for `[repo: …]`
> reference annotations. Its ADR numbers are the KIT's numbering and DO NOT map
> to this repo. Authoritative mapping for HYPERSAW:
> - The whole design is **ADR-053** here (the kit's per-axis 048–052 all collide
>   with this repo's log — our ADR-052 is the entangled-mods proposal, not the rotor).
> - Prior-art posture is **ADR-044** here (not "ADR-018/048").
> - GUI seam **ADR-019** and the (Γ, W) objects **ADR-045** coincidentally match
>   this repo and are correct as written.
> - **The prototypes are NON-GOLDEN concept tests** (the human's word overrides
>   this brief's "headless-verified / parity oracle" language) — no acceptance
>   anchors exist here yet; the rotor must be hardened to a golden reference before
>   any port. See ADR-053 and `docs/design/kuramoto-lfo-prototypes/NON-GOLDEN.md`.

**Status:** end-of-prototyping report for the modulation-source thread. Four working prototypes attached — concept tests, **not golden references** [repo correction: this brief's "headless-verified" claim does not hold here]; all parked pending a Phase-4+ modulation subsystem. This is a design + carried-lessons brief, not a work order. Nothing here touches an in-flight increment. ADR numbers below (048–052) are the kit's DECISIONS.md numbering — they collide with this repo; the design is **ADR-053** here (see the editor's note).

**Audience:** the agent building HYPERSAW under the autonomous-paradigm doctrine — force-core consumed by every engine, parity oracle against the JS reference labs, append-only ADRs, Layer-0 guards.

---

## 1. What this is

The force-core, pointed at the **modulation matrix** instead of at audio. A population of LFOs whose per-voice attributes are swarm coordinates under the same coupling law the audio engines use. Coupling K sweeps the modulators from independent (free-running, analog-ensemble drift) through near-critical flicker to locked (one shared motion) — and, because K is bipolar here as everywhere in this project, past free into **splay** (maximally even interleave), which no unipolar competitor can reach.

This is a distinct instrument from the carrier-coupling engines (SAW/SPECTRA/dynamics): there the coupled phases *are* the audio; here they *modulate* it. It should ship as a **routable modulation primitive**, with a chorus as its first legible demo — not as a hardwired chorus effect. That framing is the single most important recommendation in this document.

## 2. Prior art (posture per ADR-018/048 [repo: ADR-044])

Two finds bracket this work, both recorded in docs/PRIOR-ART.md:
- **Lem/Kuroscillator** (research): coupled oscillators for synthesis, including trigger clocks. Research prior art for the base idea.
- **Chiral Audio "Foxfire"** (product): couples 16 modulator *phases* with one *unipolar* Coupling macro, hardwired to chorus delay taps. Closest product-level precedent.

This design was **conceived independently before Foxfire was found** and diverges at the first branch: per-voice *parameters* (not just phase) are swarm-native; K is *bipolar* (adds splay); the bank is a *routable primitive* (not a fixed chorus). Cite both; claim the mechanisms and the instrument architecture, not the base idea. Foxfire validates the vein commercially without occupying this design.

## 3. The four prototypes (attached, each a parity oracle)

The modulation swarm has four axes. Prototype them (and port them) **one at a time** — this was the directive that kept each one legible, and it's how the parity tests stay clean.

1. **`swarmlfo.html` — axis 1: RATE** (ADR-048/049 [repo: ADR-053]). Per-voice LFO *rate* is the coupled quantity. Sync → all voices at a common rate; splay → even-interleaved rates (the smooth wash); tempo gravity → rates snap to beat subdivisions. Routable to chorus/filter/pan demo destinations.
2. **`swarmlfo-depth.html` — axis 2: DEPTH** (ADR-050 [repo: ADR-053]). Per-voice modulation depth as a swarm quantity. **R→depth**: motion self-shapes with coherence (amplify or erase as the swarm locks). **Thin/attrition**: fade voices out one at a time (continuous fractional active count) with a soft/sharp fade edge.
3. **`swarmlfo-dest.html` — axis 3: DESTINATION** (ADR-051 [repo: ADR-053]). The radical one. Each voice's *routing target* is a swarm coordinate; a second coupling constant J converges the whole bank onto one destination or splays it across the patch. Modulation migrates through the patch as a body. Order parameter = destination concentration.
4. **`swarmlfo-rotor.html` — the shipping face** (ADR-052 [repo: ADR-053]). The straightforward playable instrument the axes were building toward: exactly 4 coupled LFOs mapped to wavetable morph / filter cutoff / chorus amount / saturation, bipolar K, a global LFO-shape selector (sine/tri/saw/square), and the **rotor** UI (dots at phase angles on one circle, order-parameter vector, radial value spokes). **This is the demo/onboarding instrument; axes 1–3 are the research depth behind it.** If only one thing ships first, ship this.

Measured behavior and acceptance anchors for each are in DECISIONS.md (ADR-048–052) [repo: those anchors are the KIT's and do NOT exist here — the prototypes are non-golden; harden the rotor to a golden reference, then reproduce against the JS reference per ADR-003].

## 4. Architecture recommendation

Build the Kuramoto LFO as a **modulation source object** consuming the existing force-core, with:
- a per-voice state vector whose fields are the swarm coordinates (phase always; then rate / depth / destination as the axes are added);
- routable **outputs** (the bank's per-voice values + order parameters R, and for axis 3 the destination-concentration parameter) exposed to a modulation bus — *not* hardwired to any one effect;
- the rotor (ADR-052 [repo: ADR-053]) as the default visualization behind the ADR-019 GUI seam (ADR-019 correct here).

Under the ADR-045 (Γ, W) axes [repo: ADR-045 correct here] this is the force-core with its output routed to parameters — but note the audible spine (the rotor) is PHASE-domain and reuses SwarmCore's coupling, not the position-domain force-core (see ADR-053 grounding); bipolar K, tempo/consonance gravity, drift (seeded, reproducible), and onset lock/dissolve all transfer from the audio engines unchanged. The chorus is one destination, not the architecture.

## 5. Carried-forward engineering lessons (bank these — they cost real debugging to find)

These four came out of building this family and generalize beyond it. Each is a Layer-0 / test-design rule, not a one-off fix.

**(a) Constructor-initialize every field the visualization reads.** The draw loop runs on `requestAnimationFrame` and starts *before* the first control tick — so any field the visualizer reads that is only assigned inside the control tick is `undefined` on the first frames and throws (`Cannot read properties of undefined`). This crashed axis 2 (`_rScale`). Fix: initialize every visualizer-read field in the constructor. The rotor (ADR-052 [repo: ADR-053]) was built with this applied preemptively and came up clean. **Port rule:** the GUI seam reads a struct that must be fully initialized at construction, because the render thread outruns the audio thread at startup.

**(b) A modulated parameter must sit in the signal's active range, or it reads as binary/one-sided.** Axis 2's filter destination felt "binary" — a one-pole lowpass centered at 800 Hz sat *above* the pad's spectral body, so opening it was inaudible and only closing it did anything: the control appeared dead across half its throw. Fix: center the modulated cutoff *inside* the signal's body (dropped to 500 Hz, symmetric ±2 octaves in log space). **Port rule:** when a modulation target has a "resting point" (filter cutoff, a crossfade midpoint), place that point where the signal actually has energy, and sweep symmetrically in the perceptual domain (log for frequency).

**(c) Measure modulation effects over MULTIPLE LFO cycles, never a single block.** This produced a false bug report *three times* this session (filter "binary", pan "mono", a dead-destination scare). A single audio block (~23 ms) is far shorter than an LFO cycle (~0.2–1 s), so within one block the modulation is nearly static and any per-block metric (RMS, L/R correlation, centroid) reads as if the modulation isn't happening. Every one of these was the audio being *fine* and the measurement window being wrong. **Test rule:** modulation acceptance tests integrate over several full LFO cycles (≥3–4 s at these rates); per-block measurement is invalid for anything slower than audio rate.

**(d) When the swarm coordinate is not itself the audible quantity, couple the audible quantity alongside it.** Axis 1's K knob was inaudible at first: it coupled the *rates* (correctly — measured) but left the LFO *phases* independent, and rate coherence is nearly inaudible through delay/filter/pan destinations. The ear tracks phase coherence (voices moving together vs independently), not rate coherence. Fix: phase coupling rides with K (gathering phases as rates lock, scaled by R; scattering at negative K). This is also just the correct Kuramoto chorus — Foxfire couples phases directly. **Design rule:** a control that modulates a swarm coordinate one step removed from what's heard must also drive the heard quantity, or it moves something inaudible. (This is the modulator-domain analogue of ADR-016: exonerate the DSP by measuring before blaming the UI.)

## 6. Corrected finding — the emergent-resonance pitch (supersedes an earlier claim)

Field observation during axis-2 work: at high K, more synced chorus voices raise the pitch of the emergent comb resonance ("room becomes a note" in the modulator). **Correct mechanism (user-supplied, verified):** the comb pitch is set by the *base delay-tap spacing*, not the voice count. Thinning voices changes the comb's **density, depth, and brightness — NOT its pitch**; the base spacing is untouched by thinning. An earlier prototype measurement appeared to show thinning sliding the pitch (2821→2522 Hz) — that was a **spectral-centroid/brightness shift misread as pitch** (a zero-crossing "pitch" proxy catching the timbre change). Do not build a "thin to graduate resonance pitch" control on that false premise.

The user's phase-compression intuition is the right refinement: at high K, more voices pack a *denser* population into the locked phase cluster, tightening the inter-voice phase deltas — a coherence/timbre effect, not a pitch one.

**If graduating the resonance *pitch* is wanted, the correct control modulates the base tap spacing directly** (a genuinely useful knob, flagged here for design consideration — it is *not* voice-thinning).

## 7. Definitions (for parameter naming/help text)

- **Drift** — a slow, seeded random walk on each voice's *rate* (not phase), mean-reverting toward the home rate. Keeps the bank alive and non-identical even at K=0; reproducible per seed; a disordering force opposing K.
- **Base depth** — master modulation amount; the ceiling that R→depth and thin/attrition multiply against. At 0, nothing moves.
- **Attrition** — the softness of the fade-edge as thin removes a voice (sharp = voices click out individually; soft = continuous dissolve of the population). Textural, not pitch-related.

## 8. What to build first

Ship **the rotor (ADR-052 [repo: ADR-053])** as the first playable modulation instrument — it's self-contained, legible, and demonstrates the whole idea. Add the axes (rate → depth → destination) behind it one at a time, each as a routable extension with its own Layer-0 acceptance rows reproducing the reference numbers. Keep the chorus as a *demo destination*, never the architecture.
