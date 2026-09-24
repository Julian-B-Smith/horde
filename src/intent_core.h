/*
 * intent_core.h — the intent-bus resolver (B89 phase 2a, ADR-176).
 *
 * WHAT LIVES HERE: SPEC-INTENT-BUS §4.1–§4.5 and §7 as pure math —
 * effective morph position, bilinear corner weights with `^steepness`,
 * the CUMULATIVE WALK that assigns an owner corner to each atom, the
 * performance pad's spring and its intent mapping, the five-tier parameter
 * evaluation with the corner clamp in the middle, and commit-to-dominant.
 *
 * WHAT DOES NOT: storage, parameter ids, modulation SOURCES, persistence,
 * the GUI. Every span is CALLER-OWNED and every function is static — the
 * `ModCore` pattern, not the `MorphCore` fat struct, because the binding
 * table is O(corners x intents x params) and belongs to whoever sizes the
 * field once (R14 of docs/proposals/b89-phase2-intent-resolver.md).
 * Allocation-free and wall-clock-free by construction (SPEC §5.7); the only
 * randomness is mulberry32, reused from MorphCore so a device seed means the
 * same stream here and in the shipped morph.
 *
 * OWNER LAW (ADR-176 decision 1). The owner is the spec's walk A->B->C->D:
 * the first corner whose cumulative sharpened weight exceeds the atom's seed.
 * This REPLACES MorphCore's Gumbel-max when the resolver runs — same ownership
 * statistics, but one seed per atom is ONE editable flip boundary per atom,
 * which is what the boundary editor needs. `morphCoup` survives as a
 * shared-seed blend, s' = (1-c)*s + c*s_shared (blendSeed below).
 *
 * ATOMS ARE LEAD GROUPS, NOT PARAMETERS (ADR-176 decision 2). The caller hands
 * in `atomOf[p]`; several parameters mapping to one atom flip together, which
 * is how the scale (one atom of 13) and each FX slot (one atom of 3) stay
 * coherent. Nothing here knows what a group means.
 *
 * Correctness = tools/intent_check.cpp: parity at 1e-6 against
 * reference/intent-bus.html (the protected prototype, ADR-003 spec-in-code)
 * over generated fixtures, plus invariants for the parts the prototype does
 * not span (the coupling blend has no prototype analogue). Not wired into the
 * audio path — core and oracle land together, unwired, the glide_core order;
 * the shell seam is phase 2b.
 */
#pragma once

#include <cmath>
#include <cstdint>

#include "morph_core.h"

namespace hypersaw
{

struct IntentCore
{
  static constexpr int kCorners = 4;

  /* The prototype's clamp-indicator threshold, not an exact inequality.
     SPEC §4.5 says `clamped[p] = (pre-clamp v != post-clamp v)`; the prototype
     writes `Math.abs(cv-v) > 1e-4` (reference/intent-bus.html, the parameter
     loop). Under ADR-003 the prototype IS the spec, so the threshold is the
     law and the exact-inequality reading is the divergence. It also happens to
     be the honest one: an exact test would light the indicator for a parameter
     sitting a rounding error outside its range. */
  static constexpr double kClampEps = 1e-4;

  static double clamp(double v, double lo, double hi)
  {
    return v < lo ? lo : (v > hi ? hi : v);
  }
  static double clamp01(double v) { return clamp(v, 0.0, 1.0); }

  /* ---- caller-owned span layout ---------------------------------------
     Every table is flat and indexed through these two helpers, so the shell
     can allocate one block per table in morphInit and nothing here ever needs
     a stride argument it could get wrong. */
  static int cornerParamIdx(int corner, int param, int nParams)
  {
    return corner * nParams + param;
  }
  static int bindIdx(int corner, int intent, int param, int nIntents, int nParams)
  {
    return (corner * nIntents + intent) * nParams + param;
  }

  /* ---- §3.5 seeds ------------------------------------------------------
     One seed per atom, drawn in atom-index order from the device seed, THEN
     the shared seed last. Appending the shared draw is load-bearing: it leaves
     the per-atom draws bit-identical to a stream without coupling, which is
     what makes the prototype's `reshuffle()` (8 params, home, unison) and this
     function agree atom for atom. Same ordering argument as MorphCore::
     reshuffle, which draws its params before its shared vector.
     `nPrefix` (B240), same law as MorphCore::reshuffle's: only the first
     nPrefix atoms are drawn before the shared seed, the rest after it, so the
     shell can append atoms without moving the shared seed or any earlier one.
     The default (every atom) is the prototype's order exactly. */
  static void drawSeeds(uint32_t deviceSeed, double *seeds, int nAtoms, double *sharedSeed,
                        int nPrefix = 1 << 30)
  {
    const int pre = nPrefix < nAtoms ? (nPrefix < 0 ? 0 : nPrefix) : nAtoms;
    uint32_t a = deviceSeed;
    for (int i = 0; i < pre; i++) seeds[i] = MorphCore::rnd01(a);
    if (sharedSeed != nullptr) *sharedSeed = MorphCore::rnd01(a);
    for (int i = pre; i < nAtoms; i++) seeds[i] = MorphCore::rnd01(a);
  }

  /* ---- §4.1 effective morph position ----------------------------------
     Device routings that target MorphX/MorphY are applied BEFORE anything
     else, because everything downstream reads the result. `modSum` is the
     caller's already-summed, already-scaled routing contribution — sources
     are not this header's business. */
  static double effectiveMorph(double axis, double modSum)
  {
    return clamp01(axis + modSum);
  }

  /* ---- §4.2 corner weights --------------------------------------------
     Bilinear, then sharpened by `^steepness` and normalised. steepness =
     1/morphTemp (ADR-176): 1 is a soft blend, large values approach hard
     flips. The normalising sum cannot be zero for steepness > 0 — x,y in
     [0,1] forces at least one bilinear weight >= 0.25. */
  static void weights(double x, double y, double steepness, double *w)
  {
    w[0] = (1.0 - x) * (1.0 - y);
    w[1] = x * (1.0 - y);
    w[2] = (1.0 - x) * y;
    w[3] = x * y;
    double s = 0.0;
    for (int k = 0; k < kCorners; k++) { w[k] = std::pow(w[k], steepness); s += w[k]; }
    for (int k = 0; k < kCorners; k++) w[k] /= s;
  }

  /* ---- §4.3 atom resolution -------------------------------------------
     The shared-seed blend IS `morphCoup` (param 155) under the walk law: at
     c = 0 every atom keeps its own boundary, at c = 1 every atom shares one,
     so the whole field flips together. The same interpolation MorphCore
     applies to its Gumbel vectors, one dimension instead of four. */
  static double blendSeed(double seed, double sharedSeed, double coup)
  {
    return (1.0 - coup) * seed + coup * sharedSeed;
  }

  /* The walk: accumulate A->B->C->D, first corner whose cumulative weight
     exceeds the seed. STRICT `<` and the D fallback are the prototype's
     (`pick()`), and the fallback is not dead code — floating-point
     normalisation can leave the accumulated sum a hair under a seed of
     0.9999. */
  static int ownerOf(double seed, const double *w)
  {
    double acc = 0.0;
    for (int k = 0; k < kCorners; k++)
    {
      acc += w[k];
      if (seed < acc) return k;
    }
    return kCorners - 1;
  }

  static void resolveAtoms(const double *seeds, int nAtoms, double sharedSeed, double coup,
                           const double *w, int *ownerAtom)
  {
    for (int a = 0; a < nAtoms; a++)
      ownerAtom[a] = ownerOf(blendSeed(seeds[a], sharedSeed, coup), w);
  }

  /* atomOf[] is the lead map (ADR-176 decision 2): parameters sharing an atom
     index resolve to one owner, always. */
  static void mapOwners(const int *ownerAtom, const int *atomOf, int nParams, int *ownerParam)
  {
    for (int p = 0; p < nParams; p++) ownerParam[p] = ownerAtom[atomOf[p]];
  }

  /* ---- §4.4 performance pad -------------------------------------------
     Mass-spring-damper, semi-implicit Euler with BOTH velocities integrated
     before either position — the prototype's statement order, and it is not
     interchangeable with the symmetric form at these stiffnesses. Constants
     are inputs, not literals: they are in 1/s^2 and 1/s and meet dt here, so
     ADR-009's ban on hand-tuned per-tick constants is satisfied by
     construction. */
  struct Puck
  {
    double x = 0.5, y = 0.5, vx = 0.0, vy = 0.0;
  };

  struct Spring
  {
    double kRest = 90.0, dRest = 11.0;    // returning to home
    double kDrag = 600.0, dDrag = 40.0;   // stiffer while the user drags
  };

  /* latch && !dragging is the one case with NO target: the puck is parked
     where it was left and its velocity is killed, so displacement is measured
     from whatever home is current (§5, Latch). */
  static void padStep(Puck &pk, double dt, bool dragging, double dragX, double dragY,
                      bool latch, double homeX, double homeY, const Spring &sp)
  {
    double tx = 0.0, ty = 0.0;
    bool hasTarget = true;
    if (dragging) { tx = dragX; ty = dragY; }
    else if (!latch) { tx = homeX; ty = homeY; }
    else { hasTarget = false; }

    if (hasTarget)
    {
      const double k = dragging ? sp.kDrag : sp.kRest;
      const double d = dragging ? sp.dDrag : sp.dRest;
      pk.vx += (k * (tx - pk.x) - d * pk.vx) * dt;
      pk.vy += (k * (ty - pk.y) - d * pk.vy) * dt;
      pk.x += pk.vx * dt;
      pk.y += pk.vy * dt;
    }
    else { pk.vx = 0.0; pk.vy = 0.0; }

    pk.x = clamp01(pk.x);
    pk.y = clamp01(pk.y);
  }

  /* Full intent swing is +-0.5 of the pad extent from home, hence the x2.
     Y is inverted: up is positive. The clamp is on the PAD term only — a
     device routing added afterwards may legitimately push an intent past
     +-1, which is the prototype's behaviour for `intentX`. */
  static double padIntentX(const Puck &pk, double homeX) { return clamp((pk.x - homeX) * 2.0, -1.0, 1.0); }
  static double padIntentY(const Puck &pk, double homeY) { return clamp((homeY - pk.y) * 2.0, -1.0, 1.0); }

  /* ---- §4.5 parameter evaluation --------------------------------------
     Order is the whole point, so it is spelled out in three named steps
     rather than one expression:

       intentContrib   base + SUM_i intents[i] * bind[owner][i][p]
       evalCornerTier  + corner-scope mods, then CLAMP TO THE OWNER'S RANGE
       evalParam       + promoted mods + device mods, clamp01

     The clamp sits between the tiers, which is what makes a corner's range
     load-bearing for its own bindings and NOT for a promoted routing. */
  static double intentContrib(const double *intents, const double *bind, int corner,
                              int param, int nIntents, int nParams)
  {
    double s = 0.0;
    for (int i = 0; i < nIntents; i++)
      s += intents[i] * bind[bindIdx(corner, i, param, nIntents, nParams)];
    return s;
  }

  static double evalCornerTier(double base, double intentSum, double cornerModSum,
                               double lo, double hi, int *clampedOut)
  {
    const double v = base + intentSum + cornerModSum;
    const double cv = clamp(v, lo, hi);
    if (clampedOut != nullptr) *clampedOut = (std::fabs(cv - v) > kClampEps) ? 1 : 0;
    return cv;
  }

  /* `respectRange` is the DEVICE tier's flag and the caller passes it true
     ONLY for a parameter an armored device routing actually targets — the
     prototype re-clamps inside `if (gt === p)`, so a parameter with no device
     routing is never re-clamped no matter how the armor toggle is set. */
  static double evalParam(double cornerValue, double promotedModSum, double deviceModSum,
                          bool respectRange, double lo, double hi)
  {
    double v = cornerValue + promotedModSum + deviceModSum;
    if (respectRange) v = clamp(v, lo, hi);
    return clamp01(v);
  }

  /* The whole field in evaluation order. The three mod spans and the
     respectRange span may be null, meaning "zero / off everywhere" — phase 2
     ships with cornerModSum null (R5: corner-scope routings are phase 3). */
  static void stepParams(int nParams, int nIntents, const double *intents,
                         const int *ownerParam, const double *base, const double *bind,
                         const double *rangeLo, const double *rangeHi,
                         const double *cornerModSum, const double *promotedModSum,
                         const double *deviceModSum, const int *respectRange,
                         double *finalOut, int *clampedOut)
  {
    for (int p = 0; p < nParams; p++)
    {
      const int c = ownerParam[p];
      const int cp = cornerParamIdx(c, p, nParams);
      const double lo = rangeLo[cp], hi = rangeHi[cp];
      const double cv = evalCornerTier(base[cp],
                                       intentContrib(intents, bind, c, p, nIntents, nParams),
                                       cornerModSum != nullptr ? cornerModSum[p] : 0.0,
                                       lo, hi,
                                       clampedOut != nullptr ? &clampedOut[p] : nullptr);
      finalOut[p] = evalParam(cv,
                              promotedModSum != nullptr ? promotedModSum[p] : 0.0,
                              deviceModSum != nullptr ? deviceModSum[p] : 0.0,
                              respectRange != nullptr && respectRange[p] != 0,
                              lo, hi);
    }
  }

  /* ---- §7 commit -------------------------------------------------------
     Dominant = argmax weight, ties to the earliest corner (the prototype
     starts its scan at A with a best of 0 and takes strictly greater). */
  static int dominantCorner(const double *w)
  {
    int dom = 0;
    double best = 0.0;
    for (int k = 0; k < kCorners; k++)
      if (w[k] > best) { best = w[k]; dom = k; }
    return dom;
  }

  /* Bake the current offsets into the dominant corner for the parameters it
     OWNS, re-home it on the puck, and zero the non-pad intents. Corner-scope
     mod contributions are deliberately NOT baked (§7.1 — they are dynamic,
     R11 keeps it that way for phase 2). The invariant this buys: after
     commit the resolved sound at this morph position is unchanged and every
     offset reads zero, so a saved corner never depends on a displaced
     control.

     `padDriven[i] != 0` marks an intent the pad computes from displacement;
     those are not zeroed here because re-homing on the puck already zeroes
     them on the next tick. Everything else is a knob and is zeroed outright
     (§7.3's "otherwise" branch — per-corner macroRest is not built). */
  static void commit(int nParams, int nIntents, const double *intents, const int *ownerParam,
                     const double *w, double *base, const double *bind,
                     const double *rangeLo, const double *rangeHi,
                     const Puck &pk, double *homeX, double *homeY,
                     double *intentKnob, const int *padDriven, int *dominantOut)
  {
    const int dom = dominantCorner(w);
    if (dominantOut != nullptr) *dominantOut = dom;
    for (int p = 0; p < nParams; p++)
    {
      if (ownerParam[p] != dom) continue;
      const int cp = cornerParamIdx(dom, p, nParams);
      const double v = base[cp] + intentContrib(intents, bind, dom, p, nIntents, nParams);
      base[cp] = clamp(v, rangeLo[cp], rangeHi[cp]);
    }
    homeX[dom] = pk.x;
    homeY[dom] = pk.y;
    if (intentKnob != nullptr)
      for (int i = 0; i < nIntents; i++)
        if (padDriven == nullptr || padDriven[i] == 0) intentKnob[i] = 0.0;
  }
};

}  // namespace hypersaw
