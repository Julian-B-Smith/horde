/*
 * blade.h — the blade engine's parameter and state types, and its pure
 * functions, for horde 2's composed engine (h2/engine/, ROADMAP B385; design:
 * docs/port/h2-engine.md). Copied from h2/cores/scalpel/razor_core.h, the
 * parity-proven port of reference/scalpel/prototype/razor-core.js, and cleaned.
 * The stateful half of the blade (the waves that read the per-member sine->saw
 * band-limit, the noise draws, the blade evaluation itself) is in engine.h.
 */
#pragma once

#include <cmath>
#include <cstdint>

#include "js.h"
#include "swarm.h"

namespace horde2::engine {

// Every field the blade evaluation reads. The main parameter set `s` and blade 2's
// per-member view share this layout, so one evaluation serves both blades.
struct Blade {
  double mode = 4, hot = 2, base = 0, w = .2, depth = 1, hard = 0, lock = 0, mirror = 0,
         fmType = 0, I = 2, mshape = 0, mEff = 3.37, m = 3.37, colB = 0;
};

// The oracle's `t` (targets) and `s` (smoothed): the 64 target keys, plus the
// fields render() and spread() copy in from `d`. `s` and `t` start equal.
struct SP : Blade {
  double b2mix = 0, colK = 0, I2 = 2, m2 = 3.37, mHz2 = 660, morph2 = .5, mspread2 = 0, ispread2 = 0,
         rotRate2 = 0, rotSpread2 = 0, kRuleAmt2 = 1, bspread2 = 0, kspread2 = 0, wspread2 = 0,
         dspread2 = 0, benvA2 = 2, benvD2 = 250, benvK2 = 0, benvW2 = 0, benvVel2 = .5, kRuleAmt = 1,
         w2 = .2, k2 = 3, kHz2 = 800, c2 = .35, depth2 = 1, hard2 = 0, xm = 0, fb = 0, benvA = 2,
         benvD = 250, benvK = 0, benvW = 0, benvVel = .5, glide = 60, driftRate = .5, morph = .5,
         wspread = 0, dspread = 0, mspread = 0, ispread = 0, k = 6, kHz = 1320, mHz = 660, c = .875,
         rotRate = 0, rotSpread = 0, gain = .35, detune = 14, K = .35, bspread = 0, kspread = 0,
         width = .7, A = 4, D = 400, Sus = .85, R = 280, bend = 0;
  double b2order = 0, lock2 = -1, mirror2 = -1, b2fm = 0, fmType2 = 0, mshape2 = 0, mUnit2 = 0,
         b2sp = 0, kRule2 = 0, kRule = 0, mode2 = 4, hot2 = 2, b2on = 0, aa = 1, kq = 0, law = 0;
  SP() { mode = 0; w = .25; }
};

// The discrete, block-rate parameters: the oracle's `d`, then horde's (the
// composed engine's HORDE_D). kCustom is the one string.
struct DP {
  double mode = 0, hot = 2, base = 0, lock = 0, N = 5, phaseMode = 2, poly = 6, mshape = 0, kq = 0,
         fmType = 0, mUnit = 0, mirror = 0, dcMode = 2, law = 0, frame = 0, frame2 = -1,
         rot2Follow = 1, lock2 = -1, mirror2 = -1, b2fm = 0, fmType2 = 0, mshape2 = 0, mUnit2 = 0,
         cScale = 0, rotSync = 1, panOrder = 0, aa = 1, polyMode = 0, glideAlways = 0, b2on = 0,
         mode2 = 4, hot2 = 2, kRule = 0, b2sp = 0, kRule2 = 0, b2env = 0, b2order = 0;
  // horde. dist 0 (even) is how a SCALPEL preset's detune maps onto horde's law.
  double dist = 0, seed = 1234, hLaw = 0, bpm = 120, beatMult = 1, harmReach = 1, stretchB = 0,
         spread = 1, anchor = 0, onset = 0, dissolve = 0.63, driftDepth = 0, hDriftRate = 0.4,
         driftMode = 0, motionCenter = 0, inertia = 0, inertiaCurve = 2.5, freqGlide = 0,
         keepPhase = 0, pivotMode = 0, grav = 0, basin = 35, onsetScatter = 0, onsetAlpha = 0.25,
         attackScatter = 0, voiceEnv = 0, relScatter = 0;
  // ADR-189's anti-aliasing divergences from the blade oracle, each off by default
  double aaCarrier = 0, aaXin = 0, aaLoop = 0;
  static constexpr int kCustomCap = 256;
  char kCustom[kCustomCap] = "1, 5/4, 3/2";
};

// One blade's running state. `draws`: a member's noise blade draws from the host
// stream; the scratch states the BLEP probe and DC estimators use do not.
// `aaReal`, `acp`, `aOk`: D1 runs on member states only, from the carrier phase it
// last saw.
struct NS {
  double idx = 0, val = 0;
  bool inside = false;
  double g = 0, seed = 0, acc = 0, xin = 0, cacc = 0, cd = 0, ov = 0, pv = 0;
  bool draws = true;
  bool aaReal = false, aOk = false;
  double acp = 0;
};

struct BX {
  Blade* g = nullptr;
  double c = 0, k = 3, modX = 0, mr = 0, mn = 1;
  NS ns3, ns4;
};

struct Member {
  // the blade
  double phi = 0, modX = 0, inc = 0, prev = 0, dc = 0, dcS = 0;
  bool dcInit = true;
  double rv[14] = {}, rvT[14] = {};
  double dcWait = 0, lead = 0, kMul = 1, rot2 = 0, rotOff2 = 0, iMul2 = 1, mr2 = 0, mn2 = 1,
         cOff2 = 0, kAdd2 = 0, kMul2 = 1, wMul2 = 1, dAdd2 = 0, rot = 0, cOff = 0, kAdd = 0,
         rotOff = 0, wMul = 1, dAdd = 0, iMul = 1, mor = 0, kEff = 6, mr = 0, mn = 1;
  NS ns, ns2;
  double y1 = 0, y2 = 0;
  Blade g2;   // blade 2's per-member parameter view (bx.g points here)
  BX bx;
  // the swarm drive: index, oversampled-step counter, phase increment per step
  int i = 0, j = 0;
  double dph = 0;
  // B335: entry wait (samples), entry ramp, drawn time factors, per-partial envelope
  double onsD = 0, onsC = 0, onsE = 0, aMul = 1, rMul = 1, eE = 0, pg = 1, eCall = -1, eAi = 0, eRc = 0;
  int eS = 0;
  bool hold = false;
  // ADR-189 D3: the loop filter's state and its two taps
  double fu = 0, f1 = 0, f2 = 0;
};

struct Voice {
  bool active = false;
  double note = std::nan(""), freq = 110, freqT = 110, fc = 110, vel = 1, env = 0;
  int stage = 0;
  bool gate = false;
  double age = 0, rot = 0;
  Member m[kMembers];
  double rot2 = 0, be = 0;
  int bst = 0;
  double bv = 1, kE = 1, wE = 1, be2 = 0;
  int bst2 = 0;
  double bv2 = 1, kE2 = 1, wE2 = 1, gr = 1;
  double xb[kMembers] = {};
  // the swarm, and B325's look-ahead snapshot of it
  Swarm sw, pre0;
  uint64_t sn = 0;      // engine sample count: the swarm ticks where (sn & 15) == 0
  bool tick0 = false;   // a look-ahead tick is standing in for the first one
  bool cull = false;    // B323: fading out under the cap
  double cullG = 1;
  bool gOn = false;     // B335: gravity has moved this voice
  double gf0 = 0, gfb = 0;
  bool pv = false;      // B335: per-member state (onset scatter or per-partial envelopes)
  double fx0 = 0;       // D3: member 0's filtered tap before it steps
};

struct Biquad { double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0; };

namespace blade {

inline double hash(double i) { const double x = std::sin(i * 127.1 + 311.7) * 43758.5453; return (x - std::floor(x)) * 2 - 1; }

// periodic antiderivative of each shape (each is zero-mean, so an interval's
// integral is F(b) - F(a))
inline double F(double sh, double x) {
  switch (js::sel(sh)) {
    case 0: return (1 - std::cos(6.283185307179586 * x)) * 0.15915494309189535;
    case 1: { const double f = x - std::floor(x); return f < 0.25 ? 2 * f * f : f < 0.75 ? 2 * f - 2 * f * f - 0.25 : 2 * f * f - 4 * f + 2; }
    case 2: { double y = x + 0.5; y -= std::floor(y); return y * y - y; }
    case 4: { double y = x + 0.5; y -= std::floor(y); return y - y * y; }
    default: { const double f = x - std::floor(x); return f < 0.5 ? f : 1 - f; }
  }
}
inline double crushAvg(double sh, double st, double kk, double j) { return (F(sh, st + (j + 1) / kk) - F(sh, st + j / kk)) * kk; }
inline bool isFM(const Blade& p) { return p.mode == 1 || p.mode == 2; }

// 'Balanced' stereo slots per swarm size: evenly spaced pans, permuted so pan is
// uncorrelated with member order (2 and 3 members cannot be fully balanced).
inline double panSlot(int N, int i) {
  static const double PANS[9][9] = {
      {0.0}, {-1.0, 1.0}, {-1.0, 1.0, 0.0}, {-0.3333, 1.0, -1.0, 0.3333}, {-0.5, 1.0, 0.0, -1.0, 0.5},
      {-0.2, 0.6, -1.0, 1.0, -0.6, 0.2}, {-0.3333, 1.0, -1.0, 0.6667, -0.6667, 0.3333, 0.0},
      {-0.1429, 0.4286, -0.7143, 1.0, -1.0, 0.7143, -0.4286, 0.1429},
      {-0.25, 1.0, -0.75, -0.5, 0.0, 0.5, 0.75, -1.0, 0.25}};
  return PANS[N - 1][i];
}

// a blade's edge gate at blade-local position e (0 outside); crush is hard-edged
inline double gate(double e, double w, double hard, double mode) {
  if (w < 0.004 || e >= w) return 0;
  if (mode == 6) return 1;
  const double t = hard * w * 0.5;
  if (t > 1e-9) { if (e < t) return 0.5 - 0.5 * std::cos(3.141592653589793 * e / t); if (e > w - t) return 0.5 - 0.5 * std::cos(3.141592653589793 * (w - e) / t); }
  return 1;
}

// collision pitch: while the blades overlap, the upper blade's carrier runs faster
// (or slower), integrated so it chirps
inline void collide(double amt, NS& nsU, double eU0, double eU1, double kkU, double ov, double dphi) {
  nsU.ov = ov;
  if (eU1 < eU0) nsU.cacc = 0;   // blade entry
  const double dd = js::truthy(amt) ? (js::pow(4, amt * ov) - 1) * kkU * dphi : 0;
  nsU.cacc += dd; nsU.cd = dd;
}

// ADR-189 A1: where carrier ADAA (D1) beats the BLEPs it replaces. A non-sine
// carrier that has an antiderivative (shapes 1-4), AND (an FM blade, OR Band-limit
// off), AND not an S&H modulator on an FM blade (its steps are phase jumps that a
// mean over the step smears). Discrete parameters only, so automation cannot
// chatter across a threshold.
inline bool d1Takes(const Blade& p, bool aa) {
  const double md = p.mode;
  const bool fm = md == 1 || md == 2;
  if (!(fm || md == 0 || md == 5) || p.hot == 0 || p.hot > 4) return false;
  if (fm && p.mshape == 7) return false;
  return fm || !aa;
}

}  // namespace blade
}  // namespace horde2::engine
