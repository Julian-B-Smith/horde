/*
 * filter_core.h — Track E1 resonator bank (SPEC-EFFECTS §3), transcribed
 * VERBATIM from reference/swarmfilter.html's FilterLab. The population dynamics (filter
 * frequencies herded by home/sync/splay/gravity) are the shared force system
 * — this core consumes forcecore (ADR-034) for them and adds only the
 * filter-specific audio path: a polyBLEP saw exciter, an N-band TPT SVF
 * bandpass bank, collapse→Q routing, wet/dry, tanh.
 *
 * Correctness = L0-1-style audio parity with the JS FilterLab (filter goldens,
 * eps=1e-6 RMS) plus the population halves already proven by force_check
 * (L0-14/15). Parity-exactness rules match the other cores: doubles for state,
 * f32 output store per sample (JS Float32Array), reference literals verbatim.
 *
 * NON-DETERMINISM NOTE: FilterLab's exciter mixes Math.random() dither when
 * noise>0. That path cannot be bit-checked — parity scenarios set noise=0.
 * The noise mix itself is ported (below) so the runtime feature is intact;
 * only the ORACLE avoids it.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#include "force_core.h"

namespace hypersaw
{

class FilterCore
{
 public:
  static constexpr int kNBMax = 24, kPoly = 4, kTick = 16;
  static constexpr int kExDetN = 5;

  struct FParams
  {
    double nb = 16, place = 0, dist = 0, center = 9.64, spread = 0.5, seed = 1234;
    double K = 0, inertia = 0, driftDepth = 0, driftRate = 0.4, grav = 0, basin = 45;
    double noise = 0.15, qbase = 0.5, q2col = 0.4, wet = 1, vol = 0.5;
  };

  explicit FilterCore(double sampleRate) : sr(sampleRate) { rebuild(); }

  FParams p;

  bool setParam(const char *k, double v)
  {
    double *slot = paramSlot(k);
    if (!slot) return false;
    *slot = v;
    // Structural params rebuild the placement + reset the bank state, matching
    // FilterLab.setParam (nb/seed/dist also reset filter state to home).
    if (eq(k, "nb") || eq(k, "place") || eq(k, "dist") || eq(k, "center") || eq(k, "spread") ||
        eq(k, "seed"))
    {
      rebuild();
      if (eq(k, "nb") || eq(k, "seed") || eq(k, "dist"))
        for (int i = 0; i < kNBMax; i++)
        {
          pop.v[i] = pop.tHome[i];
          pop.mom[i] = 0;
          ic1[i] = 0;
          ic2[i] = 0;
          pop.dOff[i] = 0;
        }
    }
    return true;
  }
  double getParam(const char *k)
  {
    double *slot = paramSlot(k);
    return slot ? *slot : 0.0;
  }

  void rebuild()
  {
    const int n = (int)p.nb;
    forcecore::buildOffsets(pop, (int)p.dist, n, (int)p.seed);
    placeTargets();
    for (int i = 0; i < n; i++)
    {
      pop.v[i] = pop.tHome[i];
      pop.mom[i] = 0;
    }
  }

  void noteOn(int midi, double f)
  {
    Note *s = nullptr;
    for (auto &nt : notes)
      if (!nt.gate && nt.env < 1e-3)
        if (!s || nt.age < s->age) s = &nt;
    if (!s)
      for (auto &nt : notes)
        if (!s || nt.age < s->age) s = &nt;
    s->midi = midi;
    s->f0 = f;
    s->gate = 1;
    s->age = noteCounter++;
    for (int i = 0; i < kExDetN; i++) s->phase[i] = 0;
    f0last = f;
    if ((int)p.place == 2) placeTargets();  // harmonic placement retargets per note
  }
  void noteOff(int midi)
  {
    for (auto &nt : notes)
      if (nt.gate && nt.midi == midi) nt.gate = 0;
  }
  void allOff()
  {
    for (auto &nt : notes) nt.gate = 0;
  }

  // Population + coefficient tick — FilterLab.controlTick: the force half runs
  // in forcecore, the coefficient half (collapse→Q, SVF coeffs) is here.
  void controlTick()
  {
    const int n = (int)p.nb;
    forcecore::Params fp{p.K, p.inertia, p.driftDepth, p.driftRate, p.grav, p.basin};
    forcecore::Attractor att{p.grav != 0 ? 1 : 0, f0last, 0.5};
    forcecore::tick(pop, fp, forcecore::kFilterProfile, att, kTick / sr);
    // collapse→Q (FilterLab): Qb 2..40, capped 60, widened by collapse.
    const double Qb = 2 * std::pow(20.0, p.qbase);
    qEff = std::min(60.0, Qb * (1 + p.q2col * 3 * std::max(0.0, pop.collapse)));
    const double k = 1 / qEff;
    for (int i = 0; i < n; i++)
    {
      const double fc = std::pow(2, pop.v[i]);
      const double g = std::tan(kPiRefF * std::min(0.24, fc / sr));
      const double a1 = 1 / (1 + g * (g + k));
      cA1[i] = a1;
      cA2[i] = g * a1;
      cA3[i] = g * cA2[i];
      cK[i] = k;
    }
  }

  void render(float *outL, float *outR, int nSamples)
  {
    const int n = (int)p.nb;
    const double atk = forcecore::onePoleCoef(0.003, sr);
    const double rel = forcecore::onePoleCoef(0.15, sr);
    for (int smp = 0; smp < nSamples; smp++)
    {
      if (tick == 0) controlTick();
      tick = (tick + 1) & (kTick - 1);
      double dry = 0;
      for (auto &nt : notes)
      {
        if (!nt.gate && nt.env < 1e-4) continue;
        double vsum = 0;
        for (int i = 0; i < kExDetN; i++)
        {
          const double f = nt.f0 * std::pow(2, kExDet[i] / 1200.0);
          const double dph = f / sr;
          double ph = nt.phase[i] + dph;
          ph -= std::floor(ph);
          nt.phase[i] = ph;
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
          vsum += naive - bl;
        }
        vsum *= 0.2;
        if (p.noise > 0) vsum = vsum * (1 - p.noise) + (rngUniform() * 2 - 1) * p.noise;
        nt.env += ((nt.gate ? 1 : 0) - nt.env) * (nt.gate ? atk : rel);
        dry += vsum * nt.env * 0.5;
      }
      double wetS = 0;
      for (int i = 0; i < n; i++)
      {
        const double v3 = dry - ic2[i];
        const double v1 = cA1[i] * ic1[i] + cA2[i] * v3;
        const double v2 = ic2[i] + cA2[i] * ic1[i] + cA3[i] * v3;
        ic1[i] = 2 * v1 - ic1[i];
        ic2[i] = 2 * v2 - ic2[i];
        const double band = v1 * cK[i];  // unity-gain bandpass
        wetS += band;
        eEnv[i] += 0.004 * (std::fabs(band) - eEnv[i]);
      }
      wetS *= 1.6 / std::sqrt((double)n);
      const double y = std::tanh((dry * (1 - p.wet) + wetS * p.wet) * p.vol * 2);
      outL[smp] = (float)y;
      outR[smp] = (float)y;
    }
  }

  // EFFECT path (Track E1.3): process EXTERNAL audio instead of the exciter —
  // the same swarm-herded bank, driven by the input signal. The exciter
  // render() above stays the parity-frozen reference path; this shares only
  // controlTick + the SVF bank. Mono core: L/R summed in, mono out to both.
  // setNoteFreq() lets a played MIDI note move the gravity center without
  // spawning an exciter voice.
  void setNoteFreq(double f)
  {
    f0last = f;
    if ((int)p.place == 2) placeTargets();
  }
  void processExternal(const float *inL, const float *inR, float *outL, float *outR, int nSamples)
  {
    const int n = (int)p.nb;
    for (int smp = 0; smp < nSamples; smp++)
    {
      if (tick == 0) controlTick();
      tick = (tick + 1) & (kTick - 1);
      const double dry = 0.5 * ((double)inL[smp] + (double)inR[smp]);
      double wetS = 0;
      for (int i = 0; i < n; i++)
      {
        const double v3 = dry - ic2[i];
        const double v1 = cA1[i] * ic1[i] + cA2[i] * v3;
        const double v2 = ic2[i] + cA2[i] * ic1[i] + cA3[i] * v3;
        ic1[i] = 2 * v1 - ic1[i];
        ic2[i] = 2 * v2 - ic2[i];
        const double band = v1 * cK[i];
        wetS += band;
        eEnv[i] += 0.004 * (std::fabs(band) - eEnv[i]);
      }
      wetS *= 1.6 / std::sqrt((double)n);
      const double y = std::tanh((dry * (1 - p.wet) + wetS * p.wet) * p.vol * 2);
      outL[smp] = (float)y;
      outR[smp] = (float)y;
    }
  }

  // Viz/oracle access
  double bandFreq(int i) const { return std::pow(2, pop.v[i]); }
  double bandEnergy(int i) const { return eEnv[i]; }
  double collapse() const { return pop.collapse; }
  double combReg() const { return pop.combReg; }
  double qEffective() const { return qEff; }
  int bandCount() const { return (int)p.nb; }

 private:
  static constexpr double kPiRefF = 3.14159265;  // FilterLab's truncated π
  static constexpr double kExDet[kExDetN] = {-8, -4, 0, 4, 8};

  double sr;
  forcecore::Pop pop;
  double ic1[kNBMax] = {0}, ic2[kNBMax] = {0};
  double cA1[kNBMax] = {0}, cA2[kNBMax] = {0}, cA3[kNBMax] = {0}, cK[kNBMax] = {0};
  double eEnv[kNBMax] = {0};
  double qEff = 0;
  double f0last = 110;
  int tick = 0;

  struct Note
  {
    int midi = -1;
    double f0 = 110, env = 0;
    int gate = 0;
    long age = -1;
    double phase[kExDetN] = {0};
  };
  Note notes[kPoly];
  long noteCounter = 0;

  // Deterministic uniform for the noise dither, so a non-zero-noise runtime
  // patch is still reproducible (the reference uses Math.random(), which the
  // ORACLE avoids by testing at noise=0 — see the header note).
  uint32_t noiseState = 0x9E3779B9u;
  double rngUniform() { return forcecore::rngNext(noiseState); }

  void placeTargets()
  {
    // FilterLab place: 0 ERB, 1 log, 2 harmonic — with a 40 Hz floor and an
    // 11 kHz ceiling. forcecore::placeFrequencyTargets carries the arithmetic.
    forcecore::placeFrequencyTargets(pop, (int)p.place, p.center, p.spread, f0last, 40.0);
  }

  static bool eq(const char *a, const char *b)
  {
    return std::strcmp(a, b) == 0;
  }
  double *paramSlot(const char *k)
  {
    if (eq(k, "nb")) return &p.nb;
    if (eq(k, "place")) return &p.place;
    if (eq(k, "dist")) return &p.dist;
    if (eq(k, "center")) return &p.center;
    if (eq(k, "spread")) return &p.spread;
    if (eq(k, "seed")) return &p.seed;
    if (eq(k, "K")) return &p.K;
    if (eq(k, "inertia")) return &p.inertia;
    if (eq(k, "driftDepth")) return &p.driftDepth;
    if (eq(k, "driftRate")) return &p.driftRate;
    if (eq(k, "grav")) return &p.grav;
    if (eq(k, "basin")) return &p.basin;
    if (eq(k, "noise")) return &p.noise;
    if (eq(k, "qbase")) return &p.qbase;
    if (eq(k, "q2col")) return &p.q2col;
    if (eq(k, "wet")) return &p.wet;
    if (eq(k, "vol")) return &p.vol;
    return nullptr;
  }
};

}  // namespace hypersaw
