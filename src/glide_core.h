/*
 * glide_core.h — the travel laws (A1 fold, ratified 2026-08-06), transcribed
 * VERBATIM from bend-lab.html's Inertia class. Pitch does not arrive where you
 * point it, it TRAVELS there, and "how long does a glide take" has more than
 * one honest answer:
 *
 *   1 constant time  — same duration near or far (velocity set by distance)
 *   2 constant rate  — fixed cents/second, so distance sets the duration  [DEFAULT]
 *   3 lag            — one-pole, asymptotic, never technically arrives
 *   4 spring         — true inertia: overshoots and rings
 *
 * Law 5 of the prototype (lag -> constant rate) is CUT by the fold ruling: it
 * measured as the closest pair to law 3 (every headline metric identical, peak
 * divergence 8.70 cents) and added a control without adding a behaviour. Its
 * `case 5` is deliberately absent rather than dead-coded.
 *
 * SCALE QUANTISE is a MODIFIER, not a fifth law: it snaps the EMITTED pitch
 * while the law's dynamics run untouched underneath, so it composes with all
 * four (spring + quantise = an overshooting autotune wobble).
 *
 * Parity-exactness rules (as swarm_core.h): doubles for state, control rate =
 * sr/16, no wall-clock. Correctness = trajectory parity vs the JS reference
 * (tools/glide_check.cpp) — never "plausible-feeling glide".
 *
 * NOT YET WIRED INTO THE AUDIO PATH. Core + oracle first, shell integration
 * behind its own increment — the same order the swarmalator port used.
 */
#pragma once

#include <algorithm>
#include <cmath>

namespace hypersaw
{

class GlideCore
{
 public:
  enum Law { kOff = 0, kConstTime = 1, kConstRate = 2, kLag = 3, kSpring = 4 };
  /* kQuantScaleAnchor (ADR-111): scale quantise, but the base's own pitch
     class is ALWAYS admissible. kQuantScale mirrors the reference exactly and
     stays strict forever (goldens cover it with out-of-scale bases —
     glide-quant-root3); the anchored variant is a parity-safe superset the
     mono reference cannot even express, because the phenomenon it exists for
     — a global lane's rest-position correction transposing every held voice —
     needs polyphony the lab does not have (L0031 again). */
  /* kQuantScaleOffset (ADR-112): movable-do. The mask is applied RELATIVE TO
     THE ANCHOR — whatever note you play is the tonic, and a bend walks the
     scale SHAPE from there. scaleRoot is deliberately ignored: a root and a
     movable tonic cannot both be in charge. The anchor's class (idx 0) is
     admitted even if the player switched degree 1 off, because without that
     every strike would carry a rest correction into the global lane and the
     mode would drag by accident — the exact surprise ADR-111 just named. */
  enum Quant { kQuantOff = 0, kQuantChromatic = 1, kQuantScale = 2, kQuantScaleAnchor = 3,
               kQuantScaleOffset = 4 };

  struct Params
  {
    double model = kConstRate;   // ratified default: keeps 93% of wheel vibrato
    double gtime = 120;          // ms — constant-time duration
    double rate = 24;            // semitone-units/s for the rate-limited laws
    double tau = 60;             // ms — lag time constant
    double springF = 4;          // Hz
    double damp = 0.6;           // zeta
    double distOver = 1;         // overshoot ~ distance^distOver
    double retMul = 1;           // return-toward-rest time scale (BEND LANE ONLY)
    double quant = kQuantOff;
    double qhyst = 8;            // cents of stickiness at a step boundary
    // ms between step COMMITS; 0 = free. The reference has carried this since
    // 2026-08-07 and the port did not — a fold gap, not a new feature. It is
    // what turns a quantised glide from a zipper into a glissando RUN.
    double qTime = 0;
    double scaleRoot = 0;
    // major, as the reference literal
    int scaleMask[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};
  };

  // lane matters: only the BEND lane has a home to spring back to, so retMul
  // is meaningless on note pitch and the reference disables it there.
  GlideCore(double controlRate, bool bendLane) : cr(controlRate), bend(bendLane) {}

  void reset(double v)
  {
    x = v; vel = 0; y = v; D = 0;
    q = v; qStep = 0; qArmed = false; qFlips = 0; qT = 1e9;   // gate armed open
  }

  /* `base` is the lane's anchor in MIDI semitones — the note the offset rides
     on. The reference has carried it since the quantiser existed (`quantise(p,
     base = 0)`: "ABSOLUTE pitch, not the offset") and the port DROPPED it, so
     the bend lane quantised its offset as if the offset were a pitch: any note
     not aligned with the offset-space grid resolved to a shifted pitch after a
     bend ("notes resolving to a slightly different pitch", human 2026-08-21).
     Every golden ran base = 0, where the two are indistinguishable — L0031's
     blind spot, hit for the third time. */
  double step(double target, const Params &p, double base = 0.0)
  {
    const double dt = 1.0 / cr;
    // Move distance, peak-held since the last arrival: constant-time needs it
    // (velocity = distance/time, latched at the start) and so does
    // distance-steered overshoot.
    const double err = std::fabs(target - x);
    if (err > D) D = err;
    // Rearm only once the move has ARRIVED AND STOPPED. Arrival alone is wrong
    // for the spring: it crosses the target at full speed on the way to
    // overshooting, so err dips through zero mid-flight and D would rearm to
    // the overshoot height — re-deriving the damping DURING the overshoot it
    // was meant to set. (The reference records this as a calibration find.)
    if (err < 0.004 && std::fabs(vel) < 0.02) D = 0;
    const double Dc = std::max(D, 0.01);
    const double rm = (bend && std::fabs(target) < std::fabs(x))
                          ? std::max(0.05, p.retMul)
                          : 1.0;
    switch ((int)p.model)
    {
      case kOff:
        x = target; vel = 0; y = target;
        break;
      case kConstTime:
      {
        const double s = Dc / std::max(1e-3, p.gtime * 1e-3 * rm) * dt;
        const double d = target - x;
        x += std::max(-s, std::min(s, d));
        y = x;
        break;
      }
      case kConstRate:
      {
        const double s = (p.rate / rm) * dt, d = target - x;
        x += std::max(-s, std::min(s, d));
        y = x;
        break;
      }
      case kLag:
      {
        const double tau = std::max(1e-4, p.tau * 1e-3 * rm);
        x += (target - x) * (1 - std::exp(-dt / tau));
        y = x;
        break;
      }
      case kSpring:
      {
        // Forward Euler is safe: dt*omega <= 2*pi*20/2756 = 0.046.
        const double w = 6.283185307 * p.springF / rm;
        double z = p.damp;
        if (z < 1 && p.distOver != 1)
          z = zetaFromOs(osFromZeta(z) * std::pow(Dc / 2, p.distOver - 1));
        vel += (w * w * (target - x) - 2 * z * w * vel) * dt;
        x += vel * dt;
        y = x;
        break;
      }
      default:
        x = target; vel = 0; y = target;
        break;
    }
    return quantise(p, base);
  }

  double value() const { return q; }
  int flips() const { return qFlips; }

 private:
  // Closed-form overshoot of a second-order step response, and its inverse —
  // solving for the damping that PRODUCES a wanted overshoot rather than
  // scaling the output by a fudge factor.
  static double osFromZeta(double z)
  {
    return z >= 1 ? 0 : std::exp(-3.14159265358979323846 * z / std::sqrt(1 - z * z));
  }
  static double zetaFromOs(double os)
  {
    const double l = std::log(std::min(0.95, std::max(1e-4, os)));
    return -l / std::sqrt(3.14159265358979323846 * 3.14159265358979323846 + l * l);
  }

  double quantise(const Params &p, double base = 0.0)
  {
    const int qm = (int)p.quant;
    if (!qm) { q = x; return x; }
    /* ANCHOR MOVED -> START OVER. qStep, the hysteresis latch and the time
       gate all live in ABSOLUTE pitch (qStep = base + step), so they describe
       a gesture on the OLD anchor. Carrying them across a base change made the
       wheel lane emit (oldStep − newBase): a second key struck inside the gate
       window inherited the first key's step and every held voice was
       transposed by the interval between the two notes until the window
       elapsed — "playing several notes makes them glide to other notes at
       random intervals" (human, 2026-09-11; chromatic, wheel at rest). A note-on
       is not a wheel gesture: the new anchor commits at once and the gate's
       clock restarts there, exactly as reset() arms a fresh lane. The reference
       carries the same latch and never meets a base change (single-note
       bench), so every golden runs base constant and this is inert there. */
    if (qArmed && base != qBase) { qArmed = false; qT = 1e9; }
    qBase = base;
    const double semis = base + x;   // ABSOLUTE pitch, not the offset — the reference's own comment
    /* ONE CANDIDATE SEARCH FOR BOTH MODES, mirroring the reference exactly.
       Chromatic used std::lround here while the reference used Math.round, and
       those disagree on exact ties for NEGATIVE values — lround(-1.5) = -2 (half
       away from zero), Math.round(-1.5) = -1 (half toward +inf). A whole semitone
       of divergence in the shipped plugin that 1e-6 parity could never see,
       because no golden lands on a tie. Chromatic is now just "every pitch class
       admitted" running the same loop, so there is no rounding function left to
       disagree about.

       TIE-BREAK: toward the PREVIOUS EMITTED step (Tonality, HYPERSAW-002 §2).
       Strict `<` plus an ascending scan meant the lower candidate arrived first
       and won, so ties resolved downward by loop order rather than by decision. */
    double bestD = 1e300;
    long best = (long)std::lround(semis);
    bool haveTie = false;
    long tied = 0;
    {
      const bool sc = (qm == kQuantScale || qm == kQuantScaleAnchor || qm == kQuantScaleOffset);
      const long root = (qm == kQuantScaleOffset) ? (long)std::lround(base)
                                                  : sc ? (long)p.scaleRoot : 0;
      /* The anchor class comes from `base`, which each lane defines: the wheel
         lane passes lastNoteKey, a per-note MPE lane its own key. At rest
         (x = 0) semis == base, the anchor is admitted at distance zero, and q
         is exactly 0 — an out-of-scale anchor produces NO correction, which is
         the whole point of the mode. */
      const int anchorCls = (int)((((long)std::lround(base) - root) % 12 + 12) % 12);
      for (long c = (long)std::floor(semis) - 12; c <= (long)std::ceil(semis) + 12; c++)
      {
        if (sc)
        {
          const int idx = (int)(((c - root) % 12 + 12) % 12);
          if (!p.scaleMask[idx] &&
              !((qm == kQuantScaleAnchor || qm == kQuantScaleOffset) && idx == anchorCls))
            continue;
        }
        const double d = std::fabs((double)c - semis);
        if (d < bestD) { bestD = d; best = c; haveTie = false; }
        else if (d == bestD && c != best) { haveTie = true; tied = c; }
      }
    }
    if (haveTie)
    {
      // Prefer whichever candidate the previous emitted step is nearer to. With
      // nothing emitted yet there is no continuity to honour, so keep the lower —
      // stated as a choice rather than left to loop order.
      if (qArmed)
      {
        if (std::labs(tied - qStep) < std::labs(best - qStep)) best = tied;
      }
      else if (tied < best) best = tied;
    }
    // stick to the previous step until the challenger wins by qhyst cents
    const double hyst = std::max(0.0, p.qhyst) / 100.0;
    if (qArmed && best != qStep &&
        std::fabs(semis - (double)qStep) - std::fabs(semis - (double)best) < hyst)
      best = qStep;
    /* TIME GATE — the reference's, ported verbatim in semantics. With qTime > 0
       a step may only COMMIT when the gate has elapsed; between gates the
       previous step holds however far the underlying law has travelled. The
       law's dynamics are untouched (x keeps moving), only the EMISSION is gated,
       so spring + gated quantise still lands its overshoot wobble on the grid.
       The timer resets on COMMIT, not on attempt — at most one step per qTime,
       which is the glissando-run character — and reset() arms it wide open so
       the first step of a gesture is never delayed (gating the onset just reads
       as latency). qTime == 0 is the continuous path the goldens were sliced
       from, so this is inert at defaults. */
    qT += 1.0 / cr;
    const bool gateOpen = !(p.qTime > 0) || qT >= p.qTime / 1000.0;
    if (qArmed && best != qStep && !gateOpen) best = qStep;
    if (!qArmed || best != qStep) { qStep = best; qArmed = true; qFlips++; qT = 0; }
    // qStep and the hysteresis latch live in ABSOLUTE pitch (the reference's
    // qStep does too); only the RETURN converts back to this lane's units.
    q = (double)best - base;
    return q;
  }

  double cr;
  bool bend;
  double x = 0, vel = 0, y = 0, D = 0, q = 0;
  long qStep = 0;
  double qT = 1e9;
  double qBase = 0;   // the anchor qStep was committed against
  bool qArmed = false;
  int qFlips = 0;
};

}  // namespace hypersaw
