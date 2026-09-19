/*
 * strata_check — STRATA's six properties, as the spec states them (B77).
 *
 * The human's own acceptance list (STRATA-integration-spec.md §2.2/§10) is
 * the contract; this file is that list executed. Sweep-based rather than
 * example-based, because every one of these is a claim about ALL (v, u), and
 * an example test would pass on a combinator that is right in the middle and
 * wrong at the rails — which is precisely where a macro layer lives.
 *
 *   P1 neutrality   u = 0 is an EXACT passthrough at every tier
 *   P2 totality     at depth 1, u = +/-1 pins the output to the rail
 *   P3 boundedness  cascade stays in [0,1] with no clamp invoked
 *   P4 agency       monotone in v and u; agency reaches zero ONLY at |u| = 1
 *   P5 continuity   continuous across the u = 0 hinge
 *   P6 composition  same-sign contributions commute, mixed-sign ones do not,
 *                   and both match the closed forms the spec gives
 * UNWIRED: reason not stated — see B159.
 */
#include <cstdio>
#include <cmath>
#include <algorithm>

#include "../src/strata_core.h"

namespace
{
int failures = 0;
void check(bool ok, const char *what, const char *detail = "")
{
  std::printf("  %-58s %s%s%s\n", what, ok ? "ok" : "FAIL", *detail ? "  " : "", detail);
  if (!ok) failures++;
}
using S = hypersaw::StrataCore;
}  // namespace

int main()
{
  std::printf("strata_check — the six properties (STRATA spec §2.2)\n");

  // P1 — EXACT, not approximate. This is the parity-safe-superset guarantee:
  // a patch with centred pads must render bit-identically to no STRATA at all.
  {
    bool exact = true;
    double worst = 0;
    for (int i = 0; i <= 1000; i++)
    {
      const double v = i / 1000.0;
      const double out = S::evalTarget(v, 0.0, 0.0, 1.0, 1.0, S::kCascade);
      if (out != v) { exact = false; worst = std::max(worst, std::fabs(out - v)); }
      // depth 0 must also be exact passthrough, whatever the pads are doing
      if (S::evalTarget(v, 1.0, -1.0, 0.0, 0.0, S::kCascade) != v) exact = false;
    }
    char b[96];
    std::snprintf(b, sizeof b, "(worst deviation %.1e over 1001 values)", worst);
    check(exact, "P1 u=0 and depth=0 are EXACT passthroughs", b);
  }

  // P2 — a tier at full depth can always reach the rail, from anywhere.
  {
    bool hi = true, lo = true;
    for (int i = 0; i <= 100; i++)
    {
      const double v = i / 100.0;
      if (std::fabs(S::evalTarget(v, 0.0, 1.0, 1.0, 1.0, S::kCascade) - 1.0) > 1e-12) hi = false;
      if (std::fabs(S::evalTarget(v, 0.0, -1.0, 1.0, 1.0, S::kCascade) - 0.0) > 1e-12) lo = false;
      // and from the GROUP tier, with the master centred
      if (std::fabs(S::evalTarget(v, 1.0, 0.0, 1.0, 1.0, S::kCascade) - 1.0) > 1e-12) hi = false;
    }
    check(hi && lo, "P2 |u|=1 at depth 1 pins to the rail from any value");
  }

  // P3 — bounded BY CONSTRUCTION in cascade: no clamp is ever needed, which is
  // what keeps the layer free of the clipping artefacts additive mode has.
  {
    bool inRange = true;
    double lo = 1e9, hi = -1e9;
    for (int a = 0; a <= 60; a++)
      for (int b = -40; b <= 40; b++)
        for (int c = -40; c <= 40; c++)
        {
          const double out = S::evalTarget(a / 60.0, b / 40.0, c / 40.0, 1.0, 1.0, S::kCascade);
          lo = std::min(lo, out); hi = std::max(hi, out);
          if (out < -1e-12 || out > 1.0 + 1e-12) inRange = false;
        }
    char bb[96];
    std::snprintf(bb, sizeof bb, "(range over 200k points: %.3f .. %.3f)", lo, hi);
    check(inRange, "P3 cascade stays in [0,1] with no clamp", bb);
  }

  /* P4 — THE PROPERTY THE WHOLE LAYER EXISTS FOR. Monotone in both arguments,
     and a lower tier's agency (dv'/dv) must fall to zero ONLY at |u| = 1. The
     contrast test is the point: additive mode's agency is exactly zero
     everywhere past its clamp, which is the failure the human described as
     losing "the full range of autonomy of each". */
  {
    bool monoV = true, monoU = true;
    for (int b = -40; b <= 40; b++)
    {
      const double u = b / 40.0;
      double prev = -1e9;
      for (int a = 0; a <= 200; a++)
      {
        const double out = S::lift(a / 200.0, u);
        if (out < prev - 1e-12) monoV = false;
        prev = out;
      }
    }
    for (int a = 0; a <= 40; a++)
    {
      const double v = a / 40.0;
      double prev = -1e9;
      for (int b = -200; b <= 200; b++)
      {
        const double out = S::lift(v, b / 200.0);
        if (out < prev - 1e-12) monoU = false;
        prev = out;
      }
    }
    check(monoV && monoU, "P4a monotone in both the value and the tier control");

    // agency = d v'/d v, measured; the closed form is 1-|u|
    bool agencyOk = true;
    double worst = 0;
    for (int b = -39; b <= 39; b++)
    {
      const double u = b / 40.0;
      const double h = 1e-6;
      const double d = (S::lift(0.5 + h, u) - S::lift(0.5 - h, u)) / (2 * h);
      worst = std::max(worst, std::fabs(d - (1.0 - std::fabs(u))));
      if (d <= 1e-9) agencyOk = false;            // never zero before the rail
    }
    char b2[112];
    std::snprintf(b2, sizeof b2, "(|measured - (1-|u|)| worst %.2e; zero only at the rail)", worst);
    check(agencyOk && worst < 1e-6, "P4b agency is 1-|u| and vanishes ONLY at |u|=1", b2);

    // the contrast: additive truncates agency dead past the clamp
    const double h = 1e-6;
    const double dAdd = (S::additive(0.9 + h, 0.5) - S::additive(0.9 - h, 0.5)) / (2 * h);
    check(dAdd == 0.0, "P4c (contrast) additive DOES truncate agency past its clamp");
  }

  /* P5 — continuity across the hinge at u = 0. The KINK is expected (the two
     slopes are 1-v and v, equal only at v = 0.5); a JUMP would be a click
     under a moving pad.

     The assertion has to be Lipschitz, not a fixed epsilon, and getting that
     wrong is easy: across +/-e the gap is EXACTLY e by construction
     (e(1-v) + e*v), so "gap < 1e-9 at e = 1e-9" is unsatisfiable arithmetic
     dressed up as a tolerance — it fails a function that is provably
     continuous. What continuity actually claims is that the gap SHRINKS WITH
     e, so that is what is measured: halve e, and the gap must halve. */
  {
    double worstRatio = 0, worstShrink = 0;
    for (int a = 0; a <= 100; a++)
    {
      const double v = a / 100.0;
      const double e1 = 1e-6, e2 = 5e-7;
      const double g1 = std::fabs(S::lift(v, e1) - S::lift(v, -e1));
      const double g2 = std::fabs(S::lift(v, e2) - S::lift(v, -e2));
      worstRatio = std::max(worstRatio, g1 / e1);          // bounded => no jump
      worstShrink = std::max(worstShrink, std::fabs(g2 / g1 - 0.5));  // halves with e
    }
    char b[128];
    std::snprintf(b, sizeof b, "(gap/e worst %.4f, halving error %.2e)", worstRatio, worstShrink);
    check(worstRatio <= 2.0 + 1e-9 && worstShrink < 1e-6,
          "P5 continuous across u=0 (gap is O(e), halves with e)", b);
  }

  /* P6 — composition semantics, against the spec's closed forms:
       positive lifts multiply HEADROOM:  1-v' = (1-a)(1-b)(1-v)
       negative lifts multiply the VALUE: v'   = (1+a)(1+b)v
     and mixed signs must NOT commute — that non-commutation IS the hierarchy,
     so a test that found them equal would mean the tier order had stopped
     meaning anything. */
  {
    bool headroom = true, value = true, commutes = true, mixedDiffers = false;
    for (int i = 1; i <= 20; i++)
      for (int j = 1; j <= 20; j++)
      {
        const double a = i / 20.0, b = j / 20.0, v = 0.37;
        const double fwd = S::lift(S::lift(v, a), b), rev = S::lift(S::lift(v, b), a);
        if (std::fabs(fwd - rev) > 1e-12) commutes = false;
        if (std::fabs((1 - fwd) - (1 - a) * (1 - b) * (1 - v)) > 1e-12) headroom = false;
        const double nf = S::lift(S::lift(v, -a), -b);
        if (std::fabs(nf - (1 - a) * (1 - b) * v) > 1e-12) value = false;
        const double m1 = S::lift(S::lift(v, a), -b), m2 = S::lift(S::lift(v, -b), a);
        if (std::fabs(m1 - m2) > 1e-9) mixedDiffers = true;
      }
    check(headroom, "P6a positive lifts multiply headroom");
    check(value, "P6b negative lifts multiply the value");
    check(commutes, "P6c same-sign contributions commute");
    check(mixedDiffers, "P6d mixed-sign contributions do NOT — order is the hierarchy");
  }

  if (failures) std::printf("strata_check: RED (%d failure%s)\n", failures, failures == 1 ? "" : "s");
  else std::printf("strata_check: GREEN (0 failures)\n");
  return failures ? 1 : 0;
}
