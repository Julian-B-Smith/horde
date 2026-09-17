/*
 * svf_check — the SvfCore oracle (B81 increment 2; oracle-as-spec, ADR-142).
 *
 * No reference implementation exists for this core ON PURPOSE (B81: "NOT the
 * E1 swarm labs — those are bus creatures"). So this suite IS the spec:
 * invariant probes with measured tolerances, the L0031 oracle-kind lesson
 * applied from birth instead of retrofitted.
 *
 * Standalone binary, registered in CMake, NOT in ./verify — wiring it into
 * the gate set is the standing human ruling (same status as delay_check,
 * strata_check, voicetap_check).
 */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

#include "../src/svf_core.h"

static int fails = 0;
static void ok(bool pass, const char *name, const char *detail)
{
  std::printf("%s %s (%s)\n", pass ? "OK  " : "FAIL", name, detail);
  if (!pass) fails++;
}

// Steady-state amplitude of a sine pushed through the filter: feed 2 s, then
// measure peak over the last 0.5 s (transient fully decayed at every fc/res
// this suite uses; the margin was checked by doubling the warmup and watching
// the fourth digit hold still).
static double sineGain(hypersaw::SvfCore &f, int mode, double freq, double fs)
{
  const int warm = (int)(2.0 * fs), meas = (int)(0.5 * fs);
  double peak = 0.0, ph = 0.0, w = 2.0 * 3.14159265358979323846 * freq / fs;
  for (int i = 0; i < warm + meas; i++)
  {
    const double y = f.process(std::sin(ph), mode);
    ph += w;
    if (i >= warm) peak = std::max(peak, std::fabs(y));
  }
  return peak;
}

int main()
{
  const double fs = 44100.0;

  // T1 — bypass is bit-exact and touches no state. Filter a burst, then
  // bypass the SAME instance: outputs must equal inputs to the bit, and the
  // states must not advance (process a sentinel through LP after and compare
  // against a twin that never saw the bypass call).
  {
    hypersaw::SvfCore a, b;
    a.setSampleRate(fs); b.setSampleRate(fs);
    a.setParams(800, 0.7); b.setParams(800, 0.7);
    bool bitExact = true;
    for (int i = 0; i < 1000; i++)
    {
      const double x = std::sin(i * 0.1) * 0.9;
      a.process(x, hypersaw::SvfCore::kLowpass);
      b.process(x, hypersaw::SvfCore::kLowpass);
    }
    for (int i = 0; i < 100; i++)
    {
      const double x = std::sin(i * 0.37);
      if (a.process(x, hypersaw::SvfCore::kBypass) != x) bitExact = false;
    }
    const double ya = a.process(0.5, hypersaw::SvfCore::kLowpass);
    const double yb = b.process(0.5, hypersaw::SvfCore::kLowpass);
    ok(bitExact && ya == yb, "T1 bypass bit-exact, state untouched",
       bitExact ? (ya == yb ? "100 samples exact; states identical" : "states diverged")
                : "passthrough not bit-exact");
  }

  // T2 — DC law: LP passes DC at unity, HP kills it, BP kills it.
  {
    hypersaw::SvfCore f; f.setSampleRate(fs); f.setParams(500, 0.0);
    double lp = 0, hp = 0, bp = 0;
    f.reset(); for (int i = 0; i < 44100; i++) lp = f.process(1.0, hypersaw::SvfCore::kLowpass);
    f.reset(); for (int i = 0; i < 44100; i++) hp = f.process(1.0, hypersaw::SvfCore::kHighpass);
    f.reset(); for (int i = 0; i < 44100; i++) bp = f.process(1.0, hypersaw::SvfCore::kBandpass);
    char d[96]; std::snprintf(d, sizeof d, "LP %.6f HP %.2e BP %.2e", lp, std::fabs(hp), std::fabs(bp));
    ok(std::fabs(lp - 1.0) < 1e-6 && std::fabs(hp) < 1e-6 && std::fabs(bp) < 1e-6,
       "T2 DC law (LP=1, HP=0, BP=0)", d);
  }

  // T3 — the -3 dB point sits at fc (res 0), and it does so at BOTH sample
  // rates: the samplerate_check discipline applied at birth. Tolerance 0.25 dB
  // measured: TPT warps tan-exactly, error at fc is discretization only.
  for (double rate : {44100.0, 96000.0})
  {
    hypersaw::SvfCore f; f.setSampleRate(rate); f.setParams(1000, 0.0);
    f.reset();
    const double g = sineGain(f, hypersaw::SvfCore::kLowpass, 1000, rate);
    const double db = 20.0 * std::log10(g);
    char d[64]; std::snprintf(d, sizeof d, "fs=%.0f: %.3f dB at fc", rate, db);
    ok(std::fabs(db - (-3.01)) < 0.25, "T3 LP -3dB at fc", d);
  }

  // T4 — monotone rolloff: one octave above fc must lose 9-15 dB (2-pole
  // slope is 12 dB/oct asymptotic; at fc+1oct the knee still rounds it),
  // two octaves 21-27 dB. Res 0.
  {
    hypersaw::SvfCore f; f.setSampleRate(fs); f.setParams(1000, 0.0);
    f.reset(); const double g1 = sineGain(f, hypersaw::SvfCore::kLowpass, 2000, fs);
    f.reset(); const double g2 = sineGain(f, hypersaw::SvfCore::kLowpass, 4000, fs);
    const double d1 = -20.0 * std::log10(g1), d2 = -20.0 * std::log10(g2);
    char d[64]; std::snprintf(d, sizeof d, "+1oct %.1f dB, +2oct %.1f dB", d1, d2);
    ok(d1 > 9 && d1 < 15 && d2 > 21 && d2 < 27, "T4 12 dB/oct rolloff", d);
  }

  // T5 — resonance peaks AT fc and grows with res, bounded: res 0.9's peak
  // gain at fc must exceed res 0's by >= 12 dB yet stay finite (< 40 dB) —
  // the k-floor's self-oscillation guard, asserted where it matters.
  {
    hypersaw::SvfCore f; f.setSampleRate(fs);
    f.setParams(1000, 0.0); f.reset();
    const double flatG = sineGain(f, hypersaw::SvfCore::kLowpass, 1000, fs);
    f.setParams(1000, 0.9); f.reset();
    const double resG = sineGain(f, hypersaw::SvfCore::kLowpass, 1000, fs);
    const double lift = 20.0 * std::log10(resG / flatG);
    char d[64]; std::snprintf(d, sizeof d, "peak lift %.1f dB at fc", lift);
    ok(lift > 12 && lift < 40, "T5 resonance peak bounded", d);
  }

  // T6 — stability at the corners: full res, cutoff pinned to both clamp
  // ends, driven by a worst-case square wave for 5 s. The invariant is
  // BOUNDED AND DECAYING, not an absolute floor at a fixed time: the first
  // draft demanded < 1e-6 after 2 s and a 10 Hz / Q~20 ring honestly takes
  // longer — a slow decay is physics, self-oscillation is a bug. So: peak
  // bounded, and the silent tail at 3 s is under a tenth of the tail at 1 s.
  {
    bool stable = true; char d[96] = "all corners bounded, tails decay";
    for (double fcHz : {5.0, 30000.0})   // both beyond the clamp on purpose
      for (double rs : {0.0, 1.0})
      {
        hypersaw::SvfCore f; f.setSampleRate(fs); f.setParams(fcHz, rs); f.reset();
        double peak = 0;
        for (int i = 0; i < (int)(5 * fs); i++)
          peak = std::max(peak, std::fabs(f.process((i / 50) % 2 ? 1.0 : -1.0, hypersaw::SvfCore::kLowpass)));
        /* windowed peaks, not instantaneous samples: a single |y| lands on
           an arbitrary phase of the ring (measured: a zero-crossing at the
           1 s mark inflated the ratio to 0.127 on a decay that is honestly
           0.04). Peak over [0.9,1.0] s vs peak over [2.9,3.0] s. */
        double t1 = 0, t3 = 0;
        for (int i = 0; i < (int)(3 * fs); i++)
        {
          const double y = std::fabs(f.process(0.0, hypersaw::SvfCore::kLowpass));
          if (i >= (int)(0.9 * fs) && i < (int)(1.0 * fs)) t1 = std::max(t1, y);
          if (i >= (int)(2.9 * fs)) t3 = std::max(t3, y);
        }
        if (!(peak < 20.0) || !(t3 < t1 * 0.1 + 1e-12))
        {
          stable = false;
          std::snprintf(d, sizeof d, "fc=%.0f res=%.1f: peak %.3g tail1s %.3g tail3s %.3g", fcHz, rs, peak, t1, t3);
        }
      }
    ok(stable, "T6 stability at param corners", d);
  }

  // T7 — setParams is state-pure: `a` re-sets its params EVERY 16-sample tick
  // (the shell's cadence — mostly redundant sets), `b` sets only when the
  // value actually changes. Bit-identical output proves a redundant set is a
  // true no-op — no zipper state, no smoothing hidden inside the core (that
  // slew belongs to the caller, per the header contract).
  {
    hypersaw::SvfCore a, b; a.setSampleRate(fs); b.setSampleRate(fs);
    a.setParams(500, 0.5); b.setParams(500, 0.5);
    bool same = true; double lastC = 500;
    for (int i = 0; i < 4410; i++)
    {
      const double c = (i / 441) % 2 ? 2500.0 : 500.0;   // dwell 441 samples -> ~27 redundant ticks per dwell
      if (i % 16 == 0)
      {
        a.setParams(c, 0.5);                              // every tick, redundant or not
        if (c != lastC) { b.setParams(c, 0.5); lastC = c; }  // only on change
      }
      const double x = std::sin(i * 0.13) * 0.8;
      if (a.process(x, hypersaw::SvfCore::kHighpass) != b.process(x, hypersaw::SvfCore::kHighpass)) same = false;
    }
    ok(same, "T7 redundant setParams is a no-op", same ? "4410 samples bit-identical" : "diverged");
  }

  if (fails) { std::printf("svf_check: RED (%d failure(s))\n", fails); return 1; }
  std::printf("svf_check: GREEN (0 failures)\n");
  return 0;
}
