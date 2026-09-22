/* subdiv_check — the engine must not care how a buffer is SUBDIVIDED.
 *
 * A host may deliver 2048 frames or 33; a plugin may split them further (the
 * multi-oscillator mix renders oscillator 0 whole and the rest in kMixChunk
 * pieces). None of that is a musical choice, so none of it may change a sample.
 *
 * The bug this exists for (ADR-086): gravity integrated once per render call
 * with dt = the block length. Explicit Euler on a nonlinear ODE, so one step of
 * dt and two of dt/2 disagreed — the same patch sounded different at different
 * host buffer sizes, and oscillator 0 drifted from oscillators 1..N.
 *
 * WHY THIS CANNOT BE A GOLDEN TEST. The golden generator renders a fixed buffer
 * and parity_check renders kBlock, so BOTH SIDES USE THE SAME SUBDIVISION and
 * parity agrees with itself. All 147 scenarios passed throughout. The property
 * is invisible to the oracle by construction; it needs its own check.
 *
 * THERE ARE NOW NO PER-RENDER-CALL INTEGRATORS IN THIS CORE. Gravity went onto
 * the fixed grid in ADR-086; pan motion (ADR-064) followed in ADR-177 §1 (B151),
 * as a PAIRED edit to reference/swarmsaw.html and src/swarm_core.h so the nine
 * pan goldens re-baselined and L0-1 parity held. Every case below is therefore
 * gated at 0.0 exactly — the exclusion this file used to carry is gone, and a
 * third integrator added without thought would fail here rather than be
 * tolerated in a printed KNOWN row.
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

static std::vector<float> run(int chunk, double grav, double panMotion, int total)
{
  hypersaw::SwarmCore c(44100.0);
  c.setParam("seed", 1234);
  c.setParam("n", 5);
  c.setParam("grav", grav);
  c.setParam("basin", 50);
  c.setParam("panMotion", panMotion);
  c.noteOn(60, 261.6255653005986);
  c.noteOn(64, 329.6275569128699);
  c.noteOn(67, 391.99543598174927);
  std::vector<float> out(total);
  // Sized to `total`, not a fixed 4096: the reference case renders the whole
  // buffer in ONE call, and a fixed scratch overflowed it (SIGABRT on the first
  // run). A probe that crashes is at least honest; one that overflows quietly
  // would have "passed".
  std::vector<float> bL(total), bR(total);
  int done = 0;
  while (done < total)
  {
    const int m = total - done < chunk ? total - done : chunk;
    c.render(bL.data(), bR.data(), m);
    for (int i = 0; i < m; i++) out[done + i] = bL[i];
    done += m;
  }
  return out;
}

/* CONTROL (L0032). "0.0 at every chunk" is a statement about the CORE only if
   the sweep below can still SEE a per-call integrator; a detector that has
   never produced a non-zero number is not evidence. So the pre-B151 pan
   integrator is re-created here in miniature — phase += rate * frames/sr,
   sampled once per render call and HELD across the block — and run through the
   identical chunk sweep, where it must read non-zero. Its grid-cadence twin
   must read exactly 0.0, which is what makes the miniature a faithful stand-in
   rather than an unrelated signal that happens to differ.
   Deliberately a standalone simulation and not a build flag in swarm_core.h: a
   plant compiled into the core would be shipped code whose only job is to be
   wrong, and the thing under test is the DETECTOR, not the core. */
static std::vector<float> holdTrace(int chunk, int total, bool perCall)
{
  const double sr = 44100.0;
  const double rate = 0.08;   // reference/swarmsaw.html's drift LFO, voice 0
  const int grid = 256;       // ADR-086 A1 at 44.1 kHz
  std::vector<float> out(total);
  double ph = 0.0, held = 0.0;
  int accum = 0, done = 0;
  while (done < total)
  {
    const int m = total - done < chunk ? total - done : chunk;
    if (perCall)
    {
      ph += rate * (double)m / sr;
      ph -= std::floor(ph);
      held = std::sin(6.283185307 * ph);
    }
    int off = 0;
    while (off < m)   // the shape of SwarmCore::render()
    {
      if (!perCall && accum == 0)
      {
        ph += rate * (double)grid / sr;
        ph -= std::floor(ph);
        held = std::sin(6.283185307 * ph);
      }
      const int room = grid - accum;
      const int seg = (m - off) < room ? (m - off) : room;
      for (int i = 0; i < seg; i++) out[done + off + i] = (float)held;
      accum += seg;
      off += seg;
      if (accum >= grid) accum = 0;
    }
    done += m;
  }
  return out;
}

static double maxdiff(const std::vector<float> &a, const std::vector<float> &b)
{
  double m = 0;
  for (size_t i = 0; i < a.size() && i < b.size(); i++) m = std::fmax(m, std::fabs(a[i] - b[i]));
  return m;
}

int main()
{
  const int N = 44100;
  // Subdivisions a host or the mix stage might plausibly produce, including
  // sizes that are NOT multiples of the gravity grid — an accumulator that only
  // works on aligned blocks is not an accumulator.
  // 1 and 7 added with B151: 1 is the degenerate case a sample-accurate host or
  // a per-sample cyclic FX topology (ADR-175) actually produces, and 7 is
  // coprime with the grid, so neither can be satisfied by grid alignment.
  const int chunks[] = {N, 2048, 1024, 512, 333, 256, 127, 64, 7, 1};
  /* Every case is GATED. ADR-086 ratified the fixed grid for gravity; ADR-177
     §1 ruled pan motion onto the same grid, paired with reference/swarmsaw.html
     so the nine pan goldens re-baselined and parity stayed 156/156. The
     exclusion that used to print "KNOWN — pan motion excluded pending a ruling"
     (worst 0.1915 at chunk 333) is therefore retired, not relaxed: the
     behaviour it declined to assert is now the behaviour the core has. */
  struct Case { const char *name; double grav; double pan; };
  const Case cases[] = {
      {"inert (no grid-driven integrator engaged)", 0.0, 0.0},
      {"gravity engaged (ADR-086)", 0.7, 0.0},
      {"pan motion engaged (ADR-064, gridded by ADR-177 §1)", 0.0, 0.6},
      {"both engaged", 0.7, 0.6},
  };
  for (const auto &cs : cases)
  {
    const std::vector<float> ref = run(N, cs.grav, cs.pan, N);   // one whole call
    double worst = 0;
    int worstChunk = 0;
    for (int ch : chunks)
    {
      const double d = maxdiff(ref, run(ch, cs.grav, cs.pan, N));
      if (d > worst) { worst = d; worstChunk = ch; }
    }
    char detail[160];
    std::snprintf(detail, sizeof(detail), "worst %.10g at chunk %d", worst, worstChunk);
    check(worst == 0.0, cs.name, detail);
  }

  std::printf("-- controls ------------------------------------------------------\n");
  {
    double worstCall = 0, worstGrid = 0;
    int callChunk = 0, gridChunk = 0;
    const std::vector<float> refCall = holdTrace(N, N, true);
    const std::vector<float> refGrid = holdTrace(N, N, false);
    for (int ch : chunks)
    {
      const double dc = maxdiff(refCall, holdTrace(ch, N, true));
      if (dc > worstCall) { worstCall = dc; callChunk = ch; }
      const double dg = maxdiff(refGrid, holdTrace(ch, N, false));
      if (dg > worstGrid) { worstGrid = dg; gridChunk = ch; }
    }
    char d[200];
    std::snprintf(d, sizeof(d), "the pre-B151 per-call hold, same sweep: %.10g at chunk %d (must exceed 0)",
                  worstCall, callChunk);
    check(worstCall > 0.0, "CONTROL must-read-nonzero: the sweep still sees a per-call integrator", d);
    std::snprintf(d, sizeof(d), "the same miniature on the grid cadence: %.10g at chunk %d",
                  worstGrid, gridChunk);
    check(worstGrid == 0.0, "CONTROL must-read-zero: the miniature is a faithful stand-in", d);
  }

  std::printf("subdiv_check: %s (%d failures; every case gated, no exclusions)\n",
              failures ? "RED" : "GREEN", failures);
  return failures ? 1 : 0;
}
