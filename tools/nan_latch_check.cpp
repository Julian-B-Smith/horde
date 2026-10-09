/*
 * nan_latch_check — the output guard LATCHES AND REPORTS (B448 B1, ADR-197 risk row 3).
 *
 * WIRED: ./verify fast, compiled and run by tools/nan_latch_check.py
 *
 * WHAT. Drives hypersaw::NonFiniteLatch (src/output_latch.h) — the class the
 * shell's process() ends with, over the same hypersaw::zeroNonFinite as before —
 * through planted buffers and asserts, per row:
 *   - NaN (quiet, signalling, negative-signed), +Inf and -Inf are each zeroed,
 *     COUNTED exactly, and LATCH — in the left channel, the right, and both;
 *   - a clean buffer counts 0 and does not latch, and its finite samples
 *     (including -0.0, a denormal, FLT_MAX) are bit-identical afterwards;
 *   - the counters accumulate across blocks; `blocks` counts blocks with a hit,
 *     `samples` counts the samples (3 hits in one block is blocks +1, samples +3);
 *   - the latch survives later clean blocks, and only reset() clears it (counters
 *     too), after which a new event latches again.
 *
 * MUST-FAIL CONTROLS. The same rows run against five faulty guards, each one
 * defect away from the real one, and each must FAIL at least one row:
 *   zero-without-count   today's (B446) behaviour: repairs, tells nobody
 *   count-without-latch  counts, but nothing stays set
 *   self-clearing latch  a clean block un-sets it
 *   right-unreported     the right channel is repaired but never counted
 *   reset-keeps-latch    reset() zeroes the counters but leaves the latch set
 * A row set a faulty guard passes proves nothing, so a control that passes is RED.
 *
 * NOT SHOWN HERE. That the plugin's process() calls the guard: nan_latch_check.py
 * greps for the call, and hostile_events_check (./verify full) drives the shell's
 * own guardOutput through the exported hooks.
 */
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include "../src/output_latch.h"

namespace
{
using hypersaw::zeroNonFinite;

const float kNaN = std::numeric_limits<float>::quiet_NaN();
const float kSNaN = std::numeric_limits<float>::signaling_NaN();
const float kInf = std::numeric_limits<float>::infinity();

float bitsToFloat(uint32_t b)
{
  float f;
  std::memcpy(&f, &b, sizeof f);
  return f;
}

/* ---- the faulty guards: same interface as the real one ------------------- */
struct ZeroWithoutCount   // B446 as shipped
{
  uint32_t guard(float *l, float *r, uint32_t n) { return zeroNonFinite(l, n) + zeroNonFinite(r, n); }
  uint64_t samples() const { return 0; }
  uint64_t blocks() const { return 0; }
  bool latched() const { return false; }
  void reset() {}
};
struct CountWithoutLatch
{
  uint64_t s = 0, b = 0;
  uint32_t guard(float *l, float *r, uint32_t n)
  {
    const uint32_t h = zeroNonFinite(l, n) + zeroNonFinite(r, n);
    if (h) { s += h; b++; }
    return h;
  }
  uint64_t samples() const { return s; }
  uint64_t blocks() const { return b; }
  bool latched() const { return false; }
  void reset() { s = b = 0; }
};
struct SelfClearingLatch
{
  uint64_t s = 0, b = 0;
  bool l_ = false;
  uint32_t guard(float *l, float *r, uint32_t n)
  {
    const uint32_t h = zeroNonFinite(l, n) + zeroNonFinite(r, n);
    if (h) { s += h; b++; }
    l_ = h != 0;   // set on an event, cleared by the next clean block
    return h;
  }
  uint64_t samples() const { return s; }
  uint64_t blocks() const { return b; }
  bool latched() const { return l_; }
  void reset() { s = b = 0; l_ = false; }
};
struct RightUnreported
{
  uint64_t s = 0, b = 0;
  bool l_ = false;
  uint32_t guard(float *l, float *r, uint32_t n)
  {
    const uint32_t hl = zeroNonFinite(l, n);
    zeroNonFinite(r, n);   // repaired, never counted
    if (hl) { s += hl; b++; l_ = true; }
    return hl;
  }
  uint64_t samples() const { return s; }
  uint64_t blocks() const { return b; }
  bool latched() const { return l_; }
  void reset() { s = b = 0; l_ = false; }
};
struct ResetKeepsLatch
{
  uint64_t s = 0, b = 0;
  bool l_ = false;
  uint32_t guard(float *l, float *r, uint32_t n)
  {
    const uint32_t h = zeroNonFinite(l, n) + zeroNonFinite(r, n);
    if (h) { s += h; b++; l_ = true; }
    return h;
  }
  uint64_t samples() const { return s; }
  uint64_t blocks() const { return b; }
  bool latched() const { return l_; }
  void reset() { s = b = 0; }   // the latch survives a reset
};

/* ---- the rows ------------------------------------------------------------ */
struct Tally
{
  bool loud;
  int wrong = 0;
  int rows = 0;
  void expect(bool ok, const char *what)
  {
    ++rows;
    if (!ok)
    {
      ++wrong;
      if (loud) std::printf("  FAIL  %s\n", what);
    }
  }
};

constexpr uint32_t kN = 64;
struct Buf
{
  float l[kN], r[kN];
  Buf()
  {
    for (uint32_t i = 0; i < kN; i++) { l[i] = 0.25f * (float)((int)i - 32) / 32.0f; r[i] = -l[i] * 0.5f; }
  }
};

/* The planted values: every way a float stops being finite that matters. */
struct Plant { float v; const char *what; };
const Plant kPlants[] = {
    {kNaN, "quiet NaN"},
    {kSNaN, "signalling NaN"},
    {bitsToFloat(0xFFC00001u), "negative-signed NaN with payload"},
    {kInf, "+Inf"},
    {-kInf, "-Inf"},
};

template <class G>
int runRows(bool loud)
{
  Tally t{loud};
  char msg[160];

  // A: each planted value, in each channel placement, is zeroed, counted, latched.
  for (const auto &p : kPlants)
    for (int where = 0; where < 3; where++)   // 0 = left, 1 = right, 2 = both
    {
      G g;
      Buf b;
      const char *ch = where == 0 ? "left" : where == 1 ? "right" : "both";
      if (where != 1) b.l[17] = p.v;
      if (where != 0) b.r[40] = p.v;
      const uint32_t want = where == 2 ? 2 : 1;
      const uint32_t got = g.guard(b.l, b.r, kN);
      std::snprintf(msg, sizeof msg, "%s in %s: guard returns %u, want %u", p.what, ch, got, want);
      t.expect(got == want, msg);
      std::snprintf(msg, sizeof msg, "%s in %s: sample zeroed", p.what, ch);
      t.expect((where == 1 || b.l[17] == 0.0f) && (where == 0 || b.r[40] == 0.0f), msg);
      std::snprintf(msg, sizeof msg, "%s in %s: samples() == %u", p.what, ch, want);
      t.expect(g.samples() == want, msg);
      std::snprintf(msg, sizeof msg, "%s in %s: blocks() == 1", p.what, ch);
      t.expect(g.blocks() == 1, msg);
      std::snprintf(msg, sizeof msg, "%s in %s: latched", p.what, ch);
      t.expect(g.latched(), msg);
    }

  // B: a clean buffer counts 0, does not latch, and is not touched (bit-identical).
  {
    G g;
    Buf b;
    b.l[3] = -0.0f;
    b.l[4] = bitsToFloat(0x00000001u);     // smallest denormal
    b.r[5] = FLT_MAX;
    b.r[6] = -FLT_MAX;
    Buf ref = b;
    const uint32_t got = g.guard(b.l, b.r, kN);
    t.expect(got == 0, "clean buffer: guard returns 0");
    t.expect(g.samples() == 0 && g.blocks() == 0, "clean buffer: counts 0");
    t.expect(!g.latched(), "clean buffer: no latch");
    t.expect(std::memcmp(&b, &ref, sizeof b) == 0, "clean buffer: bit-identical (-0.0, denormal, FLT_MAX kept)");
  }

  // C: samples count samples, blocks count blocks; both accumulate.
  {
    G g;
    Buf b;
    b.l[0] = kNaN; b.l[1] = kInf; b.r[2] = -kInf;   // three hits, one block
    g.guard(b.l, b.r, kN);
    t.expect(g.samples() == 3 && g.blocks() == 1, "three hits in one block: samples 3, blocks 1");
    Buf b2;
    b2.r[9] = kNaN;
    g.guard(b2.l, b2.r, kN);
    t.expect(g.samples() == 4 && g.blocks() == 2, "a second event block accumulates: samples 4, blocks 2");
  }

  // D: the latch survives clean blocks; only reset() clears it (and the counts);
  //    a new event after the reset latches again.
  {
    G g;
    Buf b;
    b.l[8] = kNaN;
    g.guard(b.l, b.r, kN);
    for (int i = 0; i < 5; i++)
    {
      Buf clean;
      g.guard(clean.l, clean.r, kN);
    }
    t.expect(g.latched(), "latch survives five clean blocks");
    t.expect(g.samples() == 1 && g.blocks() == 1, "clean blocks leave the counts alone");
    g.reset();
    t.expect(!g.latched(), "reset clears the latch");
    t.expect(g.samples() == 0 && g.blocks() == 0, "reset clears the counts");
    Buf again;
    again.r[1] = -kInf;
    g.guard(again.l, again.r, kN);
    t.expect(g.latched() && g.samples() == 1, "an event after reset latches again");
  }

  // E: a zero-length block is legal and inert.
  {
    G g;
    Buf b;
    t.expect(g.guard(b.l, b.r, 0) == 0 && !g.latched(), "zero frames: nothing counted, nothing latched");
  }

  if (loud) std::printf("  %d rows, %d wrong\n", t.rows, t.wrong);
  return t.wrong;
}

struct Control { const char *name; int (*run)(bool); };

}  // namespace

int main()
{
  int red = 0;

  std::printf("REAL  hypersaw::NonFiniteLatch\n");
  const int realWrong = runRows<hypersaw::NonFiniteLatch>(true);
  if (realWrong) red = 1;
  std::printf("%s  real guard: %d row(s) wrong\n", realWrong ? "FAIL" : "PASS", realWrong);

  const Control controls[] = {
      {"zero-without-count (B446 as shipped)", runRows<ZeroWithoutCount>},
      {"count-without-latch", runRows<CountWithoutLatch>},
      {"self-clearing latch", runRows<SelfClearingLatch>},
      {"right-channel unreported", runRows<RightUnreported>},
      {"reset keeps the latch", runRows<ResetKeepsLatch>},
  };
  for (const auto &c : controls)
  {
    const int wrong = c.run(false);
    const bool wentRed = wrong > 0;
    std::printf("%s  control %-38s fails %d row(s)%s\n", wentRed ? "PASS" : "FAIL", c.name, wrong,
                wentRed ? "" : " — a faulty guard READS GREEN, the check is blind");
    if (!wentRed) red = 1;
  }

  std::printf("nan_latch_check: %s (%zu planted values x 3 placements, 5 must-fail controls)\n",
              red ? "RED" : "OK", sizeof(kPlants) / sizeof(kPlants[0]));
  return red;
}
