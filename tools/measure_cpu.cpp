/*
 * measure_cpu — CPU per voice count for docs/MEASUREMENTS.md (ROADMAP B103).
 *
 * The SHIPPED plugin through the CLAP factory (cpu_bench times SwarmCore
 * alone and missed the shell's cost — see shell_bench.cpp's header), against
 * the E-6 envelope: 44.1 kHz, 128-sample buffer. Eight held notes (keys
 * 48 + 3k, the shell_bench chord), swarm size n = 1 / 4 / 8 / 16 / 32 per
 * note (param 1 "Voices", and its osc-2 twin 1001 under the ADR-082 stride),
 * one oscillator and two (osc 2 needs BOTH enable 1150 and volume 1017 —
 * ADR-100 ships it OFF, so a volume write alone still measures one). 5 s of
 * audio per cell after a 200-block warm-up; each cell is timed three times
 * and the MINIMUM is reported (the others are scheduler noise, not the
 * plugin). Reports ms of CPU per second of audio and % of one core.
 *
 * The RENDER is deterministic (seeded streams, fixed note order, no clock
 * inside process()); only the stopwatch around it is wall-clock, which is
 * what makes this a measurement rather than a gate. Release build only — a
 * -O0 number is meaningless (global CLAUDE.md, "Test in Release").
 * Standalone, registered in CMake beside svf_check, NOT in ./verify.
 *
 * ---------------------------------------------------------------------------
 * CPU_JUDGE (B147 layer 2) — the same source, built a second time as the
 * target `cpu_check`, with a judging section appended. The B147 layer-1 audit
 * (docs/audits/2026-09-18-saw-engine-audit.md §3.8) found "Gate: none.
 * `measure_cpu` prints", and its reduction note is explicit that the fix is to
 * make this already-calibrated binary JUDGE rather than to write a second
 * stopwatch. With CPU_JUDGE undefined — i.e. as `measure_cpu` — nothing below
 * changes: same table, same exit code, same "print, never judge" contract.
 *
 * WHAT IT JUDGES, and against what. Five cells, the ones the audit recorded
 * (8 held notes, % of one core): 1 osc at n = 1 / 8 / 16 / 32, and the 2-osc
 * worst at n = 32.
 *
 *   cell              audit    bar (audit x 1.5)
 *   1 osc, n=1        0.54 %   0.81 %
 *   1 osc, n=8        2.01 %   3.02 %
 *   1 osc, n=16       3.73 %   5.60 %
 *   1 osc, n=32       7.00 %   10.50 %
 *   2 osc, n=32      14.15 %   21.23 %
 *
 * THE MARGIN IS x1.5, AND IT IS SIZED BY THE NOISE, NOT BY TASTE. The audit
 * measured +-12 % run-to-run variation between passes on this machine, and
 * each cell here is already the MINIMUM of three runs, which suppresses the
 * upper tail of that. x1.5 therefore sits about four noise-widths above the
 * recorded number: comfortably above anything the scheduler can manufacture,
 * and tight enough that the 1.5x regression it is meant to catch cannot hide
 * under it. (The audit proposed 2x; 1.5 is the tighter bar that the noise
 * still clears, and it catches a 1.6x regression 2x would pass.)
 *
 * THIS IS A REGRESSION GATE, NOT THE MIN-SPEC CLAIM. Every number is one
 * machine (Apple M3). ACCEPTANCE L0-6's min-spec is a 2018-class x86
 * ultrabook and ADR-082's question is answered only by running there; a green
 * run here says "no 1.5x regression against 2026-09-18 on this machine" and
 * says nothing else.
 *
 * IT ALSO REFUSES TO JUDGE A BUSY MACHINE. See refLoopMs() below: a fixed
 * ~147 ms FLOP loop says whether the wall clock was measuring this plugin or
 * somebody else's compiler. This is not hypothetical — under a parallel build
 * this binary read 1.94 % of a core where a quiet run read 0.53 %, a 3.7x
 * "regression" that was entirely other people's toolchain. Over the limit the
 * check declines and exits 0: a wall-clock bar on a contended machine reports
 * the machine, and the one honest thing to say then is nothing.
 *
 * A DEBUG BUILD REFUSES TO JUDGE. JUCE-era lesson in the global CLAUDE.md:
 * a -O0 build measures the compiler, not the code, and a CPU bar applied to
 * one is a bar applied to noise. Under NDEBUG-absent the judging section
 * prints why it declined and exits 0 — declining loudly, rather than passing
 * quietly or failing for the wrong reason.
 */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
#include "build_stamp.h"   // generated every build (CMake target)

namespace
{
#include "notefuzz_scaffold.inc"

constexpr double kRate = 44100.0;
constexpr uint32_t kBlk = 128;     // the E-6 envelope's buffer
constexpr double kSeconds = 5.0;
constexpr int kNotes = 8;
constexpr int kReps = 3;

struct Cell { double msPerS, pct, worstPct; double readN2, readEn2, readVol2; };

/* THE QUIET-MACHINE PROBE (B147 layer 2, and the reason cpu_check can decline).
 * A wall-clock cost gate is only a gate on an idle machine. Run under a
 * parallel build this same binary read 1.94 % of a core where a quiet run read
 * 0.53 % — a 3.7x "regression" that was entirely other people's compilers, and
 * a bar applied to it would have been a coin flip dressed as an oracle.
 *
 * So: a fixed, deterministic FLOP loop whose cost on THIS machine was measured
 * on a quiet run and recorded below. It is the same kind of thing the plugin
 * cells are, so contention slows both together; if it comes back slow, the
 * machine is busy and the plugin numbers are not measurements. `volatile`
 * keeps it from being optimised away — without that the loop costs nothing and
 * the guard reports an idle machine forever.
 *
 * ITS LENGTH IS PART OF THE DESIGN. The first version ran 2e6 iterations —
 * 6.5 ms — and min-of-5 over a 6.5 ms window will find an idle slice on a
 * machine that is 80 % busy, so it reported "quiet" while the plugin cells
 * were visibly inflated. A probe short enough to dodge the load is a probe
 * that measures nothing. 5e7 iterations is ~147 ms, long enough that sustained
 * contention cannot be waited out, and stable to 0.5 % across runs
 * (147.0 / 147.5 / 147.5 / 147.7 ms measured 2026-09-18).
 *
 * A REP-SPREAD GUARD WAS TRIED AND DROPPED, recorded rather than silently
 * removed. The idea was that bursty load inflates the worst of three reps more
 * than the best, so max/min would expose what min-of-3 hides. Measured, the
 * two bands OVERLAP: 1.01-1.33x on a machine whose numbers matched the audit
 * to within 5 %, and 1.63x under a parallel build. There is no threshold
 * between 1.33 and 1.63 worth defending, so the guard could only have flapped.
 * A guard that cannot separate its own two cases is not a loose guard, it is a
 * coin flip; deleted, with the spread still PRINTED because the reader can
 * weigh a number a gate cannot. */
constexpr double kRefLoopMsQuiet = 147.0;   // measured 2026-09-18, Apple M3, min of 3
double refLoopMs()
{
  double best = 1e9;
  for (int rep = 0; rep < 3; rep++)
  {
    volatile double sink = 0;
    const auto t0 = std::chrono::steady_clock::now();
    double acc = 0;
    for (int i = 0; i < 50000000; i++) acc += std::sqrt((double)i + 1.0) * std::sin((double)i * 1e-6);
    sink = acc;
    (void)sink;
    const auto t1 = std::chrono::steady_clock::now();
    best = std::min(best, std::chrono::duration<double>(t1 - t0).count() * 1000.0);
  }
  return best;
}

Cell timeCell(const clap_plugin_factory_t *factory, int n, bool twoOsc)
{
  double best = 1e9, worst = 0; double rn = -1, re = -1, rv = -1;
  for (int rep = 0; rep < kReps; rep++)
  {
    const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
    p->init(p); p->activate(p, kRate, 32, kBlk); p->start_processing(p);
    std::vector<float> L(kBlk), R(kBlk);
    float *chans[2] = {L.data(), R.data()};
    clap_audio_buffer_t out{}; out.data32 = chans; out.channel_count = 2;
    clap_process_t proc{}; proc.frames_count = kBlk; proc.audio_outputs = &out;
    proc.audio_outputs_count = 1; proc.out_events = &kOut;
    auto once = [&](EvList &e) { e.finalize(); proc.in_events = &e.list; p->process(p, &proc); };
    {
      EvList e;
      e.params.push_back(mkParam(1, n));
      e.params.push_back(mkParam(1001, n));
      e.params.push_back(mkParam(1150, twoOsc ? 1 : 0));
      e.params.push_back(mkParam(1017, twoOsc ? 0.4 : 0));
      once(e);
    }
    // Read back what the shell holds, so a wrong id assumption shows in the
    // table instead of silently timing the default (L0032).
    auto *ext = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
    if (ext) { ext->get_value(p, 1001, &rn); ext->get_value(p, 1150, &re); ext->get_value(p, 1017, &rv); }
    for (int k = 0; k < kNotes; k++)
    { EvList e; e.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, 0, (int16_t)(48 + k * 3), k + 1)); once(e); }
    for (int b = 0; b < 200; b++) { EvList e; once(e); }   // warm-up, untimed
    const int NB = (int)(kSeconds * kRate / kBlk);
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < NB; b++) { EvList e; once(e); }
    const auto t1 = std::chrono::steady_clock::now();
    const double cpu = std::chrono::duration<double>(t1 - t0).count();
    const double audio = NB * (double)kBlk / kRate;
    best = std::min(best, cpu / audio);
    worst = std::max(worst, cpu / audio);
    p->stop_processing(p); p->deactivate(p); p->destroy(p);
  }
  return {best * 1000.0, best * 100.0, worst * 100.0, rn, re, rv};
}
}  // namespace

int main()
{
  hypersaw_entry_init("");
  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const int ns[] = {1, 4, 8, 16, 32};
  std::printf("| voices per note (n) | 1 osc: ms CPU / s audio | 1 osc: %% of a core | 2 osc: ms CPU / s audio | 2 osc: %% of a core | readback (osc2 n / enable / vol) |\n");
  std::printf("|---|---|---|---|---|---|\n");
  double one[5] = {0}, two[5] = {0}, spread = 1.0;
  for (int i = 0; i < 5; i++)
  {
    const int n = ns[i];
    const Cell a = timeCell(factory, n, false);
    const Cell b = timeCell(factory, n, true);
    one[i] = a.pct; two[i] = b.pct;
    spread = std::max(spread, std::max(a.worstPct / a.pct, b.worstPct / b.pct));
    std::printf("| %d | %.1f | %.2f%% | %.1f | %.2f%% | %.0f / %.0f / %.1f |\n",
                n, a.msPerS, a.pct, b.msPerS, b.pct, b.readN2, b.readEn2, b.readVol2);
  }
  std::printf("\nBuild %s. %d held notes (keys 48+3k), %.0f Hz, %u-sample blocks, %.0f s per cell, min of %d runs.\n",
              HYPERSAW_BUILD_STAMP, kNotes, kRate, kBlk, kSeconds, kReps);

#if defined(CPU_JUDGE) && CPU_JUDGE
  int failures = 0;
#ifndef NDEBUG
  /* A Debug build REFUSES to judge rather than judging badly. -O0 measures the
     compiler; a bar written against -O3 numbers applied to -O0 numbers fails
     for a reason that has nothing to do with the code. Exit 0: declining is
     not a failure, and a red here would train a reader to ignore it. */
  (void)one; (void)two; (void)spread;
  std::printf("\ncpu_check: DECLINED — this is a Debug (-O0) build and a CPU bar written\n"
              "against Release numbers cannot mean anything here (global CLAUDE.md,\n"
              "\"Test in Release, never Debug\"). Rebuild with -DCMAKE_BUILD_TYPE=Release.\n");
  hypersaw_entry_deinit();
  return 0;
#else
  /* THE QUIET-MACHINE GATE, before any bar. A fixed FLOP loop, 35 % over its
     recorded quiet cost = decline and exit 0. An upper-bound gate that fires
     on a busy machine is a coin flip; declining is the only reading that stays
     true. The rep spread prints beside it as information — it is not gated,
     for the reason recorded at refLoopMs(). */
  {
    const double ref = refLoopMs();
    const bool quiet = ref <= kRefLoopMsQuiet * 1.35;
    std::printf("\nmachine: reference loop %.1f ms (quiet %.1f, limit %.1f); worst rep spread %.2fx (ungated)\n",
                ref, kRefLoopMsQuiet, kRefLoopMsQuiet * 1.35, spread);
    if (!quiet)
    {
      std::printf("cpu_check: DECLINED — this machine is not idle, so these are not measurements.\n"
                  "Nothing is judged and nothing is claimed. Re-run with the machine quiet.\n");
      hypersaw_entry_deinit();
      return 0;
    }
  }

  /* The audit's recorded cells (2026-09-18, Apple M3) and the bar at x1.5.
     `audit` is the number on the record; `bar` is a literal, not `audit*1.5`
     computed at runtime, so the bar is a decision in the file rather than a
     formula that silently follows a re-measurement. */
  struct Row { const char *name; double measured; double audit; double bar; };
  const Row rows[] = {
      {"1 osc, n=1",  one[0], 0.54,  0.81},
      {"1 osc, n=8",  one[2], 2.01,  3.02},
      {"1 osc, n=16", one[3], 3.73,  5.60},
      {"1 osc, n=32", one[4], 7.00, 10.50},
      {"2 osc, n=32", two[4], 14.15, 21.23},
  };
  std::printf("\n-- cpu_check: regression bars (audit x 1.5; NOT the min-spec claim) --\n");
  for (const auto &r : rows)
  {
    const bool ok = r.measured <= r.bar;
    if (!ok) failures++;
    std::printf("%-6s %-14s %6.2f%% of a core   (audit %5.2f%%, bar %5.2f%%, %+.0f%% vs audit)\n",
                ok ? "PASS" : "FAIL", r.name, r.measured, r.audit, r.bar,
                100.0 * (r.measured - r.audit) / r.audit);
  }
  /* THE CONTROL. Every bar above is an upper bound, so a binary that measured
     nothing at all — a plugin that failed to instantiate, a render loop that
     was optimised away — would pass all five with 0.00 %. The scaling law is
     what says the work happened: cost must RISE with n, and the 2-osc column
     must cost more than the 1-osc column. A monotone-and-doubling reading is
     not provable from an upper bound and has to be asserted separately
     (L0032: a check whose only assertions are "less than" has no way to tell
     success from absence). */
  bool rising = true;
  for (int i = 1; i < 5; i++) if (!(one[i] > one[i - 1])) rising = false;
  std::printf("%-6s %-14s n=1..32 reads %.2f -> %.2f -> %.2f -> %.2f -> %.2f %% of a core\n",
              rising ? "PASS" : "FAIL", "CONTROL rise", one[0], one[1], one[2], one[3], one[4]);
  if (!rising) failures++;
  const bool doubles = two[4] > one[4] * 1.5;
  std::printf("%-6s %-14s 2 osc at n=32 is %.2fx the 1-osc cell (must exceed 1.5x)\n",
              doubles ? "PASS" : "FAIL", "CONTROL 2 osc", one[4] > 0 ? two[4] / one[4] : 0.0);
  if (!doubles) failures++;

  std::printf("\ncpu_check: %s (%d failures)\n", failures ? "RED" : "GREEN", failures);
  hypersaw_entry_deinit();
  return failures ? 1 : 0;
#endif
#else
  hypersaw_entry_deinit();
  return 0;
#endif
}
