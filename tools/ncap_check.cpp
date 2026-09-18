/* ncap_check — B148: the swarm size can never index past kMaxV.
 *
 * WHY. `SwarmCore::p.n` is a public double and `kMaxV` (32) is the extent of
 * x[], panL[], panR[], panBase[], itdSamp[] and every per-oscillator Voice
 * buffer. Before B148 nothing in the core capped it: the audit
 * (docs/audits/2026-09-18-saw-engine-audit.md §1.2) measured `setParam("n",33)`
 * writing one double past x[32] — straight into panL[0] — and rendering the
 * resulting corruption SILENTLY, and n >= 40 taking SIGSEGV inside
 * finishRebuild()'s stack arrays `perm[kMaxV]` / `pos[kMaxV]`. The shell's
 * param row (1..32, src/hypersaw_clap.cpp) was the only cap in the system and
 * every tool in tools/ drives the core directly, under it.
 *
 * THE ASSERTION is equality, not absence-of-crash: an over-cap `n` must render
 * EXACTLY what n = 32 renders (and an under-cap `n` exactly what n = 1
 * renders), because that is what "clamped" means and a crash-free corrupted
 * render is the failure mode that actually shipped.
 *
 * CALIBRATION (L0032 — a detector that can only ever say "equal" proves
 * nothing). T6/T7 are must-DIFFER controls: n = 31 must not hash-equal n = 32,
 * and n = 2 must not hash-equal n = 1. If the render or the hash were inert,
 * those fail.
 *
 * SECOND WITNESS is a sanitizer, and it is UBSan, not ASan. Run against a
 * scratch copy of swarm_core.h with both clamps planted out, this file under
 * `-fsanitize=undefined -fno-sanitize-recover=all` aborts with
 *   swarm_core.h: runtime error: index 32 out of bounds for type 'double[32]'
 * at `x[i] = xv` in rebuild(); against the shipped header it exits 0. Two
 * reasons ASan is NOT the instrument: (a) the x[32] -> panL[0] write is
 * INTRA-object, which ASan does not instrument at all, while UBSan's
 * array-bounds sees the static extent; (b) the ASan runtime does not start on
 * this machine (Apple clang 16.0.0 / Darwin 25.6 — a `int a[4]; a[5]` smoke
 * test dies in sanitizer_malloc_mac.inc before main), so an absent ASan report
 * here would be evidence about the sanitizer, not about the code. Transcript
 * in traces/2026-09-18-b148-ncap-bitident.md.
 *
 * KNOWN BOUNDARY: this pins the CORE. The shell's own `(int)core.p.n` reads
 * (src/hypersaw_clap.cpp hypersaw_debug_phases) are not covered here.
 *
 * Standalone and NOT in ./verify — wiring a gate is the human's decision
 * (charter), proposed in the PR that adds this. Exit 1 on failure.
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "../src/swarm_core.h"

using hypersaw::SwarmCore;

namespace
{
constexpr double kSR = 44100.0;
constexpr int kBlock = 512;
constexpr int kSeconds = 2;
constexpr int kMidi = 57;   // A3, as every other core-direct check uses

double mtof(int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }

/* A patch where the pan arrays are load-bearing, so the x[32] -> panL[0]
   overwrite the audit measured actually reaches the output. */
void patch(SwarmCore &c)
{
  c.setParam("dist", 4);
  c.setParam("seed", 1234);
  c.setParam("detune", 0.35);
  c.setParam("K", 0.3);
  c.setParam("width", 1.4);
  c.setParam("panScatter", 0.4);
  c.setParam("driftDepth", 0.2);
}

uint64_t fnv(const void *p, size_t n, uint64_t h)
{
  const uint8_t *b = (const uint8_t *)p;
  for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; }
  return h;
}

/* `direct` writes p.n straight into the public Params BEFORE the patch, so
   every rebuild() the patch triggers runs with the poisoned size — the path
   the read-site clamp (voiceCount()) exists for and the setter clamp cannot
   see, since `p` is public. */
uint64_t renderHash(double nVal, bool direct)
{
  SwarmCore c(kSR);
  if (direct) c.p.n = nVal;
  patch(c);
  if (!direct) c.setParam("n", nVal);
  c.noteOn(kMidi, mtof(kMidi));
  std::vector<float> L(kBlock), R(kBlock), il(kBlock * 2);
  uint64_t h = 1469598103934665603ull;
  for (long off = 0; off < (long)(kSeconds * kSR); off += kBlock)
  {
    c.render(L.data(), R.data(), kBlock);
    for (int i = 0; i < kBlock; i++) { il[i * 2] = L[i]; il[i * 2 + 1] = R[i]; }
    h = fnv(il.data(), (size_t)kBlock * 2 * sizeof(float), h);
  }
  return h;
}

int fails = 0;
void expect(bool ok, const char *what) { std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }
}  // namespace

int main()
{
  const uint64_t h32 = renderHash(32, false);
  const uint64_t h31 = renderHash(31, false);
  const uint64_t h1 = renderHash(1, false);
  const uint64_t h2 = renderHash(2, false);

  std::printf("ncap_check — n clamped to [1, %d] at the core (B148)\n", hypersaw::kMaxV);
  std::printf("  reference hashes: n=32 %016llx  n=31 %016llx  n=1 %016llx  n=2 %016llx\n",
              (unsigned long long)h32, (unsigned long long)h31, (unsigned long long)h1,
              (unsigned long long)h2);

  expect(renderHash(33, false) == h32, "T1 setParam(n,33)  renders exactly n=32");
  expect(renderHash(200, false) == h32, "T2 setParam(n,200) renders exactly n=32");
  expect(renderHash(0, false) == h1, "T3 setParam(n,0)   renders exactly n=1");
  expect(renderHash(200, true) == h32, "T4 p.n = 200 written DIRECTLY renders exactly n=32");

  {
    SwarmCore c(kSR);
    c.setParam("n", 200);
    const double hi = c.getParam("n");
    c.setParam("n", -5);
    const double lo = c.getParam("n");
    c.setParam("n", 7.5);
    const double frac = c.getParam("n");
    std::printf("  readback: n=200 -> %.4f   n=-5 -> %.4f   n=7.5 -> %.4f\n", hi, lo, frac);
    expect(hi == 32.0 && lo == 1.0, "T5 getParam('n') reports the clamped size, not the asked one");
    // PINNED REFUSAL (L0036): the setter clamps, it does not TRUNCATE. A legal
    // fractional n must survive byte-for-byte or every golden moves.
    expect(frac == 7.5, "T5b a legal fractional n (7.5) is passed through untouched");
  }

  expect(h31 != h32, "T6 CONTROL n=31 differs from n=32 (the equalities above are not vacuous)");
  expect(h2 != h1, "T7 CONTROL n=2 differs from n=1 (ditto at the low end)");

  std::printf("ncap_check: %s\n", fails ? "FAIL" : "PASS");
  return fails ? 1 : 0;
}
