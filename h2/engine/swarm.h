/*
 * swarm.h — the swarm half of horde 2's composed engine (h2/engine/, ROADMAP
 * B385; design: docs/port/h2-engine.md). One Swarm per voice drives that voice's
 * member frequencies; the blades are evaluated on them (engine.h).
 *
 * The law is SwarmSynth's (reference/swarmsaw.html :206-547) as the composed
 * engine runs it (docs/design/scalpel-horde-engine.js), with B382's three
 * corrections built in, as the C++ swarm core has them:
 *   M1  the coupling smoother's per-tick coefficient comes from a time constant in
 *       seconds (ADR-009, B150). 44.1 kHz keeps the literal 0.08, because the
 *       seconds round trip lands three ULP short of it.
 *   M2  the onset lock is bipolar (ADR-056): Kenv = 8·onset·|onset|, positive into
 *       the sync target, negative ×3 into the splay target.
 *   M3  detune law 3 is the tempo grid (ADR-022), snapped half away from zero.
 * The expressions keep SwarmSynth's operation order. A sync blade turns a 1e-10
 * relative pitch error into ~6.5e-6 at the output (docs/port/scalpel-phase-1a.md,
 * "Detection floor"), so "the same law, reassociated" would not hold parity.
 *
 * What SwarmSynth computes and the composed engine never reads is not here: its
 * renderSeg, pan image, tone tilt, hi-tame, roundness, R→tone filter and the
 * n-th order parameter. The output stage is the blade engine's.
 */
#pragma once

#include <cmath>
#include <cstdint>

#include "js.h"

namespace horde2::engine {

constexpr int kMembers = 9;   // the blade engine's member count, and so every swarm's
// SwarmSynth sizes its arrays at 32 and draws a start phase for every slot when
// retrig is off. Only kMembers are ever read, but all 32 draws are taken, or the
// swarm's stream would not be SwarmSynth's.
constexpr int kStartDraws = 32;
constexpr double kSwarmTick = 16;   // samples per control tick, on the engine's global count
// M1: the time constant that gives SwarmSynth's 0.08 per tick at 44.1 kHz.
constexpr double kKsmTauSeconds = 0.004351220802760264;

struct Swarm {
  double phase[kMembers] = {}, driftS[kMembers] = {}, vf[kMembers] = {}, eff[kMembers] = {},
         mom[kMembers] = {}, driftPh[kMembers] = {}, driftHoldT[kMembers] = {}, vfSm[kMembers] = {},
         fRun[kMembers] = {}, cdist[kMembers] = {};
  double f0 = 220;     // the swarm's pitch: the played pitch, or gravity's f0cur
  double fBase = 0;    // the voice's pitch at the last tick (couple() reads it)
  double Kenv = 0, KsmS = 0, KsmP = 0, R = 0, psi = 0, sigma = 0, age = -1;
  uint32_t rngState = 1;
  bool fresh = true, vfInit = false;
};

// SwarmSynth.p, the keys the composed engine writes (scalpel-horde-engine.js
// syncSwarm). n, dist, seed and law start at SwarmSynth's own values; the first
// sync moves them and re-places the members.
struct SwarmParams {
  double n = 7, dist = 1, seed = 1234, law = 0;
  double detune = 0.28, K = 0, onset = 0, dissolve = 0.63, driftDepth = 0, driftRate = 0.4,
         driftMode = 0, motionCenter = 0, inertia = 0, freqGlide = 0, keepPhase = 0,
         pivotMode = 0, harmReach = 1, stretchB = 0, spread = 1, anchor = 0, retrig = 1,
         bpm = 120, beatMult = 1;
};

// The one SwarmSynth instance the composed engine owns: the member placement
// every swarm shares, the note counter, the keep-phase snapshot, and the law.
class SwarmField {
 public:
  SwarmParams p;
  double lastPhase[kMembers] = {};   // keep-phase: the newest sounding swarm, after each render

  explicit SwarmField(double sampleRate)
      : sr(sampleRate),
        ksmC(sampleRate == 44100 ? 0.08 : 1 - std::exp(-(kSwarmTick / sampleRate) / kKsmTauSeconds)) {
    rebuild();
  }

  // Member placement x[] from (n, dist, seed). The seeded distributions redraw
  // from a stream reset on every rebuild, so x[] is a pure function of the three.
  void rebuild() {
    const int n = static_cast<int>(p.n);
    grng.a = static_cast<uint32_t>(js::toInt32(js::toInt32(p.seed) + 1.0));
    static const double JP[7] = {-1, -0.5715, -0.1774, 0, 0.181, 0.565, 0.9766};
    for (int i = 0; i < n; i++) {
      const double u = (n == 1) ? 0.5 : static_cast<double>(i) / (n - 1);
      double xv;
      if (p.dist == 0) xv = 2 * u - 1;
      else if (p.dist == 1) {
        const double pos = u * 6;
        const int a = static_cast<int>(std::floor(pos));
        const int b = a + 1 < 6 ? a + 1 : 6;
        xv = JP[a] + (JP[b] - JP[a]) * (pos - a);
      } else if (p.dist == 2) {
        const double u1 = js::max(grng.next(), 1e-9), u2 = grng.next();
        xv = std::sqrt(-2 * std::log(u1)) * std::cos(6.283185307 * u2) / 2.5;
        if (xv > 1) xv = 1;
        if (xv < -1) xv = -1;
      } else if (p.dist == 3) {
        xv = std::tan(3.14159265 * (grng.next() - 0.5)) / 4;
        if (xv > 1) xv = 1;
        if (xv < -1) xv = -1;
      } else {
        xv = 2 * std::fmod((i + 1) * 0.6180339887498949, 1) - 1;   // golden placement, no draws
      }
      x[i] = xv;
    }
    if (n == 1) x[0] = 0;
    centerIdx = 0;
    for (int i = 1; i < n; i++) if (std::fabs(x[i]) < std::fabs(x[centerIdx])) centerIdx = i;
    xmin = x[0];
    for (int i = 1; i < n; i++) if (x[i] < xmin) xmin = x[i];
  }

  // SwarmSynth.noteOn on the voice's own swarm. `f` is the bent pitch.
  void noteOn(Swarm& s, double f) {
    s.f0 = f;
    s.age = noteCounter++;
    s.Kenv = 8 * p.onset * std::fabs(p.onset);   // M2
    s.KsmS = 0; s.KsmP = 0; s.fresh = true;
    s.vfInit = false;                             // the frequency glide snaps to the new note
    for (int i = 0; i < kMembers; i++) s.mom[i] = 0;
    s.rngState = static_cast<uint32_t>(js::toInt32(js::toInt32(p.seed) + s.age * 7919 + 1));
    Mulberry32 r{s.rngState};
    for (int i = 0; i < kStartDraws; i++) {
      double ph;
      if (js::truthy(p.keepPhase)) ph = i < kMembers ? lastPhase[i] : 0;
      else ph = js::truthy(p.retrig) ? 0 : r.next();
      if (i < kMembers) {
        s.driftS[i] = 0; s.driftPh[i] = i * 0.13; s.driftHoldT[i] = 0;
        s.phase[i] = ph;
      }
    }
    s.rngState = r.a;
  }

  // SwarmSynth.controlTick: drift, the detune law, the coupling targets and their
  // smoother, the mean field, the coupling pull and inertia. Once per 16 samples.
  void controlTick(Swarm& s) {
    const int n = static_cast<int>(p.n);
    const double dt = kSwarmTick / sr;
    const bool firstTick = !s.vfInit;
    const bool glideOn = p.freqGlide > 0;
    const double gCoefT = glideOn ? 1 - std::exp(-dt / p.freqGlide) : 0;
    s.Kenv *= std::exp(-dt / js::max(0.01, p.dissolve));
    Mulberry32 r{s.rngState};
    if (p.driftDepth > 0) {
      const int dm = js::toInt32(p.driftMode);
      const double rate = (0.2 + p.driftRate * 8);
      for (int i = 0; i < n; i++) {
        if (dm == 1) {   // sine: a smooth per-member LFO at decorrelated rates
          s.driftPh[i] += (0.05 + p.driftRate * 4) * (0.6 + i * 0.09) * dt;
          s.driftS[i] = std::sin(6.283185307 * s.driftPh[i]);
        } else if (dm == 2) {   // sample and hold: the hold shortens as the rate rises
          s.driftHoldT[i] -= dt;
          if (s.driftHoldT[i] <= 0) { s.driftS[i] = r.next() * 2 - 1; s.driftHoldT[i] = 0.03 + (1 - p.driftRate) * 0.6; }
        } else {   // walk: 1/f-like, leaking back to centre
          s.driftS[i] += (r.next() - 0.5) * 2 * std::sqrt(rate * dt);
          s.driftS[i] -= s.driftS[i] * 0.4 * dt;
          if (s.driftS[i] > 1) s.driftS[i] = 1;
          if (s.driftS[i] < -1) s.driftS[i] = -1;
        }
      }
    }
    s.rngState = r.a;
    // Each member's natural frequency under the detune law. spread scales every
    // law; anchor shifts every placement so that at 1 the lowest member is the root.
    const double dep = p.detune * p.spread;
    double mean = 0;
    for (int i = 0; i < n; i++) {
      double f;
      const double xv = x[i] - p.anchor * xmin;
      if (p.law == 0) f = s.f0 * std::pow(2, (xv * dep * 100) / 1200);
      else if (p.law == 1) f = s.f0 + xv * dep * 20;
      else if (p.law == 4) f = s.f0 * (1 + dep * p.harmReach * i);   // harmonic: the index is the rung
      else if (p.law == 5) {                                         // stretch: outer members spread further
        const double rat = std::pow(2, (xv * dep * 100) / 1200) - 1;
        f = s.f0 * (1 + rat * (1 + p.stretchB * xv * xv));
      } else if (p.law == 3) {   // M3, the tempo grid: every pairwise beat an exact multiple of u
        const double u = (p.bpm / 60) * p.beatMult, q = s.f0 * (std::pow(2, (xv * dep * 100) / 1200) - 1) / u;
        f = s.f0 + (q < 0 ? -js::round(-q) : js::round(q)) * u;
      } else f = s.f0 + xv * dep * 0.35 * erb(s.f0);
      // centre pin: drift scaled by the member's distance from the root (previous tick)
      if (p.driftDepth > 0) { const double mw = 1 - p.motionCenter * (1 - s.cdist[i]); f *= std::pow(2, (s.driftS[i] * p.driftDepth * mw) / 1200); }
      const double target = js::max(1, f);
      if (glideOn) {
        if (firstTick) s.vfSm[i] = target; else s.vfSm[i] += gCoefT * (target - s.vfSm[i]);
        s.vf[i] = s.vfSm[i];
      } else s.vf[i] = target;
      mean += s.vf[i];
    }
    s.vfInit = true;
    mean /= n;
    if (p.motionCenter > 0) {
      double maxdev = 1e-9;
      for (int i = 0; i < n; i++) { const double dd = std::fabs(s.vf[i] - s.f0); if (dd > maxdev) maxdev = dd; }
      for (int i = 0; i < n; i++) s.cdist[i] = std::fabs(s.vf[i] - s.f0) / maxdev;
    }
    double varsum = 0;
    for (int i = 0; i < n; i++) { const double dd = s.vf[i] - mean; varsum += dd * dd; }
    s.sigma = js::max(0.08, std::sqrt(varsum / n));
    // Coupling targets in units of the measured spread, with a squared taper on a
    // bipolar knob: K > 0 pulls toward sync, K < 0 toward the splay state.
    const double km = 4 * p.K * std::fabs(p.K);
    const double syncT = (js::max(0, km) + js::max(0, s.Kenv)) * s.sigma;
    const double splayT = (js::max(0, -km) * 3 + js::max(0, -s.Kenv) * 3) * s.sigma;
    s.KsmS += (syncT - s.KsmS) * ksmC;
    s.KsmP += (splayT - s.KsmP) * ksmC;
    double sx = 0, sy = 0;
    for (int i = 0; i < n; i++) { const double a = s.phase[i] * 6.283185307; sx += std::cos(a); sy += std::sin(a); }
    sx /= n; sy /= n;
    s.R = std::sqrt(sx * sx + sy * sy);
    s.psi = std::atan2(sy, sx);
    // pivot 1: every member entrains to the one nearest the root, which holds still,
    // so the swarm folds onto the played pitch instead of the drifting mean
    const bool pivotRoot = p.pivotMode == 1;
    int rootIdx = 0;
    if (pivotRoot) {
      double rootD = std::fabs(s.vf[0] - s.f0);
      for (int i = 1; i < n; i++) { const double dd = std::fabs(s.vf[i] - s.f0); if (dd < rootD) { rootD = dd; rootIdx = i; } }
    }
    const int c0 = pivotRoot ? rootIdx : (centerIdx < n ? centerIdx : 0);
    double couple[kMembers];
    for (int i = 0; i < n; i++) {
      double c = pivotRoot ? s.KsmS * std::sin(6.283185307 * (s.phase[rootIdx] - s.phase[i]))
                           : s.KsmS * s.R * std::sin(s.psi - s.phase[i] * 6.283185307);
      // splay: pull toward the pivot's phase plus this member's slot
      if (s.KsmP > 0.001) c += s.KsmP * std::sin(6.283185307 * (s.phase[c0] + static_cast<double>(i - c0) / n - s.phase[i]));
      couple[i] = c;
    }
    // inertia: a damped second-order swing toward each target (zeta 0.45)
    const double w = p.inertia;
    if (w <= 0.001) {
      for (int i = 0; i < n; i++) { s.eff[i] = s.vf[i] + couple[i]; s.mom[i] = 0; }
      s.fresh = false;
    } else {
      if (s.fresh) { for (int i = 0; i < n; i++) { s.eff[i] = s.vf[i] + couple[i]; s.mom[i] = 0; } s.fresh = false; }
      const double w0 = 6.283185307 * (8 * (1 - w) + 0.6);
      const double S = w0 * w0, D = 0.9 * w0;
      for (int i = 0; i < n; i++) {
        const double target = s.vf[i] + couple[i];
        s.mom[i] += (target - s.eff[i]) * S * dt;
        s.mom[i] *= std::exp(-D * dt);
        s.eff[i] += s.mom[i] * dt;
      }
    }
    if (glideOn && firstTick) for (int i = 0; i < n; i++) s.fRun[i] = s.eff[i];   // the per-sample glide snaps
  }
#ifdef H2_ENGINE_FAULTS
  void faultKsm(double c) { ksmC = c; }   // must-fail control K1 (M1 reverted), the parity check only
#endif

 private:
  static double erb(double f) { return 24.7 * (4.37 * f / 1000 + 1); }

  double sr;
  double ksmC;   // M1, resolved once: sr is fixed for the object's life
  double x[kMembers] = {}, xmin = 0;
  int centerIdx = 0;
  double noteCounter = 0;
  Mulberry32 grng;
};

}  // namespace horde2::engine
