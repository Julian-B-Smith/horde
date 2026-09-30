/*
 * force_core.h — the shared force system of the Track E effect engines
 * (SPEC-EFFECTS §1): home/sync/splay/gravity/drift/inertia acting on log2
 * positions (Hz for filters/notches, seconds for taps/lines), integrated per
 * 16-sample control tick.
 *
 * Transcribed VERBATIM from the reference labs' population code — the three
 * HTML prototypes (swarmfilter / swarmphaser / swarmtime) embed one identical
 * force expression and differ only in frozen per-engine constants, captured
 * here as Profile, plus the attractor set, captured as Attractor. Correctness
 * is defined as seed-for-seed reproduction of the labs' population
 * trajectories (force_check vs Node-generated goldens), never as plausible
 * motion. Change nothing here without re-deriving from the labs.
 *
 * Parity-exactness rules (same discipline as swarm_core.h):
 *  - doubles everywhere; the labs' population path is Float64Array, so there
 *    is NO f32 rounding here (unlike the audio path).
 *  - mulberry32 under JS int semantics via uint32 ops.
 *  - the labs' truncated literals: kTau = 6.283185307, kPiRef = 3.14159265.
 *  - expression shapes kept verbatim (e.g. lf0 + log2(h), not log2(h*f0)).
 *
 * Relationship to swarm_core.h (ADR-034): the oscillator engine's dynamics
 * are PHASE-domain Kuramoto (sin coupling on phases; frequency offsets come
 * from static placement laws) — a mathematically distinct system from this
 * POSITION-domain spring system. They share only the RNG (identical, adopted
 * from here) and a family-resemblant drift walk whose frozen constants
 * differ (swarm_core clamps the walk to ±1 and scales by depth at
 * application; the labs bake depth into the walk, unclamped). Both are
 * reference-frozen; "unifying" the constants would be a spec change on one
 * side or the other.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace horde2::swarm::forcecore
{

inline constexpr double kTau = 6.283185307;   // labs' truncated 2π literal
inline constexpr double kPiRef = 3.14159265;  // labs' truncated π literal
inline constexpr int kMaxN = 24;              // FilterLab's NB_MAX (largest of the labs)

// mulberry32 under JS int semantics — bit-identical to the labs' rngNext()
// and to swarm_core.h's (which delegates here).
inline double rngNext(uint32_t &state)
{
  state += 0x6D2B79F5u;
  uint32_t t = state;
  t = (t ^ (t >> 15)) * (t | 1u);
  t ^= t + (t ^ (t >> 7)) * (t | 61u);
  return double(t ^ (t >> 14)) / 4294967296.0;
}

// One-pole smoothing coefficient from a time constant in SECONDS (ADR-009: slew
// math is expressed in seconds and converted to a per-sample coefficient, never
// a hand-tuned constant). Bit-identical to the inlined `1 - exp(-1/(tau*sr))` it
// replaces across the cores' envelope atk/rel/dec — the invariant, in one place.
inline double onePoleCoef(double tau, double sr)
{
  return 1.0 - std::exp(-1.0 / (tau * sr));
}

// Per-engine frozen constants (from each lab's controlTick / clamp lines).
struct Profile
{
  double spanFloor;    // splay slot span floor, log2 units
  double inertiaMul;   // w0 = kTau * (inertiaMul * (1 - inertia) + inertiaAdd)
  double inertiaAdd;
  double clampLo;      // position clamps, log2 units
  double clampHi;
};
inline constexpr Profile kFilterProfile{0.15, 6, 0.5, 5.32, 13.42};  // 40 Hz .. ~11 kHz
inline constexpr Profile kPhaserProfile{0.15, 6, 0.5, 5.9, 13.42};   // ~60 Hz .. ~11 kHz
inline Profile timeProfile(int mode)  // TimeLab limits(): echo vs room
{
  return mode == 0 ? Profile{0.1, 4, 0.4, std::log2(0.03), std::log2(1.8)}
                   : Profile{0.1, 4, 0.4, std::log2(0.004), std::log2(0.17)};
}

// Gravity attractor sets (SPEC-EFFECTS §3/§5/§6). kind selects the lab's
// nearest-target arithmetic; f0/beat are the musical anchors.
struct Attractor
{
  int kind = 0;     // 0 none · 1 harmonics n·f0 · 2 rhythmic RSET×beat · 3 period multiples m/f0
  double f0 = 110;  // kinds 1 and 3
  double beat = 0.5;  // kind 2: 60/bpm seconds
};

inline constexpr double kRSet[] = {0.25, 1.0 / 3, 0.5, 2.0 / 3, 0.75, 1, 1.5, 2, 3, 4};

// Nearest attractor in log2 space, or NaN when none applies at vi.
inline double attractorTarget(const Attractor &a, double vi)
{
  if (a.kind == 1)
  {
    const double f = std::pow(2, vi);
    const double h = std::round(f / a.f0);
    if (h < 1) return std::numeric_limits<double>::quiet_NaN();
    return std::log2(a.f0) + std::log2(h);  // labs hoist lf0; same value bitwise
  }
  if (a.kind == 2)
  {
    const double t = std::pow(2, vi);
    double best = 0, be = 1e9;
    for (const double r : kRSet)
    {
      const double e = std::fabs(std::log2(t / (r * a.beat)));
      if (e < be)
      {
        be = e;
        best = std::log2(r * a.beat);
      }
    }
    return best;
  }
  if (a.kind == 3)
  {
    const double t = std::pow(2, vi);
    const double P = 1 / a.f0;
    const double m = std::max(1.0, std::round(t / P));
    return std::log2(m * P);
  }
  return std::numeric_limits<double>::quiet_NaN();
}

// Population forces (K/grav are the shared knobs; rates per SPEC-EFFECTS §1).
struct Params
{
  double K = 0;          // sync 6K² (K>0) / splay 18K² (K<0)
  double inertia = 0;    // 2nd-order transit, ζ = 0.45
  double driftDepth = 0; // cents
  double driftRate = 0.4;
  double grav = 0;       // gravity ±6|g| inside basin
  double basin = 45;     // cents
};

struct Pop
{
  int n = 0;
  uint32_t rng = 1;
  double v[kMaxN] = {};      // log2 position
  double mom[kMaxN] = {};
  double dOff[kMaxN] = {};   // drift offsets, log2 units
  double x[kMaxN] = {};      // distribution offsets in [-1, 1], sorted
  double tHome[kMaxN] = {};  // placement targets, log2
  // measured per tick (labs' formulas; gravErr is TimeLab's, computed for
  // every engine here — measurement only, it never feeds the dynamics)
  double collapse = 0;
  double combReg = 0;
  double gravErr = -1;  // mean |cents| of currently in-basin members, -1 if none
};

// Distribution offsets (shared Layer 1): 0 even · 1 gaussian · 2 cauchy,
// seeded draws + sort ascending, verbatim from the labs' rebuild().
// (These are the LABS' distribution codes — the oscillator engine's menu
// numbers JP-8000 differently; do not conflate.)
inline void buildOffsets(Pop &s, int dist, int n, int seed)
{
  s.n = n;
  s.rng = uint32_t(seed) + 1u;
  for (int i = 0; i < n; i++)
  {
    const double u = n == 1 ? 0.5 : double(i) / (n - 1);
    double x;
    if (dist == 0)
      x = 2 * u - 1;
    else if (dist == 1)
    {
      const double u1 = std::max(rngNext(s.rng), 1e-9), u2 = rngNext(s.rng);
      x = std::sqrt(-2 * std::log(u1)) * std::cos(kTau * u2) / 2.5;
      if (x > 1) x = 1;
      if (x < -1) x = -1;
    }
    else
    {
      x = std::tan(kPiRef * (rngNext(s.rng) - 0.5)) / 4;
      if (x > 1) x = 1;
      if (x < -1) x = -1;
    }
    s.x[i] = x;
  }
  std::sort(s.x, s.x + n);
}

// ERB helpers (frequency-engine placement).
inline double erbN(double f) { return 21.4 * std::log10(1 + 0.00437 * f); }
inline double erbInv(double E) { return (std::pow(10, E / 21.4) - 1) / 0.00437; }

// Frequency-engine placement (FilterLab/PhaserLab placeTargets; they differ
// only in the Hz floor — 40 for the bank, 60 for the notch swarm).
// place: 0 ERB spread · 1 log spread · 2 harmonic n·f0.
inline void placeFrequencyTargets(Pop &s, int place, double center, double spread, double f0last,
                                  double hzFloor)
{
  const double cF = std::pow(2, center);
  for (int i = 0; i < s.n; i++)
  {
    double f;
    if (place == 0)
      f = erbInv(std::max(1.0, erbN(cF) + s.x[i] * spread * 12));
    else if (place == 1)
      f = cF * std::pow(2, s.x[i] * spread * 2.5);
    else
      f = (i + 1) * f0last * std::pow(2, s.x[i] * spread * 0.12);
    s.tHome[i] = std::log2(std::min(11000.0, std::max(hzFloor, f)));
  }
}

// Time-engine placement (TimeLab rebuild: log-time spread around a size knob).
inline void placeTimeTargets(Pop &s, int mode, double size, double spread)
{
  const double centerSec =
      mode == 0 ? 0.08 * std::pow(2, size * 4.2) : 0.008 * std::pow(2, size * 4.0);
  const double c = std::log2(centerSec);
  const double halfOct = mode == 0 ? spread * 2.0 : spread * 1.3;
  const Profile prof = timeProfile(mode);
  for (int i = 0; i < s.n; i++)
    s.tHome[i] = std::min(prof.clampHi, std::max(prof.clampLo, c + s.x[i] * halfOct));
}

// Seed positions at the placement targets (the labs' post-rebuild reset).
inline void resetToHome(Pop &s)
{
  for (int i = 0; i < s.n; i++)
  {
    s.v[i] = s.tHome[i];
    s.mom[i] = 0;
    s.dOff[i] = 0;
  }
}

// One control tick — the labs' controlTick population section, verbatim.
// dt = 16 / sampleRate. Stats note: the labs compute collapse against the
// PRE-update mean (mean of v before integration vs v after) — kept as-is.
inline void tick(Pop &s, const Params &p, const Profile &prof, const Attractor &att, double dt)
{
  const int n = s.n;
  if (p.driftDepth > 0)
  {
    const double rate = 0.2 + p.driftRate * 8, amp = p.driftDepth / 1200;
    for (int i = 0; i < n; i++)
    {
      s.dOff[i] += (rngNext(s.rng) - 0.5) * 2 * std::sqrt(rate * dt) * amp;
      s.dOff[i] -= s.dOff[i] * 0.4 * dt;
    }
  }
  double mean = 0;
  for (int i = 0; i < n; i++) mean += s.v[i];
  mean /= n;
  double hMin = 1e9, hMax = -1e9;
  for (int i = 0; i < n; i++)
  {
    if (s.tHome[i] < hMin) hMin = s.tHome[i];
    if (s.tHome[i] > hMax) hMax = s.tHome[i];
  }
  const double span = std::max(prof.spanFloor, hMax - hMin);
  const double km = p.K;
  const double syncR = km > 0 ? 6 * km * km : 0;
  const double splayR = km < 0 ? 18 * km * km : 0;
  const double gravR = std::fabs(p.grav) * 6;
  double gAcc = 0;
  int gCnt = 0;
  for (int i = 0; i < n; i++)
  {
    double F = 1.5 * (s.tHome[i] + s.dOff[i] - s.v[i]);  // home spring
    if (syncR > 0) F += syncR * (mean - s.v[i]);
    if (splayR > 0)
    {
      const double slot = mean - span / 2 + span * (n == 1 ? 0.5 : double(i) / (n - 1));
      F += splayR * (slot - s.v[i]);
    }
    if (gravR > 0)
    {
      const double tgt = attractorTarget(att, s.v[i]);
      if (!std::isnan(tgt))
      {
        const double errC = 1200 * (s.v[i] - tgt);
        if (std::fabs(errC) < p.basin)
        {
          F += (p.grav > 0 ? -1 : 1) * gravR * (s.v[i] - tgt);
          gAcc += std::fabs(errC);
          gCnt++;
        }
      }
    }
    if (p.inertia <= 0.001)
    {
      s.v[i] += F * dt;
      s.mom[i] = 0;
    }
    else
    {
      const double w0 = kTau * (prof.inertiaMul * (1 - p.inertia) + prof.inertiaAdd);
      s.mom[i] += F * w0 * dt;
      s.mom[i] *= std::exp(-0.9 * w0 * dt);
      s.v[i] += s.mom[i] * dt * w0;
    }
    if (s.v[i] < prof.clampLo) s.v[i] = prof.clampLo;
    if (s.v[i] > prof.clampHi) s.v[i] = prof.clampHi;
  }
  s.gravErr = gCnt ? gAcc / gCnt : -1;
  double vv = 0, hv = 0, hMean = 0;
  for (int i = 0; i < n; i++)
  {
    const double d = s.v[i] - mean;
    vv += d * d;
  }
  for (int i = 0; i < n; i++) hMean += s.tHome[i];
  hMean /= n;
  for (int i = 0; i < n; i++)
  {
    const double d = s.tHome[i] - hMean;
    hv += d * d;
  }
  s.collapse = 1 - std::min(1.0, std::sqrt(vv / n) / std::max(0.03, std::sqrt(hv / n)));
  double sorted[kMaxN];
  for (int i = 0; i < n; i++) sorted[i] = s.v[i];
  std::sort(sorted, sorted + n);
  double gm = 0;
  const int ng = n - 1;
  for (int i = 1; i < n; i++) gm += sorted[i] - sorted[i - 1];
  gm /= std::max(1, ng);
  double gv = 0;
  for (int i = 1; i < n; i++)
  {
    const double d = (sorted[i] - sorted[i - 1]) - gm;
    gv += d * d;
  }
  s.combReg = gm > 1e-6 ? std::max(0.0, 1 - std::sqrt(gv / ng) / gm) : 0;
}

}  // namespace forcecore
