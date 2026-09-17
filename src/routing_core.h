/*
 * routing_core.h — the B23 crosspoint routing matrix (ADR-088), framework-free.
 *
 * A slot may be fed from any SOURCE and from any EARLIER slot. Each crosspoint
 * carries a continuous coefficient; each slot carries an INITIAL VALUE, so
 * `out_i = in_i + Σ g_ki · m_k` — the canonical crosspoint form (Brandtsegg,
 * Saue & Johansen, NIME 2011), which the routing lab's scheme C lacked until
 * the amendment.
 *
 * WHY DENSE AND NOT A SPARSE EDGE LIST. Quantum morph interpolates VALUES, and
 * in a dense table a coefficient of 0 *is* "not connected" — so connecting and
 * disconnecting are one continuous motion. Every sparse representation stores
 * topology as discrete structure, making an edge add/remove a hard cut. That is
 * what decided ADR-088; the cost argument (88 patch-state values, 8 automation
 * ids) merely failed to disqualify it.
 *
 * ACYCLICITY IS ENFORCED ON THE READ SIDE, ALWAYS. A slot may only read
 * strictly earlier slots, so one forward pass is always correct and no runtime
 * cycle detection is needed — an audio thread cannot afford it. The legality
 * test lives in `edgeLive()` and every consumer calls it: the signal sum, the
 * output sum, the terminal test and any renderer of the graph. It is NOT
 * enforced at the editor, because the writer set is open — preset load, morph
 * corners and automation all write topology, so a guard on the write path is
 * bypassable by construction. (The routing lab proved this the expensive way:
 * a guard in the select handler let a backwards edge through when set directly
 * on the model, and a second copy of the "is this slot consumed?" logic that
 * skipped the check silently removed a slot from the output.)
 *
 * FX-AGNOSTIC BY CONSTRUCTION. This owns the topology and nothing else — the
 * caller supplies slot processing through a callable. That keeps the matrix
 * testable against a trivial stand-in slot (so the oracle measures ROUTING and
 * never an effect), and it is the shape that transfers: a routing facility that
 * names an effect type is not a routing facility.
 */
#pragma once

#include <cstdint>

namespace hypersaw
{

template <int NSRC, int NSLOT>
struct RoutingMatrix
{
  static_assert(NSRC + NSLOT <= 32, "crosspoint mask is a uint32");

  // Bit layout of inFrom[slot]: bits [0, NSRC) are sources; bit NSRC+e is
  // slot e. One word per destination keeps a whole row in a register.
  uint32_t inFrom[NSLOT] = {0};
  double coeff[NSRC + NSLOT][NSLOT] = {{0}};   // crosspoint gain, [from][to]
  double slotInit[NSLOT] = {0};                // the `in_i` term
  double outAmount[NSLOT] = {0};               // terminal contribution

  /* THE DRY PATH (B50 phase 1c). A source's own contribution to the output,
     bypassing every slot: `y += Σ_s srcOut[s]·src[s]`. Without it a source can
     reach OUT only THROUGH a slot, so a fully bypassed rack is not expressible
     as an edge — which contradicts this matrix's own premise that bypass is an
     edge property and not a module duty (ADR-088: a coefficient of 0 *is* "not
     connected"). Zero on the serial chain, so the default renders bit
     identically to the pre-1c engine. */
  double srcOut[NSRC] = {0};

  /* A default-constructed matrix connects nothing, and "nothing connected"
     means every outAmount is 0 — i.e. SILENCE. That is the worst possible
     way for an initialization slip to fail, so the constructor establishes the
     serial chain and the zero state stays reachable only by asking for it. The
     charter's "safety by construction, not by vigilance" applied literally. */
  RoutingMatrix() { setSerialChain(); }

  /* THE legality predicate. Sources may reach anything; a slot may reach any
     slot, including itself. Every consumer calls this — see the header note on
     why it cannot live at the write sites.

     WIDENED (ADR-128, 2026-08-27) from "a slot may reach only a LATER slot".
     Backwards and self edges are now legal and carry a ONE-SAMPLE delay: they
     read `zPrev`, the previous sample's slot output, so the graph stays a
     single forward pass and the audio thread still needs no cycle detection.
     The old strictness bought exactly that property; the delay buys it back
     without forbidding the topology.
     Block rate was rejected for this: at 128 samples / 44.1 kHz a block-delayed
     loop is 2.9 ms — a flanger, not a routing primitive — and it would change
     with the host's buffer size, which our own determinism rule forbids. */
  static constexpr bool edgeLive(int, int) { return true; }

  /* Does this edge read the CURRENT pass, or the previous sample? A source, or
     a slot strictly earlier than the destination, has already been computed
     this pass. Anything else is a cycle edge and reads `zPrev`. */
  static constexpr bool edgeForward(int from, int to)
  {
    return from < NSRC ? true : (from - NSRC) < to;
  }

  /* Feedback state: each slot's output from the previous sample. Zero until a
     backwards edge exists, so a graph with no cycle never reads it and stays
     byte-identical to the forward-only engine — which is what keeps every
     golden and all nine routing invariants green. */
  double zPrev[NSLOT] = {0};
  void resetFeedback() { for (int t = 0; t < NSLOT; t++) zPrev[t] = 0.0; }

  bool connected(int from, int to) const
  {
    return edgeLive(from, to) && (inFrom[to] & (1u << from)) != 0;
  }

  /* The default topology: a plain serial chain, source 0 → slot 0 → slot 1 →
     … → out. Reproduces the pre-routing rack exactly, which is what lets the
     matrix land inert and keeps the goldens as the regression proof. */
  void setSerialChain()
  {
    for (int t = 0; t < NSLOT; t++)
    {
      inFrom[t] = 0;
      slotInit[t] = 0;
      outAmount[t] = (t == NSLOT - 1) ? 1.0 : 0.0;
      for (int f = 0; f < NSRC + NSLOT; f++) coeff[f][t] = 0.0;
    }
    for (int s = 0; s < NSRC; s++) { srcOut[s] = 0.0; inFrom[0] |= (1u << s); coeff[s][0] = 1.0; }
    for (int t = 1; t < NSLOT; t++)
    {
      inFrom[t] |= (1u << (NSRC + t - 1));
      coeff[NSRC + t - 1][t] = 1.0;
    }
  }

  /* A slot nobody reads is an output. Computed rather than stored so it cannot
     disagree with the edges — the lab's bug was a second copy of exactly this
     question that skipped the legality test. */
  bool isTerminal(int slot) const
  {
    // The loop bound `t > slot` is what enforces legality here — an edge out of
    // `slot` into a LATER slot is legal by definition, so `connected()`'s
    // acyclicity test can never reject one of these. Verified by planting the
    // lab's actual bug (the raw bit test, no legality check) and finding it a
    // NO-OP against this shape. `connected()` is kept anyway because it also
    // tests the presence bit and because a future change to the bound would
    // otherwise silently lose the check — but the protection is structural, and
    // claiming the call provides it would be a comment that lies.
    /* FORWARD consumption only. A slot read solely by a feedback edge is still
       the end of the forward chain and therefore still an output — otherwise
       closing a loop would silently mute the very slot that feeds it. */
    for (int t = slot + 1; t < NSLOT; t++)
      if (connected(NSRC + slot, t)) return false;
    return true;
  }

  /* One forward pass. `src[i]` are the source samples; `proc(slot, x)` returns
     that slot's output for input x. Returns the summed terminal output.
     Allocation-free and branch-simple: this runs on the audio thread. */
  template <class Proc>
  double process(const double *src, Proc &&proc)
  {
    double slotOut[NSLOT];
    for (int t = 0; t < NSLOT; t++)
    {
      double x = slotInit[t];
      for (int f = 0; f < NSRC + NSLOT; f++)
      {
        if (!connected(f, t)) continue;
        /* Forward edges read this pass; cycle edges read the previous sample.
           One branch, no cycle detection, no ordering constraint. */
        const double a = f < NSRC              ? src[f]
                         : edgeForward(f, t)   ? slotOut[f - NSRC]
                                               : zPrev[f - NSRC];
        x += coeff[f][t] * a;
      }
      slotOut[t] = proc(t, x);
    }
    for (int t = 0; t < NSLOT; t++) zPrev[t] = slotOut[t];
    double y = 0;
    for (int t = 0; t < NSLOT; t++)
      if (isTerminal(t)) y += outAmount[t] * slotOut[t];
    for (int s = 0; s < NSRC; s++) y += srcOut[s] * src[s];   // the dry path
    return y;
  }

  /* Block-stereo forward pass — the same topology, through the same predicates,
     for slots that a scalar callable cannot express: the real rack slots are
     STEREO (a compressor detects on both channels), STATEFUL, and BLOCK-based
     (a comb needs contiguous samples). It lives here and not in the shell
     precisely because the shell must never own a second copy of "which edges
     are live" or "which slot is an output" — that duplication is the bug the
     header note describes, and re-deriving it against real audio is where it
     would be least visible.

     Scratch is CALLER-OWNED (`slotL/slotR`, NSLOT buffers of >= n): the audio
     thread allocates nothing. `proc(slot, L, R, n)` transforms a slot's buffer
     IN PLACE, which is also what makes an Off slot a bit-exact passthrough —
     it simply does not touch the gathered input.

     `outL/outR` may alias a source buffer (the shell passes the mix bus as both
     source and destination). Safe because the output is not written until every
     slot has gathered; keep that ordering if this is ever restructured. */
  template <class Proc>
  void processBlock(const float *const *srcL, const float *const *srcR,
                    float *const *slotL, float *const *slotR,
                    float *outL, float *outR, int n, Proc &&proc) const
  {
    for (int t = 0; t < NSLOT; t++)
    {
      const float init = (float)slotInit[t];
      for (int i = 0; i < n; i++) { slotL[t][i] = init; slotR[t][i] = init; }
      for (int f = 0; f < NSRC + NSLOT; f++)
      {
        if (!connected(f, t)) continue;
        const double g = coeff[f][t];
        const float *aL = f < NSRC ? srcL[f] : slotL[f - NSRC];
        const float *aR = f < NSRC ? srcR[f] : slotR[f - NSRC];
        for (int i = 0; i < n; i++)
        {
          slotL[t][i] += (float)(g * aL[i]);
          slotR[t][i] += (float)(g * aR[i]);
        }
      }
      proc(t, slotL[t], slotR[t], n);
    }
    /* THE DRY PATH INITIALISES THE OUTPUT rather than being added after it, and
       that ordering is forced by the aliasing note above: `outL/outR` may BE a
       source buffer, so a zero-then-add-the-sources pass would clear the very
       samples it then reads. Every source is read at index i before outL[i] is
       written, and no later iteration revisits index i, so the fused form is
       safe under aliasing where two passes are not. With srcOut all zero (the
       serial chain) this writes the same zeros the old loop did. */
    for (int i = 0; i < n; i++)
    {
      double dl = 0, dr = 0;
      for (int s = 0; s < NSRC; s++)
      {
        dl += srcOut[s] * srcL[s][i];
        dr += srcOut[s] * srcR[s][i];
      }
      outL[i] = (float)dl; outR[i] = (float)dr;
    }
    for (int t = 0; t < NSLOT; t++)
    {
      if (!isTerminal(t)) continue;
      const double a = outAmount[t];
      for (int i = 0; i < n; i++)
      {
        outL[i] += (float)(a * slotL[t][i]);
        outR[i] += (float)(a * slotR[t][i]);
      }
    }
  }
};

}  // namespace hypersaw
