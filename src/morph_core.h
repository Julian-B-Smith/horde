// morph_core.h — the quantum morph engine (ADR-104), ported from
// docs/design/quantum-morph-lab.html. The lab is the reference: bilinear
// corner weights, per-parameter Gumbel-max corner assignment (temperature +
// coupling + authored bias enter ONE score), seeded mulberry32 streams.
// Deterministic by construction: same seed + same position -> same assignment,
// forever, on every machine (SPEC §5.7 — no wall-clock, seeded RNG only).
//
// What lives HERE is the pure math: weights, draws, assignment, blending.
// What does NOT: parameter ids, capture, persistence — the SHELL owns which
// parameters morph and what a corner snapshot is, the same division that keeps
// glide_core free of a scale table.
#pragma once
#include <cmath>
#include <cstdint>

namespace hypersaw {

struct MorphCore
{
  static constexpr int kCorners = 4;
  static constexpr int kMaxParams = 512;

  // mulberry32 — the reference's PRNG, bit-for-bit (it is the project standard).
  static uint32_t mulberry32(uint32_t &a)
  {
    a += 0x6D2B79F5u;
    uint32_t t = a;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return t ^ (t >> 14);
  }
  static double rnd01(uint32_t &a)
  {
    return (double)mulberry32(a) / 4294967296.0;
  }
  // Gumbel(0,1) via inverse CDF — the Gumbel-max trick: argmax(logits + gumbel)
  // IS a sample from softmax(logits), which is how one draw per (param, corner)
  // makes the whole field a committed, repeatable patchwork instead of a dice
  // roll per visit.
  static double gumbel(double u)
  {
    u = u < 1e-9 ? 1e-9 : (u > 1.0 - 1e-9 ? 1.0 - 1e-9 : u);
    return -std::log(-std::log(u));
  }

  /* Param draws first, then the shared "mod" draw — the reference's order
     (reshuffleAll: gPar before gMod), so a seed means the same field there
     and here.
     `nPrefix` (B240): draw only the first nPrefix rows before gShared and the
     rest AFTER it. The shell passes its layout-9 length, so appending slots to
     its field can never move gShared or any earlier row — without it, every
     append re-dealt which corner every quantum slot draws. The default (the
     whole field) is the reference's order exactly, and for a field no longer
     than nPrefix the stream is the same either way, bit for bit. */
  void reshuffle(uint32_t seed, int nParams, int nPrefix = kMaxParams)
  {
    n = nParams > kMaxParams ? kMaxParams : nParams;
    const int pre = nPrefix < n ? (nPrefix < 0 ? 0 : nPrefix) : n;
    uint32_t a = seed;
    for (int i = 0; i < pre; i++)
      for (int k = 0; k < kCorners; k++) g[i][k] = gumbel(rnd01(a));
    for (int k = 0; k < kCorners; k++) gShared[k] = gumbel(rnd01(a));
    for (int i = pre; i < n; i++)
      for (int k = 0; k < kCorners; k++) g[i][k] = gumbel(rnd01(a));
  }

  // Bilinear pad weights — the reference's exact expression.
  static void weights(double x, double y, double w[kCorners])
  {
    w[0] = (1 - x) * (1 - y);
    w[1] = x * (1 - y);
    w[2] = (1 - x) * y;
    w[3] = x * y;
  }

  // The single scoring law (the reference's pickCorner): what you hear and
  // what a map paints must be one function or the map lies about the sound.
  int pickCorner(int i, const double lw[kCorners], double coup) const
  {
    double best = -1e300;
    int bi = 0;
    for (int k = 0; k < kCorners; k++)
    {
      const double s = lw[k] + (1 - coup) * g[i][k] + coup * gShared[k];
      if (s > best) { best = s; bi = k; }
    }
    return bi;
  }

  static void logW(const double w[kCorners], double temp, double lw[kCorners])
  {
    const double T = temp < 0.02 ? 0.02 : temp;
    for (int k = 0; k < kCorners; k++)
      lw[k] = w[k] > 1e-9 ? std::log(w[k]) / T : -1e12;
  }

  int n = 0;
  double g[kMaxParams][kCorners] = {};
  double gShared[kCorners] = {};
};

}  // namespace hypersaw
