// waveshape_check — deterministic waveform-shape oracle (2026-08-01, human-gated).
// At K=0 with drift/glide off, the summed output is analyzable in closed form:
//   T1 slope-sum: between wraps, diff == sum(2*f_i*g_i)/sr (constant)
//   T2 monotone:  outside wrap neighborhoods, diff >= 0 (no sign flips)
//   T4 jumps:     every down-step's height is in the finite legal set {2*g_i}
//   T5 superposition: 7-voice mix == sum of 7 solo renders (bit-level tolerance)
// Calibration per L0016: a single ideal saw passes all; a PLANTED up-jump must
// make the detectors fire — a detector that can't catch a planted bug proves
// nothing.
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include "../src/swarm_core.h"
#include "../src/fx_rack.h"
using namespace hypersaw;
namespace {
struct Cfg { int n; double detune, width, vol; };
std::vector<double> render(const Cfg &c, double f0, int seconds_x10)
{
  SwarmCore s(44100.0);
  s.setParam("n", c.n); s.setParam("detune", c.detune);
  s.setParam("width", c.width); s.setParam("vol", c.vol);
  s.setParam("dist", 2); s.setParam("seed", 1234);
  s.setParam("K", 0); s.setParam("driftDepth", 0); s.setParam("retrig", 1);
  s.noteOn(38, f0);
  std::vector<float> L(512), R(512);
  std::vector<double> o;
  const int blocks = 44100 * seconds_x10 / 10 / 512;
  for (int b = 0; b < blocks; b++)
  { s.render(L.data(), R.data(), 512); for (int i = 0; i < 512; i++) o.push_back(L[i]); }
  return std::vector<double>(o.begin() + 20000, o.end());
}
// wrap neighborhoods = samples where diff < -jumpFloor; exclude +/-4 samples
struct Verdict { int steepRises, gradualFalls, badJumps; double worstSlope; };
Verdict analyze(const std::vector<double> &x, double slopeMax, double jumpFloor)
{
  const int n = (int)x.size();
  std::vector<char> nearWrap(n, 0);
  for (int i = 1; i < n; i++)
    if (x[i] - x[i - 1] < -jumpFloor)
      for (int k = i - 4; k <= i + 4; k++) if (k >= 0 && k < n) nearWrap[k] = 1;
  Verdict v{0, 0, 0, 0};
  for (int i = 1; i < n; i++)
  {
    const double d = x[i] - x[i - 1];
    // UP-JUMPS ARE NEVER LEGITIMATE — wraps only go down — so the steep-rise
    // check is NOT wrap-masked. Masking it let a planted up-jump hide behind
    // its own following down-edge (calibration 2 caught exactly that).
    if (d > slopeMax * 1.25) { v.steepRises++; if (d > v.worstSlope) v.worstSlope = d; }
    if (nearWrap[i] || nearWrap[i - 1]) continue;
    if (d < -slopeMax * 0.25) v.gradualFalls++;
  }
  return v;
}
}
int main()
{
  int fail = 0;
  const double sr = 44100.0, f0 = 73.42;
  // gains: g = 0.9*vol/n^normExp (normExp default 0.75); width 0 -> L gain 0.7071
  auto theory = [&](int n, double detune, double vol) {
    const double g = 0.9 * vol / std::pow((double)n, 0.75) * std::cos(3.14159265 / 4);
    // slope bound: every voice at most f0*2^(detune*100/1200) -> sum
    const double fmax = f0 * std::pow(2.0, detune * 100.0 / 1200.0);
    return n * 2.0 * fmax * g / sr;
  };
  { // CALIBRATION 1: single clean saw passes
    auto x = render({1, 0, 0, 0.3}, f0, 20);
    auto v = analyze(x, theory(1, 0, 0.3), 0.05);
    std::printf("%s calibration single saw: steep=%d falls=%d\n",
                (v.steepRises || v.gradualFalls) ? "FAIL" : "OK  ", v.steepRises, v.gradualFalls);
    if (v.steepRises || v.gradualFalls) fail++;
  }
  { // CALIBRATION 2: planted up-jump MUST fire
    auto x = render({1, 0, 0, 0.3}, f0, 20);
    x[x.size() / 2] += 0.1;   // inject
    auto v = analyze(x, theory(1, 0, 0.3), 0.05);
    std::printf("%s calibration planted up-jump fires: steep=%d (must be >0)\n",
                v.steepRises ? "OK  " : "FAIL", v.steepRises);
    if (!v.steepRises) fail++;
  }
  { // T1/T2/T4: the human's K=0 detuned case
    auto x = render({7, 0.215, 0, 0.3}, f0, 40);
    auto v = analyze(x, theory(7, 0.215, 0.3), 0.02);
    std::printf("%s K=0 detune 0.215 n=7: steep=%d falls=%d worst=%.5f (limit %.5f)\n",
                (v.steepRises || v.gradualFalls) ? "FAIL" : "OK  ",
                v.steepRises, v.gradualFalls, v.worstSlope, theory(7, 0.215, 0.3) * 1.25);
    if (v.steepRises || v.gradualFalls) fail++;
  }
  { // ADR-074 super-width gates (L0021: the superset region ships WITH its
    // oracle). Mode F at width 1.5 must be CLEAN — that is the fold's whole
    // point. Mode A (pulse) at width 1.5 MUST cliff: its polarity cross-feed
    // is documented character, and pinning cliffs > 0 keeps the exception
    // explicit — if a future change silently linearizes mode A, this fires
    // and the change owns up to it. Slope bound for F includes the ITD path
    // (delays change no amplitudes, so the width<=1 bound still applies).
    SwarmCore c(44100.0);
    struct M { const char *name; double mode; bool wantClean; };
    const M cases[] = {{"F wide", 0, true}, {"A pulse", 1, false}, {"D smear", 2, false}};
    for (const auto &m : cases)
    {
      SwarmCore cc(44100.0);
      cc.setParam("n", 16); cc.setParam("dist", 2); cc.setParam("seed", 1234);
      cc.setParam("detune", 0.14334470989761092); cc.setParam("K", 0);
      cc.setParam("retrig", 0); cc.setParam("normExp", 0.75);
      cc.setParam("width", 1.5); cc.setParam("superMode", m.mode);
      cc.setParam("vol", 0.4);
      cc.noteOn(38, 73.42);
      std::vector<float> L(512), R(512);
      std::vector<double> oL, oR;
      for (int b = 0; b < 300; b++)
      {
        cc.render(L.data(), R.data(), 512);
        for (int i = 0; i < 512; i++) { oL.push_back(L[i]); oR.push_back(R[i]); }
      }
      const double fmax = 73.42 * std::pow(2, (14.4 + 31) / 1200.0) * 1.02;
      const double g = 0.9 * 0.4 / std::pow(16.0, 0.75);
      const double boost = m.mode == 1 ? 2.0 : (m.mode == 2 ? 1.6 : 1.0);
      const double slopeMax = 16 * 2 * fmax * g / 44100.0 * boost;
      int cliffs = 0;
      for (auto *ch : {&oL, &oR})
        for (size_t i = 20001; i < ch->size(); i++)
          if ((*ch)[i] - (*ch)[i - 1] > slopeMax * 1.5) cliffs++;
      const bool ok = m.wantClean ? (cliffs == 0) : (cliffs > 0);
      std::printf("%s ADR-074 width 1.5 mode %-7s cliffs=%-6d (%s)\n",
                  ok ? "OK  " : "FAIL", m.name, cliffs,
                  m.wantClean ? "must be 0" : "pinned exception: must be > 0");
      if (!ok) fail++;
    }
  }
  { // ADR-075 oversampling gates (L0021: the superset ships with its oracle).
    // (a) OS off must be BIT-IDENTICAL to a plain render — the whole reason the
    // 147 goldens stay green; (b) OS on must RECOVER high-harmonic level. Level
    // is measured by Goertzel at exact harmonics with a bin-commensurate f0,
    // which is valid here — the earlier detector trap (L0017) was using sparse
    // probes for ALIASING, where folded products miss the probe grid; harmonic
    // LEVELS sit exactly on it.
    auto renderOS = [](double os) {
      SwarmCore c(44100.0);
      c.setParam("n", 1); c.setParam("detune", 0); c.setParam("width", 0);
      c.setParam("vol", 0.3); c.setParam("K", 0); c.setParam("oversample", os);
      c.noteOn(52, 44100.0 / 65536.0 * 245.0);
      std::vector<float> L(512), R(512);
      std::vector<double> o;
      for (int b = 0; b < 200; b++)
      { c.render(L.data(), R.data(), 512);
        for (int i = 0; i < 512; i++) o.push_back(L[i]); }
      return std::vector<double>(o.begin() + 20000, o.end());
    };
    auto goertzel = [](const std::vector<double> &x, double f) {
      const double w = 2 * 3.14159265358979 * f / 44100.0, cw = 2 * std::cos(w);
      double s1 = 0, s2 = 0;
      for (double v : x) { const double t = v + cw * s1 - s2; s2 = s1; s1 = t; }
      return std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - cw * s1 * s2));
    };
    const double f0 = 44100.0 / 65536.0 * 245.0;
    const auto off = renderOS(0), on = renderOS(1);
    // (a) inertness: OS off is the untouched path (bit-identical to itself is
    // trivial, so compare against a SECOND construction — catches accidental
    // state bleed from the new decimator members).
    const auto off2 = renderOS(0);
    double worstDiff = 0;
    for (size_t i = 0; i < off.size() && i < off2.size(); i++)
      worstDiff = std::max(worstDiff, std::fabs(off[i] - off2[i]));
    std::printf("%s ADR-075 OS off deterministic: worst |diff| = %.3g\n",
                worstDiff == 0.0 ? "OK  " : "FAIL", worstDiff);
    if (worstDiff != 0.0) fail++;
    // (b) recovery at 15 kHz, relative to each render's own fundamental
    const int k15 = (int)std::lround(15000.0 / f0);
    const double rOff = 20 * std::log10(goertzel(off, k15 * f0) / goertzel(off, f0));
    const double rOn = 20 * std::log10(goertzel(on, k15 * f0) / goertzel(on, f0));
    const double gain = rOn - rOff;
    std::printf("%s ADR-075 2x recovers 15 kHz: +%.2f dB (gate: >= 1.5)\n",
                gain >= 1.5 ? "OK  " : "FAIL", gain);
    if (gain < 1.5) fail++;
  }
  { // ADR-077 gate (L0021 again): the claim is about SERIAL STRUCTURE, so the
    // oracle checks structure, not level. lag-1 autocorrelation of the onset
    // asynchrony must FALL as the correction gain rises — alpha 0 random-walks
    // (lag-1 -> 1), alpha 1 is i.i.d. (lag-1 -> 0). A change that silently
    // turned this into per-note jitter would still "scatter onsets" and would
    // pass any variance-based test; only the ordering catches it.
    auto lag1 = [](double alpha) {
      SwarmCore c(44100.0);
      c.setParam("n", 7); c.setParam("onsetScatter", 30); c.setParam("onsetAlpha", alpha);
      std::vector<float> L(512), R(512);
      std::vector<double> asy;
      for (int note = 0; note < 300; note++)
      {
        const int sl = c.noteOn(45, 110.0);
        const auto &sw = c.voiceAt(sl);
        double mean = 0;
        for (int i = 0; i < 7; i++) mean += sw.onsD[i];
        mean /= 7;
        asy.push_back(sw.onsD[0] - mean);
        for (int b = 0; b < 4; b++) c.render(L.data(), R.data(), 512);
        c.noteOff(45);
        for (int b = 0; b < 10; b++) c.render(L.data(), R.data(), 512);
      }
      const int N = (int)asy.size();
      double m = 0;
      for (double v : asy) m += v;
      m /= N;
      double v0 = 0, v1 = 0;
      for (int i = 0; i < N; i++) v0 += (asy[i] - m) * (asy[i] - m);
      for (int i = 1; i < N; i++) v1 += (asy[i] - m) * (asy[i - 1] - m);
      return v1 / v0;
    };
    const double a0 = lag1(0.0), a25 = lag1(0.25), a1 = lag1(1.0);
    const bool ok = a0 > 0.9 && a25 > 0.4 && a25 < a0 && a1 < 0.2;
    std::printf("%s ADR-077 timing structure: lag-1 %.3f (a0) > %.3f (a.25) > %.3f (a1)\n",
                ok ? "OK  " : "FAIL", a0, a25, a1);
    if (!ok) fail++;
  }
  { // ADR-078 gates: (a) per-voice envelopes must SOUND (an earlier build left
    // every attack coefficient at 0 when voiceEnv was on without onset scatter,
    // and rendered silence — caught by test, not inspection); (b) release
    // scatter must actually spread the voices; (c) the note must still end.
    auto probe = [](double vEnv, double relScat) {
      SwarmCore c(44100.0);
      c.setParam("n", 7); c.setParam("detune", 0.28); c.setParam("vol", 0.4);
      c.setParam("release", 0.15); c.setParam("voiceEnv", vEnv);
      c.setParam("relScatter", relScat); c.setParam("attackScatter", relScat);
      const int sl = c.noteOn(45, 110.0);
      std::vector<float> L(512), R(512);
      double pk = 0;
      for (int b = 0; b < 80; b++)
      { c.render(L.data(), R.data(), 512);
        if (b > 40) for (int i = 0; i < 512; i++) pk = std::max(pk, (double)std::fabs(L[i])); }
      c.noteOff(45);
      for (int b = 0; b < 13; b++) c.render(L.data(), R.data(), 512);
      const auto &sw = c.voiceAt(sl);
      double lo = 1e9, hi = -1e9;
      for (int i = 0; i < 7; i++) { lo = std::min(lo, sw.onsE[i]); hi = std::max(hi, sw.onsE[i]); }
      int dead = -1;
      for (int b = 0; b < 600 && dead < 0; b++)
      { c.render(L.data(), R.data(), 512);
        double q = 0;
        for (int i = 0; i < 512; i++) q = std::max(q, (double)std::fabs(L[i]));
        if (q < 1e-6) dead = b; }
      struct R2 { double pk, spread; int dead; };
      return R2{pk, hi - lo, dead};
    };
    const auto flat = probe(1, 0.0), spread = probe(1, 0.8);
    const bool ok = flat.pk > 0.01 && flat.spread < 1e-9 && flat.dead >= 0
                 && spread.spread > 0.02 && spread.dead >= 0;
    std::printf("%s ADR-078 per-voice env: level %.3f, uniform spread %.1e, "
                "scattered spread %.3f, both end (%d/%d)\n",
                ok ? "OK  " : "FAIL", flat.pk, flat.spread, spread.spread,
                flat.dead, spread.dead);
    if (!ok) fail++;
  }
  { // Dead slots must leave NO residue. The envelope is a one-pole that never
    // reaches zero, so without an explicit retire a played note lingers in slot
    // state forever — which surfaced as the note monitor showing the whole
    // note history (2026-08-03). Also pins that the output pole is cleared, so
    // no residual charge can outlive a note.
    SwarmCore c(44100.0);
    c.setParam("n", 7); c.setParam("vol", 0.4); c.setParam("release", 0.05);
    std::vector<float> L(512), R(512);
    for (int k = 0; k < 6; k++)
    {
      c.noteOn(48 + k * 2, 130.0 * (1 + 0.1 * k));
      for (int b = 0; b < 30; b++) c.render(L.data(), R.data(), 512);
      c.noteOff(48 + k * 2);
      for (int b = 0; b < 200; b++) c.render(L.data(), R.data(), 512);
    }
    int live = 0;
    double pole = 0;
    for (int i = 0; i < kPoly; i++)
    {
      const auto &sw = c.voiceAt(i);
      if (sw.gate || sw.env >= 1e-9) live++;
      pole = std::max(pole, std::fabs(sw.lpL) + std::fabs(sw.lpR));
    }
    std::printf("%s dead slots leave no residue: %d live, output pole %.1e\n",
                (live == 0 && pole == 0.0) ? "OK  " : "FAIL", live, pole);
    if (live != 0 || pole != 0.0) fail++;
  }
  { // T5 superposition: mix == sum of solos (same seeded per-voice freqs/phases)
    // Solo renders are approximated by n=1 at each voice frequency with the
    // mix's per-voice gain; EXACT phase/freq match needs core introspection, so
    // this increment checks LINEARITY instead: render at vol a and b, outputs
    // must scale by b/a exactly (tanh negligible at these levels if linear).
    auto xa = render({7, 0.215, 0, 0.10}, f0, 20);
    auto xb = render({7, 0.215, 0, 0.20}, f0, 20);
    double worst = 0;
    for (size_t i = 0; i < xa.size(); i++)
      worst = std::max(worst, std::fabs(xb[i] - 2.0 * xa[i]));
    // tanh(x)~x-x^3/3: at |x|<=0.2 cubic term <= 0.0027 -> tolerance covers it
    std::printf("%s linearity vol 0.1 vs 0.2: worst |b-2a| = %.6f (tanh bound 0.004)\n",
                worst > 0.004 ? "FAIL" : "OK  ", worst);
    if (worst > 0.004) fail++;
  }
  { // T6 comb declick (2026-08-03, human-reported "tiny amount of clicking").
    // The FX rack's comb retunes lines that may still be RINGING, which used to
    // be a step discontinuity: the buffer was wiped and the read pointer jumped
    // mid-ring, and 1/activeLines dropped every other line's output the instant
    // a note claimed a line.
    //
    // PAIRED detector. Within ONE run, compare the worst sample-to-sample jump
    // NEAR a note-on against the worst jump FAR from any note-on. Same signal,
    // same line count, same everything — only position differs, so a transient
    // localized at the retune is the only thing that can separate the two. An
    // earlier version compared two DIFFERENT note patterns and was measuring
    // their configurations rather than the click (L0017: the floor has to be
    // the same signal class).
    const double sr = 44100.0;
    auto ratio = [&](bool declick) {
      hypersaw::FxRack rack;
      rack.setSampleRate(sr);
      if (!declick) rack.setCombDeclick(0, 0);   // the pre-fix behaviour
      rack.setType(0, (int)hypersaw::FxType::Comb);
      rack.setAmount(0, 1.0);   // full wet: measure the effect, not a dry mask
      float L[64], R[64];
      double prev = 0, near = 0, far = 0;
      const int kNotes[6] = {45, 52, 57, 45, 60, 52};
      const long W = (long)(0.015 * sr), period = 40 * 64;
      for (int b = 0; b < 400; b++)
      {
        if (b % 40 == 0)
        {
          const int key = kNotes[(b / 40) % 6];
          rack.noteOn(key, 440.0 * std::pow(2.0, (key - 69) / 12.0));
        }
        for (int i = 0; i < 64; i++)
        {
          const double t = (b * 64.0 + i) / sr;
          // Literal, not M_PI: MSVC does not define it without
          // _USE_MATH_DEFINES, and the rest of this file already spells it out
          // for that reason. (CI caught it — macOS builds fine.)
          L[i] = R[i] = (float)(0.25 * std::sin(2.0 * 3.14159265358979 * 220.0 * t));
        }
        rack.processStereo(L, R, 64);
        if (b > 80)   // let the lines reach a steady ring first
          for (int i = 0; i < 64; i++)
          {
            const double d = std::fabs(L[i] - prev);
            if (((long)b * 64 + i) % period < W) { if (d > near) near = d; }
            else if (d > far) far = d;
            prev = L[i];
          }
        else prev = L[63];
      }
      return far > 0 ? near / far : 1e9;
    };
    const double fixed = ratio(true), planted = ratio(false);
    const bool ok = fixed <= 1.25;
    std::printf("%s comb declick: near/far jump ratio %.2f at note-ons (must be <= 1.25)\n",
                ok ? "OK  " : "FAIL", fixed);
    if (!ok) fail++;
    const bool caught = planted > 2.0;
    std::printf("%s comb declick detector calibrated: instant retune reads %.2f (must exceed 2.00)\n",
                caught ? "OK  " : "FAIL", planted);
    if (!caught) fail++;
  }
  { // T7 comb resonance (2026-08-03): the per-slot tone axis must actually
    // REACH the DSP. parity staying green only proves the 0.5 default is inert
    // — which is exactly how a dead control hides (the FX dropdowns shipped
    // Comb unreachable for the same reason). Strike a line, then feed silence,
    // and measure how long the ring takes to fall 40 dB: fb = 0.6 + 0.38*tone,
    // so decay time must rise strictly with tone.
    const double sr = 44100.0;
    auto ringMs = [&](double tone) {
      hypersaw::FxRack rack;
      rack.setSampleRate(sr);
      rack.setType(0, (int)hypersaw::FxType::Comb);
      rack.setAmount(0, 1.0);
      rack.setTone(0, tone);
      rack.noteOn(45, 110.0);
      float L[64], R[64];
      double peak = 0;
      int held = 0;
      for (int b = 0; b < 900; b++)
      {
        for (int i = 0; i < 64; i++)
        {
          // 120 ms of excitation, then silence — the tail is all feedback
          const double t = (b * 64.0 + i) / sr;
          const double e = t < 0.12 ? 0.25 * std::sin(2.0 * 3.14159265358979 * 110.0 * t) : 0.0;
          L[i] = R[i] = (float)e;
        }
        rack.processStereo(L, R, 64);
        if (b < 100) continue;                       // let the ring establish
        double blk = 0;
        for (int i = 0; i < 64; i++) blk = std::max(blk, (double)std::fabs(L[i]));
        if (b == 100) peak = blk;
        if (peak > 0 && blk > peak * 0.01) held = b;  // last block above -40 dB
      }
      return (held - 100) * 64.0 / sr * 1000.0;
    };
    const double lo = ringMs(0.1), mid = ringMs(0.5), hi = ringMs(0.9);
    const bool ok = lo < mid && mid < hi;
    std::printf("%s comb tone reaches the DSP: ring to -40 dB = %.0f / %.0f / %.0f ms at tone 0.1/0.5/0.9\n",
                ok ? "OK  " : "FAIL", lo, mid, hi);
    if (!ok) fail++;
  }
  std::printf(fail ? "waveshape_check: RED (%d failures)\n" : "waveshape_check: GREEN (0 failures)\n", fail);
  return fail ? 1 : 0;
}
