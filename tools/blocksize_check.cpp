/* blocksize_check — the engine must not care how a buffer is subdivided, at
 * ANY sample rate.
 *
 * B147 layer 2. `subdiv_check` (a gate, and NOT touched by this file) already
 * proves this at 44.1 kHz for chunks {N, 2048, 1024, 512, 333, 256, 127, 64, 7, 1}.
 * The B147 layer-1 audit (docs/audits/2026-09-18-saw-engine-audit.md §3.3)
 * named the two holes it leaves:
 *
 *   1. every chunk case runs at 44.1 kHz only — the rate x chunk cross product
 *      is unmeasured, and the gravity grid is a fixed TIME (256 samples at
 *      44.1 k, 558 at 96 k), so the alignment between chunk and grid is a
 *      DIFFERENT alignment at every rate;
 *   2. chunk sizes below 64 are untested, and 1 is the degenerate case a
 *      sample-accurate host or a per-sample cyclic FX topology (ADR-175) will
 *      actually produce.
 *
 * So: {1, 7, 64, 256, 333, 1024} at 44.1 AND 48 kHz. 7 and 333 are coprime
 * with the grid at both rates; 1 is the degenerate case; 256 is exactly the
 * 44.1 k grid and exactly NOT the 48 k one.
 *
 * THRESHOLD: 0.0 exactly, for every case, at both rates. Not a tolerance — the
 * audit measured 0.0 exactly and a subdivision that changes a sample by one ULP
 * is a per-call integrator that was not there yesterday.
 *
 * PAN MOTION IS NOW GATED TOO (B151). It was a deliberate per-render-call
 * integrator (ADR-064) and reported-not-gated here while that stood; ADR-177 §1
 * ruled it onto the same fixed grid as a PAIRED edit to reference/swarmsaw.html
 * and src/swarm_core.h, so the nine pan goldens re-baselined, parity held at
 * 156/156, and the exclusion is retired rather than relaxed. All eight rows
 * read 0.0 exactly.
 *
 * CONTROL (L0032): the max-difference detector is shown BOTH ways before its
 * zeros mean anything — a planted case (the same patch rendered from a
 * different seed) must read large, and the reference compared with itself must
 * read exactly 0. A "0.0" from a detector that has never produced a non-zero
 * is not evidence.
 *
 * STANDALONE AND UNWIRED — wiring a gate is the human's decision (charter).
 */
#include <cmath>
#include <cstdio>
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

struct Patch { double grav, pan; int seed; double scatter; };

std::vector<float> run(double sr, int chunk, const Patch &pt, int total)
{
  hypersaw::SwarmCore c(sr);
  c.setParam("seed", pt.seed);
  c.setParam("n", 5);
  c.setParam("grav", pt.grav);
  c.setParam("basin", 50);
  c.setParam("scatter", pt.scatter);
  c.setParam("panMotion", pt.pan);
  c.noteOn(60, 261.6255653005986);
  c.noteOn(64, 329.6275569128699);
  c.noteOn(67, 391.99543598174927);
  std::vector<float> out(total);
  // Scratch sized to `total`: the reference case renders the whole buffer in
  // ONE call (subdiv_check's header records the SIGABRT a fixed 4096 scratch
  // bought). A probe that crashes is honest; one that overflows quietly would
  // have "passed".
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

double maxdiff(const std::vector<float> &a, const std::vector<float> &b)
{
  double m = 0;
  for (size_t i = 0; i < a.size() && i < b.size(); i++) m = std::fmax(m, std::fabs((double)a[i] - (double)b[i]));
  return m;
}

}  // namespace

int main()
{
  std::printf("blocksize_check — subdivision independence across the rate x chunk grid\n");
  std::printf("(B147 layer 2; subdiv_check is the 44.1 kHz gate and is untouched)\n\n");

  const double rates[] = {44100.0, 48000.0};
  const int chunks[] = {1, 7, 64, 256, 333, 1024};
  struct Case { const char *name; Patch pt; };
  const Case cases[] = {
      {"inert (no grid-driven integrator engaged)",        {0.0, 0.0, 1234, 0.0}},
      {"gravity engaged (ADR-086)",                        {0.7, 0.0, 1234, 0.0}},
      {"pan motion engaged (ADR-064, gridded by ADR-177 §1)", {0.0, 0.6, 1234, 0.0}},
      {"both engaged",                                     {0.7, 0.6, 1234, 0.0}},
  };

  for (const auto &cs : cases)
    for (double sr : rates)
    {
      const int N = (int)std::lround(sr);   // one second of audio at either rate
      const std::vector<float> ref = run(sr, N, cs.pt, N);
      double worst = 0; int worstChunk = 0;
      for (int ch : chunks)
      {
        const double d = maxdiff(ref, run(sr, ch, cs.pt, N));
        if (d > worst) { worst = d; worstChunk = ch; }
      }
      char detail[200];
      std::snprintf(detail, sizeof(detail), "worst %.10g at chunk %d, %.1f kHz", worst, worstChunk, sr / 1000);
      check(worst == 0.0, cs.name, detail);
    }

  std::printf("\n-- controls ------------------------------------------------------\n");
  {
    const double sr = 44100.0;
    const int N = (int)sr;
    const std::vector<float> ref = run(sr, N, {0.0, 0.0, 1234, 0.0}, N);
    const double self = maxdiff(ref, run(sr, N, {0.0, 0.0, 1234, 0.0}, N));
    char d[220];
    std::snprintf(d, sizeof(d), "same patch, same subdivision: %.10g", self);
    check(self == 0.0, "CONTROL must-read-zero: the render is reproducible at all", d);

    /* THE PLANT THAT DID NOT FIRE, kept rather than quietly replaced (L0033 —
       a plant that does not fire has measured the assertion's COVERAGE
       BOUNDARY, and silently retrying until one fires is how that boundary
       gets lost). Changing `seed` on THIS patch moves the output by exactly
       0.0, because at dist 1 with panScatter 0 and scatter 0 the seeded stream
       `grng` is never drawn. Characterised on 2026-09-18 rather than assumed:
       seed 1234 vs 999999 moves the render by 0.0 at dist 0/1/4, by 0.798 at
       dist 2, 0.682 at dist 3, 0.062 with panScatter 0.5, and 0.738 with
       scatter 1. So the reach of `seed` is conditional by construction, and
       the plant below engages `scatter` to put it in reach. The audit's gap #3
       ("no gate asserts that changing seed changes the output") is a DIFFERENT
       and still-open claim about the ensemble-timing stream; this line is not
       evidence for or against it. */
    const double inert = maxdiff(ref, run(sr, N, {0.0, 0.0, 999999, 0.0}, N));
    std::printf("RECORD seed alone moves this patch by %.6g — inert by construction, see the\n"
                "       comment above; the firing plant below engages `scatter` to reach the stream.\n", inert);

    // The plant that DOES fire: the same seed change with the seeded stream in
    // reach. Asserting a plant fires proves the SOURCE is live, not that a
    // stale object agreed with itself (L0032, three occurrences in this tree).
    const std::vector<float> scatRef = run(sr, N, {0.0, 0.0, 1234, 1.0}, N);
    const double planted = maxdiff(scatRef, run(sr, N, {0.0, 0.0, 999999, 1.0}, N));
    std::snprintf(d, sizeof(d), "seed 1234 vs 999999 with scatter 1: %.6g (must exceed 0.01)", planted);
    check(planted > 0.01, "CONTROL must-read-large: a planted divergence", d);
  }

  std::printf("\nblocksize_check: %s (%d failures; every case gated, pan motion included since B151)\n",
              g_failures ? "RED" : "GREEN", g_failures);
  return g_failures ? 1 : 0;
}
