/* denormal_check — the silent tail after a held note, in the OUTPUT and in the
 * STATE.
 *
 * B147 layer 2. This check exists under a caveat, and the caveat comes first,
 * quoted verbatim from the B147 layer-1 audit
 * (docs/audits/2026-09-18-saw-engine-audit.md §1.5 / §3.7):
 *
 *   "FTZ/DAZ active in this process: no
 *    control: subnormal-chain penalty on this CPU: x1.01 — this CPU cannot show a stall
 *    worst tail/held cost ratio 1.15 (threshold 2.0): OK; subnormal output samples: 0"
 *
 *   "The honest residue: the control says an M3 *cannot* show the stall, so
 *    this is **evidence about the metric, not about x86**. ACCEPTANCE L0-6's
 *    min-spec names a '4-core 2018-class Intel ultrabook' and 'Windows x64
 *    AVX2', where the penalty is real. **Gap, not a defect.** Note also that
 *    state which decays toward zero and is never flushed exists (`s.vlp[]`,
 *    `s.mom[]`, `apZ` at `:1837`) — the voice cull (`:862-879`) zeroes
 *    `env/lpL/lpR` but not `vlp`, and `apZ` runs unconditionally whenever
 *    `width>1 && superMode!=0` with a silent input."
 *
 * SO THIS CHECK IS ABOUT THE STATES, NOT THE TIMING. `robustness_matrix`
 * already owns the timing half and already declares that its control cannot
 * fire on this CPU; repeating a measurement whose own control says it cannot
 * fire would be theatre. What is NOT covered anywhere is the second half of
 * the audit's note: the decaying state itself. A subnormal sitting in `vlp[]`
 * or `mom[]` costs nothing on an M3 and costs a scheduler deadline on the
 * min-spec x86 in ACCEPTANCE L0-6, and no oracle looks at it.
 *
 * WHAT IS COVERED, and how it is read. Every quantity below is read through
 * the core's own public accessor `SwarmCore::voiceAt(i)`, which returns a
 * `const Voice &` — the whole per-voice state struct is public
 * (swarm_core.h:324), so no friend, no debug export and no core edit is
 * needed:
 *
 *   phase[] driftS[] couple[] vf[] eff[] mom[]      the oscillator state
 *   env Kenv KsmS KsmP KsmD R RN psi sigma RA RB RQ the control-tick scalars
 *   lpL lpR lpc                                     the per-note output pole
 *   vlp[] vlpc[] hg[] rnd[]                         tone tilt / hi-tame / round
 *   driftPh[] driftHoldT[] vfSm[] fRun[] onsD[]     drift and glide state
 *   itdRing[][] osZL[] osZR[]                       the ADR-074 / ADR-075 rings
 *
 * WHAT IS NOT COVERED, named rather than implied (L0036 — a deliberate
 * absence needs to be pinned or it becomes an accidental one): `apZ`
 * (swarm_core.h:1837, the ADR-074 mode-D allpass state) and `hb[]`
 * (swarm_core.h:~1855, the ADR-075 halfband history) are PRIVATE members of
 * SwarmCore with no accessor and no debug export. `apZ` is precisely the state
 * the audit flagged as running unconditionally into silence, and this file
 * cannot see it. It is reached only indirectly: the patch below engages it
 * (width 1.3, superMode 3), so a subnormal there would have to survive the
 * multiply-add into the output to be counted — which is a weaker claim than
 * the others, and is stated as one. Closing it needs an accessor in `src/`,
 * which is out of this brief's scope.
 *
 * WHAT IT FOUND, first run, 2026-09-18 (so a reader is not surprised by a RED):
 * the ADR-074 mode-D allpass is the one source. Over a 30 s silent tail,
 * baseline (width 0.8, superMode 0) and ITD-only (width 1.3, superMode 1) both
 * read EXACTLY 0 subnormal output samples; width 1.3 with superMode 3 reads
 * 334, smallest magnitude 1.401e-45 (FLT_TRUE_MIN). That is the audit's own
 * prediction landing — `apZ` decaying into the subnormal range with a silent
 * input — and it is the first measurement of it. Every readable STATE reads 0
 * in all three patches, so the leak is in the one state this file cannot see.
 * Fixing it is a `src/` change and needs its own ruling; this check reports it
 * RED because it is a real violation of the property the check states, not
 * because anything regressed.
 *
 * CONTROLS (L0032, both halves — a counter that has only ever returned 0 has
 * never been shown able to return anything else): the same counter is run over
 * a buffer with planted subnormals (must read exactly the number planted) and
 * over a buffer of ordinary values (must read 0). And the process's FTZ/DAZ
 * state is printed, because a flush-to-zero process would make every count
 * below a 0 for the wrong reason — the detector-shares-the-assumption trap in
 * its purest form.
 *
 * STANDALONE AND UNWIRED — wiring a gate is the human's decision (charter).
 */
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <vector>

#include "../src/swarm_core.h"

namespace
{

int g_failures = 0;
void check(bool ok, const char *what, const char *detail)
{
  std::printf("%-6s %s  (%s)\n", ok ? "PASS" : "FAIL", what, detail);
  if (!ok) g_failures++;
}

/* std::fpclassify is the portable spelling; a bit-pattern test would also have
   to know the type's layout and would quietly disagree for float vs double.
 *
 * THE TYPE IS THE MEASUREMENT, and the first version of this file got it
 * wrong. Classifying a `float` after widening it to `double` reports
 * FP_NORMAL for every subnormal float there is — 1.4e-45 is FLT_TRUE_MIN, a
 * subnormal float, and an entirely ordinary double. The first run of this
 * check printed "0 subnormal output samples" and, in the same line, "smallest
 * non-zero magnitude 1.401e-45", which is that contradiction in one sentence.
 * The output buffer and `itdRing` are float; everything else is double; each
 * is classified in its own type. (This is L0032 arriving from the inside: the
 * detector shared the widening assumption with nothing at all, and still
 * confirmed the answer it was written expecting.) */
template <typename T> bool isSub(T v) { return std::fpclassify(v) == FP_SUBNORMAL; }

struct Count
{
  long n = 0;
  double smallest = std::numeric_limits<double>::infinity();
  const char *firstField = nullptr;
  void bump(bool sub, double mag, const char *field)
  {
    if (sub) { n++; if (!firstField) firstField = field; }
    if (mag > 0 && mag < smallest) smallest = mag;
  }
  void add(double v, const char *field) { bump(isSub(v), std::fabs(v), field); }
  void addF(float v, const char *field) { bump(isSub(v), std::fabs((double)v), field); }
  void addArr(const double *v, int n_, const char *field) { for (int i = 0; i < n_; i++) add(v[i], field); }
};

void scanVoice(const hypersaw::SwarmCore::Voice &s, Count &c)
{
  const int N = hypersaw::kMaxV;
  c.addArr(s.phase, N, "phase");   c.addArr(s.driftS, N, "driftS");
  c.addArr(s.couple, N, "couple"); c.addArr(s.vf, N, "vf");
  c.addArr(s.eff, N, "eff");       c.addArr(s.mom, N, "mom");
  c.addArr(s.vlp, N, "vlp");       c.addArr(s.vlpc, N, "vlpc");
  c.addArr(s.hg, N, "hg");         c.addArr(s.rnd, N, "rnd");
  c.addArr(s.driftPh, N, "driftPh");       c.addArr(s.driftHoldT, N, "driftHoldT");
  c.addArr(s.vfSm, N, "vfSm");     c.addArr(s.fRun, N, "fRun");
  c.addArr(s.onsD, N, "onsD");
  c.add(s.env, "env");   c.add(s.Kenv, "Kenv");
  c.add(s.KsmS, "KsmS"); c.add(s.KsmP, "KsmP"); c.add(s.KsmD, "KsmD");
  c.add(s.R, "R");       c.add(s.RN, "RN");     c.add(s.psi, "psi");
  c.add(s.sigma, "sigma");
  c.add(s.RA, "RA");     c.add(s.RB, "RB");     c.add(s.RQ, "RQ");
  c.add(s.lpL, "lpL");   c.add(s.lpR, "lpR");   c.add(s.lpc, "lpc");
  c.add(s.f0, "f0");     c.add(s.f0cur, "f0cur");
  c.add(s.pressSm, "pressSm");
  for (int i = 0; i < N; i++)
    for (int j = 0; j < 256; j++) c.addF(s.itdRing[i][j], "itdRing");
  c.addArr(s.osZL, 64, "osZL"); c.addArr(s.osZR, 64, "osZR");
}

}  // namespace

int main()
{
  std::printf("denormal_check — the silent tail, in the OUTPUT and in the STATE (B147 layer 2)\n");
  std::printf("CAVEAT (audit 2026-09-18 §1.5, verbatim): \"the control says an M3 *cannot* show\n");
  std::printf("the stall, so this is evidence about the metric, not about x86\". This file\n");
  std::printf("therefore measures STATES, not timing; robustness_matrix owns the timing half.\n\n");

  // FTZ/DAZ: if the process flushes subnormals to zero, every count below is a
  // 0 for the wrong reason.
  {
    volatile double tiny = 1e-320;   // subnormal by construction
    volatile double half = tiny * 0.5;
    const bool ftz = (half == 0.0) || !isSub((double)tiny);
    std::printf("FTZ/DAZ active in this process: %s\n", ftz ? "YES — every count below is meaningless" : "no");
    if (ftz) { std::printf("FAIL   the measurement cannot run under flush-to-zero\n"); return 1; }
  }

  std::printf("\n-- controls ------------------------------------------------------\n");
  {
    char d[200];
    std::vector<double> clean(4096, 0.0);
    for (size_t i = 0; i < clean.size(); i++) clean[i] = std::sin(0.01 * (double)i);
    Count c0; for (double v : clean) c0.add(v, "control");
    std::snprintf(d, sizeof(d), "4096 ordinary doubles read %ld subnormals", c0.n);
    check(c0.n == 0, "CONTROL must-read-zero: ordinary values", d);

    std::vector<double> dirty = clean;
    for (int i = 0; i < 7; i++) dirty[i * 500] = 1e-320;
    Count c1; for (double v : dirty) c1.add(v, "control");
    std::snprintf(d, sizeof(d), "7 planted subnormal doubles read %ld", c1.n);
    check(c1.n == 7, "CONTROL must-read-large: planted subnormal doubles", d);

    // The FLOAT half of the same control, and the one that matters: the output
    // buffer is float, and a subnormal float widened to double is a perfectly
    // normal double. Without this line the float path has no calibration at
    // all — which is exactly how the first version of this file read "0".
    std::vector<float> dirtyF(4096, 0.0f);
    for (size_t i = 0; i < dirtyF.size(); i++) dirtyF[i] = (float)std::sin(0.01 * (double)i);
    Count c2; for (float v : dirtyF) c2.addF(v, "controlF");
    std::snprintf(d, sizeof(d), "4096 ordinary floats read %ld subnormals", c2.n);
    check(c2.n == 0, "CONTROL must-read-zero: ordinary floats", d);
    for (int i = 0; i < 5; i++) dirtyF[i * 700] = 1.401298464e-45f;   // FLT_TRUE_MIN
    Count c3; for (float v : dirtyF) c3.addF(v, "controlF");
    std::snprintf(d, sizeof(d), "5 planted subnormal floats read %ld", c3.n);
    check(c3.n == 5, "CONTROL must-read-large: planted subnormal FLOATS", d);
  }

  std::printf("\n-- the silent tail ------------------------------------------------\n");
  /* THREE PATCHES, so a non-zero count is LOCALISED rather than merely
     reported. The audit's §1.5 note named `apZ` (the ADR-074 mode-D allpass,
     "runs unconditionally whenever width>1 && superMode!=0 with a silent
     input") as the state most likely to decay into the subnormal range, and it
     is the one state this file cannot read directly — so the patch table is
     built to answer the question through the output instead: the same tail
     with the super-width path off, on with the ITD ring only, and on with the
     allpass. Whichever rows read non-zero is the answer. */
  struct TailPatch { const char *name; double width; double superMode; };
  const TailPatch tails[] = {
      {"baseline (width 0.8, superMode 0 — the shipped default)", 0.8, 0},
      {"super-width, ITD only (width 1.3, superMode 1)",          1.3, 1},
      {"super-width + allpass (width 1.3, superMode 3)",          1.3, 3},
  };

  long sumOut = 0, sumState = 0;
  for (const auto &tp : tails)
  {
    hypersaw::SwarmCore c(44100.0);
    c.setParam("seed", 1234);
    c.setParam("n", 7);
    c.setParam("detune", 0.28);
    c.setParam("K", 0.6);
    c.setParam("inertia", 0.7);    // mom[] — a momentum integrator decaying to rest
    c.setParam("toneTilt", 0.5);   // vlp[] — the per-voice one-pole the cull does not clear
    c.setParam("hiTame", 0.5);
    c.setParam("driftDepth", 40);
    c.setParam("width", tp.width);
    c.setParam("superMode", tp.superMode);
    c.setParam("release", 0.16);

    const int blk = 128;
    std::vector<float> L(blk), R(blk);
    c.noteOn(60, 261.6255653005986);
    for (int i = 0; i < (int)(44100 * 1.0) / blk; i++) c.render(L.data(), R.data(), blk);
    c.noteOff(60);

    // 30 s of tail. The envelope is a one-pole that asymptotes rather than
    // arriving, so the interesting window is AFTER the voice cull has retired
    // the slot and whatever it did not clear is left alone.
    Count out;
    const long tailN = (long)(44100 * 30.0);
    for (long done = 0; done < tailN; done += blk)
    {
      c.render(L.data(), R.data(), blk);
      for (int i = 0; i < blk; i++) { out.addF(L[i], "outL"); out.addF(R[i], "outR"); }
    }

    Count st;
    for (int i = 0; i < hypersaw::kPoly; i++) scanVoice(c.voiceAt(i), st);
    sumOut += out.n;
    sumState += st.n;

    std::printf("%-56s out %5ld/%ld  state %4ld%s%s  smallest out %.3e\n",
                tp.name, out.n, tailN * 2, st.n,
                st.firstField ? " in " : "", st.firstField ? st.firstField : "",
                out.smallest);
  }

  char d[240];
  std::snprintf(d, sizeof(d), "%ld subnormal OUTPUT samples summed over the three patches", sumOut);
  check(sumOut == 0, "30 s of silent tail contains no subnormal OUTPUT sample", d);
  std::snprintf(d, sizeof(d), "%ld subnormal STATE values summed over the three patches", sumState);
  check(sumState == 0, "no smoother or integrator STATE holds a subnormal", d);

  std::printf("\nNOT COVERED: apZ and hb[] are private to SwarmCore with no accessor —\n");
  std::printf("reached here only through the output (the width 1.3 rows engage them).\n");
  std::printf("denormal_check: %s (%d failures)\n", g_failures ? "RED" : "GREEN", g_failures);
  return g_failures ? 1 : 0;
}
