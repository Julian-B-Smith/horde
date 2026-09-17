/*
 * bendsweep_probe — B94's first move: reproduce "flat bend bar, notes at
 * random pitches" (human 2026-09-03: mass-spring inertia + bend quantise,
 * morphing between two corners) by sweeping GlideCore DIRECTLY.
 *
 * Two lanes, two questions:
 *  A. BEND lane, the bendCurveJson pulse (+2 st for 0.6 s): does any
 *     spring × damping × distance-curve × quantise × hysteresis × step-timing
 *     combination produce NaN/inf, a blow-up, or a FLAT emitted curve (the
 *     quantiser never leaves its first step while the law travels)? Run at
 *     the live tick rate AND the display grid rate — if the two disagree the
 *     picture lies about the sound.
 *  B. NOTE lane, a 7-semitone glide from base 60: does every EMITTED pitch
 *     (q + base) land on the admitted grid? Off-grid = "random pitches".
 * Diagnostic: prints findings, exits 0. Findings become oracles once ruled.
 */
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>
#include "../src/glide_core.h"

using hypersaw::GlideCore;

static bool admitted(long pitch, const GlideCore::Params &p, double base)
{
  const int qm = (int)p.quant;
  if (qm == GlideCore::kQuantOff) return true;
  if (qm == GlideCore::kQuantChromatic) return true;   // any integer
  const long root = (qm == GlideCore::kQuantScaleOffset) ? (long)std::lround(base) : (long)p.scaleRoot;
  const int idx = (int)(((pitch - root) % 12 + 12) % 12);
  if (p.scaleMask[idx]) return true;
  if (qm == GlideCore::kQuantScaleAnchor || qm == GlideCore::kQuantScaleOffset)
    return idx == (int)((((long)std::lround(base) - root) % 12 + 12) % 12);
  return false;
}

int main()
{
  const double sf[] = {0.5, 1, 2, 4, 8, 20};
  const double dp[] = {0.0, 0.2, 0.6, 1.0};
  const double dover[] = {0.0, 1.0, 2.0};
  const int quant[] = {0, 1, 2, 3, 4};
  const double hyst[] = {0, 8, 50};
  const double qtime[] = {0, 50, 500};
  const double rates[] = {2756.25, 2756.25};   // display grid == live tick (kBendGridSeconds = 16/44100); kept as a self-check
  int cases = 0, nan = 0, blow = 0, flat = 0, offgrid = 0, rateDisagree = 0;
  std::printf("bendsweep_probe — B94 reproduction sweep\n");
  for (double f : sf) for (double z : dp) for (double dov : dover)
  for (int qm : quant) for (double h : hyst) for (double qt : qtime)
  {
    GlideCore::Params p;
    p.model = GlideCore::kSpring; p.springF = f; p.damp = z; p.distOver = dov;
    p.quant = qm; p.qhyst = h; p.qTime = qt;
    cases++;
    // ---- A: the bend pulse at both rates
    double flatAt[2] = {0, 0}; bool bad = false;
    for (int r = 0; r < 2; r++)
    {
      const double cr = rates[r];
      const int N = (int)(1.4 * cr), t0 = (int)(0.05 * cr), t1 = (int)(0.65 * cr);
      GlideCore g(cr, true); g.reset(0);
      double maxq = 0, maxx = 0;
      for (int i = 0; i < N; i++)
      {
        const double tgt = (i >= t0 && i < t1) ? 2.0 : 0.0;
        const double q = g.step(tgt, p, 60.0);
        if (!std::isfinite(q)) { bad = true; break; }
        maxq = std::max(maxq, std::fabs(q));
        maxx = std::max(maxx, std::fabs(q));
      }
      if (bad) { nan++; std::printf("  NaN/inf  spring %.1f z %.1f dov %.0f q%d h%.0f qt%.0f @%.0f/s\n", f, z, dov, qm, h, qt, cr); break; }
      if (maxx > 10) { blow++; std::printf("  BLOWUP   spring %.1f z %.1f dov %.0f q%d h%.0f qt%.0f @%.0f/s peak %.3g\n", f, z, dov, qm, h, qt, cr, maxx); }
      flatAt[r] = maxq;
    }
    if (bad) continue;
    if (flatAt[0] < 0.25 || flatAt[1] < 0.25)
    {
      flat++;
      if (flat <= 12) std::printf("  FLAT     spring %.1f z %.1f dov %.0f q%d h%.0f qt%.0f  max|q| live %.2f / display %.2f\n", f, z, dov, qm, h, qt, flatAt[0], flatAt[1]);
    }
    if (std::fabs(flatAt[0] - flatAt[1]) > 0.5) rateDisagree++;
    // ---- B: note-lane grid honesty (7-semitone glide from 60, scale = C major)
    if (qm)
    {
      GlideCore g(rates[0], false); g.reset(0);
      const int N = (int)(2.0 * rates[0]);
      for (int i = 0; i < N; i++)
      {
        const double tgt = (i < N / 2) ? 7.0 : 0.0;
        const double q = g.step(tgt, p, 60.0);
        const long pitch = (long)std::lround(q + 60.0);
        if (std::fabs(q + 60.0 - pitch) > 1e-9 || !admitted(pitch, p, 60.0))
        {
          offgrid++;
          if (offgrid <= 8) std::printf("  OFFGRID  spring %.1f z %.1f dov %.0f q%d h%.0f qt%.0f  emitted %.4f\n", f, z, dov, qm, h, qt, q + 60.0);
          break;
        }
      }
    }
  }
  /* ---- C: SETTLED-PITCH CORRECTNESS + WRONG-STEP DWELL (note lane).
     A 7 st glide from base 60 must settle ON 67 (G, in C major). Under
     step-timing + spring overshoot the gate can COMMIT the overshoot step and
     then block the correction for a whole gate — a note audibly parked on the
     wrong pitch; the dwell length is what a listener hears as "random". */
  std::printf("\n-- C: settled pitch + longest wrong-step dwell (note lane, 60 -> 67, C major) --\n");
  int wrongSettle = 0; double worstDwell = 0; const double *wd = nullptr;
  static double worstCfg[6];
  for (double f : sf) for (double z : dp) for (int qm : {1, 2, 3, 4}) for (double h : hyst) for (double qt : qtime)
  {
    GlideCore::Params p; p.model = GlideCore::kSpring; p.springF = f; p.damp = z;
    p.quant = qm; p.qhyst = h; p.qTime = qt;
    GlideCore g(rates[0], false); g.reset(0);
    const int N = (int)(3.0 * rates[0]);
    double dwell = 0, maxDwell = 0, last = 0;
    for (int i = 0; i < N; i++)
    {
      const double q = g.step(7.0, p, 60.0);
      const double pitch = q + 60.0;
      if (i > (int)(0.4 * rates[0]))   // after the first 400 ms of honest travel
      {
        if (std::fabs(pitch - 67.0) > 1e-9) dwell += 1.0 / rates[0]; else dwell = 0;
        maxDwell = std::max(maxDwell, dwell);
      }
      last = pitch;
    }
    if (std::fabs(last - 67.0) > 1e-9) { wrongSettle++; if (wrongSettle <= 6) std::printf("  WRONG SETTLE spring %.1f z %.1f q%d h%.0f qt%.0f -> %.0f\n", f, z, qm, h, qt, last); }
    if (maxDwell > worstDwell) { worstDwell = maxDwell; worstCfg[0]=f; worstCfg[1]=z; worstCfg[2]=qm; worstCfg[3]=h; worstCfg[4]=qt; wd = worstCfg; }
  }
  // breakdown: which dampings / distance curves ever fail to settle
  {
    std::printf("  wrong-settle breakdown by damping:");
    for (double z : dp)
    {
      int n = 0;
      for (double f : sf) for (double dov : dover) for (int qm : {1, 2, 3, 4}) for (double h : hyst) for (double qt : qtime)
      {
        GlideCore::Params p; p.model = GlideCore::kSpring; p.springF = f; p.damp = z; p.distOver = dov;
        p.quant = qm; p.qhyst = h; p.qTime = qt;
        GlideCore g(rates[0], false); g.reset(0);
        double q = 0; for (int i = 0; i < (int)(3.0 * rates[0]); i++) q = g.step(7.0, p, 60.0);
        if (std::fabs(q + 60.0 - 67.0) > 1e-9) n++;
      }
      std::printf("  z=%.1f:%d", z, n);
    }
    std::printf("\n  ...and with damping 0 but distance curve != 1 (the zetaFromOs floor):");
    for (double dov : {0.0, 2.0})
    {
      int n = 0;
      for (double f : sf) for (int qm : {1, 2, 3, 4})
      {
        GlideCore::Params p; p.model = GlideCore::kSpring; p.springF = f; p.damp = 0.0; p.distOver = dov;
        p.quant = qm;
        GlideCore g(rates[0], false); g.reset(0);
        double q = 0; for (int i = 0; i < (int)(3.0 * rates[0]); i++) q = g.step(7.0, p, 60.0);
        if (std::fabs(q + 60.0 - 67.0) > 1e-9) n++;
      }
      std::printf("  dov=%.0f:%d/%d", dov, n, (int)(sizeof(sf)/sizeof(sf[0])) * 4);
    }
    std::printf("\n");
  }
  if (wd) std::printf("  worst wrong-step dwell after 400 ms: %.0f ms (spring %.1f z %.1f q%.0f h%.0f qt%.0f)\n", worstDwell * 1000, wd[0], wd[1], wd[2], wd[3], wd[4]);
  std::printf("  wrong settles: %d\n", wrongSettle);

  /* ---- D: REST-POSITION CORRECTION (bend lane). ADR-111's phenomenon: with
     the bend at REST (x = 0) does the lane emit a non-zero offset for a base
     that is out of the current scale? Per mode, for every pitch class. A
     non-zero rest offset transposes every held voice of that class. */
  std::printf("\n-- D: bend-lane REST offset by quantise mode, C major, bases 60..71 --\n");
  for (int qm : {1, 2, 3, 4})
  {
    GlideCore::Params p; p.model = GlideCore::kSpring; p.quant = qm;
    std::printf("  q%d:", qm);
    for (int b = 60; b < 72; b++)
    {
      GlideCore g(rates[0], true); g.reset(0);
      double q = 0; for (int i = 0; i < 200; i++) q = g.step(0.0, p, (double)b);
      std::printf(" %+.0f", q);
    }
    std::printf("\n");
  }
  std::printf("\n%d cases: NaN %d · blow-up %d · flat %d · display/live disagree(>0.5 st) %d · off-grid %d\n",
              cases, nan, blow, flat, rateDisagree, offgrid);
  return 0;
}
