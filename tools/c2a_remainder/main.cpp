// B667 / CONTRACT 7 C2a: what one swarm oscillator costs in the clean-room engine APART FROM its members' blade and wave
// reads (the "remainder": swarm laws, coupling, drift, envelopes, voice overhead), so PF-1's re-anchored figure can be
// proposed. Measured as PF-3 says (the clean-room bench's own method, tests/cpu/main.cpp, imitated, not linked).
//   c2a_remainder --count [--repeats N]     instructions and cycles retired over the timed span (any machine state)
//   c2a_remainder --timed [--smoke]         PF-3's protocol with a contamination check in place of its load guard (ADR-391).
//
// THE DIVERGENCE FROM PF-3 (ADR-391, the human 2026-11-10). PF-3's load guard asks "is anything else busy?" and refused all
// ten attempts on the night of 2026-11-10 because macOS's own Spotlight and Siri Suggestions services held about one core.
// What contaminates a wall-clock timing is narrower: our thread on a slower core or a lowered clock, or our thread preempted.
// So each render is bracketed by PF-3's calibration loop, and a repeat counts only if (1) both calibrations are within 2 % of
// the fastest calibration of the run and (2) its wall time is within 2 % of the thread's own CPU time. Two must-fail
// controls run every time: a calibration beside twice as many spinning threads as cores must fail (1), and a render beside
// them must fail (2); if either passes, the check is blind and nothing is reported. Load
// is still printed, never gating. ADR-391 A1: 30 s of the bench's own work first, so the chip's short boost is spent and
// "the run's fastest" is the steady clock (the boost made every repeat fail by about 6 %). The same change is proposed for PF-3 itself at the next spec round (ROADMAP B669).
//
// Why two modes. --count reads the process's own retired-instruction counter (proc_pid_rusage, RUSAGE_INFO_V4: no root, no
// dependency), which a busy machine barely moves, so it may run any time. Instructions are not time: they do not see cache
// misses or the cost of a mispredicted branch, so a count ratio is a floor on a time ratio, never the PF-1 figure itself.
// ri_cycles is printed beside it and IS contention-sensitive. --timed is the figure PF-3 defines.
//
// Determinism: no wall-clock read reaches the engine; steady_clock, getloadavg and `ps` live in the timing harness only
// (the same allowance Q-TA-50 gives the clean-room bench).
#include <libproc.h>
#include <pthread.h>
#include <pthread/qos.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "bank.h"
#include "pf3_chord.h"

using namespace horde_test;

namespace {

constexpr int kBlock = 128;
constexpr int kVoices = 8;

// ---- the cells ---------------------------------------------------------------------------------------------
struct CellSpec {
  const char* name;
  int unison;          // 0: the patch's own count
  bool blades_off;     // both blades' `on` switch at off, so no blade code runs
  bool width_zero;     // blades stay on, both widths at the law's off zone (an alternative reading of "off")
  bool silent;         // no notes: the idle floor
  int base = -1;       // baseWave option index (0 sine, 1 triangle, 2 saw, 3 reverse saw, 4 square); -1: the patch's own
};
const CellSpec kCells[] = {
    {"A as is, patch's own unison (PF-1 anchor)", 0, false, false, false},
    {"B as is, unison 9", 9, false, false, false},
    {"C blades off, unison 9", 9, true, false, false},
    {"D blades off, unison 1", 1, true, false, false},
    {"E blades off, unison 5 (a third point)", 5, true, false, false},
    {"F blades on but width 0, unison 9", 9, false, true, false},
    {"G silent control (no notes), unison 9", 9, false, false, true},
    // The base wave changes the per-member path (swarm.cpp sets s.base; wide.h/blades.cpp branch on Triangle's corners and on
    // Sine against the straight waves), so these three split the per-member slope into wave read and swarm law.
    {"H blades off, sine base, unison 1", 1, true, false, false, 0},
    {"I blades off, sine base, unison 5", 5, true, false, false, 0},
    {"J blades off, sine base, unison 9", 9, true, false, false, 0},
};
constexpr int kCellCount = int(sizeof(kCells) / sizeof(kCells[0]));

void apply(horde::Engine& e, const CellSpec& s) {
  if (s.unison) set_params(e, {{"osc1.unison", double(s.unison), true}});
  if (s.blades_off) set_params(e, {{"osc1.blade1.on", 0}, {"osc1.blade2.on", 0}});
  if (s.base >= 0) set_params(e, {{"osc1.baseWave", double(s.base)}});
  if (s.width_zero) set_params(e, {{"osc1.blade1.width", 0}, {"osc1.blade2.width", 0}});
}

int unison_of(const horde::Engine& e) { return int(std::lround(e.param_v(id_of("osc1.unison")) * 15)) + 1; }   // 16 options, 1..16

// ---- one render --------------------------------------------------------------------------------------------
struct Reading { double seconds = 0, cpu_seconds = 0; std::uint64_t instr = 0, cycles = 0, member_samples = 0; bool ok = false; };

void usage_now(std::uint64_t& instr, std::uint64_t& cycles) {
  rusage_info_v4 ri{};
  proc_pid_rusage(getpid(), RUSAGE_INFO_V4, reinterpret_cast<rusage_info_t*>(&ri));
  instr = ri.ri_instructions;
  cycles = ri.ri_cycles;
}

double thread_cpu_now() {   // this thread's own CPU time, user + system: what preemption cannot inflate
  timespec ts{};
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return double(ts.tv_sec) + 1e-9 * double(ts.tv_nsec);
}

// 0.25 s untimed, then 2 s: the PF-3 spans. Events exist only in block 0 (the chord held before the first block), so the
// timed span is process() and an empty drain, nothing else.
Reading render(const horde::BankPreset& pad, const CellSpec& s, double sr) {
  Reading out;
  auto e = horde::make_engine();
  if (!e || !e->prepare(sr, kBlock) || !e->load_state(pad.state).ok) return out;
  apply(*e, s);
  set_params(*e, {{"voice.polyphony", double(kVoices), true}});
  std::vector<horde::Event> notes;
  if (!s.silent)
    for (int k = 0; k < kVoices; ++k) {
      horde::Event on;
      on.type = horde::EventType::NoteOn; on.note_id = k + 1; on.channel = 0; on.key = horde_pf3::chord_key(k); on.value = 0.85;   // PF-3: velocity 0.85
      notes.push_back(on);
    }
  const int warm = int(std::lround(0.25 * sr / kBlock)), timed = int(std::lround(2.0 * sr / kBlock));
  std::vector<float> l(kBlock), r(kBlock);
  std::uint64_t i0 = 0, c0 = 0, m0 = 0;
  std::chrono::steady_clock::time_point t0;
  double cpu0 = 0;
  for (int b = 0; b < warm + timed; ++b) {
    const horde::Event* ev = b == 0 ? notes.data() : nullptr;
    if (b == warm) { m0 = e->counters().member_samples; usage_now(i0, c0); cpu0 = thread_cpu_now(); t0 = std::chrono::steady_clock::now(); }
    e->process(ev, b == 0 ? notes.size() : 0, nullptr, 0, l.data(), r.data(), kBlock);
    for (horde::OutEvent o[64]; e->drain_output_events(o, 64) > 0;) {}
  }
  const auto t1 = std::chrono::steady_clock::now();
  const double cpu1 = thread_cpu_now();
  std::uint64_t i1 = 0, c1 = 0;
  usage_now(i1, c1);
  const double audio = double(timed) * kBlock / sr;
  out.seconds = std::chrono::duration<double>(t1 - t0).count() / audio;   // per second of audio
  out.cpu_seconds = (cpu1 - cpu0) / audio;
  out.instr = std::uint64_t(double(i1 - i0) / audio);
  out.cycles = std::uint64_t(double(c1 - c0) / audio);
  out.member_samples = std::uint64_t(double(e->counters().member_samples - m0) / audio);
  out.ok = true;
  return out;
}

// ---- the contamination check (ADR-391) and PF-3's calibration loop ------------------------------------------
constexpr double kCleanTol = 1.02;   // ADR-391: calibration within 2 % of the run's fastest; wall within 2 % of thread CPU
constexpr int kBest = 5;          // PF-3: best of 5
constexpr double kWarmupSeconds = 30;   // ADR-391 A1: the boost is spent before the first repeat
constexpr int kMaxRounds = 20;    // then refuse: about 20 minutes of rounds without 5 clean repeats of every cell

double busiest_other_process() {   // reported only, never gating since ADR-391
  FILE* p = popen("ps -A -o %cpu=,pid=", "r");   // read-only
  if (!p) return INFINITY;
  double worst = 0, cpu = 0;
  long pid = 0;
  const long self = long(getpid());
  while (std::fscanf(p, "%lf %ld", &cpu, &pid) == 2)
    if (pid != self) worst = std::max(worst, cpu / 100.0);
  pclose(p);
  return worst;
}
double calibration() {   // 10^8 dependent multiply-adds, seconds
  volatile double seed = 1.0000001;
  double x = seed;
  const auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 100000000; ++i) x = x * 0.9999999 + 1e-9;
  const auto t1 = std::chrono::steady_clock::now();
  seed = x;
  return std::chrono::duration<double>(t1 - t0).count();
}

struct Sample { double wall, cpu, cal_before, cal_after; };
bool calibration_clean(const Sample& s, double fastest) { return std::max(s.cal_before, s.cal_after) <= kCleanTol * fastest; }
bool unpreempted(const Sample& s) { return s.wall <= kCleanTol * s.cpu; }

// Twice as many spinning threads as cores, for the controls: whatever runs beside them is preempted.
struct Spinners {
  std::atomic<bool> stop{false};
  std::vector<std::thread> spin;
  unsigned n = 2 * std::max(1u, std::thread::hardware_concurrency());
  Spinners() {
    for (unsigned i = 0; i < n; ++i)
      spin.emplace_back([this] {
        pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
        volatile double x = 1;
        while (!stop.load(std::memory_order_relaxed)) x = x * 0.999 + 1;
      });
  }
  ~Spinners() { stop = true; for (auto& t : spin) t.join(); }
};

// Control (1): PF-3's calibration slowed for real, by running it beside the spinners; it must fall outside 2 % of the run's
// fastest, or the calibration test cannot see a slow calibration. The planned plant, a calibration on an efficiency core, is
// not reachable here: on this M3 (macOS 26) a thread at background QoS, or under PRIO_DARWIN_BG, still ran the loop at the
// performance cores' 3.6 GHz (B668 trace), so a slow core is caught only through the slower calibration it would cause, and
// the clock of the fastest calibration is printed so that a run slow throughout (heat, low power) is visible.
// Control (2): a render beside the spinners; its wall time must fall outside 2 % of its CPU time.
double calibration_beside_spinners() { Spinners s; return calibration(); }
template <class Render> Sample render_beside_spinners(Render&& r) {
  Spinners s;
  const Reading rd = r();
  return {rd.seconds, rd.cpu_seconds, 0, 0};
}
double calibration_clock_ghz() {   // the calibration's own clock: cycles retired over its wall time (one thread running)
  std::uint64_t i0 = 0, c0 = 0, i1 = 0, c1 = 0;
  usage_now(i0, c0);
  const double t = calibration();
  usage_now(i1, c1);
  return double(c1 - c0) / t * 1e-9;
}

// ---- reading the cells -------------------------------------------------------------------------------------
// A value per cell (instructions or share of a core), then the decomposition PF-3 cannot give directly: blades off at unison
// 1, the patch's own count and 9 are three points of cost against members. A line through them (least squares) gives the
// per-member slope and the per-voice intercept. What the slope holds: every per-member cost with blades off, which is the base
// wave's read AND the member's swarm law, drift and gain; they cannot be told apart from these cells.
struct Line { double slope, intercept; };
Line fit(const std::vector<std::pair<double, double>>& pts) {
  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (auto [x, y] : pts) { sx += x; sy += y; sxx += x * x; sxy += x * y; }
  const double n = double(pts.size()), d = n * sxx - sx * sx;
  return {(n * sxy - sx * sy) / d, (sy - (n * sxy - sx * sy) / d * sx) / n};
}

void analyse(const char* unit, double scale, const std::vector<double>& v, double voice_samples_per_s) {
  const double A = v[0], B = v[1], C = v[2], D = v[3], E = v[4], F = v[5], G = v[6];
  const Line ln = fit({{1, D}, {5, E}, {9, C}});
  const double at9 = ln.intercept + ln.slope * 9;
  std::printf("\n  analysis in %s (blades off, cost against members; points unison 1, 5, 9):\n", unit);
  std::printf("    per-member slope (base-wave read + the member's swarm law, drift, gain; not separable): %.4f\n", ln.slope * scale);
  std::printf("    per-voice intercept at zero members (voice, envelope, swarm set-up, pan, mix; 8 voices): %.4f\n", ln.intercept * scale);
  std::printf("    line's own error at unison 9 against cell C: %+.4f (small = the three points are collinear)\n", (at9 - C) * scale);
  std::printf("    blades' share of cell B (B - C): %.4f of %.4f = %.1f %%\n", (B - C) * scale, B * scale, 100 * (B - C) / B);
  std::printf("    cell A minus cell B (the patch's own unison against 9; 0 when the patch's own is 9): %+.4f\n", (A - B) * scale);
  std::printf("    F (blades on, width 0) minus C: %+.4f (how much of the blade cost an `on` switch saves beyond width 0)\n", (F - C) * scale);
  std::printf("    idle floor G (no notes, as a share of B): %.4f = %.1f %%\n", G * scale, 100 * G / B);
  std::printf("    REMAINDER, upper bound (cell C, blades off, all members, including their base-wave reads, less the idle floor): %.4f\n", (C - G) * scale);
  if (voice_samples_per_s > 0) std::printf("    intercept per voice-sample (8 voices x sample rate): %.1f\n", ln.intercept / voice_samples_per_s);
  const Line sn = fit({{1, v[7]}, {5, v[8]}, {9, v[9]}});
  std::printf("    SINE base, blades off (cells H, I, J): slope %.4f, intercept %.4f, line's error at 9 against J %+.4f\n", sn.slope * scale, sn.intercept * scale, (sn.intercept + sn.slope * 9 - v[9]) * scale);
  std::printf("    SPLIT of the patch's own per-member slope (%.4f): wave-specific excess over sine = %.4f (%.1f %%); sine slope = %.4f (%.1f %%) = swarm law, drift, gain AND sine's own read, so the swarm law alone is at most this\n",
              ln.slope * scale, (ln.slope - sn.slope) * scale, 100 * (ln.slope - sn.slope) / ln.slope, sn.slope * scale, 100 * sn.slope / ln.slope);
  std::printf("    per-voice intercept, sine vs own base: %.4f vs %.4f (the base wave should not move it)\n", sn.intercept * scale, ln.intercept * scale);
  std::printf("    REMAINDER with the sine base (cell J less the idle floor): %.4f\n", (v[9] - G) * scale);
  std::printf("    REMAINDER, per-voice part only (the intercept): %.4f; plus members x slope = the upper bound above, the base-wave read being inside the slope\n", ln.intercept * scale);
}

}  // namespace

int main(int argc, char** argv) {
#ifndef NDEBUG
  std::puts("REFUSED  PF-3 measures a Release build only; this is not one (NDEBUG unset)");
  return 1;
#endif
  bool count = false, timed = false, smoke = false;
  int repeats = 3;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--count")) count = true;
    else if (!std::strcmp(argv[i], "--timed")) timed = true;
    else if (!std::strcmp(argv[i], "--smoke")) smoke = true;   // with --timed: ONE round, controls run, nothing required clean: NOT a measurement
    else if (!std::strcmp(argv[i], "--repeats") && i + 1 < argc) repeats = std::max(1, std::atoi(argv[++i]));
  }
  if (count == timed) { std::puts("usage: c2a_remainder --count [--repeats N] | --timed [--smoke]"); return 64; }

  horde_bank::Bank bank;
  std::string why;
  if (!horde_bank::load("bank/presets.json", bank, why)) { std::printf("FAIL  %s\n", why.c_str()); return 1; }
  const horde::BankPreset* pad = nullptr;
  for (const auto& p : bank.translated) if (p.name == "Glass horde pad") pad = &p;
  if (!pad) { std::puts("FAIL  no Glass horde pad in the bank"); return 1; }

  int own_unison = 0;
  {
    auto e = horde::make_engine();
    if (!e || !e->prepare(44100, kBlock) || !e->load_state(pad->state).ok) { std::puts("FAIL  the pad does not load"); return 1; }
    own_unison = unison_of(*e);
    const auto on = [&](const char* k) { return int(std::lround(e->param_v(id_of(k)))); };
    std::printf("Glass horde pad, translated: osc1 unison %d (patch's own), blade1.on %d, blade2.on %d, osc2.on %d, sub.on %d, noise.on %d, baseWave option %d\n", own_unison, on("osc1.blade1.on"),
                on("osc1.blade2.on"), on("osc2.on"), on("sub.on"), on("noise.on"),
                int(std::lround(e->param_v(id_of("osc1.baseWave")) * (horde::find_param("osc1.baseWave")->steps - 1))));
  }

  const double rates[] = {44100.0, 48000.0};
  if (count) {
    std::printf("MODE count: %d repeats per cell, 8 voices, velocity 0.85, Standard, 128-sample blocks, 0.25 s untimed + 2 s counted; machine state NOT guarded (instructions barely move; cycles do).\n", repeats);
    for (double sr : rates) {
      std::printf("\n== %.0f Hz ==\n%-46s %16s %16s %14s %9s\n", sr, "cell", "instr / audio s", "cycles / audio s", "member-smp / s", "instr/m-s");
      std::vector<double> ins;
      for (const auto& s : kCells) {
        std::uint64_t imin = UINT64_MAX, imax = 0, cmin = UINT64_MAX, ms = 0;
        for (int k = 0; k < repeats; ++k) {
          const Reading rd = render(*pad, s, sr);
          if (!rd.ok) { std::printf("FAIL  %s\n", s.name); return 1; }
          imin = std::min(imin, rd.instr); imax = std::max(imax, rd.instr); cmin = std::min(cmin, rd.cycles); ms = rd.member_samples;
        }
        std::printf("%-46s %16llu %16llu %14llu %9.1f   (instr spread %.4f %%)\n", s.name, (unsigned long long)imin, (unsigned long long)cmin, (unsigned long long)ms,
                    ms ? double(imin) / double(ms) : NAN, 100.0 * double(imax - imin) / double(imin));
        ins.push_back(double(imin));
      }
      analyse("millions of instructions per second of audio", 1e-6, ins, double(kVoices) * sr);
    }
    return 0;
  }

  // --timed: rounds of every cell, interleaved; each render bracketed by the calibration loop; a repeat counts only when
  // clean (ADR-391); the first 5 clean repeats of each cell are PF-3's best of 5. The controls run first and again at the end.
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);   // ask for a performance core; the check confirms it
  if (smoke) std::puts("SMOKE  one round, controls run, nothing required clean: this checks the plumbing and is NOT a PF-3 measurement");
  std::vector<Sample> samples[2][kCellCount];
  std::vector<double> calib;
  const auto clean_count = [&](int ri, int c, double fastest) {
    int n = 0;
    for (const auto& x : samples[ri][c]) n += calibration_clean(x, fastest) && unpreempted(x);
    return n;
  };
  double slowed = calibration_beside_spinners();
  const Sample preempted = render_beside_spinners([&] { return render(*pad, kCells[3], rates[0]); });   // cell D, cheap
  // ADR-391 A1: this M3 boosts (about 3.98 GHz) for the first seconds of sustained work, then holds about 3.74 GHz; "the run's
  // fastest" counts only calibrations after 30 s of the bench's own work, so every repeat is judged at the steady clock.
  {
    const auto w0 = std::chrono::steady_clock::now();
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - w0).count() < kWarmupSeconds) render(*pad, kCells[1], rates[0]);
    std::printf("WARM-UP  %.0f s of cell B before the first repeat (ADR-391 A1); its calibrations are not counted\n", kWarmupSeconds);
  }
  int rounds = 0;
  for (; rounds < (smoke ? 1 : kMaxRounds); ++rounds) {
    int rej_cal = 0, rej_pre = 0;
    for (int ri = 0; ri < 2; ++ri)
      for (int c = 0; c < kCellCount; ++c) {
        Sample x{};
        x.cal_before = calibration();
        const Reading rd = render(*pad, kCells[c], rates[ri]);
        x.cal_after = calibration();
        if (!rd.ok) { std::printf("FAIL  %s\n", kCells[c].name); return 1; }
        x.wall = rd.seconds; x.cpu = rd.cpu_seconds;
        samples[ri][c].push_back(x);
        calib.push_back(x.cal_before); calib.push_back(x.cal_after);
      }
    const double fastest = *std::min_element(calib.begin(), calib.end());
    int least = kBest;
    for (int ri = 0; ri < 2; ++ri)
      for (int c = 0; c < kCellCount; ++c) {
        least = std::min(least, clean_count(ri, c, fastest));
        const Sample& x = samples[ri][c].back();
        rej_cal += !calibration_clean(x, fastest); rej_pre += !unpreempted(x);
      }
    double load[1] = {NAN};
    getloadavg(load, 1);
    std::printf("round %d: fewest clean repeats of any cell %d of %d; this round rejected %d for calibration, %d for preemption (load %.2f, busiest other process %.2f cores: reported, not gating)\n",
                rounds + 1, least, kBest, rej_cal, rej_pre, load[0], busiest_other_process());
    std::fflush(stdout);
    if (least >= kBest) { ++rounds; break; }
  }
  slowed = std::min(slowed, calibration_beside_spinners());   // the slowed calibration at its fastest, against the run's fastest
  const double ghz = calibration_clock_ghz();
  const double fastest = *std::min_element(calib.begin(), calib.end());
  const bool ctl_cal = !calibration_clean({0, 0, slowed, slowed}, fastest);
  const bool ctl_pre = !unpreempted(preempted);
  std::printf("CONTROL (1) calibration beside %u spinning threads: %.3f s against the run's fastest %.3f s (x%.3f): %s\n", Spinners{}.n, slowed, fastest, slowed / fastest,
              ctl_cal ? "REJECTED, as it must be" : "PASSED: the calibration test is blind");
  std::printf("CONTROL (2) render beside the spinning threads: wall/CPU %.3f: %s\n", preempted.wall / preempted.cpu,
              ctl_pre ? "REJECTED, as it must be" : "PASSED: the preemption test is blind");
  if (!ctl_cal || !ctl_pre) { std::puts("REFUSED  a contamination control did not fire (ADR-391); nothing is reported"); return 3; }
  std::printf("MODE timed (PF-3, contamination check ADR-391): %d round(s); calibration %.3f to %.3f s across the run; its clock now %.2f GHz (reported, not gating)\n", rounds, fastest,
              *std::max_element(calib.begin(), calib.end()), ghz);
  for (int ri = 0; ri < 2; ++ri) {
    std::printf("\n== %.0f Hz: share of one core, best of the first %d clean repeats ==\n", rates[ri], kBest);
    std::vector<double> best;
    for (int c = 0; c < kCellCount; ++c) {
      std::vector<double> t;
      for (const auto& x : samples[ri][c])
        if (int(t.size()) < kBest && calibration_clean(x, fastest) && unpreempted(x)) t.push_back(x.wall);
      if (t.empty() && smoke) for (const auto& x : samples[ri][c]) t.push_back(x.wall);   // smoke: plumbing only
      if (t.empty() || (!smoke && int(t.size()) < kBest)) {
        std::printf("REFUSED  %s has %zu clean repeats of %d after %d rounds (ADR-391); nothing is reported\n", kCells[c].name, t.size(), kBest, rounds);
        return 2;
      }
      best.push_back(*std::min_element(t.begin(), t.end()));
      std::printf("%-46s %7.3f %%   (worst/best %.3f over %zu)\n", kCells[c].name, 100 * best.back(), *std::max_element(t.begin(), t.end()) / best.back(), t.size());
    }
    analyse("percent of one core", 100.0, best, 0);
  }
  return 0;
}
