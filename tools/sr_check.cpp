/* sr_check — sample-rate independence of the WHOLE engine, in SECONDS.
 *
 * B147 layer 2. `samplerate_check` (a gate) tests two quantities — envelope
 * attack and gravity settle — at 0.3 % tolerance, and passes. The B147 layer-1
 * audit (docs/audits/2026-09-18-saw-engine-audit.md §3.2) measured four more
 * that it is structurally blind to, and every one of them drifts:
 *
 *                                        audit    this probe   after B150
 *   K-step settling (0 -> 1, to 90 %)   54.9 %      54.876 %      1.252 %
 *   onset-lock t(R peak), dissolve 0.30  5.56 %      5.739 %      0.536 %
 *   inertia steady-state R              15.03 %      2.103 %      0.763 %
 *   default output pole at 10 kHz        0.40 dB     0.400 dB     0.400 dB
 *
 * The first two were ONE defect: both are set by the coupling smoother, and
 * expressing its 0.08 in seconds (swarm_core.h kKsmTauSeconds) closed both.
 * The last one is STOPPED, not fixed — see the bar comment.
 *
 * B150 HAS NOW RULED (human, 2026-09-18, option a): express the rate-bound
 * constants in seconds with 44.1 kHz special-cased bit-frozen, so every other
 * rate is corrected to match 44.1 k. That ruling is the ONE sanction under
 * which the bars in this file move DOWN — a gate threshold is never otherwise
 * edited without a recorded human decision (charter, Oracle discipline). Each
 * bar below therefore carries its own before/after pair and the commit that
 * moved it. The file's original posture — "the bars are today's drifts plus a
 * margin, never the aspiration" — is unchanged; what changed is which day
 * "today" is.
 *
 * STANDALONE AND UNWIRED. Wiring a gate into ./verify is the human's decision
 * (charter); this is proposed, not wired. `samplerate_check` is untouched.
 *
 * MEASUREMENT DISCIPLINE, inherited verbatim from samplerate_check's header:
 * the probe's own resolution must not track the variable under test. Every
 * probe here steps a fixed number of MILLISECONDS at every rate and
 * interpolates the crossing; a probe that stepped a fixed number of SAMPLES
 * would manufacture exactly the effect it is looking for (L0032).
 *
 * CONTROLS (L0016/L0032 — a drift detector is worthless until it has been
 * shown reading ~zero on something known-clean and large on something
 * known-dirty):
 *   - must-read-~zero: the envelope attack time, the quantity samplerate_check
 *     already certifies at 0.125 %. If the drift arithmetic here reported a
 *     large number for that, everything below is noise.
 *   - must-read-large: a synthetic quantity defined as a fixed SAMPLE COUNT
 *     (1000 samples expressed in seconds). It is the defect class in pure
 *     form, computed with no engine involved, and the detector must report
 *     ~54 % on it. A detector that cannot produce a large number has never
 *     been shown able to fail.
 *   - the output-pole probe carries its own pair: the closed-form magnitude is
 *     checked against a numerical simulation of the same recurrence (must
 *     agree), and against a deliberately wrong coefficient (must disagree).
 *   - the onset-lock probe asserts it saw an interior R peak at all. Its first
 *     version did not, and read a drift of exactly 0.000 % at every rate —
 *     green, and blind. See lockPeakSeconds.
 *   - the inertia probe carries a SAME-RATE control: the same quantity over
 *     two disjoint windows at one rate. See inertiaR.
 *
 * ONE OF THE FOUR NUMBERS DID NOT SURVIVE ITS OWN CONTROL, and that is
 * recorded here rather than smoothed over. The inertia steady-state R does not
 * settle; it wanders. Over the audit's 10-12 s window it reads 0.25485 at
 * 44.1 k and 0.29398 over 20-30 s at the SAME rate — a 15.3 % swing with the
 * sample rate held fixed, which is the size of the 15.03 % cross-rate spread
 * the audit attributed to sample rate. Averaged over 10-120 s instead, the
 * within-rate wander falls to 3.6 % and the cross-rate spread reads 2.1 %:
 * BELOW its own control. So the honest current reading is "no rate dependence
 * of this quantity is resolvable above its wander", and the audit's 15.03 %
 * survives here only as the bar it must not grow back to. This is not a
 * correction offered lightly — it is what the control says, and a control
 * that disagrees with the number outranks the number (L0032).
 */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "../src/swarm_core.h"

namespace
{

// M_PI is undefined under MSVC (L0003); every tool in this tree carries its own.
constexpr double kPi = 3.14159265358979323846;
// ULP of the two constants the kKsmTauSeconds control compares. Written as
// literals rather than std::nextafter so the bar in the printout is a fixed
// number a reader can check by hand.
constexpr double kUlp = 8.673617379884035e-19;    // ulp(0.00435122...)
constexpr double kUlp08 = 1.3877787807814457e-17; // ulp(0.08)

int g_failures = 0;
void check(bool ok, const char *what, const char *detail)
{
  std::printf("%-6s %s  (%s)\n", ok ? "PASS" : "FAIL", what, detail);
  if (!ok) g_failures++;
}

// The four rates. 88.2 k is INCLUDED even though the dispatch brief names only
// 44.1/48/96: the audit's 15.03 % inertia spread is visible ONLY at 88.2 k
// (0.25237 there against 0.279-0.294 elsewhere, and the spread is
// non-monotonic in rate). Dropping it would have silently weakened the check
// to about a third of the drift it exists to pin.
constexpr double kRates[] = {44100.0, 48000.0, 88200.0, 96000.0};
constexpr int kNR = 4;

// Steps of a fixed DURATION at every rate. 0.2 ms: the control tick is
// 16 samples = 0.363 ms at 44.1 k and 0.167 ms at 96 k, so this samples the
// tick grid at every rate rather than under it at one and over it at another.
int stepFor(double sr) { return (int)std::lround(sr * 0.0002); }
// 1 ms blocks for the long steady-state renders (R moves on the tick grid; a
// 0.2 ms step over 12 s is 60 000 render calls for no extra information).
int blockFor(double sr) { return (int)std::lround(sr * 0.001); }

// Worst |v_i - v_0| / v_0 across the rate list. Deliberately the SAME
// definition samplerate_check::worstDrift already uses — a second drift
// definition in the same repo is how two green checks come to disagree.
double worstDrift(const double *v)
{
  double w = 0;
  for (int i = 1; i < kNR; i++) w = std::fmax(w, std::fabs(v[i] - v[0]) / v[0]);
  return w;
}
// (max - min) / min. Reported alongside, because the audit's §3.5 table quotes
// its inertia figure as a spread, not as a drift from 44.1 k, and a reader
// comparing this file to that table needs both numbers to line up.
double spread(const double *v)
{
  double lo = v[0], hi = v[0];
  for (int i = 1; i < kNR; i++) { lo = std::fmin(lo, v[i]); hi = std::fmax(hi, v[i]); }
  return (hi - lo) / lo;
}

// A settled swarm holding middle C, with the SAW defaults the audit used.
void patchBase(hypersaw::SwarmCore &c)
{
  c.setParam("seed", 1234);
  c.setParam("n", 7);
  c.setParam("detune", 0.28);
  c.setParam("K", 0);
}

// Slot of the one held note (noteOn returns it; never assume 0).
int holdC4(hypersaw::SwarmCore &c) { return c.noteOn(60, 261.6255653005986); }

// ---------------------------------------------------------------- A: K step
// Seconds for the coupling smoother to reach 90 % of its final value after a
// 0 -> 1 step on the K knob. This is the quantity the hand-tuned per-tick 0.08
// (swarm_core.h:1631) sets, and it is the ADR-009 defect class in the open.
double kStepSeconds(double sr)
{
  hypersaw::SwarmCore c(sr);
  patchBase(c);
  const int slot = holdC4(c);
  const int blk = blockFor(sr), step = stepFor(sr);
  std::vector<float> L(blk), R(blk);
  for (int i = 0; i < (int)(sr * 0.5) / blk; i++) c.render(L.data(), R.data(), blk);  // settle at K = 0

  c.setParam("K", 1);
  // Final value first: the target is MEASURED, not the 4K^2*sigma formula
  // re-derived here. A detector that recomputes the engine's own expression
  // agrees with the engine by construction and sees nothing (L0032).
  std::vector<double> trace;
  const int nSteps = (int)(sr * 0.2) / step;   // 200 ms >> the ~10 ms rise
  std::vector<float> l2(step), r2(step);
  for (int i = 0; i < nSteps; i++)
  {
    c.render(l2.data(), r2.data(), step);
    trace.push_back(c.voiceAt(slot).KsmS);
  }
  const double target = 0.9 * trace.back();
  for (size_t i = 1; i < trace.size(); i++)
    if (trace[i] >= target)
    {
      const double t = (target - trace[i - 1]) / (trace[i] - trace[i - 1]);
      return ((double)i + t) * step / sr;
    }
  return -1;
}

// ------------------------------------------------------- B: onset-lock peak
// Seconds from note-on to the peak of the order parameter R under the onset
// lock. The "snap" — the engine's most characteristic gesture.
//
// `scatter 1` IS LOAD-BEARING and was the whole difficulty in reproducing the
// audit's 44.4 / 45.9 / 44.9 ms row. Without it the swarm starts phase-aligned
// (R = 1.000 on the first control tick) and R only ever decays, so "the time
// of the R peak" is t = 0 at every rate and the probe reports a drift of
// exactly 0.000 % — a green reading from a probe that never saw the gesture,
// which is the worst failure an oracle has. With the phases scattered at
// note-on, R climbs under the onset coupling, turns over, and dissolves; the
// turn-over is the snap, and WHERE it sits is what the per-tick 0.08 smoother
// moves with the sample rate.
double lockPeakSeconds(double sr, double dissolve)
{
  hypersaw::SwarmCore c(sr);
  patchBase(c);
  c.setParam("scatter", 1);
  c.setParam("onset", 1);
  c.setParam("dissolve", dissolve);
  const int slot = holdC4(c);
  const int step = stepFor(sr);
  std::vector<float> L(step), Rb(step);
  std::vector<double> trace;
  const int nSteps = (int)(sr * 0.25) / step;
  for (int i = 0; i < nSteps; i++)
  {
    c.render(L.data(), Rb.data(), step);
    trace.push_back(c.voiceAt(slot).R);
  }
  size_t best = 0;
  for (size_t i = 1; i < trace.size(); i++) if (trace[i] > trace[best]) best = i;
  // Parabolic refinement on the three samples around the maximum: the peak of
  // a smooth hump between two grid points is not at a grid point, and without
  // this the answer quantises to the step and the drift becomes step noise.
  double frac = 0;
  if (best > 0 && best + 1 < trace.size())
  {
    const double d = trace[best - 1] - 2 * trace[best] + trace[best + 1];
    if (d != 0) frac = 0.5 * (trace[best - 1] - trace[best + 1]) / d;
  }
  return ((double)best + frac) * step / sr;
}

// ------------------------------------------------- C: inertia steady-state R
// R averaged over t = 10..120 s, long after the 0.08 smoother has settled — so
// this is the spring law's own rate dependence (swarm_core.h:1805-1813,
// explicit Euler on a dt that is kTick/sr), not section A's.
//
// THE AVERAGING WINDOW IS THE MEASUREMENT, and a short one measures the wrong
// thing. R in this patch (K 0.6, inertia 0.7) does not settle to a constant —
// it WANDERS. Averaged over 10..12 s, the audit's window, it reads 0.25485 at
// 44.1 k; over 20..30 s, at the SAME rate, it reads 0.29398. That is a 15.3 %
// swing with the variable under test held fixed, and it is the same size as
// the 15.03 % cross-rate spread the audit attributed to sample rate. A
// detector whose own control moves as far as its signal has not measured its
// signal (L0032).
//
// Over 10..120 s the wander averages down: the same-rate control below (two
// disjoint 55 s halves at each rate) reads at most ~3.6 %, and the cross-rate
// spread reads ~2.1 %. So the honest reading of this quantity at this
// averaging length is "no rate dependence resolvable above the wander" — NOT
// the audit's 15.03 %, which is recorded here as the bar it must not grow back
// to rather than as a current fact. The control prints beside the number so
// the resolution is visible instead of assumed.
constexpr double kInertFrom = 10.0, kInertMid = 65.0, kInertTo = 120.0;
struct InertR { double full, half1, half2; };
InertR inertiaR(double sr)
{
  hypersaw::SwarmCore c(sr);
  patchBase(c);
  c.setParam("K", 0.6);
  c.setParam("inertia", 0.7);
  const int slot = holdC4(c);
  const int blk = blockFor(sr);
  std::vector<float> L(blk), Rb(blk);
  double a = 0, b = 0, d = 0; long na = 0, nb = 0, nd = 0;
  const long nBlocks = (long)(sr * kInertTo) / blk;
  for (long i = 0; i < nBlocks; i++)
  {
    c.render(L.data(), Rb.data(), blk);
    const double t = (double)(i + 1) * blk / sr, r = c.voiceAt(slot).R;
    if (t >= kInertFrom) { a += r; na++; }
    if (t >= kInertFrom && t < kInertMid) { b += r; nb++; }
    if (t >= kInertMid) { d += r; nd++; }
  }
  return {a / (double)na, b / (double)nb, d / (double)nd};
}

// ----------------------------------------- the must-read-zero control probe
// samplerate_check's own attack quantity, reproduced here so this file's drift
// arithmetic is exercised on something already certified at 0.125 %. Copied
// rather than shared because samplerate_check is a GATE and out of this
// brief's scope: sharing it would have meant editing a gate.
double attackSeconds(double sr)
{
  hypersaw::SwarmCore c(sr);
  c.setParam("seed", 1234);
  c.setParam("n", 5);
  c.setParam("attack", 0.05);
  c.noteOn(60, 261.6255653005986);
  const int blk = blockFor(sr);
  std::vector<float> L(blk), R(blk), env;
  double peak = 0;
  for (int done = 0; done + blk <= (int)(sr * 0.5); done += blk)
  {
    c.render(L.data(), R.data(), blk);
    double m = 0;
    for (int j = 0; j < blk; j++) m = std::fmax(m, std::fabs((double)L[j]));
    env.push_back((float)m);
    peak = std::fmax(peak, m);
  }
  const double target = 0.9 * peak;
  for (size_t j = 1; j < env.size(); j++)
    if (env[j] >= target)
      return ((double)j + (target - env[j - 1]) / (env[j] - env[j - 1])) * blk / sr;
  return -1;
}

// ------------------------------------------------------ D: the output pole
// The engine's own coefficient, read back rather than re-derived: `lpc` is a
// public Voice field written every control tick from
// `1 - exp(-kTau*fc/sr)` (swarm_core.h:1818). Reading it means a change to the
// fc law moves this check; recomputing it here would not.
double poleCoef(double sr)
{
  hypersaw::SwarmCore c(sr);
  patchBase(c);              // rtone default 0 => fc clamped to 18 kHz
  const int slot = holdC4(c);
  const int blk = blockFor(sr);
  std::vector<float> L(blk), R(blk);
  for (int i = 0; i < 20; i++) c.render(L.data(), R.data(), blk);
  return c.voiceAt(slot).lpc;
}

// |H(f)| in dB of y += a*(x - y), i.e. H(z) = a / (1 - (1-a) z^-1).
double onePoleDb(double a, double f, double sr)
{
  const double w = 2 * kPi * f / sr;
  const double b = 1 - a;
  const double re = 1 - b * std::cos(w), im = b * std::sin(w);
  return 20 * std::log10(a / std::sqrt(re * re + im * im));
}

// The same recurrence, run. Calibrates the closed form above against the
// arithmetic it claims to describe.
double onePoleSimDb(double a, double f, double sr)
{
  double y = 0;
  const int warm = (int)(sr / f) * 200, meas = (int)(sr / f) * 200;
  double peak = 0;
  for (int i = 0; i < warm + meas; i++)
  {
    const double x = std::sin(2 * kPi * f * i / sr);
    y += a * (x - y);
    if (i >= warm) peak = std::fmax(peak, std::fabs(y));
  }
  return 20 * std::log10(peak);
}

}  // namespace

int main()
{
  std::printf("sr_check — sample-rate independence in SECONDS (B147 layer 2)\n");
  std::printf("Bars are the 2026-09-18 audit's measured drifts plus a stated margin, NOT the\n");
  std::printf("0.3%% samplerate_check bar: shrinking them is ruling B150, not this file's.\n\n");

  double kstep[kNR], lock02[kNR], lock05[kNR], lock30[kNR], pole10k[kNR], atk[kNR], sampleQty[kNR];
  double inertFull[kNR], inertWander[kNR];

  std::printf("%-9s %13s %11s %11s %11s %11s %13s\n",
              "rate", "K step 90%(s)", "lock .02", "lock .05", "lock .30", "inertia R", "pole@10k(dB)");
  for (int i = 0; i < kNR; i++)
  {
    const double sr = kRates[i];
    kstep[i] = kStepSeconds(sr);
    lock02[i] = lockPeakSeconds(sr, 0.02);
    lock05[i] = lockPeakSeconds(sr, 0.05);
    lock30[i] = lockPeakSeconds(sr, 0.30);
    const InertR ir = inertiaR(sr);
    inertFull[i] = ir.full;
    inertWander[i] = std::fabs(ir.half1 - ir.half2) / std::fmin(ir.half1, ir.half2);
    pole10k[i] = onePoleDb(poleCoef(sr), 10000.0, sr);
    atk[i] = attackSeconds(sr);
    sampleQty[i] = 1000.0 / sr;   // the defect class in pure form
    std::printf("%-9.0f %13.5f %11.5f %11.5f %11.5f %11.5f %13.3f\n",
                sr, kstep[i], lock02[i], lock05[i], lock30[i], inertFull[i], pole10k[i]);
  }

  std::printf("\n%-26s %14s %11s\n", "quantity", "drift v 44.1k", "spread");
  auto row = [](const char *nm, const double *v) {
    std::printf("%-26s %13.3f%% %10.3f%%\n", nm, 100 * worstDrift(v), 100 * spread(v));
  };
  row("K step to 90%", kstep);
  row("lock peak, dissolve 0.02", lock02);
  row("lock peak, dissolve 0.05", lock05);
  row("lock peak, dissolve 0.30", lock30);
  row("inertia R, 10-120 s", inertFull);
  row("CONTROL attack 90%", atk);
  row("CONTROL 1000 samples", sampleQty);

  double poleWorst = 0;
  for (int i = 1; i < kNR; i++) poleWorst = std::fmax(poleWorst, std::fabs(pole10k[i] - pole10k[0]));
  std::printf("%-26s %13.3f dB\n", "output pole @10 kHz", poleWorst);

  double wander = 0;
  for (int i = 0; i < kNR; i++) wander = std::fmax(wander, inertWander[i]);
  std::printf("%-26s %13.3f%%  <- the SAME-RATE control: two disjoint %.0f s halves\n",
              "inertia R wander", 100 * wander, (kInertTo - kInertFrom) / 2);

  std::printf("\n-- controls ------------------------------------------------------\n");
  char d[240];
  // A drift detector that cannot read ~zero is broken; one that cannot read
  // large has never been shown able to fail.
  std::snprintf(d, sizeof(d), "envelope attack drift %.3f%% must stay under 0.3%%", 100 * worstDrift(atk));
  check(worstDrift(atk) < 0.003, "CONTROL must-read-zero: a seconds-expressed quantity", d);
  std::snprintf(d, sizeof(d), "1000-samples-as-seconds drifts %.3f%% (must exceed 40%%)", 100 * worstDrift(sampleQty));
  check(worstDrift(sampleQty) > 0.40, "CONTROL must-read-large: a per-SAMPLE quantity", d);

  // The lock probe's own control: the R trace must actually HAVE an interior
  // peak. A peak pinned at the first sample means the probe watched a gesture
  // that never happened, and every drift it then reports is 0.000 % for the
  // wrong reason (this is exactly what `scatter 0` produced — see the comment
  // on lockPeakSeconds).
  bool interior = true;
  for (int i = 0; i < kNR; i++) if (lock30[i] < 0.005) interior = false;
  std::snprintf(d, sizeof(d), "earliest peak %.5f s across the four rates (must exceed 0.005)",
                *std::min_element(lock30, lock30 + kNR));
  check(interior, "CONTROL the onset-lock probe sees an interior R peak at all", d);

  // The inertia probe's own control, and the reason its bar is loose: the
  // quantity wanders at a FIXED rate. This does not fail the run — it BOUNDS
  // what the cross-rate number is allowed to mean, and it prints either way.
  std::snprintf(d, sizeof(d), "same-rate wander %.3f%% vs cross-rate spread %.3f%% — the bar (%.1f%%) must clear the wander",
                100 * wander, 100 * spread(inertFull), 18.04);
  check(wander < 0.1804 / 3.0, "CONTROL inertia bar stands at least 3x clear of its own wander", d);

  /* The engine's seconds constant is a LITERAL (std::log is not constexpr in
     C++20), so it is pinned here: the derivation is recomputed and compared,
     and the ROUND TRIP is asserted to MISS 0.08 — which is what makes the
     44.1 kHz special case in SwarmCore's constructor load-bearing rather than
     decorative. If a future edit ever makes the round trip exact, this control
     fails and says so, and the branch can go. */
  {
    const double derived = -((double)hypersaw::kTick / 44100.0) / std::log(0.92);
    const double err = std::fabs(hypersaw::kKsmTauSeconds - derived);
    std::snprintf(d, sizeof(d), "literal %.17g vs derived %.17g (|d| = %.3g, 4 ULP = %.3g)",
                  hypersaw::kKsmTauSeconds, derived, err, 4 * kUlp);
    check(err <= 4 * kUlp, "CONTROL kKsmTauSeconds is the seconds form of the reference's 0.08", d);
    const double trip = 1 - std::exp(-((double)hypersaw::kTick / 44100.0) / hypersaw::kKsmTauSeconds);
    std::snprintf(d, sizeof(d), "round trip is %.17g, %d ULP from 0.08 — hence the 44.1 kHz branch",
                  trip, (int)std::lround((trip - 0.08) / kUlp08));
    check(trip != 0.08, "CONTROL the 44.1 kHz special case is load-bearing (round trip misses 0.08)", d);
  }

  // The closed-form pole magnitude against the recurrence it describes, and
  // against a coefficient that is deliberately wrong.
  {
    const double a = poleCoef(44100.0);
    const double closed = onePoleDb(a, 10000.0, 44100.0), sim = onePoleSimDb(a, 10000.0, 44100.0);
    std::snprintf(d, sizeof(d), "closed form %.4f dB vs simulated recurrence %.4f dB", closed, sim);
    check(std::fabs(closed - sim) < 0.02, "CONTROL pole model matches the arithmetic it models", d);
    const double wrong = onePoleDb(a * 0.5, 10000.0, 44100.0);
    std::snprintf(d, sizeof(d), "half the coefficient reads %.4f dB, %.3f dB away", wrong, std::fabs(wrong - closed));
    check(std::fabs(wrong - closed) > 0.5, "CONTROL pole model is sensitive to the coefficient", d);
  }

  std::printf("\n-- drift bars ----------------------------------------------------\n");
  /* EVERY BAR IS THE AUDIT'S RECORDED DRIFT PLUS 20 %, AND THE 20 % IS NOT A
     ROUND NUMBER PICKED BY HOPE. These renders read no clock and draw from
     seeded streams only, so run to run they are bit-identical; the margin
     exists solely to absorb the difference between THIS probe and the audit's
     scratch probe. That difference is measurable: on the onset-lock row the
     two disagree by 3.2 % relative (5.739 % here against 5.56 % recorded, from
     a 0.2 ms grid with parabolic refinement against a 0.5 ms grid), and on the
     K-step row by 0.04 %. 20 % is roughly six times the largest observed
     probe-to-probe disagreement, and it is NOT headroom for the engine: a
     20 % growth in any of these is a change in the engine and this check is
     built to see it. */
  /* B150 moved this one. Before: 54.876 % (bar 0.659 = audit 54.9 % + 20 %).
     After the seconds-expressed coupling smoother: 1.252 %, and the residual
     is NOT a rate law — 44.1 k 0.00992 s, 48 k 0.01001, 88.2 k 0.01005, 96 k
     0.00992 is non-monotonic, i.e. the 0.2 ms probe grid and the swarm's own
     sigma, not the smoother. Bar = 1.252 % + the file's standing 20 %. */
  const double kBarKStep = 0.0150;    // B150: 1.252 % + 20 % (was 0.659)
  /* B150 moved this one, and NO ENGINE LINE WAS TOUCHED FOR IT. The onset
     lock's own decay was already in seconds (`s.Kenv *= exp(-dt/dissolve)`,
     swarm_core.h, dt = kTick/sr); the snap's rate dependence was entirely the
     coupling smoother it feeds, so the B150/1 commit closed it. Before /
     after, all three dissolve settings:
        dissolve 0.02   4.415 % -> 0.197 %
        dissolve 0.05   5.400 % -> 0.454 %
        dissolve 0.30   5.739 % -> 0.536 %   <- the gated row, worst of the three
     Bar = 0.536 % + the file's standing 20 %. */
  const double kBarLock  = 0.0064;    // B150: 0.536 % + 20 % (was 0.0667)
  const double kBarInert = 0.1804;    // audit 15.03 % + 20 % (see inertiaR's header)
  const double kBarPole  = 0.480;     // audit 0.40 dB + 20 %

  std::snprintf(d, sizeof(d), "%.3f%% today, audit 54.9%%, bar %.1f%%", 100 * worstDrift(kstep), 100 * kBarKStep);
  check(worstDrift(kstep) <= kBarKStep, "K-step settling drift has not grown", d);
  std::snprintf(d, sizeof(d), "%.3f%% today, audit 5.56%%, bar %.2f%%", 100 * worstDrift(lock30), 100 * kBarLock);
  check(worstDrift(lock30) <= kBarLock, "onset-lock peak drift has not grown (dissolve 0.30)", d);
  std::snprintf(d, sizeof(d), "%.3f%% today, audit 15.03%%, bar %.2f%% (wander %.2f%%)",
                100 * spread(inertFull), 100 * kBarInert, 100 * wander);
  check(spread(inertFull) <= kBarInert, "inertia steady-state R spread has not grown", d);
  std::snprintf(d, sizeof(d), "%.3f dB today, audit 0.40 dB, bar %.2f dB", poleWorst, kBarPole);
  check(poleWorst <= kBarPole, "output-pole 10 kHz response drift has not grown", d);

  std::printf("\nsr_check: %s (%d failures)\n", g_failures ? "RED" : "GREEN", g_failures);
  return g_failures ? 1 : 0;
}
