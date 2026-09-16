/*
 * mod_core.h — the modulation matrix, increment 1: routes and the combination
 * law, nothing else. Framework-free, preallocation-only, deterministic — the
 * same charter rules as every core (no wall-clock, no allocation after
 * construction, seeded streams only if randomness ever arrives).
 *
 * WHAT LIVES HERE: the route table (source → destination, depth, polarity,
 * scope), the per-route polarity map (B134) and `evaluate()`, which turns a
 * vector of source values into per-destination deltas by the combination law. The law is SUM — each route contributes
 * depth · source, contributions to one destination add. Chosen to match the
 * standard practitioner shape and FOUNDATIONS §3.1's framing (their register
 * asks *when* sources update, not *how* they combine; the sum answers the
 * settled half). Bounding is deliberately NOT here: per their OQ-30 ruling the
 * destination's own modMin/modMax bound the applied value, and application is
 * the SHELL's act — a core that clamped would hide the shell's failure to.
 *
 * WHAT DOES NOT LIVE HERE: sources (the shell feeds values in — ENV 1 is the
 * amp envelope the shell already owns, an LFO arrives with B16), parameter
 * application, per-note fan-out, and the GUI. Scope is carried per route
 * (kGlobal / kPerNote, the B34 vocabulary) so the shell can evaluate the two
 * populations at their own cadences, but WHAT per-note means is the shell's
 * tiering, not ours.
 *
 * NO CYCLES BY CONSTRUCTION in this increment: sources are primitive slots,
 * not destinations, so a route cannot read a route. When a source can be a
 * destination (macro-of-macros), FOUNDATIONS OQ-23's block-rate unit delay is
 * the ruled semantics for the CONTROL graph — recorded here so the future
 * increment inherits a decision instead of re-deriving one.
 *
 * Correctness = tools/mod_check.cpp (invariants, calibrated), never
 * "plausible modulation". Not yet wired into the audio path — core and oracle
 * first, shell integration behind its own increment (the glide_core order).
 */
#pragma once

#include <cstdint>

namespace hypersaw
{

struct ModCore
{
  static constexpr int kMaxSources = 24;   // ADR-149: MIDI/MPE sources joined at 14-17
  static constexpr int kMaxRoutes = 64;

  enum Scope : int { kGlobal = 0, kPerNote = 1 };

  /* B134 — POLARITY IS A PROPERTY OF THE ROUTE, NOT OF THE SOURCE. The same
     modulator may drive a filter unipolar and a pan bipolar in one patch, so
     the setting rides the route. `kAsIs` is the default and means "whatever
     the source naturally is" — it is the no-op branch below, which is what
     makes every pre-B134 patch bit-inert rather than merely close. */
  enum Polarity : int { kAsIs = 0, kUnipolar = 1, kBipolar = 2, kInverted = 3 };

  /* What a SOURCE naturally emits. The table itself is the shell's (it knows
     what slot 17 is); the core only needs the flag, which keeps this header
     framework-free. Default unipolar: a source nobody has declared reads 0..1,
     the shape every envelope/macro/MIDI signal here has. */
  enum SrcPolarity : int { kSrcUnipolar = 0, kSrcBipolar = 1 };

  struct Route
  {
    uint32_t src = 0;       // source slot [0, kMaxSources)
    uint32_t dest = 0;      // destination key (the shell's param id; opaque here)
    double depth = 0;       // signed; the route's whole authority
    int polarity = kAsIs;   // B134; applied to the source value BEFORE depth
    int scope = kGlobal;    // B34 vocabulary: who fans this out, and when
    int active = 0;
  };

  Route routes[kMaxRoutes];
  int nRoutes = 0;
  double src[kMaxSources] = {0};   // written by the shell each control tick
  int srcPol[kMaxSources] = {0};   // B134; the shell's table, handed in once

  /* The polarity map, B134. Nothing clamps here — the destination's own
     modMin/modMax bound the applied value (OQ-30), the same division of labour
     evaluate() already keeps.

     kAsIs RETURNS v UNTOUCHED, not `v * 1.0` or `(v + 0) `: the as-is result
     must be the same double a build without this field produced, so the
     control in polarity_check ("pol 0 renders identically") is an exact
     equality and not a tolerance. */
  static double mapPolarity(double v, int polarity, int sourcePolarity)
  {
    switch (polarity)
    {
      case kUnipolar: return sourcePolarity == kSrcBipolar ? (v + 1.0) / 2.0 : v;
      case kBipolar:  return sourcePolarity == kSrcBipolar ? v : 2.0 * v - 1.0;
      case kInverted: return -v;
      default:        return v;
    }
  }

  /* Add fails — rather than silently dropping — on a full table or an
     out-of-range source. The caller surfaces the refusal; a matrix that eats
     routes teaches the player the feature is broken. */
  bool addRoute(uint32_t source, uint32_t dest, double depth, int scope, int polarity = kAsIs)
  {
    if (nRoutes >= kMaxRoutes || source >= (uint32_t)kMaxSources) return false;
    routes[nRoutes] = {source, dest, depth, polarity, scope, 1};
    nRoutes++;
    return true;
  }

  void removeRoute(int idx)
  {
    if (idx < 0 || idx >= nRoutes) return;
    for (int i = idx; i < nRoutes - 1; i++) routes[i] = routes[i + 1];
    nRoutes--;
  }

  void clear() { nRoutes = 0; }

  /* The combination law. Fills (dests[], deltas[]) with one entry per distinct
     destination among the ACTIVE routes of `scope`, deltas summed in route
     order. Returns the entry count. O(n²) compaction over ≤64 routes at
     control rate — a map would be an allocation for a problem this small.
     Caller owns the arrays (≥ kMaxRoutes entries); the audio thread allocates
     nothing here. */
  int evaluate(int scope, uint32_t *dests, double *deltas, int maxOut) const
  {
    int n = 0;
    for (int r = 0; r < nRoutes; r++)
    {
      const Route &q = routes[r];
      if (!q.active || q.scope != scope) continue;
      const double d = q.depth * mapPolarity(src[q.src], q.polarity, srcPol[q.src]);
      int at = -1;
      for (int i = 0; i < n; i++)
        if (dests[i] == q.dest) { at = i; break; }
      if (at >= 0) deltas[at] += d;
      else if (n < maxOut)
      {
        dests[n] = q.dest;
        deltas[n] = d;
        n++;
      }
    }
    return n;
  }
};

}  // namespace hypersaw
