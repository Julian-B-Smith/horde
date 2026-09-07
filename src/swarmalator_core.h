/*
 * swarmalator_core.h — SWARMALATOR engine: audio phase θ ↔ spatial angle ξ
 * coupled to each other (SPEC-SWARMALATOR §2), transcribed VERBATIM from
 * reference/swarmalator.html's Swarmalator core. The first engine where moving in space
 * changes the timbre and changing the timbre moves the sound in space — pan is
 * a STATE VARIABLE of the same dynamical system that makes the tone.
 *
 * Mean-field-reducible ring swarmalator: two compound order parameters
 * W± = ⟨e^{i(ξ±θ)}⟩ (two extra complex accumulators per control tick, O(N)).
 * K = ordinary Kuramoto sync (phase axis == SAW when J=0); J = the cross
 * coupling (space↔phase). Stereo out (pan from ξ), unlike the mono effects.
 *
 * Parity-exactness rules (as swarm_core.h): doubles for state; f32 output
 * store per sample (JS Float32Array); mulberry32 shared via forcecore
 * (ADR-034); reference literal kTauS = 6.283185307. K-normalization is the
 * ADR-004 σ-squared-taper. Correctness = L0-1-style stereo parity vs the JS
 * reference + the SPEC §5 acceptance anchors.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "force_core.h"

namespace hypersaw
{

class SwarmalatorCore
{
 public:
  static constexpr int kNuMax = 18, kTick = 16;
  static constexpr double kTauS = 6.283185307;

  struct SwParams
  {
    double nu = 12, kernel = 0, detune = 0.35, drift = 0.2, seed = 1234, K = 0, J = 0,
           width = 0.85, vol = 0.5;
  };

  explicit SwarmalatorCore(double sampleRate) : sr(sampleRate) { rebuild(true); }

  SwParams p;

  bool setParam(const char *k, double v)
  {
    double *slot = paramSlot(k);
    if (!slot) return false;
    *slot = v;
    if (eq(k, "nu") || eq(k, "detune") || eq(k, "drift") || eq(k, "seed"))
      rebuild(eq(k, "nu") || eq(k, "seed"));
    return true;
  }
  double getParam(const char *k)
  {
    double *slot = paramSlot(k);
    return slot ? *slot : 0.0;
  }

  void rebuild(bool replace)
  {
    const int n = (int)p.nu;
    rng = (uint32_t)((int32_t)(int64_t)p.seed + 1);
    double offs[kNuMax];
    for (int i = 0; i < n; i++)
    {
      const double u1 = std::max(rngNext(), 1e-9), u2 = rngNext();
      offs[i] = std::sqrt(-2 * std::log(u1)) * std::cos(kTauS * u2) / 2.5;
    }
    std::sort(offs, offs + n);
    const double cents = p.detune * 70;
    for (int i = 0; i < n; i++)
    {
      f[i] = f0 * std::pow(2, (offs[i] * cents) / 1200);
      nu[i] = (rngNext() * 2 - 1) * p.drift * 3.0;
      if (replace)
      {
        theta[i] = rngNext();
        xi[i] = kTauS * i / n;
      }
    }
    double m = 0;
    for (int i = 0; i < n; i++) m += f[i];
    m /= n;
    double v = 0;
    for (int i = 0; i < n; i++)
    {
      const double d = f[i] - m;
      v += d * d;
    }
    sigma = std::sqrt(v / n);
  }

  void noteOn(int midi, double freq)
  {
    curMidi = midi;
    f0 = freq;
    gate = 1;
    rebuild(false);
  }
  void noteOff(int midi)
  {
    if (midi == curMidi) gate = 0;
  }
  void allOff() { gate = 0; }

  void controlTick()
  {
    const int n = (int)p.nu;
    const double dt = kTick / sr;
    double pxr = 0, pxi = 0, mxr = 0, mxi = 0;
    for (int i = 0; i < n; i++)
    {
      const double a = kTauS * theta[i];
      const double sp = xi[i] + a, sm = xi[i] - a;
      pxr += std::cos(sp);
      pxi += std::sin(sp);
      mxr += std::cos(sm);
      mxi += std::sin(sm);
    }
    Rp = std::sqrt(pxr * pxr + pxi * pxi) / n;
    psip = std::atan2(pxi, pxr);
    Rm = std::sqrt(mxr * mxr + mxi * mxi) / n;
    psim = std::atan2(mxi, mxr);
    double cr = 0, ci = 0;
    for (int i = 0; i < n; i++)
    {
      const double a = kTauS * theta[i];
      cr += std::cos(a);
      ci += std::sin(a);
    }
    R = std::sqrt(cr * cr + ci * ci) / n;
    const double psi = std::atan2(ci, cr);
    const double kHz = 4 * p.K * std::fabs(p.K) * sigma;
    const double jRate = 7.0;
    for (int i = 0; i < n; i++)
    {
      const double a = kTauS * theta[i];
      const double sPlus = std::sin(psip - xi[i] - a);
      const double sMinus = std::sin(psim - xi[i] + a);
      const double xidot = nu[i] + p.J * jRate * 0.5 * (Rp * sPlus + Rm * sMinus);
      xi[i] += xidot * dt;
      if (xi[i] < 0)
        xi[i] += kTauS;
      else if (xi[i] >= kTauS)
        xi[i] -= kTauS;
      const double kSync = kHz * R * std::sin(psi - a) / std::max(sigma, 1e-6);
      const double jBack = p.J * jRate * 0.5 * (Rp * sPlus - Rm * sMinus) / kTauS;
      couple[i] = kSync + jBack;
      const double px = std::cos(xi[i]) * p.width;
      const double t = (px + 1) * 0.7853981634;  // (p+1)*π/4
      panL[i] = std::cos(t);
      panR[i] = std::sin(t);
    }
  }

  void render(float *outL, float *outR, int nSamples)
  {
    const int n = (int)p.nu;
    const double atk = forcecore::onePoleCoef(0.006, sr);
    const double rel = forcecore::onePoleCoef(0.25, sr);
    const double norm = 0.7 / std::sqrt((double)n);
    for (int smp = 0; smp < nSamples; smp++)
    {
      if (tick == 0) controlTick();
      tick = (tick + 1) & (kTick - 1);
      env += ((gate ? 1 : 0) - env) * (gate ? atk : rel);
      double l = 0, r = 0;
      for (int i = 0; i < n; i++)
      {
        const double dph = f[i] / sr + couple[i] / sr;
        double ph = theta[i] + dph;
        ph -= std::floor(ph);
        theta[i] = ph;
        double s;
        if ((int)p.kernel == 1)
        {
          s = std::sin(kTauS * ph);
        }
        else
        {
          const double naive = 2 * ph - 1;
          const double d = std::max(dph, 1e-6);
          double bl = 0;
          if (ph < d)
          {
            const double t = ph / d;
            bl = t + t - t * t - 1;
          }
          else if (ph > 1 - d)
          {
            const double t = (ph - 1) / d;
            bl = t * t + t + t + 1;
          }
          s = naive - bl;
        }
        l += s * panL[i];
        r += s * panR[i];
      }
      const double e = env * p.vol * norm;
      outL[smp] = (float)std::tanh(l * e * 1.6);
      outR[smp] = (float)std::tanh(r * e * 1.6);
    }
  }

  // Order-parameter access for the acceptance anchors / viz.
  double orderR() const { return R; }
  double orderRp() const { return Rp; }
  double orderRm() const { return Rm; }
  double spatialAngle(int i) const { return xi[i]; }
  double phase(int i) const { return theta[i]; }
  int units() const { return (int)p.nu; }

 private:
  double sr;
  double theta[kNuMax] = {0}, xi[kNuMax] = {0}, f[kNuMax] = {0}, nu[kNuMax] = {0};
  double couple[kNuMax] = {0}, panL[kNuMax] = {0}, panR[kNuMax] = {0};
  uint32_t rng = 1;
  double f0 = 220, env = 0, sigma = 0;
  double R = 0, Rp = 0, Rm = 0, psip = 0, psim = 0;
  int gate = 0, curMidi = -1, tick = 0;

  double rngNext() { return forcecore::rngNext(rng); }

  static bool eq(const char *a, const char *b) { return std::strcmp(a, b) == 0; }
  double *paramSlot(const char *k)
  {
    if (eq(k, "nu")) return &p.nu;
    if (eq(k, "kernel")) return &p.kernel;
    if (eq(k, "detune")) return &p.detune;
    if (eq(k, "drift")) return &p.drift;
    if (eq(k, "seed")) return &p.seed;
    if (eq(k, "K")) return &p.K;
    if (eq(k, "J")) return &p.J;
    if (eq(k, "width")) return &p.width;
    if (eq(k, "vol")) return &p.vol;
    return nullptr;
  }
};

}  // namespace hypersaw
