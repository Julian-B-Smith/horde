/*
 * time_core.h — Track E2 time engines (SPEC-EFFECTS §5/§6), transcribed
 * VERBATIM from reference/swarmtime.html's TimeLab. Two modes on the shared force core
 * (forcecore, ADR-034), the population here being DELAY TIMES (log2 seconds):
 *   - ECHO (mode 0): tap-swarm delay — N read taps on one buffer, feedback of
 *     the tap sum, DC-blocked + damped. Gravity attractors = rhythmic ratios
 *     × beat (SPEC-EFFECTS §5). Feedback norm is /N (worst-case-correlation
 *     stability law, ADR-030/031), NOT √N.
 *   - ROOM (mode 1): FDN room swarm — N delay lines, NEGATED Householder
 *     mixing so the input-excited all-ones eigenchannel carries +1 (comb at
 *     k/L, ADR-030a: matrix sign is load-bearing), per-line damping + a DC
 *     blocker with corner far below the lowest comb (ADR-031 law 2).
 *
 * Correctness = L0-1-style audio parity vs the JS TimeLab (time goldens, both
 * modes) + L0-19/20/21 (gather/grid/rhythmic-gravity, FDN comb + matrix-sign
 * guard, long-run LF stability + DC boundedness). The population halves are
 * already covered by force_check (L0-19/20). noise=0 for parity (Math.random
 * dither). NaN watchdog self-heals the feedback state. processExternal() is
 * the SWARM-FX effect path (dry = plugin input instead of the exciter).
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "force_core.h"

namespace hypersaw
{

class TimeCore
{
 public:
  static constexpr int kNBMax = 12, kPoly = 4, kTick = 16;
  static constexpr int kExDetN = 5;
  static constexpr int kEBuf = 1 << 17;  // ~3 s at 44.1k (echo shared buffer)
  static constexpr int kRBuf = 1 << 13;  // ~186 ms (room per-line)

  struct TParams
  {
    double mode = 0, nb = 8, dist = 1, size = 0.55, spread = 0.6, seed = 1234;
    double K = 0, inertia = 0, driftDepth = 30, driftRate = 0.3, grav = 0, basin = 50, bpm = 120;
    double noise = 0.2, regen = 0.5, damp = 0.4, mix = 0.5, vol = 0.5;
    double stereo = 0.7;  // stereo spread for processExternalStereo (0 = mono)
  };

  explicit TimeCore(double sampleRate) : sr(sampleRate), ebuf(kEBuf, 0.0)
  {
    rbuf.assign(kNBMax, std::vector<double>(kRBuf, 0.0));
    rebuild(true);
  }

  TParams p;

  bool setParam(const char *k, double v)
  {
    double *slot = paramSlot(k);
    if (!slot) return false;
    *slot = v;
    if (eq(k, "mode") || eq(k, "nb") || eq(k, "dist") || eq(k, "size") || eq(k, "spread") ||
        eq(k, "seed"))
    {
      rebuild(false);
      if (eq(k, "mode") || eq(k, "nb") || eq(k, "seed") || eq(k, "dist"))
      {
        for (int i = 0; i < kNBMax; i++)
        {
          pop.v[i] = pop.tHome[i];
          pop.mom[i] = 0;
          pop.dOff[i] = 0;
          tSm[i] = std::pow(2, pop.v[i]) * sr;
          lp[i] = 0;
        }
        if (eq(k, "mode"))
        {
          std::fill(ebuf.begin(), ebuf.end(), 0.0);
          for (auto &b : rbuf) std::fill(b.begin(), b.end(), 0.0);
          fbLp = 0;
          dcE = 0;
          std::fill(dcR, dcR + kNBMax, 0.0);
        }
      }
    }
    return true;
  }
  double getParam(const char *k)
  {
    double *slot = paramSlot(k);
    return slot ? *slot : 0.0;
  }

  void rebuild(bool init)
  {
    const int n = (int)p.nb;
    forcecore::buildOffsets(pop, (int)p.dist, n, (int)p.seed);
    forcecore::placeTimeTargets(pop, (int)p.mode, p.size, p.spread);
    for (int i = 0; i < n; i++)
    {
      pop.v[i] = pop.tHome[i];
      pop.mom[i] = 0;
    }
    if (init)
      for (int i = 0; i < kNBMax; i++) tSm[i] = std::pow(2, pop.v[i]) * sr;
  }

  void setNoteFreq(double f) { f0last = f; }

  void controlTick()
  {
    forcecore::Params fp{p.K, p.inertia, p.driftDepth, p.driftRate, p.grav, p.basin};
    const int mode = (int)p.mode;
    forcecore::Attractor att =
        mode == 0 ? forcecore::Attractor{p.grav != 0 ? 2 : 0, f0last, 60.0 / p.bpm}
                  : forcecore::Attractor{p.grav != 0 ? 3 : 0, f0last, 60.0 / p.bpm};
    forcecore::tick(pop, fp, forcecore::timeProfile(mode), att, kTick / sr);
  }

  double processSample(double x)
  {
    const int n = (int)p.nb;
    for (int i = 0; i < n; i++)
    {
      const double target = std::pow(2, pop.v[i]) * sr;
      tSm[i] += (target - tSm[i]) * 0.0015;  // audio-rate slew (tape chirp)
    }
    const double dampC = 1 - std::exp(-6.283185307 * (8000 * std::pow(0.06, p.damp)) / sr);
    if ((int)p.mode == 0)
    {
      ebuf[ew] = std::tanh(x + fbLp * p.regen);
      double wetRaw = 0;
      for (int i = 0; i < n; i++)
      {
        const double d = std::min((double)kEBuf - 4, std::max(1.0, tSm[i]));
        double rp = ew - d;
        // GUARD THE OTHER END OF THE WRAP (mod-lab sweep, 2026-08-05). `rp += kBuf`
        // on a rp of -1e-13 yields kBuf - 1e-13, which is inside the ulp of kBuf
        // (2.9e-11 at 1<<17) and rounds to EXACTLY kBuf -- so i0 indexes one past
        // the end. In JS that produced a NaN and a 8192x interpolator
        // extrapolation; here it is an out-of-bounds read on the audio thread.
        // i1 was already wrapped; i0 never was.
        if (rp < 0) rp += kEBuf;
        if (rp >= kEBuf) rp -= kEBuf;
        const int i0 = (int)rp;
        const double fr = rp - i0;
        const int i1 = (i0 + 1) % kEBuf;
        wetRaw += ebuf[i0] * (1 - fr) + ebuf[i1] * fr;
      }
      const double wet = wetRaw / std::sqrt((double)n);
      const double fbSig = wetRaw / n;  // /N feedback norm — worst-case-correlation law
      dcE += 0.002 * (fbSig - dcE);
      fbLp += dampC * ((fbSig - dcE) - fbLp);
      ew = (ew + 1) % kEBuf;
      return wet;
    }
    else
    {
      double outs[kNBMax];
      double s = 0;
      for (int i = 0; i < n; i++)
      {
        const double d = std::min((double)kRBuf - 4, std::max(1.0, tSm[i]));
        double rp = rw - d;
        if (rp < 0) rp += kRBuf;
        if (rp >= kRBuf) rp -= kRBuf;   // see the kEBuf note above
        const int i0 = (int)rp;
        const double fr = rp - i0;
        const int i1 = (i0 + 1) % kRBuf;
        double o = rbuf[i][i0] * (1 - fr) + rbuf[i][i1] * fr;
        lp[i] += dampC * (o - lp[i]);
        o = lp[i];
        dcR[i] += 0.0006 * (o - dcR[i]);
        o -= dcR[i];
        outs[i] = o;
        s += o;
      }
      const double h = 2.0 / n, g = p.regen;   // negated Householder: h*s - outs[i]
      const double inG = x * (1.0 / std::sqrt((double)n));
      double wet = 0;
      for (int i = 0; i < n; i++)
      {
        rbuf[i][rw] = std::tanh(inG + g * (h * s - outs[i]));
        wet += outs[i];
      }
      rw = (rw + 1) % kRBuf;
      return wet * 0.8;
    }
  }

  // Instrument-style render (exciter-driven, mono) — the parity reference path.
  void render(float *outL, float *outR, int nSamples)
  {
    heal();
    const double atk = forcecore::onePoleCoef(0.002, sr);
    const double rel = forcecore::onePoleCoef(0.09, sr);
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
          if (ph < d) { const double t = ph / d; bl = t + t - t * t - 1; }
          else if (ph > 1 - d) { const double t = (ph - 1) / d; bl = t * t + t + t + 1; }
          vsum += naive - bl;
        }
        vsum *= 0.2;
        if (p.noise > 0) vsum = vsum * (1 - p.noise) + (rngUniform() * 2 - 1) * p.noise;
        nt.env += ((nt.gate ? 1 : 0) - nt.env) * (nt.gate ? atk : rel);
        dry += vsum * nt.env * 0.5;
      }
      const double wet = processSample(dry);
      const double y = std::tanh(((1 - p.mix) * dry + p.mix * wet) * p.vol * 1.6);
      outL[smp] = (float)y;
      outR[smp] = (float)y;
    }
  }

  // STEREO effect path (2026-07-19): mono-summed into the network, but each
  // tap/line is panned across the field by index × stereo width, so the swarm
  // of delays/room-modes spreads L↔R. Dry keeps the input's own stereo; the
  // wet is the decorrelated field. A deliberate divergence from the mono
  // reference (which the parity oracle still guards via processExternal/render)
  // — the effects are experimental (ADR-050). Buffer/feedback logic mirrors
  // processSample exactly; only the wet accumulation is split L/R.
  void processExternalStereo(const float *inL, const float *inR, float *outL, float *outR,
                             int nSamples)
  {
    heal();
    const int n = (int)p.nb;
    double gL[kNBMax], gR[kNBMax];
    for (int i = 0; i < n; i++)
    {
      const double pan = (n == 1 ? 0.0 : (2.0 * i / (n - 1) - 1)) * p.stereo;
      const double t = (pan + 1) * 0.7853981634;  // constant power
      gL[i] = std::cos(t);
      gR[i] = std::sin(t);
    }
    for (int smp = 0; smp < nSamples; smp++)
    {
      if (tick == 0) controlTick();
      tick = (tick + 1) & (kTick - 1);
      for (int i = 0; i < n; i++)
      {
        const double target = std::pow(2, pop.v[i]) * sr;
        tSm[i] += (target - tSm[i]) * 0.0015;
      }
      const double dampC = 1 - std::exp(-6.283185307 * (8000 * std::pow(0.06, p.damp)) / sr);
      const double dryM = 0.5 * ((double)inL[smp] + (double)inR[smp]);
      double wetL = 0, wetR = 0;
      if ((int)p.mode == 0)
      {
        ebuf[ew] = std::tanh(dryM + fbLp * p.regen);
        double wetRaw = 0;
        for (int i = 0; i < n; i++)
        {
          const double d = std::min((double)kEBuf - 4, std::max(1.0, tSm[i]));
          double rp = ew - d;
          if (rp < 0) rp += kEBuf;
          if (rp >= kEBuf) rp -= kEBuf;   // see the kEBuf note above
          const int i0 = (int)rp;
          const double fr = rp - i0;
          const int i1 = (i0 + 1) % kEBuf;
          const double tv = ebuf[i0] * (1 - fr) + ebuf[i1] * fr;
          wetRaw += tv;
          wetL += tv * gL[i];
          wetR += tv * gR[i];
        }
        const double fbSig = wetRaw / n;
        dcE += 0.002 * (fbSig - dcE);
        fbLp += dampC * ((fbSig - dcE) - fbLp);
        ew = (ew + 1) % kEBuf;
        const double norm = 1.0 / std::sqrt((double)n);
        wetL *= norm;
        wetR *= norm;
      }
      else
      {
        double outs[kNBMax];
        double s = 0;
        for (int i = 0; i < n; i++)
        {
          const double d = std::min((double)kRBuf - 4, std::max(1.0, tSm[i]));
          double rp = rw - d;
          if (rp < 0) rp += kRBuf;
          if (rp >= kRBuf) rp -= kRBuf;   // see the kEBuf note above
          const int i0 = (int)rp;
          const double fr = rp - i0;
          const int i1 = (i0 + 1) % kRBuf;
          double o = rbuf[i][i0] * (1 - fr) + rbuf[i][i1] * fr;
          lp[i] += dampC * (o - lp[i]);
          o = lp[i];
          dcR[i] += 0.0006 * (o - dcR[i]);
          o -= dcR[i];
          outs[i] = o;
          s += o;
        }
        const double h = 2.0 / n, g = p.regen;
        const double inG = dryM * (1.0 / std::sqrt((double)n));
        for (int i = 0; i < n; i++)
        {
          rbuf[i][rw] = std::tanh(inG + g * (h * s - outs[i]));
          wetL += outs[i] * gL[i];
          wetR += outs[i] * gR[i];
        }
        rw = (rw + 1) % kRBuf;
        wetL *= 0.8;
        wetR *= 0.8;
      }
      outL[smp] = (float)std::tanh(((1 - p.mix) * (double)inL[smp] + p.mix * wetL) * p.vol * 1.6);
      outR[smp] = (float)std::tanh(((1 - p.mix) * (double)inR[smp] + p.mix * wetR) * p.vol * 1.6);
    }
  }

  // EFFECT path: process external audio (mono-summed) through the delay/room.
  void processExternal(const float *inL, const float *inR, float *outL, float *outR, int nSamples)
  {
    heal();
    for (int smp = 0; smp < nSamples; smp++)
    {
      if (tick == 0) controlTick();
      tick = (tick + 1) & (kTick - 1);
      const double dry = 0.5 * ((double)inL[smp] + (double)inR[smp]);
      const double wet = processSample(dry);
      const double y = std::tanh(((1 - p.mix) * dry + p.mix * wet) * p.vol * 1.6);
      outL[smp] = (float)y;
      outR[smp] = (float)y;
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

  double bandTime(int i) const { return std::pow(2, pop.v[i]); }
  double collapse() const { return pop.collapse; }
  double combReg() const { return pop.combReg; }

 private:
  static constexpr double kExDet[kExDetN] = {-8, -4, 0, 4, 8};
  double sr;
  forcecore::Pop pop;
  std::vector<double> ebuf;
  std::vector<std::vector<double>> rbuf;
  double tSm[kNBMax] = {0}, lp[kNBMax] = {0}, dcR[kNBMax] = {0};
  double fbLp = 0, dcE = 0;
  int ew = 0, rw = 0, tick = 0;
  double f0last = 110;
  uint32_t noiseState = 0x9E3779B9u;
  double rngUniform() { return forcecore::rngNext(noiseState); }

  // NaN watchdog (ADR-032): a NaN in feedback state renders as permanent
  // silence — self-heal to home + clear buffers instead.
  void heal()
  {
    if (std::isfinite(pop.v[0] + fbLp + lp[0] + dcE)) return;
    for (int i = 0; i < kNBMax; i++)
    {
      pop.v[i] = pop.tHome[i];
      pop.mom[i] = 0;
      pop.dOff[i] = 0;
      tSm[i] = std::pow(2, pop.v[i]) * sr;
      lp[i] = 0;
      dcR[i] = 0;
    }
    std::fill(ebuf.begin(), ebuf.end(), 0.0);
    for (auto &b : rbuf) std::fill(b.begin(), b.end(), 0.0);
    fbLp = 0;
    dcE = 0;
  }

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

  static bool eq(const char *a, const char *b) { return std::strcmp(a, b) == 0; }
  double *paramSlot(const char *k)
  {
    if (eq(k, "mode")) return &p.mode;
    if (eq(k, "nb")) return &p.nb;
    if (eq(k, "dist")) return &p.dist;
    if (eq(k, "size")) return &p.size;
    if (eq(k, "spread")) return &p.spread;
    if (eq(k, "seed")) return &p.seed;
    if (eq(k, "K")) return &p.K;
    if (eq(k, "inertia")) return &p.inertia;
    if (eq(k, "driftDepth")) return &p.driftDepth;
    if (eq(k, "driftRate")) return &p.driftRate;
    if (eq(k, "grav")) return &p.grav;
    if (eq(k, "basin")) return &p.basin;
    if (eq(k, "bpm")) return &p.bpm;
    if (eq(k, "noise")) return &p.noise;
    if (eq(k, "regen")) return &p.regen;
    if (eq(k, "damp")) return &p.damp;
    if (eq(k, "mix")) return &p.mix;
    if (eq(k, "vol")) return &p.vol;
    if (eq(k, "stereo")) return &p.stereo;
    return nullptr;
  }
};

}  // namespace hypersaw
