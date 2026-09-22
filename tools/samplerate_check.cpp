/* samplerate_check — behaviour expressed in SECONDS must not track the sample
 * rate. ADR-009 requires every slew and time constant to be declared in seconds
 * and converted to a coefficient, and bans hand-tuned per-tick constants.
 * Nothing enforced it until this file; the charter said it and the code was
 * trusted, which is exactly the shape "the audio thread must not allocate" had
 * before rtsafety_probe existed.
 *
 * IT HAS ALREADY EARNED ITS PLACE. Written the same hour ADR-086 shipped, it
 * found a defect in that fix: `kGravGrid = 256` was a fixed SAMPLE COUNT, so
 * the grid's duration shrank as the rate rose (5.81 ms at 44.1 k, 2.67 ms at
 * 96 k) and gravity's settle time drifted +0.42% at 96 kHz, monotonically. The
 * fix had removed a dependence on buffer size and left one on sample rate. No
 * golden could ever have seen it — goldens are generated at 44.1 kHz only, so
 * parity is silent about every other rate (L0031).
 *
 * THE MEASUREMENT'S OWN RESOLUTION MUST NOT TRACK THE VARIABLE UNDER TEST.
 * The first version stepped in 256-SAMPLE blocks, so its time resolution was
 * 5.8 ms at 44.1 k and 2.7 ms at 96 k — and it manufactured a 1.4% attack
 * variation that vanished to ±0.13% once the step was one MILLISECOND at every
 * rate with interpolated threshold crossings. A probe whose resolution depends
 * on the thing it is testing will report the effect it is looking for (L0032).
 * WIRED: ./verify full.
 */
#include <cmath>
#include <cstdio>
#include <vector>

#include "../src/swarm_core.h"

static int failures = 0;
static void check(bool ok, const char *what, const char *detail)
{
  std::printf("%-6s %s  (%s)\n", ok ? "OK" : "FAIL", what, detail);
  if (!ok) failures++;
}

// One millisecond of audio per step at EVERY rate, so the probe's resolution is
// constant in TIME. Crossings are interpolated for sub-step precision.
static int blockFor(double sr) { return (int)std::lround(sr * 0.001); }

// Seconds for the amplitude envelope to first reach `frac` of its peak.
static double attackSeconds(double sr, double frac)
{
  hypersaw::SwarmCore c(sr);
  c.setParam("seed", 1234);
  c.setParam("n", 5);
  c.setParam("attack", 0.05);
  c.noteOn(60, 261.6255653005986);
  const int blk = blockFor(sr), N = (int)(sr * 0.5);
  std::vector<float> L(blk), R(blk), env;
  double peak = 0;
  for (int done = 0; done + blk <= N; done += blk)
  {
    c.render(L.data(), R.data(), blk);
    double m = 0;
    for (int i = 0; i < blk; i++) m = std::fmax(m, std::fabs((double)L[i]));
    env.push_back((float)m);
    peak = std::fmax(peak, m);
  }
  const double target = frac * peak;
  for (size_t i = 1; i < env.size(); i++)
    if (env[i] >= target)
    {
      const double t = (target - env[i - 1]) / (env[i] - env[i - 1]);
      return ((double)i + t) * blk / sr;
    }
  return -1;
}

// Seconds for gravity to pull an equal-tempered major third within 0.5 cents of
// just 5/4. This is the quantity ADR-086's defect moved.
static double gravitySeconds(double sr)
{
  hypersaw::SwarmCore c(sr);
  c.setParam("seed", 1234);
  c.setParam("n", 1);
  c.setParam("detune", 0);
  c.setParam("K", 0);
  c.setParam("grav", 0.7);
  c.setParam("basin", 50);
  c.noteOn(60, 261.6255653005986);
  c.noteOn(64, 329.6275569128699);
  const int blk = blockFor(sr), N = (int)(sr * 4);
  std::vector<float> L(blk), R(blk);
  for (int done = 0; done + blk <= N; done += blk)
  {
    c.render(L.data(), R.data(), blk);
    double f[2] = {0, 0};
    int k = 0;
    for (int i = 0; i < 16 && k < 2; i++)
      if (c.voiceAt(i).gate) f[k++] = c.voiceAt(i).f0cur;
    if (f[0] > 0 && std::fabs(1200.0 * std::log2(f[1] / f[0]) - 386.3137) < 0.5)
      return (done + blk) / sr;
  }
  return -1;
}

int main()
{
  const double rates[] = {44100.0, 48000.0, 88200.0, 96000.0};
  const int NR = 4;
  double atk[NR], grv[NR];
  std::printf("%-9s %14s %16s\n", "rate", "attack 90% (s)", "gravity settle (s)");
  for (int i = 0; i < NR; i++)
  {
    atk[i] = attackSeconds(rates[i], 0.9);
    grv[i] = gravitySeconds(rates[i]);
    std::printf("%-9.0f %14.5f %16.5f\n", rates[i], atk[i], grv[i]);
  }

  auto worstDrift = [&](const double *v) {
    double w = 0;
    for (int i = 1; i < NR; i++) w = std::fmax(w, std::fabs(v[i] - v[0]) / v[0]);
    return w;
  };

  char d[160];
  // 0.3%: post-amendment worst is 0.16%, and the regression this exists for
  // measured 0.42%. The threshold sits between them with roughly 2x margin on
  // each side — chosen from measurement, then calibrated by re-planting the
  // sample-count grid, not picked for comfort.
  const double kTol = 0.003;

  const double aw = worstDrift(atk);
  std::snprintf(d, sizeof(d), "worst drift %.3f%% across 44.1-96 kHz (tol %.1f%%)",
                100 * aw, 100 * kTol);
  check(aw < kTol, "envelope attack time is expressed in seconds", d);

  const double gw = worstDrift(grv);
  std::snprintf(d, sizeof(d), "worst drift %.3f%% across 44.1-96 kHz (tol %.1f%%)",
                100 * gw, 100 * kTol);
  check(gw < kTol, "gravity settle time is expressed in seconds", d);

  std::printf("samplerate_check: %s (%d failures)\n", failures ? "RED" : "GREEN", failures);
  return failures ? 1 : 0;
}
