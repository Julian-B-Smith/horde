/*
 * robustness_matrix — B101's published robustness table, MEASURED.
 *
 * docs/ROBUSTNESS.md is a table, and a table someone typed is a claim; this
 * prints the one the doc carries so it can be regenerated rather than
 * believed. The real plugin is driven through the CLAP factory — the path
 * rtsafety_probe, mpe_check and steal_check use — at every host configuration
 * the matrix names: 44.1 / 48 / 96 / 192 kHz x 32..2048-frame buffers. Per
 * cell the human's heavy patch (user_patch_bench: both oscillators at 16
 * voices, comb + drive in the rack, drift + width) holds a four-note chord for
 * one second, releases it, and renders the tail for three. Columns:
 *
 *   allocs   operator new/delete calls inside process() — rtsafety_probe's
 *            mechanism (counting replacements of the global operators), at
 *            every rate and buffer instead of 44.1 kHz only. Must be 0.
 *   nan/inf  non-finite output samples anywhere in the four seconds. Must be 0.
 *   subn     SUBNORMAL output samples. Reported, not judged: on x86 each is a
 *            10-100x slower op, but a handful in a tail is harmless; the
 *            denormal section below is where the COST is measured.
 *   peak     dBFS over the run. Reported: the peak bound is B46's open ruling.
 *   held     RMS dBFS of the held second — the MUST-READ-LOUD control. A cell
 *            whose notes never sounded passes every other column trivially,
 *            so held must exceed -40 dBFS or the row is void.
 *   tail@3s  RMS dBFS of the last 100 ms. Must be under -60 dBFS: with release
 *            0.3 s the envelope is ~87 dB down by then and the -80 dB cull has
 *            retired the voice, so -60 is a loose bound only a leak breaches.
 *   cost     wall time / audio time for each phase, on this machine. Printed
 *            for the reader (the CPU tables live in docs/research); not judged.
 *
 * DENORMAL SECTION. The 2026-08-24 CPU audit's finding 1 ("no denormal
 * protection anywhere in src/") was struck by its own falsifier on 2026-08-25:
 * user_patch_bench with HZ_FTZ=1 measured no difference on M3 or on EPYC
 * (docs/research/2026-08-24-cpu-audit.md). This section states the mechanism
 * and re-measures it: the same patch with a 5 s release, an 8 s tail rendered
 * in 1 s windows. A window costing more than 2x the held-note window while
 * FTZ/DAZ is OFF is the stall signature and FAILS. It also prints (a) whether
 * FTZ/DAZ is active in this process — found empirically, by multiplying a
 * subnormal and seeing whether it survives — and (b) a CONTROL: a synthetic
 * all-subnormal loop timed against the same loop on normal values, so the
 * reader knows whether THIS CPU can show a stall at all. Apple silicon handles
 * subnormals at full speed and x86 does not; without (b), a clean result on an
 * M-series Mac would be the detector confirming the expected answer for the
 * wrong reason (LIBRARY: detector-shares-assumption).
 *
 * Diagnostic, not a gate: ./verify does not run it. The counting operators are
 * process-global, which is why this is its own executable rather than a mode
 * of an existing oracle.
 */
#include <algorithm>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"

namespace
{
#include "notefuzz_scaffold.inc"  // kHost, kOut, EvList, mkNote, mkParam

std::atomic<bool> g_armed{false};
std::atomic<long> g_allocs{0};
std::atomic<long> g_frees{0};
}  // namespace

void *operator new(size_t n)
{
  if (g_armed.load(std::memory_order_relaxed)) g_allocs.fetch_add(1, std::memory_order_relaxed);
  void *p = std::malloc(n ? n : 1);
  if (!p) throw std::bad_alloc();
  return p;
}
void *operator new[](size_t n) { return operator new(n); }
void operator delete(void *p) noexcept
{
  if (g_armed.load(std::memory_order_relaxed)) g_frees.fetch_add(1, std::memory_order_relaxed);
  std::free(p);
}
void operator delete[](void *p) noexcept { operator delete(p); }
void operator delete(void *p, size_t) noexcept { operator delete(p); }
void operator delete[](void *p, size_t) noexcept { operator delete(p); }

namespace
{
using Clock = std::chrono::steady_clock;

double dbOf(double lin) { return lin > 0 ? 20.0 * std::log10(lin) : -240.0; }

// The human's heavy patch, as user_patch_bench reproduces it. Release is the
// one knob the two sections set differently.
void heavyPatch(EvList &e, double releaseSec)
{
  const std::pair<clap_id, double> pv[] = {
      {1, 16},   {1001, 16},  {1150, 1}, {1017, 0.4},  // 16 voices each; osc2 enable + volume
      {22, releaseSec}, {1022, releaseSec},             // release, both oscillators
      {106, 4},  {57, 5},     {59, 1},   {9, 30}, {14, 1.5}};  // spring bend, comb, drive, drift, width
  for (auto &p : pv) e.params.push_back(mkParam(p.first, p.second));
}

const int kKeys[4] = {48, 52, 55, 60};

struct Phase
{
  long nonfinite = 0, subnormal = 0;
  double peak = 0;
  double rmsAll = 0;      // over the whole phase
  double rmsLast100 = 0;  // over the last 100 ms
  double wallSec = 0;
};

// Renders `seconds` of audio in `blk`-frame blocks; `ev` (may be null) is
// delivered with the first block. Counting is armed around each process() so
// the harness's own vectors never count.
Phase render(const clap_plugin_t *p, EvList *ev, double sr, int blk, double seconds, long &frameClock)
{
  std::vector<float> L(blk), R(blk);
  float *ch[2] = {L.data(), R.data()};
  clap_audio_buffer_t out{};
  out.data32 = ch;
  out.channel_count = 2;
  clap_process_t proc{};
  proc.frames_count = (uint32_t)blk;
  proc.audio_outputs = &out;
  proc.audio_outputs_count = 1;
  proc.out_events = &kOut;
  EvList empty;
  empty.finalize();

  const long nblocks = (long)std::llround(seconds * sr / blk);
  const long total = nblocks * blk;
  const long lastStart = total - (long)std::llround(0.1 * sr);
  Phase ph;
  double sumsq = 0, sumsqLast = 0;
  long nLast = 0;
  const auto t0 = Clock::now();
  for (long b = 0; b < nblocks; b++)
  {
    proc.in_events = (b == 0 && ev) ? &ev->list : &empty.list;
    proc.steady_time = frameClock;
    g_armed.store(true, std::memory_order_relaxed);
    p->process(p, &proc);
    g_armed.store(false, std::memory_order_relaxed);
    frameClock += blk;
    for (int i = 0; i < blk; i++)
      for (const float *c : {L.data(), R.data()})
      {
        const float v = c[i];
        if (!std::isfinite(v)) { ph.nonfinite++; continue; }
        const double a = std::fabs((double)v);
        if (v != 0.0f && a < (double)FLT_MIN) ph.subnormal++;
        ph.peak = std::fmax(ph.peak, a);
        sumsq += a * a;
        if (b * blk + i >= lastStart) { sumsqLast += a * a; nLast++; }
      }
  }
  ph.wallSec = std::chrono::duration<double>(Clock::now() - t0).count();
  ph.rmsAll = std::sqrt(sumsq / (double)(2 * total));
  ph.rmsLast100 = nLast ? std::sqrt(sumsqLast / (double)nLast) : 0.0;
  return ph;
}

const clap_plugin_t *makePlugin(const clap_plugin_factory_t *f, double sr)
{
  const clap_plugin_t *p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  p->activate(p, sr, 32, 2048);  // a typical host's declared range; blk <= 2048 below
  p->start_processing(p);
  return p;
}
void dropPlugin(const clap_plugin_t *p)
{
  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
}

// Does a subnormal survive a multiply in this process? Under FTZ the result
// is flushed; under DAZ the input is read as zero. Either way the product is
// exactly 0, and under IEEE it is 1e-40. volatile keeps it a runtime fact.
bool ftzActive()
{
  volatile float d = 1e-40f;
  volatile float one = 1.0f;
  volatile float r = d * one;
  return r == 0.0f;
}

// The CONTROL: the same dependent multiply-add chain on subnormal and on
// normal values. acc converges to 2*d, so with d subnormal every op is a
// subnormal op. The ratio is this CPU's per-op penalty (x86: 10-100 unless
// FTZ; Apple silicon: ~1).
double chainSeconds(float dv)
{
  volatile float d = dv;
  float acc = 0;
  const auto t0 = Clock::now();
  for (long i = 0; i < 20000000L; i++) acc = acc * 0.5f + d;
  const double s = std::chrono::duration<double>(Clock::now() - t0).count();
  volatile float sink = acc;
  (void)sink;
  return s;
}
}  // namespace

int main()
{
  int failures = 0;

  // The counter must FIRE before its zeros mean anything. A direct call to the
  // replaced operator, not a new-expression: the compiler may elide a paired
  // new/delete expression (it did, at -O2 — the first run of this control
  // read NO), but a call to ::operator new is not elidable.
  g_armed.store(true);
  { void *plant = ::operator new(64); ::operator delete(plant); }
  g_armed.store(false);
  const bool counterFires = g_allocs.load() == 1 && g_frees.load() == 1;
  std::printf("control: allocation counter fires on a planted new/delete: %s\n",
              counterFires ? "yes" : "NO");
  if (!counterFires) failures++;
  g_allocs.store(0);
  g_frees.store(0);

  hypersaw_entry_init("");
  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);

  // ---- the matrix ---------------------------------------------------------
  const double rates[] = {44100.0, 48000.0, 96000.0, 192000.0};
  const int blocks[] = {32, 64, 128, 256, 512, 1024, 2048};
  std::printf("\n## Matrix — heavy patch, 4-note chord held 1 s, tail 3 s\n\n");
  std::printf("| rate | buffer | allocs | nan/inf | subn | peak dBFS | held dBFS | tail@3s dBFS | held cost | tail cost | result |\n");
  std::printf("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|:---|\n");
  for (double sr : rates)
    for (int blk : blocks)
    {
      g_allocs.store(0);
      g_frees.store(0);
      const clap_plugin_t *p = makePlugin(factory, sr);
      long clock = 0;
      EvList on;
      heavyPatch(on, 0.3);
      for (int k : kKeys) on.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, 0, (int16_t)k, -1, 0.8));
      on.finalize();
      const Phase held = render(p, &on, sr, blk, 1.0, clock);
      EvList off;
      for (int k : kKeys) off.notes.push_back(mkNote(CLAP_EVENT_NOTE_OFF, 0, (int16_t)k, -1));
      off.finalize();
      const Phase tail = render(p, &off, sr, blk, 3.0, clock);
      dropPlugin(p);

      const long allocs = g_allocs.load() + g_frees.load();
      const long nonfinite = held.nonfinite + tail.nonfinite;
      const long subn = held.subnormal + tail.subnormal;
      const double peakDb = dbOf(std::fmax(held.peak, tail.peak));
      const double heldDb = dbOf(held.rmsAll);
      const double tailDb = dbOf(tail.rmsLast100);
      const bool ok = allocs == 0 && nonfinite == 0 && heldDb > -40.0 && tailDb < -60.0;
      if (!ok) failures++;
      std::printf("| %.1f k | %d | %ld | %ld | %ld | %+.2f | %.2f | %.1f | %.1f%% | %.1f%% | %s |\n",
                  sr / 1000.0, blk, allocs, nonfinite, subn, peakDb, heldDb, tailDb,
                  100.0 * held.wallSec / 1.0, 100.0 * tail.wallSec / 3.0, ok ? "OK" : "FAIL");
    }

  // ---- denormals ----------------------------------------------------------
  const bool ftz = ftzActive();
  const double subSec = chainSeconds(1e-39f), normSec = chainSeconds(1e-3f);
  const double penalty = subSec / normSec;
  std::printf("\n## Denormals — 48 kHz / 64, release 5 s, 8 s tail in 1 s windows\n\n");
  std::printf("FTZ/DAZ active in this process: %s\n", ftz ? "yes" : "no");
  std::printf("control: subnormal-chain penalty on this CPU: x%.2f (%.1f ms vs %.1f ms) — %s\n",
              penalty, 1e3 * subSec, 1e3 * normSec,
              penalty > 2.0 ? "this CPU CAN show a stall, so a clean tail below is evidence"
                            : "this CPU cannot show a stall; the tail below is not evidence either way");
  {
    const double sr = 48000.0;
    const int blk = 64;
    const clap_plugin_t *p = makePlugin(factory, sr);
    long clock = 0;
    EvList on;
    heavyPatch(on, 5.0);
    for (int k : kKeys) on.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, 0, (int16_t)k, -1, 0.8));
    on.finalize();
    const Phase held = render(p, &on, sr, blk, 1.0, clock);
    EvList off;
    for (int k : kKeys) off.notes.push_back(mkNote(CLAP_EVENT_NOTE_OFF, 0, (int16_t)k, -1));
    off.finalize();
    std::printf("\n| window | cost | subnormal samples | rms dBFS |\n|:---|---:|---:|---:|\n");
    std::printf("| held 1 s | %.1f%% | %ld | %.1f |\n", 100.0 * held.wallSec, held.subnormal, dbOf(held.rmsAll));
    bool stall = false;
    double worst = 0;
    for (int w = 0; w < 8; w++)
    {
      const Phase t = render(p, w == 0 ? &off : nullptr, sr, blk, 1.0, clock);
      const double ratio = t.wallSec / held.wallSec;
      worst = std::fmax(worst, ratio);
      if (ratio > 2.0 && !ftz) stall = true;
      std::printf("| tail t+%d s | %.1f%% | %ld | %.1f |\n", w + 1, 100.0 * t.wallSec, t.subnormal, dbOf(t.rmsAll));
    }
    dropPlugin(p);
    std::printf("\nworst tail/held cost ratio %.2f (stall threshold 2.0, judged only with FTZ/DAZ off): %s\n",
                worst, stall ? "FAIL — denormal stall signature" : "OK");
    if (stall) failures++;
  }

  hypersaw_entry_deinit();
  std::printf("\nrobustness_matrix: %s (%d failures)\n", failures ? "RED" : "GREEN", failures);
  return failures ? 1 : 0;
}
